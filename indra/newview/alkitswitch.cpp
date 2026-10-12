#include "llviewerprecompiledheaders.h"
#include "alkitswitch.h"
#include "alkitstyle.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "lluictrlfactory.h"

static LLDefaultChildRegistry::Register<ALKitSwitch> r("kit_switch");

ALKitSwitch::ALKitSwitch(const Params& p) : LLCheckBoxCtrl(p)
{
    ALKitStyle::color(ALKitStyle::WINDOW_BG);

    // LLCheckBoxCtrl normally hit-tests a child-derived bounding rect.  Layout
    // moves controls with setOrigin(), which moves mRect but does not refresh
    // that cached bounding rect.  A kit switch paints strictly inside mRect,
    // so use that live rect for owner hit-testing as well.
    setUseBoundingRect(false);

    if (mLabel)
    {
        mLabel->setVisible(false);
    }

    reshape(getRect().getWidth(), getRect().getHeight(), false);

    mLastState = isOn();

    mHaveState = true;

    mButton->setKeyboardOnlyFocusBorder(true);
}

void ALKitSwitch::reshape(S32 w, S32 h, bool from_parent)
{
    LLCheckBoxCtrl::reshape(w, h, from_parent);

    // Painting and pointer dispatch share this one live local rectangle.  Do
    // this after the stock checkbox layout, which otherwise expands its child
    // button from the hidden label's cached bounds.
    mButton->setRect(getLocalRect());

    if (mLabel)
        mLabel->setVisible(false);

    updateBoundingRect();
}

LLRect ALKitSwitch::drawnTrackRect() const
{
    return getLocalRect();
}

LLRect ALKitSwitch::clickableRect() const
{
    return mButton ? mButton->getRect() : LLRect();
}

void ALKitSwitch::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    const bool value = isOn();
    const bool enabled = getEnabled();
    const LLColor4 track = (enabled ? (value ? ALKitStyle::color(ALKitStyle::ACCENT) : ALKitStyle::color(ALKitStyle::SWITCH_OFF))
                                    : ALKitStyle::color(ALKitStyle::INPUT_BG)).get();
    const LLColor4 knob = (enabled ? (value ? ALKitStyle::color(ALKitStyle::KNOB_ON) : ALKitStyle::color(ALKitStyle::KNOB_OFF))
                                   : ALKitStyle::color(ALKitStyle::FAINT)).get();

    const LLRect track_rect = drawnTrackRect();
    const F32 radius = .5f * llmin(track_rect.getWidth(), track_rect.getHeight());
    const F32 center_y = .5f * (track_rect.mBottom + track_rect.mTop);
    const F32 left_x = track_rect.mLeft + radius;
    const F32 right_x = track_rect.mRight - radius;

    gGL.color4fv(track.mV);
    gl_rect_2d(ll_round(left_x), track_rect.mTop, ll_round(right_x), track_rect.mBottom, track, true);
    gl_circle_2d(left_x, center_y, radius, 20, true);
    gl_circle_2d(right_x, center_y, radius, 20, true);

    gGL.color4fv(knob.mV);
    gl_circle_2d(value ? right_x : left_x, center_y, llmax(0.f, radius - ALKitStyle::SWITCH_INSET), 20, true);

    if (isFocusRingVisible())
    {
        const LLColor4 c = ALKitStyle::color(ALKitStyle::TEXT).get() % .7f;
        gGL.color4fv(c.mV);
        gl_line_2d(ll_round(left_x), track_rect.mTop - 1, ll_round(right_x), track_rect.mTop - 1, c);
        gl_line_2d(ll_round(left_x), track_rect.mBottom + 1, ll_round(right_x), track_rect.mBottom + 1, c);
        const F32 focus_radius = llmax(0.f, radius - 1.f);
        gl_arc_2d(left_x, center_y, focus_radius, 20, false, F_PI_BY_TWO, F_PI + F_PI_BY_TWO);
        gl_arc_2d(right_x, center_y, focus_radius, 20, false, -F_PI_BY_TWO, F_PI_BY_TWO);
    }
}

bool ALKitSwitch::isFocusRingVisible() const
{
    return mButton && mButton->isFocusRingVisible();
}

void ALKitSwitch::noteState(bool value)
{
    if (!mHaveState || value != mLastState)
    {
        mHaveState = true;
        mLastState = value;
        mFlipSignal(value);
    }
}

void ALKitSwitch::setValue(const LLSD& value)
{
    LLCheckBoxCtrl::setValue(value);
    noteState(value.asBoolean());
}

void ALKitSwitch::setControlName(const std::string& name, LLView* context)
{
    mControlConnection.disconnect();
    LLCheckBoxCtrl::setControlName(name, context);

    mLastState = isOn();

    mHaveState = true;

    if (LLControlVariable* control = mButton->getControlVariable())
        mControlConnection =
            control->getSignal()->connect([this](LLControlVariable*, const LLSD& value, const LLSD&) { noteState(value.asBoolean()); });
}

void ALKitSwitch::onCommit()
{
    noteState(isOn());
    LLCheckBoxCtrl::onCommit();
}

void ALKitSwitch::writeBoundValue(bool value)
{
    if (LLControlVariable* control = mButton->getControlVariable())
        control->set(LLSD(value));

    else
    {
        mButton->setToggleState(value);
        noteState(value);
    }
}
