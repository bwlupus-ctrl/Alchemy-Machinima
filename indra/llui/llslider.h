/**
 * @file llslider.h
 * @brief A simple slider with no label.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// [BDMerge C8] Slider precision override: allow LLSlider::setValue() to skip
// increment-clamping for a caller-supplied value, so a value typed into the
// owning LLSliderCtrl's text editor can land between increments instead of
// being snapped. donor: Black Dragon (NiranV Dean) 94c47d42de "Added: Ability
// to override clamping to increments in sliders, allowing to input any value
// inbetween increments into the text field." Gated at the LLSliderCtrl level
// by BDMergeSliderPrecision; dragging the thumb, arrow keys and scroll wheel
// still call the 2-arg overload (precision_override defaults to false) and
// always snap to the increment. Gate-off behavior is bit-identical to stock.

#ifndef LL_LLSLIDER_H
#define LL_LLSLIDER_H

#include "llf32uictrl.h"
#include "llsliderwheelcore.h" // [SliderWheel] EWheelAdjust, SettleState
#include "v4color.h"
#include "lluiimage.h"

class LLSlider : public LLF32UICtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLF32UICtrl::Params>
    {
        Optional<std::string> orientation;

        Optional<LLUIColor> thumb_outline_color,
                            thumb_center_color;

        Optional<LLUIImage*>    thumb_image,
                                thumb_image_pressed,
                                thumb_image_disabled,
                                track_image_horizontal,
                                track_image_vertical,
                                track_highlight_horizontal_image,
                                track_highlight_vertical_image;

        Optional<CommitCallbackParam>   mouse_down_callback,
                                        mouse_up_callback;

        // [AL] Opt-in: let the ordinary (vertical) scroll wheel step a HORIZONTAL
        // slider by its increment while hovered, for precision adjustment. Off by
        // default so sliders inside scroll containers keep routing the wheel to
        // the container. Vertical sliders already take the vertical wheel.
        // [SliderWheel] Tri-state now: not provided = follows the UISliderWheelAdjust
        // switch (default on, nice 1-2-5 step, scroll-through guard); true = explicit ON
        // (legacy step, ignores the switch); false = explicit OFF (declines the wheel, e.g.
        // the media scrubber). The default value below only matters for isProvided().
        Optional<bool>                  wheel_adjust;

        // [EnvIntensity v2] Opt-in fine wheel step: with wheel_fine_div > 0 (and
        // wheel_adjust on) Shift+wheel steps by increment / wheel_fine_div, and
        // the slider's snapping grid becomes that fine step (so a saved fine
        // value survives control-variable sync). Ctrl/Alt keep the shipping x10.
        // 0 (default) = the existing wheel_adjust code path, untouched.
        Optional<F32>                   wheel_fine_div;

        Params();
    };
protected:
    LLSlider(const Params&);
    friend class LLUICtrlFactory;
public:
    virtual ~LLSlider();
    // [BDMerge C8] precision_override skips increment-rounding below; see donor note above.
    void            setValue( F32 value, bool from_event = false, bool precision_override = false );
    // overrides for LLF32UICtrl methods
    // [EnvIntensity v2] out of line: wheel_fine_div sliders keep incoming fine
    // values (control sync); others are exactly setValue((F32)v, true).
    virtual void    setValue(const LLSD& value );

    virtual void    setMinValue(const LLSD& min_value) { setMinValue((F32)min_value.asReal()); }
    virtual void    setMaxValue(const LLSD& max_value) { setMaxValue((F32)max_value.asReal()); }
    virtual void    setMinValue(F32 min_value) { LLF32UICtrl::setMinValue(min_value); updateThumbRect(); }
    virtual void    setMaxValue(F32 max_value) { LLF32UICtrl::setMaxValue(max_value); updateThumbRect(); }

    boost::signals2::connection setMouseDownCallback( const commit_signal_t::slot_type& cb );
    boost::signals2::connection setMouseUpCallback( const commit_signal_t::slot_type& cb );

    virtual bool    handleHover(S32 x, S32 y, MASK mask);
    virtual bool    handleMouseUp(S32 x, S32 y, MASK mask);
    virtual bool    handleMouseDown(S32 x, S32 y, MASK mask);
    virtual bool    handleKeyHere(KEY key, MASK mask);
    virtual bool    handleScrollWheel(S32 x, S32 y, LLScrollDelta delta);
    virtual bool    handleScrollHWheel(S32 x, S32 y, LLScrollDelta delta);
    virtual void    draw();

    // [EnvIntensity v2] accessor for LLSliderCtrl's gated wheel forwarding
    F32             getWheelFineDiv() const { return mWheelFineDiv; }
    // [SliderWheel] Runtime override of the wheel_adjust param = explicit ON / OFF (replaces the
    // [LightboxWheel] bool; the retired ALScrollFit::WheelGuard used to toggle it per event).
    void            setWheelAdjust(bool b)  { mWheelAdjust = b ? LLSliderWheel::WHEEL_ON : LLSliderWheel::WHEEL_OFF; }

    // [SliderWheel] (design v7 2.2/2.3) Resolved wheel state. The raw tri-state accessor is for
    // tests only; behaviour reads participates()/wheelEnabled()/hwheelEnabled().
    LLSliderWheel::EWheelAdjust getWheelAdjustState() const { return mWheelAdjust; }
    // ON, or UNSET with UISliderWheelAdjust on: gates ALL new behaviour (guard, nice125 step,
    // settle, the scroll-container hook). Otherwise today's code runs verbatim.
    bool            participates() const;
    // Vertical-wheel dispatch (LLSliderCtrl forwarding) / horizontal wheel.
    bool            wheelEnabled() const;
    bool            hwheelEnabled() const;

    // [SliderWheel] Wheel commit: clamp+snap, commit, and report whether the value really changed
    // AFTER the commit callbacks ran (a validation revert or a clamp is "no change"). The caller
    // schedules the settle only on true, through a re-resolved handle (design 4.1).
    bool            applyWheelValue(F32 requested, bool precision_override);
    // [SliderWheel] Wheel settle (design 4.2): one synthesised mouse-up per wheel gesture.
    void            scheduleWheelSettle();
    void            cancelWheelSettle() { mWheelSettle.invalidate(); }   // cancel, never fire
    void            fireWheelSettle(U32 gen);                            // registry/close flush only
    U32             getWheelSettleGen() const       { return mWheelSettle.gen; }
    bool            isWheelSettlePending() const    { return mWheelSettle.pending; }

private:
    bool            handleScrollWheelStock(S32 x, S32 y, LLScrollDelta delta); // pre-[SliderWheel] body, verbatim
    void            setValueAndCommit(F32 value);
    void            updateThumbRect();
    F32             snapToFineGrid(F32 value) const; // [EnvIntensity v2] wheel_fine_div grid, ends pinned exactly

    bool            mVolumeSlider;
    S32             mMouseOffset;
    LLRect          mDragStartThumbRect;

    LLPointer<LLUIImage>    mThumbImage;
    LLPointer<LLUIImage>    mThumbImagePressed;
    LLPointer<LLUIImage>    mThumbImageDisabled;
    LLPointer<LLUIImage>    mTrackImageHorizontal;
    LLPointer<LLUIImage>    mTrackImageVertical;
    LLPointer<LLUIImage>    mTrackHighlightHorizontalImage;
    LLPointer<LLUIImage>    mTrackHighlightVerticalImage;

    const EOrientation  mOrientation;
    // [AL] hovered vertical wheel steps a horizontal slider. [SliderWheel] tri-state: UNSET (param
    // not provided), ON, OFF (design 2.1). Not const: setWheelAdjust() makes it explicit ON/OFF.
    LLSliderWheel::EWheelAdjust mWheelAdjust;
    const F32           mWheelFineDiv; // [EnvIntensity v2] Shift fine divisor (0 = off)
    LLSliderWheel::SettleState mWheelSettle; // [SliderWheel] one pending generation per slider

    LLRect      mThumbRect;
    LLUIColor   mThumbOutlineColor;
    LLUIColor   mThumbCenterColor;

    commit_signal_t*    mMouseDownSignal;
    commit_signal_t*    mMouseUpSignal;
};

#endif  // LL_LLSLIDER_H
