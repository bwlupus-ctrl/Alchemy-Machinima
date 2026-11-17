#include "alkeyframetransport.h"
#include <algorithm>
#include <cmath>

bool ALKfTransport::play(bool hasKeys, double length, bool loop, double delay, bool action)
{
    if (!hasKeys || !std::isfinite(length) || length < 0 || !std::isfinite(delay) || mHandback) return false;
    const bool leased = clean();
    mLength = length; mLoop = loop && length >= .001;
    mDelay = action ? 0 : std::max(0., delay);
    mTime = 0; mState = State::START_DELAY; mAdvanced = false;
    if (!leased) mEffects.acquire = true;
    return true;
}
void ALKfTransport::preview(double time)
{
    if (mHandback || !std::isfinite(time)) return;
    if (clean()) stop(Exit::NONE);
    mState = State::PREVIEW; mTime = std::max(0., time);
}
void ALKfTransport::stop(Exit reason)
{
    if (mState == State::IDLE) return;
    if (clean()) mEffects.release = true;
    mState = State::IDLE;
    mEffects.exit = reason;
    if (reason == Exit::RLVA)
    {
        mHandback = true; mEffects.resetAgent = true;
    }
    else if (reason != Exit::SHUTDOWN) mEffects.resetFlycam = true;
}
void ALKfTransport::lifecycleCheck(const Lifecycle& l)
{
    if (l.disconnect || l.teleport)
    {
        stop(l.disconnect ? Exit::DISCONNECT : Exit::TELEPORT);
        mHandback = false;
        return;
    }
    if (mHandback && !l.restricted)
    {
        mHandback = false; mEffects.resetFlycam = true;
    }
    if (l.restricted) stop(Exit::RLVA);
    else if (l.pilot) stop(Exit::PILOT);
    else if (l.recorder) stop(Exit::RECORDER);
    else if (l.scene) stop(Exit::SCENE);
}
void ALKfTransport::advance(const Frame& frame)
{
    if (mAdvanced && mGeneration == frame.generation) return;
    mAdvanced = true; mGeneration = frame.generation;
    double dt = frame.driveCamera ? frame.presentationDelta : frame.wallDelta;
    if (!std::isfinite(dt) || dt < 0) dt = 0;
    if (mState == State::START_DELAY)
    {
        const double consumed = std::min(dt, mDelay);
        dt -= consumed; mDelay -= consumed;
        if (mDelay > 0) return;
        mState = State::PLAYING;
    }
    if (mState != State::PLAYING) return;
    mTime += dt;
    if (mLoop) mTime = std::fmod(mTime, mLength);
    else if (mTime >= mLength)
    {
        mTime = mLength; mState = State::HOLD_END;
    }
}
ALKfTransport::Effects ALKfTransport::takeEffects()
{
    const Effects effects = mEffects; mEffects = {}; return effects;
}
