=== CODEX PHASE-1B BLUEPRINT COMPLETE ===
Below is the code shape I would implement. One important correction to the earlier plan: switching from transient `ALToolGhostEdit` to `gBasicToolset` clears the transient tool (`LLToolMgr::setCurrentTool()` does that at [lltoolmgr.cpp:186](I:/alchemy-machinima/indra/newview/lltoolmgr.cpp:186)). Therefore `ALToolGhostEdit::handleDeselect()` must not automatically destroy the proxy during the intentional handoff to the stock manipulator. Edit-mode lifetime must remain a separate explicit state.

There is currently no Ghost Studio revision counter or undo subsystem. Phase 1b must add those small APIs; otherwise “one undo transaction per drag” cannot be implemented honestly.

## 1. New proxy class

[alghostmanipproxy.h](I:/alchemy-machinima/indra/newview/alghostmanipproxy.h) should look like:

```cpp
#ifndef AL_ALGHOSTMANIPPROXY_H
#define AL_ALGHOSTMANIPPROXY_H

#include "llpointer.h"
#include "llquaternion.h"
#include "lluuid.h"

class LLTool;
class LLToolset;
class LLViewerRegion;
class LLVOVolume;

class ALGhostManipProxy
{
public:
    ALGhostManipProxy() = default;
    ~ALGhostManipProxy();

    void begin(const LLUUID& instance_id);
    void selectInstance(const LLUUID& instance_id);
    void tick();
    void teardown();

    bool isActive() const { return mEditMode; }
    bool owns(const LLViewerObject* object) const;

private:
    static constexpr F32 PROXY_WIDTH  = 0.60f;
    static constexpr F32 PROXY_DEPTH  = 0.45f;
    static constexpr F32 PROXY_HEIGHT = 1.80f;

    bool createProxy(LLViewerRegion* region);
    void destroyProxy();
    void selectProxy();
    void pushInstanceToProxy();
    void pullProxyToInstance();

    bool manipHasMouseCapture() const;
    void beginDrag();
    void endDrag(bool commit);

    LLPointer<LLVOVolume> mProxy;
    LLViewerRegion* mRegion = nullptr; // compared only; never dereference after mismatch
    LLUUID mInstanceId;

    LLToolset* mSavedToolset = nullptr;
    LLTool* mSavedTool = nullptr;

    U64  mSeenRevision = 0;
    bool mEditMode = false;
    bool mSwitchingToStockTool = false;
    bool mDragging = false;
};

#endif
```

Use `LLPointer<LLVOVolume>`, never a raw owning pointer. `markDead()` can remove references during the call; the object-list code itself takes a strong pointer for that reason at [llviewerobjectlist.cpp:1446](I:/alchemy-machinima/indra/newview/llviewerobjectlist.cpp:1446).

## 2. Proxy creation

Adapt the local-mesh sequence at [lllocalmesh.cpp:1543](I:/alchemy-machinima/indra/newview/lllocalmesh.cpp:1543):

```cpp
bool ALGhostManipProxy::createProxy(LLViewerRegion* region)
{
    if (!region)
    {
        return false;
    }

    LLViewerObject* object =
        gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
    // Declaration/signature: llviewerobjectlist.h:72
    // Existing local-only usage: lllocalmesh.cpp:1543

    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume)
    {
        if (object)
        {
            object->markDead();
            // LLViewerObject::markDead(): llviewerobject.h:146
        }
        return false;
    }

    // Set these immediately, before anything can enqueue simulator traffic.
    volume->mbCanSelect = true;
    volume->mIsLocalOnly = true;
    volume->mLocalObjectKind =
        LLViewerObject::LOCAL_OBJECT_GHOST_MANIP_PROXY;

    volume->setFlagsWithoutUpdate(
        FLAGS_OBJECT_YOU_OWNER |
        FLAGS_OBJECT_MODIFY |
        FLAGS_OBJECT_MOVE |
        FLAGS_OBJECT_COPY |
        FLAGS_OBJECT_TRANSFER,
        true);
    // Identical owner setup: lllocalmesh.cpp:1555-1558

    gPipeline.createObject(volume);
    // Existing client-volume creation: lllocalmesh.cpp:1563
    // Ultimately calls LLVOVolume::createDrawable(): llvovolume.cpp:1036

    volume->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
    // Required before setVolume; precedent: lllocalmesh.cpp:1560-1564

    LLVolumeParams params;
    params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
    params.setBeginAndEndS(0.f, 1.f);
    params.setBeginAndEndT(0.f, 1.f);
    params.setRatio(1.f, 1.f);
    params.setShear(0.f, 0.f);
    // Known-valid box parameter sequence: alchatcommand.cpp:168-173

    if (!volume->setVolume(
            params,
            LLVolumeLODGroup::NUM_LODS - 1,
            true))
    {
        volume->markDead();
        return false;
    }
    // Signature: llvovolume.h:238
    // Implementation: llvovolume.cpp:1094

    // The canonical volume is a unit cube. Object scale is the desired
    // avatar-sized world bounding box.
    volume->setScale(
        LLVector3(PROXY_WIDTH, PROXY_DEPTH, PROXY_HEIGHT),
        false);
    // Signature: llviewerobject.h:412
    // Drawable-radius/extents update: llviewerobject.cpp:3926-3934

    mProxy = volume;
    mRegion = region;

    pushInstanceToProxy();

    // Do this only after volume, scale, position and rotation are valid.
    volume->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    // Flag definition: lldrawable.h:271
    // Render exclusion: pipeline.cpp:3897-3902

    return true;
}
```

