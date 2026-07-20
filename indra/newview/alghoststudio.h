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

#include <map>
#include <string>
#include <utility>
#include <vector>

class ALGhostStudio
{
public:
    static ALGhostStudio& instance();

    enum EPose : S32
    {
        POSE_LIVE   = 0,    // skins from the source's live palette every frame
        POSE_FROZEN = 1,    // skins from the snapshot captured at freeze time
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
        LLUUID      mSource;            // cast member id; null = "my avatar"
        bool        mEnabled = true;

        // ---- placement ----
        LLVector3d  mFootGlobal;        // ghost FOOT position, global coords
        F32         mYaw = 0.f;         // radians, about the ghost's vertical axis
        F32         mScale = 1.f;       // uniform, pivoted at the foot (feet stay planted)

        // ---- look ----
        S32         mStyle = 0;         // EGhostStyle id (same values as PathGhostStyle)
        F32         mAlpha = 0.6f;      // base opacity (matches the path-ghost default)
        bool        mUseActorTint = true;   // tint from the source's stable path hue
        F32         mHue = 200.f;       // degrees 0..360, used when !mUseActorTint
        // [R2-1] output brightness: the ghost draws unlit into the post-
        // tonemap overlay, so a clone reads FULLBRIGHT in a night scene --
        // dial ~0.3-0.5 to sit it into dark sets. 1 = as authored.
        F32         mBrightness = 1.f;  // 0.05..1.5

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
    void setShowAll(bool on) { mShowAll = on; }
    // the render/collect gate: master on AND at least one enabled instance
    bool anyEnabled() const;

    // ---- instance CRUD ----
    // Add spawns at the source's current rendered feet (so a fresh ghost is
    // immediately visible standing in the actor); nullptr when the source is
    // not resolvable in world. Duplicate offsets the copy one step sideways so
    // it never lands invisibly inside the original.
    Instance* addInstance(const LLUUID& source);
    Instance* duplicateInstance(const LLUUID& id);
    void      removeInstance(const LLUUID& id);
    void      removeAll();
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
    // Duplicate the instance into a LINE (along the ghost's yaw direction) or
    // a RING (centred on the ghost) of `count` total ghosts spaced `spacing`
    // metres apart. Returns how many new instances were created.
    S32 makeArray(const LLUUID& id, S32 count, F32 spacing, bool ring);

    // ---- render-side queries ----
    // raw source ids (may include null = self) of enabled instances; the batch
    // collector resolves + de-dupes them into its wanted set
    void getWantedSources(uuid_vec_t& out) const;

private:
    ALGhostStudio() = default;

    std::vector<Instance> mInstances;
    bool mShowAll = true;
};

#endif // AL_ALGHOSTSTUDIO_H
