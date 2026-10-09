/**
 * @file alfloateravatarlightresponse.h
 * @brief Thin floater host for panel_avatar_light_response.xml.
 */
#ifndef AL_ALFLOATERAVATARLIGHTRESPONSE_H
#define AL_ALFLOATERAVATARLIGHTRESPONSE_H
#include "llfloater.h"
class ALPanelAvatarLightResponse;
class ALFloaterAvatarLightResponse final : public LLFloater
{
public:
    explicit ALFloaterAvatarLightResponse(const LLSD& key);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
private:
    ALPanelAvatarLightResponse* mPanel = nullptr;
};
#endif
