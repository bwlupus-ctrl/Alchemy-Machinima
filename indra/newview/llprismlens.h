/**
 * @file llprismlens.h
 * @brief Bounded Prism Lens and virtual-camera surface feeds.
 */

#ifndef LL_LLPRISMLENS_H
#define LL_LLPRISMLENS_H

#include "stdtypes.h"
#include "llmath.h"
#include "llquaternion.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3color.h"
#include "v3math.h"

#include <array>
#include <boost/signals2/connection.hpp>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class LLFace;
class LLPlane;
class LLRenderTarget;
class LLViewerObject;
class LLVOVolume;
namespace LLPrismLens
{
// A capture owns an auxiliary render target. A display binding owns only a
// destination face and samples its capture's retained target. Keeping these
// limits separate is what makes one camera -> many faces inexpensive.
constexpr U32 MAX_CAPTURES = 8;
constexpr U32 MAX_DISPLAY_BINDINGS = 16;
constexpr U32 MAX_PROJECTOR_BINDINGS = 8;
constexpr U32 MAX_LENSES = MAX_CAPTURES; // Temporary source compatibility.

enum class ERegistryResult : U8
{
    OK,
    INVALID_SELECTION,
    DUPLICATE,
    AT_CAPACITY,
    STALE_HANDLE,
    INVALID_CONFIGURATION
};

enum class ECaptureMode : U8
{
    SURFACE_LENS,
    CAMERA_FEED
};

enum class EFitMode : U8
{
    FIT,
    FILL,
    STRETCH
};

enum class EFovMode : U8
{
    FIXED,
    FOLLOW_PROJECTOR
};

enum class EOutputRateMode : U8
{
    AUTOMATIC,
    TARGET_FPS
};

enum class EGateMode : U8
{
    MANUAL,
    AUTO_CYCLE
};

enum class ECaptureHealth : U8
{
    READY,
    UNBOUND_SOURCE,
    SOURCE_OFFLINE,
    INVALID_SOURCE,
    LENS_SURFACE_OFFLINE,
    INVALID_LENS_SURFACE
};

enum class EDisplayHealth : U8
{
    READY,
    OFFLINE,
    INVALID
};

enum class EDisplayVisibility : U8
{
    UNKNOWN,
    OFFSCREEN,
    VISIBLE
};

enum class EProjectorTarget : U8
{
    OBJECT,
    RIG_LIGHT,
    VIRTUAL
};

enum class ENoSignal : U8
{
    OWN_COOKIE,
    BLACK,
    WHITE
};

enum class EProjectorThrow : U8
{
    OFF,
    LIGHT_FOV,
    CAMERA_FOV
};

enum class EOutputState : U8
{
    EMPTY,
    CURRENT,
    HELD,
    SUPPRESSED
};

enum class EActivityState : U8
{
    IDLE,
    WAITING,
    LIVE,
    PAUSED,
    THROTTLED
};

enum class EPerformanceState : U8
{
    DISABLED,
    LEARNING,
    STEADY,
    PROTECTING,
    SUSPENDED,
    TIMING_UNAVAILABLE
};

struct OpticsSettings
{
    F32 mChromaticAberration = 0.0f; // 0.0 to 1.0 (lens fringe)
    F32 mFilmGrain = 0.0f;           // 0.0 to 1.0 (analog noise)
    F32 mCRTScanlines = 0.0f;        // 0.0 to 1.0 (CRT monitor lines)
    F32 mExposureBias = 0.0f;        // -4.0 to +4.0 EV
};

constexpr U8 BONE_ANCHOR_ME = 0;
constexpr U8 BONE_ANCHOR_A = 1;
constexpr U8 BONE_ANCHOR_B = 2;
constexpr U8 BONE_ANCHOR_C = 3;
constexpr U8 BONE_ANCHOR_D = 4;

// Current joint-selection tags. Values 0 and 2 remain reserved for normalizing
// scenes written by the first Bone POV build (Head and Neck respectively).
// NAMED deliberately keeps the old Custom value so an old viewer can consume a
// new named selection through its existing custom_joint path.
constexpr U8 BONE_JOINT_LEGACY_HEAD = 0;
constexpr U8 BONE_JOINT_EYELINE = 1;
constexpr U8 BONE_JOINT_LEGACY_NECK = 2;
constexpr U8 BONE_JOINT_NAMED = 3;

struct NormalizedBonePovJointSelection
{
    U8 mTag = BONE_JOINT_EYELINE;
    std::string mName;
};

// Normalize both the old four-way selection and the current two-way tag. This
// is intentionally pure so scene compatibility can be tested without an
// avatar or viewer singleton.
inline NormalizedBonePovJointSelection normalizeBonePovJointSelection(
    U8 selection, const std::string& selected_name)
{
    switch (selection)
    {
        case BONE_JOINT_LEGACY_HEAD:
            return { BONE_JOINT_NAMED, "mHead" };
        case BONE_JOINT_EYELINE:
            return { BONE_JOINT_EYELINE, std::string() };
        case BONE_JOINT_LEGACY_NECK:
            return { BONE_JOINT_NAMED, "mNeck" };
        case BONE_JOINT_NAMED:
            return { BONE_JOINT_NAMED, selected_name };
        default:
            return { BONE_JOINT_EYELINE, std::string() };
    }
}

// These joints use the avatar's established X-forward/Z-up torso frame. Other
// named joints default to position-only stabilized aim until the operator opts
// into their best-effort local orientation.
inline bool isBonePovSpineJoint(std::string_view name)
{
    return name == "mPelvis" || name == "mSpine1" || name == "mSpine2" ||
           name == "mSpine3" || name == "mSpine4" || name == "mTorso" ||
           name == "mChest" || name == "mNeck" || name == "mHead";
}

constexpr U8 BONE_AIM_FULL_FOLLOW = 0;
constexpr U8 BONE_AIM_STABILIZED = 1;

constexpr U8 BONE_ROLL_HORIZON_LOCK = 0;
constexpr U8 BONE_ROLL_INHERIT = 1;

// Persistent, pointer-free configuration for an optional skeleton attachment.
// Named-joint full-follow is explicitly best-effort off the spine chain: an
// arbitrary bone's +X can run down the bone, so the editor defaults a newly
// selected off-spine joint to stabilized.
struct BonePovSettings
{
    bool mEnabled = false;
    U8 mAnchorSlot = BONE_ANCHOR_ME;
    U8 mJointSelection = BONE_JOINT_EYELINE;
    U8 mAimMode = BONE_AIM_FULL_FOLLOW;
    U8 mRollMode = BONE_ROLL_HORIZON_LOCK;
    bool mScaleAware = true;
    LLVector3 mOffset;
    F32 mTrimPitchDeg = 0.f;
    F32 mTrimYawDeg = 0.f;
    F32 mFovDeg = 60.f;
    F32 mSmoothingSec = 0.15f;
    // Existing custom_joint scene slot; now the selected name for every NAMED
    // joint. Empty/unused for the synthetic EYELINE selection.
    std::string mCustomJoint;
};

struct CameraSettings
{
    EFovMode mFovMode = EFovMode::FIXED;
    F32 mFixedVerticalFovRad = 60.f * DEG_TO_RAD;
    F32 mNearClip = 0.05f;
    F32 mFarClip = 256.f;
    LLVector3 mLocalEyeOffset;
    F32 mOutputAspect = 16.f / 9.f;
    OpticsSettings mOptics;

