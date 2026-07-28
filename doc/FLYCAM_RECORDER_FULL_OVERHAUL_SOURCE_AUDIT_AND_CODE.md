# Flycam Recorder Full Overhaul

## Source Audit, True Avatar-Relative Recording, World Placement, Editing Architecture, Deterministic Capture, Interchange, and Implementation Code

**Target:** Alchemy-Machinima  
**Existing feature:** `LLFlycamRecorder`, `ALPanelFlycamRecorder`, Director Console Takes integration  
**Current take format:** LLSD-XML version 2  
**Primary requirement:** retain ordinary world-space camera recording while adding genuinely avatar-relative recording that can be instantiated as a fixed world move around an avatar, live-follow an avatar, reconstruct the original world take, or be baked into a new world-space path  
**Implementation strategy:** preserve the working recorder façade and shared panel; evolve the internal take, anchor, evaluator, clock, editing, and persistence layers incrementally  
**Status:** source-grounded architecture and coding handoff; code is implementation-grade but still requires reconciliation with the exact local branch and in-viewer validation

---

# Executive verdict

The current Flycam Recorder is a legitimate and useful foundation. It already gets several difficult decisions right:

- it records the **final `LLViewerCamera`**, rather than joystick input;
- it uses global-double camera positions;
- it preserves full quaternion camera orientation;
- it records vertical FOV;
- it samples after the active camera provider has run;
- it supports smooth position and rotation interpolation;
- it already has transport, looping, ping-pong, scrubbing, save/load, a handheld operator layer, Director Console integration, and a shared XUI panel;
- its version-2 take format is hand-editable.

The overhaul should therefore **not** discard `LLFlycamRecorder` or replace it with a disconnected greenfield subsystem.

The correct direction is:

```text
Keep:
    LLFlycamRecorder public façade
    existing transport buttons
    shared ALPanelFlycamRecorder
    final-camera recording hook
    current idle camera-dispatch branch
    existing v2 loader
    current operator integration as an optional layer

Refactor internally:
    Keyframe vector
        ↓
    versioned LLCameraTake model
        ↓
    stable anchor binding + resolver
        ↓
    true per-sample relative recording
        ↓
    absolute-time evaluator
        ↓
    robust I/O and editor model
```

The most important source finding is this:

> The current “relative playback” is a placement transform applied to world-space keys against one fixed recording anchor. It is not true relative recording.

That distinction matters whenever the recorded avatar moves or rotates during the take.

Current behavior is conceptually:

```text
record:
    store camera_world(t)

play:
    output =
        live_anchor_now
        + rotate_yaw(
            camera_world(t) - record_anchor_at_start)
```

Correct true-relative behavior is:

```text
record:
    camera_local(t) =
        inverse(record_anchor(t))
        × camera_world(t)

play:
    output_world(t) =
        chosen_playback_anchor(t or start)
        × camera_local(t)
```

With the current implementation, movement of the original subject remains embedded in the world-space camera keys. When replayed with live follow, the new subject motion may be added on top of the original subject motion. This can create drift, doubled translation, wrong orbit centers, or a move that only works when the original recording anchor was static.

The production design needs to separate four concepts:

```text
1. Stored camera space
       WORLD
       ANCHOR_LOCAL

2. Recorded anchor source
       self
       selected avatar
       selected object
       Ghost Studio actor
       manual frame
       camera-at-record-start

3. Playback placement
       original world
       fixed around anchor at playback start
       live-follow anchor
       recorded anchor trajectory
       manual frame

4. Anchor orientation policy
       translation only
       translation + yaw
       full orientation
```

For the requested primary workflow, the recommended defaults are:

```text
Record Space:
    Anchor Local

Recorded Anchor:
    Selected Avatar or Ghost Actor

Playback Placement:
    Around Anchor at Playback Start

Anchor Orientation:
    Translation + Yaw

Result:
    The camera move is placed as an ordinary fixed world-space move
    around the avatar's position and heading at the moment playback starts.
```

For a camera that must ride a moving actor:

```text
Playback Placement:
    Live Follow

Anchor Orientation:
    Translation + Yaw
```

---

# 1. Audited current architecture

The supplied source brief contains the complete current implementation of:

```text
indra/newview/llflycamrecorder.h
indra/newview/llflycamrecorder.cpp
indra/newview/alpanelflycamrecorder.h
indra/newview/alpanelflycamrecorder.cpp
indra/newview/skins/default/xui/en/panel_flycam_recorder.xml
the LLAppViewer::idle() camera-dispatch seam
```

## 1.1 Current keyframe

```cpp
struct Keyframe
{
    F32          mTime = 0.f;
    LLVector3d   mPosGlobal;
    LLQuaternion mRot;
    F32          mFov = 1.f;
};
```

## 1.2 Current recording

The recorder samples the final render camera:

```cpp
void LLFlycamRecorder::sampleCamera(F32 time)
{
    LLViewerCamera* cam = LLViewerCamera::getInstance();

    Keyframe key;
    key.mTime = time;
    key.mPosGlobal =
        gAgent.getPosGlobalFromAgent(cam->getOrigin());
    key.mRot = cam->getQuaternion();
    key.mFov = cam->getView();

    // Quaternion hemisphere continuity and duplicate-time guard.
    ...
}
```

This is the right source to record. It captures whichever subsystem actually drove the camera.

## 1.3 Current sampling schedule

```cpp
void LLFlycamRecorder::onIdleFrame()
{
    if (mState != STATE_RECORDING)
    {
        return;
    }

    const F32 interval =
        1.f / llclamp(sample_rate, 1.f, 120.f);

    const F32 t =
        mRecordTimer.getElapsedTimeF32();

    if (mKeys.empty() ||
        t - mKeys.back().mTime >= interval)
    {
        sampleCamera(t);
    }
}
```

This captures at most one key per rendered frame.

## 1.4 Current smooth evaluation

```text
position:
    uniform Catmull-Rom

rotation:
    hand-written quaternion SQUAD

FOV:
    linear

linear fallback:
    position lerp + quaternion nlerp
```

## 1.5 Current anchor behavior

The take stores one recorded anchor:

```text
record anchor position
record anchor yaw
```

The live anchor is:

```text
self
current selection
camera at playback
```

It can be latched once or refreshed every frame.

The output uses:

```cpp
const F32 dyaw =
    live_yaw - ref_yaw;

const LLQuaternion rz(
    dyaw,
    LLVector3(0.f, 0.f, 1.f));

offset =
    camera_world -
    recorded_reference_position;

offset = offset * rz;

output_position =
    live_anchor_position + offset;

output_rotation =
    recorded_world_rotation * rz;
```

## 1.6 Current playback clock

```cpp
const F32 dt =
    llclamp(
        gFrameIntervalSeconds.value(),
        0.0005f,
        0.25f);

mPlayhead +=
    dt *
    speed *
    mPlayDir;
```

## 1.7 Current camera write

The source writes:

```cpp
cam->setView(fov * fov_mul);
cam->setOrigin(out_pos);
cam->mXAxis = ...;
cam->mYAxis = ...;
cam->mZAxis = ...;
```

## 1.8 Current persistence

Version 2 stores:

```text
version
region
anchor_pos
anchor_yaw
keyframes:
    t
    pos
    rot
    fov
```

The loader sorts by time, drops equal timestamps, normalizes quaternions, canonicalizes quaternion signs, and derives a missing anchor from the first key.

---

# 2. Severity-ranked source findings

| Rank | Finding | Severity | Required action |
|---:|---|---|---|
| 1 | Relative playback is not true per-sample relative recording | Critical feature blocker | Store camera pose in the anchor frame at each sample |
| 2 | Recording anchor is always self or first camera, not the explicitly intended subject | Critical feature blocker | Capture and serialize an explicit stable recording-anchor binding |
| 3 | Selection-follow re-reads mutable viewer selection and can silently fall back to self | High | Lock stable identity at start; never use silent fallback |
| 4 | Playback calls `LLViewerCamera::setView()`, whose public contract broadcasts FOV | High side effect | Use `setViewNoBroadcast()` |
| 5 | Integrated `dt` plus a 0.25-second clamp makes playback lose time after hitches | Critical deterministic-capture blocker | Evaluate from an absolute selected clock |
| 6 | Recording density is bounded by live FPS and hitches create unmarked sample gaps | High | Record every rendered frame with F64 time; resample after recording |
| 7 | `F32` time and playhead lose precision during long takes | High for long-form capture | Change take and transport time to `F64` |
| 8 | Uniform Catmull-Rom ignores nonuniform key timing and can overshoot | High motion-quality issue | Use time-aware Hermite/centripetal evaluation |
| 9 | Current SQUAD controls ignore nonuniform timing and raw quaternion math lacks complete malformed-input containment | High | Normalize, canonicalize, cache robust controls, test edge cases |
| 10 | Linear key lookup is O(N) every frame | Medium, high for large takes | Use binary search plus a monotonic segment cache |
| 11 | Anchor loss has no explicit state or policy | High | Pause, hold, freeze frame, stop, or deliberate fallback |
| 12 | Remote-avatar live follow inherits network/render jitter | High for capture | Smooth only playback frame or use recorded actor/ghost trajectory |
| 13 | Operator velocity is finite-differenced after placement and uses Euler delta | Medium-high | Use evaluator derivatives; reset on all discontinuities |
| 14 | Ping-pong reversal does not clearly reset operator state | Medium-high | Raise reversal event or provide zero endpoint tangent |
| 15 | FOV and quaternion load values lack a fully documented finite/range gate | High malformed-file risk | Validate before normalization/evaluation |
| 16 | Save is not atomic and load needs explicit size/sample/duration caps | High persistence risk | Temporary parse, validation, transactional swap, atomic write |
| 17 | Global saved settings determine take behavior after load | Medium reproducibility risk | Store per-take playback/edit settings; globals become defaults |
| 18 | Camera ownership is an implicit priority chain | Medium-high integration risk | Preserve now; later introduce a small coordinator/lease |
| 19 | Camera cuts, seeks, target rebinds, and loop jumps do not expose a shared temporal-reset event | High TAA/motion risk | Publish camera discontinuity serial/reason |
| 20 | Current v2 cannot reconstruct a moving recorded anchor trajectory | Permanent data limitation | v3 records anchor-local keys and optional anchor track |

---

# 3. Critical finding: why current anchoring is insufficient

Assume the recording avatar walks 10 meters while the operator keeps the camera 3 meters behind it.

Current world key:

```text
camera_world(t) =
    avatar_world(t) +
    camera_offset(t)
```

Current v2 relative placement subtracts only the anchor at recording start:

```text
stored_effective_offset(t) =
    camera_world(t) -
    avatar_world(0)
```

Substitute:

```text
stored_effective_offset(t) =
    avatar_world(t) -
    avatar_world(0) +
    camera_offset(t)
```

The original avatar translation is now part of the camera path.

If playback uses a fixed new anchor:

```text
output(t) =
    new_anchor_at_start +
    original_avatar_translation(t) +
    camera_offset(t)
```

The result drifts through the original subject movement.

If playback live-follows another moving avatar:

```text
output(t) =
    new_avatar_world(t) +
    original_avatar_translation(t) +
    camera_offset(t)
```

The original and new subject translations are both present.

The correct local recording is:

```text
camera_local(t) =
    inverse(anchor_world(t)) × camera_world(t)
```

Then fixed placement is:

```text
output(t) =
    new_anchor_world(play_start) × camera_local(t)
```

And live-follow is:

```text
output(t) =
    new_anchor_world(t) × camera_local(t)
```

This is not a cosmetic distinction. It is the difference between:

```text
“Move this exact world camera path somewhere else”
```

and:

```text
“Record the camera’s relationship to this performer”
```

Both are useful and must remain separately available.

---

# 4. Product contract

## 4.1 Storage spaces

```cpp
enum class EFlycamSpace : U8
{
    WORLD_GLOBAL = 0,
    ANCHOR_LOCAL = 1
};
```

## 4.2 Playback placement

```cpp
enum class EFlycamPlacement : U8
{
    // For world keys, use the stored world pose.
    ORIGINAL_WORLD = 0,

    // Resolve one target frame when playback begins, then hold it.
    ANCHOR_AT_PLAYBACK_START = 1,

    // Resolve the target every evaluation.
    LIVE_ANCHOR = 2,

    // Reapply the anchor trajectory captured during recording.
    RECORDED_ANCHOR_TRACK = 3,

    // Use a user-supplied fixed transform.
    MANUAL_FRAME = 4
};
```

## 4.3 Anchor orientation

```cpp
enum class EFlycamAnchorOrientation : U8
{
    TRANSLATION_ONLY = 0,
    TRANSLATION_AND_YAW = 1,
    FULL_ORIENTATION = 2
};
```

Recommended avatar default:

```text
TRANSLATION_AND_YAW
```

Recommended object-rig default:

```text
FULL_ORIENTATION
```

## 4.4 Anchor loss

```cpp
enum class EFlycamAnchorLossPolicy : U8
{
    PAUSE_AND_WAIT = 0,
    HOLD_LAST_WORLD_CAMERA = 1,
    FREEZE_LAST_ANCHOR_FRAME = 2,
    STOP_AND_RELEASE_CAMERA = 3,
    EXPLICIT_FALLBACK_TO_SELF = 4
};
```

