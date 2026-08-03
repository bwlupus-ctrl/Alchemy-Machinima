/**
 * @file alghoststudio.h
 * @brief Ghost Studio: free-standing styled body copies (ghost instances).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The path preview's pose ghosts are bound to path nodes; the Ghost Studio is
 * the free form of the same renderer -- N independent GHOST INSTANCES, each a
 * styled copy of a cast member's (or your own) worn rigged body, placed
 * anywhere, rotated, scaled, restyled, and optionally FROZEN in a captured
 * pose while the live body keeps animating ("out-of-sync" ghosts).
 *
 * This singleton is the DATA MODEL only (session-scoped, UI-free): the shared
 * ALPanelGhostStudio panel is the UI over it, and the rendering rides the
 * existing LLActorMover model-ghost pipeline -- collectGhostBatches() widens
 * its wanted-set to sources referenced by enabled instances, and
 * renderStudioGhosts() draws each instance through drawGeometryGhost with the
 * per-instance placement/style/FX parameters. It lives OUTSIDE LLActorMover
 * because ghosts-as-set-dressing have their own lifecycle (an instance
 * outlives walks, marks and paths, and none of the Move machinery ever needs
 * to know about it); the mover only consumes the instance list at render time.
 *
 * Positions are stored region-GLOBAL (like LLActorMover::Waypoint::mPosGlobal
 * and LLFlycamRecorder::Keyframe) so an instance survives a region crossing.
 * A FROZEN instance additionally keeps its capture anchor in the CAPTURE-TIME
 * agent frame: the frozen matrix palettes bake vertices into that same frame,
 * so pivoting the placement around the frame-matched anchor keeps the pair
 * consistent even after the agent frame shifts under a crossing.
 *
 * Session-only; scene serialization is future work (documented).
 */

#ifndef AL_ALGHOSTSTUDIO_H
#define AL_ALGHOSTSTUDIO_H

#include "alghostgroupmodel.h"
#include "alformationsolver.h"
#include "lluuid.h"
#include "v3math.h"
#include "v3dmath.h"
#include "m4math.h"         // frozen attachment matrices
#include "llquaternion.h"   // per-instance orientation

#include <map>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Single source of truth for the Ghost Studio clone scale range (the outer
// render scale applied to a clone / ghost instance). Change the max HERE and
// every clamp/bound/validation/spinner limit follows: llghostavatar.cpp
// (setEntityScale -- the chokepoint that actually applies the scale),
// alghoststudio.cpp (instance/chaos/group-limit/import clamps),
// alghostmanipproxy.cpp (drag-scale), alchatcommand.cpp (/ghostscale) and
// alpanelghoststudio.cpp (scale spinner bounds). panel_ghost_studio.xml's
// scale_spinner and doc/MACHINIMA_USER_GUIDE.md cannot reference C++
// constants -- keep those two in sync by hand.
constexpr F32 GHOST_SCALE_MIN = 0.05f;
constexpr F32 GHOST_SCALE_MAX = 150.f;

class LLGhostAvatar;
class ALGhostNameplates;
struct ALGhostCloneRequest;
struct ALGhostCloneUpdate;
struct ALGhostSpawnResult;

class ALGhostStudio
{
public:
    static ALGhostStudio& instance();

    enum EPose : S32
    {
        POSE_LIVE   = 0,    // skins from the source's live palette every frame
        POSE_FROZEN = 1,    // skins from the snapshot captured at freeze time
    };

    enum EBackingKind : S32 { BACKING_OVERLAY = 0, BACKING_ENTITY_CLONE };
    enum ERenderIntent : S32
    {
        RENDER_INHERIT_GLOBAL = 0,
        RENDER_FORCE_FORWARD,
        RENDER_REQUEST_DEFERRED
    };
    enum ELifecycleState : S32
    {
        STATE_SPAWNING = 0, STATE_READY, STATE_SOURCE_MISSING, STATE_LOCKED,
        STATE_TEARING_DOWN, STATE_ERROR, STATE_RECOVERABLE
    };

