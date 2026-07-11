/**
 * @file alfloaterphototools.h
 * @brief Phototools: EEP environment quick-switcher, DoF focus point
 * lock/follow/crosshair, and rule-of-thirds/golden-ratio composition guide
 * overlay.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
 * Depth-of-field WYSIWYG fix, focus point lock/crosshair, and composition
 * guide overlay by William Weaver ("paperwork").
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 *
 * Ported from Firestorm's QuickPrefs/Phototools floater (donor: I:\enve
 * indra/newview/quickprefs.cpp+.h, floater_phototools.xml) to Alchemy for
 * the BD/Alchemy merge campaign, item F4 (+F8 composition/framing guides).
 * This is a thinned, from-scratch reimplementation, not a line-for-line
 * port: the donor's dynamic quick-access-bar control editor (addControl/
 * removeControl/edit mode - the separate "quickprefs" bottom-tray floater),
 * its per-axis Vignette/shadow/SSAO sliders, and its avatar Z-offset/max
 * complexity sliders are all pruned because Alchemy already exposes the
 * underlying debug settings elsewhere (floater_lightbox_settings.xml
 * Rendering tab; panel_quick_settings.xml; floater_preferences_graphics_advanced.xml)
 * - see the F4/F8 campaign report for the full pruned-control list and the
 * settings each control binds to.
 */

#ifndef AL_FLOATERPHOTOTOOLS_H
#define AL_FLOATERPHOTOTOOLS_H

#include "llfloater.h"

class LLComboBox;

// campaign F4/F8: registered as "phototools" (see llviewerfloaterreg.cpp
// insertion block in the campaign report - that file is shared/off-limits
// for this item, coordinator applies it)
class ALFloaterPhototools final : public LLFloater
{
public:
    ALFloaterPhototools(const LLSD& key);
    ~ALFloaterPhototools() override = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    // -- EEP environment quick-switcher (donor: quickprefs.cpp loadPresets/
    // loadSkyPresets/loadWaterPresets/loadDayCyclePresets/setSelectedEnvironment,
    // ported near-verbatim - this logic has no Alchemy equivalent). OpenSim
    // legacy-windlight branches (#ifdef OPENSIM in the donor) are dropped;
    // this fork does not build with OPENSIM support.
    void loadPresets();
    void loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map);
    void loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map);
    void loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map);
    void setSelectedEnvironment();

    static bool isValidPreset(const LLSD& preset);
    static void stepComboBox(LLComboBox* ctrl, bool forward);
    static void selectSkyPreset(const LLSD& preset);
    static void selectWaterPreset(const LLSD& preset);
    static void selectDayCyclePreset(const LLSD& preset);

    void onChangeWaterPreset();
    void onChangeSkyPreset();
    void onChangeDayCyclePreset();
    void onClickSkyPrev();
    void onClickSkyNext();
    void onClickWaterPrev();
    void onClickWaterNext();
    void onClickDayCyclePrev();
    void onClickDayCycleNext();
    void onClickResetToRegionDefault();

    LLComboBox* mWLPresetsCombo = nullptr;
    LLComboBox* mWaterPresetsCombo = nullptr;
    LLComboBox* mDayCyclePresetsCombo = nullptr;

    // scoped_connection (not connection): auto-disconnects on floater
    // destruction, matching alfloaterlightbox.h's mTonemapConnection/mCASConnection.
    boost::signals2::scoped_connection mEnvChangedConnection;

    // -- DoF focus point + composition guides (campaign F4/F8; genuinely new
    // controls with no Alchemy equivalent - see RenderFocusPointCrosshair /
    // RenderCompositionGuide* in the campaign report's settings.xml blocks).
    // The checkboxes/combo/swatch/spinners for these bind directly to debug
    // settings via control_name in floater_phototools.xml and need no
    // dedicated commit handlers here; RenderDepthOfField, RenderFocusPointLocked
    // and RenderFocusPointFollowsPointer bind to Alchemy's existing keys
    // (already used by floater_lightbox_settings.xml) rather than a parallel
    // FS-named key.
};

#endif // AL_FLOATERPHOTOTOOLS_H
