#include "llviewerprecompiledheaders.h"
#include "alpanelkeyframes.h"
#include "alkeyframecamera.h"
#include "alkeyframeplayer.h"
#include "alkeyframehistoryadapter.h"
#include "alkeyframetimeline.h"
#include "alkeyframeoverlay.h"
#include "alkeyframefit.h"
#include "alkeyframelibrary.h"
#include "alkeyframeviewer.h"
#include "altoolkeyframeedit.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alcleanframe.h"
#include "alghoststudio.h"
#include "aluirefresh.h"
#include "llagent.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llflycamrecorder.h"
#include "lljoint.h"
#include "lltoolmgr.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "lluictrlfactory.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static LLPanelInjector<ALPanelKeyframes> r_panel_keyframes("panel_keyframes");
namespace
{
LLHandle<ALPanelKeyframes> sOwner;
ALKf::Track* camera(ALKf::Sequence& s)
{for(auto& t:s.tracks) if(t.kind=="camera") return &t; return nullptr;}
size_t channel(const ALKf::Track& t,const char* name)
{for(size_t i=0;i<t.schema.size();++i)if(t.schema[i].name==name)return i;return 0;}
LLUUID uuid(const ALKf::Id& id) {LLUUID u; std::memcpy(u.mData,id.data(),16); return u;}
bool blocked()
{auto& p=ALKeyframePlayer::instance(); return p.ownsCamera()||p.rlvHandbackActive();}
class TargetPick final : public LLTool
{
public:
    TargetPick() : LLTool("Keyframe target") {}
    static TargetPick& instance() {static TargetPick t; return t;}
    std::function<void(LLViewerObject*)> picked;
    bool handleMouseDown(S32 x,S32 y,MASK) override
    {
        if(ALCleanFrame::active()) return true;
        auto pick=gViewerWindow->pickImmediate(x,y,false,true);
        if(!pick.getObject()) return true;
        auto cb=std::move(picked);
        LLToolMgr::getInstance()->clearTransientTool();
        if(cb) cb(pick.getObject());
        return true;
    }
    bool handleHover(S32,S32,MASK) override {gViewerWindow->setCursor(UI_CURSOR_TOOLPICKOBJECT3); return true;}
    bool handleKey(KEY key,MASK) override
    {if(key!=KEY_ESCAPE) return false; LLToolMgr::getInstance()->clearTransientTool(); return true;}
    void handleDeselect() override {picked={};}
};
}
ALPanelKeyframes::ALPanelKeyframes() = default;
ALPanelKeyframes::~ALPanelKeyframes()
{
    cancelEdits();
    release();
    for(auto& connection:mConnections)connection.disconnect();
    if(mTool)mTool->bind(nullptr,{},{});
    gIdleCallbacks.deleteFunction(idle,this);
    if(mPage) mPage->setOwnedHistory({},{},{},{},{},{});
    if(mTimeline) mTimeline->bind(nullptr,{});
}
LLUICtrl* ALPanelKeyframes::control(const std::string& name) const
{auto i=mControls.find(name); return i==mControls.end()?nullptr:i->second;}
void ALPanelKeyframes::bind(const std::string& name,LLUICtrl* c)
{
    if(!c) return;
    mControls[name]=c;
    mTooltips[name]=c->getToolTip();
    mConnections.emplace_back(c->setCommitCallback([this,name](LLUICtrl*,const LLSD&){action(name);}));
}
bool ALPanelKeyframes::postBuild()
{
    mPage=findChild<ALKitPage>("keyframes_kit");
    mTimeline=findChild<ALKeyframeTimeline>("kf_timeline");
    if(!mPage||!mTimeline) return false;
    auto& p=ALKeyframePlayer::instance();
    mHistory=std::make_unique<ALKeyframeHistoryAdapter>(p.history(),p.sequence(),blocked,[this]{mPage->historyEdited(true);});
    mPage->setOwnedHistory([this](bool r){return mHistory->sequence(r);},
        [this](bool r){cancelEdits(); mHistory->replay(r); refresh(true);},
        [this]{mHistory->discard_redo();},[this](bool r,U64 s){return mHistory->ready(r,s);},
        [this](bool r,U64 s){mHistory->discard_step(r,s);},[this](bool r){return mHistory->group_sequence(r);});
    mPage->setOwnedHistoryBlockedMessages("Stop playback to undo keyframe edits","Stop playback to redo keyframe edits");
    mTimeline->bind(mHistory.get(),[this](bool redo){mPage->undoOnPage(redo);});
    mConnections.emplace_back(mTimeline->setCommitCallback([this](LLUICtrl*,const LLSD&){refresh(true);})); // passive cancel-cache notification
    mTool=std::make_unique<ALToolKeyframeEdit>();
    mTool->bind(mHistory.get(),[this](U32 id){mTimeline->select(id); refresh(true);},[this](bool redo){mPage->undoOnPage(redo);});
    // Every generated control is registered here, and refreshed from its authority below.
    bind("kf_track",getChild<LLUICtrl>("kf_track"));
    bind("kf_name",getChild<LLUICtrl>("kf_name"));
    bind("kf_new",getChild<LLUICtrl>("kf_new"));
    bind("kf_save",getChild<LLUICtrl>("kf_save"));
    bind("kf_save_as",getChild<LLUICtrl>("kf_save_as"));
    bind("kf_load",getChild<LLUICtrl>("kf_load"));
    bind("kf_rename",getChild<LLUICtrl>("kf_rename"));
    bind("kf_delete_track",getChild<LLUICtrl>("kf_delete_track"));
    bind("kf_status",getChild<LLUICtrl>("kf_status"));
    bind("kf_add",getChild<LLUICtrl>("kf_add"));
    bind("kf_insert",getChild<LLUICtrl>("kf_insert"));
    bind("kf_delete",getChild<LLUICtrl>("kf_delete"));
    bind("kf_visit",getChild<LLUICtrl>("kf_visit"));
    bind("kf_release",getChild<LLUICtrl>("kf_release"));
    bind("kf_playhead",getChild<LLUICtrl>("kf_playhead"));
    bind("kf_undo",getChild<LLUICtrl>("kf_undo"));
    bind("kf_redo",getChild<LLUICtrl>("kf_redo"));
    bind("kf_playing",getChild<LLUICtrl>("kf_playing"));
    bind("kf_loop",getChild<LLUICtrl>("kf_loop"));
    bind("kf_delay",getChild<LLUICtrl>("kf_delay"));
    bind("kf_arm",getChild<LLUICtrl>("kf_arm"));
    bind("kf_path",getChild<LLUICtrl>("kf_path"));
    bind("kf_even",getChild<LLUICtrl>("kf_even"));
    bind("kf_total",getChild<LLUICtrl>("kf_total"));
    bind("kf_ease_ends",getChild<LLUICtrl>("kf_ease_ends"));
    bind("kf_ease_length",getChild<LLUICtrl>("kf_ease_length"));
    bind("kf_time",getChild<LLUICtrl>("kf_time"));
    bind("kf_pace_mode",getChild<LLUICtrl>("kf_pace_mode"));
    bind("kf_pace",getChild<LLUICtrl>("kf_pace"));
    bind("kf_segment",getChild<LLUICtrl>("kf_segment"));
    bind("kf_hold",getChild<LLUICtrl>("kf_hold"));
    bind("kf_ease",getChild<LLUICtrl>("kf_ease"));
    bind("kf_zoom",getChild<LLUICtrl>("kf_zoom"));
    bind("kf_roll",getChild<LLUICtrl>("kf_roll"));
    bind("kf_handles",getChild<LLUICtrl>("kf_handles"));
    bind("kf_update",getChild<LLUICtrl>("kf_update"));
    bind("kf_look",getChild<LLUICtrl>("kf_look"));
    bind("kf_own",getChild<LLUICtrl>("kf_own"));
    bind("kf_own_pick",getChild<LLUICtrl>("kf_own_pick"));
    bind("kf_own_joint",getChild<LLUICtrl>("kf_own_joint"));
    bind("kf_lock",getChild<LLUICtrl>("kf_lock"));
    bind("kf_lock_pick",getChild<LLUICtrl>("kf_lock_pick"));
    bind("kf_joint",getChild<LLUICtrl>("kf_joint"));
    bind("kf_smoothing",getChild<LLUICtrl>("kf_smoothing"));
    bind("kf_lock_status",getChild<LLUICtrl>("kf_lock_status"));
    bind("kf_show",getChild<LLUICtrl>("kf_show"));
    bind("kf_dots",getChild<LLUICtrl>("kf_dots"));
    bind("kf_icons",getChild<LLUICtrl>("kf_icons"));
    bind("kf_edit",getChild<LLUICtrl>("kf_edit"));
    bind("kf_source",getChild<LLUICtrl>("kf_source"));
    bind("kf_tolerance",getChild<LLUICtrl>("kf_tolerance"));
    bind("kf_fit_cm",getChild<LLUICtrl>("kf_fit_cm"));
    bind("kf_fit_rotation",getChild<LLUICtrl>("kf_fit_rotation"));
    bind("kf_fit_fov",getChild<LLUICtrl>("kf_fit_fov"));
    bind("kf_make",getChild<LLUICtrl>("kf_make"));
    bind("kf_result",getChild<LLUICtrl>("kf_result"));
    control("kf_tolerance")->setValue(1);
    control("kf_source")->setValue(0);
    control("kf_fit_cm")->setValue(2.);
    control("kf_fit_rotation")->setValue(.5);
    control("kf_fit_fov")->setValue(.2);
    refreshLibrary(); refreshActors(); refresh(true);
    gIdleCallbacks.addFunction(idle,this);
    return LLPanel::postBuild();
}
void ALPanelKeyframes::idle(void* data)
{
    auto* self=static_cast<ALPanelKeyframes*>(data);
    if(!self->isInVisibleChain()) {if(self->mOwner) self->release(); return;}
    const double now=LLFrameTimer::getTotalSeconds();
    if(now-self->mLastRefresh<.1) return;
    self->mLastRefresh=now;
    if(!self->mOwner) self->claim();
    if(ALCleanFrame::active()) return;
    self->refresh();
}
void ALPanelKeyframes::draw() {LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; LLPanel::draw();}
void ALPanelKeyframes::onVisibilityChange(bool visible)
{LLPanel::onVisibilityChange(visible); if(visible) {claim(); refresh(true);} else release();}
void ALPanelKeyframes::claim()
{
    if(!mTimeline||mOwner) return;
    if(auto* old=sOwner.get()) old->release();
    sOwner=getDerivedHandle<ALPanelKeyframes>(); mOwner=true;
    ALKeyframePlayer::instance().setCancelEdit([this]{cancelEdits();});
    mOverlayRevision=~U64(0);
}
void ALPanelKeyframes::cancelEdits()
{if(mTimeline) mTimeline->cancelGesture(); if(mTool) mTool->cancelGesture();}
void ALPanelKeyframes::release()
{
    if(!mOwner) return;
    cancelEdits();
    auto* current=LLToolMgr::getInstance()->getCurrentTool();
    if(current==mTool.get()||current==&TargetPick::instance()) LLToolMgr::getInstance()->clearTransientTool();
    ALKeyframePlayer::instance().setCancelEdit({});
    ALKeyframePlayer::instance().setGuideDraw({}); ALKeyframeOverlay::clear();
    mOwner=false; sOwner.markDead();
}
void ALPanelKeyframes::edit(const std::string& label,const std::function<void(ALKf::Sequence&,ALKf::Track&,ALKf::Key*)>& apply)
{
    if(blocked()) {mMessage="Release camera before editing"; return;}
    auto& s=ALKeyframePlayer::instance().sequence(); auto before=s;
    auto* t=camera(s); if(!t) {ALKf::appendTrack(s,ALKf::cameraTrack()); t=camera(s);}
    ALKf::Key* key=nullptr;
    for(auto& k:t->keys) if(k.id==mTimeline->selected()) key=&k;
    mMessage.clear();
    apply(s,*t,key);
    if(!mMessage.empty()) {s=before;return;}
    ++t->revision; ++s.revision;
    ALKf::Cache checked; std::string why;
    if(!ALKf::validate(s,why)||!ALKf::buildCache(*t,checked,why)) {s=before; mMessage=why; return;}
    mHistory->commit(before,label); mMessage.clear();
}
void ALPanelKeyframes::capture(bool insert)
{
    U32 id=0;
    edit("Capture keyframe",[&](auto& s,auto& t,auto*)
    {
        auto k=ALKeyframeCamera().capture();
        const auto captured=k.values;
        if(!t.keys.empty())k.values=t.keys.back().values;
        else k.values.resize(t.schema.size());
        const char* channels[]={"position","rotation","fov","roll"};
        for(size_t i=0;i<4;++i)k.values[channel(t,channels[i])]=captured[i];
        k.aim.mode=t.lock.kind==ALKf::RefKind::None?ALKf::AimMode::Laid:ALKf::AimMode::FollowLock;
        auto* cache=ALKeyframePlayer::instance().cameraCache();
        size_t at=t.keys.size();
        const double time=ALKeyframePlayer::instance().time();
        if(insert&&cache) at=static_cast<size_t>(std::upper_bound(cache->arrival.begin(),cache->arrival.end(),time)-cache->arrival.begin());
        if(!t.keys.empty()&&at==t.keys.size())
        {
            const auto a=std::get<ALKf::Vec3>(t.keys.back().values[channel(t,"position")]), b=std::get<ALKf::Vec3>(k.values[channel(t,"position")]);
            const double distance=ALKf::length(b-a);
            auto& previous=t.keys.back();
            previous.out={ALKf::PaceMode::Duration,distance<1e-5?2.:3.};
            if(distance>=1e-5&&cache&&!cache->timing.empty()&&cache->timing.back().cruise>1e-6)
                previous.out={ALKf::PaceMode::Speed,cache->timing.back().cruise};
        }
        id=ALKf::appendKey(s,t.id,std::move(k));
        if(at+1<t.keys.size())
        {
            auto added=std::move(t.keys.back());t.keys.pop_back();t.keys.insert(t.keys.begin()+at,std::move(added));
            // Split the authored move around the playhead, preserving its end time.
            if(cache&&at>0)
            {
                const double oldArrival=cache->arrival[at], oldDeparture=cache->departure[at-1];
                const double split=std::clamp(time,oldDeparture+.05, std::max(oldDeparture+.05,oldArrival-.05));
                t.keys[at-1].out={ALKf::PaceMode::Duration,std::max(.05,split-oldDeparture)};
                t.keys[at].out={ALKf::PaceMode::Duration,std::max(.05,oldArrival-split)};
                t.settings.evenSpeed=false;
            }
        }
    });
    if(id) mTimeline->select(id);
}
ALKf::RefSlot ALPanelKeyframes::reference(LLViewerObject* object) const
{
    ALKf::RefSlot ref;
    if(object&&object->isAttachment()) object=object->getAvatarAncestor();
    if(!object||object->isDead()) return ref;
    ref.kind=object->isAvatar()?ALKf::RefKind::Avatar:ALKf::RefKind::Object;
    ref.id=ALKeyframeViewer::id(object->getID());
    const auto pos=object->getPositionGlobal(); ref.fallback={pos.mdV[0],pos.mdV[1],pos.mdV[2]};
    if(const auto* clone=ALGhostStudio::instance().findInstanceByRuntime(object->getID()))
    {ref.kind=ALKf::RefKind::Clone; ref.stableId=ALKeyframeViewer::id(clone->mId);}
    ref.label=object->isAvatar()?"Avatar / clone":"Object";
    return ref;
}
void ALPanelKeyframes::target(LLViewerObject* object,bool own)
{
    if(!canLock()) return;
    const auto ref=reference(object);
    edit(own?"Keyframe target":"Camera Lock",[&](auto&,auto& t,auto* k)
    {if(own) {if(k) {k->aim.target=ref;k->aim.mode=ALKf::AimMode::OwnTarget;}} else t.lock=ref;});
    refreshActors(); refresh(true);
}
bool ALPanelKeyframes::canLock()
{std::string why;return !blocked()&&!ALKeyframePlayer::instance().restricted(why);}
void ALPanelKeyframes::lockOn(LLViewerObject* object)
{
    if(!canLock()) return;
    if(auto* owner=sOwner.get()) {owner->target(object,false);return;}
    // Menus work with the Director closed; the same session history retains this edit.
    ALPanelKeyframes helper;
    auto& p=ALKeyframePlayer::instance();auto before=p.sequence();
    auto* t=camera(p.sequence());if(!t) {ALKf::appendTrack(p.sequence(),ALKf::cameraTrack());t=camera(p.sequence());}
    t->lock=helper.reference(object);++t->revision;++p.sequence().revision;
    ALKeyframeHistoryAdapter history(p.history(),p.sequence(),blocked,{});history.commit(before,"Camera Lock");
}
void ALPanelKeyframes::refreshLibrary()
{
    auto* combo=dynamic_cast<LLComboBox*>(control("kf_track"));if(!combo)return;
    const auto value=combo->getValue();combo->removeall();
    combo->add("Current session",LLSD(""));
    for(const auto& [id,s]:mSessions) combo->add(s.name.empty()?"Untitled session":s.name,LLSD("session:"+id));
    for(const auto& name:ALKeyframeLibrary::names()) combo->add(name,LLSD("file:"+name));
    combo->setValue(value);
}
void ALPanelKeyframes::switchSequence(ALKf::Sequence next)
{
    if(blocked()) {mMessage="Release camera before switching tracks";return;}
    cancelEdits();
    auto& p=ALKeyframePlayer::instance(); auto previous=p.sequence();
    if(previous.uid.empty()) previous.uid=LLUUID::generateNewID().asString();
    if(next.uid.empty()) next.uid=LLUUID::generateNewID().asString();
    std::string why;
    if(p.replaceSequence(std::move(next),why))
    {mSessions[previous.uid]=std::move(previous);mTimeline->select(0);mMessage.clear();refreshLibrary();}
    else mMessage=why;
}
// ui-draw-lint: allow-function(raw-write) actor lists rebuild only on selection or actor-choice revision, from idle not draw
void ALPanelKeyframes::refreshActors()
{
    auto* t=camera(ALKeyframePlayer::instance().sequence());
    for(const char* name:{"kf_lock","kf_own"})
    {
        auto* combo=dynamic_cast<LLComboBox*>(control(name));if(!combo)continue;
        combo->removeall();combo->add("None",LLSD(""));
        for(const auto& choice:LLDirectorCast::instance().getActorChoices()) combo->add(choice.mLabel,choice.mId.asString());
        if(t)
        {
            const auto& ref=std::string(name)=="kf_lock"?t->lock:[&]() -> const ALKf::RefSlot&
            {for(const auto& k:t->keys)if(k.id==mTimeline->selected())return k.aim.target;return t->lock;}();
            if(ref.kind!=ALKf::RefKind::None) {const auto id=uuid(ref.id).asString();if(!combo->setSelectedByValue(id,true))combo->add(ref.label.empty()?"Saved target":ref.label,id);combo->setValue(id);}
        }
    }
    mCastRevision=LLDirectorCast::instance().getActorChoiceRevision();
}

