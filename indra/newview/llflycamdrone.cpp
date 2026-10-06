/**
 * @file llflycamdrone.cpp
 * @brief Pure fixed-step simulation for the flycam Drone flight model.
 */
#include "llviewerprecompiledheaders.h"

#include "llflycamdrone.h"

#include "m3math.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace
{
constexpr F32 PI = 3.14159265358979323846f;
constexpr F32 DEG = PI / 180.f;
constexpr F32 NINETY = 90.f * DEG;
constexpr F32 EIGHTY_FIVE = 85.f * DEG;
constexpr F32 EIGHTY_NINE = 89.f * DEG;

// Sections 13-14 specify only the normalized and simulation-Hz ranges; keep
// the otherwise-unspecified human sliders inside conservative defensive
// ceilings: 200 m/s speeds, 60 s times, 2 s onset, 10 km stop distance, and
// 720 degrees/s rotation.  These cover every section 10 preset generously.
constexpr F32 MAX_SPEED = 200.f;
constexpr F32 MAX_TIME = 60.f;
constexpr F32 MAX_ONSET = 2.f;
constexpr F32 MAX_STOP_DISTANCE = 10000.f;
constexpr F32 MAX_RATE_DEGREES = 720.f;
constexpr F32 MAX_ROLL_RATE = 4.f * PI;

F32 clampf(F32 value, F32 low, F32 high)
{
    if (!std::isfinite(value))
        return 0.f;
    return std::max(low, std::min(high, value));
}

F32 wrap(F32 angle)
{
    angle = std::fmod(angle + PI, 2.f * PI);
    if (angle <= 0.f)
        angle += 2.f * PI;
    return angle - PI;
}

LLVector3 clampLength(const LLVector3& value, F32 limit)
{
    const F32 length = value.length();
    return length > limit && length > 0.f ? value * (limit / length) : value;
}

F32 shaped(F32 value, F32 expo)
{
    const F32 magnitude = std::fabs(clampf(value, -1.f, 1.f));
    return std::copysign((1.f - expo) * magnitude + expo * magnitude * magnitude * magnitude, value);
}

bool finiteVector(const LLVector3& value)
{
    return std::isfinite(value.mV[VX]) && std::isfinite(value.mV[VY]) &&
           std::isfinite(value.mV[VZ]);
}

bool finiteQuaternion(const LLQuaternion& value)
{
    return std::isfinite(value.mQ[VX]) && std::isfinite(value.mQ[VY]) &&
           std::isfinite(value.mQ[VZ]) && std::isfinite(value.mQ[VW]);
}

bool validParams(const LLFlycamDrone::Params& p)
{
    return std::isfinite(p.cruise) && p.cruise > 0.f &&
        std::isfinite(p.top) && p.top >= p.cruise &&
        std::isfinite(p.acceleration) && p.acceleration >= 0.f &&
        std::isfinite(p.deceleration) && p.deceleration >= 0.f &&
        std::isfinite(p.brakeDeceleration) && p.brakeDeceleration >= 0.f &&
        std::isfinite(p.dragLinear) && p.dragLinear >= 0.f &&
        std::isfinite(p.dragQuadratic) && p.dragQuadratic >= 0.f &&
        std::isfinite(p.grip) && p.grip >= .05f &&
        std::isfinite(p.climbSpeed) && p.climbSpeed >= 0.f &&
        std::isfinite(p.climbAcceleration) && p.climbAcceleration >= 0.f &&
        std::isfinite(p.pole) && p.pole > 0.f && p.pole < 1.f &&
        std::isfinite(p.yawRate) && p.yawRate >= 0.f &&
        std::isfinite(p.speedReference) && p.speedReference > 0.f &&
        std::isfinite(p.yawAcceleration) && p.yawAcceleration >= 0.f &&
        std::isfinite(p.tiltRate) && p.tiltRate >= 0.f &&
        std::isfinite(p.tiltAcceleration) && p.tiltAcceleration >= 0.f &&
        std::isfinite(p.rollRate) && p.rollRate >= 0.f &&
        std::isfinite(p.rollAcceleration) && p.rollAcceleration >= 0.f &&
        std::isfinite(p.velocityCap) && p.velocityCap > 0.f &&
        std::isfinite(p.horizontalLimit) && p.horizontalLimit >= 0.f &&
        std::isfinite(p.dragLimit) && p.dragLimit >= 0.f &&
        std::isfinite(p.totalLimit) && p.totalLimit > 0.f &&
        std::isfinite(p.gMax) && p.gMax > 0.f &&
        std::isfinite(p.effectiveJerk) && p.effectiveJerk > 0.f &&
        std::isfinite(p.tick) && p.tick >= 1.f / 240.f && p.tick <= 1.f / 30.f &&
        std::isfinite(p.yawExpo) && p.yawExpo >= 0.f && p.yawExpo <= 1.f &&
        std::isfinite(p.gimbalExpo) && p.gimbalExpo >= 0.f && p.gimbalExpo <= 1.f &&
        (p.release == LLFlycamDrone::ReleaseMode::HOLD ||
         p.release == LLFlycamDrone::ReleaseMode::DRIFT) &&
        (p.throttleMode == LLFlycamDrone::ThrottleMode::ASSIST ||
         p.throttleMode == LLFlycamDrone::ThrottleMode::PURE);
}

bool exactZero(const LLVector3& value)
{
    return value.mV[VX] == 0.f && value.mV[VY] == 0.f && value.mV[VZ] == 0.f;
}

bool noInput(const LLVector3& move, F32 throttle, F32 brake, F32 vertical,
             F32 yaw, F32 tilt, F32 roll, const LLFlycamDrone::Input& input)
{
    return exactZero(move) && throttle == 0.f && brake == 0.f && vertical == 0.f &&
           yaw == 0.f && tilt == 0.f && roll == 0.f &&
           !input.level && !input.recentre;
}

F32 acceptedValue(const char* name, F32 requested, F32 previous, F32 low, F32 high,
                  const LLFlycamDrone::Logger& logger, bool& valid)
{
    if (std::isfinite(requested) && requested >= low && requested <= high)
        return requested;
    valid = false;
    if (logger)
    {
        std::ostringstream out;
        out << "DRONE PARAM-CLAMP " << name << '=' << requested << " kept=" << previous;
        logger(out.str());
    }
    return previous;
}

F32 clampedFloor(const char* name, F32 requested, F32 previous, F32 low, F32 high,
                 const LLFlycamDrone::Logger& logger, bool& valid)
{
    if (!std::isfinite(requested) || requested > high)
        return acceptedValue(name, requested, previous, low, high, logger, valid);
    if (requested >= low)
        return requested;
    valid = false;
    if (logger)
    {
        std::ostringstream out;
        out << "DRONE PARAM-CLAMP " << name << '=' << requested << " applied=" << low;
        logger(out.str());
    }
    return low;
}

F32 brakingDistance(F32 top, F32 deceleration, F32 tick)
{
    if (!(deceleration > 0.f))
        return std::numeric_limits<F32>::infinity();
    const F64 m = std::floor(static_cast<F64>(top) / (deceleration * tick));
    return static_cast<F32>(tick * (m * top - deceleration * tick * m * (m + 1.0) * .5));
}

void negate(LLQuaternion& value)
{
    for (S32 i = 0; i < 4; ++i)
        value.mQ[i] = -value.mQ[i];
}
}

