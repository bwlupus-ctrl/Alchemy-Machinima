/**
 * @file alpanelanimpreview.cpp
 * @brief Shared animation-preview panel -- see alpanelanimpreview.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelanimpreview.h"

#include "indra_constants.h"        // MASK_ALT
#include "alassetblocklist.h"       // Blacklist (own-avatar asset blocklist)
#include "llagent.h"                // gAgent (stop / revoke requests)
#include "llanimationstates.h"      // ANIM_AGENT_STAND, ANIM_REQUEST_STOP
#include "llbutton.h"
#include "llfloater.h"              // gFloaterView (raise host on preview click)
#include "llfocusmgr.h"             // gFocusMgr (preview mouse capture)
#include "llkeyframemotion.h"       // force the preview loop
#include "llrender.h"               // gGL (preview blit)
#include "llscriptruntimeperms.h"   // SCRIPT_PERMISSIONS (revoke)
#include "lltoolmgr.h"              // MASK_ORBIT / MASK_PAN (preview hover)
#include "llui.h"                   // LLUI::setMousePositionLocal
#include "llview.h"
#include "llviewerobjectlist.h"     // gObjectList
#include "llviewerregion.h"         // region name (blacklist)
#include "llviewerwindow.h"         // gViewerWindow (preview cursor)
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

// Both the standalone Animation Explorer floater and the Director Console
// Animate tab instantiate this class via <panel class="panel_anim_preview" ...>.
// The injector string MUST match the class= string in the XML or the panel
// silently falls back to a plain LLPanel and none of the wiring below runs.
static LLPanelInjector<ALPanelAnimPreview> t_panel_anim_preview("panel_anim_preview");

ALPanelAnimPreview::~ALPanelAnimPreview()
{
    // release the preview dummy texture (AnimationExplorer idiom)
    mAnimationPreview = nullptr;
}

//static
void ALPanelAnimPreview::setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

bool ALPanelAnimPreview::postBuild()
{
    mPreviewCtrl = findChild<LLView>("animation_preview");
    if (!mPreviewCtrl)
    {
        LL_WARNS("AnimPreview") << "Could not find animation preview placement rect" << LL_ENDL;
        return false;
    }

    mStopBtn = getChild<LLButton>("btn_anim_ex_stop");
    mRevokeBtn = getChild<LLButton>("btn_anim_ex_revoke");
    mBlacklistBtn = getChild<LLButton>("btn_anim_ex_blacklist");
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStop(); });
    mRevokeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStopAndRevoke(); });
    mBlacklistBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBlacklist(); });

    // the dummy avatar is created lazily in draw() once the agent avatar is
    // valid, so the panel can build before login completes.
    return true;
}

void ALPanelAnimPreview::previewAnim(const LLUUID& anim_id, const LLUUID& source_object)
{
    // just record the request; draw() diffs against mLoopedAnimId and re-arms
    // the looping dummy only when the anim actually changed
    mAnimId = anim_id;
    mSourceObject = source_object;
}

void ALPanelAnimPreview::draw()
{
    // create the dummy-avatar preview lazily, once the agent avatar is ready
    if (!mAnimationPreview && mPreviewCtrl && isAgentAvatarValid())
    {
        mAnimationPreview = new LLPreviewAnimation(mPreviewCtrl->getRect().getWidth(),
                                                   mPreviewCtrl->getRect().getHeight());
        mAnimationPreview->setZoom(2.0f);
    }

    // auto-play the fed animation on the dummy, on loop; re-arm when it changes
    if (mAnimationPreview)
    {
        if (LLVOAvatar* dummy = mAnimationPreview->getDummyAvatar())
        {
            if (mAnimId != mLoopedAnimId)
            {
                dummy->deactivateAllMotions();
                dummy->startMotion(ANIM_AGENT_STAND, 0.0f);
                mLoopedAnimId = mAnimId;
                if (mAnimId.notNull())
                {
                    dummy->startMotion(mAnimId, 0.0f);
                }
            }
            else if (mAnimId.notNull())
            {
                // keep it looping: force the loop flag once the asset resolves,
                // and restart if a non-looping motion has run to the end
                if (auto* motion = dynamic_cast<LLKeyframeMotion*>(dummy->findMotion(mAnimId)))
                {
                    if (!motion->getLoop())
                    {
                        motion->setLoop(true);
                        motion->setLoopOut(motion->getDuration());
                    }
                }
                if (!dummy->isMotionActive(mAnimId))
                {
                    dummy->startMotion(mAnimId, 0.0f);
                }
            }
        }
    }

    refreshActionButtons();

    LLPanel::draw();

    // blit the spinning preview dummy into its pane, in this panel's local
    // space (the preview child rect is already panel-local). Drawn after the
    // panel's children so it sits over the view_border frame.
    if (mAnimationPreview && mPreviewCtrl)
    {
        const LLRect r = mPreviewCtrl->getRect();
        mAnimationPreview->requestUpdate();
        gGL.color3f(1.0f, 1.0f, 1.0f);
        gGL.getTexUnit(0)->bind(mAnimationPreview);
        gGL.begin(LLRender::TRIANGLES);
        {
            gGL.texCoord2f(0.0f, 1.0f);
            gGL.vertex2i(r.mLeft, r.mTop);
            gGL.texCoord2f(0.0f, 0.0f);
            gGL.vertex2i(r.mLeft, r.mBottom);
            gGL.texCoord2f(1.0f, 0.0f);
            gGL.vertex2i(r.mRight, r.mBottom);

            gGL.texCoord2f(0.0f, 1.0f);
            gGL.vertex2i(r.mLeft, r.mTop);
            gGL.texCoord2f(1.0f, 0.0f);
            gGL.vertex2i(r.mRight, r.mBottom);
            gGL.texCoord2f(1.0f, 1.0f);
            gGL.vertex2i(r.mRight, r.mTop);
        }
        gGL.end();
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    }
}

// ---------------------------------------------------------------------------
// preview mouse handling: drag to orbit / pan, wheel to zoom. Gated to the
// preview child rect (panel-local coords, so it works the same embedded deep in
// the console's tab container as it does in the standalone floater).
// ---------------------------------------------------------------------------
bool ALPanelAnimPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (mAnimationPreview && mPreviewCtrl && mPreviewCtrl->getRect().pointInRect(x, y))
    {
        // raise the host floater the way a floater-level handler used to, then
        // take the mouse for the drag
        if (LLFloater* host = gFloaterView->getParentFloater(this))
        {
            host->setFrontmost(false);  // raise without stealing keyboard focus
        }
        gFocusMgr.setMouseCapture(this);
        gViewerWindow->hideCursor();
        mLastMouseX = x;
        mLastMouseY = y;
        return true;
    }
    return LLPanel::handleMouseDown(x, y, mask);
}

bool ALPanelAnimPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        gFocusMgr.setMouseCapture(nullptr);
        gViewerWindow->showCursor();
    }
    return LLPanel::handleMouseUp(x, y, mask);
}

bool ALPanelAnimPreview::handleHover(S32 x, S32 y, MASK mask)
{
    if (!mAnimationPreview || !mPreviewCtrl || !mPreviewCtrl->getRect().pointInRect(x, y))
    {
        return LLPanel::handleHover(x, y, mask);
    }

    MASK local_mask = mask & ~MASK_ALT;
    if (hasMouseCapture())
    {
        if (local_mask == MASK_PAN)
        {
            mAnimationPreview->pan((F32)(x - mLastMouseX) * -0.005f,
                                   (F32)(y - mLastMouseY) * -0.005f);
        }
        else if (local_mask == MASK_ORBIT)
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;
            mAnimationPreview->rotate(yaw_radians, pitch_radians);
        }
        else
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;
            mAnimationPreview->rotate(yaw_radians, 0.f);
            mAnimationPreview->zoom(zoom_amt);
        }
        mAnimationPreview->requestUpdate();
        LLUI::getInstance()->setMousePositionLocal(this, mLastMouseX, mLastMouseY);
    }
    else if (local_mask == MASK_ORBIT)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
    }
    else if (local_mask == MASK_PAN)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
    }
    else
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
    }
    return true;
}

bool ALPanelAnimPreview::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (mAnimationPreview && mPreviewCtrl && mPreviewCtrl->getRect().pointInRect(x, y))
    {
        mAnimationPreview->zoom((F32)delta.mPrecise * -0.2f);
        mAnimationPreview->requestUpdate();
        return true;
    }
    return LLPanel::handleScrollWheel(x, y, delta);
}

void ALPanelAnimPreview::onMouseCaptureLost()
{
    gViewerWindow->showCursor();
}

// ---------------------------------------------------------------------------
// own-avatar action controls (Stop / Stop-and-Revoke / Blacklist). Each acts on
// the fed anim id + source object; the anim always stops on YOUR avatar, and the
// revoke / blacklist target is whatever object the host said is playing it.
// ---------------------------------------------------------------------------
void ALPanelAnimPreview::refreshActionButtons()
{
    const std::string no_focus_tip("Select an animation row or paste a UUID first");
    const std::string no_source_tip(
        "This animation isn't playing on your avatar, so there's no source to act on");

    if (mStopBtn)
    {
        mStopBtn->setEnabled(mAnimId.notNull());
        setToolTipIfChanged(mStopBtn, mAnimId.isNull() ? no_focus_tip
            : std::string("Stop this animation on your own avatar and tell the region"));
    }
    if (mRevokeBtn)
    {
        mRevokeBtn->setEnabled(mAnimId.notNull() && mSourceObject.notNull());
        setToolTipIfChanged(mRevokeBtn, mAnimId.isNull() ? no_focus_tip
            : mSourceObject.isNull() ? no_source_tip
            : std::string("Stop it and revoke the source object's animation permissions"));
    }
    if (mBlacklistBtn)
    {
        mBlacklistBtn->setEnabled(mAnimId.notNull() && mSourceObject.notNull());
        setToolTipIfChanged(mBlacklistBtn, mAnimId.isNull() ? no_focus_tip
            : mSourceObject.isNull() ? no_source_tip
            : std::string("Stop it and add the animation to the asset blacklist"));
    }
}

void ALPanelAnimPreview::onStop()
{
    if (mAnimId.isNull() || !isAgentAvatarValid())
    {
        return;
    }
    gAgentAvatarp->stopMotion(mAnimId);
    gAgent.sendAnimationRequest(mAnimId, ANIM_REQUEST_STOP);
}

void ALPanelAnimPreview::onStopAndRevoke()
{
    onStop();
    if (mSourceObject.isNull())
    {
        return;
    }
    if (LLViewerObject* vo = gObjectList.findObject(mSourceObject))
    {
        // the two permission bits revoke_permissions_on_object() uses
        U32 permissions = SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_TRIGGER_ANIMATION].permbit
                         | SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_OVERRIDE_ANIMATIONS].permbit;
        gAgent.sendRevokePermissions(vo->getID(), permissions);
    }
}

void ALPanelAnimPreview::onBlacklist()
{
    if (mAnimId.isNull() || mSourceObject.isNull())
    {
        return;
    }
    onStop();
    std::string region_name;
    if (gAgent.getRegion())
    {
        region_name = gAgent.getRegion()->getName();
    }
    // ALAssetBlocklist::addEntry() records the source UUID that played the anim
    ALAssetBlocklist::instance().addEntry(mAnimId, mSourceObject, region_name, LLAssetType::AT_ANIMATION);
}
