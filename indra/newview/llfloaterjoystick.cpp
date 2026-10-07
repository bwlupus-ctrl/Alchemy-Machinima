/**
 * @file llfloaterjoystick.cpp
 * @brief Joystick preferences panel
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

// file include
#include "llfloaterjoystick.h"

// linden library includes
#include "llerror.h"
#include "llrect.h"
#include "llstring.h"
#include "lltrace.h"

// project includes
#include "aluirefresh.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llappviewer.h"
#include "llviewerjoystick.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llpanel.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"

#if LL_WINDOWS && !LL_MESA_HEADLESS
// Require DirectInput version 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#endif

static LLTrace::SampleStatHandle<>  sJoystickAxis0("Joystick axis 0"),
                                    sJoystickAxis1("Joystick axis 1"),
                                    sJoystickAxis2("Joystick axis 2"),
                                    sJoystickAxis3("Joystick axis 3"),
                                    sJoystickAxis4("Joystick axis 4"),
                                    sJoystickAxis5("Joystick axis 5");
static LLTrace::SampleStatHandle<>* sJoystickAxes[6] =
{
    &sJoystickAxis0,
    &sJoystickAxis1,
    &sJoystickAxis2,
    &sJoystickAxis3,
    &sJoystickAxis4,
    &sJoystickAxis5
};


#if LL_WINDOWS && !LL_MESA_HEADLESS

BOOL CALLBACK di8_list_devices_callback(LPCDIDEVICEINSTANCE device_instance_ptr, LPVOID pvRef)
{
    // Note: If a single device can function as more than one DirectInput
    // device type, it is enumerated as each device type that it supports.
    // Capable of detecting devices like Oculus Rift
    if (device_instance_ptr && pvRef)
    {
        std::string product_name = ll_convert_wide_to_string(std::wstring(device_instance_ptr->tszProductName));
        S32 size = sizeof(GUID);
        LLSD::Binary data; //just an std::vector
        data.resize(size);
        memcpy(&data[0], &device_instance_ptr->guidInstance /*POD _GUID*/, size);

        LLFloaterJoystick * floater = (LLFloaterJoystick*)pvRef;
        LLSD value = data;
        floater->addDevice(product_name, value);
    }
    return DIENUM_CONTINUE;
}
#endif

LLFloaterJoystick::LLFloaterJoystick(const LLSD& data)
    : LLFloater(data),
    LLEventTimer(0.1f),
    mHasDeviceList(false)
{
    if (!LLViewerJoystick::getInstance()->isJoystickInitialized())
    {
        LLViewerJoystick::getInstance()->init(false);
    }

    initFromSettings();
}

void LLFloaterJoystick::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLFloater::draw();
}

bool LLFloaterJoystick::tick()
{
    if (isWidgetOnScreen(this))
    {
        refreshLiveControls();
    }
    return false;
}

void LLFloaterJoystick::refreshLiveControls()
{
    if (!mJoysticksCombo)
    {
        return;
    }

    LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
    if (mXInputStatus && isWidgetOnScreen(mXInputStatus))
    {
        const std::string xinput_status = joystick->getXInputStatus();
        setTextIfChanged(mXInputStatus, xinput_status);
        setToolTipIfChanged(mXInputStatus,
            xinput_status + "\nD-pad Down toggles ROLL / THROTTLE.");
    }

    if (mFlightModel && isWidgetOnScreen(mFlightModel))
    {
        refreshDroneEnabled();
        refreshDroneReadouts();
    }

    if (!isWidgetOnScreen(mJoysticksCombo))
    {
        return;
    }

    bool joystick_inited = joystick->isJoystickInitialized();
    if (joystick_inited != mHasDeviceList)
    {
        refreshListOfDevices();
    }

    if (gFrameIntervalSeconds.value() == 0.0f)
    {
        joystick->updateStatus();
    }

    for (U32 i = 0; i < 6; i++)
    {
        F32 value = joystick->getJoystickAxis(i);
        sample(*sJoystickAxes[i], value);
        if (mAxisStatsBar[i])
        {
            F32 minbar, maxbar;
            mAxisStatsBar[i]->getRange(minbar, maxbar);
            F32 range = llabs(value);
            if (range > maxbar)
            {
                mAxisStatsBar[i]->setRange(-range, range);
            }
        }
    }

    for (U32 i = 0; i < 16; i++)
    {
        U32 value = joystick->getJoystickButton(i);
        if (!mAxisButton[i]->getEnabled() && value)
        {
            mAxisButton[i]->setEnabled(TRUE);
        }
        mAxisButton[i]->setToggleState(value);
    }
}

