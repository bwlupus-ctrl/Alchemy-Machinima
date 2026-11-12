/**
 * @file alpanelcinelightrig.cpp
 * @brief Shared Director/standalone cinematic light-rig panel.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alpanelcinelightrig.h"
#include "alkithistory.h"

#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "aldirectorcastui.h"
#include "alcinelightrigmodel.h"
#include "llprismlens.h"
#include "alkitbinding.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "alkitpage.h"
#include "alkitstyle.h"
#include "alkitmenu.h"
#include "alriggroupsubject.h"
#include "algazenearbyname.h"
#include "llcallbacklist.h"
#include "llmenugl.h"
#include "llviewermenu.h"
#include "aluirefresh.h" // [UIPerfC1] widget-aware value compare
#include "llbutton.h"
#include "llagent.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lldirectorcast.h"
#include "llfocusmgr.h"
#include "llfloaterreg.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llnotifications.h"
#include "llscrolllistcell.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "llviewerstats.h" // [ProbeManualRate] FPS for the Manual status line
#include "lltracerecording.h"
#include "llenvironment.h"
#include "llsettingssky.h"
#include "v3color.h"
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "pipeline.h"
#include "alkitrow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

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

// History follows the five-slot storage schema, not preset ownership. Probe,
// night mask, renderer/Rig Rim globals and manager preferences are global even
// when their names start with CineLightRig or cine_. Unknown widgets stay global.
S32 rigSlotHistoryKind(const std::string& widget)
{
    // Custom snapshots use the same lifetime context as registered entries.
    if (widget == "Light rig shafts") return 1;
    static const std::vector<std::string> slot_settings = {
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
        "CineLightRigGizmo",
        "CineLightRigRatioLock",
        "CineLightRigRatio",
        "CineLightRigCatchlight",
        "CineLightRigCatchlightEV",
        "CineLightRigCatchlightSize",
        "CineLightRigCatchlightAngle",
        "CineLightRigBounceLightsSet",
        "CineLightRigCatchlightActorOnly",
        "CineLightRigActorOnlyAll",
    };
    if (std::find(slot_settings.begin(), slot_settings.end(), widget) != slot_settings.end()) return 1;
    for (const char* role : { "Key", "Fill", "Rim", "Bg" })
    {
        for (const char* suffix : { "Yaw", "Pitch", "OffsetZ", "Profile", "EV", "Beam", "Gobo", "Gel", "ShadowSoft", "On", "Flicker", "FlickerAmount", "SwayAmount", "SwayPeriod", "FixtureMode", "Kelvin", "SourceSizeM", "FixturePreset", "DetailShadows", "WorldShadowPick", "ActorOnly" })
            if (widget == std::string("CineLightRig") + role + suffix) return 1;
        for (S32 gel = 0; gel < 3; ++gel)
            if (widget == std::string("CineLightRig") + role + "GelSlot" + std::to_string(gel)) return 1;
        for (const char* suffix : { "Gain", "Sharpness", "Wrap", "BackBias" })
            if (widget == std::string("CineRigRim") + role + suffix) return 1;
    }
    // Keep widget suffixes separate from the cine_ prefix: the kit generator
    // treats complete widget literals as typed C++ control references.
    if (widget.compare(0, 5, "cine_") != 0) return 0;
    const std::string control = widget.substr(5);
    static const std::vector<std::string> slot_widgets = {
        "actor_only_all",
        "aim_pitch_minus",
        "aim_pitch_plus",
        "aim_reset",
        "aim_yaw_minus",
        "aim_yaw_plus",
        "bounce",
        "bounce_lights_set",
        "bounce_ratio",
        "catchlight_actor_only",
        "catchlight_angle",
        "catchlight_ev",
        "catchlight_size",
        "cookie",
        "damping",
        "easy_bg",
        "easy_brightness",
        "easy_cone_width",
        "easy_drama",
        "easy_rim",
        "easy_warmth",
        "enabled",
        "fx_combo",
        "fx_stop",
        "gizmo",
        "group_state",
        "group_enable",
        "group_self", "group_a", "group_b", "group_c", "group_d",
        "light_map",
        "headroom",
        "lit_target",
        "master_ev",
        "master_temp",
        "mirror",
        "offset_z",
        "radius",
        "ratio",
        "ratio_lock",
        "seed",
        "seed_randomize",
        "shadow_mode",
        "track_mode",
        "transition",
    };
    if (std::find(slot_widgets.begin(), slot_widgets.end(), control) != slot_widgets.end()) return 1;
    static const std::vector<std::string> fixture_widgets = {
        "fixture_mode",
        "fixture_preset",
        "fixture_kelvin",
        "fixture_source_size",
        "fixture_gel_0",
        "fixture_gel_1",
        "fixture_gel_2",
        "gobo_library",
        "gobo_preview",
    };
    if (std::find(fixture_widgets.begin(), fixture_widgets.end(), control) != fixture_widgets.end()) return 2;
    // Code-built per-light rows mirror these exact per-slot fields.
    for (const char* role : { "key", "fill", "rim", "bg" })
        for (const char* suffix : { "on", "yaw", "pitch", "offset_z", "profile", "ev", "beam", "gobo", "gel", "shadow_soft", "flicker", "flicker_amount", "sway_amount", "sway_period", "detail_shadows", "world_shadow", "actor_only", "shaft", "hero", "cone_width", "projector_feed" })
            if (widget == std::string("cine_") + role + "_" + suffix) return 1;
    return 0;
}

bool rigNameVisible(const LLUUID& id)
{
    return LLDirectorCast::isSelf(id) ||
        (RlvActions::canShowName(RlvActions::SNC_DEFAULT, id) &&
         (LLDirectorCast::instance().contains(id) || ALGazeNearby::nearbyAllowed()));
}

void retireRigMenu(LLHandle<LLContextMenu>& handle)
{
    auto* menu = handle.get();
    if (!menu) return;
    menu->hide();
    // A stale callback can open another menu while this one's click is on stack.
    // The holder may also destroy it before idle, so retain only its weak handle.
    doOnIdleOneTime([handle]() { if (auto* menu = handle.get()) delete menu; });
    handle = LLHandle<LLContextMenu>();
}

void clearKitComboSelection(LLComboBox* combo)
{
    if (!combo)
        return;
    // The combo's own text-entry child (LLComboBox has no public accessor for it).
    for (LLView* child : *combo->getChildList())
        if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
            editor->deselect();
}

// [UIPerfC1] Per-role fixture settings, resolved ONCE to control pointers instead of a
// string concat + by-name gSavedSettings lookup per field per frame (syncFixtureControls,
// syncGoboLibrary, syncEasyControls all run from draw()). Indexed [role][field]; the role
// index is ALPanelCineLightRig::mFixtureRoleIndex (changes only in onFixtureRoleCommit) or
// the loop role. A pointer is only ever read through the index of ITS OWN role, so a role
// change can never leave a stale pointer behind. LLPointer keeps the variable alive.
constexpr S32 FF_MODE = 0;
constexpr S32 FF_PRESET = 1;
constexpr S32 FF_KELVIN = 2;
constexpr S32 FF_SOURCE = 3;
constexpr S32 FF_GOBO = 4;
constexpr S32 FF_SHADOWSOFT = 5;
constexpr S32 FF_BEAM = 6;
constexpr S32 FF_GEL0 = 7; // + slot
constexpr S32 FF_COUNT = FF_GEL0 + ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT;

LLPointer<LLControlVariable> sFixtureControls[ALCineLightRigModel::LIGHT_COUNT][FF_COUNT];
bool sFixtureControlsResolved = false;

void resolveFixtureControls()
{
    if (sFixtureControlsResolved)
    {
        return;
    }
    static const char* const SUFFIX[FF_GEL0] = {
        "FixtureMode", "FixturePreset", "Kelvin", "SourceSizeM",
        "Gobo", "ShadowSoft", "Beam"
    };
    bool all_found = true;
    for (S32 role = 0; role < ALCineLightRigModel::LIGHT_COUNT; ++role)
    {
        for (S32 field = 0; field < FF_COUNT; ++field)
        {
            const std::string name = std::string("CineLightRig") + ROLE_NAMES[role] +
                (field < FF_GEL0
                    ? std::string(SUFFIX[field])
                    : "GelSlot" + std::to_string(field - FF_GEL0));
            sFixtureControls[role][field] = gSavedSettings.getControl(name);
            all_found = all_found && sFixtureControls[role][field].notNull();
        }
    }
    // a missing setting is a bug; retry (and stay null-safe) rather than cache the hole
    sFixtureControlsResolved = all_found;
}

// Reads exactly what gSavedSettings.getXxx(name) would return for this control.
// False (out untouched) when the control does not exist: the caller must not write.
template <class T>
bool fixtureSetting(S32 role, S32 field, T& out)
{
    resolveFixtureControls();
    role = std::clamp(role, 0, ALCineLightRigModel::LIGHT_COUNT - 1);
    LLControlVariable* control = sFixtureControls[role][field].get();
    if (!control)
    {
        return false;
    }
    out = convert_from_llsd<T>(control->get(), control->type(), control->getName());
    return true;
}

void addLabeledSeparator(LLComboBox* combo, const std::string& label,
                         bool add_separator)
{
    if (add_separator)
    {
        combo->addSeparator(ADD_BOTTOM);
    }
    combo->add(label, LLSD(), ADD_BOTTOM, false);
}

// [LiveProbeRefresh] Volatile half of the status. The mode/config text lives
// in the word-wrapped static box; this single-line readout is clocked at 2 Hz.
std::string liveProbeRefreshLine(
    const LLReflectionMapManager::CinematicRefreshStatus& rs)
{
    using ALCineLiveProbeRefresh::Mode;
    using ALCineLiveProbeRefresh::Path;
    using ALCineLiveProbeRefresh::Reason;
    switch (rs.mMode)
    {
        case Mode::EVERY_FRAME:
            return "Refresh: Every frame";
        case Mode::BALANCED:
        case Mode::ECONOMY:
        {
            const char* const label =
                rs.mMode == Mode::BALANCED ? "Balanced" : "Economy";
            if (!rs.mStepping || rs.mPath == Path::FULL)
            {
                return llformat("Refresh: %s, warming up", label);
            }
            return llformat("Refresh: %s, %s %d/6", label,
                rs.mPassIsRadiance ? "radiance" : "irradiance",
                rs.mPassFace >= 0 ? rs.mPassFace : 6);
        }
        case Mode::MANUAL:
        {
            // [ProbeManualRate] K faces every N frames; the rate uses the
            // measured fps so the user can see what the setting costs.
            static LLCachedControl<S32> manual_faces(
                gSavedSettings, "CineLightRigLiveProbeManualFaces", 2);
            static LLCachedControl<S32> manual_frames(
                gSavedSettings, "CineLightRigLiveProbeManualFrames", 1);
            const S32 faces = llclamp((S32)manual_faces, 1, 6);
            const S32 every = llclamp((S32)manual_frames, 1, 120);
            const F32 fps = static_cast<F32>(
                LLTrace::get_frame_recording().getPeriodMeanPerSec(LLStatViewer::FPS));
            return llformat("Refresh: Manual, %d %s every %d %s (~%.1f faces/s at current fps)",
                faces, faces == 1 ? "face" : "faces", every,
                every == 1 ? "frame" : "frames",
                fps * static_cast<F32>(faces) / static_cast<F32>(every));
        }
        case Mode::ON_CHANGE:
            break;
    }
    if (!rs.mStepping)
    {
        return "Refresh: On change, starting";
    }
    switch (rs.mPath)
    {
        case Path::FULL:
            if (rs.mReason == Reason::ANIMATED)
            {
                return "Refresh: On change, live (animated)";
            }
            if (rs.mReason == Reason::SETTLING)
            {
                return "Refresh: On change, live (settling)";
            }
            return "Refresh: On change, warming up";
        case Path::BUDGET:
            return llformat("Refresh: On change, refreshing %d/6",
                rs.mPassFace >= 0 ? rs.mPassFace : 6);
        case Path::IDLE:
            break;
    }
    if (rs.mSecondsSinceReason < 0.f)
    {
        return "Refresh: On change, idle";
    }
    return llformat("Refresh: On change, idle (%s %ds ago)",
        ALCineLiveProbeRefresh::reasonName(rs.mReason),
        ll_round(rs.mSecondsSinceReason));
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
// rig master brightness controls are deliberately NOT part of any
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
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::owned(ALPresetLook::FLARE));
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
// mask on (a preset only shapes it). The complete owned look, including
// TintColor and HeightOffset, resets to defaults before the row is applied.
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
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::owned(ALPresetLook::NIGHT_MASK));
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
    retireRigMenu(mRigGroupMenu);
    for (const auto& id : mRigNotices)
        if (auto notice = LLNotificationsUtil::find(id)) LLNotificationsUtil::cancel(notice);
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
        "CineLightRigBounceLightsSet", // [ActorMask]
        "CineLightRigCatchlightActorOnly", // [ActorMask]
        "CineLightRigActorOnlyAll", // [ActorMask]
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyOffsetZ",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyGel",
        "CineLightRigKeyFixtureMode",
        "CineLightRigKeyFixturePreset",
        "CineLightRigKeyDetailShadows",
        "CineLightRigKeyWorldShadowPick",
        "CineLightRigKeyActorOnly", // [ActorMask]
        "CineLightRigKeyKelvin",
        "CineLightRigKeyGelSlot0",
        "CineLightRigKeyGelSlot1",
        "CineLightRigKeyGelSlot2",
        "CineLightRigKeySourceSizeM",
        "CineLightRigKeyFlicker",
        "CineLightRigKeyFlickerAmount",
        "CineLightRigKeySwayAmount",
        "CineLightRigKeySwayPeriod",
        "CineLightRigKeyShadowSoft",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillOffsetZ",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillGel",
        "CineLightRigFillFixtureMode",
        "CineLightRigFillFixturePreset",
        "CineLightRigFillDetailShadows",
        "CineLightRigFillWorldShadowPick",
        "CineLightRigFillActorOnly", // [ActorMask]
        "CineLightRigFillKelvin",
        "CineLightRigFillGelSlot0",
        "CineLightRigFillGelSlot1",
        "CineLightRigFillGelSlot2",
        "CineLightRigFillSourceSizeM",
        "CineLightRigFillFlicker",
        "CineLightRigFillFlickerAmount",
        "CineLightRigFillSwayAmount",
        "CineLightRigFillSwayPeriod",
        "CineLightRigFillShadowSoft",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimOffsetZ",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimGel",
        "CineLightRigRimFixtureMode",
        "CineLightRigRimFixturePreset",
        "CineLightRigRimDetailShadows",
        "CineLightRigRimWorldShadowPick",
        "CineLightRigRimActorOnly", // [ActorMask]
        "CineLightRigRimKelvin",
        "CineLightRigRimGelSlot0",
        "CineLightRigRimGelSlot1",
        "CineLightRigRimGelSlot2",
        "CineLightRigRimSourceSizeM",
        "CineLightRigRimFlicker",
        "CineLightRigRimFlickerAmount",
        "CineLightRigRimSwayAmount",
        "CineLightRigRimSwayPeriod",
        "CineLightRigRimShadowSoft",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgOffsetZ",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgGel",
        "CineLightRigBgFixtureMode",
        "CineLightRigBgFixturePreset",
        "CineLightRigBgDetailShadows",
        "CineLightRigBgWorldShadowPick",
        "CineLightRigBgActorOnly", // [ActorMask]
        "CineLightRigBgKelvin",
        "CineLightRigBgGelSlot0",
        "CineLightRigBgGelSlot1",
        "CineLightRigBgGelSlot2",
        "CineLightRigBgSourceSizeM",
        "CineLightRigBgFlicker",
        "CineLightRigBgFlickerAmount",
        "CineLightRigBgSwayAmount",
        "CineLightRigBgSwayPeriod",
        "CineLightRigBgShadowSoft",
        "CineLightRigBgOn",
        "CineLightRigLiveProbeEnabled",
        "CineLightRigLiveProbeTarget",
        "CineLightRigLiveProbeRadius",
        "CineLightRigLiveProbeOffsetZ",
        "CineLightRigLiveProbeAmbiance",
        "CineLightRigLiveProbeBounceKeep",
        "CineLightRigLiveProbeGizmo",
        "CineLightRigLiveProbeRefresh", // [LiveProbeRefresh]
        "CineLightRigLiveProbeManualFaces", // [ProbeManualRate]
        "CineLightRigLiveProbeManualFrames", // [ProbeManualRate]
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
        // [RigRim] Rig Rim Light (Phase A).
        "CineRigRimEnabled",
        "CineRigRimMasterGain",
        "CineRigRimRoughnessSoften",
        "CineRigRimBackSoftness",
        "CineRigRimTint",
        "CineRigRimShadow", // [RigRim]
        "CineRigRimIncludeAlpha",
        "CineRigRimIncludeProbes",
        "CineRigRimIncludeBounce",
        "CineRigRimDebugRimOnly",
        "CineRigRimSmoothness", "CineRigRimSmoothRadius", "CineRigRimTextureNormals",
        "CineRigRimAntiShimmer", "CineRigRimSilhouette",
        "CineRigRimSilhouetteWidth", "CineRigRimSilhouetteUnits", "CineRigRimSilhouetteSoft",
        "CineRigRimSilhouetteGap", "CineRigRimSilhouetteSeedThreshold", "CineRigRimSilhouetteSeedSoft",
        "CineRigRimActorsOnly", "CineRigRimActorsOnlyAlpha", "CineRigRimWrapEnabled",
        "CineRigRimWrapStrength", "CineRigRimWrapWidth", "CineRigRimWrapBlur",
        "CineRigRimWrapSaturation", "CineRigRimWrapClamp", "CineRigRimDebugView",
        "CineRigRimKeyGain",
        "CineRigRimKeySharpness",
        "CineRigRimKeyWrap",
        "CineRigRimKeyBackBias",
        "CineRigRimFillGain",
        "CineRigRimFillSharpness",
        "CineRigRimFillWrap",
        "CineRigRimFillBackBias",
        "CineRigRimRimGain",
        "CineRigRimRimSharpness",
        "CineRigRimRimWrap",
        "CineRigRimRimBackBias",
        "CineRigRimBgGain",
        "CineRigRimBgSharpness",
        "CineRigRimBgWrap",
        "CineRigRimBgBackBias",
        // [TronT1] Rig Rim "Tron" palette tint row on this same card.
        "CineRigRimTronMode",
        "CineRigRimTronMix",
        "CineRigRimTronColorSource",
    };
    return names;
}

bool ALPanelCineLightRig::postBuild()
{
    mAnchorCombo = getChild<LLComboBox>("cine_anchor");
    mAllRigsOff = findChild<LLButton>("cine_all_rigs_off");
    mRemoveBorrowedRig = findChild<LLButton>("cine_remove_borrowed_rig");
    if (mRemoveBorrowedRig) mRemoveBorrowedRig->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        auto& manager = ALCineLightRigManager::instance();
        if (manager.selectedSlot() == ALCineLightRigManager::SLOT_SELF || manager.selected().getAnchor().isNull()) return;
        ALKitHistory::ScopedSuspend suspend;
        manager.removeBorrowedRig(manager.selectedSlot());
        finishRigAction(0);
    });
    mRigGroupStatus = findChild<LLTextBox>("cine_rig_group_status");
    mRigKitPage = findChild<ALKitPage>("cine_light_rig_kit");
    if (mRigKitPage)
    {
        mRigKitPage->setRigPanel(this);
        mRigKitPage->setPersistentMenu(
            [this](LLUICtrl* anchor) { showRigGroupMenu(anchor, false); },
            "Give an avatar their own rig, turn Subject rigs off or turn all rigs off.");
        if (LLButton* add = findChild<LLButton>("cine_add_avatar_rig_group"))
            add->setCommitCallback([this](LLUICtrl* anchor, const LLSD&) { showRigGroupMenu(anchor, true); });
    }
    mRigEnabled = findChild<LLCheckBoxCtrl>("cine_enabled");
    mRigRoster[0] = findChild<LLTextBox>("cine_rig_roster_self");
    mRigRoster[1] = findChild<LLTextBox>("cine_rig_roster_a");
    mRigRoster[2] = findChild<LLTextBox>("cine_rig_roster_b");
    mRigRoster[3] = findChild<LLTextBox>("cine_rig_roster_c");
    mRigRoster[4] = findChild<LLTextBox>("cine_rig_roster_d");
    if (mAllRigsOff) mAllRigsOff->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onRigGroupAction(2); });
    mObjectTargetButton = getChild<LLButton>("cine_target_object");
    mObjectTargetClear = getChild<LLButton>("cine_clear_object");
    mObjectTargetStatus = getChild<LLTextBox>("cine_object_target_status");
    mLitTarget = findChild<LLComboBox>("cine_lit_target");
    mLitMembers = findChild<LLTextBox>("cine_lit_members");
    mLitFallback = findChild<LLTextBox>("cine_lit_fallback");
    mLitCap = findChild<LLTextBox>("cine_lit_cap");
    mLitActorOnly = findChild<LLTextBox>("cine_lit_actor_only");
    mLitWide = findChild<LLTextBox>("cine_lit_wide");
    if (mLitTarget) mLitTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLitTargetCommit(); });
    mGroupEnable = findChild<LLCheckBoxCtrl>("cine_group_enable");
    mGroupStatus = findChild<LLTextBox>("cine_group_status");
    const char* const group_slot_names[5] = {
        "cine_group_self", "cine_group_a", "cine_group_b",
        "cine_group_c", "cine_group_d",
    };
    for (S32 i = 0; i < 5; ++i)
    {
        mGroupSlotChecks[i] = findChild<LLCheckBoxCtrl>(group_slot_names[i]);
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
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        mProjectorFeeds[light] = findChild<LLComboBox>(
            std::string("cine_") + ROLE_WIDGET_NAMES[light] +
            "_projector_feed");
        if (mProjectorFeeds[light])
        {
            mProjectorFeeds[light]->setCommitCallback(
                [this, light](LLUICtrl*, const LLSD&)
                { onProjectorFeedCommit(light); });
        }
    }
    mLiveProbeStatus = getChild<LLTextBox>("cine_live_probe_status");
    mLiveProbeLiveStatus = getChild<LLTextBox>("cine_live_probe_live_status");
    mLiveProbeRefreshNow = getChild<LLButton>("cine_live_probe_refresh_now"); // [LiveProbeRefresh]
    mLiveProbeManualFaces = getChild<LLUICtrl>("cine_live_probe_manual_faces"); // [ProbeManualRate]
    mLiveProbeManualFrames = getChild<LLUICtrl>("cine_live_probe_manual_frames"); // [ProbeManualRate]
    mLiveProbeManualFacesReset = getChild<LLUICtrl>("cine_live_probe_manual_faces_reset"); // [ProbeManualRate]
    mLiveProbeManualFramesReset = getChild<LLUICtrl>("cine_live_probe_manual_frames_reset"); // [ProbeManualRate]
    mNightMaskPreset = getChild<LLComboBox>("cine_night_mask_preset");
    mEasyNightMaskPreset = getChild<LLComboBox>("cine_easy_night_mask_preset");
    mRim.attach(this);
    mNightMaskDarkness = getChild<LLUICtrl>("cine_night_mask_darkness"); // [NightMaskPercent]
    mEasyNightMaskDarkness = getChild<LLUICtrl>("cine_easy_night_mask_darkness"); // [NightMaskPercent]
    mNightMaskStatus = getChild<LLTextBox>("night_mask_status");
    mSubjectTargetCombos[0] = findChild<LLComboBox>("cine_easy_probe_target");
    mSubjectTargetCombos[1] = findChild<LLComboBox>("cine_live_probe_target");
    mSubjectTargetCombos[2] = findChild<LLComboBox>("cine_night_mask_target");
    for (S32 index = 0; index < 3; ++index)
    {
        LLComboBox* combo = mSubjectTargetCombos[index];
        if (combo && combo->getControlVariable())
        {
            mSubjectTargetConnections[index] =
                combo->getControlVariable()->getSignal()->connect(
                    [combo](LLControlVariable*, const LLSD& value, const LLSD&)
                    {
                        ALDirectorCastUI::showStoredValue(combo, value, 0, 1);
                    });
        }
    }
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
    // The generated Director page has a kit child before postBuild; the
    // standalone legacy panel keeps its original live label colour.
    mRadiusDefaultColor = mRigKitPage ? LLUIColorTable::instance().getColor("KitTextSecondary")
        : LLUIColorTable::instance().getColor("LabelTextColor", LLColor4::white);
    mRadiusWarningColor = mRigKitPage ? LLUIColorTable::instance().getColor("KitWarnBandText")
        : LLUIColor(LLColor4(1.f, 0.75f, 0.25f, 1.f));
    mSetupSave = getChild<LLButton>("cine_setup_save");
    mSetupDelete = getChild<LLButton>("cine_setup_delete");
    // [UIPerf] cache the formerly per-frame name lookups (tolerant findChild,
    // same null semantics as before).
    mFXStop = getChild<LLUICtrl>("cine_fx_stop");
    {
        static const char* const easy_gated_names[8] = {
            "cine_easy_shaft_length", "cine_easy_flare_preset",
            "cine_easy_probe_enable", "cine_easy_probe_gizmo",
            "cine_easy_probe_radius", "cine_easy_night_mask_enable",
            "cine_easy_night_mask_preset", "cine_easy_night_mask_darkness",
        };
        static const char* const easy_reset_names[8] = {
            "cine_easy_brightness_reset", "cine_easy_drama_reset",
            "cine_easy_warmth_reset", "cine_easy_cone_width_reset",
            "cine_easy_cone_feather_reset", "cine_easy_shaft_length_reset",
            "cine_easy_probe_radius_reset", "cine_easy_night_mask_darkness_reset",
        };
        for (S32 i = 0; i < 8; ++i)
        {
            mEasyGatedControls[i] = findChild<LLUICtrl>(easy_gated_names[i]);
            mEasyResetButtons[i] = findChild<LLUICtrl>(easy_reset_names[i]);
        }
    }
    mFlowGrid = findChildView("cine_light_rig_flow_grid");
    if (std::find(LIVE_PANELS.begin(), LIVE_PANELS.end(), this) ==
        LIVE_PANELS.end())
    {
        LIVE_PANELS.push_back(this);
    }

    populateStaticCombos();
    updateAnchorList();
    refreshSetupList();
    if (gSavedSettings.getBOOL("PrismLensEnabled"))
        syncProjectorFeeds(true);

    mAnchorCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAnchorSelected(); });
    mObjectTargetButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onTargetObject(); });
    mObjectTargetClear->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClearObjectTarget(); });
    if (mGroupEnable) mGroupEnable->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onGroupEnabledCommit(); });
    for (LLCheckBoxCtrl* control : mGroupSlotChecks)
    {
        if (control) control->setCommitCallback(
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
        { adjustSelectedLightAim("Yaw", -1.f); });
    getChild<LLButton>("cine_aim_yaw_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustSelectedLightAim("Yaw", 1.f); });
    getChild<LLButton>("cine_aim_pitch_minus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustSelectedLightAim("Pitch", -1.f); });
    getChild<LLButton>("cine_aim_pitch_plus")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        { adjustSelectedLightAim("Pitch", 1.f); });
    getChild<LLButton>("cine_aim_reset")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAim(); });
    getChild<LLButton>("cine_reset_all")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { resetAll(); });
    // [LiveProbeRefresh] One-shot re-render of the live probe (On change mode).
    mLiveProbeRefreshNow->setCommitCallback(
        [](LLUICtrl*, const LLSD&)
        { gPipeline.mReflectionMapManager.requestCinematicLiveProbeRefresh(); });
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

    // [ActorMask] Actor only controls: control_name-bound checkboxes (the rig
    // reads the settings itself), so only the lookups are needed here. The
    // XML tooltip is captured as the enabled-state text; syncActorOnlyControls()
    // swaps in the object-target explanation while locked.
    for (S32 i = 0; i < ACTOR_ONLY_CONTROL_COUNT; ++i)
    {
        std::string base;
        if (i < ALCineLightRigModel::LIGHT_COUNT)
        {
            base = std::string("cine_") + ROLE_WIDGET_NAMES[i] + "_actor_only";
        }
        else
        {
            base = (i == ALCineLightRigModel::LIGHT_COUNT)
                ? "cine_bounce_lights_set"
                : (i == ALCineLightRigModel::LIGHT_COUNT + 1)
                    ? "cine_catchlight_actor_only" : "cine_actor_only_all";
        }
        mActorOnlyControls[i] = getChild<LLUICtrl>(base);
        mActorOnlyResets[i] = getChild<LLUICtrl>(base + "_reset");
        mActorOnlyTips[i] = mActorOnlyControls[i]->getToolTip();
    }
    mActorOnlyLockState = -1;
    // [ActorMask] The master acts on USER CLICKS only (not on rig/preset/cue
    // loads, which write the settings themselves): it ticks or unticks every
    // per-light + catchlight box, i.e. exactly the proven per-light path.
    if (LLUICtrl* master = mActorOnlyControls[ACTOR_ONLY_MASTER_INDEX])
    {
        master->setCommitCallback([this](LLUICtrl* ctrl, const LLSD&)
        {
            onActorOnlyAllChanged(ctrl->getValue().asBoolean());
        });
    }

    // [NightMaskPercent] Percent sliders over the 0..1 brightness-floor setting.
    mNightMaskDarkness->setCommitCallback(
        [this](LLUICtrl* control, const LLSD&) { onNightMaskDarknessCommit(control); });
    mEasyNightMaskDarkness->setCommitCallback(
        [this](LLUICtrl* control, const LLSD&) { onNightMaskDarknessCommit(control); });
    if (LLControlVariable* darkness_control =
            gSavedSettings.getControl("CineLightRigNightMaskDarkness"))
    {
        mNightMaskDarknessConnection = darkness_control->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&)
            {
                syncNightMaskDarknessSliders();
            });
    }
    syncNightMaskDarknessSliders();

    // Percent UI over the persisted 0..1 sway amount. Resolve widgets once;
    // settings signals cover preset loads, slot switches and undo as well.
    mSwayAmounts[0] = getChild<LLUICtrl>("cine_key_sway_amount");
    mSwayAmounts[1] = getChild<LLUICtrl>("cine_fill_sway_amount");
    mSwayAmounts[2] = getChild<LLUICtrl>("cine_rim_sway_amount");
    mSwayAmounts[3] = getChild<LLUICtrl>("cine_bg_sway_amount");
    static const char* const sway_settings[4] = {
        "CineLightRigKeySwayAmount", "CineLightRigFillSwayAmount",
        "CineLightRigRimSwayAmount", "CineLightRigBgSwayAmount"
    };
    for (S32 light = 0; light < 4; ++light)
    {
        const std::string setting = sway_settings[light];
        mSwayAmounts[light]->setCommitCallback(
            [setting](LLUICtrl* control, const LLSD&)
            {
                gSavedSettings.setF32(setting, llclamp(
                    static_cast<F32>(control->getValue().asReal()) / 100.f, 0.f, 1.f));
            });
        if (LLControlVariable* variable = gSavedSettings.getControl(setting))
        {
            mSwayConnections[light] = variable->getSignal()->connect(
                [this, light](LLControlVariable*, const LLSD& value, const LLSD&)
                {
                    mSwayAmounts[light]->setValue(100.f * static_cast<F32>(value.asReal()));
                });
        }
        mSwayAmounts[light]->setValue(100.f * gSavedSettings.getF32(setting));
    }

    syncSeedEditor();
    syncObjectTargetControls(true);
    syncGroupControls();
    syncFixtureControls(true);
    syncGoboLibrary(true);
    updateDerivedStatus();
    const auto handle=getDerivedHandle<ALPanelCineLightRig>();
    if(ALKitShell* shell=ALKitShell::find(this))
    {
        shell->history().registerContext("light_group/cine_light_rig_tab",[handle](const std::string& widget)
        {
            if(!handle.get())return std::string();
            static const char* const slots[] = { "Self", "A", "B", "C", "D" };
            const S32 slot = std::clamp((S32)ALCineLightRigManager::instance().selectedSlot(), 0, 4);
            const std::string rig_context = std::string(slots[slot]) + " rig #" +
                std::to_string(ALCineLightRigManager::instance().historyEpoch(static_cast<ALCineLightRigManager::Slot>(slot))) +
                " actor " + LLDirectorCast::canonicalActorId(ALCineLightRigManager::instance().slotActor(
                    static_cast<ALCineLightRigManager::Slot>(slot))).asString();
            const S32 fixture = std::clamp(handle.get()->mFixtureRoleIndex, 0, ALCineLightRigModel::LIGHT_COUNT - 1);
            const S32 kind = rigSlotHistoryKind(widget);
            if (kind == 2) return rig_context + ", " + ROLE_NAMES[fixture] + " fixture";
            if (kind == 1) return rig_context;
            return std::string();
        }, true);
        shell->history().registerWidgetValidator("light_group/cine_light_rig_tab",
            [handle](const std::string& widget, const LLSD& value, std::string& reason)
            {
                if (widget != "cine_lit_target") return true;
                if (!validateLitTarget(value, reason)) return false;
                const std::string target = value.asString();
                if (target.compare(0, 6, "group:") == 0)
                    if (auto* panel = handle.get(); panel && panel->mLitTarget &&
                        std::find(panel->mLitTargetValues.begin(), panel->mLitTargetValues.end(), target) == panel->mLitTargetValues.end())
                    {
                        panel->mLitTarget->add("Group: " + target.substr(6) + " (0)", target);
                        panel->mLitTargetValues.push_back(target);
                        panel->mLitTargetForceRefresh = true;
                    }
                if (target == "legacy")
                {
                    if (auto* panel = handle.get(); panel && panel->mLitTarget &&
                        std::find(panel->mLitTargetValues.begin(), panel->mLitTargetValues.end(), "legacy") == panel->mLitTargetValues.end())
                    {
                        panel->mLitTarget->add("Custom (old scene)", "legacy");
                        panel->mLitTargetValues.push_back("legacy"); panel->mLitTargetForceRefresh = true;
                    }
                    return true;
                }
                return true;
            });
        if (ALKitPage* page = findChild<ALKitPage>("cine_light_rig_kit"))
            page->setAvailabilityRefresh([handle]()
            {
                if (ALPanelCineLightRig* panel = handle.get())
                {
                    panel->applyManualLock();
                    panel->syncFixtureControls();
                    panel->syncGroupControls();
                    panel->updateDerivedStatus();
                }
            });
        ALKitModeAdapters::add(this,"cine_rig_easy",[handle](bool desired,ALKitModeAdapters::EOrigin origin)
        {
            ALPanelCineLightRig* panel=handle.get();
            if(!panel||!panel->mEasyModeToggle)return false;
            return panel->applyEasyMode(desired,origin==ALKitModeAdapters::USER?EasyModeOrigin::USER:
                origin==ALKitModeAdapters::SEARCH?EasyModeOrigin::SEARCH:EasyModeOrigin::PASSIVE);
        });
        ALKitWhoAdapters::add("cine_rig_slot",[](const LLUUID& id)
        {
            if(id.isNull()||id==gAgentID)return LLSD(0);
            const auto slot = ALCineLightRigManager::instance().actorSlot(id);
            if (slot != ALCineLightRigManager::Slot::COUNT) return LLSD(static_cast<S32>(slot));
            return LLSD();
        });
        if (mRigKitPage)
        {
            mRigKitPage->setWhoUnavailable(
                [handle](const LLUUID& id)
                {
                    auto* panel = handle.get();
                    if (!panel) return std::string();
                    return panel->pendingBorrowLabel(id);
                }, [handle](const LLUUID& id)
                {
                    if (auto* panel = handle.get(); panel && !panel->addAvatarToRigGroup(id))
                    {
                        panel->mRigKitPage->cancelWhoUnavailable();
                        panel->showRigNotice("Actor unavailable");
                    }
                });
            mRigKitPage->setWhoAvailableAction([handle]()
            {
                auto* panel = handle.get();
                if (!panel) return false;
                const auto& manager = ALCineLightRigManager::instance();
                if (manager.selectedSlot() == ALCineLightRigManager::SLOT_SELF || manager.selected().getAnchor().isNull()) return false;
                panel->showBorrowMenu(manager.selected().getAnchor(), true);
                return true;
            });
        }
        if(LLControlVariable* easy=gSavedSettings.getControl("CineLightRigEasyMode").get())
        {
            mEasyModeConnection=easy->getSignal()->connect([handle](LLControlVariable*,const LLSD& value,const LLSD&)
            {
                if(ALPanelCineLightRig* panel=handle.get())
                {
                    ALKitHistory::ScopedSuspend suspend;
                    panel->mEasyModeOverride=-1;
                    panel->syncEasyModeState(value.asBoolean(),EasyModeOrigin::PASSIVE);
                }
            });
            ALKitHistory::ScopedSuspend suspend;
            syncEasyModeState(easy->get().asBoolean(),EasyModeOrigin::RESTORE,true);
        }
    }
    return true;
}

// [NightMaskPercent] slider percent -> brightness floor (the setting keeps its
// 0..1 semantics: 0 = pitch black, 1 = no darkening).
void ALPanelCineLightRig::onNightMaskDarknessCommit(LLUICtrl* source)
{
    if (!source)
    {
        return;
    }
    // Round first so a typed fractional entry (e.g. 89.4) stores exactly the
    // whole percent the slider then displays.
    const F32 percent = static_cast<F32>(std::lround(
        llclamp(static_cast<F32>(source->getValue().asReal()), 0.f, 100.f)));
    const F32 floor_value = llclamp(1.f - percent / 100.f, 0.f, 1.f);
    // The setting-signal listener refreshes both sliders from the new value.
    gSavedSettings.setF32("CineLightRigNightMaskDarkness", floor_value);
    // A same-value write fires no signal; re-sync so a typed/fractional entry
    // still snaps to a whole percent.
    syncNightMaskDarknessSliders();
}

void ALPanelCineLightRig::syncNightMaskDarknessSliders()
{
    const F32 floor_value = llclamp(
        gSavedSettings.getF32("CineLightRigNightMaskDarkness"), 0.f, 1.f);
    const F32 percent = static_cast<F32>(std::lround((1.f - floor_value) * 100.f));
    if (mNightMaskDarkness)
    {
        mNightMaskDarkness->setValue(percent);
    }
    if (mEasyNightMaskDarkness)
    {
        mEasyNightMaskDarkness->setValue(percent);
    }
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

// ui-draw-lint: allow-function(raw-write) combo rebuild is configuration-revision and rig-slot gated
void ALPanelCineLightRig::syncProjectorFeeds(bool force)
{
    static LLCachedControl<bool> prism_enabled(
        gSavedSettings, "PrismLensEnabled", false);
    if (!prism_enabled)
    {
        // Empty state hides the control: no fake static "Cam N" items.
        for (LLComboBox* combo : mProjectorFeeds)
            if (combo) ALKitRow::setControlVisible(combo, false);
        mProjectorFeedRevision = 0;
        return;
    }
    const U8 rig_slot = static_cast<U8>(
        ALCineLightRigManager::instance().selectedSlot());
    const U64 revision = LLPrismLens::configurationRevision();
    if (!force && revision == mProjectorFeedRevision &&
        mProjectorFeedRigSlot == static_cast<S32>(rig_slot))
    {
        return;
    }
    for (LLComboBox* combo : mProjectorFeeds)
        if (combo && combo->isListVisible()) return;
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    mProjectorFeedRevision = snapshot.mConfigurationRevision;
    mProjectorFeedRigSlot = static_cast<S32>(rig_slot);
    U32 camera_count = 0;
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
        if (LLPrismLens::canRouteSource(snapshot.mCaptures[index]))
            ++camera_count;
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        LLComboBox* combo = mProjectorFeeds[light];
        if (!combo) continue;
        ALKitRow::setControlVisible(combo, camera_count != 0);
        combo->removeall();
        combo->add("None", LLUUID::null);
        for (U32 index = 0; index < snapshot.mSlotCount; ++index)
        {
            const LLPrismLens::CaptureDefinition& capture =
                snapshot.mCaptures[index];
            if (LLPrismLens::canRouteSource(capture))
                combo->add(LLPrismLens::captureDisplayLabel(capture),
                           capture.mHandle.mId);
        }
        LLUUID selected;
        for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
        {
            const LLPrismLens::ProjectorDefinition& projector =
                snapshot.mProjectors[index];
            if (projector.mTarget == LLPrismLens::EProjectorTarget::RIG_LIGHT &&
                projector.mRigSlot == rig_slot &&
                projector.mRigLight == static_cast<U8>(light))
            {
                selected = projector.mCapture.mId;
                break;
            }
        }
        setValueIfChanged(combo, LLSD(selected), true);
    }
}

void ALPanelCineLightRig::onProjectorFeedCommit(S32 light)
{
    if (light < 0 || light >= ALCineLightRigModel::LIGHT_COUNT ||
        !mProjectorFeeds[light])
    {
        return;
    }
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();
    const U8 rig_slot = static_cast<U8>(
        ALCineLightRigManager::instance().selectedSlot());
    LLPrismLens::ProjectorHandle existing;
    for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
    {
        const LLPrismLens::ProjectorDefinition& projector =
            snapshot.mProjectors[index];
        if (projector.mTarget == LLPrismLens::EProjectorTarget::RIG_LIGHT &&
            projector.mRigSlot == rig_slot &&
            projector.mRigLight == static_cast<U8>(light))
        {
            existing = projector.mHandle;
            break;
        }
    }
    const LLUUID capture_id = mProjectorFeeds[light]->getValue().asUUID();
    if (capture_id.isNull())
    {
        if (existing.mId.notNull()) LLPrismLens::removeProjector(existing);
        syncProjectorFeeds(true);
        return;
    }
    LLPrismLens::CaptureHandle capture;
    for (U32 index = 0; index < snapshot.mSlotCount; ++index)
    {
        if (snapshot.mCaptures[index].mHandle.mId == capture_id)
        {
            capture = snapshot.mCaptures[index].mHandle;
            break;
        }
    }
    if (capture.mId.isNull())
    {
        syncProjectorFeeds(true);
        return;
    }
    if (existing.mId.notNull())
        LLPrismLens::reassignProjector(existing, capture);
    else
        LLPrismLens::bindRigProjector(
            capture, rig_slot, static_cast<U8>(light),
            LLPrismLens::ProjectorSettings(), nullptr);
    syncProjectorFeeds(true);
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
    // [UIPerfC1] role-indexed cached control pointers (see fixtureSetting); no per-frame
    // prefix string / by-name lookups. The panel's own descendant-focus guard and `force`
    // semantics are unchanged: only the compare moves to setValueIfChanged (force=true
    // there, because the guard has already been applied here).
    const S32 role = mFixtureRoleIndex;
    auto sync_value = [force](LLUICtrl* control, const LLSD& value)
    {
        if (force || (!control->hasFocus() &&
                      !gFocusMgr.childHasKeyboardFocus(control)))
        {
            setValueIfChanged(control, value, true);
        }
    };

    bool fixture_mode = false;
    if (fixtureSetting(role, FF_MODE, fixture_mode))
    {
        sync_value(mFixtureMode, fixture_mode);
    }
    S32 preset = 0;
    if (fixtureSetting(role, FF_PRESET, preset))
    {
        sync_value(mFixturePreset, preset);
    }
    F32 kelvin = 0.f;
    if (fixtureSetting(role, FF_KELVIN, kelvin))
    {
        sync_value(mFixtureKelvin, kelvin);
    }
    F32 source_size = 0.f;
    if (fixtureSetting(role, FF_SOURCE, source_size))
    {
        sync_value(mFixtureSourceSize, source_size);
    }
    for (S32 slot = 0;
         slot < ALCineLightRigModel::FIXTURE_GEL_SLOT_COUNT; ++slot)
    {
        S32 gel = 0;
        if (fixtureSetting(role, FF_GEL0 + slot, gel))
        {
            sync_value(mFixtureGelSlots[slot], gel);
        }
        setEnabledIfChanged(mFixtureGelSlots[slot], fixture_mode);
    }
    setEnabledIfChanged(mFixtureKelvin, fixture_mode);
    setEnabledIfChanged(mFixtureSourceSize, fixture_mode);
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
    if (ALPresetLook::replaying()) return;
    const S32 index = std::clamp(
        mFixturePreset->getValue().asInteger(),
        0, ALCineLightRigModel::FIXTURE_PRESET_COUNT - 1);
    const ALCineLightRigModel::FixturePreset& preset =
        ALCineLightRigModel::fixturePreset(index);
    const std::string prefix = fixtureSettingPrefix();
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::fixtureSettings(prefix));
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
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightRig syncGoboLibrary");
    if (!mGoboLibrary || !mGoboPreview || !mGoboSoftness)
    {
        return;
    }
    // [UIPerfC1] role-indexed cached control pointers (see fixtureSetting). A missing
    // control falls back to the setting's zero default and is never written to the widget.
    const S32 role = mFixtureRoleIndex;
    S32 gobo_raw = 0;
    const bool have_gobo = fixtureSetting(role, FF_GOBO, gobo_raw);
    const S32 index = std::clamp(
        gobo_raw, 0, ALCineLightRigModel::GOBO_COUNT - 1);
    if (have_gobo &&
        (force || (!mGoboLibrary->hasFocus() &&
                   !gFocusMgr.childHasKeyboardFocus(mGoboLibrary))))
    {
        // panel guard applied above, so the helper's own guard is bypassed (force=true)
        setValueIfChanged(mGoboLibrary, LLSD(index), true);
    }
    setVisibleIfChanged(mGoboPreview, index > 0);
    if (index > 0 && (force || index != mDisplayedGoboIndex))
    {
        mGoboPreview->setImage(LLUI::getUIImage(llformat("CineGobo%02d", index)));
    }
    mDisplayedGoboIndex = index;
    bool fixture_mode = false;
    fixtureSetting(role, FF_MODE, fixture_mode);
    F32 source_size = 0.f;
    fixtureSetting(role, FF_SOURCE, source_size);
    F32 shadow_soft = 0.f;
    fixtureSetting(role, FF_SHADOWSOFT, shadow_soft);
    static LLCachedControl<F32> rig_radius(
        gSavedSettings, "CineLightRigRadius", 4.f);
    const F32 softness = fixture_mode
        ? ALCineLightRigModel::penumbraSoftness(
              source_size,
              (F32)rig_radius)
        : shadow_soft;
    static const char* const bucket_names[] = { "sharp", "medium", "heavy" };
    const std::string softness_text = llformat(
        "%s variant (softness %.2f)",
        bucket_names[ALCineLightRigModel::goboBlurBucket(softness)], softness);
    if (!mGoboSoftnessSet || softness_text != mLastGoboSoftness) // [UIPerf]
    {
        setTextIfChanged(mGoboSoftness, softness_text);
        mLastGoboSoftness = softness_text;
        mGoboSoftnessSet = true;
    }
}

void ALPanelCineLightRig::onGoboLibraryCommit()
{
    gSavedSettings.setS32(
        fixtureSettingPrefix() + "Gobo",
        std::clamp(mGoboLibrary->getValue().asInteger(),
                   0, ALCineLightRigModel::GOBO_COUNT - 1));
    syncGoboLibrary(true);
}

void ALPanelCineLightRig::clearRigLabels(const char* area)
{
    if (std::string(area) == "chip") mPendingBorrowLabelValid = false;
    mRigDisplayedLabels.erase(area);
}

std::string ALPanelCineLightRig::rigActorName(const LLUUID& id, const char* area)
{
    const bool visible = rigNameVisible(id);
    if (id.notNull()) mRigDisplayedLabels[area][id] = visible;
    if (id.isNull()) return "not set";
    if (LLDirectorCast::isSelf(id)) return "You";
    if (!visible) return "Actor";
    if (const auto* member = LLDirectorCast::instance().getMember(id); member && !member->mLastName.empty())
        return member->mLastName;
    const std::string name = ALGazeNearby::nonCastTargetName(id);
    return name.empty() ? "Actor" : name;
}

std::string ALPanelCineLightRig::rigLabel(S32 slot, const char* area)
{
    const auto& manager = ALCineLightRigManager::instance();
    const auto which = static_cast<ALCineLightRigManager::Slot>(slot);
    return slot == 0 ? rigActorName(gAgentID, area) : manager.at(which).getAnchor().notNull()
        ? llformat("Rig %d: ", slot + 1) + rigActorName(manager.slotActor(which), area)
        : llformat("Subject %c", 'A' + slot - 1);
}

std::string ALPanelCineLightRig::rigLabelBase(S32 slot)
{
    if (slot == 0) return "You";
    return ALCineLightRigManager::instance().at(static_cast<ALCineLightRigManager::Slot>(slot)).getAnchor().notNull()
        ? llformat("Rig %d", slot + 1) : llformat("Subject %c", 'A' + slot - 1);
}

std::string ALPanelCineLightRig::rigTargetDescription(S32 slot, const char* area)
{
    const auto& rig = ALCineLightRigManager::instance().at(
        static_cast<ALCineLightRigManager::Slot>(slot));
    if (!rig.getLitGroup().empty() && rig.getObjectTarget().isNull()) return "group " + rig.getLitGroup();
    // Object targeting overrides both group mode and the slot assignment in tick().
    if (rig.getObjectTarget().notNull())
        return rig.getObjectTarget().asString().substr(0, 8) + " (object)";

    const auto name = [this, &rig, area](S32 value) -> std::string
    {
        auto& manager = ALCineLightRigManager::instance();
        const auto slot = static_cast<ALCineLightRigManager::Slot>(value);
        return rigActorName(rig.isGroupEnabled() ? manager.groupSlotActor(slot) : manager.slotActor(slot), area);
    };
    if (!rig.isGroupEnabled()) return name(slot);

    // tick() already resolves and deduplicates members. Never resolve avatars here.
    const U32 resolved = rig.lastResolvedGroupSlots() & rig.getGroupSlots();
    std::string members;
    for (S32 value = 0; value < 5; ++value)
        if (resolved & (1u << value))
        {
            if (!members.empty()) members += ", ";
            members += name(value);
        }
    return "group: " + (members.empty() ? std::string("nobody") : members);
}

std::string ALPanelCineLightRig::pendingBorrowLabel(const LLUUID& id)
{
    auto& manager = ALCineLightRigManager::instance();
    const U64 revision = LLDirectorCast::instance().getActorChoiceRevision();
    const S32 slot = static_cast<S32>(manager.actorSlot(id));
    const bool visible = rigNameVisible(id);
    const bool nearby = ALGazeNearby::nearbyAllowed();
    // Non-Cast display-name arrivals do not advance the Cast revision.
    const std::string nearby_name = visible && !LLDirectorCast::isSelf(id) &&
        !LLDirectorCast::instance().contains(id) ? ALGazeNearby::nonCastTargetName(id) : std::string();
    if (mPendingBorrowLabelValid && mPendingBorrowLabelActor == id &&
        mPendingBorrowLabelRevision == revision && mPendingBorrowLabelSlot == slot &&
        mPendingBorrowLabelVisible == visible && mPendingBorrowLabelNearby == nearby &&
        mPendingBorrowLabelNearbyName == nearby_name)
        return mPendingBorrowLabel;
    if (!mPendingBorrowPick || *mPendingBorrowPick != id)
        mPendingBorrowPick = manager.retainBorrowActor(id);
    clearRigLabels("chip");
    mPendingBorrowLabel = slot == static_cast<S32>(ALCineLightRigManager::Slot::COUNT)
        ? "Give " + rigActorName(id, "chip") + " a rig" : std::string();
    mPendingBorrowLabelActor = id;
    mPendingBorrowLabelRevision = revision;
    mPendingBorrowLabelSlot = slot;
    mPendingBorrowLabelVisible = visible;
    mPendingBorrowLabelNearby = nearby;
    mPendingBorrowLabelNearbyName = nearby_name;
    mPendingBorrowLabelValid = true;
    return mPendingBorrowLabel;
}

bool ALPanelCineLightRig::selectActorRig(const LLUUID& id)
{
    auto& manager = ALCineLightRigManager::instance();
    const auto slot = manager.actorSlot(id);
    if (slot == ALCineLightRigManager::Slot::COUNT || !mAnchorCombo) return false;
    if (auto* shell = ALKitShell::find(this)) shell->history().finishGesture();
    ALKitHistory::ScopedSuspend suspend;
    // An explicit You pick restores legacy SELF to its normal self anchor.
    if (LLDirectorCast::canonicalActorId(id) == gAgentID) manager.restoreSelfAnchor();
    mAnchorCombo->hideList();
    mCastListInitialized = false;
    updateAnchorList(false);
    mAnchorCombo->setValue(LLSD(static_cast<S32>(slot)));
    onAnchorSelected();
    updateAnchorList(false); // Publish caches for the completed selection.
    return manager.selectedSlot() == slot;
}

// ui-draw-lint: allow-function(raw-write) anchor combo rebuild is identity-revision or explicit model-mutation gated
void ALPanelCineLightRig::updateAnchorList(bool refresh_who)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightRig updateAnchorList");
    LLDirectorCast& cast = LLDirectorCast::instance();
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    const U64 subject_revision = cast.getActorChoiceRevision();
    if (mAnchorCastGroupRevision != cast.getGroupRevision())
    {
        mAnchorCastGroupRevision = cast.getGroupRevision();
        mCastListInitialized = false;
    }
    if (subject_revision != mSubjectChoiceRevision)
    {
        mSubjectChoiceRevision = subject_revision;
        clearRigLabels("subjects");
        for (LLComboBox* combo : mSubjectTargetCombos)
        {
            if (combo)
            {
                ALDirectorCastUI::mergeSubjectCombo(combo, 0, 1, 4, {},
                    [this](const LLUUID& id) { return rigActorName(id, "subjects"); });
            }
        }
        mCastListInitialized = false;
        mDisplayedGroupSlots = ~0u;
    }
    const S32 selected_slot = static_cast<S32>(manager.selectedSlot());
    std::array<LLUUID, 5> anchor_ids{};
    for (S32 i = 0; i < 5; ++i) anchor_ids[i] = manager.slotActor(static_cast<ALCineLightRigManager::Slot>(i));
    std::array<U32, 5> group_states{};
    std::array<LLUUID, 5> object_targets{};
    U32 enabled_slots = 0;
    U32 assigned_slots = 0;
    for (S32 value = 0; value < 5; ++value)
    {
        const auto& rig = manager.at(static_cast<ALCineLightRigManager::Slot>(value));
        group_states[value] = (rig.isGroupEnabled() ? 1u : 0u) |
            (rig.getGroupSlots() << 1) | (rig.lastResolvedGroupSlots() << 6);
        if (mAnchorLitGroups[value] != rig.getLitGroup() || mAnchorLitMembers[value] != rig.lastResolvedMembers())
        {
            mAnchorLitGroups[value] = rig.getLitGroup();
            mAnchorLitMembers[value] = rig.lastResolvedMembers();
            mCastListInitialized = false;
        }
        object_targets[value] = rig.getObjectTarget();
        if (anchor_ids[value].notNull())
            assigned_slots |= 1u << value;
        if (manager.isSlotEnabled(static_cast<ALCineLightRigManager::Slot>(value)))
            enabled_slots |= 1u << value;
    }
    const U32 visibility_state = assigned_slots | (enabled_slots << 5) |
                                 (static_cast<U32>(selected_slot) << 10);
    if (visibility_state != mAnchorVisibilityState ||
        group_states != mAnchorGroupStates || object_targets != mAnchorObjectTargets ||
        anchor_ids != mAnchorAssignments)
        mCastListInitialized = false;
    if (mCastListInitialized)
    {
        syncAnchorSelection();
        if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();
        return;
    }
    clearRigLabels("roster");
    setEnabledIfChanged(mAllRigsOff, enabled_slots != 0);
    setEnabledIfChanged(mRemoveBorrowedRig, selected_slot != 0 && manager.selected().getAnchor().notNull());
    const S32 count = manager.enabledCount();
    setTextIfChanged(mRigGroupStatus, llformat("%d %s on", count, count == 1 ? "rig" : "rigs"));
    if (mRigKitPage)
    {
        const S32 others = count - ((enabled_slots & (1u << selected_slot)) ? 1 : 0);
        mRigKitPage->setMasterContext("This rig",
            llformat("This rig is off \xE2\x80\x94 %d other %s on", others, others == 1 ? "rig" : "rigs"));
    }
    else
        setLabelIfChanged(mRigEnabled, "This rig");
    for (S32 value = 0; value < 5; ++value)
    {
        const std::string label = rigLabelBase(value);
        if (mRigKitPage)
            if (ALKitRow* row = mRigKitPage->findRow(mRigRoster[value]->getName())) row->setDisplayLabel(label);
        const std::string text = label + ((enabled_slots & (1u << value)) ? ": on - " : ": off - ") +
            rigTargetDescription(value, "roster");
        setTextIfChanged(mRigRoster[value], text);
        setToolTipIfChanged(mRigRoster[value], text);
        ALKitRow::setControlVisible(mRigRoster[value], value == 0 || anchor_ids[value].notNull());
    }
    // Only the combo's rows are immutable while its list is open. Other rig
    // status, roster, enable states and chip must continue to track the model.
    if (mAnchorCombo->isListVisible())
    {
        if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();
        return;
    }
    const auto anchor_labels = [this]()
    {
        const auto found = mRigDisplayedLabels.find("anchors");
        return found == mRigDisplayedLabels.end() ? std::map<LLUUID, bool>() : found->second;
    };
    const auto old_anchor_labels = anchor_labels();
    clearRigLabels("anchors");
    std::vector<LLUUID> ids;
    std::vector<std::string> labels;
    std::vector<S32> values;
    ids.reserve(5);
    labels.reserve(5);
    values.reserve(5);
    for (S32 value = 0; value < 5; ++value)
    {
        const ALCineLightRigManager::Slot slot =
            static_cast<ALCineLightRigManager::Slot>(value);
        const LLUUID& id = anchor_ids[value];
        if (value != 0 && id.isNull())
            continue;
        const auto& rig = manager.at(slot);
        std::string label = rigLabel(value);
        if (((value == 0 || rig.getAnchor().isNull()) && (value != 0 || rig.getAnchor().notNull())) ||
            rig.isGroupEnabled() || !rig.getLitGroup().empty() || rig.getObjectTarget().notNull())
            label += ": " + rigTargetDescription(value);
        ids.push_back(id);
        labels.push_back(label);
        values.push_back(value);
    }
    const bool changed = !mCastListInitialized || ids != mCastIds ||
                         labels != mCastNames || values != mCastValues;
    if (!changed)
    {
        syncAnchorSelection();
        if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();
        return;
    }

    // The shell copies the enabled anchor rows into immutable choice buttons.
    // Forced/unchanged refreshes and unrelated ledgers do not retire them.
    if (ids != mCastIds || labels != mCastNames || values != mCastValues ||
        old_anchor_labels != anchor_labels())
        if (auto* shell = ALKitShell::find(this)) shell->hidePopover();
    mCastIds = ids;
    mCastNames = labels;
    mCastValues = values;
    mAnchorVisibilityState = visibility_state;
    mAnchorGroupStates = group_states;
    mAnchorObjectTargets = object_targets;
    mAnchorAssignments = anchor_ids;
    mAnchorCombo->clearRows();
    for (S32 i = 0; i < static_cast<S32>(values.size()); ++i)
    {
        mAnchorCombo->add(labels[i], LLSD(values[i]));
    }
    mCastListInitialized = true;
    mAnchorSelectionInitialized = false;
    syncAnchorSelection();
    if (refresh_who && mRigKitPage) mRigKitPage->refreshWhoUnavailable();
}

void ALPanelCineLightRig::showRigGroupMenu(LLUICtrl* anchor, bool add_only)
{
    if (!mRigKitPage || !anchor || !gMenuHolder)
        return;
    retireRigMenu(mRigGroupMenu);
    clearRigLabels("menu");
    LLContextMenu::Params params;
    params.name = "cine_rig_group_menu";
    params.visible = false;
    LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(params);
    mRigGroupMenu = menu->getHandle();
    gMenuHolder->addChild(menu);
    auto add = [](LLContextMenu* target, const std::string& name, const std::string& label,
                  bool enabled, std::function<void()> action)
    {
        LLMenuItemCallGL::Params item;
        item.name = name;
        item.label = label;
        item.enabled = enabled;
        if (name == "subject_rigs_off") item.tool_tip = "Turn off rigs 2-5, including borrowed rigs. Leave You unchanged.";
        if (name == "all_off") item.tool_tip = "Turn off every rig, including borrowed rigs and You.";
        item.on_click.function = [action](LLUICtrl*, const LLSD&) { if (action) action(); };
        target->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
    };
    LLContextMenu* avatars = menu;
    if (!add_only)
    {
        params.name = "add_avatar";
        params.label = "Give avatar a rig";
        avatars = LLUICtrlFactory::create<LLContextMenu>(params);
        menu->appendContextSubMenu(avatars);
    }
    const auto& cast = LLDirectorCast::instance();
    std::set<LLUUID> offered;
    for (const auto& member : cast.getCast())
    {
        const LLUUID id = member.mId;
        if (id == gAgentID) continue;
        offered.insert(id);
        add(avatars, "cast_" + id.asString(), rigActorName(id, "menu"), true,
            [this, id, pick = ALCineLightRigManager::instance().retainBorrowActor(id)]()
            {
                const LLUUID actor = ALCineLightRigManager::instance().resolveBorrowActor(id, true);
                if (actor.isNull()) { showRigNotice("Actor unavailable"); return; }
                addAvatarToRigGroup(actor);
            });
    }
    if (ALGazeNearby::nearbyAllowed())
        for (LLCharacter* character : LLCharacter::sInstances)
            if (auto* avatar = dynamic_cast<LLVOAvatar*>(character);
                avatar && !avatar->isDead() && !avatar->isControlAvatar() && !avatar->mIsDummy && !avatar->isGhostAvatar())
            {
                const LLUUID id = avatar->getID();
                if (id == gAgentID || !offered.insert(id).second) continue;
                add(avatars, "nearby_" + id.asString(), rigActorName(id, "menu"), true,
                    [this, id]() { addAvatarToRigGroup(id); });
            }
    if (offered.empty()) add(avatars, "no_actors", "No actors available", false, {});
    auto& manager = ALCineLightRigManager::instance();
    if (!add_only && manager.selectedSlot() != ALCineLightRigManager::SLOT_SELF && manager.selected().getAnchor().notNull())
    {
        const auto slot = manager.selectedSlot();
        const LLUUID expected = manager.selected().getAnchor();
        const bool expected_was_cast = cast.contains(expected);
        const auto handle = getDerivedHandle<ALPanelCineLightRig>();
        add(menu, "remove_borrowed_rig", "Remove rig", true, [handle, slot, expected, expected_was_cast, pick = manager.retainBorrowActor(expected)]()
        {
            auto* panel = handle.get();
            auto& rigs = ALCineLightRigManager::instance();
            if (!panel) return;
            const LLUUID actor = rigs.resolveBorrowActor(expected, expected_was_cast);
            if (actor.isNull())
                { panel->showRigNotice("Actor unavailable"); return; }
            if (LLDirectorCast::canonicalActorId(rigs.at(slot).getAnchor()) != actor) return;
            if (auto* shell = ALKitShell::find(panel)) shell->history().finishGesture();
            ALKitHistory::ScopedSuspend suspend;
            rigs.removeBorrowedRig(slot);
            panel->finishRigAction(0);
        });
    }
    if (!add_only)
    {
        add(menu, "subject_rigs_off", "Subject rigs off", true, [this]() { onRigGroupAction(1); });
        add(menu, "all_off", "All off", true, [this]() { onRigGroupAction(2); });
    }
    avatars->buildDrawLabels();
    menu->buildDrawLabels();
    ALKitMenu::showContextMenu(anchor, menu, 0, 0);
}

void ALPanelCineLightRig::ensureRigActorInCast(const LLUUID& id)
{
    auto& cast = LLDirectorCast::instance();
    if (cast.contains(id)) return;
    clearRigLabels("notice");
    const std::string name = rigActorName(id, "notice");
    cast.add(id);
    showRigNotice("Added " + name + " to Cast");
}

void ALPanelCineLightRig::showRigNotice(const std::string& message)
{
    if (const auto notice = LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", message)))
    {
        mRigNotices.push_back(notice->getID());
        const auto labels = mRigDisplayedLabels.find("notice");
        if (labels != mRigDisplayedLabels.end() && !labels->second.empty())
            mRigDisplayedLabels.emplace("notice:" + notice->getID().asString(), std::move(labels->second));
        if (!mRigNoticeTimerRunning)
        {
            mRigNoticeTimerRunning = true;
            const auto handle = getDerivedHandle<ALPanelCineLightRig>();
            doOnIdleRepeating([handle]()
            {
                auto* panel = handle.get();
                if (!panel) return true;
                if (panel->mRigNoticeClock.due()) panel->refreshRigNotices(!panel->isInVisibleChain());
                if (!panel->mRigNotices.empty()) return false;
                panel->mRigNoticeTimerRunning = false;
                return true;
            });
        }
    }
    clearRigLabels("notice");
}

void ALPanelCineLightRig::refreshRigNotices(bool cancel_names)
{
    for (auto it = mRigNotices.begin(); it != mRigNotices.end();)
    {
        const auto notice = LLNotificationsUtil::find(*it);
        const std::string area = "notice:" + it->asString();
        const auto labels = mRigDisplayedLabels.find(area);
        bool changed = false;
        if (labels != mRigDisplayedLabels.end())
            for (const auto& label : labels->second)
                changed |= cancel_names || rigNameVisible(label.first) != label.second;
        if (changed && notice && notice->isActive()) LLNotificationsUtil::cancel(notice);
        if (!notice || !notice->isActive())
        {
            mRigDisplayedLabels.erase(area);
            it = mRigNotices.erase(it);
        }
        else ++it;
    }
}

void ALPanelCineLightRig::finishRigAction(S32 slot, bool follow)
{
    if (auto* shell = ALKitShell::find(this)) shell->history().finishGesture();
    ALKitHistory::ScopedSuspend suspend;
    {
        ALKitHistory::ScopedSuspend suspend;
        ALCineLightRigManager::instance().setSelectedSlot(static_cast<ALCineLightRigManager::Slot>(slot));
    }
    mPendingBorrowPick.reset();
    mCastListInitialized = false;
    mLitTargetForceRefresh = true;
    updateAnchorList();
    if (mRigKitPage)
    {
        mRigKitPage->setWhoPin(follow ? LLSD() : LLSD(slot));
        mRigKitPage->refreshWhoUnavailable();
    }
    syncObjectTargetControls(true);
    syncGroupControls();
    if (auto* shell = ALKitShell::find(this)) shell->history().finishGesture();
}

bool ALPanelCineLightRig::addAvatarToRigGroup(const LLUUID& id)
{
    if (id.isNull() || id == gAgentID || !mRigKitPage) return false;
    auto& manager = ALCineLightRigManager::instance();
    const LLUUID actor = manager.resolveBorrowActor(id);
    if (actor.isNull()) return false;
    if (actor != id) return addAvatarToRigGroup(actor);
    const auto slot = manager.borrowSlot(id);
    if (slot == ALCineLightRigManager::Slot::COUNT) { showBorrowMenu(id); return true; }
    const bool follow = !mRigKitPage->whoPinned() &&
        LLDirectorCast::canonicalActorId(mRigKitPage->actorTarget()) == LLDirectorCast::canonicalActorId(id);
    if (auto* shell = ALKitShell::find(this)) shell->history().finishGesture();
    ALKitHistory::ScopedSuspend suspend;
    ensureRigActorInCast(id);
    if (manager.actorSlot(id) == ALCineLightRigManager::Slot::COUNT)
        manager.giveActorRig(slot, id);
    else
    {
        manager.setSelectedSlot(slot);
        manager.touchSlot(slot);
        gSavedSettings.setBOOL("CineLightRigEnabled", true);
    }
    finishRigAction(static_cast<S32>(slot), follow);
    return true;
}

void ALPanelCineLightRig::showBorrowMenu(const LLUUID& id, bool remove)
{
    if (!mRigKitPage || !mRigKitPage->whoChip() || !gMenuHolder) return;
    auto& manager = ALCineLightRigManager::instance();
    const auto slot = remove ? manager.actorSlot(id) : manager.leastRecentlyUsedSlot();
    if (slot == ALCineLightRigManager::Slot::COUNT) return;
    retireRigMenu(mRigGroupMenu);
    clearRigLabels("menu");
    LLContextMenu::Params params;
    params.name = "cine_borrow_rig_menu"; params.visible = false;
    auto* menu = LLUICtrlFactory::create<LLContextMenu>(params);
    mRigGroupMenu = menu->getHandle(); gMenuHolder->addChild(menu);
    const auto handle = getDerivedHandle<ALPanelCineLightRig>();
    const auto add = [menu](const std::string& name, const std::string& label, std::function<void()> action)
    {
        LLMenuItemCallGL::Params p; p.name = name; p.label = label;
        p.on_click.function = [action](LLUICtrl*, const LLSD&) { action(); };
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(p));
    };
    const LLUUID expected = manager.slotActor(slot);
    const bool expected_was_cast = LLDirectorCast::instance().contains(expected);
    const LLUUID captured = LLDirectorCast::canonicalActorId(id);
    const bool was_cast = LLDirectorCast::instance().contains(captured);
    const auto picked = manager.retainBorrowActor(captured);
    const auto expected_pick = manager.retainBorrowActor(expected);
    if (remove)
    {
        add("follow_cast", "Follow cast selection", [handle]()
        { if (auto* panel = handle.get()) panel->mRigKitPage->setWhoPin(LLSD()); });
        for (S32 i = 0; i < ALCineLightRigManager::SLOT_COUNT; ++i)
        {
            const auto which = static_cast<ALCineLightRigManager::Slot>(i);
            if (i != 0 && manager.slotActor(which).isNull()) continue;
            add("edit_rig_" + std::to_string(i), i == 0 ? "Edit your rig" : "Edit " + rigLabel(i, "menu"), [handle, i]()
            { if (auto* panel = handle.get()) panel->finishRigAction(i); });
        }
        add("remove_rig", "Remove rig", [handle, captured, slot, was_cast, picked]()
        {
            if (auto* panel = handle.get())
            {
                auto& rigs = ALCineLightRigManager::instance();
                const LLUUID actor = rigs.resolveBorrowActor(captured, was_cast);
                if (actor.isNull())
                    { panel->showRigNotice("Actor unavailable"); return; }
                if (LLDirectorCast::canonicalActorId(rigs.at(slot).getAnchor()) != actor) return;
                if (auto* shell = ALKitShell::find(panel)) shell->history().finishGesture();
                ALKitHistory::ScopedSuspend suspend;
                rigs.removeBorrowedRig(slot);
                panel->finishRigAction(0);
            }
        });
    }
    else
    {
        // First item is the default. Never mutate until the user chooses it.
        add("replace_rig", "Replace " + (expected.isNull() ? llformat("Rig %d", static_cast<S32>(slot) + 1) : rigActorName(expected, "menu")), [handle, captured, was_cast, slot, expected, expected_was_cast, picked, expected_pick]()
        {
            if (auto* panel = handle.get())
            {
                auto& rigs = ALCineLightRigManager::instance();
                const LLUUID id = rigs.resolveBorrowActor(captured, was_cast);
                if (id.isNull()) { panel->showRigNotice("Actor unavailable"); return; }
                if (rigs.borrowSlot(id) != ALCineLightRigManager::Slot::COUNT ||
                    rigs.leastRecentlyUsedSlot() != slot || rigs.slotActor(slot) != (expected_was_cast ? rigs.resolveBorrowActor(expected, true) : expected))
                    { panel->addAvatarToRigGroup(id); return; }
                const bool follow = !panel->mRigKitPage->whoPinned() &&
                    LLDirectorCast::canonicalActorId(panel->mRigKitPage->actorTarget()) == LLDirectorCast::canonicalActorId(id);
                if (auto* shell = ALKitShell::find(panel)) shell->history().finishGesture();
                ALKitHistory::ScopedSuspend suspend;
                panel->ensureRigActorInCast(id);
                rigs.giveActorRig(slot, id);
                panel->finishRigAction(static_cast<S32>(slot), follow);
            }
        });
        add("share_rig", "Share " + (expected.isNull() ? llformat("Rig %d", static_cast<S32>(slot) + 1) : rigActorName(expected, "menu") + "'s rig"), [handle, captured, was_cast, slot, expected, expected_was_cast, picked, expected_pick]()
        {
            if (auto* panel = handle.get())
            {
                auto& rigs = ALCineLightRigManager::instance();
                const LLUUID id = rigs.resolveBorrowActor(captured, was_cast);
                if (id.isNull()) { panel->showRigNotice("Actor unavailable"); return; }
                if (rigs.slotActor(slot) != (expected_was_cast ? rigs.resolveBorrowActor(expected, true) : expected) || rigs.borrowSlot(id) != ALCineLightRigManager::Slot::COUNT)
                    { panel->addAvatarToRigGroup(id); return; }
                panel->shareRig(id, static_cast<S32>(slot));
            }
        });
    }
    add("cancel", "Cancel", []() {});
    // The menu now owns the captured pick. Dismissal (Escape, outside click or
    // Cancel) must leave the master controlling the actual selected rig.
    mRigKitPage->cancelWhoUnavailable();
    mPendingBorrowPick.reset();
    menu->buildDrawLabels();
    ALKitMenu::showContextMenu(mRigKitPage->whoChip(), menu, 0, 0);
}

void ALPanelCineLightRig::shareRig(const LLUUID& captured, S32 value)
{
    auto& manager = ALCineLightRigManager::instance();
    const LLUUID id = manager.resolveBorrowActor(captured);
    if (id.isNull()) { showRigNotice("Actor unavailable"); return; }
    if (auto* shell = ALKitShell::find(this)) shell->history().finishGesture();
    ALKitHistory::ScopedSuspend suspend;
    const auto slot = static_cast<ALCineLightRigManager::Slot>(value);
    auto& rig = manager.at(slot);
    // Snapshot the effective recipients before enrollment changes group
    // resolution. In particular, a currently falling-back rig still lights
    // its own actor even though its named group has only away members.
    const auto& effective = rig.lastResolvedStoredMembers().empty()
        ? rig.lastResolvedMembers() : rig.lastResolvedStoredMembers();
    uuid_vec_t recipients;
    for (const auto& member : effective)
    {
        // A clone may have rekeyed since this snapshot's last rig tick.
        // Resolve retirement too, without dropping legitimate away recipients.
        const LLUUID actor = manager.resolveRigActorIdentity(member);
        if (actor.notNull() && std::find(recipients.begin(), recipients.end(), actor) == recipients.end()) recipients.push_back(actor);
    }
    if (!rig.getLitGroup().empty())
    {
        for (const auto& member : LLDirectorCast::instance().membersInGroup(rig.getLitGroup()))
        {
            const LLUUID actor = LLDirectorCast::canonicalActorId(member);
            if (actor.notNull() && std::find(recipients.begin(), recipients.end(), actor) == recipients.end())
                recipients.push_back(actor);
        }
        // A missing/empty named group uses the engine's solo fallback.
        if (recipients.empty())
        {
            const LLUUID actor = LLDirectorCast::canonicalActorId(manager.slotActor(slot));
            if (actor.notNull()) recipients.push_back(actor);
        }
    }
    else if (rig.isGroupEnabled())
    {
        // Always union authored membership with effective recipients, including
        // away actors. Legacy slots are Subjects, never rig anchors.
        for (S32 i = 0; i < ALCineLightRigManager::SLOT_COUNT; ++i)
            if (rig.getGroupSlots() & (1u << i))
            {
                const LLUUID actor = LLDirectorCast::canonicalActorId(
                    manager.groupSlotActor(static_cast<ALCineLightRigManager::Slot>(i)));
                if (actor.notNull() && std::find(recipients.begin(), recipients.end(), actor) == recipients.end())
                    recipients.push_back(actor);
            }
    }
    else if (recipients.empty())
    {
        const LLUUID retained = LLDirectorCast::canonicalActorId(manager.slotActor(slot));
        if (retained.notNull()) recipients.push_back(retained);
    }
    for (const auto& recipient : recipients) ensureRigActorInCast(recipient);
    ensureRigActorInCast(id);
    if (std::find(recipients.begin(), recipients.end(), id) == recipients.end()) recipients.push_back(id);
    auto& cast = LLDirectorCast::instance();
    const auto before = cast.captureGroups();
    std::string group = rig.getLitGroup();
    if (group.empty() || cast.membersInGroup(group).empty())
    {
        group = cast.makeGroup(recipients);
    }
    else
    {
        cast.addToGroup(recipients, group);
    }
    manager.setSelectedSlot(slot);
    manager.setLitGroup(slot, group);
    rig.setObjectTarget(LLUUID::null);
    gSavedSettings.setString("CineLightRigObjectTarget", "");
    rig.setGroupEnabled(false);
    manager.touchSlot(slot);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    std::string message = "Sharing '" + group + "'. Rename in Cast > Groups";
    for (const auto& member : cast.getCast())
    {
        auto old = before.tags.find(member.mId);
        if (old != before.tags.end() && !old->second.empty() && old->second != member.mGroup)
            message += ". Moved " + rigActorName(member.mId, "notice") + " from '" + old->second + "'";
    }
    showRigNotice(message);
    finishRigAction(value);
}

void ALPanelCineLightRig::onRigGroupAction(S32 action)
{
    ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    ALKitShell* shell = ALKitShell::find(this);
    {
        ALKitHistory::ScopedSuspend suspend;
        if (action == 1) manager.subjectRigsOff();
        else manager.allOff();
    }
    if (shell) shell->history().barrier("rigs changed");
    mCastListInitialized = false;
    updateAnchorList();
    if (mRigKitPage)
        mRigKitPage->refreshWhoUnavailable();
    syncObjectTargetControls(true);
    syncGroupControls();
}

void ALPanelCineLightRig::syncAnchorSelection()
{
    if (mRigKitPage) mRigKitPage->syncRigTarget();
    const S32 selected = static_cast<S32>(
        ALCineLightRigManager::instance().selectedSlot());
    if ((mAnchorSelectionInitialized && selected == mDisplayedSlot) ||
        mAnchorCombo->hasFocus() ||
        gFocusMgr.childHasKeyboardFocus(mAnchorCombo))
    {
        return;
    }
    setValueIfChanged(mAnchorCombo, LLSD(selected), true);
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
        ALCineLightRigManager& manager=ALCineLightRigManager::instance();
        if (mRigKitPage && !ALKitHistory::suspended()) mRigKitPage->cancelWhoUnavailable();
        if(value==static_cast<S32>(manager.selectedSlot()))
        {
            if (mRigKitPage) mRigKitPage->syncRigTarget();
            syncObjectTargetControls(true);
            return;
        }
        ALKitShell* shell=ALKitShell::find(this);
        if (shell) shell->history().finishGesture();
        {
            ALKitHistory::ScopedSuspend suspend;
            manager.setSelectedSlot(
                static_cast<ALCineLightRigManager::Slot>(value));
        }
        if (mRigKitPage) mRigKitPage->syncRigTarget();
        if(shell && !ALKitHistory::suspended())shell->history().barrier("rig slot changed");
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
    setEnabledIfChanged(mObjectTargetClear, target.notNull());
    if (target.isNull())
    {
        setTextIfChanged(mObjectTargetStatus,
            std::string("\xE2\x80\x94 avatar \xE2\x80\x94"), true);
        setToolTipIfChanged(mObjectTargetStatus,
            std::string("This slot follows its avatar anchor."));
    }
    else
    {
        const std::string key = target.asString();
        setTextIfChanged(mObjectTargetStatus, "object " + key.substr(0, 8), true);
        setToolTipIfChanged(mObjectTargetStatus, key);
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
    mCastListInitialized = false;
    gSavedSettings.setString(
        "CineLightRigObjectTarget", root->getID().asString());
    syncObjectTargetControls(true);
    syncGroupControls();
    updateAnchorList();
}

void ALPanelCineLightRig::onClearObjectTarget()
{
    ALCineLightRigManager::instance().selected().setObjectTarget(LLUUID::null);
    mCastListInitialized = false;
    gSavedSettings.setString("CineLightRigObjectTarget", std::string());
    syncObjectTargetControls(true);
    syncGroupControls();
    updateAnchorList();
}

bool ALPanelCineLightRig::validateLitTarget(const LLSD& value, std::string& reason)
{
    const std::string target = value.asString();
    // Missing names are authored targets: fallback now, rebind by name later.
    if (target == "make") { reason = "Transient Lights action"; return false; }
    return true;
}

void ALPanelCineLightRig::onLitTargetCommit()
{
    auto& rig = ALCineLightRigManager::instance().selected();
    auto& cast = LLDirectorCast::instance();
    std::string value = mLitTarget->getValue().asString();
    const bool make = value == "make";
    auto before = cast.captureGroups();
    if (make)
        if (auto* shell = ALKitShell::find(this))
        {
            auto& history = shell->history();
            const LLSD baseline(!rig.getLitGroup().empty() ? "group:" + rig.getLitGroup()
                : rig.isGroupEnabled() ? "legacy" : "subject");
            // The row observer already recorded Make, possibly coalescing it with
            // an earlier choice. Flush that choice before creating this action.
            history.normalizeWidgetValue(mLitTarget, LLSD("make"), baseline);
            history.finishGesture();
            history.recordWidget(mLitTarget, baseline, LLSD("make"), "Make group");
        }
    uuid_vec_t made_ids;
    // Normalize the isolated widget action to the created group's final value.
    mLitTargetForceRefresh = true;
    if (value == "legacy")
    {
        if (!ALKitHistory::replaying()) return;
        ALCineLightRigManager::instance().setLitGroup(ALCineLightRigManager::instance().selectedSlot(), std::string());
        rig.setGroupEnabled(true);
        syncGroupControls();
        return;
    }
    if (value == "make")
    {
        cast.add(gAgentID);
        before = cast.captureGroups();
        uuid_vec_t ids{gAgentID};
        for (const LLUUID& id : {ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_A), ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_B), ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_C), ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_D)})
            if (id.notNull() && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        made_ids = ids;
        value = "group:" + cast.makeGroup(ids);
    }
    ALCineLightRigManager::instance().setLitGroup(ALCineLightRigManager::instance().selectedSlot(),
        value.compare(0, 6, "group:") == 0 ? value.substr(6) : std::string());
    rig.setGroupEnabled(false);
    mCastListInitialized = false;
    syncGroupControls();
    updateAnchorList();
    if (make)
    {
        if (auto* shell = ALKitShell::find(this))
        {
            auto& history = shell->history();
            history.normalizeWidgetValue(mLitTarget, LLSD("make"), LLSD(value));
            // Membership belongs to the custom entry; the widget owns the binding.
            struct Edit { ALDirectorGroups::GroupSnapshot before, after; };
            auto edit = std::make_shared<Edit>(Edit{before, cast.captureGroups()});
            auto validate = [edit](bool redo, std::string& reason)
            {
                auto current = LLDirectorCast::instance().captureGroups();
                if (ALDirectorGroups::patchDelta(current, redo ? edit->before : edit->after,
                    redo ? edit->after : edit->before, false)) return true;
                reason = "An edited group value changed or a member left the cast";
                return false;
            };
            history.recordCustom(mLitTarget, "rig_make_group", "Make group",
                [edit, validate](bool redo, std::string& reason)
                {
                    if (!validate(redo, reason)) return false;
                    auto& cast = LLDirectorCast::instance();
                    auto current = cast.captureGroups();
                    ALDirectorGroups::patchDelta(current, redo ? edit->before : edit->after,
                        redo ? edit->after : edit->before, true);
                    cast.restoreGroups(current);
                    return true;
                }, true, validate,
                [edit](const LLUUID& old_id, const LLUUID& new_id)
                {
                    edit->before.rewriteActor(old_id, new_id);
                    edit->after.rewriteActor(old_id, new_id);
                });
            history.finishGesture();
        }
        clearRigLabels("notice");
        std::string moved;
        for (const auto& id : made_ids)
        {
            auto old = before.tags.find(id);
            const auto* member = cast.getMember(id);
            if (!member || old == before.tags.end() || old->second.empty() || old->second == member->mGroup) continue;
            if (!moved.empty()) moved += ", ";
            moved += rigActorName(id, "notice") + " from '" + old->second + "'";
        }
        std::string message = llformat("Made '%s' (%d). Rename in Cast > Groups", rig.getLitGroup().c_str(), (S32)cast.membersInGroup(rig.getLitGroup()).size());
        if (!moved.empty()) message += ". Moved " + moved;
        showRigNotice(message);
    }
}

void ALPanelCineLightRig::onGroupEnabledCommit()
{
    if (mGroupEnable->getValue().asBoolean())
        onGroupSlotsCommit();
    else
    {
        commitGroupState(ALCineLightRigManager::instance().selected().getGroupSlots(), false);
    }
}

void ALPanelCineLightRig::onGroupSlotsCommit()
{
    U32 slots = 0;
    std::set<LLUUID> people;
    const bool self_checked = mGroupSlotChecks[0]->getValue().asBoolean();
    const auto& manager = ALCineLightRigManager::instance();
    for (S32 i = 0; i < ALCineLightRigManager::SLOT_COUNT; ++i)
    {
        const LLUUID actor = manager.groupSlotActor(static_cast<ALCineLightRigManager::Slot>(i));
        const bool duplicate_self = i > 0 && LLDirectorCast::isSelf(actor);
        if (actor.notNull() &&
            (duplicate_self ? self_checked : mGroupSlotChecks[i]->getValue().asBoolean()))
        {
            slots |= 1u << i;
            people.insert(LLDirectorCast::canonicalActorId(actor));
        }
    }
    commitGroupState(slots, people.size() >= 2);
}

void ALPanelCineLightRig::commitGroupState(U32 slots, bool enabled)
{
    auto& manager = ALCineLightRigManager::instance();
    const auto slot = manager.selectedSlot();
    auto& rig = manager.selected();
    const U32 old_slots = rig.getGroupSlots();
    const bool old_enabled = rig.isGroupEnabled();
    rig.setGroupSlots(slots);
    rig.setGroupEnabled(enabled);
    syncGroupControls();
    if (old_slots == slots && old_enabled == enabled) return;
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        const auto handle = getDerivedHandle<ALPanelCineLightRig>();
        shell->history().recordCustom(this, "cine_group_state", "Who's lit",
            [handle, slot, old_slots, old_enabled, slots, enabled](bool redo, std::string&)
            {
                auto& manager = ALCineLightRigManager::instance();
                auto& rig = manager.at(slot);
                // Replay the authored pair; never run the user auto-on policy.
                rig.setGroupSlots(redo ? slots : old_slots);
                rig.setGroupEnabled(redo ? enabled : old_enabled);
                if (handle.get() && manager.selectedSlot() == slot)
                    handle.get()->syncGroupControls();
                return true;
            }, false,
            [slot, old_slots, old_enabled, slots, enabled](bool redo, std::string& reason)
            {
                auto& manager = ALCineLightRigManager::instance();
                const auto& rig = manager.at(slot);
                if (manager.selectedSlot() == slot &&
                    rig.getGroupSlots() == (redo ? old_slots : slots) &&
                    rig.isGroupEnabled() == (redo ? old_enabled : enabled)) return true;
                reason = "Skipped a lit group changed elsewhere";
                return false;
            });
    }
}

// ui-draw-lint: allow-function(raw-write) combo rebuild is revision/target gated and deferred while its list is open
void ALPanelCineLightRig::syncGroupControls()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    if (mLitTarget)
    {
        const auto actorName = [this](const LLUUID& id, const char* area) -> std::string
        {
            return rigActorName(id.isNull() ? gAgentID : id, area);
        };
        const S32 slot = static_cast<S32>(ALCineLightRigManager::instance().selectedSlot());
        std::array<LLUUID, 5> ids{};
        for (S32 i = 0; i < 5; ++i) ids[i] = ALCineLightRigManager::instance().slotActor(static_cast<ALCineLightRigManager::Slot>(i));
        const bool legacy = rig.getLitGroup().empty() && rig.isGroupEnabled();
        if (!mLitTarget->isListVisible() && (mLitTargetForceRefresh || mLitComboCastRevision != cast.getActorChoiceRevision() ||
            mLitComboGroupRevision != cast.getGroupRevision() || mLitComboSlot != slot ||
            mLitComboGroup != rig.getLitGroup() || mLitComboSlots != rig.getGroupSlots() || mLitComboLegacy != legacy))
        {
            clearRigLabels("lights_combo");
            const std::string subject = rig.getAnchor().notNull() && slot != 0 ? rigLabel(slot, "lights_combo") : actorName(ids[slot], "lights_combo");
            std::vector<std::string> labels{ "This rig's subject: " + subject }, values{ "subject" };
            for (const auto& name : cast.getGroupNames())
            {
                labels.push_back("Group: " + name + llformat(" (%d)", static_cast<S32>(cast.membersInGroup(name).size())));
                values.push_back("group:" + name);
            }
            if (!rig.getLitGroup().empty() && std::find(values.begin(), values.end(), "group:" + rig.getLitGroup()) == values.end())
            {
                labels.push_back("Group: " + rig.getLitGroup() + " (0)");
                values.push_back("group:" + rig.getLitGroup());
            }
            if (legacy)
            {
                std::string custom;
                for (S32 i = 0; i < 5; ++i) if (rig.getGroupSlots() & (1u << i))
                {
                    if (!custom.empty()) custom += " + ";
                    custom += i == 0 ? "You" : llformat("%c", 'A' + i - 1);
                }
                labels.push_back("Custom (old scene): " + custom); values.push_back("legacy");
            }
            labels.push_back("Make a group..."); values.push_back("make");
            if (!mLitTarget->isListVisible())
            {
                mLitTarget->removeall();
                for (size_t i = 0; i < labels.size(); ++i) mLitTarget->add(labels[i], values[i]);
                mLitTargetLabels = labels; mLitTargetValues = values;
                mLitComboCastRevision = cast.getActorChoiceRevision();
                mLitComboGroupRevision = cast.getGroupRevision();
                mLitComboSlot = slot;
                mLitComboGroup = rig.getLitGroup();
                mLitComboSlots = rig.getGroupSlots();
                mLitComboLegacy = legacy;
                mLitTargetForceRefresh = false;
            }
        }
        if (!mLitTarget->isListVisible())
            setValueIfChanged(mLitTarget, LLSD(!rig.getLitGroup().empty() ? "group:" + rig.getLitGroup() : legacy ? "legacy" : "subject"), true);
        setEnabledIfChanged(mLitTarget, rig.getObjectTarget().isNull());
        clearRigLabels("lights_status");
        std::string names;
        const bool on = ALCineLightRigManager::instance().isSlotEnabled(
            static_cast<ALCineLightRigManager::Slot>(slot));
        if (on && !rig.getLitGroup().empty() && rig.getObjectTarget().isNull())
        for (const auto& id : rig.lastResolvedStoredMembers())
        {
            if (id.isNull()) continue;
            if (!names.empty()) names += ", ";
            names += actorName(id, "lights_status");
        }
        if (on && (rig.getLitGroup().empty() || rig.getObjectTarget().notNull())) names = rigTargetDescription(slot, "lights_status");
        else names += llformat(" (%d of %d here)", static_cast<S32>(rig.litGroupFallback() ? 0 : rig.lastResolvedMembers().size()), rig.litGroupSize());
        const bool no_subject = slot != 0 && ids[slot].isNull();
        if (!on) names = "Off";
        else if (no_subject && rig.lastResolvedMembers().empty() && rig.getObjectTarget().isNull()) names = "No subject assigned";
        setTextIfChanged(mLitMembers, "Lit now: " + names);
        setToolTipIfChanged(mLitMembers, "Lit now: " + names);
        const auto chip = [](LLTextBox* control, const std::string& text)
        {
            setTextIfChanged(control, text);
            ALKitRow::setControlVisible(control, !text.empty());
        };
        chip(mLitFallback, on && rig.getObjectTarget().isNull() && rig.litGroupFallback()
            ? (no_subject ? "Group empty, missing or away: no subject assigned" : "Group empty, missing or away: lighting this rig's subject") : "");
        chip(mLitCap, on && rig.getObjectTarget().isNull() && rig.litGroupPresent() > ALCineLightRigModel::LIT_GROUP_MAX
            ? llformat("Lighting the first 16 of %d", rig.litGroupPresent()) : "");
        auto& manager = ALCineLightRigManager::instance();
        S32 unlit = manager.actorOnlyUnlit(rig.lastResolvedMembers());
        bool actor_only = rig.actorOnlyAll() || rig.catchlightActorOnlyEffective();
        for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i) actor_only |= rig.lightActorOnly(i);
        chip(mLitActorOnly, on && rig.getObjectTarget().isNull() && actor_only && manager.actorOnlyOverflow() && unlit > 0
            ? llformat("Actor-only lights reach 6 people; %d members aren't lit by actor-only lights", unlit) : "");
        chip(mLitWide, on && rig.getObjectTarget().isNull() && rig.litGroupWide() ? "Wide spread: group framing has reached the light radius limit" : "");
        return;
    }
    if (!mGroupEnable) return;
    const bool legacy_controls = rig.getLitGroup().empty() && (rig.isGroupEnabled() || rig.getGroupSlots() != 0);
    ALKitRow::setControlVisible(mGroupEnable, legacy_controls);
    if (!legacy_controls)
    {
        clearRigLabels("checks");
        for (auto* control : mGroupSlotChecks) ALKitRow::setControlVisible(control, false);
        mDisplayedGroupSlots = ~0u;
        setTextIfChanged(mGroupStatus, rig.getLitGroup().empty() ? std::string() : "Group: " + rig.getLitGroup() + " (edit in Director)");
        return;
    }
    ALKitRow::setControlVisible(mGroupSlotChecks[0], true);
    const bool enabled = rig.isGroupEnabled();
    const U32 slots = rig.getGroupSlots();
    const U32 resolved = rig.lastResolvedGroupSlots();
    const LLUUID& object_target = rig.getObjectTarget();
    // A rejected On click can leave the model unchanged. Correct the widget
    // even when the rest of the displayed-model cache is still current.
    setValueIfChanged(mGroupEnable, LLSD(enabled), true);
    if (enabled == mDisplayedGroupEnabled &&
        slots == mDisplayedGroupSlots &&
        resolved == mDisplayedResolvedSlots &&
        object_target == mDisplayedGroupObjectTarget)
    {
        return;
    }

    clearRigLabels("checks");
    setEnabledIfChanged(mGroupEnable, object_target.isNull() && rig.getLitGroup().empty());
    const LLUUID subjects[] = { ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_A),
        ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_B),
        ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_C),
        ALCineLightRigManager::instance().groupSlotActor(ALCineLightRigManager::SLOT_D) };
    U32 self_slot_mask = 1u;
    for (S32 i = 0; i < 4; ++i)
        if (subjects[i].notNull() && LLDirectorCast::isSelf(subjects[i]))
            self_slot_mask |= 1u << (i + 1);
    const bool self_selected = (slots & self_slot_mask) != 0;
    for (S32 i = 0; i < 5; ++i)
    {
        const bool duplicate_self = i > 0 && (self_slot_mask & (1u << i));
        setValueIfChanged(mGroupSlotChecks[i], LLSD((i == 0 || duplicate_self)
            ? self_selected : (slots & (1u << i)) != 0), true);
        setEnabledIfChanged(mGroupSlotChecks[i], object_target.isNull() && rig.getLitGroup().empty());
    }
    std::string self_letters;
    for (S32 i = 0; i < 4; ++i)
    {
        const bool duplicate_self = subjects[i].notNull() &&
                                    LLDirectorCast::isSelf(subjects[i]);
        ALKitRow::setControlVisible(mGroupSlotChecks[i + 1], subjects[i].notNull() && !duplicate_self);
        if (duplicate_self)
        {
            if (!self_letters.empty()) self_letters += ", ";
            self_letters += char('A' + i);
        }
    }
    auto label = [this](S32 i, const std::string& text)
    {
        if (mRigKitPage)
            if (ALKitRow* row = mRigKitPage->findRow(mGroupSlotChecks[i]->getName()))
            {
                row->setDisplayLabel(text);
                return;
            }
        setLabelIfChanged(mGroupSlotChecks[i], text);
    };
    for (S32 i = 0; i < 4; ++i)
    {
        if (subjects[i].notNull() && !LLDirectorCast::isSelf(subjects[i]))
            label(i + 1, rigLabelBase(i + 1) + " - " + rigActorName(subjects[i], "checks"));
        else
            label(i + 1, llformat("Subject %c", 'A' + i));
    }
    label(0, self_letters.empty()
        ? "You" : (self_letters.size() == 1
            ? "You (Subject " + self_letters + ")"
            : "You (Subjects " + self_letters + ")"));
    setToolTipIfChanged(mGroupSlotChecks[0], self_letters.empty()
        ? "Include you in the lit group."
        : "The hidden subject toggle is the same identity; this one switch controls you once.");

    if (object_target.notNull())
    {
        setTextIfChanged(mGroupStatus, std::string("object: group is avatar-only"));
        mGroupStatus->setColor(mRadiusDefaultColor);
    }
    else if (!enabled)
    {
        setTextIfChanged(mGroupStatus, std::string());
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
            setTextIfChanged(mGroupStatus, std::string("none resolved"));
            mGroupStatus->setColor(mRadiusWarningColor);
        }
        else
        {
            setTextIfChanged(mGroupStatus, llformat(
                "%d of %d lit", resolved_count, requested_count));
            mGroupStatus->setColor(mRadiusDefaultColor);
        }
    }
    mDisplayedGroupEnabled = enabled;
    mDisplayedGroupSlots = slots;
    mDisplayedResolvedSlots = resolved;
    mDisplayedGroupObjectTarget = object_target;
}

void ALPanelCineLightRig::adjustSelectedLightAim(const char* axis, F32 delta)
{
    const std::string setting = fixtureSettingPrefix() + axis;
    F32 value = gSavedSettings.getF32(setting) + delta;
    if (std::string(axis) == "Yaw")
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

// ui-draw-lint: allow-function(raw-write) setup combo rebuild runs only for a pending or explicit refresh
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
        if (ALKitShell::find(this))
            clearKitComboSelection(mSetupCombo);
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
    if (ALKitShell::find(this))
        clearKitComboSelection(mSetupCombo);
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
        if (ALKitShell::find(this))
            clearKitComboSelection(mSetupCombo);
        syncEasyModeForSelected(true);
    }
}

void ALPanelCineLightRig::onFlarePresetSelected(LLComboBox* source)
{
    if (ALPresetLook::replaying()) return;
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
    ALPresetLook::Load load;
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
    if (ALPresetLook::replaying()) return;
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

// [RigRimPreset] Mirrors onNightMaskPresetSelected: apply the row, then snap
// the combo back to the sentinel -- a preset is an action ("apply this
// look"), not a persistent mode. Enable/Rim-only/Probes and the rig's own
// light colours/positions are never touched.
// [TronT1] See the header comment.
bool ALPanelCineLightRig::applyRigRimPresetByName(const std::string& name)
{
    return ALRigRimController::applyPresetByName(name);
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
    {
        ALKitHistory::ScopedSuspend history_suspend;
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
        mCastListInitialized = false;
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
    }
    ALKitHistory::globalBarrier("rig reset");
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
    static LLCachedControl<U32> seed_control(
        gSavedSettings, "CineLightRigSeed", 1u);
    const U32 seed = seed_control;
    if ((mSeedInitialized && seed == mDisplayedSeed) ||
        (!force && (mSeedEditor->hasFocus() ||
                    gFocusMgr.childHasKeyboardFocus(mSeedEditor))))
    {
        return;
    }
    setTextIfChanged(mSeedEditor, llformat("%u", seed), force);
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
    static LLCachedControl<F32> key_ev(gSavedSettings, "CineLightRigKeyEV", 0.f);
    static LLCachedControl<bool> rim_on(gSavedSettings, "CineLightRigRimOn", false);
    static LLCachedControl<F32> rim_ev(gSavedSettings, "CineLightRigRimEV", 0.f);
    static LLCachedControl<bool> bg_on(gSavedSettings, "CineLightRigBgOn", false);
    static LLCachedControl<F32> bg_ev(gSavedSettings, "CineLightRigBgEV", 0.f);
    if (std::fabs((F32)key_ev) > 0.01f)
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

    return (!(bool)rim_on || is_bucket((F32)rim_ev, true)) &&
           (!(bool)bg_on || is_bucket((F32)bg_ev, false));
}

bool ALPanelCineLightRig::normalizeSelectedForEasy(bool explicit_entry)
{
    // Passive display synchronization must preserve authored preset values and
    // the values just captured by preset undo, including near-zero EVs.
    if (!explicit_entry) return true;
    // Fold the key's own EV into Master so subject exposure is preserved while
    // Key is anchored at 0. If the folded value cannot be represented within
    // Master EV's +/-16 range, refuse the fold and leave the rig for Advanced
    // (render would otherwise clamp Master and silently drop exposure).
    static LLCachedControl<F32> key_ev_control(
        gSavedSettings, "CineLightRigKeyEV", 0.f);
    static LLCachedControl<F32> master_ev_control(
        gSavedSettings, "CineLightRigMasterEV", 0.f);
    const F32 key_ev = key_ev_control;
    const F32 folded = (F32)master_ev_control + key_ev;
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
    // click may snap. Passive sync returns above without rewriting near-bucket
    // authored values on first show / slot switch / scene load.
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
    // Recompute the desired mode on every call, not just on a slot change: a
    // Director scene or preset can replace the selected slot's settings in
    // place. If that makes an active-Easy instance no longer Easy-native (e.g.
    // an old scene restoring a non-zero Key EV), we must demote to Advanced so
    // the disabled EV/Ratio controls re-enable and a later Brightness commit
    // does not zero Key EV without folding it into Master.
    static LLCachedControl<bool> easy_mode(
        gSavedSettings, "CineLightRigEasyMode", false);
    syncEasyModeState((bool)easy_mode,EasyModeOrigin::PASSIVE,force);
}

bool ALPanelCineLightRig::applyEasyMode(bool desired,EasyModeOrigin origin,bool force)
{
    if(origin==EasyModeOrigin::USER)mEasyModeOverride=-1;
    if(mSyncingEasyControls||mApplyingEasyMode)
    {
        return mEasyModeActive;
    }
    static LLCachedControl<bool> easy_mode(
        gSavedSettings,"CineLightRigEasyMode",false);
    if(origin==EasyModeOrigin::USER&&(bool)easy_mode!=desired)
    {
        mApplyingEasyMode=true;
        gSavedSettings.setBOOL("CineLightRigEasyMode",desired);
        mApplyingEasyMode=false;
    }
    return syncEasyModeState(desired,origin,force);
}

bool ALPanelCineLightRig::syncEasyModeState(bool desired,EasyModeOrigin origin,bool force)
{
    if (mSyncingEasyControls||mApplyingEasyMode)
    {
        return mEasyModeActive;
    }
    const S32 selected=static_cast<S32>(ALCineLightRigManager::instance().selectedSlot());
    const bool explicit_entry=origin==EasyModeOrigin::USER;
    if(selected!=mEasyModeSlot)mEasyModeOverride=-1;
    if(origin==EasyModeOrigin::SEARCH)
    {
        if(desired&&!selectedIsEasyNative())return mEasyModeActive;
        mEasyModeOverride=desired;
    }
    const bool displayed=mEasyModeOverride<0?desired:mEasyModeOverride!=0;
    const bool effective=displayed&&(explicit_entry||selectedIsEasyNative());
    if(!force&&!explicit_entry&&selected==mEasyModeSlot&&effective==mEasyModeActive)
    {
        return mEasyModeActive;
    }
    mSyncingEasyControls=true;
    mEasyModeSlot=selected;
    mEasyModeActive=effective;
    if(mEasyModeActive)
    {
        if(explicit_entry)
        {
            if(!normalizeSelectedForEasy(true))mEasyModeActive=false;
        }
        else
        {
            ALKitHistory::ScopedSuspend suspend;
            if(!normalizeSelectedForEasy(false))mEasyModeActive=false;
        }
    }
    mSyncingEasyControls=false;
    if(explicit_entry||ALKitShell::find(this))syncEasyControls();
    return mEasyModeActive;
}

void ALPanelCineLightRig::onEasyModeCommit()
{
    applyEasyMode(mEasyModeToggle->getValue().asBoolean(),EasyModeOrigin::USER);
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
    static LLCachedControl<bool> manual_control(
        gSavedSettings, "CineLightRigManual", false);
    static LLCachedControl<S32> fx_control(
        gSavedSettings, "CineLightRigFX", -1);
    const bool manual = manual_control;
    if (manual && (S32)fx_control != -1)
    {
        // Manual owns the pose; drop any running effect and hold it off.
        gSavedSettings.setS32("CineLightRigFX", -1);
    }
    // Lock the effect selector while manual so an effect cannot silently take
    // over per-light aim/exposure/colour again.
    setEnabledIfChanged(mFXCombo, !manual);
    setEnabledIfChanged(mFXStop, !manual); // [UIPerf] cached in postBuild
}

void ALPanelCineLightRig::syncEasyControls()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightRig syncEasyControls");
    mSyncingEasyControls = true;
    setValueIfChanged(mEasyModeToggle, LLSD(mEasyModeActive), true);

    static LLCachedControl<F32> master_ev(gSavedSettings, "CineLightRigMasterEV", 0.f);
    static LLCachedControl<bool> ratio_lock(gSavedSettings, "CineLightRigRatioLock", false);
    static LLCachedControl<F32> ratio(gSavedSettings, "CineLightRigRatio", 0.f);
    static LLCachedControl<F32> key_ev(gSavedSettings, "CineLightRigKeyEV", 0.f);
    static LLCachedControl<F32> fill_ev(gSavedSettings, "CineLightRigFillEV", 0.f);
    static LLCachedControl<bool> rim_on(gSavedSettings, "CineLightRigRimOn", false);
    static LLCachedControl<F32> rim_ev(gSavedSettings, "CineLightRigRimEV", 0.f);
    static LLCachedControl<bool> bg_on(gSavedSettings, "CineLightRigBgOn", false);
    static LLCachedControl<F32> bg_ev(gSavedSettings, "CineLightRigBgEV", 0.f);
    static LLCachedControl<F32> warmth(gSavedSettings, "CineLightRigMasterTempMired", 0.f);
    static LLCachedControl<S32> key_beam(gSavedSettings, "CineLightRigKeyBeam", 0);
    static LLCachedControl<F32> feather(gSavedSettings, "BDMergeProjectorVolumetricsFeather", 0.f);

    const auto set_unfocused = [](LLUICtrl* control, const LLSD& value)
    {
        if (!control->hasFocus() &&
            !gFocusMgr.childHasKeyboardFocus(control))
        {
            // [UIPerfC1] own guard kept; only the compare moves to the helper
            setValueIfChanged(control, value, true);
        }
    };
    set_unfocused(mEasyBrightness, LLSD(
        ALCineLightRigModel::easyBrightnessClamp(
            (F32)master_ev)));
    const F32 drama = (bool)ratio_lock
        ? (F32)ratio : (F32)key_ev - (F32)fill_ev;
    set_unfocused(mEasyDrama, LLSD(
        ALCineLightRigModel::easyDramaClamp(drama)));
    set_unfocused(mEasyRim, LLSD(
        ALCineLightRigModel::rimPresenceFromEV(
            (bool)rim_on, (F32)rim_ev)));
    set_unfocused(mEasyBg, LLSD(
        ALCineLightRigModel::bgPresenceFromEV(
            (bool)bg_on, (F32)bg_ev)));
    set_unfocused(mEasyWarmth, LLSD(
        (F32)warmth));
    set_unfocused(mEasyConeWidth, LLSD(
        ALCineLightRigModel::easyConeWidthFromBeam(
            (S32)key_beam)));
    set_unfocused(mEasyConeFeather, LLSD(
        (F32)feather));
    for (S32 light = 0; light < ALCineLightRigModel::LIGHT_COUNT; ++light)
    {
        // [UIPerfC1] cached role x field control; a missing control is not written
        S32 beam = 0;
        if (fixtureSetting(light, FF_BEAM, beam))
        {
            set_unfocused(mManualConeWidths[light], LLSD(
                ALCineLightRigModel::easyConeWidthFromBeam(beam)));
        }
        setEnabledIfChanged(mManualConeWidths[light], !mEasyModeActive);
    }

    setEnabledIfChanged(mEasyBrightness, mEasyModeActive);
    setEnabledIfChanged(mEasyDrama, mEasyModeActive);
    setEnabledIfChanged(mEasyRim, mEasyModeActive);
    setEnabledIfChanged(mEasyBg, mEasyModeActive);
    setEnabledIfChanged(mEasyWarmth, mEasyModeActive);
    setEnabledIfChanged(mEasyConeWidth, mEasyModeActive);
    setEnabledIfChanged(mEasyConeFeather, mEasyModeActive);
    // [UIPerf] The Easy card's gated controls (Shaft length, Flare preset, Live
    // Probe row, Night row) and their reset buttons are direct control_name
    // bindings / apply-and-snap-back combos with no remap handler, so they are
    // gated on Easy mode here. Pointers cached in postBuild() (were findChild
    // by name, 16x per frame); the Easy reset buttons write the same backing
    // settings, so they are gated too.
    for (LLUICtrl* control : mEasyGatedControls)
    {
        if (control)
        {
            setEnabledIfChanged(control, mEasyModeActive);
        }
    }
    for (LLUICtrl* button : mEasyResetButtons)
    {
        if (button)
        {
            setEnabledIfChanged(button, mEasyModeActive);
        }
    }
    for (LLUICtrl* control : mAdvancedDrivenControls)
    {
        setEnabledIfChanged(control, !mEasyModeActive);
    }
    mSyncingEasyControls = false;
}

// [ActorMask] Actor only is avatar-only in v1: the registry skips object-target
// rigs, so the six controls (and their reset buttons) are disabled for them and
// carry an explanation. The saved values are left untouched, so un-targeting the
// rig restores the user's choices. Runs from updateDerivedStatus(); the cached
// lock state keeps the tooltip writes off the per-frame path.
// The rig-level master "Actor only (all lights)" is locked by an object target
// too, and while it is on it additionally greys the four per-light boxes and the
// catchlight box (their stored values are never touched). The cached state
// (object-target bit | master bit) keeps all of this off the per-frame path
// except for one compare.
void ALPanelCineLightRig::syncActorOnlyControls(
    bool object_targeted, bool actor_only_all)
{
    const S32 state = (object_targeted ? 1 : 0) | (actor_only_all ? 2 : 0);
    if (state == mActorOnlyLockState)
    {
        return;
    }
    mActorOnlyLockState = state;
    static const std::string LOCKED_TIP =
        "Actor only isn't supported for object targets in this version. "
        "With an object target these lights light normally.";
    static const std::string OVERRIDDEN_TIP =
        "Ticked by \"Actor only (all lights)\". "
        "Untick that to set lights individually.";
    for (S32 i = 0; i < ACTOR_ONLY_CONTROL_COUNT; ++i)
    {
        if (!mActorOnlyControls[i] || !mActorOnlyResets[i])
        {
            continue;
        }
        // Bounce lights the set is not overridden by the master; only the four
        // per-light boxes and the catchlight box are.
        const bool overridden = actor_only_all &&
            i != ALCineLightRigModel::LIGHT_COUNT &&
            i != ACTOR_ONLY_MASTER_INDEX;
        const bool enabled = !object_targeted && !overridden;
        setEnabledIfChanged(mActorOnlyControls[i], enabled);
        setEnabledIfChanged(mActorOnlyResets[i], enabled);
        setToolTipIfChanged(mActorOnlyControls[i], object_targeted ? LOCKED_TIP
            : (overridden ? OVERRIDDEN_TIP : mActorOnlyTips[i]));
    }
}

void ALPanelCineLightRig::onActorOnlyAllChanged(bool enabled)
{
    // [ActorMask] KEY, FILL, RIM, BG, catchlight -- the settings the per-light
    // boxes are bound to. Writing them is exactly what ticking each box does.
    static const char* const NAMES[5] = {
        "CineLightRigKeyActorOnly", "CineLightRigFillActorOnly",
        "CineLightRigRimActorOnly", "CineLightRigBgActorOnly",
        "CineLightRigCatchlightActorOnly"
    };
    for (S32 i = 0; i < 5; ++i)
    {
        if (gSavedSettings.getBOOL(NAMES[i]) != enabled) gSavedSettings.setBOOL(NAMES[i], enabled);
    }
}

void ALPanelCineLightRig::onClickShadowFixIt()
{
    const S32 requested = computeRequestedShadowSlots();
    gSavedSettings.setU32("BDMergeMaxSpotShadows",
        std::clamp(static_cast<U32>(requested), 2u, 10u));
}

void ALPanelCineLightRig::updateDerivedStatus()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightRig updateDerivedStatus");
    static LLCachedControl<bool> ratio_lock_control(gSavedSettings, "CineLightRigRatioLock", false);
    static LLCachedControl<F32> key_ev_control(gSavedSettings, "CineLightRigKeyEV", 0.f);
    static LLCachedControl<F32> ratio_control(gSavedSettings, "CineLightRigRatio", 0.f);
    static LLCachedControl<F32> fill_ev_control(gSavedSettings, "CineLightRigFillEV", 0.f);
    static LLCachedControl<bool> actor_only_all_control(gSavedSettings, "CineLightRigActorOnlyAll", false);
    static LLCachedControl<S32> shadow_mode_control(gSavedSettings, "CineLightRigShadowMode", 0);
    static LLCachedControl<bool> key_on(gSavedSettings, "CineLightRigKeyOn", true);
    static LLCachedControl<bool> fill_on(gSavedSettings, "CineLightRigFillOn", true);
    static LLCachedControl<bool> rim_on(gSavedSettings, "CineLightRigRimOn", true);
    static LLCachedControl<bool> bg_on(gSavedSettings, "CineLightRigBgOn", true);
    static LLCachedControl<U32> shadow_slots(gSavedSettings, "BDMergeMaxSpotShadows", 2u);
    static LLCachedControl<F32> rig_radius(gSavedSettings, "CineLightRigRadius", 4.f);
    static LLCachedControl<S32> refresh_control(gSavedSettings, "CineLightRigLiveProbeRefresh", 3);
    static LLCachedControl<bool> night_enabled(gSavedSettings, "CineLightRigNightMaskEnabled", false);
    static LLCachedControl<bool> hdr_enabled(gSavedSettings, "RenderHDREnabled", false);
    static LLCachedControl<S32> night_target(gSavedSettings, "CineLightRigNightMaskTarget", 0);
    syncObjectTargetControls();
    syncEasyControls();
    ALCineLightRig& rig = ALCineLightRigManager::instance().selected();
    const bool ratio_locked = ratio_lock_control;
    setEnabledIfChanged(mFillEV, !mEasyModeActive && !ratio_locked);
    if (!mFillEV->hasFocus() && !gFocusMgr.childHasKeyboardFocus(mFillEV))
    {
        const F32 displayed_fill_ev = ratio_locked
            ? (F32)key_ev_control -
                std::clamp((F32)ratio_control,
                           0.f, 5.f)
            : (F32)fill_ev_control;
        setValueIfChanged(mFillEV, LLSD((F64)displayed_fill_ev));
    }
    for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
    {
        setVisibleIfChanged(mClipStatus[i], rig.isClipped(i));
        setValueIfChanged(mShaftControls[i], LLSD(rig.isShaftEnabled(i)), true);
        setValueIfChanged(mHeroControls[i], LLSD(rig.isHeroEnabled(i)), true);
    }
    syncActorOnlyControls(rig.getObjectTarget().notNull(),
        (bool)actor_only_all_control); // [ActorMask]

    const S32 mode = std::clamp((S32)shadow_mode_control, 0, 2);
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
            !(i == 0 ? (bool)key_on : i == 1 ? (bool)fill_on :
              i == 2 ? (bool)rim_on : (bool)bg_on))
        {
            shaft_on_dark_light = true;
        }
    }
    // BDMergeMaxSpotShadows is a U32 setting; getS32 on it failed conversion
    // every panel update (log flood) and returned the wrong value.
    const S32 slots = (S32)(U32)shadow_slots;
    const S32 hint_state = requested > slots ? 1
        : shaft_suppressed ? 2
        : shaft_on_dark_light ? 3 : 0;
    if (hint_state != mShadowHintState ||
        (hint_state == 1 &&
         (requested != mShadowHintRequested || slots != mShadowHintSlots)))
    {
        setVisibleIfChanged(mShadowHint, hint_state != 0);
        setVisibleIfChanged(mShadowFixIt, hint_state == 1);
        if (hint_state == 1)
        {
            const S32 displayed_request = std::clamp(requested, 2, 10);
            setTextIfChanged(mShadowHint, llformat(
                "%d rig projectors request %d shadow slots. Raise Max Spot "
                "Shadows or use Key only; shafts also require a slot.",
                displayed_request, slots));
            setLabelIfChanged(mShadowFixIt, llformat(
                "Allow %d spot shadows", displayed_request));
        }
        else if (hint_state == 2)
        {
            setTextIfChanged(mShadowHint, std::string(
                "A shafted light is excluded by the current shadow policy. "
                "Shafts require a shadow slot; choose All projectors "
                "compete (or Key only for KEY)."));
        }
        else if (hint_state == 3)
        {
            setTextIfChanged(mShadowHint, std::string(
                "A shaft is enabled on an Off light. Turn that light On before "
                "it can request the shadow slot required for its shaft."));
        }
        mShadowHintState = hint_state;
        mShadowHintRequested = requested;
        mShadowHintSlots = slots;
    }
    const bool radius_over_ceiling =
        (F32)rig_radius >
        ALCineLightRigModel::SCALED_RADIUS_CEIL;
    if (!mRadiusCueInitialized ||
        radius_over_ceiling != mRadiusOverCeiling)
    {
        mRadiusLabel->setColor(radius_over_ceiling
            ? mRadiusWarningColor : mRadiusDefaultColor);
        mRadiusOverCeiling = radius_over_ceiling;
        mRadiusCueInitialized = true;
    }
    const ALCineLightRigManager& manager = ALCineLightRigManager::instance();
    const ALCineLightRigManager::LiveProbeState probe_state = manager.liveProbeState();
    std::string probe_state_text;
    switch (probe_state)
    {
        case ALCineLightRigManager::LiveProbeState::DISABLED:
            probe_state_text = "Disabled";
            break;
        case ALCineLightRigManager::LiveProbeState::UNAVAILABLE:
            probe_state_text = "Unavailable: enable probe coverage and at least 2 slots";
            break;
        case ALCineLightRigManager::LiveProbeState::WAITING_FOR_TARGET:
            probe_state_text = "Waiting for the target rig";
            break;
        case ALCineLightRigManager::LiveProbeState::WARMING:
            probe_state_text = llformat("Warming... %d%%",
                ll_round(manager.liveProbeFade() * 100.f));
            break;
        case ALCineLightRigManager::LiveProbeState::LIVE:
            probe_state_text = "Live";
            break;
    }
    {
        // [LiveProbeRefresh] "Refresh now" only means something in On change
        // and, [ProbeManualRate], in Manual.
        const ALCineLiveProbeRefresh::Mode refresh_mode =
            ALCineLiveProbeRefresh::sanitizeMode(
                (S32)refresh_control);
        setEnabledIfChanged(mLiveProbeRefreshNow,
            refresh_mode == ALCineLiveProbeRefresh::Mode::ON_CHANGE ||
            refresh_mode == ALCineLiveProbeRefresh::Mode::MANUAL);
        // [ProbeManualRate] the Faces / Every spinners only apply in Manual.
        const bool manual_rate = refresh_mode == ALCineLiveProbeRefresh::Mode::MANUAL;
        setEnabledIfChanged(mLiveProbeManualFaces, manual_rate);
        setEnabledIfChanged(mLiveProbeManualFrames, manual_rate);
        setEnabledIfChanged(mLiveProbeManualFacesReset, manual_rate);
        setEnabledIfChanged(mLiveProbeManualFramesReset, manual_rate);
    }
    std::string probe_live = probe_state_text;
    if (probe_state == ALCineLightRigManager::LiveProbeState::WARMING ||
        probe_state == ALCineLightRigManager::LiveProbeState::LIVE)
    {
        probe_live += "\n" + liveProbeRefreshLine(
            gPipeline.mReflectionMapManager.getCinematicRefreshStatus());
    }
    std::string probe_status;
    if (probe_state != ALCineLightRigManager::LiveProbeState::DISABLED)
    {
        // Effective fill ambiance, computed exactly as LLReflectionMapManager
        // does (max of the sky's probe ambiance and this probe's): a probe
        // Ambiance at or below the sky floor has no visible effect.
        static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        static LLCachedControl<F32> probe_ambiance(gSavedSettings, "CineLightRigLiveProbeAmbiance", 1.f);
        static LLCachedControl<F32> bounce_keep(gSavedSettings, "CineLightRigLiveProbeBounceKeep", 1.f);
        F32 sky_floor = 0.f;
        if (LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky())
        {
            sky_floor = psky->getReflectionProbeAmbiance(should_auto_adjust);
        }
        const F32 probe_amb = llclamp((F32)probe_ambiance, 0.f, 8.f);
        const F32 effective = llmax(sky_floor, probe_amb);
        const F32 keep = llclamp((F32)bounce_keep, 0.f, 1.f);
        probe_status = llformat("Fill: probe %.2f, sky floor %.2f -> %.2f%s. Rig bounce kept %d%%",
            probe_amb, sky_floor, effective,
            (probe_amb <= sky_floor) ? " (probe Ambiance has no effect)" : "",
            ll_round(keep * 100.f));
    }
    if (!mLiveProbeStatusSet || probe_status != mLastLiveProbeStatus) // [UIPerf]
    {
        setTextIfChanged(mLiveProbeStatus, probe_status);
        mLastLiveProbeStatus = probe_status;
        mLiveProbeStatusSet = true;
    }
    const S32 probe_state_value = static_cast<S32>(probe_state);
    const bool probe_state_changed = probe_state_value != mLastLiveProbeState;
    const bool probe_live_due = mLiveProbeClock.due();
    if ((probe_state_changed || probe_live_due) &&
        isWidgetOnScreen(mLiveProbeLiveStatus))
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LightRig liveProbeStatus setText");
        setTextIfChanged(mLiveProbeLiveStatus, probe_live);
        mLastLiveProbeState = probe_state_value;
    }
    if (mNightMaskStatus)
    {
        std::string night_status;
        if ((bool)night_enabled)
        {
            // M1: Night Mask only runs inside LLPipeline::renderFinalize's
            // `if (hdr)` block — mirror that exact gate here so the status
            // line agrees with what actually renders.
            const bool hdr_active = gGLManager.mGLVersion > 4.05f &&
                (bool)hdr_enabled;
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
                switch ((S32)night_target)
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
        if (!mNightMaskStatusSet || night_status != mLastNightMaskStatus) // [UIPerf]
        {
            setTextIfChanged(mNightMaskStatus, night_status);
            mLastNightMaskStatus = night_status;
            mNightMaskStatusSet = true;
        }
    }
    std::string setup_name = mSetupCombo
        ? mSetupCombo->getSimple() : std::string();
    LLStringUtil::trim(setup_name);
    setEnabledIfChanged(mSetupSave, !setup_name.empty() &&
                           !ALCineLightRig::isMasterSetup(setup_name) &&
                           !ALCineLightRig::isSetupDecorationName(
                               setup_name));
    const std::string selected = mSetupCombo
        ? mSetupCombo->getSelectedValue().asString() : std::string();
    setEnabledIfChanged(mSetupDelete, !selected.empty() &&
                             !ALCineLightRig::isMasterSetup(selected) &&
                             !ALCineLightRig::isSetupDecorationName(selected));

    // [TronT1] Tron tint row (Rig Rim card): only meaningful once Rig Rim
    // itself is enabled. findChild every draw (cheap, same idiom as the
    // clip/shaft/hero status loop above) rather than a dedicated settings
    // signal connection for three rows.
    mRim.tickRefresh();
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
    if (!mFlowGrid)
    {
        mFlowGrid = findChildView("cine_light_rig_flow_grid"); // [UIPerf] lazy; reshape may precede postBuild
    }
    if (LLView* grid = mFlowGrid)
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

// ui-draw-lint: allow-function(raw-write) privacy invalidation runs at 1 Hz and only rebuilds on a visibility change
void ALPanelCineLightRig::refreshRigPrivacy()
{
    refreshRigNotices();
    // Closed menus no longer display their snapshot. Do not expand this set
    // from Cast: unrelated actors must not invalidate this page's privacy.
    if (!mRigGroupMenu.get() || !mRigGroupMenu.get()->getVisible()) mRigDisplayedLabels.erase("menu");
    bool changed = mRigNearbyAllowed != ALGazeNearby::nearbyAllowed();
    for (const auto& area : mRigDisplayedLabels)
        for (const auto& label : area.second)
        {
            const bool visible = rigNameVisible(label.first);
            changed |= visible != label.second;
        }
    if (!changed) return;
    mRigNearbyAllowed = ALGazeNearby::nearbyAllowed();
    clearRigLabels("menu");
    clearRigLabels("chip");
    clearRigLabels("notice");
    retireRigMenu(mRigGroupMenu);
    // The shell's generic who picker owns independent snapshots of rig labels.
    if (auto* shell = ALKitShell::find(this)) shell->hidePopover();
    mCastListInitialized = false;
    mSubjectChoiceRevision = ~U64(0);
    mLitTargetForceRefresh = true;
    mDisplayedGroupSlots = ~0u;
    if (mAnchorCombo)
    {
        mAnchorCombo->hideList();
        mAnchorCombo->setFocus(false);
    }
    if (mLitTarget) mLitTarget->hideList();
    for (auto* combo : mSubjectTargetCombos) if (combo) combo->hideList();
    updateAnchorList();
    syncGroupControls();
    if (mRigKitPage) mRigKitPage->refreshWhoUnavailable();
}

void ALPanelCineLightRig::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; // [UIPerfC1]
    if (!isWidgetOnScreen(this))
    {
        return;
    }
    // These two calls enforce model state, not just presentation. Keep their
    // pre-batch per-frame cadence; their UI writes are diff-guarded.
    syncEasyModeForSelected();
    applyManualLock();
    if (mPanelRefreshClock.due())
    {
        if (mRigPrivacyClock.due()) refreshRigPrivacy();
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
        syncProjectorFeeds();
        updateDerivedStatus();
    }
    mRim.tickFrame();
    LLPanel::draw();
}

void ALPanelCineLightRig::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        syncEasyModeForSelected();
        mRim.onReveal();
        mLiveProbeClock.forceDue();
        refreshRigPrivacy();
        updateAnchorList();
        syncObjectTargetControls(true);
        syncGroupControls();
        syncFixtureControls(true);
        syncGoboLibrary(true);
        syncProjectorFeeds(true);
        refreshSetupList(mSetupCombo
            ? mSetupCombo->getSelectedValue().asString() : std::string(),
            true);
        updateDerivedStatus();
        // The reveal pass above is the immediate refresh; consume the first
        // 10 Hz tick so draw cannot repeat the same work in that frame.
        (void)mPanelRefreshClock.due();
    }
    else refreshRigNotices(true);
    LLPanel::onVisibilityChange(new_visibility);
}