    // How a client-only ENTITY clone is animated, INDEPENDENT of its source
    // avatar. MIRROR = copy the source's live animation state (default; the
    // current behaviour). DIRECTED = play mDirectedAnim on the clone's OWN
    // motion controller only, ignoring the source. FROZEN = hold the current
    // pose. Ignored for overlay instances.
    enum EDriveMode : S32 { DRIVE_MIRROR = 0, DRIVE_DIRECTED, DRIVE_FROZEN };
    enum ELoopMode : S32 { LOOP_RETRIGGER = 0, LOOP_PLAY_ONCE };
    enum ELockMode : S32
    {
        LOCK_OFF = 0,
        LOCK_RIGID_UNIT,
        LOCK_MOVE_FACE
    };
    enum EGhostLook : S32
    {
        LOOK_NORMAL = 0, LOOK_APPARITION, LOOK_HOLOGRAM, LOOK_CHROME,
        LOOK_TOON, LOOK_SILHOUETTE
    };
    enum ELookTarget : S32
    {
        LOOK_TARGET_CAMERA = 0, LOOK_TARGET_ME, LOOK_TARGET_ACTOR,
        LOOK_TARGET_GHOST,
        // A bare world position, held in mLookPointGlobal / mTurnPointGlobal.
        // The only target that is not an object -- this is what "set a world
        // point" needed and what nothing could express before.
        LOOK_TARGET_POINT
    };

    // TURN-TO carries its own target, independent of the look-at target, and is
    // RATE-LIMITED where look-at snaps.
    //
    // ⚠️ THEY ARE NOT INDEPENDENT OUTPUT CHANNELS YET. Both ultimately write the
    // instance's BODY yaw (mRotation) -- look-at via faceInstance/aimInstanceAt,
    // turn via stepTurn -- so they cannot both apply at once. The precedence is
    // explicit and enforced in stepAllTurns()/updatePerFrame(): if an instance
    // has a turn target, TURN WINS and keep-facing is skipped for it.
    // A true "face one way, glance another" needs look-at to drive HEAD/EYES
    // instead of the body. That is a separate piece of work and is NOT done.
    enum ETurnMode : S32
    {
        TURN_MODE_OFF = 0,   // body facing is whatever placement/formation set
        TURN_MODE_ONCE,      // rotate to the target once, then hold
        TURN_MODE_TRACK      // keep facing a moving target
    };
    enum EFormation : S32
    {
        FORMATION_LINE = 0, FORMATION_RING, FORMATION_ARC, FORMATION_GRID,
        FORMATION_V, FORMATION_SPIRAL, FORMATION_STAIRCASE, FORMATION_TUNNEL,
        FORMATION_SCATTER, FORMATION_AMPHITHEATER, FORMATION_WEDGE,
        FORMATION_CONCENTRIC_RINGS, FORMATION_CHECKERBOARD, FORMATION_CRESCENT,
        FORMATION_PERIMETER_LINE, FORMATION_PATH, FORMATION_CLUSTERS,
        // Appended so persisted values for every legacy formation stay stable.
        FORMATION_STAGGERED_ROWS,
        FORMATION_SPLIT_ROW, FORMATION_SOUL_TRAIN, FORMATION_CHEVRON,
        FORMATION_ZIGZAG, FORMATION_HORSESHOE, FORMATION_INFINITY,
        FORMATION_PENTAGRAM, FORMATION_STAR_OUTLINE, FORMATION_DIAMOND,
        FORMATION_CROSS, FORMATION_ARROW, FORMATION_HEART,
        FORMATION_SUNBURST, FORMATION_BRIGADE,
        // Appended so persisted formation values stay stable: a symmetric
        // two-block "Soul Train" corridor and a configurable staggered/echelon
        // military rank grid.
        FORMATION_SOUL_TRAIN_MILITARY, FORMATION_STAGGERED_MILITARY,
        // A military brigade wrapped into concentric rings: the grid's files are
        // the members per ring and its ranks are the number of rings, with a
        // tunable ring spacing and an angular stagger. Appended so persisted
        // formation values stay stable.
        FORMATION_RING_BRIGADE
    };
    enum EGuideShape : S32
    {
        GUIDE_OFF = 0,
        GUIDE_CIRCLE,
        GUIDE_SQUARE
    };
    enum EFormationFacing : S32
    {
        FACING_AUTHOR = 0, FACING_SOURCE, FACING_CAMERA, FACING_CENTROID_IN,
        FACING_OUTWARD, FACING_WORLD_POINT, FACING_PATH_HEADING,
        FACING_MIRROR_SOURCE, FACING_TARGET_ACTOR, FACING_RANDOM,
        FACING_AISLE, FACING_TRACK_SUBJECT, FACING_FACE_ACROSS,
        FACING_CLUSTER_IN, FACING_LOOSE, FACING_CHAIN, FACING_WAVE
    };
    struct FormationOptions
    {
        U32 mSeed = 0;
        F32 mJitter = 0.f;       // fraction of spacing
        F32 mMinDistance = 0.f;  // metres; deterministic forward relaxation
        bool mCenterWeighted = false;
        bool mTerrainConform = true;
        EFormationFacing mFacing = FACING_AUTHOR;
        ELockMode mLockMode = LOCK_OFF;
    };

