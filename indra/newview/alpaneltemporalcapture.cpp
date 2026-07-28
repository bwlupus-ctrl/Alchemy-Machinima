/**
 * @file alpaneltemporalcapture.cpp
 * @brief Temporal Capture transport panel -- see alpaneltemporalcapture.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpaneltemporalcapture.h"

#include "llbutton.h"
#include "lltextbox.h"
#include "lluictrl.h"
#include "llstring.h"               // llformat
#include "llviewercontrol.h"        // gSavedSettings
#include "llpresentationtime.h"

// Both the standalone Temporal Capture floater and the Director Console Temporal
// tab instantiate this via <panel class="panel_temporal_capture" .../>. The
// injector string MUST match the class= string in the XML or the panel silently
// falls back to a plain LLPanel and none of the wiring below runs.
static LLPanelInjector<ALPanelTemporalCapture> t_panel_temporal_capture("panel_temporal_capture");

bool ALPanelTemporalCapture::postBuild()
{
    mEffectiveText = getChild<LLTextBox>("effective_text");
    mPostSpeedText = getChild<LLTextBox>("postspeed_text");
    mStatusText    = getChild<LLTextBox>("status_text");

    getChild<LLButton>("btn_freeze")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickScalePreset(0.f, true); });
    getChild<LLButton>("btn_quarter")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickScalePreset(0.25f, true); });
    getChild<LLButton>("btn_half")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickScalePreset(0.5f, true); });
    getChild<LLButton>("btn_realtime")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickScalePreset(1.f, false); });
    getChild<LLButton>("btn_double")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickScalePreset(2.f, true); });

    // The slider/spinner are settings-backed (they write TemporalWorldScale on
    // their own); this extra commit just flips Live -> Manual so dragging the
    // speed actually does something without a separate mode click.
    if (LLUICtrl* s = findChild<LLUICtrl>("scale_slider"))
    {
        s->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScaleCommit(); });
    }
    if (LLUICtrl* sp = findChild<LLUICtrl>("scale_spinner"))
    {
        sp->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScaleCommit(); });
    }

    return true;
}

void ALPanelTemporalCapture::onClickScalePreset(F32 scale, bool manual)
{
    gSavedSettings.setF32("TemporalWorldScale", scale);
    // Realtime (manual = false) returns to Live; every other preset engages Manual.
    gSavedSettings.setS32("TemporalMode", manual ? 1 : 0);
}

void ALPanelTemporalCapture::onScaleCommit()
{
    if (gSavedSettings.getS32("TemporalMode") == 0 &&
        gSavedSettings.getF32("TemporalWorldScale") != 1.f)
    {
        gSavedSettings.setS32("TemporalMode", 1);
    }
}

void ALPanelTemporalCapture::draw()
{
    // Read the live frozen frame so the readouts show the ACTUAL effective scale
    // (which equals the requested scale in Landing 1, but is sourced from the
    // service so it stays correct if adaptive scaling lands later).
    const LLTemporalFrameContext& tf = LLPresentationTime::currentFrame();
    const F32 eff = tf.effective_scale;

    std::string eff_str = tf.active()
        ? llformat("Effective speed: %.2fx%s", eff, tf.paused ? "  (frozen)" : "")
        : std::string("Effective speed: 1.00x (live)");
    if (eff_str != mLastEffective)
    {
        mEffectiveText->setText(eff_str);
        mLastEffective = eff_str;
    }

    std::string post_str;
    if (tf.active() && eff > 0.f)
    {
        post_str = llformat("Recommended post speed: %.0f%%", 100.f / eff);
    }
    else if (tf.active())   // paused / 0x
    {
        post_str = "Recommended post speed: --";
    }
    else
    {
        post_str = "Recommended post speed: 100%";
    }
    if (post_str != mLastPost)
    {
        mPostSpeedText->setText(post_str);
        mLastPost = post_str;
    }

    std::string status = tf.paused ? "Paused (0x)"
                                   : (tf.active() ? "Manual scale (grid time is real)"
                                                  : "Live");
    if (status != mLastStatus)
    {
        mStatusText->setText(status);
        mLastStatus = status;
    }

    LLPanel::draw();
}
