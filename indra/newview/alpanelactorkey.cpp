/** Director Capture > Key: settings-bound kit page and capture feedback. */
#include "llviewerprecompiledheaders.h"
#include "alpanelactorkey.h"
#include "alactorkeyui.h"
#include "llactormover.h"
#include "alactorchromakey.h"
#include "alghoststudio.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "lluicolortable.h"
#include "llbutton.h"
#include "llcharacter.h"
#include "llfontgl.h"
#include "llglslshader.h"
#include "llviewershadermgr.h"
#include "llnotificationsutil.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvisualeffect.h"
#include "llvoavatar.h"
#include "pipeline.h"

static LLPanelInjector<ALPanelActorKey> t_panel_actor_key("panel_actor_key");

namespace
{
std::string sLastResult = "No self-test run this session.";

const char* modeName(U32 mode)
{
    return mode == 1 ? "Set only" : mode == 2 ? "Alpha matte" : "Actors on key";
}

std::string codes(const LLColor3& colour, F32 scale = 1.f)
{
    return llformat("%d,%d,%d", ll_round(llclamp(colour.mV[0] * scale, 0.f, 1.f) * 255.f),
        ll_round(llclamp(colour.mV[1] * scale, 0.f, 1.f) * 255.f),
        ll_round(llclamp(colour.mV[2] * scale, 0.f, 1.f) * 255.f));
}

std::string captureWarnings()
{
    static LLCachedControl<bool> guides(gSavedSettings, "RenderCompositionGuide", false);
    static LLCachedControl<bool> hidden_guides(gSavedSettings, "RenderCompositionGuideWithUIHidden", true);
    static LLCachedControl<bool> viewfinder(gSavedSettings, "CinematicViewfinderEnabled", false);
    static LLCachedControl<S32> area(gSavedSettings, "RenderAreaShadowDebugView", 0);
    static LLCachedControl<S32> world_area(gSavedSettings, "RenderAreaShadowWorldDebug", 0);
    static LLCachedControl<bool> hud(gSavedSettings, "ActorKeyHideHUD", true);
    static LLCachedControl<bool> chart(gSavedSettings, "AlchemyColorChartShow", false);
    static LLCachedControl<U32> blink(gSavedSettings, "POVBlinkStyle", 0);
    static LLCachedControl<bool> time(gSavedSettings, "DebugShowTime", false);
    static LLCachedControl<bool> memory(gSavedSettings, "DebugShowMemory", false);
    static LLCachedControl<bool> render(gSavedSettings, "DebugShowRenderInfo", false);
    static LLCachedControl<bool> avatars(gSavedSettings, "DebugShowAvatarRenderInfo", false);
    static LLCachedControl<bool> matrices(gSavedSettings, "DebugShowRenderMatrices", false);
    static LLCachedControl<bool> colour(gSavedSettings, "DebugShowColor", false);
    static LLCachedControl<bool> texture(gSavedSettings, "DebugShowTextureInfo", false);
    std::string warnings;
    const auto add = [&warnings](bool active, const char* text)
    {
        if (active) { if (!warnings.empty()) warnings += "; "; warnings += text; }
    };
    add(guides && hidden_guides, "Composition guides with UI hidden");
    add(viewfinder, "Viewfinder (survives both UI hide mechanisms)");
    add(gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES), "Physics shapes");
    add(area != 0 || world_area != 0, "Area-shadow debug view");
    add(time || memory || render || avatars || matrices || colour || texture, "Debug text (check hidden-UI capture)");
    add(LLVfxManager::instance().hasEffect(EVisualEffect::RlvOverlay), "RLVa overlay");
    add(!hud, "HUD attachments opted in");
    add(chart && !ALActorChromaKey::alphaMatte(), "Colour chart");
    add(blink == 1 && !ALActorChromaKey::alphaMatte(), "Warm blink can tint the key while blinking");
    return warnings.empty() ? "No capture overlays enabled." : warnings;
}
}

void ALActorKeyUI::recordSelfTest(const std::string& result)
{
    sLastResult = result;
}

void ALActorKeyUI::shadowTestUnavailable()
{
    if (!gSavedSettings.getU32("ActorKeyShadowSelfTest")) return;
    gSavedSettings.setU32("ActorKeyShadowSelfTest", 0);
    const std::string result = "ACTORKEY SHADOW-EXACT INCONCLUSIVE: Exact isolation/current shadow frame required; " + shadowStatus();
    LL_INFOS("ActorKey") << result << LL_ENDL;
    recordSelfTest(result);
}

