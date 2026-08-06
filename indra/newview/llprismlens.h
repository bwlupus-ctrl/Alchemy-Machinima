/**
 * @file llprismlens.h
 * @brief Bounded Prism Lens and virtual-camera surface feeds.
 */

#ifndef LL_LLPRISMLENS_H
#define LL_LLPRISMLENS_H

#include "stdtypes.h"
#include "llmath.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3math.h"

#include <array>
#include <string>

class LLFace;
class LLPlane;
class LLRenderTarget;
namespace LLPrismLens
{
// A capture owns an auxiliary render target. A display binding owns only a
// destination face and samples its capture's retained target. Keeping these
// limits separate is what makes one camera -> many faces inexpensive.
constexpr U32 MAX_CAPTURES = 3;
constexpr U32 MAX_DISPLAY_BINDINGS = 16;
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

struct CameraSettings
{
    EFovMode mFovMode = EFovMode::FIXED;
    F32 mFixedVerticalFovRad = 60.f * DEG_TO_RAD;
    F32 mNearClip = 0.05f;
    F32 mFarClip = 256.f;
    LLVector3 mLocalEyeOffset;
    F32 mOutputAspect = 16.f / 9.f;
    OpticsSettings mOptics;
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
// mScanlineCount and mRollSpeed are shaping parameters that only matter while
// their gating strength (mScanlines / mVerticalRoll) is non-zero, which is
// why they carry non-zero defaults and are excluded from isZero().
struct ScreenEffects
{
    F32 mScanlines      = 0.f;  // 0..1 scanline blend strength
    F32 mScanlineCount  = 0.5f; // 0..1 line density (0.5 = mid density)
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
        mScanlineCount = llclamp(mScanlineCount, 0.f, 1.f);
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
};

struct CaptureDefinition
{
    U32 mSlot = MAX_CAPTURES;
    CaptureHandle mHandle;
    ECaptureMode mMode = ECaptureMode::SURFACE_LENS;
    LLUUID mCameraObjectId;
    CameraSettings mCamera;
    CaptureRateSettings mRate;
    CaptureRuntimeState mRuntime;
    U32 mDisplayCount = 0;
};

struct DisplayDefinition
{
    DisplayHandle mHandle;
    CaptureHandle mCapture;
    LLUUID mDisplayObjectId;
    S32 mDisplayTextureEntry = -1;
    DisplaySettings mSettings;
    DisplayRuntimeState mRuntime;
};

struct RegistrySnapshot
{
    U64 mConfigurationRevision = 0;
    U64 mRuntimeRevision = 0;
    U32 mCaptureCount = 0;
    U32 mDisplayCount = 0;
    std::array<CaptureDefinition, MAX_CAPTURES> mCaptures;
    std::array<DisplayDefinition, MAX_DISPLAY_BINDINGS> mDisplays;
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

    // Per-display TV/CRT screen effect uniform packs (4x vec4). All-zero is
    // a strict shader no-op; see ScreenEffects.
    F32 mScreenEffect0[4] = { 0.f, 0.f, 0.f, 0.f }; // Scanlines, ScanlineCount, Pixelate, Grayscale
    F32 mScreenEffect1[4] = { 0.f, 0.f, 0.f, 0.f }; // Sepia, Static, VerticalRoll, RollSpeed
    F32 mScreenEffect2[4] = { 0.f, 0.f, 0.f, 0.f }; // Tracking, Flicker, ChromaBleed, Vignette
    F32 mScreenEffect3[4] = { 0.f, 0.f, 0.f, 0.f }; // Interlace, Dropout, Brightness, Reserved
    // Feature A + B orientation/sheen pack. All-zero is a strict shader no-op.
    F32 mScreenEffect4[4] = { 0.f, 0.f, 0.f, 0.f }; // FlipH, FlipV, Rotate90, Sheen
};

// Viewer-local registry. No call writes prim, TE, or material data and no call
// sends an object update to the simulator.
ActionStatus addCameraSelectionStatus();
ActionStatus addLensSelectionStatus();
ActionStatus addDisplaySelectionStatus(const CaptureHandle& capture);
ActionStatus setCameraSelectionStatus(const CaptureHandle& capture);
ERegistryResult addCameraCaptureFromSelectedObject(
    CaptureHandle* capture, std::string* reason = nullptr);
ERegistryResult addSurfaceLensFromSelectedFace(
    CaptureHandle* capture, std::string* reason = nullptr);
ERegistryResult addSelectedDisplay(const CaptureHandle& capture, EFitMode fit,
                                   DisplayHandle* binding,
                                   std::string* reason = nullptr);
bool setSelectedCamera(const CaptureHandle& capture,
                       std::string* reason = nullptr);
bool setCameraSettings(const CaptureHandle& capture,
                       const CameraSettings& settings,
                       std::string* reason = nullptr);
bool setCaptureRateSettings(const CaptureHandle& capture,
                            const CaptureRateSettings& settings,
                            std::string* reason = nullptr);
bool setDisplaySettings(const DisplayHandle& binding,
                        const DisplaySettings& settings,
                        std::string* reason = nullptr);
bool removeDisplay(const DisplayHandle& binding,
                   std::string* reason = nullptr);
bool removeCapture(const CaptureHandle& capture);
U64 configurationRevision();
U64 runtimeRevision();
RegistrySnapshot registrySnapshot();
PerformanceSnapshot performanceSnapshot();
LLSD sceneData();
bool applySceneData(const LLSD& data, std::string* reason = nullptr);

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

// Plane whose non-negative half-space is behind the lens, in agent space.
bool getActiveClipPlane(LLPlane& plane);

// Copy the most recently refreshed retained beauty into the debug rectangle.
void compositeDebug();
}

#endif // LL_LLPRISMLENS_H