LLFlycamDrone::LLFlycamDrone()
{
    HumanParams accepted;
    compile(presets()[static_cast<std::size_t>(PresetId::NORMAL)].human,
            presets()[static_cast<std::size_t>(PresetId::NORMAL)].human,
            1.f / 120.f, 1.f, mParams, &accepted);
    mCur.pole = mParams.pole;
    mCur.totalLimitMax = mParams.totalLimit;
    mPrev = mCur;
}

LLFlycamDrone::LLFlycamDrone(const Params& params, Logger logger)
: mLogger(std::move(logger))
{
    if (validParams(params))
    {
        mParams = params;
    }
    else
    {
        HumanParams accepted;
        compile(presets()[static_cast<std::size_t>(PresetId::NORMAL)].human,
                presets()[static_cast<std::size_t>(PresetId::NORMAL)].human,
                1.f / 120.f, 1.f, mParams, &accepted);
        log("DRONE PARAM-CLAMP invalid compiled block; Normal kept");
    }
    mCur.pole = mParams.pole;
    mCur.totalLimitMax = mParams.totalLimit;
    mPrev = mCur;
}

const std::array<LLFlycamDrone::Preset, static_cast<std::size_t>(LLFlycamDrone::PresetId::COUNT)>&
LLFlycamDrone::presets()
{
    // Keep rows and fields positional with the table in brief section 10.
    static const std::array<Preset, static_cast<std::size_t>(PresetId::COUNT)> table = {{
        { "Cine",   { 2.f,  6.f, 3.f,  8.f, 1.2f, 6.f, .7f, .15f, .35f,  45.f, .7f,  30.f, 1.f, 1.f, ReleaseMode::HOLD,  ThrottleMode::ASSIST, false } },
        { "Normal", { 4.f, 12.f, 2.f, 10.f,  .8f, 4.f, .5f, .10f, .60f,  70.f, .5f,  50.f, 2.f, .8f, ReleaseMode::HOLD,  ThrottleMode::ASSIST, false } },
        { "Sport",  { 8.f, 30.f, 1.6f,18.f,  .6f, 3.f, .3f, .07f, .80f, 110.f, .4f,  80.f, 4.f, .6f, ReleaseMode::DRIFT, ThrottleMode::ASSIST, false } },
        { "Dolly",  { 1.f,  3.f, 2.5f, 3.f, 1.f,  5.f, .8f, .20f, .90f,  25.f, .2f,  20.f, 0.f, 1.f, ReleaseMode::HOLD,  ThrottleMode::ASSIST, false } },
        { "FPV",    {10.f, 40.f, 1.f, 25.f,  .5f, 2.f, .2f, .05f, .50f, 200.f, .3f, 150.f, 6.f, .4f, ReleaseMode::DRIFT, ThrottleMode::ASSIST, true  } }
    }};
    static_assert(static_cast<std::size_t>(PresetId::COUNT) == 5,
                  "section 10 preset count changed");
    return table;
}

