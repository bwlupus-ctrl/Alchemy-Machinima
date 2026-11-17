#ifndef AL_KEYFRAME_HISTORY_ADAPTER_H
#define AL_KEYFRAME_HISTORY_ADAPTER_H
#include "alkeyframecore.h"
#include "alundosequence.h"
class ALKeyframeHistoryAdapter
{
public:
    ALKeyframeHistoryAdapter(ALKf::History& history, ALKf::Sequence& live,
                            std::function<bool()> ownsCamera, std::function<void()> edited);
    void commit(const ALKf::Sequence& before, const std::string& label);
    U64 sequence(bool redo) const;
    void replay(bool redo);
    void discard_redo();
    bool ready(bool redo, U64 seq) const;
    void discard_step(bool redo, U64 seq);
    U64 group_sequence(bool redo) const;
private:
    ALKf::History& mHistory;
    ALKf::Sequence& mLive;
    std::function<bool()> mOwnsCamera;
    std::function<void()> mEdited;
};
#endif
