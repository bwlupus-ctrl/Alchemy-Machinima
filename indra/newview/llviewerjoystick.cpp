/**
 * @file llviewerjoystick.cpp
 * @brief Joystick / NDOF device functionality.
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

#include "llviewerprecompiledheaders.h"

#include "llviewerjoystick.h"
#include "alcameradispatch.h"
#include "alflycamclassic.h"
#include "alxinputmath.h"
#include "llflycamdrone.h"
#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "llpresentationtime.h"

#include "llframetimer.h"
#include "llcallbacklist.h"
#include "llnotificationsutil.h"
#include "llpathcamera.h"       // clear a latched path-camera preview on flycam engage
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llappviewer.h"
#include "llkeyboard.h"
#include "lltoolmgr.h"
#include "llselectmgr.h"
#include "llviewermenu.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llfocusmgr.h"
#include "llcameraoperator.h"   // [phoenix port] procedural handheld operator
#include "llcinematiccamera.h"  // [machinima] Flycam Orbit anchor resolution

#if LL_WINDOWS && !LL_MESA_HEADLESS
// Require DirectInput version 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <Xinput.h>
#include <cwctype>
#endif


// ----------------------------------------------------------------------------
// Constants

#define  X_I    1
#define  Y_I    2
#define  Z_I    0
#define RX_I    4
#define RY_I    5
#define RZ_I    3

std::array<F32, 7>  LLViewerJoystick::sLastDelta = {0,0,0,0,0,0,0};
std::array<F32, 7>  LLViewerJoystick::sDelta = {0,0,0,0,0,0,0};

// These constants specify the maximum absolute value coming in from the device.
// HACK ALERT! the value of MAX_JOYSTICK_INPUT_VALUE is not arbitrary as it
// should be.  It has to be equal to 3000 because the SpaceNavigator on Windows
// refuses to respond to the DirectInput SetProperty call; it always returns
// values in the [-3000, 3000] range.
#define MAX_SPACENAVIGATOR_INPUT  3000.0f
#define MAX_JOYSTICK_INPUT_VALUE  MAX_SPACENAVIGATOR_INPUT

namespace
{
struct FlycamPoseSample
{
    LLVector3d positionGlobal;
    LLQuaternion rotation;
    F64 dt = 0.0;
    bool valid = false;
};

struct FlycamDroneRuntime
{
    bool nonFiniteSinceReport = false;
    bool boundAccelerationSinceReport = false;
    bool boundJerkSinceReport = false;
    bool backlogSinceReport = false;
    bool triggersStale = false;
    F64 reportSeconds = 0.0;
    F64 triggerStaleSeconds = 0.0;
    U64 reportSteps = 0;
    U64 reportClamps = 0;
    F32 maxVelocity = 0.f;
    F32 maxAcceleration = 0.f;
    F32 maxJerk = 0.f;
    F32 accelerationLimitAtMax = 0.f;
    F32 jerkLimitAtMax = 0.f;
    LLFlycamDrone drone;
    LLFlycamDrone::Params baseParams;
    LLFlycamDrone::HumanParams accepted =
        LLFlycamDrone::presets()[static_cast<std::size_t>(LLFlycamDrone::PresetId::NORMAL)].human;
    LLFlycamDrone::HumanParams lastRequested;
    F32 lastRequestedTick = 0.f;
    F32 lastRequestedRoll = 0.f;
    bool haveRequested = false;
    ALFlycamPoseOwner owner = ALFlycamPoseOwner::CLASSIC;
    FlycamPoseSample previousPose;
    FlycamPoseSample currentPose;
    LLFlycamDrone::RenderPose lastDronePose;
    bool haveLastDronePose = false;
    bool resetPending = true;
    U64 lastWrittenSerial = 0;
    F32 buildScale = 1.f;
    bool buildScaleValid = false;

    FlycamDroneRuntime()
    : drone(LLFlycamDrone().params(), [this](const std::string& message)
      {
          if (message.compare(0, 15, "DRONE NONFINITE") == 0)
          {
              nonFiniteSinceReport = true;
          }
          else if (message.compare(0, 18, "DRONE ORIGIN-SHIFT") == 0)
          {
              LL_INFOS("FlycamDrone") << message << LL_ENDL;
          }
      }),
      baseParams(drone.params())
    {}
};

struct FlycamDroneSettings
{
    LLCachedControl<S32> preset{gSavedSettings, "FlycamDronePreset", 1};
    LLCachedControl<F32> cruise{gSavedSettings, "FlycamDroneCruise", 4.f};
    LLCachedControl<F32> top{gSavedSettings, "FlycamDroneTop", 12.f};
    LLCachedControl<F32> timeToTop{gSavedSettings, "FlycamDroneTimeToTop", 2.f};
    LLCachedControl<F32> stopDistance{gSavedSettings, "FlycamDroneStopDistance", 10.f};
    LLCachedControl<F32> holdTime{gSavedSettings, "FlycamDroneHoldTime", .8f};
    LLCachedControl<F32> coastTime{gSavedSettings, "FlycamDroneCoastTime", 4.f};
    LLCachedControl<F32> highSpeedFloat{gSavedSettings, "FlycamDroneHighSpeedFloat", .5f};
    LLCachedControl<F32> onset{gSavedSettings, "FlycamDroneOnset", .1f};
    LLCachedControl<F32> grip{gSavedSettings, "FlycamDroneGrip", .6f};
    LLCachedControl<F32> turnRate{gSavedSettings, "FlycamDroneTurnRate", 70.f};
    LLCachedControl<F32> turnWeight{gSavedSettings, "FlycamDroneTurnWeight", .5f};
    LLCachedControl<F32> tiltRate{gSavedSettings, "FlycamDroneTiltRate", 50.f};
    LLCachedControl<F32> climb{gSavedSettings, "FlycamDroneClimb", 2.f};
    LLCachedControl<F32> climbTime{gSavedSettings, "FlycamDroneClimbTime", .8f};
    LLCachedControl<S32> release{gSavedSettings, "FlycamDroneRelease", 0};
    LLCachedControl<S32> throttleMode{gSavedSettings, "FlycamDroneThrottleMode", 0};
    LLCachedControl<bool> freeFly{gSavedSettings, "FlycamDroneFreeFly", false};
    LLCachedControl<F32> simulationHz{gSavedSettings, "FlycamDroneSimulationHz", 120.f};
    LLCachedControl<F32> moveDeadzone{gSavedSettings, "FlycamDroneMoveDeadzone", .10f};
    LLCachedControl<F32> lookDeadzone{gSavedSettings, "FlycamDroneLookDeadzone", .12f};
    LLCachedControl<F32> saturation{gSavedSettings, "FlycamDroneSaturation", .05f};
    LLCachedControl<F32> expoMove{gSavedSettings, "FlycamDroneExpoMove", .35f};
    LLCachedControl<F32> expoYaw{gSavedSettings, "FlycamDroneExpoYaw", .45f};
    LLCachedControl<F32> expoGimbal{gSavedSettings, "FlycamDroneExpoGimbal", .45f};
    LLCachedControl<S32> resumeMomentum{gSavedSettings, "FlycamDroneResumeMomentum", 0};
    LLCachedControl<F32> triggerDeadzone{gSavedSettings, "JoystickXInputTriggerDeadZone", .06f};
    LLCachedControl<F32> triggerCurve{gSavedSettings, "JoystickXInputTriggerCurve", 2.f};
};

LLCachedControl<S32>& flycamFlightModel()
{
    static LLCachedControl<S32> setting(gSavedSettings, "FlycamFlightModel", 0);
    return setting;
}

FlycamDroneSettings& droneSettings()
{
    static FlycamDroneSettings settings;
    return settings;
}

LLFlycamDrone::CompiledValues& cachedDroneCompiledValues()
{
    static LLFlycamDrone::CompiledValues values;
    return values;
}

struct DroneCompilePreviewCache
{
    LLFlycamDrone::HumanParams accepted =
        LLFlycamDrone::presets()[static_cast<std::size_t>(LLFlycamDrone::PresetId::NORMAL)].human;
    LLFlycamDrone::Params params;
};

DroneCompilePreviewCache& droneCompilePreviewCache()
{
    static DroneCompilePreviewCache cache;
    return cache;
}

struct DronePresetProvenance
{
    S32 source = static_cast<S32>(LLFlycamDrone::PresetId::NORMAL);
    bool custom = false;
    U64 revision = 1;
};

DronePresetProvenance& dronePresetProvenance()
{
    static DronePresetProvenance provenance;
    return provenance;
}

void syncDronePresetProvenance(S32 preset)
{
    DronePresetProvenance& provenance = dronePresetProvenance();
    if (preset >= 0 && preset < static_cast<S32>(LLFlycamDrone::PresetId::COUNT))
    {
        provenance.source = preset;
        provenance.custom = false;
    }
    else
    {
        provenance.custom = true;
    }
    ++provenance.revision;
}

LLFlycamDrone::HumanParams requestedDroneHumanParams()
{
    FlycamDroneSettings& s = droneSettings();
    LLFlycamDrone::HumanParams requested;
    requested.cruise = s.cruise;
    requested.top = s.top;
    requested.timeToTop = s.timeToTop;
    requested.stopDistance = s.stopDistance;
    requested.holdTime = s.holdTime;
    requested.coastTime = s.coastTime;
    requested.highSpeedFloat = s.highSpeedFloat;
    requested.onset = s.onset;
    requested.grip = s.grip;
    requested.turnRateDegrees = s.turnRate;
    requested.turnWeight = s.turnWeight;
    requested.tiltRateDegrees = s.tiltRate;
    requested.climb = s.climb;
    requested.climbTime = s.climbTime;
    requested.release = (S32)s.release == 1
        ? LLFlycamDrone::ReleaseMode::DRIFT : LLFlycamDrone::ReleaseMode::HOLD;
    requested.throttleMode = (S32)s.throttleMode == 1
        ? LLFlycamDrone::ThrottleMode::PURE : LLFlycamDrone::ThrottleMode::ASSIST;
    requested.freeFly = s.freeFly;
    return requested;
}

U64& droneCompilePreviewGeneration()
{
    static U64 generation = 0;
    return generation;
}

void refreshDroneCompilePreviewCache()
{
    DroneCompilePreviewCache& cache = droneCompilePreviewCache();
    const LLFlycamDrone::HumanParams requested = requestedDroneHumanParams();
    const F32 requested_hz = droneSettings().simulationHz;
    LLFlycamDrone::compile(requested, cache.accepted, 1.f / requested_hz,
        fabsf(gSavedSettings.getF32("FlycamAxisScale3")), cache.params, &cache.accepted);
    cachedDroneCompiledValues() = { cache.params.totalLimit, cache.params.effectiveJerk,
                                    cache.params.validationClamps };
}

void scheduleDroneCompilePreview()
{
    const U64 generation = ++droneCompilePreviewGeneration();
    doAfterInterval([generation]()
    {
        if (generation == droneCompilePreviewGeneration())
        {
            refreshDroneCompilePreviewCache();
        }
    }, .15f);
}

F32 signedExpo(F32 value, F32 amount)
{
    const F32 magnitude = llclamp(fabsf(value), 0.f, 1.f);
    const F32 k = llclamp(amount, 0.f, 1.f);
    const F32 shaped = (1.f - k) * magnitude + k * magnitude * magnitude * magnitude;
    return value < 0.f ? -shaped : shaped;
}

bool sameSetting(F32 a, F32 b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

bool sameHumanParams(const LLFlycamDrone::HumanParams& a,
                     const LLFlycamDrone::HumanParams& b)
{
    return sameSetting(a.cruise, b.cruise) && sameSetting(a.top, b.top) &&
        sameSetting(a.timeToTop, b.timeToTop) &&
        sameSetting(a.stopDistance, b.stopDistance) &&
        sameSetting(a.holdTime, b.holdTime) && sameSetting(a.coastTime, b.coastTime) &&
        sameSetting(a.highSpeedFloat, b.highSpeedFloat) &&
        sameSetting(a.onset, b.onset) && sameSetting(a.grip, b.grip) &&
        sameSetting(a.turnRateDegrees, b.turnRateDegrees) &&
        sameSetting(a.turnWeight, b.turnWeight) &&
        sameSetting(a.tiltRateDegrees, b.tiltRateDegrees) &&
        sameSetting(a.climb, b.climb) && sameSetting(a.climbTime, b.climbTime) &&
        a.release == b.release && a.throttleMode == b.throttleMode &&
        a.freeFly == b.freeFly;
}

void logDroneClamp(FlycamDroneRuntime& runtime, const char* name,
                   F32 requested, F32 applied)
{
    if (sameSetting(requested, applied))
        return;
    LL_INFOS("FlycamDrone") << "DRONE CLAMP " << name << ' ' << requested
        << "\xE2\x86\x92" << applied << LL_ENDL;
    ++runtime.reportClamps;
}

const char* dronePresetName(S32 preset)
{
    const auto& presets = LLFlycamDrone::presets();
    return preset >= 0 && preset < static_cast<S32>(presets.size())
        ? presets[static_cast<std::size_t>(preset)].name : "Custom";
}

void logDroneValidation(FlycamDroneRuntime& runtime,
                        const LLFlycamDrone::HumanParams& requested,
                        const LLFlycamDrone::HumanParams& applied,
                        F32 requestedHz, const LLFlycamDrone::Params& compiled,
                        F32 requestedRoll)
{
    logDroneClamp(runtime, "cruise", requested.cruise, applied.cruise);
    logDroneClamp(runtime, "top", requested.top, applied.top);
    logDroneClamp(runtime, "timeToTop", requested.timeToTop, applied.timeToTop);
    logDroneClamp(runtime, "stopDistance", requested.stopDistance, applied.stopDistance);
    logDroneClamp(runtime, "holdTime", requested.holdTime, applied.holdTime);
    logDroneClamp(runtime, "coastTime", requested.coastTime, applied.coastTime);
    logDroneClamp(runtime, "highSpeedFloat", requested.highSpeedFloat, applied.highSpeedFloat);
    logDroneClamp(runtime, "onset", requested.onset, applied.onset);
    logDroneClamp(runtime, "grip", requested.grip, applied.grip);
    logDroneClamp(runtime, "turnRate", requested.turnRateDegrees, applied.turnRateDegrees);
    logDroneClamp(runtime, "turnWeight", requested.turnWeight, applied.turnWeight);
    logDroneClamp(runtime, "tiltRate", requested.tiltRateDegrees, applied.tiltRateDegrees);
    logDroneClamp(runtime, "climb", requested.climb, applied.climb);
    logDroneClamp(runtime, "climbTime", requested.climbTime, applied.climbTime);
    const F32 applied_hz = compiled.tick > 0.f ? 1.f / compiled.tick : 0.f;
    logDroneClamp(runtime, "simulationHz", requestedHz, applied_hz);
    logDroneClamp(runtime, "rollRate", requestedRoll, compiled.rollRate);
}

void shapeRadial(F32& x, F32& y, F32 deadzone, F32 saturation, F32 expo)
{
    const F32 radius = sqrtf(x * x + y * y);
    const F32 d = llclamp(deadzone, 0.f, .5f);
    const F32 o = llclamp(saturation, 0.f, .5f);
    const F32 denominator = llmax(1.f - d - o, .001f);
    const F32 remapped = llclamp((radius - d) / denominator, 0.f, 1.f);
    if (radius <= 0.f || remapped <= 0.f)
    {
        x = y = 0.f;
        return;
    }
    const F32 shaped = signedExpo(remapped, expo) / radius;
    x *= shaped;
    y *= shaped;
}

LLFlycamDrone::Params buildScaledParams(const LLFlycamDrone::Params& source, F32 scale)
{
    LLFlycamDrone::Params p = source;
    const F32 s = llclamp(scale, .001f, 1000.f);
    p.cruise *= s;
    p.top *= s;
    p.acceleration *= s;
    p.deceleration *= s;
    p.brakeDeceleration *= s;
    p.dragQuadratic /= s;
    p.climbSpeed *= s;
    p.climbAcceleration *= s;
    p.speedReference *= s;
    p.velocityCap *= s;
    p.horizontalLimit *= s;
    p.dragLimit *= s;
    p.totalLimit *= s;
    p.effectiveJerk *= s;
    return p;
}

LLVector3 angularVelocity(const LLQuaternion& from, const LLQuaternion& to, F32 dt)
{
    if (!(dt > 0.f) || !std::isfinite(dt) || !from.isFinite() || !to.isFinite())
        return LLVector3::zero;
    LLQuaternion delta = to * ~from;
    if (delta.mQ[VW] < 0.f)
        delta = -delta;
    delta.normalize();
    F32 angle = 0.f;
    LLVector3 axis;
    delta.getAngleAxis(&angle, axis);
    return axis * (angle / dt);
}

void configureDrone(FlycamDroneRuntime& runtime, F32 rollRate,
                    bool autoLevel, bool buildMode, F32 frameDt,
                    F32 buildModeScale)
{
    FlycamDroneSettings& s = droneSettings();
    const LLFlycamDrone::HumanParams requested = requestedDroneHumanParams();

    LLFlycamDrone::Params compiled = runtime.baseParams;
    const F32 requested_hz = s.simulationHz;
    const F32 requested_tick = 1.f / requested_hz;
    const F32 requested_roll = fabsf(rollRate);
    if (!runtime.haveRequested || !sameHumanParams(requested, runtime.lastRequested) ||
        !sameSetting(requested_tick, runtime.lastRequestedTick) ||
        !sameSetting(requested_roll, runtime.lastRequestedRoll))
    {
        LLFlycamDrone::compile(requested, runtime.accepted, requested_tick,
            requested_roll, compiled, &runtime.accepted);
        logDroneValidation(runtime, requested, runtime.accepted, requested_hz,
                           compiled, requested_roll);
        runtime.lastRequested = requested;
        runtime.lastRequestedTick = requested_tick;
        runtime.lastRequestedRoll = requested_roll;
        runtime.haveRequested = true;
    }
    compiled.autoLevel = autoLevel;
    compiled.yawExpo = llclamp((F32)s.expoYaw, 0.f, 1.f);
    compiled.gimbalExpo = llclamp((F32)s.expoGimbal, 0.f, 1.f);
    runtime.baseParams = compiled;

    const F32 targetScale = buildMode
        ? llclamp(buildModeScale, .001f, 1000.f) : 1.f;
    if (!runtime.buildScaleValid)
    {
        runtime.buildScale = targetScale;
        runtime.buildScaleValid = true;
    }
    else
    {
        const F32 dt = llclamp(frameDt, 0.f, .25f);
        runtime.buildScale += (targetScale - runtime.buildScale) *
            (1.f - expf(-dt / .3f));
    }
    runtime.drone.setParams(buildScaledParams(compiled, runtime.buildScale));
}
}


#if LIB_NDOF
std::ostream& operator<<(std::ostream& out, NDOF_Device* ptr)
{
    if (! ptr)
    {
        return out << "nullptr";
    }
    out << "NDOF_Device{ ";
    out << "axes [";
    const char* delim = "";
    for (short axis = 0; axis < ptr->axes_count; ++axis)
    {
        out << delim << ptr->axes[axis];
        delim = ", ";
    }
    out << "]";
    out << ", buttons [";
    delim = "";
    for (short button = 0; button < ptr->btn_count; ++button)
    {
        out << delim << ptr->buttons[button];
        delim = ", ";
    }
    out << "]";
    out << ", range " << ptr->axes_min << ':' << ptr->axes_max;
    // If we don't coerce these to unsigned, they're streamed as characters,
    // e.g. ctrl-A or nul.
    out << ", absolute " << unsigned(ptr->absolute);
    out << ", valid " << unsigned(ptr->valid);
    out << ", manufacturer '" << ptr->manufacturer << "'";
    out << ", product '" << ptr->product << "'";
    out << ", private " << ptr->private_data;
    out << " }";
    return out;
}
#endif // LIB_NDOF


#if LL_WINDOWS && !LL_MESA_HEADLESS
namespace
{
// [XInputPad] Types only from Xinput.h: no import library or application DLL search.
HMODULE sXInputLibrary = nullptr;
using XInputGetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetStateFn sXInputGetState = nullptr;
using XInputSetStateFn = DWORD (WINAPI*)(DWORD, XINPUT_VIBRATION*);
XInputSetStateFn sXInputSetState = nullptr; // [XInputRumble] optional; absence only disables rumble
bool sXInputLoadAttempted = false;

bool loadXInput()
{
    if (sXInputLoadAttempted) return sXInputGetState != nullptr;
    sXInputLoadAttempted = true;
    for (const wchar_t* name : { L"xinput1_4.dll", L"xinput9_1_0.dll" })
    {
        HMODULE library = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            wchar_t directory[MAX_PATH];
            const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
            {
                const std::wstring path = std::wstring(directory) + L"\\" + name;
                library = LoadLibraryW(path.c_str());
            }
        }
        if (!library) continue;
        FARPROC symbol = GetProcAddress(library, "XInputGetState");
        if (symbol)
        {
            static_assert(sizeof(symbol) == sizeof(sXInputGetState), "function pointer size");
            memcpy(&sXInputGetState, &symbol, sizeof(symbol)); // avoid MSVC C4191
            sXInputLibrary = library;
            FARPROC set_symbol = GetProcAddress(library, "XInputSetState");
            if (set_symbol)
            {
                static_assert(sizeof(set_symbol) == sizeof(sXInputSetState), "function pointer size");
                memcpy(&sXInputSetState, &set_symbol, sizeof(set_symbol));
            }
            else LL_INFOS("XInput") << "[XInputRumble] XInputSetState missing; rumble inert." << LL_ENDL;
            return true;
        }
        FreeLibrary(library);
    }
    LL_INFOS("XInput") << "[XInputPad] System XInput unavailable; feature inert." << LL_ENDL;
    return false;
}

struct XInputDeviceProbe
{
    LPDIRECTINPUT8 di = nullptr;
    S32 count = 0;
    bool failed = false;
    LLSD guid;
};

BOOL CALLBACK di8_xinput_probe_callback(LPCDIDEVICEINSTANCE instance, LPVOID data)
{
    auto& probe = *static_cast<XInputDeviceProbe*>(data);
    LPDIRECTINPUTDEVICE8 device = nullptr;
    if (!instance || FAILED(probe.di->CreateDevice(instance->guidInstance, &device, nullptr)))
    {
        probe.failed = true;
        return DIENUM_CONTINUE;
    }
    DIPROPGUIDANDPATH property = {};
    property.diph.dwSize = sizeof(property);
    property.diph.dwHeaderSize = sizeof(property.diph);
    property.diph.dwHow = DIPH_DEVICE;
    const HRESULT result = device->GetProperty(DIPROP_GUIDANDPATH, &property.diph);
    device->Release();
    if (FAILED(result))
    {
        probe.failed = true;
        return DIENUM_CONTINUE;
    }
    std::wstring path(property.wszPath);
    std::transform(path.begin(), path.end(), path.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    if (path.find(L"IG_") != std::wstring::npos)
    {
        ++probe.count;
        LLSD::Binary guid(sizeof(GUID));
        memcpy(guid.data(), &instance->guidInstance, sizeof(GUID));
        probe.guid = LLSD(guid);
        LL_DEBUGS("XInput") << "[XInputPad] IG product VID/PID " << instance->guidProduct.Data1 << LL_ENDL;
    }
    return DIENUM_CONTINUE;
}

constexpr F32 XBOX_DEVICE_POLL_SECONDS = 3.f;

bool isXboxProductName(std::string product_name)
{
    LLStringUtil::toLower(product_name);
    return product_name.find("xbox") != std::string::npos;
}

bool sameWindowsDeviceId(const LLSD& lhs, const LLSD& rhs)
{
    return lhs.isBinary() && rhs.isBinary() &&
           lhs.asBinary() == rhs.asBinary();
}

struct XboxDeviceProbe
{
    bool mFound = false;
    std::string mProductName;
    LLSD mGuid;
};

BOOL CALLBACK di8_xbox_probe_callback(
    LPCDIDEVICEINSTANCE device_instance_ptr, LPVOID user_data)
{
    XboxDeviceProbe* probe = static_cast<XboxDeviceProbe*>(user_data);
    if (!device_instance_ptr || !probe)
    {
        return DIENUM_CONTINUE;
    }

    std::string product_name = ll_convert_wide_to_string(
        std::wstring(device_instance_ptr->tszProductName));
    if (!isXboxProductName(product_name))
    {
        return DIENUM_CONTINUE;
    }

    LLSD::Binary binary_data(sizeof(GUID));
    memcpy(binary_data.data(), &device_instance_ptr->guidInstance,
           sizeof(GUID));
    probe->mFound = true;
    probe->mProductName = product_name;
    probe->mGuid = LLSD(binary_data);
    return DIENUM_STOP;
}
}

// this should reflect ndof and set axises, see ndofdev_win.cpp from ndof package
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* inst, VOID* user_data)
{
    if (inst->dwType & DIDFT_AXIS)
    {
        LPDIRECTINPUTDEVICE8 device = *((LPDIRECTINPUTDEVICE8 *)user_data);
        DIPROPRANGE diprg;
        diprg.diph.dwSize = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwHow = DIPH_BYID;
        diprg.diph.dwObj = inst->dwType; // specify the enumerated axis

        // Set the range for the axis
        diprg.lMin = (long)-MAX_JOYSTICK_INPUT_VALUE;
        diprg.lMax = (long)+MAX_JOYSTICK_INPUT_VALUE;
        HRESULT hr = device->SetProperty(DIPROP_RANGE, &diprg.diph);

        if (FAILED(hr))
        {
            return DIENUM_STOP;
        }
    }

    return DIENUM_CONTINUE;
}

BOOL CALLBACK di8_devices_callback(LPCDIDEVICEINSTANCE device_instance_ptr, LPVOID pvRef)
{
    // Note: If a single device can function as more than one DirectInput
    // device type, it is enumerated as each device type that it supports.
    // Capable of detecting devices like Oculus Rift
    if (device_instance_ptr)
    {
        std::string product_name = ll_convert_wide_to_string(std::wstring(device_instance_ptr->tszProductName));

        LLSD guid = LLViewerJoystick::getInstance()->getDeviceUUID();

        bool init_device = false;
        if (guid.isBinary())
        {
            std::vector<U8> bin_bucket = guid.asBinary();
            init_device = memcmp(&bin_bucket[0], &device_instance_ptr->guidInstance, sizeof(GUID)) == 0;
        }
        else
        {
            // It might be better to init space navigator here, but if system doesn't has one,
            // ndof will pick a random device, it is simpler to pick first device now to have an id
            init_device = true;
        }

        if (init_device)
        {
            LL_DEBUGS("Joystick") << "Found and attempting to use device: " << product_name << LL_ENDL;
            LPDIRECTINPUT8       di8_interface = *((LPDIRECTINPUT8 *)gViewerWindow->getWindow()->getDirectInput8());
            LPDIRECTINPUTDEVICE8 device = NULL;

            HRESULT status = di8_interface->CreateDevice(
                device_instance_ptr->guidInstance, // REFGUID rguid,
                &device,                           // LPDIRECTINPUTDEVICE * lplpDirectInputDevice,
                NULL                               // LPUNKNOWN pUnkOuter
                );

            if (status == DI_OK)
            {
                // prerequisite for aquire()
                LL_DEBUGS("Joystick") << "Device created" << LL_ENDL;
                status = device->SetDataFormat(&c_dfDIJoystick); // c_dfDIJoystick2
            }

            if (status == DI_OK)
            {
                // set properties
                LL_DEBUGS("Joystick") << "Format set" << LL_ENDL;
                status = device->EnumObjects(EnumObjectsCallback, &device, DIDFT_ALL);
            }

            if (status == DI_OK)
            {
                LL_DEBUGS("Joystick") << "Properties updated" << LL_ENDL;

                S32 size = sizeof(GUID);
                LLSD::Binary binary_data; //just an std::vector
                binary_data.resize(size);
                memcpy(&binary_data[0], &device_instance_ptr->guidInstance /*POD _GUID*/, size);
                LLSD data(binary_data);
                LLViewerJoystick::getInstance()->initDevice(&device, product_name, data);
                return DIENUM_STOP;
            }
        }
        else
        {
            LL_DEBUGS("Joystick") << "Found device: " << product_name << LL_ENDL;
        }
    }
    return DIENUM_CONTINUE;
}

