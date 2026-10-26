/**
 * @file lldrawbudget.h
 * @brief Scoped, resumable vertex submission limits for weather cover tiles.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef LL_DRAW_BUDGET_H
#define LL_DRAW_BUDGET_H
#include <algorithm>
#include <chrono>
#include <cstdint>

// Only installed around the weather caster traversal. The cursor counts input
// indices, so a single large triangle batch can be resumed without exceeding
// either cap. Ordinary rendering pays one null-pointer check per submission.
class LLDrawBudget
{
public:
    static constexpr unsigned DRAW_LIMIT = 16, TRIANGLE_LIMIT = 2048;
    inline static thread_local LLDrawBudget* active = nullptr;
    explicit LLDrawBudget(uint64_t& cursor) : mCursor(cursor), mSkip(cursor), mPrevious(active) { active = this; }
    ~LLDrawBudget() { active = mPrevious; }
    bool complete() const { return !mDeferred; }
    unsigned draws() const { return mDraws; }
    unsigned triangles() const { return mTriangles; }
    // Even a deadline-limited frame admits one draw (or one 2048-triangle
    // segment). This bounds completion for the observed finite caster list.
    uint64_t chunksBound() const { return std::max<uint64_t>(1, mChunksBound); }
    static bool take(unsigned mode, unsigned& offset, unsigned& count)
    {
        if (!active) return true;
        return active->select(mode, offset, count);
    }
private:
    bool select(unsigned mode, unsigned& offset, unsigned& count)
    {
        // Weather's world, tree and terrain casters submit triangle lists.
        // Never silently accept an unbounded alternate primitive submission.
        if (mode != 0) { mDeferred = true; return false; }
        mChunksBound += (uint64_t(count) + TRIANGLE_LIMIT*3 - 1)/(TRIANGLE_LIMIT*3);
        const unsigned skipped = unsigned(std::min<uint64_t>(mSkip, count));
        mSkip -= skipped; offset += skipped; count -= skipped;
        if (!count) return false;
        if (mDeferred || mDraws == DRAW_LIMIT || mTriangles == TRIANGLE_LIMIT ||
            (mDraws && std::chrono::steady_clock::now() - mStart >= std::chrono::microseconds(250)))
        { mDeferred = true; return false; }
        const unsigned accepted = std::min(count / 3, TRIANGLE_LIMIT - mTriangles) * 3;
        if (accepted < count) mDeferred = true;
        count = accepted;
        if (!count) return false;
        ++mDraws; mTriangles += count / 3; mCursor += count;
        return true;
    }
    uint64_t& mCursor;
    uint64_t mSkip, mChunksBound = 0;
    LLDrawBudget* mPrevious;
    std::chrono::steady_clock::time_point mStart = std::chrono::steady_clock::now();
    unsigned mDraws = 0, mTriangles = 0;
    bool mDeferred = false;
};
#endif