    // Render-only Blender-style camera "frustum gizmo", drawn client-side each
    // frame at this camera's world transform. Pure overlay: no rezzed prim, no
    // render target, no shader, no allocation. Default OFF so a default-
    // constructed capture is byte-identical/inert. The four aids toggle
    // independently, each gated by the master mShowGuide.
    bool mShowGuide        = false; // master per-camera guide toggle
    bool mGuideThirds      = true;  // rule-of-thirds grid on the framing gate
    bool mGuideUpRoll      = true;  // up/roll nub on the gate's top edge
    bool mGuideCrosshair   = false; // center crosshair at the gate centre
    bool mGuideClipMarkers = true;  // near-clip rectangle marker

    // Prim-free "virtual camera". When set, the capture derives its eye and
    // orientation from the stored transform below instead of from an in-world
    // object (mCameraObjectId is ignored / null), so it needs no rezzed prim and
    // works in no-rez parcels. Default OFF so a default-constructed capture is
    // byte-identical/inert and every existing object-anchored path is unchanged.
    //
    // COORDINATE SPACE: mVirtualPos is stored in AGENT space -- the exact same
    // space the object render path reads via LLViewerObject::getRenderPosition()
    // (which returns getPositionAgent()) and the same space that
    // LLViewerCamera::getOrigin() returns. No conversion is needed on capture or
    // on render; "Snap to my view" stores getOrigin() verbatim. mVirtualRot maps
    // the camera's local axes to agent space with the render-path convention:
    // forward = local -Z, up = local +Y (so (0,0,-1)*mVirtualRot == view
    // forward and (0,1,0)*mVirtualRot == view up). mLocalEyeOffset is NOT applied
    // to a virtual camera (offset is an object-rig convenience only).
    bool         mVirtual  = false; // objectless: use stored transform, not mCameraObjectId
    bool         mTestPattern = false; // objectless, static local SMPTE/orientation source
    LLVector3    mVirtualPos;       // stored eye position, AGENT space
    LLQuaternion mVirtualRot;       // stored orientation (fwd = local -Z, up = local +Y)