void LLFloaterJoystick::refreshDronePresetLabel()
{
    if (!mDronePreset)
        return;
    LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
    const U64 revision = joystick->getDronePresetProvenanceRevision();
    if (revision == mDronePresetRevision)
        return;
    mDronePresetRevision = revision;
    if (joystick->isDronePresetCustom())
    {
        static const char* const names[] = { "Cine", "Normal", "Sport", "Dolly", "FPV" };
        const S32 source = llclamp(joystick->getDronePresetProvenance(), 0, 4);
        const std::string label = llformat("Custom (from %s)", names[source]);
        if (mDronePreset->getSimple() != label)
            mDronePreset->setLabel(LLStringExplicit(label));
    }
    else
    {
        static const char* const names[] = { "Cine", "Normal", "Sport", "Dolly", "FPV" };
        const char* label = names[llclamp(joystick->getDronePresetProvenance(), 0, 4)];
        if (mDronePreset->getSimple() != label)
            mDronePreset->setLabel(LLStringExplicit(label));
    }
}

void LLFloaterJoystick::refreshDroneReadouts()
{
    if (!mDroneLimits || !mDroneValidation || !isWidgetOnScreen(mDroneLimits))
        return;

    LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
    const LLFlycamDrone::CompiledValues values = joystick->getDroneCompiledValues();
    const LLFlycamDrone::HumanParams accepted = joystick->getDroneAcceptedValues();
    const F32 tick = joystick->getDroneCompiledTick();
    const F32 min_time = 4.f * tick;
    for (LLSliderCtrl* slider : mDroneTimeSliders)
    {
        if (slider && slider->getMinValue() != min_time)
            slider->setMinValue(min_time);
    }
    const F32 stop_floor = accepted.top * (accepted.onset + 2.f * tick);
    if (mDroneStopDistance && mDroneStopDistance->getMinValue() != stop_floor)
        mDroneStopDistance->setMinValue(stop_floor);

    setTextIfChanged(mDroneLimits, llformat("L_tot %.2f m/s\xC2\xB2   J_eff %.2f m/s\xC2\xB3",
                                            values.totalLimit, values.effectiveJerk));

    std::string reason;
    const struct { U32 bit; const char* name; } fields[] =
    {
        { LLFlycamDrone::CLAMP_CRUISE, "Cruise" }, { LLFlycamDrone::CLAMP_TOP, "Top speed" },
        { LLFlycamDrone::CLAMP_TIME_TO_TOP, "Time to top" }, { LLFlycamDrone::CLAMP_STOP_DISTANCE, "Stop distance" },
        { LLFlycamDrone::CLAMP_HOLD_TIME, "Hold time" }, { LLFlycamDrone::CLAMP_COAST_TIME, "Coast time" },
        { LLFlycamDrone::CLAMP_FLOAT, "Float" }, { LLFlycamDrone::CLAMP_ONSET, "Onset" },
        { LLFlycamDrone::CLAMP_GRIP, "Grip" }, { LLFlycamDrone::CLAMP_TURN_RATE, "Turn rate" },
        { LLFlycamDrone::CLAMP_TURN_WEIGHT, "Turn weight" }, { LLFlycamDrone::CLAMP_TILT_RATE, "Tilt rate" },
        { LLFlycamDrone::CLAMP_CLIMB, "Climb" }, { LLFlycamDrone::CLAMP_CLIMB_TIME, "Climb time" },
        { LLFlycamDrone::CLAMP_SIMULATION_HZ, "Simulation Hz" }
    };
    for (const auto& field : fields)
    {
        if (values.validationClamps & field.bit)
        {
            if (!reason.empty()) reason += ", ";
            reason += field.name;
        }
    }
    if (setTextIfChanged(mDroneValidation, reason.empty() ? std::string() : "Validation adjusted: " + reason))
    {
        const LLRect old_rect = mDroneValidation->getRect();
        mDroneValidation->reshape(old_rect.getWidth(),
            llmax(1, mDroneValidation->getTextPixelHeight()), true);
        mDroneValidation->translate(0, old_rect.mTop - mDroneValidation->getRect().mTop);
        mScrollFit.fit(this, mTabs);
    }
    setVisibleIfChanged(mDroneValidation, !reason.empty());
    refreshDronePresetLabel();
}

