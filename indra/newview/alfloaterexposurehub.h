/**
 * @file alfloaterexposurehub.h
 * @brief Thin floater host for panel_exposure_hub.xml.
 */
#ifndef AL_ALFLOATEREXPOSUREHUB_H
#define AL_ALFLOATEREXPOSUREHUB_H
#include "llfloater.h"
class ALFloaterExposureHub final : public LLFloater
{
public:
    explicit ALFloaterExposureHub(const LLSD& key);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
};
#endif
