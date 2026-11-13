/** @file alpanelprojectorshafts.h @brief Shared panel_projector_shafts.xml controls. */
#ifndef AL_ALPANELPROJECTORSHAFTS_H
#define AL_ALPANELPROJECTORSHAFTS_H
#include "llpanel.h"
#include "aluirefresh.h"
#include "alshaftstatusconsumer.h"
#include <boost/signals2/connection.hpp>
std::string alSelectedProjectorShaftStatus();
class LLTextBox;
class LLComboBox;
class ALPanelProjectorShafts final : public LLPanel
{
public:
    ALPanelProjectorShafts();
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
private:
    void refreshPresets();
    LLComboBox* mPreset = nullptr;
    boost::signals2::scoped_connection mEngineConnection;
    boost::signals2::scoped_connection mPresetConnection;
    LLTextBox* mShaftCost = nullptr;
    LLView* mFlowGrid = nullptr;
    ALLiveReadoutClock mCostClock{0.5f}; // same launcher-clock pattern, every 2 s
    LLTextBox* mShaftStatus = nullptr;      // optional "shaft_status_text" row; may be absent
    ALLiveReadoutClock mStatusClock{1.f};   // selected-projector status line, every 1 s
};
#endif
