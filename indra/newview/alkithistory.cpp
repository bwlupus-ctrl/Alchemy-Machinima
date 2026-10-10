#include "llviewerprecompiledheaders.h"
#include "alkithistory.h"
#include "lluictrl.h"
static S32 sKitHistorySuspend=0;

ALKitHistory::ScopedSuspend::ScopedSuspend()
{
    ++sKitHistorySuspend;
}
ALKitHistory::ScopedSuspend::~ScopedSuspend()
{
    --sKitHistorySuspend;
}
bool ALKitHistory::suspended()
{
    return sKitHistorySuspend>0;
}

void ALKitHistory::beginGesture(EOrigin o,const std::string& p,const std::string& label)
{
    mOrigin=o;
    mPage=p;
    mGestureLabel=label;
}
void ALKitHistory::noteKey()
{
}

void ALKitHistory::watchSettings(const std::string&,const std::vector<std::string>&)
{
}

void ALKitHistory::recordWidget(LLUICtrl* c,const LLSD& oldv,const LLSD& newv,const std::string& label)
{
    if(suspended()||!c||oldv==newv)return;
    Step s;
    s.label=mGestureLabel.empty()?label:mGestureLabel;
    s.page=mPage;
    s.scene_generation=mGeneration;
    Entry e;
    e.widget=true;
    e.ctrl=c->getHandle();
    e.oldv=oldv;
    e.newv=newv;
    s.entries.push_back(e);
    mUndo.push_back(std::move(s));
    if(mUndo.size()>MAX_STEPS)mUndo.erase(mUndo.begin());
    mRedo.clear();
    ++mRevision;
}

void ALKitHistory::recordCustom(const std::string& key,const std::string& label,std::function<bool(bool,std::string&)> fn)
{
    if(suspended())return;
    Step s;
    s.label=label;
    s.page=mPage;
    s.scene_generation=mGeneration;
    s.custom.push_back(
    {
        key,std::move(fn)
    }
    );
    mUndo.push_back(std::move(s));
    mRedo.clear();
    ++mRevision;
}

void ALKitHistory::barrier(const std::string&)
{
    mUndo.clear();
    mRedo.clear();
    ++mGeneration;
    ++mRevision;
}
void ALKitHistory::globalBarrier(const std::string&)
{
}

static void replayKitStep(ALKitHistory::Step& s,bool redo)
{
    ALKitHistory::ScopedSuspend guard;
    for(auto& e:s.entries)if(LLUICtrl* c=e.ctrl.get())
    {
        c->setValue(redo?e.newv:e.oldv);
        c->onCommit();
    }
    for(auto& e:s.custom)
    {
        std::string reason;
        e.replay(redo,reason);
    }
}

void ALKitHistory::undo()
{
    if(!canUndo())return;
    Step s=std::move(mUndo.back());
    mUndo.pop_back();
    replayKitStep(s,false);
    mRedo.push_back(std::move(s));
    ++mRevision;
}
void ALKitHistory::redo()
{
    if(!canRedo())return;
    Step s=std::move(mRedo.back());
    mRedo.pop_back();
    replayKitStep(s,true);
    mUndo.push_back(std::move(s));
    ++mRevision;
}

void ALKitHistory::registerContext(const std::string&,std::function<std::string()>)
{
}
