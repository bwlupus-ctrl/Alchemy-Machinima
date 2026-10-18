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
#include <set>
#include <atomic>
#include <sstream>
#include "../llplugin/llpluginfileclose.h"
#include "alvcammediafilepolicy.h"
namespace ALMediaFile
{
// One bounded attempt per explicit Play after failure. Pending attempts coalesce;
// maintenance never retries automatically, and a later Play can recover again.
inline bool requestCacheOwnerRetry(std::atomic<int>& state)
{ int failed = -1; return state.compare_exchange_strong(failed, 0); }
// Invalid editor text never reaches source replacement (which can stop a player).
template<class Apply> bool commitVideoDraft(const std::string& draft, std::string& error, bool windows, Apply apply)
{
    Descriptor syntax;
    if (!classify(draft, syntax, error, windows)) return false;
    return apply(syntax);
}
// Only an explicit Enter/Load/Play requests transport, after a valid commit.
// The transport owns same-source coalescing and asynchronous admission.
template<class Apply, class Play> bool commitVideoAction(const std::string& draft,
    std::string& error, bool windows, bool start, Apply apply, Play play)
{
    if (!commitVideoDraft(draft, error, windows, apply)) return false;
    if (start && !draft.empty()) play();
    return true;
}
// Call only after reconciling the committed source with the runtime owner.
// Repeated explicit Play requests share an already playing/pending load.
inline bool beginVideoPlay(Transport& desired)
{
    if (desired == Transport::PLAYING) return false;
    desired = Transport::PLAYING;
    return true;
}
// Decide lifetime work without doing I/O or disturbing a playing lease.
inline bool clearCacheAtStartup(bool owner, bool keep, bool purge)
{ return owner && (!keep || purge); }
template<class Schedule> bool scheduleSessionCleanup(bool owner, bool keep, Schedule schedule)
{
    if (!owner || keep) return false;
    schedule(); // detached maintenance only; the caller never unlinks or waits
    return true;
}
inline bool deferCacheDeletion(unsigned leases)
{ return leases != 0; }

// Executed closures can still hold deny-delete validation handles. Destroy all
// captures before allowing a sweep, including when an item throws.
template<class Queue, class Run> void executeRetirements(Queue& garbage, Run run)
{
    for (auto& fn : garbage) run(fn);
    garbage.clear();
}
inline bool cacheMaintenanceDue(int64_t now, int64_t nextRetry, bool newClear, bool released)
{ return newClear || released || now >= nextRetry; }
inline bool finishShutdownMaintenance(bool keep, bool workersDrained, bool retirementPending, bool deletionDue)
{ return keep || (workersDrained && !retirementPending && !deletionDue); }
inline bool shutdownRetirementPending(bool garbage, bool closing, bool cachedLeases)
{ return garbage || (closing && cachedLeases); }
inline constexpr int64_t CACHE_OWNER_WAIT_MS = 10000;
inline constexpr int64_t CACHE_CLEAR_WAIT_MS = 10000;
inline int64_t clearFailureRetry(int64_t now, int64_t ordinaryRetry, bool pending, unsigned /*failures*/, bool shutdown)
{ return pending && !shutdown ? now + 250 : ordinaryRetry; }
inline bool acknowledgeStartupPurge(bool owner, bool pending, bool degraded, bool empty, bool covered)
{ return owner && !pending && covered && (degraded || empty); }

// This is the production drain loop, including beginShutdown's policy capture.
// Keep ON hands outstanding jobs to the existing post-persistence forced exit.
struct ShutdownDrain
{
    bool keep = false, draining = false;
    int64_t deadline = 0;
    void begin(bool keepBetweenSessions, int64_t now)
    { keep = keepBetweenSessions; deadline = now + 2000; }
    template<class Active, class Now, class Tick, class Wait>
    bool drain(const Active& active, Now now, Tick tick, Wait wait)
    {
        draining = true;
        if (keep) return active == 0;
        while (active && now() < deadline) { tick(); wait(); }
        return active == 0;
    }
};

// Used by validation's unadopted pin and normal lease retirement alike.
template<class Leases, class Released> void releaseCachePin(Leases& leases, Released& released)
{ --leases; ++released; }
template<class Entry> struct CacheValidationPin
{
    std::shared_ptr<Entry> entry;
    std::atomic<uint64_t>& released;
    bool adopted = false;
    ~CacheValidationPin() { if (!adopted) releaseCachePin(entry->leases, released); }
};

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

// Play may proceed after degradation while Clear completion still waits for
// a complete inventory, real deletion attempts and all retained leases.
template<class Pending, class Degraded, class Work, class Remaining, class Drop, class Covered>
void maintainCache(Pending& clearPending, Pending& cleanupPending, unsigned& failures,
    Degraded& degraded, Work sweep, Remaining remaining, Drop drop, Covered covered)
{
    try
    {
        sweep(clearPending && !degraded);
        if (clearPending) clearPending = remaining() || !covered();
        cleanupPending = false;
        failures = 0;
    }
    catch (...)
    {
        cleanupPending = true;
        if (!clearPending) throw;
        if (failures < 3) ++failures;
        if (failures >= 3 && !degraded) { drop(); degraded = true; }
        if (!degraded || !covered()) throw;
        cleanupPending = false;
        clearPending = false;
        failures = 0;
    }
}

template<class Pending, class Now, class Cancel, class Wait>
void awaitCacheClearWhile(Pending pending, int64_t deadline, Now now, Cancel cancel, Wait wait)
{
    while (pending())
    {
        cancel();
        if (deadlineExpired(deadline, now())) throw std::runtime_error(CLEAR_TIMEOUT);
        wait();
    }
}

template<class Pending, class Now, class Cancel, class Wait>
void awaitCacheClear(const Pending& pending, int64_t deadline, Now now, Cancel cancel, Wait wait)
{ awaitCacheClearWhile([&]() { return bool(pending); }, deadline, now, cancel, wait); }

// Picker/preflight results only commit a descriptor after successful validation.
template<class Apply> bool commitFileBrowse(std::string& status, const std::string& result,
    bool validated, bool current, Apply apply)
{ status = result; if (!validated || !current) return false; apply(); return true; }

// Restore the slot's own reason before reconciliation so an overlay can never
// become the next frame's underlying status.
struct BrowseOverlay
{
    std::string underlying, shown;
    void restore(std::string& reason)
    { if (!shown.empty() && reason == shown) reason = underlying; }
    void apply(std::string& status, bool, std::string& reason, int64_t, uint64_t = 0)
    {
        if (status.empty()) { *this = {}; return; }
        // Only an explicit slot action clears status; neither time nor a live
        // player's reconciliation can erase a refused Browse.
        underlying = reason; shown = status; reason = shown;
    }
};

// Unindexed bytes remain charged until unlink succeeds. Unknown enumeration
// results prohibit reservations until a complete later inventory succeeds.
struct CacheOrphans
{
    std::map<std::string,uint64_t> files;
    std::map<std::string,unsigned> failed; // only actual unsuccessful unlink attempts
    bool unknown = false;
    int64_t nextRetry = 0;
    std::string cursor;
    size_t unattempted() const
    {
        size_t count = 0;
        for (const auto& file : files) if (!failed.count(file.first)) ++count;
        return count;
    }
    bool covered() const
    {
        if (unknown || unattempted()) return false;
        for (const auto& file : files)
            if (failed.at(file.first) < 3) return false;
        return true;
    }
    // Preserve strikes only for children still present in a COMPLETE inventory.
    void inventoried(std::map<std::string,uint64_t> found)
    {
        files = std::move(found);
        for (auto it = failed.begin(); it != failed.end();)
            if (!files.count(it->first)) it = failed.erase(it); else ++it;
        unknown = false;
    }
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
            if (remove(it->first)) { failed.erase(it->first); files.erase(it); }
            else { auto& strikes = failed[it->first]; if (strikes < 3) ++strikes; }
        }
    }
    // Continue a quit sweep in bounded batches without retrying failed names.
    // A new release/explicit sweep starts a new attempt set; the ordinary retry
    // deadline still governs already attempted, undeletable files.
    template<class Remove> bool quitBatch(std::set<std::string>& attempted, Remove remove)
    {
        unsigned count = 0;
        for (auto it = files.begin(); it != files.end();)
        {
            if (attempted.count(it->first)) { ++it; continue; }
            if (count++ == 128) return true;
            attempted.insert(it->first);
            if (remove(it->first)) it = files.erase(it); else ++it;
        }
        return false;
    }
};
// One bounded line, with no paths, filenames or cache keys.
inline std::string cacheClearSummary(const CacheOrphans& orphans, size_t leased)
{
    std::ostringstream out;
    const auto unattempted = orphans.unattempted();
    out << "Clear degraded: failed=" << orphans.files.size() - unattempted
        << " unattempted=" << unattempted << " leased=" << leased
        << " inventory_unknown=" << orphans.unknown;
    size_t index = 0;
    for (const auto& file : orphans.files)
    {
        if (index == 3) break;
        const auto failed = orphans.failed.find(file.first);
        out << " example[" << ++index << "]=" << (failed == orphans.failed.end() ? "unattempted" : "failed")
            << " bytes=" << file.second;
    }
    return out.str();
}
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
inline bool matchingPrivateOwner(bool occupied, bool url, uint64_t generation, uint64_t owner, bool sameSource)
{ return occupied && url && generation == owner && sameSource; }
// Retirement can reconcile synchronously. Publish retention before destroying
// the child so that callback cannot mistake an idle release for a user Stop.
template<class Retire>
void releaseGatePrivate(bool& released, Transport& desired, Retire retire)
{ released = true; desired = Transport::PAUSED; retire(); }
inline bool gatePrivateRetainsImage(bool gate, bool released)
{ return gate && released; }
// Ownership handoff runs only after the decoder retirement receipt. Clear the
// retained-image bypass before resuming or creating a standalone player.
template<class Detach, class Resume>
void unclaimGatePrivate(bool& gate, bool& released, bool& retiring,
                        bool& play, Transport& desired, Detach detach, Resume resume)
{
    const bool requested = play;
    gate = released = retiring = play = false;
    desired = requested ? Transport::PLAYING : Transport::PAUSED;
    detach(); // resets the plugin envelope to one, restoring authored Volume
    if (requested) resume();
}
inline bool privatePlayerMayCreate(bool lease, bool player, Transport desired,
                                   bool closed, bool gate, bool gate_allowed)
{ return lease && !player && desired == Transport::PLAYING && closed && (!gate || gate_allowed); }
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