    // Optional avatar/clone bone driver. Default-disabled and pointer-free;
    // transient smoothing/history lives in ALVCamBonePov, not in scene data.
    BonePovSettings mBonePov;
};

// An armed camera is a reference to one of the user's existing captures. The
// Gate owns no producer and arming never changes the capture count.
struct GateArmedCamera
{
    LLUUID mArmId;
    std::string mLabel;
    bool mEnabled = true;
    LLUUID mCaptureId;
};

inline bool isOtsPairArmLabel(const std::string& label)
{
    return label == "OTS A" || label == "OTS B";
}

struct GateSettings
{
    LLUUID mGateId;
    bool mActive = false;
    EGateMode mMode = EGateMode::MANUAL;
    F64 mIntervalSeconds = 8.0;
    U32 mPrewarmFrames = 3;
    std::vector<GateArmedCamera> mArmed;
    S32 mProgramArmIndex = 0;
    S32 mPreviewArmIndex = -1;
};

struct GateSnapshot
{
    U64 mRevision = 0;
    GateSettings mSettings;
    S32 mOnAirArmIndex = -1;
    S32 mWarmArmIndex = -1;
    U64 mCutSerial = 0;
    std::string mReason;
};

struct CaptureRateSettings
{
    EOutputRateMode mMode = EOutputRateMode::AUTOMATIC;
    // Preserved while Automatic so changing modes restores creative intent.
    F32 mTargetFps = 30.f;
};

struct CaptureRuntimeState
{
    ECaptureHealth mHealth = ECaptureHealth::READY;
    EOutputState mOutput = EOutputState::EMPTY;
    EActivityState mActivity = EActivityState::IDLE;
    std::string mReason;
    F32 mEffectiveVerticalFovRad = 0.f;
    F32 mEffectiveFarClip = 0.f;
    F32 mEffectiveResolutionScale = 0.f;
    F32 mCadenceEntitlementHz = 0.f;
    F32 mPreviewRequestedHz = 0.f;
    F32 mPreviewEntitlementHz = 0.f;
    bool mPreviewOnly = false;
    F32 mObservedPublicationHz = 0.f;
    F32 mOutputAgeSeconds = 0.f;
};

struct DisplayRuntimeState
{
    EDisplayHealth mHealth = EDisplayHealth::READY;
    EDisplayVisibility mVisibility = EDisplayVisibility::UNKNOWN;
    std::string mHealthReason;
    std::string mVisibilityReason;
};

struct PerformanceSnapshot
{
    U64 mRevision = 0;
    bool mAdaptiveEnabled = true;
    EPerformanceState mState = EPerformanceState::LEARNING;
    F32 mRequestedProtectedMainFps = 30.f;
    F32 mEffectiveProtectedMainFps = 30.f;
    bool mRequestedTargetAvailable = true;
    F32 mMainRenderP95Milliseconds = 0.f;
    F32 mPresentedFps = 0.f;
    F32 mUserResolutionScaleCeiling = 1.f;
    F32 mRequestedTotalCaptureBudgetHz = 30.f; // 0 means Manual Every Frame.
    F32 mEffectiveTotalCaptureBudgetHz = 30.f;
    F32 mMinimumAppliedResolutionScale = 0.f;
    F32 mMaximumAppliedResolutionScale = 0.f;
    F32 mAdmittedGlobalAttemptRateHz = 0.f;
    F32 mObservedGlobalAttemptRateHz = 0.f;
    bool mGpuTimingReliable = false;
    std::string mReason;
};

struct CaptureHandle
{
    LLUUID mId;
    U64 mGeneration = 0;
};

struct DisplayHandle
{
    LLUUID mId;
    U64 mGeneration = 0;
};

struct ProjectorHandle
{
    LLUUID mId;
    U64 mGeneration = 0;
};

struct ActionStatus
{
    ERegistryResult mResult = ERegistryResult::INVALID_SELECTION;
    std::string mReason;
    bool allowed() const { return mResult == ERegistryResult::OK; }
};

// Per-display TV/CRT screen effects, applied at composite time only inside
// prismLensF.glsl on the already-retained capture picture. No extra render
// pass, target, or allocation is involved. Every knob is a strict no-op at 0
// (the shader gates each effect block at > 0.001), so a default-constructed
// struct leaves the composite bit-identical to a build without this feature.
// Scanline shape parameters and mRollSpeed only matter while
// their gating strength (mScanlines / mVerticalRoll) is non-zero, which is
// why they carry non-zero defaults and are excluded from isZero().
struct ScreenEffects
{
    F32 mScanlines      = 0.f;  // 0..1 scanline blend strength
    // [PrismScanlines] Legacy coordinate: lines = 100 + 500*y.
    F32 mScanlineCount  = 0.5f; // -0.16..2.2 => 20..1200 dark/bright pairs
    F32 mScanlineDepth = 0.45f; // 0..1 maximum darkening
    F32 mScanlineSharpness = 0.f; // 0..1 sine to crisp bands
    F32 mScanlineThickness = 0.5f; // 0.1..0.9 bright-line duty
    F32 mPixelate       = 0.f;  // 0..1 block quantization strength
    F32 mGrayscale      = 0.f;  // 0..1 luma desaturation
    F32 mSepia          = 0.f;  // 0..1 sepia tone blend
    F32 mStatic         = 0.f;  // 0..1 analog hash noise strength
    F32 mVerticalRoll   = 0.f;  // 0..1 V-sync roll band strength
    F32 mRollSpeed      = 0.2f; // 0..1 roll speed rate
    F32 mTracking       = 0.f;  // 0..1 VHS horizontal tear/jitter
    F32 mFlicker        = 0.f;  // 0..1 brightness flicker strength
    F32 mChromaBleed    = 0.f;  // 0..1 horizontal color fringe
    F32 mVignette       = 0.f;  // 0..1 CRT edge darkening
    F32 mInterlace      = 0.f;  // 0..1 field line shimmer
    F32 mDropout        = 0.f;  // 0..1 signal dropout darkening
    // Simple per-display linear gain, -1..+1 with 0 = unchanged. Deliberately
    // distinct from the per-capture exposure OPTIC (an EV bias applied to the
    // camera's whole picture); both may coexist on the same face.
    F32 mBrightness     = 0.f;

    // Feature A - per-display screen-axis orientation. Applied to the sampling
    // UV before every other effect and the fetch, so all three are a strict
    // no-op when unset (false/false/false leaves the composite bit-identical).
    bool mFlipH         = false; // Mirror the picture left<->right
    bool mFlipV         = false; // Mirror the picture top<->bottom
    bool mRotate90      = false; // Quarter-turn the sampled feed in UV space

    // Feature B - environmental sheen / reflectivity, 0..1 with 0 = OFF. Adds a
    // Fresnel-weighted environment reflection over the feed for a glossy-panel
    // look. Strictly gated (> 0.001) in the shader, so 0 is bit-identical.
    F32 mSheen          = 0.f;

    void clampAndValidate()
    {
        mScanlines     = llclamp(mScanlines, 0.f, 1.f);
        // [PrismScanlines] Non-finite values recover legacy defaults.
        mScanlineCount = std::isfinite(mScanlineCount) ? llclamp(mScanlineCount, -0.16f, 2.2f) : 0.5f;
        mScanlineDepth = std::isfinite(mScanlineDepth) ? llclamp(mScanlineDepth, 0.f, 1.f) : 0.45f;
        mScanlineSharpness = std::isfinite(mScanlineSharpness) ? llclamp(mScanlineSharpness, 0.f, 1.f) : 0.f;
        mScanlineThickness = std::isfinite(mScanlineThickness) ? llclamp(mScanlineThickness, 0.1f, 0.9f) : 0.5f;
        mPixelate      = llclamp(mPixelate, 0.f, 1.f);
        mGrayscale     = llclamp(mGrayscale, 0.f, 1.f);
        mSepia         = llclamp(mSepia, 0.f, 1.f);
        mStatic        = llclamp(mStatic, 0.f, 1.f);
        mVerticalRoll  = llclamp(mVerticalRoll, 0.f, 1.f);
        mRollSpeed     = llclamp(mRollSpeed, 0.f, 1.f);
        mTracking      = llclamp(mTracking, 0.f, 1.f);
        mFlicker       = llclamp(mFlicker, 0.f, 1.f);
        mChromaBleed   = llclamp(mChromaBleed, 0.f, 1.f);
        mVignette      = llclamp(mVignette, 0.f, 1.f);
        mInterlace     = llclamp(mInterlace, 0.f, 1.f);
        mDropout       = llclamp(mDropout, 0.f, 1.f);
        mBrightness    = llclamp(mBrightness, -1.f, 1.f);
        mSheen         = llclamp(mSheen, 0.f, 1.f);
        // mFlipH / mFlipV / mRotate90 are bools; nothing to clamp.
    }

