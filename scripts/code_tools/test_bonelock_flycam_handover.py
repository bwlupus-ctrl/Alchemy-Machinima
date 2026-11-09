#!/usr/bin/env python3
"""Run the production pure policies and their TUT tests without a viewer build.

Only TUT registration/assertions and camera services are fixtures. The math,
decision functions and test bodies come from this worktree. Also checks
the runtime wiring and rejects planted precedence/frame-selection regressions.
Run with ordinary Python (without -I); artifacts stay in .tmp/bonelock_flycam.
"""
import os
import json
from pathlib import Path
import re
import shutil
import subprocess
import unittest
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
OUT = ROOT / ".tmp/bonelock_flycam"


def read(name):
    return (VIEW / name).read_text(encoding="utf-8")


def body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def compiler_environment():
    env = os.environ.copy()
    if os.name == "nt" and not shutil.which("cl"):
        candidates = sorted(Path(env.get("ProgramFiles", "C:/Program Files")).glob(
            "Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat"))
        if not candidates:
            raise RuntimeError("MSVC vcvars64.bat not found")
        result = subprocess.run(f'cmd /d /s /c ""{candidates[-1]}" >nul && set"',
                                check=True, capture_output=True, text=True)
        env.update(line.split("=", 1) for line in result.stdout.splitlines()
                   if "=" in line and not line.startswith("="))
    return env


SHIM = r'''
#include "linden_common.h"
#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"
#include "v3dmath.h"
#include "m3math.h"
#include "alflycamclassic.h"
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <type_traits>
namespace tut {
inline void ensure(const char* name, bool condition) {
    if (!condition) throw std::runtime_error(name);
}
template<class T> struct test_group {
    struct object {
        void set_test_name(const char*) {}
        template<int N> void test();
    };
    explicit test_group(const char*) {}
};
}
'''
MAIN = r'''
int main() {
    try {
        tut::handover_object tests;
        tests.test<1>(); tests.test<2>(); tests.test<3>(); tests.test<4>();
        tests.test<5>(); tests.test<6>(); tests.test<7>();
        control_contracts();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    return 0;
}
'''

# Camera/actor services only: every method under test and dispatch is extracted
# verbatim below. No replacement targeting, readiness or owner policy lives here.
RUNTIME_SHIM = r'''
#include <cmath>
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>
using S32 = int; using F32 = float; using F64 = double;
void enableWrite();
struct LLSD { bool value; explicit LLSD(bool v) : value(v) {} };
struct LLControlVariable { void setValue(LLSD, bool); } enableControl;
struct LLUUID { int value = 0; static const LLUUID null; };
const LLUUID LLUUID::null;
struct LLVOAvatar {
    LLUUID id;
    bool isDead() const { return false; }
    LLUUID getID() const { return id; }
    LLVOAvatar* asAvatar() { return this; }
};
using LLViewerObject = LLVOAvatar;
LLVOAvatar actorA{{1}}, actorB{{2}}, actorC{{3}};
int targetResolutions = 0;
struct Objects { LLViewerObject* findObject(LLUUID id) {
    return id.value == 2 ? &actorB : id.value == 3 ? &actorC : nullptr;
} } gObjectList;
struct Settings {
    std::map<std::string, int> values;
    bool getBOOL(const char* key) const { return values.at(key) != 0; }
    void setS32(const char* key, int value) { values[key] = value; }
} gSavedSettings;
void LLControlVariable::setValue(LLSD value, bool) {
    enableWrite(); gSavedSettings.values["CinematicCamEnabled"] = value.value;
}
struct SwitcherControls {
    struct Camera { operator bool() const { return gSavedSettings.getBOOL("CinematicCamEnabled"); } } camera_enabled;
    LLControlVariable* camera = &enableControl;
};
SwitcherControls& controls() { static SwitcherControls c; return c; }
F64 presentation_now() { return 1.; }
template<class T> struct LLCachedControl {
    Settings& settings; std::string key;
    LLCachedControl(Settings& s, const char* k, T) : settings(s), key(k) {}
    operator T() const { return static_cast<T>(settings.values.at(key)); }
};
template<class... T> std::string llformat(const char*, T...) { return ""; }
struct Input { bool suspended = false; bool worldInputSuspended() const { return suspended; } } gViewerInput;
struct Focus { bool focused = true; bool getAppHasFocus() const { return focused; } } gFocusMgr;
constexpr int JDS_INITIALIZED = 1;
int cineWrites = 0, flyWrites = 0, seeds = 0, otherWrites = 0;
float cameraPosition = 10.f, seededPosition = 0.f;
struct LLViewerJoystick {
    bool overrideCamera = false, mJoystickEnabled = true, mFlycamEnabled = true;
    int mDriverState = JDS_INITIALIZED;
    static LLViewerJoystick* getInstance() { static LLViewerJoystick x; return &x; }
    bool getOverrideCamera() const { return overrideCamera; }
    bool canPresentFlycam() const;
    bool moveFlycam(bool reset = false) {
        if (!canPresentFlycam()) return false;
        if (reset) { ++seeds; seededPosition = cameraPosition; }
        ++flyWrites; return true;
    }
    void moveObjects() {}
};
struct Pilot { bool active = false; bool isPlaying() const { return active; }
    bool getOverrideCamera() const { return active; } void moveCamera() { ++otherWrites; }
} gAgentPilot;
struct LLFlycamRecorder { bool active = false;
    static LLFlycamRecorder& instance() { static LLFlycamRecorder x; return x; }
    bool isPlaybackActive() const { return active; }
    void updateCamera() { cameraPosition = 42.f; ++otherWrites; }
};
struct LLPathCamera { bool active = false;
    static LLPathCamera& instance() { static LLPathCamera x; return x; }
    bool isActive() const { return active; }
    void updateCamera() { cameraPosition = 73.f; ++otherWrites; }
};
struct LLToolMgr { static LLToolMgr* getInstance() { static LLToolMgr x; return &x; }
    bool inBuildMode() const { return false; }
};
struct AgentCamera { void updateCamera() { ++otherWrites; } } gAgentCamera;
struct ALDirectorSwitcher {
    enum { SUBJECT_DEFAULT = 0, SUBJECT_A = 1, SUBJECT_B = 2 };
    bool mWasArmed = true;
    bool mWritingCameraEnable = false, mWroteEnable = false, mLastWrittenEnable = false;
    bool mWasCameraEnabled = true;
    static constexpr int SLOT_COUNT = 8;
    int mActiveMode = 1, mActiveSlot = 4;
    struct Slot { int mPrimarySubject = SUBJECT_B; } mActiveSlotConfig;
    static ALDirectorSwitcher& instance() { static ALDirectorSwitcher x; return x; }
    bool shouldYieldToFlycam() const;
    bool isDrivingCamera() const;
    bool boneLockFlycamSource(S32&, S32&) const;
    bool writingCameraEnable() const { return mWritingCameraEnable; }
    void writeCameraEnabled(bool);
    bool punch(S32);
    void tick(F64) {} // scheduling/time services
    static int loadBank() { return 0; }
    bool applySlot(S32, F64, F64, int, bool, bool);
    int activeMode() const { return mActiveMode; }
    int activePrimarySubject() const { return isDrivingCamera() ? mActiveSlotConfig.mPrimarySubject : 0; }
    static int sanitizePrimarySubject(int subject) { return subject; }
};
struct LLCinematicCamera {
    enum { MODE_OFF = 0, MODE_BONE_LOCK = 1, MODE_STATIC_FULL = 40, MODE_OBJECT_LOCK = 41 };
    bool mFlycamSuspended = false, mFlycamSourceValid = false, mFlycamSourceExited = false;
    int mFlycamSourceSlot = -1, mFlycamSourceSubject = 0;
    LLUUID mFlycamSourceTarget;
    static LLCinematicCamera& instance() { static LLCinematicCamera x; return x; }
    static int migrateLegacyMode(int mode) { return mode; }
    void prepareFlycamHandover(bool engaging = false); bool flycamSourceConfigured() const;
    void endFlycamHandover();
    LLVOAvatar* resolveFlycamSource() const;
    LLVOAvatar* resolveTarget(std::string* source = nullptr) const;
    LLVOAvatar* resolveDefaultTarget(std::string* = nullptr) const { ++targetResolutions; return &actorA; }
    LLVOAvatar* resolveMarkedTarget(int subject) const { ++targetResolutions; return subject == 2 ? &actorB : nullptr; }
    bool flycamHandoverActive() const; bool isActive() const;
    bool flycamSuspended() const { return mFlycamSuspended; }
    void suspendForFlycam() { mFlycamSuspended = true; }
    void updateCamera() { ++cineWrites; mFlycamSuspended = false; }
};
'''