    // Reliable crowd-placement contract. Unlike FormationOptions (which is
    // retained for freeze strips and legacy array callers), every distance
    // here has one explicit meaning and is validated by ALFormationSolver.
    struct FormationSpec
    {
        EFormation mFormation = FORMATION_LINE;
        // Retained for source/persistence compatibility. Reliable crowd
        // placement always resolves the complete count with FitCount.
        ALFormationSolver::EnvelopePolicy mEnvelopePolicy =
            ALFormationSolver::EnvelopePolicy::FitCount;
        S32 mCount = 5;
        F32 mRadius = 6.f;             // legacy; use mGuideSize for the guide
        F32 mCenterSpacing = 1.5f;
        F32 mEdgeGap = 0.10f;
        F32 mJitter = 0.f;             // metres, not a spacing fraction
        U32 mGridColumns = 0;          // 0 = solver chooses compact dimensions
        U32 mGridRows = 0;
        F32 mFileSpacing = 1.5f;
        F32 mRankSpacing = 1.5f;
        F32 mArcSweepDegrees = 120.f;
        EGuideShape mGuideShape = GUIDE_CIRCLE;
        F32 mGuideSize = 6.f;           // visual aid only; never a fit boundary
        F32 mLaneGap = 2.f;
        F32 mChevronAngleDegrees = 90.f;
        F32 mHorseshoeOpeningDegrees = 90.f;
        F32 mSpiralTurns = 2.f;
        F32 mAspectRatio = 1.f;
        U32 mRayCount = 8;
        U32 mZigzagColumns = 4;
        // Alternate-rank offset for FORMATION_STAGGERED_MILITARY as a fraction
        // of file spacing (>=0 brick stagger, <0 echelon diagonal).
        F32 mRankOffsetFraction = 0.5f;
        U32 mSeed = 0;
        bool mTerrainConform = false;
        EFormationFacing mFacing = FACING_AUTHOR;
        ELockMode mLockMode = LOCK_RIGID_UNIT;
        bool mHasFacingPoint = false;
        LLVector3d mFacingPointGlobal;
    };

    struct PlacementSourceSnapshot
    {
        LLUUID mInstanceId;
        LLUUID mSourceId;
        LLUUID mEntityId;
        LLUUID mGroupId;
        EBackingKind mKind = BACKING_OVERLAY;
        LLVector3d mFoot;
        LLQuaternion mRotation;
        F32 mScale = 1.f;
        U64 mTransformRevision = 0;
    };

    struct ResolvedFormationSlot
    {
        U64 mMemberId = 0;
        U32 mInputIndex = 0;
        LLVector3d mFoot;
        LLQuaternion mRotation;
        F32 mScale = 1.f;
        std::string mRole;
    };

    struct PlacementValidation
    {
        bool mCanCommit = false;
        bool mPartial = false;
        std::string mMessage;
        ALFormationSolver::Status mSolverStatus =
            ALFormationSolver::Status::InvalidInput;
        U32 mRequestedCount = 0;
        U32 mPlacedCount = 0;
        U32 mOverflowCount = 0;
        F32 mEnvelopeRadius = 0.f;
        F32 mRequiredRadius = 0.f;
        F32 mPlacedRadius = 0.f;
        F32 mActualMinEdgeGap = 0.f;
    };

    struct CrowdPlacementDraft
    {
        bool mActive = false;
        bool mPinned = false;
        LLUUID mOwnerId;
        LLUUID mSessionId;
        PlacementSourceSnapshot mSource;
        FormationSpec mSpec;
        LLVector3d mAnchor;
        F32 mYaw = 0.f;
        F32 mHeight = 0.f;
        U64 mRevision = 0;
        U64 mInputFingerprint = 0;
        U64 mSolutionFingerprint = 0;
        PlacementValidation mValidation;
        std::vector<ResolvedFormationSlot> mSlots;
        std::vector<U64> mOverflowMemberIds;
    };

    struct PlacementCommitResult
    {
        bool mSuccess = false;
        std::string mMessage;
        S32 mPlacedCount = 0;
        S32 mCreatedCount = 0;
        LLUUID mGroupId;
        std::vector<LLUUID> mInstanceIds;
    };
    enum EMotion : S32
    {
        MOTION_OFF = 0, MOTION_ORBIT, MOTION_SPIN, MOTION_BREATHE, MOTION_RIPPLE,
        MOTION_PATH
    };

    // Frozen matrix palettes, keyed by (DRAWING avatar id, skin hash). The
    // drawing avatar is each batch's own mAvatar -- the wearer for body mesh,
    // an attachment's LLControlAvatar for animesh -- so a frozen outfit
    // freezes whole. Values are the GL-ready 3x4 float palettes
    // (MatrixPaletteCache::mGLMp copies; 12 floats per joint).
    typedef std::map<std::pair<LLUUID, U64>, std::vector<F32> > palette_map_t;

