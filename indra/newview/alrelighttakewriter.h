/** Bounded take diagnostics; queue locks never cover serialization or disk I/O. */
#ifndef AL_RELIGHT_TAKE_WRITER_H
#define AL_RELIGHT_TAKE_WRITER_H
#include "llsd.h"
#include "llsdserialize.h"
#include "llfile.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace ALRelight
{
class TakeWriter
{
public:
    static constexpr std::size_t capacity = 64;
    using Sink = std::function<bool(const LLSD&)>;
    explicit TakeWriter(Sink sink) : mSink(std::move(sink)) {}

    // Ownership transfer: the tree must share no LLSD references with live state.
    bool enqueue(std::unique_ptr<LLSD> record)
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (mClosed) return false;
        if (mQueue.size() == capacity)
        {
            mQueue.pop_front();
            ++mDropped;
        }
        mQueue.push_back(std::move(record));
        return true;
    }

    bool requestDrain()
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if (mScheduled || mQueue.empty()) return false;
        mScheduled = true;
        return true;
    }

    void postFailed()
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mScheduled = false; // Retain records for the next enqueue or shutdown.
    }

    void drain()
    {
        // A real, blocking writer mutex: concurrent drains never discard work.
        std::lock_guard<std::mutex> writer(mWriterMutex);
        for (;;)
        {
            std::unique_ptr<LLSD> record;
            std::uint64_t dropped;
            {
                std::lock_guard<std::mutex> lock(mQueueMutex);
                if (mQueue.empty())
                {
                    mScheduled = false;
                    return;
                }
                record = std::move(mQueue.front());
                mQueue.pop_front();
                dropped = mDropped;
                mDropped = 0;
            }
            (*record)["dropped_records"] = std::to_string(dropped);
            if (!mSink(*record))
            {
                std::lock_guard<std::mutex> lock(mQueueMutex);
                mDropped += dropped + 1; // Report failed writes on the next record.
            }
        }
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mClosed = true;
        }
        drain(); // Also works if General was already closed or destroyed.
    }

private:
    Sink mSink;
    std::mutex mQueueMutex, mWriterMutex;
    std::deque<std::unique_ptr<LLSD>> mQueue;
    std::uint64_t mDropped = 0;
    bool mScheduled = false, mClosed = false;
};

inline bool appendTakeRecord(const std::string& path, const LLSD& record)
{
    llofstream file(path, std::ios::out | std::ios::app);
    if (!file) return false;
    LLSDSerialize::toNotation(record, file);
    file << '\n';
    file.flush();
    return file.good();
}
}
#endif
