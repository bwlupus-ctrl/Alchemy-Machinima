/**
* @file media_plugin_libvlc.cpp
* @brief LibVLC plugin for LLMedia API plugin system
*
* @cond
* $LicenseInfo:firstyear=2008&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
* @endcond
*/

#include "linden_common.h"

#include "llgl.h"
#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llplugingateclock.h"
#include "llpluginmessageclasses.h"
#include "media_plugin_base.h"

#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "vlc/vlc.h"
#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>
#include <deque>
#include <cstring>
#include <thread>
#include <condition_variable>
#include "../../newview/alvcammediafilepolicy.h"
#include "../../newview/alvcammediafileposix.h"
#include "vlc/libvlc_version.h"

#if LL_WINDOWS
// needed for waveOut call - see below for description
#include <mmsystem.h>
#endif

////////////////////////////////////////////////////////////////////////////////
//
class MediaPluginLibVLC :
    public MediaPluginBase
{
public:
    MediaPluginLibVLC(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data);
    ~MediaPluginLibVLC();

    /*virtual*/ void receiveMessage(const std::string& message_string);

private:
    bool init();

    void initVLC();
    void playMedia(bool admittedEdit = false, double position = 0.);
    void setLoop(bool loop);
    static void repeatLog(void*, int, const libvlc_log_t*, const char*, va_list);
    void resetVLC();
    void stopPlayer();
    void reportVideoSize();
    void tickPrivateFile();
    void privateAdmissionMessage(const char* reason = "player opened", unsigned tracks = 0, bool video = false);
    static void privateEvents(const libvlc_event_t* event, void* ptr);
    void setVolume(const F64 volume);
    void setVolumeVLC();
    void updateTitle(const char* title);

    static void* lock(void* data, void** p_pixels);
    static void unlock(void* data, void* id, void* const* raw_pixels);
    static void display(void* data, void* id);
    static void displayFrame(void* data, void* id);

    /*virtual*/ void setDirty(int left, int top, int right, int bottom) /* override, but that is not supported in gcc 4.6 */;
    void setDurationDirty();
    void setFrameDirty(int left, int top, int right, int bottom);

    static void eventCallbacks(const libvlc_event_t* event, void* ptr);

    libvlc_instance_t* mLibVLC;
    libvlc_media_t* mLibVLCMedia;
    libvlc_media_player_t* mLibVLCMediaPlayer;

    struct mLibVLCContext
    {
        unsigned char* texture_pixels;
        libvlc_media_player_t* mp;
        MediaPluginLibVLC* parent = nullptr;
        // Message-thread only. A setting transition can subscribe to this
        // same player's retained pixels; decoder callbacks never read tokens.
        std::string loadToken;
        const int width = 0;
        const int height = 0;
        std::atomic<bool> frameReady{false};
        std::atomic<bool> hasFrame{false};
        std::atomic<bool> trackFrames{false};
        std::atomic<unsigned> events{0};
        std::atomic<unsigned> wrapState{0};
        ALMediaFile::WrapPolicy wrapPolicy; // time samples: message thread only
        void latchWrap() { wrapState.fetch_or(ALMediaFile::WrapPolicy::Marker, std::memory_order_acq_rel); }
        unsigned publishAdmission()
        {
            unsigned state = wrapState.load(std::memory_order_acquire);
            unsigned next;
            do { next = ALMediaFile::WrapPolicy::admit(state); }
            while (!wrapState.compare_exchange_weak(state, next, std::memory_order_acq_rel, std::memory_order_acquire));
            return next;
        }
        std::atomic<uint64_t> repeats{0};
        uint64_t presentedRepeat = 0; // display callback only
        std::atomic<double> lastPresentation{0.};
        std::mutex seamMutex;
        std::atomic<double> frameMilliseconds{0.};
        double recentFrameMilliseconds = 0.; // display callback only
        struct Seam { double gap, frame; };
        std::deque<Seam> seamGaps;
        mLibVLCContext() = default;
        mLibVLCContext(unsigned char* pixels, libvlc_media_player_t* player,
                       const std::string& token, int w, int h)
            : texture_pixels(pixels), mp(player), loadToken(token), width(w), height(h),
              trackFrames(!token.empty()) {}
    };
    std::unique_ptr<mLibVLCContext> mLibVLCCallbackContext;
    std::string mLoadToken;
    unsigned mVideoWidth = 0;
    unsigned mVideoHeight = 0;
    unsigned mVideoSarNum = 1, mVideoSarDen = 1;

    std::string mURL;
    F64 mCurVolume;

    bool mIsLooping;
    bool mInputLoop = false;
    ALMediaFile::ResumeWait mResume;
    double mRecoveryPresentation = 0.;

    F64 mCurTime;
    F64 mDuration;
    EStatus mVlcStatus;
    bool mPrivateFile = false;
    std::string mPrivateDemux;
    ALMediaFile::Transport mDesired = ALMediaFile::Transport::STOPPED;
    ALMediaFile::Admission mAdmission;
    uint64_t mPlayerToken = 0;
    double mNextAdmissionCheck = 0;
    bool mPrivateStarted = false;
    bool mPrivateEnded = false;
    // Independent 10 ms step-edge sampler and gain timer. It never sends a
    // plugin message or touches frame storage. stopPlayer fences its pointer.
    std::mutex mGateMutex;
    std::condition_variable mGateWake;
    std::thread mGateTimer;
    bool mGateQuit = false;
    U32 mGateReaders = 0;
    bool mGateParkFirstFrame = false;
    std::atomic<bool> mGateRestartParking{false}, mGateFirstFrame{false};
    libvlc_media_player_t* mGatePlayer = nullptr;
    U64 mGateTransportToken = 0, mGateEnvelopeToken = 0, mGateSampleToken = 0;
    bool mGateSamplePaused = false, mGateSamplePlaying = false;
    bool mGateOwned = false;
    F64 mGateGain = 0., mGateFrom = 0., mGateTo = 0., mGateStart = 0., mGateLength = 0.;
    F64 mGateBase = 0., mGateSample = 0., mGatePosition = 0.;
    F64 mGateSampleError = 1., mGateViewerSend = 0., mGateSocketReceipt = 0., mGatePausedAt = 0.;
    F64 mGateFramePeriod = .1, mGateEnvelopeReceipt = 0.;
    bool mGateEnvelopeDegraded = true, mGateNativeLoop = false;
    LLPluginGateClock mGateClock;
    void gateTimer();
    void gateReport();
#if !LL_WINDOWS
    int mPrivateFD = -1;
    ALMediaFile::FileIdentity mPrivateIdentity;
    bool mPrivateIdentityValid = false;
#endif
};

static double privateNow()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Observe VLC's exact native repeat boundary, without querying LibVLC from
// vout callbacks or relying on its 250ms TimeChanged clock samples. Only this
// fixed repeat/seek-refusal formats are inspected; no paths/log text is emitted.
void MediaPluginLibVLC::repeatLog(void* data, int, const libvlc_log_t*, const char* format, va_list)
{
    if (std::strcmp(format, "repeating the same input (%d)") == 0)
    {
        auto* context = static_cast<mLibVLCContext*>(data);
        context->latchWrap(); // one atomic RMW latches admission AT the marker
        context->repeats.fetch_add(1, std::memory_order_release);
        context->events.fetch_or(16u, std::memory_order_release);
    }
    else if (std::strncmp(format, "INPUT_CONTROL_SET_TIME ", sizeof("INPUT_CONTROL_SET_TIME ") - 1) == 0 &&
             std::strstr(format, " failed or not possible"))
    {
        // VLC 3's void set_time reports a demux refusal only here. Inspect the
        // format, never varargs. ResumeWait's deadline also covers format drift.
        static_cast<mLibVLCContext*>(data)->events.fetch_or(32u, std::memory_order_release);
    }
}