    struct Instance
    {
        LLUUID      mId;                // instance key (minted at Add)
        std::string mName;              // session-only display name; not an identity key
        LLUUID      mSource;            // cast member id; null = "my avatar"
        EBackingKind mKind = BACKING_OVERLAY;
        LLUUID      mEntityId;          // weak runtime id; never owns the clone
        std::string mSourceLabel;       // cached UI label
        ERenderIntent mRenderIntent = RENDER_INHERIT_GLOBAL;
        ELifecycleState mState = STATE_READY;
        bool        mEnabled = true;
        LLUUID      mGroupId;            // non-null = locked, indivisible unit
        ELockMode   mLockMode = LOCK_OFF;

        // ---- entity-clone animation drive (Track B; ignored for overlays) ----
        EDriveMode  mDriveMode = DRIVE_MIRROR;
        LLUUID      mDirectedAnim;       // anim asset played in DRIVE_DIRECTED
        F32         mAnimSpeed = 1.f;    // per-clone multiplier, before Chaos
        bool        mPhysicsEnabled = true;
        ELoopMode   mLoopMode = LOOP_RETRIGGER;
        bool        mRestartOnResume = false;
        EDriveMode  mResumeDriveMode = DRIVE_MIRROR;
        LLUUID      mResumeDirectedAnim;
        U8          mPendingFreezeFrames = 0;
        bool        mWasDirectorSubjectA = false;
        bool        mWasDirectorSubjectB = false;
        bool        mWasDirectorSubjectC = false;
        bool        mWasDirectorSubjectD = false;
        bool        mWasCinematicFollow = false;

        // ---- placement ----
        LLVector3d  mFootGlobal;        // ghost FOOT position, global coords
        // Orientation about the foot pivot (identity = the source's facing).
        // mYaw is gone: yaw-only UI/gestures go through getYaw()/setYaw(), and
        // full 3-axis editing (the manip proxy) writes mRotation directly.
        LLQuaternion mRotation;
        F32         mScale = 1.f;       // uniform, pivoted at the foot (feet stay planted)
        ELookTarget mLookTarget = LOOK_TARGET_CAMERA;
        LLUUID      mLookTargetId;      // actor/cast id or Ghost Studio instance id
        LLVector3d  mLookPointGlobal;   // used when mLookTarget == LOOK_TARGET_POINT
        bool        mKeepFacing = false;
        EFormationFacing mCrowdFacing = FACING_AUTHOR;
        U32         mCrowdSlot = 0;
        F32         mCrowdBaseYaw = 0.f;
        F64         mCrowdFacingStart = 0.0;

        // ---- body turn (independent of look-at above) ----
        ETurnMode   mTurnMode = TURN_MODE_OFF;
        ELookTarget mTurnTarget = LOOK_TARGET_CAMERA;
        LLUUID      mTurnTargetId;
        LLVector3d  mTurnPointGlobal;   // used when mTurnTarget == LOOK_TARGET_POINT
        // TURN_MODE_ONCE clears itself once it has settled, so it does not keep
        // fighting the director if they hand-rotate the clone afterwards.
        bool        mTurnSettled = false;
        // Where the clone WAS when it settled. stepTurn() re-arms whenever the
        // foot has moved since. Checked at the choke point rather than in the
        // setters, because formation motion, chaos and the overlay duplication
        // paths all write mFootGlobal DIRECTLY and would each have to remember
        // to re-arm -- exactly the kind of cooperation that rots.
        LLVector3d  mTurnSettledFoot;
        bool        mChaosEnabled = false;
        F32         mChaosAmount = 0.f;
        bool        mChaosHasBase = false;
        LLVector3d  mChaosBaseFoot;
        LLQuaternion mChaosBaseRotation;
        F32         mChaosBaseScale = 1.f;

        // Authored transform retained while procedural formation motion is on.
        EMotion     mMotion = MOTION_OFF;
        F32         mMotionSpeed = 0.2f;     // cycles/second
        F32         mMotionAmplitude = 1.f; // metres, or radial fraction for breathe
        bool        mMotionHasBase = false;
        LLVector3d  mMotionBaseFoot;
        LLQuaternion mMotionBaseRotation;
        F32         mMotionBaseScale = 1.f;
        LLVector3d  mMotionCentre;
        F32         mMotionPathYaw = 0.f;
        S32         mMotionSlot = 0;
        F64         mMotionStart = 0.0;
        F32         mPoseRateHz = 0.f;    // 0 = smooth/live every frame
        F32         mEffectFps = 0.f;     // 0 = smooth shader time, 1..30 = stepped
        F64         mNextPoseRefresh = 0.0;
        LLVector3   mCadenceFootAgent;
        LLVector3   mCadenceHeadAgent;
        bool        mCadenceHeadValid = false;
        palette_map_t mCadencePalettes;