void LLFloaterJoystick::refreshDroneEnabled()
{
    if (!mFlightModel || !mDroneSettingsPanel || !mDroneCursorReason)
    {
        return;
    }

    const bool cursor_3d = gSavedSettings.getBOOL("Cursor3D");
    const bool drone = gSavedSettings.getS32("FlycamFlightModel") == 1;
    setEnabledIfChanged(mFlightModel, !cursor_3d);
    setVisibleIfChanged(mDroneCursorReason, cursor_3d);
    setEnabledIfChanged(mDroneSettingsPanel, drone && !cursor_3d);
}

bool LLFloaterJoystick::postBuild()
{
    center();
    F32 range = gSavedSettings.getBOOL("Cursor3D") ? 128.f : 0.5f;

    for (U32 i = 0; i < 6; i++)
    {
        std::string stat_name(llformat("Joystick axis %d", i));
        std::string axisname = llformat("axis%d", i);
        mAxisStatsBar[i] = getChild<LLStatBar>(axisname);
        if (mAxisStatsBar[i])
        {
            mAxisStatsBar[i]->setStat(stat_name);
            mAxisStatsBar[i]->setRange(-range, range);
        }
    }

    for (U32 i = 0; i < 16; i++)
    {
        std::string btn_name = llformat("btn%d", i);
        mAxisButton[i] = getChild<LLButton>(btn_name);
    }

    mJoysticksCombo = getChild<LLComboBox>("joystick_combo");
    childSetCommitCallback("joystick_combo",onCommitJoystickEnabled,this);
    mCheckFlycamEnabled = getChild<LLCheckBoxCtrl>("JoystickFlycamEnabled");
    childSetCommitCallback("JoystickFlycamEnabled",onCommitJoystickEnabled,this);

    childSetAction("SpaceNavigatorDefaults", onClickRestoreSNDefaults, this);
    childSetAction("XboxDefaults", onClickRestoreXboxDefaults, this);
    childSetAction("reset_drone", onClickResetDrone, this);
    childSetAction("cancel_btn", onClickCancel, this);
    childSetAction("ok_btn", onClickOK, this);

    mTabs = getChild<LLTabContainer>("joystick_tabs");
    mXInputStatus = getChild<LLTextBox>("xinput_status");
    mFlightModel = getChild<LLUICtrl>("FlycamFlightModel");
    mDroneSettingsPanel = getChild<LLPanel>("drone_settings_panel");
    mDroneCursorReason = getChild<LLTextBox>("drone_cursor_reason");
    mDronePreset = getChild<LLComboBox>("FlycamDronePreset");
    mDroneLimits = getChild<LLTextBox>("drone_compiled_limits");
    mDroneValidation = getChild<LLTextBox>("drone_validation_reason");
    mDroneTimeSliders[0] = getChild<LLSliderCtrl>("FlycamDroneTimeToTop");
    mDroneTimeSliders[1] = getChild<LLSliderCtrl>("FlycamDroneHoldTime");
    mDroneTimeSliders[2] = getChild<LLSliderCtrl>("FlycamDroneCoastTime");
    mDroneTimeSliders[3] = getChild<LLSliderCtrl>("FlycamDroneOnset");
    mDroneTimeSliders[4] = getChild<LLSliderCtrl>("FlycamDroneClimbTime");
    mDroneStopDistance = getChild<LLSliderCtrl>("FlycamDroneStopDistance");
    childSetCommitCallback("FlycamFlightModel", onCommitFlightModel, this);
    childSetCommitCallback("FlycamDronePreset", onCommitDronePreset, this);

    static const char* const tuning_controls[] =
    {
        "FlycamDroneCruise", "FlycamDroneTop", "FlycamDroneTimeToTop",
        "FlycamDroneStopDistance", "FlycamDroneHoldTime", "FlycamDroneCoastTime",
        "FlycamDroneHighSpeedFloat", "FlycamDroneOnset", "FlycamDroneGrip",
        "FlycamDroneTurnRate", "FlycamDroneTurnWeight", "FlycamDroneTiltRate",
        "FlycamDroneClimb", "FlycamDroneClimbTime", "FlycamDroneRelease",
        "FlycamDroneFreeFly", "FlycamDroneThrottleMode", "FlycamDroneResumeMomentum",
        "FlycamDroneSimulationHz", "FlycamDroneMoveDeadzone", "FlycamDroneLookDeadzone",
        "FlycamDroneSaturation", "FlycamDroneExpoMove", "FlycamDroneExpoYaw",
        "FlycamDroneExpoGimbal"
    };
    for (const char* name : tuning_controls)
    {
        childSetCommitCallback(name, onCommitDroneTuning, this);
    }

    mScrollFit.installForTabs(mTabs, this,
        { { "drone_scroll", "drone_scroll_content" } });

    refresh();
    refreshListOfDevices();
    refreshLiveControls();
    return TRUE;
}

