/**
 * @file alpanelcinelightrig.cpp
 * @brief Shared Director/standalone cinematic light-rig panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelcinelightrig.h"

#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "alcinelightrigmodel.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lldirectorcast.h"
#include "llfocusmgr.h"
#include "llfloaterreg.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llscrolllistcell.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "v3color.h"
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "pipeline.h"

#include <algorithm>
#include <cmath>

static LLPanelInjector<ALPanelCineLightRig>
    t_panel_cine_light_rig("panel_cine_light_rig");

namespace
{
constexpr S32 GOBO_THUMBNAIL_SIZE = 32;
constexpr S32 GOBO_LABEL_COLUMN_WIDTH = 304;
constexpr S32 GOBO_PREVIEW_COLUMN_WIDTH = 40;

const char* const ROLE_NAMES[ALCineLightRigModel::LIGHT_COUNT] = {
    "Key", "Fill", "Rim", "Bg"
};
const char* const ROLE_WIDGET_NAMES[ALCineLightRigModel::LIGHT_COUNT] = {
    "key", "fill", "rim", "bg"
};
const char* const ROLE_ON_SETTINGS[ALCineLightRigModel::LIGHT_COUNT] = {
    "CineLightRigKeyOn", "CineLightRigFillOn",
    "CineLightRigRimOn", "CineLightRigBgOn"
};
std::vector<ALPanelCineLightRig*> LIVE_PANELS;

void addLabeledSeparator(LLComboBox* combo, const std::string& label,
                         bool add_separator)
{
    if (add_separator)
    {
        combo->addSeparator(ADD_BOTTOM);
    }
    combo->add(label, LLSD(), ADD_BOTTOM, false);
}

void addThumbnailGobo(LLComboBox* combo, S32 index)
{
    LLSD row;
    row["value"] = index;
    row["columns"][0]["column"] = "label";
    row["columns"][0]["value"] = ALCineLightRigModel::goboName(index);
    row["columns"][0]["width"] = GOBO_LABEL_COLUMN_WIDTH;
    row["columns"][1]["column"] = "preview";
    row["columns"][1]["type"] = "icon";
    row["columns"][1]["value"] = index > 0
        ? llformat("CineGobo%02d", index) : std::string();
    row["columns"][1]["width"] = GOBO_PREVIEW_COLUMN_WIDTH;
    row["columns"][1]["icon_size"] = GOBO_THUMBNAIL_SIZE;
    combo->addElement(row, ADD_BOTTOM);
}

void addThumbnailGoboHeader(LLComboBox* combo, const std::string& label)
{
    LLSD row;
    row["value"] = LLSD();
    row["columns"][0]["column"] = "label";
    row["columns"][0]["value"] = "\xE2\x80\x94 " + label + " \xE2\x80\x94";
    row["columns"][0]["width"] = GOBO_LABEL_COLUMN_WIDTH;
    row["columns"][1]["column"] = "preview";
    row["columns"][1]["value"] = "";
    row["columns"][1]["width"] = GOBO_PREVIEW_COLUMN_WIDTH;
    if (LLScrollListItem* item = combo->addElement(row, ADD_BOTTOM))
    {
        item->setEnabled(false);
    }
}

void addThumbnailGoboLibrary(LLComboBox* combo)
{
    addThumbnailGobo(combo, 0);
    for (S32 category = 1;
         category < ALCineLightRigModel::GOBO_CATEGORY_COUNT; ++category)
    {
        addThumbnailGoboHeader(
            combo, ALCineLightRigModel::goboCategoryName(category));
        for (S32 index = 1; index < ALCineLightRigModel::GOBO_COUNT; ++index)
        {
            if (ALCineLightRigModel::goboCategory(index) == category)
            {
                addThumbnailGobo(combo, index);
            }
        }
    }
}

// 19-preset lens flare table, adapted from the VirtualCinema reference (see
// docs/projector_lensflare_design.md). Applying a preset stamps every field
// below onto its matching RenderLensFlare* setting; RenderLensFlareMaster and
// the RenderCineLensFlare* rig controls are deliberately NOT part of any
// preset row — MasterIntensity is a one-knob scale independent of preset, and
// the rig master/intensity are a separate on/off + brightness the user sets
// once for their scene.
struct FlarePresetRow
{
    const char* mName;
    F32 mStreakInt;
    F32 mTintR, mTintG, mTintB;
    S32 mGhostCount;
    F32 mGhostDisp;
    F32 mGhostInt;
    F32 mGhostChroma;
    F32 mHaloInt;
    F32 mHaloWidth;
    F32 mHaloChroma;
    F32 mRingInt;
    F32 mRingRadius;
    F32 mRingWidth;
    F32 mRingDisp;
    S32 mRingCount;
    F32 mCircleInt;
    F32 mCircleScale;
    F32 mCircleSpacing;
    S32 mCircleCount;
    F32 mCore;
    F32 mSpikeInt;
    S32 mSpikeCount;
    F32 mSpikeLen;
    F32 mIrisInt;
    S32 mIrisCount;
    S32 mIrisSides;
    F32 mIrisSize;
    F32 mArcInt;
};

const FlarePresetRow FLARE_PRESETS[19] = {
    // name                  streak  tintR tintG tintB  gN  gDisp gInt gChr   hInt hWid hChr    rInt rRad rWid rDsp rN   cInt cScl cSpc cN   core  spI  spN spLen  irI  irN irSd irSz  arc
    { "Anamorphic Blue",       1.2f, .35f, .55f, 1.f,   4, .35f, .22f, .012f, .28f, .35f, .015f, .15f, .30f, .10f, 1.0f, 1,  .12f, .15f, .14f, 5,  .35f, .10f, 6, .40f,  .15f, 4, 6, .055f, .08f },
    { "Anamorphic Gold",       1.1f, 1.f, .78f, .42f,   4, .33f, .24f, .014f, .30f, .33f, .016f, .16f, .29f, .10f, 1.0f, 1,  .13f, .15f, .13f, 5,  .35f, .12f, 6, .40f,  .16f, 4, 6, .055f, .08f },
    { "Spherical Prime",       .35f, .80f, .85f, 1.f,   5, .30f, .28f, .010f, .22f, .30f, .012f, .12f, .32f, .09f, 1.0f, 1,  .15f, .14f, .12f, 6,  .25f, .32f, 8, .30f,  .26f, 6, 9, .050f, .06f },
    { "Vintage Uncoated",      .55f, 1.f, .72f, .50f,   8, .28f, .40f, .022f, .55f, .40f, .024f, .35f, .30f, .12f, 1.3f, 2,  .30f, .16f, .16f, 7,  .45f, .15f, 12, .25f, .40f, 8, 5, .070f, .22f },
    { "Master Anamorphic",     1.4f, .40f, .60f, 1.f,   2, .40f, .14f, .008f, .20f, .30f, .010f, .08f, .30f, .08f, 1.0f, 1,  .06f, .14f, .12f, 3,  .28f, .06f, 4, .45f,  .10f, 2, 8, .045f, .04f },
    { "Sci-Fi / Neon",         1.6f, .30f, .90f, 1.f,   7, .36f, .45f, .028f, .50f, .42f, .030f, .40f, .35f, .13f, 1.6f, 3,  .35f, .18f, .17f, 7,  .50f, .45f, 10, .50f, .35f, 7, 6, .065f, .30f },
    { "Minimal Glint",         .30f, .85f, .90f, 1.f,   1, .30f, .08f, .008f, .12f, .28f, .010f, .05f, .28f, .08f, 1.0f, 1,  .04f, .14f, .12f, 2,  .15f, .12f, 6, .25f,  .06f, 1, 7, .040f, .03f },
    { "Rainbow Prism",         .60f, .70f, .80f, 1.f,   6, .34f, .30f, .020f, .35f, .34f, .025f, .45f, .30f, .13f, 1.8f, 3,  .40f, .16f, .16f, 7,  .35f, .25f, 14, .35f, .30f, 7, 6, .065f, .45f },
    { "Dreamy Halo",           .25f, .90f, .92f, 1.f,   3, .30f, .15f, .012f, .70f, .42f, .020f, .15f, .32f, .12f, 1.0f, 1,  .18f, .16f, .14f, 5,  .55f, .08f, 4, .50f,  .12f, 3, 9, .060f, .12f },
    { "JJ Blue Blast",         2.0f, .30f, .55f, 1.f,   6, .40f, .35f, .015f, .35f, .35f, .015f, .20f, .30f, .10f, 1.2f, 2,  .15f, .15f, .14f, 4,  .65f, .18f, 4, .60f,  .22f, 6, 8, .055f, .08f },
    { "Retro 70s Warm",        .50f, 1.f, .72f, .45f,   8, .28f, .40f, .024f, .50f, .40f, .024f, .35f, .30f, .13f, 1.3f, 2,  .35f, .17f, .16f, 7,  .40f, .18f, 10, .30f, .45f, 8, 5, .075f, .20f },
    { "Cyberpunk Neon",        1.5f, 1.f, .30f, .90f,   7, .36f, .45f, .028f, .50f, .42f, .030f, .40f, .35f, .13f, 1.6f, 3,  .40f, .18f, .17f, 7,  .50f, .42f, 12, .45f, .35f, 7, 3, .060f, .35f },
    { "Ethereal Angelic",      .30f, 1.f, .98f, .92f,   4, .30f, .18f, .014f, .65f, .44f, .018f, .18f, .33f, .12f, 1.0f, 1,  .22f, .16f, .14f, 6,  .60f, .15f, 6, .55f,  .10f, 3, 9, .060f, .10f },
    { "Golden Hour",           .80f, 1.f, .70f, .45f,   3, .32f, .18f, .012f, .40f, .38f, .018f, .12f, .32f, .11f, 1.0f, 1,  .15f, .16f, .14f, 4,  .55f, .10f, 6, .45f,  .12f, 3, 7, .060f, .06f },
    { "Noir Practical",        .35f, .75f, .85f, 1.f,   2, .30f, .12f, .008f, .18f, .30f, .010f, .05f, .30f, .09f, 1.0f, 1,  .05f, .14f, .12f, 2,  .30f, .06f, 4, .30f,  .06f, 2, 8, .045f, .02f },
    { "Documentary Real",      .20f, .90f, .95f, 1.f,   2, .28f, .10f, .006f, .10f, .28f, .008f, .03f, .30f, .08f, 1.0f, 1,  .03f, .14f, .12f, 2,  .18f, .05f, 6, .20f,  .05f, 2, 7, .040f, .02f },
    { "Blockbuster T-O",       1.3f, .25f, .75f, .90f,  5, .36f, .28f, .014f, .32f, .34f, .016f, .15f, .32f, .11f, 1.2f, 2,  .18f, .16f, .15f, 5,  .50f, .15f, 6, .45f,  .20f, 5, 8, .055f, .10f },
    { "Sodium Night",          .55f, 1.f, .62f, .25f,   6, .30f, .35f, .020f, .45f, .40f, .022f, .18f, .33f, .12f, 1.2f, 2,  .22f, .17f, .15f, 6,  .45f, .10f, 8, .30f,  .30f, 6, 5, .065f, .12f },
    { "Music Video Glam",      .90f, 1.f, .85f, .95f,   5, .34f, .30f, .018f, .60f, .42f, .022f, .22f, .34f, .12f, 1.1f, 2,  .30f, .17f, .15f, 6,  .55f, .30f, 12, .40f, .25f, 6, 9, .060f, .20f },
};

void applyFlarePreset(const FlarePresetRow& row)
{
    gSavedSettings.setF32("RenderLensFlareStreakIntensity", row.mStreakInt);
    // LLControlGroup has no setColor3(); Color3-typed controls are written
    // via the generic LLSD path (mirrors LLColor3's DefaultParam usage
    // elsewhere, e.g. llsettingsvo.cpp).
    gSavedSettings.setUntypedValue("RenderLensFlareStreakTint",
        LLColor3(row.mTintR, row.mTintG, row.mTintB).getValue());
    gSavedSettings.setS32("RenderLensFlareGhostCount", row.mGhostCount);
    gSavedSettings.setF32("RenderLensFlareGhostSpacing", row.mGhostDisp);
    gSavedSettings.setF32("RenderLensFlareGhost", row.mGhostInt);
    gSavedSettings.setF32("RenderLensFlareGhostChroma", row.mGhostChroma);
    gSavedSettings.setF32("RenderLensFlareHalo", row.mHaloInt);
    gSavedSettings.setF32("RenderLensFlareHaloWidth", row.mHaloWidth);
    gSavedSettings.setF32("RenderLensFlareHaloChroma", row.mHaloChroma);
    gSavedSettings.setF32("RenderLensFlareRing", row.mRingInt);
    gSavedSettings.setF32("RenderLensFlareRingRadius", row.mRingRadius);
    gSavedSettings.setF32("RenderLensFlareRingWidth", row.mRingWidth);
    gSavedSettings.setF32("RenderLensFlareRingDispersion", row.mRingDisp);
    gSavedSettings.setS32("RenderLensFlareRingCount", row.mRingCount);
    gSavedSettings.setF32("RenderLensFlareCircle", row.mCircleInt);
    gSavedSettings.setF32("RenderLensFlareCircleScale", row.mCircleScale);
    gSavedSettings.setF32("RenderLensFlareCircleSpacing", row.mCircleSpacing);
    gSavedSettings.setS32("RenderLensFlareCircleCount", row.mCircleCount);
    gSavedSettings.setF32("RenderLensFlareGlow", row.mCore);
    gSavedSettings.setF32("RenderLensFlareStarburst", row.mSpikeInt);
    gSavedSettings.setS32("RenderLensFlareStarburstSpikes", row.mSpikeCount);
    gSavedSettings.setF32("RenderLensFlareStarburstLength", row.mSpikeLen);
    gSavedSettings.setF32("RenderLensFlareIris", row.mIrisInt);
    gSavedSettings.setS32("RenderLensFlareIrisCount", row.mIrisCount);
    gSavedSettings.setS32("RenderLensFlareIrisSides", row.mIrisSides);
    gSavedSettings.setF32("RenderLensFlareIrisSize", row.mIrisSize);
    gSavedSettings.setF32("RenderLensFlareArc", row.mArcInt);
}

// 6-preset Night Mask table. Applying a preset stamps the mask SHAPE and its
// strength knobs (distance/feather/darkness/tint strength/desaturation) onto
// the matching CineLightRigNightMask* setting. Enabled and TintColor are
// deliberately NOT part of any preset row: the checkbox is what turns the
// mask on (a preset only shapes it), and TintColor stays whatever the user
// last picked in the swatch.
struct NightMaskPresetRow
{
    const char* mName;
    S32 mShape;
    F32 mDistance;
    F32 mFeather;
    F32 mDarkness;
    F32 mTintStrength;
    F32 mDesaturation;
};

const NightMaskPresetRow NIGHT_MASK_PRESETS[6] = {
    // name                 shape   dist  feath  dark  tint  desat
    { "Campfire Bubble",     1,     4.f,  3.f,  0.15f, 0.25f, 0.20f },
    { "Moonlit Iris",        1,     6.f,  5.f,  0.25f, 0.50f, 0.40f },
    { "Stage Box",           2,     5.f,  2.f,  0.10f, 0.00f, 0.00f },
    { "Background Drop",     0,     8.f,  6.f,  0.20f, 0.30f, 0.25f },
    { "Deep Night Wide",     1,    10.f,  8.f,  0.08f, 0.60f, 0.50f },
    { "Pitch Black Cell",    2,     3.f,  1.f,  0.02f, 0.00f, 0.00f },
};

void applyNightMaskPreset(const NightMaskPresetRow& row)
{
    gSavedSettings.setS32("CineLightRigNightMaskShape", row.mShape);
    gSavedSettings.setF32("CineLightRigNightMaskDistance", row.mDistance);
    gSavedSettings.setF32("CineLightRigNightMaskFeather", row.mFeather);
    gSavedSettings.setF32("CineLightRigNightMaskDarkness", row.mDarkness);
    gSavedSettings.setF32("CineLightRigNightMaskTintStrength", row.mTintStrength);
    gSavedSettings.setF32("CineLightRigNightMaskDesaturation", row.mDesaturation);
}

void registerCineLightRigResetControl()
{
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;
    LLUICtrl::CommitCallbackRegistry::defaultRegistrar().add(
        "CineLightRig.ResetControl",
        [](LLUICtrl*, const LLSD& param)
        {
            // Accept a single control name or a comma-separated list so one
            // reset button (e.g. the Easy cone width) can restore every backing
            // setting at once.
            const std::string names = param.asString();
            size_t start = 0;
            while (start <= names.size())
            {
                const size_t comma = names.find(',', start);
                const size_t end =
                    (comma == std::string::npos) ? names.size() : comma;
                const size_t first = names.find_first_not_of(" \t", start);
                if (first != std::string::npos && first < end)
                {
                    const size_t last = names.find_last_not_of(" \t", end - 1);
                    const std::string name =
                        names.substr(first, last - first + 1);
                    if (LLControlVariable* control =
                            gSavedSettings.getControl(name))
                    {
                        control->resetToDefault(true);
                    }
                }
                if (comma == std::string::npos)
                {
                    break;
                }
                start = comma + 1;
            }
        });
}
}

ALPanelCineLightRig::ALPanelCineLightRig()
{
    // XUI commit callbacks are resolved while the panel's children are built.
    registerCineLightRigResetControl();
}

ALPanelCineLightRig::~ALPanelCineLightRig()
{
    LIVE_PANELS.erase(
        std::remove(LIVE_PANELS.begin(), LIVE_PANELS.end(), this),
        LIVE_PANELS.end());
}

const std::vector<std::string>& ALPanelCineLightRig::settings()
{
    static const std::vector<std::string> names = {
        "CineLightRigEnabled",
        "CineLightRigScaleAware",
        "CineLightRigPower",
        "CineLightRigRadius",
        "CineLightRigMasterEV",
        "CineLightRigMasterTempMired",
        "CineLightRigOffsetZ",
        "CineLightRigHeadroomStops",
        "CineLightRigBounceEnabled",
        "CineLightRigBounceRatio",
        "CineLightRigTransitionSec",
        "CineLightRigDamping",
        "CineLightRigTrackMode",
        "CineLightRigObjectTarget",
        "CineLightRigCookieUUID",
        "CineLightRigSeed",
        "CineLightRigMirror",
        "CineLightRigOrbitYaw",
        "CineLightRigOrbitPitch",
        "CineLightRigFX",
        "CineLightRigShadowMode",
        "CineLightRigAutoShadowSlots",
        "CineLightRigGizmo",
        "CineLightRigRatioLock",
        "CineLightRigRatio",
        "CineLightRigCatchlight",
        "CineLightRigCatchlightEV",
        "CineLightRigCatchlightSize",
        "CineLightRigCatchlightAngle",
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyGel",
        "CineLightRigKeyFixtureMode",
        "CineLightRigKeyFixturePreset",
        "CineLightRigKeyKelvin",
        "CineLightRigKeyGelSlot0",
        "CineLightRigKeyGelSlot1",
        "CineLightRigKeyGelSlot2",
        "CineLightRigKeySourceSizeM",
        "CineLightRigKeyFlicker",
        "CineLightRigKeyFlickerAmount",
        "CineLightRigKeyShadowSoft",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillGel",
        "CineLightRigFillFixtureMode",
        "CineLightRigFillFixturePreset",
        "CineLightRigFillKelvin",
        "CineLightRigFillGelSlot0",
        "CineLightRigFillGelSlot1",
        "CineLightRigFillGelSlot2",
        "CineLightRigFillSourceSizeM",
        "CineLightRigFillFlicker",
        "CineLightRigFillFlickerAmount",
        "CineLightRigFillShadowSoft",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimGel",
        "CineLightRigRimFixtureMode",
        "CineLightRigRimFixturePreset",
        "CineLightRigRimKelvin",
        "CineLightRigRimGelSlot0",
        "CineLightRigRimGelSlot1",
        "CineLightRigRimGelSlot2",
        "CineLightRigRimSourceSizeM",
        "CineLightRigRimFlicker",
        "CineLightRigRimFlickerAmount",
        "CineLightRigRimShadowSoft",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgGel",
        "CineLightRigBgFixtureMode",
        "CineLightRigBgFixturePreset",
        "CineLightRigBgKelvin",
        "CineLightRigBgGelSlot0",
        "CineLightRigBgGelSlot1",
        "CineLightRigBgGelSlot2",
        "CineLightRigBgSourceSizeM",
        "CineLightRigBgFlicker",
        "CineLightRigBgFlickerAmount",
        "CineLightRigBgShadowSoft",
        "CineLightRigBgOn",
        "CineLightRigLiveProbeEnabled",
        "CineLightRigLiveProbeTarget",
        "CineLightRigLiveProbeRadius",
        "CineLightRigLiveProbeOffsetZ",
        "CineLightRigLiveProbeAmbiance",
        "CineLightRigLiveProbeReplaceBounce",
        "CineLightRigLiveProbeGizmo",
        "CineLightRigNightMaskEnabled",
        "CineLightRigNightMaskTarget",
        "CineLightRigNightMaskShape",
        "CineLightRigNightMaskDistance",
        "CineLightRigNightMaskFeather",
        "CineLightRigNightMaskDarkness",
        "CineLightRigNightMaskHeightOffset",
        "CineLightRigNightMaskGizmo",
        "CineLightRigNightMaskTintStrength",
        "CineLightRigNightMaskTintColor",
        "CineLightRigNightMaskDesaturation",
    };
    return names;
}

bool ALPanelCineLightRig::postBuild()
{
    mAnchorCombo = getChild<LLComboBox>("cine_anchor");
    mObjectTargetButton = getChild<LLButton>("cine_target_object");
    mObjectTargetClear = getChild<LLButton>("cine_clear_object");
    mObjectTargetStatus = getChild<LLTextBox>("cine_object_target_status");
    mGroupEnable = getChild<LLCheckBoxCtrl>("cine_group_enable");
    mGroupStatus = getChild<LLTextBox>("cine_group_status");
    const char* const group_slot_names[5] = {
        "cine_group_self", "cine_group_a", "cine_group_b",
        "cine_group_c", "cine_group_d",
    };
    for (S32 i = 0; i < 5; ++i)
    {
        mGroupSlotChecks[i] = getChild<LLCheckBoxCtrl>(group_slot_names[i]);
    }
    mSetupCombo = getChild<LLComboBox>("cine_setup_combo");
    mFlarePreset = getChild<LLComboBox>("cine_flare_preset");
    mEasyFlarePreset = getChild<LLComboBox>("cine_easy_flare_preset");
    mFXCombo = getChild<LLComboBox>("cine_fx_combo");
    mSeedEditor = getChild<LLLineEditor>("cine_seed");
    mFillEV = getChild<LLSpinCtrl>("cine_fill_ev");
    mEasyBrightness = getChild<LLUICtrl>("cine_easy_brightness");
    mEasyDrama = getChild<LLUICtrl>("cine_easy_drama");
    mEasyRim = getChild<LLComboBox>("cine_easy_rim");
    mEasyBg = getChild<LLComboBox>("cine_easy_bg");
    mEasyWarmth = getChild<LLUICtrl>("cine_easy_warmth");
    mEasyConeWidth = getChild<LLUICtrl>("cine_easy_cone_width");
    mEasyConeFeather = getChild<LLUICtrl>("cine_easy_cone_feather");
    mEasyModeToggle = getChild<LLCheckBoxCtrl>("cine_easy_mode");
    mFixtureRole = getChild<LLComboBox>("cine_fixture_role");
    mFixtureMode = getChild<LLCheckBoxCtrl>("cine_fixture_mode");
    mFixturePreset = getChild<LLComboBox>("cine_fixture_preset");
    mFixtureKelvin = getChild<LLSpinCtrl>("cine_fixture_kelvin");
    mFixtureSourceSize = getChild<LLSpinCtrl>("cine_fixture_source_size");
    mFixtureGelSlots[0] = getChild<LLComboBox>("cine_fixture_gel_0");
    mFixtureGelSlots[1] = getChild<LLComboBox>("cine_fixture_gel_1");
    mFixtureGelSlots[2] = getChild<LLComboBox>("cine_fixture_gel_2");
    mGoboLibrary = getChild<LLComboBox>("cine_gobo_library");
    mGoboPreview = getChild<LLIconCtrl>("cine_gobo_preview");
    mGoboSoftness = getChild<LLTextBox>("cine_gobo_softness");
    mLiveProbeStatus = getChild<LLTextBox>("cine_live_probe_status");
    mNightMaskPreset = getChild<LLComboBox>("cine_night_mask_preset");
    mEasyNightMaskPreset = getChild<LLComboBox>("cine_easy_night_mask_preset");
    mNightMaskStatus = getChild<LLTextBox>("night_mask_status");
    const char* const advanced_driven_names[] = {
        "cine_master_ev", "cine_master_ev_reset",
        "cine_key_ev", "cine_key_ev_reset",
        "cine_fill_ev", "cine_fill_ev_reset",
        "cine_rim_ev", "cine_rim_ev_reset",
        "cine_bg_ev", "cine_bg_ev_reset",
        "cine_ratio_lock", "cine_ratio_lock_reset",
        "cine_ratio", "cine_ratio_reset",
    };
    for (const char* name : advanced_driven_names)
    {
        mAdvancedDrivenControls.push_back(getChild<LLUICtrl>(name));
    }
    mShadowHint = getChild<LLTextBox>("cine_shadow_hint");
    mShadowFixIt = getChild<LLButton>("cine_shadow_fixit");
    mRadiusLabel = getChild<LLTextBox>("cine_radius_label");
    mRadiusDefaultColor = LLUIColorTable::instance().getColor(
        "LabelTextColor", LLColor4::white);
    mSetupSave = getChild<LLButton>("cine_setup_save");
    mSetupDelete = getChild<LLButton>("cine_setup_delete");
    if (std::find(LIVE_PANELS.begin(), LIVE_PANELS.end(), this) ==
        LIVE_PANELS.end())
    {
        LIVE_PANELS.push_back(this);
    }

    populateStaticCombos();
    updateAnchorList();
    refreshSetupList();

    mAnchorCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAnchorSelected(); });
    mObjectTargetButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onTargetObject(); });
    mObjectTargetClear->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClearObjectTarget(); });
    mGroupEnable->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGroupEnabledCommit(); });
    for (LLCheckBoxCtrl* control : mGroupSlotChecks)
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onGroupSlotsCommit(); });
    }
    mSetupCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSetupSelected(); });
    mFlarePreset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFlarePresetSelected(mFlarePreset); });
    mEasyFlarePreset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFlarePresetSelected(mEasyFlarePreset); });
    mNightMaskPreset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onNightMaskPresetSelected(mNightMaskPreset); });
    mEasyNightMaskPreset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onNightMaskPresetSelected(mEasyNightMaskPreset); });
    mSetupSave->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { saveSetup(); });
    getChild<LLButton>("cine_setup_delete")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { deleteSetup(); });
    getChild<LLButton>("cine_fx_stop")->setCommitCallback(
        [](LLUICtrl*, const LLSD&)
        { ALCineLightRigManager::instance().selected().stopFX(); });
    getChild<LLButton>("cine_aim_yaw_minus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitYaw", -15.f); });
    getChild<LLButton>("cine_aim_yaw_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitYaw", 15.f); });
    getChild<LLButton>("cine_aim_pitch_minus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitPitch", -5.f); });
    getChild<LLButton>("cine_aim_pitch_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustAim("CineLightRigOrbitPitch", 5.f); });
    getChild<LLButton>("cine_aim_reset")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAim(); });
    getChild<LLButton>("cine_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAll(); });
    mSeedEditor->setMaxTextLength(10);
    mSeedEditor->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { commitSeed(); });
    getChild<LLButton>("cine_seed_randomize")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { randomizeSeed(); });
    mShadowFixIt->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickShadowFixIt(); });
    mEasyModeToggle->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyModeCommit(); });
    mEasyBrightness->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyBrightnessCommit(); });
    mEasyDrama->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyDramaCommit(); });
    mEasyRim->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyRimCommit(); });
    mEasyBg->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyBgCommit(); });
    mEasyWarmth->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyWarmthCommit(); });
    mEasyConeWidth->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyConeWidthCommit(); });
    mEasyConeFeather->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onEasyConeFeatherCommit(); });
    mFixtureRole->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFixtureRoleCommit(); });
    mFixtureMode->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFixtureModeCommit(); });
    mFixturePreset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFixturePresetCommit(); });
    mFixtureKelvin->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFixtureValuesCommit(); });
    mFixtureSourceSize->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onFixtureValuesCommit(); });
    for (LLComboBox* gel : mFixtureGelSlots)
    {
        gel->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onFixtureValuesCommit(); });
    }
    mGoboLibrary->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGoboLibraryCommit(); });
    getChild<LLButton>("cine_open_cues")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("cine_light_cues"); });
    getChild<LLUICtrl>("cine_manual")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onManualCommit(); });
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        const std::string prefix =
            std::string("cine_") + ROLE_WIDGET_NAMES[i];
        mClipStatus[i] = getChild<LLTextBox>(prefix + "_clip");
        mShaftControls[i] = getChild<LLCheckBoxCtrl>(prefix + "_shaft");
        mHeroControls[i] = getChild<LLCheckBoxCtrl>(prefix + "_hero");
        mManualConeWidths[i] = getChild<LLUICtrl>(prefix + "_cone_width");
        mManualConeWidths[i]->setCommitCallback(
            [this, i](LLUICtrl*, const LLSD&) { onManualConeWidthCommit(i); });
        mShaftControls[i]->setCommitCallback(
            [i](LLUICtrl* control, const LLSD&)
            {
                ALCineLightRigManager::instance().selected().setShaftEnabled(
                    i, control->getValue().asBoolean());
            });
        mHeroControls[i]->setCommitCallback(
            [i](LLUICtrl* control, const LLSD&)
            {
                ALCineLightRigManager::instance().selected().setHeroEnabled(
                    i, control->getValue().asBoolean());
            });
    }

    syncSeedEditor();
    syncObjectTargetControls(true);
    syncGroupControls();
    syncFixtureControls(true);
    syncGoboLibrary(true);
    updateDerivedStatus();
    return true;
}

void ALPanelCineLightRig::populateStaticCombos()
{
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        const std::string widget_prefix =
            std::string("cine_") + ROLE_WIDGET_NAMES[light];
        LLComboBox* profile = getChild<LLComboBox>(widget_prefix + "_profile");
        LLComboBox* gobo = getChild<LLComboBox>(widget_prefix + "_gobo");
        LLComboBox* gel = getChild<LLComboBox>(widget_prefix + "_gel");
        LLComboBox* flicker = getChild<LLComboBox>(
            widget_prefix + "_flicker");
        for (S32 i = 0; i < ALCineLightRigModel::PROFILE_COUNT; ++i)
        {
            profile->add(ALCineLightRigModel::profileName(i), LLSD(i));
        }
        addThumbnailGoboLibrary(gobo);
        gel->add(ALCineLightRigModel::gelName(0), LLSD(0));
        addLabeledSeparator(gel, "Colour Temp", true);
        for (S32 i = 1; i < ALCineLightRigModel::GEL_COUNT; ++i)
        {
            if (ALCineLightRigModel::gelIsColourTemperature(i))
            {
                gel->add(ALCineLightRigModel::gelName(i), LLSD(i));
            }
        }
        for (S32 i = 0; i < ALCineLightRigModel::FLICKER_COUNT; ++i)
        {
            flicker->add(
                ALCineLightRigModel::flickerProgramName(i), LLSD(i));
        }
        addLabeledSeparator(gel, "Colour", true);
        for (S32 i = 1; i < ALCineLightRigModel::GEL_COUNT; ++i)
        {
            if (!ALCineLightRigModel::gelIsColourTemperature(i))
            {
                gel->add(ALCineLightRigModel::gelName(i), LLSD(i));
            }
        }
        const std::string setting_prefix =
            std::string("CineLightRig") + ROLE_NAMES[light];
        profile->setValue(gSavedSettings.getS32(setting_prefix + "Profile"));
        gobo->setValue(gSavedSettings.getS32(setting_prefix + "Gobo"));
        gel->setValue(gSavedSettings.getS32(setting_prefix + "Gel"));
        flicker->setValue(gSavedSettings.getS32(setting_prefix + "Flicker"));
    }

    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        mFixtureRole->add(ROLE_NAMES[light], LLSD(light));
    }
    mFixtureRole->setValue(mFixtureRoleIndex);
    for (S32 preset = 0;
         preset < ALCineLightRigModel::FIXTURE_PRESET_COUNT; ++preset)
    {
        mFixturePreset->add(
            ALCineLightRigModel::fixturePresetName(preset), LLSD(preset));
    }
    for (LLComboBox* gel : mFixtureGelSlots)
    {
        gel->add(ALCineLightRigModel::fixtureGelName(0), LLSD(0));
        addLabeledSeparator(gel, "Colour Temp", true);
        for (S32 index = 1;
             index < ALCineLightRigModel::FIXTURE_GEL_COUNT; ++index)
        {
            if (ALCineLightRigModel::fixtureGelIsColourTemperature(index))
            {
                gel->add(
                    ALCineLightRigModel::fixtureGelName(index), LLSD(index));
            }
        }
        addLabeledSeparator(gel, "Colour", true);
        for (S32 index = 1;
             index < ALCineLightRigModel::FIXTURE_GEL_COUNT; ++index)
        {
            if (!ALCineLightRigModel::fixtureGelIsColourTemperature(index))
            {
                gel->add(
                    ALCineLightRigModel::fixtureGelName(index), LLSD(index));
            }
        }
    }

    addThumbnailGoboLibrary(mGoboLibrary);

    mFXCombo->add("None", LLSD(-1));
    for (S32 fx = 0; fx < ALCineLightRigModel::FX_COUNT; ++fx)
    {
        mFXCombo->add(ALCineLightRigModel::fxName(fx), LLSD(fx));
    }
    mFXCombo->setValue(gSavedSettings.getS32("CineLightRigFX"));

    // Lens flare presets: 0 is the "Choose preset..." sentinel (manual
    // sliders / whatever the user last dialed in); 1..19 apply-and-snap-back,
    // mirroring the gaze Movement Style / Lean preset boxes. The Easy card
    // carries a shortcut copy of this same combo, so both are populated
    // identically here and share the single apply/snap-back code path in
    // onFlarePresetSelected().
    constexpr S32 FLARE_PRESET_COUNT =
        static_cast<S32>(sizeof(FLARE_PRESETS) / sizeof(FLARE_PRESETS[0]));
    LLComboBox* const flare_combos[] = { mFlarePreset, mEasyFlarePreset };
    for (LLComboBox* combo : flare_combos)
    {
        if (!combo)
        {
            continue;
        }
        combo->add("Choose preset...", LLSD(0));
        for (S32 i = 0; i < FLARE_PRESET_COUNT; ++i)
        {
            combo->add(
                llformat("%d  %s", i + 1, FLARE_PRESETS[i].mName), LLSD(i + 1));
        }
        combo->setValue(0);
    }

    // Night Mask presets: same sentinel / apply-and-snap-back pattern as the
    // lens flare presets above, shared identically by the Advanced card combo
    // and the Easy card's shortcut copy.
    constexpr S32 NIGHT_MASK_PRESET_COUNT =
        static_cast<S32>(sizeof(NIGHT_MASK_PRESETS) / sizeof(NIGHT_MASK_PRESETS[0]));
    LLComboBox* const night_mask_combos[] = { mNightMaskPreset, mEasyNightMaskPreset };
    for (LLComboBox* combo : night_mask_combos)
    {
        if (!combo)
        {
            continue;
        }
        combo->add("Choose preset...", LLSD(0));
        for (S32 i = 0; i < NIGHT_MASK_PRESET_COUNT; ++i)
        {
            combo->add(
                llformat("%d  %s", i + 1, NIGHT_MASK_PRESETS[i].mName), LLSD(i + 1));
        }
        combo->setValue(0);
    }
}

std::string ALPanelCineLightRig::fixtureSettingPrefix() const
{
    const S32 role = std::clamp(
        mFixtureRoleIndex, 0, ALCineLightRigModel::LIGHT_COUNT - 1);
    return std::string("CineLightRig") + ROLE_NAMES[role];
}

void ALPanelCineLightRig::syncFixtureControls(bool force)
{
    if (!mFixtureRole || !mFixtureMode || !mFixturePreset ||
        !mFixtureKelvin || !mFixtureSourceSize)
    {
        return;
    }
    const S32 selected_slot = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    if (selected_slot != mDisplayedFixtureSlot)
    {
        force = true;
        mDisplayedFixtureSlot = selected_slot;
    }
    const std::string prefix = fixtureSettingPrefix();
    auto sync_value = [force](LLUICtrl* control, const LLSD& value)
    {
        if (force || (!control->hasFocus() &&
                      !gFocusMgr.childHasKeyboardFocus(control)))
        {
            control->setValue(value);
        }
    };

    const bool fixture_mode =
        gSavedSettings.getBOOL(prefix + "FixtureMode");
    sync_value(mFixtureMode, fixture_mode);
    sync_value(mFixturePreset,
               gSavedSettings.getS32(prefix + "FixturePreset"));
    sync_value(mFixtureKelvin,
               gSavedSettings.getF32(prefix + "Kelvin"));
    sync_value(mFixtureSourceSize,
               gSavedSettings.getF32(prefix + "SourceSizeM"));
    for (S32 slot = 0;
         slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        sync_value(mFixtureGelSlots[slot], gSavedSettings.getS32(
            prefix + "GelSlot" + std::to_string(slot)));
        mFixtureGelSlots[slot]->setEnabled(fixture_mode);
    }
    mFixtureKelvin->setEnabled(fixture_mode);
    mFixtureSourceSize->setEnabled(fixture_mode);
}

void ALPanelCineLightRig::onFixtureRoleCommit()
{
    mFixtureRoleIndex = std::clamp(
        mFixtureRole->getValue().asInteger(),
        0, ALCineLightRigModel::LIGHT_COUNT - 1);
    syncFixtureControls(true);
    syncGoboLibrary(true);
}

void ALPanelCineLightRig::onFixtureModeCommit()
{
    gSavedSettings.setBOOL(
        fixtureSettingPrefix() + "FixtureMode",
        mFixtureMode->getValue().asBoolean());
    syncFixtureControls(true);
}

void ALPanelCineLightRig::onFixturePresetCommit()
{
    const S32 index = std::clamp(
        mFixturePreset->getValue().asInteger(),
        0, ALCineLightRigModel::FIXTURE_PRESET_COUNT - 1);
    const ALCineLightRigModel::FixturePreset& preset =
        ALCineLightRigModel::fixturePreset(index);
    const std::string prefix = fixtureSettingPrefix();
    gSavedSettings.setBOOL(prefix + "FixtureMode", true);
    gSavedSettings.setS32(prefix + "FixturePreset", index);
    gSavedSettings.setF32(prefix + "Kelvin", preset.mKelvin);
    gSavedSettings.setF32(prefix + "SourceSizeM", preset.mSourceSizeM);
    gSavedSettings.setS32(prefix + "Beam", preset.mBeam);
    for (S32 slot = 0;
         slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        gSavedSettings.setS32(
            prefix + "GelSlot" + std::to_string(slot),
            preset.mSuggestedGels[slot]);
    }
    syncFixtureControls(true);
}

void ALPanelCineLightRig::onFixtureValuesCommit()
{
    const std::string prefix = fixtureSettingPrefix();
    gSavedSettings.setF32(
        prefix + "Kelvin",
        static_cast<F32>(mFixtureKelvin->getValue().asReal()));
    gSavedSettings.setF32(
        prefix + "SourceSizeM",
        static_cast<F32>(mFixtureSourceSize->getValue().asReal()));
    for (S32 slot = 0;
         slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        gSavedSettings.setS32(
            prefix + "GelSlot" + std::to_string(slot),
            mFixtureGelSlots[slot]->getValue().asInteger());
    }
}

void ALPanelCineLightRig::syncGoboLibrary(bool force)
{
    if (!mGoboLibrary || !mGoboPreview || !mGoboSoftness)
    {
        return;
    }
    const std::string prefix = fixtureSettingPrefix();
    const S32 index = std::clamp(
        gSavedSettings.getS32(prefix + "Gobo"),
        0, ALCineLightRigModel::GOBO_COUNT - 1);
    if (force || (!mGoboLibrary->hasFocus() &&
                  !gFocusMgr.childHasKeyboardFocus(mGoboLibrary)))
    {
        mGoboLibrary->setValue(index);
    }
    mGoboPreview->setVisible(index > 0);
    if (index > 0)
    {
        mGoboPreview->setImage(LLUI::getUIImage(llformat("CineGobo%02d", index)));
    }
    const F32 softness = gSavedSettings.getBOOL(prefix + "FixtureMode")
        ? ALCineLightRigModel::penumbraSoftness(
              gSavedSettings.getF32(prefix + "SourceSizeM"),
              gSavedSettings.getF32("CineLightRigRadius"))
        : gSavedSettings.getF32(prefix + "ShadowSoft");
    static const char* const bucket_names[] = { "sharp", "medium", "heavy" };
    mGoboSoftness->setText(llformat(
        "%s variant (softness %.2f)",
        bucket_names[ALCineLightRigModel::goboBlurBucket(softness)], softness));
}

void ALPanelCineLightRig::onGoboLibraryCommit()
{
    gSavedSettings.setS32(
        fixtureSettingPrefix() + "Gobo",
        std::clamp(mGoboLibrary->getValue().asInteger(),
                   0, ALCineLightRigModel::GOBO_COUNT - 1));
    syncGoboLibrary(true);
}

void ALPanelCineLightRig::updateAnchorList()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    const LLUUID subject_ids[5] = {
        LLUUID::null, cast.getSubjectA(), cast.getSubjectB(),
        cast.getSubjectC(), cast.getSubjectD()
    };
    const char* const subject_names[5] = {
        "You", "Subject A", "Subject B", "Subject C", "Subject D"
    };
    std::vector<LLUUID> ids;
    std::vector<std::string> labels;
    ids.reserve(5);
    labels.reserve(5);
    for (S32 i = 0; i < 5; ++i)
    {
        const ALCineLightRigManager::Slot slot =
            static_cast<ALCineLightRigManager::Slot>(i);
        std::string label(subject_names[i]);
        if (i > 0)
        {
            std::string resolved_name("unset");
            if (subject_ids[i].notNull())
            {
                const LLDirectorCast::CastMember* member =
                    cast.getMember(subject_ids[i]);
                resolved_name = member && !member->mLastName.empty()
                    ? member->mLastName : subject_ids[i].asString();
            }
            label += " - " + resolved_name;
        }
        if (manager.at(slot).getObjectTarget().notNull())
        {
            label += " (object)";
        }
        if (manager.isSlotLit(slot))
        {
            label = "\xE2\x97\x8F " + label; // filled circle: lit
        }
        else if (manager.isSlotEnabled(slot))
        {
            label = "\xE2\x97\x8B " + label; // hollow circle: enabled/dark
        }
        ids.push_back(subject_ids[i]);
        labels.push_back(label);
    }
    const bool changed = !mCastListInitialized || ids != mCastIds ||
                         labels != mCastNames;
    if (!changed)
    {
        syncAnchorSelection();
        return;
    }

    if (mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }

    mCastIds = ids;
    mCastNames = labels;
    mAnchorCombo->clearRows();
    for (S32 i = 0; i < 5; ++i)
    {
        mAnchorCombo->add(labels[i], LLSD(i));
    }
    mCastListInitialized = true;
    mAnchorSelectionInitialized = false;
    syncAnchorSelection();
}

void ALPanelCineLightRig::syncAnchorSelection()
{
    const S32 selected = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    if ((mAnchorSelectionInitialized && selected == mDisplayedSlot) ||
        mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }
    mAnchorCombo->setValue(selected);
    mDisplayedSlot = selected;
    mAnchorSelectionInitialized = true;
    mDisplayedGroupEnabled = false;
    mDisplayedGroupSlots = ~0u;
    mDisplayedResolvedSlots = ~0u;
    mDisplayedObjectTargetSlot = -1;
    mSeedInitialized = false;
    mShadowHintState = -1;
    mRadiusCueInitialized = false;
}

void ALPanelCineLightRig::onAnchorSelected()
{
    if (mAnchorCombo)
    {
        const S32 value = mAnchorCombo->getSelectedValue().asInteger();
        if (value < 0 || value >= ALCineLightRigManager::SLOT_COUNT)
        {
            return;
        }
        ALCineLightRigManager::instance().setSelectedSlot(
            static_cast<ALCineLightRigManager::Slot>(value));
        mDisplayedSlot = value;
        mAnchorSelectionInitialized = true;
        mDisplayedGroupEnabled = false;
        mDisplayedGroupSlots = ~0u;
        mDisplayedResolvedSlots = ~0u;
        mDisplayedObjectTargetSlot = -1;
        mSeedInitialized = false;
        mShadowHintState = -1;
        mRadiusCueInitialized = false;
        syncObjectTargetControls(true);
    }
}

void ALPanelCineLightRig::syncObjectTargetControls(bool force)
{
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    const S32 selected = static_cast<S32>(manager.selectedSlot());
    const LLUUID& target = manager.selected().getObjectTarget();
    if (!force && selected == mDisplayedObjectTargetSlot &&
        target == mDisplayedObjectTarget)
    {
        return;
    }

    mDisplayedObjectTargetSlot = selected;
    mDisplayedObjectTarget = target;
    mObjectTargetClear->setEnabled(target.notNull());
    if (target.isNull())
    {
        mObjectTargetStatus->setText(
            std::string("\xE2\x80\x94 avatar \xE2\x80\x94"));
        mObjectTargetStatus->setToolTip(
            std::string("This slot follows its avatar anchor."));
    }
    else
    {
        const std::string key = target.asString();
        mObjectTargetStatus->setText("object " + key.substr(0, 8));
        mObjectTargetStatus->setToolTip(key);
    }
}

void ALPanelCineLightRig::onTargetObject()
{
    LLObjectSelectionHandle selection =
        LLSelectMgr::getInstance()->getSelection();
    LLViewerObject* object = selection->getPrimaryObject();
    if (!object)
    {
        mObjectTargetStatus->setText(std::string("select an object first"));
        mObjectTargetStatus->setToolTip(
            std::string("Select an in-world prim or linkset, then try again."));
        return;
    }

    LLViewerObject* root = object->getRootEdit();
    if (!root || object->isAvatar() || root->isAvatar() ||
        object->isAttachment() || root->isAttachment())
    {
        mObjectTargetStatus->setText(std::string("use an in-world object"));
        mObjectTargetStatus->setToolTip(std::string(
            "Avatars and worn attachments cannot be object targets."));
        return;
    }

    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    rig.setObjectTarget(root->getID());
    gSavedSettings.setString(
        "CineLightRigObjectTarget", root->getID().asString());
    syncObjectTargetControls(true);
    syncGroupControls();
    updateAnchorList();
}

void ALPanelCineLightRig::onClearObjectTarget()
{
    ALCineLightRigManager::instance().selected().setObjectTarget(LLUUID::null);
    gSavedSettings.setString("CineLightRigObjectTarget", std::string());
    syncObjectTargetControls(true);
    syncGroupControls();
    updateAnchorList();
}

void ALPanelCineLightRig::onGroupEnabledCommit()
{
    ALCineLightRigManager::instance().selected().setGroupEnabled(
        mGroupEnable->getValue().asBoolean());
    syncGroupControls();
}

void ALPanelCineLightRig::onGroupSlotsCommit()
{
    U32 slots = 0;
    for (S32 i = 0; i < 5; ++i)
    {
        if (mGroupSlotChecks[i]->getValue().asBoolean())
        {
            slots |= 1u << i;
        }
    }
    ALCineLightRigManager::instance().selected().setGroupSlots(slots);
    syncGroupControls();
}

void ALPanelCineLightRig::syncGroupControls()
{
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const bool enabled = rig.isGroupEnabled();
    const U32 slots = rig.getGroupSlots();
    const U32 resolved = rig.lastResolvedGroupSlots();
    const LLUUID& object_target = rig.getObjectTarget();
    if (enabled == mDisplayedGroupEnabled &&
        slots == mDisplayedGroupSlots &&
        resolved == mDisplayedResolvedSlots &&
        object_target == mDisplayedGroupObjectTarget)
    {
        return;
    }

    mGroupEnable->setValue(enabled);
    mGroupEnable->setEnabled(object_target.isNull());
    for (S32 i = 0; i < 5; ++i)
    {
        mGroupSlotChecks[i]->setValue((slots & (1u << i)) != 0);
        mGroupSlotChecks[i]->setEnabled(enabled && object_target.isNull());
    }

    if (object_target.notNull())
    {
        mGroupStatus->setText(std::string("object: group is avatar-only"));
        mGroupStatus->setColor(mRadiusDefaultColor);
    }
    else if (!enabled)
    {
        mGroupStatus->setText(std::string());
        mGroupStatus->setColor(mRadiusDefaultColor);
    }
    else
    {
        S32 requested_count = 0;
        S32 resolved_count = 0;
        for (S32 i = 0; i < 5; ++i)
        {
            requested_count += (slots & (1u << i)) ? 1 : 0;
            resolved_count += (resolved & (1u << i)) ? 1 : 0;
        }
        if (resolved_count == 0)
        {
            mGroupStatus->setText(std::string("none resolved"));
            mGroupStatus->setColor(
                LLUIColor(LLColor4(1.f, 0.75f, 0.25f, 1.f)));
        }
        else
        {
            mGroupStatus->setText(llformat(
                "%d of %d lit", resolved_count, requested_count));
            mGroupStatus->setColor(mRadiusDefaultColor);
        }
    }
    mDisplayedGroupEnabled = enabled;
    mDisplayedGroupSlots = slots;
    mDisplayedResolvedSlots = resolved;
    mDisplayedGroupObjectTarget = object_target;
}

void ALPanelCineLightRig::adjustAim(const std::string& setting, F32 delta)
{
    F32 value = gSavedSettings.getF32(setting) + delta;
    if (setting == "CineLightRigOrbitYaw")
    {
        value = ALCineLightRigModel::wrap180(value);
    }
    else
    {
        value = std::clamp(value,
            -ALCineLightRigModel::PITCH_LIMIT_DEG,
             ALCineLightRigModel::PITCH_LIMIT_DEG);
    }
    gSavedSettings.setF32(setting, value);
}

void ALPanelCineLightRig::resetAim()
{
    gSavedSettings.setBOOL("CineLightRigMirror", false);
    gSavedSettings.setF32("CineLightRigOrbitYaw", 0.f);
    gSavedSettings.setF32("CineLightRigOrbitPitch", 0.f);
}

void ALPanelCineLightRig::refreshSetupList(const std::string& select_name,
                                           bool allow_empty, bool force)
{
    if (!mSetupCombo)
    {
        return;
    }
    if (!force && (mSetupCombo->hasFocus() ||
                   gFocusMgr.childHasKeyboardFocus(mSetupCombo)))
    {
        mPendingSetupSelection = select_name;
        mPendingSetupAllowEmpty = allow_empty;
        mSetupRefreshPending = true;
        return;
    }

    mSetupRefreshPending = false;
    mSetupCombo->clearRows();
    addLabeledSeparator(
        mSetupCombo, ALCineLightRig::BUILT_IN_SETUP_CAPTION, false);
    bool added_genre_caption = false;
    bool added_local_caption = false;
    for (const ALCineLightRig::SetupEntry& entry :
         ALCineLightRigManager::instance().selected().setupNamesGrouped())
    {
        if (entry.mMaster && entry.mGenre && !added_genre_caption)
        {
            addLabeledSeparator(
                mSetupCombo, ALCineLightRig::GENRE_SETUP_CAPTION, true);
            added_genre_caption = true;
        }
        if (!entry.mMaster && !added_local_caption)
        {
            addLabeledSeparator(
                mSetupCombo, ALCineLightRig::LOCAL_SETUP_CAPTION, true);
            added_local_caption = true;
        }
        LLScrollListItem* item = mSetupCombo->add(
            entry.mName, LLSD(entry.mName));
        if (entry.mMaster && item)
        {
            const ALCineLightRig::MasterSetup* master =
                ALCineLightRig::findMasterSetup(entry.mName);
            if (master && !master->mIntent.empty() && item->getColumn(0))
            {
                item->getColumn(0)->setToolTip(master->mIntent);
            }
        }
    }
    if (!select_name.empty() &&
        mSetupCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    if (allow_empty)
    {
        mSetupCombo->clear();
    }
    else
    {
        mSetupCombo->setSelectedByValue(
            ALCineLightRig::masterSetups().front().mName, true);
    }
}

//static
void ALPanelCineLightRig::refreshAllSetupLists(
    ALPanelCineLightRig* acting_panel,
    const std::string& acting_selection,
    const std::string& deleted_name)
{
    for (ALPanelCineLightRig* panel : LIVE_PANELS)
    {
        if (panel)
        {
            std::string selection = panel->mSetupCombo
                ? panel->mSetupCombo->getSelectedValue().asString()
                : std::string();
            if (panel == acting_panel)
            {
                selection = acting_selection;
            }
            if (!deleted_name.empty() && selection == deleted_name)
            {
                selection.clear();
            }
            panel->refreshSetupList(selection, true,
                                    panel == acting_panel);
        }
    }
}

void ALPanelCineLightRig::onSetupSelected()
{
    const std::string name = mSetupCombo->getSelectedValue().asString();
    if (!name.empty() &&
        ALCineLightRigManager::instance().selected().loadSetup(name))
    {
        mSetupCombo->setValue(name);
        syncEasyModeForSelected(true);
    }
}

void ALPanelCineLightRig::onFlarePresetSelected(LLComboBox* source)
{
    if (!source)
    {
        return;
    }
    const S32 index = source->getSelectedValue().asInteger();
    constexpr S32 FLARE_PRESET_COUNT =
        static_cast<S32>(sizeof(FLARE_PRESETS) / sizeof(FLARE_PRESETS[0]));
    // 0 is the sentinel ("Choose preset..." / manual sliders) — nothing to
    // stamp. Guard the upper bound too in case the combo ever desyncs from
    // the table.
    if (index <= 0 || index > FLARE_PRESET_COUNT)
    {
        return;
    }
    applyFlarePreset(FLARE_PRESETS[index - 1]);
    gSavedSettings.setS32("RenderCineLensFlarePreset", index);
    // Snap both the main-panel combo and the Easy card's shortcut copy back
    // to the sentinel so each always reads as an action ("apply this
    // look"), not a persistent mode — matches the gaze preset boxes' pick ->
    // stamp -> reset-to-sentinel pattern.
    if (mFlarePreset)
    {
        mFlarePreset->setValue(0);
    }
    if (mEasyFlarePreset)
    {
        mEasyFlarePreset->setValue(0);
    }
}

void ALPanelCineLightRig::onNightMaskPresetSelected(LLComboBox* source)
{
    if (!source)
    {
        return;
    }
    const S32 index = source->getSelectedValue().asInteger();
    constexpr S32 NIGHT_MASK_PRESET_COUNT =
        static_cast<S32>(sizeof(NIGHT_MASK_PRESETS) / sizeof(NIGHT_MASK_PRESETS[0]));
    // 0 is the sentinel ("Choose preset..." / manual sliders) — nothing to
    // stamp. Guard the upper bound too in case the combo ever desyncs from
    // the table.
    if (index <= 0 || index > NIGHT_MASK_PRESET_COUNT)
    {
        return;
    }
    applyNightMaskPreset(NIGHT_MASK_PRESETS[index - 1]);
    // m3: snap BOTH the Advanced-card combo and the Easy card's shortcut copy
    // back to the sentinel — a preset is an action ("apply this look"), not a
    // persistent mode. Enable is untouched: the checkbox is what turns Night
    // Mask on.
    if (mNightMaskPreset)
    {
        mNightMaskPreset->setValue(0);
    }
    if (mEasyNightMaskPreset)
    {
        mEasyNightMaskPreset->setValue(0);
    }
}

void ALPanelCineLightRig::saveSetup()
{
    std::string name = mSetupCombo ? mSetupCombo->getSimple() : std::string();
    LLStringUtil::trim(name);
    if (name.empty() || ALCineLightRig::isMasterSetup(name) ||
        ALCineLightRig::isSetupDecorationName(name))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Enter a unique setup name before saving. Built-in setups cannot be overwritten."));
        return;
    }
    if (!ALCineLightRigManager::instance().selected().saveSetup(name))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Could not write the cinematic light setup to disk."));
        return;
    }
    refreshAllSetupLists(this, name);
}

void ALPanelCineLightRig::deleteSetup()
{
    const std::string name = mSetupCombo
        ? mSetupCombo->getSelectedValue().asString() : std::string();
    if (name.empty() || ALCineLightRig::isMasterSetup(name) ||
        ALCineLightRig::isSetupDecorationName(name))
    {
        return;
    }
    LLHandle<ALPanelCineLightRig> handle =
        getDerivedHandle<ALPanelCineLightRig>();
    LLNotificationsUtil::add(
        "GenericAlertYesCancel",
        LLSD().with("MESSAGE", "Delete cinematic light setup '" + name + "'?"),
        LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineLightRig* self = handle.get())
            {
                self->deleteSetupCallback(notification, response, name);
            }
        });
}

bool ALPanelCineLightRig::deleteSetupCallback(
    const LLSD& notification, const LLSD& response, const std::string name)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
    {
        if (!ALCineLightRigManager::instance().selected().deleteSetup(name))
        {
            LLNotificationsUtil::add(
                "GenericAlert",
                LLSD().with("MESSAGE",
                    "Could not delete the cinematic light setup from disk."));
            return false;
        }
        const std::string selection = mSetupCombo
            ? mSetupCombo->getSelectedValue().asString() : std::string();
        refreshAllSetupLists(this, selection, name);
    }
    return false;
}

void ALPanelCineLightRig::resetAll()
{
    LLHandle<ALPanelCineLightRig> handle =
        getDerivedHandle<ALPanelCineLightRig>();
    LLNotificationsUtil::add(
        "GenericAlertYesCancel",
        LLSD().with("MESSAGE",
            "Reset every Cinematic Light Rig setting to its default?"),
        LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (ALPanelCineLightRig* self = handle.get())
            {
                self->resetAllCallback(notification, response);
            }
        });
}

bool ALPanelCineLightRig::resetAllCallback(
    const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }
    for (const std::string& setting : settings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(setting))
        {
            control->resetToDefault(true);
        }
    }
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    ALCineLightRig& rig = manager.selected();
    rig.setAnchor(LLUUID::null);
    rig.setObjectTarget(LLUUID::null);
    rig.setGroupEnabled(false);
    rig.setGroupSlots(0);
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        rig.setShaftEnabled(i, false);
        rig.setHeroEnabled(i, false);
    }
    manager.resetAllToSelf();
    mAnchorSelectionInitialized = false;
    mDisplayedSlot = -1;
    mDisplayedObjectTargetSlot = -1;
    mDisplayedGroupSlots = ~0u;
    mDisplayedResolvedSlots = ~0u;
    mSeedInitialized = false;
    mDisplayedFixtureSlot = -1;
    syncObjectTargetControls(true);
    refreshAllSetupLists(
        this, ALCineLightRig::masterSetups().front().mName);
    return false;
}

void ALPanelCineLightRig::commitSeed()
{
    const std::string text = mSeedEditor->getText();
    U32 seed = 0;
    const bool digits_only = !text.empty() && std::all_of(
        text.begin(), text.end(), [](char character)
        {
            return character >= '0' && character <= '9';
        });
    if (!digits_only || !LLStringUtil::convertToU32(text, seed) || seed == 0)
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE",
                "Seed must be an integer from 1 through 4294967295."));
        mSeedInitialized = false;
        syncSeedEditor(true);
        return;
    }
    gSavedSettings.setU32("CineLightRigSeed", seed);
    mSeedEditor->setText(llformat("%u", seed));
    mDisplayedSeed = seed;
    mSeedInitialized = true;
}

void ALPanelCineLightRig::randomizeSeed()
{
    U32 seed = LLUUID::generateNewID().getCRC32();
    if (seed == 0)
    {
        seed = 1;
    }
    gSavedSettings.setU32("CineLightRigSeed", seed);
    mSeedInitialized = false;
    syncSeedEditor(true);
}

void ALPanelCineLightRig::syncSeedEditor(bool force)
{
    const U32 seed = gSavedSettings.getU32("CineLightRigSeed");
    if ((mSeedInitialized && seed == mDisplayedSeed) ||
        (!force && (mSeedEditor->hasFocus() ||
                    gFocusMgr.childHasKeyboardFocus(mSeedEditor))))
    {
        return;
    }
    mSeedEditor->setText(llformat("%u", seed));
    mDisplayedSeed = seed;
    mSeedInitialized = true;
}

S32 ALPanelCineLightRig::computeRequestedShadowSlots() const
{
    return static_cast<S32>(
        ALCineLightRigManager::instance().requestedShadowSlots(
            LLPipeline::MAX_SPOT_SHADOWS));
}

bool ALPanelCineLightRig::selectedIsEasyNative() const
{
    if (std::fabs(gSavedSettings.getF32("CineLightRigKeyEV")) > 0.01f)
    {
        return false;
    }

    const auto is_bucket = [](F32 ev, bool rim)
    {
        for (S32 presence = 1; presence < 4; ++presence)
        {
            const F32 bucket = rim
                ? ALCineLightRigModel::easyRimEV(presence)
                : ALCineLightRigModel::easyBgEV(presence);
            if (std::fabs(ev - bucket) <= 0.01f)
            {
                return true;
            }
        }
        return false;
    };

    return (!gSavedSettings.getBOOL("CineLightRigRimOn") ||
            is_bucket(gSavedSettings.getF32("CineLightRigRimEV"), true)) &&
           (!gSavedSettings.getBOOL("CineLightRigBgOn") ||
            is_bucket(gSavedSettings.getF32("CineLightRigBgEV"), false));
}

bool ALPanelCineLightRig::normalizeSelectedForEasy(bool explicit_entry)
{
    // Fold the key's own EV into Master so subject exposure is preserved while
    // Key is anchored at 0. If the folded value cannot be represented within
    // Master EV's +/-16 range, refuse the fold and leave the rig for Advanced
    // (render would otherwise clamp Master and silently drop exposure).
    const F32 key_ev = gSavedSettings.getF32("CineLightRigKeyEV");
    const F32 folded = gSavedSettings.getF32("CineLightRigMasterEV") + key_ev;
    if (std::fabs(folded) > 16.f + 0.01f)
    {
        return false;
    }
    gSavedSettings.setF32("CineLightRigMasterEV", folded);
    gSavedSettings.setF32("CineLightRigKeyEV", 0.f);
    gSavedSettings.setBOOL("CineLightRigKeyOn", true);

    // Rim/Bg EVs must sit exactly on the Easy presence buckets or the
    // per-frame syncEasyModeForSelected() re-check (selectedIsEasyNative)
    // vetoes the mode right back off - which made Easy unenterable while any
    // authored look with free-form EVs was active. Only an EXPLICIT toggle
    // click may snap: the passive sync path reaches here with EVs already
    // within selectedIsEasyNative()'s 0.01 tolerance, and rewriting a
    // near-bucket authored EV to the exact bucket there would silently
    // mutate looks on first show / slot switch / scene load.
    if (!explicit_entry)
    {
        return true;
    }
    const auto snap_to_bucket = [](const char* setting, bool rim)
    {
        const F32 ev = gSavedSettings.getF32(setting);
        S32 best = 1;
        F32 best_dist = F32_MAX;
        for (S32 presence = 1; presence < 4; ++presence)
        {
            const F32 bucket = rim
                ? ALCineLightRigModel::easyRimEV(presence)
                : ALCineLightRigModel::easyBgEV(presence);
            const F32 dist = std::fabs(ev - bucket);
            if (dist < best_dist)
            {
                best_dist = dist;
                best = presence;
            }
        }
        gSavedSettings.setF32(setting,
            rim ? ALCineLightRigModel::easyRimEV(best)
                : ALCineLightRigModel::easyBgEV(best));
    };
    if (gSavedSettings.getBOOL("CineLightRigRimOn"))
    {
        snap_to_bucket("CineLightRigRimEV", true);
    }
    if (gSavedSettings.getBOOL("CineLightRigBgOn"))
    {
        snap_to_bucket("CineLightRigBgEV", false);
    }
    return true;
}

void ALPanelCineLightRig::syncEasyModeForSelected(bool force)
{
    const S32 selected = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    // Recompute the desired mode on every call, not just on a slot change: a
    // Director scene or preset can replace the selected slot's settings in
    // place. If that makes an active-Easy instance no longer Easy-native (e.g.
    // an old scene restoring a non-zero Key EV), we must demote to Advanced so
    // the disabled EV/Ratio controls re-enable and a later Brightness commit
    // does not zero Key EV without folding it into Master.
    const bool want_active =
        gSavedSettings.getBOOL("CineLightRigEasyMode") &&
        selectedIsEasyNative();
    if (!force && selected == mEasyModeSlot && want_active == mEasyModeActive)
    {
        return;
    }

    mEasyModeSlot = selected;
    mEasyModeActive = want_active;
    if (mEasyModeActive)
    {
        // First show and an instance switch both enter Easy only for an
        // already Easy-native instance (Key EV ~ 0), so this fold is a no-op
        // here; explicit entry via the toggle is what folds a real Key EV.
        // If it somehow cannot be represented, stay in Advanced.
        if (!normalizeSelectedForEasy(false))
        {
            mEasyModeActive = false;
        }
    }
}

void ALPanelCineLightRig::onEasyModeCommit()
{
    if (mSyncingEasyControls)
    {
        return;
    }
    const bool desired = mEasyModeToggle->getValue().asBoolean();
    // Record the user's preference even if this particular look cannot enter
    // Easy, so other (Easy-native) instances still open in Easy.
    gSavedSettings.setBOOL("CineLightRigEasyMode", desired);
    mEasyModeSlot = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    mEasyModeActive = desired;
    if (mEasyModeActive && !normalizeSelectedForEasy(true))
    {
        // Exposure would exceed Master EV's range (an extreme Advanced look);
        // keep this instance in Advanced. syncEasyControls reflects it.
        mEasyModeActive = false;
    }
    syncEasyControls();
}

void ALPanelCineLightRig::onEasyBrightnessCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setF32(
        "CineLightRigMasterEV",
        ALCineLightRigModel::easyBrightnessClamp(
            static_cast<F32>(mEasyBrightness->getValue().asReal())));
    gSavedSettings.setF32("CineLightRigKeyEV", 0.f);
    gSavedSettings.setBOOL("CineLightRigKeyOn", true);
}

void ALPanelCineLightRig::onEasyDramaCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setBOOL("CineLightRigRatioLock", true);
    gSavedSettings.setF32(
        "CineLightRigRatio",
        ALCineLightRigModel::easyDramaClamp(
            static_cast<F32>(mEasyDrama->getValue().asReal())));
    gSavedSettings.setBOOL("CineLightRigFillOn", true);
}

void ALPanelCineLightRig::onEasyRimCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    const S32 presence = std::clamp(
        mEasyRim->getValue().asInteger(), 0, 3);
    const bool on = ALCineLightRigModel::easyRimOn(presence);
    gSavedSettings.setBOOL("CineLightRigRimOn", on);
    if (on)
    {
        gSavedSettings.setF32(
            "CineLightRigRimEV",
            ALCineLightRigModel::easyRimEV(presence));
    }
}

void ALPanelCineLightRig::onEasyBgCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    const S32 presence = std::clamp(
        mEasyBg->getValue().asInteger(), 0, 3);
    const bool on = ALCineLightRigModel::easyBgOn(presence);
    gSavedSettings.setBOOL("CineLightRigBgOn", on);
    if (on)
    {
        gSavedSettings.setF32(
            "CineLightRigBgEV",
            ALCineLightRigModel::easyBgEV(presence));
    }
}

void ALPanelCineLightRig::onEasyWarmthCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setF32(
        "CineLightRigMasterTempMired",
        std::clamp(static_cast<F32>(mEasyWarmth->getValue().asReal()),
                   ALCineLightRigModel::MASTER_TEMP_MIRED_MIN,
                   ALCineLightRigModel::MASTER_TEMP_MIRED_MAX));
}

void ALPanelCineLightRig::onEasyConeWidthCommit()
{
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    const S32 beam = ALCineLightRigModel::easyConeWidthToBeam(
        mEasyConeWidth->getValue().asInteger());
    static constexpr const char* BEAM_SETTINGS[] = {
        "CineLightRigKeyBeam", "CineLightRigFillBeam",
        "CineLightRigRimBeam", "CineLightRigBgBeam",
    };
    for (const char* setting : BEAM_SETTINGS)
    {
        gSavedSettings.setS32(setting, beam);
    }
}

void ALPanelCineLightRig::onEasyConeFeatherCommit()
{
    // Guarded like the other Easy controls so the slider is inert (no write to
    // the global projector feather) whenever Easy mode is not active.
    if (mSyncingEasyControls || !mEasyModeActive)
    {
        return;
    }
    gSavedSettings.setF32(
        "BDMergeProjectorVolumetricsFeather",
        std::clamp(static_cast<F32>(mEasyConeFeather->getValue().asReal()),
                   0.f, 0.5f));
}

void ALPanelCineLightRig::onManualConeWidthCommit(S32 light)
{
    if (mSyncingEasyControls || light < 0 ||
        light >= ALCineLightRigModel::LIGHT_COUNT)
    {
        return;
    }
    const S32 beam = ALCineLightRigModel::easyConeWidthToBeam(
        mManualConeWidths[light]->getValue().asInteger());
    gSavedSettings.setS32(
        std::string("CineLightRig") + ROLE_NAMES[light] + "Beam", beam);
}

void ALPanelCineLightRig::onManualCommit()
{
    // The check box is control_name-bound, so CineLightRigManual is already
    // updated; just apply the side effects (force FX off + lock the selector).
    applyManualLock();
}

void ALPanelCineLightRig::applyManualLock()
{
    const bool manual = gSavedSettings.getBOOL("CineLightRigManual");
    if (manual && gSavedSettings.getS32("CineLightRigFX") != -1)
    {
        // Manual owns the pose; drop any running effect and hold it off.
        gSavedSettings.setS32("CineLightRigFX", -1);
    }
    // Lock the effect selector while manual so an effect cannot silently take
    // over per-light aim/exposure/colour again.
    mFXCombo->setEnabled(!manual);
    getChild<LLUICtrl>("cine_fx_stop")->setEnabled(!manual);
}

void ALPanelCineLightRig::syncEasyControls()
{
    mSyncingEasyControls = true;
    mEasyModeToggle->setValue(mEasyModeActive);

    const auto set_unfocused = [](LLUICtrl* control, const LLSD& value)
    {
        if (!control->hasFocus() &&
            !gFocusMgr.childHasKeyboardFocus(control))
        {
            control->setValue(value);
        }
    };
    set_unfocused(mEasyBrightness, LLSD(
        ALCineLightRigModel::easyBrightnessClamp(
            gSavedSettings.getF32("CineLightRigMasterEV"))));
    const F32 drama = gSavedSettings.getBOOL("CineLightRigRatioLock")
        ? gSavedSettings.getF32("CineLightRigRatio")
        : gSavedSettings.getF32("CineLightRigKeyEV") -
          gSavedSettings.getF32("CineLightRigFillEV");
    set_unfocused(mEasyDrama, LLSD(
        ALCineLightRigModel::easyDramaClamp(drama)));
    set_unfocused(mEasyRim, LLSD(
        ALCineLightRigModel::rimPresenceFromEV(
            gSavedSettings.getBOOL("CineLightRigRimOn"),
            gSavedSettings.getF32("CineLightRigRimEV"))));
    set_unfocused(mEasyBg, LLSD(
        ALCineLightRigModel::bgPresenceFromEV(
            gSavedSettings.getBOOL("CineLightRigBgOn"),
            gSavedSettings.getF32("CineLightRigBgEV"))));
    set_unfocused(mEasyWarmth, LLSD(
        gSavedSettings.getF32("CineLightRigMasterTempMired")));
    set_unfocused(mEasyConeWidth, LLSD(
        ALCineLightRigModel::easyConeWidthFromBeam(
            gSavedSettings.getS32("CineLightRigKeyBeam"))));
    set_unfocused(mEasyConeFeather, LLSD(
        gSavedSettings.getF32("BDMergeProjectorVolumetricsFeather")));
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        set_unfocused(mManualConeWidths[light], LLSD(
            ALCineLightRigModel::easyConeWidthFromBeam(
                gSavedSettings.getS32(
                    std::string("CineLightRig") + ROLE_NAMES[light] +
                    "Beam"))));
        mManualConeWidths[light]->setEnabled(!mEasyModeActive);
    }

    mEasyBrightness->setEnabled(mEasyModeActive);
    mEasyDrama->setEnabled(mEasyModeActive);
    mEasyRim->setEnabled(mEasyModeActive);
    mEasyBg->setEnabled(mEasyModeActive);
    mEasyWarmth->setEnabled(mEasyModeActive);
    mEasyConeWidth->setEnabled(mEasyModeActive);
    mEasyConeFeather->setEnabled(mEasyModeActive);
    // Easy Shaft length is a direct control_name binding (no remap handler), but
    // it edits a global lever, so gate it on Easy exactly like Cone/Edge above.
    if (LLUICtrl* shaft = findChild<LLUICtrl>("cine_easy_shaft_length"))
    {
        shaft->setEnabled(mEasyModeActive);
    }
    // The Easy card's Flare preset shortcut and Live Probe row are likewise
    // direct control_name bindings / apply-and-snap-back combos with no
    // dedicated remap handler, so gate them the same way.
    if (LLUICtrl* flare = findChild<LLUICtrl>("cine_easy_flare_preset"))
    {
        flare->setEnabled(mEasyModeActive);
    }
    if (LLUICtrl* probe_enable = findChild<LLUICtrl>("cine_easy_probe_enable"))
    {
        probe_enable->setEnabled(mEasyModeActive);
    }
    if (LLUICtrl* probe_gizmo = findChild<LLUICtrl>("cine_easy_probe_gizmo"))
    {
        probe_gizmo->setEnabled(mEasyModeActive);
    }
    if (LLUICtrl* probe_radius = findChild<LLUICtrl>("cine_easy_probe_radius"))
    {
        probe_radius->setEnabled(mEasyModeActive);
    }
    // The Easy card's Night row (Enable + preset shortcut + Darkness) is
    // likewise a direct control_name binding / apply-and-snap-back combo with
    // no dedicated remap handler, so gate it the same way as Flare/Probe above.
    if (LLUICtrl* night_enable = findChild<LLUICtrl>("cine_easy_night_mask_enable"))
    {
        night_enable->setEnabled(mEasyModeActive);
    }
    if (LLUICtrl* night_preset = findChild<LLUICtrl>("cine_easy_night_mask_preset"))
    {
        night_preset->setEnabled(mEasyModeActive);
    }
    if (LLUICtrl* night_darkness = findChild<LLUICtrl>("cine_easy_night_mask_darkness"))
    {
        night_darkness->setEnabled(mEasyModeActive);
    }
    // The Easy reset buttons write the same backing settings, so gate them too;
    // otherwise a reset click would edit the light while Easy mode is off.
    static const char* const EASY_RESET_BUTTONS[] = {
        "cine_easy_brightness_reset", "cine_easy_drama_reset",
        "cine_easy_warmth_reset", "cine_easy_cone_width_reset",
        "cine_easy_cone_feather_reset", "cine_easy_shaft_length_reset",
        "cine_easy_probe_radius_reset", "cine_easy_night_mask_darkness_reset",
    };
    for (const char* button_name : EASY_RESET_BUTTONS)
    {
        if (LLUICtrl* button = findChild<LLUICtrl>(button_name))
        {
            button->setEnabled(mEasyModeActive);
        }
    }
    for (LLUICtrl* control : mAdvancedDrivenControls)
    {
        control->setEnabled(!mEasyModeActive);
    }
    mSyncingEasyControls = false;
}

void ALPanelCineLightRig::onClickShadowFixIt()
{
    const S32 requested = computeRequestedShadowSlots();
    gSavedSettings.setU32("BDMergeMaxSpotShadows",
        std::clamp(static_cast<U32>(requested), 2u, 10u));
}

void ALPanelCineLightRig::updateDerivedStatus()
{
    syncObjectTargetControls();
    syncEasyModeForSelected();
    syncEasyControls();
    applyManualLock();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const bool ratio_locked =
        gSavedSettings.getBOOL("CineLightRigRatioLock");
    mFillEV->setEnabled(!mEasyModeActive && !ratio_locked);
    if (!mFillEV->hasFocus() && !gFocusMgr.childHasKeyboardFocus(mFillEV))
    {
        const F32 displayed_fill_ev = ratio_locked
            ? gSavedSettings.getF32("CineLightRigKeyEV") -
                std::clamp(gSavedSettings.getF32("CineLightRigRatio"),
                           0.f, 5.f)
            : gSavedSettings.getF32("CineLightRigFillEV");
        mFillEV->setValue(displayed_fill_ev);
    }
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        mClipStatus[i]->setVisible(rig.isClipped(i));
        mShaftControls[i]->setValue(rig.isShaftEnabled(i));
        mHeroControls[i]->setValue(rig.isHeroEnabled(i));
    }

    const S32 mode = std::clamp(
        gSavedSettings.getS32("CineLightRigShadowMode"), 0, 2);
    const S32 requested = computeRequestedShadowSlots();
    bool shaft_suppressed = false;
    bool shaft_on_dark_light = false;
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        if (rig.isShaftEnabled(i) &&
            (mode == 0 || (mode == 1 && i != 0)))
        {
            shaft_suppressed = true;
        }
        if (rig.isShaftEnabled(i) &&
            !gSavedSettings.getBOOL(ROLE_ON_SETTINGS[i]))
        {
            shaft_on_dark_light = true;
        }
    }
    const S32 slots = gSavedSettings.getS32("BDMergeMaxSpotShadows");
    const S32 hint_state = requested > slots ? 1
        : shaft_suppressed ? 2
        : shaft_on_dark_light ? 3 : 0;
    if (hint_state != mShadowHintState ||
        (hint_state == 1 &&
         (requested != mShadowHintRequested || slots != mShadowHintSlots)))
    {
        mShadowHint->setVisible(hint_state != 0);
        mShadowFixIt->setVisible(hint_state == 1);
        if (hint_state == 1)
        {
            const S32 displayed_request = std::clamp(requested, 2, 10);
            mShadowHint->setText(llformat(
                "%d rig projectors request %d shadow slots. Raise Max Spot "
                "Shadows or use Key only; shafts also require a slot.",
                displayed_request, slots));
            mShadowFixIt->setLabel(llformat(
                "Allow %d spot shadows", displayed_request));
        }
        else if (hint_state == 2)
        {
            mShadowHint->setText(std::string(
                "A shafted light is excluded by the current shadow policy. "
                "Shafts require a shadow slot; choose All projectors "
                "compete (or Key only for KEY)."));
        }
        else if (hint_state == 3)
        {
            mShadowHint->setText(std::string(
                "A shaft is enabled on an Off light. Turn that light On before "
                "it can request the shadow slot required for its shaft."));
        }
        mShadowHintState = hint_state;
        mShadowHintRequested = requested;
        mShadowHintSlots = slots;
    }
    const bool radius_over_ceiling =
        gSavedSettings.getF32("CineLightRigRadius") >
        ALCineLightRigModel::SCALED_RADIUS_CEIL;
    if (!mRadiusCueInitialized ||
        radius_over_ceiling != mRadiusOverCeiling)
    {
        mRadiusLabel->setColor(radius_over_ceiling
            ? LLUIColor(LLColor4(1.f, 0.75f, 0.25f, 1.f))
            : mRadiusDefaultColor);
        mRadiusOverCeiling = radius_over_ceiling;
        mRadiusCueInitialized = true;
    }
    const ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    switch (manager.liveProbeState())
    {
        case ALCineLightRigManager::LiveProbeState::DISABLED:
            mLiveProbeStatus->setText(LLStringExplicit("Disabled"));
            break;
        case ALCineLightRigManager::LiveProbeState::UNAVAILABLE:
            mLiveProbeStatus->setText(
                LLStringExplicit("Unavailable: enable probe coverage and at least 2 slots"));
            break;
        case ALCineLightRigManager::LiveProbeState::WAITING_FOR_TARGET:
            mLiveProbeStatus->setText(LLStringExplicit("Waiting for the target rig"));
            break;
        case ALCineLightRigManager::LiveProbeState::WARMING:
            mLiveProbeStatus->setText(llformat("Warming... %d%%",
                ll_round(manager.liveProbeFade() * 100.f)));
            break;
        case ALCineLightRigManager::LiveProbeState::LIVE:
            mLiveProbeStatus->setText(LLStringExplicit("Live"));
            break;
    }
    if (mNightMaskStatus)
    {
        std::string night_status;
        if (gSavedSettings.getBOOL("CineLightRigNightMaskEnabled"))
        {
            // M1: Night Mask only runs inside LLPipeline::renderFinalize's
            // `if (hdr)` block — mirror that exact gate here so the status
            // line agrees with what actually renders.
            const bool hdr_active = gGLManager.mGLVersion > 4.05f &&
                gSavedSettings.getBOOL("RenderHDREnabled");
            if (!hdr_active)
            {
                night_status = "Requires HDR rendering";
            }
            else
            {
                // Same Director Cast lookup as the Target combo / M3's
                // independent anchor resolution in LLPipeline::updateNightMaskAnchor.
                LLDirectorCast& cast = LLDirectorCast::instance();
                LLVOAvatar* target_avatar = nullptr;
                switch (gSavedSettings.getS32("CineLightRigNightMaskTarget"))
                {
                    case 1: target_avatar = cast.resolveSubjectA(); break;
                    case 2: target_avatar = cast.resolveSubjectB(); break;
                    case 3: target_avatar = cast.resolveSubjectC(); break;
                    case 4: target_avatar = cast.resolveSubjectD(); break;
                    default: target_avatar = cast.resolve(LLUUID::null); break;
                }
                if (!target_avatar || target_avatar->isDead())
                {
                    night_status = "No target";
                }
            }
        }
        mNightMaskStatus->setText(LLStringExplicit(night_status));
    }
    std::string setup_name = mSetupCombo
        ? mSetupCombo->getSimple() : std::string();
    LLStringUtil::trim(setup_name);
    mSetupSave->setEnabled(!setup_name.empty() &&
                           !ALCineLightRig::isMasterSetup(setup_name) &&
                           !ALCineLightRig::isSetupDecorationName(
                               setup_name));
    const std::string selected = mSetupCombo
        ? mSetupCombo->getSelectedValue().asString() : std::string();
    mSetupDelete->setEnabled(!selected.empty() &&
                             !ALCineLightRig::isMasterSetup(selected) &&
                             !ALCineLightRig::isSetupDecorationName(selected));
}

void ALPanelCineLightRig::reshape(S32 width, S32 height, bool called_from_parent)
{
    // Cascades width (and any height delta) down to our children, including
    // the "cine_light_rig_flow_grid" flow_grid. That grid's own overridden
    // reshape() runs synchronously inside this call and repacks its cards
    // for the new width, ending with the grid's rect set to its true packed
    // content height.
    LLPanel::reshape(width, height, called_from_parent);

    // We are typically embedded directly as an LLScrollContainer's scroll
    // document (see floater_cine_light_rig.xml / floater_director.xml), and
    // LLScrollContainer reads *our* rect -- not the grid's -- every frame to
    // compute the scrollable range. Mirror the grid's natural height onto
    // ourselves so scrolling always reaches the full card layout. setRect()
    // is used instead of another reshape() so we don't re-trigger the
    // generic follows-based child cascade above and disturb the grid's
    // already-correct position.
    if (LLView* grid = findChildView("cine_light_rig_flow_grid"))
    {
        const S32 grid_height = grid->getRect().getHeight();
        if (grid_height != getRect().getHeight())
        {
            LLRect r = getRect();
            r.mTop = r.mBottom + grid_height;
            setRect(r);
        }
    }
}

void ALPanelCineLightRig::draw()
{
    updateAnchorList();
    syncGroupControls();
    if (mSetupRefreshPending &&
        !mSetupCombo->hasFocus() &&
        !gFocusMgr.childHasKeyboardFocus(mSetupCombo))
    {
        refreshSetupList(mPendingSetupSelection,
                         mPendingSetupAllowEmpty, true);
    }
    syncSeedEditor();
    syncFixtureControls();
    syncGoboLibrary();
    updateDerivedStatus();
    LLPanel::draw();
}

void ALPanelCineLightRig::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        updateAnchorList();
        syncObjectTargetControls(true);
        syncGroupControls();
        syncFixtureControls(true);
        syncGoboLibrary(true);
        refreshSetupList(mSetupCombo
            ? mSetupCombo->getSelectedValue().asString() : std::string(),
            true);
        updateDerivedStatus();
    }
    LLPanel::onVisibilityChange(new_visibility);
}
