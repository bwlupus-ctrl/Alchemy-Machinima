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

    const F32 alpha = getEnabled() ? 1.f : .4f;

    const LLColor4 track = (isOn() ? ALKitStyle::color(ALKitStyle::ACCENT) : ALKitStyle::color(ALKitStyle::SWITCH_OFF)).get() % alpha;

    const LLColor4 knob = (isOn() ? ALKitStyle::color(ALKitStyle::KNOB_ON) : ALKitStyle::color(ALKitStyle::KNOB_OFF)).get() % alpha;

    gGL.color4fv(track.mV);
    gl_rect_2d(9, ALKitStyle::SWITCH_H, 25, 0, track, true);
    gl_circle_2d(9.f, 9.f, 9.f, 20, true);
    gl_circle_2d(25.f, 9.f, 9.f, 20, true);

    gGL.color4fv(knob.mV);
    gl_circle_2d(isOn() ? 25.f : 9.f, 9.f, 7.f, 20, true);

    if (hasFocus() || mButton->hasFocus())
    {
        const LLColor4 c = ALKitStyle::color(ALKitStyle::ACCENT).get() % alpha;
        gGL.color4fv(c.mV);
        gl_circle_2d(9.f, 9.f, 9.f, 20, false);
        gl_circle_2d(25.f, 9.f, 9.f, 20, false);
        gl_line_2d(9, 18, 25, 18, c);
        gl_line_2d(9, 0, 25, 0, c);
    }
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
