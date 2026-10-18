/** Pure lifecycle rules shared with the isolated worker fixtures. */
#ifndef AL_VCAM_MEDIA_FILE_LIFECYCLE_H
#define AL_VCAM_MEDIA_FILE_LIFECYCLE_H
#include <cstdint>
#include <utility>
#include <functional>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <map>
#include "../llplugin/llpluginfileclose.h"
#include "alvcammediafilepolicy.h"
namespace ALMediaFile
{
inline bool deadlineExpired(int64_t deadline, int64_t now)
{ return deadline != 0 && now >= deadline; }
template<class Deadline> bool ioExpired(const Deadline& deadline, int64_t now)
{ const auto snapshot = deadline.load(); return deadlineExpired(snapshot, now); }
inline bool totalExpired(bool maintenance, int64_t started, int64_t now)
{ return !maintenance && started != 0 && now - started >= 30 * 60 * 1000; }
inline bool finishOperation(bool maintenance, bool expired, bool unwinding)
{
    if (!expired) return false;
    if (!maintenance) return true; // transfer cancellation is sticky
    if (!unwinding) throw std::runtime_error(FAILED);
    return false; // preserve the original exception; never poison maintenance
}
template<class Work, class Report> bool maintenanceItem(Work&& work, Report&& report)
{
    try { work(); return true; }
    catch (...) { report(); return false; }
}
template<class Work, class Failed> void maintenanceWork(Work work, Failed& failed)
{ failed = !maintenanceItem(work, []() {}); }

template<class Pending, class Work, class Remaining>
void maintainCache(Pending& clearPending, Pending& cleanupPending, Work sweep, Remaining remaining)
{
    const bool clearing = clearPending;
    try
    {
        sweep(clearing);
        if (clearing) clearPending = remaining();
        cleanupPending = false;
    }
    catch (...)
    {
        // Keep Clear gated until its sweep and index publication succeed.
        cleanupPending = true;
        throw;
    }
}

// A failed unlink must not gate all later HTTP requests forever. The final
// fallback only drops records; it never unlinks a leased/undeletable file.
template<class Pending, class Degraded, class Work, class Remaining, class Drop>
void maintainCache(Pending& clearPending, Pending& cleanupPending, unsigned& failures,
    Degraded& degraded, Work sweep, Remaining remaining, Drop drop)
{
    try
    {
        maintainCache(clearPending, cleanupPending, sweep, remaining);
        failures = 0;
    }
    catch (...)
    {
        if (!clearPending || ++failures < 3) throw;
        drop();
        degraded = true;
        cleanupPending = false;
        clearPending = false;
        failures = 0;
    }
}

template<class Pending, class Cancel, class Wait>
void awaitCacheClear(const Pending& pending, Cancel cancel, Wait wait)
{ while (pending) { cancel(); wait(); } }

// Picker/preflight results only commit a descriptor after successful validation.
template<class Apply> bool commitFileBrowse(std::string& status, const std::string& result,
    bool validated, bool current, Apply apply)
{ status = result; if (!validated || !current) return false; apply(); return true; }

// Restore the slot's own reason before reconciliation so an overlay can never
// become the next frame's underlying status.
struct BrowseOverlay
{
    std::string underlying, shown;
    int64_t until = 0;
    uint64_t observedState = 0;
    void restore(std::string& reason)
    { if (!shown.empty() && reason == shown) reason = underlying; }
    void apply(std::string& status, bool pending, std::string& reason, int64_t now, uint64_t state = 0)
    {
        if (status.empty()) { *this = {}; return; }
        if (pending) until = 0;
        else if (!until) until = now + 5000;
        else if (now >= until || reason != underlying || state != observedState)
        { status.clear(); *this = {}; return; }
        underlying = reason; shown = status; observedState = state; reason = shown;
    }
};

// Unindexed bytes remain charged until unlink succeeds. Unknown enumeration
// results prohibit reservations until a complete later inventory succeeds.
struct CacheOrphans
{
    std::map<std::string,uint64_t> files;
    bool unknown = false;
    int64_t nextRetry = 0;
    std::string cursor;
    void abandon(const std::string& name, uint64_t written, uint64_t reservation)
    { files[name] = std::max(written, reservation); }
    uint64_t bytes() const
    {
        uint64_t used = 0;
        for (const auto& file : files) used += std::min(file.second, UINT64_MAX-used);
        return unknown ? UINT64_MAX : used;
    }
    template<class Remove> void retry(int64_t now, Remove remove)
    {
        if (now < nextRetry) return;
        nextRetry = now + 60000;
        const auto count = std::min<size_t>(128, files.size());
        for (size_t i = 0; i < count; ++i)
        {
            auto it = files.upper_bound(cursor);
            if (it == files.end()) it = files.begin();
            cursor = it->first;
            if (remove(it->first)) files.erase(it);
        }
    }
};
inline bool cacheFits(uint64_t used, uint64_t reservation, uint64_t limit)
{ return used <= limit && reservation <= limit-used; }
template<class Entries, class Evicted> void dropCacheHits(Entries& entries, Evicted& evicted)
{
    for (const auto& pair : entries) evicted.push_back(pair.second);
    entries.clear();
}
template<class Entries, class Evicted>
uint64_t cacheUsage(const Entries& entries, const Evicted& evicted, const CacheOrphans& orphans)
{
    uint64_t used = orphans.bytes();
    for (const auto& pair : entries) used += std::min(pair.second->size, UINT64_MAX-used);
    for (const auto& entry : evicted) used += std::min(entry->size, UINT64_MAX-used);
    return used;
}

// Scene commit core, shared by the viewer's validated scene-apply entry point.
template<class Slots, class Stop> void replaceSceneSources(Slots& slots, Stop stop)
{
    for (unsigned slot = 0; slot < std::size(slots); ++slot) stop(slot);
    for (auto& slot : slots) slot = {};
}

// A picker is independent of playback ownership until a path is chosen.
struct FilePickerTicket
{
    uint64_t serial = 0;
    uint64_t begin() { return ++serial; }
    void cancel() { ++serial; }
    template<class Apply> bool picked(uint64_t ticket, bool current, Apply apply) const
    { if (ticket != serial || !current) return false; apply(); return true; }
};
inline bool matchingPrivateOwner(bool occupied, bool url, uint64_t generation, uint64_t owner)
{ return occupied && url && generation == owner; }
template<class Owners, class Matches, class Stop>
void sweepPrivateOwners(Owners& owners, Matches matches, Stop stop)
{
    for (unsigned slot = 0; slot < owners.size(); ++slot)
        if (owners[slot] && !matches(slot)) stop(slot);
}
inline double privateOwnerDemand(bool matching, bool retiring, bool sameImpl, bool active)
{ return matching && !retiring && sameImpl && active ? 1024. * 1024. : 0.; }
inline bool retryCacheHit(const std::string& reason)
{ return reason == MISSING || reason == NOT_VIDEO; }
template<class Validate, class Publish, class Evict>
bool validatedCacheHit(Validate validate, Publish publish, Evict evict)
{
    try { publish(validate()); return true; }
    catch (const std::runtime_error& e)
    {
        if (!retryCacheHit(e.what())) throw;
        evict();
        return false; // caller follows its ordinary miss path once
    }
}
inline bool privateEndStops(bool admitted, bool done, bool loop)
{ return admitted && done && !loop; }
inline bool windowMinimizedHides(bool privateFile, bool active)
{ return !privateFile && !active; }
struct DeferredClose
{
    std::shared_ptr<void> retention;
    std::function<bool()> closed;
    DeferredClose(std::shared_ptr<void> keep, std::function<bool()> done)
        : retention(std::move(keep)), closed(std::move(done)) {}
    DeferredClose(std::shared_ptr<void> keep, std::shared_ptr<LLPluginFileClose> fence)
        : DeferredClose(std::move(keep), [fence]() { return !fence || fence->poll(); }) {}
    bool poll()
    {
        if (!closed()) return false;
        retention.reset();
        return true;
    }
};
struct FileIdentity
{
    uint64_t device = 0, inode = 0, size = 0;
    int64_t seconds = 0, nanos = 0;
    bool operator==(const FileIdentity& b) const
    { return device == b.device && inode == b.inode && size == b.size && seconds == b.seconds && nanos == b.nanos; }
};
}
#endif
