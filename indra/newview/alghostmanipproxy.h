/**
 * @file alghostmanipproxy.h
 * @brief In-world build-mode manipulation for Ghost Studio instances via a
 *        client-only proxy object.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The stock build-mode manipulators (LLManipTranslate/Rotate via LLToolComp*)
 * only drive a selected LLViewerObject. A Ghost Studio Instance is not one, so
 * this owns ONE invisible, local-only LLVOVolume "proxy" whose transform IS the
 * selected instance's transform: selecting it and switching to the stock
 * translate/rotate tool gives the ghost the familiar 3-axis gizmos, and a
 * per-frame tick() syncs the proxy <-> Instance (the Instance stays
 * authoritative). Uniform scale stays on the panel numeric for now.
 *
 * SAFETY: the proxy is mIsLocalOnly + LOCAL_OBJECT_GHOST_MANIP_PROXY, so
 * llselectmgr suppresses all sim traffic and never routes it into LLLocalMeshMgr
 * (see the isLocalMeshPreview() split). Edit-mode lifetime is an EXPLICIT state,
 * independent of "the current tool is ALToolGhostEdit": switching to the stock
 * toolset necessarily deselects the transient ghost tool, and that MUST NOT tear
 * the proxy down (else the stock manipulator holds a dead object).
 */

#ifndef AL_ALGHOSTMANIPPROXY_H
#define AL_ALGHOSTMANIPPROXY_H

#include "llpointer.h"
#include "llquaternion.h"
#include "lluuid.h"
#include "stdtypes.h"
#include "v3dmath.h"

class LLTool;
class LLToolset;
class LLViewerObject;
class LLViewerRegion;
class LLVOVolume;

class ALGhostManipProxy
{
public:
    // ctor AND dtor are out-of-line so the LLPointer<LLVOVolume> member's
    // ref/unref is only instantiated in the .cpp (where LLVOVolume is complete),
    // not in every TU that includes this header with only the forward decl.
    ALGhostManipProxy();
    ~ALGhostManipProxy();

    // Enter edit mode (arms the proxy on the next tick). The proxy follows the
    // shared ALGhostStudio::getSelected() -- both the panel list and the in-world
    // pick already write it, so no extra selection wiring is needed.
    void begin();
    // Per-frame: follow the selection, (re)create/select the proxy, pull while a
    // manip drags, push on an external edit, reassert invisibility. Tears down
    // when not active.
    void tick();
    // Idempotent: release capture, deselect, destroy the proxy. restore_toolset
    // restores the pre-edit toolset (true for an explicit panel/Esc exit; FALSE
    // when the user deliberately switched away from the stock toolset, so we must
    // not yank them back).
    void teardown(bool restore_toolset = true);

    bool isActive() const { return mEditMode; }
    // True if `object` IS our proxy (for pick/command policy callers).
    bool owns(const LLViewerObject* object) const;

private:
    // Canonical avatar-sized manipulation bounds (world metres). The proxy volume
    // is a UNIT cube; object scale gives it these dimensions.
    static constexpr F32 PROXY_WIDTH  = 0.60f;
    static constexpr F32 PROXY_DEPTH  = 0.45f;
    static constexpr F32 PROXY_HEIGHT = 1.80f;

    bool createProxy(LLViewerRegion* region);
    void destroyProxy();
    void selectProxy();
    // Transition to "State B": no instance is selected, so drop the proxy and hand
    // the in-world ghost picker (ALToolGhostEdit) back to the user WITHOUT leaving
    // edit mode. Idempotent -- owns drag-cancel, capture release, deselect, proxy
    // destroy, bookkeeping reset, and transient-picker install, so the deleted /
    // deselected / removed paths all funnel through one place.
    void enterPickerState();
    void pushInstanceToProxy();     // Instance -> proxy transform
    void pullProxyToInstance();     // proxy transform -> Instance (no revision bump)

    bool manipHasMouseCapture() const;
    void beginDrag();
    void endDrag(bool commit);

    LLPointer<LLVOVolume> mProxy;
    LLViewerRegion*       mRegion = nullptr;   // compared only; never deref after a mismatch
    LLUUID                mInstanceId;

    LLToolset*  mSavedToolset = nullptr;
    LLTool*     mSavedTool    = nullptr;

    U64  mSeenRevision = 0;
    bool mEditMode = false;
    bool mDragging = false;

    // Pre-drag transform snapshot, restored when a drag is cancelled.
    LLVector3d   mDragStartFoot;
    LLQuaternion mDragStartRotation;
    F32          mDragStartScale = 1.f;
};

#endif // AL_ALGHOSTMANIPPROXY_H