There must be no implicit fallback.

## 4.5 Recording modes

```cpp
enum class EFlycamRecordEditMode : U8
{
    REPLACE_TAKE = 0,
    APPEND_AFTER_END = 1,
    INSERT_AND_SHIFT = 2,
    PUNCH_OVERWRITE_RANGE = 3,
    RECORD_NEW_LAYER = 4
};
```

---

# 5. Incremental internal architecture

Preserve the public singleton:

```cpp
LLFlycamRecorder::instance()
```

Preserve the existing panel-facing transport methods initially.

Move data and algorithms behind the façade:

```text
LLFlycamRecorder
    ├── LLFlycamTake
    ├── LLFlycamTransport
    ├── LLFlycamEvaluator
    ├── LLFlycamAnchorResolver
    ├── LLFlycamRecordingSession
    ├── LLFlycamTakeIO
    └── LLFlycamEditHistory
```

Recommended new files:

```text
indra/newview/llflycamtake.h
indra/newview/llflycamtake.cpp

indra/newview/llflycamanchor.h
indra/newview/llflycamanchor.cpp

indra/newview/llflycamevaluator.h
indra/newview/llflycamevaluator.cpp

indra/newview/llflycamtime.h
indra/newview/llflycamtime.cpp

indra/newview/llflycamtakeio.h
indra/newview/llflycamtakeio.cpp

indra/newview/llflycameditmodel.h
indra/newview/llflycameditmodel.cpp

indra/newview/altoolflycampathedit.h
indra/newview/altoolflycampathedit.cpp

indra/newview/alflycamtimeline.h
indra/newview/alflycamtimeline.cpp
```

Do not require all files in the first patch. The initial migration may keep helpers in `llflycamrecorder.cpp`, then extract after tests exist.

---

# 6. Take format version 3

## 6.1 Stable key identity

Vector indices are not stable under insert/delete/retime. Editing requires a key ID.

```cpp
struct LLFlycamKeyID
{
    LLUUID value;
};
```

## 6.2 Lens data

```cpp
struct LLFlycamLens
{
    F32 mVerticalFov = 1.f;

    F32 mFocusDistance = 0.f;
    F32 mApertureFStop = 0.f;
    F32 mFocalLengthMM = 0.f;
    F32 mSensorHeightMM = 0.f;

    bool mHasFocusDistance = false;
    bool mHasAperture = false;
    bool mHasFocalLength = false;
    bool mHasSensorHeight = false;
};
```

## 6.3 Per-key interpolation

```cpp
enum class EFlycamInterpolation : U8
{
    HOLD = 0,
    LINEAR = 1,
    SMOOTH_AUTO = 2,
    CUBIC_FREE = 3
};

enum class EFlycamEase : U8
{
    NONE = 0,
    EASE_IN = 1,
    EASE_OUT = 2,
    EASE_IN_OUT = 3,
    CUSTOM_BEZIER = 4
};
```

## 6.4 Keyframe

For a world take, `mPosition` and `mRotation` are world values.

For an anchor-local take, they are anchor-local values.

```cpp
struct LLFlycamKeyframe
{
    LLUUID mID;

    F64 mTime = 0.0;

    LLVector3d mPosition;
    LLQuaternion mRotation;

    LLFlycamLens mLens;

    EFlycamInterpolation mPositionInterpolation =
        EFlycamInterpolation::SMOOTH_AUTO;

    EFlycamInterpolation mRotationInterpolation =
        EFlycamInterpolation::SMOOTH_AUTO;

    EFlycamInterpolation mLensInterpolation =
        EFlycamInterpolation::LINEAR;

    EFlycamEase mEaseOut =
        EFlycamEase::NONE;

    // Used only for CUBIC_FREE.
    LLVector3d mPositionTangentIn;
    LLVector3d mPositionTangentOut;

    // Optional editor metadata.
    std::string mLabel;
    bool mProtected = false;

    // Temporal boundaries.
    bool mCutBefore = false;
    bool mGapBefore = false;
};
```

## 6.5 Anchor binding

```cpp
enum class EFlycamAnchorKind : U8
{
    NONE = 0,
    SELF_AVATAR = 1,
    AVATAR_UUID = 2,
    OBJECT_ROOT_UUID = 3,
    GHOST_STUDIO_INSTANCE = 4,
    CAMERA_FRAME = 5,
    MANUAL_FRAME = 6
};

enum class EFlycamAvatarAnchorPoint : U8
{
    ROOT = 0,
    FEET = 1,
    PELVIS = 2,
    CHEST = 3,
    HEAD = 4,
    CUSTOM_LOCAL_OFFSET = 5
};

struct LLFlycamAnchorBinding
{
    EFlycamAnchorKind mKind =
        EFlycamAnchorKind::NONE;

    // Stable live-object identity.
    LLUUID mObjectID;

    // Stable Ghost Studio project/instance identity.
    LLUUID mGhostInstanceID;

    EFlycamAvatarAnchorPoint mAvatarPoint =
        EFlycamAvatarAnchorPoint::ROOT;

    EFlycamAnchorOrientation mOrientationMode =
        EFlycamAnchorOrientation::
            TRANSLATION_AND_YAW;

    LLVector3 mCustomOffsetLocal;

    EFlycamAnchorLossPolicy mLossPolicy =
        EFlycamAnchorLossPolicy::
            PAUSE_AND_WAIT;
};
```

## 6.6 Anchor frame

```cpp
struct LLFlycamAnchorFrame
{
    LLVector3d mPositionGlobal;
    LLQuaternion mRotationWorld;

    LLUUID mResolvedRuntimeID;

    U64 mRevision = 0;
    bool mValid = false;
    bool mDiscontinuous = false;
};
```

## 6.7 Optional recorded anchor track

```cpp
struct LLFlycamAnchorSample
{
    F64 mTime = 0.0;

    LLVector3d mPositionGlobal;
    LLQuaternion mRotationWorld;

    LLUUID mRuntimeID;

    bool mDiscontinuity = false;
};
```

## 6.8 Take

```cpp
struct LLFlycamTake
{
    static constexpr S32 FORMAT_VERSION = 3;

    LLUUID mTakeID;
    std::string mName;
    std::string mDescription;

    EFlycamSpace mSpace =
        EFlycamSpace::WORLD_GLOBAL;

    EFlycamPlacement mDefaultPlacement =
        EFlycamPlacement::ORIGINAL_WORLD;

    LLFlycamAnchorBinding mRecordedAnchor;

    std::vector<LLFlycamKeyframe> mKeys;

    // Optional but recommended for every anchor-local live recording.
    std::vector<LLFlycamAnchorSample> mRecordedAnchorTrack;

    // Per-take reproducibility settings.
    F64 mNominalSampleRate = 30.0;
    F64 mDefaultSpeed = 1.0;
    S32 mDefaultLoopMode = 0;
    bool mDefaultUseOperator = false;

    // Export/local-origin metadata.
    LLVector3d mReferenceOriginGlobal;

    U64 mEditRevision = 0;
};
```

---

# 7. Version-3 LLSD example

```xml
<llsd>
  <map>
    <key>format</key>
    <string>alchemy.flycam_take</string>

    <key>version</key>
    <integer>3</integer>

    <key>take_id</key>
    <uuid>...</uuid>

    <key>name</key>
    <string>Hero Orbit</string>

    <key>space</key>
    <string>anchor_local</string>

    <key>default_placement</key>
    <string>anchor_at_playback_start</string>

    <key>record_anchor</key>
    <map>
      <key>kind</key>
      <string>ghost_studio_instance</string>

      <key>ghost_instance_id</key>
      <uuid>...</uuid>

      <key>avatar_point</key>
      <string>chest</string>

      <key>orientation</key>
      <string>translation_and_yaw</string>

      <key>loss_policy</key>
      <string>pause_and_wait</string>
    </map>

    <key>keys</key>
    <array>
      <map>
        <key>id</key>
        <uuid>...</uuid>

        <key>t</key>
        <real>0.0</real>

        <key>pos</key>
        <array>
          <real>-3.0</real>
          <real>0.5</real>
          <real>1.8</real>
        </array>

        <key>rot</key>
        <array>
          <real>0.0</real>
          <real>0.0</real>
          <real>0.0</real>
          <real>1.0</real>
        </array>

        <key>fov</key>
        <real>0.82</real>

        <key>pos_interp</key>
        <string>smooth_auto</string>

        <key>rot_interp</key>
        <string>smooth_auto</string>

        <key>label</key>
        <string>Start behind hero</string>
      </map>
    </array>
  </map>
</llsd>
```

---

# 8. Stable selection binding

## 8.1 Current problem

In selection mode, the current resolver asks the selection manager for the current primary object each time follow mode refreshes.

This means:

```text
change selection during playback
    → camera may jump to another target

clear selection
    → current code may silently resolve self

selected object disappears
    → behavior depends on whether the selection still returns a dead object
```

The comment that a resolution failure preserves the prior latch does not cover the silent self fallback when no selected target remains.

## 8.2 Capture selection once

```cpp
bool LLFlycamRecorder::captureAnchorBindingFromSelection(
    LLFlycamAnchorBinding& out_binding)
{
    LLObjectSelectionHandle selection =
        LLSelectMgr::getInstance()->getSelection();

    if (!selection)
    {
        return false;
    }

    LLViewerObject* selected =
        selection->getPrimaryObject();

    if (!selected || selected->isDead())
    {
        return false;
    }

    if (LLVOAvatar* avatar =
            selected->getAvatar())
    {
        out_binding.mKind =
            EFlycamAnchorKind::AVATAR_UUID;

        out_binding.mObjectID =
            avatar->getID();

        return true;
    }

    LLViewerObject* root =
        selected->getRootEdit();

    if (!root || root->isDead())
    {
        return false;
    }

    out_binding.mKind =
        EFlycamAnchorKind::OBJECT_ROOT_UUID;

    out_binding.mObjectID =
        root->getID();

    return true;
}
```

For Ghost Studio, the UI should capture the stable Ghost Studio instance UUID through a dedicated adapter rather than infer it from a transient selected runtime object.

## 8.3 No silent fallback

```cpp
bool LLFlycamAnchorResolver::resolve(
    const LLFlycamAnchorBinding& binding,
    LLFlycamAnchorFrame& out_frame)
{
    switch (binding.mKind)
    {
        case EFlycamAnchorKind::SELF_AVATAR:
            return resolveSelf(binding, out_frame);

        case EFlycamAnchorKind::AVATAR_UUID:
            return resolveAvatar(
                binding.mObjectID,
                binding,
                out_frame);

        case EFlycamAnchorKind::OBJECT_ROOT_UUID:
            return resolveObject(
                binding.mObjectID,
                binding,
                out_frame);

        case EFlycamAnchorKind::GHOST_STUDIO_INSTANCE:
            return resolveGhostStudio(
                binding.mGhostInstanceID,
                binding,
                out_frame);

        case EFlycamAnchorKind::CAMERA_FRAME:
            return resolveCapturedCameraFrame(
                binding,
                out_frame);

        case EFlycamAnchorKind::MANUAL_FRAME:
            return resolveManualFrame(
                binding,
                out_frame);

        case EFlycamAnchorKind::NONE:
        default:
            return false;
    }
}
```

Fallback to self occurs only when the binding itself says:

```text
EXPLICIT_FALLBACK_TO_SELF
```

and only after the user has selected that policy.

---

# 9. Building an anchor frame

## 9.1 Orientation mode

```cpp
LLQuaternion LLFlycamAnchorResolver::makeAnchorRotation(
    const LLQuaternion& full_rotation,
    EFlycamAnchorOrientation mode)
{
    if (!full_rotation.isFinite())
    {
        return LLQuaternion::DEFAULT;
    }

    switch (mode)
    {
        case EFlycamAnchorOrientation::
            TRANSLATION_ONLY:
            return LLQuaternion::DEFAULT;

        case EFlycamAnchorOrientation::
            FULL_ORIENTATION:
        {
            LLQuaternion result = full_rotation;

            if (result.normalize() <= 0.f)
            {
                return LLQuaternion::DEFAULT;
            }

            return result;
        }

        case EFlycamAnchorOrientation::
            TRANSLATION_AND_YAW:
        default:
        {
            LLMatrix3 matrix(full_rotation);
            LLVector3 at(matrix.mMatrix[0]);

            at.mV[VZ] = 0.f;

            if (!at.isFinite() ||
                at.normalize() < 0.00001f)
            {
                return mLastValidYawRotation;
            }

            const F32 yaw =
                atan2f(
                    at.mV[VY],
                    at.mV[VX]);

            LLQuaternion result(
                yaw,
                LLVector3(0.f, 0.f, 1.f));

            mLastValidYawRotation = result;
            return result;
        }
    }
}
```

## 9.2 Avatar point