RUNTIME_MAIN = r'''
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() { try {
    gSavedSettings.values = {{"CinematicCamEnabled", 1}, {"CinematicCamMode", 2},
        {"DirectorSwitcherArmed", 1}, {"DirectorSwitcherOverridesFlycam", 0},
        {"CinematicBoneLockFlycamHandover", 1}, {"FlycamOrbitEnabled", 1}};
    auto& cine = LLCinematicCamera::instance();
    auto& switcher = ALDirectorSwitcher::instance();
    auto& fly = *LLViewerJoystick::getInstance();
    cine.prepareFlycamHandover(true);
    require(cine.mFlycamSourceSlot == 4 && cine.mFlycamSourceTarget.value == 2, "outgoing slot B captured");
    fly.overrideCamera = true;
    require(switcher.shouldYieldToFlycam() && !switcher.isDrivingCamera(), "switcher yields after override edge");
    gSavedSettings.values["CinematicCamEnabled"] = 0; // switcher restores its false enable baseline
    switcher.mActiveSlotConfig.mPrimarySubject = 1; // later schedule changes cannot retarget the outgoing shot
    cine.prepareFlycamHandover();
    require(cine.flycamHandoverActive(), "switcher enable restoration must not block handover");
    require(cine.resolveTarget() == &actorB, "switcher yield must keep outgoing B, not default A");
    dispatch();
    require(gCameraDispatchOwner == ECameraOwner::FLYCAM && seeds == 1 && cine.flycamSuspended(), "user Orbit on transfers and seeds");
    dispatch();
    require(seeds == 1, "continuous flycam keeps its state");
    for (bool path : {false, true}) {
        LLFlycamRecorder::instance().active = !path;
        LLPathCamera::instance().active = path;
        dispatch();
        require(gCameraDispatchOwner == (path ? ECameraOwner::PATH : ECameraOwner::RECORDER), "foreign dispatch wins");
        gCameraDispatchForeignSinceFlycamWrite = true; // production final-sample history
        LLFlycamRecorder::instance().active = false;
        LLPathCamera::instance().active = false;
        const int before = seeds;
        const float presented = cameraPosition;
        dispatch();
        require(seeds == before + 1 && seededPosition == presented, "foreign-owner re-entry must seed presented pose");
        require(cine.resolveTarget() == &actorB, "foreign re-entry retains outgoing source");
    }
    for (int gate = 0; gate < 5; ++gate) {
        gViewerInput.suspended = gate == 0;
        gFocusMgr.focused = gate != 1;
        fly.mDriverState = gate == 2 ? 0 : JDS_INITIALIZED;
        fly.mJoystickEnabled = gate != 3;
        fly.mFlycamEnabled = gate != 4;
        require(!fly.canPresentFlycam() && !cine.flycamHandoverActive(), "readiness must block yield");
        require(!switcher.shouldYieldToFlycam(), "not-ready Bone Lock switcher keeps its enable lease");
        gSavedSettings.values["CinematicCamEnabled"] = 1; // tick reacquires enable lease
        cine.mFlycamSuspended = false;
        const int before = cineWrites;
        dispatch();
        require(gCameraDispatchOwner == ECameraOwner::CINE && cineWrites == before + 1, "not-ready flycam leaves CINE writing");
        require(!cine.flycamSuspended(), "failed readiness cannot acknowledge suspension");
    }
    gViewerInput.suspended = false; gFocusMgr.focused = true;
    fly.mDriverState = JDS_INITIALIZED; fly.mJoystickEnabled = true; fly.mFlycamEnabled = true;
    for (int reason = 0; reason < 4; ++reason) {
        switcher.mWasArmed = true; switcher.mActiveMode = 1;
        gSavedSettings.values["DirectorSwitcherArmed"] = 1;
        gSavedSettings.values["DirectorSwitcherOverridesFlycam"] = 0;
        gSavedSettings.values["CinematicCamEnabled"] = 1;
        gSavedSettings.values["CinematicCamMode"] = 1;
        cine.mFlycamSourceValid = false;
        cine.prepareFlycamHandover(true);
        require(cine.flycamHandoverActive(), "switcher handover before configuration exit");
        // Execute the real switcher write and production enable-write observer.
        switcher.writeCameraEnabled(false);
        require(cine.flycamHandoverActive(), "lease restore keeps capture");
        if (reason == 0) {
            gSavedSettings.values["DirectorSwitcherOverridesFlycam"] = 1;
            require(switcher.punch(5), "punch non-Bone-Lock slot");
        }
        if (reason == 1) enableControl.setValue(LLSD(false), true); // already false
        if (reason == 2) gSavedSettings.values["DirectorSwitcherArmed"] = 0;
        if (reason == 3) {
            gSavedSettings.values["CinematicCamMode"] = 2;
            modeChanged(2);
        }
        require(!cine.flycamHandoverActive(), "switcher configuration exit ends handover immediately");
        if (reason == 0 || reason == 1 || reason == 3)
            require(!cine.mFlycamSourceValid, "real configuration exits drop captured source");
        dispatch();
        if (reason == 0) require(gCameraDispatchOwner == ECameraOwner::CINE,
            "non-Bone-Lock punch restores normal CINE precedence the same frame");
        cine.prepareFlycamHandover();
        require(!cine.mFlycamSourceValid && !cine.flycamHandoverActive(), "exit cannot recapture from stale mode");
    }
    switcher.mWasArmed = false;
    for (int scenario = 0; scenario < 4; ++scenario) {
        gSavedSettings.values["CinematicCamEnabled"] = 0;
        gSavedSettings.values["CinematicBoneLockFlycamHandover"] = scenario == 1 ? 0 : 1;
        gSavedSettings.values["CinematicCamMode"] = scenario == 2 ? 2 : 1;
        gSavedSettings.values["FlycamOrbitEnabled"] = scenario == 3;
        gFocusMgr.focused = false;
        dispatch();
        require(gCameraDispatchOwner == ECameraOwner::FLYCAM, "ordinary focus loss retains FLYCAM dispatch");
    }
    gFocusMgr.focused = true;
    gSavedSettings.values["CinematicCamEnabled"] = 1;
    gSavedSettings.values["CinematicCamMode"] = 1;
    for (int reason = 0; reason < 3; ++reason) {
        gSavedSettings.values["CinematicCamEnabled"] = 1;
        gSavedSettings.values["CinematicCamMode"] = 1;
        gSavedSettings.values["CinematicBoneLockFlycamHandover"] = 1;
        cine.mFlycamSourceValid = false;
        cine.prepareFlycamHandover(true);
        require(cine.flycamHandoverActive(), "direct Bone Lock handover before exit");
        if (reason == 0) gSavedSettings.values["CinematicCamEnabled"] = 0;
        if (reason == 1) gSavedSettings.values["CinematicCamMode"] = 2;
        if (reason == 2) gSavedSettings.values["CinematicBoneLockFlycamHandover"] = 0;
        require(!cine.flycamHandoverActive() && fly.overrideCamera,
            "CineCam disable, mode change and kill switch end handover with flycam engaged");
    }
    gSavedSettings.values["CinematicCamEnabled"] = 1;
    for (int scenario = 0; scenario < 2; ++scenario) {
        gSavedSettings.values["CinematicBoneLockFlycamHandover"] = scenario == 0 ? 0 : 1;
        fly.overrideCamera = scenario == 0;
        cine.mFlycamSourceValid = false;
        const int before = targetResolutions;
        for (int idle = 0; idle < 100; ++idle) cine.prepareFlycamHandover();
        require(targetResolutions == before, "OFF/disengaged idle must not capture targets");
    }
    fly.overrideCamera = false;
    const int before = targetResolutions;
    cine.prepareFlycamHandover(true);
    require(targetResolutions > before, "engage edge captures target");
    fly.overrideCamera = true;
    const int captured = targetResolutions;
    for (int idle = 0; idle < 100; ++idle) cine.prepareFlycamHandover();
    require(targetResolutions == captured, "engaged idle uses cached source");
    std::cout << "PASS production dispatch/source/readiness and ordinary focus/capture cost regressions\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
'''