    bool isZero() const
    {
        return mScanlines == 0.f && mPixelate == 0.f && mGrayscale == 0.f &&
               mSepia == 0.f && mStatic == 0.f && mVerticalRoll == 0.f &&
               mTracking == 0.f && mFlicker == 0.f && mChromaBleed == 0.f &&
               mVignette == 0.f && mInterlace == 0.f && mDropout == 0.f &&
               mBrightness == 0.f && !mFlipH && !mFlipV && !mRotate90 &&
               mSheen == 0.f;
    }
};

struct DisplaySettings
{
    EFitMode mFitMode = EFitMode::FIT;
    F32 mAnchor[2] = { 0.5f, 0.5f };
    F32 mBarColorLinear[3] = { 0.f, 0.f, 0.f };
    ScreenEffects mEffects; // Per-display TV screen effects.

    // Prim-free "virtual screen". When set, this display is a viewer-drawn quad
    // at a stored world transform instead of a bound prim face, so it needs no
    // rezzed object and works in no-rez parcels. The identity fields on the
    // display (mObjectId / mTE) stay null / -1 and every face-driven code path is
    // gated on mVirtual, so a real-face display is byte-identical to before.
    //
    // COORDINATE SPACE: mPos / the synthesized corners are stored in AGENT space,
    // the exact same space the composite pass draws with (gGLModelView maps agent
    // space -> eye), so the quad positions and the surface uniforms share one
    // space and the shader's prism_uv = (position - surfaceOrigin) . UDual is
    // space-invariant. mRot maps the screen's local axes to agent space with the
    // convention right = local +X, up = local +Y (the same convention the camera
    // guide uses); the quad lies in the right/up plane. These live in
    // DisplaySettings (not the top-level display identity) so they flow through
    // the existing setDisplaySettings / snapshot / persist paths for free, and so
    // "Reposition to my view" and the size/aspect editor commit through the same
    // display-settings path a real display already uses.
    bool         mVirtual = false; // faceless: draw a quad at the stored transform
    LLVector3    mPos;             // screen centre, AGENT space
    LLQuaternion mRot;             // screen orientation (right = +X, up = +Y)
    F32          mWidth  = 1.6f;   // metres (default ~16:9 with mHeight)
    F32          mHeight = 0.9f;   // metres
};

struct ProjectorLook
{
    F32 mKeystoneH = 0.f;
    F32 mKeystoneV = 0.f;
    F32 mShutterFlicker = 0.f;
    F32 mShutterHz = 24.f;
    F32 mGateWeave = 0.f;
    F32 mHotspot = 0.f;
    F32 mLampKelvin = 6500.f;
    LLColor3 mGelTint = LLColor3(1.f, 1.f, 1.f);
    F32 mDust = 0.f;
    F32 mScratches = 0.f;
    F32 mBurn = 0.f;
    F32 mGrain = 0.f;
    F32 mHalation = 0.f;
    EProjectorThrow mThrowMode = EProjectorThrow::OFF;

    bool isDefault() const
    {
        return mKeystoneH == 0.f && mKeystoneV == 0.f &&
               mShutterFlicker == 0.f && mShutterHz == 24.f &&
               mGateWeave == 0.f && mHotspot == 0.f &&
               mLampKelvin == 6500.f &&
               mGelTint == LLColor3(1.f, 1.f, 1.f) &&
               mDust == 0.f && mScratches == 0.f && mBurn == 0.f &&
               mGrain == 0.f && mHalation == 0.f &&
               mThrowMode == EProjectorThrow::OFF;
    }

