#ifndef AL_PANEL_RIG_RIM_H
#define AL_PANEL_RIG_RIM_H

#include "alrigrimcontroller.h"
#include "llpanel.h"

class ALKitShell;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLContextMenu;

// Session targeting state lives outside the Rim look preset family.
class ALExtraRimUI
{
public:
    ~ALExtraRimUI();
    void attach(LLPanel* root);
    void refresh();
    void onReveal() { mRevision = 0; }
    static void recordEdit(ALKitShell* shell, const LLSD& before, const std::string& label);
private:
    LLUUID selectedActor() const;
    void syncSelection();
    void removeSelected();
    void showMenu(S32 x, S32 y);
    LLPanel* mRoot = nullptr;
    LLScrollListCtrl* mList = nullptr;
    LLComboBox* mSource = nullptr;
    LLSliderCtrl* mTrim = nullptr;
    LLTextBox* mStatus = nullptr;
    LLButton* mAdd = nullptr;
    LLButton* mRemove = nullptr;
    LLHandle<LLContextMenu> mMenu;
    U64 mRevision = 0;
};

class ALPanelRigRim final : public LLPanel
{
public:
    ALPanelRigRim();
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;

private:
    ALRigRimController mRim;
    ALExtraRimUI mExtras;
    ALLiveReadoutClock mRefreshClock{ 10.f };
};

#endif