// Windows guids
// This is GUID2 so teoretically it can be memcpy copied into LLUUID
void guid_from_string(GUID &guid, const std::string &input)
{
    CLSIDFromString(ll_convert_string_to_wide(input).c_str(), &guid);
}

std::string string_from_guid(const GUID &guid)
{
    OLECHAR* guidString; //wchat
    StringFromCLSID(guid, &guidString);

    // use guidString...

    std::string res = ll_convert_wide_to_string(std::wstring(guidString));
    // ensure memory is freed
    ::CoTaskMemFree(guidString);

    return res;
}
#elif LL_DARWIN

bool macos_devices_callback(std::string &product_name, LLSD &data, void* userdata)
{
    std::string product = data["product"].asString();

    return LLViewerJoystick::getInstance()->initDevice(nullptr, product, data);
}

#endif


// -----------------------------------------------------------------------------
void LLViewerJoystick::updateEnabled(bool autoenable)
{
    if (mDriverState == JDS_UNINITIALIZED)
    {
        gSavedSettings.setBOOL("JoystickEnabled", FALSE );
    }
    else
    {
        // autoenable if user specifically chose this device
        if (autoenable && (isLikeSpaceNavigator() || isDeviceUUIDSet()))
        {
            gSavedSettings.setBOOL("JoystickEnabled", TRUE );
        }
    }
    if (!gSavedSettings.getBOOL("JoystickEnabled"))
    {
        mOverrideCamera = FALSE;
    }
}

void LLViewerJoystick::setOverrideCamera(bool val)
{
    mXILevelPending = false;
    if (!mJoystickEnabled)
    {
        mOverrideCamera = FALSE;
    }
    else
    {
        mOverrideCamera = val;
    }

    updateXInputTakeover();
    if (mOverrideCamera)
    {
        gAgentCamera.changeCameraToDefault();
    }
}

// -----------------------------------------------------------------------------
#if LIB_NDOF
NDOF_HotPlugResult LLViewerJoystick::HotPlugAddCallback(NDOF_Device *dev)
{
    NDOF_HotPlugResult res = NDOF_DISCARD_HOTPLUGGED;
    LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
    if (joystick->mDriverState == JDS_UNINITIALIZED)
    {
        LL_INFOS("Joystick") << "HotPlugAddCallback: will use device:" << LL_ENDL;
        ndof_dump(stderr, dev);
        joystick->mNdofDev = dev;
        joystick->mDriverState = JDS_INITIALIZED;
        res = NDOF_KEEP_HOTPLUGGED;
    }
    joystick->invalidateXInput(true);
    joystick->updateEnabled(true);
    return res;
}
#endif

// -----------------------------------------------------------------------------
#if LIB_NDOF
void LLViewerJoystick::HotPlugRemovalCallback(NDOF_Device *dev)
{
    LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
    if (joystick->mNdofDev == dev)
    {
        LL_INFOS("Joystick") << "HotPlugRemovalCallback: joystick->mNdofDev="
                << joystick->mNdofDev << "; removed device:" << LL_ENDL;
        ndof_dump(stderr, dev);
        joystick->mDriverState = JDS_UNINITIALIZED;
    }
    joystick->invalidateXInput(true);
    joystick->updateEnabled(true);
}
#endif

// -----------------------------------------------------------------------------
LLViewerJoystick::LLViewerJoystick()
:   mDriverState(JDS_UNINITIALIZED),
    mNdofDev(NULL),
    mResetFlag(false),
    mCameraUpdated(true),
    mOverrideCamera(false),
    mJoystickRun(0),
    mXboxWasPresent(false),
    mXboxUserReleased(false)
{
    for (int i = 0; i < 6; i++)
    {
        mAxes[i] = sDelta[i] = sLastDelta[i] = 0.0f;
    }

    memset(mBtn, 0, sizeof(mBtn));

    // factor in bandwidth? bandwidth = gViewerStats->mKBitStat
    mPerfScale = 4000.f / (F32)gSysCPU.getMHz(); // hmm.  why?  [fork: getMHz returns F64 here]

    mLastDeviceUUID = LLSD::Integer(1);

    gSavedSettings.getControl("JoystickEnabled")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("JoystickAxis0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickAxis6")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("Cursor3D")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AutoLeveling")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("ZoomDirect")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("JoystickAvatarEnabled")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickBuildEnabled")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickFlycamEnabled")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("AvatarAxisScale0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisScale1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisScale2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisScale3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisScale4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisScale5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("BuildAxisScale0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisScale1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisScale2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisScale3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisScale4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisScale5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("FlycamAxisScale0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisScale6")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("AvatarAxisDeadZone0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisDeadZone1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisDeadZone2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisDeadZone3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisDeadZone4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("AvatarAxisDeadZone5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("BuildAxisDeadZone0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisDeadZone1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisDeadZone2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisDeadZone3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisDeadZone4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildAxisDeadZone5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("FlycamAxisDeadZone0")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone1")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone2")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone3")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone4")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone5")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamAxisDeadZone6")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("AvatarFeathering")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("BuildFeathering")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("FlycamFeathering")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("JoystickInvertPitch")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    gSavedSettings.getControl("JoystickButtonRollLeft")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonRollRight")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonRollDefault")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonZoomOut")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonZoomIn")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonZoomDefault")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonJump")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonCrouch")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonFly")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonMouselook")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonFlycam")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });
    gSavedSettings.getControl("JoystickButtonRunToggle")->getCommitSignal()->connect([this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val) { refreshFromSettings(); });

    static const char* const drone_compile_controls[] =
    {
        "FlycamDroneCruise", "FlycamDroneTop", "FlycamDroneTimeToTop",
        "FlycamDroneStopDistance", "FlycamDroneHoldTime", "FlycamDroneCoastTime",
        "FlycamDroneHighSpeedFloat", "FlycamDroneOnset", "FlycamDroneGrip",
        "FlycamDroneTurnRate", "FlycamDroneTurnWeight", "FlycamDroneTiltRate",
        "FlycamDroneClimb", "FlycamDroneClimbTime", "FlycamDroneRelease",
        "FlycamDroneThrottleMode", "FlycamDroneFreeFly", "FlycamDroneSimulationHz",
        "FlycamAxisScale3"
    };
    for (const char* name : drone_compile_controls)
    {
        gSavedSettings.getControl(name)->getCommitSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&)
            {
                scheduleDroneCompilePreview();
            });
    }

    gSavedSettings.getControl("FlycamDronePreset")->getCommitSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            syncDronePresetProvenance(value.asInteger());
        });

    refreshFromSettings();
    refreshDroneCompilePreviewCache();
    syncDronePresetProvenance(gSavedSettings.getS32("FlycamDronePreset"));
}

