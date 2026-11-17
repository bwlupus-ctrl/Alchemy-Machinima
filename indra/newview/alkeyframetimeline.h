#ifndef AL_KEYFRAME_TIMELINE_H
#define AL_KEYFRAME_TIMELINE_H
#include "lluictrl.h"
#include "alkeyframecore.h"
class ALKeyframeHistoryAdapter;
class ALKeyframeTimeline : public LLUICtrl
{
public:
    struct Params : LLInitParam::Block<Params, LLUICtrl::Params> {};
    explicit ALKeyframeTimeline(const Params&);
    ~ALKeyframeTimeline() override;
    void draw() override;
    void cancelGesture();
    void bind(ALKeyframeHistoryAdapter*, std::function<void(bool)> undo);
    void refresh(); // model/cache work is performed by the owner's idle refresher
    U32 selected() const { return mSelected; }
    void select(U32 id) { mSelected = id; }
    bool retime(double seconds);
    void eraseSelected();
    bool handleMouseDown(S32, S32, MASK) override;
    bool handleMouseUp(S32, S32, MASK) override;
    bool handleHover(S32, S32, MASK) override;
    bool handleDoubleClick(S32, S32, MASK) override;
    bool handleToolTip(S32 x,S32 y,MASK) override {return inside(x,y);}
    bool handleScrollHWheel(S32 x,S32 y,LLScrollDelta d) override {return handleScrollWheel(x,y,d); }
    bool handleRightMouseDown(S32 x, S32 y, MASK) override { return inside(x,y); }
    bool handleRightMouseUp(S32 x, S32 y, MASK) override { return inside(x,y); }
    bool handleMiddleMouseDown(S32 x, S32 y, MASK) override { return inside(x,y); }
    bool handleMiddleMouseUp(S32 x, S32 y, MASK) override { return inside(x,y); }
    bool handleScrollWheel(S32, S32, LLScrollDelta) override;
    bool handleKeyHere(KEY, MASK) override;
    bool handleUnicodeCharHere(llwchar) override {return true;}
    void onMouseCaptureLost() override;
private:
    bool inside(S32 x, S32 y) const { return getLocalRect().pointInRect(x,y); }
    double seconds(S32 x) const;
    S32 pixel(double seconds) const;
    U32 hit(S32 x) const;
    ALKeyframeHistoryAdapter* mHistory = nullptr;
    std::function<void(bool)> mUndo;
    ALKf::Sequence mBefore;
    std::vector<std::pair<U32,double>> mKeys;
    U64 mRevision = ~U64(0);
    U32 mSelected = 0;
    double mEnd = 3, mOrigin = 0, mZoom = 1, mPlayhead = 0;
    bool mDragging = false, mScrubbing = false;
};
#endif