bool LLFlycamDrone::compile(const HumanParams& requested, const HumanParams& previous,
                            F32 tick, F32 rollRate, Params& out,
                            HumanParams* accepted, Logger logger)
{
    if ((requested.release != ReleaseMode::HOLD && requested.release != ReleaseMode::DRIFT) ||
        (requested.throttleMode != ThrottleMode::ASSIST &&
         requested.throttleMode != ThrottleMode::PURE))
    {
        if (logger)
            logger("DRONE PARAM-CLAMP invalid release/throttle enum; previous block kept");
        if (accepted)
            *accepted = previous;
        return false;
    }
    // A huge but finite onset can round p to one before later validation.
    // Section 13/14 leaves this slider maximum open; the defensive 2 s
    // ceiling above is therefore a rejection, not a coefficient saturation.
    if (std::isfinite(requested.onset) && requested.onset > MAX_ONSET)
    {
        if (logger)
            logger("DRONE PARAM-CLAMP onset exceeds 2 s; previous block kept");
        if (accepted)
            *accepted = previous;
        return false;
    }
    bool valid = true;
    const F32 previousTick = std::isfinite(out.tick) ? out.tick : 1.f / 120.f;
    const F32 previousRoll = std::isfinite(out.rollRate) ? out.rollRate : 1.f;
    tick = acceptedValue("tick", tick, previousTick, 1.f / 240.f, 1.f / 30.f, logger, valid);
    HumanParams h = requested;
    h.cruise = acceptedValue("cruise", h.cruise, previous.cruise, .001f, MAX_SPEED, logger, valid);
    h.top = acceptedValue("top", h.top, previous.top, .001f, MAX_SPEED, logger, valid);
    if (h.top < h.cruise)
    {
        valid = false;
        if (logger)
            logger("DRONE PARAM-CLAMP top must be >= cruise; previous pair kept");
        h.cruise = previous.cruise;
        h.top = previous.top;
    }
    const F32 minTime = 4.f * tick;
    h.timeToTop = acceptedValue("timeToTop", h.timeToTop, std::max(previous.timeToTop, minTime), minTime, MAX_TIME, logger, valid);
    h.holdTime = acceptedValue("holdTime", h.holdTime, std::max(previous.holdTime, minTime), minTime, MAX_TIME, logger, valid);
    h.coastTime = acceptedValue("coastTime", h.coastTime, std::max(previous.coastTime, minTime), minTime, MAX_TIME, logger, valid);
    h.onset = acceptedValue("onset", h.onset, std::max(previous.onset, minTime), minTime, MAX_ONSET, logger, valid);
    h.highSpeedFloat = acceptedValue("highSpeedFloat", h.highSpeedFloat, previous.highSpeedFloat, 0.f, 1.f, logger, valid);
    h.grip = clampedFloor("grip", h.grip, previous.grip, .05f, 1.f, logger, valid);
    h.turnRateDegrees = acceptedValue("turnRate", h.turnRateDegrees, previous.turnRateDegrees, .001f, MAX_RATE_DEGREES, logger, valid);
    h.turnWeight = acceptedValue("turnWeight", h.turnWeight, previous.turnWeight, 0.f, 1.f, logger, valid);
    h.tiltRateDegrees = acceptedValue("tiltRate", h.tiltRateDegrees, previous.tiltRateDegrees, .001f, MAX_RATE_DEGREES, logger, valid);
    h.climb = acceptedValue("climb", h.climb, previous.climb, 0.f, MAX_SPEED, logger, valid);
    h.climbTime = acceptedValue("climbTime", h.climbTime, std::max(previous.climbTime, minTime), minTime, MAX_TIME, logger, valid);
    const F32 stopFloor = h.top * (h.onset + 2.f * tick);
    h.stopDistance = acceptedValue("stopDistance", h.stopDistance, previous.stopDistance,
                                   stopFloor, MAX_STOP_DISTANCE, logger, valid);
    if (h.stopDistance < stopFloor)
    {
        valid = false;
        h.stopDistance = stopFloor;
        if (logger)
            logger("DRONE PARAM-CLAMP stopDistance raised to onset floor");
    }
    rollRate = acceptedValue("rollRate", std::fabs(rollRate), previousRoll, 0.f, MAX_ROLL_RATE, logger, valid);

    Params p;
    p.tick = tick;
    p.cruise = h.cruise;
    p.top = h.top;
    p.acceleration = h.top * std::atanh(.9f) / h.timeToTop;
    p.deceleration = .9f * h.cruise / h.holdTime;
    const F32 desiredDistance = h.stopDistance - h.top * h.onset;
    F32 low = h.top * h.top / (2.f * (desiredDistance + h.top * tick));
    F32 high = h.top * h.top / (2.f * desiredDistance);
    for (S32 i = 0; i < 60; ++i)
    {
        const F32 middle = (low + high) * .5f;
        if (brakingDistance(h.top, middle, tick) > desiredDistance)
            low = middle;
        else
            high = middle;
    }
    p.brakeDeceleration = (low + high) * .5f;
    const F32 r = (1.f - h.highSpeedFloat) * 4.f / h.top;
    const F32 dragE = .1f * (1.f + r * h.cruise) / (1.f + .1f * r * h.cruise);
    p.dragLinear = -std::log(dragE) / h.coastTime;
    p.dragQuadratic = r * p.dragLinear;
    p.grip = h.grip;
    p.climbSpeed = h.climb;
    p.climbAcceleration = h.climb > 0.f ? .9f * h.climb / h.climbTime : 0.f;
    p.pole = h.onset / (h.onset + 2.f * tick);
    p.yawRate = h.turnRateDegrees * DEG;
    p.speedReference = h.top * (1.05f - h.turnWeight);
    p.yawAcceleration = p.yawRate / .15f;
    p.tiltRate = h.tiltRateDegrees * DEG;
    p.tiltAcceleration = p.tiltRate / .15f;
    p.rollRate = rollRate;
    p.rollAcceleration = rollRate / .15f;
    p.velocityCap = 2.f * h.top;
    const F32 along = std::max(p.acceleration, std::max(p.deceleration, p.brakeDeceleration));
    p.horizontalLimit = std::sqrt(along * along + p.grip * p.acceleration * p.grip * p.acceleration);
    p.dragLimit = p.dragLinear * p.velocityCap + p.dragQuadratic * p.velocityCap * p.velocityCap;
    p.totalLimit = std::max(std::sqrt(p.horizontalLimit * p.horizontalLimit +
                                     p.climbAcceleration * p.climbAcceleration), p.dragLimit);
    p.gMax = gMax(p.pole);
    p.effectiveJerk = effectiveJerk(p.pole, p.totalLimit, tick);
    p.release = h.release;
    p.throttleMode = h.throttleMode;
    p.freeFly = h.freeFly;
    const F32 jerkToLimit = p.effectiveJerk / p.totalLimit;
    const bool derivedValid = std::isfinite(p.acceleration) && std::isfinite(p.deceleration) &&
        std::isfinite(p.brakeDeceleration) && std::isfinite(p.dragLinear) &&
        std::isfinite(p.dragQuadratic) && std::isfinite(p.climbAcceleration) &&
        std::isfinite(p.pole) && p.pole > 0.f && p.pole < 1.f &&
        std::isfinite(p.yawRate) &&
        std::isfinite(p.speedReference) && std::isfinite(p.yawAcceleration) &&
        std::isfinite(p.tiltRate) && std::isfinite(p.tiltAcceleration) &&
        std::isfinite(p.rollAcceleration) && std::isfinite(p.velocityCap) &&
        std::isfinite(p.horizontalLimit) && std::isfinite(p.dragLimit) &&
        std::isfinite(p.totalLimit) && p.totalLimit > 0.f &&
        std::isfinite(p.gMax) && p.gMax > 0.f &&
        std::isfinite(p.effectiveJerk) && p.effectiveJerk > 0.f &&
        std::isfinite(jerkToLimit) && jerkToLimit > 0.f;
    if (!derivedValid)
    {
        if (logger)
            logger("DRONE PARAM-CLAMP derived coefficient outside finite domain; previous block kept");
        if (accepted)
            *accepted = previous;
        return false;
    }
    out = p;
    if (accepted)
        *accepted = h;
    return valid;
}

