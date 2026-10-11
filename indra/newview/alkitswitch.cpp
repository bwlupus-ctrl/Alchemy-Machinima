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

    mButton->setRect(LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0));

    if (mLabel)
    {
        mLabel->setVisible(false);
    }

    mLastState = isOn();

    mHaveState = true;

    mButton->setKeyboardOnlyFocusBorder(true);
}

void ALKitSwitch::reshape(S32 w, S32 h, bool from_parent)
{
    LLCheckBoxCtrl::reshape(w, h, from_parent);

    mButton->setRect(LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0));

    if (mLabel)
        mLabel->setVisible(false);
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

    gGL.color4fv(track.mV);
    gl_rect_2d(9, ALKitStyle::SWITCH_H, 25, 0, track, true);
    gl_circle_2d(9.f, 9.f, 9.f, 20, true);
    gl_circle_2d(25.f, 9.f, 9.f, 20, true);

    gGL.color4fv(knob.mV);
    gl_circle_2d(value ? 25.f : 9.f, 9.f, 7.f, 20, true);

    if (isFocusRingVisible())
    {
        const LLColor4 c = ALKitStyle::color(ALKitStyle::ACCENT).get();
        gGL.color4fv(c.mV);
        gl_line_2d(9, ALKitStyle::SWITCH_H, 25, ALKitStyle::SWITCH_H, c);
        gl_line_2d(9, 0, 25, 0, c);
        gl_arc_2d(9.f, 9.f, 9.f, 20, false, F_PI_BY_TWO, F_PI + F_PI_BY_TWO);
        gl_arc_2d(25.f, 9.f, 9.f, 20, false, -F_PI_BY_TWO, F_PI_BY_TWO);
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
