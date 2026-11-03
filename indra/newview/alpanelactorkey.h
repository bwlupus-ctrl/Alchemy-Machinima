/** Director Capture > Key presentation; no renderer or guard policy lives here. */
#pragma once
#include "llpanel.h"
#include "aluirefresh.h"
#include <boost/signals2/connection.hpp>
#include <array>
#include <vector>

class ALKitPage;
class ALKitRow;
class LLTextBox;

class ALPanelActorKey final : public LLPanel
{
public:
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;
private:
    void updateConditions();
    void refreshReadouts();
    LLTextBox* mShadowCodes = nullptr;
    ALKitRow* mRetry = nullptr;
    std::array<bool, 9> mConditionVisible{};
    ALKitPage* mPage = nullptr;
    LLUICtrl* mColour = nullptr;
    LLUICtrl* mShadow = nullptr;
    std::array<ALKitRow*, 9> mConditional{};
    std::array<LLTextBox*, 9> mReadouts{};
    std::vector<boost::signals2::scoped_connection> mConnections;
    ALLiveReadoutClock mClock{2.f};
    ALRevisionGate mRevision;
};