    bool hasFeedEffects() const
    {
        return mKeystoneH != 0.f || mKeystoneV != 0.f || mHotspot != 0.f ||
               mLampKelvin != 6500.f || mGelTint != LLColor3(1.f, 1.f, 1.f) ||
               mDust != 0.f || mScratches != 0.f || mBurn != 0.f ||
               mGrain != 0.f || mHalation != 0.f;
    }
};

struct ProjectorSettings
{
    EFitMode mFitMode = EFitMode::FIT;
    F32 mAnchor[2] = { 0.5f, 0.5f };
    F32 mFeedEV = 0.f;
    bool mFollowMainExposure = true;
    U16 mResolution = 512;
    ENoSignal mNoSignal = ENoSignal::OWN_COOKIE;
    bool mKeepGobo = false;
    bool mFlipH = false;
    bool mFlipV = false;
    U8 mRotate = 0; // clockwise quarter turns: 0, 1, 2, 3
    ScreenEffects mEffects;
    ProjectorLook mLook;
};

// CPU mirrors of the shader's two identity-sensitive transforms. Keeping
// these tiny helpers public makes the contract testable without a GL context.
inline LLColor3 projectorKelvinRgb(F32 kelvin)
{
    kelvin = llclamp(kelvin, 2700.f, 10000.f);
    if (kelvin == 6500.f) return LLColor3(1.f, 1.f, 1.f);

    const auto approximate = [](F32 temperature)
    {
        const F32 t = temperature / 100.f;
        const F32 r = t <= 66.f ? 255.f
            : 329.698727446f * powf(t - 60.f, -0.1332047592f);
        const F32 g = t <= 66.f
            ? 99.4708025861f * logf(t) - 161.1195681661f
            : 288.1221695283f * powf(t - 60.f, -0.0755148492f);
        const F32 b = t >= 66.f ? 255.f : (t <= 19.f ? 0.f
            : 138.5177312231f * logf(t - 10.f) - 305.0447927307f);
        return LLColor3(llclamp(r, 0.f, 255.f), llclamp(g, 0.f, 255.f),
                        llclamp(b, 0.f, 255.f));
    };
    const LLColor3 rgb = approximate(kelvin);
    const LLColor3 neutral = approximate(6500.f);
    return LLColor3(rgb.mV[0] / neutral.mV[0],
                    rgb.mV[1] / neutral.mV[1],
                    rgb.mV[2] / neutral.mV[2]);
}

inline void projectorKeystoneUv(F32 u, F32 v, F32 horizontal, F32 vertical,
                                F32& warped_u, F32& warped_v, bool& inside)
{
    const F32 x = u - 0.5f;
    const F32 y = v - 0.5f;
    const F32 dx = 1.f + llclamp(horizontal, -1.f, 1.f) * y * 1.5f;
    const F32 dy = 1.f + llclamp(vertical, -1.f, 1.f) * x * 1.5f;
    warped_u = x / dx + 0.5f;
    warped_v = y / dy + 0.5f;
    inside = warped_u >= 0.f && warped_u <= 1.f &&
             warped_v >= 0.f && warped_v <= 1.f;
}

inline LLSD projectorLookToLLSD(const ProjectorLook& look)
{
    LLSD data = LLSD::emptyMap();
    data["keystone_h"] = look.mKeystoneH;
    data["keystone_v"] = look.mKeystoneV;
    data["shutter_flicker"] = look.mShutterFlicker;
    data["shutter_hz"] = look.mShutterHz;
    data["gate_weave"] = look.mGateWeave;
    data["hotspot"] = look.mHotspot;
    data["lamp_kelvin"] = look.mLampKelvin;
    LLSD gel = LLSD::emptyArray();
    gel.append(look.mGelTint.mV[0]);
    gel.append(look.mGelTint.mV[1]);
    gel.append(look.mGelTint.mV[2]);
    data["gel_tint"] = gel;
    data["dust"] = look.mDust;
    data["scratches"] = look.mScratches;
    data["burn"] = look.mBurn;
    data["grain"] = look.mGrain;
    data["halation"] = look.mHalation;
    data["throw_mode"] = look.mThrowMode == EProjectorThrow::LIGHT_FOV
        ? "light_fov" : (look.mThrowMode == EProjectorThrow::CAMERA_FOV
        ? "camera_fov" : "off");
    return data;
}

inline bool projectorLookFromLLSD(const LLSD& data, ProjectorLook& look)
{
    if (!data.isMap()) return false;
    ProjectorLook parsed;
    const auto read_clamped = [&data](const char* key, F32& value,
                                      F32 minimum, F32 maximum)
    {
        if (!data.has(key) ||
            (!data[key].isReal() && !data[key].isInteger()))
        {
            return;
        }
        const F32 candidate = static_cast<F32>(data[key].asReal());
        if (std::isfinite(candidate)) value = llclamp(candidate, minimum, maximum);
    };
    read_clamped("keystone_h", parsed.mKeystoneH, -1.f, 1.f);
    read_clamped("keystone_v", parsed.mKeystoneV, -1.f, 1.f);
    read_clamped("shutter_flicker", parsed.mShutterFlicker, 0.f, 1.f);
    read_clamped("shutter_hz", parsed.mShutterHz, 12.f, 48.f);
    read_clamped("gate_weave", parsed.mGateWeave, 0.f, 1.f);
    read_clamped("hotspot", parsed.mHotspot, 0.f, 1.f);
    read_clamped("lamp_kelvin", parsed.mLampKelvin, 2700.f, 10000.f);
    read_clamped("dust", parsed.mDust, 0.f, 1.f);
    read_clamped("scratches", parsed.mScratches, 0.f, 1.f);
    read_clamped("burn", parsed.mBurn, 0.f, 1.f);
    read_clamped("grain", parsed.mGrain, 0.f, 1.f);
    read_clamped("halation", parsed.mHalation, 0.f, 1.f);
    if (data.has("gel_tint"))
    {
        const LLSD& gel = data["gel_tint"];
        if (gel.isArray() && gel.size() == 3 &&
            (gel[0].isReal() || gel[0].isInteger()) &&
            (gel[1].isReal() || gel[1].isInteger()) &&
            (gel[2].isReal() || gel[2].isInteger()))
        {
            const F32 r = static_cast<F32>(gel[0].asReal());
            const F32 g = static_cast<F32>(gel[1].asReal());
            const F32 b = static_cast<F32>(gel[2].asReal());
            if (std::isfinite(r) && std::isfinite(g) && std::isfinite(b))
                parsed.mGelTint = LLColor3(llmax(0.f, r), llmax(0.f, g),
                                           llmax(0.f, b));
        }
    }
    if (data.has("throw_mode"))
    {
        const std::string mode = data["throw_mode"].asString();
        if (mode == "light_fov") parsed.mThrowMode = EProjectorThrow::LIGHT_FOV;
        else if (mode == "camera_fov") parsed.mThrowMode = EProjectorThrow::CAMERA_FOV;
    }
    look = parsed;
    return true;
}

struct ProjectorRuntimeState
{
    EDisplayHealth mHealth = EDisplayHealth::OFFLINE;
    std::string mReason;
    U32 mLastConsumedFrame = 0;
    bool mDemanded = false;
};

struct CaptureDefinition
{
    U32 mSlot = MAX_CAPTURES;
    CaptureHandle mHandle;
    std::string mLabel;
    ECaptureMode mMode = ECaptureMode::SURFACE_LENS;
    LLUUID mCameraObjectId;
    CameraSettings mCamera;
    CaptureRateSettings mRate;
    CaptureRuntimeState mRuntime;
    U32 mDisplayCount = 0;
    U32 mProjectorCount = 0;
};

struct DisplayDefinition
{
    DisplayHandle mHandle;
    CaptureHandle mCapture;
    LLUUID mDisplayObjectId;
    S32 mDisplayTextureEntry = -1;
    DisplaySettings mSettings;
    DisplayRuntimeState mRuntime;
    bool mGateSubscribed = false;
    bool mQuarantined = false;
    std::string mQuarantineReason;
};

struct ProjectorDefinition
{
    ProjectorHandle mHandle;
    CaptureHandle mCapture;
    EProjectorTarget mTarget = EProjectorTarget::OBJECT;
    LLUUID mObjectId;
    U8 mRigSlot = 0;
    U8 mRigLight = 0;
    ProjectorSettings mSettings;
    ProjectorRuntimeState mRuntime;
    bool mGateSubscribed = false;
    bool mQuarantined = false;
    std::string mQuarantineReason;
};

inline bool validRigProjectorIdentity(U8 rig_slot, U8 rig_light,
                                      U8 rig_count, U8 light_count)
{
    return rig_slot < rig_count && rig_light < light_count;
}

inline bool sameRigProjectorIdentity(const ProjectorDefinition& projector,
                                     U8 rig_slot, U8 rig_light)
{
    return projector.mTarget == EProjectorTarget::RIG_LIGHT &&
           projector.mRigSlot == rig_slot && projector.mRigLight == rig_light;
}

inline bool captureOutputRequired(U32 display_count, U32 projector_count)
{
    return display_count != 0 || projector_count != 0;
}

inline bool removeProjectorWithCapture(bool owns_removed_capture,
                                       bool gate_subscribed)
{
    return owns_removed_capture && !gate_subscribed;
}

inline bool quarantineLoadedProjectorTarget(bool object_present,
                                            bool live_volume,
                                            bool hud_attachment,
                                            bool light,
                                            bool spotlight)
{
    return !object_present || !live_volume || hud_attachment ||
           !light || !spotlight;
}

// Registry-facing description of the retained producer. VP3 converts this
// signal into the pipeline-owned opaque, mipmapped cookie; callers never retain
// the source target pointer or change registry/target identity.
struct ProjectorFeed
{
    ProjectorHandle mProjector;
    CaptureHandle mCapture;
    LLRenderTarget* mOutput = nullptr; // Render-thread only; never retain.
    U32 mOutputWidth = 0;
    U32 mOutputHeight = 0;
    F32 mUvScale[2] = { 1.f, 1.f };
    F32 mUvOffset[2] = { 0.f, 0.f };
    U64 mGeneration = 0;
    F32 mCameraVerticalFovRad = 0.f;
    F32 mCameraAspect = 1.f;
    ProjectorSettings mSettings;
};

namespace ProjectorDemandPolicy
{
constexpr U32 CONSUMED_FRAMES = 2;
constexpr U32 BOOTSTRAP_FRAMES = 30;

inline bool active(U32 frame, U32 last_consumed_frame, U32 bound_frame,
                   bool target_ready)
{
    if (!target_ready) return false;
    const bool consumed = last_consumed_frame != 0 &&
        frame - last_consumed_frame <= CONSUMED_FRAMES;
    const bool bootstrap = frame - bound_frame < BOOTSTRAP_FRAMES;
    return consumed || bootstrap;
}
}

struct SkippedBinding
{
    DisplayHandle mHandle;
    LLUUID mObjectId;
    S32 mTextureEntry = -1;
    std::string mReason;
};

struct ApplyReport
{
    bool mSuccess = false;
    std::string mFatalReason;
    std::vector<SkippedBinding> mSkippedBindings;
    operator bool() const { return mSuccess; }
};

struct RegistrySnapshot
{
    U64 mConfigurationRevision = 0;
    U64 mRuntimeRevision = 0;
    U64 mGateRevision = 0;
    U32 mCaptureCount = 0;
    U32 mDisplayCount = 0;
    U32 mProjectorCount = 0;
    std::array<CaptureDefinition, MAX_CAPTURES> mCaptures;
    std::array<DisplayDefinition, MAX_DISPLAY_BINDINGS> mDisplays;
    std::array<ProjectorDefinition, MAX_PROJECTOR_BINDINGS> mProjectors;
};

// Coalesces nested registry mutations into one publication per revision kind.
// This is public so higher-level multi-capture operations can wrap the
// individually-scoped registry calls without exposing registry internals.
class ScopedRevisionSignals
{
public:
    ScopedRevisionSignals();
    ~ScopedRevisionSignals();

