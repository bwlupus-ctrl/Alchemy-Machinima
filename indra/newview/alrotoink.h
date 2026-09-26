/**
 * @file alrotoink.h
 * @brief Rotoscope Ink: settings -> Lightbox UI / Director scene list glue.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [RotoInk] Native deferred ink-line post pass (successor of the Cine
 * Outline Phase-1 Sobel pass). The shader/uniform side lives in
 * app_settings/shaders/class1/deferred/cineOutlineF.glsl and
 * LLPipeline::renderCineOutline (pipeline.cpp); this file only turns
 * CineOutline* settings into full presets, resets, and the key list the
 * Lightbox tab and Director's scene save/load use.
 *
 * Default OFF (CineOutlineEnabled = false). When disabled the pass does not
 * run at all (no scratch copy, no draw) -- see LLPipeline::renderCineOutline.
 */

#pragma once

#ifndef AL_ROTOINK_H
#define AL_ROTOINK_H

#include <string>
#include <vector>

namespace ALRotoInk
{
    // Apply a named look (section 5 of the contract): writes every non-
    // Enabled, non-Subject* CineOutline* setting. Unknown key -> no-op.
    // Never touches CineOutlineEnabled or CineOutlineSubject* (shot-specific,
    // never part of a look).
    void applyPreset(const std::string& key);

    // Resets every CineOutline* setting (including Enabled and Subject*) to
    // its settings.xml default. Used by the Lightbox tab's "Reset all".
    void resetToDefaults();

    // Full list of CineOutline* setting names (Enabled, Color, Intensity,
    // Glow, and every [RotoInk] key from settings.xml). Used by
    // LLFloaterDirector::sceneSettingsList() and the old-scene reset in
    // LLFloaterDirector::loadScene().
    const std::vector<std::string>& settings();
}

#endif // AL_ROTOINK_H
