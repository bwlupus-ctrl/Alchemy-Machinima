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

#include "lluuid.h"
#include "v3math.h"
#include "v3dmath.h"
#include "m4math.h"         // frozen attachment matrices
#include "llquaternion.h"   // per-instance orientation

#include <map>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class LLGhostAvatar;

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
    enum EGhostLook : S32
    {
        LOOK_NORMAL = 0, LOOK_APPARITION, LOOK_HOLOGRAM, LOOK_CHROME,
        LOOK_TOON, LOOK_SILHOUETTE
    };
    enum ELookTarget : S32
    {
        LOOK_TARGET_CAMERA = 0, LOOK_TARGET_ME, LOOK_TARGET_ACTOR,
        LOOK_TARGET_GHOST
    };
    enum EFormation : S32
    {
        FORMATION_LINE = 0, FORMATION_RING, FORMATION_ARC, FORMATION_GRID,
        FORMATION_V, FORMATION_SPIRAL, FORMATION_STAIRCASE, FORMATION_TUNNEL,
        FORMATION_SCATTER
    };
    enum EMotion : S32
    {
        MOTION_OFF = 0, MOTION_ORBIT, MOTION_SPIN, MOTION_BREATHE, MOTION_RIPPLE
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
        ELifecycleState mState = STATE_READY;
        bool        mEnabled = true;

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
        bool        mKeepFacing = false;
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
        S32         mMotionSlot = 0;
        F64         mMotionStart = 0.0;

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

        // ---- pose ----
        S32         mPose = POSE_LIVE;
        // FROZEN capture: the source's foot in the CAPTURE-TIME agent frame --
        // the same frame the frozen palettes bake their vertices into, which is
        // what makes the pivot math region-crossing-proof (see file header)
        LLVector3   mFrozenFootAgent;
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
    void          setSelected(const LLUUID& id) { mSelected = id; }
    const LLUUID& getSelected() const { return mSelected; }

    // ---- instance CRUD ----
    // Add spawns at the source's current rendered feet (so a fresh ghost is
    // immediately visible standing in the actor); nullptr when the source is
    // not resolvable in world. Duplicate offsets the copy one step sideways so
    // it never lands invisibly inside the original.
    Instance* addInstance(const LLUUID& source);
    Instance* spawnEntityClone(const LLUUID& source, const std::string& source_label);
    Instance* duplicateInstance(const LLUUID& id);
    Instance* duplicateInstanceInPlace(const LLUUID& id);
    bool      renameInstance(const LLUUID& id, const std::string& name);
    bool      setInstanceEnabled(const LLUUID& id, bool enabled);
    bool      setInstanceScale(const LLUUID& id, F32 scale);
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
    void      removeAll();
    S32       removeEntityClones();
    Instance* getInstance(const LLUUID& id);
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
                  EFormation formation, F32 parameter = 0.f);

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
    LLVector3d formationSlot(const Instance& proto, S32 slot, S32 count,
                             F32 spacing, EFormation formation,
                             F32 parameter) const;
    void updateFreezeStrips(F64 now);
    void updateFormationMotion(F64 now);
    LLGhostAvatar* createEntityRuntime(Instance& inst, S32& attachments);
    void applyEntityRuntimeState(Instance& inst, LLGhostAvatar* ghost);
    void onEntityRuntimeReplaced(Instance& inst, const LLUUID& new_runtime,
                                 LLGhostAvatar* new_ghost = nullptr,
                                 bool removing_instance = false);
    void finishPendingRuntimeFreezes();
    std::string makeDefaultName() const;
    ALGhostStudio();

    using runtime_consumer_t =
        std::function<void(const LLUUID&, const LLUUID&, const LLUUID&, bool)>;
    std::vector<runtime_consumer_t> mRuntimeConsumers;

    std::vector<Instance> mInstances;
    bool mShowAll = true;
    LLUUID mSelected;       // [R2-3] shared edit selection (null = none)
    std::vector<FreezeStrip> mFreezeStrips;
    U32 mNextStripId = 1;
    std::string mLastStripStatus;
    bool mHasMotion = false;
};

#endif // AL_ALGHOSTSTUDIO_H