void LLFloaterJoystick::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    mEventTimer.start();
    refresh();
    mScrollFit.fit(this, mTabs);
    refreshLiveControls();
}

LLFloaterJoystick::~LLFloaterJoystick()
{
    // Children all cleaned up by default view destructor.
}


void LLFloaterJoystick::apply()
{
}

void LLFloaterJoystick::initFromSettings()
{
    mXInputSnapshot["JoystickXInputEnabled"] = gSavedSettings.getControl("JoystickXInputEnabled")->getValue();
    mXInputSnapshot["JoystickXInputRollShaped"] = gSavedSettings.getControl("JoystickXInputRollShaped")->getValue();
    mXInputSnapshot["JoystickXInputThrottleMin"] = gSavedSettings.getControl("JoystickXInputThrottleMin")->getValue();
    mXInputSnapshot["JoystickXInputThrottleMax"] = gSavedSettings.getControl("JoystickXInputThrottleMax")->getValue();
    mXInputSnapshot["JoystickXInputTriggerDeadZone"] = gSavedSettings.getControl("JoystickXInputTriggerDeadZone")->getValue();
    mXInputSnapshot["JoystickXInputTriggerCurve"] = gSavedSettings.getControl("JoystickXInputTriggerCurve")->getValue();
    mXInputSnapshot["JoystickXInputDpadLeft"] = gSavedSettings.getControl("JoystickXInputDpadLeft")->getValue();
    mXInputSnapshot["JoystickXInputDpadRight"] = gSavedSettings.getControl("JoystickXInputDpadRight")->getValue();
    mXInputSnapshot["JoystickXInputDpadUp"] = gSavedSettings.getControl("JoystickXInputDpadUp")->getValue();
    mXInputSnapshot["JoystickXInputRumble"] = gSavedSettings.getControl("JoystickXInputRumble")->getValue();
    mXInputSnapshot["JoystickXInputRumbleStrength"] = gSavedSettings.getControl("JoystickXInputRumbleStrength")->getValue();
    static const char* const drone_controls[] =
    {
        "FlycamFlightModel", "FlycamDronePreset", "FlycamDroneCruise", "FlycamDroneTop",
        "FlycamDroneTimeToTop", "FlycamDroneStopDistance", "FlycamDroneHoldTime",
        "FlycamDroneCoastTime", "FlycamDroneHighSpeedFloat", "FlycamDroneOnset",
        "FlycamDroneGrip", "FlycamDroneTurnRate", "FlycamDroneTurnWeight",
        "FlycamDroneTiltRate", "FlycamDroneClimb", "FlycamDroneClimbTime",
        "FlycamDroneRelease", "FlycamDroneThrottleMode", "FlycamDroneFreeFly",
        "FlycamDroneSimulationHz", "FlycamDroneMoveDeadzone", "FlycamDroneLookDeadzone",
        "FlycamDroneSaturation", "FlycamDroneExpoMove", "FlycamDroneExpoYaw",
        "FlycamDroneExpoGimbal", "FlycamDroneResumeMomentum",
        "FlycamDroneInvertLook" // snapshot/restore on Cancel only; deliberately NOT a tuning->Custom control
    };
    for (const char* name : drone_controls)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            mXInputSnapshot[name] = control->getValue();
        }
    }
    mJoystickEnabled = gSavedSettings.getBOOL("JoystickEnabled");
    mAutoFlycamEnabled = gSavedSettings.getBOOL("JoystickAutoFlycamEnabled");
    mJoystickId = gSavedSettings.getLLSD("JoystickDeviceUUID");

    mJoystickAxis[0] = gSavedSettings.getS32("JoystickAxis0");
    mJoystickAxis[1] = gSavedSettings.getS32("JoystickAxis1");
    mJoystickAxis[2] = gSavedSettings.getS32("JoystickAxis2");
    mJoystickAxis[3] = gSavedSettings.getS32("JoystickAxis3");
    mJoystickAxis[4] = gSavedSettings.getS32("JoystickAxis4");
    mJoystickAxis[5] = gSavedSettings.getS32("JoystickAxis5");
    mJoystickAxis[6] = gSavedSettings.getS32("JoystickAxis6");

    m3DCursor = gSavedSettings.getBOOL("Cursor3D");
    mAutoLeveling = gSavedSettings.getBOOL("AutoLeveling");
    mZoomDirect  = gSavedSettings.getBOOL("ZoomDirect");

    mAvatarEnabled = gSavedSettings.getBOOL("JoystickAvatarEnabled");
    mBuildEnabled = gSavedSettings.getBOOL("JoystickBuildEnabled");
    mFlycamEnabled = gSavedSettings.getBOOL("JoystickFlycamEnabled");

    mAvatarAxisScale[0] = gSavedSettings.getF32("AvatarAxisScale0");
    mAvatarAxisScale[1] = gSavedSettings.getF32("AvatarAxisScale1");
    mAvatarAxisScale[2] = gSavedSettings.getF32("AvatarAxisScale2");
    mAvatarAxisScale[3] = gSavedSettings.getF32("AvatarAxisScale3");
    mAvatarAxisScale[4] = gSavedSettings.getF32("AvatarAxisScale4");
    mAvatarAxisScale[5] = gSavedSettings.getF32("AvatarAxisScale5");

    mBuildAxisScale[0] = gSavedSettings.getF32("BuildAxisScale0");
    mBuildAxisScale[1] = gSavedSettings.getF32("BuildAxisScale1");
    mBuildAxisScale[2] = gSavedSettings.getF32("BuildAxisScale2");
    mBuildAxisScale[3] = gSavedSettings.getF32("BuildAxisScale3");
    mBuildAxisScale[4] = gSavedSettings.getF32("BuildAxisScale4");
    mBuildAxisScale[5] = gSavedSettings.getF32("BuildAxisScale5");

    mFlycamAxisScale[0] = gSavedSettings.getF32("FlycamAxisScale0");
    mFlycamAxisScale[1] = gSavedSettings.getF32("FlycamAxisScale1");
    mFlycamAxisScale[2] = gSavedSettings.getF32("FlycamAxisScale2");
    mFlycamAxisScale[3] = gSavedSettings.getF32("FlycamAxisScale3");
    mFlycamAxisScale[4] = gSavedSettings.getF32("FlycamAxisScale4");
    mFlycamAxisScale[5] = gSavedSettings.getF32("FlycamAxisScale5");
    mFlycamAxisScale[6] = gSavedSettings.getF32("FlycamAxisScale6");

    mAvatarAxisDeadZone[0] = gSavedSettings.getF32("AvatarAxisDeadZone0");
    mAvatarAxisDeadZone[1] = gSavedSettings.getF32("AvatarAxisDeadZone1");
    mAvatarAxisDeadZone[2] = gSavedSettings.getF32("AvatarAxisDeadZone2");
    mAvatarAxisDeadZone[3] = gSavedSettings.getF32("AvatarAxisDeadZone3");
    mAvatarAxisDeadZone[4] = gSavedSettings.getF32("AvatarAxisDeadZone4");
    mAvatarAxisDeadZone[5] = gSavedSettings.getF32("AvatarAxisDeadZone5");

    mBuildAxisDeadZone[0] = gSavedSettings.getF32("BuildAxisDeadZone0");
    mBuildAxisDeadZone[1] = gSavedSettings.getF32("BuildAxisDeadZone1");
    mBuildAxisDeadZone[2] = gSavedSettings.getF32("BuildAxisDeadZone2");
    mBuildAxisDeadZone[3] = gSavedSettings.getF32("BuildAxisDeadZone3");
    mBuildAxisDeadZone[4] = gSavedSettings.getF32("BuildAxisDeadZone4");
    mBuildAxisDeadZone[5] = gSavedSettings.getF32("BuildAxisDeadZone5");

    mFlycamAxisDeadZone[0] = gSavedSettings.getF32("FlycamAxisDeadZone0");
    mFlycamAxisDeadZone[1] = gSavedSettings.getF32("FlycamAxisDeadZone1");
    mFlycamAxisDeadZone[2] = gSavedSettings.getF32("FlycamAxisDeadZone2");
    mFlycamAxisDeadZone[3] = gSavedSettings.getF32("FlycamAxisDeadZone3");
    mFlycamAxisDeadZone[4] = gSavedSettings.getF32("FlycamAxisDeadZone4");
    mFlycamAxisDeadZone[5] = gSavedSettings.getF32("FlycamAxisDeadZone5");
    mFlycamAxisDeadZone[6] = gSavedSettings.getF32("FlycamAxisDeadZone6");

    mAvatarFeathering = gSavedSettings.getF32("AvatarFeathering");
    mBuildFeathering = gSavedSettings.getF32("BuildFeathering");
    mFlycamFeathering = gSavedSettings.getF32("FlycamFeathering");
}

