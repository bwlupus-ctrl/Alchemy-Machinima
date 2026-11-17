#include "llviewerprecompiledheaders.h"
#include "alkeyframeplayer.h"
#include "alkeyframecamera.h"
#include "alkeyframeviewer.h"
#include "alcameradispatch.h"
#include "alcleantelemetry.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llagentpilot.h"
#include "llappviewer.h"
#include "llflycamrecorder.h"
#include "llfocusmgr.h"
#include "llmenugl.h"
#include "lltooltip.h"
#include "llfloaterreg.h"
#include "llsetkeybinddialog.h"
#include "llviewerjoystick.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "lltemporalframecontext.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "rlvhandler.h"
#include <algorithm>

struct ALKeyframePlayer::Impl
{
    ALKfTransport transport;
    ALKf::Sequence sequence;
    ALKf::History history;
    ALKeyframeCamera camera;
    struct Entry { std::uint32_t id; std::string uid; ALKf::Cache cache; ALKf::ClientRuntime runtime; };
    std::vector<Entry> entries;
    std::string status;
    std::function<void()> cancelEdit, guideDraw;
    boost::signals2::scoped_connection restriction;
    LLViewerRegion* region = nullptr;
    double wallDelta = 0, wall = 0, lastSent = 0;
    F32 lastFov = 0;
    U64 generation = 0, lastPresented = 0;
    bool advanced = false, alive = true;
    bool prepare()
    {
        if (entries.size() != sequence.tracks.size()) entries.clear();
        for (std::size_t i=0; i<sequence.tracks.size(); ++i)
        {
            const auto& track = sequence.tracks[i];
            if(i==entries.size()) entries.push_back({track.id,track.uid,{},{}});
            auto& e=entries[i];
            if(e.id!=track.id || e.uid!=track.uid || e.cache.revision!=track.revision || e.cache.channels.empty())
            {
                if(!ALKf::buildCache(track,e.cache,status)) return false;
                e.id=track.id; e.uid=track.uid; e.runtime.reset();
            }
        }
        return true;
    }
    void broadcast(bool force)
    {
        const F32 fov=LLViewerCamera::getInstance()->getView();
        if(gDisconnected || !gAgent.getRegion() || !gMessageSystem) return;
        if(force || (wall-lastSent>=1 && std::abs(fov-lastFov)>2*ALKf::degree))
        {
            LLViewerCamera::getInstance()->sendViewToSimulator(fov);
            lastFov=fov; lastSent=wall;
        }
    }
};
ALKeyframePlayer& ALKeyframePlayer::instance() { static ALKeyframePlayer player; return player; }
ALKeyframePlayer::ALKeyframePlayer() : mImpl(std::make_unique<Impl>())
{
    mImpl->restriction=gRlvHandler.setBehaviourToggleCallback([](ERlvBehaviour,ERlvParamType){ ALKeyframeViewer::dirtyRestriction(); });
}
ALKeyframePlayer::~ALKeyframePlayer() = default;
ALKf::Sequence& ALKeyframePlayer::sequence() { return mImpl->sequence; }
ALKf::History& ALKeyframePlayer::history() { return mImpl->history; }
ALKfTransport::State ALKeyframePlayer::state() const { return mImpl->transport.state(); }
double ALKeyframePlayer::time() const { return mImpl->transport.time(); }
std::uint64_t ALKeyframePlayer::revision() const { return mImpl->sequence.revision; }
const std::string& ALKeyframePlayer::status() const { return mImpl->status; }
bool ALKeyframePlayer::ownsCamera() const { return mImpl->transport.ownsCamera(); }
bool ALKeyframePlayer::rlvHandbackActive() const { return mImpl->transport.rlvHandbackActive(); }
bool ALKeyframePlayer::restricted(std::string& why) const { return mImpl->camera.restricted(why); }
void ALKeyframePlayer::setCancelEdit(std::function<void()> cancel) { mImpl->cancelEdit=std::move(cancel); }
void ALKeyframePlayer::setGuideDraw(std::function<void()> draw) { mImpl->guideDraw=std::move(draw); }
bool ALKeyframePlayer::hasGuideDraw() const { return bool(mImpl->guideDraw) && !ALCleanFrame::active() && !rlvHandbackActive(); }
void ALKeyframePlayer::drawGuides() const
{
    if(!ALCleanFrame::active() && hasGuideDraw()) mImpl->guideDraw();
}
double ALKeyframePlayer::duration()
{
    if(!mImpl->prepare()) return 0;
    double end=0; for(const auto& e:mImpl->entries) end=std::max(end,e.cache.end); return end;
}
const ALKf::Cache* ALKeyframePlayer::cameraCache()
{
    if(!mImpl->prepare()) return nullptr;
    for(std::size_t i=0;i<mImpl->entries.size();++i)
        if(sequence().tracks[i].kind=="camera") return &mImpl->entries[i].cache;
    return nullptr;
}
bool ALKeyframePlayer::play(bool action)
{
    auto& p=*mImpl;
    if(!p.alive || restricted(p.status) || gDisconnected || !gAgent.getRegion() ||
       (gAgentPilot.isPlaying() && gAgentPilot.getOverrideCamera())) return false;
    if(p.cancelEdit) p.cancelEdit();
    p.history.cancelGesture(p.sequence);
    if(!p.prepare()) return false;
    const auto* cache=cameraCache();
    if(!cache || cache->arrival.empty()) { p.status="No camera keys"; return false; }
    LLFlycamRecorder::instance().stopPlayback(); // capture is deliberately allowed
    if(!p.transport.play(true,duration(),p.sequence.loop,p.sequence.startDelay,action)) return false;
    for(auto& e:p.entries) e.runtime.reset();
    p.region=gAgent.getRegion();
    lifecycleCheck(); // consume acquire before the first presentation
    p.wallDelta=0; updateCamera(); p.broadcast(true);
    p.status="Playing"; return true;
}
bool ALKeyframePlayer::scrub(double t)
{
    auto& p=*mImpl;
    if(!p.alive || restricted(p.status) || gDisconnected || !gAgent.getRegion() || !std::isfinite(t) ||
       (gAgentPilot.isPlaying() && gAgentPilot.getOverrideCamera())) return false;
    const auto* cache=cameraCache(); if(!cache || cache->arrival.empty()) return false;
    if(p.transport.clean()) stop(ALKfTransport::Exit::NONE);
    LLFlycamRecorder::instance().stopPlayback();
    p.transport.preview(std::clamp(t,0.,duration())); p.region=gAgent.getRegion();
    for(auto& e:p.entries) e.runtime.reset();
    updateCamera(); return true;
}
bool ALKeyframePlayer::visit(std::uint32_t key)
{
    const auto* cache=cameraCache(); if(!cache) return false;
    for(const auto& track:sequence().tracks) if(track.kind=="camera")
        for(std::size_t i=0;i<track.keys.size();++i) if(track.keys[i].id==key) return scrub(cache->arrival[i]);
    return false;
}
bool ALKeyframePlayer::replaceSequence(ALKf::Sequence next,std::string& reason)
{
    if(!ALKf::validate(next,reason)) return false;
    stop(ALKfTransport::Exit::SCENE);
    next.revision=std::max(next.revision,sequence().revision)+1;
    mImpl->sequence=std::move(next); mImpl->history={}; mImpl->entries.clear(); return true;
}
void ALKeyframePlayer::stop(ALKfTransport::Exit reason)
{
    mImpl->transport.stop(reason); lifecycleCheck();
}
void ALKeyframePlayer::lifecycleCheck()
{
    auto& p=*mImpl; if(!p.alive) return;
    if(state()==ALKfTransport::State::PREVIEW && LLViewerJoystick::getInstance()->getOverrideCamera())
    {
        // Cached declared stick dead zones; an intentional flycam input releases Visit.
        static LLCachedControl<S32> a0(gSavedSettings,"JoystickAxis0",0),a1(gSavedSettings,"JoystickAxis1",1),a2(gSavedSettings,"JoystickAxis2",2);
        static LLCachedControl<S32> a3(gSavedSettings,"JoystickAxis3",3),a4(gSavedSettings,"JoystickAxis4",4),a5(gSavedSettings,"JoystickAxis5",5);
        static LLCachedControl<F32> d0(gSavedSettings,"FlycamAxisDeadZone0",.1f),d1(gSavedSettings,"FlycamAxisDeadZone1",.1f),d2(gSavedSettings,"FlycamAxisDeadZone2",.1f);
        static LLCachedControl<F32> d3(gSavedSettings,"FlycamAxisDeadZone3",.1f),d4(gSavedSettings,"FlycamAxisDeadZone4",.1f),d5(gSavedSettings,"FlycamAxisDeadZone5",.1f);
        const S32 axes[]={a0,a1,a2,a3,a4,a5}; const F32 dead[]={d0,d1,d2,d3,d4,d5};
        for(unsigned i=0;i<6;++i) if(axes[i]>=0 && std::abs(LLViewerJoystick::getInstance()->getJoystickAxis(axes[i]))>dead[i])
        { p.transport.stop(ALKfTransport::Exit::USER_INPUT); break; }
    }
    const bool regionChanged=ownsCamera() && p.region && p.region!=gAgent.getRegion();
    p.transport.lifecycleCheck({gTeleportDisplay || gAgent.getTeleportState()!=LLAgent::TELEPORT_NONE || regionChanged,
        gDisconnected || LLAppViewer::instance()->logoutRequestSent(), ALKeyframeViewer::cameraRestricted(),
        gAgentPilot.isPlaying() && gAgentPilot.getOverrideCamera(), LLFlycamRecorder::instance().isPlaybackActive(),false});
    const auto fx=p.transport.takeEffects();
    if(fx.acquire)
    {
        ALCleanTelemetry::begin(); ALCleanFrame::setActive(true);
        if(p.cancelEdit) p.cancelEdit();
        gFocusMgr.setMouseCapture(NULL); // stock callbacks must finalize their grabs
        if(LLMenuGL::sMenuContainer) LLMenuGL::sMenuContainer->hideMenus();
        LLToolTipMgr::instance().blockToolTips();
        LLFloaterReg::hideInstance("keybind_dialog");
        gFocusMgr.setKeyboardFocus(NULL);
        ALKeyframeViewer::cleanParticleTransition();
    }
    if(fx.release)
    {
        ++ALCleanTelemetry::releases;
        auto samples=ALCleanTelemetry::intervals;
        const auto count=std::min<std::uint64_t>(ALCleanTelemetry::frames,samples.size());
        std::sort(samples.begin(),samples.begin()+count);
        LL_INFOS("Keyframes") << "clean verdict " << ALCleanTelemetry::verdict()
            << " presented " << ALCleanTelemetry::frames << " frame interval p50/p95/max "
            << (count?samples[count/2]:0) << "/" << (count?samples[(count-1)*95/100]:0) << "/" << ALCleanTelemetry::maximum
            << " owner conflicts " << ALCleanTelemetry::conflicts << " exits " << (int)fx.exit
            << " lease " << ALCleanTelemetry::acquires << "/" << ALCleanTelemetry::releases << LL_ENDL;
        for(int i=0;i<ALCleanTelemetry::Count;++i)
        {
            const auto& c=ALCleanTelemetry::sites[i];
            LL_INFOS("Keyframes") << "clean site " << i << " evaluated/blocked/forbidden " << c.evaluated << "/" << c.blocked << "/" << c.forbidden << LL_ENDL;
        }
        ALCleanFrame::setActive(false); ALKeyframeViewer::cleanParticleTransition(); p.broadcast(true);
    }
    if(fx.resetFlycam) LLViewerJoystick::getInstance()->setNeedsReset(true);
    if(fx.resetAgent) gAgentCamera.resetView(true,false);
}
void ALKeyframePlayer::advance(const LLTemporalFrameContext& frame)
{
    auto& p=*mImpl;
    if(p.advanced && p.generation==frame.generation) return;
    p.advanced=true; p.generation=frame.generation; p.wallDelta=frame.wall_delta; p.wall=frame.wall_time;
    p.transport.advance({frame.generation,frame.wall_delta,frame.presentation_delta,frame.drives(LLTemporalFeature::CAMERA)});
}
void ALKeyframePlayer::updateCamera()
{
    auto& p=*mImpl; if(!ownsCamera() || !p.prepare()) return;
    for(std::size_t i=0;i<p.entries.size();++i) if(p.sequence.tracks[i].kind=="camera")
    {
        p.entries[i].runtime.wallDelta=p.wallDelta;
        p.camera.apply(p.sequence.tracks[i],p.entries[i].cache,time(),p.entries[i].runtime);
    }
    p.broadcast(false);
}
void ALKeyframePlayer::presented()
{
    if(ALCleanFrame::active() && mImpl->lastPresented!=mImpl->generation)
    {
        mImpl->lastPresented=mImpl->generation;
        if(gCameraDispatchOwner!=ECameraOwner::KEYFRAME) ++ALCleanTelemetry::conflicts;
        ALCleanTelemetry::frame(mImpl->wallDelta);
    }
}
void ALKeyframePlayer::onDisconnect()
{
    mImpl->transport.lifecycleCheck({false,true}); lifecycleCheck();
}
void ALKeyframePlayer::shutdown()
{
    mImpl->alive=false; mImpl->transport.stop(ALKfTransport::Exit::SHUTDOWN);
    ALCleanFrame::setActive(false); // shutdown: no UI or renderer calls
}
void ALKeyframePlayer::rewriteActorReferences(const ALKf::Id& oldId,const ALKf::Id& newId)
{
    LL_PROFILE_ZONE_NAMED("keyframe rekey");
    bool liveAffected=false;
    auto matches=[&](const ALKf::RefSlot& ref){ return ref.kind==ALKf::RefKind::Clone && ref.id==oldId; };
    std::vector<bool> current(mImpl->entries.size(),false);
    for(std::size_t i=0;i<sequence().tracks.size();++i)
    {
        const auto& track=sequence().tracks[i];
        bool affected=matches(track.lock);
        for(const auto& key:track.keys) affected |= matches(key.aim.target);
        liveAffected |= affected;
        if(i<mImpl->entries.size())
        {
            const auto& entry=mImpl->entries[i];
            current[i]=!affected && entry.id==track.id && entry.uid==track.uid && entry.cache.revision==track.revision;
        }
    }
    mImpl->history.rekey(sequence(),[&](ALKf::RefSlot& ref){ if(matches(ref)) { ref.id=newId; if(newId==ALKf::Id{}) ref.kind=ALKf::RefKind::Point; } });
    // Restamp only caches proven current BEFORE rekey; pending edits and identities
    // rebuild normally. Runtime for unrelated live references remains continuous.
    for(std::size_t i=0;i<current.size();++i)
        if(current[i]) mImpl->entries[i].cache.revision=sequence().tracks[i].revision;
    if(liveAffected && newId==ALKf::Id{} && ownsCamera()) stop(ALKfTransport::Exit::TARGET_GONE);
}

void ALKeyframePlayer::targetRemoved(const ALKf::Id& id)
{
    if(!ownsCamera() || id==ALKf::Id{}) return;
    const auto matches=[&](const ALKf::RefSlot& ref){
        return ref.kind!=ALKf::RefKind::None && ref.kind!=ALKf::RefKind::Point && ref.id==id;
    };
    for(const auto& track:sequence().tracks)
    {
        if(matches(track.lock)) { stop(ALKfTransport::Exit::TARGET_GONE); return; }
        for(const auto& key:track.keys)
            if(matches(key.aim.target)) { stop(ALKfTransport::Exit::TARGET_GONE); return; }
    }
}
