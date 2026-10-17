#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alrigrimcontroller.h"

#include "alkitrow.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "pipeline.h"
#include "lltextbox.h"
#include "llviewercontrol.h"

namespace
{
// [RigRimPreset] 20-preset Rig Rim table. ALPresetLook::RIM resets the complete
// owned look first, including quality controls added after these rows.
// A preset stamps the global rim
// levers (master/soften/back-softness/tint/shadow/alpha/bounce) and every
// per-light gain/sharpness/wrap/back-bias quad. Enabled, DebugRimOnly and
// IncludeProbes are deliberately NOT part of any row (matches the Night Mask
// preset's Enabled/TintColor exclusion above): the checkbox is what turns
// Rig Rim on, "Rim only" is a debug view, and probe capture is a perf/user
// choice, not a look. The rig's own light colours/positions/EVs are never
// touched either -- a preset is a lighting TERM, not a re-light.
struct RigRimPresetRow
{
    const char* mName;
    F32         mMaster, mSoften, mBackSoft, mTint, mShadow;
    bool        mAlpha, mBounce;
    F32         mKey[4]; // gain, sharpness, wrap, back bias
    F32         mFill[4];
    F32         mRim[4];
    F32         mBg[4];
};

const RigRimPresetRow RIG_RIM_PRESETS[20] = {
    // name                       Mast Sft  BSft Tint  Shdw  A  B   KEY g/k/wrap/b0                FILL                            RIM BG
    { "Subtle Edge",
      0.6f,
      0.5f,
      0.5f,
      0.25f,
      1.0f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 1.0f, 4.0f, 0.25f, 0.10f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Soft Portrait Wrap",
      1.0f,
      1.0f,
      0.8f,
      0.40f,
      0.8f,
      true,
      true,
      { 0.30f, 1.5f, 0.60f, -0.50f },
      { 0.25f, 1.5f, 0.70f, -0.50f },
      { 0.90f, 2.0f, 0.50f, -0.20f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Strong Backlight",
      1.6f,
      0.3f,
      0.35f,
      0.15f,
      0.5f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 2.5f, 3.0f, 0.40f, 0.20f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Hair Shine",
      1.3f,
      0.15f,
      0.4f,
      0.20f,
      0.6f,
      true,
      false,
      { 0.40f, 6.0f, 0.10f, -0.30f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 2.0f, 6.0f, 0.15f, 0.25f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Silhouette Glow (any angle)",
      1.2f,
      0.6f,
      0.01f,
      0.10f,
      0.7f,
      true,
      true,
      { 1.0f, 2.5f, 0.80f, -1.f },
      { 1.0f, 2.5f, 0.80f, -1.f },
      { 1.5f, 2.5f, 0.80f, -1.f },
      { 0.8f, 2.5f, 0.80f, -1.f } },
    { "Neon / Stage",
      2.5f,
      0.1f,
      0.25f,
      0.00f,
      0.4f,
      true,
      false,
      { 0.8f, 5.0f, 0.20f, -0.60f },
      { 0.8f, 5.0f, 0.20f, -0.60f },
      { 3.0f, 5.0f, 0.30f, 0.10f },
      { 2.0f, 5.0f, 0.30f, 0.10f } },
    { "Dreamy Halo",
      1.4f,
      1.0f,
      1.5f,
      0.30f,
      0.5f,
      true,
      true,
      { 0.5f, 0.75f, 0.90f, -0.80f },
      { 0.5f, 0.75f, 0.90f, -0.80f },
      { 2.0f, 0.75f, 0.90f, -0.60f },
      { 0.6f, 0.75f, 0.90f, -0.60f } },
    { "Noir Kicker",
      1.5f,
      0.2f,
      0.2f,
      0.10f,
      1.0f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 2.2f, 5.0f, 0.10f, 0.45f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Two-Sided Kick",
      1.4f,
      0.3f,
      0.3f,
      0.15f,
      0.6f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 1.8f, 4.0f, 0.30f, 0.15f },
      { 1.8f, 4.0f, 0.30f, 0.15f } },
    { "Fashion Edge",
      1.2f,
      0.4f,
      0.6f,
      0.35f,
      0.9f,
      true,
      true,
      { 0.9f, 2.5f, 0.50f, -1.f },
      { 0.6f, 2.5f, 0.50f, -1.f },
      { 1.2f, 3.0f, 0.40f, -0.30f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Sci-Fi Hologram",
      2.0f,
      0.0f,
      0.01f,
      0.00f,
      0.2f,
      true,
      false,
      { 1.0f, 1.5f, 1.0f, -1.f },
      { 1.0f, 1.5f, 1.0f, -1.f },
      { 1.5f, 1.5f, 1.0f, -1.f },
      { 1.0f, 1.5f, 1.0f, -1.f } },
    { "Candle Glow",
      1.0f,
      0.9f,
      0.9f,
      0.60f,
      0.8f,
      true,
      true,
      { 0.8f, 1.5f, 0.70f, -0.70f },
      { 0.5f, 1.5f, 0.70f, -0.70f },
      { 0.7f, 1.5f, 0.70f, -0.40f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Metal Sheen",
      1.3f,
      0.05f,
      0.5f,
      0.75f,
      1.0f,
      false,
      false,
      { 1.0f, 4.0f, 0.20f, -0.80f },
      { 0.6f, 4.0f, 0.20f, -0.80f },
      { 1.2f, 4.0f, 0.20f, -0.20f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Skin Sheen",
      0.8f,
      0.7f,
      0.7f,
      0.65f,
      1.0f,
      true,
      true,
      { 0.7f, 2.0f, 0.50f, -0.60f },
      { 0.35f, 2.0f, 0.50f, -0.60f },
      { 0.8f, 2.5f, 0.40f, -0.10f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Soft Fill Wrap",
      0.9f,
      1.0f,
      1.2f,
      0.45f,
      0.9f,
      true,
      true,
      { 0.4f, 1.0f, 1.0f, -0.90f },
      { 0.9f, 1.0f, 1.0f, -0.90f },
      { 0.5f, 1.5f, 0.80f, -0.50f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Horror Edge",
      1.8f,
      0.2f,
      0.15f,
      0.05f,
      0.3f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 1.5f, 6.0f, 0.10f, 0.35f },
      { 2.5f, 6.0f, 0.10f, 0.35f } },
    { "Moonlit Rim",
      1.1f,
      0.5f,
      0.5f,
      0.20f,
      0.85f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 1.6f, 3.5f, 0.30f, 0.30f },
      { 0.5f, 3.5f, 0.30f, 0.30f } },
    { "Glass Shell",
      1.6f,
      0.0f,
      0.01f,
      0.00f,
      0.1f,
      true,
      false,
      { 1.2f, 8.0f, 1.0f, -1.f },
      { 1.2f, 8.0f, 1.0f, -1.f },
      { 1.2f, 8.0f, 1.0f, -1.f },
      { 1.2f, 8.0f, 1.0f, -1.f } },
    // [TronT1] T1 contract section 5 -- linked from the Tron Legacy Cyan /
    // Ares Red look presets (ALTron::applyPreset). Never write
    // CineRigRimTronMode themselves: a Rig Rim preset is a lighting term,
    // the Tron look preset is what turns the tint on.
    { "Tron Suit Kick",
      2.2f,
      0.5f,
      0.5f,
      0.00f,
      0.4f,
      true,
      false,
      { 0.6f, 5.0f, 0.20f, -0.60f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 3.0f, 5.0f, 0.30f, 0.10f },
      { 0.f, 3.f, 0.35f, 0.f } },
    { "Ares Red Backlight",
      2.0f,
      0.5f,
      0.5f,
      0.00f,
      0.5f,
      true,
      false,
      { 0.f, 3.f, 0.35f, 0.f },
      { 0.f, 3.f, 0.35f, 0.f },
      { 2.8f, 4.0f, 0.35f, 0.20f },
      { 1.6f, 4.0f, 0.30f, 0.20f } },
};
static_assert(LL_ARRAY_SIZE(RIG_RIM_PRESETS) == 20, "Rig Rim preset count changed");

void applyRigRimPreset(const RigRimPresetRow& row)
{
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::owned(ALPresetLook::RIM));

    gSavedSettings.setF32("CineRigRimMasterGain", row.mMaster);
    gSavedSettings.setF32("CineRigRimRoughnessSoften", row.mSoften);
    gSavedSettings.setF32("CineRigRimBackSoftness", row.mBackSoft);
    gSavedSettings.setF32("CineRigRimTint", row.mTint);
    gSavedSettings.setF32("CineRigRimShadow", row.mShadow);
    gSavedSettings.setBOOL("CineRigRimIncludeAlpha", row.mAlpha);
    gSavedSettings.setBOOL("CineRigRimIncludeBounce", row.mBounce);

    gSavedSettings.setF32("CineRigRimKeyGain", row.mKey[0]);
    gSavedSettings.setF32("CineRigRimKeySharpness", row.mKey[1]);
    gSavedSettings.setF32("CineRigRimKeyWrap", row.mKey[2]);
    gSavedSettings.setF32("CineRigRimKeyBackBias", row.mKey[3]);

    gSavedSettings.setF32("CineRigRimFillGain", row.mFill[0]);
    gSavedSettings.setF32("CineRigRimFillSharpness", row.mFill[1]);
    gSavedSettings.setF32("CineRigRimFillWrap", row.mFill[2]);
    gSavedSettings.setF32("CineRigRimFillBackBias", row.mFill[3]);

    gSavedSettings.setF32("CineRigRimRimGain", row.mRim[0]);
    gSavedSettings.setF32("CineRigRimRimSharpness", row.mRim[1]);
    gSavedSettings.setF32("CineRigRimRimWrap", row.mRim[2]);
    gSavedSettings.setF32("CineRigRimRimBackBias", row.mRim[3]);

    gSavedSettings.setF32("CineRigRimBgGain", row.mBg[0]);
    gSavedSettings.setF32("CineRigRimBgSharpness", row.mBg[1]);
    gSavedSettings.setF32("CineRigRimBgWrap", row.mBg[2]);
    gSavedSettings.setF32("CineRigRimBgBackBias", row.mBg[3]);
}
} // namespace

const std::vector<std::string>& ALRigRimController::presetNames()
{
    static const std::vector<std::string> names = []
    {
        std::vector<std::string> result;
        result.reserve(LL_ARRAY_SIZE(RIG_RIM_PRESETS));
        for (const RigRimPresetRow& row : RIG_RIM_PRESETS)
            result.emplace_back(row.mName);
        return result;
    }();
    return names;
}

void ALRigRimController::attach(LLView* root)
{
    if (!root)
        return;
    mPreset                                = root->findChild<LLComboBox>("cine_rig_rim_preset");
    mQualityStatus                         = root->findChild<LLTextBox>("cine_rimq_status");
    mEnableTag                             = root->findChild<LLButton>("cine_rimq_enable_tag");
    static const char* const tron_names[7] = { "cine_rig_rim_tron_label", "cine_rig_rim_tron_mode",      "cine_rig_rim_tron_mix_label",
                                               "cine_rig_rim_tron_mix",   "cine_rig_rim_tron_mix_reset", "cine_rig_rim_tron_color_label",
                                               "cine_rig_rim_tron_color" };
    for (S32 i = 0; i < 7; ++i)
    {
        mTronTintWidgets[i] = root->findChild<LLView>(tron_names[i]);
    }
    if (mEnableTag)
    {
        mEnableTag->setVisible(false);
        LLHandle<LLButton> handle = mEnableTag->getDerivedHandle<LLButton>();
        mEnableTag->setCommitCallback(
            [handle](LLUICtrl*, const LLSD&)
            {
                LLNotificationsUtil::add("EnableGBufferAvatarTagConfirm", LLSD(), LLSD(),
                                         [handle](const LLSD& notification, const LLSD& response)
                                         {
                                             if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                                             {
                                                 gSavedSettings.setBOOL("RenderGBufferAvatarTag", true);
                                                 if (LLButton* button = handle.get())
                                                     button->setVisible(false);
                                             }
                                             return false;
                                         });
            });
    }
    if (mPreset)
    {
        mPreset->removeall();
        mPreset->add("Choose preset...", LLSD(0));
        const std::vector<std::string>& names = presetNames();
        for (S32 i = 0; i < (S32)names.size(); ++i)
        {
            mPreset->add(llformat("%d  %s", i + 1, names[i].c_str()), LLSD(i + 1));
        }
        mPreset->setValue(0);
        LLHandle<LLComboBox> handle = mPreset->getDerivedHandle<LLComboBox>();
        mPreset->setCommitCallback(
            [handle](LLUICtrl*, const LLSD&)
            {
                LLComboBox* combo = handle.get();
                if (!combo)
                {
                    return;
                }
                const S32                       index = combo->getSelectedValue().asInteger() - 1;
                const std::vector<std::string>& names = presetNames();
                if (index >= 0 && index < (S32)names.size())
                {
                    applyPresetByName(names[index]);
                }
                combo->setValue(0);
            });
    }
}

void ALRigRimController::onReveal()
{
    mStatusGate.reset();
    mStatusClock.forceDue();
}

void ALRigRimController::tickFrame()
{
    if (mStatusClock.due() && isWidgetOnScreen(mQualityStatus) && mStatusGate.changed({ gPipeline.rigRimStatusRevision() }))
    {
        const std::string& status = gPipeline.rigRimStatus();
        setTextIfChanged(mQualityStatus, status);
        ALKitRow::setControlVisible(mEnableTag, status.find("NO AVATAR TAG") != std::string::npos);
    }
}

void ALRigRimController::tickRefresh()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CineRigRimEnabled", false);
    for (LLView* view : mTronTintWidgets)
    {
        if (view)
        {
            ALKitRow::setControlVisible(view, enabled);
        }
    }
}

bool ALRigRimController::applyPresetByName(const std::string& name)
{
    if (ALPresetLook::replaying()) return false;
    for (const RigRimPresetRow& row : RIG_RIM_PRESETS)
    {
        if (name == row.mName)
        {
            applyRigRimPreset(row);
            return true;
        }
    }
    return false;
}