The correct combination is:

```cpp
unit-box LLVolumeParams
+ volume->setScale(LLVector3(0.60f, 0.45f, 1.80f), false)
```

Do not encode the dimensions into path/profile parameters and also call `setScale()`: that double-scales manipulator bounds. `LLVOVolume::volumePositionToAgent()` applies object scale to unit-volume positions at [llvovolume.cpp:4703](I:/alchemy-machinima/indra/newview/llvovolume.cpp:4703).

## 3. Invisibility while retaining extents and picking

Use:

```cpp
mProxy->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
```

`FORCE_INVISIBLE` prevents pipeline visibility/render submission at [pipeline.cpp:3899](I:/alchemy-machinima/indra/newview/pipeline.cpp:3899), but does not change:

- the drawable render type;
- volume geometry;
- scale;
- spatial extents;
- `mbCanSelect`.

Do not disable `RENDER_TYPE_VOLUME`: `LLVOVolume::lineSegmentIntersect()` rejects objects whose drawable render type is disabled at [llvovolume.cpp:4730](I:/alchemy-machinima/indra/newview/llvovolume.cpp:4730).

Reassert `FORCE_INVISIBLE` after each push/tick because generic pipeline orphan/reparent processing contains paths which clear it, including [pipeline.cpp:15439](I:/alchemy-machinima/indra/newview/pipeline.cpp:15439).

```cpp
if (mProxy.notNull() && mProxy->mDrawable.notNull())
{
    mProxy->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
}
```

## 4. Center ↔ foot conversion

The proxy’s local center is `height / 2` above its local foot. Rotation may contain pitch and roll, so rotate the offset:

```cpp
static LLVector3d proxyCenterFromFoot(
    const LLVector3d& foot_global,
    const LLQuaternion& rotation,
    F32 height)
{
    const LLVector3 local_offset(0.f, 0.f, 0.5f * height);
    const LLVector3 world_offset = local_offset * rotation;
    return foot_global + LLVector3d(world_offset);
}

static LLVector3d footFromProxyCenter(
    const LLVector3d& center_global,
    const LLQuaternion& rotation,
    F32 height)
{
    const LLVector3 local_offset(0.f, 0.f, 0.5f * height);
    const LLVector3 world_offset = local_offset * rotation;
    return center_global - LLVector3d(world_offset);
}
```

Push:

```cpp
mProxy->setRotation(instance->mRotation, false);
// API: llviewerobject.h:358
// Implementation: llviewerobject.h:1082

mProxy->setPositionGlobal(
    proxyCenterFromFoot(instance->mFootGlobal,
                        instance->mRotation,
                        PROXY_HEIGHT),
    false);
// API: llviewerobject.h:349
// Implementation: llviewerobject.cpp:4677
```

Pull:

```cpp
const LLQuaternion rotation = mProxy->getRotation();
const LLVector3d center = mProxy->getPositionGlobal();
// getPositionGlobal API: llviewerobject.h:333
// implementation: llviewerobject.cpp:4452

instance->mRotation = rotation;
instance->mFootGlobal =
    footFromProxyCenter(center, rotation, PROXY_HEIGHT);
```

Use `PROXY_HEIGHT`, the same `1.80f` used in `setScale()`. Do not use `getPelvisToFoot()` here: the proxy represents canonical manipulation bounds, while Ghost Studio rendering already pivots the actual avatar geometry at its live/frozen foot around [llactormover.cpp:3730](I:/alchemy-machinima/indra/newview/llactormover.cpp:3730).

## 5. Selection and stock tool handoff