void ALActorKeyUI::initialize()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    // A nested colour write must not turn a preset selection into Custom.
    static bool applying_preset = false;
    const auto apply = []()
    {
        static const U8 presets[8][3] = {{0,177,64}, {0,255,0}, {0,71,187}, {0,0,255},
            {255,0,255}, {0,0,0}, {255,255,255}, {128,128,128}};
        const U32 preset = gSavedSettings.getU32("ActorKeyColorPreset");
        if (preset >= 8) return;
        applying_preset = true;
        gSavedSettings.set("ActorKeyColor", LLColor3(presets[preset][0] / 255.f,
            presets[preset][1] / 255.f, presets[preset][2] / 255.f).getValue());
        applying_preset = false;
    };
    gSavedSettings.getControl("ActorKeyColorPreset")->getSignal()->connect(
        [apply](LLControlVariable*, const LLSD&, const LLSD&) { apply(); });
    gSavedSettings.getControl("ActorKeyColor")->getSignal()->connect(
        [](LLControlVariable*, const LLSD&, const LLSD&)
        {
            if (!applying_preset) gSavedSettings.setU32("ActorKeyColorPreset", 8);
        });
    apply();
    gSavedSettings.getControl("ActorKeyEnabled")->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            if (!value.asBoolean()) return;
            LLSD args;
            args["MODE"] = modeName(ALActorChromaKey::mode());
            args["COLOUR"] = codes(ALActorChromaKey::colour());
            LLNotificationsUtil::add("ActorKeyEnabled", args);
        });
}

bool ALPanelActorKey::postBuild()
{
    mPage = getChild<ALKitPage>("key_kit");
    mColour = getChild<LLUICtrl>("ak_color");
    mShadow = getChild<LLUICtrl>("ak_shadow");
    mShadowCodes = getChild<LLTextBox>("ak_shadow_codes");
    const char* conditional[] = {"ak_invert", "ak_glass", "ak_shadow", "ak_iso", "ak_sstr", "ak_scasc", "ak_ssoft", "ak_sstatus", "ak_mshadow"};
    for (size_t i = 0; i < mConditional.size(); ++i) mConditional[i] = mPage->findRow(conditional[i]);
    const char* readouts[] = {"ak_status", "ak_codes", "ak_count", "ak_sstatus", "ak_warn", "ak_result", "ak_limits"};
    for (size_t i = 0; i < mReadouts.size(); ++i) mReadouts[i] = getChild<LLTextBox>(readouts[i]);
    getChild<LLButton>("ak_selftest")->setCommitCallback([](LLUICtrl*, const LLSD&)
        { gSavedSettings.setU32("ActorKeySelfTest", 1); });
    getChild<LLButton>("ak_shadowtest")->setCommitCallback([](LLUICtrl*, const LLSD&)
        { gSavedSettings.setU32("ActorKeyShadowSelfTest", 1); });
    for (const char* name : {"ActorKeyMode", "ActorKeyIncludeClones", "ActorKeyColorPreset", "RenderShadowDetail"})
        mConnections.emplace_back(gSavedSettings.getControl(name)->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { updateConditions(); }));
    mPage->setRowRevealGuard([this](ALKitRow* row)
    {
        for (size_t i = 0; i < mConditional.size(); ++i)
            if (row == mConditional[i]) return mConditionVisible[i];
        return true;
    });
    updateConditions();
    refreshReadouts();
    return LLPanel::postBuild();
}

void ALPanelActorKey::updateConditions()
{
    const U32 mode = gSavedSettings.getU32("ActorKeyMode");
    const bool clones = gSavedSettings.getBOOL("ActorKeyIncludeClones");
    for (size_t i = 0; i < mConditional.size(); ++i)
    {
        const bool visible = i == 0 || i == 8 ? mode == 2 : i == 1 ? mode == 0 && clones : mode == 0;
        mConditionVisible[i] = visible;
        if (mConditional[i]) mConditional[i]->setControlVisible(visible);
    }
    setEnabledIfChanged(mColour, gSavedSettings.getU32("ActorKeyColorPreset") == 8);
    setEnabledIfChanged(mShadow, gSavedSettings.getS32("RenderShadowDetail") != 0);
    setToolTipIfChanged(mShadow, gSavedSettings.getS32("RenderShadowDetail") == 0
        ? "Enable sun/moon shadows in graphics to keep actor shadows." : "Actor sun/moon shadows darken the key.");
}

