#ifndef AL_KEYFRAME_TIMELINE_H
#define AL_KEYFRAME_TIMELINE_H
#include "lluictrl.h"
class ALKeyframeTimeline : public LLUICtrl
{
public:
    struct Params : LLInitParam::Block<Params, LLUICtrl::Params> {};
    explicit ALKeyframeTimeline(const Params&);
    ~ALKeyframeTimeline() override;
    void draw() override;
    void cancelGesture();
};
#endif