    ScopedRevisionSignals(const ScopedRevisionSignals&) = delete;
    ScopedRevisionSignals& operator=(const ScopedRevisionSignals&) = delete;
};

enum class EDesignationResult
{
    ELIGIBLE,
    ADDED,
    ALREADY_EXISTS,
    INVALID_SELECTION,
    AT_CAPACITY
};

struct Designation
{
    U32 mSlot = MAX_LENSES;
    LLUUID mObjectId;
    S32 mTextureEntry = -1;
};

struct CompositeState
{
    U32 mCaptureSlot = MAX_CAPTURES; // Output owner, never a display index.
    LLFace* mFace = nullptr; // Valid only for the current call/frame.
    // Prim-free "virtual screen". When mVirtual is set, mFace is null and the
    // pipeline draws mVirtualCorners (a world/agent-space quad: TL, TR, BR, BL)
    // under gPrismLensProgram with the base world modelview instead of pushing a
    // face matrix and calling renderIndexed(). The surface uniforms below are
    // synthesized in the SAME world/agent space as the corners, so the composite
    // shader is reused unchanged. Default false => a real-face state is
    // byte-identical to before.
    bool mVirtual = false;
    F32 mVirtualCorners[4][3] = { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f },
                                  { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } };
    F32 mSurfaceOrigin[3] = { 0.f, 0.f, 0.f };
    F32 mSurfaceUDual[3] = { 0.f, 0.f, 0.f };
    F32 mSurfaceVDual[3] = { 0.f, 0.f, 0.f };

    // Per-display mapping. FIT may deliberately produce logical UVs outside
    // [0, 1], which the fragment shader turns into the configured bar color.
    F32 mDisplayToCaptureScale[2] = { 1.f, 1.f };
    F32 mDisplayToCaptureOffset[2] = { 0.f, 0.f };
    S32 mLetterbox = 0;
    F32 mBarColorLinear[3] = { 0.f, 0.f, 0.f };

    // Capture-owned publication metadata. Lens orientation is frozen when the
    // retained image is published; Camera Feed uses the identity transform.
    F32 mRetainedOrientationScale[2] = { 1.f, 1.f };
    F32 mRetainedOrientationOffset[2] = { 0.f, 0.f };

