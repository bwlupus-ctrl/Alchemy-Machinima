# Director Animation and Control for a Cinematic Second Life Viewer

## Deep Research and Implementation Blueprint for Alchemy-Machinima

**Target repository:** [`bwlupus-ctrl/Alchemy-Machinima`](https://github.com/bwlupus-ctrl/Alchemy-Machinima), branch `develop`  
**Upstream lineage:** Alchemy Viewer / Linden Lab Second Life Viewer  
**Primary constraint:** avatar animation remains grounded in Second Life `.anim` assets and `.bvh` import  
**Intended use:** implementation reference for later coding in the viewer  
**Document status:** architecture recommendation and phased engineering plan

---

## Executive verdict

The correct design is not a new character-animation system. It is a **director and sequencing layer around the viewer's existing motion engine**.

The viewer already contains the hard runtime machinery required to play Second Life animations:

- `LLMotionController` creates, activates, stops, fades, updates, and blends motions.
- `LLKeyframeMotion` loads and evaluates uploaded animation assets.
- `LLPose` and `LLPoseBlender` combine joint states.
- animation priority and per-joint signatures decide which motion controls each joint;
- uploaded asset metadata supplies loop points, duration, ease-in, ease-out, hand pose, expression/emote, and joint tracks;
- `LLVOAvatar` and `LLCharacter` expose the avatar-facing start/stop interface;
- simulator animation state and local motion state are already distinct enough to support a controlled local layer.

The cinematic client should therefore add four systems above the existing runtime:

1. **Director Sequence Model**  
   Stores tracks, clips, cues, actors, cameras, transforms, and shot state independently of live playback.

2. **Director Evaluation Clock**  
   Converts a sequence time into a deterministic set of desired avatar clips and local property values.

3. **Actor Playback Adapter**  
   Reconciles the desired clip set with `LLMotionController`, without modifying `.anim` data or replacing native pose blending.

4. **Authority and Restoration Layer**  
   Separates live simulator state, AO state, local preview state, clone state, and Director-owned state, then restores everything cleanly on exit.

The crucial architectural split is:

```text
Avatar skeletal animation
    = existing .anim clips evaluated by LLKeyframeMotion
    = scheduled and reconciled by Director Mode

Camera, clone placement, visibility, lights, environment, and shot properties
    = ordinary Director-owned property tracks
    = evaluated directly from sequence time
```

This gives the viewer a Blender-like timeline without claiming Blender-like skeletal editing.

---

# 1. Scope

## 1.1 Primary goal

Build a viewer-native animation and control workspace that lets a filmmaker:

- assign uploaded `.anim` assets to live avatars or client-only clones;
- place clips on a timeline;
- preview and rehearse multi-actor sequences;
- coordinate avatar clips with camera, clone movement, visibility, environment, and cues;
- pause, scrub, seek, loop, record takes, and restore normal viewer behavior;
- detect priority and joint conflicts before capture;
- produce deterministic-enough local playback for machinima;
- retain full compatibility with Second Life's current animation asset system.

## 1.2 Non-goals

The first implementation should not attempt:

- arbitrary bone-key editing inside the viewer;
- a replacement animation asset format;
- runtime export of an entire sequence as one `.anim`;
- generalized retargeting between unrelated skeletons;
- Blender-style F-curves for every avatar joint;
- a new full-body IK solver as the main animation source;
- mathematically exact NLA blending independent of SL priorities;
- simulator-authoritative choreography of arbitrary other residents;
- editing uploaded asset contents in place;
- claiming network synchronization tighter than the Second Life protocol provides.

## 1.3 Design principle

> The viewer should behave like a non-destructive cinematic control surface around Second Life, not like a replacement for Second Life's animation runtime.

---

# 2. Source baseline

## 2.1 Repositories

Primary:

- [Alchemy-Machinima, `develop`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/tree/develop)
- [Alchemy Viewer](https://github.com/AlchemyViewer/Alchemy)
- [Second Life official viewer](https://github.com/secondlife/viewer)
- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)

Relevant existing source directory:

- [`indra/llcharacter`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/tree/develop/indra/llcharacter)

Important files:

```text
indra/llcharacter/llmotion.h
indra/llcharacter/llmotion.cpp
indra/llcharacter/llmotioncontroller.h
indra/llcharacter/llmotioncontroller.cpp
indra/llcharacter/llkeyframemotion.h
indra/llcharacter/llkeyframemotion.cpp
indra/llcharacter/llpose.h
indra/llcharacter/llpose.cpp
indra/llcharacter/lljointstate.h
indra/llcharacter/llbvhloader.h
indra/llcharacter/llbvhloader.cpp
indra/llcharacter/llanimationstates.h
indra/llcharacter/llanimationstates.cpp

indra/newview/llvoavatar.h
indra/newview/llvoavatar.cpp
indra/newview/llvoavatarself.h
indra/newview/llvoavatarself.cpp
indra/newview/llviewermessage.cpp
indra/newview/llviewerassetstorage.cpp
indra/newview/llfloateranimpreview.*
indra/newview/llpreviewanim.*
indra/newview/llagent.*
indra/newview/llagentcamera.*
```

Clone integration targets expected in the fork:

```text
indra/newview/llghostavatar.*
indra/newview/alghoststudio.*
indra/newview/llactormover.*
```

## 2.2 Official behavior references

Useful Second Life references:

- [Internal Animation Format](https://wiki.secondlife.com/wiki/Internal_Animation_Format)
- [Animation Upload Priority](https://wiki.secondlife.com/wiki/Animation_Upload_Priority)
- [Animation Upload Ease In and Ease Out](https://wiki.secondlife.com/wiki/Animation_Upload_Ease_In_and_Ease_Out)
- [Animation Upload Loop Controls](https://wiki.secondlife.com/wiki/Animation_Upload_Loop_Controls)
- [How to create animations](https://wiki.secondlife.com/wiki/How_to_create_animations)
- [Animation Test](https://wiki.secondlife.com/wiki/Animation_Test)

The official format documentation describes the practical pipeline:

```text
BVH parse
  → LLKeyframeMotion data
  → upload options applied
  → serialized animation asset
  → asset fetch
  → LLKeyframeMotion deserialize
  → runtime pose evaluation
```

This is the pipeline Director Mode should use rather than bypass.

---

# 3. What the existing runtime already provides

## 3.1 `LLMotion`

`LLMotion` is the runtime abstraction for one animation behavior. A motion reports:

- loop state;
- duration;
- ease-in and ease-out durations;
- priority;
- blend type;
- minimum pixel area;
- activation and deactivation behavior;
- per-update joint state.

A motion owns a pose and runtime timing state. Uploaded animations normally become `LLKeyframeMotion` instances.

### Implication

Director Mode should not directly write joint transforms for normal clip playback. It should ask the existing motion system to run the asset, then control **when** and **under which ownership policy** it runs.

## 3.2 `LLMotionController`

`LLMotionController` is the per-character scheduler and blender.

Observed responsibilities include:

- a registry mapping motion UUIDs to constructors;
- default creation of `LLKeyframeMotion` for unregistered animation UUIDs;
- asynchronous asset loading;
- active, loaded, loading, and deprecated motion collections;
- activation by UUID and start offset;
- graceful or immediate stopping;
- ease-in/ease-out weighting;
- regular and additive blend passes;
- per-joint signature conflict handling;
- LOD-driven fade behavior;
- automatic timed stop for non-looping clips;
- pause and unpause;
- time factor;
- loaded-motion purging;
- pose blending and application.

The existing public behavior is highly useful:

```cpp
bool startMotion(const LLUUID& id, F32 start_offset);
bool stopMotionLocally(const LLUUID& id, bool stop_immediate);
void pauseAllMotions();
void unpauseAllMotions();
void setTimeFactor(F32 factor);
void updateMotions(bool force_update);
```

### Important limitation: the native clock is not a sequencer clock

`updateMotions()` advances `mAnimTime` from an elapsed frame timer. The code contains a `setTimeStep()` path, but comments explicitly state that the time-quantum optimization is disabled or unreliable.

Therefore:

- do not base deterministic capture on `setTimeStep()`;
- do not attempt arbitrary timeline scrubbing by mutating `mAnimTime` from UI code;
- do not assume reverse playback is supported;
- do not assume repeated seeks are side-effect-free.

Director Mode needs a higher-level clock and a reconciliation method.

## 3.3 `LLKeyframeMotion`

`LLKeyframeMotion`:

- requests animation asset data if not cached;
- deserializes the binary animation asset;
- constructs joint-state tracks;
- sets loop in/out points;
- evaluates joint positions and rotations over time;
- can trigger an associated emote;
- supports constraints present in the asset;
- produces a pose consumed by the motion controller.

### Implication

The `.anim` asset is more than raw transforms. Director tooling should inspect and display:

- duration;
- loop flag;
- loop-in and loop-out;
- ease-in;
- ease-out;
- base priority;
- animated joint list;
- joint usage;
- associated hand pose;
- associated emote/expression;
- asset availability and load state.

## 3.4 Priority and joint ownership

Second Life priority is not simply "one clip wins for the whole avatar." The runtime has per-joint signatures and blends poses in motion order and priority context.

Practical rules from official documentation and viewer behavior:

- uploaded priority applies to the animation asset;
- only joints keyed by that asset participate;
- unkeyed joints remain available to other animations;
- higher priority normally wins on overlapping keyed joints;
- equal-priority ordering can make the later-started motion win;
- ease-in/ease-out affects weight but does not convert incompatible tracks into a sophisticated NLA blend;
- built-in locomotion, breathing, typing, AO clips, gestures, and scripted animations can all conflict.

### Director implication

A conflict analyzer should reason at **joint-set level**, not only at clip level.

---

# 4. Recommended product model

## 4.1 Director project

A project is viewer-local data:

```yaml
project:
  version: 1
  name: "Warehouse Scene"
  frame_rate: 30
  start_time: 0.0
  end_time: 42.0
  actors: []
  tracks: []
  markers: []
  takes: []
  capture_profile: {}
```

It should not contain simulator secrets, asset binary payloads, or assumptions that every referenced UUID remains accessible.

## 4.2 Actor

An actor is a Director abstraction around a target:

```cpp
enum class LLDirectorActorKind
{
    SELF_AVATAR,
    REMOTE_AVATAR,
    GHOST_AVATAR,
    CONTROL_AVATAR,
    LOCAL_PROP
};
```

Recommended actor record:

```cpp
struct LLDirectorActor
{
    LLUUID actor_id;              // Director-local identity
    LLDirectorActorKind kind;
    LLUUID target_object_id;      // viewer object UUID when applicable
    LLHandle<LLViewerObject> target;
    std::string display_name;

    LLDirectorAuthorityMode authority;
    LLDirectorRestoreSnapshot restore_snapshot;

    bool enabled;
    bool allow_local_animation;
    bool allow_network_animation_request;
    bool lock_ao;
    bool lock_locomotion;
};
```

## 4.3 Authority modes

```cpp
enum class LLDirectorAuthorityMode
{
    OBSERVE_ONLY,
    LOCAL_PREVIEW,
    LOCAL_OVERRIDE,
    NETWORK_REQUEST_SELF,
    GHOST_FULL_CONTROL
};
```

Meaning:

- `OBSERVE_ONLY`: record or display state but do not intervene.
- `LOCAL_PREVIEW`: local motion only; no simulator request.
- `LOCAL_OVERRIDE`: Director owns selected local channels while active.
- `NETWORK_REQUEST_SELF`: may request self-avatar animations through normal viewer protocol.
- `GHOST_FULL_CONTROL`: client-only actor; Director owns all animation and transform state.

Remote real avatars should default to `OBSERVE_ONLY`. Any local visual override of another resident must be explicitly labeled as local-only and should not imply simulator authority.

---

# 5. Hybrid timeline architecture

## 5.1 Track families

The timeline should support two fundamentally different evaluation models.

### Clip tracks

Used for avatar animations:

```text
Actor Animation Track
  ├── animation asset clip
  ├── pose clip
  ├── transition clip
  └── cue/event clip
```

Clip tracks schedule existing `.anim` assets.

### Property tracks

Used for locally controllable numeric or discrete properties:

```text
Camera Transform
Camera FOV
Focus Distance
Ghost Transform
Visibility
Local Light Intensity
Environment Preset
Post-process Value
Capture Marker
```

Property tracks can use keyframes and interpolation.

## 5.2 Clip record

```cpp
struct LLDirectorAnimClip
{
    LLUUID clip_id;
    LLUUID asset_id;
    LLUUID actor_id;

    F64 sequence_start;
    F64 sequence_end;

    F64 source_in;
    F64 source_out;

    bool loop_region;
    F32 playback_rate;

    LLDirectorClipStartPolicy start_policy;
    LLDirectorClipStopPolicy stop_policy;
    LLDirectorTransitionPolicy transition_policy;

    bool suppress_ao;
    bool suppress_locomotion;
    bool local_only;

    std::string label;
};
```

### Do not duplicate immutable asset metadata unnecessarily

Priority, loop metadata, easing, and animated joints belong to the asset. The project may cache them for analysis, but the asset remains authoritative.

The following should be treated as sequence-level behavior rather than false edits to the asset:

- timeline trim;
- source offset;
- restart policy;
- scheduling;
- pre-roll;
- actor ownership;
- whether Director suppresses competing systems;
- transition warning classification.

## 5.3 Property keyframes

```cpp
template<typename T>
struct LLDirectorKeyframe
{
    F64 time;
    T value;
    LLDirectorInterpolation interpolation;
};
```

Recommended interpolation:

- stepped;
- linear;
- smooth cubic;
- Bezier for camera and transform properties;
- quaternion slerp for rotations.

Do not apply this keyframe model to internal avatar joints in the initial system.

---

# 6. Director evaluation clock

## 6.1 Why a separate clock is required

A sequencer must support:

- play;
- pause;
- stop;
- loop range;
- seek;
- frame stepping;
- preroll;
- capture at a declared frame rate;
- scene evaluation while the viewer frame rate varies.

The native motion controller advances from real elapsed time. It is suitable for normal playback, but not sufficient as the authoritative sequence clock.

## 6.2 Clock interface

```cpp
class LLDirectorClock
{
public:
    enum class Mode
    {
        STOPPED,
        REALTIME_PLAYBACK,
        SCRUBBING,
        FRAME_STEP,
        CAPTURE
    };

    void play();
    void pause();
    void stop();
    void seek(F64 seconds);
    void stepFrames(S32 count);

    F64 sequenceTime() const;
    F64 previousSequenceTime() const;
    F64 delta() const;
    S64 frameIndex() const;

    void setFrameRate(F64 fps);
    void setLoopRange(F64 in_time, F64 out_time);
};
```

## 6.3 Realtime mode

Realtime mode advances sequence time from a monotonic timer.

```text
sequence_time += real_delta × director_playback_rate
```

The actor adapter starts native motions with appropriate offsets.

## 6.4 Capture mode

Capture mode advances in exact sequence increments:

```text
sequence_time = frame_index / project_frame_rate
```

However, this does not automatically make native motion evaluation deterministic because `LLMotionController` still uses its own timer.

Three implementation options exist.

### Option A — restart/reconcile at capture boundaries

At take start:

1. preload all assets;
2. stop Director-owned motions;
3. start each active clip with calculated source offset;
4. allow native controller to advance normally;
5. capture frames according to the Director frame clock.

This is least invasive and appropriate for the first implementation.

Weakness: viewer render timing and motion timing remain loosely coupled.

### Option B — add an externally driven animation-time mode

Extend `LLMotionController` with an opt-in external clock:

```cpp
enum class LLAnimationClockMode
{
    INTERNAL_REALTIME,
    EXTERNAL_ABSOLUTE
};

void setExternalAnimTime(F32 time);
void evaluateAtExternalTime(bool force_update);
```

In external mode:

- the controller does not advance from `mTimer`;
- Director supplies absolute animation time;
- loading and lifecycle behavior remain intact;
- pose update and blending run using the supplied value.

This is the recommended long-term capture architecture, but it needs careful testing because lifecycle code assumes monotonic forward time.

### Option C — direct per-clip sampling outside `LLMotionController`

Instantiate and sample `LLKeyframeMotion` directly into a custom pose stack.

Reject for the initial system. It duplicates conflict resolution, pose blending, lifecycle behavior, emotes, constraints, and asset loading. It becomes a replacement animation engine.

## 6.5 Seeking strategy

Arbitrary seek is the hardest control problem.

The safest first implementation is **rebuild-on-seek**:

```text
pause Director
  → stop Director-owned motions immediately
  → restore/suppress competing layers according to policy
  → determine all clips active at target time
  → start each clip with source offset
  → force one or more animation updates
  → evaluate local property tracks
```

Pseudocode:

```cpp
void LLDirectorActorRuntime::seek(F64 sequence_time)
{
    stopOwnedMotions(/* immediate */ true);

    const auto desired = mSequence.queryActiveClips(mActorId, sequence_time);

    for (const LLDirectorAnimClip* clip : desired)
    {
        const F32 source_offset =
            computeClipSourceTime(*clip, sequence_time);

        startOwnedMotion(clip->asset_id, source_offset);
    }

    mCharacter->getMotionController().updateMotions(true);
}
```

Limitations to document in UI:

- constraints may depend on prior frames;
- emote side effects can retrigger;
- ease state after a jump may differ from continuous playback;
- source offsets on looping assets require correct loop mapping;
- reverse scrubbing should rebuild rather than run motions backward.

---

# 7. Actor playback adapter

## 7.1 Purpose

The sequence should describe the desired state. The runtime adapter should convert that desired state into the minimal native start/stop operations.

```cpp
class LLDirectorActorRuntime
{
public:
    void bind(LLDirectorActor& actor);
    void enterDirectorControl();
    void leaveDirectorControl();

    void evaluate(F64 previous_time, F64 current_time);
    void seek(F64 current_time);

private:
    void reconcile(const LLDirectorDesiredClipSet& desired);
    void startOwnedMotion(const LLUUID& asset_id, F32 source_offset);
    void stopOwnedMotion(const LLUUID& asset_id, bool immediate);
};
```

## 7.2 Ownership ledger

Do not identify Director ownership merely by animation UUID. The same asset may already be playing from another source.

Maintain a ledger:

```cpp
struct LLDirectorOwnedMotion
{
    LLUUID asset_id;
    LLUUID clip_id;
    F64 started_at_sequence_time;
    F32 started_with_source_offset;
    bool local_only;
};
```

The ledger is needed to avoid stopping a motion that Director did not start.

## 7.3 Reconciliation algorithm

At each sequence evaluation:

1. query active clips at current time;
2. compare with owned motions;
3. start newly entered clips;
4. stop clips that exited;
5. restart clips when crossing a restart boundary;
6. preserve clips that remain active;
7. update suppression state;
8. update diagnostics.

Avoid issuing start/stop every frame.

## 7.4 Same asset used by overlapping clips

`LLMotionController` canonicalizes by UUID and can deprecate/recreate instances under certain conditions. Overlapping timeline clips referencing the same asset are therefore not guaranteed to behave like independent NLA strips.

Policy:

- flag overlapping use of the same asset on the same actor;
- either merge them, reject them, or require an explicit restart/cut;
- do not promise two independent instances of one asset without a dedicated controller extension.

---

# 8. AO, locomotion, and simulator state

## 8.1 State domains

At minimum, distinguish:

```text
A. Simulator-signaled animation state
B. Viewer default locomotion/state motions
C. AO-selected replacement animations
D. Script-triggered animations
E. User local preview animations
F. Director-owned animations
G. Ghost actor animations
```

## 8.2 Do not globally disable animation systems

A global "stop all animations" approach is unsafe because it:

- destroys context needed for restoration;
- can stop script-controlled animation unexpectedly;
- can affect unrelated avatars or control avatars;
- makes Director exit unreliable;
- hides conflicts rather than diagnosing them.

## 8.3 Suppression policy

Use scoped suppression per actor and per category.

```cpp
enum class LLDirectorSuppressionCategory : U32
{
    NONE          = 0,
    AO            = 1 << 0,
    LOCOMOTION    = 1 << 1,
    TYPING        = 1 << 2,
    HEAD_LOOK     = 1 << 3,
    EYE_LOOK      = 1 << 4,
    GESTURES      = 1 << 5,
    SCRIPT_ANIMS  = 1 << 6
};
```

First release recommendation:

- support AO suppression for the self avatar through the AO integration point used by the fork;
- support default locomotion suppression for ghost actors;
- do not silently suppress scripted animations;
- show scripted conflicts in the inspector;
- require an explicit "Director owns self pose" toggle.

## 8.4 Restoration snapshot

On entry:

```cpp
struct LLDirectorRestoreSnapshot
{
    std::set<LLUUID> locally_active_motions;
    std::set<LLUUID> signaled_motions;
    bool ao_enabled;
    bool default_motions_enabled;
    F32 motion_time_factor;
    bool motions_paused;
};
```

On exit:

1. stop Director-owned motions;
2. remove Director suppression;
3. restore AO/default-motion settings;
4. allow simulator state to reconcile;
5. restart only local states the Director explicitly interrupted and can identify safely.

Do not blindly replay every snapshot motion. Some may have legitimately ended during Director Mode.

A better rule is:

- restore configuration;
- clear Director ownership;
- request a normal animation-state refresh/reconciliation;
- only restart known local preview/AO state when required.

---

# 9. Live avatars versus ghost actors

## 9.1 Self avatar

Capabilities:

- local preview;
- normal start/stop requests;
- AO control where integrated;
- locomotion suppression;
- camera-independent filming;
- optional network-visible animation request.

Risks:

- simulator corrections;
- seated state;
- scripts with animation permissions;
- control inputs;
- AO restart races;
- movement state changing default motions.

## 9.2 Remote avatar

Default:

```text
observe and record only
```

A viewer may locally alter how a remote avatar is rendered, but that is neither authoritative nor visible to others. The Director UI must label this clearly.

Recommended first release:

- allow state capture;
- allow clone creation from the avatar;
- direct the clone instead of the live remote avatar;
- avoid a "control remote avatar" feature.

## 9.3 `LLGhostAvatar`

Ghost actors are the ideal Director target because they are client-only.

Recommended behavior:

- disable simulator animation ingestion for the ghost;
- copy appearance and optional initial pose from source;
- give the ghost its own `LLMotionController`;
- let Director own its desired animation set;
- permit separate animation time, pause state, clip set, and movement;
- keep source and clone animation independent after spawn unless a mirror mode is explicitly enabled.

### Shared asset, independent runtime

The ghost may use the same `.anim` asset UUID and cached binary data as the source, while maintaining:

- its own motion instances;
- its own motion clock;
- its own active set;
- its own pose blender;
- its own transform and movement track.

This is the correct meaning of an independent animated clone.

## 9.4 Clone initialization modes

```cpp
enum class LLGhostPoseInitialization
{
    NEUTRAL,
    COPY_CURRENT_POSE_ONCE,
    MIRROR_SOURCE_LIVE,
    START_DIRECTOR_SEQUENCE
};
```

`MIRROR_SOURCE_LIVE` should be a distinct mode, not the default architecture.

---

# 10. Clip transitions

## 10.1 Native capabilities

The existing runtime supplies:

- asset ease-in;
- asset ease-out;
- overlap through multiple active motions;
- priority and joint ownership;
- pose weighting;
- additive motions for motions authored as additive/runtime types.

It does not automatically provide:

- per-clip editable crossfade curves;
- arbitrary joint masks created by the timeline;
- normalized two-clip blending comparable to a DCC NLA editor;
- motion matching;
- foot locking;
- guaranteed root continuity.

## 10.2 Transition classes

The UI should classify transitions rather than expose misleading controls.

```text
SAFE
  Same pose family, compatible keyed joints, deliberate overlap.

CONDITIONAL
  Some overlapping joints or priority competition; preview required.

HARD CUT
  Incompatible pose, locomotion discontinuity, or same-asset restart.

TRANSITION ASSET RECOMMENDED
  Sit/stand, turn, mount/dismount, major stance change.
```

## 10.3 Practical transition strategies

Ranked:

1. purpose-authored transition `.anim`;
2. overlap using native asset easing;
3. neutral pose bridge;
4. same-priority restart timed at a low-motion frame;
5. direct cut hidden by camera edit;
6. local pose freeze followed by new clip.

Do not advertise an arbitrary crossfade duration if the asset's own ease metadata and priorities make the result materially different.

---

# 11. Animation asset index and inspection

## 11.1 Asset library

The Director workspace needs a searchable local index of animation inventory items.

Suggested metadata:

```cpp
struct LLDirectorAnimAssetInfo
{
    LLUUID asset_id;
    LLUUID inventory_item_id;
    std::string name;
    std::string description;
    std::vector<std::string> tags;

    bool metadata_loaded;
    F32 duration;
    bool loop;
    F32 loop_in;
    F32 loop_out;
    F32 ease_in;
    F32 ease_out;
    S32 priority;

    std::vector<LLDirectorJointUsage> joints;
    LLUUID emote_id;
    S32 hand_pose;

    LLDirectorAssetAvailability availability;
};
```

## 11.2 Metadata loading

Where permissions and cache access allow:

1. request or read the animation asset;
2. deserialize through the same code path as `LLKeyframeMotion`;
3. expose read-only metadata;
4. cache a derived index keyed by asset UUID and cache version.

Do not duplicate the entire binary asset in the project file.

## 11.3 Joint conflict map

For every clip, derive:

```text
joint name
  → position keyed?
  → rotation keyed?
  → asset priority
```

For overlapping clips, report:

- same joint keyed by both;
- priorities;
- likely winner;
- equal-priority order dependence;
- whether either clip is easing;
- whether the conflict is currently visible.

## 11.4 BVH workflow

BVH should remain an authoring/import source:

```text
External DCC
  → BVH
  → viewer validation and preview
  → upload options
  → SL animation asset
  → Director library
```

Director additions to the existing preview flow:

- validate skeleton and joint names;
- show ignored/unrecognized joints;
- display duration and frame rate;
- show loop boundaries graphically;
- preview against selected actor appearance;
- compare expected and uploaded metadata;
- tag and add the uploaded asset to the Director library;
- optionally create a timeline clip immediately after upload.

---

# 12. Camera and actor movement synchronization

Avatar animation and actor movement are separate channels.

## 12.1 Why separate them

Many SL walk animations do not provide authoritative world-space motion. Avatar movement normally comes from simulator or viewer movement state, while the animation supplies pose.

For ghost actors:

```text
Ghost transform track
  + walk/run/turn .anim clip
  = apparent locomotion
```

The timeline should not assume the animation asset contains usable root motion.

## 12.2 `LLActorMover` integration

Recommended control flow:

```text
Director sequence time
  → actor path sampler
  → LLActorMover desired transform/velocity
  → ghost world transform

Director sequence time
  → animation clip evaluator
  → LLMotionController desired clip set
  → ghost pose
```

## 12.3 Locomotion helper

A later convenience layer can select animation clips from measured path speed:

```cpp
enum class LLDirectorLocomotionState
{
    IDLE,
    WALK,
    RUN,
    TURN_LEFT,
    TURN_RIGHT
};
```

This should remain an optional automation layer. Explicit timeline clips must override it.

## 12.4 Foot sliding

Without root-motion extraction and foot locking, sliding is possible.

Mitigations:

- calibrate path speed per animation asset;
- store `meters_per_cycle` metadata in the Director library;
- provide a stride calibration tool;
- snap clip cycle timing to path distance;
- use camera framing to hide residual error;
- add foot-lock IK only as a later local correction feature.

---

# 13. Recording and takes

## 13.1 What can be recorded reliably

A live performance recorder can capture:

- active animation UUID set and changes;
- avatar world transform;
- avatar velocity;
- camera transform and FOV;
- look-at target;
- gestures/emotes;
- selected environment properties;
- markers and user cues.

It cannot reconstruct original joint keys from only animation state unless the asset data is available.

## 13.2 Event recording model

```cpp
struct LLDirectorRecordedAnimEvent
{
    F64 time;
    LLUUID actor_id;
    LLUUID asset_id;
    bool start;
};
```

Transform samples should be simplified into keyframes after recording.

## 13.3 Take model

```yaml
take:
  id: "..."
  name: "Take 3"
  sequence_revision: 17
  recorded_at: "..."
  events: []
  transform_samples: []
  notes: ""
```

A take should not mutate the master sequence until the user applies or promotes it.

---

# 14. Serialization

## 14.1 Recommended format

Use LLSD or JSON-compatible LLSD for project persistence.

Suggested extension:

```text
.sldirector
```

Example:

```json
{
  "schema": "alchemy.director.sequence",
  "version": 1,
  "frame_rate": 30.0,
  "duration": 24.0,
  "actors": [
    {
      "id": "actor-ghost-a",
      "kind": "ghost_avatar",
      "label": "Performer A"
    }
  ],
  "tracks": [
    {
      "type": "avatar_animation",
      "actor": "actor-ghost-a",
      "clips": [
        {
          "id": "clip-001",
          "asset_id": "00000000-0000-0000-0000-000000000000",
          "start": 2.0,
          "end": 7.0,
          "source_in": 0.0,
          "rate": 1.0
        }
      ]
    }
  ]
}
```

## 14.2 Versioning

Every serialized object should be versioned.

Rules:

- unknown fields are ignored;
- missing optional fields receive defaults;
- asset UUID references may be unresolved;
- actor bindings are repaired interactively;
- no raw pointers or viewer object addresses are serialized.

---

# 15. UI recommendation

## 15.1 Main panes

```text
Director Outliner
Timeline / Dope Sheet
Clip Inspector
Animation Library
Conflict Inspector
Transport Controls
Actor Control Toolbar
```

## 15.2 Transport

Required:

- jump to start/end;
- previous/next marker;
- play/pause;
- stop and restore;
- loop range;
- frame step;
- current time/frame input;
- preroll;
- capture arm.

## 15.3 Clip inspector

Display separately:

**Asset metadata**

- duration;
- loop;
- loop range;
- priority;
- ease values;
- joint set;
- hand pose;
- emote.

**Timeline behavior**

- start/end;
- source offset;
- playback rate;
- restart policy;
- local/network mode;
- suppression policy;
- transition warning.

This prevents the user from believing timeline settings rewrite the uploaded animation.

## 15.4 Conflict visualization

```text
Red hatch    = overlapping keyed joints with likely hard conflict
Amber hatch  = equal-priority/order-sensitive overlap
Blue edge    = native ease region
Purple icon  = scripted animation conflict
Green check  = asset loaded and ready
Cloud icon   = asset fetch pending
```

---

# 16. Recommended code architecture

## 16.1 New modules

```text
indra/newview/lldirectorclock.h/.cpp
indra/newview/lldirectorsequence.h/.cpp
indra/newview/lldirectortrack.h/.cpp
indra/newview/lldirectoranimclip.h/.cpp
indra/newview/lldirectoractor.h/.cpp
indra/newview/lldirectoractorruntime.h/.cpp
indra/newview/lldirectorassetindex.h/.cpp
indra/newview/lldirectorconflict.h/.cpp
indra/newview/lldirectorprojectio.h/.cpp
indra/newview/lldirectorrecorder.h/.cpp

indra/newview/alfloatertimeline.h/.cpp
indra/newview/alpaneldirectoroutliner.h/.cpp
indra/newview/alpanelanimlibrary.h/.cpp
indra/newview/alpanelclipinspector.h/.cpp
```

## 16.2 Avoid placing timeline concepts in `llcharacter`

`indra/llcharacter` should remain the generic runtime animation layer.

Place Director concepts in `indra/newview` because they depend on:

- inventory;
- simulator state;
- viewer objects;
- camera;
- UI;
- ghost actors;
- project persistence;
- capture workflow.

Only the optional external-clock extension belongs in `llcharacter`.

## 16.3 Minimal initial changes to existing classes

### `LLCharacter`

Potential additions:

```cpp
LLMotionController& getMotionController();
const LLMotionController& getMotionController() const;
```

Only add if equivalent access does not already exist.

### `LLMotionController`

Phase-one goal: no invasive changes.

Later external-clock additions:

```cpp
void setClockMode(LLAnimationClockMode mode);
void setExternalAnimTime(F32 time);
F32 getAnimTime() const;
void evaluateExternal(bool force_update);
```

Guard all new behavior behind the mode flag.

### `LLVOAvatar` / `LLGhostAvatar`

Add Director classification and suppression hooks at the highest-level state reconciliation seam, not inside every motion.

```cpp
virtual bool isDirectorActor() const;
virtual LLDirectorAnimationPolicy* getDirectorAnimationPolicy();
```

For the ghost:

```cpp
bool acceptsSimulatorAnimationState() const override { return false; }
```

Use the fork's existing `isGhostAvatar()` mechanism where available.

---

# 17. External-clock design detail

This is the most likely later low-level modification.

## 17.1 Desired semantics

```cpp
void LLMotionController::updateMotions(bool force_update)
{
    mLastTime = mAnimTime;

    if (mClockMode == INTERNAL_REALTIME)
    {
        advanceFromTimer();
    }
    else
    {
        mAnimTime = mPendingExternalAnimTime;
        syncTimerBookkeepingWithoutAdvancing();
    }

    updateLoadingMotions();
    evaluateAndBlend(force_update);
}
```

## 17.2 Forward-only guarantee

Initially require:

```text
external_time >= previous_external_time
```

On backward seek:

- clear Director-owned motions;
- reconstruct active set at target;
- reset/rebase controller time;
- evaluate forward from a small preroll if constraints require it.

Do not permit ordinary active motions to experience negative delta without auditing every motion subclass.

## 17.3 Clock domain per actor

Each ghost actor may need an independent motion clock.

Live self avatar may remain on the normal internal clock in ordinary rehearsal mode.

For deterministic ghost capture:

```text
Director clock
  → per-ghost external animation time
  → motion controller evaluate
```

## 17.4 Asset-loading barrier

Capture must not begin until every clip needed in the preroll and take range is loaded.

```cpp
enum class LLDirectorPreflightStatus
{
    READY,
    ASSET_LOADING,
    ASSET_MISSING,
    ACTOR_UNBOUND,
    CONFLICT_WARNING,
    FATAL_ERROR
};
```

---

# 18. Preflight and capture determinism

## 18.1 Preflight checklist

Before capture:

- all actors bound;
- all animation assets cached and deserialized;
- ghost appearances fully loaded;
- attachments loaded;
- timeline contains no unresolved critical conflicts;
- camera path evaluated;
- environment state locked;
- AO and locomotion suppression applied;
- capture range known;
- sequence clock reset to preroll start;
- all actors evaluated through preroll;
- no pending asset fetch in critical actors.

## 18.2 Preroll

Preroll is required for:

- native ease-in;
- constraints;
- cloth/hair visual settling where applicable;
- camera effects;
- asset initialization;
- transition poses.

Recommended project setting:

```yaml
capture:
  preroll_seconds: 2.0
```

## 18.3 Determinism levels

Be explicit:

### Level 0 — interactive rehearsal

Best effort. Real-time frame timer.

### Level 1 — repeatable sequence starts

Assets preloaded; clip set reconstructed; fixed project start.

### Level 2 — externally clocked ghost animation

Director supplies monotonic animation time to ghost controllers.

### Level 3 — frame-locked full capture

Animation, camera, actor transforms, environment, and capture all evaluated from the same frame index.

Level 3 may require broader viewer capture-pipeline work and should not be promised in the first animation milestone.

---

# 19. Safety and local-only invariants

For ghost actors:

- never send animation requests to the simulator;
- never request permissions;
- never update agent controls;
- never alter the source avatar's AO state;
- never write source animation state;
- never persist runtime pointer bindings;
- never let ghost stop events call network paths.

Recommended adapter:

```cpp
class LLDirectorAnimationSink
{
public:
    virtual void start(const LLUUID& id, F32 offset) = 0;
    virtual void stop(const LLUUID& id, bool immediate) = 0;
};

class LLLocalGhostAnimationSink final : public LLDirectorAnimationSink
{
    // Calls local LLCharacter motion functions only.
};

class LLSelfAvatarAnimationSink final : public LLDirectorAnimationSink
{
    // Policy decides local preview vs normal network request.
};
```

This prevents one generic timeline command from accidentally taking the wrong authority route.

---

# 20. Testing strategy

## 20.1 Unit tests

Add tests for:

- sequence-time to source-time mapping;
- loop mapping;
- clip entry/exit;
- overlapping same-asset detection;
- actor binding serialization;
- conflict graph construction;
- restoration ledger;
- frame/time conversion;
- project version migration.

## 20.2 Motion integration tests

Test with assets covering:

- non-looping full-body animation;
- looping idle;
- partial upper-body gesture;
- high-priority pose;
- equal-priority overlap;
- hand pose;
- emote;
- pelvis translation;
- Bento face/finger joints;
- zero ease;
- long ease;
- loop-in/loop-out subset.

## 20.3 Actor matrix

```text
Self avatar
Ghost avatar
Remote avatar observe-only
Attached animesh/control avatar
Sitting avatar
Avatar with AO
Avatar with script-triggered animation
```

## 20.4 Seek tests

For each test sequence:

1. play continuously to time T and capture joint transforms;
2. reset;
3. seek directly to T using rebuild-on-seek;
4. compare transforms;
5. record expected differences for constraints/ease;
6. determine whether preroll removes the discrepancy.

## 20.5 Restoration tests

- enter Director Mode while walking;
- enter while sitting;
- enter with AO enabled;
- enter during a scripted animation;
- play Director clips;
- exit;
- verify controls, AO, state motions, and script behavior recover.

## 20.6 Clone independence tests

- source and ghost start identical;
- pause only ghost;
- start different ghost clip;
- source must remain unchanged;
- seek ghost;
- source must remain unchanged;
- destroy ghost;
- no source or simulator state changes.

---

# 21. Phased implementation roadmap

## Phase 0 — source audit and instrumentation

Deliverables:

- map exact animation message and reconciliation paths in the current branch;
- identify AO integration class in Alchemy-Machinima;
- identify existing Ghost Studio and `LLActorMover` seams;
- add debug panel showing active/loading/loaded motions;
- log animation source and ownership.

Acceptance gate:

- one can explain why every active animation on the self avatar is running;
- ghost start/stop is proven local-only.

## Phase 1 — animation asset browser and local preview

Deliverables:

- inventory animation index;
- metadata inspection;
- local preview on self;
- local preview on one ghost;
- load-state reporting;
- joint list and priority display.

Acceptance gate:

- an uploaded `.anim` can be selected, inspected, loaded, played locally, stopped, and replayed with source offset.

## Phase 2 — single-actor clip timeline

Deliverables:

- sequence clock;
- one animation track;
- clip placement;
- play/pause/stop;
- loop range;
- rebuild-on-seek;
- owned-motion ledger;
- project serialization.

Acceptance gate:

- a ghost actor plays a repeatable sequence of at least five clips without simulator traffic.

## Phase 3 — conflict analysis and suppression

Deliverables:

- joint-set conflict analyzer;
- priority warnings;
- AO suppression for self;
- locomotion suppression for ghost;
- restoration snapshot;
- transition classification.

Acceptance gate:

- Director exit restores normal self behavior in walking, sitting, AO, and scripted-animation scenarios.

## Phase 4 — multi-actor sequencing

Deliverables:

- multiple actor tracks;
- synchronized start;
- actor outliner;
- markers and cues;
- ghost transform tracks through `LLActorMover`;
- camera track integration.

Acceptance gate:

- three ghost actors and one camera repeat a 30-second scene with stable cue ordering.

## Phase 5 — recorder and takes

Deliverables:

- live animation-event recording;
- actor transform sampling;
- take storage;
- take comparison;
- promote take to sequence.

Acceptance gate:

- a live rehearsal can be recorded, replayed on ghosts, and edited.

## Phase 6 — external animation clock for ghosts

Deliverables:

- opt-in external clock in `LLMotionController`;
- monotonic absolute-time evaluation;
- capture preflight;
- asset barrier;
- preroll;
- fixed-frame ghost evaluation.

Acceptance gate:

- repeated captures at the same frame index produce matching ghost joint transforms within a defined tolerance.

## Phase 7 — advanced corrections

Optional:

- stride calibration;
- look-at tracks;
- local eye/head suppression;
- lightweight foot-lock correction;
- transition asset suggestions;
- clip thumbnails;
- waveform/dialogue cue integration.

---

# 22. Immediate coding backlog

## Milestone A: diagnostic foundation

1. Add an animation debug model exposing:
   - motion UUID;
   - name;
   - loaded/loading/active/deprecated state;
   - priority;
   - loop;
   - current local time;
   - fade weight;
   - ownership source.

2. Add ghost local-only assertions around motion request paths.

3. Identify and document:
   - self animation request function;
   - simulator animation message handler;
   - `mSignaledAnimations` reconciliation;
   - AO replacement hook;
   - default motion enable/disable hook.

## Milestone B: project core

1. `LLDirectorClock`
2. `LLDirectorSequence`
3. `LLDirectorActor`
4. `LLDirectorAnimTrack`
5. `LLDirectorAnimClip`
6. LLSD serialization
7. no UI beyond a debug command

## Milestone C: runtime adapter

1. bind one ghost;
2. start clip at zero;
3. start clip with offset;
4. stop graceful;
5. stop immediate;
6. reconcile clip boundaries;
7. rebuild on seek;
8. ledger and restoration.

## Milestone D: first timeline UI

1. transport controls;
2. one actor row;
3. draggable clip;
4. playhead;
5. clip inspector;
6. save/load.

---

# 23. Key engineering risks

## 23.1 Hidden simulator reconciliation

Live avatar animation state may be reapplied after Director locally stops a clip.

Mitigation:

- use scoped suppression at the reconciliation layer;
- prefer ghosts for fully controlled actors;
- record the source of each active animation.

## 23.2 AO implementation divergence

Alchemy/Firestorm-derived viewers may implement AO outside the Linden baseline.

Mitigation:

- locate the exact branch implementation before designing interfaces;
- keep AO integration behind an adapter;
- avoid hard-coding Firestorm-specific assumptions into `llcharacter`.

## 23.3 Seek mismatch

Direct start offset may not reproduce continuous history for constraints and easing.

Mitigation:

- rebuild plus preroll;
- display a "seek approximation" warning where needed;
- external-clock work only after phase-two behavior is measured.

## 23.4 Same UUID instance semantics

Two overlapping clips with the same asset UUID may collapse into one canonical motion or trigger deprecation behavior.

Mitigation:

- disallow ambiguous overlap initially;
- add explicit restart semantics;
- only extend the controller if a real production case requires independent instances.

## 23.5 Asset permissions and availability

Inventory reference does not guarantee asset binary availability forever or on another account.

Mitigation:

- unresolved-asset state;
- relink workflow;
- never embed protected asset data in project files.

## 23.6 Motion count limits

`LLMotionController` purges excess loaded motions and warns when counts exceed its expected limit.

Mitigation:

- preload only a bounded sequence window;
- release inactive Director assets;
- expose actor motion counts in preflight;
- avoid one controller per irrelevant preview object.

## 23.7 LOD-driven motion fading

Motion updates can be affected by pixel-area thresholds.

Mitigation:

- Director-controlled ghosts used in capture should have an override ensuring required motions evaluate;
- do not globally disable avatar animation LOD;
- scope the override to armed Director actors.

---

# 24. Final recommendation

Build the animation system as a **clip sequencer, authority manager, and deterministic evaluation layer** over the existing Second Life motion runtime.

The first coding target should not be a graph editor, IK rig, or joint-keyframe editor. It should be:

```text
Animation asset inspection
  → one ghost actor
  → clip timeline
  → owned-motion reconciliation
  → seek by reconstruction
  → conflict diagnostics
  → clean restoration
```

Then add:

```text
multi-actor control
  → transform/path tracks
  → camera synchronization
  → performance recording
  → external animation clock for capture
```

The existing runtime is capable enough to provide the actual skeletal result. The project's value comes from making that runtime **visible, schedulable, repeatable, diagnosable, and safe to direct**.

The most important low-level conclusion is:

> Do not replace `LLMotionController` or `LLKeyframeMotion`. Add a Director layer above them, and only later add a narrowly scoped external-clock mode for client-only actors when fixed-frame capture requires it.

---

# Appendix A — Proposed class relationships

```text
LLDirectorProject
  └── LLDirectorSequence
        ├── LLDirectorClock
        ├── LLDirectorActor[]
        │     └── LLDirectorActorRuntime
        │           └── LLDirectorAnimationSink
        │                 ├── LLLocalGhostAnimationSink
        │                 └── LLSelfAvatarAnimationSink
        ├── LLDirectorTrack[]
        │     ├── LLDirectorAnimTrack
        │     │     └── LLDirectorAnimClip[]
        │     ├── LLDirectorTransformTrack
        │     ├── LLDirectorCameraTrack
        │     └── LLDirectorEventTrack
        ├── LLDirectorConflictAnalyzer
        ├── LLDirectorRecorder
        └── LLDirectorProjectIO

LLLocalGhostAnimationSink
  → LLGhostAvatar / LLCharacter
      → LLMotionController
          → LLKeyframeMotion
              → LLPose
                  → LLPoseBlender
                      → LLJointState / skeleton
```

# Appendix B — Desired runtime flow

```text
UI transport command
  → LLDirectorClock changes state
  → LLDirectorSequence evaluates time
  → each track produces desired state
  → each actor runtime reconciles desired clips
  → animation sink starts/stops native motions
  → LLMotionController loads/evaluates/blends
  → avatar skeleton updates
  → render pipeline draws actor
```

# Appendix C — Seek flow

```text
User drags playhead
  → Director clock enters SCRUBBING
  → actor runtime stops Director-owned motions
  → sequence queries active clips at target
  → assets are checked/loaded
  → active clips restart with calculated offsets
  → optional preroll evaluation
  → property tracks evaluate directly
  → viewer renders target frame
```

# Appendix D — Research references

## Viewer source

- [Alchemy-Machinima repository](https://github.com/bwlupus-ctrl/Alchemy-Machinima)
- [Alchemy-Machinima `llcharacter` directory](https://github.com/bwlupus-ctrl/Alchemy-Machinima/tree/develop/indra/llcharacter)
- [`llmotioncontroller.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llcharacter/llmotioncontroller.cpp)
- [`llkeyframemotion.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/llcharacter/llkeyframemotion.cpp)
- [`llvoavatar.cpp`](https://github.com/bwlupus-ctrl/Alchemy-Machinima/blob/develop/indra/newview/llvoavatar.cpp)
- [Alchemy Viewer](https://github.com/AlchemyViewer/Alchemy)
- [Second Life viewer](https://github.com/secondlife/viewer)
- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)

## Second Life animation behavior

- [Internal Animation Format](https://wiki.secondlife.com/wiki/Internal_Animation_Format)
- [Animation Upload Priority](https://wiki.secondlife.com/wiki/Animation_Upload_Priority)
- [Animation Upload Ease In and Ease Out](https://wiki.secondlife.com/wiki/Animation_Upload_Ease_In_and_Ease_Out)
- [Animation Upload Loop Controls](https://wiki.secondlife.com/wiki/Animation_Upload_Loop_Controls)
- [How to create animations](https://wiki.secondlife.com/wiki/How_to_create_animations)
- [Animation Test](https://wiki.secondlife.com/wiki/Animation_Test)

# Appendix E — Questions to resolve during the Phase 0 source audit

1. Which exact class implements AO in the current `develop` branch?
2. Where does the branch reconcile `mSignaledAnimations` into `mPlayingAnimations`?
3. Which self-avatar start/stop paths send simulator messages versus remaining local?
4. Does `LLGhostAvatar` already override `requestStopMotion()` or equivalent network callbacks?
5. Does the ghost currently inherit default locomotion motions?
6. Can a ghost safely use `pauseAllMotions()` independently?
7. Are motion-controller clocks already independent per ghost?
8. Which motion subclasses depend on monotonic elapsed time outside `LLMotionController`?
9. Which constraints in `LLKeyframeMotion` require historical evaluation?
10. What is the branch's current animation asset cache and permission behavior?
11. How are animation previews instantiated in the current upload floater?
12. Which Director/Ghost Studio UI framework should own the timeline floater?
13. How does `LLActorMover` currently derive velocity and yaw?
14. Are camera controls already serialized in an existing machinima project format?
15. Which capture code path can expose exact frame stepping later?