```cpp
bool LLFlycamAnchorResolver::resolveAvatar(
    const LLUUID& avatar_id,
    const LLFlycamAnchorBinding& binding,
    LLFlycamAnchorFrame& out_frame)
{
    LLViewerObject* object =
        gObjectList.findObject(avatar_id);

    LLVOAvatar* avatar =
        object
            ? object->asAvatar()
            : nullptr;

    if (!avatar || avatar->isDead())
    {
        return false;
    }

    LLVector3 point_agent =
        avatar->getRenderPosition();

    switch (binding.mAvatarPoint)
    {
        case EFlycamAvatarAnchorPoint::FEET:
            point_agent.mV[VZ] -=
                avatar->getPelvisToFoot();
            break;

        case EFlycamAvatarAnchorPoint::HEAD:
            if (LLJoint* joint =
                    avatar->getJoint("mHead"))
            {
                point_agent =
                    joint->getWorldPosition();
            }
            break;

        case EFlycamAvatarAnchorPoint::CHEST:
            if (LLJoint* joint =
                    avatar->getJoint("mChest"))
            {
                point_agent =
                    joint->getWorldPosition();
            }
            break;

        case EFlycamAvatarAnchorPoint::
            CUSTOM_LOCAL_OFFSET:
        {
            LLVector3 offset =
                binding.mCustomOffsetLocal;

            offset =
                offset *
                avatar->getRenderRotation();

            point_agent += offset;
            break;
        }

        case EFlycamAvatarAnchorPoint::PELVIS:
        case EFlycamAvatarAnchorPoint::ROOT:
        default:
            break;
    }

    out_frame.mPositionGlobal =
        gAgent.getPosGlobalFromAgent(
            point_agent);

    out_frame.mRotationWorld =
        makeAnchorRotation(
            avatar->getRenderRotation(),
            binding.mOrientationMode);

    out_frame.mResolvedRuntimeID =
        avatar->getID();

    out_frame.mValid =
        out_frame.mPositionGlobal.isFinite() &&
        out_frame.mRotationWorld.isFinite();

    return out_frame.mValid;
}
```

The exact preferred avatar root accessor should be confirmed against the local ghost/entity implementation. The camera system must use the same visible/render pose that the viewer displays, not an unsmoothed simulator position.

---

# 10. Coordinate conversion

The current source uses the viewer convention:

```cpp
vector * quaternion
```

and applies a world-yaw delta as:

```cpp
rotation = rotation * yaw_delta;
```

Isolate that convention in tested helpers.

```cpp
namespace LLFlycamSpace
{
    LLVector3d worldPointToLocal(
        const LLVector3d& world,
        const LLFlycamAnchorFrame& anchor)
    {
        const LLQuaternion inverse =
            ~anchor.mRotationWorld;

        return
            (world - anchor.mPositionGlobal) *
            inverse;
    }

    LLVector3d localPointToWorld(
        const LLVector3d& local,
        const LLFlycamAnchorFrame& anchor)
    {
        return
            anchor.mPositionGlobal +
            local * anchor.mRotationWorld;
    }

    LLQuaternion worldRotationToLocal(
        const LLQuaternion& world,
        const LLFlycamAnchorFrame& anchor)
    {
        LLQuaternion local =
            world *
            ~anchor.mRotationWorld;

        local.normalize();
        return local;
    }

    LLQuaternion localRotationToWorld(
        const LLQuaternion& local,
        const LLFlycamAnchorFrame& anchor)
    {
        LLQuaternion world =
            local *
            anchor.mRotationWorld;

        world.normalize();
        return world;
    }
}
```

## 10.1 Mandatory round-trip test

```cpp
void testAnchorRoundTrip()
{
    LLFlycamAnchorFrame anchor;

    anchor.mPositionGlobal =
        LLVector3d(
            1'000'000.25,
            2'000'000.50,
            3500.75);

    anchor.mRotationWorld =
        LLQuaternion(
            1.234f,
            LLVector3(0.f, 0.f, 1.f));

    anchor.mValid = true;

    const LLVector3d world_position =
        anchor.mPositionGlobal +
        LLVector3d(3.2, -1.7, 2.1);

    LLQuaternion world_rotation;
    world_rotation.setEulerAngles(
        0.2f,
        -0.3f,
        2.0f);

    const LLVector3d local_position =
        LLFlycamSpace::worldPointToLocal(
            world_position,
            anchor);

    const LLQuaternion local_rotation =
        LLFlycamSpace::worldRotationToLocal(
            world_rotation,
            anchor);

    const LLVector3d restored_position =
        LLFlycamSpace::localPointToWorld(
            local_position,
            anchor);

    const LLQuaternion restored_rotation =
        LLFlycamSpace::localRotationToWorld(
            local_rotation,
            anchor);

    ensure_approximately_equals(
        "relative position round trip",
        (restored_position -
         world_position).length(),
        0.0,
        1.0e-6);

    ensure_approximately_equals(
        "relative rotation round trip",
        std::fabs(dot(
            restored_rotation,
            world_rotation)),
        1.0,
        1.0e-5);
}
```

Also test known axis mappings. A shared mistake in both directions can pass a round-trip test.

---

# 11. True avatar-relative recording

## 11.1 Recording start order

Current start order is effectively:

```text
sample first camera key
latch self anchor
```

For anchor-local recording, the correct order is:

```text
capture stable anchor binding
resolve initial anchor frame
start clock
capture first camera + same-frame anchor
```

## 11.2 Record-session state

```cpp
struct LLFlycamRecordingSession
{
    bool mActive = false;

    EFlycamSpace mSpace =
        EFlycamSpace::WORLD_GLOBAL;

    LLFlycamAnchorBinding mAnchorBinding;

    F64 mClockStart = 0.0;

    LLFlycamAnchorFrame mLastAnchorFrame;
    bool mHaveLastAnchorFrame = false;

    F64 mLastSampleTime = -1.0;

    U64 mDroppedOrGapCount = 0;
};
```

## 11.3 Capture final camera

```cpp
struct LLFlycamWorldPose
{
    LLVector3d mPositionGlobal;
    LLQuaternion mRotationWorld;
    LLFlycamLens mLens;
};

bool captureFinalViewerCamera(
    LLFlycamWorldPose& out_pose)
{
    LLViewerCamera* camera =
        LLViewerCamera::getInstance();

    if (!camera)
    {
        return false;
    }

    out_pose.mPositionGlobal =
        gAgent.getPosGlobalFromAgent(
            camera->getOrigin());

    out_pose.mRotationWorld =
        camera->getQuaternion();

    out_pose.mLens.mVerticalFov =
        camera->getView();

    return
        validateAndNormalizeWorldPose(
            out_pose);
}
```

## 11.4 Record one rendered frame

```cpp
bool LLFlycamRecorder::captureRenderedFrame(
    const LLFlycamFrameContext& frame)
{
    if (mState != STATE_RECORDING)
    {
        return false;
    }

    LLFlycamWorldPose camera_world;

    if (!captureFinalViewerCamera(
            camera_world))
    {
        setRecordingError(
            "Final camera pose was invalid.");
        return false;
    }

    LLFlycamKeyframe key;
    key.mID.generate();

    key.mTime =
        frame.mRecordTime -
        mRecordingSession.mClockStart;

    if (!llfinite(key.mTime) ||
        key.mTime <=
            mRecordingSession.mLastSampleTime)
    {
        return false;
    }

    if (mTake.mSpace ==
        EFlycamSpace::WORLD_GLOBAL)
    {
        key.mPosition =
            camera_world.mPositionGlobal;

        key.mRotation =
            camera_world.mRotationWorld;
    }
    else
    {
        LLFlycamAnchorFrame anchor;

        if (!mAnchorResolver.resolve(
                mRecordingSession.mAnchorBinding,
                anchor))
        {
            return handleRecordAnchorLoss(
                frame);
        }

        if (detectAnchorDiscontinuity(
                mRecordingSession.mLastAnchorFrame,
                anchor,
                frame.mRecordDelta))
        {
            key.mCutBefore = true;
        }

        key.mPosition =
            LLFlycamSpace::worldPointToLocal(
                camera_world.mPositionGlobal,
                anchor);

        key.mRotation =
            LLFlycamSpace::worldRotationToLocal(
                camera_world.mRotationWorld,
                anchor);

        if (mCaptureRecordedAnchorTrack)
        {
            LLFlycamAnchorSample anchor_sample;

            anchor_sample.mTime =
                key.mTime;

            anchor_sample.mPositionGlobal =
                anchor.mPositionGlobal;

            anchor_sample.mRotationWorld =
                anchor.mRotationWorld;

            anchor_sample.mRuntimeID =
                anchor.mResolvedRuntimeID;

            anchor_sample.mDiscontinuity =
                key.mCutBefore;

            mTake.mRecordedAnchorTrack.push_back(
                anchor_sample);
        }

        mRecordingSession.mLastAnchorFrame =
            anchor;

        mRecordingSession.mHaveLastAnchorFrame =
            true;
    }

    key.mLens =
        camera_world.mLens;

    canonicalizeAgainstPrevious(
        key);

    mTake.mKeys.push_back(
        key);

    mRecordingSession.mLastSampleTime =
        key.mTime;

    return true;
}
```

## 11.5 Anchor-loss recording policy

Do not fabricate a local sample without an anchor.

```cpp
bool LLFlycamRecorder::handleRecordAnchorLoss(
    const LLFlycamFrameContext& frame)
{
    switch (mRecordingAnchorLossPolicy)
    {
        case EFlycamAnchorLossPolicy::
            PAUSE_AND_WAIT:
            mState =
                STATE_RECORDING_WAITING_FOR_ANCHOR;

            mStatus =
                "Recording paused: anchor unavailable.";

            return false;

        case EFlycamAnchorLossPolicy::
            STOP_AND_RELEASE_CAMERA:
            stopRecordingWithError(
                "Recording stopped: anchor unavailable.");

            return false;

        case EFlycamAnchorLossPolicy::
            FREEZE_LAST_ANCHOR_FRAME:
            // Allowed only when an explicit user setting enables it.
            return captureUsingFrozenAnchor(
                frame);

        case EFlycamAnchorLossPolicy::
            HOLD_LAST_WORLD_CAMERA:
        case EFlycamAnchorLossPolicy::
            EXPLICIT_FALLBACK_TO_SELF:
        default:
            // These policies are inappropriate for writing anchor-local data
            // unless the take records a discontinuity and a new binding.
            return false;
    }
}
```

---

# 12. Frame-rate-independent recording strategy

## 12.1 What is impossible

If the viewer renders no frame during a hitch, there is no final rendered camera pose for the missing time.

A live SpaceMouse or mouse camera input history cannot be reconstructed after the fact unless the input system separately records high-frequency events and can reevaluate the camera.

Therefore, the recorder must not pretend that it captured camera states that never existed.

## 12.2 Honest production model

```text
during live recording:
    capture every rendered final-camera pose with exact F64 timestamp

after recording:
    detect gaps
    optionally resample to a uniform rate
    optionally simplify by visual error
    preserve gap/cut markers
```

Replace the current “only sample when interval elapsed” gate with:

```cpp
void LLFlycamRecorder::onIdleFrame(
    const LLFlycamFrameContext& frame)
{
    if (mState != STATE_RECORDING)
    {
        return;
    }

    captureRenderedFrame(frame);
}
```

## 12.3 Gap detection

```cpp
bool isRecordingGap(
    F64 previous_time,
    F64 current_time,
    F64 expected_frame_period)
{
    const F64 threshold =
        llmax(
            expected_frame_period * 2.5,
            0.100);

    return
        current_time - previous_time >
        threshold;
}
```

When a gap exists:

```cpp
key.mGapBefore = true;
key.mCutBefore =
    mGapPolicy ==
    EFlycamGapPolicy::CUT_ACROSS_LARGE_GAPS;
```

## 12.4 Post-record resampling

```cpp
struct LLFlycamResampleOptions
{
    F64 mRate = 30.0;

    bool mPreserveCuts = true;
    bool mPreserveGaps = true;

    bool mLinearAcrossGaps = true;
};
```

Resampling creates predictable editing density, but it cannot restore high-frequency live input lost during a hitch.

## 12.5 Offline bake exception

Procedural sources such as:

```text
LLCinematicCamera
an existing Flycam take
Director camera clips
```

may support absolute evaluation at arbitrary time. These can be baked offline at 30/60/120 Hz without depending on live render FPS.

Expose that as a separate command:

```text
Bake Procedural Camera to Take
```

Do not use it for live joystick capture unless the input stream itself is recorded.

---

# 13. F64 time conversion

Current `F32` time precision degrades with duration.

Approximate float resolution:

```text
1 hour:   0.00043 seconds
6 hours:  0.00257 seconds
12 hours: 0.00515 seconds
24 hours: 0.01030 seconds
```

That becomes visible in high-frame-rate editing and deterministic capture.

Change:

```cpp
F32 mTime;
F32 mPlayhead;
```

to:

```cpp
F64 mTime;
F64 mPlayhead;
```

Change all evaluator, trim, transport, and serialization paths to `F64`.

Keep UI controls as `F32` only at the final widget boundary if the widget API requires it.

---

# 14. Absolute-time transport

## 14.1 Clock modes

```cpp
enum class EFlycamClock : U8
{
    WALL_MONOTONIC = 0,
    PRESENTATION_TIME = 1,
    DIRECTOR_SEQUENCE = 2,
    FIXED_CAPTURE_FRAME = 3
};
```

## 14.2 Frame context

```cpp
struct LLFlycamFrameContext
{
    F64 mWallTime = 0.0;
    F64 mWallDelta = 0.0;

    F64 mPresentationTime = 0.0;
    F64 mPresentationDelta = 0.0;

    F64 mDirectorTime = 0.0;

    bool mFixedCapture = false;
    S64 mCaptureFrame = 0;
    F64 mOutputFPS = 0.0;

    bool mSeek = false;
    bool mCameraCut = false;
};
```

