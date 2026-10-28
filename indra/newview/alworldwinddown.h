/**
 * @file alworldwinddown.h
 * @brief Pure, wall-time world-clock wind-down envelope (session state only).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALWORLDWINDDOWN_H
#define AL_ALWORLDWINDDOWN_H

#include "alcameracurve.h"
#include "llmath.h"
#include "stdtypes.h"

#include <cmath>

namespace ALWorldWindDown
{
enum class State { IDLE, WINDING_DOWN, FROZEN, SPINNING_UP };
enum Curve : S32 { TURNTABLE = 0, TAPE_STOP = 1, CUSTOM = 2 };

struct Config
{
    F32 mDownSeconds = 1.5f;
    F32 mUpSeconds = 1.f;
    S32 mCurve = TURNTABLE;
    F32 mX1 = .42f;
    F32 mY1 = 0.f;
    F32 mX2 = 1.f;
    F32 mY2 = 1.f;
};

struct Clock
{
    S32 mMode = 0;
    F32 mScale = 1.f;

    F32 effective() const
    {
        return mMode == 1
            ? (std::isfinite(mScale) ? llclamp(mScale, 0.f, 8.f) : 1.f)
            : 1.f;
    }
};

inline bool alreadyFrozen(const Clock& clock)
{
    return clock.mMode == 1 && clock.effective() == 0.f;
}

// Shared with the real switcher adapter: never acquire its cut lease from a
// wind-down-owned clock, including a ramp that has not yet reached zero.
inline bool switcherMayEase(State wind_down_state)
{
    return wind_down_state == State::IDLE;
}

inline F32 seconds(F32 value, F32 fallback)
{
    return std::isfinite(value) ? llclamp(value, .2f, 10.f) : fallback;
}

// Reduction in speed. Custom uses precisely the switcher's shared evaluator
// and clamp, with zero feather. Normalize the exponential so its tail reaches
// zero continuously, rather than snapping off the remaining exponential tail.
inline F32 reduction(const Config& config, F32 u)
{
    u = llclamp(u, 0.f, 1.f);
    if (u <= 0.f) return 0.f;
    if (u >= 1.f) return 1.f;
    if (config.mCurve == TAPE_STOP)
    {
        return (1.f - expf(-6.f * u)) / (1.f - expf(-6.f));
    }
    return llclamp(ALCameraCurve::evalFeathered(
        config.mCurve == CUSTOM ? ALCameraCurve::CUSTOM_BEZIER : ALCameraCurve::CSS_EASE_IN,
        u, config.mX1, config.mY1, config.mX2, config.mY2, 0.f), 0.f, 1.f);
}

class Ramp
{
public:
    State state() const { return mState; }
    bool active() const { return mState != State::IDLE; }
    const Clock& restoreClock() const { return mRestore; }
    void cancel() { mState = State::IDLE; }

    F32 speed(F64 wall_now) const
    {
        if (mState == State::IDLE) return mRestore.effective();
        if (mState == State::FROZEN) return 0.f;
        const F64 elapsed = llmax(0.0, wall_now - mStarted);
        if (elapsed >= mSeconds) return mTarget;
        if (elapsed <= 0.0) return mFrom;
        const F32 u = static_cast<F32>(elapsed / mSeconds);
        // Spin-up is the time mirror of wind-down, including Custom.
        const F32 weight = mState == State::SPINNING_UP
            ? 1.f - reduction(mConfig, 1.f - u) : reduction(mConfig, u);
        return mFrom + (mTarget - mFrom) * weight;
    }

    void update(F64 wall_now)
    {
        if ((mState == State::WINDING_DOWN || mState == State::SPINNING_UP) &&
            wall_now - mStarted >= mSeconds)
        {
            mState = mState == State::WINDING_DOWN ? State::FROZEN : State::IDLE;
        }
    }

    void toggle(F64 wall_now, const Clock& clock, const Config& config)
    {
        toggle(wall_now, clock, config, clock.effective());
    }

    // A switcher handoff supplies a current speed distinct from its saved
    // baseline. Only the first leg uses that speed; reversals sample the ramp.
    void toggle(F64 wall_now, const Clock& clock, const Config& config, F32 starting_speed)
    {
        update(wall_now);
        const bool down = mState == State::IDLE || mState == State::SPINNING_UP;
        const F32 current = active() ? speed(wall_now) : starting_speed;
        if (!active()) mRestore = clock;
        mConfig = config; // latch each leg; editing a curve cannot jump a ramp
        mFrom = current;
        mTarget = down ? 0.f : mRestore.effective();
        mStarted = wall_now;
        mSeconds = static_cast<F64>(down
            ? seconds(config.mDownSeconds, 1.5f) : seconds(config.mUpSeconds, 1.f));
        mState = down ? State::WINDING_DOWN : State::SPINNING_UP;
    }

private:
    State mState = State::IDLE;
    Clock mRestore;
    Config mConfig;
    F64 mStarted = 0.0;
    F64 mSeconds = 1.5;
    F32 mFrom = 1.f;
    F32 mTarget = 0.f;
};
} // namespace ALWorldWindDown

#endif // AL_ALWORLDWINDDOWN_H