void MediaPluginLibVLC::setLoop(bool loop)
{
    mIsLooping = loop;
    if (mPrivateFile && mLibVLCMediaPlayer && mInputLoop != loop && mDesired != ALMediaFile::Transport::STOPPED)
    {
        const bool seekable = mAdmission.seekable && libvlc_media_player_is_seekable(mLibVLCMediaPlayer);
        const double position = mResume.loopTarget(seekable
            ? std::max(0., static_cast<double>(libvlc_media_player_get_time(mLibVLCMediaPlayer)) / 1000.) : 0., seekable);
        // Explicit user edit only. libvlc cannot update input-repeat on a live
        // input. Reprepare at its position and re-admit with gain/publication 0.
        playMedia(true, position);
    }
}

void MediaPluginLibVLC::privateEvents(const libvlc_event_t* event, void* ptr)
{
    auto* context = static_cast<mLibVLCContext*>(ptr);
    // Immutable producing-player context, quiesced before release. Decoder
    // callbacks do no LibVLC queries, messaging, transport or volume writes.
    const unsigned flags = event->type == libvlc_MediaPlayerEndReached ? 2u :
        event->type == libvlc_MediaPlayerEncounteredError ? 4u :
        event->type == libvlc_MediaPlayerPlaying ? 8u : 1u;
    context->events.fetch_or(flags, std::memory_order_release);
}

void MediaPluginLibVLC::privateAdmissionMessage(const char* reason, unsigned tracks, bool video)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "private_file_admission");
    message.setValue("load_token", mLoadToken);
    message.setValueS32("verdict", mAdmission.verdict == ALMediaFile::Verdict::ADMITTED ? 1 :
        mAdmission.verdict == ALMediaFile::Verdict::REJECTED ? -1 : 0);
    message.setValue("reason", reason);
    message.setValueS32("tracks", tracks);
    message.setValueBoolean("video", video);
    message.setValueBoolean("seekable", mAdmission.seekable);
    message.setValueBoolean("creation_loop", mInputLoop);
    message.setValueBoolean("opened", mLibVLCMediaPlayer != nullptr);
    message.setValueBoolean("loop_reverify", mAdmission.loopReverify);
    message.setValue("player_token", std::to_string(mPlayerToken));
    sendMessage(message);
}

void MediaPluginLibVLC::tickPrivateFile()
{
    if (!mPrivateFile || !mLibVLCMediaPlayer || !mLibVLCCallbackContext || !mPrivateStarted || mPrivateEnded) return;
    auto& context = *mLibVLCCallbackContext;
    if (mGateParkFirstFrame && context.hasFrame.load(std::memory_order_acquire))
    {
        mGateParkFirstFrame = false; mGateRestartParking = false; mDesired = ALMediaFile::Transport::PAUSED;
        libvlc_media_player_set_pause(mLibVLCMediaPlayer, 1);
        setVolumeVLC();
    }
    const unsigned flags = context.events.exchange(0, std::memory_order_acquire);
    const double now = privateNow();
    std::deque<mLibVLCContext::Seam> gaps;
    {
        std::lock_guard<std::mutex> guard(context.seamMutex);
        gaps.swap(context.seamGaps);
    }
    for (const auto& seam : gaps)
    {
        LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "loop_seam");
        message.setValue("load_token", mLoadToken);
        message.setValueReal("gap_ms", seam.gap);
        message.setValueReal("frame_ms", seam.frame);
        sendMessage(message); // viewer owns logging/aggregation, never SLPlugin's disabled file log
    }
    if (!flags && !(mResume.position > 0.) && now < mNextAdmissionCheck &&
        !ALMediaFile::WrapPolicy::rejects(context.wrapState.load(std::memory_order_acquire))) return;
    mNextAdmissionCheck = now + .1;
    mDuration = static_cast<double>(libvlc_media_get_duration(mLibVLCMedia)) / 1000.;
    mCurTime = static_cast<double>(libvlc_media_player_get_time(mLibVLCMediaPlayer)) / 1000.;
    const auto before = mAdmission.verdict;
    const bool seekable = mAdmission.seekable;
    bool video = false;
    libvlc_media_track_t** tracks = nullptr;
    const auto count = libvlc_media_tracks_get(mLibVLCMedia, &tracks);
    for (unsigned i = 0; i < count; ++i)
        if (tracks[i] && tracks[i]->i_type == libvlc_track_video)
        {
            video = true;
            const auto* track = tracks[i]->video;
            if (track && track->i_frame_rate_num && track->i_frame_rate_den)
                context.frameMilliseconds.store(1000. * track->i_frame_rate_den / track->i_frame_rate_num, std::memory_order_relaxed);
        }
    libvlc_media_tracks_release(tracks, count);
    const bool playing = libvlc_media_player_get_state(mLibVLCMediaPlayer) == libvlc_Playing;
    const auto resume = mResume.step(now, playing,
        (flags & 32u) || (playing && !libvlc_media_player_is_seekable(mLibVLCMediaPlayer)), mCurTime);
    if (resume == ALMediaFile::ResumeWait::Action::SEEK)
    {
        // VLC 3's setter returns void: an unreported demux refusal is bounded
        // by ResumeWait; a reported non-seekable input releases it immediately.
        libvlc_media_player_set_time(mLibVLCMediaPlayer, static_cast<libvlc_time_t>(mResume.position * 1000.));
    }
    if (resume != ALMediaFile::ResumeWait::Action::READY) video = false;
    if (context.wrapPolicy.timeRegression(mCurTime, mDuration, before == ALMediaFile::Verdict::PENDING))
        context.latchWrap();
    // Audio-first/Playing/ESAdded never proves enumeration complete. Unknown
    // length is a transport capability only, not an admission condition.
    const bool pendingWrap = ALMediaFile::WrapPolicy::rejects(context.wrapState.load(std::memory_order_acquire));
    mAdmission.observe(mPlayerToken, now, video, pendingWrap || (flags & 4) || ((flags & 2) && before != ALMediaFile::Verdict::ADMITTED), mDuration);
#if !LL_WINDOWS
    if ((!mPrivateIdentityValid || !ALMediaFile::unchangedFile(mPrivateFD, mPrivateIdentity)))
        mAdmission.verdict = ALMediaFile::Verdict::REJECTED;
