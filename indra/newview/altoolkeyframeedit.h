#ifndef AL_TOOL_KEYFRAME_EDIT_H
#define AL_TOOL_KEYFRAME_EDIT_H
#include "lltool.h"
class ALToolKeyframeEdit : public LLTool
{
public:
    ALToolKeyframeEdit();
    ~ALToolKeyframeEdit() override;
    void cancelGesture();
};
#endif
