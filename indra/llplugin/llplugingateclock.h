/** @file llplugingateclock.h
 * Bounded, injectable Gate media-clock estimator. No decoder or wall clock IO.
 */
#ifndef LL_LLPLUGINGATECLOCK_H
#define LL_LLPLUGINGATECLOCK_H
#include <algorithm>
#include <array>
#include <cmath>

class LLPluginGateClock
{
public:
    struct Sample { double position = 0., error = 1., pausedAt = 0.; };
    void reset() { *this = LLPluginGateClock(); }
    void seek(double target)
    {
        mSeek = target; mRaw = -1.; mAnchored = false;
        // A commanded seek preserves observed cadence, never invents edges.
    }
    Sample sample(double now, double raw, bool playing, bool paused, double frame)
    {
        frame = std::isfinite(frame) && frame > 0. ? frame : .1;
        if (!std::isfinite(now) || !std::isfinite(raw) || raw < 0.) return {};
        if (playing && !mPlaying)
        {
            mEdge = now; mRaw = raw;
            if (mAnchored) mAnchorTime = now;
        }
        if (raw != mRaw)
        {
            if (playing && mRaw >= 0. && raw > mRaw && raw - mRaw < .5 && mSeek < 0.)
            {
                ++mEdges;
                mPeriods[mSlot] = std::max(0., now - mEdge);
                mResiduals[mSlot] = std::abs(raw - mRaw - mPeriods[mSlot]);
                mSlot = (mSlot + 1) % mPeriods.size();
            }
            else if (mSeek < 0. && raw < mRaw - .15) { mEdges = 0; mAnchored = false; }
            mRaw = raw; mEdge = now;
        }
        if (mSeek >= 0. && paused && std::abs(raw - mSeek) <= .05)
        {
            mAnchored = true; mAnchorPosition = raw; mAnchorTime = now;
            // The discontinuity includes one decoded-frame interval and the
            // 10 ms observer. This remains an estimate, not a frame-PTS claim.
            mDiscontinuity = std::abs(raw - mSeek) + frame + .01; mSeek = -1.;
        }
        if (!playing && mPlaying)
        {
            mPausedAt = now;
            if (mAnchored) mAnchorPosition += std::max(0., now - mAnchorTime);
        }
        mPlaying = playing;
        const double p = period(), r = residual();
        Sample out; out.pausedAt = mPausedAt;
        out.position = mAnchored ? mAnchorPosition + (playing ? std::max(0., now - mAnchorTime) : 0.) :
            raw + (playing ? std::min(.5, now - mEdge) + p * .5 : 0.);
        out.error = mAnchored ? mDiscontinuity + r + std::max(0., std::abs(out.position - raw) - p - frame) :
            playing ? (mEdges < 2 || now - mEdge > .5 ? 1. : .01 + p * .5 + r) : .01;
        return out;
    }
    static double boundedPosition(double position, double duration, bool loop)
    {
        if (!(duration > 0.)) return std::max(0., position);
        return loop ? std::fmod(std::max(0., position), duration) : std::clamp(position, 0., duration);
    }
    double period() const { return *std::max_element(mPeriods.begin(), mPeriods.end()); }
    double residual() const { return *std::max_element(mResiduals.begin(), mResiduals.end()); }
private:
    std::array<double, 8> mPeriods{}, mResiduals{};
    unsigned mSlot = 0, mEdges = 0;
    double mRaw = -1., mEdge = 0., mSeek = -1., mPausedAt = 0.;
    double mAnchorPosition = 0., mAnchorTime = 0., mDiscontinuity = 1.;
    bool mPlaying = false, mAnchored = false;
};
#endif