void ALPanelActorKey::refreshReadouts()
{
    std::string status = "Off (off at every login)";
    if (ALActorChromaKey::sActive)
        status = ALActorChromaKey::keyedThisFrame()
            ? std::string("Keying: ") + modeName(ALActorChromaKey::mode())
            : ALActorChromaKey::reason().empty() ? "Arming" : "Suspended: " + ALActorChromaKey::reason();
    const std::string warnings = captureWarnings();
    static const LLUIColor warning = LLUIColorTable::instance().getColor("KitWarnBandText");
    const LLUIColor& tint = ALActorChromaKey::sActive &&
        (!ALActorChromaKey::keyedThisFrame() || warnings != "No capture overlays enabled.")
        ? warning : ALKitStyle::color(ALKitStyle::TEXT);
    setTextIfChanged(mReadouts[0], status);
    mReadouts[0]->setColor(tint);
    setTextIfChanged(mReadouts[1], codes(ALActorChromaKey::colour()));
    U32 keyed = 0, clones = 0, impostors = 0;
    for (LLCharacter* character : LLCharacter::sInstances)
        if (auto* av = dynamic_cast<LLVOAvatar*>(character); av && ALActorChromaKey::keepsLive(av))
        {
            if (!LLActorMover::actorMaskAvatarEligible(av)) ++impostors;
            else { ++keyed; if (av->isGhostAvatar()) ++clones; }
        }
    for (const auto& instance : ALGhostStudio::instance().getInstances())
        if (instance.mEntityId.isNull() && ALActorChromaKey::instanceKeyed(instance.mId)) { ++keyed; ++clones; }
    setTextIfChanged(mReadouts[2], llformat("%u keyed · %u clones · %u not keyed (impostor / ineligible)", keyed, clones, impostors));
    static LLCachedControl<F32> strength(gSavedSettings, "ActorKeyShadowStrength", .5f);
    setTextIfChanged(mShadowCodes, "code " + codes(ALActorChromaKey::colour(), 1.f - strength()));
    setTextIfChanged(mReadouts[3], ALActorKeyUI::shadowStatus());
    setTextIfChanged(mReadouts[4], warnings);
    static LLCachedControl<U32> test(gSavedSettings, "ActorKeySelfTest", 0);
    static LLCachedControl<U32> shadow_test(gSavedSettings, "ActorKeyShadowSelfTest", 0);
    setTextIfChanged(mReadouts[5], sLastResult + (test
        ? " Key test pending; waiting for keyed frames."
        : shadow_test ? " Exact shadow test pending; waiting for renderer." : ""));
}

void ALPanelActorKey::onVisibilityChange(bool visible)
{
    LLPanel::onVisibilityChange(visible);
    if (visible) mRevision.reset();
}

void ALPanelActorKey::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // Revision covers settings / membership; the timed fallback also catches
    // WP2's transient finalize reason and overlays which have no revision yet.
    if (isWidgetOnScreen(this) && mClock.due() && mRevision.changed({ALActorChromaKey::revision()})) refreshReadouts();
    LLPanel::draw();
}

void ALActorKeyUI::drawBadge()
{
    if (!ALActorChromaKey::sActive || !gViewerWindow->getUIVisibility() ||
        !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;
    static U64 revision = ~U64(0);
    static std::string label;
    if (revision != ALActorChromaKey::revision())
    {
        revision = ALActorChromaKey::revision();
        label = std::string("KEY ON · ") + modeName(ALActorChromaKey::mode());
    }
    LLGLSLShader* previous_shader = LLGLSLShader::sCurBoundShaderPtr;
    gUIProgram.bind();
    if (const LLFontGL* font = LLFontGL::getFontSansSerifSmall())
        font->renderUTF8(label, 0, gViewerWindow->getWorldViewRectScaled().mRight - 12,
            gViewerWindow->getWorldViewRectScaled().mTop - 42, LLColor4(1.f,.7f,.15f,1.f),
            LLFontGL::RIGHT, LLFontGL::TOP, LLFontGL::NORMAL, LLFontGL::DROP_SHADOW);
    if (previous_shader) previous_shader->bind();
    else gUIProgram.unbind();
}
