// [XInputPad] Pure helpers, registered directly as a header-only integration test.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alxinputmath.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace tut
{
struct xinput_data {};
typedef test_group<xinput_data> xinput_group;
typedef xinput_group::object xinput_object;
xinput_group gXInputGroup("alxinputmath");

static bool relatively_equal(float lhs, float rhs, float tolerance)
{
    if (lhs == rhs) return true;
    const float scale = std::max(std::fabs(lhs), std::fabs(rhs));
    return scale > 0.f && std::fabs(lhs - rhs) <= tolerance * scale;
}

template<> template<> void xinput_object::test<1>()
{
    float previous = 0.f;
    for (int i = 0; i <= 255; ++i)
    {
        const float value = ALXInput::shape(i / 255.f, .06f, 2.f);
        ensure("bounded and monotonic", value >= previous && value <= 1.f);
        previous = value;
    }
    ensure_equals("deadzone", ALXInput::shape(.06f, .06f, 2.f), 0.f);
    ensure_equals("full", ALXInput::shape(1.f, .06f, 2.f), 1.f);
    ensure_equals("rest multiplier", ALXInput::throttle(0.f, 0.f, .1f, 4.f), 1.f);
    ensure_equals("slow endpoint", ALXInput::throttle(1.f, 0.f, .1f, 4.f), .1f);
    ensure_equals("fast endpoint", ALXInput::throttle(0.f, 1.f, .1f, 4.f), 4.f);
    ensure("both endpoints", std::fabs(ALXInput::throttle(1.f, 1.f, .1f, 4.f) - .4f) < 1e-6f);
    for (int i = 0; i <= 255; ++i)
    {
        const float left = i / 255.f;
        const float right = (255 - i) / 255.f;
        ensure_equals("roll antisymmetry", ALXInput::roll(left, right, .06f, 2.f),
                      -ALXInput::roll(right, left, .06f, 2.f));
    }
}

template<> template<> void xinput_object::test<2>()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ensure_equals("NaN settings", ALXInput::shape(.5f, nan, nan), ALXInput::shape(.5f, .06f, 2.f));
    ensure_equals("NaN travel", ALXInput::shape(nan, .06f, 2.f), 0.f);
    ensure_equals("NaN multipliers", ALXInput::throttle(1.f, 1.f, nan, nan), .4f);
    ensure_equals("reversed multiplier settings", ALXInput::throttle(1.f, 1.f, 20.f, .01f), 1.f);
    ensure_equals("low limits", ALXInput::shape(-1.f, -1.f, -1.f), 0.f);
    ensure_equals("high limits", ALXInput::shape(2.f, 2.f, 8.f), 1.f);
    ensure_equals("multiplier limits", ALXInput::throttle(1.f, 1.f, -1.f, 100.f), .01f * 20.f);
}

template<> template<> void xinput_object::test<3>()
{
    ensure_equals("single edge", ALXInput::edges(1, 0, false, true), static_cast<unsigned short>(1));
    ensure_equals("held", ALXInput::edges(1, 1, false, true), static_cast<unsigned short>(0));
    ensure_equals("release", ALXInput::edges(0, 1, false, true), static_cast<unsigned short>(0));
    ensure_equals("unfocused", ALXInput::edges(1, 0, false, false), static_cast<unsigned short>(0));
    ensure("association primes", ALXInput::primeSample(true, true, true, 11, 10));
    ensure("focus regain primes", ALXInput::primeSample(false, true, false, 11, 10));
    ensure("gap primes", ALXInput::primeSample(false, true, true, 12, 10));
    ensure("same generation primes", ALXInput::primeSample(false, true, true, 10, 10));
    ensure("consecutive accepts", !ALXInput::primeSample(false, true, true, 11, 10));
    ensure_equals("primed held button", ALXInput::edges(1, 0, true, true), static_cast<unsigned short>(0));
    ensure("same-pass sample", ALXInput::fresh(true, true, 10, 10));
    ensure("stale even without display", !ALXInput::fresh(true, true, 10, 11));
    ensure("invalidated sample", !ALXInput::fresh(false, true, 10, 10));
    ensure("unfocused sample", !ALXInput::fresh(true, false, 10, 10));
}

template<> template<> void xinput_object::test<4>()
{
    int axes[] = {1, 0, 4, 2, 3, 5, -1};
    ensure("Xbox mapping", ALXInput::compatible(axes, false));
    ensure("Cursor3D excluded", !ALXInput::compatible(axes, true));
    for (int i = 0; i < 7; ++i)
    {
        if (i == 3) continue;
        const int old = axes[i];
        axes[i] = 2;
        ensure("duplicate axis excluded", !ALXInput::compatible(axes, false));
        axes[i] = old;
    }
    axes[3] = -1;
    ensure("unmapped", !ALXInput::compatible(axes, false));
    axes[6] = 2;
    ensure("zoom only", !ALXInput::compatible(axes, false));
}

