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
    // [RotoInk Anim] layer-1 shape/tempo/step/seed.
    F32  moShape;
    F32  moTempo, moPhase;
    S32  moStep;
    F32  moSeed;
    // [RotoInk Anim] layer 2.
    S32  m2Style;
    F32  m2Speed, m2Amt, m2Scale, m2Shape, m2Angle;
    // [RotoInk Anim] line pattern.
    S32  ptType;
    F32  ptSize, ptRatio, ptDrift, hAngle, hReach;
    bool hCross;
    F32  ptSeed;
    // [RotoInk Anim] secondary colour.
    F32  c2r, c2g, c2b;
    S32  c2Mode;
    F32  c2Speed, c2Len;
};

// [TronT1] Builder for a Tron look's linked ink preset (T1 contract section
// 9's common column: silOn true/silThr 0.02/silW 1.0, crOn true/crAngle 30/
// crW 0.7, depthMode 0/metricThr 0.1, inkMode 0/reach 6/opacity 1/blend 2,
// sketch off, layer-2 off, no pattern unless overridden, glow 1). Every
// field is still written (Preset's own header comment rule), just from a
// named/positional argument list instead of a 59-field literal row.
Preset make_tron_ink_preset(const char* key, const char* label,
                             F32 r, F32 g, F32 b, F32 intensity, F32 width, F32 soft,
                             S32 moStyle, F32 moSpeed, F32 moAmt, F32 moScale, F32 moShape,
                             S32 ptType, F32 ptSize, F32 ptRatio, F32 ptDrift,
                             F32 c2r, F32 c2g, F32 c2b, S32 c2Mode, F32 c2Speed, F32 c2Len)
{
    Preset p{};
    p.mKey = key;
    p.mLabel = label;
    p.r = r; p.g = g; p.b = b;
    p.intensity = intensity;
    p.glow = 1.0f;
    p.silOn = true; p.silThr = 0.02f; p.silW = 1.0f;
    p.crOn = true; p.crAngle = 30.0f; p.crW = 0.7f;
    p.depthMode = 0; p.metricThr = 0.1f; p.width = width; p.soft = soft; p.farCutoff = 0.0f;
    p.inkMode = 0; p.reach = 6.0f; p.opacity = 1.0f; p.blend = 2;
    p.skOn = false; p.skAmt = 2.0f; p.skDet = 0.45f; p.skFps = 12.0f; p.skStrokes = 2; p.skRough = 0.3f; p.skSeed = 0.0f;
    p.moStyle = moStyle; p.moSpeed = moSpeed; p.moAmt = moAmt; p.moScale = moScale; p.moAngle = 90.0f;
    p.moShape = moShape; p.moTempo = 0.0f; p.moPhase = 0.0f; p.moStep = 0; p.moSeed = 0.0f;
    p.m2Style = 0; p.m2Speed = 0.5f; p.m2Amt = 0.5f; p.m2Scale = 0.3f; p.m2Shape = 0.5f; p.m2Angle = 90.0f;
    p.ptType = ptType; p.ptSize = ptSize; p.ptRatio = ptRatio; p.ptDrift = ptDrift;
    p.hAngle = 45.0f; p.hReach = 0.0f; p.hCross = false; p.ptSeed = 0.0f;
    p.c2r = c2r; p.c2g = c2g; p.c2b = c2b; p.c2Mode = c2Mode; p.c2Speed = c2Speed; p.c2Len = c2Len;
    return p;
}

