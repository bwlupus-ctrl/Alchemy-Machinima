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
#include "alfloatertemporalcapture.h"
#include "alscrollfit.h"

#include "alkitrow.h"
#include "llbutton.h"
#include "llscrollcontainer.h"
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

    if (LLButton* button = findChild<LLButton>("btn_world_wind_down"))
    {
        button->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            LLPresentationTime::instance().toggleWorldWindDown();
        });
    }
    mWindDownBezier[0] = findChild<LLView>("wind_down_bezier_x1");
    mWindDownBezier[1] = findChild<LLView>("wind_down_bezier_y1");
    mWindDownBezier[2] = findChild<LLView>("wind_down_bezier_x2");
    mWindDownBezier[3] = findChild<LLView>("wind_down_bezier_y2");
    mWindDownCard = findChild<LLView>("wind_down_card"); // legacy only; v2 lays out kit rows
    if (mWindDownCard)
    {
        const S32 card_height = mWindDownCard->getRect().getHeight();
        if (LLUICtrl* curve = findChild<LLUICtrl>("wind_down_curve"))
            mWindDownCompactHeight = card_height - curve->getRect().mBottom + 10;
        for (LLView* field : mWindDownBezier)
        {
            if (field)
                mWindDownCustomHeight = llmax(mWindDownCustomHeight,
                    card_height - field->getRect().mBottom + 10);
        }
    }
    if (LLControlVariable* curve = gSavedSettings.getControl("WorldWindDownCurve"))
    {
        // A setting notification preserves every widget's existing commit
        // callbacks, and updates all hosts without resizing from draw().
        mWindDownCurveConnection = curve->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { syncWindDownControls(); });
    }
    syncWindDownControls();

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

    // [FrameLimiter] show_if fps_limit_on: the slider row (created hidden by the
    // generator) appears while the cap is enabled. The toggle commit makes it
    // instant; the 4 Hz draw sync covers changes made elsewhere (Preferences).
    mFpsLimitSlider = findChild<LLView>("fps_limit_slider", true);
    if (LLUICtrl* t = findChild<LLUICtrl>("fps_limit_check"))
    {
        t->setCommitCallback([this](LLUICtrl*, const LLSD&) { syncFpsLimitRow(); });
    }
    syncFpsLimitRow();

    return true;
}

// ui-draw-lint: allow-function(raw-write) change-guarded; runs on a toggle click or the 4 Hz readout poll only
void ALPanelTemporalCapture::syncFpsLimitRow()
{
    if (!mFpsLimitSlider)
    {
        return;
    }
    static LLCachedControl<bool> limit_enabled(gSavedSettings, "FramerateLimitEnabled", false);
    // Through the kit row so the card relayouts and leaves no gap (a raw setVisible on the
    // row is overridden by the card layout).
    ALKitRow::setControlVisible(mFpsLimitSlider, limit_enabled);
}

void ALPanelTemporalCapture::onClickScalePreset(F32 scale, bool manual)
{
    LLPresentationTime::instance().cancelWorldWindDown();
    gSavedSettings.setF32("TemporalWorldScale", scale);
    // Realtime (manual = false) returns to Live; every other preset engages Manual.
    gSavedSettings.setS32("TemporalMode", manual ? 1 : 0);
}

void ALPanelTemporalCapture::onScaleCommit()
{
    LLPresentationTime::instance().cancelWorldWindDown();
    if (gSavedSettings.getS32("TemporalMode") == 0 &&
        gSavedSettings.getF32("TemporalWorldScale") != 1.f)
    {
        gSavedSettings.setS32("TemporalMode", 1);
    }
}

void ALPanelTemporalCapture::syncWindDownControls()
{
    const S32 curve = gSavedSettings.getS32("WorldWindDownCurve");
    if (mLastWindDownCurve == curve)
    {
        return;
    }
    mLastWindDownCurve = curve;
    for (LLView* field : mWindDownBezier)
    {
        if (field)
        {
            // ALKitRow's non-kit fallback is diff-guarded setVisible. Keep
            // that same explicit contract for the legacy card's controls.
            if (mWindDownCard)
                setVisibleIfChanged(field, curve == ALWorldWindDown::CUSTOM);
            else
                ALKitRow::setControlVisible(field, curve == ALWorldWindDown::CUSTOM);
        }
    }
    if (mWindDownCard && mWindDownCompactHeight > 0 && mWindDownCustomHeight > 0)
    {
        const S32 height = curve == ALWorldWindDown::CUSTOM
            ? mWindDownCustomHeight : mWindDownCompactHeight;
        const S32 delta = height - mWindDownCard->getRect().getHeight();
        if (delta != 0)
        {
            const S32 card_top = mWindDownCard->getRect().mTop;
            mWindDownCard->reshape(mWindDownCard->getRect().getWidth(), height, true);
            mWindDownCard->translate(0, card_top - mWindDownCard->getRect().mTop);
            ALScrollFit::fitPanelToChildren(this, 10);
            if (auto* floater = dynamic_cast<ALFloaterTemporalCapture*>(getParent()))
                floater->fitToContent(this);
            // The unchanged v1 Director wraps this shared panel in its tab
            // scroller. Refit that document when Custom adds/removes rows.
            for (LLView* view = getParent(); view; view = view->getParent())
            {
                if (auto* scroller = dynamic_cast<LLScrollContainer*>(view))
                {
                    ALScrollFit::fitContainer(scroller);
                    break;
                }
            }
        }
    }
}

void ALPanelTemporalCapture::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [DirFix] D9
    if (isWidgetOnScreen(this) && mReadoutClock.due())
    {
        syncFpsLimitRow();
        // Read the live frozen frame so the readouts show the ACTUAL effective scale.
        const LLTemporalFrameContext& tf = LLPresentationTime::currentFrame();
        const F32 eff = tf.effective_scale;

        std::string eff_str = tf.active()
            ? llformat("Effective speed: %.2fx%s", eff, tf.paused ? "  (frozen)" : "")
            : std::string("Effective speed: 1.00x (live)");
        if (eff_str != mLastEffective)
        {
            setTextIfChanged(mEffectiveText, eff_str);
            mLastEffective = eff_str;
        }

        std::string post_str;
        if (tf.active() && eff > 0.f)
            post_str = llformat("Recommended post speed: %.0f%%", 100.f / eff);
        else if (tf.active())
            post_str = "Recommended post speed: --";
        else
            post_str = "Recommended post speed: 100%";
        if (post_str != mLastPost)
        {
            setTextIfChanged(mPostSpeedText, post_str);
            mLastPost = post_str;
        }

        std::string status = tf.paused ? "Paused (0x)"
                                       : (tf.active() ? "Manual scale (grid time is real)"
                                                      : "Live");
        switch (LLPresentationTime::instance().worldWindDownState())
        {
            case ALWorldWindDown::State::WINDING_DOWN: status = "Winding down\xe2\x80\xa6"; break;
            case ALWorldWindDown::State::FROZEN: status = "Frozen (wind-down)"; break;
            case ALWorldWindDown::State::SPINNING_UP: status = "Spinning up\xe2\x80\xa6"; break;
            default: break;
        }
        if (LLPresentationTime::instance().worldAlreadyFrozen())
            status = "Already frozen";
        if (!LLPresentationTime::instance().hasWorldClockDrives())
            status = "Nothing driven (all drives off)";
        if (status != mLastStatus)
        {
            setTextIfChanged(mStatusText, status);
            mLastStatus = status;
        }
    }

    LLPanel::draw();
}