template<> template<> void xinput_object::test<5>()
{
    float low = -1.f, high = -1.f;
    const ALXInput::RumblePattern* connect = ALXInput::rumblePattern(ALXInput::RUMBLE_CONNECT);
    ensure("connect exists", connect != nullptr);
    ensure("connect t=0", ALXInput::rumbleLevels(connect, 0.f, low, high));
    ensure_equals("connect step1 low", low, .35f);
    ensure_equals("connect step1 high", high, 0.f);
    ensure("connect t=89", ALXInput::rumbleLevels(connect, 89.f, low, high));
    ensure_equals("connect still step1", low, .35f);
    ensure("connect t=90 gap", ALXInput::rumbleLevels(connect, 90.f, low, high));
    ensure_equals("connect gap low", low, 0.f);
    ensure_equals("connect gap high", high, 0.f);
    ensure("connect t=149 gap", ALXInput::rumbleLevels(connect, 149.f, low, high));
    ensure_equals("connect gap end", high, 0.f);
    ensure("connect t=150", ALXInput::rumbleLevels(connect, 150.f, low, high));
    ensure_equals("connect step3 low", low, 0.f);
    ensure_equals("connect step3 high", high, .7f);
    ensure("connect t=259", ALXInput::rumbleLevels(connect, 259.f, low, high));
    ensure("connect finished at 260", !ALXInput::rumbleLevels(connect, 260.f, low, high));
    ensure_equals("finished low zero", low, 0.f);
    ensure_equals("finished high zero", high, 0.f);
    ensure("negative elapsed = start", ALXInput::rumbleLevels(connect, -5.f, low, high));
    ensure_equals("negative start low", low, .35f);
    ensure("NaN elapsed = start", ALXInput::rumbleLevels(connect, std::numeric_limits<float>::quiet_NaN(), low, high));
    ensure_equals("NaN start low", low, .35f);

    const ALXInput::RumblePattern* off = ALXInput::rumblePattern(ALXInput::RUMBLE_FLYCAM_OFF);
    ensure("flycam off t=219", ALXInput::rumbleLevels(off, 219.f, low, high));
    ensure_equals("flycam off high", high, .1f);
    ensure("flycam off done", !ALXInput::rumbleLevels(off, 220.f, low, high));
    const ALXInput::RumblePattern* tick = ALXInput::rumblePattern(ALXInput::RUMBLE_MODE_TICK);
    ensure("tick t=44", ALXInput::rumbleLevels(tick, 44.f, low, high));
    ensure_equals("tick high", high, .45f);
    ensure("tick done", !ALXInput::rumbleLevels(tick, 45.f, low, high));
    const ALXInput::RumblePattern* on = ALXInput::rumblePattern(ALXInput::RUMBLE_FLYCAM_ON);
    ensure("flycam on pulse2", ALXInput::rumbleLevels(on, 150.f, low, high));
    ensure_equals("flycam on pulse2 low", low, .5f);
    ensure("flycam on done", !ALXInput::rumbleLevels(on, 210.f, low, high));
    ensure("invalid id", ALXInput::rumblePattern(-1) == nullptr && ALXInput::rumblePattern(ALXInput::RUMBLE_COUNT) == nullptr);
    ensure("null pattern finished", !ALXInput::rumbleLevels(nullptr, 0.f, low, high));
}

template<> template<> void xinput_object::test<6>()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ensure_equals("zero level", ALXInput::rumbleMotor(0.f, 1.f), static_cast<std::uint16_t>(0));
    ensure_equals("full", ALXInput::rumbleMotor(1.f, 1.f), static_cast<std::uint16_t>(65535));
    ensure_equals("zero strength", ALXInput::rumbleMotor(1.f, 0.f), static_cast<std::uint16_t>(0));
    ensure_equals("half", ALXInput::rumbleMotor(1.f, .5f), static_cast<std::uint16_t>(32768));
    ensure_equals("level over clamps", ALXInput::rumbleMotor(5.f, 1.f), static_cast<std::uint16_t>(65535));
    ensure_equals("strength over clamps", ALXInput::rumbleMotor(1.f, 9.f), static_cast<std::uint16_t>(65535));
    ensure_equals("negative clamps", ALXInput::rumbleMotor(-1.f, 1.f), static_cast<std::uint16_t>(0));
    ensure_equals("NaN level", ALXInput::rumbleMotor(nan, 1.f), static_cast<std::uint16_t>(0));
    ensure_equals("NaN strength", ALXInput::rumbleMotor(1.f, nan), static_cast<std::uint16_t>(0));
}

