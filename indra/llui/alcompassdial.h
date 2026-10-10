/**
 * @file alcompassdial.h
 * @brief Circular compass dial control: drag the needle to pick an angle.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Generic direct-manipulation angle picker (poser fs_virtual_trackpad
 * idiom): a ring with N/E/S/W ticks and a needle at the current value.
 * Click or drag anywhere sets the angle from the dial center (atan2),
 * committing live while dragging; the mouse wheel trims +/-5 degrees per
 * click. Value range is -180..180 degrees with 0 = north (up); the
 * "clockwise" param picks which way positive values turn the needle
 * (compass-style clockwise by default, counterclockwise for math-yaw
 * bindings like the Actor Mover heading trim). Angle in/out only -- no
 * consumer includes here, so the same widget can later be re-bound as a
 * wind direction dial.
 */

#ifndef AL_ALCOMPASSDIAL_H
#define AL_ALCOMPASSDIAL_H

#include "lluictrl.h"
#include "lluicolor.h"

class LLFontGL;

class ALCompassDial : public LLUICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Optional<LLUIColor> ring_color,
                            tick_color,
                            needle_color,
                            text_color,
                            face_color,
                            range_color,
                            track_color,
                            focus_color;
        Optional<bool>      clockwise;      // true: +deg turns the needle east (compass); false: west (math yaw)
        Optional<bool>      show_readout;   // secondary numeric "<n> deg" readout under the hub
        Optional<std::string> mode, arc_zero;
        Optional<F32> min_val, max_val, increment, wheel_step;
        Optional<S32> decimal_digits;
        Optional<bool> wheel_adjust, kit_geometry, show_cardinals, show_range_labels;

        Params();
    };

    ~ALCompassDial() override = default;

    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, LLScrollDelta delta) override;
    bool handleKeyHere(KEY key, MASK mask) override;
    void draw() override;
    void onCommit() override;

    // degrees, wrapped to [-180, 180]; 0 = north
    void setValue(const LLSD& value) override;
    LLSD getValue() const override;

protected:
    friend class LLUICtrlFactory;
    ALCompassDial(const Params& p);

private:
    void setValueFromPoint(S32 x, S32 y);
    // dial geometry in local coords
    void getDialGeometry(F32& cx, F32& cy, F32& radius) const;

    F32 mValueDegrees = 0.f;

    LLUIColor mRingColor;
    LLUIColor mTickColor;
    LLUIColor mNeedleColor;
    LLUIColor mTextColor;
    LLUIColor mFaceColor;
    LLUIColor mRangeColor;
    LLUIColor mTrackColor;
    LLUIColor mFocusColor;
    bool      mClockwise = true;
    bool      mShowReadout = true;
    bool      mArc = false;
    bool      mWheelAdjust = true;
    bool      mKitGeometry = false;
    bool      mShowCardinals = true;
    bool      mShowRangeLabels = false;
    bool      mRangeProvided = false;
    F32       mMinValue = -180.f;
    F32       mMaxValue = 180.f;
    F32       mIncrement = 1.f;
    F32       mWheelStep = 5.f;
    S32       mDecimalDigits = 1;
    std::string mReadoutText;
    std::string mMinimumText;
    std::string mMaximumText;
    const LLFontGL* mKitSansFont = nullptr;
    const LLFontGL* mKitMonoFont = nullptr;
};

#endif // AL_ALCOMPASSDIAL_H