void LLFlycamDrone::setParams(const Params& params)
{
    if (!validParams(params))
    {
        log("DRONE PARAM-CLAMP invalid compiled block; previous kept");
        return;
    }
    mParams = params;
    if (params.pole != mCur.pole)
    {
        mCur.pendingPole = params.pole;
        mCur.hasPendingPole = true;
        mPrev.pendingPole = params.pole;
        mPrev.hasPendingPole = true;
    }
    else
    {
        mCur.hasPendingPole = false;
        mPrev.hasPendingPole = false;
    }
}

F32 LLFlycamDrone::gMax(F32 pole)
{
    if (!(pole >= 0.f && pole < 1.f))
        return pole == 1.f ? 0.f : 1.f;
    if (pole == 0.f)
        return 1.f;

    // g_k=(1-p)^2(k+1)p^k.  log(g_k) is strictly concave and its
    // real-valued maximum is k*=-1/log(p)-1, so the exact integer maximum is
    // one of floor(k*) and ceil(k*) (with k constrained to be non-negative).
    const F64 p = pole;
    const F64 logPole = std::log(p);
    const F64 peak = std::max(0.0, -1.0 / logPole - 1.0);
    const F64 lower = std::floor(peak);
    const F64 upper = std::ceil(peak);
    const auto kernel = [p, logPole](F64 k)
    {
        return std::exp(2.0 * std::log1p(-p) + std::log(k + 1.0) + k * logPole);
    };
    return static_cast<F32>(std::max(kernel(lower), kernel(upper)));
}

F32 LLFlycamDrone::effectiveJerk(F32 pole, F32 totalLimit, F32 tick)
{
    return tick > 0.f ? 2.f * gMax(pole) * totalLimit / tick : 0.f;
}

