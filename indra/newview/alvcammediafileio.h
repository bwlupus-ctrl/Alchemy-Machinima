/** Detached private-file I/O. Main owns subscriptions, never worker handles. */
#ifndef AL_VCAM_MEDIA_FILE_IO_H
#define AL_VCAM_MEDIA_FILE_IO_H
#include "alvcammediafilepolicy.h"
#include <memory>
#include <functional>
struct LLPluginFileClose;

namespace ALMediaFile
{
struct Policy
{
    std::string cacheRoot, caFile, proxy;
    uint64_t downloadBytes = 0, cacheBytes = 0;
    bool allowDownloads = true;
    bool keepBetweenSessions = false, purgeOnStartup = false;
    std::string denialReason;
};
struct Lease
{
    std::string path, extension;
    bool cached = false;
    std::shared_ptr<void> retention;
};
struct Result
{
    uint64_t id = 0, epoch = 0, bytes = 0, total = 0;
    bool terminal = false;
    std::string status = "checking file";
    std::shared_ptr<Lease> lease;
};
struct Job;
class Subscription
{
public:
    explicit Subscription(std::shared_ptr<Job> job);
    ~Subscription();
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Result poll(); // Bounded, try-lock only. Deadline terminalizes without waiting.
private:
    std::shared_ptr<Job> mJob;
    bool mReportedRefusal = false;
};
using Request = std::shared_ptr<Subscription>;
struct CacheStatus
{
    bool owner = false, checking = false;
    uint64_t bytes = 0, reservedBytes = 0, limit = 0;
    std::string status = "Cache idle";
};
CacheStatus cacheStatus(); // immutable, owner-only usage; no inline I/O
std::string cacheKey(const std::string& url);
void initialize(const Policy& policy); // Startup maintenance also runs with VCam disabled; no inline I/O.
void tick(); // bounded deadline/cancellation polling; no filesystem access
void retireAfterClose(std::shared_ptr<void> retention, std::shared_ptr<LLPluginFileClose> fence);
Request startPreflight(const Descriptor& source);
Request startDownload(const Descriptor& source);
void clearCache(); // Caller first stops cache-backed players, then advances epoch.
void beginShutdown(bool keepBetweenSessions); // Cancels requests; maintenance cleans within the existing drain.
bool drainShutdown(); // At most the remainder of the single two-second budget.
bool jobsRemain();
}
#endif
