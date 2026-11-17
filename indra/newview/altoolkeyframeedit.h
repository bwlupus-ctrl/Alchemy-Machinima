#ifndef AL_TOOL_KEYFRAME_EDIT_H
#define AL_TOOL_KEYFRAME_EDIT_H
#include "lltool.h"
#include "alkeyframecore.h"
class ALKeyframeHistoryAdapter;
class ALToolKeyframeEdit : public LLTool
{
public:
    ALToolKeyframeEdit();
    ~ALToolKeyframeEdit() override;
    void cancelGesture();
    void bind(ALKeyframeHistoryAdapter*, std::function<void(U32)> select, std::function<void(bool)> undo);
    bool handleMouseDown(S32,S32,MASK) override;
    bool handleMouseUp(S32,S32,MASK) override;
    bool handleHover(S32,S32,MASK) override;
    bool handleKey(KEY,MASK) override;
    void handleDeselect() override { cancelGesture(); }
    void onMouseCaptureLost() override { cancelGesture(); }
private:
    ALKeyframeHistoryAdapter* mHistory=nullptr;
    std::function<void(U32)> mSelect;
    std::function<void(bool)> mUndo;
    ALKf::Sequence mBefore;
    ALKf::Vec3 mStart, mPlaneHit, mIn, mOut;
    U32 mKey=0;
    int mHandle=0;
    bool mDragging=false;
};
#endif