def control_fixture():
    """Execute the actual exit, manual-engage, zoom integration and FOV write fragments."""
    fly = body(read("llviewerjoystick.cpp"), "bool LLViewerJoystick::moveFlycam(")
    exit_block = body(fly, "if (handover_exit)")
    manual = body(fly, "if (!sOrbitWasActive)\n")
    zoom_start = fly.index("    ALFlycamClassic::finishIntegration(")
    zoom_end = fly.index("    // Drone->Orbit engages", zoom_start)
    zoom = fly[zoom_start:zoom_end]
    final = re.search(r"    const F32 plain_fov = .*;", fly).group()
    reset = body(fly, "if (reset || mResetFlag)")
    reset_tail = reset[reset.index("        // Ordinary reset frames"):reset.rindex("}")]
    transition_start = fly.index("    bool handover_origin_rebased = false;")
    transition = fly[transition_start:fly.index("    if (handover && (!sHandoverWasActive", transition_start)]
    compensation = body(fly, "if (handover && (!sHandoverWasActive")
    reset_seed = reset[reset.index("        const LLVector3 captured_position"):reset.index("        resetDeltas")]
    reset_zoom = reset[reset.index("            const F32 multiplier ="):reset.index("            sHandoverWasActive = true;")]
    reset_publication = reset_tail[reset_tail.index("        LLCinematicCamera::rememberPreOperatorPresentation("):
        reset_tail.index("        LLViewerCamera::getInstance()->setOrigin(captured_position);")]
    fov_start = fly.index("    const F32 plain_fov =")
    fov_tail = fly[fov_start:fly.index("    // [phoenix port] Procedural handheld", fov_start)]
    def render_fragments(source):
        op = body(source, "if (operator_enabled || apply_active)")
        start = source.index(op) + len(op)
        no_op = body(source[start:], "else")
        return ("    if (operator_enabled || apply_active) {\n" +
                op[op.index("        LLMatrix3 wobble"):op.rindex("}")] + "}\n" + no_op)
    operator_tail = render_fragments(fly)
    base_fly = subprocess.run(["git", "show", "4f1246f040e:indra/newview/llviewerjoystick.cpp"],
        cwd=ROOT, capture_output=True, text=True, check=True).stdout
    base_start = base_fly.index("    const F32 plain_fov = sFlycamZoom * orbit_fov_mul;")
    base_fov_tail = base_fly[base_start:base_fly.index("    // [phoenix port] Procedural handheld", base_start)]
    publication = fly[fly.index("    const bool boundary_lensed ="):fly.index("    if (tagged_dispatch && sDrone.lastWrittenSerial")]
    cine = read("llcinematiccamera.cpp")
    presentation_helpers = "\n".join(body(cine, signature) for signature in (
        "void LLCinematicCamera::rememberPreOperatorPresentation(",
        "bool LLCinematicCamera::preOperatorPresentedPose(",
        "F32 LLCinematicCamera::seedFlycamHandoverZoom("))
    # Execute both CineCam publication sites, including their ease boundary.
    cine_publications = []
    for signature in ("void LLCinematicCamera::updateObjectLock(", "void LLCinematicCamera::updateCamera()"):
        method = body(cine, signature)
        start = method.index("    const F32 plain_boundary_fov =")
        end = method.index("&plain_boundary_fov, fov_mul, &seed_boundary_fov);", start) + len("&plain_boundary_fov, fov_mul, &seed_boundary_fov);")
        cine_publications.append(method[start:end])
    orbit_body = body(fly, "if (orbit_active)")
    smoothing = body(orbit_body, "if (orbit_resolved)")
    handover_move = body(orbit_body, "if (handover)")
    return r'''
namespace ControlFixture {
struct LLUUID { int value = 0; bool operator!=(LLUUID other) const { return value != other.value; } };
struct LLViewerCamera {
    LLVector3 position{8.f, 2.f, 1.f}; LLQuaternion rotation; F32 view = 0.8f;
    static LLViewerCamera* getInstance() { static LLViewerCamera camera; return &camera; }
    LLVector3 getOrigin() const { return position; }
    LLQuaternion getQuaternion() const { return rotation; }
    F32 getView() const { return view; }
    std::vector<F32> viewWrites, agentFovs;
    std::vector<int> sequence;
    void setOrigin(const LLVector3& value) { position = value; ++writes; sequence.push_back(2); }
    bool clampView = false;
    F32 getMinView() const { return 0.1f; }
    F32 getMaxView() const { return 1.718058f; }
    F32 getDefaultFOV() const { return defaultFov; }
    F32 getAspect() const { return 16.f / 9.f; }
    F32 defaultFov = 0.8f;
    void setView(F32 value) {
        value = clampView ? llclamp(value, getMinView(), getMaxView()) : value;
        if (value != view) agentFovs.push_back(value);
        view = value; viewWrites.push_back(value); ++writes; sequence.push_back(1);
    }
    LLVector3 mXAxis, mYAxis, mZAxis;
    int writes = 0;
};
F32 lensShift = 0.08f;
bool clampLens = false;
int gSavedSettings;
template<class T> struct LLCachedControl {
    T value;
    LLCachedControl(int, const char*, T initial) : value(initial) {}
    operator T() const {
        if constexpr (std::is_same_v<T, bool>) return true;
        return value;
    }
};
bool cc_targetFrameAspect(F32& aspect) {
    aspect = (16.f / 9.f) * (1.f + lensShift * 50.f / 12.f);
    return true;
}
''' + body(cine, "F32 cc_applyFrameLens(") + r'''
struct LLCinematicCamera {
    static LLCinematicCamera& instance() { static LLCinematicCamera camera; return camera; }
    static F32 seedFlycamHandoverZoom(F32, F32, LLViewerCamera*, const F32* = nullptr);
    static F32 applyFrameLens(F32 view, LLViewerCamera*) {
        return clampLens ? cc_applyFrameLens(view, LLViewerCamera::getInstance())
            : 2.f * atanf(tanf(view * 0.5f) + lensShift);
    }
    bool flycamHandoverActive() const { return true; }
    static bool preOperatorPresentedPose(LLVector3&, LLQuaternion&, F32&, F32* = nullptr, F32* = nullptr);
    static void rememberPreOperatorPresentation(const LLVector3&, const LLQuaternion&, F32,
        const F32* = nullptr, F32 = 1.f, const F32* = nullptr);
};
bool operator_enabled = false, apply_active = false;
struct LLCameraOperator {
    static LLCameraOperator& instance() { static LLCameraOperator op; return op; }
    void reset() {}
};
struct { ALFlycamPoseOwner owner; int lastWrittenSerial = 0; } sDrone;
ECameraOwner gCameraDispatchOwner = ECameraOwner::FLYCAM;
U64 gCameraDispatchSerial = 7;
struct Sample { U64 serial = 0; };
std::array<Sample, 4> gCameraDispatchSamples;
U32 gCameraDispatchSampleStart = 0, gCameraDispatchSampleCount = 0;
ALPreOperatorPresentation sPreOperatorPresentation;
bool gCameraDispatchFlycamWrote = false, gCameraDispatchForeignSinceFlycamWrite = true;
LLVector3 sFlycamPosition, sOrbitAnchorPos, sOrbitFocus;
LLQuaternion sFlycamRotation, sOrbitAnchorRot;
F32 sFlycamZoom, sHandoverZoomOffset, sOrbitRadius, sOrbitAzimuth, sOrbitElevation, sOrbitRoll;
F32 orbit_zoom = 2.f, min_r = 0.1f, max_r = 30.f;
bool sOrbitWasActive;
bool sHandoverExitZoom = false, sHandoverWasActive = false;
LLUUID sHandoverAnchorId;
LLVector3d sHandoverOrigin;
struct Agent {
    LLVector3d origin;
    LLVector3d getPosGlobalFromAgent(const LLVector3& value) const { return origin + LLVector3d(value); }
    LLVector3 getPosAgentFromGlobal(const LLVector3d& value) const {
        const LLVector3d relative = value - origin;
        return LLVector3((F32)relative.mdV[0], (F32)relative.mdV[1], (F32)relative.mdV[2]);
    }
} gAgent;
''' + presentation_helpers + r'''
ALBoneLockOrbit sHandoverOrbit;
std::array<F32, 7> sDelta{};
constexpr int X_AXIS = 0, Y_AXIS = 1, Z_AXIS = 2, CAM_X_AXIS = 3, CAM_Y_AXIS = 4, CAM_Z_AXIS = 5;
int mJoystickAxis = 0, resets = 0;
void resetDeltas(int) { ++resets; }
bool resetTail(bool sHandoverWasActive) {
    const F32 captured_fov = 0.8f;
    const F32 pre_operator_fov = 0.8f;
    const F32 plain_seed_fov = 0.8f, previous_operator_multiplier = 1.f;
    const bool reversible_fov = false;
    const LLVector3 captured_position = LLViewerCamera::getInstance()->getOrigin();
''' + reset_tail + r'''
}
void resetSeed() {
''' + reset_seed + r'''
    if (reset_handover) {
''' + reset_zoom + reset_publication + r'''
    }
}
void exit(bool handover_exit, bool orbit_resolved) {
''' + exit_block + r'''
}
void manualEngage() {
''' + manual + r'''
}
void anchorTransition(bool handover, bool orbit_resolved, LLUUID resolved_anchor_id) {
''' + transition + r'''
}
void refreshCompensation(bool handover, bool orbit_resolved) {
''' + compensation + r'''
}
struct OperatorOutput {
    F32 mRoll = 2.f * DEG_TO_RAD, mPitch = 0.f, mYaw = 0.f, mFovMul = 1.07f;
    LLVector3 mPosOffset{0.03f, -0.02f, 0.01f};
};
void renderTail(bool handover, bool orbit_active, F32 orbit_fov_mul, OperatorOutput op = {}) {
    auto* cam = LLViewerCamera::getInstance();
    LLMatrix3 mat(sFlycamRotation);
''' + fov_tail + operator_tail + r'''
    LLMatrix3 axes;
    axes.setRows(cam->mXAxis, cam->mYAxis, cam->mZAxis);
    cam->rotation = LLQuaternion(axes);
''' + publication + r'''
}
void renderBaseTail(bool orbit_active, F32 orbit_fov_mul, OperatorOutput op) {
    auto* cam = LLViewerCamera::getInstance();
    LLMatrix3 mat(sFlycamRotation);
''' + base_fov_tail + render_fragments(base_fly) + r'''
}
void publishCine(F32 mode_fov_mul, F32 fov_mul, bool easing_this_frame, bool objectLock = false) {
    auto* cam = LLViewerCamera::getInstance();
    const auto& base_pos = sFlycamPosition;
    const auto& base_rot = sFlycamRotation;
    const auto& pre_operator_position = base_pos;
    const auto& pre_operator_rotation = base_rot;
    const F32 ease_weight = 0.4f;
    struct Ease {
        F32 blendFov(F32 value, bool active, F32 weight) const {
            return active ? 0.8f * (1.f - weight) + value * weight : value;
        }
    } mCutEase;
    const F32 final_fov = mCutEase.blendFov(
        LLCinematicCamera::applyFrameLens(cam->getDefaultFOV() * mode_fov_mul * fov_mul, cam),
        easing_this_frame, ease_weight);
    cam->setView(final_fov);
    const auto seedFlycamHandoverZoom = LLCinematicCamera::seedFlycamHandoverZoom;
    const auto rememberPreOperatorPresentation = LLCinematicCamera::rememberPreOperatorPresentation;
    const auto applyFrameLens = LLCinematicCamera::applyFrameLens;
    if (objectLock) {
''' + cine_publications[0].replace("seedFlycamHandoverZoom(final_fov, 1.f, cam)", "seedFlycamHandoverZoom(final_fov, 1.f, cam, nullptr)") + r'''
    } else {
''' + cine_publications[1].replace("seedFlycamHandoverZoom(final_fov, 1.f, cam)", "seedFlycamHandoverZoom(final_fov, 1.f, cam, nullptr)") + r'''
    }
}
void orbitStep(LLVector3 resolved_anchor_pos, LLQuaternion resolved_anchor_rot) {
    const bool orbit_resolved = true, handover = true, orbit_level_this_frame = false;
    const F32 dt = 1.f / 60.f, orbit_smooth = 0.15f;
    const LLVector3 shadow_entry_position = sFlycamPosition;
    const LLQuaternion shadow_entry_rotation = sFlycamRotation;
''' + smoothing + handover_move + r'''
    sOrbitWasActive = true;
}
F32 zoomControl(ALFlycamClassic::Input classic_input, bool handover, F32 initial,
                F32& rendered, F32 sHandoverZoomOffset, bool retain = false,
                F32 multiplier = 2.f, bool lens = true, F32 previous_axis = 0.f) {
    std::array<F32, 7> last{}, delta{};
    LLVector3 position; LLQuaternion rotation;
    F32 sFlycamZoom = initial;
    const F32 shadow_entry_zoom = initial;
    const F32 shadow_entry_zoom_axis = previous_axis;
    bool handover_exit = false;
    bool sHandoverExitZoom = retain;
    ALFlycamClassic::State classic_state{last, delta, position, rotation, sFlycamZoom};
    const auto classic_intermediate = ALFlycamClassic::integrateDeltas(classic_input, classic_state);
    auto& sDelta = delta;
    auto& sLastDelta = last;
''' + zoom + r'''
    F32 orbit_fov_mul = multiplier;
''' + final + r'''
    rendered = lens ? 2.f * atanf(tanf(plain_fov * 0.5f) + lensShift) : plain_fov;
    return sFlycamZoom;
}
}
void control_contracts() {
    using namespace ControlFixture;
    auto* held = LLViewerCamera::getInstance();
    tut::ensure("ordinary reset reports no write", !resetTail(false));
    tut::ensure("ordinary reset cannot write or clear foreign history", held->writes == 0 &&
        !gCameraDispatchFlycamWrote && gCameraDispatchForeignSinceFlycamWrite && sDrone.lastWrittenSerial == 0);
    tut::ensure("handover reset acknowledges presentation", resetTail(true));
    tut::ensure("handover reset writes and clears foreign history", held->writes == 2 &&
        gCameraDispatchFlycamWrote && !gCameraDispatchForeignSinceFlycamWrite && sDrone.lastWrittenSerial == 7);
    sPreOperatorPresentation.valid = false;
    for (F32 shift : {0.f, 0.08f, 0.4f}) {
    lensShift = shift;
    for (bool manualOrbit : {false, true})
    for (bool lens : {false, true}) {
        operator_enabled = lens;
        auto* camera = LLViewerCamera::getInstance();
        camera->position = LLVector3(8.f, 2.f, 1.f); camera->view = 0.8f;
        sOrbitWasActive = true; sOrbitRadius = 3.f;
        sFlycamPosition = LLVector3(-50.f, 0.f, 0.f); sHandoverZoomOffset = -0.3f;
        sDrone.owner = ALFlycamPoseOwner::ORBIT;
        const int before = resets;
        exit(true, manualOrbit);
        tut::ensure("exit clears manual Orbit engage history", !sOrbitWasActive);
        tut::ensure("exit seeds presented position", (sFlycamPosition - camera->position).magVec() < 1e-6f);
        tut::ensure("exit clears compensation and resets controls", sHandoverZoomOffset == 0.f && resets == before + 1);
        tut::ensure("destination uses normal fresh engage arbitration", sDrone.owner == ALFlycamPoseOwner::CLASSIC);
        F32 fov = sFlycamZoom * (manualOrbit ? orbit_zoom : 1.f);
        if (manualOrbit || lens) fov = 2.f * atanf(tanf(fov * 0.5f) + lensShift);
        tut::ensure("manual/free exit FOV is continuous", fabsf(fov - camera->view) < 1e-6f);
        for (bool bd : {false, true})
        for (bool direct : {false, true})
        for (F32 multiplier : {0.1f, 1.f, 4.f}) {
            orbit_zoom = multiplier;
            exit(true, manualOrbit);
            ALFlycamClassic::Input input{};
            input.blackDragon = bd; input.zoomDirect = direct;
            input.frameIntervalSeconds = 1.f / 60.f; input.flycamFeathering = 10.f;
            input.minView = 0.1f; input.maxView = 1.718f;
            input.axisScale.fill(1.f); input.axisDeadZone.fill(0.1f);
            F32 value = sFlycamZoom, rendered = 0.f;
            for (int frame = 0; frame < 120; ++frame) {
                value = zoomControl(input, false, value, rendered, 0.f, sHandoverExitZoom,
                    manualOrbit ? multiplier : 1.f, manualOrbit || lens);
                tut::ensure("exit FOV stable for 120 zero-input frames", fabsf(rendered - camera->view) < 1e-6f);
            }
            const F32 before = value;
            input.axis[6] = value > input.maxView ? 0.2f : -0.2f;
            value = zoomControl(input, false, value, rendered, 0.f, sHandoverExitZoom,
                manualOrbit ? multiplier : 1.f, manualOrbit || lens);
            tut::ensure("zoom input moves exit FOV without projecting to limits",
                value != before && fabsf(value - before) <= 0.201f);
            if (direct) {
                const F32 moved = value;
                for (int frame = 0; frame < 120; ++frame)
                    value = zoomControl(input, false, value, rendered, 0.f, sHandoverExitZoom,
                        manualOrbit ? multiplier : 1.f, manualOrbit || lens, -input.axis[6]);
                tut::ensure("held absolute zoom after exit never accumulates", value == moved);
            }
        }
        orbit_zoom = 2.f;
        if (manualOrbit) {
            manualEngage();
            tut::ensure("manual Orbit recovers current radius rather than stale three metres",
                fabsf(sOrbitRadius - camera->position.magVec()) < 1e-5f);
        }
    }
    }
    lensShift = 0.08f;
    for (bool bd : {false, true})
    for (bool direct : {false, true})
    for (bool reset : {false, true}) {
        ALFlycamClassic::Input input{};
        input.blackDragon = bd; input.zoomDirect = direct; input.zoomDefault = reset;
        input.cameraAngle = 0.9f; input.minView = 0.1f; input.maxView = 2.f;
        input.frameIntervalSeconds = 0.02f; input.flycamFeathering = 10.f;
        input.axisScale.fill(1.f); input.axisDeadZone.fill(0.1f);
        input.axis[6] = direct ? -0.4f : 0.f;
        F32 unused, rendered;
        const F32 initial = 0.8f;
        const F32 compensation = alSeedHandoverZoom(initial, lensShift, 2.f) - initial;
        const F32 ordinary = zoomControl(input, false, initial, unused, 0.f);
        F32 handover = zoomControl(input, true, initial, rendered, compensation);
        tut::ensure("handover retains exact ordinary zoom control result", handover == ordinary);
        if (direct) {
            const F32 expected = bd ? 1.4f : 0.5f;
            tut::ensure("direct zoom remains absolute", fabsf(handover - expected) < 1e-6f);
            handover = zoomControl(input, true, handover, rendered, compensation);
            tut::ensure("held absolute zoom never accumulates", fabsf(handover - expected) < 1e-6f);
        } else if (bd && reset) {
            tut::ensure("BD default survives integration", handover == input.cameraAngle);
        } else {
            tut::ensure("compensated resting FOV remains captured", fabsf(rendered - initial) < 1e-6f);
        }
        const F32 expected = 2.f * atanf(tanf((handover + compensation) * 2.f * 0.5f) + lensShift);
        tut::ensure("only final FOV receives compensation", fabsf(rendered - expected) < 1e-6f);
    }
    // Exercise the production identity/origin transition and smoothed Orbit
    // blocks with a previously panned and rotated handover rig.
    LLQuaternion anchorRotation, aim;
    anchorRotation.setEulerAngles(0.3f, -0.5f, 0.7f);
    aim.setEulerAngles(-0.2f, 0.4f, 0.1f);
    LLVector3 anchor(5.f, -7.f, 2.f);
    sFlycamPosition = LLVector3(8.f, 2.f, 1.f); sFlycamRotation = aim;
    sOrbitAnchorPos = anchor; sOrbitAnchorRot = anchorRotation;
    sHandoverOrbit.seed(sFlycamPosition, aim, anchor, anchorRotation);
    sHandoverOrbit.move(0.2f, 0.1f, 0.f, 0.3f, 0.2f, 0.1f, min_r, max_r,
        anchor, anchorRotation, sFlycamPosition, sFlycamRotation);
    auto* camera = LLViewerCamera::getInstance();
    camera->position = sFlycamPosition; camera->rotation = sFlycamRotation;
    sOrbitWasActive = sHandoverWasActive = true; sHandoverAnchorId = {1};
    sHandoverOrigin = gAgent.origin = LLVector3d(1024., 2048., 0.);
    anchorTransition(true, true, {2});
    anchor += LLVector3(80.f, -40.f, 3.f);
    orbitStep(anchor, LLQuaternion());
    tut::ensure("replacement anchor reseeds without position change",
        (sFlycamPosition - camera->position).magVec() < 0.00002f);
    for (const LLVector3& axis : {LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 1.f, 0.f)})
        tut::ensure("replacement anchor preserves presented aim",
            (axis * sFlycamRotation - axis * camera->rotation).magVec() < 0.00002f);
    // Global camera pose must stay constant across a region origin shift.
    const LLVector3 before = sFlycamPosition;
    gAgent.origin = LLVector3d(1280., 2048., 0.);
    camera->position = before - LLVector3(256.f, 0.f, 0.f);
    anchor -= LLVector3(256.f, 0.f, 0.f);
    anchorTransition(true, true, {2});
    tut::ensure("origin shift rebases stored camera before any integration",
        (sFlycamPosition - camera->position).magVec() < 0.00003f);
    orbitStep(anchor, LLQuaternion());
    tut::ensure("256 m origin shift causes no global camera displacement",
        (sFlycamPosition + LLVector3(256.f, 0.f, 0.f) - before).magVec() < 0.00003f);
    tut::ensure("origin shift rebases smoothed anchor", (sOrbitAnchorPos - anchor).magVec() < 0.00003f);
    anchorTransition(true, true, {2});
    orbitStep(anchor, LLQuaternion());
    tut::ensure("origin correction is not repeated next frame",
        (sFlycamPosition + LLVector3(256.f, 0.f, 0.f) - before).magVec() < 0.00003f);
    anchorTransition(true, true, {2});
    orbitStep(anchor + LLVector3(1.f, 0.f, 0.f), LLQuaternion());
    tut::ensure("unchanged identity continues following its smoothed anchor",
        (sFlycamPosition + LLVector3(256.f, 0.f, 0.f) - before).magVec() > 0.05f);

    // A steady operator must be applied once after every kind of reseed.
    // Execute the production publication, shared capture/read helpers and
    // reset/identity/compensation/exit fragments, with nonzero translation/FOV.
    for (bool shakeOnly : {false, true})
    for (int reseed : {0, 1, 2, 3, 4, 5}) {
        operator_enabled = !shakeOnly; apply_active = shakeOnly;
        orbit_zoom = 2.f; lensShift = (12.f / 50.f) * (2.35f / (16.f / 9.f) - 1.f);
        sPreOperatorPresentation.valid = false;
        gAgent.origin = LLVector3d(1024., 2048., 0.);
        sHandoverOrigin = gAgent.origin;
        sFlycamPosition = LLVector3(8.123456f, 2.654321f, 1.34567f);
        sFlycamRotation.setEulerAngles(0.1f, -0.2f, 0.3f);
        sFlycamZoom = alSeedHandoverZoom(0.8f, lensShift, orbit_zoom);
        sHandoverZoomOffset = 0.f;
        sOrbitWasActive = sHandoverWasActive = true; sHandoverAnchorId = {1};
        renderTail(true, true, orbit_zoom);
        LLVector3 presentedPosition = camera->position;
        const LLQuaternion presentedRotation = camera->rotation;
        const F32 presentedFov = camera->view;
        gCameraDispatchSamples[0].serial = gCameraDispatchSerial++;
        gCameraDispatchSampleCount = 1;
        if (reseed == 5) {
            const LLVector3 shift(256.f, 256.f, 0.f);
            gAgent.origin += LLVector3d(shift);
            camera->position -= shift;
            presentedPosition -= shift;
        }
        if (reseed <= 1 || reseed == 5) {
            // Engage and foreign re-entry both take the production reset path.
            resetSeed();
        } else if (reseed == 2) {
            anchorTransition(true, true, {2});
            refreshCompensation(true, true);
            orbitStep(LLVector3(50.f, -30.f, 4.f), LLQuaternion());
        } else if (reseed == 3) {
            exit(true, false);
        } else {
            sOrbitWasActive = false;
            refreshCompensation(true, true);
        }
        renderTail(reseed != 3, reseed != 3, reseed == 3 ? 1.f : orbit_zoom);
        tut::ensure("steady 2 degree roll survives reseed without doubling",
            (LLVector3(0.f, 1.f, 0.f) * camera->rotation -
             LLVector3(0.f, 1.f, 0.f) * presentedRotation).magVec() < 0.00002f);
        tut::ensure("operator translation is applied exactly once at reseed",
            (camera->position - presentedPosition).magVec() < 0.00005f);
        tut::ensure("operator breathing is applied exactly once at reseed",
            fabsf(camera->view - presentedFov) < 0.000002f);
    }
    // The frame lens itself clamps the pre-operator value in the review's
    // 1.7 rad / 0.95 / 16:9 -> 2.35 / 50mm case. Neither that clamp nor
    // the camera clamp may discard the authored pre-lens input.
    clampLens = camera->clampView = true;
    lensShift = (12.f / 50.f) * (2.35f / (16.f / 9.f) - 1.f);
    OperatorOutput reviewOperator;
    reviewOperator.mFovMul = 0.95f;
    operator_enabled = true; apply_active = false;
    for (int source : {0, 1, 2, 3, 4})
    for (int reseed : {0, 1, 2, 3, 4, 5}) {
        sPreOperatorPresentation.valid = false;
        sFlycamZoom = 1.7f / 2.f; sHandoverZoomOffset = 0.f; orbit_zoom = 2.f;
        sOrbitWasActive = sHandoverWasActive = true; sHandoverAnchorId = {1};
        sHandoverOrigin = gAgent.origin;
        camera->defaultFov = 1.7f;
        if (source == 0) renderTail(true, true, orbit_zoom, reviewOperator);
        else publishCine(1.f, reviewOperator.mFovMul, source >= 3, source == 2 || source == 4);
        const F32 presented = camera->view;
        tut::ensure("boundary retains operator multiplier", sPreOperatorPresentation.operatorMultiplier == 0.95f);
        tut::ensure("boundary retains 1.7 before frame-lens clamp, including during cuts",
            sPreOperatorPresentation.plainFov == 1.7f);
        if (source < 3) {
            tut::ensure("review case actually hits frame-lens clamp", sPreOperatorPresentation.fov == camera->getMaxView());
            tut::ensure("review case presents 1.686063 radians", fabsf(presented - 1.686063f) < 0.000002f);
        }
        gCameraDispatchSamples[0].serial = gCameraDispatchSerial++;
        if (reseed <= 1 || reseed == 5) {
            resetSeed();
            tut::ensure("reset republishes reversible FOV and multiplier", sPreOperatorPresentation.reversibleFov &&
                sPreOperatorPresentation.operatorMultiplier == 0.95f);
        } else if (reseed == 2) {
            anchorTransition(true, true, {2});
            refreshCompensation(true, true);
        } else if (reseed == 3) {
            exit(true, false);
        } else {
            sOrbitWasActive = false;
            refreshCompensation(true, true);
        }
        renderTail(reseed != 3, reseed != 3, reseed == 3 ? 1.f : orbit_zoom, reviewOperator);
        tut::ensure("lens clamp never changes FOV on reseed", fabsf(camera->view - presented) < 0.000002f);
    }
    clampLens = false;
    // Record before the intermediate camera write clamps FOV: breathing can
    // bring an out-of-range control FOV back inside the presented range.
    camera->clampView = true;
    lensShift = 0.f;
    sFlycamZoom = 2.f; sHandoverZoomOffset = 0.f;
    OperatorOutput narrowing;
    narrowing.mFovMul = 0.8f;
    renderTail(true, false, 1.f, narrowing);
    tut::ensure("operator may narrow an out-of-range control FOV", fabsf(camera->view - 1.6f) < 1e-6f);
    exit(true, false);
    renderTail(false, false, 1.f, narrowing);
    tut::ensure("reseed retains FOV before intermediate write clamps it", fabsf(camera->view - 1.6f) < 1e-6f);
    camera->clampView = false;
    // The review's shake-only case: no manual Orbit/operator, 16:9 -> 2.35,
    // 50mm frame lens, neutral operator breathing, 0.8 rad presented FOV.
    operator_enabled = false; apply_active = true;
    lensShift = (12.f / 50.f) * (2.35f / (16.f / 9.f) - 1.f);
    sPreOperatorPresentation.valid = false;
    camera->view = 0.8f;
    exit(true, false);
    ALFlycamClassic::Input idle{};
    idle.frameIntervalSeconds = 1.f / 60.f; idle.flycamFeathering = 10.f;
    idle.minView = 0.1f; idle.maxView = 1.718f;
    idle.axisScale.fill(1.f); idle.axisDeadZone.fill(0.1f);
    OperatorOutput neutral;
    neutral.mFovMul = 1.f;
    for (int frame = 0; frame < 120; ++frame) {
        F32 unused;
        sFlycamZoom = zoomControl(idle, false, sFlycamZoom, unused, 0.f, sHandoverExitZoom, 1.f);
        renderTail(false, false, 1.f, neutral);
        tut::ensure("shake-only frame-lens exit FOV stays 0.8 for 120 frames",
            fabsf(camera->view - 0.8f) < 0.000001f);
    }
    // A foreign camera that did not publish an operator boundary must use
    // its own authored pose, even if it matches an older camera's output.
    gCameraDispatchSamples[0].serial = ++gCameraDispatchSerial;
    ++gCameraDispatchSerial;
    LLVector3 rawPosition; LLQuaternion rawRotation; F32 rawFov;
    LLCinematicCamera::preOperatorPresentedPose(rawPosition, rawRotation, rawFov);
    tut::ensure("foreign presentation cannot reuse a stale operator boundary",
        rawPosition == camera->position && rawFov == camera->view);

    // Compare the actual base/current tail writes and AgentFOV-on-change
    // events for 120 frames, covering every ordinary lens/operator predicate.
    clampLens = camera->clampView = true;
    lensShift = (12.f / 50.f) * (2.35f / (16.f / 9.f) - 1.f);
    for (bool orbit : {false, true})
    for (bool useOperator : {false, true})
    for (bool shake : {false, true})
    for (F32 plain : {0.8f, 1.7f}) {
        operator_enabled = useOperator; apply_active = shake;
        sFlycamZoom = plain; sHandoverZoomOffset = 0.f; sHandoverExitZoom = false;
        LLViewerCamera initial = *camera;
        initial.view = 0.7f; initial.viewWrites.clear(); initial.agentFovs.clear(); initial.sequence.clear();
        *camera = initial;
        for (int frame = 0; frame < 120; ++frame) renderBaseTail(orbit, 1.f, neutral);
        const LLViewerCamera baseResult = *camera;
        *camera = initial;
        for (int frame = 0; frame < 120; ++frame) renderTail(false, orbit, 1.f, neutral);
        tut::ensure("ordinary complete write sequence matches base", camera->sequence == baseResult.sequence &&
            camera->viewWrites == baseResult.viewWrites && camera->agentFovs == baseResult.agentFovs &&
            camera->position == baseResult.position && camera->mXAxis == baseResult.mXAxis &&
            camera->mYAxis == baseResult.mYAxis && camera->mZAxis == baseResult.mZAxis);
        if (!orbit && !useOperator && shake && plain == 0.8f)
            tut::ensure("shake-only sends plain then lensed AgentFOV each frame", camera->agentFovs.size() == 240 &&
                camera->viewWrites[0] == 0.8f && fabsf(camera->viewWrites[1] - 0.927364f) < 0.000002f);
    }
}
'''