#endif
    // The callback may run after flags.exchange or even observe(). Compete
    // with it in the SAME atomic word before enabling any audio/publication.
    if (mAdmission.verdict == ALMediaFile::Verdict::ADMITTED &&
        ALMediaFile::WrapPolicy::rejects(context.publishAdmission()))
        mAdmission.verdict = ALMediaFile::Verdict::REJECTED;
    if (mAdmission.verdict == ALMediaFile::Verdict::REJECTED)
    {
        mAdmission.seekable = false; mAdmission.loopReverify = false;
        context.wrapState.fetch_and(~ALMediaFile::WrapPolicy::Admitted, std::memory_order_release);
    }
    if (before != mAdmission.verdict || seekable != mAdmission.seekable)
        privateAdmissionMessage(mAdmission.verdict == ALMediaFile::Verdict::ADMITTED ? "video track admitted" :
            now >= mAdmission.deadline ? "video-track timeout" : (flags & 4) ? "decoder error" :
            ALMediaFile::WrapPolicy::rejects(context.wrapState.load(std::memory_order_acquire)) || (flags & 2)
                ? "ended before video admission" : "file identity changed", count, video);
    if (mAdmission.verdict == ALMediaFile::Verdict::REJECTED)
    {
        mDesired = ALMediaFile::Transport::STOPPED;
        setVolumeVLC(); stopPlayer(); mVlcStatus = STATUS_ERROR;
        return;
    }
    if ((flags & (2 | 16)) && ALMediaFile::unknownLoopRecovery(mAdmission, mIsLooping, mDesired, mDuration))
    {
        // mkv.cpp DEMUX_SET_POSITION/Seek refuse missing duration. Native
        // repeat can consume EndReached without making progress; its marker
        // wakes this bounded recovery as well as an ordinary terminal EOF.
        mRecoveryPresentation = context.lastPresentation.load(std::memory_order_acquire);
        privateAdmissionMessage(ALMediaFile::UNKNOWN_LOOP);
        playMedia(true);
        return;
    }
    setVolumeVLC();
    if ((flags & 2) && mAdmission.verdict == ALMediaFile::Verdict::ADMITTED)
    {
        // Native input-repeat consumes repeating EOF inside input.c. An EOF
        // delivered here is terminal (Loop OFF / exhausted / failed repeat).
        // Never stop/play or rebuild during an unattended loop.
        mPrivateEnded = true;
        mDesired = ALMediaFile::Transport::STOPPED;
        mVlcStatus = STATUS_DONE;
        setVolumeVLC();
    }
    else mVlcStatus = mDesired == ALMediaFile::Transport::PLAYING ? STATUS_PLAYING : STATUS_PAUSED;
}

////////////////////////////////////////////////////////////////////////////////
//
MediaPluginLibVLC::MediaPluginLibVLC(LLPluginInstance::sendMessageFunction host_send_func, void *host_user_data) :
MediaPluginBase(host_send_func, host_user_data)
{
    mTextureWidth = 0;
    mTextureHeight = 0;
    mWidth = 0;
    mHeight = 0;
    mDepth = 4;
    mPixels = 0;

    mLibVLC = 0;
    mLibVLCMedia = 0;
    mLibVLCMediaPlayer = 0;

    mCurVolume = 0.0;

    mIsLooping = false;

    mCurTime = 0.0;
    mDuration = 0.0;

    mURL = std::string();

    mVlcStatus = STATUS_NONE;
    setStatus(STATUS_NONE);
    mGateTimer = std::thread([this]() { gateTimer(); });
}

////////////////////////////////////////////////////////////////////////////////
//
MediaPluginLibVLC::~MediaPluginLibVLC()
{
    { std::lock_guard<std::mutex> guard(mGateMutex); mGateQuit = true; }
    mGateWake.notify_all();
    if (mGateTimer.joinable()) mGateTimer.join();
}

void MediaPluginLibVLC::gateTimer()
{
    std::unique_lock<std::mutex> guard(mGateMutex);
    while (!mGateQuit)
    {
        if (!mGateOwned)
        { mGateWake.wait(guard, [this]() { return mGateQuit || mGateOwned; }); if (mGateQuit) break; }
        const F64 now = privateNow();
        if (auto* player = mGatePlayer)
        {
            // stopPlayer clears the pointer then waits (without holding the
            // mutex) for this reader. VLC callbacks may acquire mGateMutex.
            ++mGateReaders;
            const U64 transport = mGateTransportToken;
            guard.unlock();
            if (mGateRestartParking.load() && mGateFirstFrame.load()) libvlc_media_player_set_pause(player, 1);
            const F64 raw = libvlc_media_player_get_time(player) / 1000.;
            const auto state = libvlc_media_player_get_state(player);
            const F64 duration = libvlc_media_player_get_length(player) / 1000.;
            guard.lock();
            if (player == mGatePlayer && transport == mGateTransportToken)
            {
                const bool playing = state == libvlc_Playing, paused = state == libvlc_Paused;
                const auto clock = mGateClock.sample(now, raw, playing, paused, mGateFramePeriod);
                mGatePosition = LLPluginGateClock::boundedPosition(clock.position, duration, mGateNativeLoop);
                mGateSampleError = clock.error; mGatePausedAt = clock.pausedAt;
                mGateSamplePaused = paused; mGateSamplePlaying = playing;
                mGateSampleToken = transport; mGateSample = now;
            }
            if (mGateLength > 0.)
            {
                const F64 alpha = std::clamp((now - mGateStart) / mGateLength, 0., 1.);
                mGateGain = mGateFrom + (mGateTo - mGateFrom) * alpha;
                if (alpha >= 1.) mGateLength = 0.;
            }
            const int volume = static_cast<int>(100. * mGateBase * mGateGain);
            const bool apply = mGateOwned && player == mGatePlayer;
            guard.unlock();
            if (apply) libvlc_audio_set_volume(player, volume);
            guard.lock();
            --mGateReaders; mGateWake.notify_all();
        }
        mGateWake.wait_for(guard, std::chrono::milliseconds(10), [this]() { return mGateQuit; });
    }
}

void MediaPluginLibVLC::gateReport()
{
    if (!mGateOwned || !mPrivateFile) return;
    LLPluginMessage reply(LLPLUGIN_MESSAGE_CLASS_MEDIA, "gate_time");
    reply.setValue("load_token", mLoadToken);
    {
        std::lock_guard<std::mutex> guard(mGateMutex);
        if (mLibVLCCallbackContext)
        {
            const F64 frame = mLibVLCCallbackContext->frameMilliseconds.load(std::memory_order_relaxed) / 1000.;
            mGateFramePeriod = frame > 0. ? frame : .1;
        }
        reply.setValueReal("frame_period", mGateFramePeriod);
        reply.setValue("token", std::to_string(mGateSampleToken));
        reply.setValue("envelope_token", std::to_string(mGateEnvelopeToken));
        reply.setValueReal("envelope_start", mGateStart);
        reply.setValueReal("envelope_receipt", mGateEnvelopeReceipt);
        reply.setValueBoolean("envelope_degraded", mGateEnvelopeDegraded);
        reply.setValueReal("edge_period", mGateClock.period());
        reply.setValueReal("edge_residual", mGateClock.residual());
        reply.setValueBoolean("paused", mGateSamplePaused);
        reply.setValueBoolean("playing", mGateSamplePlaying);
        reply.setValueReal("position", mGatePosition);
        reply.setValueReal("sample_time", mGateSample);
        reply.setValueReal("error", mGateSampleError);
        reply.setValueReal("pause_time", mGatePausedAt);
        reply.setValueReal("viewer_send", mGateViewerSend);
        reply.setValueReal("plugin_socket_receipt", mGateSocketReceipt);
        mGateViewerSend = 0.;
    }
    reply.setValueReal("duration", mDuration);

    reply.setValueBoolean("eof", mPrivateEnded);
    sendMessage(reply);
}