LLFlycamDrone::State LLFlycamDrone::step(const State& source, const Input& input, F32 tick) const
{
    State next = source;
    next.snapped = next.settled = next.nonFiniteRecovered = false;
    if (mParams.pole != source.pole)
    {
        next.pendingPole = mParams.pole;
        next.hasPendingPole = true;
    }
    else
    {
        next.hasPendingPole = false;
    }
    next.totalLimitMax = std::max(source.totalLimitMax, mParams.totalLimit);

    const F32 throttle = clampf(input.throttle, 0.f, 1.f);
    const F32 brake = clampf(input.brake, 0.f, 1.f);
    const F32 vertical = clampf(input.vertical, -1.f, 1.f);
    const F32 yawInput = clampf(input.yaw, -1.f, 1.f);
    const F32 tiltInput = clampf(input.tilt, -1.f, 1.f);
    const F32 rollInput = clampf(input.roll, -1.f, 1.f);
    LLVector3 move(std::isfinite(input.move.mV[VX]) ? input.move.mV[VX] : 0.f,
                   std::isfinite(input.move.mV[VY]) ? input.move.mV[VY] : 0.f, 0.f);
    move = clampLength(move, 1.f);
    const F32 moveLength = move.length();
    const bool assist = mParams.throttleMode == ThrottleMode::ASSIST || !input.droneThrottle;
    const F32 speedLimit = assist ? mParams.cruise + (mParams.top - mParams.cruise) * throttle
                                  : throttle * mParams.top;
    const F32 requestedSpeed = moveLength * speedLimit * (1.f - brake);
    LLVector3 reference = source.referenceVelocity;

    if (mParams.freeFly)
    {
        const LLQuaternion q = rotation(source.yaw, source.tilt, source.roll);
        LLVector3 direction = LLVector3(move.mV[VX], move.mV[VY], 0.f) * q;
        if (moveLength > 0.f)
            direction /= moveLength;
        LLVector3 target = direction * requestedSpeed + LLVector3(0.f, 0.f, vertical * mParams.climbSpeed);
        target = clampLength(target, mParams.top);
        const F32 targetSpeed = target.length();
        LLVector3 n;
        if (targetSpeed > 0.f)
            n = target / targetSpeed;
        else if (reference.length() > 0.f)
            n = reference / reference.length();
        const bool engaged = moveLength > 0.f || brake > 0.f || mParams.release == ReleaseMode::HOLD;
        if (engaged)
        {
            F32 along = reference * n;
            LLVector3 cross = reference - n * along;
            const F32 limit = mParams.deceleration +
                              (mParams.brakeDeceleration - mParams.deceleration) * brake;
            if (along < 0.f)
                along = std::min(0.f, along + limit * tick);
            else if (along > targetSpeed)
                along = std::max(targetSpeed, along - limit * tick);
            else
            {
                const F32 ratio = along / mParams.top;
                const F32 flow = ratio >= 1.f ? mParams.top
                    : mParams.top * std::tanh(std::atanh(ratio) +
                                              mParams.acceleration * tick / mParams.top);
                along = std::min(targetSpeed, flow);
            }
            cross -= clampLength(cross, mParams.grip * mParams.acceleration * tick);
            reference = n * along + cross;
        }
        else
        {
            F32 speed = reference.length();
            if (speed > 0.f)
            {
                const F32 e = std::exp(-mParams.dragLinear * tick);
                const F32 exact = mParams.dragLinear > 0.f
                    ? mParams.dragLinear * speed * e /
                      (mParams.dragLinear + mParams.dragQuadratic * speed * (1.f - e))
                    : speed;
                speed = std::max(exact, speed - mParams.dragLimit * tick);
                if (speed <= std::min(mParams.totalLimit * tick, .01f))
                    speed = 0.f;
                reference = speed > 0.f ? reference * (speed / source.referenceVelocity.length()) : LLVector3::zero;
            }
        }
    }
    else
    {
        LLVector3 horizontal(reference.mV[VX], reference.mV[VY], 0.f);
        LLVector3 n;
        if (requestedSpeed > 0.f)
        {
            const F32 c = std::cos(source.yaw);
            const F32 s = std::sin(source.yaw);
            n.set(c * move.mV[VX] - s * move.mV[VY],
                  s * move.mV[VX] + c * move.mV[VY], 0.f);
            n.normalize();
        }
        else if (horizontal.length() > 0.f)
            n = horizontal / horizontal.length();
        const bool engaged = moveLength > 0.f || brake > 0.f || mParams.release == ReleaseMode::HOLD;
        if (engaged)
        {
            F32 along = horizontal * n;
            LLVector3 cross = horizontal - n * along;
            const F32 limit = mParams.deceleration +
                              (mParams.brakeDeceleration - mParams.deceleration) * brake;
            if (along < 0.f)
                along = std::min(0.f, along + limit * tick);
            else if (along > requestedSpeed)
                along = std::max(requestedSpeed, along - limit * tick);
            else
            {
                const F32 ratio = along / mParams.top;
                const F32 flow = ratio >= 1.f ? mParams.top
                    : mParams.top * std::tanh(std::atanh(ratio) +
                                              mParams.acceleration * tick / mParams.top);
                along = std::min(requestedSpeed, flow);
            }
            cross -= clampLength(cross, mParams.grip * mParams.acceleration * tick);
            horizontal = n * along + cross;
        }
        else
        {
            F32 speed = horizontal.length();
            if (speed > 0.f)
            {
                const F32 e = std::exp(-mParams.dragLinear * tick);
                const F32 exact = mParams.dragLinear > 0.f
                    ? mParams.dragLinear * speed * e /
                      (mParams.dragLinear + mParams.dragQuadratic * speed * (1.f - e))
                    : speed;
                speed = std::max(exact, speed - mParams.dragLimit * tick);
                if (speed <= std::min(mParams.totalLimit * tick, .01f))
                    speed = 0.f;
                horizontal = speed > 0.f ? horizontal * (speed / LLVector3(reference.mV[VX], reference.mV[VY], 0.f).length())
                                        : LLVector3::zero;
            }
        }
        reference.set(horizontal.mV[VX], horizontal.mV[VY],
                      reference.mV[VZ] + clampf(vertical * mParams.climbSpeed - reference.mV[VZ],
                                                -mParams.climbAcceleration * tick,
                                                 mParams.climbAcceleration * tick));
    }
    next.referenceVelocity = reference;

    const F32 poleAfter = next.hasPendingPole ? next.pendingPole : source.pole;
    const F32 jerkBefore = effectiveJerk(source.pole, next.totalLimitMax, tick);
    const F32 jerkAfter = effectiveJerk(poleAfter, mParams.totalLimit, tick);
    const F32 jerkStar = std::min(jerkBefore, jerkAfter);
    const F32 epsilonSnap = std::min(jerkStar * tick * tick / 4.f, .005f);
    const F32 epsilonQ = epsilonSnap / 4.f;
    const bool steadyNear = (source.smootherVelocity - reference).length() <= epsilonQ &&
                            (source.velocity - source.smootherVelocity).length() <= epsilonQ &&
                            source.deltaVelocity.length() <= epsilonQ;
    const bool settle = noInput(move, throttle, brake, vertical, yawInput, tiltInput,
                                rollInput, input) && exactZero(reference) && steadyNear;
    const bool rebaseline = next.totalLimitMax > mParams.totalLimit && steadyNear;
    if (steadyNear && (settle || rebaseline || next.hasPendingPole))
    {
        next.smootherVelocity = reference;
        next.velocity = reference;
        next.pole = poleAfter;
        next.hasPendingPole = false;
        next.totalLimitMax = mParams.totalLimit;
        next.snapped = true;
        next.settled = settle;
    }
    else
    {
        next.smootherVelocity = source.smootherVelocity * source.pole + reference * (1.f - source.pole);
        next.velocity = source.velocity * source.pole + next.smootherVelocity * (1.f - source.pole);
    }
    next.deltaVelocity = next.velocity - source.velocity;
    next.position = source.position + next.velocity * tick;

    const F32 yawTarget = shaped(yawInput, mParams.yawExpo) * mParams.yawRate /
                          (1.f + LLVector3(next.velocity.mV[VX], next.velocity.mV[VY], 0.f).length() /
                           mParams.speedReference);
    next.yawVelocity += clampf(yawTarget - next.yawVelocity,
                               -mParams.yawAcceleration * tick, mParams.yawAcceleration * tick);
    next.yaw = wrap(source.yaw + next.yawVelocity * tick);
    if (input.recentre)
        next.recentre = true;
    if (next.recentre && tiltInput != 0.f)
        next.recentre = false;
    next.tiltLimit = std::max(EIGHTY_NINE, std::fabs(source.tilt) <= EIGHTY_NINE
                                           ? EIGHTY_NINE : source.tiltLimit);
    // Section 6.3 step 7 completes against the current state, before a new
    // target, rate, or integrated angle is computed.
    if (next.recentre && std::fabs(source.tilt) < .1f * DEG &&
        std::fabs(source.tiltVelocity) < .5f * DEG)
    {
        next.tilt = next.tiltVelocity = 0.f;
        next.recentre = false;
    }
    else
    {
        F32 tiltTarget = shaped(tiltInput, mParams.gimbalExpo) * mParams.tiltRate;
        if (next.recentre)
            tiltTarget = clampf(-source.tilt * mParams.tiltRate / (10.f * DEG),
                                -mParams.tiltRate, mParams.tiltRate);
        next.tiltVelocity += clampf(tiltTarget - next.tiltVelocity,
                                    -mParams.tiltAcceleration * tick, mParams.tiltAcceleration * tick);
        next.tilt = source.tilt + next.tiltVelocity * tick;
        if (std::fabs(next.tilt) > next.tiltLimit)
        {
            next.tilt = std::copysign(next.tiltLimit, next.tilt);
            next.tiltVelocity = 0.f;
        }
    }
    if (input.level)
    {
        next.roll = next.rollVelocity = 0.f;
    }
    else
    {
        const F32 rollTarget = rollInput * mParams.rollRate;
        next.rollVelocity += clampf(rollTarget - next.rollVelocity,
                                    -mParams.rollAcceleration * tick, mParams.rollAcceleration * tick);
        next.roll = wrap(source.roll + next.rollVelocity * tick);
        if (mParams.autoLevel && rollInput == 0.f)
            next.roll -= next.roll * std::min(1.f, tick / .5f);
    }

    if (!finite(next))
    {
        next = finite(source) ? source : State();
        next.referenceVelocity = next.smootherVelocity = next.velocity = LLVector3::zero;
        next.deltaVelocity = LLVector3::zero;
        next.nonFiniteRecovered = true;
        log("DRONE NONFINITE");
    }
    return next;
}

