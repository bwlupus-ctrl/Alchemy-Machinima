/**
 * @file llfloaterjoystick.h
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

#ifndef LL_LLFLOATERJOYSTICK_H
#define LL_LLFLOATERJOYSTICK_H

#include "alscrollfit.h"
#include "lleventtimer.h"
#include "llfloater.h"
#include "llstatview.h"

class LLCheckBoxCtrl;
class LLComboBox;
class LLPanel;
class LLTabContainer;
class LLTextBox;
class LLSliderCtrl;
class LLUICtrl;

class LLFloaterJoystick final : public LLFloater, public LLEventTimer
{
    friend class LLFloaterReg;

public:

    bool postBuild() override;   // [fork] base class migrated BOOL->bool
    void onOpen(const LLSD& key) override;
    void refresh() override;
    void apply() override;   // Apply the changed values.
    void cancel() override;  // Cancel the changed values.
    void draw() override;
    bool tick() override;
    static  void setSNDefaults();
    static  void setXboxDefaults();

    static bool addDeviceCallback(std::string &name, LLSD& value, void* userdata);
    void addDevice(std::string &name, LLSD& value);

protected:

    void refreshListOfDevices();
    void onClose(bool app_quitting);
    void onClickCloseBtn(bool app_quitting);

private:

    LLFloaterJoystick(const LLSD& data);
    virtual ~LLFloaterJoystick();

    void initFromSettings();
    void refreshLiveControls();
    void refreshDroneEnabled();
    void refreshDroneReadouts();
    void refreshDronePresetLabel();

    static void onCommitJoystickEnabled(LLUICtrl*, void*);
    static void onCommitFlightModel(LLUICtrl*, void*);
    static void onCommitDronePreset(LLUICtrl*, void*);
    static void onCommitDroneTuning(LLUICtrl*, void*);
    static void onClickRestoreSNDefaults(void*);
    static void onClickRestoreXboxDefaults(void*);
    static void onClickResetDrone(void*);
    static void onClickCancel(void*);
    static void onClickOK(void*);

private:
    // Device prefs
    bool mJoystickEnabled;
    bool mAutoFlycamEnabled;
    LLSD mXInputSnapshot; // XInput + Drone controls; excludes pad-owned TriggerMode
    LLSD mJoystickId;
    S32 mJoystickAxis[7];
    bool m3DCursor;
    bool mAutoLeveling;
    bool mZoomDirect;

    // Modes prefs
    bool mAvatarEnabled;
    bool mBuildEnabled;
    bool mFlycamEnabled;
    F32 mAvatarAxisScale[6];
    F32 mBuildAxisScale[6];
    F32 mFlycamAxisScale[7];
    F32 mAvatarAxisDeadZone[6];
    F32 mBuildAxisDeadZone[6];
    F32 mFlycamAxisDeadZone[7];
    F32 mAvatarFeathering;
    F32 mBuildFeathering;
    F32 mFlycamFeathering;

    // Controls that can disable the flycam
    LLCheckBoxCtrl  *mCheckFlycamEnabled;
    LLComboBox      *mJoysticksCombo;
    LLTabContainer  *mTabs = nullptr;
    LLTextBox       *mXInputStatus = nullptr;
    LLUICtrl        *mFlightModel = nullptr;
    LLPanel         *mDroneSettingsPanel = nullptr;
    LLTextBox       *mDroneCursorReason = nullptr;
    LLComboBox      *mDronePreset = nullptr;
    LLTextBox       *mDroneLimits = nullptr;
    LLTextBox       *mDroneValidation = nullptr;
    LLSliderCtrl    *mDroneTimeSliders[5]{};
    LLSliderCtrl    *mDroneStopDistance = nullptr;

    bool mHasDeviceList;

    ALScrollFit::Controller mScrollFit;

    // stats view
    LLStatBar* mAxisStatsBar[6]{};

    LLButton* mAxisButton[16]{};
};

#endif