// Common column (section 5): DM 0 / MT 0.10 / F 0 / IM 0 / R 6 / sketch off
// (2.0/0.45/12/2/0.30/0) / motion none (0.5/0.5/0.3/90). Rows override only
// what differs from the common column.
const Preset kPresets[] = {
    // -- existing looks (round 1-3); anim fields hold the neutral/off values --
    { "comic", "Comic Ink", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "bold", "Bold Comic", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.025f,1.0f,true,35.0f,0.8f,0,0.1f,4.0f,0.3f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "manga", "Manga Fine Line", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.015f,1.0f,true,20.0f,0.9f,0,0.1f,1.0f,0.6f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "anime", "Anime Cel Line", 0.06f,0.03f,0.02f,1.0f,0.0f,true,0.02f,1.0f,true,45.0f,0.5f,0,0.1f,1.5f,0.5f,0.0f,0,6.0f,0.95f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "pencil", "Pencil Sketch", 0.12f,0.12f,0.13f,1.0f,0.0f,true,0.018f,1.0f,true,25.0f,0.6f,0,0.1f,1.5f,0.7f,0.0f,0,6.0f,0.85f,0,true,1.5f,0.55f,12.0f,3,0.45f,7.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "charcoal", "Charcoal", 0.03f,0.03f,0.03f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.5f,0,0.1f,4.0f,0.9f,0.0f,0,6.0f,0.9f,0,true,3.0f,0.35f,8.0f,2,0.6f,3.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "boil", "Rotoscope Boil", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,28.0f,0.65f,0,0.1f,2.5f,0.35f,0.0f,0,6.0f,1.0f,0,true,2.0f,0.45f,12.0f,1,0.0f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "storyboard", "Storyboard", 0.08f,0.09f,0.12f,1.0f,0.0f,true,0.03f,1.0f,true,40.0f,0.35f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,0.8f,0,true,1.0f,0.6f,6.0f,2,0.25f,11.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "chalk", "Chalk on Dark", 1.0f,1.0f,1.0f,1.0f,0.15f,true,0.02f,1.0f,true,25.0f,0.5f,0,0.1f,2.0f,0.8f,0.0f,0,6.0f,0.9f,0,true,1.2f,0.7f,10.0f,2,0.55f,5.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "neon", "Neon Line", 1.0f,0.1f,0.6f,6.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "tron", "Tron Glow", 0.15f,0.9f,1.0f,8.0f,1.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,1.5f,0.4f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,1,0.5f,0.25f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "inkwash", "Ink Wash", 0.05f,0.06f,0.1f,1.0f,0.0f,true,0.025f,1.0f,true,30.0f,0.3f,0,0.1f,5.0f,1.0f,0.0f,0,6.0f,0.7f,1,true,1.5f,0.3f,8.0f,2,0.2f,2.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "blueprint", "Blueprint", 0.85f,0.92f,1.0f,1.2f,0.0f,true,0.015f,1.0f,true,15.0f,1.0f,0,0.1f,1.0f,0.3f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "stainedglass", "Stained Glass", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,12.0f,1.0f,0,0.1f,3.5f,0.2f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "holoscan", "Hologram Scan", 0.3f,1.0f,0.8f,5.0f,0.8f,true,0.02f,1.0f,true,25.0f,0.6f,0,0.1f,1.5f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,3,0.35f,0.85f,0.12f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "chase", "Chase Light", 1.0f,0.6f,0.15f,8.0f,1.0f,true,0.02f,1.0f,false,30.0f,0.0f,0,0.1f,2.5f,0.4f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,2,0.6f,1.0f,0.35f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "matchglow", "Light-Matched Glow", 1.0f,1.0f,1.0f,3.0f,1.0f,true,0.02f,1.0f,true,40.0f,0.3f,0,0.1f,2.0f,0.5f,0.0f,1,8.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "wireshimmer", "Wire Shimmer", 0.75f,0.85f,1.0f,4.0f,0.6f,true,0.02f,1.0f,true,20.0f,0.9f,0,0.1f,1.0f,0.4f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,5,1.2f,0.7f,0.5f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "woodcut", "Woodcut", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.03f,1.0f,true,20.0f,0.9f,0,0.1f,3.0f,0.15f,0.0f,0,6.0f,1.0f,0,true,0.6f,0.85f,24.0f,1,0.35f,13.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "ballpoint", "Ballpoint Doodle", 0.05f,0.08f,0.35f,1.0f,0.0f,true,0.015f,1.0f,true,25.0f,0.7f,0,0.1f,1.0f,0.5f,0.0f,0,6.0f,1.0f,0,true,2.5f,0.5f,12.0f,4,0.15f,21.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    // -- [RotoInk Anim] round-A animated presets --
    { "ants", "Marching Ants", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,6,1.0f,1.0f,0.25f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "heartbeat", "Neon Heartbeat", 1,0.1f,0.3f,6.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,7,1.2f,0.9f,0.5f,90.0f,0.35f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,0.8f,0.6f,3,0.25f,0.0f },
    { "electric", "Electric Arc", 0.6f,0.85f,1,8.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,12,1.0f,0.9f,0.2f,90.0f,0.35f,0.0f,0.0f,0,0.0f,30,0.5f,0.8f,0.4f,0.4f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,1,1,3,0.25f,0.0f },
    { "rainbow", "Rainbow Cycle", 1.0f,0.1f,0.6f,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,16,0.5f,0.5f,0.4f,90.0f,0.3f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,1,1,2,0.3f,600 },
    { "glitch", "Glitch Line", 0.2f,1,0.9f,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,13,1.0f,0.8f,0.35f,90.0f,0.5f,0.0f,0.0f,1,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,0.2f,0.4f,4,0.25f,0.0f },
    { "holoflicker", "Hologram Flicker", 0.3f,1,0.8f,5.0f,0.8f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,31,0.35f,0.85f,0.12f,90,0.6f,0.0f,0.0f,0,0.0f,18,1.0f,0.6f,0.3f,0.3f,90.0f,1,6,0.6f,40,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "writeon", "Ink Write-On", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,10,0.25f,1.0f,0.15f,90,0.8f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "ripple", "Ripple Pulse", 0.5f,0.7f,1,4.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,9,0.5f,0.9f,0.12f,90.0f,0.6f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "dissolve", "Dissolve Sketch", 0.12f,0.12f,0.13f,1.0f,0.0f,true,0.018f,1.0f,true,25.0f,0.6f,0,0.1f,1.5f,0.7f,0.0f,0,6.0f,0.85f,0,true,1.5f,0.55f,12.0f,3,0.45f,7.0f,11,0.2f,0.7f,0.5f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "speedlines", "Comic Speed Lines", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.5f,0.4f,0.0f,0,6.0f,1.0f,0,true,2.0f,0.45f,12.0f,2,0.0f,0.0f,2,0.8f,0.7f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,4,6,0.35f,0,0,24,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "strobe", "Strobe Poster", 0.1f,0,0.15f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.4f,0.0f,0,6.0f,1.0f,1,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,8,1.0f,1.0f,0.4f,90.0f,0.5f,0.0f,0.0f,2,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "sparkle", "Sparkle Trail", 1,0.85f,0.5f,4.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,15,0.6f,1.0f,0.35f,90.0f,0.3f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,1,1,3,0.25f,0.0f },
    { "brush", "Breathing Brush", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.6f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,14,0.3f,0.8f,0.4f,90.0f,0.2f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,5,40,0.35f,0,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "dashblue", "Dashed Blueprint", 0.85f,0.92f,1.0f,1.2f,0.0f,true,0.015f,1.0f,true,15.0f,1.0f,0,0.1f,1.0f,0.3f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,1,10,0.55f,20,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "pointillist", "Dotted Pointillist", 0.05f,0.05f,0.08f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,5,0.8f,0.4f,0.5f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,2,7,0.7f,0,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "doubleline", "Double-Line Cartoon", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,5.0f,0.3f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,3,12,0.3f,0,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "engraving", "Cross-hatch Engraving", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,20.0f,0.9f,0,0.1f,1.5f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,4,5,0.3f,0,45,30,true,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "heatshimmer", "Heat Shimmer", 0.1f,0.05f,0.02f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,32,0.4f,0.7f,0.4f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "morse", "Morse Code", 1,0.9f,0.5f,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,19,1.0f,1.0f,0.3f,90.0f,0.3f,0.0f,0.0f,1,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "villain", "Villain Aura", 0.6f,0.05f,0.9f,4.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,22,0.5f,0.8f,0.45f,90.0f,0.8f,0.0f,0.0f,0,0.0f,1,0.5f,0.3f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,0.1f,0.3f,1,0.2f,400 },
    { "barber", "Barber Pole", 1,0.2f,0.2f,4.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,3.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,17,0.6f,1.0f,0.35f,45,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "neonbuzz", "Neon Tube Buzz", 1,0.3f,0.1f,6.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.5f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,18,1.0f,0.8f,0.4f,90.0f,0.35f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "oldfilm", "Old Film Jitter", 0.1f,0.08f,0.06f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.6f,0.0f,0,6.0f,1.0f,0,true,0.8f,0.6f,18.0f,1,0.35f,9.0f,20,1.0f,0.7f,0.5f,90.0f,0.4f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "pixel8", "8-bit Pixel", 0.1f,0.1f,0.2f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.05f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,21,1.0f,1.0f,0.3f,90.0f,0.5f,0.0f,0.0f,2,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "shockwave", "Shockwave", 0.7f,0.9f,1,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,23,0.5f,1.0f,0.6f,90.0f,0.4f,0.0f,0.0f,0,0.0f,22,0.5f,0.5f,0.3f,0.9f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "fireflies", "Orbit Fireflies", 0.9f,1,0.5f,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,24,0.3f,1.0f,0.45f,90.0f,0.15f,0.0f,0.0f,0,0.0f,5,1.0f,0.3f,0.5f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "constellation", "Constellation", 0.8f,0.9f,1,4.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,25,0.5f,0.85f,0.4f,90.0f,0.3f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "fire", "Fire Lick", 1,0.55f,0.1f,6.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,3.0f,0.7f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,26,0.6f,0.9f,0.4f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,0.1f,0,4,0.25f,0.0f },
    { "smoke", "Smoke Wisps", 0.25f,0.25f,0.28f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.8f,0.0f,0,6.0f,0.8f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,27,0.25f,0.7f,0.6f,90.0f,0.8f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "drip", "Ink Drip", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.5f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,28,0.15f,1.0f,0.3f,90.0f,0.35f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "splatter", "Splatter Pop", 0.1f,0,0.05f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,29,0.5f,1.0f,0.4f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "lightning", "Lightning Storm", 0.8f,0.85f,1,8.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,30,0.6f,1.0f,0.6f,90.0f,0.5f,0.0f,0.0f,0,0.0f,8,1.0f,0.3f,0.6f,0.3f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,1,1,4,0.25f,0.0f },
    { "jelly", "Jelly Wobble", 0.9f,0.3f,0.6f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,33,0.5f,0.8f,0.5f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "painton", "Paint-On Bottom-Up", 0.0f,0.0f,0.0f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,2.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,34,0.2f,1.0f,0.2f,90,0.8f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "radial", "Radial Reveal", 1,1,1,3.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,35,0.25f,1.0f,0.25f,90.0f,0.4f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    { "tempo120", "Tempo Pulse 120", 1,0.2f,0.6f,6.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,7,1.0f,0.9f,0.6f,90.0f,0.3f,120.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1,1,1,3,0.25f,0.0f },
    { "candy", "Two-Tone Candy", 1,0.2f,0.4f,1.0f,0.0f,true,0.02f,1.0f,true,30.0f,0.7f,0,0.1f,3.0f,0.4f,0.0f,0,6.0f,1.0f,0,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,0,0.5f,0.5f,0.3f,90.0f,0.5f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,1,14,0.5f,30,45.0f,0.0f,false,0.0f,0.2f,0.6f,1,1,0.3f,60 },
    { "waverunner", "Wave Runner", 0.2f,0.9f,1,5.0f,1.0f,true,0.02f,1.0f,true,35.0f,0.4f,0,0.1f,2.0f,0.5f,0.0f,0,6.0f,1.0f,2,false,2.0f,0.45f,12.0f,2,0.3f,0.0f,16,1.0f,1.0f,0.3f,90.0f,0.7f,0.0f,0.0f,0,0.0f,0,0.5f,0.5f,0.3f,0.5f,90.0f,0,12.0f,0.5f,0.0f,45.0f,0.0f,false,0.0f,1.0f,1.0f,1.0f,0,0.25f,0.0f },
    // -- [TronT1] Tron look presets (ALTron::applyPreset) -- built via
    // make_tron_ink_preset() below instead of 59-field positional rows.
    make_tron_ink_preset("tron_legacy",     "Tron Legacy Ink",  0.15f,0.90f,1.00f, 8.0f, 1.5f, 0.4f,
                         1, 0.5f, 0.25f, 0.3f, 0.5f,
                         0, 12.0f, 0.5f, 0.0f,
                         1.0f,1.0f,1.0f, 3, 0.25f, 0.0f),
    make_tron_ink_preset("tron_ares",       "Tron Ares Ink",    1.00f,0.08f,0.06f, 9.0f, 2.0f, 0.4f,
                         7, 1.2f, 0.9f, 0.5f, 0.35f,
                         0, 12.0f, 0.5f, 0.0f,
                         1.0f,0.6f,0.5f, 4, 0.25f, 0.0f),
    make_tron_ink_preset("tron_1982",       "Classic 1982 Ink", 0.55f,0.85f,1.00f, 5.0f, 1.0f, 0.4f,
                         0, 0.5f, 0.5f,  0.3f, 0.5f,
                         1, 10.0f, 0.6f, 20.0f,
                         1.0f,0.9f,0.6f, 1, 0.25f, 300.0f),
    make_tron_ink_preset("tron_recognizer", "Recognizer Ink",   1.00f,0.45f,0.05f, 8.0f, 2.5f, 0.4f,
                         2, 0.6f, 1.0f,  0.35f, 0.5f,
                         0, 12.0f, 0.5f, 0.0f,
                         1.0f,1.0f,1.0f, 0, 0.25f, 0.0f),
    make_tron_ink_preset("tron_uprising",   "Uprising White Ink", 1.0f,1.0f,1.0f, 4.0f, 1.2f, 0.4f,
                         3, 0.35f, 0.85f, 0.12f, 0.5f,
                         0, 12.0f, 0.5f, 0.0f,
                         0.2f,0.9f,1.0f, 3, 0.25f, 0.0f),
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
    // Subject*, this is which-pass-timing, not part of a look).
    "CineOutlineLayer",
    // [RotoInk Speed] Master animation speed/pause/Director-follow (never
    // written by a preset -- rate control, not part of a look).
    "CineOutlineAnimSpeed",
    "CineOutlineAnimPause",
    "CineOutlineAnimFollowDirector",
    // [TronT1 P2-4 fix] Was missing from this list entirely, so neither
    // ALRotoInk::resetToDefaults() nor the Director/Lightbox key list
    // (settings(), below) ever touched it. Rate control, not part of a
    // look -- ALRotoInk::applyPreset() never writes it, same as the three
    // Anim keys above, so adding it here does not change applyPreset()'s
    // write count.
    "CineOutlineAnimUseTronClock",
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
    // [RotoInk Anim] Subject isolation extras (never written by a preset).
    "CineOutlineSubjectTargetSet",
    "CineOutlineSubjectMaxTargets",
    "CineOutlineSubjectShape",
    "CineOutlineSubjectInvert",
    "CineOutlineSubjectTargetColor",
    "CineOutlineSubjectDepthNear",
    "CineOutlineSubjectDepthFar",
    "CineOutlineSubjectScreenCX",
    "CineOutlineSubjectScreenCY",
    "CineOutlineSubjectScreenRX",
    "CineOutlineSubjectScreenRY",
    // [TronA0] Subject mask source (never written by a preset).
    "CineOutlineSubjectSource",
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
    // [RotoInk Anim] Motion layer 1 extras.
    "CineOutlineMotionShape",
    "CineOutlineMotionTempo",
    "CineOutlineMotionPhase",
    "CineOutlineMotionStep",
    "CineOutlineMotionSeed",
    // [RotoInk Anim] Motion layer 2.
    "CineOutlineMotion2Style",
    "CineOutlineMotion2Speed",
    "CineOutlineMotion2Amount",
    "CineOutlineMotion2Scale",
    "CineOutlineMotion2Shape",
    "CineOutlineMotion2Angle",
    // [RotoInk Anim] Line pattern.
    "CineOutlinePatternType",
    "CineOutlinePatternSize",
    "CineOutlinePatternRatio",
    "CineOutlinePatternDrift",
    "CineOutlineHatchAngle",
    "CineOutlineHatchReach",
    "CineOutlineHatchCross",
    "CineOutlinePatternSeed",
    // [RotoInk Anim] Secondary colour.
    "CineOutlineColor2",
    "CineOutlineColor2Mode",
    "CineOutlineColor2Speed",
    "CineOutlineColor2Length",
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

    // [RotoInk Anim] Motion layer 1 extras.
    gSavedSettings.setF32("CineOutlineMotionShape", row->moShape);
    gSavedSettings.setF32("CineOutlineMotionTempo", row->moTempo);
    gSavedSettings.setF32("CineOutlineMotionPhase", row->moPhase);
    gSavedSettings.setS32("CineOutlineMotionStep", row->moStep);
    gSavedSettings.setF32("CineOutlineMotionSeed", row->moSeed);

    // [RotoInk Anim] Motion layer 2.
    gSavedSettings.setS32("CineOutlineMotion2Style", row->m2Style);
    gSavedSettings.setF32("CineOutlineMotion2Speed", row->m2Speed);
    gSavedSettings.setF32("CineOutlineMotion2Amount", row->m2Amt);
    gSavedSettings.setF32("CineOutlineMotion2Scale", row->m2Scale);
    gSavedSettings.setF32("CineOutlineMotion2Shape", row->m2Shape);
    gSavedSettings.setF32("CineOutlineMotion2Angle", row->m2Angle);

    // [RotoInk Anim] Line pattern.
    gSavedSettings.setS32("CineOutlinePatternType", row->ptType);
    gSavedSettings.setF32("CineOutlinePatternSize", row->ptSize);
    gSavedSettings.setF32("CineOutlinePatternRatio", row->ptRatio);
    gSavedSettings.setF32("CineOutlinePatternDrift", row->ptDrift);
    gSavedSettings.setF32("CineOutlineHatchAngle", row->hAngle);
    gSavedSettings.setF32("CineOutlineHatchReach", row->hReach);
    gSavedSettings.setBOOL("CineOutlineHatchCross", row->hCross);
    gSavedSettings.setF32("CineOutlinePatternSeed", row->ptSeed);

    // [RotoInk Anim] Secondary colour.
    gSavedSettings.setUntypedValue("CineOutlineColor2",
        LLColor3(row->c2r, row->c2g, row->c2b).getValue());
    gSavedSettings.setS32("CineOutlineColor2Mode", row->c2Mode);
    gSavedSettings.setF32("CineOutlineColor2Speed", row->c2Speed);
    gSavedSettings.setF32("CineOutlineColor2Length", row->c2Len);

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
