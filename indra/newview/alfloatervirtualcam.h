/**
 * @file alfloatervirtualcam.h
 * @brief Standalone host for the Virtual Cam controls.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFLOATERVIRTUALCAM_H
#define AL_ALFLOATERVIRTUALCAM_H

#include "llfloater.h"

class LLButton;
class LLTextBox;

class ALFloaterVirtualCam final : public LLFloater
{
public:
    ALFloaterVirtualCam(const LLSD& key);
    ~ALFloaterVirtualCam() override = default;

    bool postBuild() override;
    void draw() override;

private:
    void onClickManagePrism();
    void onClickBuildOtsPair();
    void onClickZolly();
    void onClickHeroArc();
    void refreshControls();

    LLTextBox* mPrismSummaryText = nullptr;
    LLButton*  mPrismManageBtn = nullptr;
    LLButton*  mOtsPairBtn = nullptr;
    LLButton*  mZollyBtn = nullptr;
    LLButton*  mHeroArcBtn = nullptr;
    U64        mPrismConfigurationRevision = 0;
    U64        mPrismRuntimeRevision = 0;
    bool       mHavePrismSummary = false;
};

#endif // AL_ALFLOATERVIRTUALCAM_H
