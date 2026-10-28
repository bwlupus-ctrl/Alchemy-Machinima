#!/usr/bin/env python3
"""CineCam Object Lock settings, UI and no-avatar dispatch contracts."""
import json
import re
import unittest
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"
DEFAULTS = {
    "LockTarget": ("String", ""),
    "OffsetForward": ("F32", 0.0),
    "OffsetLeft": ("F32", 0.0),
    "OffsetUp": ("F32", 0.3),
    "AimYaw": ("F32", 0.0),
    "AimPitch": ("F32", 0.0),
    "FollowYaw": ("Boolean", 1),
    "FollowPitch": ("Boolean", 0),
    "FollowRoll": ("Boolean", 0),
    "Smoothing": ("F32", 0.0),
}


def settings():
    children = list(ET.parse(VIEW / "app_settings/settings.xml").getroot().find("map"))
    result = {}
    for key, value in zip(children[::2], children[1::2]):
        fields = list(value)
        result[key.text] = {k.text: v.text or "" for k, v in zip(fields[::2], fields[1::2])}
    return result


class ObjectLockContracts(unittest.TestCase):
    def test_defaults_persistence_and_shot_ownership(self):
        values = settings()
        for suffix, (kind, default) in DEFAULTS.items():
            with self.subTest(suffix=suffix):
                value = values["CinematicCamObject" + suffix]
                self.assertEqual(value["Type"], kind)
                self.assertEqual(value["Persist"], "1")
                self.assertEqual(value["Value"] if kind == "String" else float(value["Value"]), default)
                self.assertIn("Shot key", value["Comment"])

    def test_all_settings_registered_and_scene_saved(self):
        panel = (VIEW / "alpanelcinecamparams.cpp").read_text(encoding="utf-8")
        scene = (VIEW / "llfloaterdirector.cpp").read_text(encoding="utf-8")
        for suffix in DEFAULTS:
            name = '"CinematicCamObject' + suffix + '"'
            self.assertIn(name, panel)
            self.assertIn(name, scene)
        # New targets and transforms cannot be inherited from a look preset.
        self.assertIn('if (setting.rfind("CinematicCamObject", 0) == 0) return;', panel)

    def test_no_avatar_dispatch_and_hold_ownership(self):
        source = (VIEW / "llcinematiccamera.cpp").read_text(encoding="utf-8")
        tick = source.split("void LLCinematicCamera::updateCamera()", 1)[1]
        # The avatar bypass is now before shared take bookkeeping; dispatch is
        # after it so Object Lock cannot silently skip switcher serial/easing.
        self.assertIn("LLVOAvatar* av = object_lock ? nullptr : resolveTarget();", tick)
        self.assertIn("if (!object_lock && !av)", tick)
        self.assertIn("object_lock ? LLUUID((std::string)object_target) : av->getID()", tick)
        branch = tick.index("if (object_lock)")
        self.assertLess(tick.index("mCutEase.begin("), branch)
        self.assertLess(tick.index("mLastSwitcherCutSerial = switcher_serial;"), branch)
        self.assertLess(tick.index("mConsumedTriggerSerial = mTriggerSerial;"), branch)
        self.assertLess(branch, tick.index("LLVector3 focus = av->getPositionAgent()"))
        self.assertRegex(tick[branch:], r"updateObjectLock\(fresh_activation \|\| mode_changed,\s*"
                         r"target_changed \|\| serial_changed \|\| trigger_changed\);\s*return;")
        object_tick = source.split("void LLCinematicCamera::updateObjectLock(", 1)[1].split(
            "void LLCinematicCamera::triggerMode", 1)[0]
        self.assertIn("mCutEase.apply(", object_tick)
        self.assertIn("mCutEase.blendFov(", object_tick)
        self.assertIn("ALCineCamPresentation::writeCamera(*cam", object_tick)
        self.assertNotIn("mCutEase.mActive = false", object_tick)
        # Main recording still consumes the final, post-move main camera.
        dispatch = (VIEW / "llappviewer.cpp").read_text(encoding="utf-8")
        self.assertLess(dispatch.index("gPipeline.updateMove();"),
                        dispatch.index("LLCinematicCamera::instance().updateCamera();"))
        self.assertLess(dispatch.index("LLCinematicCamera::instance().updateCamera();"),
                        dispatch.index("LLFlycamRecorder::instance().onIdleFrame();"))
        active = source.split("bool LLCinematicCamera::isActive() const", 1)[1].split("\n}", 1)[0]
        self.assertIn("effective_mode == MODE_OBJECT_LOCK || resolveTarget() != nullptr", active)

    def test_exact_prim_lookup_and_rejection(self):
        source = (VIEW / "llcinematiccamera.cpp").read_text(encoding="utf-8")
        lock = source.split("void LLCinematicCamera::lockToObject", 1)[1].split("void LLCinematicCamera::releaseObjectLock", 1)[0]
        tick = source.split("void LLCinematicCamera::updateObjectLock(", 1)[1].split("void LLCinematicCamera::triggerMode", 1)[0]
        self.assertIn('object->getID().asString()', lock)
        self.assertNotIn("getRoot", lock)
        self.assertIn("gObjectList.findObject(object_id)", tick)
        for rejection in ("!object->isAvatar()", "!object->isHUDAttachment()", "!object->isDead()"):
            self.assertIn(rejection, tick)
        self.assertIn("objectDrawablePose(*object->mDrawable, pose)", tick)
        self.assertIn("mObjectPose.update(object_id, render_valid ? &pose : nullptr", tick)

    def test_legacy_and_director_controls(self):
        legacy = ET.parse(XUI / "panel_cinecam_params.xml").getroot()
        mode = legacy.find(".//combo_box[@name='CinematicCamMode']")
        self.assertIsNotNone(mode.find("combo_box.item[@value='52']"))
        block = legacy.find("panel[@name='panel_mode_object']")
        self.assertIsNotNone(block)
        director = ET.parse(XUI / "panel_dpage_camera_shot.xml").getroot()
        option = director.find(".//kit_option[@value='52']")
        self.assertIsNotNone(option)
        for suffix in DEFAULTS:
            if suffix == "LockTarget":
                continue
            name = "CinematicCamObject" + suffix
            self.assertIsNotNone(block.find(f".//*[@control_name='{name}']"))
            self.assertIsNotNone(option.find(f"kit_row[@setting='{name}']"))
            self.assertIsNotNone(block.find(f"button[@name='{name}_reset']"))
        for name in ("cc_object_lock", "cc_object_release", "cc_object_status"):
            self.assertIsNotNone(block.find(f".//*[@name='{name}']"))
            self.assertIsNotNone(option.find(f"kit_row[@widget='{name}']"))
        self.assertEqual(len(option.findall("kit_row")), 12)
        for row in option.findall("kit_row"):
            self.assertIn("spin bottle ride along follow yaw pitch roll", row.get("keywords", ""))

    def test_context_menu_keeps_vcam_and_main_camera_separate(self):
        menu = ET.parse(XUI / "menu_object.xml").getroot()
        for action in ("Object.LockVCam", "Object.ReleaseVCam", "Object.LockCamera", "Object.ReleaseCamera"):
            self.assertIsNotNone(menu.find(f".//menu_item_call.on_click[@function='{action}']"))

    def test_disabled_mode_selection_tracks_previous_enable_state(self):
        source = (VIEW / "llcinematiccamera.cpp").read_text(encoding="utf-8")
        observer = source.split("LLCinematicCamera::LLCinematicCamera()", 1)[1].split(
            "bool LLCinematicCamera::objectLockActive()", 1)[0]
        self.assertIn('mode->getSignal()->connect', observer)
        self.assertIn('gSavedSettings.getBOOL("CinematicCamEnabled")', observer)
        self.assertIn('cameraEnableBaseline(mObjectPreviousEnabled)', observer)
        self.assertIn('mObjectHavePrevious = false', observer)

    def test_disabled_release_ui_and_atomic_restore(self):
        source = (VIEW / "llcinematiccamera.cpp").read_text(encoding="utf-8")
        configured = source.split("bool LLCinematicCamera::objectLockConfigured() const", 1)[1].split("\n}", 1)[0]
        self.assertNotIn("CinematicCamEnabled", configured)
        self.assertNotIn("isDrivingCamera", configured)
        self.assertIn("DirectorSwitcherArmed", configured)
        self.assertIn("activeMode()", configured)
        self.assertIn("ALCineCamObjectLock::isConfigured", configured)
        release = source.split("void LLCinematicCamera::releaseObjectLock()", 1)[1].split(
            "std::string LLCinematicCamera::objectLockStatus()", 1)[0]
        self.assertIn("if (objectLockConfigured())", release)
        self.assertNotIn("updateCamera", release)
        self.assertNotIn("triggerMode", release)
        self.assertLess(release.index("ALCineCamObjectLock::restoreState("),
                        release.index("releaseForManualCamera()"))
        self.assertLess(release.index("ALCineCamObjectLock::restoreState("),
                        release.index('gSavedSettings.setS32("CinematicCamMode", restore.mMode)'))
        self.assertIn('gSavedSettings.setBOOL("CinematicCamEnabled", restore.mEnabled)', release)
        panel = (VIEW / "alpaneldirectorswitcher.cpp").read_text(encoding="utf-8")
        self.assertIn("setEnabledIfChanged(mRelease, LLCinematicCamera::instance().objectLockConfigured())", panel)
        menu = (VIEW / "llviewermenu.cpp").read_text(encoding="utf-8")
        callback = menu.split('enable.add("Object.CameraLocked"', 1)[1].split("});", 1)[0]
        self.assertIn("objectLockConfigured()", callback)
        self.assertNotIn("objectLockActive()", callback)
        self.assertIn('#include "llcinematiccamera.h"', menu)

    def test_outer_transform_and_worn_prop_policy(self):
        source = (VIEW / "llcinematiccamera.cpp").read_text(encoding="utf-8")
        tick = source.split("void LLCinematicCamera::updateObjectLock(", 1)[1].split(
            "void LLCinematicCamera::triggerMode", 1)[0]
        self.assertLess(tick.index("applyOuterTransform(pose, object->getClientOuterTransform())"),
                        tick.index("mObjectPose.update("))
        lock = source.split("void LLCinematicCamera::lockToObject", 1)[1].split(
            "void LLCinematicCamera::releaseObjectLock", 1)[0]
        self.assertIn("Ordinary worn props are valid mounts", lock)
        self.assertNotIn("object->isAttachment()", lock)
        self.assertIn("object->isAvatar()", lock)
        self.assertIn("object->isHUDAttachment()", lock)
        legacy = ET.parse(XUI / "panel_cinecam_params.xml").getroot()
        tooltip = legacy.find(".//button[@name='cc_object_lock']").get("tool_tip", "")
        self.assertIn("worn", tooltip)
        self.assertIn("HUD", tooltip)
        director = ET.parse(XUI / "panel_dpage_camera_shot.xml").getroot()
        help_text = director.find(".//kit_row[@widget='cc_object_lock']").get("help", "")
        self.assertIn("worn", help_text)
        self.assertIn("HUD", help_text)

    def test_census_extends_by_exactly_twelve_rows(self):
        data = json.loads((ROOT / "scripts/code_tools/director_kit/director_mockup_data.json").read_text(encoding="utf-8"))
        shot = next(page for group in data["groups"] if group["name"] == "camera_group"
                    for page in group["pages"] if page["name"] == "shot_tab")
        option = next(option for card in shot["cards"] for option in card.get("opts", [])
                      if option.get("value") == 52)
        self.assertEqual(len(option["rows"]), 12)
        generator = (ROOT / "scripts/code_tools/director_kit_gen.py").read_text(encoding="utf-8")
        self.assertIn("row_count != 2351", generator)


if __name__ == "__main__":
    unittest.main()