```cpp
void ALGhostManipProxy::selectProxy()
{
    if (mProxy.isNull() || mProxy->isDead())
    {
        return;
    }

    LLSelectMgr* select_mgr = LLSelectMgr::getInstance();
    select_mgr->deselectAll();
    // Existing API use: llfloatertelehub.cpp:182

    select_mgr->selectObjectOnly(mProxy.get(), SELECT_ALL_TES);
    // Existing exact call: lltoolselect.cpp:192

    LLToolMgr* tool_mgr = LLToolMgr::getInstance();

    if (!mSavedToolset)
    {
        mSavedToolset = tool_mgr->getCurrentToolset();
        // API: lltoolmgr.h:74
        mSavedTool = mSavedToolset
            ? mSavedToolset->getSelectedTool()
            : tool_mgr->getCurrentTool();
        // LLToolset API: lltoolmgr.h:101
        // LLToolMgr current-tool implementation: lltoolmgr.cpp:199
    }

    mSwitchingToStockTool = true;
    tool_mgr->setCurrentToolset(gBasicToolset);
    gBasicToolset->selectTool(LLToolCompTranslate::getInstance());
    mSwitchingToStockTool = false;
    // Exact telehub pattern: llfloatertelehub.cpp:77-78
    // Public LLToolset::selectTool(): lltoolmgr.h:105
}
```

Rotate can be selected later with:

```cpp
gBasicToolset->selectTool(LLToolCompRotate::getInstance());
```

Do not call private `LLToolMgr::setCurrentTool()`; it is protected at [lltoolmgr.h:81](I:/alchemy-machinima/indra/newview/lltoolmgr.h:81).

`synthesizeLocalPreviewNode()` already covers every `isLocalOnly()` object, not just local meshes:

```cpp
return obj && obj->isLocalOnly();
```

at [llselectmgr.cpp:111](I:/alchemy-machinima/indra/newview/llselectmgr.cpp:111). It sets `mValid`, ownership and full permission masks at [llselectmgr.cpp:162](I:/alchemy-machinima/indra/newview/llselectmgr.cpp:162). `selectObjectOnly()` reaches it at [llselectmgr.cpp:1163](I:/alchemy-machinima/indra/newview/llselectmgr.cpp:1163). No additional select-node construction is needed.

I would change its placeholder name for clarity:

```cpp
nodep->mName = objectp->isGhostManipProxy()
    ? "(ghost manipulation proxy)"
    : "(local mesh preview)";
```

## 6. Revision and undo additions

Add to `ALGhostStudio::Instance` near [alghoststudio.h:90](I:/alchemy-machinima/indra/newview/alghoststudio.h:90):

```cpp
U64 mTransformRevision = 1;

void setTransform(const LLVector3d& foot, const LLQuaternion& rotation)
{
    mFootGlobal = foot;
    mRotation = rotation;
    ++mTransformRevision;
}

void touchTransform()
{
    ++mTransformRevision;
}
```

Every panel/tool write currently assigning `mFootGlobal`, `mRotation`, or calling `setYaw()` must increment this revision. Better still, replace those writes with `setTransform()`/`setFootGlobal()`/`setRotation()`.

Add a minimal transform-only undo record to `ALGhostStudio`:

```cpp
struct TransformSnapshot
{
    LLUUID mInstanceId;
    LLVector3d mFootGlobal;
    LLQuaternion mRotation;
};

void beginTransformUndo(const LLUUID& id);
void commitTransformUndo(const LLUUID& id);
void cancelTransformUndo(const LLUUID& id);
```

There is no existing Ghost Studio undo transaction API in the current source. These are required new APIs, not wrappers around an existing viewer undo call.

## 7. Per-frame tick

Call it immediately before Ghost Studio rendering in [llviewerdisplay.cpp:1574](I:/alchemy-machinima/indra/newview/llviewerdisplay.cpp:1574):

```cpp
ALToolGhostEdit::getInstance()->getManipProxy().tick();
LLActorMover::instance().renderStudioGhosts();
```

This path is guaranteed to run in the same render phase as the ghosts and occurs before their draw. It also avoids coupling the proxy to the UI-debug-feature gate described at [llviewerdisplay.cpp:1565](I:/alchemy-machinima/indra/newview/llviewerdisplay.cpp:1565).

Tick body:

