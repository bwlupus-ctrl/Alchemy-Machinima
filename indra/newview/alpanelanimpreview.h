/**
 * @file alpanelanimpreview.h
 * @brief Shared animation-preview panel: a spinning LLPreviewAnimation dummy
 *        (drag to orbit / pan, wheel to zoom) that auto-plays a fed animation
 *        UUID on loop, plus the own-avatar action controls (Capture-all,
 *        Stop, Stop and Revoke, Blacklist).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Extracted from BOTH the standalone Animation Explorer floater AND the
 * Director Console's Animate tab, which each hand-copied the preview pane, the
 * floater-level mouse-drag-to-rotate handlers, and the auto-loop-on-select
 * logic (the real drift risk). Now that duplicated view + mouse handling lives
 * in ONE place: an LLPanel that owns its preview child, handles the mouse
 * itself (gated to the preview child rect in panel-local coords), and blits +
 * loops its own dummy in draw(). Both hosts embed the same panel
 * (panel_anim_preview.xml, class "panel_anim_preview") and feed it the anim to
 * show via previewAnim(); each embed gets its OWN LLPreviewAnimation dummy, so
 * two live instances (Explorer floater + console Animate tab) never fight over
 * one dummy.
 *
 * Host-specific by design (NOT here): the Explorer's recent-played list and the
 * console's cast-scoped signaled list / set-loco / play-local / paste row --
 * different data, one copy each, so they cannot drift. The "source object" a
 * Stop-and-Revoke / Blacklist acts on is also host-specific (the Explorer's
 * played-by column vs. the console's own-avatar animation source), so the host
 * feeds it alongside the anim id; the panel never derives it.
 */

#ifndef AL_ALPANELANIMPREVIEW_H
#define AL_ALPANELANIMPREVIEW_H

#include "llpanel.h"
#include "llfloaterbvhpreview.h"     // LLPreviewAnimation (the spinning dummy)
#include "llscrolldelta.h"           // LLScrollDelta (handleScrollWheel signature)

class LLButton;
class LLView;

class ALPanelAnimPreview final : public LLPanel
{
public:
    ALPanelAnimPreview() = default;
    ~ALPanelAnimPreview() override;

    bool postBuild() override;
    void draw() override;

    // Feed the panel the animation to preview + the in-world object the own-
    // avatar actions (Stop and Revoke / Blacklist) should target. The host
    // calls this when its list selection / pasted UUID changes; the panel
    // diffs internally and re-arms the looping dummy only on a change.
    //  - anim_id null   -> nothing previews (clearPreview)
    //  - source_object  -> revoke/blacklist target; null = those stay disabled
    void previewAnim(const LLUUID& anim_id, const LLUUID& source_object);
    void clearPreview() { previewAnim(LLUUID::null, LLUUID::null); }

    // The preview owns the mouse while dragging within its preview child rect
    // (LLFloaterBvhPreview / AnimationExplorer idiom, moved off the host
    // floater onto this panel so it lives in exactly one place).
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, LLScrollDelta delta) override;
    void onMouseCaptureLost() override;

private:
    // own-avatar action controls (shared with what the Explorer wired to its
    // Stop / Stop-and-Revoke / Blacklist buttons)
    void onStop();
    void onStopAndRevoke();
    void onBlacklist();
    void refreshActionButtons();

    // set a tooltip only when it changed (draw()-rate friendly)
    static void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip);

    LLView*                       mPreviewCtrl = nullptr;   // preview placement rect
    LLPointer<LLPreviewAnimation> mAnimationPreview;        // this panel's own dummy

    LLUUID mAnimId;          // anim the host wants previewed / acted on
    LLUUID mSourceObject;    // object playing it (revoke / blacklist target)
    LLUUID mLoopedAnimId;    // anim currently armed on the dummy (change-diff)

    S32 mLastMouseX = 0;
    S32 mLastMouseY = 0;

    LLButton* mStopBtn = nullptr;
    LLButton* mRevokeBtn = nullptr;
    LLButton* mBlacklistBtn = nullptr;
};

#endif // AL_ALPANELANIMPREVIEW_H