        // [ManipProxy] bumped on EXTERNAL transform edits (panel numeric, array
        // helpers). The in-world manip proxy PULL writes mFootGlobal/mRotation
        // DIRECTLY without bumping this, so it never triggers a push-back that
        // would fight an in-progress drag. Starts at 1 so a fresh proxy (which
        // seeds mSeenRevision at 0) gets an initial push.
        U64         mTransformRevision = 1;

        // Heading (radians, CCW from +X -- the former mYaw convention), extracted
        // from mRotation; exact for a pure world-Z rotation (the grounded edit
        // default), a reasonable heading under authored pitch/roll.
        F32  getYaw() const
        {
            const LLVector3 fwd = LLVector3::x_axis * mRotation;
            return atan2f(fwd.mV[VY], fwd.mV[VX]);
        }
        // Grounded edit: replace orientation with a pure world-Z rotation (no
        // pitch/roll authored on this path). Free-rotate sets mRotation directly.
        void setYaw(F32 yaw_rad)
        {
            mRotation = LLQuaternion(yaw_rad, LLVector3::z_axis);
            if (mMotionHasBase) mMotionBaseRotation = mRotation;
            mChaosHasBase = false;
            ++mTransformRevision;
        }
        // External foot / full-transform edits (bump the revision so the in-world
        // proxy re-syncs). The proxy pull deliberately does NOT use these.
        void setFootGlobal(const LLVector3d& foot)
        {
            mFootGlobal = foot;
            if (mMotionHasBase) mMotionBaseFoot = foot;
            mChaosHasBase = false;
            ++mTransformRevision;
        }
        void setTransform(const LLVector3d& foot, const LLQuaternion& rot)
        {
            mFootGlobal = foot;
            mRotation   = rot;
            if (mMotionHasBase)
            {
                mMotionBaseFoot = foot;
                mMotionBaseRotation = rot;
            }
            mChaosHasBase = false;
            ++mTransformRevision;
        }
        void setScale(F32 scale)
        {
            mScale = scale;
            if (mMotionHasBase) mMotionBaseScale = scale;
            mChaosHasBase = false;
            ++mTransformRevision;    // re-syncs the manip proxy box to the new size
        }

        // ---- look ----
        S32         mStyle = 0;         // EGhostStyle id (same values as PathGhostStyle)
        F32         mAlpha = 0.6f;      // base opacity (matches the path-ghost default)
        bool        mUseActorTint = true;   // tint from the source's stable path hue
        F32         mHue = 200.f;       // degrees 0..360, used when !mUseActorTint
        // [R2-1] output brightness: the ghost draws unlit into the post-
        // tonemap overlay, so a clone reads FULLBRIGHT in a night scene --
        // dial ~0.3-0.5 to sit it into dark sets. 1 = as authored.
        F32         mBrightness = 1.f;  // 0.05..1.5
        EGhostLook  mLook = LOOK_NORMAL;
        F32         mLookAlpha = 0.42f;

        // ---- cheap creative FX (each defaults OFF = byte-identical output) --
        F32         mShimmerSpeed = 1.f;      // Hz (only matters when intensity > 0)
        F32         mShimmerIntensity = 0.f;  // 0..1 brightness/alpha wobble depth
        F32         mPixelSize = 0.f;         // screen-space pixelation block, px (0 = off)
        F32         mGlitch = 0.f;            // 0..1 slice-offset + chroma-split amount
        S32         mDistort = 0;              // independent EGhostDistortion id
        F32         mDistortAmount = 0.5f;     // 0..1 strength for selected distortion

        // ---- pose ----
        S32         mPose = POSE_LIVE;
        // FROZEN capture: the source's foot in the CAPTURE-TIME agent frame --
        // the same frame the frozen palettes bake their vertices into, which is
        // what makes the pivot math region-crossing-proof (see file header)
        LLVector3   mFrozenFootAgent;
        LLVector3   mFrozenHeadAgent;
        bool        mFrozenHeadValid = false;
        palette_map_t mFrozenPalettes;
        // [R2-2] frozen NON-RIGGED attachment placement: object id -> that
        // object's render matrix at freeze time (capture agent frame, like
        // the palettes). A draw-time miss keeps the LIVE matrix; FLEXI
        // attachments always render at live physics pose (their vertices are
        // CPU-deformed in the shared buffer every frame -- documented
        // limitation, see the freeze tooltip).
        std::map<LLUUID, LLMatrix4> mFrozenAttachMats;
    };