## 14.3 Transport state

```cpp
struct LLFlycamTransport
{
    EFlycamClock mClock =
        EFlycamClock::WALL_MONOTONIC;

    F64 mPathBaseTime = 0.0;
    F64 mClockStartTime = 0.0;

    F64 mPausedPathTime = 0.0;

    F64 mSpeed = 1.0;
    S32 mDirection = 1;

    bool mPlaying = false;
};
```

## 14.4 Resolve clock

```cpp
F64 resolveFlycamClock(
    const LLFlycamFrameContext& frame,
    EFlycamClock clock)
{
    switch (clock)
    {
        case EFlycamClock::
            PRESENTATION_TIME:
            return frame.mPresentationTime;

        case EFlycamClock::
            DIRECTOR_SEQUENCE:
            return frame.mDirectorTime;

        case EFlycamClock::
            FIXED_CAPTURE_FRAME:
            if (frame.mFixedCapture &&
                frame.mOutputFPS > 0.0)
            {
                return
                    static_cast<F64>(
                        frame.mCaptureFrame) /
                    frame.mOutputFPS;
            }

            return frame.mDirectorTime;

        case EFlycamClock::
            WALL_MONOTONIC:
        default:
            return frame.mWallTime;
    }
}
```

## 14.5 Path time

```cpp
F64 LLFlycamTransport::pathTime(
    const LLFlycamFrameContext& frame) const
{
    if (!mPlaying)
    {
        return mPausedPathTime;
    }

    const F64 clock_now =
        resolveFlycamClock(
            frame,
            mClock);

    return
        mPathBaseTime +
        (clock_now -
         mClockStartTime) *
        mSpeed *
        static_cast<F64>(
            mDirection);
}
```

A hitch no longer permanently slows the take.

The operator simulation may still receive a clamped delta for stability. Transport time and procedural-noise integration must not be conflated.

---

# 15. Mathematical loop and ping-pong mapping

```cpp
struct LLFlycamMappedTime
{
    F64 mTime = 0.0;
    S32 mDirection = 1;

    bool mWrapped = false;
    bool mReversed = false;
    bool mFinished = false;
};

LLFlycamMappedTime mapTakeTime(
    F64 raw_time,
    F64 in_time,
    F64 out_time,
    ELoopMode mode)
{
    LLFlycamMappedTime result;

    const F64 length =
        out_time - in_time;

    if (!(length > 0.0))
    {
        result.mTime = in_time;
        result.mFinished = true;
        return result;
    }

    const F64 local =
        raw_time - in_time;

    switch (mode)
    {
        case LOOP_REPEAT:
        {
            F64 wrapped =
                std::fmod(local, length);

            if (wrapped < 0.0)
            {
                wrapped += length;
            }

            result.mTime =
                in_time + wrapped;

            result.mWrapped =
                local < 0.0 ||
                local >= length;

            return result;
        }

        case LOOP_PINGPONG:
        {
            const F64 period =
                2.0 * length;

            F64 wrapped =
                std::fmod(local, period);

            if (wrapped < 0.0)
            {
                wrapped += period;
            }

            if (wrapped <= length)
            {
                result.mTime =
                    in_time + wrapped;

                result.mDirection = 1;
            }
            else
            {
                result.mTime =
                    out_time -
                    (wrapped - length);

                result.mDirection = -1;
                result.mReversed = true;
            }

            result.mWrapped =
                local < 0.0 ||
                local >= period;

            return result;
        }

        case LOOP_HOLD_END:
        default:
            result.mTime =
                llclamp(
                    raw_time,
                    in_time,
                    out_time);

            result.mFinished =
                raw_time >= out_time;

            return result;
    }
}
```

On wrap, cut, seek, target rebind, or direction reversal:

```cpp
resetOperatorHistory();
incrementCameraDiscontinuitySerial();
```

For a smooth ping-pong reversal without a velocity cusp, the endpoint tangent must be explicitly zero or the curve must be authored to reverse smoothly.

---

# 16. Evaluator

## 16.1 Binary search

```cpp
struct LLFlycamSegment
{
    size_t mLeft = 0;
    size_t mRight = 0;

    F64 mU = 0.0;
    F64 mDuration = 0.0;
};

LLFlycamSegment findSegment(
    const std::vector<LLFlycamKeyframe>& keys,
    F64 time)
{
    if (time <= keys.front().mTime)
    {
        return {0, 0, 0.0, 0.0};
    }

    if (time >= keys.back().mTime)
    {
        const size_t last =
            keys.size() - 1;

        return {
            last,
            last,
            0.0,
            0.0
        };
    }

    const auto upper =
        std::upper_bound(
            keys.begin(),
            keys.end(),
            time,
            [](F64 value,
               const LLFlycamKeyframe& key)
            {
                return value < key.mTime;
            });

    const size_t right =
        static_cast<size_t>(
            std::distance(
                keys.begin(),
                upper));

    const size_t left =
        right - 1;

    const F64 duration =
        keys[right].mTime -
        keys[left].mTime;

    const F64 u =
        duration > 0.0
            ? (time -
               keys[left].mTime) /
              duration
            : 0.0;

    return {
        left,
        right,
        llclamp(u, 0.0, 1.0),
        duration
    };
}
```

During normal forward playback, also cache the last segment and advance it monotonically. Binary search remains the fallback for seek/reverse.

## 16.2 Cut boundaries

```cpp
if (right_key.mCutBefore)
{
    return
        time < right_key.mTime
            ? poseOf(left_key)
            : poseOf(right_key);
}
```

Never spline across a teleport, missing-anchor gap, explicit cut, or incompatible target rebind.

---

# 17. Time-aware smooth position interpolation

Uniform Catmull-Rom treats every key as equally spaced even when timestamps are not.

A safer first production implementation is cubic Hermite using actual time.

## 17.1 Automatic tangent

```cpp
LLVector3d autoPositionTangent(
    const std::vector<LLFlycamKeyframe>& keys,
    size_t index)
{
    if (keys.size() < 2)
    {
        return LLVector3d::zero;
    }

    if (index == 0)
    {
        const F64 dt =
            keys[1].mTime -
            keys[0].mTime;

        return
            dt > 0.0
                ? (keys[1].mPosition -
                   keys[0].mPosition) /
                  dt
                : LLVector3d::zero;
    }

    if (index + 1 >= keys.size())
    {
        const F64 dt =
            keys[index].mTime -
            keys[index - 1].mTime;

        return
            dt > 0.0
                ? (keys[index].mPosition -
                   keys[index - 1].mPosition) /
                  dt
                : LLVector3d::zero;
    }

    const F64 dt =
        keys[index + 1].mTime -
        keys[index - 1].mTime;

    return
        dt > 0.0
            ? (keys[index + 1].mPosition -
               keys[index - 1].mPosition) /
              dt
            : LLVector3d::zero;
}
```

## 17.2 Hermite

```cpp
LLVector3d hermitePosition(
    const LLVector3d& p0,
    const LLVector3d& p1,
    const LLVector3d& velocity0,
    const LLVector3d& velocity1,
    F64 segment_duration,
    F64 u)
{
    const F64 u2 = u * u;
    const F64 u3 = u2 * u;

    const F64 h00 =
        2.0 * u3 -
        3.0 * u2 +
        1.0;

    const F64 h10 =
        u3 -
        2.0 * u2 +
        u;

    const F64 h01 =
        -2.0 * u3 +
        3.0 * u2;

    const F64 h11 =
        u3 -
        u2;

    return
        p0 * h00 +
        velocity0 *
            (segment_duration * h10) +
        p1 * h01 +
        velocity1 *
            (segment_duration * h11);
}
```

## 17.3 Overshoot containment

Provide per-segment choices:

```text
Hold
Linear
Smooth Auto
Smooth Clamped
Free Tangents
```

For `Smooth Clamped`, cap tangent magnitude relative to neighboring chord lengths.

```cpp
LLVector3d clampTangent(
    const LLVector3d& tangent,
    F64 maximum_speed)
{
    const F64 length =
        tangent.length();

    if (length <= maximum_speed ||
        length <= 0.0)
    {
        return tangent;
    }

    return
        tangent *
        (maximum_speed / length);
}
```

Do not silently clamp hand-authored free tangents.

---

# 18. Easing

Interpolation shape and time easing are different controls.

```cpp
F64 applyEase(
    F64 u,
    EFlycamEase ease)
{
    u = llclamp(u, 0.0, 1.0);

    switch (ease)
    {
        case EFlycamEase::EASE_IN:
            return u * u;

        case EFlycamEase::EASE_OUT:
            return
                1.0 -
                (1.0 - u) *
                (1.0 - u);

        case EFlycamEase::EASE_IN_OUT:
            return
                u * u *
                (3.0 - 2.0 * u);

        case EFlycamEase::NONE:
        default:
            return u;
    }
}
```

Custom cubic Bézier ease can be added after inversion of the x-curve is tested and bounded.

---

# 19. Quaternion robustness

## 19.1 Validate before normalize

```cpp
bool normalizeQuaternionChecked(
    LLQuaternion& quaternion)
{
    if (!quaternion.isFinite())
    {
        return false;
    }

    const F64 norm_squared =
        static_cast<F64>(
            quaternion.mQ[VX]) *
            quaternion.mQ[VX] +
        static_cast<F64>(
            quaternion.mQ[VY]) *
            quaternion.mQ[VY] +
        static_cast<F64>(
            quaternion.mQ[VZ]) *
            quaternion.mQ[VZ] +
        static_cast<F64>(
            quaternion.mQ[VW]) *
            quaternion.mQ[VW];

    if (!llfinite(norm_squared) ||
        norm_squared < 1.0e-16)
    {
        return false;
    }

    quaternion.normalize();
    return quaternion.isFinite();
}
```

## 19.2 Hemisphere canonicalization

```cpp
void canonicalizeQuaternionSequence(
    std::vector<LLFlycamKeyframe>& keys)
{
    if (keys.empty())
    {
        return;
    }

    normalizeQuaternionChecked(
        keys.front().mRotation);

    for (size_t index = 1;
         index < keys.size();
         ++index)
    {
        LLQuaternion& previous =
            keys[index - 1].mRotation;

        LLQuaternion& current =
            keys[index].mRotation;

        if (!normalizeQuaternionChecked(
                current))
        {
            current = previous;
            keys[index].mCutBefore = true;
        }

        if (dot(previous, current) < 0.f)
        {
            current = -current;
        }
    }
}
```

Run this after:

```text
load
insert
paste
retime
record finalization
import
anchor-space conversion
```

## 19.3 Safer raw log

The current raw-log function uses:

```text
acos(w) / sqrt(1 - w²)
```

A more stable unit-quaternion log uses `atan2`.

```cpp
FRQuat fr_log_unit(FRQuat q)
{
    q = fr_normalize_or_identity(q);

    // Canonicalize close to identity rather than -identity.
    if (q.w < 0.f)
    {
        q = fr_scale(q, -1.f);
    }

    const F32 vector_length =
        sqrtf(
            q.x * q.x +
            q.y * q.y +
            q.z * q.z);

    if (vector_length < 1.0e-6f)
    {
        // log(q) approaches the vector part near identity.
        return FRQuat{
            q.x,
            q.y,
            q.z,
            0.f
        };
    }

    const F32 angle =
        atan2f(
            vector_length,
            llclamp(
                q.w,
                -1.f,
                1.f));

    const F32 scale =
        angle /
        vector_length;

    return FRQuat{
        q.x * scale,
        q.y * scale,
        q.z * scale,
        0.f
    };
}
```

## 19.4 Slerp

```cpp
FRQuat fr_slerp_checked(
    FRQuat a,
    FRQuat b,
    F32 t)
{
    a = fr_normalize_or_identity(a);
    b = fr_normalize_or_identity(b);

    F32 cosine =
        fr_dot(a, b);

    if (cosine < 0.f)
    {
        b = fr_scale(b, -1.f);
        cosine = -cosine;
    }

    cosine =
        llclamp(cosine, -1.f, 1.f);

    if (cosine > 0.9995f)
    {
        return fr_normalize_or_identity(
            fr_add(
                fr_scale(a, 1.f - t),
                fr_scale(b, t)));
    }

    const F32 angle =
        acosf(cosine);

    const F32 sine =
        sinf(angle);

    if (fabsf(sine) < 1.0e-6f)
    {
        return a;
    }

    return fr_normalize_or_identity(
        fr_add(
            fr_scale(
                a,
                sinf(
                    (1.f - t) *
                    angle) /
                    sine),
            fr_scale(
                b,
                sinf(
                    t *
                    angle) /
                    sine)));
}
```

## 19.5 Deployment sequence

Because the current SQUAD path already works for ordinary data:

```text
Phase 1:
    retain SQUAD
    add validation, canonicalization, cache, tests

Phase 2:
    introduce time-weighted controls or fall back to per-segment slerp
    for nonuniform/edited keys

Phase 3:
    expose Smooth Rotation as an authored per-segment choice
```

Do not replace working rotation interpolation and the anchor system in the same patch.

---

# 20. Rotation interpolation choices

Initial production modes:

```text
Hold
Slerp
Squad Auto
```

For edited keys with irregular spacing, Slerp is the safest default.

Cache SQUAD controls:

```cpp
struct LLFlycamRotationCache
{
    U64 mTakeRevision = 0;

    std::vector<FRQuat> mInnerControls;
};
```

Invalidate when:

```text
key rotation changes
key time changes
key inserted/deleted
take reloaded
space converted
```

---

# 21. Constant spatial speed

Constant speed is not the same as smooth interpolation.

Provide:

```cpp
enum class EFlycamTimingMode : U8
{
    RECORDED_TIMING = 0,
    CONSTANT_LOCAL_PATH_SPEED = 1,
    SPEED_CURVE = 2
};
```

## 21.1 Arc-length table

```cpp
struct LLFlycamArcSample
{
    F64 mPathTime = 0.0;
    F64 mDistance = 0.0;
};

struct LLFlycamArcLengthTable
{
    U64 mTakeRevision = 0;

    std::vector<LLFlycamArcSample> mSamples;

    F64 mTotalDistance = 0.0;
};
```

Build by adaptively subdividing segments until chord error is below tolerance.

## 21.2 Important relative-path rule

For a live-follow relative path:

```text
constant speed applies to the local camera move
```

The world camera also contains anchor motion, so its resulting world speed may vary. That is correct.

---

# 22. Lens, focus, and depth of field

## 22.1 FOV validation

```cpp
bool validateFov(F32& fov)
{
    if (!llfinite(fov))
    {
        return false;
    }

    constexpr F32 MATHEMATICAL_MIN =
        0.01f;

    constexpr F32 MATHEMATICAL_MAX =
        F_PI - 0.01f;

    fov =
        llclamp(
            fov,
            MATHEMATICAL_MIN,
            MATHEMATICAL_MAX);

    return true;
}
```

The UI should use the existing viewer camera-angle range rather than the broad mathematical limits.

## 22.2 Local FOV setter

Replace:

```cpp
cam->setView(fov * fov_mul);
```

with:

```cpp
F32 final_fov =
    fov *
    fov_mul;

if (!validateFov(final_fov))
{
    return failCameraEvaluation(
        "Invalid camera FOV.");
}

cam->setViewNoBroadcast(
    final_fov);
```

This is an immediate hardening patch.

## 22.3 Focus model

```cpp
enum class EFlycamFocusMode : U8
{
    NONE = 0,
    DISTANCE = 1,
    WORLD_POINT = 2,
    ANCHOR_LOCAL_POINT = 3,
    TRACK_BINDING = 4
};

struct LLFlycamFocus
{
    EFlycamFocusMode mMode =
        EFlycamFocusMode::NONE;

    F32 mDistance = 0.f;

    LLVector3d mPoint;

    LLFlycamAnchorBinding mBinding;
};
```

The renderer-specific adapter for focus distance/aperture must be audited against the private depth-of-field implementation before these values are wired into rendering.

## 22.4 Lens interpolation

For ordinary viewer compatibility:

```text
interpolate vertical FOV
```

For DCC interchange:

```text
optionally store focal length + sensor height
derive vertical FOV:
    yfov = 2 × atan(sensor_height / (2 × focal_length))
```

Do not silently mix horizontal and vertical FOV.

---

# 23. Playback placement

## 23.1 Fixed around avatar at playback start

This is the main requested behavior.

```cpp
bool LLFlycamRecorder::capturePlaybackStartAnchor()
{
    if (!mAnchorResolver.resolve(
            mPlaybackAnchorBinding,
            mPlaybackStartAnchor))
    {
        mStatus =
            "Cannot start: playback anchor unavailable.";

        return false;
    }

    mHavePlaybackStartAnchor = true;
    return true;
}
```

Evaluation:

```cpp
bool resolvePlacementFrame(
    F64 time,
    LLFlycamAnchorFrame& out_frame)
{
    switch (mPlacementMode)
    {
        case EFlycamPlacement::
            ANCHOR_AT_PLAYBACK_START:
            if (!mHavePlaybackStartAnchor)
            {
                return false;
            }

            out_frame =
                mPlaybackStartAnchor;

            return true;

        case EFlycamPlacement::
            LIVE_ANCHOR:
            return mAnchorResolver.resolve(
                mPlaybackAnchorBinding,
                out_frame);

        case EFlycamPlacement::
            RECORDED_ANCHOR_TRACK:
            return evaluateRecordedAnchorTrack(
                time,
                out_frame);

        case EFlycamPlacement::
            MANUAL_FRAME:
            out_frame =
                mManualPlacementFrame;

            return out_frame.mValid;

        case EFlycamPlacement::
            ORIGINAL_WORLD:
        default:
            return false;
    }
}
```

## 23.2 World evaluation

```cpp
bool LLFlycamEvaluator::evaluateWorld(
    const LLFlycamTake& take,
    F64 time,
    const LLFlycamAnchorFrame* placement,
    LLFlycamWorldPose& out_pose)
{
    LLFlycamStoredPose stored;

    if (!evaluateStored(
            take,
            time,
            stored))
    {
        return false;
    }

    if (take.mSpace ==
        EFlycamSpace::WORLD_GLOBAL)
    {
        out_pose.mPositionGlobal =
            stored.mPosition;

        out_pose.mRotationWorld =
            stored.mRotation;
    }
    else
    {
        if (!placement ||
            !placement->mValid)
        {
            return false;
        }

        out_pose.mPositionGlobal =
            LLFlycamSpace::localPointToWorld(
                stored.mPosition,
                *placement);

        out_pose.mRotationWorld =
            LLFlycamSpace::localRotationToWorld(
                stored.mRotation,
                *placement);
    }

    out_pose.mLens =
        stored.mLens;

    return
        validateAndNormalizeWorldPose(
            out_pose);
}
```

---

# 24. Bake relative take to world

```cpp
bool bakeTakeToWorld(
    const LLFlycamTake& source,
    const LLFlycamAnchorFrame& fixed_frame,
    LLFlycamTake& output)
{
    if (source.mSpace !=
            EFlycamSpace::ANCHOR_LOCAL ||
        !fixed_frame.mValid)
    {
        return false;
    }

    output = source;

    output.mTakeID.generate();

    output.mName =
        source.mName +
        " — Baked World";

    output.mSpace =
        EFlycamSpace::WORLD_GLOBAL;

    output.mDefaultPlacement =
        EFlycamPlacement::ORIGINAL_WORLD;

    output.mRecordedAnchor =
        LLFlycamAnchorBinding();

    output.mRecordedAnchorTrack.clear();

    for (LLFlycamKeyframe& key :
         output.mKeys)
    {
        key.mPosition =
            LLFlycamSpace::localPointToWorld(
                key.mPosition,
                fixed_frame);

        key.mRotation =
            LLFlycamSpace::localRotationToWorld(
                key.mRotation,
                fixed_frame);
    }

    ++output.mEditRevision;

    canonicalizeQuaternionSequence(
        output.mKeys);

    return true;
}
```

The bake command must create a new take by default. Do not destructively overwrite the only relative source without confirmation.

---

# 25. Recorded anchor trajectory

When recording relative to a moving performer, save the anchor track.

Benefits:

- reproduce the original world take;
- compare local and world motion;
- diagnose anchor discontinuities;
- re-time actor and camera together;
- convert to a world take later;
- support fixed-frame replay after the original avatar is gone.

## 25.1 Evaluate anchor track

Use:

```text
position:
    linear or time-aware Hermite

rotation:
    slerp

cuts:
    step
```

Do not smooth across runtime replacement, teleport, sit-parent discontinuity, or missing sample.

---

# 26. Anchor smoothing

## 26.1 Recording

When recording anchor-local data, use the same render-space anchor pose that the user sees.

Do not separately low-pass the anchor while recording unless the camera is also framed against that same smoothed anchor. Otherwise the stored local relationship becomes false.

## 26.2 Live playback

Remote-avatar live follow may use an optional playback-only anchor filter:

```cpp
struct LLFlycamAnchorFilterConfig
{
    bool mEnabled = false;

    F64 mPositionHalfLife = 0.08;
    F64 mYawHalfLife = 0.06;

    F64 mTeleportDistance = 8.0;
};
```

A teleport or runtime replacement bypasses smoothing and raises a discontinuity.

Ghost actors and recorded actor tracks should normally require no network-jitter filter.

---

# 27. Operator layer

## 27.1 Current weakness

The current operator derives:

```text
linear velocity:
    current placed camera - previous placed camera

angular velocity:
    Euler angles of quaternion delta / dt
```

Problems:

- Euler extraction can wrap;
- hitches distort velocity;
- live-anchor motion and local camera motion are mixed;
- seek/loop/rebind events can create impulses;
- ping-pong reversal is a cusp unless reset;
- fixed-frame capture needs deterministic operator state.

## 27.2 Evaluator derivative

```cpp
struct LLFlycamPoseDerivative
{
    LLVector3d mLinearVelocityWorld;

    LLVector3 mAngularVelocityLocal;

    F32 mFovVelocity = 0.f;

    bool mValid = false;
};
```

For position Hermite, compute the analytic derivative.

For rotation, use quaternion logarithm over a small symmetric evaluation interval:

```cpp
LLVector3 angularVelocityFromSamples(
    const LLQuaternion& before,
    const LLQuaternion& after,
    F64 delta_time)
{
    if (!(delta_time > 0.0))
    {
        return LLVector3::zero;
    }

    LLQuaternion delta =
        after *
        ~before;

    if (delta.mQ[VW] < 0.f)
    {
        delta = -delta;
    }

    F32 angle = 0.f;
    LLVector3 axis;

    delta.getAngleAxis(
        &angle,
        axis);

    if (!axis.isFinite())
    {
        return LLVector3::zero;
    }

    return
        axis *
        static_cast<F32>(
            angle /
            delta_time);
}
```

## 27.3 Deterministic operator

For fixed-frame capture, either:

1. make the operator a pure absolute-time noise function seeded by take/shot ID; or
2. reconstruct/preroll state deterministically from a known start; or
3. disable the stateful operator and bake its result into a take before capture.

The first option is preferred long-term.

---

# 28. Temporal discontinuity service

```cpp
enum class ECameraDiscontinuity : U32
{
    NONE = 0,
    START_PLAYBACK = 1u << 0,
    STOP_PLAYBACK = 1u << 1,
    SEEK = 1u << 2,
    CUT_KEY = 1u << 3,
    LOOP_WRAP = 1u << 4,
    PINGPONG_REVERSE = 1u << 5,
    ANCHOR_LOST = 1u << 6,
    ANCHOR_REBOUND = 1u << 7,
    TELEPORT = 1u << 8,
    PROJECTION_CHANGE = 1u << 9,
    CAMERA_OWNER_CHANGED = 1u << 10
};

struct LLCameraDiscontinuityState
{
    U64 mSerial = 0;
    ECameraDiscontinuity mReasons =
        ECameraDiscontinuity::NONE;
};
```

Consumers:

```text
native motion-vector history
TAA
ReShade bridge reset flags
operator history
camera smoothing
capture metadata
```

---

# 29. Camera ownership

## 29.1 Keep the existing branch initially

The current camera-dispatch seam is valuable and working.

Do not combine the first relative-recording fix with a total camera-dispatch rewrite.

Immediate hardening:

```text
show current owner in UI
stop/release on teleport
stop/release on logout
Escape emergency release
disable conflicting start commands
publish owner-change discontinuity
```

## 29.2 Later coordinator

```cpp
enum class ECameraDriver : U8
{
    NORMAL_AGENT = 0,
    AGENT_PILOT,
    FLYCAM_TAKE,
    PATH_CAMERA,
    CINEMATIC_CAMERA,
    JOYSTICK_FLYCAM,
    CAPTURE_OVERRIDE
};

class LLCameraControlCoordinator
{
public:
    bool request(
        ECameraDriver driver,
        S32 priority);

    void release(
        ECameraDriver driver);

    ECameraDriver owner() const;

    U64 generation() const;

    void emergencyRelease();
};
```

Every driver still fully writes the one global camera. The coordinator only centralizes exclusivity and lifecycle.

---

# 30. Safe camera application

```cpp
bool applyFlycamPose(
    const LLFlycamWorldPose& pose,
    F32 operator_fov_multiplier)
{
    LLViewerCamera* camera =
        LLViewerCamera::getInstance();

    if (!camera)
    {
        return false;
    }

    if (!pose.mPositionGlobal.isFinite() ||
        !pose.mRotationWorld.isFinite())
    {
        return false;
    }

    LLQuaternion rotation =
        pose.mRotationWorld;

    if (!normalizeQuaternionChecked(
            rotation))
    {
        return false;
    }

    const LLVector3 origin_agent =
        gAgent.getPosAgentFromGlobal(
            pose.mPositionGlobal);

    if (!origin_agent.isFinite())
    {
        return false;
    }

    F32 fov =
        pose.mLens.mVerticalFov *
        operator_fov_multiplier;

    if (!validateFov(fov))
    {
        return false;
    }

    const LLMatrix3 axes(
        rotation);

    camera->setViewNoBroadcast(
        fov);

    camera->setOrigin(
        origin_agent);

    camera->mXAxis =
        LLVector3(
            axes.mMatrix[0]);

    camera->mYAxis =
        LLVector3(
            axes.mMatrix[1]);

    camera->mZAxis =
        LLVector3(
            axes.mMatrix[2]);

    return true;
}
```