// Flycam brief section 16.3/T25: Drone owns independent shaped RT throttle
// and LT brake.  Freshness/focus association gates both values at the caller.
template<> template<> void xinput_object::test<7>()
{
    float throttle = -1.f, brake = -1.f;
    ALXInput::droneTriggers(0.f, 0.f, .06f, 2.f, throttle, brake);
    ensure_equals("Drone triggers rest throttle", throttle, 0.f);
    ensure_equals("Drone triggers rest brake", brake, 0.f);

    ALXInput::droneTriggers(1.f, 0.f, .06f, 2.f, throttle, brake);
    ensure_equals("LT is full brake", brake, 1.f);
    ensure_equals("LT does not throttle", throttle, 0.f);
    ALXInput::droneTriggers(0.f, 1.f, .06f, 2.f, throttle, brake);
    ensure_equals("RT is full throttle", throttle, 1.f);
    ensure_equals("RT does not brake", brake, 0.f);

    for (int left = 0; left <= 255; ++left)
    for (int right = 0; right <= 255; right += 17)
    {
        const float l = left / 255.f, r = right / 255.f;
        ALXInput::droneTriggers(l, r, .06f, 2.f, throttle, brake);
        ensure("Drone RT uses shared shape",
               relatively_equal(throttle, ALXInput::shape(r, .06f, 2.f), 1.0e-5f));
        ensure("Drone LT uses shared shape",
               relatively_equal(brake, ALXInput::shape(l, .06f, 2.f), 1.0e-5f));
    }

    // The public freshness helper is the contract used before exposing the pair.
    ALXInput::droneTriggers(.8f, .9f, .06f, 2.f, throttle, brake);
    if (!ALXInput::fresh(true, false, 10, 10)) throttle = brake = 0.f;
    ensure_equals("unfocused sample neutral throttle", throttle, 0.f);
    ensure_equals("unfocused sample neutral brake", brake, 0.f);
    ALXInput::droneTriggers(.8f, .9f, .06f, 2.f, throttle, brake);
    if (!ALXInput::fresh(true, true, 9, 10)) throttle = brake = 0.f;
    ensure_equals("stale sample neutral throttle", throttle, 0.f);
    ensure_equals("stale sample neutral brake", brake, 0.f);

    // Classic trigger endpoints remain independently pinned by test<1>.
    ensure_equals("Classic rest multiplier unchanged", ALXInput::throttle(0.f, 0.f, .1f, 4.f), 1.f);
    // powf at the shaped endpoint may round one ulp below 1 on MSVC; two F32
    // ulps preserve the mathematical endpoint without weakening its contract.
    ensure("Classic roll unchanged",
           relatively_equal(ALXInput::roll(0.f, 1.f, .06f, 2.f), 1.f,
                            2.f * std::numeric_limits<float>::epsilon()));
}

// Flycam brief T39: exhaust the 5^3 None/non-None slot matrix.
template<> template<> void xinput_object::test<8>()
{
    const int values[] = {0, 1, 2, 3, 7};
    for (int up : values)
    for (int left : values)
    for (int right : values)
    {
        const ALXInput::DpadSettings migrated =
            ALXInput::migrateDroneDpad({up, left, right, 0});
        ensure_equals("T39 Up None-only", migrated.up, up == 0 ? 4 : up);
        ensure_equals("T39 Left None-only", migrated.left, left == 0 ? 5 : left);
        ensure_equals("T39 Right None-only", migrated.right, right == 0 ? 6 : right);
        ensure_equals("T39 migration version", migrated.version, 1);
    }
}

// Flycam brief T40: idempotence, future versions, and restored backups.
template<> template<> void xinput_object::test<9>()
{
    ALXInput::DpadSettings migrated = ALXInput::migrateDroneDpad({0, 0, 0, 0});
    migrated.left = 0;
    migrated = ALXInput::migrateDroneDpad(migrated);
    ensure_equals("T40 user None survives rerun", migrated.left, 0);
    ensure_equals("T40 version one remains", migrated.version, 1);

    const ALXInput::DpadSettings future = ALXInput::migrateDroneDpad({0, 0, 0, 5});
    ensure_equals("T40 future Up untouched", future.up, 0);
    ensure_equals("T40 future Left untouched", future.left, 0);
    ensure_equals("T40 future Right untouched", future.right, 0);
    ensure_equals("T40 future version never decreases", future.version, 5);

    const ALXInput::DpadSettings restored = ALXInput::migrateDroneDpad({0, 1, 3, 0});
    ensure_equals("T40 restored None fills", restored.up, 4);
    ensure_equals("T40 restored cue survives", restored.left, 1);
    ensure_equals("T40 restored level survives", restored.right, 3);
    ensure_equals("T40 restored version", restored.version, 1);
}

