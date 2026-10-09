/**
 * @file alfloateravatarlightresponse.cpp
 * @brief Thin floater host for panel_avatar_light_response.xml.
 */
#include "llviewerprecompiledheaders.h"
#include "alfloateravatarlightresponse.h"
#include "alavatarlightresponse.h"
#include "alpanelavatarlightresponse.h"

ALFloaterAvatarLightResponse::ALFloaterAvatarLightResponse(const LLSD& key)
:   LLFloater(key)
{
}

bool ALFloaterAvatarLightResponse::postBuild()
{
    mPanel = findChild<ALPanelAvatarLightResponse>("light_response_panel");
    return LLFloater::postBuild();
}

void ALFloaterAvatarLightResponse::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    if (mPanel)
    {
        mPanel->focusTarget(key.isUUID() ? key.asUUID() : LLUUID::null);
    }
}

void ALFloaterAvatarLightResponse::onClose(bool app_quitting)
{
    ALAvatarLightResponse::instance().saveIfPersist();
    LLFloater::onClose(app_quitting);
}