void LLFloaterJoystick::refresh()
{
    LLFloater::refresh();

    initFromSettings();
}

bool LLFloaterJoystick::addDeviceCallback(std::string &name, LLSD& value, void* userdata)
{
    LLFloaterJoystick * floater = (LLFloaterJoystick*)userdata;
    floater->mJoysticksCombo->add(name, value, ADD_BOTTOM, 1);
    return false; // keep searching
}

void LLFloaterJoystick::addDevice(std::string &name, LLSD& value)
{
    mJoysticksCombo->add(name, value, ADD_BOTTOM, 1);
}

void LLFloaterJoystick::refreshListOfDevices()
{
    mJoysticksCombo->removeall();
    std::string no_device = getString("JoystickDisabled");
    LLSD value = LLSD::Integer(0);
    addDevice(no_device, value);

    mHasDeviceList = false;

    void* win_calback = nullptr;
    // di8_devices_callback callback is immediate and happens in scope of getInputDevices()
#if LL_WINDOWS && !LL_MESA_HEADLESS
    // space navigator is marked as DI8DEVCLASS_GAMECTRL in ndof lib
    U32 device_type = DI8DEVCLASS_GAMECTRL;
    win_calback = di8_list_devices_callback;
#elif LL_DARWIN
    U32 device_type = 0;
#else
    // On MAC it is possible to specify product
    // and manufacturer in NDOF_Device for
    // ndof_init_first to pick specific device
    U32 device_type = 0;
#endif
    if (gViewerWindow->getWindow()->getInputDevices(device_type, addDeviceCallback, win_calback, this))
    {
        mHasDeviceList = true;
    }

    bool is_device_id_set = LLViewerJoystick::getInstance()->isDeviceUUIDSet();

    if (LLViewerJoystick::getInstance()->isJoystickInitialized() &&
        (!mHasDeviceList || !is_device_id_set))
    {
#if LL_WINDOWS && !LL_MESA_HEADLESS
        LL_WARNS() << "NDOF connected to device without using SL provided handle" << LL_ENDL;
#endif
        std::string desc = LLViewerJoystick::getInstance()->getDescription();
        if (!desc.empty())
        {
            LLSD value = LLSD::Integer(1); // value for selection
            addDevice(desc, value);
            mHasDeviceList = true;
        }
    }

    if (gSavedSettings.getBOOL("JoystickEnabled") && mHasDeviceList)
    {
        if (is_device_id_set)
        {
            LLSD guid = LLViewerJoystick::getInstance()->getDeviceUUID();
            mJoysticksCombo->selectByValue(guid);
        }
        else
        {
            mJoysticksCombo->selectByValue(LLSD::Integer(1));
        }
    }
    else
    {
        mJoysticksCombo->selectByValue(LLSD::Integer(0));
    }
}