```cpp
void ALGhostManipProxy::tick()
{
    if (!mEditMode)
    {
        teardown();
        return;
    }

    ALGhostStudio& studio = ALGhostStudio::instance();
    ALGhostStudio::Instance* instance =
        studio.getInstance(mInstanceId);

    LLViewerRegion* region = gAgent.getRegion();
    // Existing region acquisition: lllocalmesh.cpp:1543

    if (!instance || !region)
    {
        teardown();
        return;
    }

    // Region pointers can become invalid after region removal. Compare only
    // while the strong proxy still reports the same current region.
    if (mProxy.isNull() ||
        mProxy->isDead() ||
        mProxy->getRegion() != region)
    {
        destroyProxy();

        // mFootGlobal is authoritative across region changes.
        if (!createProxy(region))
        {
            return;
        }
        selectProxy();
        mSeenRevision = instance->mTransformRevision;
    }

    const bool dragging = manipHasMouseCapture();

    if (dragging && !mDragging)
    {
        beginDrag();
    }

    if (dragging)
    {
        pullProxyToInstance();
    }
    else
    {
        if (mDragging)
        {
            endDrag(true);
        }

        if (instance->mTransformRevision != mSeenRevision)
        {
            pushInstanceToProxy();
            mSeenRevision = instance->mTransformRevision;
        }
    }

    if (mProxy->mDrawable.notNull())
    {
        mProxy->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
        // Flag: lldrawable.h:271
    }
}
```

Capture detection:

```cpp
bool ALGhostManipProxy::manipHasMouseCapture() const
{
    LLTool* tool = LLToolMgr::getInstance()->getCurrentTool();
    return tool &&
           tool != ALToolGhostEdit::getInstance() &&
           tool->hasMouseCapture();
}
```

Use the current `LLTool`’s `hasMouseCapture()`. `LLToolMgr::getCurrentTool()` deliberately preserves a selected tool while it owns mouse capture at [lltoolmgr.cpp:210](I:/alchemy-machinima/indra/newview/lltoolmgr.cpp:210). Do not inspect `gViewerWindow`; capture ownership is managed by `LLTool`/`gFocusMgr`, and querying only “some capture exists” would incorrectly treat camera/UI captures as manipulator drags.

Narrow it further to the stock composites if desired:

```cpp
const bool stock_tool =
    tool == LLToolCompTranslate::getInstance() ||
    tool == LLToolCompRotate::getInstance();

return stock_tool && tool->hasMouseCapture();
```

Because the composites delegate to internal manip subtools, the most robust version is to add a public `isManipulating()` accessor to `LLToolCompTranslate` and `LLToolCompRotate` that returns `mManip->hasMouseCapture()`. The internal manip is selected during drag at [lltoolcomp.cpp:297](I:/alchemy-machinima/indra/newview/lltoolcomp.cpp:297) and [lltoolcomp.cpp:631](I:/alchemy-machinima/indra/newview/lltoolcomp.cpp:631).

Drag edges:

```cpp
void ALGhostManipProxy::beginDrag()
{
    mDragging = true;
    ALGhostStudio::instance().beginTransformUndo(mInstanceId);
}

void ALGhostManipProxy::endDrag(bool commit)
{
    if (!mDragging)
    {
        return;
    }

    if (commit)
    {
        pullProxyToInstance();
        ALGhostStudio::instance().commitTransformUndo(mInstanceId);
    }
    else
    {
        ALGhostStudio::instance().cancelTransformUndo(mInstanceId);
    }

    mDragging = false;
}
```

## 8. Teardown

```cpp
void ALGhostManipProxy::destroyProxy()
{
    LLPointer<LLVOVolume> dying = mProxy;
    mProxy = nullptr;
    mRegion = nullptr;

    if (dying.notNull() && !dying->isDead())
    {
        dying->markDead();
        // API: llviewerobject.h:146
        // implementation: llvovolume.cpp:288
    }
}

void ALGhostManipProxy::teardown()
{
    if (!mEditMode &&
        mProxy.isNull() &&
        !mDragging &&
        !mSavedToolset)
    {
        return;
    }

    mEditMode = false;

    LLTool* current = LLToolMgr::getInstance()->getCurrentTool();
    if (current && current->hasMouseCapture())
    {
        current->setMouseCapture(false);
    }

    endDrag(false);

    LLSelectMgr::getInstance()->deselectAll();
    // API usage: llfloatertelehub.cpp:182

    destroyProxy(); // clears registry before markDead

    LLToolset* saved_set = mSavedToolset;
    LLTool* saved_tool = mSavedTool;
    mSavedToolset = nullptr;
    mSavedTool = nullptr;
    mInstanceId.setNull();
    mSeenRevision = 0;

    if (saved_set)
    {
        LLToolMgr::getInstance()->setCurrentToolset(saved_set);
        // Public API: lltoolmgr.h:73

        if (saved_tool)
        {
            saved_set->selectTool(saved_tool);
            // Public API: lltoolmgr.h:105
        }
    }
}
```

Trigger teardown on:

