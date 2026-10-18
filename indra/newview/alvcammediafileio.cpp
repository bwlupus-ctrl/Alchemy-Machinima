/** Private media I/O: detached, bounded, and independent of viewer teardown. */
#include "llviewerprecompiledheaders.h"
#include "alvcammediafileio.h"
#include "alvcammediafileposix.h"
#include "alvcammediafilecontainer.h"
#include "alvcammediafilelifecycle.h"
#include "alvcammediafilecache.h"
#include "llviewercontrol.h"
#include "httpcommon.h"
#include <openssl/sha.h>
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <array>
#include <set>
#include <exception>
#if LL_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#if LL_LINUX
#include <sys/vfs.h>
#else
#include <sys/mount.h>
#endif
#endif

namespace ALMediaFile
{
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
static int64_t millis() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
struct Job
{
    Policy policy;
    const char* rule = "request admission"; // worker-only diagnostic stage; no source strings
    unsigned long osError = 0; // captured at the failing OS call, worker only
#if LL_WINDOWS
    const std::error_category* osCategory = &std::system_category();
#else
    const std::error_category* osCategory = &std::generic_category();
#endif
    uint64_t id = 0, epoch = 0;
    std::atomic<unsigned> subscribers{0};
    std::atomic<bool> cancel{false}, done{false}, closed{false};
    bool cancellationLogged = false; // worker only; one terminal diagnostic
    bool reaped = false; // main only
    bool transferLease = false;
    bool cacheJob = false;
    bool maintenance = false;
    std::atomic<bool> awaitingApproval{false}, approved{false};
    std::atomic<int64_t> ioDeadline{0};
    std::atomic<int64_t> ownerDeadline{0}; // independent of transfer start and filesystem operations
    std::atomic<bool> ownerTimedOut{false};
    std::atomic<int64_t> clearDeadline{0};
    std::atomic<bool> clearTimedOut{false};
    std::atomic<uint64_t> bytes{0}, total{0};
    std::atomic<int64_t> started{0}; // queued time is not transfer time
    std::mutex mailbox;
    Result result;
#if LL_WINDOWS
    // Worker owns its native handle; main may request synchronous cancellation
    // under this tiny lock, which is never held across filesystem operations.
    std::mutex nativeMutex;
    HANDLE native = nullptr;
#endif
};
static thread_local Job* sWorkerJob = nullptr;
struct Entry
{
    std::string key, name, ext, mime;
    uint64_t id = 0, size = 0, lru = 0;
    std::atomic<unsigned> leases{0};
};
struct File;
struct Service
{
    Policy policy;
    Policy nextPolicy; // main only; copied into each detached job
    bool started = false, accepting = true; // main only
    ShutdownDrain shutdown; // main only; captures Keep at beginShutdown
    uint64_t nextId = 1;
    std::atomic<uint64_t> epoch{1};
    std::atomic<unsigned> active{0};
    std::atomic<int> cacheState{0}; // 0 checking, 1 owner, -1 unavailable
    std::atomic<CacheFailure> cacheFailure{CacheFailure::IO};
    std::atomic<bool> cacheRetryRequested{false};
    std::shared_ptr<File> cachePins; // process-lifetime ownership, retained across recovery retries
    std::atomic<bool> clearPending{false};
    std::atomic<bool> cleanupPending{false};
    std::atomic<bool> shutdownClear{false}, drainRequested{false};
    std::atomic<bool> purgeCompleted{false};
    LLPointer<LLControlVariable> purgeSetting; // resolved once on main during initialization
    std::atomic<uint64_t> released{0}; // new work, not a repeating quit retry trigger
    unsigned clearFailures = 0; // maintenance worker only
    std::atomic<bool> clearDegraded{false};
    std::atomic<uint64_t> completeBytes{0}, reservedBytes{0}, limitBytes{0};
    std::vector<std::shared_ptr<Job>> jobs; // main only; includes cancelled jobs
    std::map<std::string, std::weak_ptr<Job>> inflight; // main only
    std::mutex cacheMutex; // workers only; main NEVER acquires this
    std::mutex transferMutex; // serialize misses without blocking cache hits
    std::map<std::string, std::shared_ptr<Entry>> entries;
    std::vector<std::shared_ptr<Entry>> evicted; // unindexed but still leased
    std::set<std::string> activeFiles; // protected from maintenance sweeps
    CacheOrphans orphans; // cacheMutex; survives degraded Clear
    std::set<std::string> shutdownAttempted; // worker only; failed names wait for backoff/new releases
    bool shutdownBatchPending = false; // unattempted work, not retries of undeletable files
    uint64_t sweptClearEpoch = 0; // worker only; explicit Clear permits one immediate retry
    uint64_t lruClock = 0; // serialized, persisted order; independent of reboot clocks
    std::mutex garbageMutex;
    std::deque<std::function<void()>> garbage;
    std::vector<DeferredClose> closing; // under garbageMutex; polled by maintenance only
    std::atomic<bool> maintenanceFailed{false};
};
// Deliberately process-lifetime. No CRT/static destruction below blocked jobs.
static Service* sService = nullptr; // main initializes before any worker starts
static Service& service() { if (!sService) sService = new Service; return *sService; }
static void cancelled(Job& job)
{
    const auto now = millis();
    if (job.cancel.load() || totalExpired(job.maintenance, job.started.load(), now) || ioExpired(job.ioDeadline, now))
    {
        if (!job.maintenance && !job.cancellationLogged)
        {
            job.cancellationLogged = true;
            LL_INFOS("VCamMedia") << "I/O refused job=" << job.id << " stage=" << job.rule << " reason=" <<
                (ioExpired(job.ioDeadline, now) ? "I/O deadline expired" : totalExpired(job.maintenance, job.started.load(), now) ? "total transfer deadline expired" : "cancelled") << LL_ENDL;
        }
        throw std::runtime_error(FAILED);
    }
}
struct Operation
{
    Job& job;
    int64_t previous;
    explicit Operation(Job& j) : job(j), previous(j.ioDeadline.load())
    { cancelled(job); const auto deadline = millis() + 5000; job.ioDeadline = previous ? std::min(previous, deadline) : deadline; }
    ~Operation() noexcept(false)
    {
        const auto deadline = job.ioDeadline.load();
        const bool expired = deadlineExpired(deadline, millis());
        job.ioDeadline = previous;
        if (finishOperation(job.maintenance, expired, std::uncaught_exceptions() != 0)) job.cancel = true;
    }
};
static void cancelJob(Job& job)
{
    job.cancel = true;
#if LL_WINDOWS
    std::unique_lock<std::mutex> lock(job.nativeMutex, std::try_to_lock);
    if (lock && job.native) CancelSynchronousIo(job.native);
#endif
}
static void publish(Job& job, const std::string& status, std::shared_ptr<Lease> lease = {})
{
    std::lock_guard<std::mutex> lock(job.mailbox);
    if (job.closed || job.cancel || (job.cacheJob && job.epoch != service().epoch.load())) return;
    job.result = {job.id, job.epoch, job.bytes.load(), job.total.load(), true, status, std::move(lease)};
}
static std::string failureStatus(const std::exception& error)
{
    const auto* system = dynamic_cast<const std::system_error*>(&error);
    if (system && system->code() == std::errc::no_space_on_device) return NO_SPACE;
    const std::string reason(error.what());
    if (reason.rfind(CACHE_UNAVAILABLE, 0) == 0) return reason;
    for (const char* known : {UNSUPPORTED, REMOTE, NOT_VIDEO, MISSING, LOCKED, BUDGET, TOO_LARGE, NO_SPACE, CLEAR_TIMEOUT, FAILED})
        if (reason == known || (known == FAILED && reason.rfind(FAILED, 0) == 0)) return reason;
    return std::string(FAILED) + " (" + reason + ")";
}
static void retire(std::function<void()> fn)
{
    auto& s = service();
    std::lock_guard<std::mutex> lock(s.garbageMutex);
    s.garbage.push_back(std::move(fn));
}
void retireAfterClose(std::shared_ptr<void> retention, std::shared_ptr<LLPluginFileClose> fence)
{
    if (!retention) return;
    auto& s = service();
    std::lock_guard<std::mutex> lock(s.garbageMutex);
    s.closing.emplace_back(std::move(retention), std::move(fence));
}
template<class T> static std::shared_ptr<T> isolated(T* value)
{
    return std::shared_ptr<T>(value, [](T* p)
    {
        // Failed validation already runs on an isolated worker. Close its
        // handle here so eviction/redownload does not race deferred close.
        if (sWorkerJob) delete p;
        else retire([p]() { delete p; });
    });
}
struct File
{
#if LL_WINDOWS
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::vector<HANDLE> directories;
#else
    int handle = -1, directory = -1;
#endif
    uint64_t size = 0;
    ~File()
    {
        // Destruction runs only on an isolated worker. Even failure/cancel
        // cleanup gets an observable deadline without throwing from a dtor.
        const auto previous = sWorkerJob ? sWorkerJob->ioDeadline.exchange(millis() + 5000) : 0;
#if LL_WINDOWS
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
        for (HANDLE h : directories) CloseHandle(h);
#else
        if (handle >= 0) close(handle);
        if (directory >= 0) close(directory);
#endif
        if (sWorkerJob) sWorkerJob->ioDeadline = previous;
    }
};
#if LL_WINDOWS
static std::wstring wide(const std::string& text)
{
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n) throw std::runtime_error(REMOTE);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}
static void localDirectories(Job& job, const std::string& path, File& file, bool create)
{
    job.rule = "local path spelling/absolute drive";
    // This lexical refusal precedes GetDriveType, CreateFile, stat, canonical,
    // and every other filesystem API, including for a configured cache root.
    if (networkSpelling(path) || path.size() < 3 || path[1] != ':' ||
        (path[2] != '/' && path[2] != '\\')) throw std::runtime_error(REMOTE);
    const auto native = wide(path);
    {
        Operation op(job);
        job.rule = "local drive type (fixed/removable/RAM/CDROM only)";
        const auto type = GetDriveTypeW(native.substr(0, 3).c_str());
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE && type != DRIVE_RAMDISK && type != DRIVE_CDROM) throw std::runtime_error(REMOTE);
    }
    for (size_t p = 3; p < native.size(); ++p)
    {
        if (native[p] != '/' && native[p] != '\\') continue;
        const auto part = native.substr(0, p);
        Operation op(job);
        if (create && !CreateDirectoryW(part.c_str(), nullptr))
        {
            const auto error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) { job.osError = error; throw std::runtime_error(FAILED); }
        }
        job.rule = "local directory open (no reparse points)";
        HANDLE h = CreateFileW(part.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            const auto error = GetLastError();
            job.osError = error;
            throw std::runtime_error(error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND ? MISSING : REMOTE);
        }
        file.directories.push_back(h); // deny directory rename until playback ends
        job.rule = "local directory reparse-point refusal";
        FILE_ATTRIBUTE_TAG_INFO info{};
        if (!GetFileInformationByHandleEx(h, FileAttributeTagInfo, &info, sizeof(info)))
        { job.osError = GetLastError(); throw std::runtime_error(REMOTE); }
        if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error(REMOTE);
    }
}
static std::shared_ptr<File> openLocal(Job& job, const std::string& path)
{
    auto file = isolated(new File);
    localDirectories(job, path, *file, false);
    Operation op(job);
    job.rule = "local file open (read-only/no-follow)";
    file->handle = CreateFileW(wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OVERLAPPED, nullptr);
    if (file->handle == INVALID_HANDLE_VALUE)
    {
        const auto err = GetLastError();
        throw std::runtime_error(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ? MISSING : FAILED);
    }
    job.rule = "local file attributes/reparse-point refusal";
    FILE_ATTRIBUTE_TAG_INFO info{};
    LARGE_INTEGER size{};
    if (!GetFileInformationByHandleEx(file->handle, FileAttributeTagInfo, &info, sizeof(info))) throw std::runtime_error(REMOTE);
    if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error(REMOTE);
    job.rule = "local file must be nonempty and not a directory";
    if ((info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        !GetFileSizeEx(file->handle, &size) || size.QuadPart <= 0) throw std::runtime_error(NOT_VIDEO);
    file->size = static_cast<uint64_t>(size.QuadPart);
    return file;
}
static void transfer(Job& job, File& file, void* bytes, size_t count, uint64_t offset, bool write)
{
    Operation op(job);
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(offset); ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) throw std::runtime_error(FAILED);
    DWORD done = 0;
    BOOL ok = write ? WriteFile(file.handle, bytes, static_cast<DWORD>(count), &done, &ov) : ReadFile(file.handle, bytes, static_cast<DWORD>(count), &done, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING)
    {
        while (!job.cancel && millis() < job.ioDeadline && WaitForSingleObject(ov.hEvent, 25) == WAIT_TIMEOUT) {}
        if (job.cancel || millis() >= job.ioDeadline) CancelIoEx(file.handle, &ov);
        // Worker alone waits for cancellation completion so OVERLAPPED/buffer
        // remains alive. Main has already terminalized the subscription.
        ok = GetOverlappedResult(file.handle, &ov, &done, TRUE);
    }
    const auto err = GetLastError();
    CloseHandle(ov.hEvent);
    if (!ok || done != count) throw std::runtime_error(err == ERROR_DISK_FULL || err == ERROR_HANDLE_DISK_FULL ? NO_SPACE : FAILED);
    cancelled(job);
}
#else
static std::shared_ptr<File> openLocal(Job& job, const std::string& path)
{
    Operation op(job);
    auto file = isolated(new File);
    job.rule = "POSIX local mount/no-follow/regular-file policy";
    file->handle = localOpen(path);
    struct stat st{};
    if (fstat(file->handle, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0) throw std::runtime_error(NOT_VIDEO);
    file->size = static_cast<uint64_t>(st.st_size);
    return file;
}
static void transfer(Job& job, File& file, void* bytes, size_t count, uint64_t offset, bool write)
{
    Operation op(job);
    const auto n = write ? pwrite(file.handle, bytes, count, offset) : pread(file.handle, bytes, count, offset);
    if (n < 0 || size_t(n) != count) throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
    cancelled(job);
}
#endif
// Structural container walk checks reference metadata, including a moov after
// mdat. Encoded frame payload stays opaque; it cannot create false markers.
static void preflight(Job& job, File& file, const std::string& ext)
{
    LL_INFOS("VCamMedia") << "Container preflight start job=" << job.id << " size=" << file.size
        << " ext=" << diagnosticExtension("file." + ext) << LL_ENDL;
    try
    {
        job.rule = "container header read";
        std::array<unsigned char, 64> header{};
        const auto n = static_cast<size_t>(std::min<uint64_t>(64, file.size));
        transfer(job, file, header.data(), n, 0, false);
        if (!sniff(ext, header.data(), n, file.size)) throw PolicyRefusal("header/extension signature mismatch");
        job.rule = "container structural walk/read";
        checkContainer([&job, &file](uint64_t offset, void* bytes, size_t count)
        { transfer(job, file, bytes, count, offset, false); }, file.size, ext);
        LL_INFOS("VCamMedia") << "Container preflight finish job=" << job.id << " verdict=ADMITTED rule=signature/structure"
            << " size=" << file.size << " ext=" << diagnosticExtension("file." + ext) << LL_ENDL;
    }
    catch (const std::exception& error)
    {
        LL_INFOS("VCamMedia") << "Container preflight finish job=" << job.id << " verdict=REFUSED stage=" << job.rule
            << " reason=" << diagnosticReason(error) << " size=" << file.size
            << " ext=" << diagnosticExtension("file." + ext) << LL_ENDL;
        throw;
    }
}

static fs::path root()
{
#if LL_WINDOWS
    return fs::path(wide(service().policy.cacheRoot));
#else
    return fs::path(service().policy.cacheRoot);
#endif
}
static std::string pathText(const fs::path& path)
{
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}
static void index(Job& job)
{
    auto& s = service();
    Operation op(job);
    job.rule = "cache index temporary cleanup";
    removeCacheIndexTemporary(root());
    job.rule = "cache index write";
    const auto temporary = root() / "index.tmp";
    std::ofstream out(temporary, std::ios::trunc | std::ios::binary);
    const auto usage = cacheUsage(s.entries, s.evicted, s.orphans);
    for (const auto& pair : s.entries)
    {
        const auto& e = *pair.second;
        out << e.key << ' ' << e.id << ' ' << e.ext << ' ' << e.mime << ' ' << e.size << ' ' << e.lru << '\n';
    }
    out.flush();
    if (!out)
    {
#if LL_WINDOWS
        job.osError = GetLastError();
#else
        job.osError = errno;
#endif
        throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
    }
    out.close();
    if (!out)
    {
#if LL_WINDOWS
        job.osError = GetLastError();
#else
        job.osError = errno;
#endif
        throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
    }
    job.rule = "cache index replace";
    replaceCacheIndex(root());
    s.completeBytes = usage;
    cancelled(job);
}
static void eraseFile(Job& job, const fs::path& path)
{
    Operation op(job);
    job.rule = "cache sweep unlink";
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) throw std::system_error(ec);
}
static void sweep(Job& job, bool clear)
{
    auto& s = service();
    const auto now = millis();
    const auto epoch = s.epoch.load();
    if (clear && epoch != s.sweptClearEpoch)
    {
        s.sweptClearEpoch = epoch;
        s.orphans.nextRetry = 0;
        s.orphans.unknown = true;
        s.orphans.failed.clear();
        s.clearFailures = 0;
    }
    // Cleanup requests can arrive every frame; disk retries stay bounded.
    const bool inventory = now >= s.orphans.nextRetry;
    if (!inventory && s.orphans.unknown) throw std::runtime_error(FAILED);
    std::vector<std::string> keep{"cache.lock", "index"};
    keep.insert(keep.end(), s.activeFiles.begin(), s.activeFiles.end());
    for (auto it = s.entries.begin(); it != s.entries.end();)
        if (clear)
        { s.evicted.push_back(it->second); it = s.entries.erase(it); }
        else { keep.push_back(it->second->name); ++it; }
    for (auto it = s.evicted.begin(); it != s.evicted.end();)
        if (deferCacheDeletion((*it)->leases.load())) { keep.push_back((*it)->name); ++it; }
        else
        {
            s.orphans.files[(*it)->name] = (*it)->size;
            it = s.evicted.erase(it);
        }
    if (inventory)
    {
        // Set unknown before any fallible enumeration/size query. A partial
        // inventory must never permit new bytes under an understated total.
        s.orphans.unknown = true;
        s.completeBytes = UINT64_MAX;
        s.orphans.nextRetry = now + 60000;
        Operation op(job);
        job.rule = "cache sweep inventory";
        std::map<std::string,uint64_t> found;
        for (const auto& file : fs::directory_iterator(root()))
        {
            cancelled(job);
            const auto name = pathText(file.path().filename());
            if (std::find(keep.begin(), keep.end(), name) == keep.end())
                // Never follow an orphan symlink/reparse target for sizing.
                // Unknown object sizes consume all capacity until removed.
                found[name] = fs::is_regular_file(file.symlink_status()) ? file.file_size() : UINT64_MAX;
        }
        s.orphans.inventoried(std::move(found));
        if (!s.shutdownClear)
        {
            s.orphans.nextRetry = now;
            s.orphans.retry(now, [&](const std::string& name)
            { return maintenanceItem([&]() { eraseFile(job, root()/name); }, []() {}); });
        }
    }
    if (s.shutdownClear)
    {
        if (inventory) s.shutdownAttempted.clear();
        s.orphans.nextRetry = now + 60000;
        s.shutdownBatchPending = s.orphans.quitBatch(s.shutdownAttempted, [&](const std::string& name)
        { return maintenanceItem([&]() { eraseFile(job, root()/name); }, []() {}); });
    }
    index(job);
    if (clear && !s.orphans.failed.empty()) throw std::runtime_error(FAILED);
    cancelled(job);
}
static void ownCache(Job& job)
{
    auto& s = service();
    if (!s.cachePins)
    {
        // A failed acquisition releases its handle/pins before the next Play.
        // A successful lock survives metadata failures and their bounded retry.
        auto pins = isolated(new File);
#if LL_WINDOWS
        localDirectories(job, s.policy.cacheRoot + "/cache.lock", *pins, true);
        job.rule = "cache lock acquire";
        for (unsigned attempt = 1; attempt <= 5; ++attempt)
        {
            Operation op(job);
            pins->handle = CreateFileW((root() / "cache.lock").c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (pins->handle != INVALID_HANDLE_VALUE) break;
            job.osError = GetLastError();
            const auto delay = lockRetryMs(attempt, job.osError == ERROR_SHARING_VIOLATION);
            if (!delay) throw std::runtime_error(job.osError == ERROR_SHARING_VIOLATION ? LOCKED : FAILED);
            for (unsigned ms = 0; ms < delay && !job.cancel; ms += 25) std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        job.osError = 0;
        job.rule = "cache lock validate";
        FILE_ATTRIBUTE_TAG_INFO info{};
        if (!GetFileInformationByHandleEx(pins->handle, FileAttributeTagInfo, &info, sizeof(info)))
        { job.osError = GetLastError(); throw std::runtime_error(REMOTE); }
        if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error(REMOTE);
#else
        job.rule = "cache local directory/lock open";
        {
            Operation op(job);
            pins->directory = localOpen(s.policy.cacheRoot, true, true);
            pins->handle = openat(pins->directory, "cache.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
            const int error = errno;
            if (pins->handle < 0) { job.osError = error; throw std::runtime_error(REMOTE); }
            struct stat info{};
            if (fstat(pins->handle, &info)) { job.osError = errno; throw std::runtime_error(REMOTE); }
            if (!S_ISREG(info.st_mode)) throw std::runtime_error(REMOTE);
        }
        job.rule = "cache lock acquire";
        for (unsigned attempt = 1; attempt <= 5; ++attempt)
        {
            Operation op(job);
            if (!flock(pins->handle, LOCK_EX | LOCK_NB)) break;
            job.osError = errno;
            const auto delay = lockRetryMs(attempt, job.osError == EWOULDBLOCK || job.osError == EAGAIN);
            if (!delay) throw std::runtime_error(LOCKED);
            for (unsigned ms = 0; ms < delay && !job.cancel; ms += 25) std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
#endif
        s.cachePins = std::move(pins);
    }
    bool recovered = false;
    {
        Operation op(job);
        job.osError = 0;
        job.rule = "cache index read";
        const auto metadata = readCacheIndex(root() / "index"); // reader CLOSED on return
        recovered = metadata.recovered;
        decltype(s.entries) entries;
        uint64_t clock = 0;
        for (const auto& record : metadata.entries)
        {
            auto e = std::make_shared<Entry>();
            e->key = record.key; e->id = record.id; e->ext = record.ext;
            e->mime = record.mime; e->size = record.size; e->lru = record.lru;
            e->name = e->key + "." + std::to_string(e->id) + "." + e->ext;
            entries[e->key] = e;
            clock = std::max(clock, e->lru);
        }
        s.entries = std::move(entries);
        s.lruClock = clock;
        job.rule = "cache index temporary cleanup";
        removeCacheIndexTemporary(root());
        // Retry must inventory again even if a prior sweep failed after enumeration.
        s.orphans.nextRetry = 0;
        if (clearCacheAtStartup(true, s.policy.keepBetweenSessions, s.policy.purgeOnStartup))
        { s.orphans.unknown = true; s.clearPending = true; }
        job.rule = "cache startup sweep";
        if (!s.clearPending) sweep(job, false);
    } // Complete the deadline check before publishing ownership to download workers.
    s.cacheState = 1;
    LL_INFOS("VCamMedia") << "ownCache outcome=owner stage=" << job.rule
        << " os_error=0 category=" << job.osCategory->name()
        << " metadata=" << (recovered ? "rebuild scheduled/completed" : "valid") << LL_ENDL;
}
static void tryOwnCache(Job& job)
{
    auto& s = service();
    job.osError = 0;
#if LL_WINDOWS
    job.osCategory = &std::system_category();
#else
    job.osCategory = &std::generic_category();
#endif
    try { std::lock_guard<std::mutex> lock(s.cacheMutex); ownCache(job); return; }
    catch (const std::exception& error)
    {
        const auto* system = dynamic_cast<const std::system_error*>(&error);
        if (system) { job.osError = system->code().value(); job.osCategory = &system->code().category(); }
        const std::string reason = error.what();
        CacheFailure failure = reason == LOCKED ? CacheFailure::IN_USE :
            reason == NO_SPACE ? CacheFailure::NO_SPACE : reason == REMOTE ? CacheFailure::LOCAL_ONLY : CacheFailure::IO;
        failure = cacheFailureFromError(std::error_code(static_cast<int>(job.osError), *job.osCategory), failure);
        s.cacheFailure = failure;
    }
    catch (...) { s.cacheFailure = CacheFailure::IO; }
    LL_INFOS("VCamMedia") << "ownCache outcome=unavailable stage=" << job.rule
        << " os_error=" << job.osError << " category=" << job.osCategory->name()
        << " reason=" << cacheFailureStatus(s.cacheFailure.load()) << LL_ENDL;
    s.cacheState = -1;
}
static void capacity(Job& job, uint64_t reservation)
{
    auto& s = service();
    std::lock_guard<std::mutex> lock(s.cacheMutex);
    auto used = cacheUsage(s.entries, s.evicted, s.orphans);
    if (s.orphans.unknown) throw std::runtime_error(NO_SPACE);
    while (!cacheFits(used, reservation, job.policy.cacheBytes))
    {
        auto oldest = s.entries.end();
        for (auto it = s.entries.begin(); it != s.entries.end(); ++it)
            if (!it->second->leases && (oldest == s.entries.end() || it->second->lru < oldest->second->lru)) oldest = it;
        if (oldest == s.entries.end()) throw std::runtime_error(NO_SPACE);
        eraseFile(job, root() / oldest->second->name);
        used -= oldest->second->size;
        s.entries.erase(oldest);
    }
    index(job);
    s.reservedBytes = reservation;
}
// Caller pins the entry under cacheMutex before dropping it for validation.
static std::shared_ptr<Lease> cacheLease(Job& job, const std::shared_ptr<Entry>& entry)
{
    CacheValidationPin<Entry> pin{entry, service().released};
    auto file = openLocal(job, pathText(root() / entry->name));
    if (file->size != entry->size) throw std::runtime_error(NOT_VIDEO);
    preflight(job, *file, entry->ext);
    auto lease = std::make_shared<Lease>();
    lease->cached = true; lease->path = pathText(root() / entry->name); lease->extension = entry->ext;
    lease->retention = std::shared_ptr<void>(new int(0), [entry, file](void* p) { delete static_cast<int*>(p); retire([entry, file]() { releaseCachePin(entry->leases, service().released); service().cleanupPending = true; }); });
    pin.adopted = true;
    return lease;
}
static bool cached(Job& job, const std::string& key)
{
    auto& s = service();
    std::shared_ptr<Entry> entry;
    // Keep one deadline across Clear/cacheMutex races; main can terminalize
    // even if maintenance or this worker is blocked inside the filesystem.
    job.clearDeadline = millis() + CACHE_CLEAR_WAIT_MS;
    {
        // Clear may be requested between the wait and taking cacheMutex.
        // Retry without holding the mutex the maintenance sweep needs.
        std::unique_lock<std::mutex> lock(s.cacheMutex, std::defer_lock);
        for (;;)
        {
            awaitCacheClearWhile([&]() { return s.clearPending && !s.clearDegraded; }, job.clearDeadline.load(), millis, [&]() { cancelled(job); }, []()
            { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
            lock.lock();
            if (!s.clearPending || s.clearDegraded) break;
            lock.unlock();
        }
        cancelled(job);
        if (deadlineExpired(job.clearDeadline.load(), millis()))
        { job.clearTimedOut = true; throw std::runtime_error(CLEAR_TIMEOUT); }
        job.clearDeadline = 0;
        const auto found = s.entries.find(key);
        if (found == s.entries.end()) return false;
        entry = found->second;
        ++entry->leases;
    }
    return validatedCacheHit([&]() { return cacheLease(job, entry); },
        [&](std::shared_ptr<Lease> lease)
    {
        {
            std::lock_guard<std::mutex> lock(s.cacheMutex);
            if (s.lruClock == UINT64_MAX) throw std::runtime_error(FAILED);
            entry->lru = ++s.lruClock;
            index(job);
        }
        publish(job, "PENDING, MUTED", std::move(lease));
    }, [&]()
    {
        std::lock_guard<std::mutex> lock(s.cacheMutex);
        const auto found = s.entries.find(key);
        if (found != s.entries.end() && found->second == entry)
        {
            // Remove the record even if an old player still leases its file.
            // Sweep retries unlink after that lease has actually quiesced.
            s.evicted.push_back(entry);
            s.cleanupPending = true;
            ++s.released;
            s.entries.erase(found);
            index(job);
        }
    });
}
struct Download
{
    Job& job;
    File file;
    std::string url, location, mime, encoding, error;
    long status = 0;
    uint64_t declared = 0;
    bool hasLength = false, approved = false;
    explicit Download(Job& j) : job(j) {}
};
static size_t headers(char* data, size_t unit, size_t count, void* opaque)
{
    auto& d = *static_cast<Download*>(opaque);
    try
    {
        cancelled(d.job);
        if (unit && count > SIZE_MAX / unit) return 0;
        const size_t n = unit * count;
        if (n > 16384) return 0;
        std::string line(data, n);
        if (line.rfind("HTTP/", 0) == 0)
        {
            const auto space = line.find(' ');
            d.status = space == std::string::npos ? 0 : std::strtol(line.c_str() + space + 1, nullptr, 10);
            d.mime.clear(); d.encoding.clear(); d.location.clear(); d.hasLength = false; d.declared = 0; d.approved = false;
        }
        else if (line == "\r\n")
        {
            if (d.status >= 100 && d.status < 200) return n;
            if (d.status != 200) return 0; // headers only; no redirect/error body
            Url url;
            d.job.rule = "HTTP final URL/extension/MIME/content-encoding";
            if (!httpUrl(d.url, url) || extension(url.path).empty() || !mimeAllowed(d.mime) || (!d.encoding.empty() && lower(d.encoding) != "identity")) throw std::runtime_error(NOT_VIDEO);
            d.job.rule = "HTTP Content-Length cap";
            if (d.hasLength && d.declared > d.job.policy.downloadBytes) throw std::runtime_error(TOO_LARGE);
            d.job.rule = "cache capacity reservation/free-space";
            capacity(d.job, d.hasLength ? d.declared : d.job.policy.downloadBytes);
            d.job.total = d.declared;
            d.approved = true;
        }
        else
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos) return n;
            const auto name = lower(line.substr(0, colon));
            auto value = line.substr(colon+1);
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();
            while (!value.empty() && value.front() == ' ') value.erase(0, 1);
            if (name == "location") d.location = value;
            else if (name == "content-type") d.mime = lower(value.substr(0, value.find(';')));
            else if (name == "content-encoding") d.encoding = value;
            else if (name == "content-length")
            {
                if (value.empty() || value.size() > 20 || value.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error(FAILED);
                const auto length = std::stoull(value);
                if (d.hasLength && d.declared != length) throw std::runtime_error(FAILED);
                d.declared = length; d.hasLength = true;
            }
        }
        return n;
    }
    catch (const std::exception& e) { d.error = e.what(); return 0; }
}
static size_t body(char* data, size_t unit, size_t count, void* opaque)
{
    auto& d = *static_cast<Download*>(opaque);
    try
    {
        cancelled(d.job);
        if (!d.approved || (unit && count > SIZE_MAX / unit)) return 0;
        const size_t n = unit * count;
        const auto received = d.job.bytes.load();
        d.job.rule = "download streaming byte cap";
        if (n > d.job.policy.downloadBytes - std::min(received, d.job.policy.downloadBytes)) throw std::runtime_error(TOO_LARGE);
        d.job.rule = "download declared-length bound";
        if (d.hasLength && n > d.declared - std::min(received, d.declared)) throw std::runtime_error(FAILED);
        for (size_t offset = 0; offset < n;)
        {
            const auto chunk = std::min<size_t>(65536, n - offset);
            transfer(d.job, d.file, data + offset, chunk, received + offset, true);
            offset += chunk;
        }
        d.job.bytes += n;
        return n;
    }
    catch (const std::exception& e) { d.error = e.what(); return 0; }
}
static int progress(void* opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    auto& job = *static_cast<Job*>(opaque);
    return job.cancel || totalExpired(false, job.started.load(), millis()) ? 1 : 0;
}
static void download(Job& job, const Descriptor& source)
{
    auto& s = service();
    job.rule = "download policy allowDownloads";
    if (!job.policy.allowDownloads) throw std::runtime_error(std::string(FAILED) + " (" + job.policy.denialReason + ")");
    const auto key = cacheKey(source.value);
    if (cached(job, key)) return;
    std::unique_lock<std::mutex> transferLock(s.transferMutex, std::defer_lock);
    while (!transferLock.try_lock()) { cancelled(job); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    job.started = millis(); // the queued request has now become a transfer
    if (cached(job, key)) return;
    struct Reservation { ~Reservation() { service().reservedBytes = 0; } } reservation;
    {
        std::lock_guard<std::mutex> lock(s.cacheMutex);
        if (s.cleanupPending) { sweep(job, false); s.cleanupPending = false; }
    }
    // Only index, reservations and lease bookkeeping hold cacheMutex.
    const std::string stem = key + "." + std::to_string(job.id);
    fs::path partial = root() / (stem + ".part"), completed;
    {
        std::lock_guard<std::mutex> lock(s.cacheMutex);
        s.activeFiles.insert(pathText(partial.filename()));
    }
    struct ActiveFiles
    {
        std::string stem;
        Job& job;
        std::string name;
        bool removed = false;
        ~ActiveFiles()
        {
            auto& s = service();
            std::lock_guard<std::mutex> lock(s.cacheMutex);
            for (auto it = s.activeFiles.begin(); it != s.activeFiles.end();)
                if (it->rfind(stem + ".", 0) == 0) it = s.activeFiles.erase(it); else ++it;
            // A partial write can fail before the progress counter advances.
            // Retain its reservation until inventory/unlink proves the size.
            bool tracked = false;
            for (const auto& pair : s.entries) tracked |= pair.second->name == name;
            for (const auto& entry : s.evicted) tracked |= entry->name == name;
            if (!tracked && !removed) s.orphans.abandon(name, job.bytes.load(), s.reservedBytes.load());
            s.completeBytes = cacheUsage(s.entries, s.evicted, s.orphans);
            s.cleanupPending = true;
            ++s.released;
        }
    } activeFiles{stem, job, pathText(partial.filename())};
    Download d(job);
    d.url = source.value.substr(0, source.value.find('#'));
    bool promoted = false, indexed = false;
#if LL_WINDOWS
    bool disposition = false;
#endif
    try
    {
        {
            Operation op(job);
#if LL_WINDOWS
            d.file.handle = CreateFileW(partial.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_FLAG_OVERLAPPED | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (d.file.handle == INVALID_HANDLE_VALUE) throw std::runtime_error(GetLastError() == ERROR_DISK_FULL ? NO_SPACE : FAILED);
            FILE_DISPOSITION_INFO_EX info{FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE};
            disposition = SetFileInformationByHandle(d.file.handle, FileDispositionInfoEx, &info, sizeof(info)) != FALSE;
            if (!disposition && GetLastError() != ERROR_INVALID_PARAMETER && GetLastError() != ERROR_NOT_SUPPORTED && GetLastError() != ERROR_INVALID_FUNCTION) throw std::runtime_error(FAILED);
#else
            d.file.handle = open(partial.c_str(), O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW, 0600);
            if (d.file.handle < 0) throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
#endif
        }
        for (unsigned hop = 0;; ++hop)
        {
            cancelled(job);
            Url approved;
            if (!httpUrl(d.url, approved)) throw std::runtime_error(UNSUPPORTED);
            CURL* handle = curl_easy_init(); // raw, job-owned, per-hop settings
            if (!handle) throw std::runtime_error(FAILED);
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(handle, curl_easy_cleanup);
            auto option = [handle](CURLoption name, auto value) { if (curl_easy_setopt(handle, name, value) != CURLE_OK) throw std::runtime_error(FAILED); };
            option(CURLOPT_NOSIGNAL, 1L);
            option(CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
            option(CURLOPT_FOLLOWLOCATION, 0L);
            option(CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
            option(CURLOPT_REDIR_PROTOCOLS, long(CURLPROTO_HTTP | CURLPROTO_HTTPS));
            option(CURLOPT_SSL_VERIFYPEER, 1L); option(CURLOPT_SSL_VERIFYHOST, 2L);
            option(CURLOPT_CAINFO, job.policy.caFile.c_str());
            option(CURLOPT_PROXY, job.policy.proxy.c_str()); // empty disables ambient proxies
            option(CURLOPT_NETRC, long(CURL_NETRC_IGNORED));
            option(CURLOPT_HTTPAUTH, long(CURLAUTH_NONE));
            option(CURLOPT_ACCEPT_ENCODING, "identity"); option(CURLOPT_HTTP_CONTENT_DECODING, 0L);
            option(CURLOPT_CONNECTTIMEOUT, 30L); option(CURLOPT_LOW_SPEED_LIMIT, 1L); option(CURLOPT_LOW_SPEED_TIME, 30L);
            option(CURLOPT_TIMEOUT_MS, long(std::max<int64_t>(1, 30 * 60 * 1000 - (millis() - job.started.load()))));
            option(CURLOPT_URL, d.url.c_str());
            option(CURLOPT_HEADERFUNCTION, headers); option(CURLOPT_HEADERDATA, &d);
            option(CURLOPT_WRITEFUNCTION, body); option(CURLOPT_WRITEDATA, &d);
            option(CURLOPT_NOPROGRESS, 0L); option(CURLOPT_XFERINFOFUNCTION, progress); option(CURLOPT_XFERINFODATA, &job);
            d.status = 0; d.error.clear();
            const auto code = curl_easy_perform(handle);
            cancelled(job);
            if (d.status == 301 || d.status == 302 || d.status == 303 || d.status == 307 || d.status == 308)
            {
                std::string next;
                job.rule = "HTTP redirect target/scheme/hop limit";
                if (!redirect(d.url, d.location, hop, next)) throw std::runtime_error(FAILED);
                d.url = next;
                continue;
            }
            if (!d.error.empty()) throw std::runtime_error(d.error);
            if (d.status != 200)
            {
                LL_INFOS("VCamMedia") << "Download response refused job=" << job.id << " rule=HTTP 200 required status=" << d.status << LL_ENDL;
                throw std::runtime_error(std::string(FAILED) + " (HTTP " + std::to_string(d.status) + ")");
            }
            job.rule = "curl completion/nonempty/exact Content-Length";
            if (code != CURLE_OK || !d.approved || !job.bytes || (d.hasLength && job.bytes != d.declared)) throw std::runtime_error(FAILED);
            break;
        }
        Url finalUrl; if (!httpUrl(d.url, finalUrl)) throw std::runtime_error(FAILED);
        const auto ext = extension(finalUrl.path);
        d.file.size = job.bytes;
        {
            Operation op(job);
#if LL_WINDOWS
            if (!FlushFileBuffers(d.file.handle)) throw std::runtime_error(GetLastError() == ERROR_DISK_FULL ? NO_SPACE : FAILED);
#else
            if (fsync(d.file.handle)) throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
#endif
        }
        preflight(job, d.file, ext);
        cancelled(job);
        if (job.epoch != s.epoch) throw std::runtime_error(FAILED);
        completed = root() / (stem + "." + ext);
        {
            std::lock_guard<std::mutex> lock(s.cacheMutex);
            s.activeFiles.insert(pathText(completed.filename()));
        }
        {
            Operation op(job);
#if LL_WINDOWS
            if (disposition)
            {
                FILE_DISPOSITION_INFO_EX info{FILE_DISPOSITION_FLAG_DO_NOT_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE};
                if (!SetFileInformationByHandle(d.file.handle, FileDispositionInfoEx, &info, sizeof(info))) throw std::runtime_error(FAILED);
            }
            const auto name = completed.wstring();
            std::vector<unsigned char> buffer(sizeof(FILE_RENAME_INFO) + name.size() * sizeof(wchar_t));
            auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
            rename->ReplaceIfExists = FALSE; rename->RootDirectory = nullptr;
            rename->FileNameLength = static_cast<DWORD>(name.size() * sizeof(wchar_t));
            std::memcpy(rename->FileName, name.data(), rename->FileNameLength);
            if (!SetFileInformationByHandle(d.file.handle, FileRenameInfo, rename, static_cast<DWORD>(buffer.size())))
                throw std::runtime_error(GetLastError() == ERROR_DISK_FULL || GetLastError() == ERROR_HANDLE_DISK_FULL ? NO_SPACE : FAILED);
            promoted = true;
            activeFiles.name = pathText(completed.filename());
            CloseHandle(d.file.handle); d.file.handle = INVALID_HANDLE_VALUE;
#else
            if (link(partial.c_str(), completed.c_str())) throw std::runtime_error(errno == ENOSPC ? NO_SPACE : FAILED);
            promoted = true;
            activeFiles.name = pathText(completed.filename());
            if (unlink(partial.c_str()))
            {
                std::lock_guard<std::mutex> lock(s.cacheMutex);
                s.orphans.files[pathText(partial.filename())] = job.bytes.load();
            }
            close(d.file.handle); d.file.handle = -1;
#endif
        }
        auto entry = std::make_shared<Entry>();
        entry->key = key; entry->id = job.id; entry->name = pathText(completed.filename());
        entry->ext = ext; entry->mime = d.mime; entry->size = job.bytes;
        ++entry->leases;
        auto lease = cacheLease(job, entry);
        cancelled(job);
        if (job.epoch != s.epoch) throw std::runtime_error(FAILED);
        // Only a still-subscribed, current main-thread poll can approve index
        // publication. No player receives the lease until that commit finishes.
        job.awaitingApproval = true;
        const auto approvalDeadline = millis() + 5000;
        while (!job.approved)
        {
            cancelled(job);
            if (millis() >= approvalDeadline) throw std::runtime_error(FAILED);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        job.awaitingApproval = false;
        {
            std::lock_guard<std::mutex> lock(s.cacheMutex);
            cancelled(job);
            if (job.epoch != s.epoch) throw std::runtime_error(FAILED);
            if (s.lruClock == UINT64_MAX) throw std::runtime_error(FAILED);
            entry->lru = ++s.lruClock;
            s.entries[key] = entry;
            index(job);
            indexed = true;
        }
        publish(job, "PENDING, MUTED", std::move(lease));
    }
    catch (...)
    {
        if (!indexed)
        {
            {
                std::lock_guard<std::mutex> lock(s.cacheMutex);
                const auto found = s.entries.find(key);
                if (found != s.entries.end() && found->second->id == job.id) s.entries.erase(found);
            }
            // Cleanup occurs only on this isolated thread. A cancelled syscall
            // may return later; unique names cannot touch a newer job's file.
#if LL_WINDOWS
            if (d.file.handle != INVALID_HANDLE_VALUE)
            {
                if (disposition) { FILE_DISPOSITION_INFO_EX info{FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE}; SetFileInformationByHandle(d.file.handle, FileDispositionInfoEx, &info, sizeof(info)); }
                CloseHandle(d.file.handle); d.file.handle = INVALID_HANDLE_VALUE;
            }
#else
            if (d.file.handle >= 0) { close(d.file.handle); d.file.handle = -1; }
#endif
            std::error_code ec; fs::remove(promoted ? completed : partial, ec);
            if (!ec) activeFiles.removed = true;
            if (ec) s.cleanupPending = true;
        }
        throw;
    }
}
static std::shared_ptr<Job> launch(std::function<void(Job&)> work, bool transferLease = false, bool maintenance = false)
{
    auto& s = service();
    if (!s.accepting || s.active >= 8) return {};
    auto job = std::make_shared<Job>();
    job->policy = s.nextPolicy;
    job->id = s.nextId++; job->epoch = s.epoch;
    job->transferLease = transferLease;
    job->cacheJob = transferLease;
    if (transferLease) job->ownerDeadline = millis() + CACHE_OWNER_WAIT_MS;
    job->maintenance = maintenance;
    job->result.id = job->id; job->result.epoch = job->epoch;
    s.jobs.push_back(job); ++s.active;
    // Acquire on main before cleanup can run, release only after curl cleanup.
    if (transferLease) LLCore::LLHttp::retainDetachedTransfer();
    try
    {
        std::thread([job, work = std::move(work), transferLease]()
        {
            sWorkerJob = job.get();
            if (!job->transferLease && !job->maintenance) job->started = millis();
#if LL_WINDOWS
            { std::lock_guard<std::mutex> lock(job->nativeMutex); DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &job->native, THREAD_TERMINATE | SYNCHRONIZE, FALSE, 0); }
#endif
            try { work(*job); }
            catch (const std::exception& e) { publish(*job, failureStatus(e)); }
            catch (...) { publish(*job, FAILED); }
#if LL_WINDOWS
            // Keep the duplicated thread handle until main observes actual OS
            // termination, including the thread's CRT/TLS epilogue.
#endif
            // POSIX has no portable signalled handle for a detached thread's
            // complete CRT/TLS epilogue. Retain its curl lease until the forced
            // process exit below instead of guessing that work-done means exit.
            sWorkerJob = nullptr;
            job->done = true;
#if !LL_WINDOWS
            --service().active;
#endif
        }).detach();
    }
    catch (...) { --s.active; job->done = true; job->reaped = true; if (transferLease) LLCore::LLHttp::releaseDetachedTransfer(); return {}; }
    return job;
}
void initialize(const Policy& policy)
{
    auto& s = service();
    if (!s.accepting) return;
    s.nextPolicy = policy; s.limitBytes = policy.cacheBytes;
    if (s.started) return;
    s.policy = policy; s.started = true;
    s.purgeSetting = gSavedSettings.getControl("VCamMediaCachePurgePending");
    s.nextId = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count());
    launch([](Job& job)
    {
        auto& s = service();
        tryOwnCache(job);
        // Each item has an independent deadline and exception boundary. The
        // permanent worker has no transfer lifetime and cannot be poisoned by
        // a prior synchronous-I/O cancellation.
        auto report = [&s]() { s.maintenanceFailed = true; };
        uint64_t sweptRelease = 0;
        bool shutdownSweepStarted = false;
        while (!job.cancel)
        {
            if (s.cacheRetryRequested.exchange(false)) tryOwnCache(job);
            // Observe ordinary workers drained BEFORE the final sweep, so a
            // transfer finishing during this iteration cannot escape cleanup.
            const bool finalPass = s.drainRequested && s.active == 1;
            maintenanceItem([&]()
            {
                std::deque<std::function<void()>> garbage;
                std::vector<DeferredClose> closing;
                {
                    std::lock_guard<std::mutex> lock(s.garbageMutex);
                    garbage.swap(s.garbage); closing.swap(s.closing);
                }
                executeRetirements(garbage, [&](auto& fn)
                { maintenanceWork([&]() { Operation op(job); fn(); cancelled(job); }, s.maintenanceFailed); });
                for (auto& item : closing)
                {
                    bool closed = false;
                    maintenanceWork([&]() { closed = item.poll(); }, s.maintenanceFailed);
                    if (!closed)
                    {
                        std::lock_guard<std::mutex> lock(s.garbageMutex);
                        s.closing.push_back(std::move(item));
                    }
                    // Otherwise retention destructs here, on the worker.
                }
                closing.clear();
                if (s.cacheState == 1)
                {
                    std::unique_lock<std::mutex> cache(s.cacheMutex, std::try_to_lock);
                    const auto released = s.released.load();
                    const bool newShutdown = s.shutdownClear && !shutdownSweepStarted;
                    if (cache && cacheMaintenanceDue(millis(), s.orphans.nextRetry,
                        newShutdown || s.shutdownBatchPending || (s.clearPending && s.sweptClearEpoch != s.epoch), released != sweptRelease))
                        maintenanceWork([&]()
                        {
                            if (newShutdown || released != sweptRelease)
                            {
                                if (s.shutdownClear) s.clearPending = true;
                                shutdownSweepStarted |= newShutdown;
                                sweptRelease = released;
                                s.orphans.nextRetry = 0;
                            }
                            try { maintainCache(s.clearPending, s.cleanupPending, s.clearFailures, s.clearDegraded,
                                [&](bool clearing) { sweep(job, clearing || s.shutdownClear); },
                                [&]() { return (!s.clearDegraded && !s.entries.empty()) || !s.evicted.empty(); }, [&]()
                            {
                                // No fallible enumeration/unlink in the fallback:
                                // drop every hit even when listing the disk failed.
                                dropCacheHits(s.entries, s.evicted);
                                LL_WARNS("VCamMedia") << cacheClearSummary(s.orphans, s.evicted.size()) << LL_ENDL;
                                maintenanceItem([&]() { index(job); }, [&]()
                                { maintenanceItem([&]() { eraseFile(job, root() / "index"); }, []() {}); });
                            }, [&]() { return s.orphans.covered() && s.evicted.empty(); });
                                s.orphans.nextRetry = clearFailureRetry(millis(), s.orphans.nextRetry,
                                    s.clearPending, s.clearFailures, s.shutdownClear); }
                            catch (...)
                            {
                                // Continue incomplete inventory/attempt coverage at 250 ms.
                                // Degradation opens Play without acknowledging Clear.
                                // Quit's one-pass/batched deletion rule is unchanged.
                                s.orphans.nextRetry = clearFailureRetry(millis(), s.orphans.nextRetry,
                                    s.clearPending, s.clearFailures, s.shutdownClear);
                                throw;
                            }
                            if (s.policy.purgeOnStartup && acknowledgeStartupPurge(s.cacheState == 1,
                                s.clearPending, s.clearDegraded,
                                s.entries.empty() && s.evicted.empty() && s.orphans.files.empty() && !s.orphans.unknown,
                                s.orphans.covered() && s.evicted.empty()))
                            { s.policy.purgeOnStartup = false; s.purgeCompleted = true; }
                        }, s.maintenanceFailed);
                }
            }, report);
            // drainShutdown arms this only after main has retired player owners.
            // A surviving transfer/close keeps maintenance alive for the bounded
            // drain; blocked I/O and undeletable files fall to the next startup.
            if (finalPass)
            {
                std::lock_guard<std::mutex> lock(s.garbageMutex);
                // Queue emptiness alone is insufficient: executed captures can
                // enqueue another retirement. Every released pin needs a pass.
                const bool deletionDue = s.shutdownClear && s.cacheState == 1 &&
                    (!shutdownSweepStarted || s.shutdownBatchPending || sweptRelease != s.released || millis() >= s.orphans.nextRetry);
                // Local players have no cache deletion work. Their fence/pin can
                // stay in the process-lifetime service without delaying quit.
                const bool retirementPending = shutdownRetirementPending(!s.garbage.empty(),
                    !s.closing.empty(), s.cacheState == 1 && !s.evicted.empty());
                if (finishShutdownMaintenance(false, true, retirementPending, deletionDue)) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::deque<std::function<void()>> garbage;
        { std::lock_guard<std::mutex> lock(s.garbageMutex); garbage.swap(s.garbage); }
        executeRetirements(garbage, [&](auto& fn) { maintenanceWork(fn, s.maintenanceFailed); });
    }, false, true);
}

Subscription::Subscription(std::shared_ptr<Job> job) : mJob(std::move(job)) { ++mJob->subscribers; }
Subscription::~Subscription()
{
    if (--mJob->subscribers == 0) { mJob->closed = true; cancelJob(*mJob); }
}
void tick()
{
    if (!sService) return;
    auto& s = service();
    // The worker publishes this one-shot acknowledgement only after the owner
    // completes the purge. The main-thread setting sink is already resolved.
    if (s.purgeCompleted.exchange(false) && s.purgeSetting) s.purgeSetting->set(false);
    bool mailboxesDrained = true;
    for (auto& job : s.jobs)
    {
        if (!s.accepting && !job->maintenance)
        {
            // Closed subscribers can outlive shutdown. Move their mailbox
            // leases to maintenance without waiting or destroying files here.
            std::unique_lock<std::mutex> mailbox(job->mailbox, std::try_to_lock);
            if (!mailbox) mailboxesDrained = false;
            else if (job->result.lease)
                retire([lease = std::move(job->result.lease)]() {});
        }
        if (!job->done && ioExpired(job->ioDeadline, millis()))
        {
            if (!job->maintenance) cancelJob(*job);
#if LL_WINDOWS
            else
            {
                std::unique_lock<std::mutex> lock(job->nativeMutex, std::try_to_lock);
                if (lock && job->native) CancelSynchronousIo(job->native);
            }
#endif
        }
#if LL_WINDOWS
        if (job->done && !job->reaped)
        {
            std::unique_lock<std::mutex> lock(job->nativeMutex, std::try_to_lock);
            if (lock && job->native && WaitForSingleObject(job->native, 0) == WAIT_OBJECT_0)
            {
                CloseHandle(job->native); job->native = nullptr; job->reaped = true;
                if (job->transferLease) LLCore::LLHttp::releaseDetachedTransfer();
                --s.active;
            }
        }
#else
        job->reaped = job->done;
#endif
    }
    s.jobs.erase(std::remove_if(s.jobs.begin(), s.jobs.end(), [](const auto& job) { return job->reaped && job->subscribers == 0; }), s.jobs.end());
    // Publish quiescence AFTER job erasure has released every result retention.
    // Only main can make this handoff; active==1 by itself races that erasure.
    if (s.shutdown.draining && mailboxesDrained && s.active == 1) s.drainRequested = true;
}
Result Subscription::poll()
{
    auto& j = *mJob;
    const auto now = millis();
    if (j.clearTimedOut || deadlineExpired(j.clearDeadline.load(), now))
    {
        j.clearTimedOut = true;
        cancelJob(j);
        return {j.id, j.epoch, j.bytes, j.total, true, CLEAR_TIMEOUT, {}};
    }
    if (j.ownerTimedOut || deadlineExpired(j.ownerDeadline.load(), now))
    {
        j.ownerTimedOut = true;
        cancelJob(j);
        return {j.id, j.epoch, j.bytes, j.total, true, cacheFailureStatus(service().cacheFailure.load()), {}};
    }
    const bool io_timeout = ioExpired(j.ioDeadline, now);
    const bool total_timeout = totalExpired(j.maintenance, j.started.load(), now);
    if (io_timeout || total_timeout) cancelJob(j);
    if (j.cancel || (j.cacheJob && j.epoch != service().epoch))
    {
        if (!mReportedRefusal)
        {
            mReportedRefusal = true;
            // Main can terminalize a blocked worker. Report here too, without
            // reading worker-only fields or waiting for that worker to return.
            LL_INFOS("VCamMedia") << "I/O subscription finish job=" << j.id << " verdict=REFUSED reason=" <<
                (io_timeout ? "I/O deadline expired" : total_timeout ? "total transfer deadline expired" :
                 j.epoch != service().epoch ? "cache epoch changed" : "cancelled") << LL_ENDL;
        }
        return {j.id, j.epoch, j.bytes, j.total, true, FAILED, {}};
    }
    if (j.awaitingApproval) j.approved = true;
    std::unique_lock<std::mutex> lock(j.mailbox, std::try_to_lock);
    Result result = lock ? j.result : Result{};
    result.id = j.id; result.epoch = j.epoch; result.bytes = j.bytes; result.total = j.total;
    if (!result.terminal && j.cacheJob && service().clearPending && !service().clearDegraded) result.status = CLEARING;
    else if (!result.terminal && result.bytes)
        result.status = result.total ? "Downloading " + std::to_string(std::min<uint64_t>(99, result.bytes * 100 / result.total)) + "%" : "Downloading...";
    return result;
}
Request startPreflight(const Descriptor& source)
{
    tick();
    auto job = launch([source](Job& job)
    {
        uint64_t size = 0;
        LL_INFOS("VCamMedia") << "Preflight start job=" << job.id << " size=unknown ext=" << diagnosticExtension(source.value) << LL_ENDL;
        try
        {
            job.rule = "local-file syntax";
            Descriptor validated; std::string reason;
            if (!classify(source.value, validated, reason, LL_WINDOWS) || validated.location != Location::LOCAL_FILE) throw std::runtime_error(reason.empty() ? UNSUPPORTED : reason);
            auto file = openLocal(job, validated.value);
            size = file->size;
            const auto ext = extension(validated.value);
            preflight(job, *file, ext);
            auto lease = std::make_shared<Lease>();
            lease->path = validated.value; lease->extension = ext; lease->retention = file;
            publish(job, "PENDING, MUTED", std::move(lease));
            LL_INFOS("VCamMedia") << "Preflight finish job=" << job.id << " verdict=ADMITTED size=" << size
                << " ext=" << diagnosticExtension(source.value) << " reason=local-file gates passed" << LL_ENDL;
        }
        catch (const std::exception& error)
        {
            LL_INFOS("VCamMedia") << "Preflight finish job=" << job.id << " verdict=REFUSED rule=" << job.rule
                << " reason=" << diagnosticReason(error) << " size=" << (size ? std::to_string(size) : "unknown")
                << " ext=" << diagnosticExtension(source.value) << LL_ENDL;
            throw;
        }
    });
    if (!job) LL_INFOS("VCamMedia") << "Preflight refused rule=service/job admission reason=closed or worker budget"
        << " size=unknown ext=" << diagnosticExtension(source.value) << LL_ENDL;
    return job ? std::make_shared<Subscription>(job) : Request{};
}
Request startDownload(const Descriptor& source)
{
    tick();
    auto& s = service();
    if (!s.started || !s.accepting)
    {
        LL_INFOS("VCamMedia") << "Download refused rule=service admission reason=not started or closed" << LL_ENDL;
        return {};
    }
    const auto key = cacheKey(source.value);
    if (auto previous = s.inflight[key].lock())
        if (!previous->cancel && previous->epoch == s.epoch && (!previous->closed)) return std::make_shared<Subscription>(previous);
    if (requestCacheOwnerRetry(s.cacheState))
    {
        s.cacheRetryRequested = true; // existing maintenance worker performs the retry
    }
    auto job = launch([source](Job& job)
    {
        LL_INFOS("VCamMedia") << "Download start job=" << job.id << " ext=" << diagnosticExtension(source.value) << LL_ENDL;
        try
        {
            job.rule = "cache owner/lock";
            auto& s = service();
            while (s.cacheState == 0)
            {
                if (deadlineExpired(job.ownerDeadline.load(), millis()))
                {
                    job.ownerTimedOut = true;
                    throw std::runtime_error(cacheFailureStatus(s.cacheFailure.load()));
                }
                cancelled(job); std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            job.ownerDeadline = 0;
            if (s.cacheState != 1) throw std::runtime_error(cacheFailureStatus(s.cacheFailure.load()));
            job.rule = "HTTP/cache transfer";
            download(job, source);
            LL_INFOS("VCamMedia") << "Download end job=" << job.id << " verdict=ADMITTED bytes=" << job.bytes.load()
                << " ext=" << diagnosticExtension(source.value) << " reason=complete validated lease" << LL_ENDL;
        }
        catch (const std::exception& error)
        {
            LL_INFOS("VCamMedia") << "Download failure job=" << job.id << " rule=" << job.rule
                << " reason=" << diagnosticReason(error) << " bytes=" << job.bytes.load()
                << " ext=" << diagnosticExtension(source.value) << LL_ENDL;
            throw;
        }
    }, true);
    if (!job) return {};
    s.inflight[key] = job;
    return std::make_shared<Subscription>(job);
}
void clearCache()
{
    auto& s = service();
    ++s.epoch;
    for (auto& job : s.jobs) if (job->cacheJob && job->subscribers) { job->closed = true; cancelJob(*job); }
    s.inflight.clear();
    s.clearDegraded = false;
    s.clearPending = true;
}
void beginShutdown(bool keepBetweenSessions)
{
    if (!sService) return;
    auto& s = service();
    if (!s.accepting) return;
    s.accepting = false; s.shutdown.begin(keepBetweenSessions, millis());
    // State 0 may still acquire the lock: only that worker may execute the
    // scheduled sweep. A non-owner never touches the index or any cache file.
    scheduleSessionCleanup(s.cacheState != -1, keepBetweenSessions, [&]()
    { s.shutdownClear = true; s.clearPending = true; s.cleanupPending = true; });
    for (auto& job : s.jobs)
    {
        job->closed = true;
        if (finishShutdownMaintenance(keepBetweenSessions, false, true, true) || !job->maintenance) cancelJob(*job);
    }
}
bool drainShutdown()
{
    if (!sService) return true;
    auto& s = service();
    // Player retirement is complete. Keep ON must bypass even blocked workers;
    // jobsRemain() still enforces _exit after logs/settings have been flushed.
    return s.shutdown.drain(s.active, millis, tick, []()
    { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
}
bool jobsRemain()
{
    tick();
#if LL_WINDOWS
    return sService && sService->active != 0;
#else
    // A fully drained Windows run can return normally. POSIX conservatively
    // skips static teardown after using this service, including a drained run.
    return sService && sService->started;
#endif
}
CacheStatus cacheStatus()
{
    CacheStatus result;
    if (!sService) return result;
    const auto state = sService->cacheState.load();
    result.owner = state == 1; result.checking = state == 0;
    if (result.checking) result.status = "Checking video cache...";
    if (result.owner)
    {
        result.bytes = sService->completeBytes; result.reservedBytes = sService->reservedBytes;
        result.limit = sService->limitBytes;
        result.status = sService->clearPending ? CLEARING : sService->clearDegraded ? CLEAR_DEGRADED :
            sService->maintenanceFailed ? FAILED : "";
    }
    else if (state < 0) result.status = cacheFailureStatus(sService->cacheFailure.load());
    return result;
}
}