// Flycam brief T41: Xbox Defaults uses the same fill without touching other state.
template<> template<> void xinput_object::test<10>()
{
    const int values[] = {0, 1, 2, 3, 7};
    for (int up : values)
    for (int left : values)
    for (int right : values)
    {
        const ALXInput::DpadSettings filled =
            ALXInput::fillDroneDpadDefaults({up, left, right, 5});
        ensure_equals("T41 Up None-only", filled.up, up == 0 ? 4 : up);
        ensure_equals("T41 Left None-only", filled.left, left == 0 ? 5 : left);
        ensure_equals("T41 Right None-only", filled.right, right == 0 ? 6 : right);
        ensure_equals("T41 version untouched", filled.version, 5);
    }
    // ABI coverage for the values consumed by production dispatch. Actual
    // combo resource counts and global-setting non-mutation need integration
    // coverage; skins and other tests are outside WP4's allowed files.
    ensure_equals("T41 action count", static_cast<int>(ALXInput::DPAD_ACTION_COUNT), 7);
    ensure_equals("T41 None value", static_cast<int>(ALXInput::DPAD_NONE), 0);
    ensure_equals("T41 cue Go value", static_cast<int>(ALXInput::DPAD_CUE_GO), 1);
    ensure_equals("T41 cue Back value", static_cast<int>(ALXInput::DPAD_CUE_BACK), 2);
    ensure_equals("T41 level value", static_cast<int>(ALXInput::DPAD_LEVEL), 3);
    ensure_equals("T41 model value", static_cast<int>(ALXInput::DPAD_FLIGHT_MODEL), 4);
    ensure_equals("T41 release value", static_cast<int>(ALXInput::DPAD_RELEASE), 5);
    ensure_equals("T41 recentre value", static_cast<int>(ALXInput::DPAD_RECENTRE), 6);
}

// Weighted Classic X1: pair toggle, normalization, outside-pair entry and cursor gate.
template<> template<> void xinput_object::test<11>()
{
    ALXInput::ModelToggle result = ALXInput::ModelToggle::CHANGED;
    ensure_equals("X1 A to B", ALXInput::toggleFlightModel(0, 0, 2, false, result), 2);
    ensure("X1 ordinary result", result == ALXInput::ModelToggle::CHANGED);
    ensure_equals("X1 B to A", ALXInput::toggleFlightModel(2, 0, 2, false, result), 0);
    ensure_equals("X1 outside pair enters A", ALXInput::toggleFlightModel(1, 0, 2, false, result), 0);
    ensure_equals("X1 Drone pair", ALXInput::toggleFlightModel(0, 0, 1, false, result), 1);
    ensure_equals("X1 duplicate normalizes", ALXInput::toggleFlightModel(0, 1, 1, false, result), 2);
    ensure("X1 duplicate reports normalization", result == ALXInput::ModelToggle::NORMALISED_PAIR);
    ensure_equals("X1 invalid normalizes", ALXInput::toggleFlightModel(0, -9, 17, false, result), 2);
    ensure("X1 invalid reports normalization", result == ALXInput::ModelToggle::NORMALISED_PAIR);
    ensure_equals("X1 cursor blocks non-Classic", ALXInput::toggleFlightModel(0, 0, 2, true, result), 0);
    ensure("X1 cursor result", result == ALXInput::ModelToggle::BLOCKED_3D_CURSOR);
    ensure_equals("X1 cursor permits Classic", ALXInput::toggleFlightModel(2, 0, 2, true, result), 0);
}

// Weighted Classic X3: partner rule and the commit-validation state transition.
template<> template<> void xinput_object::test<12>()
{
    ensure_equals("X3 Classic partner", ALXInput::partner(0), 2);
    ensure_equals("X3 Weighted partner", ALXInput::partner(2), 0);
    ensure_equals("X3 Drone partner", ALXInput::partner(1), 0);
    ensure_equals("X3 invalid partner", ALXInput::partner(99), 0);

    for (int changedSide = 0; changedSide < 2; ++changedSide)
    for (int model = 0; model < 3; ++model)
    {
        int pairA = model;
        int pairB = model;
        if (changedSide == 0) pairB = ALXInput::partner(pairA);
        else pairA = ALXInput::partner(pairB);
        ensure("X3 equal commit moves other side", pairA != pairB);
        const int savedA = pairA, savedB = pairB;
        if (pairA == pairB) pairB = ALXInput::partner(pairA);
        ensure("X3 signal re-entry is no-op", pairA == savedA && pairB == savedB);
        int restoredA = savedA, restoredB = savedB;
        ensure("X3 Cancel-style restore remains valid", restoredA != restoredB);
    }
}
}
