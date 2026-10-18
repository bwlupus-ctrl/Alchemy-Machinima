/** Completed CPU upload provenance; independent of the player and GL context. */
#ifndef LL_LLMEDIAFRAMESTATE_H
#define LL_LLMEDIAFRAMESTATE_H

#include "stdtypes.h"
#include <cstddef>
#include <limits>
#include <string>
#include <functional>

namespace LLMediaFrame
{
struct TerminalCleanup
{
    std::function<void()> release;
    ~TerminalCleanup() { if (release) release(); }
    TerminalCleanup() = default;
    TerminalCleanup(const TerminalCleanup&) = delete;
    TerminalCleanup& operator=(const TerminalCleanup&) = delete;
};
inline bool acceptsToken(const std::string& expected, const std::string& incoming)
{
    return !expected.empty() && incoming == expected;
}

// Requested tracking and current-load tracking are distinct: an existing
// untagged player stays on the legacy upload path until its first tagged frame.
inline bool trackingEnabled(bool master, bool slots, bool consumed)
{ return master && slots && consumed; }

// Ordinary media keeps the legacy GL handoff, independent of slot epochs.
template<class Texture> void syncLegacyUpload(Texture& texture, U32 name, bool worker)
{
    if (worker) texture.syncToMainThread(name);
    else texture.syncTexName(name);
}

struct TrackingState
{
    bool enabled = false;
    bool tokenLoads = false;
    bool active = false;

    bool configure(bool enable, bool tokens)
    {
        tokens = enable && tokens;
        if (enabled == enable && tokenLoads == tokens) return false;
        enabled = enable;
        tokenLoads = tokens;
        awaitFrame();
        return true;
    }
    void awaitFrame() { active = enabled && !tokenLoads; }
    bool acceptsUpdate(const std::string& expected, const std::string& incoming)
    {
        if (!tokenLoads) return true;
        if (incoming.empty()) return !active;
        if (!acceptsToken(expected, incoming)) return false;
        active = true;
        return true;
    }
};

struct Identity
{
    U64 source = 0;
    U64 layout = 0;
    std::string load;
    bool operator==(const Identity& rhs) const
    { return source == rhs.source && layout == rhs.layout && load == rhs.load; }
};

// Division first: no size/offset multiplication may overflow before validation.
inline bool validPixels(size_t extent, S32 stride, S32 rows, S32 depth,
                        S32 x, S32 y, S32 width, S32 height)
{
    return stride > 0 && rows > 0 && depth > 0 && x >= 0 && y >= 0 &&
        width > 0 && height > 0 && x <= stride && width <= stride - x &&
        y <= rows && height <= rows - y &&
        size_t(stride) <= extent / size_t(depth) / size_t(rows);
}

// One coalesced retry, attempted only by the normal policy-gated update loop.
struct UploadState
{
    bool pending = false;
    bool retry = false;
    Identity identity;
    U64 serial = 0;
    bool ready = false;

    void invalidate() { retry = false; ready = false; }
    bool sourceChanged(bool consumed, U64& epoch)
    {
        if (!consumed) return false;
        ++epoch;
        invalidate();
        return true;
    }
    bool finish(const Identity& job, const Identity& current, bool success)
    {
        pending = false;
        if (!(job == current)) { retry = false; return false; }
        retry = !success;
        if (success) { ready = true; ++serial; }
        return success;
    }
};
}
#endif
