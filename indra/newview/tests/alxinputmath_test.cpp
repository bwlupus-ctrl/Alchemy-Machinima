// [XInputPad] Pure helpers, registered directly as a header-only integration test.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alxinputmath.h"
#include <limits>

namespace tut
{
struct xinput_data {};
typedef test_group<xinput_data> xinput_group;
typedef xinput_group::object xinput_object;
xinput_group gXInputGroup("alxinputmath");

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
        ensure_equals("Drone RT uses shared shape", throttle, ALXInput::shape(r, .06f, 2.f));
        ensure_equals("Drone LT uses shared shape", brake, ALXInput::shape(l, .06f, 2.f));
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
    ensure_equals("Classic roll unchanged", ALXInput::roll(0.f, 1.f, .06f, 2.f), 1.f);
}

// Section 16.3 coverage boundary (rev 11):
// - Pure helpers above pin focus-loss edge suppression, reconnect/invalidate
//   freshness, Drone trigger neutralisation, and the MODE_TICK rumble pattern.
// - D-pad Down/model-aware hint dispatch, actions 0-6, Cursor3D refusal, and
//   THROTTLE->ROLL release-pending are LLViewerJoystick lifecycle behavior and
//   have no pure helper in alxinputmath.h; they remain integration items.
// - TODO(WP4/T39-T41): once the migration/default helpers exist, exhaust all
//   5^3 slot matrices for None-only migration, version 0/1/5 behavior, restored
//   backups, Xbox Defaults, seven-entry values 0-6, and unchanged flight model.
//   Do not duplicate settings mutation here: that would not test the WP4 code.
}
