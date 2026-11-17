#include "llviewerprecompiledheaders.h"
#include "alkeyframetimeline.h"
#include "alkeyframeplayer.h"
#include "alkeyframehistoryadapter.h"
#include "alcleanframe.h"
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llsliderwheel.h"
#include "lluictrlfactory.h"
#include "llrender.h"
#include "llui.h"
#include <algorithm>
#include <cmath>

static LLDefaultChildRegistry::Register<ALKeyframeTimeline> r_keyframe_timeline("keyframe_timeline");
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

ALKf::Track* camera(ALKf::Sequence& s)
{
    for (auto& t : s.tracks) if (t.kind == "camera") return &t;
    return nullptr;
}
bool editable()
{
    auto& p = ALKeyframePlayer::instance();
    return !p.ownsCamera() && !p.rlvHandbackActive();
}
}
ALKeyframeTimeline::ALKeyframeTimeline(const Params& p) : LLUICtrl(p) {}
ALKeyframeTimeline::~ALKeyframeTimeline() { cancelGesture(); }
void ALKeyframeTimeline::bind(ALKeyframeHistoryAdapter* h, std::function<void(bool)> u)
{ mHistory = h; mUndo = std::move(u); }
void ALKeyframeTimeline::refresh()
{
    auto& p = ALKeyframePlayer::instance();
    mPlayhead = p.time();
    if (mRevision == p.sequence().revision) return;
    mRevision = p.sequence().revision;
    mKeys.clear();
    const auto* t = camera(p.sequence());
    const auto* c = p.cameraCache();
    if (t && c)
    {
        for (size_t i=0; i<t->keys.size(); ++i) mKeys.emplace_back(t->keys[i].id,c->arrival[i]);
        mEnd = std::max(3.,c->end);
    }
    if (std::none_of(mKeys.begin(),mKeys.end(),[this](const auto& k){return k.first==mSelected;})) mSelected=0;
}
double ALKeyframeTimeline::seconds(S32 x) const
{ return std::max(0.,mOrigin+(x-10)*mEnd/(std::max(1,getRect().getWidth()-20)*mZoom)); }
S32 ALKeyframeTimeline::pixel(double t) const
{ return 10+static_cast<S32>((t-mOrigin)*mZoom*std::max(1,getRect().getWidth()-20)/mEnd); }
U32 ALKeyframeTimeline::hit(S32 x) const
{
    U32 id=0; S32 best=10;
    for (const auto& k:mKeys) if (std::abs(pixel(k.second)-x)<best) {best=std::abs(pixel(k.second)-x); id=k.first;}
    return id;
}
bool ALKeyframeTimeline::retime(double t)
{
    if (!editable()) return false;
    auto& s=ALKeyframePlayer::instance().sequence();
    auto* track=camera(s);
    if (!track) return false;
    ALKf::Cache c; std::string why;
    if (!ALKf::buildCache(*track,c,why)) return false;
    for (size_t i=1;i<track->keys.size();++i) if(track->keys[i].id==mSelected)
    {
        const double low=c.departure[i-1]+.05;
        const double high=i+1<track->keys.size()?c.arrival[i+1]-track->keys[i].hold-.05:1e6;
        if (high<low) return false;
        t=std::clamp(t,low,high);
        if(std::abs(t-c.arrival[i])<1e-8) return false;
        auto candidate=*track;
        candidate.settings.evenSpeed=false;
        candidate.keys[i-1].out={ALKf::PaceMode::Duration,t-c.departure[i-1]};
        if(i+1<track->keys.size()) candidate.keys[i].out={ALKf::PaceMode::Duration,c.arrival[i+1]-t-candidate.keys[i].hold};
        ++candidate.revision;
        ALKf::Cache checked;
        if(!ALKf::buildCache(candidate,checked,why)) return false;
        *track=std::move(candidate); ++s.revision;
        refresh(); return true;
    }
    return false;
}
bool ALKeyframeTimeline::handleMouseDown(S32 x,S32 y,MASK)
{
    if(!inside(x,y)) return false;
    setFocus(true); refresh();
    if(ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive()) return true;
    mSelected=hit(x);
    if(mSelected && editable() && mHistory)
    {
        mBefore=ALKeyframePlayer::instance().sequence();
        ALKeyframePlayer::instance().history().beginGesture(mBefore);
        mDragging=true;
    }
    else if(!mSelected) {mScrubbing=true; ALKeyframePlayer::instance().scrub(seconds(x));}
    if(mDragging||mScrubbing) gFocusMgr.setMouseCapture(this);
    return true;
}
bool ALKeyframeTimeline::handleHover(S32 x,S32 y,MASK)
{
    if(mDragging&&mBefore.uid!=ALKeyframePlayer::instance().sequence().uid) cancelGesture();
    if(mDragging) retime(seconds(x));
    if(mScrubbing) ALKeyframePlayer::instance().scrub(seconds(x));
    return mDragging||mScrubbing||inside(x,y);
}
bool ALKeyframeTimeline::handleMouseUp(S32 x,S32 y,MASK)
{
    const bool consumed=mDragging||mScrubbing||inside(x,y);
    if(mDragging && mHistory)
    {
        syncReferences(mBefore,ALKeyframePlayer::instance().sequence());
        ALKeyframePlayer::instance().history().endGesture();
        if(mBefore!=ALKeyframePlayer::instance().sequence()) mHistory->commit(mBefore,"Retime keyframe");
    }
    mDragging=mScrubbing=false;
    if(gFocusMgr.getMouseCapture()==this) gFocusMgr.setMouseCapture(nullptr);
    return consumed;
}
void ALKeyframeTimeline::cancelGesture()
{
    const bool restored=mDragging && ALKeyframePlayer::instance().history().cancelGesture(ALKeyframePlayer::instance().sequence());
    mDragging=mScrubbing=false;
    if(gFocusMgr.getMouseCapture()==this) gFocusMgr.setMouseCapture(nullptr);
    // Core cancellation restores the original revision. Refresh all UI-owned
    // caches now, before a second gesture can reuse an intermediate revision.
    if(restored) {refresh();onCommit();}
}
void ALKeyframeTimeline::onMouseCaptureLost() { cancelGesture(); }
bool ALKeyframeTimeline::handleDoubleClick(S32 x,S32 y,MASK)
{
    if(!inside(x,y)) return false;
    refresh(); mSelected=hit(x);
    if(mSelected) ALKeyframePlayer::instance().visit(mSelected);
    return true;
}
void ALKeyframeTimeline::eraseSelected()
{
    if(!editable()||!mHistory) return;
    auto& s=ALKeyframePlayer::instance().sequence(); const auto before=s;
    if(auto* t=camera(s)) if(ALKf::eraseKey(s,t->id,mSelected)) mHistory->commit(before,"Delete keyframe");
    refresh();
}
bool ALKeyframeTimeline::handleKeyHere(KEY k,MASK mask)
{
    if(k==KEY_ESCAPE) cancelGesture();
    else if(k==KEY_DELETE||k==KEY_BACKSPACE) {cancelGesture();eraseSelected();}
    else if((mask&MASK_CONTROL) && (k=='Z'||k=='Y') && mUndo) {cancelGesture();mUndo(k=='Y');}
    return true; // focused keys never become world input
}
bool ALKeyframeTimeline::handleScrollWheel(S32 x,S32 y,LLScrollDelta delta)
{
    const MASK mask=gKeyboard->currentMask(true);
    if(!(mask&(MASK_CONTROL|MASK_SHIFT))) return false;
    if(!inside(x,y)) return false;
    const double steps=delta.mClicks;
    if(mask&MASK_CONTROL) mZoom=std::clamp(mZoom*std::pow(1.15,-steps),.25,100.);
    else mOrigin=std::max(0.,mOrigin+steps*mEnd/(10*mZoom));
    LLSliderWheel::noteScroll(this);
    return true;
}
void ALKeyframeTimeline::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    gl_rect_2d(getLocalRect(),LLColor4(.08f,.10f,.12f,1.f));
    for(const auto& k:mKeys)
    {
        const S32 x=pixel(k.second), y=getRect().getHeight()/2;
        if(x<0||x>=getRect().getWidth()) continue;
        gGL.color4fv((k.first==mSelected?LLColor4::yellow:LLColor4::white).mV);
        gGL.begin(LLRender::LINE_LOOP);
        gGL.vertex2i(x,y+6); gGL.vertex2i(x+6,y); gGL.vertex2i(x,y-6); gGL.vertex2i(x-6,y); gGL.end();
    }
    gl_line_2d(pixel(mPlayhead),0,pixel(mPlayhead),getRect().getHeight(),LLColor4::cyan);
    LLUICtrl::draw();
}