void LLFloaterJoystick::cancel()
{
    for (LLSD::map_const_iterator it = mXInputSnapshot.beginMap(); it != mXInputSnapshot.endMap(); ++it)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(it->first))
        {
            control->setValue(it->second);
        }
    }
    gSavedSettings.setBOOL("JoystickEnabled", mJoystickEnabled);
    gSavedSettings.setBOOL("JoystickAutoFlycamEnabled", mAutoFlycamEnabled);
    gSavedSettings.setLLSD("JoystickDeviceUUID", mJoystickId);

    gSavedSettings.setS32("JoystickAxis0", mJoystickAxis[0]);
    gSavedSettings.setS32("JoystickAxis1", mJoystickAxis[1]);
    gSavedSettings.setS32("JoystickAxis2", mJoystickAxis[2]);
    gSavedSettings.setS32("JoystickAxis3", mJoystickAxis[3]);
    gSavedSettings.setS32("JoystickAxis4", mJoystickAxis[4]);
    gSavedSettings.setS32("JoystickAxis5", mJoystickAxis[5]);
    gSavedSettings.setS32("JoystickAxis6", mJoystickAxis[6]);

    gSavedSettings.setBOOL("Cursor3D", m3DCursor);
    gSavedSettings.setBOOL("AutoLeveling", mAutoLeveling);
    gSavedSettings.setBOOL("ZoomDirect", mZoomDirect );

    gSavedSettings.setBOOL("JoystickAvatarEnabled", mAvatarEnabled);
    gSavedSettings.setBOOL("JoystickBuildEnabled", mBuildEnabled);
    gSavedSettings.setBOOL("JoystickFlycamEnabled", mFlycamEnabled);

    gSavedSettings.setF32("AvatarAxisScale0", mAvatarAxisScale[0]);
    gSavedSettings.setF32("AvatarAxisScale1", mAvatarAxisScale[1]);
    gSavedSettings.setF32("AvatarAxisScale2", mAvatarAxisScale[2]);
    gSavedSettings.setF32("AvatarAxisScale3", mAvatarAxisScale[3]);
    gSavedSettings.setF32("AvatarAxisScale4", mAvatarAxisScale[4]);
    gSavedSettings.setF32("AvatarAxisScale5", mAvatarAxisScale[5]);

    gSavedSettings.setF32("BuildAxisScale0", mBuildAxisScale[0]);
    gSavedSettings.setF32("BuildAxisScale1", mBuildAxisScale[1]);
    gSavedSettings.setF32("BuildAxisScale2", mBuildAxisScale[2]);
    gSavedSettings.setF32("BuildAxisScale3", mBuildAxisScale[3]);
    gSavedSettings.setF32("BuildAxisScale4", mBuildAxisScale[4]);
    gSavedSettings.setF32("BuildAxisScale5", mBuildAxisScale[5]);

    gSavedSettings.setF32("FlycamAxisScale0", mFlycamAxisScale[0]);
    gSavedSettings.setF32("FlycamAxisScale1", mFlycamAxisScale[1]);
    gSavedSettings.setF32("FlycamAxisScale2", mFlycamAxisScale[2]);
    gSavedSettings.setF32("FlycamAxisScale3", mFlycamAxisScale[3]);
    gSavedSettings.setF32("FlycamAxisScale4", mFlycamAxisScale[4]);
    gSavedSettings.setF32("FlycamAxisScale5", mFlycamAxisScale[5]);
    gSavedSettings.setF32("FlycamAxisScale6", mFlycamAxisScale[6]);

    gSavedSettings.setF32("AvatarAxisDeadZone0", mAvatarAxisDeadZone[0]);
    gSavedSettings.setF32("AvatarAxisDeadZone1", mAvatarAxisDeadZone[1]);
    gSavedSettings.setF32("AvatarAxisDeadZone2", mAvatarAxisDeadZone[2]);
    gSavedSettings.setF32("AvatarAxisDeadZone3", mAvatarAxisDeadZone[3]);
    gSavedSettings.setF32("AvatarAxisDeadZone4", mAvatarAxisDeadZone[4]);
    gSavedSettings.setF32("AvatarAxisDeadZone5", mAvatarAxisDeadZone[5]);

    gSavedSettings.setF32("BuildAxisDeadZone0", mBuildAxisDeadZone[0]);
    gSavedSettings.setF32("BuildAxisDeadZone1", mBuildAxisDeadZone[1]);
    gSavedSettings.setF32("BuildAxisDeadZone2", mBuildAxisDeadZone[2]);
    gSavedSettings.setF32("BuildAxisDeadZone3", mBuildAxisDeadZone[3]);
    gSavedSettings.setF32("BuildAxisDeadZone4", mBuildAxisDeadZone[4]);
    gSavedSettings.setF32("BuildAxisDeadZone5", mBuildAxisDeadZone[5]);

    gSavedSettings.setF32("FlycamAxisDeadZone0", mFlycamAxisDeadZone[0]);
    gSavedSettings.setF32("FlycamAxisDeadZone1", mFlycamAxisDeadZone[1]);
    gSavedSettings.setF32("FlycamAxisDeadZone2", mFlycamAxisDeadZone[2]);
    gSavedSettings.setF32("FlycamAxisDeadZone3", mFlycamAxisDeadZone[3]);
    gSavedSettings.setF32("FlycamAxisDeadZone4", mFlycamAxisDeadZone[4]);
    gSavedSettings.setF32("FlycamAxisDeadZone5", mFlycamAxisDeadZone[5]);
    gSavedSettings.setF32("FlycamAxisDeadZone6", mFlycamAxisDeadZone[6]);

    gSavedSettings.setF32("AvatarFeathering", mAvatarFeathering);
    gSavedSettings.setF32("BuildFeathering", mBuildFeathering);
    gSavedSettings.setF32("FlycamFeathering", mFlycamFeathering);
}

