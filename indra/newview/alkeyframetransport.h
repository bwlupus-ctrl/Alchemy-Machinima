#ifndef AL_KEYFRAME_TRANSPORT_H
#define AL_KEYFRAME_TRANSPORT_H
#include <cstdint>
// Implementation belongs to WP3. Effects are consumed once by the player.
class ALKfTransport
{
public:
    enum class State { IDLE, PREVIEW, START_DELAY, PLAYING, HOLD_END };
    enum class Exit { NONE, ESCAPE, CUT, TELEPORT, DISCONNECT, RLVA, PILOT, RECORDER, SCENE, SHUTDOWN, REGION, TARGET_GONE, USER_INPUT };
    struct Effects { bool acquire = false, release = false, resetFlycam = false, resetAgent = false; Exit exit = Exit::NONE; };
    struct Lifecycle { bool teleport = false, disconnect = false, restricted = false, pilot = false, recorder = false, scene = false; };
    struct Frame { std::uint64_t generation = 0; double wallDelta = 0, presentationDelta = 0; bool driveCamera = false; };
    bool play(bool hasKeys, double length, bool loop, double delay, bool action = false);
    void preview(double time);
    void stop(Exit);
    void lifecycleCheck(const Lifecycle&);
    void advance(const Frame&);
    Effects takeEffects();
    State state() const { return mState; }
    double time() const { return mTime; }
    bool ownsCamera() const { return mState != State::IDLE; }
    bool clean() const { return mState == State::START_DELAY || mState == State::PLAYING || mState == State::HOLD_END; }
    bool rlvHandbackActive() const { return mHandback; }
private:
    State mState = State::IDLE;
    Effects mEffects;
    double mTime = 0, mLength = 0, mDelay = 0;
    bool mLoop = false, mHandback = false, mAdvanced = false;
    std::uint64_t mGeneration = 0;
};
#endif
