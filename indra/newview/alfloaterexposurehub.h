/**
 * @file alfloaterexposurehub.h
 * @brief [ExposureHub] One floater that edits the EXISTING exposure / brightness settings.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * UI-only hub. It adds no setting, no exposure mode and no behaviour: every control binds to a
 * setting that already exists (control_name in floater_exposure_hub.xml), so it stays in sync with
 * the original controls (Lightbox, Personal Lighting, Preferences, Cine Light Rig, Debug Settings),
 * which remain where they are. Every bound setting is read live by the engine
 * (LLCachedControl / signal), so no commit-side apply path is needed.
 *
 * The only code here is what XML cannot express:
 *  - ALScrollFit wiring for the tabs (CLAUDE.md UI mandate);
 *  - the per-row reset buttons (reset-to-default of the same setting);
 *  - enabling the per-operator exposure rows (AMD LPM / S-Log) only for their tonemapper, driven
 *    by a change signal on AlchemyRenderTonemapType (never from draw());
 *  - the Night Mask Darkness percent slider, which maps percent -> the 0..1 brightness-floor
 *    setting exactly like ALPanelCineLightRig::onNightMaskDarknessCommit.
 */

#ifndef AL_ALFLOATEREXPOSUREHUB_H
#define AL_ALFLOATEREXPOSUREHUB_H

#include "llfloater.h"
#include "alscrollfit.h"

#include <boost/signals2.hpp>

class LLUICtrl;

class ALFloaterExposureHub final : public LLFloater
{
public:
    ALFloaterExposureHub(const LLSD& key);
    ~ALFloaterExposureHub() override = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    // Enables the AMD LPM exposure row for tonemap type 7 and the S-Log EV row for type 8.
    void updateTonemapGating();
    // Night Mask darkness: percent slider <-> CineLightRigNightMaskDarkness (0..1 floor).
    void onNightMaskDarknessCommit(LLUICtrl* source);
    void syncNightMaskDarkness();

    ALScrollFit::Controller mScrollFit;

    boost::signals2::scoped_connection mTonemapConnection;
    boost::signals2::scoped_connection mNightMaskConnection;
};

#endif // AL_ALFLOATEREXPOSUREHUB_H