void ALPanelKeyframes::action(const std::string& name)
{
    if(!control(name)) return;
    claim();
    ALScopedUndoSequence group;
    const LLSD value=control(name)->getValue();
    auto& p=ALKeyframePlayer::instance();
    if(name=="kf_playing") {if(value.asBoolean()) p.play();else p.stop(ALKfTransport::Exit::CUT);}
    else if(name=="kf_release") p.stop(ALKfTransport::Exit::USER_INPUT);
    else if(name=="kf_visit") p.visit(mTimeline->selected());
    else if(name=="kf_playhead") p.scrub(value.asReal());
    else if(name=="kf_undo") {cancelEdits();mPage->undoOnPage(false);}
    else if(name=="kf_redo") {cancelEdits();mPage->undoOnPage(true);}
    else if(name=="kf_delete") mTimeline->eraseSelected();
    else if(name=="kf_add"||name=="kf_insert") capture(name=="kf_insert");
    else if(name=="kf_time")
    {
        const auto before=p.sequence();
        if(mTimeline->retime(value.asReal())) mHistory->commit(before,"Retime keyframe");
    }
    else if(name=="kf_show"||name=="kf_dots"||name=="kf_icons")
    {
        if(name=="kf_show") mShow=value.asBoolean();
        if(name=="kf_dots") mDots=value.asBoolean();
        if(name=="kf_icons") mIcons=value.asBoolean();
        mOverlayRevision=~U64(0);
    }
    else if(name=="kf_edit")
    {
        if(value.asBoolean()&&canLock()) LLToolMgr::getInstance()->setTransientTool(mTool.get());
        else if(LLToolMgr::getInstance()->getCurrentTool()==mTool.get()) LLToolMgr::getInstance()->clearTransientTool();
    }
    else if(name=="kf_lock_pick"||name=="kf_own_pick")
    {
        if(canLock())
        {
            cancelEdits();
            auto handle=getDerivedHandle<ALPanelKeyframes>();
            TargetPick::instance().picked=[handle,own=name=="kf_own_pick"](LLViewerObject* object)
            {if(auto* page=handle.get()) if(page->mOwner) page->target(object,own);};
            LLToolMgr::getInstance()->setTransientTool(&TargetPick::instance());
        }
    }
    else if(name=="kf_lock"||name=="kf_own")
    {
        if(value.asString().empty()) target(nullptr,name=="kf_own");
        else if(auto* object=gObjectList.findObject(value.asUUID())) target(object,name=="kf_own");
        else mMessage="Target is away; choose a present actor or pick in world";
    }
    else if(name=="kf_new")
    {ALKf::Sequence next;next.name="Untitled";switchSequence(std::move(next));mLoadedName.clear();}
    else if(name=="kf_track"||name=="kf_load")
    {
        const auto selected=control("kf_track")->getValue().asString();
        if(selected.starts_with("session:"))
        {auto it=mSessions.find(selected.substr(8));if(it!=mSessions.end()) switchSequence(it->second);mLoadedName.clear();}
        else if(selected.starts_with("file:"))
        {
            ALKf::Sequence next;std::string why;const auto file=selected.substr(5);
            if(ALKeyframeLibrary::load(ALKeyframeLibrary::path(file),next,why)) {switchSequence(std::move(next));if(!blocked())mLoadedName=file;}
            else mMessage=why;
        }
    }
    else if(name=="kf_save"||name=="kf_save_as"||name=="kf_rename")
    {
        if(!blocked())
        {
            const auto wanted=control("kf_name")->getValue().asString();
            const auto file=ALKeyframeLibrary::path(wanted);std::string why;
            auto next=p.sequence();next.name=wanted;
            const bool exists=LLFile::isfile(file);
            if(file.empty()) mMessage="Enter a valid track name";
            else if(name!="kf_save"&&exists&&wanted!=mLoadedName) mMessage="That name already exists; choose another name";
            else if(ALKeyframeLibrary::saveVerified(file,next,why))
            {
                if(name=="kf_rename"&&!mLoadedName.empty()&&mLoadedName!=wanted) ALKeyframeLibrary::remove(mLoadedName);
                edit("Name camera track",[&](auto& seq,auto&,auto*){seq.name=wanted;});
                mLoadedName=wanted;mMessage="Saved and verified "+wanted;refreshLibrary();
            }
            else mMessage=why;
        }
    }
    else if(name=="kf_delete_track")
    {
        const auto selected=control("kf_track")->getValue().asString();
        if(selected.starts_with("file:")&&!blocked())
        {mMessage=ALKeyframeLibrary::remove(selected.substr(5))?"Deleted library file; session retained":"Delete failed";refreshLibrary();}
    }
    else if(name=="kf_make"&&!blocked())
    {
        if(control("kf_source")->getValue().asInteger()!=0) {mFitResult="REFUSED: select the current recorded take";return;}
        ALKf::FitOptions options;
        const int tolerance=control("kf_tolerance")->getValue().asInteger();
        if(tolerance==0) options.tolerance={.005,.15*ALKf::degree,.05*ALKf::degree};
        else if(tolerance==2) options.tolerance={.08,2*ALKf::degree,.8*ALKf::degree};
        else if(tolerance==3) options.tolerance={control("kf_fit_cm")->getValue().asReal()/100,
            control("kf_fit_rotation")->getValue().asReal()*ALKf::degree,control("kf_fit_fov")->getValue().asReal()*ALKf::degree};
        const auto result=ALKf::fitTake(LLFlycamRecorder::instance(),options);
        const char* verdict=result.verdict==ALKf::FitVerdict::Pass?"PASS":result.verdict==ALKf::FitVerdict::Approximate?"APPROXIMATE":"REFUSED";
        mFitResult=llformat("%s: %.3f cm / %.4f deg rotation / %.4f deg FOV. ",verdict,result.worst.position*100,result.worst.rotation/ALKf::degree,result.worst.fov/ALKf::degree)+result.reason;
        if(result.verdict!=ALKf::FitVerdict::Refused)
        {
            ALKf::Sequence next;next.name="Fitted take";U32 id=0;
            if(ALKf::appendFit(next,result,id)) {switchSequence(std::move(next));mLoadedName.clear();}
        }
    }
    else if(name=="kf_arm") {} // authoritative setting-bound kit control
    else if(name=="kf_loop"||name=="kf_delay"||name=="kf_path"||name=="kf_even"||name=="kf_total"||
        name=="kf_ease_ends"||name=="kf_ease_length"||name=="kf_pace_mode"||name=="kf_pace"||name=="kf_hold"||
        name=="kf_ease"||name=="kf_zoom"||name=="kf_roll"||name=="kf_handles"||name=="kf_update"||
        name=="kf_look"||name=="kf_joint"||name=="kf_own_joint")
    {
        if((name=="kf_joint"||name=="kf_own_joint"||name=="kf_look")&&!canLock()) return;
        edit("Edit camera keyframes",[&](auto& seq,auto& t,auto* key)
        {
            if(name=="kf_loop")seq.loop=value.asBoolean();
            else if(name=="kf_delay")seq.startDelay=value.asReal();
            else if(name=="kf_path")t.settings.path=static_cast<ALKf::Path>(value.asInteger());
            else if(name=="kf_even")
            {
                if(value.asBoolean())
                {
                    ALKf::Cache c;std::string why;
                    if(!ALKf::buildCache(t,c,why)) {mMessage=why;return;}
                    t.settings.total=c.end;
                }
                t.settings.evenSpeed=value.asBoolean();
            }
            else if(name=="kf_total") {std::string why;if(!ALKf::editTotal(t,value.asReal(),why)) mMessage=why;}
            else if(name=="kf_ease_ends")t.settings.easeEnds=value.asBoolean();
            else if(name=="kf_ease_length")t.settings.easeSeconds=value.asReal();
            else if(name=="kf_joint"||name=="kf_own_joint")
            {
                auto* ref=name=="kf_joint"?&t.lock:key?&key->aim.target:nullptr;
                if(ref)
                {
                    if(value.asString().empty())ref->joint=0;
                    else if(auto* object=gObjectList.findObject(uuid(ref->id))) if(auto* avatar=object->asAvatar())
                        if(auto* joint=avatar->getJoint(value.asString()))ref->joint=joint->getJointNum();
                }
            }
            else if(key)
            {
                if(name=="kf_pace_mode") {key->out.mode=static_cast<ALKf::PaceMode>(value.asInteger());}
                else if(name=="kf_pace")key->out.value=value.asReal();
                else if(name=="kf_hold")key->hold=value.asReal();
                else if(name=="kf_ease")key->ease=value.asBoolean();
                else if(name=="kf_zoom")key->values[channel(t,"fov")]=2*std::atan(18./value.asReal());
                else if(name=="kf_roll")
                {
                    const double old=std::get<double>(key->values[channel(t,"roll")]), roll=value.asReal()*ALKf::degree;
                    auto q=std::get<ALKf::Quat>(key->values[channel(t,"rotation")]);
                    key->values[channel(t,"rotation")]=q*ALKf::axisAngle({1,0,0},roll-old);key->values[channel(t,"roll")]=roll;
                }
                else if(name=="kf_handles")
                {
                    if(key->handles.mode==ALKf::HandleMode::Auto)
                    {
                        ALKf::Cache c;std::string why;
                        if(ALKf::buildCache(t,c,why))
                        {
                            const size_t i=static_cast<size_t>(key-t.keys.data());
                            const auto pos=std::get<ALKf::Vec3>(key->values[c.position]);
                            const auto& curves=c.channels[c.position].spatial;
                            if(i>0)key->handles.in=curves[i-1].curve.p[2]-pos;
                            if(i<curves.size())key->handles.out=curves[i].curve.p[1]-pos;
                        }
                    }
                    key->handles.mode=static_cast<ALKf::HandleMode>(value.asInteger());
                }
                else if(name=="kf_look")key->aim.mode=static_cast<ALKf::AimMode>(value.asInteger());
                else if(name=="kf_update")
                {const auto pose=ALKeyframeCamera().capture();
                    const char* channels[]={"position","rotation","fov","roll"};
                    for(size_t i=0;i<4;++i)key->values[channel(t,channels[i])]=pose.values[i];}
            }
        });
    }
    refresh(true);
}


