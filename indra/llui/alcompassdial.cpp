/**
 * @file alcompassdial.cpp
 * @brief Circular compass dial control -- see alcompassdial.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alcompassdial.h"

#include "llfocusmgr.h"
#include "llfontgl.h"
#include "llcontrol.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "llsliderwheel.h"
#include "lluictrlfactory.h"
#include "lluicolortable.h"
#include <cstdio>

// same static-registration idiom as fs_virtual_trackpad
static LLDefaultChildRegistry::Register<ALCompassDial> register_compass_dial("compass_dial");

namespace
{
F32 wrap_range(F32 value, F32 minimum, F32 maximum)
{
    const F32 span = maximum - minimum;
    if (span <= 0.f) return minimum;
    value = fmodf(value - minimum, span);
    if (value < 0.f) value += span;
    return minimum + value;
}
} // anonymous namespace

ALCompassDial::Params::Params()
:   ring_color("ring_color"),
    tick_color("tick_color"),
    needle_color("needle_color"),
    text_color("text_color"),
    face_color("face_color"),
    range_color("range_color"),
    track_color("track_color"),
    focus_color("focus_color"),
    clockwise("clockwise", true),
    show_readout("show_readout", true),
    mode("mode", "compass"), arc_zero("arc_zero", "right"),
    min_val("min_val", -180.f), max_val("max_val", 180.f),
    increment("increment", 1.f), wheel_step("wheel_step", 5.f),
    decimal_digits("decimal_digits", 1), wheel_adjust("wheel_adjust", true),
    kit_geometry("kit_geometry", false), show_cardinals("show_cardinals", true),
    show_range_labels("show_range_labels", false)
{
}

ALCompassDial::ALCompassDial(const Params& p)
:   LLUICtrl(p),
    mRingColor(p.ring_color),
    mTickColor(p.tick_color),
    mNeedleColor(p.needle_color),
    mTextColor(p.text_color),
    mFaceColor(p.face_color.isProvided() ? p.face_color() : p.ring_color()),
    mRangeColor(p.range_color.isProvided() ? p.range_color() : p.text_color()),
    mTrackColor(p.track_color.isProvided() ? p.track_color() : p.tick_color()),
    mFocusColor(p.focus_color.isProvided() ? p.focus_color() : p.needle_color()),
    mClockwise(p.clockwise),
    mShowReadout(p.show_readout),
    mArc(p.mode() == "arc"), mWheelAdjust(p.wheel_adjust),
    mKitGeometry(p.kit_geometry), mShowCardinals(p.show_cardinals),
    mShowRangeLabels(p.show_range_labels), mRangeProvided(p.min_val.isProvided() || p.max_val.isProvided()),
    mMinValue(p.min_val), mMaxValue(p.max_val),
    mIncrement(p.increment), mWheelStep(p.wheel_step), mDecimalDigits(p.decimal_digits)
{
    if (mArc && !p.min_val.isProvided()) mMinValue = -90.f;
    if (mArc && !p.max_val.isProvided()) mMaxValue = 90.f;
    if (mArc && !p.show_range_labels.isProvided()) mShowRangeLabels = true;
    if (mArc)
    {
        if (!p.ring_color.isProvided()) mRingColor = LLUIColorTable::instance().getColor("KitDialRing");
        if (!p.tick_color.isProvided()) mTickColor = LLUIColorTable::instance().getColor("KitDialRing");
        if (!p.needle_color.isProvided()) mNeedleColor = LLUIColorTable::instance().getColor("KitAccent");
        if (!p.text_color.isProvided()) mTextColor = LLUIColorTable::instance().getColor("KitText");
        if (!p.face_color.isProvided()) mFaceColor = LLUIColorTable::instance().getColor("KitDialFace");
        if (!p.range_color.isProvided()) mRangeColor = LLUIColorTable::instance().getColor("KitMuted");
        if (!p.track_color.isProvided()) mTrackColor = LLUIColorTable::instance().getColor("KitArcTrack");
        if (!p.focus_color.isProvided()) mFocusColor = LLUIColorTable::instance().getColor("KitAccent");
    }
    if (mKitGeometry)
    {
        setTabStop(true);
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%.0f", mMinValue); mMinimumText = text;
    std::snprintf(text, sizeof(text), "%.0f", mMaxValue); mMaximumText = text;
    setValue(mValueDegrees);
}

// ---------------------------------------------------------------------------
// value
// ---------------------------------------------------------------------------
void ALCompassDial::setValue(const LLSD& value)
{
    const F32 v = (F32)value.asReal();
    if (!mKitGeometry && !mArc && !mRangeProvided)
    {
        // Preserve the legacy (-180, 180] endpoint convention.
        mValueDegrees = fmodf(v, 360.f);
        if (mValueDegrees > 180.f) mValueDegrees -= 360.f;
        if (mValueDegrees <= -180.f) mValueDegrees += 360.f;
    }
    else
    {
        mValueDegrees = (!mArc && mMaxValue - mMinValue >= 360.f)
            ? wrap_range(v, mMinValue, mMaxValue)
            : llclamp(v, mMinValue, mMaxValue);
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%d\xC2\xB0", ll_round(mValueDegrees));
    mReadoutText = text;
}

LLSD ALCompassDial::getValue() const
{
    return LLSD(mValueDegrees);
}

// ---------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------
void ALCompassDial::getDialGeometry(F32& cx, F32& cy, F32& radius) const
{
    const LLRect& r = getLocalRect();
    if (mKitGeometry)
    {
        const F32 scale = llmin(r.getWidth(), r.getHeight()) / 48.f;
        cy = r.getHeight() * .5f;
        if (mArc)
        {
            cx = 6.f * scale;
            radius = llmax(1.f, llmin(20.f * scale,
                llmin(cy - 3.f * scale, r.getWidth() - 9.f * scale)));
        }
        else
        {
            cx = r.getWidth() * .5f;
            radius = llmax(1.f, llmin(16.f * scale, cy - 7.f * scale));
        }
        return;
    }
    cx = (F32)r.getCenterX();
    cy = (F32)r.getCenterY();
    // leave room for the N/E/S/W letters outside the ring
    radius = llmax(4.f, llmin(r.getWidth(), r.getHeight()) * 0.5f - 12.f);
}

void ALCompassDial::setValueFromPoint(S32 x, S32 y)
{
    F32 cx, cy, radius;
    getDialGeometry(cx, cy, radius);
    const F32 dx = (F32)x - cx;
    const F32 dy = (F32)y - cy;
    const F32 dead_zone_sq = mKitGeometry ? 9.f : .25f;
    if (dx * dx + dy * dy < dead_zone_sq)
    {
        return;     // dead center: no defined bearing, keep the value
    }
    if (mArc)
    {
        const F32 angle = llclamp(atan2f(dy, dx) * RAD_TO_DEG, -90.f, 90.f);
        const bool tilt = mMinValue >= -90.f && mMaxValue <= 90.f && mMinValue <= 0.f && mMaxValue >= 0.f;
        setValue(tilt ? llclamp(angle, mMinValue, mMaxValue)
                      : mMinValue + (mMaxValue - mMinValue) * (angle + 90.f) / 180.f);
    }
    else
    {
        const F32 deg = atan2f(mClockwise ? dx : -dx, dy) * RAD_TO_DEG;
        setValue(deg);
    }
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------
bool ALCompassDial::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (!getEnabled())
    {
        return LLUICtrl::handleMouseDown(x, y, mask);
    }
    gFocusMgr.setMouseCapture(this);
    setFocus(true);
    setValueFromPoint(x, y);
    onCommit();
    return true;
}

bool ALCompassDial::handleHover(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        setValueFromPoint(x, y);
        // live commit while dragging so bound state (and any in-world
        // preview reading it) tracks the needle
        onCommit();
        return true;
    }
    return LLUICtrl::handleHover(x, y, mask);
}

bool ALCompassDial::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        gFocusMgr.setMouseCapture(nullptr);
        setValueFromPoint(x, y);
        onCommit();
        return true;
    }
    return LLUICtrl::handleMouseUp(x, y, mask);
}

bool ALCompassDial::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (!getEnabled() || !mWheelAdjust)
    {
        return false;
    }
    if (LLSliderWheel::guarded(this))
    {
        LLSliderWheel::keepAlive();
        return false;
    }
    setValue(mValueDegrees - (F32)delta.mClicks * mWheelStep);
    onCommit();
    return true;
}

bool ALCompassDial::handleKeyHere(KEY key, MASK mask)
{
    if (!mKitGeometry)
    {
        return LLUICtrl::handleKeyHere(key, mask);
    }
    F32 delta = (mask & MASK_SHIFT) ? mIncrement * 10.f : mIncrement;
    switch (key)
    {
    case KEY_LEFT: case KEY_DOWN: delta = -delta; break;
    case KEY_RIGHT: case KEY_UP: break;
    case KEY_PAGE_DOWN: delta = -15.f; break;
    case KEY_PAGE_UP: delta = 15.f; break;
    case KEY_HOME:
        if (LLControlVariable* control = getControlVariable()) setValue(control->getDefault());
        else return LLUICtrl::handleKeyHere(key, mask);
        onCommit(); return true;
    default: return LLUICtrl::handleKeyHere(key, mask);
    }
    setValue(mValueDegrees + delta); onCommit(); return true;
}

void ALCompassDial::onCommit()
{
    if (getControlVariable())
    {
        const F32 scale = powf(10.f, (F32)mDecimalDigits);
        setControlValue(LLSD(llround(mValueDegrees * scale) / scale));
    }
    LLUICtrl::onCommit();
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void ALCompassDial::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    F32 cx, cy, radius;
    getDialGeometry(cx, cy, radius);

    const F32 alpha = getEnabled() ? 1.f : 0.4f;
    LLColor4 ring = mRingColor.get() % alpha;
    LLColor4 tick = mTickColor.get() % alpha;
    LLColor4 needle = mNeedleColor.get() % alpha;
    LLColor4 text = mTextColor.get() % alpha;

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    if (mArc)
    {
        const LLColor4 face = mFaceColor.get() % alpha;
        for (S32 i = 0; i < 24; ++i)
        {
            const F32 a0 = (-90.f + i * 180.f / 24.f) * DEG_TO_RAD;
            const F32 a1 = (-90.f + (i + 1) * 180.f / 24.f) * DEG_TO_RAD;
            gl_triangle_2d(ll_round(cx), ll_round(cy), ll_round(cx + radius*cosf(a0)), ll_round(cy + radius*sinf(a0)), ll_round(cx + radius*cosf(a1)), ll_round(cy + radius*sinf(a1)), face, true);
        }
        const LLColor4 track = mTrackColor.get() % alpha;
        const LLColor4 range = mRangeColor.get() % alpha;
        const bool tilt = mMinValue >= -90.f && mMaxValue <= 90.f && mMinValue <= 0.f && mMaxValue >= 0.f;
        const F32 angle = (tilt ? llclamp(mValueDegrees, -90.f, 90.f)
                                : -90.f + 180.f * (mValueDegrees - mMinValue) / llmax(.001f, mMaxValue - mMinValue)) * DEG_TO_RAD;
        const S32 segments = 24;
        for (S32 i = 0; i < segments; ++i)
        {
            const F32 a0 = (-90.f + i * 180.f / segments) * DEG_TO_RAD;
            const F32 a1 = (-90.f + (i + 1) * 180.f / segments) * DEG_TO_RAD;
            gl_line_2d(ll_round(cx + radius*cosf(a0)), ll_round(cy + radius*sinf(a0)),
                       ll_round(cx + radius*cosf(a1)), ll_round(cy + radius*sinf(a1)), track);
        }
        gl_line_2d(ll_round(cx), ll_round(cy), ll_round(cx + radius), ll_round(cy), track);
        gl_line_2d(ll_round(cx), ll_round(cy), ll_round(cx + (radius-3.f)*cosf(angle)),
                   ll_round(cy + (radius-3.f)*sinf(angle)), needle);
        gGL.color4fv(needle.mV); gl_circle_2d(cx, cy, 2.5f, 12, true);
        if (hasFocus())
        {
            const LLColor4 focus = mFocusColor.get() % alpha;
            gGL.color4fv(focus.mV); gl_circle_2d(cx, cy, radius + 2.f, 32, false);
        }
        if (mShowRangeLabels)
        {
            if (const LLFontGL* font = LLFontGL::getFontSansSerifSmall())
            {
                font->renderUTF8(mMaximumText, 0, ll_round(cx), ll_round(cy + radius - 5.f), range, LLFontGL::LEFT, LLFontGL::VCENTER);
                font->renderUTF8(mMinimumText, 0, ll_round(cx), ll_round(cy - radius + 5.f), range, LLFontGL::LEFT, LLFontGL::VCENTER);
            }
        }
        LLUICtrl::draw(); return;
    }

    // ring
    gGL.color4fv(ring.mV);
    gl_circle_2d(cx, cy, radius, 48, false);

    // ticks every 45 deg; majors (N/E/S/W) longer
    for (S32 i = 0; i < 8; ++i)
    {
        const F32 a = (F32)i * 45.f * DEG_TO_RAD;
        const F32 dx = sinf(a);
        const F32 dy = cosf(a);
        const bool major = (i % 2) == 0;
        const F32 inner = radius - (major ? 7.f : 4.f);
        gl_line_2d(ll_round(cx + dx * inner),  ll_round(cy + dy * inner),
                   ll_round(cx + dx * radius), ll_round(cy + dy * radius),
                   major ? ring : tick);
    }

    // needle: hub dot + line toward the current bearing
    const F32 vrad = mValueDegrees * DEG_TO_RAD;
    const F32 ndx = mClockwise ? sinf(vrad) : -sinf(vrad);
    const F32 ndy = cosf(vrad);
    gl_line_2d(ll_round(cx), ll_round(cy),
               ll_round(cx + ndx * (radius - 8.f)),
               ll_round(cy + ndy * (radius - 8.f)),
               needle);
    gGL.color4fv(needle.mV);
    gl_circle_2d(cx, cy, 2.5f, 12, true);

    // cardinal letters outside the ring
    const LLFontGL* font = LLFontGL::getFontSansSerifSmall();
    if (font && mShowCardinals)
    {
        const F32 scale = llmin(getRect().getWidth(), getRect().getHeight()) / 48.f;
        const F32 lr = radius + (mKitGeometry ? 4.f * scale : 6.f);
        font->renderUTF8("N", 0, ll_round(cx), ll_round(cy + lr), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("S", 0, ll_round(cx), ll_round(cy - lr), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("E", 0, ll_round(cx + lr), ll_round(cy), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("W", 0, ll_round(cx - lr), ll_round(cy), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);

        // secondary numeric readout under the hub
        if (mShowReadout)
        {
            font->renderUTF8(mReadoutText, 0,
                             ll_round(cx), ll_round(cy - radius * 0.45f), text,
                             LLFontGL::HCENTER, LLFontGL::VCENTER);
        }
    }

    LLUICtrl::draw();
}