LLFlycamDrone::RenderPose LLFlycamDrone::advance(F64 frameDt, const Input& input)
{
    return advanceImpl(frameDt, input, true);
}

LLFlycamDrone::RenderPose LLFlycamDrone::advanceRaw(F64 frameDt, const Input& input)
{
    return advanceImpl(frameDt, input, false);
}

LLFlycamDrone::RenderPose LLFlycamDrone::advanceImpl(F64 frameDt, const Input& input, bool clampFrame)
{
    if (!std::isfinite(frameDt))
        frameDt = 0.0;
    frameDt = std::max(0.0, frameDt);
    if (clampFrame)
        frameDt = std::min(.25, frameDt);
    mAccumulator += frameDt;
    S32 steps = 0;
    while (mAccumulator + 1.0e-9 >= mParams.tick && steps < 512)
    {
        mPrev = mCur;
        mCur = step(mCur, input, mParams.tick);
        mAccumulator -= mParams.tick;
        ++steps;
    }
    return renderPose();
}

LLFlycamDrone::RenderPose LLFlycamDrone::renderPose() const
{
    const F32 alpha = mParams.tick > 0.f
        ? clampf(static_cast<F32>(mAccumulator / mParams.tick), 0.f, 1.f) : 0.f;
    RenderPose result;
    result.position = mPrev.position + (mCur.position - mPrev.position) * alpha;
    result.yaw = mPrev.yaw + wrap(mCur.yaw - mPrev.yaw) * alpha;
    result.tilt = mPrev.tilt + (mCur.tilt - mPrev.tilt) * alpha;
    result.roll = mPrev.roll + (mCur.roll - mPrev.roll) * alpha;
    result.rotation = rotation(result.yaw, result.tilt, result.roll);
    if (mCur.quaternionSign < 0.f)
        negate(result.rotation);
    return result;
}