void ALPanelKeyframes::refresh(bool force)
{
    if(!mTimeline||ALCleanFrame::active()) return;
    auto& p=ALKeyframePlayer::instance();auto& seq=p.sequence();
    auto* t=camera(seq);const auto* cache=p.cameraCache();
    mTimeline->refresh();
    const bool selected=mSelection!=mTimeline->selected();
    if(selected||mCastRevision!=LLDirectorCast::instance().getActorChoiceRevision()) refreshActors();
    const bool changed=force||selected||mRevision!=seq.revision;
    mSelection=mTimeline->selected();
    ALKf::Key* key=nullptr;size_t index=0;
    if(t) for(size_t i=0;i<t->keys.size();++i) if(t->keys[i].id==mSelection) {key=&t->keys[i];index=i;}
    auto set=[&](const char* name,const LLSD& value)
    {auto* c=control(name);if(c&&setValueIfChanged(c,value,selected||value.isBoolean()||(force&&std::string(name)!="kf_name"))) ALKitRow::notifyValueChanged(c);};
    set("kf_playing",p.state()==ALKfTransport::State::START_DELAY||p.state()==ALKfTransport::State::PLAYING||p.state()==ALKfTransport::State::HOLD_END);
    set("kf_track",mLoadedName.empty()?std::string():"file:"+mLoadedName);
    set("kf_playhead",p.time());set("kf_show",mShow);set("kf_dots",mDots);set("kf_icons",mIcons);
    set("kf_edit",LLToolMgr::getInstance()->getCurrentTool()==mTool.get());
    std::string why;const bool restricted=p.restricted(why);
    const bool unlocked=!blocked();
    for(const auto& [name,c]:mControls)
    {
        bool enabled=true;
        if(name=="kf_playing")enabled=!restricted||ALCleanFrame::active();
        else if(name=="kf_release")enabled=p.ownsCamera();
        else if(name=="kf_visit"||name=="kf_playhead")enabled=!restricted&&!p.rlvHandbackActive()&&!ALCleanFrame::active()&&(name!="kf_visit"||key);
        else if(name=="kf_lock"||name=="kf_joint"||name=="kf_lock_pick"||name=="kf_look"||name=="kf_own"||name=="kf_own_joint"||name=="kf_own_pick")enabled=unlocked&&!restricted&&(name=="kf_lock"||name=="kf_joint"||name=="kf_lock_pick"||key);
        else if(name=="kf_undo"||name=="kf_redo")enabled=mPage->canUndoOnPage(name=="kf_redo");
        else if(name!="kf_show"&&name!="kf_dots"&&name!="kf_icons"&&name!="kf_status"&&name!="kf_lock_status"&&name!="kf_result")enabled=unlocked;
        const bool needsKey=name=="kf_time"||name=="kf_pace_mode"||name=="kf_pace"||name=="kf_hold"||
            name=="kf_ease"||name=="kf_zoom"||name=="kf_roll"||name=="kf_handles"||name=="kf_update"||name=="kf_delete";
        if(needsKey&&!key)enabled=false;
        if((name=="kf_pace"||name=="kf_pace_mode")&&t&&key&&index+1==t->keys.size())enabled=false;
        if((name=="kf_even"||name=="kf_total")&&(!t||t->keys.size()<2))enabled=false;
        setEnabledIfChanged(c,enabled);
        setToolTipIfChanged(c,restricted?why:mTooltips[name]);
    }
    std::string status=restricted?why:!mMessage.empty()?mMessage:p.status();
    if(cache)
    {
        for(const auto& ch:cache->channels)
        {
            if(std::any_of(ch.rotation.begin(),ch.rotation.end(),[](const auto& r){return r.highAngle;}))status+=" | Warning: rotation exceeds 170 degrees";
            if(std::any_of(ch.spatial.begin(),ch.spatial.end(),[](const auto& a){return a.capped;}))status+=" | Arc sampling capped";
        }
    }
    set("kf_status",status);set("kf_lock_status",restricted?why:t&&t->lock.kind!=ALKf::RefKind::None?t->lock.label:"No Lock target");
    set("kf_result",mFitResult);set("kf_smoothing","Automatic target smoothing (0.15 s)");
    if(changed)
    {
        set("kf_name",seq.name);set("kf_loop",seq.loop);set("kf_delay",seq.startDelay);
        if(t)
        {
            set("kf_path",static_cast<int>(t->settings.path));set("kf_even",t->settings.evenSpeed);
            set("kf_total",cache?cache->end:0);set("kf_ease_ends",t->settings.easeEnds);set("kf_ease_length",t->settings.easeSeconds);
            set("kf_lock",t->lock.kind==ALKf::RefKind::None?"":uuid(t->lock.id).asString());
            auto showJoint=[&](const char* name,const ALKf::RefSlot& ref)
            {
                std::string joint;
                if(ref.joint)if(auto* object=gObjectList.findObject(uuid(ref.id)))if(auto* avatar=object->asAvatar())if(auto* j=avatar->getJoint(ref.joint))joint=j->getName();
                set(name,joint);
            };
            showJoint("kf_joint",t->lock);
            if(key)
            {
                set("kf_time",cache?cache->arrival[index]:0);set("kf_pace_mode",static_cast<int>(key->out.mode));set("kf_pace",key->out.value);
                set("kf_hold",key->hold);set("kf_ease",key->ease);set("kf_zoom",18./std::tan(std::get<double>(key->values[channel(*t,"fov")])/2));set("kf_roll",std::get<double>(key->values[channel(*t,"roll")])/ALKf::degree);
                set("kf_handles",static_cast<int>(key->handles.mode));set("kf_look",static_cast<int>(key->aim.mode));
                set("kf_own",key->aim.target.kind==ALKf::RefKind::None?"":uuid(key->aim.target.id).asString());showJoint("kf_own_joint",key->aim.target);
                const bool pan=cache&&index<cache->timing.size()&&cache->distance[index+1]-cache->distance[index]<1e-5;
                set("kf_segment",index+1==t->keys.size()?"End key":pan?"Pan (Duration only)":"Moving");
                if(pan) setEnabledIfChanged(control("kf_pace_mode"),false);
            }
        }
        mRevision=seq.revision;
    }
    if(mOwner)
    {
        if(mShow&&t&&cache&&!p.rlvHandbackActive())
        {
            if(changed||mOverlayRevision!=seq.revision)
            {ALKeyframeOverlay::refresh(*t,*cache,mSelection,mDots,mIcons);mOverlayRevision=seq.revision;}
            p.setGuideDraw(ALKeyframeOverlay::draw);
        }
        else {p.setGuideDraw({});ALKeyframeOverlay::clear();}
    }
}