void LLFloaterJoystick::onCommitJoystickEnabled(LLUICtrl*, void *joy_panel)
{
    LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;

    LLSD value = self->mJoysticksCombo->getValue();
    bool joystick_enabled = true;
    // value is 0 for no device,
    // 1 for a device on Mac (single device, no list support yet)
    // binary packed guid for a device on windows (can have multiple devices)
    if (value.isInteger())
    {
        // ndof already has a device selected, we are just setting it enabled or disabled
        joystick_enabled = value.asInteger();
    }
    else
    {
        LLViewerJoystick::getInstance()->initDevice(value);
        // else joystick is enabled, because combobox holds id of the device
        joystick_enabled = true;
    }
    gSavedSettings.setBOOL("JoystickEnabled", joystick_enabled);
    BOOL flycam_enabled = self->mCheckFlycamEnabled->get();

    if (!joystick_enabled || !flycam_enabled)
    {
        // Turn off flycam
        LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
        if (joystick->getOverrideCamera())
        {
            joystick->toggleFlycam();
        }
    }

    LLViewerJoystick::getInstance()->saveDeviceIdToSettings();

    std::string device_string = LLViewerJoystick::getInstance()->getDeviceUUIDString();
    LL_DEBUGS("Joystick") << "Selected " << device_string << " as joystick." << LL_ENDL;

    self->refreshListOfDevices();
}