- Ghost edit toggle turned off;
- explicit Escape;
- viewer logout/disconnect;
- `ALToolGhostEdit` destruction/viewer shutdown;
- selected instance removed;
- `removeAll()`;
- selection becomes null;
- failure to resolve the selected instance;
- loss of agent region without a replacement;
- switching to an unrelated tool/mode deliberately.

Do not teardown merely because `ALToolGhostEdit::handleDeselect()` fires during the intentional transition to `gBasicToolset`.

Region change is not teardown of edit mode: destroy the old proxy, create a new one in `gAgent.getRegion()`, push from authoritative `mFootGlobal`, reselect it, and keep the transaction closed until a new drag begins.

## 9. ALToolGhostEdit wiring

Add:

```cpp
#include "alghostmanipproxy.h"

class ALToolGhostEdit final ...
{
public:
    ALGhostManipProxy& getManipProxy() { return mManipProxy; }
    bool isEditModeActive() const { return mEditModeActive; }
    void stopEditMode();

private:
    ALGhostManipProxy mManipProxy;
    bool mEditModeActive = false;
};
```

`handleSelect()`:

```cpp
void ALToolGhostEdit::handleSelect()
{
    mEditModeActive = true;
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);

    const LLUUID selected = ALGhostStudio::instance().getSelected();
    if (selected.notNull())
    {
        mManipProxy.begin(selected);
    }
}
```

Ghost click in `handleMouseDown()`, replacing the old custom drag start at [altoolghostedit.cpp:91](I:/alchemy-machinima/indra/newview/altoolghostedit.cpp:91):

```cpp
if (hit.notNull())
{
    studio.setSelected(hit);
    mManipProxy.selectInstance(hit);
    return true;
}
```

Remove the old `mDragInstance`, Shift-yaw capture, and hover-drag implementation after the proxy path is enabled. Translate/rotate should have one writer, the stock manipulator.

`handleDeselect()`:

```cpp
void ALToolGhostEdit::handleDeselect()
{
    // setCurrentToolset(gBasicToolset) necessarily deselects this transient
    // tool. The proxy owns the continuing edit-mode lifetime.
    if (!mManipProxy.isActive())
    {
        mEditModeActive = false;
    }
}
```

Explicit stop:

```cpp
void ALToolGhostEdit::stopEditMode()
{
    mEditModeActive = false;
    mManipProxy.teardown();
}
```

Escape and the panel toggle-off call `stopEditMode()` before clearing the transient/current mode.

When the panel list selection changes while edit mode is active, call:

```cpp
ALToolGhostEdit::getInstance()->getManipProxy().selectInstance(id);
```

## 10. CMake registration

Add alphabetically beside the existing Ghost Studio entries:

```cmake
# viewer_SOURCE_FILES, currently alghoststudio.cpp at CMakeLists.txt:273
alghostmanipproxy.cpp
alghoststudio.cpp
altoolghostedit.cpp
```

and:

```cmake
# viewer_HEADER_FILES, currently alghoststudio.h at CMakeLists.txt:1084
alghostmanipproxy.h
alghoststudio.h
altoolghostedit.h
```

`altoolghostedit.cpp` is already compiled in the current working tree; place the new proxy file immediately before it/alghoststudio in each list.

## 11. Most likely breakage

The most likely failure is destroying the proxy from `ALToolGhostEdit::handleDeselect()` during the intentional stock-tool handoff.

The exact chain is:

```text
selectProxy()
  -> setCurrentToolset(gBasicToolset)
  -> LLToolMgr::setCurrentTool()
  -> clears mTransientTool
  -> ALToolGhostEdit loses selection
  -> handleDeselect()
  -> accidental teardown/markDead
  -> stock manipulator retains an LLSelectNode/drawable reference to a dead proxy
```

The relevant transient-clearing code is [lltoolmgr.cpp:186](I:/alchemy-machinima/indra/newview/lltoolmgr.cpp:186), and toolset switching deselects the current tool at [lltoolmgr.cpp:159](I:/alchemy-machinima/indra/newview/lltoolmgr.cpp:159).

Prevent it by:

- making edit mode an explicit state independent of “current tool equals `ALToolGhostEdit`”;
- not tearing down in `handleDeselect()` during the stock-tool transition;
- calling `deselectAll()` before `markDead()`;
- clearing `mProxy`/registry before `markDead()`;
- holding a local `LLPointer` through `markDead()`;
- cancelling capture and the open undo transaction before destruction.

Codex session ID: 019f8a6a-d6c8-73f1-87cc-f232dd738ea2
Resume in Codex: codex resume 019f8a6a-d6c8-73f1-87cc-f232dd738ea2
