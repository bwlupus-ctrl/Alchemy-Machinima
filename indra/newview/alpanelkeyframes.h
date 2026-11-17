#ifndef AL_PANEL_KEYFRAMES_H
#define AL_PANEL_KEYFRAMES_H
#include "llpanel.h"
#include "alkeyframecore.h"
#include <memory>
#include <boost/signals2.hpp>
class ALKeyframeHistoryAdapter;
class ALKeyframeTimeline;
class ALToolKeyframeEdit;
class ALKitPage;
class LLViewerObject;
class ALPanelKeyframes : public LLPanel
{
public:
    ALPanelKeyframes();
    ~ALPanelKeyframes() override;
    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool visible) override;
    static bool canLock();
    static void lockOn(LLViewerObject*);
private:
    void bind(const std::string&, LLUICtrl*);
    void action(const std::string&);
    void refresh(bool force=false);
    void refreshLibrary();
    void refreshActors();
    void cancelEdits();
    void claim();
    void release();
    void switchSequence(ALKf::Sequence);
    void capture(bool insert);
    void target(LLViewerObject*, bool own);
    void edit(const std::string&, const std::function<void(ALKf::Sequence&,ALKf::Track&,ALKf::Key*)>&);
    ALKf::RefSlot reference(LLViewerObject*) const;
    LLUICtrl* control(const std::string&) const;
    static void idle(void*);
    ALKitPage* mPage=nullptr;
    ALKeyframeTimeline* mTimeline=nullptr;
    std::unique_ptr<ALKeyframeHistoryAdapter> mHistory;
    std::unique_ptr<ALToolKeyframeEdit> mTool;
    std::vector<boost::signals2::scoped_connection> mConnections;
    std::map<std::string,LLUICtrl*> mControls;
    std::map<std::string,std::string> mTooltips;
    std::map<std::string,ALKf::Sequence> mSessions;
    U64 mRevision=~U64(0), mCastRevision=~U64(0), mOverlayRevision=~U64(0);
    U32 mSelection=0;
    double mLastRefresh=-1;
    bool mOwner=false, mShow=true, mDots=true, mIcons=true;
    std::string mMessage, mFitResult, mLoadedName;
};
#endif
