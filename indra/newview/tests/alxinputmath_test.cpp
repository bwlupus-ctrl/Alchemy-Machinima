// [XInputPad] Pure helpers. Deliberately not registered: this task forbids CMake edits.
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
}
