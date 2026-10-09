/**
 * @file alfloaterexposurehub.cpp
 * @brief Thin floater host for panel_exposure_hub.xml.
 */
#include "llviewerprecompiledheaders.h"
#include "alfloaterexposurehub.h"

ALFloaterExposureHub::ALFloaterExposureHub(const LLSD& key)
:   LLFloater(key)
{
}

bool ALFloaterExposureHub::postBuild()
{
    return LLFloater::postBuild();
}

void ALFloaterExposureHub::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
}