    // ---- master visibility ----
    // one-click "hide all" that keeps every per-instance enable intact
    // (session-only by design: a persisted hidden-everything would read as
    // "the studio is broken" next session)
    bool getShowAll() const { return mShowAll; }
    void setShowAll(bool on);
    // the render/collect gate: master on AND at least one enabled instance
    bool anyEnabled() const;

    // ---- [R2-3] shared edit selection ----
    // THE selected instance, shared by every UI over the studio -- both panel
    // hosts' lists AND the in-world edit tool (ALToolGhostEdit) read/write it,
    // the same one-selection model as LLActorMover's edit actor/node. The
    // panels mirror it into their list each draw; the selected ghost gets the
    // in-world highlight ring while the edit tool is active.
    void          setSelected(const LLUUID& id);
    const LLUUID& getSelected() const { return mSelected; }
    U64           getSelectionRevision() const { return mSelectionRevision; }

    // ---- instance CRUD ----
    // Add spawns at the source's current rendered feet (so a fresh ghost is
    // immediately visible standing in the actor); nullptr when the source is
    // not resolvable in world. Duplicate offsets the copy one step sideways so
    // it never lands invisibly inside the original.
    Instance* addInstance(const LLUUID& source);
    // Unified, capability-selected front door. Legacy CRUD remains available
    // while direct spawn/refresh/crowd callers migrate in controlled stages.
    ALGhostSpawnResult spawnClone(const ALGhostCloneRequest& request);
    bool updateClone(const LLUUID& id, const ALGhostCloneUpdate& update);
    bool lockClone(const LLUUID& id);
    bool despawnClone(const LLUUID& id);

    Instance* spawnEntityClone(const LLUUID& source, const std::string& source_label);
    Instance* duplicateInstance(const LLUUID& id);
    Instance* duplicateInstanceInPlace(const LLUUID& id);
    // Duplicate a crowd as one atomic authoring entity. On success the ordered
    // member ids are returned; on any backing/model failure nothing is kept.
    std::vector<LLUUID> duplicateGroup(const LLUUID& id, bool in_place);
    bool      renameInstance(const LLUUID& id, const std::string& name);
    bool      setInstanceEnabled(const LLUUID& id, bool enabled);
    bool      setInstanceScale(const LLUUID& id, F32 scale);
    bool      setPoseRate(const LLUUID& id, F32 hz);
    bool      setEffectFps(const LLUUID& id, F32 fps);
    bool      refreshPoseCadence(const LLUUID& id, F64 now);
    bool      setInstanceAnimSpeed(const LLUUID& id, F32 speed);
    bool      setInstancePhysicsEnabled(const LLUUID& id, bool enabled);
    bool      setInstancePaused(const LLUUID& id, bool paused);
    bool      setInstanceLoopMode(const LLUUID& id, ELoopMode mode);
    bool      restartInstanceAnimation(const LLUUID& id);
    bool      applyEntityTransform(const LLUUID& id);
    // Yaw-only client transform. target_global and mFootGlobal share the
    // global frame; entity clones are pushed, overlays consume mRotation.
    bool      aimInstanceAt(const LLUUID& id, const LLVector3d& target_global);
    bool      faceInstance(const LLUUID& id);
    void      setLookTarget(const LLUUID& id, ELookTarget target,
                            const LLUUID& target_id, bool keep_facing);
    void      updateLookAt();
    void      updatePerFrame();
    bool      refreshEntityClone(const LLUUID& id);   // re-pull source appearance + worn attachments onto the clone
    bool      setInstanceChaos(const LLUUID& id, F32 amount);
    bool      setInstanceLook(const LLUUID& id, EGhostLook look);
    LLGhostAvatar* resolveEntityClone(const LLUUID& id) const;
    bool      setInstanceDriveMode(const LLUUID& id, EDriveMode mode,
                                   const LLUUID& directed_anim = LLUUID::null);
    void      refreshLifecycleStates();
    void      removeInstance(const LLUUID& id);
    S32       lockGroup(const std::vector<LLUUID>& ids,
                        ELockMode mode = LOCK_RIGID_UNIT);
    S32       setLockMode(const LLUUID& id, ELockMode mode);
    S32       ungroup(const LLUUID& id);
    std::vector<LLUUID> groupMembers(const LLUUID& id) const;
    const ALGhostGroupModel::Group* groupForMember(const LLUUID& id) const;
    ALGhostGroupModel::Group* groupForMember(const LLUUID& id);
    bool      renameGroup(const LLUUID& id, const std::string& name);
    bool      setGroupEditMembers(const LLUUID& id, bool editing);
    bool      resetGroupMemberOffset(const LLUUID& id);
    bool      setGroupMemberPinned(const LLUUID& id, bool pinned);
    bool      getUnitTransform(const LLUUID& id, LLVector3d& foot,
                               LLQuaternion& rotation, F32& scale) const;
    bool      getGroupScaleLimits(const LLUUID& id,
                                  F32& minimum, F32& maximum) const;
    bool      transformGroup(const LLUUID& id, const LLVector3d& foot,
                             const LLQuaternion& rotation, F32 scale);
    bool      transformUnit(const LLUUID& id, const LLVector3d& foot,
                             const LLQuaternion& rotation, F32 scale);
    void      removeAll();
    S32       removeEntityClones();
    Instance* getInstance(const LLUUID& id);
    // const overload: the target resolver and the formation preview are both
    // read-only and must not need a mutable studio to look an instance up.
    const Instance* getInstance(const LLUUID& id) const;
    const std::vector<Instance>& getInstances() const { return mInstances; }
    std::vector<Instance>&       getInstances()       { return mInstances; }