---

# 31. Persistence hardening

## 31.1 Limits

```cpp
struct LLFlycamLoadLimits
{
    U64 mMaximumFileBytes =
        64ull * 1024ull * 1024ull;

    size_t mMaximumKeys =
        2'000'000;

    size_t mMaximumAnchorSamples =
        2'000'000;

    F64 mMaximumDurationSeconds =
        24.0 * 60.0 * 60.0;
};
```

These are safety caps, not advertised creative targets.

## 31.2 Transactional load

```cpp
bool LLFlycamTakeIO::load(
    const std::string& filename,
    LLFlycamTake& destination,
    std::string& error)
{
    if (!validateFileSize(
            filename,
            mLimits.mMaximumFileBytes,
            error))
    {
        return false;
    }

    LLSD document;

    if (!parseLLSD(
            filename,
            document,
            error))
    {
        return false;
    }

    LLFlycamTake temporary;

    if (!decodeDocument(
            document,
            temporary,
            error))
    {
        return false;
    }

    if (!validateTake(
            temporary,
            mLimits,
            error))
    {
        return false;
    }

    canonicalizeQuaternionSequence(
        temporary.mKeys);

    rebuildDerivedCaches(
        temporary);

    // Commit only after every stage succeeds.
    destination =
        std::move(temporary);

    return true;
}
```

Never clear the current take before a new file has fully loaded.

## 31.3 Atomic save

```cpp
bool LLFlycamTakeIO::saveAtomic(
    const LLFlycamTake& take,
    const std::string& filename,
    std::string& error)
{
    const std::string temporary =
        filename + ".tmp";

    const LLSD document =
        encodeTake(take);

    {
        llofstream stream(
            temporary.c_str(),
            std::ios::binary |
            std::ios::trunc);

        if (!stream.is_open())
        {
            error =
                "Could not open temporary take file.";
            return false;
        }

        LLSDSerialize::toPrettyXML(
            document,
            stream);

        stream.flush();

        if (!stream.good())
        {
            error =
                "Could not finish writing take.";
            stream.close();
            LLFile::remove(temporary);
            return false;
        }
    }

    // Reconcile the exact atomic-replace helper with LLFile on the target OS.
    if (!replaceFileAtomically(
            temporary,
            filename,
            error))
    {
        LLFile::remove(temporary);
        return false;
    }

    return true;
}
```

## 31.4 Key validation

```cpp
bool validateKey(
    LLFlycamKeyframe& key,
    F64 previous_time,
    std::string& error)
{
    if (!llfinite(key.mTime) ||
        key.mTime < 0.0 ||
        key.mTime <= previous_time)
    {
        error =
            "Key timestamps must be finite and strictly increasing.";
        return false;
    }

    if (!key.mPosition.isFinite())
    {
        error =
            "Camera position is non-finite.";
        return false;
    }

    if (!normalizeQuaternionChecked(
            key.mRotation))
    {
        error =
            "Camera rotation is invalid.";
        return false;
    }

    if (!validateFov(
            key.mLens.mVerticalFov))
    {
        error =
            "Camera FOV is invalid.";
        return false;
    }

    if (key.mID.isNull())
    {
        key.mID.generate();
    }

    return true;
}
```

---

# 32. Version-2 compatibility

The v2 loader remains supported.

## 32.1 Import mapping

```text
v2 key:
    t F32
    world pos
    world rot
    FOV

v3 mapping:
    time F64
    space WORLD_GLOBAL
    generated stable key IDs
    legacy record anchor metadata retained
```

## 32.2 Preserve existing v2 relative behavior

A v2 take can retain its old placement semantics as:

```text
legacy world offset transform
```

Do not silently reinterpret it as a true anchor-local take.

## 32.3 Conversion command

Offer:

```text
Convert Legacy Relative Placement to Anchor-Local
```

This conversion is exact only under the assumption:

```text
the recording anchor remained fixed during the take
```

If no recorded anchor trajectory exists, motion of the original subject cannot be removed retrospectively.

The UI must say so.

---

# 33. Per-take settings

Current playback behavior depends on live global settings such as:

```text
FlycamRecSmooth
FlycamRecSpeed
FlycamRecLoopMode
FlycamRecUseOperator
FlycamRecAnchorMode
FlycamRecFollowAnchor
```

For reproducible takes:

```text
gSavedSettings:
    defaults for new takes and current UI preference

LLFlycamTake:
    saved creative behavior

LLDirectorShot:
    optional per-shot override
```

A loaded take should not change because the user changed a global default for an unrelated take.

---

# 34. Timeline / dope-sheet

## 34.1 Minimal viable control

Create a custom XUI control:

```cpp
class ALFlycamTimeline
    : public LLView
{
public:
    bool handleMouseDown(
        S32 x,
        S32 y,
        MASK mask) override;

    bool handleHover(
        S32 x,
        S32 y,
        MASK mask) override;

    bool handleMouseUp(
        S32 x,
        S32 y,
        MASK mask) override;

    bool handleScrollWheel(
        S32 x,
        S32 y,
        S32 clicks) override;

    void draw() override;
};
```

## 34.2 Tracks

Initial tracks:

```text
Camera Position
Camera Rotation
FOV
Focus
Markers/Cuts
```

The first milestone may render one combined camera-key row plus a cut row.

## 34.3 Editing operations

```text
Click:
    select key

Ctrl-click:
    toggle selection

Shift-click:
    range select

Drag:
    retime selected keys

Alt-drag:
    duplicate and retime

Delete:
    delete selected keys

Ctrl-D:
    duplicate

Ctrl-C / Ctrl-V:
    copy/paste keys

S:
    snap toggle

Home:
    frame take

F:
    frame selection

I:
    insert key at playhead

K:
    set selected key from current camera

M:
    add marker

C:
    add cut
```

## 34.4 Stable identity

Selection stores key UUIDs:

```cpp
std::set<LLUUID> mSelectedKeyIDs;
```

Never store vector indices as persistent editor selection.

## 34.5 Retime transaction

```cpp
class LLFlycamRetimeCommand
{
public:
    struct Entry
    {
        LLUUID mKeyID;
        F64 mBefore;
        F64 mAfter;
    };

    void apply(LLFlycamTake&);
    void undo(LLFlycamTake&);
};
```

## 34.6 Collision policy

Dragging keys onto the same time requires an explicit policy:

```text
Prevent overlap
Ripple following keys
Merge selected keys
Replace existing key
Allow stack only for separate property tracks
```

Initial recommendation:

```text
Prevent overlap with a minimum epsilon
```

## 34.7 Shared-panel synchronization

Two `ALPanelFlycamRecorder` instances can exist:

```text
standalone floater
Director Console Takes tab
```

Add model signals:

```cpp
boost::signals2::signal<void(U64)> mTakeChanged;
boost::signals2::signal<void(F64)> mPlayheadChanged;
boost::signals2::signal<void()> mSelectionChanged;
```

Each panel observes the same model but may keep local zoom/scroll state.

---

# 35. Key inspector

Fields:

```text
Time
Label
Position X/Y/Z
Rotation:
    Quaternion readout
    User-facing yaw/pitch/roll editor
FOV
Focus mode/distance/point
Position interpolation
Rotation interpolation
Lens interpolation
Ease
Cut before
Protected key
```

Euler values are an editing presentation only.

Store quaternions as authoritative data.

---

# 36. In-world path editor

Reuse the architecture and visual language of:

```text
ALToolPathEdit
ALPanelPathEditor
```

Create:

```cpp
class ALToolFlycamPathEdit
    : public LLTool
{
public:
    bool handleMouseDown(
        S32 x,
        S32 y,
        MASK mask) override;

    bool handleHover(
        S32 x,
        S32 y,
        MASK mask) override;

    bool handleMouseUp(
        S32 x,
        S32 y,
        MASK mask) override;

    void render() override;
};
```

## 36.1 Render

Display:

```text
camera path spline
key markers
selected key marker
camera frusta
forward/up orientation axes
focus rays
anchor origin and axes
cuts/gaps
tangent handles
speed tick marks
```

## 36.2 Relative paths

Store keys in anchor-local space.

For display:

```text
local key
    ↓ current editor placement frame
world marker
```

Dragging a marker:

```text
world drag result
    ↓ inverse current editor placement frame
updated local key
```

## 36.3 Picking

Do not create simulator objects.

Use:

```text
screen-space marker projection
nearest marker within pixel radius
depth-aware tie breaking
stable key UUID
```

For dense paths, add:

```text
spatial screen bins
selected-key priority
hide intermediate sampled points
```

## 36.4 First movement gizmo

Ship small:

```text
screen-plane drag
Shift:
    vertical world-Z drag
Ctrl:
    surface snap
```

Later:

```text
world/local XYZ axis handles
plane handles
tangent handles
orientation ring
```

Do not reuse object selection manipulators if that would mutate the user's actual selection or send object changes.

## 36.5 Useful commands

```text
Insert Key at Playhead
Set Key from Current Camera
Set FOV from Current Camera
Look Key at Selected Subject
Set Focus from Selected Subject
Duplicate Key
Delete Key
Bake Relative Take Here
Retarget Preview to Selected Actor
```

---

# 37. Record and hand-edit coexistence

## 37.1 Replace take

Current behavior:

```text
Record
    → clear existing take
    → capture new take
```

Keep as default.

## 37.2 Append

```text
Append starts at current take end.
New raw timestamps are offset by existing duration.
```

## 37.3 Insert and shift

```text
Insert at playhead:
    shift all keys at/after playhead by inserted duration
```

## 37.4 Punch-in overwrite

Workflow:

```text
mark punch in
mark punch out
preroll
record final camera
replace keys in range
blend boundary handles
```

Punch-in needs a temporary recording buffer. Do not mutate the live take until recording succeeds.

## 37.5 Layer

A later non-destructive layer system can support:

```text
base path
handheld layer
aim correction layer
FOV layer
noise layer
```

Do not block ordinary punch-in recording on the layer system.

---

# 38. Undo and redo

Every editor mutation must be reversible.

```cpp
class LLFlycamEditCommand
{
public:
    virtual ~LLFlycamEditCommand() = default;

    virtual void apply(
        LLFlycamTake&) = 0;

    virtual void undo(
        LLFlycamTake&) = 0;
};
```

Command stack:

```cpp
class LLFlycamEditHistory
{
public:
    void execute(
        std::unique_ptr<
            LLFlycamEditCommand> command);

    bool canUndo() const;
    bool canRedo() const;

    void undo();
    void redo();

    void markSavedRevision();
    bool isDirty() const;
};
```

For a drag, coalesce many hover updates into one command committed on mouse-up.

---

# 39. Named take library

```cpp
class LLFlycamTakeLibrary
{
public:
    bool add(
        std::shared_ptr<
            LLFlycamTake> take);

    bool remove(
        const LLUUID& take_id);

    std::shared_ptr<
        LLFlycamTake> find(
            const LLUUID& take_id);

    const std::vector<LLUUID>& order() const;
};
```

Each take has:

```text
stable UUID
name
description
duration
space
record anchor
thumbnail/bookmark metadata
dirty state
file path
```

Do not keep one unnamed mutable take as the only project model.

---

# 40. Director shot timeline

```cpp
struct LLDirectorCameraShot
{
    LLUUID mShotID;
    std::string mName;

    LLUUID mTakeID;

    F64 mSequenceStart = 0.0;
    F64 mSequenceDuration = 0.0;

    F64 mTakeIn = 0.0;
    F64 mTakeOut = 0.0;

    F64 mSpeed = 1.0;

    EFlycamPlacement mPlacement =
        EFlycamPlacement::
            ANCHOR_AT_PLAYBACK_START;

    LLFlycamAnchorBinding mAnchorOverride;
    bool mHasAnchorOverride = false;

    EFlycamClock mClock =
        EFlycamClock::
            DIRECTOR_SEQUENCE;

    F64 mWorldSpeed = 1.0;

    bool mHardCutIn = true;
    F64 mBlendIn = 0.0;
    F64 mBlendOut = 0.0;

    LLUUID mOperatorProfileID;
};
```

A scene contains:

```text
cast
subjects A/B
actor tracks
camera shots
world-time settings
take bindings
capture settings
```

---

# 41. Temporal Capture integration

## 41.1 Optional world-clock playback

A take should choose its time domain.

```text
Wall:
    camera runs independently of world slowdown

Presentation:
    camera slows/freezes with local world presentation

Director:
    camera follows sequence transport

Fixed Capture:
    camera evaluates exact output-frame time
```

Do not globally force all camera paths onto presentation time.

## 41.2 Fixed-frame control flow

```cpp
for (S64 frame_index = first_frame;
     frame_index <= last_frame;
     ++frame_index)
{
    LLTemporalFrameContext temporal =
        capture_coordinator.beginFrame(
            frame_index,
            output_fps);

    LLFlycamFrameContext camera_frame =
        makeFlycamFrameContext(
            temporal);

    const F64 take_time =
        shot.evaluateTakeTime(
            temporal.presentation_time);

    LLFlycamWorldPose camera_pose;

    flycam_evaluator.evaluateWorld(
        take,
        take_time,
        placement_frame,
        camera_pose);

    applyFlycamPose(
        camera_pose,
        1.f);

    evaluateWorldAt(
        temporal.presentation_time);

    renderOutputFrame();

    capture_coordinator.commitFrame();
}
```

