/**
 * @file alrotoink.cpp
 * @brief Rotoscope Ink: settings -> Lightbox UI / Director scene list glue.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alrotoink.h"

#include "llviewercontrol.h"
#include "v3color.h"

namespace
{
// [RotoInk] One preset = one fully-specified ink look. Every field is
// written on apply so switching presets never inherits a previous preset's
// colour, sketch or motion. CineOutlineEnabled and every CineOutlineSubject*
// setting are deliberately NOT part of any row (mirrors ALCineHaze's Enable/
// TintColor exclusion and the Rig Rim preset's Enabled/DebugRimOnly/
// IncludeProbes exclusion): a preset shapes the ink look, never the shot
// (subject isolation) or whether Rotoscope Ink is on at all.
struct Preset
{
    const char* mKey;
    const char* mLabel;
    F32  r, g, b;
    F32  intensity, glow;
    bool silOn;
    F32  silThr, silW;
    bool crOn;
    F32  crAngle, crW;
    S32  depthMode;
    F32  metricThr, width, soft, farCutoff; // [RotoInk] not "far": Windows headers #define far
    S32  inkMode;
    F32  reach, opacity;
    S32  blend;
    bool skOn;
    F32  skAmt, skDet, skFps;
    S32  skStrokes;
    F32  skRough, skSeed;
    S32  moStyle;
    F32  moSpeed, moAmt, moScale, moAngle;
};

// Common column (section 5): DM 0 / MT 0.10 / F 0 / IM 0 / R 6 / sketch off
// (2.0/0.45/12/2/0.30/0) / motion none (0.5/0.5/0.3/90). Rows override only
// what differs from the common column.
const Preset kPresets[] = {
    // key            label                    r      g      b     I    G     silOn  silThr silW  crOn  crAngle crW    DM MT    W    S    F  IM R  Op   B  skOn  skAmt skDet skFps skStr skRough skSeed moStyle moSpeed moAmt moScale moAngle
    { "comic",        "Comic Ink",             0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.020f,1.0f, true, 30.f,  0.70f, 0, 0.10f,2.0f,0.4f,0.f,0, 6.f,1.0f, 0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "bold",         "Bold Comic",            0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.025f,1.0f, true, 35.f,  0.80f, 0, 0.10f,4.0f,0.3f,0.f,0, 6.f,1.0f, 0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "manga",        "Manga Fine Line",       0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.015f,1.0f, true, 20.f,  0.90f, 0, 0.10f,1.0f,0.6f,0.f,0, 6.f,1.0f, 0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "anime",        "Anime Cel Line",        0.06f, 0.03f, 0.02f,1.f, 0.f,  true,  0.020f,1.0f, true, 45.f,  0.50f, 0, 0.10f,1.5f,0.5f,0.f,0, 6.f,0.95f,0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "pencil",       "Pencil Sketch",         0.12f, 0.12f, 0.13f,1.f, 0.f,  true,  0.018f,1.0f, true, 25.f,  0.60f, 0, 0.10f,1.5f,0.7f,0.f,0, 6.f,0.85f,0, true, 1.5f,0.55f,12.f,3,0.45f,7.f,  0,0.5f,0.5f,0.3f,90.f },
    { "charcoal",     "Charcoal",              0.03f, 0.03f, 0.03f,1.f, 0.f,  true,  0.020f,1.0f, true, 30.f,  0.50f, 0, 0.10f,4.0f,0.9f,0.f,0, 6.f,0.9f, 0, true, 3.0f,0.35f,8.f, 2,0.60f,3.f,  0,0.5f,0.5f,0.3f,90.f },
    { "boil",         "Rotoscope Boil",        0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.020f,1.0f, true, 28.f,  0.65f, 0, 0.10f,2.5f,0.35f,0.f,0,6.f,1.0f, 0, true, 2.0f,0.45f,12.f,1,0.0f, 0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "storyboard",   "Storyboard",            0.08f, 0.09f, 0.12f,1.f, 0.f,  true,  0.030f,1.0f, true, 40.f,  0.35f, 0, 0.10f,2.0f,0.5f,0.f,0, 6.f,0.8f, 0, true, 1.0f,0.60f,6.f, 2,0.25f,11.f, 0,0.5f,0.5f,0.3f,90.f },
    { "chalk",        "Chalk on Dark",         1.f,   1.f,   1.f,  1.f, 0.15f,true,  0.020f,1.0f, true, 25.f,  0.50f, 0, 0.10f,2.0f,0.8f,0.f,0, 6.f,0.9f, 0, true, 1.2f,0.70f,10.f,2,0.55f,5.f,  0,0.5f,0.5f,0.3f,90.f },
    { "neon",         "Neon Line",             1.0f,  0.10f, 0.60f,6.f, 1.f,  true,  0.020f,1.0f, true, 35.f,  0.40f, 0, 0.10f,2.0f,0.5f,0.f,0, 6.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "tron",         "Tron Glow",             0.15f, 0.90f, 1.0f, 8.f, 1.f,  true,  0.020f,1.0f, true, 30.f,  0.70f, 0, 0.10f,1.5f,0.4f,0.f,0, 6.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  1,0.5f,0.25f,0.3f,90.f },
    { "inkwash",      "Ink Wash",              0.05f, 0.06f, 0.10f,1.f, 0.f,  true,  0.025f,1.0f, true, 30.f,  0.30f, 0, 0.10f,5.0f,1.0f,0.f,0, 6.f,0.7f, 1, true, 1.5f,0.30f,8.f, 2,0.20f,2.f,  0,0.5f,0.5f,0.3f,90.f },
    { "blueprint",    "Blueprint",             0.85f, 0.92f, 1.0f, 1.2f,0.f,  true,  0.015f,1.0f, true, 15.f,  1.00f, 0, 0.10f,1.0f,0.3f,0.f,0, 6.f,1.0f, 0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "stainedglass", "Stained Glass",         0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.020f,1.0f, true, 12.f,  1.00f, 0, 0.10f,3.5f,0.2f,0.f,0, 6.f,1.0f, 0, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "holoscan",     "Hologram Scan",         0.30f, 1.0f,  0.80f,5.f, 0.8f, true,  0.020f,1.0f, true, 25.f,  0.60f, 0, 0.10f,1.5f,0.5f,0.f,0, 6.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  3,0.35f,0.85f,0.12f,90.f },
    { "chase",        "Chase Light",           1.0f,  0.60f, 0.15f,8.f, 1.f,  true,  0.020f,1.0f, false,30.f,  0.0f,  0, 0.10f,2.5f,0.4f,0.f,0, 6.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  2,0.6f,1.0f,0.35f,90.f },
    { "matchglow",    "Light-Matched Glow",    1.f,   1.f,   1.f,  3.f, 1.f,  true,  0.020f,1.0f, true, 40.f,  0.30f, 0, 0.10f,2.0f,0.5f,0.f,1, 8.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  0,0.5f,0.5f,0.3f,90.f },
    { "wireshimmer",  "Wire Shimmer",          0.75f, 0.85f, 1.0f, 4.f, 0.6f, true,  0.020f,1.0f, true, 20.f,  0.90f, 0, 0.10f,1.0f,0.4f,0.f,0, 6.f,1.0f, 2, false,2.0f,0.45f,12.f,2,0.30f,0.f,  5,1.2f,0.7f,0.5f,90.f },
    { "woodcut",      "Woodcut",               0.f,   0.f,   0.f,  1.f, 0.f,  true,  0.030f,1.0f, true, 20.f,  0.90f, 0, 0.10f,3.0f,0.15f,0.f,0,6.f,1.0f, 0, true, 0.6f,0.85f,24.f,1,0.35f,13.f, 0,0.5f,0.5f,0.3f,90.f },
    { "ballpoint",    "Ballpoint Doodle",      0.05f, 0.08f, 0.35f,1.f, 0.f,  true,  0.015f,1.0f, true, 25.f,  0.70f, 0, 0.10f,1.0f,0.5f,0.f,0, 6.f,1.0f, 0, true, 2.5f,0.50f,12.f,4,0.15f,21.f, 0,0.5f,0.5f,0.3f,90.f },
};

// Every CineOutline* setting that ships with the viewer (settings.xml), used
// both as the preset's write-list source of truth and as the Director /
// Lightbox key list. Order: legacy 4, then the [RotoInk] keys in
// settings.xml order.
const char* const kAllSettings[] = {
    "CineOutlineEnabled",
    "CineOutlineColor",
    "CineOutlineIntensity",
    "CineOutlineGlow",
    // [RotoInk Round-3] Ink layer (never written by a preset -- like
    // Subject*, this is "which pass timing", not part of a look).
    "CineOutlineLayer",
    // [RotoInk] Line
    "CineOutlineSilhouetteEnabled",
    "CineOutlineSilhouetteThreshold",
    "CineOutlineSilhouetteWeight",
    "CineOutlineCreaseEnabled",
    "CineOutlineCreaseAngle",
    "CineOutlineCreaseWeight",
    "CineOutlineDepthMode",
    "CineOutlineMetricThreshold",
    "CineOutlineWidth",
    "CineOutlineSoftness",
    "CineOutlineFarCutoff",
    // [RotoInk] Subject (never written by a preset)
    "CineOutlineSubjectMode",
    "CineOutlineSubjectTarget",
    "CineOutlineSubjectManualDepth",
    "CineOutlineSubjectDepthRange",
    "CineOutlineSubjectDepthFeather",
    "CineOutlineSubjectEllipse",
    "CineOutlineSubjectEllipseScale",
    "CineOutlineSubjectEllipseFeather",
    // [RotoInk] Colour
    "CineOutlineInkColorMode",
    "CineOutlineMatchReach",
    "CineOutlineOpacity",
    "CineOutlineBlendMode",
    // [RotoInk] Sketch / boil
    "CineOutlineSketchEnabled",
    "CineOutlineSketchAmount",
    "CineOutlineSketchDetail",
    "CineOutlineSketchFPS",
    "CineOutlineSketchStrokes",
    "CineOutlineSketchRoughness",
    "CineOutlineSketchSeed",
    // [RotoInk] Motion
    "CineOutlineMotionStyle",
    "CineOutlineMotionSpeed",
    "CineOutlineMotionAmount",
    "CineOutlineMotionScale",
    "CineOutlineMotionAngle",
};

void reset_setting(const char* name)
{
    if (LLControlVariable* control = gSavedSettings.getControl(name))
    {
        control->resetToDefault(true);
    }
}
} // anonymous namespace

void ALRotoInk::applyPreset(const std::string& key)
{
    const Preset* row = nullptr;
    for (const Preset& p : kPresets)
    {
        if (key == p.mKey)
        {
            row = &p;
            break;
        }
    }
    if (!row)
    {
        return; // unknown preset key: leave settings untouched
    }

    // Colour / intensity / glow.
    gSavedSettings.setUntypedValue("CineOutlineColor",
        LLColor3(row->r, row->g, row->b).getValue());
    gSavedSettings.setF32("CineOutlineIntensity", row->intensity);
    gSavedSettings.setF32("CineOutlineGlow", row->glow);

    // Line.
    gSavedSettings.setBOOL("CineOutlineSilhouetteEnabled", row->silOn);
    gSavedSettings.setF32("CineOutlineSilhouetteThreshold", row->silThr);
    gSavedSettings.setF32("CineOutlineSilhouetteWeight", row->silW);
    gSavedSettings.setBOOL("CineOutlineCreaseEnabled", row->crOn);
    gSavedSettings.setF32("CineOutlineCreaseAngle", row->crAngle);
    gSavedSettings.setF32("CineOutlineCreaseWeight", row->crW);
    gSavedSettings.setS32("CineOutlineDepthMode", row->depthMode);
    gSavedSettings.setF32("CineOutlineMetricThreshold", row->metricThr);
    gSavedSettings.setF32("CineOutlineWidth", row->width);
    gSavedSettings.setF32("CineOutlineSoftness", row->soft);
    gSavedSettings.setF32("CineOutlineFarCutoff", row->farCutoff);

    // Colour mode / opacity / blend.
    gSavedSettings.setS32("CineOutlineInkColorMode", row->inkMode);
    gSavedSettings.setF32("CineOutlineMatchReach", row->reach);
    gSavedSettings.setF32("CineOutlineOpacity", row->opacity);
    gSavedSettings.setS32("CineOutlineBlendMode", row->blend);

    // Sketch / boil.
    gSavedSettings.setBOOL("CineOutlineSketchEnabled", row->skOn);
    gSavedSettings.setF32("CineOutlineSketchAmount", row->skAmt);
    gSavedSettings.setF32("CineOutlineSketchDetail", row->skDet);
    gSavedSettings.setF32("CineOutlineSketchFPS", row->skFps);
    gSavedSettings.setS32("CineOutlineSketchStrokes", row->skStrokes);
    gSavedSettings.setF32("CineOutlineSketchRoughness", row->skRough);
    gSavedSettings.setF32("CineOutlineSketchSeed", row->skSeed);

    // Motion.
    gSavedSettings.setS32("CineOutlineMotionStyle", row->moStyle);
    gSavedSettings.setF32("CineOutlineMotionSpeed", row->moSpeed);
    gSavedSettings.setF32("CineOutlineMotionAmount", row->moAmt);
    gSavedSettings.setF32("CineOutlineMotionScale", row->moScale);
    gSavedSettings.setF32("CineOutlineMotionAngle", row->moAngle);

    // CineOutlineEnabled and every CineOutlineSubject* setting are
    // deliberately left untouched -- a preset is a look, not a shot.
}

void ALRotoInk::resetToDefaults()
{
    for (const char* name : kAllSettings)
    {
        reset_setting(name);
    }
}

const std::vector<std::string>& ALRotoInk::settings()
{
    static const std::vector<std::string> s_settings(
        std::begin(kAllSettings), std::end(kAllSettings));
    return s_settings;
}