    // Maps canonical capture UVs into the logical subrectangle of the fixed-
    // capacity retained texture. This is independent of physical allocation.
    F32 mTextureRegionScale[2] = { 1.f, 1.f };
    F32 mTextureRegionOffset[2] = { 0.f, 0.f };

    S32 mScissor[4] = { 0, 0, 0, 0 };
    F32 mEdgeFeather = 0.f;

    // Option C: Cinematic optics parameters passed to compositor
    // x: Chromatic Aberration, y: Film Grain, z: CRT Scanlines, w: Exposure Bias
    F32 mOpticsParams[4] = { 0.f, 0.f, 0.f, 0.f };

    // Per-display TV/CRT screen effect uniform packs (6x vec4). Zero strength is
    // a strict shader no-op; see ScreenEffects.
    F32 mScreenEffect0[4] = { 0.f, 0.f, 0.f, 0.f }; // Scanlines, ScanlineCount, Pixelate, Grayscale
    F32 mScreenEffect1[4] = { 0.f, 0.f, 0.f, 0.f }; // Sepia, Static, VerticalRoll, RollSpeed
    F32 mScreenEffect2[4] = { 0.f, 0.f, 0.f, 0.f }; // Tracking, Flicker, ChromaBleed, Vignette
    F32 mScreenEffect3[4] = { 0.f, 0.f, 0.f, 0.f }; // Interlace, Dropout, Brightness, Reserved
    // Feature A + B orientation/sheen pack. All-zero is a strict shader no-op.
    F32 mScreenEffect4[4] = { 0.f, 0.f, 0.f, 0.f }; // FlipH, FlipV, Rotate90, Sheen
    F32 mScreenEffect5[4] = { 0.45f, 0.f, 0.5f, 0.f }; // [PrismScanlines] Depth, Sharpness, Thickness, Reserved
};

// Viewer-local registry. No call writes prim, TE, or material data and no call
// sends an object update to the simulator.
ActionStatus addCameraSelectionStatus();
ActionStatus addLensSelectionStatus();
ActionStatus addDisplaySelectionStatus(const CaptureHandle& capture);
ActionStatus displayTargetStatus(const CaptureHandle& capture,
                                 LLViewerObject* object, S32 te,
                                 const DisplayHandle* reassigning = nullptr);
ActionStatus setCameraSelectionStatus(const CaptureHandle& capture);
ERegistryResult addCameraCaptureFromSelectedObject(
    CaptureHandle* capture, std::string* reason = nullptr);
// Prim-free camera: allocate a CAMERA_FEED capture anchored to a stored world
// transform (AGENT space; forward = local -Z, up = local +Y) instead of an
// in-world object. No selection, no rezzed prim, no eligibility check. The
// capture's mCameraObjectId stays null and mCamera.mVirtual is set true.
ERegistryResult addVirtualCamera(CaptureHandle* capture, const LLVector3& pos,
                                 const LLQuaternion& rot,
                                 std::string* reason = nullptr);
// Build and arm a matched reciprocal OTS pair from Director Subjects A/B.
// Both virtual eyes stay on the same side of the action axis. Geometry is
// evaluated only when invoked; the resulting captures are fixed AGENT-space
// transforms and require no per-frame work.
bool buildOtsPairFromSubjects(CaptureHandle* ots_a = nullptr,
                              CaptureHandle* ots_b = nullptr,
                              std::string* reason = nullptr);
ERegistryResult addSurfaceLensFromSelectedFace(
    CaptureHandle* capture, std::string* reason = nullptr);
ERegistryResult addSelectedDisplay(const CaptureHandle& capture, EFitMode fit,
                                   DisplayHandle* binding,
                                   std::string* reason = nullptr);
ERegistryResult bindDisplayTarget(const CaptureHandle& capture,
                                  LLViewerObject* object, S32 te, EFitMode fit,
                                  DisplayHandle* binding,
                                  std::string* reason = nullptr);
ERegistryResult reassignDisplay(const DisplayHandle& binding,
                                const CaptureHandle& capture,
                                std::string* reason = nullptr);
ActionStatus projectorTargetStatus(const CaptureHandle& capture,
                                   LLViewerObject* object,
                                   const ProjectorHandle* reassigning = nullptr);
ERegistryResult bindProjector(const CaptureHandle& capture,
                              LLViewerObject* object,
                              const ProjectorSettings& settings,
                              ProjectorHandle* binding,
                              std::string* reason = nullptr);
ERegistryResult bindRigProjector(const CaptureHandle& capture,
                                 U8 rig_slot, U8 rig_light,
                                 const ProjectorSettings& settings,
                                 ProjectorHandle* binding,
                                 std::string* reason = nullptr);
ERegistryResult reassignProjector(const ProjectorHandle& binding,
                                  const CaptureHandle& capture,
                                  std::string* reason = nullptr);
bool setProjectorSettings(const ProjectorHandle& binding,
                          const ProjectorSettings& settings,
                          std::string* reason = nullptr);
ERegistryResult setProjectorGateSubscribed(
    const ProjectorHandle& binding, bool subscribed,
    std::string* reason = nullptr);
bool removeProjector(const ProjectorHandle& binding,
                     std::string* reason = nullptr);
// Prim-free screen: allocate a faceless display bound to `capture`, drawn as a
// viewer quad at the stored world transform (AGENT space; right = local +X, up =
// local +Y) with width/height in metres, instead of a selected prim face. No
// selection, no rezzed prim, no eligibility check. The display's mObjectId stays
// null / mTE stays -1 and mSettings.mVirtual is set true. Counts toward the
// capture's display total exactly like a real display binding.
ERegistryResult addVirtualDisplay(const CaptureHandle& capture,
                                  const LLVector3& pos, const LLQuaternion& rot,
                                  F32 width, F32 height, DisplayHandle* binding,
                                  std::string* reason = nullptr);
bool setSelectedCamera(const CaptureHandle& capture,
                       std::string* reason = nullptr);
