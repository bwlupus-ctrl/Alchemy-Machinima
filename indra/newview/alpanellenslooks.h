/**
 * @file alpanellenslooks.h
 * @brief Shared Lens Looks controls from panel_lens_looks.xml.
 */
#ifndef AL_ALPANELLENSLOOKS_H
#define AL_ALPANELLENSLOOKS_H

#include "llpanel.h"

#include <boost/signals2.hpp>

class LLButton;
class LLTextBox;

class ALPanelLensLooks final : public LLPanel
{
public:
    ALPanelLensLooks();
    ~ALPanelLensLooks() override;

    bool postBuild() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    void updateHints();

    LLTextBox* mInfraredHint = nullptr;
    LLButton* mInfraredEnableTag = nullptr;
    LLTextBox* mSoftEdgeDoFHint = nullptr;
    boost::signals2::scoped_connection mInfraredEnabledConnection;
    boost::signals2::scoped_connection mAvatarTagConnection;
    boost::signals2::scoped_connection mSoftEdgeEnabledConnection;
    boost::signals2::scoped_connection mSoftEdgeBlurTypeConnection;
    boost::signals2::scoped_connection mDoFConnection;
};

#endif
