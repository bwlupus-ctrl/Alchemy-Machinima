// [Viewfinder] Pure readout formatting for the Cinematic Viewfinder overlay;
// header-only, registered directly.
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alviewfinder.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace tut
{
struct alviewfinder_data {};
typedef test_group<alviewfinder_data> alviewfinder_group;
typedef alviewfinder_group::object alviewfinder_object;
alviewfinder_group gAlViewfinderGroup("alviewfinder");

namespace
{
std::string value(int line, float fov, float fps)
{
    char buf[48];
    ALViewfinder::formatValue(line, buf, sizeof(buf), fov, fps);
    return std::string(buf);
}

std::string textLine(int line, float fov, float fps)
{
    char buf[64];
    ALViewfinder::formatLine(line, buf, sizeof(buf), fov, fps);
    return std::string(buf);
}

// All four lines joined with '\n', as drawn top to bottom.
std::string block(float fov, float fps)
{
    std::string out;
    for (int i = 0; i < ALViewfinder::LINE_COUNT; ++i)
    {
        if (i)
        {
            out += '\n';
        }
        out += textLine(i, fov, fps);
    }
    return out;
}
} // namespace

template<> template<>
void alviewfinder_object::test<1>()
{
    set_test_name("0.588 rad -> 33.7 deg, 39.63 mm (shown as 40 mm)");
    ensure_approximately_equals_range("deg", ALViewfinder::fovDegrees(0.588f), 33.69f, 0.01f);
    ensure_approximately_equals_range("mm", ALViewfinder::focalLengthMm(0.588f), 39.63f, 0.02f);
    ensure_equals("fov", value(ALViewfinder::LINE_FOV, 0.588f, 30.f), std::string("33.7\xC2\xB0"));
    ensure_equals("angle", value(ALViewfinder::LINE_ANGLE, 0.588f, 30.f), std::string("0.588"));
    ensure_equals("lens", value(ALViewfinder::LINE_LENS, 0.588f, 30.f), std::string("40 mm"));
    ensure_equals("fps", value(ALViewfinder::LINE_FPS, 0.588f, 30.f), std::string("30"));
}

template<> template<>
void alviewfinder_object::test<2>()
{
    set_test_name("four labelled lines, label column padded");
    ensure_equals("FOV line", textLine(ALViewfinder::LINE_FOV, 0.588f, 30.f), std::string("FOV   33.7\xC2\xB0"));
    ensure_equals("Angle line", textLine(ALViewfinder::LINE_ANGLE, 0.588f, 30.f), std::string("Angle 0.588"));
    ensure_equals("Lens line", textLine(ALViewfinder::LINE_LENS, 0.588f, 30.f), std::string("Lens  40 mm"));
    ensure_equals("FPS line", textLine(ALViewfinder::LINE_FPS, 0.588f, 30.f), std::string("FPS   30"));
    ensure_equals("block", block(0.588f, 30.f),
        std::string("FOV   33.7\xC2\xB0\nAngle 0.588\nLens  40 mm\nFPS   30"));
    // Values start in the same text column on every line.
    ensure_equals("value column", ALViewfinder::LABEL_PAD, 6);
    for (int i = 0; i < ALViewfinder::LINE_COUNT; ++i)
    {
        const std::string line = textLine(i, 0.588f, 30.f);
        ensure_equals("value offset", line.substr(ALViewfinder::LABEL_PAD),
            value(i, 0.588f, 30.f));
    }
}

template<> template<>
void alviewfinder_object::test<3>()
{
    set_test_name("focal length reference points");
    // 90 deg vertical on a 24 mm back is exactly a 12 mm lens.
    ensure_approximately_equals_range("90 deg", ALViewfinder::focalLengthMm(ALViewfinder::PI * 0.5f), 12.f, 0.001f);
    // 60 deg vertical -> 12 / tan(30 deg) = 20.78 mm.
    ensure_approximately_equals_range("60 deg", ALViewfinder::focalLengthMm(ALViewfinder::PI / 3.f), 20.785f, 0.01f);
    ensure_equals("60 deg block", block(ALViewfinder::PI / 3.f, 59.6f),
        std::string("FOV   60.0\xC2\xB0\nAngle 1.047\nLens  21 mm\nFPS   60"));
}

template<> template<>
void alviewfinder_object::test<4>()
{
    set_test_name("FPS rounding and clamping");
    ensure_equals("round down", ALViewfinder::roundedFps(29.49f), 29);
    ensure_equals("round up", ALViewfinder::roundedFps(29.5f), 30);
    ensure_equals("zero", ALViewfinder::roundedFps(0.f), 0);
    ensure_equals("negative", ALViewfinder::roundedFps(-5.f), 0);
    ensure_equals("nan", ALViewfinder::roundedFps(std::numeric_limits<float>::quiet_NaN()), 0);
    ensure_equals("inf", ALViewfinder::roundedFps(std::numeric_limits<float>::infinity()), 0);
}

template<> template<>
void alviewfinder_object::test<5>()
{
    set_test_name("unusable FOV prints placeholders, never garbage");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::string expected("FOV   --\nAngle --\nLens  --\nFPS   12");
    ensure_equals("zero", block(0.f, 12.f), expected);
    ensure_equals("negative", block(-1.f, 12.f), expected);
    ensure_equals("pi", block(ALViewfinder::PI, 12.f), expected);
    ensure_equals("nan", block(nan, 12.f), expected);
    ensure_equals("focal of zero fov", ALViewfinder::focalLengthMm(0.f), 0.f);
}

template<> template<>
void alviewfinder_object::test<6>()
{
    set_test_name("labels, bad line index and small buffers");
    ensure_equals("label 0", std::string(ALViewfinder::label(0)), std::string("FOV"));
    ensure_equals("label 1", std::string(ALViewfinder::label(1)), std::string("Angle"));
    ensure_equals("label 2", std::string(ALViewfinder::label(2)), std::string("Lens"));
    ensure_equals("label 3", std::string(ALViewfinder::label(3)), std::string("FPS"));
    ensure_equals("label out of range", std::string(ALViewfinder::label(9)), std::string());
    ensure_equals("value out of range", value(9, 0.588f, 30.f), std::string());
    ensure_equals("value negative index", value(-1, 0.588f, 30.f), std::string());

    char buf[4];
    std::fill(buf, buf + sizeof(buf), 'x');
    ALViewfinder::formatLine(ALViewfinder::LINE_ANGLE, buf, sizeof(buf), 0.588f, 30.f);
    ensure("terminated", std::string(buf).size() < sizeof(buf));
    ensure_equals("null buffer", ALViewfinder::formatLine(0, nullptr, 0, 0.588f, 30.f), 0);
}

template<> template<>
void alviewfinder_object::test<7>()
{
    set_test_name("Size 1.0 keeps the original layout");
    const ALViewfinder::Layout l = ALViewfinder::layout(1.f);
    ensure_equals("line", l.line, 1);
    ensure_equals("shadow", l.shadow, 1);
    ensure_equals("margin", l.margin, 12);
    ensure_equals("arm_min", l.arm_min, 12);
    ensure_equals("arm_max", l.arm_max, 40);
    ensure_equals("min_box", l.min_box, 64);
    ensure_equals("cross", l.cross, 6);
    ensure_equals("text_dx", l.text_dx, 10);
    ensure_equals("text_dy", l.text_dy, 8);
    ensure_equals("col_gap", l.col_gap, 6);
    ensure_equals("line_extra", l.line_extra, 1);
    ensure_equals("font step", ALViewfinder::fontStep(1.f), 4);
    ensure_equals("font name", std::string(ALViewfinder::fontSizeName(4)), std::string("Viewfinder4"));
}

template<> template<>
void alviewfinder_object::test<8>()
{
    set_test_name("Size 0.5 and 4.0 scale every dimension together");
    const ALViewfinder::Layout half = ALViewfinder::layout(0.5f);
    ensure_equals("0.5 line", half.line, 1);      // never thinner than 1
    ensure_equals("0.5 shadow", half.shadow, 1);
    ensure_equals("0.5 margin", half.margin, 6);
    ensure_equals("0.5 arm_min", half.arm_min, 6);
    ensure_equals("0.5 arm_max", half.arm_max, 20);
    ensure_equals("0.5 min_box", half.min_box, 32);
    ensure_equals("0.5 cross", half.cross, 3);
    ensure_equals("0.5 text_dx", half.text_dx, 5);
    ensure_equals("0.5 text_dy", half.text_dy, 4);
    ensure_equals("0.5 col_gap", half.col_gap, 3);
    ensure_equals("0.5 font step", ALViewfinder::fontStep(0.5f), 0);

    const ALViewfinder::Layout big = ALViewfinder::layout(4.f);
    ensure_equals("4 line", big.line, 4);
    ensure_equals("4 shadow", big.shadow, 4);
    ensure_equals("4 margin", big.margin, 48);
    ensure_equals("4 arm_min", big.arm_min, 48);
    ensure_equals("4 arm_max", big.arm_max, 160);
    ensure_equals("4 min_box", big.min_box, 256);
    ensure_equals("4 cross", big.cross, 24);
    ensure_equals("4 text_dx", big.text_dx, 40);
    ensure_equals("4 text_dy", big.text_dy, 32);
    ensure_equals("4 col_gap", big.col_gap, 24);
    ensure_equals("4 line_extra", big.line_extra, 4);
    ensure_equals("4 font step", ALViewfinder::fontStep(4.f), ALViewfinder::FONT_STEPS - 1);
    ensure_equals("4 font name", std::string(ALViewfinder::fontSizeName(12)), std::string("Viewfinder12"));
}

template<> template<>
void alviewfinder_object::test<9>()
{
    set_test_name("Size clamps, font ladder and arm length");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ensure_equals("below", ALViewfinder::clampScale(0.1f), ALViewfinder::SCALE_MIN);
    ensure_equals("above", ALViewfinder::clampScale(9.f), ALViewfinder::SCALE_MAX);
    ensure_equals("nan", ALViewfinder::clampScale(nan), 1.f);
    ensure_equals("inf", ALViewfinder::clampScale(std::numeric_limits<float>::infinity()), 1.f);
    ensure_equals("in range", ALViewfinder::clampScale(2.25f), 2.25f);
    // Out-of-range input lays out like the clamp, never garbage.
    ensure_equals("layout below", ALViewfinder::layout(0.f).margin, ALViewfinder::layout(0.5f).margin);
    ensure_equals("layout above", ALViewfinder::layout(99.f).margin, ALViewfinder::layout(4.f).margin);

    // Ladder: 2^(1/4) per step, every 4th rung is a doubling.
    ensure_equals("step 2.0", ALViewfinder::fontStep(2.f), 8);
    ensure_equals("step 1.19", ALViewfinder::fontStep(1.19f), 5);
    int previous = -1;
    for (float s = ALViewfinder::SCALE_MIN; s <= ALViewfinder::SCALE_MAX + 1e-4f; s += 0.05f)
    {
        const int step = ALViewfinder::fontStep(s);
        ensure("step in range", step >= 0 && step < ALViewfinder::FONT_STEPS);
        ensure("step never decreases", step >= previous);
        previous = step;
    }
    ensure_equals("name clamps low", std::string(ALViewfinder::fontSizeName(-3)), std::string("Viewfinder0"));
    ensure_equals("name clamps high", std::string(ALViewfinder::fontSizeName(99)), std::string("Viewfinder12"));

    // Bracket arm: a fraction of the short side, bounded by the scaled limits.
    const ALViewfinder::Layout one = ALViewfinder::layout(1.f);
    ensure_equals("arm small box", ALViewfinder::armLength(one, 100, 100), 12);
    ensure_equals("arm mid box", ALViewfinder::armLength(one, 1920, 280), 20);
    ensure_equals("arm big box", ALViewfinder::armLength(one, 3840, 2160), 40);
    const ALViewfinder::Layout four = ALViewfinder::layout(4.f);
    ensure_equals("arm 4x small", ALViewfinder::armLength(four, 100, 100), 48);
    ensure_equals("arm 4x big", ALViewfinder::armLength(four, 3840, 2160), 154);
}
} // namespace tut