void LLFlycamDrone::pauseAtRenderPose()
{
    const RenderPose pose = renderPose();
    State paused = mCur;
    paused.position = pose.position;
    paused.yaw = pose.yaw;
    paused.tilt = pose.tilt;
    paused.roll = pose.roll;
    if (finite(paused) && finiteQuaternion(pose.rotation))
    {
        mCur = paused;
    }
    else
    {
        log("DRONE NONFINITE pause pose ignored");
    }
    mPrev = mCur;
    mAccumulator = 0.0;
}

void LLFlycamDrone::discardAccumulator()
{
    mAccumulator = 0.0;
}

LLQuaternion LLFlycamDrone::rotation(F32 yaw, F32 tilt, F32 roll)
{
    LLQuaternion result;
    result.setQuat(roll, tilt, yaw);
    return result;
}

void LLFlycamDrone::decompose(const LLQuaternion& source, const State* prior,
                              F32& yaw, F32& tilt, F32& roll)
{
    LLQuaternion q = source;
    if (!finiteQuaternion(q) || q.normalize() == 0.f)
        q.loadIdentity();
    const LLMatrix3 matrix(q);
    const LLVector3 forward = matrix.getFwdRow();
    const LLVector3 left = matrix.getLeftRow();
    const F32 horizontal = std::sqrt(forward.mV[VX] * forward.mV[VX] +
                                     forward.mV[VY] * forward.mV[VY]);
    yaw = horizontal >= 1.0e-4f ? std::atan2(forward.mV[VY], forward.mV[VX])
                                : (prior ? prior->yaw
                                         : std::atan2(left.mV[VY], left.mV[VX]) - NINETY);
    tilt = std::atan2(forward.mV[VZ], horizontal);
    const LLQuaternion base = rotation(yaw, tilt, 0.f);
    const LLVector3 baseLeft = LLVector3(0.f, 1.f, 0.f) * base;
    roll = std::atan2(forward * (baseLeft % left), baseLeft * left);
}

void LLFlycamDrone::seededRates(const LLQuaternion& previous, const LLQuaternion& current,
                                F64 frameDt, F32 yaw, F32 tilt, const Params& params,
                                F32& yawRate, F32& tiltRate, F32& rollRate)
{
    yawRate = tiltRate = rollRate = 0.f;
    if (!(std::isfinite(frameDt) && frameDt > 0.0) ||
        !finiteQuaternion(previous) || !finiteQuaternion(current))
        return;
    LLQuaternion conjugate = previous;
    conjugate.conjugate();
    LLQuaternion delta = current * conjugate;
    delta.normalize();
    if (delta.mQ[VW] < 0.f)
        negate(delta);
    F32 angle = 0.f;
    LLVector3 axis;
    delta.getAngleAxis(&angle, axis);
    const LLVector3 omega = axis * static_cast<F32>(angle / frameDt);
    LLQuaternion yawRotation;
    yawRotation.setQuat(0.f, 0.f, yaw);
    const LLVector3 gimbal = LLVector3(0.f, 1.f, 0.f) * yawRotation;
    const LLVector3 horizontalForward(std::cos(yaw), std::sin(yaw), 0.f);
    tiltRate = omega * gimbal;
    if (std::fabs(tilt) <= EIGHTY_FIVE)
    {
        rollRate = (omega * horizontalForward) / std::cos(tilt);
        yawRate = omega.mV[VZ] - rollRate * std::sin(tilt);
    }
    else
    {
        rollRate = 0.f;
        yawRate = omega.mV[VZ];
    }
    yawRate = clampf(yawRate, -params.yawRate, params.yawRate);
    tiltRate = clampf(tiltRate, -params.tiltRate, params.tiltRate);
    rollRate = clampf(rollRate, -params.rollRate, params.rollRate);
}

