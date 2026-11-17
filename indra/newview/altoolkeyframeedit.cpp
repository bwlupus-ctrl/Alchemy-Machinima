#include "llviewerprecompiledheaders.h"
#include "altoolkeyframeedit.h"
#include "alkeyframeoverlay.h"
#include "alkeyframeplayer.h"
#include "alkeyframehistoryadapter.h"
#include "alcleanframe.h"
#include "llagent.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include <limits>

namespace
{
// Actor rekey notifications also rewrite History's drag snapshot. Keep the UI's
// commit snapshot in step, without replacing its drag-start geometry or timing.
void syncReferences(ALKf::Sequence& before,const ALKf::Sequence& live)
{
    for(auto& t:before.tracks)for(const auto& current:live.tracks)if(t.id==current.id)
    {
        t.lock=current.lock;
        for(size_t i=0;i<t.keys.size()&&i<current.keys.size();++i)
            if(t.keys[i].id==current.keys[i].id)t.keys[i].aim=current.keys[i].aim;
    }
}

bool allowed()
{
    auto& p=ALKeyframePlayer::instance();
    std::string why;
    return !ALCleanFrame::active() && !p.ownsCamera() && !p.rlvHandbackActive() && !p.restricted(why);
}
LLVector3d global(ALKf::Vec3 p) { return LLVector3d(p.x,p.y,p.z); }
}
ALToolKeyframeEdit::ALToolKeyframeEdit() : LLTool("KeyframeEdit") {}
ALToolKeyframeEdit::~ALToolKeyframeEdit() { cancelGesture(); }
void ALToolKeyframeEdit::bind(ALKeyframeHistoryAdapter* h,std::function<void(U32)> select,std::function<void(bool)> undo)
{ mHistory=h; mSelect=std::move(select); mUndo=std::move(undo); }
bool ALToolKeyframeEdit::handleMouseDown(S32 x,S32 y,MASK)
{
    if(!allowed()||!mHistory) return true;
    double best=16*16;
    const ALKeyframeOverlay::Pick* nearest=nullptr;
    for(const auto& pick:ALKeyframeOverlay::picks())
    {
        LLCoordGL screen;
        if(!LLViewerCamera::getInstance()->projectPosAgentToScreen(gAgent.getPosAgentFromGlobal(global(pick.position)),screen,false)) continue;
        const double d=double(screen.mX-x)*(screen.mX-x)+double(screen.mY-y)*(screen.mY-y);
        if(d<best) {best=d; nearest=&pick;}
    }
    if(!nearest) return true;
    mKey=nearest->key; mHandle=nearest->handle; mStart=nearest->position;
    mIn=mOut={};
    ALKf::Vec3 node;
    for(const auto& pick:ALKeyframeOverlay::picks()) if(pick.key==mKey&&pick.handle==0)node=pick.position;
    for(const auto& pick:ALKeyframeOverlay::picks()) if(pick.key==mKey)
    {if(pick.handle<0)mIn=pick.position-node;if(pick.handle>0)mOut=pick.position-node;}
    if(mSelect) mSelect(mKey);
    LLVector3d hit;
    if(!gViewerWindow->mousePointOnPlaneGlobal(hit,x,y,global(mStart),LLViewerCamera::getInstance()->getAtAxis())) return true;
    mPlaneHit={hit.mdV[0],hit.mdV[1],hit.mdV[2]};
    mBefore=ALKeyframePlayer::instance().sequence();
    ALKeyframePlayer::instance().history().beginGesture(mBefore);
    mDragging=true; setMouseCapture(true); return true;
}
bool ALToolKeyframeEdit::handleHover(S32 x,S32 y,MASK)
{
    if(mDragging && (mBefore.uid!=ALKeyframePlayer::instance().sequence().uid || !allowed())) {cancelGesture(); return true;}
    if(mDragging && hasMouseCapture())
    {
        LLVector3d hit;
        if(!gViewerWindow->mousePointOnPlaneGlobal(hit,x,y,global(mStart),LLViewerCamera::getInstance()->getAtAxis())) return true;
        const ALKf::Vec3 delta=ALKf::Vec3{hit.mdV[0],hit.mdV[1],hit.mdV[2]}-mPlaneHit;
        auto& live=ALKeyframePlayer::instance().sequence();
        if(ALKf::length(delta)<1e-8 && live==mBefore)return true;
        syncReferences(mBefore,live);
        auto candidate=mBefore;
        for(auto& t:candidate.tracks) if(t.kind=="camera") for(auto& k:t.keys) if(k.id==mKey)
        {
            size_t position=0;
            for(size_t i=0;i<t.schema.size();++i)if(t.schema[i].name=="position")position=i;
            auto& pos=std::get<ALKf::Vec3>(k.values[position]);
            if(mHandle==0) pos=pos+delta;
            else
            {
                const auto offset=mStart+delta-pos;
                if(k.handles.mode==ALKf::HandleMode::Auto) {k.handles.in=mIn;k.handles.out=mOut;k.handles.mode=ALKf::HandleMode::Aligned;}
                auto& moved=mHandle<0?k.handles.in:k.handles.out;
                auto& opposite=mHandle<0?k.handles.out:k.handles.in;
                const double otherLength=ALKf::length(opposite);
                moved=offset;
                if(k.handles.mode==ALKf::HandleMode::Aligned) opposite=ALKf::unit(-offset)*otherLength;
            }
            for(const auto& old:live.tracks) if(old.id==t.id) t.revision=old.revision+1;
        }
        candidate.revision=live.revision+1;
        std::string why;
        if(ALKf::validate(candidate,why)) live=std::move(candidate);
    }
    gViewerWindow->setCursor(UI_CURSOR_TOOLGRAB); return true;
}
bool ALToolKeyframeEdit::handleMouseUp(S32,S32,MASK)
{
    if(mDragging)
    {
        syncReferences(mBefore,ALKeyframePlayer::instance().sequence());
        ALKeyframePlayer::instance().history().endGesture();
        if(mBefore!=ALKeyframePlayer::instance().sequence()) mHistory->commit(mBefore,"Shape camera path");
    }
    mDragging=false; setMouseCapture(false); return true;
}
void ALToolKeyframeEdit::cancelGesture()
{
    const bool restored=mDragging && ALKeyframePlayer::instance().history().cancelGesture(ALKeyframePlayer::instance().sequence());
    mDragging=false;
    if(hasMouseCapture()) setMouseCapture(false);
    if(restored && mSelect)mSelect(mKey); // synchronize restored revisions before the next drag
}
bool ALToolKeyframeEdit::handleKey(KEY key,MASK mask)
{
    if(key==KEY_ESCAPE) {cancelGesture(); return true;}
    if((mask&MASK_CONTROL)&&(key=='Z'||key=='Y')&&mUndo)
    {cancelGesture();mUndo(key=='Y');return true;}
    if((key==KEY_DELETE||key==KEY_BACKSPACE)&&allowed()&&mHistory)
    {
        cancelGesture();
        auto& seq=ALKeyframePlayer::instance().sequence();const auto before=seq;
        for(auto& t:seq.tracks)if(t.kind=="camera")
            if(ALKf::eraseKey(seq,t.id,mKey)) {mHistory->commit(before,"Delete camera keyframe");break;}
        return true;
    }
    return false;
}