class BoneLockFlycamContracts(unittest.TestCase):
    def test_ordinary_parity_hunks(self):
        baseline = subprocess.run(["git", "show", "4f1246f040e:indra/newview/llviewerjoystick.cpp"],
                                  cwd=ROOT, capture_output=True, text=True, check=True).stdout
        current = read("llviewerjoystick.cpp")
        def canonical(source):
            return re.sub(r'\s+', '', re.sub(r'//[^\n]*', '', source))
        old = body(baseline, "void LLViewerJoystick::moveFlycam(")
        new = body(current, "bool LLViewerJoystick::moveFlycam(")
        # The complete manual Orbit engage/integration block remains byte-equivalent
        # after ignoring only comments/whitespace and the new handover-only wrapper.
        start = "            if (!sOrbitWasActive)\n"
        end = "            sFlycamRotation = orbit_rot;"
        self.assertEqual(canonical(old[old.index(start):old.index(end) + len(end)]),
                         canonical(new[new.index(start):new.index(end) + len(end)]))
        for marker in ("    if (!gFocusMgr.getAppHasFocus()", "    if (tagged_dispatch && sDrone.resetPending)"):
            self.assertEqual(canonical(body(old, marker)),
                             canonical(body(new, marker).replace("return false;", "return;")))
        # Reset preparation before the handover seed and Drone bookkeeping are unchanged.
        for begin, finish in (("        const bool drone_selected", "        mDroneRecentreActive = false;"),):
            old_block = old[old.index(begin):old.index(finish) + len(finish)]
            new_block = new[new.index(begin):new.index(finish) + len(finish)]
            new_block = new_block.replace("sOrbitWasActive ? ALFlycamPoseOwner::ORBIT :\n            ", "")
            self.assertEqual(canonical(old_block), canonical(new_block))
        self.assertLess(new.index("if (!sHandoverWasActive)\n            return false;"),
                        new.index("LLViewerCamera::getInstance()->setOrigin(captured_position)"))
        # Compare every statement in the ordinary initial lens write and
        # operator/no-operator tail, including both setView calls and axes.
        start = "    const bool cinematic_driven = orbit_active || operator_enabled;"
        finish = ": plain_fov);"
        self.assertEqual(canonical(old[old.index(start):old.index(finish, old.index(start)) + len(finish)]),
                         canonical(new[new.index(start):new.index(finish, new.index(start)) + len(finish)]))
        start = "    if (operator_enabled || apply_active)"
        new_tail = body(new, start).replace("        operator_multiplier = op.mFovMul;\n", "")
        self.assertEqual(canonical(body(old, start)), canonical(new_tail))
        self.assertNotIn("alFlycamFrameLensApplied", new[new.index("    LLMatrix3 mat(sFlycamRotation);"):])
        for filename in ("alflycamclassic.cpp", "llflycamdrone.cpp", "alflycamweighted.cpp"):
            self.assertTrue((VIEW / filename).is_file())
            unchanged = subprocess.run(["git", "diff", "--exit-code", "4f1246f040e", "--", "indra/newview/" + filename],
                cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(unchanged.returncode, 0, filename)
        writes = r'(?:cam|LLViewerCamera::getInstance\(\))->(?:set\w+\([^;]*|m[XYZ]Axis\s*=[^;]*);'
        ordinary = new
        reset = body(ordinary, "if (reset || mResetFlag)")
        ordinary = ordinary.replace(reset, reset[:reset.index("        // Ordinary reset frames")] + "return false;\n}")
        handover_write = body(ordinary, "if (handover)\n        cam->setView(")
        # body() reaches the ordinary else braces for this unbraced if.
        ordinary = ordinary.replace(handover_write, handover_write[handover_write.index("{"):])
        self.assertEqual([canonical(s) for s in re.findall(writes, old)],
                         [canonical(s) for s in re.findall(writes, ordinary)])
        for filename in ("llcinematiccamera.cpp", "llagentcamera.cpp", "llflycamrecorder.cpp"):
            baseline = subprocess.run(["git", "show", "4f1246f040e:indra/newview/" + filename],
                cwd=ROOT, capture_output=True, text=True, check=True).stdout
            pattern = writes + r'|ALCineCamPresentation::writeCamera\([^;]*;'
            self.assertEqual([canonical(s) for s in re.findall(pattern, baseline)],
                             [canonical(s) for s in re.findall(pattern, read(filename))], filename)

    def test_native_dispatch_switcher_yield_and_readiness(self):
        cine = read("llcinematiccamera.cpp")
        switcher = read("aldirectorswitcher.cpp")
        idle = read("llappviewer.cpp")
        header = read("alcameradispatch.h")
        policy = header[header.index("enum class ECameraOwner"):header.index("struct ALCameraDispatchSample")]
        methods = "\n".join(body(cine, signature) for signature in (
            "void LLCinematicCamera::endFlycamHandover()",
            "void LLCinematicCamera::prepareFlycamHandover(",
            "bool LLCinematicCamera::flycamSourceConfigured() const",
            "LLVOAvatar* LLCinematicCamera::resolveFlycamSource() const",
            "LLVOAvatar* LLCinematicCamera::resolveTarget(",
            "bool LLCinematicCamera::flycamHandoverActive() const",
            "bool LLCinematicCamera::isActive() const"))
        methods += "\n" + "\n".join(body(switcher, signature) for signature in (
            "bool ALDirectorSwitcher::shouldYieldToFlycam() const",
            "bool ALDirectorSwitcher::isDrivingCamera() const",
            "bool ALDirectorSwitcher::boneLockFlycamSource(",
            "void ALDirectorSwitcher::writeCameraEnabled(",
            "bool ALDirectorSwitcher::punch("))
        enable_observer = body(cine, "[this](LLControlVariable*)")
        methods += "\nvoid enableWrite() " + enable_observer[enable_observer.index("{"):].replace(
            "endFlycamHandover();", "LLCinematicCamera::instance().endFlycamHandover();")
        mode_exit = (
            "if (selected_mode != MODE_BONE_LOCK)\n                    endFlycamHandover();")
        self.assertIn(mode_exit, cine)
        methods += "\nvoid modeChanged(int selected_mode) { constexpr int MODE_BONE_LOCK = 1;\n" + mode_exit.replace(
            "endFlycamHandover();", "LLCinematicCamera::instance().endFlycamHandover();") + "\n}"
        # The bank/time model is a service; execute the production slot mutation
        # and exit notification through the verbatim punch() entry point.
        slot_start = switcher.index("    mActiveSlot = slot;", switcher.index("bool ALDirectorSwitcher::applySlot("))
        slot_end = switcher.index("    mActiveSince =", slot_start)
        methods += "\nbool ALDirectorSwitcher::applySlot(S32 slot, F64, F64, int, bool, bool) {\n" + (
            "const int next_mode = slot == 5 ? 2 : 1; Slot next_config;\n" +
            switcher[slot_start:slot_end] + "return true;\n}")
        methods += "\n" + body(read("llviewerjoystick.cpp"), "bool LLViewerJoystick::canPresentFlycam() const")
        start = idle.index("    const ECameraOwner previous_camera_owner = gCameraDispatchOwner;")
        dispatch = idle[start:idle.index("    // Follow the final VCam/flycam camera;", start)]
        globals = """
ECameraOwner gCameraDispatchOwner = ECameraOwner::CINE;
int gCameraDispatchSerial = 0;
bool gCameraDispatchFlycamWrote = false, gCameraDispatchForeignSinceFlycamWrite = false;
"""
        dispatch = "void dispatch() {\n" + dispatch + "\ngCameraDispatchForeignSinceFlycamWrite = gCameraDispatchOwner != ECameraOwner::FLYCAM;\n}\n"
        source = RUNTIME_SHIM + policy + globals + methods + dispatch + RUNTIME_MAIN
        variants = {"runtime_production": source}
        for name, old, new in (
                ("yield_loses_source", "if (flycamHandoverActive() && flycamSourceConfigured())", "if (false)"),
                ("yield_loses_enable", "flycamSourceConfigured() || (enabled", "false || (enabled"),
                ("dispatch_reentry_ignored", "!suspended || foreignSinceWrite || previous != ECameraOwner::FLYCAM", "!suspended"),
                ("readiness_focus_ignored", "gFocusMgr.getAppHasFocus() &&", ""),
                ("ordinary_dispatch_gated", "else if (LLViewerJoystick::getInstance()->getOverrideCamera())",
                 "else if (LLViewerJoystick::getInstance()->getOverrideCamera() && LLViewerJoystick::getInstance()->canPresentFlycam())"),
                ("switcher_exit_ignored", "if (next_mode != LLCinematicCamera::MODE_BONE_LOCK)", "if (false)"),
                ("explicit_disable_ignored", "if (!ALDirectorSwitcher::instance().writingCameraEnable())", "if (false)"),
                ("lease_restore_exits", "mWritingCameraEnable = true;", "mWritingCameraEnable = false;"),
                ("disarm_ignored", "armed && ALDirectorSwitcher::instance().activeMode() == MODE_BONE_LOCK",
                 "ALDirectorSwitcher::instance().activeMode() == MODE_BONE_LOCK"),
                ("mode_exit_ignored", "if (selected_mode != MODE_BONE_LOCK)", "if (false)"),
                ("idle_capture_when_off", "if (!handover || (!engaging && !LLViewerJoystick::getInstance()->getOverrideCamera()))", "if (false)")):
            self.assertEqual(source.count(old), 1, name)
            variants[name] = source.replace(old, new)
        env = compiler_environment()
        compiler = shutil.which("cl", path=env.get("Path", env.get("PATH")))
        for name, variant in variants.items():
            cpp, exe = OUT / (name + ".cpp"), OUT / (name + ".exe")
            cpp.write_text(variant, encoding="utf-8")
            result = subprocess.run([compiler, "/nologo", "/std:c++17", "/EHsc", str(cpp),
                "/Fe:" + str(exe), "/Fo:" + str(OUT / (name + ".obj"))],
                env=env, cwd=OUT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=OUT, capture_output=True, text=True)
            if name == "runtime_production":
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                print(result.stdout.strip())
            else:
                self.assertNotEqual(result.returncode, 0, "surviving mutation: " + name)
                print("REJECTED planted negative: " + name + " (" + result.stderr.strip() + ")")

    def test_setting_is_persisted_default_on_without_ui(self):
        entries = list(ET.fromstring(read("app_settings/settings.xml")).find("map"))
        settings = {key.text: value for key, value in zip(entries[::2], entries[1::2])}
        fields = list(settings["CinematicBoneLockFlycamHandover"])
        value = {key.text: item.text for key, item in zip(fields[::2], fields[1::2])}
        self.assertEqual((value["Type"], value["Persist"], value["Value"]), ("Boolean", "1", "1"))
        self.assertEqual(value["Comment"], "When on, engaging the flycam during a Bone Lock camera move flies an orbit around the same actor and joint; Bone Lock resumes when the flycam is turned off.")
        for path in (VIEW / "skins/default/xui/en").glob("panel_dpage_camera*.xml"):
            self.assertNotIn("CinematicBoneLockFlycamHandover", path.read_text(encoding="utf-8"))

    def test_dispatch_and_consumers_share_runtime_suspension(self):
        cine = read("llcinematiccamera.cpp")
        query = body(cine, "bool LLCinematicCamera::flycamHandoverActive() const")
        for required in ("effective_mode == MODE_BONE_LOCK", "switcher.activeMode()",
                         "getOverrideCamera()", "alResolveCineFlycamOwner(true",
                         "decision.owner == ECameraOwner::FLYCAM", "decision.handover",
                         "gAgentPilot.isPlaying()", "isPlaybackActive()", "LLPathCamera::instance().isActive()"):
            self.assertIn(required, query)
        for name in ("isActive() const", "isActiveMountedTarget(const LLUUID& avatar_id) const"):
            self.assertIn("flycamHandoverActive()", body(cine, "bool LLCinematicCamera::" + name))
        idle = read("llappviewer.cpp")
        start = idle.index("++gCameraDispatchSerial;")
        dispatch = idle[start:idle.index("alCameraDispatchSampleFinal();", start)]
        order = [dispatch.index(token) for token in ("gAgentPilot.isPlaying()", "isPlaybackActive()",
                 "LLPathCamera::instance().isActive()", "LLCinematicCamera::instance().isActive()",
                 "getOverrideCamera()", "gCameraDispatchOwner = ECameraOwner::AGENT")]
        # getOverrideCamera also occurs in the pilot expression: use the flycam branch.
        order[4] = dispatch.index("else if (LLViewerJoystick::getInstance()->getOverrideCamera())")
        self.assertEqual(order, sorted(order))
        fly = dispatch[order[4]:order[5]]
        self.assertNotIn("canPresentFlycam()", fly)
        self.assertIn("previous_camera_owner", fly)
        self.assertIn("gCameraDispatchForeignSinceFlycamWrite", fly)
        self.assertLess(fly.index("moveFlycam(seed)"), fly.index("suspendForFlycam()"))
        self.assertLess(idle.index("prepareFlycamHandover();"), idle.index("ALDirectorSwitcher::instance().tick("))
        self.assertIn("canPresentFlycam()", query)
        tick = body(cine, "void LLCinematicCamera::updateCamera()")
        self.assertIn("mFlycamSuspended || gFrameCount > mLastUpdateFrame + 3", tick)
        self.assertIn("mFlycamSuspended = false;", tick)

    def test_exact_mount_target_joint_and_existing_orbit_frame(self):
        cine = read("llcinematiccamera.cpp")
        anchor = body(cine, "bool LLCinematicCamera::resolveAnchor(")
        self.assertIn("flycamHandoverActive() ? resolveTarget() : resolveDefaultTarget()", anchor)
        self.assertIn('"CinematicCamJoint"', anchor)
        self.assertIn('joint = av->getJoint("mHead")', anchor)
        orbit_target = body(cine, "bool LLCinematicCamera::isActiveOrbitAnchor(")
        self.assertIn("handover ? resolveTarget() : resolveDefaultTarget()", orbit_target)
        self.assertIn("(!orbit_enabled && !handover)", orbit_target)
        fly = body(read("llviewerjoystick.cpp"), "bool LLViewerJoystick::moveFlycam(")
        self.assertIn("(orbit_enabled || handover)", fly)
        self.assertIn("anchor_resolved).orbit", fly)
        self.assertIn("m3DCursor, orbit_resolved, sDrone.owner", fly)
        self.assertIn("(sFlycamPosition - sOrbitAnchorPos) * ~sOrbitAnchorRot", fly)
        self.assertIn("handover ? (bool)bone_level : (bool)orbit_level", fly)
        self.assertIn("orbit_resolved && !handover", fly)
        self.assertIn("sHandoverOrbit.seed(shadow_entry_position, shadow_entry_rotation", fly)
        self.assertIn("seedFlycamHandoverZoom", fly)
        reset = body(fly, "if (reset || mResetFlag)")
        self.assertIn("sHandoverOrbit.seed(sFlycamPosition, sFlycamRotation", reset)
        self.assertIn("setView(captured_fov)", reset)
        self.assertIn("gCameraDispatchFlycamWrote = true", reset)
        self.assertIn("return true;", reset)
        # Every seed consumes the same published boundary; camera owners that
        # can apply this layer publish it after their ordinary final write.
        for block in (reset, body(fly, "if (handover_exit)"),
                      body(fly, "if (handover && (!sHandoverWasActive"),
                      body(fly, "if (orbit_resolved && sHandoverWasActive && sOrbitWasActive &&")):
            self.assertIn("LLCinematicCamera::preOperatorPresentedPose(", block)
        self.assertIn("alFlycamFrameLensApplied(false, orbit_resolved, operator_enabled, apply_active)", fly)
        self.assertIn("const bool cinematic_driven = orbit_active || operator_enabled;", fly)
        self.assertIn("rememberPreOperatorPresentation(base_pos, base_rot", cine)
        self.assertIn("rememberPreOperatorPresentation(pre_operator_position, pre_operator_rotation", cine)
        for owner in ("llviewerjoystick.cpp", "llflycamrecorder.cpp", "llagentcamera.cpp"):
            self.assertIn("LLCinematicCamera::rememberPreOperatorPresentation(", read(owner))
        for setting in ("FlycamOrbitEnabled", "CinematicCamEnabled", "CinematicCamMode"):
            self.assertNotRegex(fly, r'set\w+\("' + setting + '"')
            self.assertNotRegex(body(cine, "bool LLCinematicCamera::flycamHandoverActive() const"),
                                r'set\w+\("' + setting + '"')

    def test_native_unit_tests_and_planted_negatives(self):
        header = read("alcameradispatch.h")
        policy = header[header.index("enum class ECameraOwner"):header.index("struct ALCameraDispatchSample")]
        math = re.sub(r'^#include[^\n]*\n', '', read("alcineflycamhandover.h"), flags=re.M)
        tests = re.sub(r'^#include[^\n]*\n', '', read("tests/alcineflycamhandover_test.cpp"), flags=re.M)
        mutations = {
            "cine_first": ("cinematicActive && !handover", "cinematicActive"),
            "unresolved_stalls": ("const bool handover = boneLockEnabled", "const bool handover = anchorResolved && boneLockEnabled"),
            "kill_ignored": ("handoverEnabled && flycamReady", "flycamReady"),
            "other_modes": ("boneLockEnabled && flycamEngaged", "(boneLockEnabled || cinematicActive) && flycamEngaged"),
            "user_orbit_veto": ("handoverEnabled && flycamReady", "handoverEnabled && flycamReady && !userOrbitEnabled"),
            "readiness_ignored": ("handoverEnabled && flycamReady", "handoverEnabled"),
            "foreign_reentry_ignored": ("!suspended || foreignSinceWrite || previous != ECameraOwner::FLYCAM", "!suspended"),
            "missing_anchor_orbits": ("(handover || userOrbitEnabled) && anchorResolved", "(handover || userOrbitEnabled)"),
            "drone_beats_orbit": ("orbitResolved ? ALFlycamPoseOwner::ORBIT :", "orbitResolved && modelSetting != 1 ? ALFlycamPoseOwner::ORBIT :"),
        }
        variants = {"production": policy + math}
        for name, (old, new) in mutations.items():
            self.assertEqual(policy.count(old), 1, name)
            variants[name] = policy.replace(old, new) + math
        for name, old, new in (
                ("aim_snaps", "aimOffset = rotation * ~basis(azimuth, elevation, anchorRotation);", "aimOffset = LLQuaternion();"),
                ("pole_fallback_flip", "const LLVector3 left = LLVector3(sinf(azimuth), -cosf(azimuth), 0.f) * anchor;",
                 "LLVector3 left = (LLVector3(0.f, 0.f, 1.f) * anchor) % at; if (left.magVecSquared() < 1e-6f) left = LLVector3(0.f, 1.f, 0.f) * anchor; left.normVec();"),
                ("radius_projects", "radius = offset.magVec();", "radius = llclamp(offset.magVec(), 1.f, 30.f); offset = LLVector3(1.f, 0.f, 0.f) * radius;"),
                ("limits_project_at_rest", "return llclamp(value + delta, llmin(value, low), llmax(value, high));", "return llclamp(value + delta, low, high);"),
                ("lens_applied_twice", "return plain / orbitMultiplier;", "return rendered;"),
                ("shake_lens_predicate_omitted", "return handover || orbit || cameraOperator || shake;", "return handover || orbit || cameraOperator;")):
            self.assertEqual(math.count(old), 1, name)
            variants[name] = policy + math.replace(old, new)
        classic = re.sub(r'^#include[^\n]*\n', '', read("alflycamclassic.cpp"), flags=re.M)
        controls = control_fixture()
        variants = {name: source + classic + controls for name, source in variants.items()}
        for name, old, new in (
                ("exit_skips_manual_init", "sOrbitWasActive = false;\n        sHandoverZoomOffset", "sOrbitWasActive = true;\n        sHandoverZoomOffset"),
                ("ordinary_reset_writes", "if (!sHandoverWasActive)", "if (false)"),
                ("exit_leaks_zoom", "sFlycamZoom = destination_lens ?", "sFlycamZoom = false ?"),
                ("exit_stale_pose", "// handover's Orbit flag must not suppress manual Orbit initialization.\n        F32 pre_operator_fov, plain_seed_fov;\n        const bool reversible_fov = LLCinematicCamera::preOperatorPresentedPose(\n            sFlycamPosition, sFlycamRotation, pre_operator_fov, &plain_seed_fov);", "F32 pre_operator_fov = LLViewerCamera::getInstance()->getView(), plain_seed_fov = 0.f; const bool reversible_fov = false; sFlycamPosition = LLVector3();"),
                ("exit_fov_one_frame", "else if (sHandoverExitZoom)", "else if (false)"),
                ("exit_fov_projects", "alHandoverLimitedMove(shadow_entry_zoom, zoom_delta,", "llclamp(shadow_entry_zoom + zoom_delta,"),
                ("exit_fov_direct_accumulates", "(sLastDelta[6] - shadow_entry_zoom_axis)", "sLastDelta[6]"),
                ("anchor_identity_ignored", "resolved_anchor_id != sHandoverAnchorId", "false"),
                ("anchor_origin_stale", "sOrbitAnchorPos += delta;", ";"),
                ("position_origin_stale", "sFlycamPosition += delta;", ";"),
                ("zoom_delta_overwrites_controls", "    // The destination's first frame holds", "    if (handover) sFlycamZoom = alHandoverLimitedMove(shadow_entry_zoom, sDelta[6], classic_input.minView, classic_input.maxView);\n    // The destination's first frame holds"),
                ("zoom_default_discarded", "    // The destination's first frame holds", "    if (handover && classic_input.blackDragon && classic_input.zoomDefault) sFlycamZoom = shadow_entry_zoom;\n    // The destination's first frame holds"),
                ("zoom_compensation_lost", "handover ? sFlycamZoom + sHandoverZoomOffset : sFlycamZoom", "sFlycamZoom")):
            self.assertEqual(controls.count(old), 2 if name == "zoom_compensation_lost" else 1, name)
            variants[name] = policy + math + classic + controls.replace(old, new)
        for name, old, new in (
                ("operator_pose_doubled", "rotation = sPreOperatorPresentation.rotation;", "rotation = cam->getQuaternion();"),
                ("operator_position_doubled", "position = gAgent.getPosAgentFromGlobal(sPreOperatorPresentation.position);", "position = cam->getOrigin();"),
                ("operator_fov_doubled", "fov = sPreOperatorPresentation.fov;", "fov = cam->getView(); return false;"),
                ("shake_exit_lens_omitted", "alFlycamFrameLensApplied(false, orbit_resolved, operator_enabled, apply_active)", "alFlycamFrameLensApplied(false, orbit_resolved, operator_enabled, false)"),
                ("operator_boundary_stale", "U64 last_serial = gCameraDispatchSerial;", "U64 last_serial = sPreOperatorPresentation.serial;"),
                ("clamped_seed_inverted", "if (plain_fov)\n        return *plain_fov / orbit_multiplier;", "if (false)\n        return *plain_fov / orbit_multiplier;"),
                ("flycam_raw_boundary_lost", "boundary_lensed ? &plain_fov : nullptr", "nullptr"),
                ("ordinary_shake_first_write_lensed", "const bool cinematic_driven = orbit_active || operator_enabled;", "const bool cinematic_driven = orbit_active || operator_enabled || apply_active;")):
            count = 2 if name == "ordinary_shake_first_write_lensed" else 1
            self.assertEqual(controls.count(old), count, name)
            variants[name] = policy + math + classic + controls.replace(old, new, 1)
        for name, old, new in (
                ("cinecam_raw_boundary_lost", "const F32 plain_boundary_fov = cam->getDefaultFOV() * mode_fov_mul;", "const F32 plain_boundary_fov = alSeedHandoverZoom(sPreOperatorPresentation.fov, lensShift, 1.f);"),
                ("cinecam_operator_multiplier_lost", "&plain_boundary_fov, fov_mul, &seed_boundary_fov);", "&plain_boundary_fov, 1.f, &seed_boundary_fov);"),
                ("cinecam_cut_seed_lost", "&plain_boundary_fov, fov_mul, &seed_boundary_fov);", "&plain_boundary_fov, fov_mul, &plain_boundary_fov);")):
            self.assertEqual(controls.count(old), 1 if name == "cinecam_raw_boundary_lost" else 2, name)
            variants[name] = policy + math + classic + controls.replace(old, new)
        OUT.mkdir(parents=True, exist_ok=True)
        env = compiler_environment()
        # Compile only the repository's math support; never configure/build the viewer.
        cache = json.loads((OUT / "zs_flags_cache.json").read_text(encoding="utf-8-sig"))
        flags = next(iter(cache.values()))["flags"]
        includes = [flag.replace('"', '') for flag in flags
                    if flag.startswith(("/I", "/D"))]
        includes += ["/external:I" + path for flag in flags
                     for path in re.findall(r'/external:I\s+"([^"]+)"', flag)]
        includes += ["/I" + str(VIEW), "/external:W0", "/utf-8"]
        compiler = shutil.which("cl", path=env.get("Path", env.get("PATH")))
        math_support = SHIM
        # Verbatim reachable LL math definitions, avoiding unrelated LLSD/IO link dependencies.
        for filename, signatures in (
                ("llquaternion.cpp", ("LLQuaternion::LLQuaternion(const LLMatrix3 &mat)",
                    "const LLQuaternion& LLQuaternion::setEulerAngles(",
                    "LLMatrix3   LLQuaternion::getMatrix3(void) const",
                    "F32 dot(const LLQuaternion &a, const LLQuaternion &b)",
                    "LLQuaternion lerp(F32 t, const LLQuaternion &p, const LLQuaternion &q)",
                    "LLQuaternion slerp( F32 u, const LLQuaternion &a, const LLQuaternion &b )",
                    "LLQuaternion nlerp(F32 t, const LLQuaternion &a, const LLQuaternion &b)",
                    "LLQuaternion    operator*(const LLQuaternion &a, const LLQuaternion &b)",
                    "LLVector3       operator*(const LLVector3 &a, const LLQuaternion &rot)")),
                ("m3math.cpp", ("LLMatrix3::LLMatrix3(const F32 roll, const F32 pitch, const F32 yaw)",
                    "LLMatrix3::LLMatrix3(const LLQuaternion &q)",
                    "const LLMatrix3&    LLMatrix3::setRot(const LLQuaternion &q)",
                    "LLQuaternion    LLMatrix3::quaternion() const",
                    "const LLMatrix3&    LLMatrix3::setRot(const F32 roll, const F32 pitch, const F32 yaw)",
                    "const LLMatrix3&    LLMatrix3::setRows(",
                    "const LLMatrix3&    LLMatrix3::orthogonalize()"))):
            source = (ROOT / "indra/llmath" / filename).read_text(encoding="utf-8")
            source = re.sub(r'#if 0\b.*?#endif', '', source, flags=re.S)
            math_support += "\n".join(body(source, signature) for signature in signatures)
        support_cpp = OUT / "math_support.cpp"
        support_cpp.write_text(math_support, encoding="utf-8")
        obj = OUT / "math_support.obj"
        command = [compiler, "/nologo", "/c", "/std:c++20", "/EHsc", "/O2"] + includes + [
            str(support_cpp), "/Fo:" + str(obj)]
        result = subprocess.run(command, env=env, cwd=OUT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        objects = [str(obj)]
        for name, source in variants.items():
            cpp = OUT / (name + ".cpp")
            exe = OUT / (name + (".exe" if os.name == "nt" else ""))
            cpp.write_text(SHIM + source + tests + MAIN, encoding="utf-8")
            if os.name == "nt":
                command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/Gy", "/Zc:inline"] + includes + [str(cpp)] + objects + [
                           "/Fe:" + str(exe), "/Fo:" + str(OUT / (name + ".obj")), "/link", "/OPT:REF"]
            else:
                command = ["c++", "-std=c++17", str(cpp), "-o", str(exe)]
            result = subprocess.run(command, env=env, cwd=OUT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=OUT, capture_output=True, text=True)
            if name == "production":
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                print("PASS seven production TUT tests plus production reseed/operator/exit/manual-engage/Classic-zoom/FOV fragments (2 degree roll; shake-only FOV stable for 120 frames)")
            else:
                self.assertNotEqual(result.returncode, 0, "surviving mutation: " + name)
                print("REJECTED planted negative: " + name + " (" + result.stderr.strip() + ")")


if __name__ == "__main__":
    unittest.main()