Camera and visible world must use the same frozen temporal frame when the shot is locked to world presentation.

## 41.3 Motion blur

True shutter sub-sampling:

```cpp
for each output frame:
    for each shutter sample:
        t = frame_time +
            shutter_offset(sample)

        evaluate camera at t
        evaluate world at t
        render subframe
        accumulate

    normalize and write output frame
```

Moving only the camera N times while the world remains at one time is not correct general motion blur.

It may be useful as a deliberate camera-only effect, but it must be labeled as such.

---

# 42. Camera-path prefetch

An absolute evaluator can sample future camera poses.

```cpp
void collectPrefetchPoses(
    const LLFlycamTake& take,
    F64 current_time,
    F64 lookahead,
    F64 interval,
    std::vector<LLFlycamWorldPose>& output);
```

Use future poses to:

```text
prewarm texture priority
prewarm mesh priority
pin upcoming shot subjects
expand frustum recency
avoid whip-pan request storms
```

This is a later integration after camera correctness is proven.

---

# 43. Interchange recommendation

## 43.1 Priority order

1. **Blender Python package plus JSON/LLSD data**
2. **Nuke/Houdini-style `.chan`**
3. **glTF 2.0 camera-node transform animation**
4. **After Effects JSX**
5. **FBX through an external converter, not an in-viewer SDK dependency**

## 43.2 Blender Python package

Recommended export pair:

```text
take_name.flycam.json
take_name_import.py
```

Benefits:

- preserves quaternion transforms;
- preserves vertical FOV and custom lens metadata;
- can create the camera, animation action, markers, and origin empty;
- can preserve take/anchor metadata;
- avoids forcing the viewer to write `.blend`;
- can subtract a global reference origin for precision;
- can be versioned independently.

Blender uses a Cartesian coordinate system with Z up. Camera-axis mapping still needs an explicit conversion because the viewer camera basis is not the same as a Blender camera object's local viewing axis.

Export basis rather than converting through Euler angles.

## 43.3 `.chan`

Foundry documents that Nuke cameras and objects can import/export channel files containing per-frame Cartesian transform data. Foundry also explicitly notes that `.chan` is not a standardized file format.

Therefore export a named flavor:

```text
AlchemyNukeChanV1
```

Suggested columns:

```text
frame
translate_x
translate_y
translate_z
rotate_x
rotate_y
rotate_z
vertical_fov_degrees
```

Also write a sidecar:

```text
take.chan.json
```

containing:

```text
frame rate
rotation order
axis mapping
unit scale
reference global origin
FOV convention
take UUID
```

Because `.chan` is Euler/per-frame interchange, resample the take at the selected export FPS.

## 43.4 glTF 2.0

Core glTF can represent:

- a perspective camera;
- a node that instantiates it;
- animated node translation;
- animated node rotation quaternion;
- animation time in seconds;
- LINEAR, STEP, or CUBICSPLINE samplers;
- perspective `yfov`.

However, core glTF animation channels target only:

```text
translation
rotation
scale
weights
```

Core glTF does **not** provide a standard animation target for camera `yfov`.

Consequences:

```text
camera transform:
    standard glTF animation

static FOV:
    standard camera yfov

animated FOV:
    custom extras/extension or companion metadata
```

glTF camera convention:

```text
local +X:
    right

local +Y:
    camera up

local -Z:
    viewing direction
```

Viewer export should construct the target basis explicitly.

Large Second Life global coordinates should be localized around a reference origin before writing float translations. Store the subtracted origin in `extras`.

## 43.5 FBX

The Autodesk FBX SDK supports:

```text
cameras
scene axis systems
scene units
node transforms
animation curves
```

But adding FBX SDK directly to the viewer would add:

```text
large dependency
platform/build integration
SDK distribution/licensing review
binary compatibility burden
maintenance cost
```

Recommendation:

```text
Flycam take
    → Blender Python or standalone converter
    → FBX
```

Do not make FBX SDK a prerequisite for the core recorder.

## 43.6 After Effects

Export:

```text
JSON + JSX script
```

The script creates:

```text
camera layer
position keys
orientation/rotation keys
zoom/FOV-derived values
optional point-of-interest keys
markers
```

A script is preferable to copying formatted keyframe text because it can preserve metadata and handle coordinate conversion explicitly.

---

# 44. Axis conversion policy

Never export Euler angles as the canonical interchange representation.

Start from viewer camera basis:

```text
viewer forward
viewer left
viewer up
```

Build target basis:

```text
target right =
    -viewer left

target up =
    viewer up

target forward =
    viewer forward
```

For a target camera that looks down local `-Z`:

```text
target local -Z =
    target forward

target local +Y =
    target up

target local +X =
    target right
```

Create the target quaternion from that orthonormal basis.

Unit-test:

```text
identity camera
90-degree yaw
90-degree pitch
90-degree roll
combined rotation
round-trip through import/export
```

---

# 45. UI overhaul

Keep the shared panel idiom.

Recommended structure:

```text
Transport
Take Library
Record
Anchor
Playback
Timeline
Key Inspector
Path Tools
Interchange
Diagnostics
```

Because the Director Console tab has fixed height, use:

```text
scroll container
collapsible sections
timeline with fixed minimum height
horizontal timeline scrolling
```

## 45.1 Main panel sketch

```text
┌ Flycam Recorder ─────────────────────────────────────────┐
│ Take: [Hero Orbit                    ▼] [New] [Duplicate]│
│ [Record] [Pause] [Stop] [Play] [<] [>]                 │
│                                                        │
│ Record                                                  │
│ Space:  [Relative to Anchor ▼]                          │
│ Anchor: [Ghost: Hero A       ▼] [Pick Current]          │
│ Point:  [Chest ▼]   Orientation: [Yaw ▼]                │
│ Mode:   [Replace Take ▼]                                │
│                                                        │
│ Playback                                                │
│ Placement: [Around Anchor at Start ▼]                   │
│ Target:    [Ghost: Hero B ▼]                            │
│ Missing:   [Pause and Wait ▼]                           │
│ Clock:     [Director ▼]  Speed: [1.000]                 │
│                                                        │
│ Timeline                                                │
│ |◆------◆----------◆--C--◆---------------------------| │
│                                                        │
│ Selected Key                                            │
│ Time [4.000]  FOV [47.0°]  Interp [Smooth Auto ▼]      │
│ [Set from Camera] [Look at Subject] [Delete]            │
│                                                        │
│ [Edit in World] [Bake Here] [Export] [Save]            │
└────────────────────────────────────────────────────────┘
```

## 45.2 Status

Always display:

```text
current camera owner
recording space
stable anchor ID
resolved runtime ID
anchor valid/lost
placement mode
clock mode
playhead
duration
raw sample count
edited key count
gap count
cut count
take dirty state
discontinuity serial
```

---

# 46. Existing settings migration

Keep current settings as defaults:

```text
FlycamRecSampleRate
FlycamRecSpeed
FlycamRecLoopMode
FlycamRecSmooth
FlycamRecAnchorMode
FlycamRecFollowAnchor
FlycamRecUseOperator
```

Add:

```text
FlycamRecDefaultSpace
FlycamRecDefaultPlacement
FlycamRecDefaultAnchorOrientation
FlycamRecDefaultAnchorLossPolicy
FlycamRecDefaultClock
FlycamRecGapThreshold
FlycamRecResampleOnStop
FlycamRecSimplifyOnStop
FlycamRecTimelineSnapFPS
FlycamRecVisualizePath
FlycamRecAnchorSmoothing
FlycamRecUseLocalFOVSetter
```

The old anchor settings map to v2 compatibility behavior. New takes use the v3 per-take model.

---

# 47. File and function patch map

| File | Change |
|---|---|
| `llflycamrecorder.h` | Keep façade; move to F64 time; add take/record/placement APIs |
| `llflycamrecorder.cpp` | Local FOV setter, absolute transport, true relative capture, lifecycle |
| `llflycamtake.*` | Version-3 take, key, lens, anchor-track model |
| `llflycamanchor.*` | Stable bindings, resolver, coordinate conversion |
| `llflycamevaluator.*` | Binary lookup, Hermite, rotation, lens, derivatives, arc length |
| `llflycamtakeio.*` | v2/v3 load, validation, atomic save, import/export |
| `llflycameditmodel.*` | Selection, mutations, undo/redo, observer signals |
| `alpanelflycamrecorder.*` | Take library, anchor controls, timeline, inspector |
| `panel_flycam_recorder.xml` | Collapsible editor sections |
| `alflycamtimeline.*` | Timeline control |
| `altoolflycampathedit.*` | In-world path editing |
| `llappviewer.cpp` | Frame context, lifecycle, existing dispatch integration |
| `llviewercamera.*` | Ideally no algorithm change; use existing no-broadcast setter |
| `llpresentationtime.*` | Read immutable frame context only |
| `alghoststudio.*` | Stable instance-to-runtime resolver adapter |
| `settings.xml` | Defaults and editor settings |
| `CMakeLists.txt` | New source/header files |
| test CMake/source lists | New unit tests |

---

# 48. Immediate hardening patch

Before large architecture work, land a small patch.

## 48.1 F64 time

```cpp
struct Keyframe
{
    F64 mTime = 0.0;
    ...
};

F64 mPlayhead = 0.0;
```

## 48.2 Local FOV

```cpp
cam->setViewNoBroadcast(
    validated_fov);
```

## 48.3 Stable selection latch

At playback start:

```cpp
mPlaybackAnchorBinding =
    captureBindingFromCurrentUI();

mHaveLiveAnchor = false;
```

During playback:

```text
resolve stable binding
do not query mutable selection again
do not silently use self
```

## 48.4 Finite validation

Validate:

```text
time
position
quaternion norm
FOV
anchor position/yaw
```

## 48.5 Lifecycle

Stop and release on:

```text
teleport start
logout
region teardown where current camera cannot remain valid
explicit Escape
failed camera application
```

## 48.6 Operator reset

Reset on:

```text
seek
loop wrap
ping-pong reverse
anchor rebind
camera cut
start/stop
```

This patch is independently shippable and lowers the risk of later work.

---

# 49. Phased implementation roadmap

## Phase 0 — source baseline and runtime telemetry

Work:

```text
freeze exact local files and commit
log camera owner
log recorder state
log record/play clock
log selected and resolved anchor IDs
log FOV setter path
log discontinuity reasons
```

Gate:

```text
one runtime trace explains who wrote the camera and which pose was recorded
for every frame of a controlled test
```

## Phase 1 — hardening without format change

Work:

```text
F64 internal time
setViewNoBroadcast
FOV/quaternion finite validation
stable selected anchor latch
no silent fallback
binary key lookup
teleport/logout/Escape release
operator reset completeness
transactional load
atomic save
```

Gate:

```text
all version-2 takes retain existing visual behavior
except corrected target-loss and network side effects
```

## Phase 2 — version-3 take model

Work:

```text
stable take/key UUIDs
per-take settings
world vs anchor-local space
stable anchor binding
placement policy
v2 compatibility loader
v3 writer
```

Gate:

```text
v2 loads
v3 round-trips
unknown/new schema fails closed
```

## Phase 3 — true relative recording

Work:

```text
resolve anchor for each rendered camera sample
store local camera pose
record optional anchor trajectory
fixed-at-start placement
live-follow placement
recorded-track placement
bake-to-world
```

Gate:

```text
record behind a walking avatar
replay around a different walking avatar
no original actor translation remains in the local path
```

## Phase 4 — absolute-time playback and Temporal Capture

Work:

```text
clock enum
absolute transport
presentation-time option
Director time
fixed-frame time
camera discontinuity service
deterministic operator policy
```

Gate:

```text
same frame index produces identical camera pose regardless of render duration
```

## Phase 5 — interpolation quality

Work:

```text
time-aware Hermite
per-key interpolation
per-key ease
robust SQUAD cache
arc-length table
constant local speed
analytic derivatives
```

Gate:

```text
sparse irregular keys do not overshoot unexpectedly
constant-speed mode has bounded measured speed error
```

## Phase 6 — timeline and key inspector

Work:

```text
custom timeline control
stable selection
retime/add/delete/duplicate
snap
key inspector
undo/redo
shared-panel model signals
```

Gate:

```text
all edits round-trip and undo exactly
both panel hosts remain synchronized
```

## Phase 7 — in-world editor

Work:

```text
spline/frustum rendering
screen-space marker picking
drag translation
insert/set-from-camera
relative display/edit transform
focus rays
```

Gate:

```text
moving a local key under a rotated placement frame updates the stored local
coordinate correctly and does not alter simulator objects
```

## Phase 8 — take library and Director shots

Work:

```text
named takes
shot/cut timeline
per-shot target/placement/clock
scene persistence
multi-camera sequence
```

Gate:

```text
Director scene reload reproduces the same take bindings and cuts
```

## Phase 9 — interchange

Work:

```text
Blender Python + JSON
AlchemyNukeChanV1 + sidecar
glTF transform export + FOV metadata
After Effects JSX
external FBX bridge
```

Gate:

```text
round-trip basis, position, orientation, frame timing, and FOV tests
```

---

# 50. Falsifiable validation matrix

## 50.1 World recording

```text
static camera
slow dolly
fast flycam
roll
FOV zoom
long take
region boundary
```

Pass:

```text
world replay matches recorded global pose within declared tolerances
```

## 50.2 True relative recording

```text
stationary avatar
walking avatar
turning avatar
walking + turning
sitting avatar
Ghost Studio actor
remote avatar
object anchor
```

Pass:

```text
local camera offset and orientation are independent of original anchor motion
```

## 50.3 Placement modes

```text
original world
anchor at play start
live anchor
recorded anchor track
manual frame
bake to world
```

## 50.4 Anchor orientation

```text
translation only
yaw
full orientation
```

## 50.5 Anchor loss

```text
selection cleared
target derezzes
target dies
Ghost runtime replaced
region crossing
teleport
```

Pass:

```text
no snap to origin
no silent self rebind
declared policy executes
```

## 50.6 Time

```text
15 FPS
30 FPS
60 FPS
144 FPS
irregular hitches
presentation 0×
presentation 0.2×
fixed-frame 24/30/60 FPS
reverse
loop
ping-pong
seek
```

## 50.7 Files

```text
v2 load
v3 load
newer version
empty file
truncated XML
NaN
zero quaternion
duplicate time
out-of-order time
extreme FOV
huge key count
disk-full save
```

## 50.8 Editing

```text
insert
delete
multi-retime
snap
copy/paste
undo/redo
punch-in
relative in-world drag
save/reload
```

## 50.9 Interchange

```text
identity orientation
90° yaw
90° pitch
90° roll
combined orientation
large global origin
animated FOV
nonuniform timing
```

---

# 51. Unit-test starting points

## 51.1 Moving anchor removal

```cpp
void testRelativeRecordingRemovesAnchorMotion()
{
    LLFlycamAnchorFrame anchor_a;
    anchor_a.mPositionGlobal =
        LLVector3d(100.0, 100.0, 20.0);
    anchor_a.mRotationWorld =
        yawQuaternion(0.0f);
    anchor_a.mValid = true;

    LLFlycamAnchorFrame anchor_b;
    anchor_b.mPositionGlobal =
        LLVector3d(110.0, 100.0, 20.0);
    anchor_b.mRotationWorld =
        yawQuaternion(F_PI_BY_TWO);
    anchor_b.mValid = true;

    const LLVector3d camera_local(
        -3.0,
        1.0,
        1.8);

    const LLVector3d camera_world_a =
        LLFlycamSpace::localPointToWorld(
            camera_local,
            anchor_a);

    const LLVector3d camera_world_b =
        LLFlycamSpace::localPointToWorld(
            camera_local,
            anchor_b);

    const LLVector3d recovered_a =
        LLFlycamSpace::worldPointToLocal(
            camera_world_a,
            anchor_a);

    const LLVector3d recovered_b =
        LLFlycamSpace::worldPointToLocal(
            camera_world_b,
            anchor_b);

    ensure_approximately_equals(
        "same local camera under moving anchor",
        (recovered_a -
         recovered_b).length(),
        0.0,
        1.0e-6);
}
```

## 51.2 Place around new avatar

```cpp
void testPlaceAtPlaybackStart()
{
    LLFlycamAnchorFrame original;
    original.mPositionGlobal =
        LLVector3d(100.0, 100.0, 20.0);
    original.mRotationWorld =
        yawQuaternion(0.f);
    original.mValid = true;

    LLFlycamAnchorFrame target;
    target.mPositionGlobal =
        LLVector3d(1000.0, 500.0, 40.0);
    target.mRotationWorld =
        yawQuaternion(F_PI_BY_TWO);
    target.mValid = true;

    const LLVector3d camera_world(
        97.0,
        101.0,
        21.8);

    const LLVector3d local =
        LLFlycamSpace::worldPointToLocal(
            camera_world,
            original);

    const LLVector3d placed =
        LLFlycamSpace::localPointToWorld(
            local,
            target);

    // Exact expected sign depends on viewer basis;
    // verify against a calibrated basis test.
    ensure(
        "placed pose finite",
        placed.isFinite());
}
```

## 51.3 Fixed frame

```cpp
void testFixedFrameIndependentOfWallDuration()
{
    LLFlycamFrameContext frame_a;
    frame_a.mFixedCapture = true;
    frame_a.mCaptureFrame = 120;
    frame_a.mOutputFPS = 30.0;
    frame_a.mWallTime = 100.0;

    LLFlycamFrameContext frame_b =
        frame_a;

    frame_b.mWallTime = 1000.0;

    const F64 time_a =
        resolveFlycamClock(
            frame_a,
            EFlycamClock::
                FIXED_CAPTURE_FRAME);

    const F64 time_b =
        resolveFlycamClock(
            frame_b,
            EFlycamClock::
                FIXED_CAPTURE_FRAME);

    ensure_approximately_equals(
        "fixed camera time",
        time_a,
        time_b,
        1.0e-12);
}
```

## 51.4 No mutable selection dependency

```cpp
void testSelectionBindingIsStable()
{
    LLFlycamAnchorBinding binding =
        captureSelectedObjectBinding();

    changeViewerSelectionToAnotherObject();

    LLFlycamAnchorFrame frame;

    ensure(
        "resolves original selected ID",
        resolver.resolve(
            binding,
            frame));

    ensure_equals(
        "runtime identity unchanged",
        frame.mResolvedRuntimeID,
        binding.mObjectID);
}
```

---

# 52. Risk register

| Rank | Risk | Severity | Mitigation |
|---:|---|---|---|
| 1 | Wrong quaternion/order convention mirrors relative paths | Critical | Isolated helpers and basis tests |
| 2 | Existing v2 takes change behavior unexpectedly | Critical | Dedicated compatibility path |
| 3 | Moving-anchor recording remains world-motion contaminated | Critical | Per-sample local conversion |
| 4 | Target loss silently rebinds | Critical | Stable binding and explicit policy |
| 5 | Fixed capture still uses wall-integrated playhead | Critical | Absolute frame context |
| 6 | Camera FOV playback sends network updates | High | No-broadcast setter |
| 7 | Camera owner remains active after teleport/logout | Critical | Lifecycle hooks and emergency release |
| 8 | Anchor-local edit is applied in wrong display frame | High | World/local inverse tests |
| 9 | Spline overshoot collides with scene | High | Linear/clamped modes and visible path |
| 10 | SQUAD edge case yields NaN | High | Checked math and fallback |
| 11 | Operator creates impulse at cut/loop/rebind | High | Discontinuity reset and derivatives |
| 12 | Remote avatar jitter shakes camera | High | Optional filter or recorded ghost |
| 13 | Timeline indices become stale after edits | High | Stable key UUIDs |
| 14 | Save corruption loses take | High | Atomic replacement |
| 15 | Malformed XML consumes excessive memory | High | Preflight caps and temporary model |
| 16 | Shared panel instances diverge | Medium | Observer-backed shared model |
| 17 | glTF export promises animated FOV that core cannot encode | High | Companion metadata, explicit limit |
| 18 | `.chan` flavor is interpreted differently by a DCC | High | Named flavor and sidecar |
| 19 | FBX SDK bloats viewer maintenance | Medium-high | External converter |
| 20 | Camera-only shutter samples misrepresent moving world | High | Full temporal subframe evaluation |

---

# 53. Definition of done

The overhaul is complete only when:

1. Existing world-space recording remains available.
2. Existing version-2 takes load.
3. New takes use `F64` time.
4. Playback uses `setViewNoBroadcast()`.
5. A selected anchor is captured as a stable identity.
6. Changing or clearing selection during playback does not retarget the camera.
7. Missing anchors never resolve to origin.
8. Missing anchors never silently resolve to self.
9. Anchor-local recording converts each camera sample against the same-frame anchor.
10. A moving original subject leaves no residual world translation in the local camera path.
11. Fixed-at-start placement produces a fixed world move around the chosen avatar.
12. Live placement follows the chosen avatar once, without doubled original motion.
13. Original-world reconstruction is available when an anchor track was recorded.
14. Relative takes can be baked into new world takes.
15. Playback is evaluated from absolute wall, presentation, Director, or capture time.
16. Fixed output frame time is independent of render duration.
17. Cuts, seeks, loops, rebinds, and projection changes raise temporal reset events.
18. Interpolation uses real key timing.
19. Users can select linear/clamped/smooth interpolation per segment.
20. Quaternion sequences are finite, normalized, and hemisphere-canonical.
21. FOV is finite and bounded.
22. Load is transactional and bounded.
23. Save is atomic.
24. Timeline edits use stable key IDs.
25. Undo/redo covers every mutation.
26. In-world editing does not rez or mutate simulator objects.
27. Shared panel hosts remain synchronized.
28. Ghost Studio anchors use stable instance identity.
29. Director scenes persist take and shot bindings.
30. Blender/`.chan` exports document origin, axes, units, FPS, and FOV.
31. glTF export clearly labels animated-FOV limitations.
32. Feature-off behavior leaves ordinary camera dispatch unchanged.

---

# 54. Recommended first coding series

```text
Commit 1
    F64 time
    finite validation
    setViewNoBroadcast
    binary key search
    operator reset fixes

Commit 2
    stable anchor binding
    remove silent self fallback
    anchor-loss state/policy
    teleport/logout/Escape cleanup

Commit 3
    version-3 take model
    stable take/key IDs
    per-take settings
    v2 compatibility reader

Commit 4
    per-sample anchor-local recording
    fixed-at-start playback
    live-follow playback
    optional recorded anchor track

Commit 5
    absolute clock transport
    presentation/Director/fixed-frame context
    discontinuity service

Commit 6
    time-aware Hermite
    robust rotation cache
    derivatives
    arc-length mode

Commit 7
    timeline
    inspector
    undo/redo
    save/load UI

Commit 8
    in-world editor
    bake/retarget
    focus tools

Commit 9
    named take library
    Director shot timeline

Commit 10
    Blender/JSON
    .chan + sidecar
    glTF transform export
    After Effects script
```

---

# 55. Final recommendation

Do not rewrite the recorder.

Its foundational decision—record the final render camera—is correct.

Upgrade it around three source-level corrections:

```text
1. Record true anchor-local poses at every sample.

2. Replace integrated/clamped playhead time with absolute selectable time.

3. Give anchors, keys, takes, and camera ownership stable identities and explicit lifecycle policies.
```

The desired workflow then becomes exact:

```text
Record camera relative to Avatar A
    ↓
store camera-local motion
    ↓
choose Avatar B at playback
    ↓
resolve Avatar B once
    ↓
instantiate the camera move as a fixed world path around Avatar B
```

or:

```text
Record camera relative to Avatar A
    ↓
store camera-local motion
    ↓
bind Avatar B live
    ↓
evaluate Avatar B every frame
    ↓
camera rides Avatar B without carrying Avatar A's original world motion
```

That architecture also creates the correct base for:

```text
keyframe editing
in-world spline editing
focus pulls
named takes
multi-camera cuts
world-time playback
fixed-frame capture
true shutter sub-sampling
asset prefetch
Blender/Nuke interchange
```

---

# Appendix A — External primary-source findings

## A.1 glTF

The Khronos glTF 2.0 specification defines:

```text
camera instantiated by a node
perspective vertical yfov
camera local +X right
camera local +Y up
camera looks toward local -Z
animation channels for node translation, rotation, scale, and morph weights
time inputs in seconds
LINEAR, STEP, and CUBICSPLINE interpolation
```

Core animation does not target camera `yfov`, so animated FOV requires companion metadata or an extension.

**Primary source:** Khronos glTF 2.0 Specification, camera and animation sections.

## A.2 Nuke `.chan`

Foundry documents importing and exporting channel files for cameras and objects. Foundry also notes that `.chan` is not a standardized file format.

**Primary source:** Foundry Nuke documentation, “Applying Tracks to an Object.”

## A.3 FBX

Autodesk documents FBX SDK support for cameras, axis systems, scene units, node transforms, and animation curves.

**Primary source:** Autodesk FBX SDK documentation, “Supported Scene Elements.”

## A.4 Blender

Blender documents a Cartesian coordinate system with Z up.

**Primary source:** Blender Manual, “Viewpoint.”

---

# Appendix B — Source evidence ledger

## Supplied private/current source brief

```text
FLYCAM_RECORDER_DEEP_RESEARCH_BRIEF.md
```

It contains the complete current:

```text
llflycamrecorder.h
llflycamrecorder.cpp
alpanelflycamrecorder.h
alpanelflycamrecorder.cpp
panel_flycam_recorder.xml
idle camera-dispatch integration
settings
known limitations
```

## Public viewer cross-checks

```text
llviewerjoystick.h/.cpp
llappviewer.cpp
llagentpilot.h/.cpp
llviewercamera.h
llagent.h
lltimer.h
llquaternion.h
llviewerobject.h
llvoavatar.h
```

Important public contracts cross-checked:

```text
LLTimer exposes F64 elapsed time.
LLAgent converts global-double and agent-space positions.
LLViewerCamera exposes setViewNoBroadcast().
The stock flycam already tracks global-double position, quaternion, and FOV.
The camera dispatch runs before apparent-angle/LOD and audio-listener updates.
```