/////////////////////////////////////////////////////////////////////////////////
//
void* MediaPluginLibVLC::lock(void* data, void** p_pixels)
{
    struct mLibVLCContext* context = (mLibVLCContext*)data;

    *p_pixels = context->texture_pixels;

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::unlock(void* data, void* id, void* const* raw_pixels)
{
    // nothing to do here for the moment
    // we *could* modify pixels here to, for example, Y flip, but this is done with
    // a VLC video filter transform.
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::display(void* data, void* id)
{
    struct mLibVLCContext* context = (mLibVLCContext*)data;

    context->parent->setDirty(0, 0, context->parent->mWidth, context->parent->mHeight);
}

void MediaPluginLibVLC::displayFrame(void* data, void* id)
{
    auto* context = static_cast<mLibVLCContext*>(data);
    context->parent->mGateFirstFrame.store(true);
    if (context->wrapState.load(std::memory_order_acquire) & ALMediaFile::WrapPolicy::Admitted)
    {
        const double now = privateNow();
        const uint64_t repeat = context->repeats.load(std::memory_order_acquire);
        const double last = context->lastPresentation.load(std::memory_order_relaxed);
        if (repeat != context->presentedRepeat)
        {
            // The native marker follows decoder/vout drain and precedes seek.
            // Also measures the explicitly labelled duration-less recovery.
            if (last > 0.)
            {
                std::lock_guard<std::mutex> guard(context->seamMutex);
                const double frame = context->frameMilliseconds.load(std::memory_order_relaxed);
                context->seamGaps.push_back({(now - last) * 1000., frame > 0. ? frame : context->recentFrameMilliseconds});
            }
            context->presentedRepeat = repeat;
        }
        else if (last > 0.) context->recentFrameMilliseconds = (now - last) * 1000.;
        context->lastPresentation.store(now, std::memory_order_release);
    }
    context->hasFrame.store(true, std::memory_order_release);
    // Switch notification policy without replacing live decoder callbacks or
    // their context. Untagged/disabled players use the original sender.
    if (context->trackFrames.load(std::memory_order_acquire))
        context->frameReady.store(true, std::memory_order_release);
    else
        display(data, id);
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::initVLC()
{
    char const* vlc_argv[] =
    {
        "--no-xlib",
        "--video-filter=transform{type=vflip}",  // MAINT-6578 Y flip textures in plugin vs client
    };

#if LL_DARWIN
    setenv("VLC_PLUGIN_PATH", "./plugins", 1);
#endif

    int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);
    mLibVLC = libvlc_new(vlc_argc, vlc_argv);

    if (!mLibVLC)
    {
        // for the moment, if this fails, the plugin will fail and
        // the media sub-system will tell the viewer something went wrong.
    }
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::resetVLC()
{
    stopPlayer();
    if (mLibVLC) libvlc_release(mLibVLC);
    mLibVLC = nullptr;
}

void MediaPluginLibVLC::stopPlayer()
{
    {
        std::unique_lock<std::mutex> guard(mGateMutex);
        mGatePlayer = nullptr; mGateBase = 0.;
        mGateWake.wait(guard, [this]() { return mGateReaders == 0; });
        // The admission clamp holds a rebuild at zero. Keep the tokened Gate
        // envelope so an admitted Program reprepare restores its current gain;
        // warm/retired owners already have an explicit zero envelope.
        mGateClock.reset(); mGateSampleError = 1.;
    }
    mGateParkFirstFrame = false; mGateRestartParking = false; mGateFirstFrame = false;
    if (mPrivateFile && mLibVLC) libvlc_log_unset(mLibVLC); // quiesce repeat observer before context release
    if (mPrivateFile && mLibVLCCallbackContext && mLibVLCMedia)
    {
        auto* metadata = libvlc_media_event_manager(mLibVLCMedia);
        for (auto event : {libvlc_MediaMetaChanged, libvlc_MediaParsedChanged})
            libvlc_event_detach(metadata, event, privateEvents, mLibVLCCallbackContext.get());
    }
    if (mLibVLCMediaPlayer)
    {
        // libvlc 3 stop/release quiesces callbacks before reusing pixels/context.
        libvlc_media_player_stop(mLibVLCMediaPlayer);
        libvlc_media_player_release(mLibVLCMediaPlayer);
        mLibVLCMediaPlayer = nullptr;
    }
    mLibVLCCallbackContext.reset();
    if (mLibVLCMedia) libvlc_media_release(mLibVLCMedia);
    mLibVLCMedia = nullptr;
#if !LL_WINDOWS
    if (mPrivateFD >= 0) { close(mPrivateFD); mPrivateFD = -1; }
#endif
}

void MediaPluginLibVLC::reportVideoSize()
{
    unsigned width = 0, height = 0;
    if (!mLibVLCMediaPlayer || !mLibVLCMedia || !mLibVLCCallbackContext ||
        libvlc_video_get_size(mLibVLCMediaPlayer, 0, &width, &height) != 0 ||
        !width || !height) return;
    unsigned sarNum = 1, sarDen = 1;
    libvlc_media_track_t** tracks = nullptr;
    const unsigned count = libvlc_media_tracks_get(mLibVLCMedia, &tracks);
    const int selected = libvlc_video_get_track(mLibVLCMediaPlayer);
    for (unsigned i = 0; i < count; ++i)
    {
        const auto* track = tracks[i];
        if (track && track->i_type == libvlc_track_video &&
            track->i_id == selected && track->video &&
            track->video->i_sar_num && track->video->i_sar_den)
        {
            sarNum = track->video->i_sar_num;
            sarDen = track->video->i_sar_den;
            break;
        }
    }
    libvlc_media_tracks_release(tracks, count);
    if (width == mVideoWidth && height == mVideoHeight &&
        sarNum == mVideoSarNum && sarDen == mVideoSarDen) return;
    mVideoWidth = width;
    mVideoHeight = height;
    mVideoSarNum = sarNum;
    mVideoSarDen = sarDen;
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "source_video_size");
    message.setValue("load_token", mLibVLCCallbackContext->loadToken);
    message.setValueS32("width", width);
    message.setValueS32("height", height);
    message.setValueU32("sar_num", sarNum);
    message.setValueU32("sar_den", sarDen);
    sendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
// *virtual*
void MediaPluginLibVLC::setDirty(int left, int top, int right, int bottom)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

    message.setValueS32("left", left);
    message.setValueS32("top", top);
    message.setValueS32("right", right);
    message.setValueS32("bottom", bottom);

    message.setValueReal("current_time", mCurTime);
    message.setValueReal("duration", mDuration);
    message.setValueReal("current_rate", 1.0f);

    sendMessage(message);
}

void MediaPluginLibVLC::setFrameDirty(int left, int top, int right, int bottom)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

    if (!mLibVLCCallbackContext) return;
    message.setValue("load_token", mLibVLCCallbackContext->loadToken);
    message.setValueS32("left", left);
    message.setValueS32("top", top);
    message.setValueS32("right", right);
    message.setValueS32("bottom", bottom);

    message.setValueReal("current_time", mCurTime);
    message.setValueReal("duration", mDuration);
    message.setValueReal("current_rate", 1.0f);

    sendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
// *virtual*
void MediaPluginLibVLC::setDurationDirty()
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

    message.setValueReal("current_time", mCurTime);
    message.setValueReal("duration", mDuration);
    message.setValueReal("current_rate", 1.0f);

    sendMessage(message);
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::eventCallbacks(const libvlc_event_t* event, void* ptr)
{
    MediaPluginLibVLC* parent = (MediaPluginLibVLC*)ptr;
    if (parent == 0)
    {
        return;
    }

    switch (event->type)
    {
    case libvlc_MediaPlayerOpening:
        parent->mVlcStatus = STATUS_LOADING;
        break;

    case libvlc_MediaPlayerPlaying:
        parent->mDuration = (float)(libvlc_media_get_duration(parent->mLibVLCMedia)) / 1000.0f;
        parent->mVlcStatus = STATUS_PLAYING;
        parent->setVolumeVLC();
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerPaused:
        parent->mVlcStatus = STATUS_PAUSED;
        break;

    case libvlc_MediaPlayerStopped:
        parent->mVlcStatus = STATUS_DONE;
        break;

    case libvlc_MediaPlayerEndReached:
        parent->mVlcStatus = STATUS_DONE;
        parent->mCurTime = parent->mDuration;
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerEncounteredError:
        parent->mVlcStatus = STATUS_ERROR;
        break;

    case libvlc_MediaPlayerTimeChanged:
        parent->mCurTime = (float)libvlc_media_player_get_time(parent->mLibVLCMediaPlayer) / 1000.0f;
        if (parent->mVlcStatus == STATUS_DONE && libvlc_media_player_is_playing(parent->mLibVLCMediaPlayer))
        {
            parent->mVlcStatus = STATUS_PLAYING;
        }
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerPositionChanged:
        break;

    case libvlc_MediaPlayerLengthChanged:
        parent->mDuration = (float)libvlc_media_get_duration(parent->mLibVLCMedia) / 1000.0f;
        parent->setDurationDirty();
        break;

    case libvlc_MediaPlayerTitleChanged:
    {
        char* title = libvlc_media_get_meta(parent->mLibVLCMedia, libvlc_meta_Title);
        if (title)
        {
            parent->updateTitle(title);
        }
    }
    break;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::playMedia(bool admittedEdit, double position)
{
    if (mPrivateFile && (mDesired == ALMediaFile::Transport::STOPPED || !mPixels || mWidth <= 0 || mHeight <= 0)) return;
    if (mURL.length() == 0)
    {
        return;
    }

    // A new call to play the media is received after the initial one. Typically
    // this is due to a size change request either as the media naturally resizes
    // to the size of the prim container, or else, as a 2D window is resized by the
    // user.  Stopping the media, helps avoid a race condition where the media pixel
    // buffer size is out of sync with the declared size (width/height) for a frame
    // or two and the plugin crashes as VLC tries to decode a frame into unallocated
    // memory.
#if !LL_WINDOWS
    if (mPrivateFile && mPrivateFD >= 0 && mPrivateIdentityValid &&
        !ALMediaFile::unchangedFile(mPrivateFD, mPrivateIdentity))
    {
        mAdmission.verdict = ALMediaFile::Verdict::REJECTED;
        privateAdmissionMessage("file identity changed"); stopPlayer(); mVlcStatus = STATUS_ERROR;
        return;
    }
#endif
    stopPlayer();

    if (mPrivateFile)
    {
        if (mURL.rfind("file:///", 0) != 0 || (mPrivateDemux != "mp4" && mPrivateDemux != "mkv"))
        { mAdmission.verdict = ALMediaFile::Verdict::REJECTED; privateAdmissionMessage("private URI/demux policy refused"); return; }
        ++mPlayerToken;
        // Every replacement must prove video again before gaining audio/frames.
        mAdmission.rebuild(mPlayerToken, privateNow(), admittedEdit);
        mPrivateStarted = false;
        mPrivateEnded = false;
        mNextAdmissionCheck = 0;
    }

#if !LL_WINDOWS
    if (mPrivateFile)
    {
        try
        {
            ALMediaFile::FileIdentity identity;
            mPrivateFD = ALMediaFile::verifiedLocalFile(mURL, mPrivateDemux == "mkv" ? "mkv" : "mp4", &identity,
                mPrivateIdentityValid ? &mPrivateIdentity : nullptr);
            if (mPrivateIdentityValid && !(mPrivateIdentity == identity)) throw std::runtime_error(ALMediaFile::NOT_VIDEO);
            mPrivateIdentity = identity; mPrivateIdentityValid = true;
            // verifiedLocalFile's pread leaves offset alone. Recheck the exact
            // descriptor, then rewind before EVERY new fd-backed input.
            if (!ALMediaFile::unchangedFile(mPrivateFD, mPrivateIdentity) || lseek(mPrivateFD, 0, SEEK_SET) != 0)
                throw std::runtime_error(ALMediaFile::NOT_VIDEO);
            mLibVLCMedia = libvlc_media_new_fd(mLibVLC, mPrivateFD);
        }
        catch (...)
        { mAdmission.verdict = ALMediaFile::Verdict::REJECTED; privateAdmissionMessage("verified local-file policy refused"); stopPlayer(); return; }
    }
    else
#endif
        mLibVLCMedia = libvlc_media_new_location(mLibVLC, mURL.c_str());
    if (!mLibVLCMedia)
    {
        if (mPrivateFile) { mAdmission.verdict = ALMediaFile::Verdict::REJECTED; privateAdmissionMessage("VLC media/player creation refused"); }
        mLibVLCMediaPlayer = 0;
        setStatus(STATUS_ERROR);
        return;
    }

    if (mPrivateFile)
    {
        // External references were conservatively rejected on the exact pinned
        // file by the viewer. Pin demux before player creation; no fallback.
        libvlc_media_add_option(mLibVLCMedia, (":demux=" + mPrivateDemux).c_str());
        libvlc_media_add_option(mLibVLCMedia, ":no-sub-autodetect-file");
        libvlc_media_add_option(mLibVLCMedia, ":no-mkv-preload-local-dir");
        libvlc_media_add_option(mLibVLCMedia, ":file-caching=50"); // private local/cache bytes only
        // Repeat eligibility is fixed before input_Create. Admission, rather
        // than this option, gates ALL publication and audio; its 10s deadline
        // still terminates a repeating input that never proves a video track.
        libvlc_media_add_option(mLibVLCMedia, ALMediaFile::inputRepeatOption(mIsLooping));
        mInputLoop = mIsLooping;
        mResume.open(position, position > 0.);
    }
    mLibVLCMediaPlayer = libvlc_media_player_new_from_media(mLibVLCMedia);
    {
        std::lock_guard<std::mutex> guard(mGateMutex);
        mGatePlayer = mLibVLCMediaPlayer; mGateNativeLoop = mInputLoop;
        if (mLibVLCCallbackContext)
        {
            const F64 frame = mLibVLCCallbackContext->frameMilliseconds.load(std::memory_order_relaxed) / 1000.;
            mGateFramePeriod = frame > 0. ? frame : .1;
        }
    }
    if (!mLibVLCMediaPlayer)
    {
        if (mPrivateFile) { mAdmission.verdict = ALMediaFile::Verdict::REJECTED; privateAdmissionMessage("VLC media/player creation refused"); }
        setStatus(STATUS_ERROR);
        return;
    }

    if (mPrivateFile)
    {
        privateAdmissionMessage(); // reports the replacement player's verdict
        mLibVLCCallbackContext = std::make_unique<mLibVLCContext>(
            mPixels, mLibVLCMediaPlayer, mLoadToken, mWidth, mHeight);
        mLibVLCCallbackContext->parent = this;
        if (mRecoveryPresentation > 0.)
        {
            mLibVLCCallbackContext->lastPresentation.store(mRecoveryPresentation, std::memory_order_relaxed);
            mLibVLCCallbackContext->repeats.store(1, std::memory_order_relaxed);
            mRecoveryPresentation = 0.;
        }
        libvlc_log_set(mLibVLC, repeatLog, mLibVLCCallbackContext.get());
    }
    // listen to events
    libvlc_event_manager_t* em = libvlc_media_player_event_manager(mLibVLCMediaPlayer);
    if (em)
    {
        if (mPrivateFile)
        {
            for (auto event : {libvlc_MediaPlayerPlaying, libvlc_MediaPlayerESAdded,
                libvlc_MediaPlayerLengthChanged, libvlc_MediaPlayerEndReached, libvlc_MediaPlayerEncounteredError})
                libvlc_event_attach(em, event, privateEvents, mLibVLCCallbackContext.get());
            auto* metadata = libvlc_media_event_manager(mLibVLCMedia);
            for (auto event : {libvlc_MediaMetaChanged, libvlc_MediaParsedChanged})
                libvlc_event_attach(metadata, event, privateEvents, mLibVLCCallbackContext.get());
        }
        else
        {
        libvlc_event_attach(em, libvlc_MediaPlayerOpening, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPlaying, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPaused, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerStopped, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerEndReached, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, eventCallbacks, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, eventCallbacks, this);
        }
    }

    if (!mPrivateFile)
    {
        mLibVLCCallbackContext = std::make_unique<mLibVLCContext>(
            mPixels, mLibVLCMediaPlayer, mLoadToken, mWidth, mHeight);
        mLibVLCCallbackContext->parent = this;
    }
    libvlc_video_set_callbacks(mLibVLCMediaPlayer, lock, unlock, displayFrame, mLibVLCCallbackContext.get());
    libvlc_video_set_format(mLibVLCMediaPlayer, "RV32", mWidth, mHeight, mWidth * mDepth);

    // Send a "navigate begin" event.
    // This is really a browser message but the QuickTime plugin did it and
    // the media system relies on this message to update internal state so we must send it too
    // Note: see "navigate_complete" message below too
    // https://jira.secondlife.com/browse/MAINT-6528
    LLPluginMessage message_begin(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "navigate_begin");
    message_begin.setValue("uri", mURL);
    message_begin.setValueBoolean("history_back_available", false);
    message_begin.setValueBoolean("history_forward_available", false);
    sendMessage(message_begin);

    // volume level gets set before VLC is initialized (thanks media system) so we have to record
    // it in mCurVolume and set it again here so that volume levels are correctly initialized
    setVolume(mCurVolume);

    setStatus(STATUS_LOADED);

    // note this relies on the "set_loop" message arriving before the "start" (play) one
    // but that appears to always be the case
    if (mIsLooping && !mPrivateFile)
    {
        libvlc_media_add_option(mLibVLCMedia, "input-repeat=65535");
    }

    if (!mPrivateFile || mDesired == ALMediaFile::Transport::PLAYING)
    {
        if (mPrivateFile) { mPrivateStarted = true; mAdmission.resumed(privateNow()); }
        libvlc_media_player_play(mLibVLCMediaPlayer);
    }

    // send a "location_changed" message - this informs the media system
    // that a new URL is the 'current' one and is used extensively.
    // Again, this is really a browser message but we will use it here.
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "location_changed");
    message.setValue("uri", mURL);
    sendMessage(message);

    // Send a "navigate complete" event.
    // This is really a browser message but the QuickTime plugin did it and
    // the media system relies on this message to update internal state so we must send it too
    // Note: see "navigate_begin" message above too
    // https://jira.secondlife.com/browse/MAINT-6528
    LLPluginMessage message_complete(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER, "navigate_complete");
    message_complete.setValue("uri", mURL);
    message_complete.setValueS32("result_code", 200);
    message_complete.setValue("result_string", "OK");
    sendMessage(message_complete);
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::updateTitle(const char* title)
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    message.setValue("name", title);
    sendMessage(message);
}

void MediaPluginLibVLC::setVolumeVLC()
{
    if (mLibVLCMediaPlayer)
    {
        double effective = mPrivateFile && !mAdmission.audible(mDesired) ? 0. : mCurVolume;
        {
            std::lock_guard<std::mutex> guard(mGateMutex);
            mGateBase = effective; // Admission, desired transport and global policy remain ABOVE the envelope.
            if (mGateOwned) effective *= mGateGain;
        }
        int vlc_vol = (int)(effective * 100);

        int result = libvlc_audio_set_volume(mLibVLCMediaPlayer, vlc_vol);
        if (result == 0)
        {
            // volume change was accepted by LibVLC
        }
        else
        {
            // volume change was NOT accepted by LibVLC and not actioned
        }

#if LL_WINDOWS
        // https ://jira.secondlife.com/browse/MAINT-8119
        // CEF media plugin uses code in media_plugins/cef/windows_volume_catcher.cpp to
        // set the actual output volume of the plugin process since there is no API in
        // CEF to otherwise do this.
        // There are explicit calls to change the volume in LibVLC but sometimes they
        // are ignored SLPlugin.exe process volume is set to 0 so you never heard audio
        // from the VLC media stream.
        // The right way to solve this is to move the volume catcher stuff out of
        // the CEF plugin and into it's own folder under media_plugins and have it referenced
        // by both CEF and VLC. That's for later. The code there boils down to this so for
                // now, as we approach a release, the less risky option is to do it directly vs
                // calls to volume catcher code.
        DWORD left_channel = (DWORD)(effective * 65535.0f);
        DWORD right_channel = (DWORD)(effective * 65535.0f);
        DWORD hw_volume = left_channel << 16 | right_channel;
        waveOutSetVolume(NULL, hw_volume);
#endif
    }
    else
    {
        // volume change was requested but VLC wasn't ready.
        // that's okay though because we saved the value in mCurVolume and
        // the next volume change after the VLC system is initilzied  will set it
    }
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::setVolume(const F64 volume)
{
    mCurVolume = volume;

    setVolumeVLC();
}

////////////////////////////////////////////////////////////////////////////////
//
void MediaPluginLibVLC::receiveMessage(const std::string& message_string)
{
    LLPluginMessage message_in;

    if (message_in.parse(message_string) >= 0)
    {
        std::string message_class = message_in.getClass();
        std::string message_name = message_in.getName();
        if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
        {
            if (message_name == "init")
            {
                initVLC();

                LLPluginMessage message("base", "init_response");
                LLSD versions = LLSD::emptyMap();
                versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
                versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] = LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
                versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME] = LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME_VERSION;
                message.setValueLLSD("versions", versions);

                std::ostringstream s;
                s << "LibVLC plugin ";
                s << LIBVLC_VERSION_MAJOR;
                s << ".";
                s << LIBVLC_VERSION_MINOR;
                s << ".";
                s << LIBVLC_VERSION_REVISION;

                message.setValue("plugin_version", s.str());
                sendMessage(message);
            }
            else if (message_name == "idle")
            {
                if (mPrivateFile) tickPrivateFile();
                gateReport();
                setStatus(mVlcStatus);
                if (!mLoadToken.empty())
                {
                    reportVideoSize();
                    if ((!mPrivateFile || mAdmission.verdict == ALMediaFile::Verdict::ADMITTED) && mLibVLCCallbackContext && mLibVLCCallbackContext->frameReady.exchange(false, std::memory_order_acquire))
                        setFrameDirty(0, 0, mLibVLCCallbackContext->width, mLibVLCCallbackContext->height);
                }
            }
            else if (message_name == "cleanup")
            {
                resetVLC();
            }
            else if (message_name == "force_exit")
            {
                mDeleteMe = true;
            }
            else if (message_name == "shm_added")
            {
                SharedSegmentInfo info;
                info.mAddress = message_in.getValuePointer("address");
                info.mSize = (size_t)message_in.getValueS32("size");
                std::string name = message_in.getValue("name");

                mSharedSegments.insert(SharedSegmentMap::value_type(name, info));

            }
            else if (message_name == "shm_remove")
            {
                std::string name = message_in.getValue("name");

                SharedSegmentMap::iterator iter = mSharedSegments.find(name);
                if (iter != mSharedSegments.end())
                {
                    if (mPixels == iter->second.mAddress)
                    {
                        stopPlayer();

                        mPixels = NULL;
                        mTextureSegmentName.clear();
                    }
                    mSharedSegments.erase(iter);
                }
                else
                {
                    //std::cerr << "MediaPluginWebKit::receiveMessage: unknown shared memory region!" << std::endl;
                }

                // Send the response so it can be cleaned up.
                LLPluginMessage message("base", "shm_remove_response");
                message.setValue("name", name);
                sendMessage(message);
            }
            else
            {
                //std::cerr << "MediaPluginWebKit::receiveMessage: unknown base message: " << message_name << std::endl;
            }
        }
        else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA)
        {
            if (message_name == "init")
            {
                LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "texture_params");
                message.setValueS32("default_width", 1024);
                message.setValueS32("default_height", 1024);
                message.setValueS32("depth", mDepth);
                message.setValueU32("internalformat", GL_RGB);
                message.setValueU32("format", GL_BGRA);
                message.setValueU32("type", GL_UNSIGNED_BYTE);
                message.setValueBoolean("coords_opengl", true);
                sendMessage(message);
            }
            else if (message_name == "size_change")
            {
                std::string name = message_in.getValue("name");
                S32 width = message_in.getValueS32("width");
                S32 height = message_in.getValueS32("height");
                S32 texture_width = message_in.getValueS32("texture_width");
                S32 texture_height = message_in.getValueS32("texture_height");

                if (!name.empty())
                {
                    // Find the shared memory region with this name
                    SharedSegmentMap::iterator iter = mSharedSegments.find(name);
                    if (iter != mSharedSegments.end())
                    {
                        mPixels = (unsigned char*)iter->second.mAddress;
                        mWidth = width;
                        mHeight = height;
                        mTextureWidth = texture_width;
                        mTextureHeight = texture_height;

                        libvlc_time_t time = (libvlc_time_t)(1000.0 * mCurTime);

                        playMedia();

                        if (mLibVLCMediaPlayer)
                        {
                            libvlc_media_player_set_time(mLibVLCMediaPlayer, time);
                            time = libvlc_media_player_get_time(mLibVLCMediaPlayer);
                            if (time < 0)
                            {
                                // -1 if there is no media
                                mCurTime = 0;
                            }
                            else
                            {
                                mCurTime = (F64)time / 1000.0;
                            }
                        }
                    };
                };

                LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "size_change_response");
                message.setValue("name", name);
                message.setValueS32("width", width);
                message.setValueS32("height", height);
                message.setValueS32("texture_width", texture_width);
                message.setValueS32("texture_height", texture_height);
                sendMessage(message);
            }
            else if (message_name == "enable_media_frames")
            {
                if (mPrivateFile) return; // private loads never fall back to untagged frames
                mLoadToken = message_in.getValue("load_token");
                mVideoWidth = mVideoHeight = 0;
                mVideoSarNum = mVideoSarDen = 1;
                if (mLibVLCCallbackContext)
                {
                    auto& context = *mLibVLCCallbackContext;
                    context.trackFrames.store(false, std::memory_order_release);
                    context.frameReady.exchange(false, std::memory_order_acq_rel);
                    context.loadToken = mLoadToken;
                    context.trackFrames.store(!mLoadToken.empty(), std::memory_order_release);
                    // Renewing tracking must work for paused/static players.
                    // Only this current player's completed pixels may be tagged.
                    if (context.hasFrame.load(std::memory_order_acquire))
                    {
                        if (!mLoadToken.empty())
                        {
                            reportVideoSize();
                            setFrameDirty(0, 0, context.width, context.height);
                        }
                        else
                            setDirty(0, 0, context.width, context.height);
                    }
                }
            }
            else if (message_name == "gate_ping")
            {
                if (mPrivateFile && message_in.getValue("load_token") == mLoadToken)
                {
                    std::lock_guard<std::mutex> guard(mGateMutex);
                    mGateViewerSend = message_in.getValueReal("viewer_send");
                    mGateSocketReceipt = message_in.getValueReal("plugin_socket_receipt");
                }
            }
            else if (message_name == "gate_envelope")
            {
                if (!mPrivateFile || message_in.getValue("load_token") != mLoadToken) return;
                const U64 token = std::strtoull(message_in.getValue("token").c_str(), nullptr, 10);
                {
                std::lock_guard<std::mutex> guard(mGateMutex);
                if (token <= mGateEnvelopeToken) return;
                mGateEnvelopeToken = token; mGateOwned = true; mGateWake.notify_all();
                mGateFrom = mGateGain; mGateTo = std::clamp(message_in.getValueReal("gain"), 0., 1.);
                mGateLength = std::clamp(message_in.getValueReal("seconds"), 0., 3.);
                mGateStart = privateNow(); // A delayed handler still runs the full receipt fallback ramp.
                const F64 mapped_start = message_in.getValueReal("start_plugin");
                mGateEnvelopeReceipt = privateNow();
                mGateEnvelopeDegraded = !(mapped_start > 0. && std::abs(mGateEnvelopeReceipt - mapped_start) <= .02);
                if (!mGateEnvelopeDegraded) mGateStart = mapped_start;
                if (mGateLength == 0.)
                {
                    mGateGain = mGateTo;

                }
                }
                setVolumeVLC(); // Never invoke VLC under the Gate mutex.
            }
            else if (message_name == "gate_transport")
            {
                if (!mPrivateFile || message_in.getValue("load_token") != mLoadToken) return;
                const U64 token = std::strtoull(message_in.getValue("token").c_str(), nullptr, 10);
                {
                    std::lock_guard<std::mutex> guard(mGateMutex);
                    if (token <= mGateTransportToken) return;
                    mGateTransportToken = token; mGateOwned = true;
                    if (message_in.getValueS32("desired") < 0)
                    { mGateOwned = false; mGateGain = mGateFrom = mGateTo = 1.; mGateLength = 0.; }
                    mGateWake.notify_all();
                    mGateViewerSend = message_in.getValueReal("viewer_send");
                    mGateSocketReceipt = message_in.getValueReal("plugin_socket_receipt");
                    const F64 seek = message_in.getValueReal("seek");
                    if (seek >= 0.) { mGateClock.seek(seek); mGateSampleError = 1.; }
                }
                if (message_in.getValueS32("desired") < 0)
                {
                    mGateParkFirstFrame = mGateRestartParking = false;
                    setVolumeVLC(); // envelope release must restore Volume even if its value did not change
                    return;
                }
                mDesired = static_cast<ALMediaFile::Transport>(message_in.getValueS32("desired"));
                // RESUME wins even before admission starts tickPrivateFile.
                if (mDesired == ALMediaFile::Transport::PLAYING)
                { mGateParkFirstFrame = false; mGateRestartParking = false; }
                if (mPrivateEnded && mDesired == ALMediaFile::Transport::PLAYING)
                    playMedia(true, std::max(0., message_in.getValueReal("seek")));
                if (mLibVLCMediaPlayer)
                {
                    const F64 target = message_in.getValueReal("seek");
                    ALMediaFile::applyGateSeek(target, mAdmission.seekable,
                        [this](F64 seconds) { libvlc_media_player_set_time(mLibVLCMediaPlayer, static_cast<libvlc_time_t>(seconds * 1000.)); },
                        [this]()
                        {
                            mDesired = ALMediaFile::Transport::PLAYING;
                            playMedia(true, 0.); // Same pinned file, repeat and admission latch.
                            mGateParkFirstFrame = true; mGateRestartParking = true;
                            std::lock_guard<std::mutex> guard(mGateMutex); mGateClock.seek(0.);
                        });
                    if (mLibVLCMediaPlayer) libvlc_media_player_set_pause(mLibVLCMediaPlayer, mDesired != ALMediaFile::Transport::PLAYING);
                }
                setVolumeVLC(); gateReport();
            }
            else if (message_name == "private_transport")
            {
                mDesired = static_cast<ALMediaFile::Transport>(message_in.getValueS32("desired"));
                setLoop(message_in.getValueBoolean("loop"));
                if (mPrivateFile)
                {
                    if (mDesired == ALMediaFile::Transport::STOPPED)
                    {
                        setVolumeVLC(); stopPlayer();
                        LLPluginMessage closed(LLPLUGIN_MESSAGE_CLASS_MEDIA, "private_file_closed");
                        closed.setValue("load_token", mLoadToken);
                        sendMessage(closed);
                    }
                    else if (mDesired == ALMediaFile::Transport::PAUSED && mLibVLCMediaPlayer)
                    {
                        if (mAdmission.verdict == ALMediaFile::Verdict::PENDING) mPrivateStarted = false;
                        libvlc_media_player_set_pause(mLibVLCMediaPlayer, 1);
                    }
                    else if (mDesired == ALMediaFile::Transport::PLAYING)
                    {
                        if (!mLibVLCMediaPlayer || mPrivateEnded) playMedia();
                        else
                        {
                            if (!mPrivateStarted)
                            {
                                mPrivateStarted = true;
                                mAdmission.resumed(privateNow());
                                mResume.resumed();
                            }
                            libvlc_media_player_set_pause(mLibVLCMediaPlayer, 0);
                            libvlc_media_player_play(mLibVLCMediaPlayer);
                        }
                    }
                    setVolumeVLC();
                }
            }
            else if (message_name == "load_uri")
            {
                stopPlayer();
#if !LL_WINDOWS
                mPrivateIdentityValid = false;
#endif
                mPrivateFile = message_in.getValueBoolean("private_file");
                if (mPrivateFile)
                {
                    mAdmission = ALMediaFile::Admission(); // fresh URI never inherits admission
                    mRecoveryPresentation = 0.;
                    mPrivateDemux = message_in.getValue("demux");
                    mDesired = static_cast<ALMediaFile::Transport>(message_in.getValueS32("desired"));
                    mIsLooping = message_in.getValueBoolean("private_loop");
                }
                mLoadToken = message_in.getValue("load_token");
                mVideoWidth = mVideoHeight = 0;
                mVideoSarNum = mVideoSarDen = 1;
                mURL = message_in.getValue("uri");
                playMedia();
            }
        }
        else
            if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME)
            {
                if (mPrivateFile && message_name != "set_volume" && message_name != "set_loop" && message_name != "seek") return;
                if (message_name == "stop")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_media_player_stop(mLibVLCMediaPlayer);
                    }
                }
                else if (message_name == "start")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        if (mVlcStatus == STATUS_DONE && !libvlc_media_player_is_playing(mLibVLCMediaPlayer))
                        {
                            // stop or vlc will ignore 'play', it will just
                            // make an MediaPlayerEndReached event even if
                            // seek was used
                            libvlc_media_player_stop(mLibVLCMediaPlayer);
                        }
                        libvlc_media_player_play(mLibVLCMediaPlayer);
                    }
                }
                else if (message_name == "pause")
                {
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_media_player_set_pause(mLibVLCMediaPlayer, 1);
                    }
                }
                else if (message_name == "seek")
                {
                    if (mPrivateFile && (mAdmission.verdict != ALMediaFile::Verdict::ADMITTED || !mAdmission.seekable)) return;
                    if (mLibVLCMediaPlayer)
                    {
                        libvlc_time_t time = (libvlc_time_t)(1000.0 * message_in.getValueReal("time"));
                        libvlc_media_player_set_time(mLibVLCMediaPlayer, time);
                        time = libvlc_media_player_get_time(mLibVLCMediaPlayer);
                        if (time < 0)
                        {
                            // -1 if there is no media
                            mCurTime = 0;
                        }
                        else
                        {
                            mCurTime = (F64)time / 1000.0;
                        }

                        if (!libvlc_media_player_is_playing(mLibVLCMediaPlayer))
                        {
                            // if paused, won't trigger update, update now
                            setDurationDirty();
                        }
                    }
                }
                else if (message_name == "set_loop")
                {
                    bool loop = message_in.getValueBoolean("loop");
                    setLoop(loop);
                }
                else if (message_name == "set_volume")
                {
                    // volume comes in 0 -> 1.0
                    F64 volume = message_in.getValueReal("volume");
                    setVolume(volume);
                }
            }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
bool MediaPluginLibVLC::init()
{
    LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    message.setValue("name", "LibVLC Plugin");
    sendMessage(message);

    return true;
};

////////////////////////////////////////////////////////////////////////////////
//
int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
    void* host_user_data,
    LLPluginInstance::sendMessageFunction *plugin_send_func,
    void **plugin_user_data)
{
    MediaPluginLibVLC* self = new MediaPluginLibVLC(host_send_func, host_user_data);
    *plugin_send_func = MediaPluginLibVLC::staticReceiveMessage;
    *plugin_user_data = (void*)self;

    return 0;
}
