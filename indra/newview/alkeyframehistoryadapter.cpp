#include "alkeyframehistoryadapter.h"

ALKeyframeHistoryAdapter::ALKeyframeHistoryAdapter(ALKf::History& h,ALKf::Sequence& s,
    std::function<bool()> owns,std::function<void()> edited)
    : mHistory(h),mLive(s),mOwnsCamera(std::move(owns)),mEdited(std::move(edited)) {}
void ALKeyframeHistoryAdapter::commit(const ALKf::Sequence& before,const std::string& label)
{
    mHistory.commit(before,mLive,label,alNextUndoSequence());
    if(mEdited) mEdited(); // WP5 binds historyEdited(true)
}
U64 ALKeyframeHistoryAdapter::sequence(bool redo) const {return mHistory.sequence(redo);}
void ALKeyframeHistoryAdapter::replay(bool redo)
{
    // Replay receives a fresh shared-clock stamp, just like kit/settings histories.
    // historyEdited(true) is commit-only: it clears redo, including this owner's redo.
    if(ready(redo,sequence(redo))) mHistory.replay(redo,mLive,alNextUndoSequence());
}
void ALKeyframeHistoryAdapter::discard_redo() {mHistory.discardRedo();}
bool ALKeyframeHistoryAdapter::ready(bool redo,U64 seq) const
{
    return mHistory.ready(redo,seq,mLive,mOwnsCamera && mOwnsCamera());
}
void ALKeyframeHistoryAdapter::discard_step(bool redo,U64 seq) {mHistory.discardStep(redo,seq);}
U64 ALKeyframeHistoryAdapter::group_sequence(bool redo) const {return sequence(redo);}