bool setCameraSettings(const CaptureHandle& capture,
                       const CameraSettings& settings,
                       std::string* reason = nullptr);
// Runtime-only bone-driver writes. These update the existing virtual camera
// fields and runtime revision without churning the configuration revision.
bool setVirtualCameraTransform(const CaptureHandle& capture,
                               const LLVector3& pos,
                               const LLQuaternion& rot,
                               F32 vertical_fov_rad,
                               std::string* reason = nullptr);
// Stabilized mode intentionally has no rotation parameter, making it impossible
// for that path to contend with operator-owned mVirtualRot.
bool setVirtualCameraPosition(const CaptureHandle& capture,
                              const LLVector3& pos,
                              F32 vertical_fov_rad,
                              std::string* reason = nullptr);
bool setCaptureRateSettings(const CaptureHandle& capture,
                            const CaptureRateSettings& settings,
                            std::string* reason = nullptr);
bool setCaptureLabel(const CaptureHandle& capture, const std::string& label,
                     std::string* reason = nullptr);
bool setDisplaySettings(const DisplayHandle& binding,
                        const DisplaySettings& settings,
                        std::string* reason = nullptr);
bool setGateSettings(const GateSettings& settings,
                     std::string* reason = nullptr);
ERegistryResult gateArm(const CaptureHandle& capture,
                        const std::string& label,
                        LLUUID* arm_id = nullptr,
                        std::string* reason = nullptr);
ERegistryResult gateDisarm(const LLUUID& arm_id,
                           std::string* reason = nullptr);
ERegistryResult gateTake(std::string* reason = nullptr);
ERegistryResult setDisplayGateSubscribed(const DisplayHandle& binding,
                                         bool subscribed,
                                         const CaptureHandle* fixed_capture = nullptr,
                                         std::string* reason = nullptr);
GateSnapshot gateSnapshot();
bool gateOnAirCameraEye(LLVector3& out_agent,
                        U64* out_cut_serial = nullptr,
                        LLQuaternion* out_rotation = nullptr);
bool removeDisplay(const DisplayHandle& binding,
                   std::string* reason = nullptr);
bool removeCapture(const CaptureHandle& capture);
U64 configurationRevision();
U64 runtimeRevision();
U64 performanceRevision();
RegistrySnapshot registrySnapshot();
PerformanceSnapshot performanceSnapshot();
LLSD sceneData();
ApplyReport applySceneData(const LLSD& data, std::string* reason = nullptr);

// VP2 consumption/demand seam. The pipeline stamps only main-view projector
// use; auxiliary and cube/probe renders never keep a camera alive.
void markProjectorConsumed(const LLVOVolume* volume);
bool getProjectorFeed(const ProjectorHandle& binding, ProjectorFeed& feed);
bool projectorFeedFor(const LLVOVolume* volume, ProjectorFeed& feed);

using revision_connection_t = boost::signals2::connection;
revision_connection_t connectConfigurationChanged(
    const std::function<void(U64)>& callback);
revision_connection_t connectRuntimeChanged(
    const std::function<void(U64)>& callback);
revision_connection_t connectPerformanceChanged(
    const std::function<void(U64)>& callback);

// Compatibility surface for the original Director Prism Lens controls.
EDesignationResult selectedFaceStatus(std::string* reason = nullptr);
bool canDesignateSelectedFace();
EDesignationResult designateSelectedFace(std::string* reason = nullptr);
bool removeDesignation(U32 slot);
void clearDesignations();
bool hasDesignation();
U32 designationCount();
U32 designationRevision();
bool getDesignation(U32 slot, Designation& designation);

// Prepare all visible lenses and render at most one off-axis view this frame.
void renderAuxiliaryView();

// Render-only camera guides: a Blender-style wireframe frustum gizmo drawn per
// CAMERA_FEED capture that has its per-camera guide enabled. Client-side draw
// only (gUIProgram / immediate-mode lines and triangles in world coordinates);
// no prim, render target, shader, or allocation. Cheap early-out when the
// global kill-switch is off or no camera has its guide on. Must be called from
// the 2.5D UI overlay pass (render_ui_3d) after gUIProgram is bound and inside
// the UI-visibility gate, so it hides while filming.
void renderCameraGuides();

// Pipeline/context teardown notification. Clears publication/runtime ownership
// only; the caller has already released the GL targets.
void onRenderTargetsReleased();

// Resolve current-frame faces for all valid retained-output HDR composites.
U32 getCompositeStates(LLRenderTarget* screen_target, CompositeState* states,
                       U32 capacity);

// [Prism camera feed - recursive mirror] Auxiliary-capture twin of
// getCompositeStates. Runs ONLY inside the Prism aux (VCam) render
// (sPrismLensRender == true) and only into the active aux pack screen, letting
// display faces composite the PREVIOUS frame's retained feed while the aux draws
// the current frame - a stable, 1-frame-lagged recursive mirror. Kept separate so
// the main-view composite path (getCompositeStates) stays byte-identical; the
// pipeline selects this variant only when sPrismLensRender is set.
U32 getAuxCompositeStates(LLRenderTarget* screen_target, CompositeState* states,
                          U32 capacity);

// VC3 monitor compositor. Raw uses a null display handle. A selected real
// face must have a resolved basis; otherwise offline is returned and callers
// draw a slate rather than guessing an aspect or sheen input.
bool buildPreviewCompositeState(const CaptureHandle& capture,
                                const DisplayHandle& display,
                                F32 display_aspect,
                                CompositeState& state,
                                bool& offline);
bool buildProjectorCompositeState(const ProjectorHandle& projector,
                                  CompositeState& state,
                                  U32& target_width,
                                  U32& target_height);
bool previewTileActivity(const CaptureHandle& capture,
                         EActivityState& activity,
                         bool& healthy);
U32 captureSlot(const CaptureHandle& capture);

// Plane whose non-negative half-space is behind the lens, in agent space.
bool getActiveClipPlane(LLPlane& plane);

// Copy the most recently refreshed retained beauty into the debug rectangle.
void compositeDebug();
}

#endif // LL_LLPRISMLENS_H