    // ---- FROZEN pose (the out-of-sync feature) ----
    // freezeInstance snapshots the source's CURRENT matrix palettes per
    // (drawing avatar, skin hash) from the frame's collected ghost batches,
    // plus the frame-matched foot anchor, and flips the instance to FROZEN.
    // False (and stays LIVE) when there is nothing to snapshot -- the ghost
    // pipeline must be rendering the source this frame (enabled instance or
    // path ghosts) for the batch set to exist. unfreeze drops the snapshot.
    bool freezeInstance(const LLUUID& id);
    void unfreezeInstance(const LLUUID& id);

    // ---- array helper ----
    // `parameter` is degrees for Arc/V, rise metres for Staircase, and radius
    // metres for Scatter. Zero selects the shape's natural default.
    S32 makeArray(const LLUUID& id, S32 count, F32 spacing,
                  EFormation formation, F32 parameter = 0.f,
                  const FormationOptions& options = FormationOptions());

    // ---- interactive crowd placement ----
    // beginCrowdPlacement() is the only acquisition path: it snapshots the
    // prototype and creates a new owner/session. Later edits resolve a new
    // immutable world-space slot list and bump the draft revision. Commit
    // consumes that exact list; it never queries terrain/camera/targets again.
    bool beginCrowdPlacement(const LLUUID& owner_id,
                             const LLUUID& prototype_id,
                             const FormationSpec& spec,
                             const LLVector3d& anchor,
                             F32 yaw, F32 height = 0.f);
    bool updateCrowdPlacementSpec(const LLUUID& owner_id,
                                  const FormationSpec& spec);
    bool updateCrowdPlacementTransform(const LLUUID& owner_id,
                                       const LLVector3d& anchor,
                                       F32 yaw, F32 height);
    bool setCrowdPlacementPinned(const LLUUID& owner_id, bool pinned);
    bool cancelCrowdPlacement(const LLUUID& owner_id = LLUUID::null);
    PlacementCommitResult commitCrowdPlacement(const LLUUID& owner_id,
                                                const LLUUID& session_id);
    const CrowdPlacementDraft& getCrowdPlacementDraft() const
    {
        return mCrowdDraft;
    }
    bool crowdPlacementSourceUnchanged(std::string* reason = nullptr) const;

    // Captures are scheduled on updatePerFrame(), one at t0, then at real-time
    // interval boundaries. Starting another strip is allowed; each job owns
    // its source id and output names independently.
    U32  startFreezeStrip(const LLUUID& source_id, S32 count, F32 interval,
                          F32 spacing, EFormation formation,
                          F32 parameter = 0.f);
    bool cancelFreezeStrip(U32 strip_id);
    std::string freezeStripStatus() const;

    // Enables/reconfigures one motion group. OFF restores every member's
    // authored baseline. Invalid/deleted ids are ignored.
    S32 setFormationMotion(const std::vector<LLUUID>& ids, EMotion motion,
                           F32 speed, F32 amplitude);

    // ---- body turn ----
    // Set an instance's TURN target. Independent of setLookTarget(); calling
    // one never disturbs the other.
    void setTurnTarget(const LLUUID& id, ETurnMode mode, ELookTarget target,
                       const LLUUID& target_id,
                       const LLVector3d& point_global = LLVector3d());
    // Rate-limited step toward the turn target. Returns false when there is
    // nothing to do. Driven from updatePerFrame().
    bool stepTurn(const LLUUID& id, F32 dt);
    // Resolve any target kind to a world position. Shared by look and turn.
    bool resolveTargetGlobal(const Instance& inst, ELookTarget target,
                             const LLUUID& target_id,
                             const LLVector3d& point_global,
                             LLVector3d& out) const;