void LLFloaterJoystick::onCommitDroneTuning(LLUICtrl*, void* userdata)
{
    LLFloaterJoystick* self = static_cast<LLFloaterJoystick*>(userdata);
    LLViewerJoystick::getInstance()->markDronePresetCustom();
    if (self)
        self->refreshDronePresetLabel();
}

void LLFloaterJoystick::onCommitFlightModel(LLUICtrl*, void* userdata)
{
    LLFloaterJoystick* self = static_cast<LLFloaterJoystick*>(userdata);
    if (self)
    {
        self->refreshDroneEnabled();
    }
}

void LLFloaterJoystick::onCommitDronePreset(LLUICtrl* control, void* userdata)
{
    const S32 preset = control ? control->getValue().asInteger() : 5;
    if (preset >= 0 && preset < 5)
    {
        LLViewerJoystick::getInstance()->applyDronePreset(preset);
    }
}

void LLFloaterJoystick::onClickRestoreSNDefaults(void *joy_panel)
{
    setSNDefaults();
}

void LLFloaterJoystick::onClickRestoreXboxDefaults(void* joy_panel)
{
    setXboxDefaults();
}

void LLFloaterJoystick::onClickResetDrone(void* userdata)
{
    // WP4 owns the preset/reset implementation and exposes this entry point.
    LLViewerJoystick::getInstance()->resetDroneSettings();
    LLFloaterJoystick* self = static_cast<LLFloaterJoystick*>(userdata);
    if (self)
    {
        self->refreshDroneEnabled();
    }
}

void LLFloaterJoystick::onClickCancel(void *joy_panel)
{
    if (joy_panel)
    {
        LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;

        if (self)
        {
            self->cancel();
            self->closeFloater();
        }
    }
}

void LLFloaterJoystick::onClickOK(void *joy_panel)
{
    if (joy_panel)
    {
        LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;

        if (self)
        {
            self->closeFloater();
        }
    }
}

void LLFloaterJoystick::onClickCloseBtn(bool app_quitting)
{
    cancel();
    closeFloater(app_quitting);
}

void LLFloaterJoystick::setSNDefaults()
{
    LLViewerJoystick::getInstance()->setSNDefaults();
}

void LLFloaterJoystick::setXboxDefaults()
{
    LLViewerJoystick::getInstance()->setXboxDefaults();
}

void LLFloaterJoystick::onClose(bool app_quitting)
{
    mEventTimer.stop();
    if (app_quitting)
    {
        cancel();
    }
}