// -----------------------------------------------------------------------------
LLViewerJoystick::~LLViewerJoystick()
{
    if (mDriverState == JDS_INITIALIZED)
    {
        terminate();
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::init(bool autoenable, bool apply_defaults)
{
    ALXInput::DpadSettings dpad = ALXInput::migrateDroneDpad({
        gSavedSettings.getS32("JoystickXInputDpadUp"),
        gSavedSettings.getS32("JoystickXInputDpadLeft"),
        gSavedSettings.getS32("JoystickXInputDpadRight"),
        gSavedSettings.getS32("JoystickXInputDpadMigrationVersion")});
    gSavedSettings.setS32("JoystickXInputDpadUp", dpad.up);
    gSavedSettings.setS32("JoystickXInputDpadLeft", dpad.left);
    gSavedSettings.setS32("JoystickXInputDpadRight", dpad.right);
    gSavedSettings.setS32("JoystickXInputDpadMigrationVersion", dpad.version);

#if LIB_NDOF
    static bool libinit = false;
    mDriverState = JDS_INITIALIZING;

    loadDeviceIdFromSettings();

    if (libinit == false)
    {
        // Note: The HotPlug callbacks are not actually getting called on Windows
        if (ndof_libinit(HotPlugAddCallback,
                         HotPlugRemovalCallback,
                         gViewerWindow->getWindow()->getDirectInput8()))
        {
            mDriverState = JDS_UNINITIALIZED;
        }
        else
        {
            // NB: ndof_libinit succeeds when there's no device
            libinit = true;

            // allocate memory once for an eventual device
            mNdofDev = ndof_create();
        }
    }

    if (libinit)
    {
        if (mNdofDev)
        {
            U32 device_type = 0;
            void* win_callback = nullptr;
            std::function<bool(std::string&, LLSD&, void*)> osx_callback;
            // di8_devices_callback callback is immediate and happens in scope of getInputDevices()
#if LL_WINDOWS && !LL_MESA_HEADLESS
            // space navigator is marked as DI8DEVCLASS_GAMECTRL in ndof lib
            device_type = DI8DEVCLASS_GAMECTRL;
            win_callback = &di8_devices_callback;
#elif LL_DARWIN
            osx_callback = macos_devices_callback;

            if (mLastDeviceUUID.isMap())
            {
                std::string manufacturer = mLastDeviceUUID["manufacturer"].asString();
                std::string product = mLastDeviceUUID["product"].asString();

                strncpy(mNdofDev->manufacturer, manufacturer.c_str(), sizeof(mNdofDev->manufacturer));
                strncpy(mNdofDev->product, product.c_str(), sizeof(mNdofDev->product));

                if (ndof_init_first(mNdofDev, nullptr))
                {
                    mDriverState = JDS_INITIALIZING;
                    // Saved device no longer exist
                    // No device found
                    LL_WARNS() << "ndof_init_first FAILED" << LL_ENDL;
                }
                else
                {
                    mDriverState = JDS_INITIALIZED;
                }
            }
#endif
            if (mDriverState != JDS_INITIALIZED)
            {
                if (!gViewerWindow->getWindow()->getInputDevices(device_type, osx_callback, win_callback, NULL))
                {
                    LL_INFOS("Joystick") << "Failed to gather input devices. Falling back to ndof's init" << LL_ENDL;
                    // Failed to gather devices, init first suitable one
                    mLastDeviceUUID = LLSD();
                    void *preffered_device = NULL;
                    initDevice(preffered_device);
                }
            }

            if (mDriverState == JDS_INITIALIZING)
            {
                LL_INFOS("Joystick") << "Found no matching joystick devices." << LL_ENDL;
                mDriverState = JDS_UNINITIALIZED;
            }
        }
        else
        {
            mDriverState = JDS_UNINITIALIZED;
        }
    }

    // Autoenable the joystick for recognized devices if nothing was connected previously
    if (!autoenable)
    {
        autoenable = gSavedSettings.getString("JoystickInitialized").empty() ? true : false;
    }
    updateEnabled(autoenable);

    if (mDriverState == JDS_INITIALIZED)
    {
        // A Joystick device is plugged in
        if (isLikeSpaceNavigator())
        {
            // It's a space navigator, we have defaults for it.
            if (apply_defaults &&
                gSavedSettings.getString("JoystickInitialized") != "SpaceNavigator")
            {
                // Only set the defaults if we haven't already (in case they were overridden)
                setSNDefaults();
            }
            gSavedSettings.setString("JoystickInitialized", "SpaceNavigator");
        }
        else
        {
            // It's not a Space Navigator
            gSavedSettings.setString("JoystickInitialized", "UnknownDevice");
        }
    }
    else
    {
        // No device connected, don't change any settings
    }

    LL_INFOS("Joystick") << "ndof: mDriverState=" << mDriverState << "; mNdofDev="
            << mNdofDev << "; libinit=" << libinit << LL_ENDL;
#endif
}

void LLViewerJoystick::refreshFromSettings()
{
    mJoystickEnabled = gSavedSettings.getBOOL("JoystickEnabled");

    mJoystickAxis[0] = gSavedSettings.getS32("JoystickAxis0");
    mJoystickAxis[1] = gSavedSettings.getS32("JoystickAxis1");
    mJoystickAxis[2] = gSavedSettings.getS32("JoystickAxis2");
    mJoystickAxis[3] = gSavedSettings.getS32("JoystickAxis3");
    mJoystickAxis[4] = gSavedSettings.getS32("JoystickAxis4");
    mJoystickAxis[5] = gSavedSettings.getS32("JoystickAxis5");
    mJoystickAxis[6] = gSavedSettings.getS32("JoystickAxis6");

    m3DCursor = gSavedSettings.getBOOL("Cursor3D");
    mAutoLeveling = gSavedSettings.getBOOL("AutoLeveling");
    mZoomDirect = gSavedSettings.getBOOL("ZoomDirect");

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

    mAvatarFeathering = gSavedSettings.getF32("AvatarFeathering"); // note: max feather is 32.0
    mBuildFeathering = gSavedSettings.getF32("BuildFeathering");
    mFlycamFeathering = gSavedSettings.getF32("FlycamFeathering");

    mInvertPitch = gSavedSettings.getBOOL("JoystickInvertPitch");

    mMappedButtons[ROLL_LEFT] = gSavedSettings.getS32("JoystickButtonRollLeft");
    mMappedButtons[ROLL_RIGHT] = gSavedSettings.getS32("JoystickButtonRollRight");
    mMappedButtons[ROLL_DEFAULT] = gSavedSettings.getS32("JoystickButtonRollDefault");
    mMappedButtons[ZOOM_OUT] = gSavedSettings.getS32("JoystickButtonZoomOut");
    mMappedButtons[ZOOM_IN] = gSavedSettings.getS32("JoystickButtonZoomIn");
    mMappedButtons[ZOOM_DEFAULT] = gSavedSettings.getS32("JoystickButtonZoomDefault");
    mMappedButtons[JUMP] = gSavedSettings.getS32("JoystickButtonJump");
    mMappedButtons[CROUCH] = gSavedSettings.getS32("JoystickButtonCrouch");
    mMappedButtons[FLY] = gSavedSettings.getS32("JoystickButtonFly");
    mMappedButtons[MOUSELOOK] = gSavedSettings.getS32("JoystickButtonMouselook");
    mMappedButtons[FLYCAM] = gSavedSettings.getS32("JoystickButtonFlycam");
    mMappedButtons[TOGGLE_RUN] = gSavedSettings.getS32("JoystickButtonRunToggle");
    updateXInputTakeover();
}

void LLViewerJoystick::initDevice(LLSD &guid)
{
#if LIB_NDOF
    invalidateXInput(true);
    mLastDeviceUUID = guid;
    U32 device_type = 0;
    void* win_callback = nullptr;
    std::function<bool(std::string&, LLSD&, void*)> osx_callback;
    mDriverState = JDS_INITIALIZING;

#if LL_WINDOWS && !LL_MESA_HEADLESS
    // space navigator is marked as DI8DEVCLASS_GAMECTRL in ndof lib
    device_type = DI8DEVCLASS_GAMECTRL;
    win_callback = &di8_devices_callback;
#elif LL_DARWIN
    osx_callback = macos_devices_callback;
    if (mLastDeviceUUID.isMap())
    {
        std::string manufacturer = mLastDeviceUUID["manufacturer"].asString();
        std::string product = mLastDeviceUUID["product"].asString();

        strncpy(mNdofDev->manufacturer, manufacturer.c_str(), sizeof(mNdofDev->manufacturer));
        strncpy(mNdofDev->product, product.c_str(), sizeof(mNdofDev->product));

        if (ndof_init_first(mNdofDev, nullptr))
        {
            mDriverState = JDS_INITIALIZING;
            // Saved device no longer exist
            // Np other device present
            LL_WARNS() << "ndof_init_first FAILED" << LL_ENDL;
        }
        else
        {
            mDriverState = JDS_INITIALIZED;
        }
    }
#endif

    if (mDriverState != JDS_INITIALIZED)
    {
        if (!gViewerWindow->getWindow()->getInputDevices(device_type, osx_callback, win_callback, NULL))
        {
            LL_INFOS("Joystick") << "Failed to gather input devices. Falling back to ndof's init" << LL_ENDL;
            // Failed to gather devices from window, init first suitable one
            void *preffered_device = NULL;
            mLastDeviceUUID = LLSD();
            initDevice(preffered_device);
        }
    }

    if (mDriverState == JDS_INITIALIZING)
    {
        LL_INFOS("Joystick") << "Found no matching joystick devices." << LL_ENDL;
        mDriverState = JDS_UNINITIALIZED;
    }
#endif
}

bool LLViewerJoystick::initDevice(void * preffered_device /*LPDIRECTINPUTDEVICE8*/, std::string &name, LLSD &guid)
{
#if LIB_NDOF
    invalidateXInput(true);
    mLastDeviceUUID = guid;

#if LL_DARWIN
    if (guid.isMap())
    {
        std::string manufacturer = mLastDeviceUUID["manufacturer"].asString();
        std::string product = mLastDeviceUUID["product"].asString();

        strncpy(mNdofDev->manufacturer, manufacturer.c_str(), sizeof(mNdofDev->manufacturer));
        strncpy(mNdofDev->product, product.c_str(), sizeof(mNdofDev->product));
    }
    else
    {
        mNdofDev->product[0] = '\0';
        mNdofDev->manufacturer[0] = '\0';
    }
#else
    size_t dest_size = sizeof(mNdofDev->product);
    strncpy(mNdofDev->product, name.c_str(), dest_size);
    mNdofDev->product[dest_size-1] = '\0';

    mNdofDev->manufacturer[0] = '\0';
#endif

    return initDevice(preffered_device);
#else
    return false;
#endif
}

bool LLViewerJoystick::initDevice(void * preffered_device /* LPDIRECTINPUTDEVICE8* */)
{
    invalidateXInput(true);
#if LIB_NDOF
    // Different joysticks will return different ranges of raw values.
    // Since we want to handle every device in the same uniform way,
    // we initialize the mNdofDev struct and we set the range
    // of values we would like to receive.
    //
    // HACK: On Windows, libndofdev passes our range to DI with a
    // SetProperty call. This works but with one notable exception, the
    // SpaceNavigator, who doesn't seem to care about the SetProperty
    // call. In theory, we should handle this case inside libndofdev.
    // However, the range we're setting here is arbitrary anyway,
    // so let's just use the SpaceNavigator range for our purposes.
    mNdofDev->axes_min = (long)-MAX_JOYSTICK_INPUT_VALUE;
    mNdofDev->axes_max = (long)+MAX_JOYSTICK_INPUT_VALUE;

    // libndofdev could be used to return deltas.  Here we choose to
    // just have the absolute values instead.
    mNdofDev->absolute = 1;
    // init & use the first suitable NDOF device found on the USB chain
    // On windows preffered_device needs to be a pointer to LPDIRECTINPUTDEVICE8
    if (ndof_init_first(mNdofDev, preffered_device))
    {
        mDriverState = JDS_UNINITIALIZED;
        LL_WARNS() << "ndof_init_first FAILED" << LL_ENDL;
    }
    else
    {
        mDriverState = JDS_INITIALIZED;
        mXIDisconnectedGuid = LLSD(); // [XInputPad] A newly initialized DI device is readable.
        return true;
    }
#endif
    return false;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::terminate()
{
    stopRumble(true); // [XInputRumble] zero the pad before the library is freed
    invalidateXInput(false);
#if LL_WINDOWS && !LL_MESA_HEADLESS
    sXInputGetState = nullptr;
    sXInputSetState = nullptr;
    if (sXInputLibrary) FreeLibrary(sXInputLibrary);
    sXInputLibrary = nullptr;
#endif
#if LIB_NDOF
    if (mNdofDev != NULL)
    {
        ndof_libcleanup(); // frees alocated memory in mNdofDev
        mDriverState = JDS_UNINITIALIZED;
        mNdofDev = NULL;
        LL_INFOS("Joystick") << "Terminated connection with NDOF device." << LL_ENDL;
    }
#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::updateStatus()
{
#if LIB_NDOF
    if (!mNdofDev || !mNdofDev->private_data)
    {
        return;
    }

    ndof_update(mNdofDev);

#if LL_WINDOWS && !LL_MESA_HEADLESS
    // [XInputPad] Failed DI reads retain cached axes; suppress only the lost pad,
    // and only while the feature is on (turning it off restores legacy reads).
    static LLCachedControl<bool> xi_enabled(gSavedSettings, "JoystickXInputEnabled", true);
    if (!xi_enabled && mXIDisconnectedGuid.isDefined())
    {
        mXIDisconnectedGuid = LLSD();
    }
    const bool disconnected = xi_enabled && sameWindowsDeviceId(mLastDeviceUUID, mXIDisconnectedGuid);
#endif
    for (int i=0; i<6; i++)
    {
        mAxes[i] = (F32) mNdofDev->axes[i] / mNdofDev->axes_max;
#if LL_WINDOWS && !LL_MESA_HEADLESS
        if (disconnected) mAxes[i] = 0.f;
#endif
    }

    for (int i=0; i<16; i++)
    {
        mBtn[i] = mNdofDev->buttons[i];
    }

#endif
}

// -----------------------------------------------------------------------------
F32 LLViewerJoystick::getJoystickAxis(U32 axis) const
{
    if (axis < 6)
    {
        return mAxes[axis];
    }
    return 0.f;
}

// -----------------------------------------------------------------------------
U32 LLViewerJoystick::getJoystickButton(U32 button) const
{
    if (button < 16)
    {
        return mBtn[button];
    }
    return 0;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::handleRun(F32 inc)
{
    // Decide whether to walk or run by applying a threshold, with slight
    // hysteresis to avoid oscillating between the two with input spikes.
    // Analog speed control would be better, but not likely any time soon.
    static LLCachedControl<F32> joy_run_threshold(gSavedSettings, "JoystickRunThreshold");
    if (inc > joy_run_threshold)
    {
        if (1 == mJoystickRun)
        {
            ++mJoystickRun;
//          gAgent.setRunning();
//          gAgent.sendWalkRun(gAgent.getRunning());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
            gAgent.setTempRun();
// [/RLVa:KB]
        }
        else if (0 == mJoystickRun)
        {
            // hysteresis - respond NEXT frame
            ++mJoystickRun;
        }
    }
    else
    {
        if (mJoystickRun > 0)
        {
            --mJoystickRun;
            if (0 == mJoystickRun)
            {
//              gAgent.clearRunning();
//              gAgent.sendWalkRun(gAgent.getRunning());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
                gAgent.clearTempRun();
// [/RLVa:KB]
            }
        }
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentJump()
{
    gAgent.moveUp(1);
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentSlide(F32 inc)
{
    if (inc < 0.f)
    {
        gAgent.moveLeft(1);
    }
    else if (inc > 0.f)
    {
        gAgent.moveLeft(-1);
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentPush(F32 inc)
{
    if (inc < 0.f)                            // forward
    {
        gAgent.moveAt(1, false);
    }
    else if (inc > 0.f)                       // backward
    {
        gAgent.moveAt(-1, false);
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentFly(F32 inc)
{
    // [fork] no ALControlCache in this base; cached control instead
    static LLCachedControl<bool> automatic_fly(gSavedSettings, "AutomaticFly", true);
    if (inc < 0.f)
    {
        if (! (gAgent.getFlying() ||
               !gAgent.canFly() ||
               gAgent.upGrabbed() ||
               !automatic_fly) )
        {
            gAgent.setFlying(true);
        }
        gAgent.moveUp(1);
    }
    else if (inc > 0.f)
    {
        // crouch
        gAgent.moveUp(-1);
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentPitch(F32 pitch_inc)
{
    if (pitch_inc < 0)
    {
        gAgent.setControlFlags(AGENT_CONTROL_PITCH_POS);
    }
    else if (pitch_inc > 0)
    {
        gAgent.setControlFlags(AGENT_CONTROL_PITCH_NEG);
    }

    gAgent.pitch(-pitch_inc);
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentYaw(F32 yaw_inc)
{
    // Cannot steer some vehicles in mouselook if the script grabs the controls
    static LLCachedControl<bool> joy_mouse_yaw(gSavedSettings, "JoystickMouselookYaw");
    if (gAgentCamera.cameraMouselook() && !joy_mouse_yaw)
    {
        gAgent.rotate(-yaw_inc, gAgent.getReferenceUpVector());
    }
    else
    {
        if (yaw_inc < 0)
        {
            gAgent.setControlFlags(AGENT_CONTROL_YAW_POS);
        }
        else if (yaw_inc > 0)
        {
            gAgent.setControlFlags(AGENT_CONTROL_YAW_NEG);
        }

        gAgent.yaw(-yaw_inc);
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::resetDeltas(S32 axis[])
{
    for (U32 i = 0; i < 6; i++)
    {
        sLastDelta[i] = -getJoystickAxis(axis[i]);
        sDelta[i] = 0.f;
    }

    sLastDelta[6] = sDelta[6] = 0.f;
    mResetFlag = false;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveObjects(bool reset)
{
    static bool toggle_send_to_sim = false;

    if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
        || !mJoystickEnabled || !mBuildEnabled)
    {
        return;
    }

    if (reset || mResetFlag)
    {
        resetDeltas(mJoystickAxis);
        return;
    }

    F32 cur_delta[6];
    F32 time = gFrameIntervalSeconds.value();
    bool is_zero = true;

    static LLCachedControl<bool> blackdragon(gSavedSettings, "BlackDragonControls", false);
    if(blackdragon)
    {
        //BD - Avoid making ridicously big movements if there's a big drop in fps
        time = llclamp(time, 0.016f, 0.033f);

    //  //BD - Remappable Joystick Controls


        for (U32 i = 0; i < 6; i++)
        {
            cur_delta[i] = -getJoystickAxis(mJoystickAxis[i]);

            if (i == CAM_X_AXIS)
            {
                cur_delta[i] -= (F32)getJoystickButton(mMappedButtons[ROLL_LEFT]);
                cur_delta[i] += (F32)getJoystickButton(mMappedButtons[ROLL_RIGHT]);
            }
            if (i == Z_AXIS)
            {
                cur_delta[i] += (F32)getJoystickButton(mMappedButtons[JUMP]);
                cur_delta[i] -= (F32)getJoystickButton(mMappedButtons[CROUCH]);
            }

    //      //BD - Invertable Pitch Controls
            if (mInvertPitch && i == 4)
                cur_delta[4] *= -1.f;

            F32 tmp = cur_delta[i];
            F32 axis_deadzone = mBuildAxisDeadZone[i];
            if (m3DCursor || llabs(cur_delta[i]) < axis_deadzone)
            {
                cur_delta[i] = cur_delta[i] - sLastDelta[i];
            }
            sLastDelta[i] = tmp;
            is_zero = is_zero && (cur_delta[i] == 0.f);

            //BD - We assume that delta 1.0 is the maximum.
            if (llabs(cur_delta[i]) > axis_deadzone)
            {
                //BD - Clamp the delta between 1 and -1 while taking the deadzone into account.
                if (cur_delta[i] > 0)
                {
                    cur_delta[i] = llclamp(cur_delta[i] - axis_deadzone, 0.f, 1.f - axis_deadzone);
                }
                else
                {
                    cur_delta[i] = llclamp(cur_delta[i] + axis_deadzone, -1.f + axis_deadzone, 0.f);
                }
                //BD - Rescale the remaining delta to match the maximum to get a new clean 0 to 1 range.
                cur_delta[i] = cur_delta[i] / (1.f - axis_deadzone);
            }
            cur_delta[i] *= mBuildAxisScale[i];

            if (!m3DCursor)
            {
                cur_delta[i] *= time;
            }

            sDelta[i] = sDelta[i] + (cur_delta[i] - sDelta[i]) * time * mBuildFeathering;
        }
    }
    else
    {
        // avoid making ridicously big movements if there's a big drop in fps
        if (time > .2f)
        {
            time = .2f;
        }

        // max feather is 32
        F32 feather = mBuildFeathering;
        bool absolute = m3DCursor;

        for (U32 i = 0; i < 6; i++)
        {
            cur_delta[i] = -mAxes[mJoystickAxis[i]];
    //      //BD - Invertable Pitch Controls
            if (!mInvertPitch && i == 4)
                cur_delta[i] = -cur_delta[i];

            F32 tmp = cur_delta[i];
            if (absolute)
            {
                cur_delta[i] = cur_delta[i] - sLastDelta[i];
            }
            sLastDelta[i] = tmp;
            is_zero = is_zero && (cur_delta[i] == 0.f);

            if (cur_delta[i] > 0)
            {
                cur_delta[i] = llmax(cur_delta[i]- mBuildAxisDeadZone[i], 0.f);
            }
            else
            {
                cur_delta[i] = llmin(cur_delta[i]+ mBuildAxisDeadZone[i], 0.f);
            }
            cur_delta[i] *= mBuildAxisScale[i];

            if (!absolute)
            {
                cur_delta[i] *= time;
            }

            sDelta[i] = sDelta[i] + (cur_delta[i]-sDelta[i])*time*feather;
        }
    }

    U32 upd_type = UPD_NONE;
    LLVector3 v;

    if (!is_zero)
    {
        // Clear AFK state if moved beyond the deadzone
        if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
        {
            gAgent.clearAFK();
        }

        if (sDelta[0] || sDelta[1] || sDelta[2])
        {
            upd_type |= UPD_POSITION;
            v.setVec(sDelta[0], sDelta[1], sDelta[2]);
        }

        if (sDelta[3] || sDelta[4] || sDelta[5])
        {
            upd_type |= UPD_ROTATION;
        }

        // the selection update could fail, so we won't send
        if (LLSelectMgr::getInstance()->selectionMove(v, sDelta[3],sDelta[4],sDelta[5], upd_type))
        {
            toggle_send_to_sim = true;
        }
    }
    else if (toggle_send_to_sim)
    {
        LLSelectMgr::getInstance()->sendSelectionMove();
        toggle_send_to_sim = false;
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveAvatar(bool reset)
{
    if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
        || !mJoystickEnabled || !mAvatarEnabled)
    {
        return;
    }

    if (reset || mResetFlag)
    {
        resetDeltas(mJoystickAxis);
        if (reset)
        {
            // Note: moving the agent triggers agent camera mode;
            //  don't do this every time we set mResetFlag (e.g. because we gained focus)
            gAgent.moveAt(0, true);
        }
        return;
    }

    static LLCachedControl<bool> blackdragon(gSavedSettings, "BlackDragonControls", false);
    if (blackdragon)
    {
        bool is_zero = true;
        static bool button_held = false;
        //BD
        static bool w_button_held = false;
        static bool m_button_held = false;

    //  //BD - Remappable Joystick Controls
        if (getJoystickButton(mMappedButtons[FLY]) == 1 && !button_held)
        {
            button_held = true;
            if (gAgent.getFlying())
            {
                gAgent.setFlying(FALSE);
            }
            else
            {
                gAgent.setFlying(TRUE);
            }
        }
        else if (getJoystickButton(mMappedButtons[FLY]) == 0 && button_held)
        {
            button_held = false;
        }

        if (getJoystickButton(mMappedButtons[TOGGLE_RUN]) == 1 && !w_button_held)
        {
            w_button_held = true;
            if (gAgent.getAlwaysRun())
            {
                gAgent.clearAlwaysRun();
            }
            else
            {
                gAgent.setAlwaysRun();
            }
        }
        else if (getJoystickButton(mMappedButtons[TOGGLE_RUN]) == 0 && w_button_held)
        {
            w_button_held = false;
        }

        if (getJoystickButton(mMappedButtons[MOUSELOOK]) == 1 && !m_button_held)
        {
            m_button_held = true;
            if (gAgentCamera.cameraMouselook())
            {
                gAgentCamera.changeCameraToDefault();
            }
            else
            {
                gAgentCamera.changeCameraToMouselook();
            }
        }
        else if (getJoystickButton(mMappedButtons[MOUSELOOK]) == 0 && m_button_held)
        {
            m_button_held = false;
        }

        // time interval in seconds between this frame and the previous
        F32 time = gFrameIntervalSeconds.value();

        //BD - Avoid making ridicously big movements if there's a big drop in fps
        time = llclamp(time, 0.016f, 0.033f);

        // note: max feather is 32.0

        F32 cur_delta[6] = {};
        F32 val, dom_mov = 0.f;
        U32 dom_axis = Z_I;

        // remove dead zones and determine biggest movement on the joystick
        for (U32 i = 0; i < 6; i++)
        {
            cur_delta[i] = -getJoystickAxis(mJoystickAxis[i]);

            if(i == Z_AXIS)
            {
                cur_delta[i] += (F32)getJoystickButton(mMappedButtons[JUMP]);
                cur_delta[i] -= (F32)getJoystickButton(mMappedButtons[CROUCH]);
            }

            F32 axis_deadzone = mAvatarAxisDeadZone[i];

            //BD - We assume that delta 1.0 is the maximum.
            if (llabs(cur_delta[i]) > axis_deadzone)
            {
                //BD - Clamp the delta between 1 and -1 while taking the deadzone into account.
                if (cur_delta[i] > 0)
                {
                    cur_delta[i] = llclamp(cur_delta[i] - axis_deadzone, 0.f, 1.f - axis_deadzone);
                }
                else
                {
                    cur_delta[i] = llclamp(cur_delta[i] + axis_deadzone, -1.f + axis_deadzone, 0.f);
                }
                //BD - Rescale the remaining delta to match the maximum to get a new clean 0 to 1 range.
                cur_delta[i] = cur_delta[i] / (1.f - axis_deadzone);
            }
            else
            {
                cur_delta[i] = 0.f;
            }

            // we don't care about Roll (RZ) and Z is calculated after the loop
            if (i != Z_I && i != RZ_I)
            {
                // find out the axis with the biggest joystick motion
                val = fabs(cur_delta[i]);
                if (val > dom_mov)
                {
                    dom_axis = i;
                    dom_mov = val;
                }
            }

            cur_delta[i] *= mAvatarAxisScale[i] * time;

            //sDelta[i] = sDelta[i] + (cur_delta[i] - sDelta[i]) * time;
            is_zero = is_zero && (cur_delta[i] == 0.f);
        }

        if (!is_zero)
        {
            // Clear AFK state if moved beyond the deadzone
            if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
            {
                gAgent.clearAFK();
            }

            setCameraNeedsUpdate(true);
        }

    //  //BD - Invertable Pitch Controls
        if (!mInvertPitch)
            cur_delta[RX_I] = -cur_delta[RX_I];

        // forward|backward movements overrule the real dominant movement if
        // they're bigger than its 20%. This is what you want 'cos moving forward
        // is what you do most. We also added a special (even more lenient) case
        // for RX|RY to allow walking while pitching and turning
        if (fabs(cur_delta[Z_I]) > .2f * dom_mov
            || ((dom_axis == RX_I || dom_axis == RY_I)
            && fabs(cur_delta[Z_I]) > .05f * dom_mov))
        {
            dom_axis = Z_I;
        }

        sDelta[X_I] = -cur_delta[X_I];
        sDelta[Y_I] = -cur_delta[Y_I];
        sDelta[Z_I] = -cur_delta[Z_I];
        cur_delta[RX_I] *= -mAvatarAxisScale[RX_I];
        cur_delta[RY_I] *= -mAvatarAxisScale[RY_I];

        sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * mAvatarFeathering;
        sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * mAvatarFeathering;

        handleRun((F32) sqrt(sDelta[Z_I]*sDelta[Z_I] + sDelta[X_I]*sDelta[X_I]));

    //  //BD - Xbox360 Controller Support
        //     Use raw deltas, do not add any stupid limitations or extra dead zones
        //     otherwise alot controllers will cry and camera movement will bug out
        //     or be completely ignored on some controllers. Especially fixes Xbox 360
        //     controller avatar movement.
        agentSlide(sDelta[X_I]);        // move sideways
        agentFly(sDelta[Y_I]);          // up/down & crouch
        agentPush(sDelta[Z_I]);         // forward/back
        agentPitch(sDelta[RX_I]);       // pitch
        agentYaw(sDelta[RY_I]);         // turn
    }
    else
    {
        bool is_zero = true;
        static bool button_held = false;

        if (mBtn[1] == 1)
        {
            // If AutomaticFly is enabled, then button1 merely causes a
            // jump (as the up/down axis already controls flying) if on the
            // ground, or cease flight if already flying.
            // If AutomaticFly is disabled, then button1 toggles flying.
            // [fork] no ALControlCache in this base; cached control instead
            static LLCachedControl<bool> automatic_fly(gSavedSettings, "AutomaticFly", true);
            if (automatic_fly)
            {
                if (!gAgent.getFlying())
                {
                    gAgent.moveUp(1);
                }
                else if (!button_held)
                {
                    button_held = true;
                    gAgent.setFlying(FALSE);
                }
            }
            else if (!button_held)
            {
                button_held = true;
                gAgent.setFlying(!gAgent.getFlying());
            }

            is_zero = false;
        }
        else
        {
            button_held = false;
        }

        // time interval in seconds between this frame and the previous
        F32 time = gFrameIntervalSeconds.value();

        // avoid making ridicously big movements if there's a big drop in fps
        if (time > .2f)
        {
            time = .2f;
        }

        // note: max feather is 32.0
        F32 feather = mAvatarFeathering;

        F32 cur_delta[6];
        F32 val, dom_mov = 0.f;
        U32 dom_axis = Z_I;
    #if LIB_NDOF
        bool absolute = (m3DCursor && mNdofDev->absolute);
    #else
        bool absolute = false;
    #endif
        // remove dead zones and determine biggest movement on the joystick
        for (U32 i = 0; i < 6; i++)
        {
            cur_delta[i] = -mAxes[mJoystickAxis[i]];

            if (!mInvertPitch && i == 4)
                cur_delta[i] = -cur_delta[i];

            if (absolute)
            {
                F32 tmp = cur_delta[i];
                cur_delta[i] = cur_delta[i] - sLastDelta[i];
                sLastDelta[i] = tmp;
            }

            if (cur_delta[i] > 0)
            {
                cur_delta[i] = llmax(cur_delta[i]- mAvatarAxisDeadZone[i], 0.f);
            }
            else
            {
                cur_delta[i] = llmin(cur_delta[i]+ mAvatarAxisDeadZone[i], 0.f);
            }

            // we don't care about Roll (RZ) and Z is calculated after the loop
            if (i != Z_I && i != RZ_I)
            {
                // find out the axis with the biggest joystick motion
                val = fabs(cur_delta[i]);
                if (val > dom_mov)
                {
                    dom_axis = i;
                    dom_mov = val;
                }
            }

            is_zero = is_zero && (cur_delta[i] == 0.f);
        }

        if (!is_zero)
        {
            // Clear AFK state if moved beyond the deadzone
            if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
            {
                gAgent.clearAFK();
            }

            setCameraNeedsUpdate(true);
        }

        // forward|backward movements overrule the real dominant movement if
        // they're bigger than its 20%. This is what you want 'cos moving forward
        // is what you do most. We also added a special (even more lenient) case
        // for RX|RY to allow walking while pitching and turning
        if (fabs(cur_delta[Z_I]) > .2f * dom_mov
            || ((dom_axis == RX_I || dom_axis == RY_I)
            && fabs(cur_delta[Z_I]) > .05f * dom_mov))
        {
            dom_axis = Z_I;
        }

        sDelta[X_I] = -cur_delta[X_I] * mAvatarAxisScale[X_I];
        sDelta[Y_I] = -cur_delta[Y_I] * mAvatarAxisScale[Y_I];
        sDelta[Z_I] = -cur_delta[Z_I] * mAvatarAxisScale[Z_I];
        cur_delta[RX_I] *= -mAvatarAxisScale[RX_I] * mPerfScale;
        cur_delta[RY_I] *= -mAvatarAxisScale[RY_I] * mPerfScale;

        if (!absolute)
        {
            cur_delta[RX_I] *= time;
            cur_delta[RY_I] *= time;
        }
        sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * feather;
        sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * feather;

        handleRun((F32) sqrt(sDelta[Z_I]*sDelta[Z_I] + sDelta[X_I]*sDelta[X_I]));

        // Allow forward/backward movement some priority
        if (dom_axis == Z_I)
        {
            agentPush(sDelta[Z_I]);         // forward/back

            if (fabs(sDelta[X_I])  > .1f)
            {
                agentSlide(sDelta[X_I]);    // move sideways
            }

            if (fabs(sDelta[Y_I])  > .1f)
            {
                agentFly(sDelta[Y_I]);      // up/down & crouch
            }

            // too many rotations during walking can be confusing, so apply
            // the deadzones one more time (quick & dirty), at 50%|30% power
            F32 eff_rx = .3f * mAvatarAxisDeadZone[RX_I];
            F32 eff_ry = .3f * mAvatarAxisDeadZone[RY_I];

            if (sDelta[RX_I] > 0)
            {
                eff_rx = llmax(sDelta[RX_I] - eff_rx, 0.f);
            }
            else
            {
                eff_rx = llmin(sDelta[RX_I] + eff_rx, 0.f);
            }

            if (sDelta[RY_I] > 0)
            {
                eff_ry = llmax(sDelta[RY_I] - eff_ry, 0.f);
            }
            else
            {
                eff_ry = llmin(sDelta[RY_I] + eff_ry, 0.f);
            }


            if (fabs(eff_rx) > 0.f || fabs(eff_ry) > 0.f)
            {
                if (gAgent.getFlying())
                {
                    agentPitch(eff_rx);
                    agentYaw(eff_ry);
                }
                else
                {
                    agentPitch(eff_rx);
                    agentYaw(2.f * eff_ry);
                }
            }
        }
        else
        {
            agentSlide(sDelta[X_I]);        // move sideways
            agentFly(sDelta[Y_I]);          // up/down & crouch
            agentPush(sDelta[Z_I]);         // forward/back
            agentPitch(sDelta[RX_I]);       // pitch
            agentYaw(sDelta[RY_I]);         // turn
        }
    }
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveFlycam(bool reset)
{
    static LLQuaternion         sFlycamRotation;
    static LLVector3            sFlycamPosition;
    static F32                  sFlycamZoom;

    // [machinima] Flycam Orbit state: orbit maintained relative to a moving
    // bone anchor (azimuth/elevation/radius + anchor-local pan offset)
    static bool                 sOrbitWasActive = false;
    static LLVector3            sOrbitAnchorPos;        // smoothed anchor pose
    static LLQuaternion         sOrbitAnchorRot;
    static F32                  sOrbitAzimuth = 0.f;    // radians, anchor frame
    static F32                  sOrbitElevation = 0.f;
    static F32                  sOrbitRadius = 3.f;
    static LLVector3            sOrbitFocus;            // pan offset, anchor-local
    static F32                  sOrbitRoll = 0.f;       // view-axis roll (unleveled mode)
    static FlycamDroneRuntime   sDrone;
    static LLCachedControl<F32> build_mode_scale(gSavedSettings, "FlycamBuildModeScale", 1.0);
    static LLCachedControl<bool> blackdragon(gSavedSettings, "BlackDragonControls", false);

    updateXInputTakeover();
    if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
        || !mJoystickEnabled || !mFlycamEnabled)
    {
        return;
    }

    bool in_build_mode = LLToolMgr::getInstance()->inBuildMode();
    if (reset || mResetFlag)
    {
        sFlycamPosition = LLViewerCamera::getInstance()->getOrigin();
        sFlycamRotation = LLViewerCamera::getInstance()->getQuaternion();
        sFlycamZoom = LLViewerCamera::getInstance()->getView();

        resetDeltas(mJoystickAxis);
        updateXInputTakeover(true);
        // [phoenix port] don't carry reactive momentum across flycam toggles/teleports
        LLCameraOperator::instance().reset();
        // [machinima] re-seed the orbit from the fresh camera pose next frame
        sOrbitWasActive = false;

        const bool drone_selected = (S32)flycamFlightModel() == 1 && !m3DCursor;
        if (drone_selected)
        {
            configureDrone(sDrone, mFlycamAxisScale[CAM_X_AXIS], mAutoLeveling,
                           in_build_mode, gFrameIntervalSeconds.value(), build_mode_scale);
            const LLVector3d origin = gAgent.getPosGlobalFromAgent(LLVector3::zero);
            sDrone.drone.seed(sFlycamPosition, sFlycamRotation, LLVector3::zero, origin);
            sDrone.lastDronePose = sDrone.drone.renderPose();
            sDrone.haveLastDronePose = true;
        }
        sDrone.owner = drone_selected ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::CLASSIC;
        sDrone.previousPose = FlycamPoseSample();
        sDrone.currentPose = FlycamPoseSample();
        sDrone.resetPending = true;
        mXILevelPending = false;
        mXIRecentrePending = false;
        mDroneCameraOwner = false;
        mDroneRecentreActive = false;

        return;
    }

    F32 trigger_roll = 0.f;
    F32 trigger_throttle = 1.f;
    computeTriggerOutputs(trigger_roll, trigger_throttle);
    const bool level_horizon = mXILevelPending;
    if (mXIDebugTimer.getElapsedTimeF32() >= 1.f && xInputAssociated())
    {
        mXIDebugTimer.reset();
        LL_DEBUGS("XInput") << "[XInputPad] assoc=" << xInputAssociated()
            << " compat=" << ALXInput::compatible(mJoystickAxis, m3DCursor)
            << " takeover=" << mXITakeover << " mode=" << mXIMode
            << " LT=" << static_cast<U32>(mXILT) << " RT=" << static_cast<U32>(mXIRT)
            << " m=" << trigger_throttle << " r=" << trigger_roll << LL_ENDL;
    }

    // Resolve Orbit once. The same result selects and drives this frame.
    static LLCachedControl<bool> orbit_enabled(gSavedSettings, "FlycamOrbitEnabled", false);
    static LLCachedControl<bool> orbit_level(gSavedSettings, "FlycamOrbitLevel", true);
    static LLCachedControl<F32>  orbit_min_r(gSavedSettings, "FlycamOrbitMinRadius", 0.1f);
    static LLCachedControl<F32>  orbit_max_r(gSavedSettings, "FlycamOrbitMaxRadius", 30.f);
    static LLCachedControl<F32>  orbit_smooth(gSavedSettings, "FlycamOrbitSmoothing", 0.15f);
    static LLCachedControl<F32>  orbit_zoom(gSavedSettings, "FlycamOrbitZoom", 1.f);
    static LLCachedControl<F32>  orbit_off_up(gSavedSettings, "FlycamOrbitOffsetUp", 0.f);
    static LLCachedControl<F32>  orbit_off_left(gSavedSettings, "FlycamOrbitOffsetLeft", 0.f);
    LLVector3 resolved_anchor_pos;
    LLQuaternion resolved_anchor_rot;
    const bool orbit_resolved = orbit_enabled &&
        LLCinematicCamera::instance().resolveAnchor(
            resolved_anchor_pos, resolved_anchor_rot, orbit_level);
    if (orbit_resolved)
    {
        resolved_anchor_pos += LLVector3(0.f, (F32)orbit_off_left,
                                        (F32)orbit_off_up) * resolved_anchor_rot;
    }

    const bool tagged_dispatch = gCameraDispatchOwner == ECameraOwner::FLYCAM;
    const bool dispatch_foreign = !sDrone.resetPending &&
        gCameraDispatchForeignSinceFlycamWrite;
    const bool drone_selected = (S32)flycamFlightModel() == 1 && !m3DCursor;
    const ALFlycamFrameDecision frame_decision = alResolveFlycamFrame(
        (S32)flycamFlightModel(), m3DCursor, orbit_resolved, sDrone.owner,
        tagged_dispatch, dispatch_foreign);
    const ALFlycamPoseOwner desired_owner = frame_decision.selectedOwner;
    const ALFlycamPoseOwner frame_owner = frame_decision.frameOwner;
    const bool owner_edge = frame_decision.edge != ALFlycamOwnerEdge::NONE;
    const bool drone_frame = frame_owner == ALFlycamPoseOwner::DRONE;
    const bool orbit_frame = frame_owner == ALFlycamPoseOwner::ORBIT;

    if (frame_decision.discardDroneAccumulator)
        sDrone.drone.discardAccumulator();

    // Configuration is Drone-owned work. Classic->Drone is configured at its
    // end-of-frame seed; Orbit->Drone is an immediate edge.
    if (drone_selected && drone_frame)
    {
        configureDrone(sDrone, mFlycamAxisScale[CAM_X_AXIS], mAutoLeveling,
                       in_build_mode, gFrameIntervalSeconds.value(), build_mode_scale);
    }

    if (tagged_dispatch && sDrone.resetPending)
    {
        sDrone.lastWrittenSerial = gCameraDispatchSerial - 1;
        gCameraDispatchForeignSinceFlycamWrite = false;
        sDrone.resetPending = false;
    }

    if (drone_frame || desired_owner == ALFlycamPoseOwner::DRONE)
    {
        const LLVector3d new_origin = gAgent.getPosGlobalFromAgent(LLVector3::zero);
        const LLVector3d old_origin = sDrone.drone.originTag();
        if (sDrone.drone.applyOrigin(new_origin))
        {
            const LLVector3d wide_delta = old_origin - new_origin;
            const LLVector3 delta((F32)wide_delta.mdV[VX],
                                  (F32)wide_delta.mdV[VY],
                                  (F32)wide_delta.mdV[VZ]);
            sFlycamPosition += delta;
            if (sDrone.haveLastDronePose)
                sDrone.lastDronePose.position += delta;
        }

        if (frame_decision.seedDrone &&
            frame_decision.edge == ALFlycamOwnerEdge::ORBIT_TO_DRONE &&
            !frame_decision.foreignDroneRegain)
        {
            if (sDrone.previousPose.valid && sDrone.currentPose.valid)
            {
                sDrone.drone.seedFromHistory(
                    sDrone.previousPose.positionGlobal, sDrone.previousPose.rotation,
                    sDrone.currentPose.positionGlobal, sDrone.currentPose.rotation,
                    sDrone.currentPose.dt, new_origin, &sDrone.drone.state());
            }
            else
            {
                sDrone.drone.seed(sFlycamPosition, sFlycamRotation,
                    LLVector3::zero, new_origin, &sDrone.drone.state());
            }
            sDrone.lastDronePose = sDrone.drone.renderPose();
            sDrone.haveLastDronePose = true;
            mXILevelPending = false;
            mXIRecentrePending = false;
            mDroneRecentreActive = false;
        }

        if (tagged_dispatch)
        {
            alCameraDispatchFinalizePresented();
            {
                const bool gap = gCameraDispatchSerial != sDrone.lastWrittenSerial + 1;
                const bool foreign = gCameraDispatchForeignSinceFlycamWrite;
                if ((gap && drone_frame) || foreign)
                {
                    const bool keep = (S32)droneSettings().resumeMomentum == 1;
                    std::string outcome = "stop";
                    if (!foreign)
                    {
                        if (sDrone.haveLastDronePose)
                        {
                            sFlycamPosition = sDrone.lastDronePose.position;
                            sFlycamRotation = sDrone.lastDronePose.rotation;
                        }
                        if (!keep)
                        {
                            sDrone.drone.seed(sFlycamPosition, sFlycamRotation,
                                LLVector3::zero, new_origin, &sDrone.drone.state());
                            mXIRecentrePending = false;
                            mDroneRecentreActive = false;
                        }
                        else
                        {
                            // SELF-Keep is an exact pause of the pose that was
                            // last presented: prev = cur and acc = 0.
                            sDrone.drone.pauseAtRenderPose();
                            sDrone.lastDronePose = sDrone.drone.renderPose();
                            sFlycamPosition = sDrone.lastDronePose.position;
                            sFlycamRotation = sDrone.lastDronePose.rotation;
                            outcome = llformat("keep v=%.3f",
                                sDrone.drone.state().velocity.length());
                        }
                        LL_INFOS("FlycamDrone") << "DRONE OWNER-REGAIN self momentum="
                            << outcome << LL_ENDL;
                    }
                    else
                    {
                        LLViewerCamera* camera = LLViewerCamera::getInstance();
                        sFlycamPosition = camera->getOrigin();
                        sFlycamRotation = camera->getQuaternion();
                        const LLVector3d current_global =
                            gAgent.getPosGlobalFromAgent(sFlycamPosition);
                        std::string reject;
                        LLVector3 v_keep;
                        if (keep)
                        {
                            if (gCameraDispatchSampleCount < 3)
                                reject = "stale";
                            else
                            {
                                const U32 n = gCameraDispatchSampleCount;
                                const U32 capacity = gCameraDispatchSamples.size();
                                const ALCameraDispatchSample& a = gCameraDispatchSamples[
                                    (gCameraDispatchSampleStart + n - 3) % capacity];
                                const ALCameraDispatchSample& b = gCameraDispatchSamples[
                                    (gCameraDispatchSampleStart + n - 2) % capacity];
                                const ALCameraDispatchSample& c = gCameraDispatchSamples[
                                    (gCameraDispatchSampleStart + n - 1) % capacity];
                                if (a.serial + 1 != b.serial || b.serial + 1 != c.serial ||
                                    c.serial + 1 != gCameraDispatchSerial)
                                    reject = "stale";
                                else if (a.owner != b.owner || b.owner != c.owner ||
                                         c.owner == ECameraOwner::FLYCAM)
                                    reject = "straddle";
                                else if (!a.presented || !b.presented || !c.presented)
                                    reject = "unpresented";
                                else if (!(a.dt > .0005f && a.dt <= .25f &&
                                           b.dt > .0005f && b.dt <= .25f &&
                                           c.dt > .0005f && c.dt <= .25f))
                                    reject = "dt";
                                else if (!a.positionGlobal.isFinite() ||
                                         !b.positionGlobal.isFinite() ||
                                         !c.positionGlobal.isFinite() ||
                                         !a.rotation.isFinite() || !b.rotation.isFinite() ||
                                         !c.rotation.isFinite())
                                    reject = "nonfinite";
                                else
                                {
                                    const LLVector3 va = LLVector3(b.positionGlobal - a.positionGlobal) / b.dt;
                                    const LLVector3 vb = LLVector3(c.positionGlobal - b.positionGlobal) / c.dt;
                                    const F32 mean_dt = (b.dt + c.dt) * .5f;
                                    const LLVector3 accel = (vb - va) / mean_dt;
                                    const LLVector3 omega_a = angularVelocity(a.rotation, b.rotation, b.dt);
                                    const LLVector3 omega_b = angularVelocity(b.rotation, c.rotation, c.dt);
                                    const LLVector3 angular_accel = (omega_b - omega_a) / mean_dt;
                                    if (!va.isFinite() || !vb.isFinite() || !accel.isFinite() ||
                                        !omega_a.isFinite() || !omega_b.isFinite() ||
                                        !angular_accel.isFinite())
                                        reject = "nonfinite";
                                    else if (vb.length() > sDrone.drone.params().velocityCap ||
                                             omega_b.length() > 2.f * sDrone.drone.params().yawRate)
                                        reject = "cut";
                                    else if (accel.length() > 1.5f * sDrone.drone.params().totalLimit ||
                                             angular_accel.length() > 2.f * sDrone.drone.params().yawAcceleration)
                                        reject = "inconsistent";
                                    else
                                        v_keep = vb + accel * (c.dt * .5f);

                                    if (reject.empty())
                                    {
                                        const LLVector3d synthetic_previous = current_global -
                                            LLVector3d(v_keep) * (F64)c.dt;
                                        sDrone.drone.seedFromHistory(synthetic_previous, b.rotation,
                                            current_global, c.rotation, c.dt, new_origin,
                                            &sDrone.drone.state());
                                        outcome = llformat("keep v=%.3f", v_keep.length());
                                    }
                                }
                            }
                        }
                        if (!keep || !reject.empty())
                        {
                            sDrone.drone.seed(sFlycamPosition, sFlycamRotation,
                                LLVector3::zero, new_origin, &sDrone.drone.state());
                            if (!reject.empty()) outcome = "keep-rejected:" + reject;
                        }
                        sDrone.lastDronePose = sDrone.drone.renderPose();
                        sDrone.haveLastDronePose = true;
                        mXILevelPending = false;
                        mXIRecentrePending = false;
                        mDroneRecentreActive = false;
                        LLCameraOperator::instance().reset();
                        LL_INFOS("FlycamDrone") << "DRONE OWNER-REGAIN foreign momentum="
                            << outcome << LL_ENDL;
                    }
                }
            }
        }
    }

    ALFlycamClassic::Input classic_input;
    for (U32 i = 0; i < ALFlycamClassic::AXIS_COUNT; ++i)
    {
        classic_input.axis[i] = getFlycamAxis(mJoystickAxis[i]);
        classic_input.axisDeadZone[i] = mFlycamAxisDeadZone[i];
        classic_input.axisScale[i] = mFlycamAxisScale[i];
    }
    classic_input.frameIntervalSeconds = gFrameIntervalSeconds.value();
    classic_input.flycamFeathering = mFlycamFeathering;
    classic_input.buildModeScale = build_mode_scale;
    classic_input.minView = LLViewerCamera::getInstance()->getMinView();
    classic_input.maxView = LLViewerCamera::getInstance()->getMaxView();
    classic_input.cameraAngle = gSavedSettings.getF32("CameraAngle");
    classic_input.triggerRoll = trigger_roll;
    classic_input.triggerThrottle = trigger_throttle;
    classic_input.zoomDefault = getJoystickButton(mMappedButtons[ZOOM_DEFAULT]);
    classic_input.zoomOut = getJoystickButton(mMappedButtons[ZOOM_OUT]);
    classic_input.zoomIn = getJoystickButton(mMappedButtons[ZOOM_IN]);
    classic_input.jump = getJoystickButton(mMappedButtons[JUMP]);
    classic_input.crouch = getJoystickButton(mMappedButtons[CROUCH]);
    classic_input.rollLeft = getJoystickButton(mMappedButtons[ROLL_LEFT]);
    classic_input.rollRight = getJoystickButton(mMappedButtons[ROLL_RIGHT]);
    classic_input.rollDefault = getJoystickButton(mMappedButtons[ROLL_DEFAULT]);
    classic_input.xiMode = mXIMode;
    classic_input.blackDragon = blackdragon;
    classic_input.cursor3D = m3DCursor;
    classic_input.xiTakeover = mXITakeover;
    classic_input.buildMode = in_build_mode;
    classic_input.invertPitch = mInvertPitch;
    classic_input.autoLeveling = mAutoLeveling;
    classic_input.zoomDirect = mZoomDirect;
    classic_input.levelHorizon = level_horizon;

    const LLVector3 shadow_entry_position = sFlycamPosition;
    const LLQuaternion shadow_entry_rotation = sFlycamRotation;
    ALFlycamClassic::State classic_state =
    {
        sLastDelta,
        sDelta,
        sFlycamPosition,
        sFlycamRotation,
        sFlycamZoom
    };
    const ALFlycamClassic::Intermediate classic_intermediate =
        ALFlycamClassic::integrateDeltas(classic_input, classic_state);

    // Clear AFK state if moved beyond the deadzone
    if (!classic_intermediate.isZero &&
        gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
    {
        gAgent.clearAFK();
    }

    ALFlycamClassic::finishIntegration(classic_input, classic_intermediate, classic_state);

    // Drone->Orbit engages from Drone's last rendered pose. Keep the Classic
    // deltas for Orbit controls, but discard the shadow pose increment.
    if (frame_decision.discardClassicShadowPose)
    {
        sFlycamPosition = shadow_entry_position;
        sFlycamRotation = shadow_entry_rotation;
    }

    // Classic is always advanced above.  Drone owns only the final pose and
    // overwrites that shadow result before Orbit and the common camera tail.
    if (drone_frame)
    {
        F32 raw[6];
        for (U32 i = 0; i < 6; ++i)
        {
            raw[i] = -getFlycamAxis(mJoystickAxis[i]);
            if (!blackdragon && i == CAM_Y_AXIS && !mInvertPitch)
                raw[i] = -raw[i];
            const F32 scale = mFlycamAxisScale[i];
            raw[i] *= scale > 0.f ? 1.f : scale < 0.f ? -1.f : 0.f;
        }

        LLFlycamDrone::Input input;
        F32 move_forward = raw[X_AXIS];
        F32 move_left = raw[Y_AXIS];
        shapeRadial(move_forward, move_left, droneSettings().moveDeadzone,
                    droneSettings().saturation, droneSettings().expoMove);
        input.move.set(move_forward, move_left, 0.f);

        F32 yaw = raw[CAM_Z_AXIS];
        F32 tilt = raw[CAM_Y_AXIS];
        shapeRadial(yaw, tilt, droneSettings().lookDeadzone,
                    droneSettings().saturation, 0.f);
        input.yaw = yaw;
        input.tilt = tilt;
        input.vertical = llclamp(raw[Z_AXIS] +
            (F32)getJoystickButton(mMappedButtons[JUMP]) -
            (F32)getJoystickButton(mMappedButtons[CROUCH]), -1.f, 1.f);
        input.roll = raw[CAM_X_AXIS] + trigger_roll;
        if (fabsf(input.roll) <= mFlycamAxisDeadZone[CAM_X_AXIS])
            input.roll = 0.f;

        const bool trigger_fresh = mXITakeover && xInputAssociated() &&
            ALXInput::fresh(mXISampleValid, gFocusMgr.getAppHasFocus(),
                            mXISampleGeneration, mXILoopGeneration);
        input.droneThrottle = trigger_fresh && mXIMode == 1;
        if (input.droneThrottle)
        {
            ALXInput::droneTriggers(mXILT / 255.f, mXIRT / 255.f,
                droneSettings().triggerDeadzone, droneSettings().triggerCurve,
                input.throttle, input.brake);
        }
        const F64 elapsed_dt = std::isfinite(gFrameIntervalSeconds.value())
            ? llmax((F64)gFrameIntervalSeconds.value(), 0.0) : 0.0;
        const F64 frame_dt = llmin(elapsed_dt, .25);
        if (mXIMode == 1 && gFocusMgr.getAppHasFocus())
        {
            if (trigger_fresh)
            {
                sDrone.triggerStaleSeconds = 0.0;
                sDrone.triggersStale = false;
            }
            else
            {
                sDrone.triggerStaleSeconds += elapsed_dt;
                if (sDrone.triggerStaleSeconds > 2.0 && !sDrone.triggersStale)
                {
                    LL_INFOS("FlycamDrone") << "DRONE TRIGGERS-STALE" << LL_ENDL;
                    sDrone.triggersStale = true;
                }
            }
        }
        else
        {
            sDrone.triggerStaleSeconds = 0.0;
            sDrone.triggersStale = false;
        }
        const F64 accumulator_before = sDrone.drone.accumulator();
        const F32 tick = sDrone.drone.params().tick;
        const U64 steps = tick > 0.f
            ? static_cast<U64>(llmin(512.0, floor((accumulator_before + frame_dt + 1.0e-9) / tick)))
            : 0;
        const bool executes_step = sDrone.drone.accumulator() + frame_dt + 1.0e-9 >=
            tick;
        input.level = mXILevelPending;
        input.recentre = mXIRecentrePending;

        const LLFlycamDrone::RenderPose pose =
            sDrone.drone.advance(gFrameIntervalSeconds.value(), input);

        // Sample the last real fixed step produced by advance(). Do not replay
        // step() for telemetry: that doubled model work on multi-step frames.
        if (steps > 0)
        {
            const LLFlycamDrone::State& measured = sDrone.drone.previousState();
            const LLFlycamDrone::State& next = sDrone.drone.state();
            const F32 acceleration = tick > 0.f
                ? next.deltaVelocity.length() / tick : 0.f;
            const F32 jerk = tick > 0.f
                ? (next.deltaVelocity - measured.deltaVelocity).length() / (tick * tick) : 0.f;
            sDrone.maxVelocity = llmax(sDrone.maxVelocity, next.velocity.length());
            const F32 acceleration_limit = next.totalLimitMax;
            const F32 jerk_limit = LLFlycamDrone::effectiveJerk(
                next.pole, next.totalLimitMax, tick);
            if (acceleration >= sDrone.maxAcceleration)
            {
                sDrone.maxAcceleration = acceleration;
                sDrone.accelerationLimitAtMax = acceleration_limit;
            }
            if (jerk >= sDrone.maxJerk)
            {
                sDrone.maxJerk = jerk;
                sDrone.jerkLimitAtMax = jerk_limit;
            }
            if (acceleration > acceleration_limit * 1.001f &&
                !sDrone.boundAccelerationSinceReport)
            {
                sDrone.boundAccelerationSinceReport = true;
            }
            if (jerk > jerk_limit * 1.001f && !sDrone.boundJerkSinceReport)
            {
                sDrone.boundJerkSinceReport = true;
            }
        }
        sDrone.reportSeconds += elapsed_dt;
        sDrone.reportSteps += steps;
        if (sDrone.drone.accumulator() > .1 && !sDrone.backlogSinceReport)
        {
            LL_INFOS("FlycamDrone") << "DRONE BACKLOG "
                << sDrone.drone.accumulator() << LL_ENDL;
            sDrone.backlogSinceReport = true;
        }
        if (sDrone.reportSeconds >= 5.0)
        {
            const char* verdict = sDrone.nonFiniteSinceReport ? "DRONE NONFINITE" :
                sDrone.boundAccelerationSinceReport ? "DRONE BOUND-EXCEEDED acc" :
                sDrone.boundJerkSinceReport ? "DRONE BOUND-EXCEEDED jerk" : "DRONE OK";
            LL_INFOS("FlycamDrone") << verdict
                << " preset=" << dronePresetName(droneSettings().preset)
                << " hz=" << (tick > 0.f ? 1.f / tick : 0.f)
                << " steps/s=" << (sDrone.reportSteps / sDrone.reportSeconds)
                << " backlog=" << sDrone.drone.accumulator()
                << " vmax=" << sDrone.maxVelocity
                << " acc_max_seen=" << sDrone.maxAcceleration
                << " (L_tot=" << sDrone.accelerationLimitAtMax << ')'
                << " jerk_max_seen=" << sDrone.maxJerk
                << " (J_eff=" << sDrone.jerkLimitAtMax << ')'
                << " clamps=" << sDrone.reportClamps << LL_ENDL;
            sDrone.reportSeconds = 0.0;
            sDrone.reportSteps = 0;
            sDrone.reportClamps = 0;
            sDrone.maxVelocity = 0.f;
            sDrone.maxAcceleration = 0.f;
            sDrone.maxJerk = 0.f;
            sDrone.accelerationLimitAtMax = 0.f;
            sDrone.jerkLimitAtMax = 0.f;
            sDrone.nonFiniteSinceReport = false;
            sDrone.boundAccelerationSinceReport = false;
            sDrone.boundJerkSinceReport = false;
            sDrone.backlogSinceReport = false;
        }
        if (executes_step)
        {
            if (input.level)
                mXILevelPending = false;
            if (input.recentre)
                mXIRecentrePending = false;
        }
        mDroneRecentreActive = sDrone.drone.state().recentre;
        sFlycamPosition = pose.position;
        sFlycamRotation = pose.rotation;
        sDrone.lastDronePose = pose;
        sDrone.haveLastDronePose = true;
    }
    else if (level_horizon)
    {
        // Classic and Orbit consume Level directly in their frame integrator.
        mXILevelPending = false;
    }

    static bool sLoggedCursorInactive = false;
    const bool cursor_blocks_drone = (S32)flycamFlightModel() == 1 && m3DCursor;
    if (cursor_blocks_drone && !sLoggedCursorInactive)
        LL_INFOS("FlycamDrone") << "DRONE INACTIVE 3d-cursor" << LL_ENDL;
    sLoggedCursorInactive = cursor_blocks_drone;

    // [machinima] Flycam Orbit: bone-locked stabilized orbit. When active, the
    // free-space pose integrated above is replaced by an orbit maintained
    // RELATIVE to the Cinematic Camera anchor (target avatar + CinematicCamJoint),
    // so the whole rig rides the subject as it walks/turns/animates. The
    // already-feathered sDelta values are consumed as orbit-space controls
    // (yaw->azimuth, pitch->elevation, forward->radius, left/up->focus pan);
    // zoom (sDelta[CAM_W_AXIS]) is untouched above. The result is written back
    // into sFlycamPosition/sFlycamRotation so the tail below (camera write,
    // handheld operator layer, flycam recorder) works unchanged, and free
    // flight resumes seamlessly from wherever the orbit leaves the camera.
    bool orbit_active = orbit_frame;
    F32 orbit_fov_mul = 1.f;    // FOV multiplier, applied only while orbiting
    if (orbit_active)
    {
        // lens: composed into the camera write below, never into
        // sFlycamZoom itself, so leaving orbit restores free-flight FOV
        orbit_fov_mul = llclamp((F32)orbit_zoom, 0.1f, 4.f);
        const F32 dt = llclamp(gFrameIntervalSeconds.value(), 0.0005f, 0.25f);
        const F32 min_r = llmax((F32)orbit_min_r, 0.01f);
        const F32 max_r = llmax((F32)orbit_max_r, min_r);

        // On the outgoing falling-edge frame, retain the last resolved anchor.
        if (orbit_resolved)
        {
            // temporal smoothing of the ANCHOR only (one-pole, dt-correct);
            // user input stays direct
            const F32 tau = llmax((F32)orbit_smooth, 0.f);
            if (!sOrbitWasActive || tau < 1e-3f)
            {
                sOrbitAnchorPos = resolved_anchor_pos;
                sOrbitAnchorRot = resolved_anchor_rot;
            }
            else
            {
                const F32 alpha = 1.f - expf(-dt / tau);
                sOrbitAnchorPos += (resolved_anchor_pos - sOrbitAnchorPos) * alpha;
                sOrbitAnchorRot = nlerp(alpha, sOrbitAnchorRot, resolved_anchor_rot);
            }
        }

            if (!sOrbitWasActive)
            {
                // engage without a position jump: recover the orbit state from
                // the camera's current pose relative to the anchor
                LLVector3 rel = (sFlycamPosition - sOrbitAnchorPos) * ~sOrbitAnchorRot;
                const F32 len = rel.magVec();
                sOrbitRadius = llclamp(len, min_r, max_r);
                if (len > 0.001f)
                {
                    sOrbitAzimuth = atan2f(rel.mV[VY], rel.mV[VX]);
                    sOrbitElevation = asinf(llclamp(rel.mV[VZ] / len, -1.f, 1.f));
                }
                else
                {
                    sOrbitAzimuth = 0.f;
                    sOrbitElevation = 0.f;
                }
                sOrbitElevation = llclamp(sOrbitElevation, -85.f * DEG_TO_RAD, 85.f * DEG_TO_RAD);
                sOrbitFocus.setZero();
                sOrbitRoll = 0.f;
            }

            // consume the feathered deltas as orbit controls
            sOrbitAzimuth += sDelta[CAM_Z_AXIS];
            if (sOrbitAzimuth > F_PI)       sOrbitAzimuth -= F_TWO_PI;
            else if (sOrbitAzimuth < -F_PI) sOrbitAzimuth += F_TWO_PI;
            sOrbitElevation = llclamp(sOrbitElevation + sDelta[CAM_Y_AXIS],
                                      -85.f * DEG_TO_RAD, 85.f * DEG_TO_RAD);
            sOrbitRadius = llclamp(sOrbitRadius - sDelta[X_AXIS], min_r, max_r);

            // camera offset from the focus point, in the anchor frame
            const F32 ce = cosf(sOrbitElevation);
            const LLVector3 sph(cosf(sOrbitAzimuth) * ce,
                                sinf(sOrbitAzimuth) * ce,
                                sinf(sOrbitElevation));
            const LLVector3 off_world = (sph * sOrbitRadius) * sOrbitAnchorRot;

            // look-at basis: X=at (toward focus), up referenced to the anchor
            // frame so the unleveled (GoPro) mode tumbles with the bone
            LLVector3 at = -off_world;
            at.normVec();
            LLVector3 up_ref = LLVector3(0.f, 0.f, 1.f) * sOrbitAnchorRot;
            LLVector3 left = up_ref % at;
            if (left.magVecSquared() < 1e-6f)   // looking straight along up_ref
            {
                left = LLVector3(0.f, 1.f, 0.f) * sOrbitAnchorRot;
            }
            left.normVec();
            LLVector3 up = at % left;
            LLMatrix3 basis;
            basis.setRows(at, left, up);
            LLQuaternion orbit_rot(basis);

            // pan the focus point in the camera's left/up plane (stored
            // anchor-local so it rides the subject)
            sOrbitFocus += (left * sDelta[Y_AXIS] + up * sDelta[Z_AXIS]) * ~sOrbitAnchorRot;

            // roll about the view axis: ignored while leveled, free otherwise
            if (!orbit_level)
            {
                sOrbitRoll += sDelta[CAM_X_AXIS];
                if (level_horizon) sOrbitRoll = 0.f;
                if (sOrbitRoll > F_PI)       sOrbitRoll -= F_TWO_PI;
                else if (sOrbitRoll < -F_PI) sOrbitRoll += F_TWO_PI;
                if (fabsf(sOrbitRoll) > 0.0001f)
                {
                    LLQuaternion roll_q;
                    roll_q.setEulerAngles(sOrbitRoll, 0.f, 0.f);
                    orbit_rot = roll_q * orbit_rot;
                }
            }
            else
            {
                sOrbitRoll = 0.f;
            }

            sFlycamPosition = sOrbitAnchorPos + (sOrbitFocus * sOrbitAnchorRot) + off_world;
            sFlycamRotation = orbit_rot;
    }
    sOrbitWasActive = orbit_active;

    // Record the final pre-operator pose before applying an ownership edge.
    sDrone.previousPose = sDrone.currentPose;
    sDrone.currentPose.positionGlobal = gAgent.getPosGlobalFromAgent(sFlycamPosition);
    sDrone.currentPose.rotation = sFlycamRotation;
    sDrone.currentPose.dt = gFrameIntervalSeconds.value();
    sDrone.currentPose.valid = sDrone.currentPose.positionGlobal.isFinite() &&
        sFlycamRotation.isFinite() && std::isfinite(sDrone.currentPose.dt);

    if (owner_edge)
    {
        const LLVector3d origin = gAgent.getPosGlobalFromAgent(LLVector3::zero);
        if (frame_decision.seedDrone &&
            frame_decision.edge == ALFlycamOwnerEdge::CLASSIC_TO_DRONE &&
            !frame_decision.foreignDroneRegain)
        {
            // The outgoing owner has completed its edge frame.  Only now may
            // Drone advance build scaling and install the parameters used by
            // the history seed.
            configureDrone(sDrone, mFlycamAxisScale[CAM_X_AXIS], mAutoLeveling,
                           in_build_mode, gFrameIntervalSeconds.value(),
                           build_mode_scale);
            if (sDrone.previousPose.valid && sDrone.currentPose.valid)
            {
                sDrone.drone.seedFromHistory(
                    sDrone.previousPose.positionGlobal, sDrone.previousPose.rotation,
                    sDrone.currentPose.positionGlobal, sDrone.currentPose.rotation,
                    sDrone.currentPose.dt, origin, &sDrone.drone.state());
            }
            else
            {
                sDrone.drone.seed(sFlycamPosition, sFlycamRotation,
                                  LLVector3::zero, origin, &sDrone.drone.state());
            }
            sDrone.lastDronePose = sDrone.drone.renderPose();
            sDrone.haveLastDronePose = true;
        }
        else if (frame_decision.edge == ALFlycamOwnerEdge::DRONE_TO_CLASSIC)
        {
            const std::array<F32, 6> seed =
                sDrone.drone.classicSeed(gFrameIntervalSeconds.value());
            for (U32 i = 0; i < seed.size(); ++i)
                sDelta[i] = seed[i];
        }
        sDrone.owner = desired_owner;
        if (desired_owner != ALFlycamPoseOwner::DRONE)
        {
            mXIRecentrePending = false;
            mDroneRecentreActive = false;
        }
    }

    const bool orbit_operator_edge =
        frame_decision.edge == ALFlycamOwnerEdge::DRONE_TO_ORBIT ||
        frame_decision.edge == ALFlycamOwnerEdge::ORBIT_TO_DRONE;
    if (frame_decision.resetOperator && orbit_operator_edge &&
        !frame_decision.foreignDroneRegain)
    {
        LLCameraOperator::instance().reset();
    }

    LLMatrix3 mat(sFlycamRotation);

    static LLCachedControl<bool> operator_enabled(
        gSavedSettings, "FlycamOperatorEnabled", false);
    static LLCachedControl<bool> apply_active(
        gSavedSettings, "CameraShakeApplyActive", false);
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 plain_fov = sFlycamZoom * orbit_fov_mul;
    // A resolved Flycam Orbit is the bone-mounted cinematic rider; the
    // operator flag is the other cinematic flycam driver. A free standalone
    // flycam has neither, so its FOV write remains exactly the original value.
    const bool cinematic_driven = orbit_active || operator_enabled;
    cam->setView(cinematic_driven
        ? LLCinematicCamera::applyFrameLens(plain_fov, cam)
        : plain_fov);

    // [phoenix port] Procedural handheld camera operator (native port of the
    // VirtualCinema Handheld ReShade design). Fed the flycam's own per-frame
    // deltas as ground-truth motion, applied only to the rendered camera this
    // frame -- sFlycamPosition/sFlycamRotation are never mutated, so nothing
    // drifts and disabling the effect snaps cleanly back. Re-grafted onto the
    // upstream 7.1.9.2516 flycam (agent-space LLVector3 position; dt taken
    // from gFrameIntervalSeconds because the upstream tail has no local time).
    if (operator_enabled || apply_active)
    {
        if (apply_active && !drone_frame)
        {
            static U32 sLastOperatorFrame = 0;
            if (sLastOperatorFrame == 0 || gFrameCount > sLastOperatorFrame + 1)
            {
                LLCameraOperator::instance().reset();
            }
            sLastOperatorFrame = gFrameCount;
        }
        const F32 op_dt = llmax(gFrameIntervalSeconds.value(), 0.0005f);
        LLCameraOperatorOutput op;
        if (drone_frame)
        {
            op = LLCameraOperator::instance().updateFromPose(
                op_dt, sFlycamPosition, sFlycamRotation);
        }
        else
        {
            const F32 inv_t = 1.f / op_dt;
            LLCameraOperatorInput opin;
            opin.mDeltaTime  = op_dt;
            // sDelta[0..2] is the camera-local translation applied this frame,
            // sDelta[3..5] the (roll, pitch, yaw) rotation applied this frame.
            opin.mLinearVel  = LLVector3(sDelta[VX], sDelta[VY], sDelta[VZ]) * inv_t;
            opin.mAngularVel = LLVector3(sDelta[3], sDelta[4], sDelta[5]) * inv_t;
            op = LLCameraOperator::instance().update(opin);
        }

        // orientation wobble pre-multiplied as a quaternion (same idiom as the
        // flycam's own rotation above) so the basis stays orthonormal
        LLMatrix3 wobble(op.mRoll, op.mPitch, op.mYaw);
        LLMatrix3 shaken(LLQuaternion(wobble) * sFlycamRotation);

        // gait/breath translation in camera-local space
        LLVector3 shaken_pos = sFlycamPosition
                             + LLVector3(shaken.mMatrix[0]) * op.mPosOffset.mV[VX]
                             + LLVector3(shaken.mMatrix[1]) * op.mPosOffset.mV[VY]
                             + LLVector3(shaken.mMatrix[2]) * op.mPosOffset.mV[VZ];

        cam->setView(LLCinematicCamera::applyFrameLens(
            plain_fov * op.mFovMul, cam));
        LLViewerCamera::getInstance()->setOrigin(shaken_pos);
        LLViewerCamera::getInstance()->mXAxis = LLVector3(shaken.mMatrix[0]);
        LLViewerCamera::getInstance()->mYAxis = LLVector3(shaken.mMatrix[1]);
        LLViewerCamera::getInstance()->mZAxis = LLVector3(shaken.mMatrix[2]);
    }
    else
    {
        LLViewerCamera::getInstance()->setOrigin(sFlycamPosition);
        LLViewerCamera::getInstance()->mXAxis = LLVector3(mat.mMatrix[0]);
        LLViewerCamera::getInstance()->mYAxis = LLVector3(mat.mMatrix[1]);
        LLViewerCamera::getInstance()->mZAxis = LLVector3(mat.mMatrix[2]);
    }

    if (tagged_dispatch && sDrone.lastWrittenSerial != gCameraDispatchSerial)
    {
        sDrone.lastWrittenSerial = gCameraDispatchSerial;
        gCameraDispatchFlycamWrote = true;
        gCameraDispatchForeignSinceFlycamWrite = false;
    }
    mDroneCameraOwner = tagged_dispatch && drone_frame && !orbit_frame;
    if (frame_decision.resetOperator && !orbit_operator_edge &&
        !frame_decision.foreignDroneRegain)
        LLCameraOperator::instance().reset();
}

// -----------------------------------------------------------------------------
bool LLViewerJoystick::toggleFlycam()
{
    mXILevelPending = false;
    mXIRecentrePending = false;
    mDroneCameraOwner = false;
    mDroneRecentreActive = false;
    if (!mJoystickEnabled || !mFlycamEnabled)
    {
        mOverrideCamera = false;
        updateXInputTakeover();
        return false;
    }

    if (!mOverrideCamera)
    {
        gAgentCamera.changeCameraToDefault();
    }

    if (gAwayTimer.getElapsedTimeF32() > LLAgent::MIN_AFK_TIME)
    {
        gAgent.clearAFK();
    }

    mOverrideCamera = !mOverrideCamera;
    updateXInputTakeover();
    if (mOverrideCamera)
    {
        // Taking manual camera control definitively ends any latched path-camera
        // preview so it can't sit above the flycam in the idle dispatch and
        // starve it (Flycam Orbit regression).
        LLPathCamera::instance().stopPreview();
        moveFlycam(true);

    }
    else
    {
        // Exiting from the flycam mode: since we are going to keep the flycam POV for
        // the main camera until the avatar moves, we need to track this situation.
        setCameraNeedsUpdate(false);
        setNeedsReset(true);
    }
    playRumble(mOverrideCamera ? ALXInput::RUMBLE_FLYCAM_ON : ALXInput::RUMBLE_FLYCAM_OFF);
    return true;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::pollForXboxController()
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (!gSavedSettings.getBOOL("JoystickAutoFlycamEnabled"))
    {
        // Fully reset the session state so re-enabling is a clean start.
        mXboxWasPresent = false;
        mXboxUserReleased = false;
        mXboxAutoGuid = LLSD();
        // Guarantee one fresh enumeration on re-enable even if no device
        // event fires in between.
        mDeviceListDirty = true;
        return;
    }

    // The user manually selected a different device after an automatic handoff:
    // respect that choice for the rest of the session (until auto-detect is
    // toggled off/on or the app restarts). Do not re-grab the Xbox or emit its
    // connect/disconnect toasts once the user has taken control.
    if (mXboxUserReleased)
    {
        return;
    }

    // EVENT-DRIVEN, not periodic: DI8 EnumDevices walks the HID/registry
    // stack on the main thread (tens of ms on some driver stacks — a visible
    // micro-stutter while filming), so it must never run on a timer. We
    // enumerate only when WM_DEVICECHANGE marked the device list dirty
    // (plug AND unplug both broadcast DBT_DEVNODES_CHANGED), plus once at
    // startup. The short timer below is only a DEBOUNCE: one arrival can
    // fire several WM_DEVICECHANGE messages, and the poll interval collapses
    // that burst into a single enumeration.
    if (!mDeviceListDirty)
    {
        return;
    }
    static LLFrameTimer poll_timer;
    if (poll_timer.getElapsedTimeF32() < XBOX_DEVICE_POLL_SECONDS)
    {
        return;
    }
    poll_timer.reset();
    mDeviceListDirty = false;

    if (!gViewerWindow || !gViewerWindow->getWindow())
    {
        return;
    }

    XboxDeviceProbe probe;
    std::function<bool(std::string&, LLSD&, void*)> osx_callback;
    void* win_callback = &di8_xbox_probe_callback;
    const bool enumerated = gViewerWindow->getWindow()->getInputDevices(
        DI8DEVCLASS_GAMECTRL, osx_callback, win_callback, &probe);

    if (!enumerated)
    {
        // A transient EnumDevices FAILURE is not a disconnect: releasing an
        // active pad and toasting on it would be wrong, and with event-driven
        // scans there may be no later device event to correct the mistake.
        // Preserve presence and re-arm so the next scan (debounced) retries.
        mDeviceListDirty = true;
        LL_WARNS("Joystick")
            << "DirectInput device enumeration failed; will retry."
            << LL_ENDL;
        return;
    }

    if (!probe.mFound)
    {
        if (mXboxWasPresent)
        {
            const bool active_device_was_xbox =
                isXboxProductName(getDescription());
            if (active_device_was_xbox)
            {
                if (mOverrideCamera)
                {
                    toggleFlycam();
                }
                mDriverState = JDS_UNINITIALIZED;
                mXIHelloOnReconnect = true; // [XInputRumble] real disconnect: hello on replug
                std::fill(std::begin(mAxes), std::end(mAxes), 0.f);
                memset(mBtn, 0, sizeof(mBtn));
                LL_INFOS("Joystick")
                    << "Xbox controller disconnected; Flycam released."
                    << LL_ENDL;
                // Only announce the disconnect for a controller we were actually
                // driving — not one the user had already switched away from.
                LLNotificationsUtil::add("XboxControllerDisconnected");
            }
        }
        mXboxWasPresent = false;
        mXboxAutoGuid = LLSD();
        return;
    }

    // Presence is edge-triggered. Once the automatic handoff has happened,
    // manual Flycam/device choices remain authoritative until a reconnect.
    if (mXboxWasPresent)
    {
        // If the user has since selected a different active device, latch that
        // as a manual override and stop auto-managing the Xbox for this session.
        if (mXboxAutoGuid.isDefined() &&
            !sameWindowsDeviceId(mLastDeviceUUID, mXboxAutoGuid))
        {
            mXboxUserReleased = true;
        }
        return;
    }

    // Ensure libndof and its reusable device object exist. init() can choose a
    // different saved/first controller, so explicitly select the Xbox GUID
    // afterward when necessary.
    if (!mNdofDev)
    {
        init(false, false);
    }
    if (mDriverState != JDS_INITIALIZED ||
        !sameWindowsDeviceId(mLastDeviceUUID, probe.mGuid))
    {
        LLSD guid = probe.mGuid;
        initDevice(guid);
    }

    if (mDriverState != JDS_INITIALIZED ||
        !sameWindowsDeviceId(mLastDeviceUUID, probe.mGuid))
    {
        mXboxWasPresent = false;
        // Event-driven scans need an explicit re-arm to retry a failed init
        // (the debounce timer paces the retries to one per poll interval).
        mDeviceListDirty = true;
        LL_WARNS("Joystick")
            << "Detected Xbox controller '" << probe.mProductName
            << "' but could not initialize it; the next scan will retry."
            << LL_ENDL;
        return;
    }

    // Hot-plug selection must not touch mappings, scales, or dead zones. Use
    // the controller configuration already persisted by the viewer.
    gSavedSettings.setBOOL("JoystickFlycamEnabled", true);
    gSavedSettings.setBOOL("JoystickEnabled", true);
    gSavedSettings.setString("JoystickInitialized", "XboxController");
    saveDeviceIdToSettings();
    refreshFromSettings();

    mXboxWasPresent = true;
    mXboxAutoGuid = probe.mGuid;   // remember what we handed off to, to detect a later manual override
    LL_INFOS("Joystick")
        << "Xbox controller '" << probe.mProductName
        << "' connected and assigned to Flycam."
        << LL_ENDL;
    LLSD args;
    args["DEVICE"] = probe.mProductName;
    LLNotificationsUtil::add("XboxControllerConnected", args);
#endif
}

// -----------------------------------------------------------------------------
namespace
{
struct XInputSettings
{
    LLCachedControl<bool> enabled{gSavedSettings, "JoystickXInputEnabled", true};
    LLCachedControl<S32> mode{gSavedSettings, "JoystickXInputTriggerMode", 0};
    LLCachedControl<bool> shaped{gSavedSettings, "JoystickXInputRollShaped", false};
    LLCachedControl<F32> minimum{gSavedSettings, "JoystickXInputThrottleMin", .1f};
    LLCachedControl<F32> maximum{gSavedSettings, "JoystickXInputThrottleMax", 4.f};
    LLCachedControl<F32> deadzone{gSavedSettings, "JoystickXInputTriggerDeadZone", .06f};
    LLCachedControl<F32> curve{gSavedSettings, "JoystickXInputTriggerCurve", 2.f};
    LLCachedControl<S32> left{gSavedSettings, "JoystickXInputDpadLeft", 0};
    LLCachedControl<S32> right{gSavedSettings, "JoystickXInputDpadRight", 0};
    LLCachedControl<S32> up{gSavedSettings, "JoystickXInputDpadUp", 0};
    LLCachedControl<bool> rumble{gSavedSettings, "JoystickXInputRumble", true};
    LLCachedControl<F32> rumbleStrength{gSavedSettings, "JoystickXInputRumbleStrength", 1.f};
};

const XInputSettings& xInputSettings()
{
    static XInputSettings settings;
    return settings;
}
}

void LLViewerJoystick::setDeviceListDirty()
{
    mDeviceListDirty = true;
    invalidateXInput(true); // invalidate NOW; only discovery is debounced
}

void LLViewerJoystick::invalidateXInput(bool rescan)
{
    stopRumble(); // [XInputRumble] BEFORE the slot is cleared; uses mXIRumbleSlot
    mXIAssociated = false;
    mXIGuid = LLSD();
    mXISlot = -1;
    mXISampleValid = false;
    mXILT = mXIRT = 0;
    mXIEdges = mXIPrevRaw = 0;
    mXIPrime = true;
    mXIWasFocused = false;
    if (!mDroneCameraOwner)
    {
        mXILevelPending = false;
        mXIRecentrePending = false;
    }
    mXIRetry = rescan ? 0 : 4;
    mXIRetryTimer.reset();
    mXIStatus = rescan ? "waiting" : "off";
    if (mXITakeover)
    {
        // [XInputPad] Arm before seeding, including WM_DEVICECHANGE invalidation.
        mXIReleasePending = true;
        mXIReleaseGuid = mLastDeviceUUID;
        mXITakeover = false;
        seedTriggerHistory();
    }
}

bool LLViewerJoystick::xInputAssociated() const
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    return xInputSettings().enabled && sXInputGetState && mXIAssociated &&
        mDriverState == JDS_INITIALIZED && mJoystickEnabled &&
        mXIDIGeneration == mXIScanGeneration && mXISlotGeneration == mXIScanGeneration &&
        sameWindowsDeviceId(mLastDeviceUUID, mXIGuid);
#else
    return false;
#endif
}

// [XInputRumble] Start (replacing any in-progress) pattern; inert unless fully eligible.
void LLViewerJoystick::playRumble(S32 pattern)
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    const auto& settings = xInputSettings();
    if (!settings.rumble || !(settings.rumbleStrength > 0.f) || !sXInputSetState ||
        !gFocusMgr.getAppHasFocus() || !xInputAssociated() || mXISlot < 0 ||
        !ALXInput::rumblePattern(pattern)) return;
    mXIRumblePattern = pattern;
    mXIRumbleTimer.reset();
    updateXInputRumble(); // apply step 0 now; later steps advance in beginMainLoop
#endif
}

// [XInputRumble] Zero the last-driven slot. No-op when nothing is playing and both motors are 0.
// A failed zero-write keeps the cached motors non-zero so updateXInputRumble retries it
// (backed off 250 ms); force bypasses the backoff for terminate/quit.
void LLViewerJoystick::stopRumble(bool force)
{
    mXIRumblePattern = -1;
    if (!mXIRumbleLow && !mXIRumbleHigh) return;
#if LL_WINDOWS && !LL_MESA_HEADLESS
    if (sXInputSetState && mXIRumbleSlot >= 0)
    {
        const F64 now = LLTimer::getTotalSeconds();
        if (!force && now < mXIRumbleRetryAt) return;
        XINPUT_VIBRATION off = {};
        const DWORD result = sXInputSetState(static_cast<DWORD>(mXIRumbleSlot), &off);
        if (result != ERROR_SUCCESS && result != ERROR_DEVICE_NOT_CONNECTED)
        {
            mXIRumbleRetryAt = now + 0.25;
            return;
        }
    }
#endif
    mXIRumbleRetryAt = 0.0;
    mXIRumbleLow = mXIRumbleHigh = 0;
}

// [XInputRumble] Send only on change; cache only what the pad accepted.
void LLViewerJoystick::setRumbleMotors(U16 low, U16 high)
{
#if LL_WINDOWS && !LL_MESA_HEADLESS
    if (low == mXIRumbleLow && high == mXIRumbleHigh) return;
    if (!sXInputSetState || mXISlot < 0) return;
    XINPUT_VIBRATION vibration = {};
    vibration.wLeftMotorSpeed = low;
    vibration.wRightMotorSpeed = high;
    mXIRumbleSlot = mXISlot;
    const DWORD result = sXInputSetState(static_cast<DWORD>(mXIRumbleSlot), &vibration);
    if (result == ERROR_SUCCESS)
    {
        mXIRumbleLow = low;
        mXIRumbleHigh = high;
    }
    else if (result == ERROR_DEVICE_NOT_CONNECTED)
    {
        mXIRumblePattern = -1;
        mXIRumbleLow = mXIRumbleHigh = 0; // device gone: nothing left to zero
    }
    else
    {
        // Unknown motor state: mark dirty so the next pass re-sends and any stop sends zero.
        mXIRumbleLow = mXIRumbleHigh = 0xFFFF;
    }
#endif
}

void LLViewerJoystick::updateXInputRumble()
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (mXIRumblePattern < 0 && !mXIRumbleLow && !mXIRumbleHigh) return;
    const auto& settings = xInputSettings();
    if (!settings.rumble || !(settings.rumbleStrength > 0.f) || !sXInputSetState ||
        !gFocusMgr.getAppHasFocus() || !xInputAssociated() || mXIRumblePattern < 0)
    {
        stopRumble(); // setting off / strength 0 / focus lost / pad gone / stray motors
        return;
    }
    float low = 0.f;
    float high = 0.f;
    if (!ALXInput::rumbleLevels(ALXInput::rumblePattern(mXIRumblePattern),
                                mXIRumbleTimer.getElapsedTimeF32() * 1000.f, low, high))
    {
        stopRumble();
        return;
    }
    const F32 strength = settings.rumbleStrength;
    setRumbleMotors(ALXInput::rumbleMotor(low, strength), ALXInput::rumbleMotor(high, strength));
#endif
}

void LLViewerJoystick::beginMainLoop()
{
    ++mXILoopGeneration;
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    const bool enabled = xInputSettings().enabled;
    if (enabled != mXIMaster)
    {
        mXIMaster = enabled;
        invalidateXInput(enabled);
    }
    if (mXIObservedGuid.isBinary() != mLastDeviceUUID.isBinary() ||
        (mLastDeviceUUID.isBinary() && !sameWindowsDeviceId(mXIObservedGuid, mLastDeviceUUID)))
    {
        mXIObservedGuid = mLastDeviceUUID;
        invalidateXInput(enabled);
    }
    if (mXIAssociated && !xInputAssociated()) invalidateXInput(enabled);
#endif
    // Also notice focus loss on passes where scanJoystick is gated out.
    if (!gFocusMgr.getAppHasFocus()) mXIWasFocused = false;
    const bool flycam_dispatch_owns_camera = getOverrideCamera() &&
        gCameraDispatchOwner == ECameraOwner::FLYCAM;
    if (mXILevelPending && (!flycam_dispatch_owns_camera ||
        mXILoopGeneration - mXILevelGeneration > 1))
    {
        mXILevelPending = false;
    }
    updateXInputTakeover();
    updateXInputRumble(); // [XInputRumble] advance pattern; also stops on focus loss / setting off
}

void LLViewerJoystick::scanXInputAssociation()
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (!xInputSettings().enabled || mXIAssociated || mXIRetry >= 4) return;
    static const F32 delays[] = { .5f, 1.f, 2.f, 4.f };
    if (mXIRetryTimer.getElapsedTimeF32() < delays[mXIRetry]) return;
    mXIRetryTimer.reset(); // [XInputPad] Each attempt gets its own debounce interval.
    ++mXIRetry;
    if (!loadXInput())
    {
        mXIRetry = 4;
        mXIStatus = "XInput unavailable";
        return;
    }
    ++mXIScanGeneration;
    XInputDeviceProbe probe;
    bool enumerated = false;
    if (gViewerWindow && gViewerWindow->getWindow())
    {
        auto* window = gViewerWindow->getWindow();
        void* direct_input = window->getDirectInput8();
        if (direct_input) probe.di = *static_cast<LPDIRECTINPUT8*>(direct_input);
        if (probe.di)
        {
            std::function<bool(std::string&, LLSD&, void*)> osx_callback;
            enumerated = window->getInputDevices(DI8DEVCLASS_GAMECTRL, osx_callback,
                                                 &di8_xinput_probe_callback, &probe);
        }
    }
    mXIDIGeneration = mXIScanGeneration;
    S32 slots = 0;
    S32 slot = -1;
    bool slot_failed = false;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        XINPUT_STATE state = {};
        const DWORD result = sXInputGetState(i, &state);
        if (result == ERROR_SUCCESS) { ++slots; slot = static_cast<S32>(i); }
        else if (result != ERROR_DEVICE_NOT_CONNECTED) slot_failed = true;
    }
    mXISlotGeneration = mXIScanGeneration;
    // [XInputRumble] A clean scan with no XInput pad is confirmed absence: hello on the next
    // association (setDeviceListDirty can invalidate before pollXInputState sees the unplug).
    if (!slot_failed && slots == 0) mXIHelloOnReconnect = true;
    if (!enumerated || probe.failed || slot_failed) mXIStatus = "scan failed";
    else if (slots == 0 && probe.count == 0)
    {
        mXIStatus = "none";
        mXIRetry = 4; // [XInputPad] Successful empty scan: wait for a device event.
    }
    else if (slots > 1 || probe.count > 1) mXIStatus = llformat("ambiguous (%d XInput)", slots);
    else if (slots != 1 || probe.count != 1) mXIStatus = "none";
    else if (!sameWindowsDeviceId(mLastDeviceUUID, probe.guid)) mXIStatus = "active device mismatch";
    else if (mDriverState != JDS_INITIALIZED || !mJoystickEnabled) mXIStatus = "joystick disabled";
    else
    {
        mXIAssociated = true;
        mXIGuid = probe.guid;
        mXISlot = slot;
        mXIPrime = true;
        mXIRetry = 4;
        mXIStatus = "ready";
        // [XInputRumble] Hello only for a genuine (re)connection, not spurious re-association.
        const bool hello = !mXIHelloGuid.isDefined() || mXIHelloOnReconnect ||
            !sameWindowsDeviceId(mXIHelloGuid, probe.guid);
        mXIHelloOnReconnect = false;
        if (hello)
        {
            mXIHelloGuid = probe.guid;
            playRumble(ALXInput::RUMBLE_CONNECT);
        }
    }
    LL_INFOS("XInput") << "[XInputPad] scan " << mXIScanGeneration
        << " IG=" << probe.count << " slots=" << slots << " verdict="
        << (mXIAssociated ? "PASS: " : "INCONCLUSIVE: ") << mXIStatus << LL_ENDL;
#endif
}

void LLViewerJoystick::pollXInputState()
{
    mXIEdges = 0;
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (!xInputAssociated()) return;
    XINPUT_STATE state = {};
    const DWORD result = sXInputGetState(static_cast<DWORD>(mXISlot), &state);
    if (result != ERROR_SUCCESS)
    {
        if (result == ERROR_DEVICE_NOT_CONNECTED)
        {
            // [XInputPad] xInputAssociated() above proves this is the active DI pad.
            mXIDisconnectedGuid = mXIGuid;
            mXIHelloOnReconnect = true; // [XInputRumble] real disconnect: hello on next association
            std::fill(std::begin(mAxes), std::end(mAxes), 0.f);
        }
        invalidateXInput(true);
        return;
    }
    mXIDisconnectedGuid = LLSD();
    const bool focused = gFocusMgr.getAppHasFocus();
    const U16 raw = state.Gamepad.wButtons;
    const bool prime = ALXInput::primeSample(mXIPrime, focused, mXIWasFocused,
                                            mXILoopGeneration, mXILastScanGeneration);
    mXIEdges = ALXInput::edges(raw, mXIPrevRaw, prime, focused);
    mXIPrevRaw = raw; // raw history is never overwritten by focus policy
    mXIPrime = false;
    mXIWasFocused = focused;
    mXILastScanGeneration = mXILoopGeneration;
    mXISampleGeneration = mXILoopGeneration;
    mXISampleValid = true;
    mXILT = state.Gamepad.bLeftTrigger;
    mXIRT = state.Gamepad.bRightTrigger;
#endif
}

void LLViewerJoystick::seedTriggerHistory()
{
    static LLCachedControl<bool> blackdragon(gSavedSettings, "BlackDragonControls", false);
    for (S32 i = 0; i < MAX_AXES; ++i)
    {
        // Departing ownership must cover the old mapping as well as the new one.
        if (mJoystickAxis[i] == 2 || mXIPreviousAxes[i] == 2)
        {
            sLastDelta[i] = -getFlycamAxis(mJoystickAxis[i]);
            if (!blackdragon && !mInvertPitch && i == CAM_Y_AXIS)
                sLastDelta[i] = -sLastDelta[i];
        }
    }
    sDelta[CAM_X_AXIS] = 0.f;
}

void LLViewerJoystick::updateXInputTakeover(bool reset)
{
    const auto& settings = xInputSettings();
    const S32 mode = settings.mode == 1 ? 1 : 0;
    const bool shaped = settings.shaped;
    const bool compatible = ALXInput::compatible(mJoystickAxis, m3DCursor);
    bool release_cleared = false;
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (mXIReleasePending && (!settings.enabled || !compatible ||
        !sameWindowsDeviceId(mXIReleaseGuid, mLastDeviceUUID) ||
        llabs(getJoystickAxis(2)) <= mFlycamAxisDeadZone[CAM_X_AXIS]))
    {
        mXIReleasePending = false;
        mXIReleaseGuid = LLSD();
        release_cleared = true;
    }
#endif
    const bool takeover = xInputAssociated() &&
        compatible &&
        mOverrideCamera && mFlycamEnabled && (mode == 1 || shaped);
    const bool changed = takeover != mXITakeover ||
        (takeover && (mode != mXIMode || shaped != mXIShaped));
    const bool had_takeover = mXITakeover;
    mXITakeover = takeover;
    // Legacy resets/mode toggles are intentionally untouched.
    if (release_cleared || (changed && (had_takeover || takeover)) || (reset && takeover))
        seedTriggerHistory();
    mXIMode = mode;
    mXIShaped = shaped;
    std::copy(std::begin(mJoystickAxis), std::end(mJoystickAxis), mXIPreviousAxes.begin());
}

F32 LLViewerJoystick::getFlycamAxis(U32 axis) const
{
    return (mXITakeover || mXIReleasePending) && axis == 2 ? 0.f : getJoystickAxis(axis);
}

void LLViewerJoystick::computeTriggerOutputs(F32& roll, F32& throttle) const
{
    roll = 0.f;
    throttle = 1.f;
    if (!mXITakeover || !xInputAssociated() ||
        !ALXInput::fresh(mXISampleValid, gFocusMgr.getAppHasFocus(),
                        mXISampleGeneration, mXILoopGeneration)) return;
    const auto& settings = xInputSettings();
    if (mXIMode == 1)
    {
        const F32 left = ALXInput::shape(mXILT / 255.f, settings.deadzone, settings.curve);
        const F32 right = ALXInput::shape(mXIRT / 255.f, settings.deadzone, settings.curve);
        throttle = ALXInput::throttle(left, right, settings.minimum, settings.maximum);
    }
    else roll = ALXInput::roll(mXILT / 255.f, mXIRT / 255.f, settings.deadzone, settings.curve);
}

void LLViewerJoystick::xInputHint(const std::string& message)
{
    if (mXIHint) LLNotificationsUtil::cancel(mXIHint);
    LLSD args;
    args["MESSAGE"] = message;
    mXIHint = LLNotificationsUtil::add("XInputPadHint", args);
}

void LLViewerJoystick::handleXInputDpad()
{
#if LIB_NDOF && LL_WINDOWS && !LL_MESA_HEADLESS
    if (!xInputAssociated() ||
        !ALXInput::fresh(mXISampleValid, gFocusMgr.getAppHasFocus(),
                        mXISampleGeneration, mXILoopGeneration)) return;
    const U16 edges = mXIEdges;
    mXIEdges = 0;
    const auto& settings = xInputSettings();
    if (edges & XINPUT_GAMEPAD_DPAD_DOWN)
    {
        const S32 mode = settings.mode == 1 ? 0 : 1;
        gSavedSettings.setS32("JoystickXInputTriggerMode", mode);
        const bool drone = gSavedSettings.getS32("FlycamFlightModel") == 1 &&
            getOverrideCamera() && !m3DCursor;
        xInputHint(drone
            ? (mode == 1 ? "Triggers: THROTTLE + BRAKE (Drone)" : "Triggers: ROLL (Drone)")
            : (mode == 1 ? "Triggers: THROTTLE" : "Triggers: ROLL"));
        playRumble(ALXInput::RUMBLE_MODE_TICK);
    }
    const auto action = [this](S32 value)
    {
        if (value == ALXInput::DPAD_FLIGHT_MODEL)
        {
            const bool drone = gSavedSettings.getS32("FlycamFlightModel") == 1;
            if (!drone && m3DCursor)
            {
                xInputHint("Drone unavailable: 3D cursor");
                return;
            }
            gSavedSettings.setS32("FlycamFlightModel", drone ? 0 : 1);
            xInputHint(drone ? "Flight: Classic" : "Flight: Drone");
            playRumble(ALXInput::RUMBLE_MODE_TICK);
        }
        else if (value == ALXInput::DPAD_RELEASE)
        {
            const bool drone_owns_camera = getOverrideCamera() &&
                gCameraDispatchOwner == ECameraOwner::FLYCAM && mDroneCameraOwner;
            if (!drone_owns_camera)
            {
                xInputHint("Drone not active");
                return;
            }
            const S32 release = gSavedSettings.getS32("FlycamDroneRelease") == 1 ? 0 : 1;
            gSavedSettings.setS32("FlycamDroneRelease", release);
            markDronePresetCustom();
            xInputHint(release == 1 ? "Release: Drift" : "Release: Hold");
        }
        else if (value == ALXInput::DPAD_RECENTRE)
        {
            const bool drone_owns_camera = getOverrideCamera() &&
                gCameraDispatchOwner == ECameraOwner::FLYCAM && mDroneCameraOwner;
            if (!drone_owns_camera)
            {
                xInputHint("Drone not active");
                return;
            }
            if (mXIRecentrePending || mDroneRecentreActive)
                return;
            mXIRecentrePending = true;
            mXIRecentreGeneration = mXILoopGeneration;
            xInputHint("Gimbal recentring");
        }
        else if (value == ALXInput::DPAD_LEVEL)
        {
            const bool flycam_owns_camera = getOverrideCamera() &&
                gCameraDispatchOwner == ECameraOwner::FLYCAM;
            if (!flycam_owns_camera) { xInputHint("Flycam not active"); return; }
            mXILevelPending = true;
            mXILevelGeneration = mXILoopGeneration;
            xInputHint("Level horizon");
        }
        else if (value == ALXInput::DPAD_CUE_GO || value == ALXInput::DPAD_CUE_BACK)
        {
            auto& rig = ALCineLightRigManager::instance().selected();
            if (rig.cueList().mTimecodeMode) { xInputHint("Timecode mode"); return; }
            if (rig.cueList().mCues.empty()) { xInputHint("No light cues"); return; }
            const F64 time = llmax(LLPresentationTime::currentFrame().presentation_time, 0.0);
            if (value == ALXInput::DPAD_CUE_GO) rig.cueGo(time); else rig.cueBack(time);
            xInputHint(llformat("Light cue %d/%d", rig.activeCue() + 1,
                               static_cast<S32>(rig.cueList().mCues.size())));
        }
    };
    if (edges & XINPUT_GAMEPAD_DPAD_LEFT) action(settings.left);
    if (edges & XINPUT_GAMEPAD_DPAD_RIGHT) action(settings.right);
    if (edges & XINPUT_GAMEPAD_DPAD_UP) action(settings.up);
    updateXInputTakeover();
#endif
}

std::string LLViewerJoystick::getXInputStatus() const
{
    const auto& settings = xInputSettings();
    const bool drone = mOverrideCamera && gCameraDispatchOwner == ECameraOwner::FLYCAM &&
        mDroneCameraOwner;
    const char* mode = !xInputAssociated() ? "inactive"
        : m3DCursor ? "legacy: 3D cursor"
        : !ALXInput::compatible(mJoystickAxis, false) ? "legacy: mapping"
        : drone ? (settings.mode == 1 ? "THROTTLE + BRAKE (Drone)" : "ROLL (Drone)")
        : settings.mode == 1 ? "THROTTLE" : settings.shaped ? "ROLL shaped" : "ROLL legacy";
    const bool fresh = xInputAssociated() &&
        ALXInput::fresh(mXISampleValid, gFocusMgr.getAppHasFocus(),
                        mXISampleGeneration, mXILoopGeneration);
    return "Pad: " + (settings.enabled ? mXIStatus : std::string("off")) +
        llformat(" - LT %.2f RT %.2f", fresh ? mXILT / 255.f : 0.f,
                 fresh ? mXIRT / 255.f : 0.f) + "\nTriggers: " + mode;
}

void LLViewerJoystick::scanJoystick()
{
    pollForXboxController();
    scanXInputAssociation();

    if (mDriverState != JDS_INITIALIZED || !mJoystickEnabled)
    {
        return;
    }

    pollXInputState();

#if LL_WINDOWS
    // On windows, the flycam is updated syncronously with a timer, so there is
    // no need to update the status of the joystick here.
    if (!mOverrideCamera)
#endif
    updateStatus();

    // App focus check Needs to happen AFTER updateStatus in case the joystick
    // is not centred when the app loses focus.
    if (!gFocusMgr.getAppHasFocus())
    {
        return;
    }

    handleXInputDpad();

    static long toggle_flycam = 0;

    static LLCachedControl<bool> blackdragon(gSavedSettings, "BlackDragonControls", false);
    if (blackdragon)
    {
        if (getJoystickButton(mMappedButtons[FLYCAM]) == 1)
        {
            if (getJoystickButton(mMappedButtons[FLYCAM]) != toggle_flycam)
            {
                toggle_flycam = toggleFlycam() ? 1 : 0;
            }
        }
        else
        {
            toggle_flycam = 0;
        }
    }
    else
    {
        if (mBtn[0] == 1)
        {
            if (mBtn[0] != toggle_flycam)
            {
                toggle_flycam = toggleFlycam() ? 1 : 0;
            }
        }
        else
        {
            toggle_flycam = 0;
        }
    }

    if (!mOverrideCamera && !(LLToolMgr::getInstance()->inBuildMode() && mBuildEnabled))
    {
        moveAvatar();
    }
}

// -----------------------------------------------------------------------------
bool LLViewerJoystick::isDeviceUUIDSet()
{
#if LL_WINDOWS && !LL_MESA_HEADLESS
    // for ease of comparison and to dial less with platform specific variables, we store id as LLSD binary
    return mLastDeviceUUID.isBinary();
#elif LL_DARWIN
    return mLastDeviceUUID.isMap();
#else
    return false;
#endif
}

LLSD LLViewerJoystick::getDeviceUUID()
{
    return mLastDeviceUUID;
}

std::string LLViewerJoystick::getDeviceUUIDString()
{
#if LL_WINDOWS && !LL_MESA_HEADLESS
    // Might be simpler to just convert _GUID into string everywhere, store and compare as string
    if (mLastDeviceUUID.isBinary())
    {
        S32 size = sizeof(GUID);
        LLSD::Binary data = mLastDeviceUUID.asBinary();
        GUID guid;
        memcpy(&guid, &data[0], size);
        return string_from_guid(guid);
    }
    else
    {
        return std::string();
    }
#elif LL_DARWIN
    if (mLastDeviceUUID.isMap())
    {
        std::string manufacturer = mLastDeviceUUID["manufacturer"].asString();
        std::string product = mLastDeviceUUID["product"].asString();
        return manufacturer + ":" + product;
    }
    else
    {
        return std::string();
    }
#else
    return std::string();
#endif
}

void LLViewerJoystick::saveDeviceIdToSettings()
{
#if LL_WINDOWS && !LL_MESA_HEADLESS
    // can't save as binary directly,
    // someone editing the xml will corrupt it
    // so convert to string first
    std::string device_string = getDeviceUUIDString();
    gSavedSettings.setLLSD("JoystickDeviceUUID", LLSD(device_string));
#else
    LLSD device_id = getDeviceUUID();
    gSavedSettings.setLLSD("JoystickDeviceUUID", device_id);
#endif
}

void LLViewerJoystick::loadDeviceIdFromSettings()
{
    LLSD dev_id = gSavedSettings.getLLSD("JoystickDeviceUUID");
#if LL_WINDOWS && !LL_MESA_HEADLESS
    // We can't save binary data to gSavedSettings, somebody editing the file will corrupt it,
    // so _GUID data gets converted to string (we probably can convert it to LLUUID with memcpy)
    // and here we need to convert it back to binary from string
    std::string device_string;
    if (dev_id.isString())
    {
        device_string = dev_id.asString();
    }
    if (device_string.empty())
    {
        mLastDeviceUUID = LLSD();
    }
    else
    {
        LL_DEBUGS("Joystick") << "Looking for device by id: " << device_string << LL_ENDL;
        GUID guid;
        guid_from_string(guid, device_string);
        S32 size = sizeof(GUID);
        LLSD::Binary data; //just an std::vector
        data.resize(size);
        memcpy(&data[0], &guid /*POD _GUID*/, size);
        // We store this data in LLSD since it can handle both GUID2 and long
        mLastDeviceUUID = LLSD(data);
    }
#elif LL_DARWIN
    if (!dev_id.isMap())
    {
        mLastDeviceUUID = LLSD();
    }
    else
    {
        std::string manufacturer = mLastDeviceUUID["manufacturer"].asString();
        std::string product = mLastDeviceUUID["product"].asString();
        LL_DEBUGS("Joystick") << "Looking for device by manufacturer: " << manufacturer << " and product: " << product <<  LL_ENDL;
        // We store this data in LLSD since it can handle both GUID2 and long
        mLastDeviceUUID = dev_id;
    }
#else
    mLastDeviceUUID = LLSD();
    //mLastDeviceUUID = gSavedSettings.getLLSD("JoystickDeviceUUID");
#endif
}

// -----------------------------------------------------------------------------
std::string LLViewerJoystick::getDescription()
{
    std::string res;
#if LIB_NDOF
    if (mDriverState == JDS_INITIALIZED && mNdofDev)
    {
        res = ll_safe_string(mNdofDev->product);
    }
#endif
    return res;
}

bool LLViewerJoystick::isLikeSpaceNavigator() const
{
#if LIB_NDOF
    return (isJoystickInitialized()
            && (strncmp(mNdofDev->product, "SpaceNavigator", 14) == 0
                || strncmp(mNdofDev->product, "3Dconnexion SpaceNavigator", 26) == 0
                || strncmp(mNdofDev->product, "SpaceExplorer", 13) == 0
                || strncmp(mNdofDev->product, "SpaceTraveler", 13) == 0
                || strncmp(mNdofDev->product, "SpacePilot", 10) == 0));
#else
    return false;
#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::setSNDefaults()
{
#if LL_DARWIN || LL_LINUX
    const float platformScale = 20.f;
    const float platformScaleAvXZ = 1.f;
    // The SpaceNavigator doesn't act as a 3D cursor on OS X / Linux.
    const bool is_3d_cursor = false;
#else
    const float platformScale = 1.f;
    const float platformScaleAvXZ = 2.f;
    const bool is_3d_cursor = true;
#endif

    //gViewerWindow->alertXml("CacheWillClear");
    LL_INFOS("Joystick") << "restoring SpaceNavigator defaults..." << LL_ENDL;
    gSavedSettings.setBOOL("BlackDragonControls", FALSE);

    gSavedSettings.setS32("JoystickAxis0", 1); // z (at)
    gSavedSettings.setS32("JoystickAxis1", 0); // x (slide)
    gSavedSettings.setS32("JoystickAxis2", 2); // y (up)
    gSavedSettings.setS32("JoystickAxis3", 4); // pitch
    gSavedSettings.setS32("JoystickAxis4", 3); // roll
    gSavedSettings.setS32("JoystickAxis5", 5); // yaw
    gSavedSettings.setS32("JoystickAxis6", -1);

    gSavedSettings.setS32("JoystickButtonJump", -1);
    gSavedSettings.setS32("JoystickButtonCrouch", -1);
    gSavedSettings.setS32("JoystickButtonFly", -1);
    gSavedSettings.setS32("JoystickButtonRunToggle", -1);
    gSavedSettings.setS32("JoystickButtonMouselook", -1);
    gSavedSettings.setS32("JoystickButtonZoomDefault", -1);
    gSavedSettings.setS32("JoystickButtonFlycam", 0);
    gSavedSettings.setS32("JoystickButtonZoomOut", -1);
    gSavedSettings.setS32("JoystickButtonZoomIn", -1);
    gSavedSettings.setS32("JoystickButtonRollLeft", -1);
    gSavedSettings.setS32("JoystickButtonRollRight", -1);
    gSavedSettings.setS32("JoystickButtonRollDefault", -1);

    gSavedSettings.setBOOL("Cursor3D", is_3d_cursor);
    gSavedSettings.setBOOL("AutoLeveling", true);
    gSavedSettings.setBOOL("ZoomDirect", false);
    gSavedSettings.setBOOL("JoystickInvertPitch", true);

    gSavedSettings.setF32("AvatarAxisScale0", 1.f * platformScaleAvXZ);
    gSavedSettings.setF32("AvatarAxisScale1", 1.f * platformScaleAvXZ);
    gSavedSettings.setF32("AvatarAxisScale2", 1.f);
    gSavedSettings.setF32("AvatarAxisScale4", .1f * platformScale);
    gSavedSettings.setF32("AvatarAxisScale5", .1f * platformScale);
    gSavedSettings.setF32("AvatarAxisScale3", 0.f * platformScale);
    gSavedSettings.setF32("BuildAxisScale1", .3f * platformScale);
    gSavedSettings.setF32("BuildAxisScale2", .3f * platformScale);
    gSavedSettings.setF32("BuildAxisScale0", .3f * platformScale);
    gSavedSettings.setF32("BuildAxisScale4", .3f * platformScale);
    gSavedSettings.setF32("BuildAxisScale5", .3f * platformScale);
    gSavedSettings.setF32("BuildAxisScale3", .3f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale1", 2.f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale2", 2.f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale0", 2.1f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale4", .1f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale5", .15f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale3", 0.f * platformScale);
    gSavedSettings.setF32("FlycamAxisScale6", 0.f * platformScale);

    gSavedSettings.setF32("AvatarAxisDeadZone0", .1f);
    gSavedSettings.setF32("AvatarAxisDeadZone1", .1f);
    gSavedSettings.setF32("AvatarAxisDeadZone2", .1f);
    gSavedSettings.setF32("AvatarAxisDeadZone3", 1.f);
    gSavedSettings.setF32("AvatarAxisDeadZone4", .02f);
    gSavedSettings.setF32("AvatarAxisDeadZone5", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone0", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone1", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone2", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone3", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone4", .01f);
    gSavedSettings.setF32("BuildAxisDeadZone5", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone0", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone1", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone2", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone3", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone4", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone5", .01f);
    gSavedSettings.setF32("FlycamAxisDeadZone6", 1.f);

    gSavedSettings.setF32("AvatarFeathering", 6.f);
    gSavedSettings.setF32("BuildFeathering", 12.f);
    gSavedSettings.setF32("FlycamFeathering", 5.f);
}

//BD - Xbox360 Controller Support
void LLViewerJoystick::setXboxDefaults()
{
    LL_INFOS() << "restoring Xbox Controller defaults..." << LL_ENDL;
    gSavedSettings.setBOOL("BlackDragonControls", TRUE);

    gSavedSettings.setS32("JoystickAxis0", 1);  // Z
    gSavedSettings.setS32("JoystickAxis1", 0);  // X
    gSavedSettings.setS32("JoystickAxis2", -1); // Y
    gSavedSettings.setS32("JoystickAxis3", 2);  // Roll
    gSavedSettings.setS32("JoystickAxis4", 4);  // Pitch
    gSavedSettings.setS32("JoystickAxis5", 3);  // Yaw
    gSavedSettings.setS32("JoystickAxis6", -1); // Zoom

    // [fork] Personalized defaults: values below marked [user] come from the
    // owner's proven AlchemyBeta 7.1.9.2516 config (Desktop settings.xml
    // snapshot, 2026-07-16) so "Xbox Defaults" restores THAT setup, not stock.
    gSavedSettings.setS32("JoystickButtonJump", 1);     // [user] (stock 0)
    gSavedSettings.setS32("JoystickButtonCrouch", 0);   // [user] (stock 1)
    gSavedSettings.setS32("JoystickButtonFly", 2);
    gSavedSettings.setS32("JoystickButtonRunToggle", 6);  // [user] (stock 8)
    gSavedSettings.setS32("JoystickButtonMouselook", 9);
    gSavedSettings.setS32("JoystickButtonZoomDefault", 7); // [user] (stock 6)
    gSavedSettings.setS32("JoystickButtonFlycam", 7);
    gSavedSettings.setS32("JoystickButtonZoomOut", 5);
    gSavedSettings.setS32("JoystickButtonZoomIn", 4);
    gSavedSettings.setS32("JoystickButtonRollLeft", -1);
    gSavedSettings.setS32("JoystickButtonRollRight", -1);
    gSavedSettings.setS32("JoystickButtonRollDefault", -1);

    const ALXInput::DpadSettings dpad = ALXInput::fillDroneDpadDefaults({
        gSavedSettings.getS32("JoystickXInputDpadUp"),
        gSavedSettings.getS32("JoystickXInputDpadLeft"),
        gSavedSettings.getS32("JoystickXInputDpadRight"),
        gSavedSettings.getS32("JoystickXInputDpadMigrationVersion")});
    gSavedSettings.setS32("JoystickXInputDpadUp", dpad.up);
    gSavedSettings.setS32("JoystickXInputDpadLeft", dpad.left);
    gSavedSettings.setS32("JoystickXInputDpadRight", dpad.right);

    gSavedSettings.setBOOL("Cursor3D", false); // Xbox Gamepad, not 3D Mouse
    gSavedSettings.setBOOL("AutoLeveling", false);
    gSavedSettings.setBOOL("ZoomDirect", false);
    gSavedSettings.setBOOL("JoystickInvertPitch", false);

    gSavedSettings.setF32("AvatarAxisScale0", 1.f);
    gSavedSettings.setF32("AvatarAxisScale2", 1.f);
    gSavedSettings.setF32("AvatarAxisScale1", 1.f);
    gSavedSettings.setF32("AvatarAxisScale4", 1.f);
    gSavedSettings.setF32("AvatarAxisScale5", 1.f);
    gSavedSettings.setF32("AvatarAxisScale3", 1.f);
    gSavedSettings.setF32("BuildAxisScale0", 1.25f);
    gSavedSettings.setF32("BuildAxisScale2", 1.25f);
    gSavedSettings.setF32("BuildAxisScale1", 1.25f);
    gSavedSettings.setF32("BuildAxisScale4", 1.f);
    gSavedSettings.setF32("BuildAxisScale5", 1.f);
    gSavedSettings.setF32("BuildAxisScale3", 1.f);
    // [user] flycam scales: slow, machinima-tuned; scale4 negative = inverted pitch
    gSavedSettings.setF32("FlycamAxisScale0", 0.6f);    // [user] (stock 5.0)
    gSavedSettings.setF32("FlycamAxisScale2", 0.03f);   // [user] (stock 5.0)
    gSavedSettings.setF32("FlycamAxisScale1", 0.4f);    // [user] (stock 5.0)
    gSavedSettings.setF32("FlycamAxisScale4", -0.75f);  // [user] (stock 2.0)
    gSavedSettings.setF32("FlycamAxisScale5", 0.66f);   // [user] (stock 2.5)
    gSavedSettings.setF32("FlycamAxisScale3", 0.2f);    // [user] (stock 2.0)
    gSavedSettings.setF32("FlycamAxisScale6", 0.03f);   // [user] (stock 1.0)

    gSavedSettings.setF32("AvatarAxisDeadZone0", .6f);
    gSavedSettings.setF32("AvatarAxisDeadZone2", .3f);
    gSavedSettings.setF32("AvatarAxisDeadZone1", .6f);
    gSavedSettings.setF32("AvatarAxisDeadZone3", .3f);
    gSavedSettings.setF32("AvatarAxisDeadZone4", .3f);
    gSavedSettings.setF32("AvatarAxisDeadZone5", .3f);
    gSavedSettings.setF32("BuildAxisDeadZone0", .34f);  // [user] (stock .25)
    gSavedSettings.setF32("BuildAxisDeadZone2", .25f);
    gSavedSettings.setF32("BuildAxisDeadZone1", .25f);
    gSavedSettings.setF32("BuildAxisDeadZone3", .3f);
    gSavedSettings.setF32("BuildAxisDeadZone4", .3f);
    gSavedSettings.setF32("BuildAxisDeadZone5", .1f);
    // [user] flycam deadzones: tight (precise SpaceNav-style input on gamepad)
    gSavedSettings.setF32("FlycamAxisDeadZone0", .1f);  // [user] (stock .25)
    gSavedSettings.setF32("FlycamAxisDeadZone2", .1f);  // [user] (stock .25)
    gSavedSettings.setF32("FlycamAxisDeadZone1", .1f);  // [user] (stock .25)
    gSavedSettings.setF32("FlycamAxisDeadZone3", .02f); // [user] (stock .1)
    gSavedSettings.setF32("FlycamAxisDeadZone4", .1f);  // [user] (stock .3)
    gSavedSettings.setF32("FlycamAxisDeadZone5", .1f);  // [user] (stock .3)
    gSavedSettings.setF32("FlycamAxisDeadZone6", 0.f);  // [user] (stock .1)

    gSavedSettings.setF32("AvatarFeathering", 20.0f);
    gSavedSettings.setF32("BuildFeathering", 3.f);
    gSavedSettings.setF32("FlycamFeathering", 1.0f);
}

bool LLViewerJoystick::applyDronePreset(S32 preset)
{
    if (preset < 0 || preset >= static_cast<S32>(LLFlycamDrone::PresetId::COUNT))
        return false;

    DronePresetProvenance& provenance = dronePresetProvenance();
    provenance.source = preset;
    provenance.custom = false;
    ++provenance.revision;

    const LLFlycamDrone::HumanParams& p =
        LLFlycamDrone::presets()[static_cast<std::size_t>(preset)].human;
    gSavedSettings.setS32("FlycamDronePreset", preset);
    gSavedSettings.setF32("FlycamDroneCruise", p.cruise);
    gSavedSettings.setF32("FlycamDroneTop", p.top);
    gSavedSettings.setF32("FlycamDroneTimeToTop", p.timeToTop);
    gSavedSettings.setF32("FlycamDroneStopDistance", p.stopDistance);
    gSavedSettings.setF32("FlycamDroneHoldTime", p.holdTime);
    gSavedSettings.setF32("FlycamDroneCoastTime", p.coastTime);
    gSavedSettings.setF32("FlycamDroneHighSpeedFloat", p.highSpeedFloat);
    gSavedSettings.setF32("FlycamDroneOnset", p.onset);
    gSavedSettings.setF32("FlycamDroneGrip", p.grip);
    gSavedSettings.setF32("FlycamDroneTurnRate", p.turnRateDegrees);
    gSavedSettings.setF32("FlycamDroneTurnWeight", p.turnWeight);
    gSavedSettings.setF32("FlycamDroneTiltRate", p.tiltRateDegrees);
    gSavedSettings.setF32("FlycamDroneClimb", p.climb);
    gSavedSettings.setF32("FlycamDroneClimbTime", p.climbTime);
    gSavedSettings.setS32("FlycamDroneRelease",
        p.release == LLFlycamDrone::ReleaseMode::DRIFT ? 1 : 0);
    gSavedSettings.setS32("FlycamDroneThrottleMode",
        p.throttleMode == LLFlycamDrone::ThrottleMode::PURE ? 1 : 0);
    gSavedSettings.setBOOL("FlycamDroneFreeFly", p.freeFly);
    return true;
}

LLFlycamDrone::CompiledValues LLViewerJoystick::getDroneCompiledValues() const
{
    return cachedDroneCompiledValues();
}

LLFlycamDrone::HumanParams LLViewerJoystick::getDroneAcceptedValues() const
{
    return droneCompilePreviewCache().accepted;
}

F32 LLViewerJoystick::getDroneCompiledTick() const
{
    return droneCompilePreviewCache().params.tick;
}

void LLViewerJoystick::markDronePresetCustom()
{
    DronePresetProvenance& provenance = dronePresetProvenance();
    const S32 current = gSavedSettings.getS32("FlycamDronePreset");
    if (current >= 0 && current < static_cast<S32>(LLFlycamDrone::PresetId::COUNT))
    {
        provenance.source = current;
    }
    provenance.custom = true;
    gSavedSettings.setS32("FlycamDronePreset", 5);
    ++provenance.revision;
}

S32 LLViewerJoystick::getDronePresetProvenance() const
{
    return dronePresetProvenance().source;
}

bool LLViewerJoystick::isDronePresetCustom() const
{
    return dronePresetProvenance().custom;
}

U64 LLViewerJoystick::getDronePresetProvenanceRevision() const
{
    return dronePresetProvenance().revision;
}

void LLViewerJoystick::resetDroneSettings()
{
    gSavedSettings.setS32("FlycamFlightModel", 0);
    applyDronePreset(static_cast<S32>(LLFlycamDrone::PresetId::NORMAL));
    gSavedSettings.setS32("FlycamDroneThrottleMode", 0);
    gSavedSettings.setF32("FlycamDroneSimulationHz", 120.f);
    gSavedSettings.setF32("FlycamDroneMoveDeadzone", .10f);
    gSavedSettings.setF32("FlycamDroneLookDeadzone", .12f);
    gSavedSettings.setF32("FlycamDroneSaturation", .05f);
    gSavedSettings.setF32("FlycamDroneExpoMove", .35f);
    gSavedSettings.setF32("FlycamDroneExpoYaw", .45f);
    gSavedSettings.setF32("FlycamDroneExpoGimbal", .45f);
    gSavedSettings.setS32("FlycamDroneResumeMomentum", 0);
}
