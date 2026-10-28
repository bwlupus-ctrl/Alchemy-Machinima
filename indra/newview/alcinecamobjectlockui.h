/** Shared bindings for legacy CineCam and Director's main-camera mount rows. */
#ifndef AL_CINECAM_OBJECT_LOCK_UI_H
#define AL_CINECAM_OBJECT_LOCK_UI_H

#include "aluirefresh.h"

class LLTextBox;
class LLPanel;

class ALCineCamObjectLockUI
{
public:
    void bind(LLView* root);
    void refresh();

private:
    void updateMode();
    LLPanel* mMotion = nullptr;
    boost::signals2::scoped_connection mModeConnection;
    LLTextBox* mStatus = nullptr;
    LLButton* mRelease = nullptr;
    ALLiveReadoutClock mClock{2.f};
};

#endif