    // ONE definition of where a formation puts slot N, and which way it faces.
    //
    // These used to be TWO implementations -- formationSlot() and the inline
    // per-formation code in makeArray() -- and they DISAGREED: V extended
    // forward vs backward, Spiral stepped 0.8 rad vs the golden angle,
    // Staircase defaulted 0.75m vs spacing*0.5, Tunnel was spacing vs
    // spacing*0.5 each side, Scatter's radius and seed channels both differed.
    // That is why placement felt erratic, and it is why a preview built on the
    // old helper would have drawn one thing and placed another.
    //
    // makeArray() was the authoritative one (it is what actually places
    // clones), so this matches makeArray's behaviour exactly and makeArray now
    // consumes it. Existing arrays are unchanged.
    struct FormationSlot
    {
        LLVector3d mFoot;
        F32        mYaw = 0.f;
        // false = inherit the prototype's full rotation rather than authoring a
        // yaw (Line/Grid/Staircase/Scatter). Ring/Arc/Spiral face outward, V
        // angles along its arm, Tunnel faces inward.
        bool       mAuthorYaw = true;
    };
    // ---- formation preview ----
    // Every slot a makeArray() with these arguments WOULD produce, including
    // slot 0 (the prototype itself). Same code path as the real build, so the
    // preview cannot disagree with the result. Empty if the id is unknown or
    // the arguments are rejected by the same guards makeArray uses.
    void formationPreviewSlots(const LLUUID& id, S32 count, F32 spacing,
                               EFormation formation, F32 parameter,
                               std::vector<FormationSlot>& out,
                               const FormationOptions& options = FormationOptions()) const;

    // Draw the stored reliable crowd draft in-world. The preview consumes the
    // same fully resolved world transforms commitCrowdPlacement() consumes.
    void renderFormationPreview();

    // ---- render-side queries ----
    // raw source ids (may include null = self) of enabled instances; the batch
    // collector resolves + de-dupes them into its wanted set
    void getWantedSources(uuid_vec_t& out) const;

private:
    struct FreezeStrip
    {
        U32 mId = 0;
        LLUUID mSourceId;
        S32 mCount = 0;
        S32 mCaptured = 0;
        F32 mInterval = 0.f;
        F32 mSpacing = 1.5f;
        EFormation mFormation = FORMATION_LINE;
        F32 mParameter = 0.f;
        F64 mNextCapture = 0.0;
    };
    FormationSlot formationSlotAt(const Instance& proto, S32 slot, S32 count,
                                  F32 spacing, EFormation formation,
                                  F32 parameter,
                                  const FormationOptions& options = FormationOptions()) const;
    LLVector3d formationSlot(const Instance& proto, S32 slot, S32 count,
                             F32 spacing, EFormation formation,
                             F32 parameter) const;
    bool resolveCrowdPlacement();
    Instance* duplicateInstanceSnapshotInPlace(Instance prototype);
    static bool solverShape(EFormation formation,
                            ALFormationSolver::Shape& shape);
    static std::string formationRole(EFormation formation, U32 slot);
    CrowdPlacementDraft mCrowdDraft;
    U64 mNextCrowdDraftRevision = 1;
    bool mCrowdCommitInProgress = false;

    void stepAllTurns();
    void updateFreezeStrips(F64 now);
    void updateFormationMotion(F64 now);
    LLGhostAvatar* createEntityRuntime(Instance& inst, S32& attachments);
    void applyEntityRuntimeState(Instance& inst, LLGhostAvatar* ghost);
    void onEntityRuntimeReplaced(Instance& inst, const LLUUID& new_runtime,
                                 LLGhostAvatar* new_ghost = nullptr,
                                 bool removing_instance = false);
    void finishPendingRuntimeFreezes();
    bool applyGroupTransforms(
        const LLUUID& group_id,
        const ALGhostGroupModel::Group* rollback_model = nullptr);
    bool nameplateAnchor(const Instance& inst, LLVector3& anchor_agent) const;
    void updateNameplates();
    std::string makeDefaultName() const;
    ALGhostStudio();
    ~ALGhostStudio();

    using runtime_consumer_t =
        std::function<void(const LLUUID&, const LLUUID&, const LLUUID&, bool)>;
    std::vector<runtime_consumer_t> mRuntimeConsumers;
    ALGhostGroupModel mGroups;
    std::unique_ptr<ALGhostNameplates> mNameplates;
    U32 mNextGroupNumber = 1;

    std::vector<Instance> mInstances;
    bool mShowAll = true;
    LLUUID mSelected;       // [R2-3] shared edit selection (null = none)
    U64    mSelectionRevision = 0;
    std::vector<FreezeStrip> mFreezeStrips;
    U32 mNextStripId = 1;
    std::string mLastStripStatus;
    bool mHasMotion = false;
};

#endif // AL_ALGHOSTSTUDIO_H
