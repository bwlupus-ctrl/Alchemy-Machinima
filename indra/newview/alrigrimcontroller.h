#ifndef AL_RIG_RIM_CONTROLLER_H
#define AL_RIG_RIM_CONTROLLER_H

#include "aluirefresh.h"

#include <string>
#include <vector>

class LLButton;
class LLComboBox;
class LLTextBox;
class LLView;

class ALRigRimController
{
public:
    void attach(LLView* root);
    void onReveal();
    void tickFrame();
    void tickRefresh();
    static bool applyPresetByName(const std::string& name);
    static const std::vector<std::string>& presetNames();

private:
    LLComboBox* mPreset = nullptr;
    LLTextBox* mQualityStatus = nullptr;
    LLButton* mEnableTag = nullptr;
    LLView* mTronTintWidgets[7] = {};
    ALRevisionGate mStatusGate;
    ALLiveReadoutClock mStatusClock{2.f};
};

#endif
