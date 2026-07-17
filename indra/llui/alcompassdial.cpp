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
#include "llrender.h"
#include "llrender2dutils.h"
#include "lluictrlfactory.h"

// same static-registration idiom as fs_virtual_trackpad
static LLDefaultChildRegistry::Register<ALCompassDial> register_compass_dial("compass_dial");

namespace
{
// wrap any angle into (-180, 180]
F32 wrap_degrees(F32 deg)
{
    while (deg > 180.f)  deg -= 360.f;
    while (deg <= -180.f) deg += 360.f;
    return deg;
}
} // anonymous namespace

ALCompassDial::Params::Params()
:   ring_color("ring_color"),
    tick_color("tick_color"),
    needle_color("needle_color"),
    text_color("text_color"),
    clockwise("clockwise", true),
    show_readout("show_readout", true)
{
}

ALCompassDial::ALCompassDial(const Params& p)
:   LLUICtrl(p),
    mRingColor(p.ring_color),
    mTickColor(p.tick_color),
    mNeedleColor(p.needle_color),
    mTextColor(p.text_color),
    mClockwise(p.clockwise),
    mShowReadout(p.show_readout)
{
}

// ---------------------------------------------------------------------------
// value
// ---------------------------------------------------------------------------
void ALCompassDial::setValue(const LLSD& value)
{
    mValueDegrees = wrap_degrees((F32)value.asReal());
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
    if (fabsf(dx) < 0.5f && fabsf(dy) < 0.5f)
    {
        return;     // dead center: no defined bearing, keep the value
    }
    // atan2(east, north): 0 = up, positive toward +x (screen east)
    F32 deg = atan2f(mClockwise ? dx : -dx, dy) * RAD_TO_DEG;
    mValueDegrees = wrap_degrees(deg);
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
    if (!getEnabled())
    {
        return false;
    }
    // wheel up = +5 deg fine trim (clicks are negative scrolling up)
    mValueDegrees = wrap_degrees(mValueDegrees - (F32)delta.mClicks * 5.f);
    onCommit();
    return true;
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void ALCompassDial::draw()
{
    F32 cx, cy, radius;
    getDialGeometry(cx, cy, radius);

    const F32 alpha = getEnabled() ? 1.f : 0.4f;
    LLColor4 ring = mRingColor.get() % alpha;
    LLColor4 tick = mTickColor.get() % alpha;
    LLColor4 needle = mNeedleColor.get() % alpha;
    LLColor4 text = mTextColor.get() % alpha;

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

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
        gl_line_2d((S32)llround(cx + dx * inner),  (S32)llround(cy + dy * inner),
                   (S32)llround(cx + dx * radius), (S32)llround(cy + dy * radius),
                   major ? ring : tick);
    }

    // needle: hub dot + line toward the current bearing
    const F32 vrad = mValueDegrees * DEG_TO_RAD;
    const F32 ndx = mClockwise ? sinf(vrad) : -sinf(vrad);
    const F32 ndy = cosf(vrad);
    gl_line_2d((S32)llround(cx), (S32)llround(cy),
               (S32)llround(cx + ndx * (radius - 8.f)),
               (S32)llround(cy + ndy * (radius - 8.f)),
               needle);
    gGL.color4fv(needle.mV);
    gl_circle_2d(cx, cy, 2.5f, 12, true);

    // cardinal letters outside the ring
    const LLFontGL* font = LLFontGL::getFontSansSerifSmall();
    if (font)
    {
        const F32 lr = radius + 6.f;
        font->renderUTF8("N", 0, (S32)llround(cx), (S32)llround(cy + lr), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("S", 0, (S32)llround(cx), (S32)llround(cy - lr), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("E", 0, (S32)llround(cx + lr), (S32)llround(cy), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);
        font->renderUTF8("W", 0, (S32)llround(cx - lr), (S32)llround(cy), text,
                         LLFontGL::HCENTER, LLFontGL::VCENTER);

        // secondary numeric readout under the hub
        if (mShowReadout)
        {
            font->renderUTF8(llformat("%d\xC2\xB0", (S32)llround(mValueDegrees)), 0,
                             (S32)llround(cx), (S32)llround(cy - radius * 0.45f), text,
                             LLFontGL::HCENTER, LLFontGL::VCENTER);
        }
    }

    LLUICtrl::draw();
}
