/**
 * @file llflycamdrone.h
 * @brief Pure fixed-step simulation for the flycam Drone flight model.
 */
#ifndef LL_LLFLYCAMDRONE_H
#define LL_LLFLYCAMDRONE_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3dmath.h"
#include "v3math.h"

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class LLFlycamDrone
{
public:
    enum class ReleaseMode { HOLD, DRIFT };
    enum class ThrottleMode { ASSIST, PURE_THROTTLE };   // not "PURE": <objbase.h> #defines PURE as "= 0"
    enum class PresetId { CINE, NORMAL, SPORT, DOLLY, FPV, COUNT };

    enum ValidationClamp : U32
    {
        CLAMP_CRUISE       = 1u << 0,
        CLAMP_TOP          = 1u << 1,
        CLAMP_TIME_TO_TOP  = 1u << 2,
        CLAMP_STOP_DISTANCE= 1u << 3,
        CLAMP_HOLD_TIME    = 1u << 4,
        CLAMP_COAST_TIME   = 1u << 5,
        CLAMP_FLOAT        = 1u << 6,
        CLAMP_ONSET        = 1u << 7,
        CLAMP_GRIP         = 1u << 8,
        CLAMP_TURN_RATE    = 1u << 9,
        CLAMP_TURN_WEIGHT  = 1u << 10,
        CLAMP_TILT_RATE    = 1u << 11,
        CLAMP_CLIMB        = 1u << 12,
        CLAMP_CLIMB_TIME   = 1u << 13,
        CLAMP_SIMULATION_HZ= 1u << 14
    };

    struct CompiledValues
    {
        F32 totalLimit = 0.f;       // L_tot
        F32 effectiveJerk = 0.f;    // J_eff
        U32 validationClamps = 0;
    };

    struct HumanParams
    {
        F32 cruise = 4.f;
        F32 top = 12.f;
        F32 timeToTop = 2.f;
        F32 stopDistance = 10.f;
        F32 holdTime = .8f;
        F32 coastTime = 4.f;
        F32 highSpeedFloat = .5f;
        F32 onset = .1f;
        F32 grip = .6f;
        F32 turnRateDegrees = 70.f;
        F32 turnWeight = .5f;
        F32 tiltRateDegrees = 50.f;
        F32 climb = 2.f;
        F32 climbTime = .8f;
        ReleaseMode release = ReleaseMode::HOLD;
        ThrottleMode throttleMode = ThrottleMode::ASSIST;
        bool freeFly = false;
    };

    struct Preset
    {
        const char* name;
        HumanParams human;
    };

    struct Params
    {
        F32 cruise = 4.f;
        F32 top = 12.f;
        F32 acceleration = 0.f;
        F32 deceleration = 0.f;
        F32 brakeDeceleration = 0.f;
        F32 dragLinear = 0.f;
        F32 dragQuadratic = 0.f;
        F32 grip = .6f;
        F32 climbSpeed = 2.f;
        F32 climbAcceleration = 0.f;
        F32 pole = 0.f;
        F32 yawRate = 0.f;
        F32 speedReference = 1.f;
        F32 yawAcceleration = 0.f;
        F32 tiltRate = 0.f;
        F32 tiltAcceleration = 0.f;
        F32 rollRate = 0.f;
        F32 rollAcceleration = 0.f;
        F32 velocityCap = 0.f;
        F32 horizontalLimit = 0.f;
        F32 dragLimit = 0.f;
        F32 totalLimit = 0.f;
        F32 gMax = 0.f;
        F32 effectiveJerk = 0.f;
        U32 validationClamps = 0;
        F32 tick = 1.f / 120.f;
        ReleaseMode release = ReleaseMode::HOLD;
        ThrottleMode throttleMode = ThrottleMode::ASSIST;
        bool freeFly = false;
        bool autoLevel = false;
        F32 yawExpo = .45f;
        F32 gimbalExpo = .45f;
    };

    struct Input
    {
        // Move is (forward, left). Values supplied here are post dead-zone.
        LLVector3 move;
        F32 throttle = 0.f;
        F32 brake = 0.f;
        // True only while the unified trigger mode is Drone-THROTTLE.  The
        // model uses Assist when this is false, even if the preset says Pure.
        bool droneThrottle = false;
        // Trigger momentum is opt-in so the disabled path remains legacy-exact.
        bool triggerMomentum = false;
        F32 gasResponse = .35f;
        F32 triggerImpact = 1.f;
        F32 brakeStopTime = 1.f;
        F32 brakeRotation = 1.f;
        F32 vertical = 0.f;
        F32 yaw = 0.f;
        F32 tilt = 0.f;
        F32 roll = 0.f;
        bool level = false;
        bool recentre = false;
    };

    struct State
    {
        LLVector3 position;
        LLVector3 referenceVelocity;
        LLVector3 smootherVelocity;
        LLVector3 velocity;
        LLVector3 deltaVelocity;
        F32 yaw = 0.f;
        F32 yawVelocity = 0.f;
        F32 tilt = 0.f;
        F32 tiltVelocity = 0.f;
        F32 roll = 0.f;
        F32 rollVelocity = 0.f;
        F32 quaternionSign = 1.f;
        F32 tiltLimit = 0.f;
        F32 pole = 0.f;
        F32 pendingPole = 0.f;
        F32 totalLimitMax = 0.f;
        F32 brakeReference = 0.f;
        F32 rotationBrakeReference = 0.f;
        bool hasPendingPole = false;
        bool brakeLatched = false;
        bool recentre = false;
        bool snapped = false;
        bool settled = false;
        bool nonFiniteRecovered = false;
    };

    struct RenderPose
    {
        LLVector3 position;
        LLQuaternion rotation;
        F32 yaw = 0.f;
        F32 tilt = 0.f;
        F32 roll = 0.f;
    };

    using Logger = std::function<void(const std::string&)>;

    LLFlycamDrone();
    explicit LLFlycamDrone(const Params& params, Logger logger = Logger());

    static const std::array<Preset, static_cast<std::size_t>(PresetId::COUNT)>& presets();
    static bool compile(const HumanParams& requested, const HumanParams& previous,
                        F32 tick, F32 rollRate, Params& compiled,
                        HumanParams* accepted = nullptr, Logger logger = Logger());

    void setParams(const Params& params);
    const Params& params() const { return mParams; }
    CompiledValues compiledValues() const
    {
        return { mParams.totalLimit, mParams.effectiveJerk, mParams.validationClamps };
    }
    const State& state() const { return mCur; }
    const State& previousState() const { return mPrev; }
    F64 accumulator() const { return mAccumulator; }

    State step(const State& state, const Input& input, F32 tick) const;
    RenderPose advance(F64 frameDt, const Input& input);
    RenderPose advanceRaw(F64 frameDt, const Input& input);
    RenderPose renderPose() const;

    // Section 11.2.1 SELF-Keep: materialize the last rendered dynamic state
    // as prev = cur and discard the gap's wall time.
    void pauseAtRenderPose();
    // Section 11.3 Orbit ownership: discard wall time without changing state.
    void discardAccumulator();

    void seed(const LLVector3& position, const LLQuaternion& rotation,
              const LLVector3& velocity = LLVector3::zero,
              const LLVector3d& originTag = LLVector3d::zero,
              const State* prior = nullptr);
    void seedFromHistory(const LLVector3d& previousGlobal, const LLQuaternion& previousRotation,
                         const LLVector3d& currentGlobal, const LLQuaternion& currentRotation,
                         F64 frameDt, const LLVector3d& originTag,
                         const State* prior = nullptr);
    std::array<F32, 6> classicSeed(F32 frameDt) const;

    // Re-express all owned agent-space positions in a new agent origin.
    bool applyOrigin(const LLVector3d& newOrigin);
    const LLVector3d& originTag() const { return mOriginTag; }
    bool hasOriginTag() const { return mHasOriginTag; }

    static LLQuaternion rotation(F32 yaw, F32 tilt, F32 roll);
    static void decompose(const LLQuaternion& rotation, const State* prior,
                          F32& yaw, F32& tilt, F32& roll);
    static void seededRates(const LLQuaternion& previous, const LLQuaternion& current,
                            F64 frameDt, F32 yaw, F32 tilt,
                            const Params& params, F32& yawRate,
                            F32& tiltRate, F32& rollRate);
    static F32 gMax(F32 pole);
    static F32 effectiveJerk(F32 pole, F32 totalLimit, F32 tick);

private:
    RenderPose advanceImpl(F64 frameDt, const Input& input, bool clampFrame);
    static bool finite(const State& state);
    void log(const std::string& message) const;

    Params mParams;
    State mPrev;
    State mCur;
    F64 mAccumulator = 0.0;
    LLVector3d mOriginTag;
    bool mHasOriginTag = false;
    Logger mLogger;
};

#endif