void LLFlycamDrone::seed(const LLVector3& position, const LLQuaternion& orientation,
                         const LLVector3& velocity, const LLVector3d& originTag,
                         const State* prior)
{
    State seeded;
    seeded.position = finiteVector(position) ? position : LLVector3::zero;
    seeded.velocity = seeded.referenceVelocity = seeded.smootherVelocity =
        clampLength(finiteVector(velocity) ? velocity : LLVector3::zero, mParams.velocityCap);
    decompose(orientation, prior, seeded.yaw, seeded.tilt, seeded.roll);
    LLQuaternion reconstructed = rotation(seeded.yaw, seeded.tilt, seeded.roll);
    seeded.quaternionSign = dot(reconstructed, orientation) < 0.f ? -1.f : 1.f;
    seeded.tiltLimit = std::max(EIGHTY_NINE, std::fabs(seeded.tilt));
    seeded.pole = mParams.pole;
    seeded.totalLimitMax = mParams.totalLimit;
    mCur = mPrev = seeded;
    mAccumulator = 0.0;
    mOriginTag = originTag;
    mHasOriginTag = originTag.isFinite();
}

void LLFlycamDrone::seedFromHistory(const LLVector3d& previousGlobal,
                                    const LLQuaternion& previousRotation,
                                    const LLVector3d& currentGlobal,
                                    const LLQuaternion& currentRotation,
                                    F64 frameDt, const LLVector3d& originTag,
                                    const State* prior)
{
    LLVector3 velocity;
    if (previousGlobal.isFinite() && currentGlobal.isFinite() &&
        std::isfinite(frameDt) && frameDt > 0.0)
    {
        const LLVector3d delta = currentGlobal - previousGlobal;
        velocity.set(static_cast<F32>(delta.mdV[VX] / frameDt),
                     static_cast<F32>(delta.mdV[VY] / frameDt),
                     static_cast<F32>(delta.mdV[VZ] / frameDt));
    }
    const LLVector3d local = currentGlobal - originTag;
    seed(LLVector3(static_cast<F32>(local.mdV[VX]), static_cast<F32>(local.mdV[VY]),
                   static_cast<F32>(local.mdV[VZ])), currentRotation, velocity, originTag, prior);
    seededRates(previousRotation, currentRotation, frameDt, mCur.yaw, mCur.tilt,
                mParams, mCur.yawVelocity, mCur.tiltVelocity, mCur.rollVelocity);
    mPrev = mCur;
}

std::array<F32, 6> LLFlycamDrone::classicSeed(F32 frameDt) const
{
    if (!std::isfinite(frameDt) || frameDt < 0.f)
        frameDt = 0.f;
    const LLMatrix3 axes(rotation(mCur.yaw, mCur.tilt, mCur.roll));
    return {{ mCur.velocity * axes.getFwdRow() * frameDt,
              mCur.velocity * axes.getLeftRow() * frameDt,
              mCur.velocity * axes.getUpRow() * frameDt,
              mCur.rollVelocity * frameDt,
              mCur.tiltVelocity * frameDt,
              mCur.yawVelocity * frameDt }};
}

bool LLFlycamDrone::applyOrigin(const LLVector3d& newOrigin)
{
    if (!newOrigin.isFinite())
        return false;
    if (!mHasOriginTag)
    {
        mOriginTag = newOrigin;
        mHasOriginTag = true;
        return false;
    }
    if (newOrigin == mOriginTag)
        return false;
    const LLVector3d wideDelta = mOriginTag - newOrigin;
    const LLVector3 delta(static_cast<F32>(wideDelta.mdV[VX]),
                          static_cast<F32>(wideDelta.mdV[VY]),
                          static_cast<F32>(wideDelta.mdV[VZ]));
    const LLVector3 shiftedCur = mCur.position + delta;
    const LLVector3 shiftedPrev = mPrev.position + delta;
    if (!wideDelta.isFinite() || !finiteVector(delta) ||
        !finiteVector(shiftedCur) || !finiteVector(shiftedPrev))
    {
        // Section 6.3 step 8 recovery: a narrowing overflow cannot preserve an
        // agent-space pose, so reseed safely in the new section 11.9 frame.
        const bool stateWasFinite = finite(mCur);
        const LLQuaternion orientation = stateWasFinite
            ? rotation(mCur.yaw, mCur.tilt, mCur.roll) : LLQuaternion();
        const State* prior = stateWasFinite ? &mCur : nullptr;
        seed(LLVector3::zero, orientation, LLVector3::zero, newOrigin, prior);
        mCur.nonFiniteRecovered = true;
        mPrev = mCur;
        log("DRONE NONFINITE origin-shift reseed");
        return true;
    }
    mCur.position = shiftedCur;
    mPrev.position = shiftedPrev;
    mOriginTag = newOrigin;
    std::ostringstream out;
    out << "DRONE ORIGIN-SHIFT " << wideDelta.length();
    log(out.str());
    return true;
}

bool LLFlycamDrone::finite(const State& state)
{
    return finiteVector(state.position) && finiteVector(state.referenceVelocity) &&
           finiteVector(state.smootherVelocity) && finiteVector(state.velocity) &&
           finiteVector(state.deltaVelocity) && std::isfinite(state.yaw) &&
           std::isfinite(state.yawVelocity) && std::isfinite(state.tilt) &&
           std::isfinite(state.tiltVelocity) && std::isfinite(state.roll) &&
           std::isfinite(state.rollVelocity) && std::isfinite(state.tiltLimit) &&
           std::isfinite(state.quaternionSign) &&
           std::isfinite(state.pole) && std::isfinite(state.pendingPole) &&
           std::isfinite(state.totalLimitMax);
}

void LLFlycamDrone::log(const std::string& message) const
{
    if (mLogger)
        mLogger(message);
}
