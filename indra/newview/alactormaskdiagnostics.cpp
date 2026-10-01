/**
 * @file alactormaskdiagnostics.cpp
 * @brief [ActorMask] WP7 instrumentation: summary line, query verdicts,
 *        debug view and the isolation/parity test instrument.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Design: doc/CINE_RIG_ACTOR_MASK_BRIEF_V3.md sec. 9 (EMPTY removed: a zero tight
 * count is VISIBILITY INCONCLUSIVE with the loose count as a diagnostic; a
 * silently broken mask is found with the Debug=1 overlay plus NO-GEOMETRY).
 *
 * Verdicts are computed only from the tag captured when each query was issued
 * (LLActorMaskQueryTag), never from current-frame state. Results are drained
 * from LLActorMaskQueries::takeResults(), which never blocks on the GPU.
 */

#include "llviewerprecompiledheaders.h"

#include "alactormaskdiagnostics.h"

#include "alcinehaze.h"
#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "alcinelightrigmodel.h"
#include "llactormaskeligibility.h"
#include "llcallbacklist.h"
#include "llframetimer.h"
#include "llghostdeferreddiagnostics.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llshadermgr.h"
#include "llvertexbuffer.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

LLGLSLShader gActorMaskDebugProgram; // [ActorMask] WP7

namespace
{
// ---------------------------------------------------------------------------
// [ActorMask] Tunables. Cadences are wall-clock; holds also need frames.
// ---------------------------------------------------------------------------
constexpr F64 SUMMARY_PERIOD_S = 5.0;          // summary line cadence
constexpr F64 VERDICT_MIN_INTERVAL_S = 5.0;    // per recipient, between verdict lines
constexpr U32 INCONCLUSIVE_HINT_RUN = 10;      // consecutive INCONCLUSIVE summaries
constexpr U32 GROUP_SETTLE_FRAMES = 8;         // wait this long for a raster result
constexpr U32 GROUP_DROP_FRAMES = 16;          // then drop the incomplete group
constexpr U32 REACTIVATE_GAP_FRAMES = 30;      // main-view gap that means "was off"
constexpr U32 TEST_HOLD_FRAMES = 90;           // brief sec. 9.3: >= 90 frames ...
constexpr F64 TEST_HOLD_SECONDS = 1.5;         // ... and >= 1.5 s per mode
constexpr F64 TEST_STEP_TIMEOUT_S = 30.0;      // a step that never captures
constexpr U32 TEST_STEP_COUNT = 5;             // F0 F1 F2 F3 F0'
constexpr U32 READ_STRIP_ROWS = 32;            // readback strip height
constexpr S32 FORWARD_DILATE_PX = 8;           // ISOLATION-FORWARD bounds dilation
constexpr S32 PARITY_ERODE_PX = 2;             // PARITY mask erosion

F64 now_seconds()
{
    return LLFrameTimer::getElapsedSeconds().value();
}

bool actor_mask_view_ready(const LLPipeline& pipe)
{
    return pipe.mActorMaskView == LLPipeline::ActorMaskView::MASK && pipe.mActorMaskValid;
}

std::string bit_string(U8 bit)
{
    std::ostringstream out;
    out << "0x" << std::hex << static_cast<U32>(bit);
    return out.str();
}

// ---------------------------------------------------------------------------
// [ActorMask] Query verdicts (sec. 9.1, EMPTY removed).
// ---------------------------------------------------------------------------
enum class EVerdict : U8
{
    NONE,
    OK,
    INCONCLUSIVE,   // "VISIBILITY INCONCLUSIVE"
    NO_GEOMETRY,
    OFFSCREEN,
    IMPOSTOR,       // logged on change by the WP1 mask pass; tracked silently here
    STYLED          // logged on change by the WP1 mask pass; tracked silently here
};

const char* verdict_name(EVerdict v)
{
    switch (v)
    {
    case EVerdict::OK:           return "OK";
    case EVerdict::INCONCLUSIVE: return "VISIBILITY INCONCLUSIVE";
    case EVerdict::NO_GEOMETRY:  return "NO-GEOMETRY";
    case EVerdict::OFFSCREEN:    return "OFFSCREEN";
    case EVerdict::IMPOSTOR:     return "IMPOSTOR";
    case EVerdict::STYLED:       return "STYLED";
    case EVerdict::NONE:
    default:                     return "NONE";
    }
}

// One (frame, view) bracket of one recipient: up to TIGHT + LOOSE + RASTER.
struct QueryGroup
{
    bool mHasTight = false;
    bool mHasLoose = false;
    bool mHasRaster = false;
    U32 mTight = 0;
    U32 mLoose = 0;
    U32 mRaster = 0;
    LLActorMaskQueryTag mTag; // the TIGHT tag when present (draws/frustum/state)
};

struct RecipientVerdictState
{
    std::map<std::pair<U32, U64>, QueryGroup> mGroups; // ordered oldest first
    U8 mBit = 0;
    EVerdict mCurrent = EVerdict::NONE;
    U32 mStreak = 0; // consecutive evaluations with mCurrent (anti-flap)
    std::string mDetail;
    EVerdict mPrinted = EVerdict::NONE;
    F64 mPrintedAt = -1.0;
    U32 mInconclusiveRun = 0;
    bool mHintPrinted = false;
};

// Decides a verdict from one group's captured tag. Returns false while a zero
// tight count still waits for its raster probe (or never gets one).
bool classify_group(const QueryGroup& g, EVerdict& verdict, std::string& detail)
{
    const LLActorMaskQueryTag& t = g.mTag;
    std::ostringstream out;
    if (t.impostor)
    {
        verdict = EVerdict::IMPOSTOR;
    }
    else if (t.styled && t.drawsIssued == 0)
    {
        verdict = EVerdict::STYLED;
    }
    else if (!t.inFrustum)
    {
        verdict = EVerdict::OFFSCREEN;
        out << "in_frustum=0";
    }
    else if (t.drawsIssued == 0)
    {
        // [ActorMask] Review #7: an AABB in the frustum with no replayed draw is
        // also what native occlusion culling produces, so draws == 0 alone is not
        // a harvest failure. Only a raster hit may claim NO-GEOMETRY; raster == 0
        // is OFFSCREEN (culled), and no raster probe yet means no verdict.
        if (!g.mHasRaster) return false;
        if (g.mRaster == 0)
        {
            // Final review: tight and raster replay the same harvest, so an empty
            // harvest and native culling are indistinguishable here. Undecidable.
            verdict = EVerdict::INCONCLUSIVE;
            out << "in_frustum=1 draws=0 raster=0 skipped_like_native=" << t.batchesSkippedLikeNative
                << " (natively culled, or harvest failure: the Debug=1 overlay decides)";
        }
        else
        {
            verdict = EVerdict::NO_GEOMETRY;
            out << "in_frustum=1 draws=0 raster=" << g.mRaster
                << " skipped_like_native=" << t.batchesSkippedLikeNative;
        }
    }
    else if (g.mTight > 0)
    {
        verdict = EVerdict::OK;
        out << "tight=" << g.mTight << " draws=" << t.drawsIssued;
    }
    else if (g.mHasRaster)
    {
        if (g.mRaster == 0)
        {
            verdict = EVerdict::OFFSCREEN;
            out << "in_frustum=1 raster=0 draws=" << t.drawsIssued;
        }
        else
        {
            // Occluded subject or broken mask: undecidable from queries (Codex R2 #7).
            verdict = EVerdict::INCONCLUSIVE;
            out << "tight=0 loose=";
            if (g.mHasLoose) out << g.mLoose; else out << "n/a";
            out << " raster=" << g.mRaster << " draws=" << t.drawsIssued;
        }
    }
    else
    {
        return false;
    }
    detail = out.str();
    return true;
}

// ---------------------------------------------------------------------------
// [ActorMask] Readback helpers for the test instrument.
// ---------------------------------------------------------------------------
class ScopedReadState
{
public:
    ScopedReadState()
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFBO);
        glGetIntegerv(GL_READ_BUFFER, &mReadBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &mPackAlignment);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &mPackBuffer);
    }
    ~ScopedReadState()
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(mPackBuffer));
        glPixelStorei(GL_PACK_ALIGNMENT, mPackAlignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(mReadFBO));
        glReadBuffer(static_cast<GLenum>(mReadBuffer));
    }
private:
    GLint mReadFBO = 0;
    GLint mReadBuffer = GL_NONE;
    GLint mPackAlignment = 4;
    GLint mPackBuffer = 0;
};

// [ActorMask] Review #3: exact vertex-input state for the debug draw. setBuffer()
// rebinds GL_ARRAY_BUFFER / GL_ELEMENT_ARRAY_BUFFER and rewrites attribute
// pointers, and the shader bind changes enabled arrays; restore every attribute
// slot LLVertexBuffer uses, both buffer bindings and LLVertexBuffer's caches.
class ScopedVertexInputState
{
public:
    ScopedVertexInputState()
    {
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &mArrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &mElementBuffer);
        mCacheBuffer = LLVertexBuffer::sGLRenderBuffer;
        mCacheIndices = LLVertexBuffer::sGLRenderIndices;
        mCacheMask = LLVertexBuffer::sLastMask;
        GLint max_attribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
        mCount = static_cast<U32>(llclamp(max_attribs, 0, static_cast<GLint>(LLVertexBuffer::TYPE_MAX)));
        for (U32 i = 0; i < mCount; ++i)
        {
            Attrib& a = mAttribs[i];
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &a.mEnabled);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &a.mBuffer);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &a.mSize);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &a.mType);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &a.mNormalized);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &a.mInteger);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &a.mStride);
            glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &a.mPointer);
        }
    }

    // Call after the previous program is rebound (its bind re-runs
    // setupClientArrays, which this then overrides with the captured truth).
    void restore() const
    {
        for (U32 i = 0; i < mCount; ++i)
        {
            const Attrib& a = mAttribs[i];
            if (a.mBuffer)
            {
                // Core profile: a pointer is only meaningful with a buffer bound.
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(a.mBuffer));
                if (a.mInteger)
                    glVertexAttribIPointer(i, a.mSize, static_cast<GLenum>(a.mType), a.mStride, a.mPointer);
                else
                    glVertexAttribPointer(i, a.mSize, static_cast<GLenum>(a.mType),
                                          a.mNormalized ? GL_TRUE : GL_FALSE, a.mStride, a.mPointer);
            }
            if (a.mEnabled) glEnableVertexAttribArray(i);
            else glDisableVertexAttribArray(i);
        }
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(mArrayBuffer));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(mElementBuffer));
        LLVertexBuffer::sGLRenderBuffer = mCacheBuffer;
        LLVertexBuffer::sGLRenderIndices = mCacheIndices;
        LLVertexBuffer::sLastMask = mCacheMask;
    }

private:
    struct Attrib
    {
        GLint mEnabled = 0, mBuffer = 0, mSize = 4, mType = GL_FLOAT;
        GLint mNormalized = 0, mInteger = 0, mStride = 0;
        void* mPointer = nullptr;
    };
    std::array<Attrib, LLVertexBuffer::TYPE_MAX> mAttribs{};
    U32 mCount = 0;
    GLint mArrayBuffer = 0;
    GLint mElementBuffer = 0;
    U32 mCacheBuffer = 0;
    U32 mCacheIndices = 0;
    U32 mCacheMask = 0;
};

// Reads colour attachment 0 bottom-up in strips: fn(y0, rows, bytes).
template <typename Fn>
bool read_attachment0(const LLRenderTarget& target, GLenum format, GLenum type,
                      U32 bytes_per_pixel, Fn&& fn, std::string& error)
{
    const U32 width = target.getWidth();
    const U32 height = target.getHeight();
    if (!target.getFBO() || !width || !height)
    {
        error = "target not allocated";
        return false;
    }
    ScopedReadState saved;
    for (U32 i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i)
    {
        // Drop stale errors so the check below reports only this readback.
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFBO());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<U8> strip(static_cast<size_t>(width) * READ_STRIP_ROWS * bytes_per_pixel);
    for (U32 y0 = 0; y0 < height; y0 += READ_STRIP_ROWS)
    {
        const U32 rows = std::min(READ_STRIP_ROWS, height - y0);
        glReadPixels(0, static_cast<GLint>(y0), static_cast<GLsizei>(width),
                     static_cast<GLsizei>(rows), format, type, strip.data());
        if (glGetError() != GL_NO_ERROR)
        {
            error = "glReadPixels failed";
            return false;
        }
        fn(y0, rows, strip.data());
    }
    return true;
}

U64 splitmix64(U64 x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// Exact RGBA float texel (read from RGBA16F, so lossless) -> 32-bit digest.
// "Bit-identical" below means equal digests; a false match needs a 2^-32
// collision on a pixel that really changed.
U32 pixel_digest(const U8* texel)
{
    U64 a = 0;
    U64 b = 0;
    std::memcpy(&a, texel, sizeof(a));
    std::memcpy(&b, texel + sizeof(a), sizeof(b));
    const U64 h = splitmix64(a ^ splitmix64(b));
    return static_cast<U32>(h ^ (h >> 32));
}

// Per-pixel facts accumulated across the five captures.
enum : U8
{
    PX_MASK_NZ       = 1 << 0, // F1 mask byte != 0
    PX_PARITY        = 1 << 1, // F1 eroded mask pixel lit by every actor-only light
    PX_OUTSIDE       = 1 << 2, // outside every recipient bound dilated by 8 px
    PX_DEF_CHANGED   = 1 << 3, // C_deferred F1 != F0
    PX_POST_CHANGED  = 1 << 4, // C_post F2 != F1
    PX_PARITY_FAIL   = 1 << 5, // C_deferred F1 vs F3 outside tolerance
    PX_STABLE_DEF    = 1 << 6, // C_deferred F0' == F0
    PX_STABLE_POST   = 1 << 7  // C_post F0' == F0
};

struct ScreenRect
{
    S32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; // half-open
};

const char* test_mode_name(U32 step)
{
    static const char* const names[TEST_STEP_COUNT] = { "F0", "F1", "F2", "F3", "F0'" };
    return step < TEST_STEP_COUNT ? names[step] : "?";
}

LLPipeline::ActorMaskTestMode test_mode_for(U32 step)
{
    switch (step)
    {
    case 1: return LLPipeline::ActorMaskTestMode::F1;
    case 2: return LLPipeline::ActorMaskTestMode::F2;
    case 3: return LLPipeline::ActorMaskTestMode::F3;
    case 0:
    case 4:
    default: return LLPipeline::ActorMaskTestMode::F0;
    }
}

void test_line(const char* name, const char* verdict, const std::string& reason)
{
    LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK TEST " << name << ' ' << verdict
        << ' ' << reason << LL_ENDL;
}

void test_all_inconclusive(const std::string& reason)
{
    test_line("ISOLATION-DEFERRED", "INCONCLUSIVE", reason);
    test_line("ISOLATION-FORWARD", "INCONCLUSIVE", reason);
    test_line("PARITY", "INCONCLUSIVE", reason);
}

void reset_test_setting_on_idle()
{
    // Never write the control from inside its own commit signal.
    doOnIdleOneTime([]()
    {
        if (gSavedSettings.getU32("CineLightRigActorMaskTest") != 0)
            gSavedSettings.setU32("CineLightRigActorMaskTest", 0);
    });
}

std::string percent(U64 part, U64 whole)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << (whole ? 100.0 * static_cast<F64>(part) / static_cast<F64>(whole) : 0.0) << '%';
    return out.str();
}
} // anonymous namespace

// ===========================================================================
// [ActorMask] Implementation state.
// ===========================================================================
struct ALActorMaskDiagnostics::Impl
{
    // ---- summary window -----------------------------------------------------
    bool mActive = false;
    U32 mLastMainFrame = 0;
    F64 mSummaryStart = 0.0;
    U64 mViews = 0;
    U64 mMaskDraws = 0;
    U64 mSwitchBase = 0;
    U64 mOverflowBase = 0;
    std::map<LLUUID, RecipientVerdictState> mRecipients;
    bool mDebugViewSkippedLogged = false;

    // ---- test instrument ----------------------------------------------------
    struct Test
    {
        bool mRunning = false;
        U32 mStep = 0;
        U32 mStepFrame = 0;
        F64 mStepTime = 0.0;
        U32 mCaptureFrame = 0;
        bool mGotDeferred = false;

        U32 mWidth = 0;
        U32 mHeight = 0;
        bool mHaveCamera = false;
        glm::mat4 mModelview{ 1.f };
        glm::mat4 mProjection{ 1.f };

        std::vector<U32> mDigestD0;   // C_deferred F0 (until F0')
        std::vector<U32> mDigestP0;   // C_post F0 (until F0')
        std::vector<U32> mDigestP1;   // C_post F1 (until F2)
        std::vector<U8> mFlags;       // PX_* per pixel
        std::vector<U8> mMask;        // F1 mask bytes (until F3)
        std::vector<F32> mParityRef;  // F1 RGB at PX_PARITY pixels, scan order (until F3)
        std::array<U64, 256> mHistogram{};
        U64 mMaskPixels = 0;

        bool mBoundsTrusted = true;
        bool mMaskChanged = false;
        bool mAnimating = false;
        std::string mVolumetrics;      // non-empty: which forward confound was on
        std::string mParityPath;       // non-empty: WP2 F3 path-record reason
        bool mShadowSlotsDiffer = false;
        std::vector<const void*> mShadowF1;
        F32 mParityMaxErr = 0.f;
        bool mHaveRigPrint = false;   // [ActorMask] review #1 rig-frame baseline (F1..F3)
        std::vector<F32> mRigPrint;
        U32 mRunId = 0;               // [ActorMask] review #2 watchdog generation
    } mTest;
    U32 mNextRunId = 0;

    void resetWindow(LLPipeline& pipe)
    {
        mSummaryStart = now_seconds();
        mViews = 0;
        mMaskDraws = 0;
        mSwitchBase = pipe.mActorMaskAlphaSwitches;
        mOverflowBase = pipe.mActorMaskAlphaSlotOverflow;
        mRecipients.clear();
    }

    // -----------------------------------------------------------------------
    // Verdicts
    // -----------------------------------------------------------------------
    void drainQueries(LLPipeline& pipe)
    {
        for (const LLActorMaskQueryResult& r : pipe.mActorMaskQueries.takeResults())
        {
            RecipientVerdictState& st = mRecipients[r.tag.recipient]; // [ActorMask] bit set from the live registry in settle
            QueryGroup& g = st.mGroups[{ r.tag.frame, r.tag.view }];
            switch (r.tag.kind)
            {
            case LLActorMaskQueryKind::TIGHT:
                g.mHasTight = true;
                g.mTight = r.samples;
                g.mTag = r.tag;
                break;
            case LLActorMaskQueryKind::LOOSE:
                g.mHasLoose = true;
                g.mLoose = r.samples;
                if (!g.mHasTight) g.mTag = r.tag;
                break;
            case LLActorMaskQueryKind::RASTER:
                g.mHasRaster = true;
                g.mRaster = r.samples;
                if (!g.mHasTight) g.mTag = r.tag;
                break;
            }
        }
    }

    // [ActorMask] Review #5: the live bit of a recipient that is still in the
    // registry and alive, or 0 when it was removed, died or was re-bitted away.
    static U8 liveBitFor(const LLUUID& id)
    {
        for (const auto& r : ALCineLightRigManager::instance().actorOnlyRecipients())
        {
            if (r.mAvatarId == id && r.mAvatar.notNull() && !r.mAvatar->isDead()) return r.mBit;
        }
        return 0;
    }

    void settleVerdicts(U32 debug)
    {
        for (auto entry = mRecipients.begin(); entry != mRecipients.end();)
        {
            // [ActorMask] Review #5: never evaluate or print a removed recipient.
            const U8 live_bit = liveBitFor(entry->first);
            if (!live_bit)
            {
                entry = mRecipients.erase(entry);
                continue;
            }
            RecipientVerdictState& st = entry->second;
            if (st.mBit != live_bit)
            {
                // New or re-bitted: verdict history belongs to the old identity.
                // Keep the pending groups; stale-bit tags are dropped below.
                auto groups = std::move(st.mGroups);
                st = RecipientVerdictState();
                st.mGroups = std::move(groups);
                st.mBit = live_bit;
            }
            for (auto it = st.mGroups.begin(); it != st.mGroups.end();)
            {
                const U32 age = gFrameCount - it->first.first;
                const QueryGroup& g = it->second;
                if (g.mTag.recipientBit != live_bit)
                {
                    it = st.mGroups.erase(it); // tag from a stale identity
                    continue;
                }
                bool decided = false;
                if (g.mHasTight)
                {
                    EVerdict v = EVerdict::NONE;
                    std::string detail;
                    if (classify_group(g, v, detail))
                    {
                        decided = true;
                        st.mStreak = v == st.mCurrent ? st.mStreak + 1 : 1;
                        st.mCurrent = v;
                        st.mDetail = detail;
                        // With Debug 0 every complete bracket comes from a 5 s
                        // summary frame, so this counts consecutive summaries.
                        if (v == EVerdict::INCONCLUSIVE)
                        {
                            if (debug == 0 && g.mHasRaster) ++st.mInconclusiveRun;
                            else if (debug != 0) st.mInconclusiveRun = 0;
                        }
                        else
                        {
                            st.mInconclusiveRun = 0;
                            st.mHintPrinted = false;
                        }
                    }
                }
                if (decided || age >= GROUP_DROP_FRAMES ||
                    (!g.mHasTight && age >= GROUP_SETTLE_FRAMES))
                    it = st.mGroups.erase(it);
                else
                    ++it;
            }
        }
    }

    void printVerdicts()
    {
        const F64 now = now_seconds();
        for (auto& [id, st] : mRecipients)
        {
            // Change-only, confirmed by two consecutive evaluations, and at most
            // one line per recipient per 5 s: a flapping state can never spam.
            if (st.mCurrent != EVerdict::NONE && st.mCurrent != st.mPrinted && st.mStreak >= 2 &&
                (st.mPrintedAt < 0.0 || now - st.mPrintedAt >= VERDICT_MIN_INTERVAL_S))
            {
                st.mPrinted = st.mCurrent;
                // IMPOSTOR/STYLED are already logged on change by the mask pass.
                if (st.mCurrent != EVerdict::IMPOSTOR && st.mCurrent != EVerdict::STYLED)
                {
                    st.mPrintedAt = now;
                    LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK " << verdict_name(st.mCurrent)
                        << ' ' << id << " bit=" << bit_string(st.mBit) << ' ' << st.mDetail << LL_ENDL;
                }
            }
            if (st.mCurrent == EVerdict::INCONCLUSIVE &&
                st.mInconclusiveRun >= INCONCLUSIVE_HINT_RUN && !st.mHintPrinted)
            {
                st.mHintPrinted = true;
                st.mPrintedAt = now;
                LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK VISIBILITY INCONCLUSIVE " << id
                    << " bit=" << bit_string(st.mBit) << ' ' << st.mDetail << " ("
                    << st.mInconclusiveRun << " consecutive summaries) check Debug=1 overlay" << LL_ENDL;
            }
        }
    }

    void pruneRecipients()
    {
        const auto& live = ALCineLightRigManager::instance().actorOnlyRecipients();
        for (auto it = mRecipients.begin(); it != mRecipients.end();)
        {
            const bool alive = std::any_of(live.begin(), live.end(), [&](const auto& r)
            {
                return r.mAvatarId == it->first && r.mAvatar.notNull() && !r.mAvatar->isDead();
            });
            if (alive) ++it; else it = mRecipients.erase(it);
        }
    }

    void maybeSummary(LLPipeline& pipe)
    {
        const F64 now = now_seconds();
        if (now - mSummaryStart < SUMMARY_PERIOD_S) return;
        const auto& registry = ALCineLightRigManager::instance();
        LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK SUMMARY views=" << mViews
            << " recipients=" << registry.actorOnlyRecipients().size()
            << " mask_draws=" << mMaskDraws
            << " actor_only_lights=" << registry.actorOnlyLightCount()
            << " alpha_switches=" << (pipe.mActorMaskAlphaSwitches - mSwitchBase)
            << " alpha_slot_overflow=" << (pipe.mActorMaskAlphaSlotOverflow - mOverflowBase)
            << LL_ENDL;
        mSummaryStart = now;
        mViews = 0;
        mMaskDraws = 0;
        mSwitchBase = pipe.mActorMaskAlphaSwitches;
        mOverflowBase = pipe.mActorMaskAlphaSlotOverflow;
        pruneRecipients();
    }

    // -----------------------------------------------------------------------
    // Test instrument (sec. 9.3)
    // -----------------------------------------------------------------------
    void resetTest(LLPipeline& pipe, bool clear_setting)
    {
        mTest = Test();
        pipe.mActorMaskTestMode = LLPipeline::ActorMaskTestMode::NORMAL;
        if (clear_setting && gSavedSettings.getU32("CineLightRigActorMaskTest") != 0)
            gSavedSettings.setU32("CineLightRigActorMaskTest", 0);
    }

    void abortTest(LLPipeline& pipe, const std::string& reason)
    {
        test_all_inconclusive(reason);
        resetTest(pipe, true);
    }

    // [ActorMask] Review #2: ungated idle watchdog. Returns true (stop) once this
    // run is over; otherwise ends the run on cancel, off-transition or timeout.
    bool testWatchdog(U32 run)
    {
        if (!mTest.mRunning || mTest.mRunId != run) return true;
        const U32 command = gSavedSettings.getU32("CineLightRigActorMaskTest");
        if (command != 1)
            abortTest(gPipeline, "cancelled");
        else if (!ALCineLightRigManager::anyActorOnly())
            abortTest(gPipeline, "actor-only lights went off during the test");
        else if (now_seconds() - mTest.mStepTime > TEST_STEP_TIMEOUT_S)
            abortTest(gPipeline, std::string("no capture within 30 s in ") + test_mode_name(mTest.mStep));
        return !mTest.mRunning;
    }

    // [ActorMask] Review #2: listener-side cancel. Runs inside the control's
    // commit signal, so it never writes the control itself (resetTest(false)).
    void cancelFromListener(S32 command)
    {
        if (!mTest.mRunning || command == 1) return;
        test_all_inconclusive("cancelled");
        resetTest(gPipeline, false);
    }

    static std::string forwardConfound()
    {
        static LLCachedControl<bool> shafts(gSavedSettings, "BDMergeProjectorVolumetrics", false);
        static LLCachedControl<bool> froxel(gSavedSettings, "BDMergeFroxelVolumetrics", false);
        std::string what;
        if (shafts) what += "shafts ";
        if (froxel) what += "froxel ";
        if (ALCineHaze::isEnabled()) what += "haze ";
        return what;
    }

    void tickTest(LLPipeline& pipe)
    {
        static LLCachedControl<U32> command(gSavedSettings, "CineLightRigActorMaskTest", 0);
        static LLCachedControl<bool> ssr(gSavedSettings, "RenderScreenSpaceReflections", false);
        const auto& registry = ALCineLightRigManager::instance();
        const U8 live_bits = static_cast<U8>(registry.liveRecipientBits() &
                                             ~ALCineLightRigManager::ACTOR_ONLY_ORPHAN_BIT);

        // [ActorMask] Review #2: 2 cancels; so does 0 (or anything but 1) while running.
        if (command == 2 || (mTest.mRunning && command != 1))
        {
            if (mTest.mRunning) abortTest(pipe, "cancelled");
            else resetTest(pipe, true);
            return;
        }
        if (!mTest.mRunning)
        {
            if (pipe.mActorMaskTestMode != LLPipeline::ActorMaskTestMode::NORMAL)
                pipe.mActorMaskTestMode = LLPipeline::ActorMaskTestMode::NORMAL;
            if (command != 1) return;
            if (ssr)
            {
                abortTest(pipe, "SSR is on");
                return;
            }
            if (registry.actorOnlyLightCount() == 0 || live_bits == 0)
            {
                abortTest(pipe, "no active actor-only light");
                return;
            }
            mTest = Test();
            mTest.mRunning = true;
            mTest.mStep = 0;
            mTest.mStepFrame = gFrameCount;
            mTest.mStepTime = now_seconds();
            mTest.mRunId = ++mNextRunId;
            LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK TEST armed: F0 F1 F2 F3 F0', each held "
                << TEST_HOLD_SECONDS << " s / " << TEST_HOLD_FRAMES
                << " frames. Keep the camera fixed and the subject frozen." << LL_ENDL;
            // [ActorMask] Review #2: cancel / off-transition / timeout cleanup must
            // not depend on this render tick, which only runs while anyActorOnly()
            // and a main view renders. The idle watchdog lives exactly as long as
            // this run (generation-checked) and stops itself afterwards.
            const U32 run = mTest.mRunId;
            doOnIdleRepeating([run]() -> bool
            {
                return ALActorMaskDiagnostics::instance().mImpl->testWatchdog(run);
            });
        }

        if (now_seconds() - mTest.mStepTime > TEST_STEP_TIMEOUT_S)
        {
            abortTest(pipe, std::string("no capture within 30 s in ") + test_mode_name(mTest.mStep));
            return;
        }
        if (ssr)
        {
            abortTest(pipe, "SSR is on");
            return;
        }
        if (live_bits == 0)
        {
            abortTest(pipe, "no active actor-only light");
            return;
        }
        pipe.mActorMaskTestMode = test_mode_for(mTest.mStep);
        // [ActorMask] Review #1: liveProbeAnimating() skips actor-only emitters
        // (WP4), so it cannot guard PARITY. Sample every enabled rig emitter.
        if (mTest.mStep >= 1 && mTest.mStep <= 3 && rigsAnimatingForTest())
            mTest.mAnimating = true;
        if (mTest.mCaptureFrame == 0 &&
            gFrameCount - mTest.mStepFrame >= TEST_HOLD_FRAMES &&
            now_seconds() - mTest.mStepTime >= TEST_HOLD_SECONDS)
        {
            mTest.mCaptureFrame = gFrameCount;
            mTest.mGotDeferred = false;
        }
    }

    // [ActorMask] Review #1: test-only animation predicate over EVERY emitter of
    // every enabled rig, actor-only ones included. True for an active FX, an
    // animated gobo on a lit projector, or any frame-to-frame change of the
    // evaluated rig frame (transitions, cues, flicker, FX modulation all land
    // in lastFrame()). Conservative: any drift reads as animating.
    bool rigsAnimatingForTest()
    {
        const auto& mgr = ALCineLightRigManager::instance();
        bool animating = false;
        std::vector<F32> print;
        const auto add_emitter = [&](const ALCineLightRigModel::EmitterState& e)
        {
            const F32 fields[] = { e.mOffX, e.mOffY, e.mOffZ, e.mAimX, e.mAimY, e.mAimZ,
                e.mSR, e.mSG, e.mSB, e.mIntensity, e.mLightRadius, e.mFalloff, e.mFovRad,
                e.mOn ? 1.f : 0.f, e.mClipped ? 1.f : 0.f, static_cast<F32>(e.mGobo) };
            print.insert(print.end(), std::begin(fields), std::end(fields));
        };
        for (S32 slot = 0; slot < ALCineLightRigManager::SLOT_COUNT; ++slot)
        {
            const auto rig_slot = static_cast<ALCineLightRigManager::Slot>(slot);
            if (!mgr.isSlotEnabled(rig_slot)) continue;
            const ALCineLightRig& rig = mgr.at(rig_slot);
            if (rig.activeFX() >= 0) animating = true;
            const ALCineLightRigModel::RigFrame& frame = rig.lastFrame();
            print.push_back(static_cast<F32>(slot));
            for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
            {
                if (frame.mProj[i].mOn && ALCineLightRigModel::goboIsAnimated(frame.mProj[i].mGobo))
                    animating = true;
                add_emitter(frame.mProj[i]);
                add_emitter(frame.mOmni[i]);
                print.push_back(frame.mDerivedShadowSoftness[i]);
            }
            print.push_back(frame.mCatchlightSizeScale);
        }
        if (!mTest.mHaveRigPrint)
        {
            mTest.mHaveRigPrint = true;
            mTest.mRigPrint = std::move(print);
            return animating;
        }
        if (print.size() != mTest.mRigPrint.size() ||
            (!print.empty() && std::memcmp(print.data(), mTest.mRigPrint.data(),
                                           print.size() * sizeof(F32)) != 0))
            animating = true;
        return animating;
    }

    bool checkCamera(const LLRenderTarget& screen, std::string& reason)
    {
        const glm::mat4 modelview = get_current_modelview();
        const glm::mat4 projection = get_current_projection();
        if (!mTest.mHaveCamera)
        {
            mTest.mHaveCamera = true;
            mTest.mModelview = modelview;
            mTest.mProjection = projection;
            mTest.mWidth = screen.getWidth();
            mTest.mHeight = screen.getHeight();
            return true;
        }
        if (screen.getWidth() != mTest.mWidth || screen.getHeight() != mTest.mHeight)
        {
            reason = "screen size changed";
            return false;
        }
        if (std::memcmp(&modelview, &mTest.mModelview, sizeof(glm::mat4)) != 0 ||
            std::memcmp(&projection, &mTest.mProjection, sizeof(glm::mat4)) != 0)
        {
            reason = "camera changed";
            return false;
        }
        return true;
    }

    std::vector<const void*> shadowSlots(const LLPipeline& pipe) const
    {
        std::vector<const void*> slots;
        for (const auto& light : pipe.mShadowSpotLight) slots.push_back(light.get());
        for (const auto& light : pipe.mTargetShadowSpotLight) slots.push_back(light.get());
        return slots;
    }

    // Distinct live light_bits of every actor-only emitter (projector, omni, catchlight).
    static std::vector<U8> actorLightBits()
    {
        const auto& mgr = ALCineLightRigManager::instance();
        const U8 live = static_cast<U8>(mgr.liveRecipientBits() &
                                        ~ALCineLightRigManager::ACTOR_ONLY_ORPHAN_BIT);
        std::vector<U8> bits;
        const auto add = [&](const LLUUID& id)
        {
            if (id.isNull()) return;
            const U8 b = static_cast<U8>(mgr.actorOnlyBits(id) & live);
            if (b && std::find(bits.begin(), bits.end(), b) == bits.end()) bits.push_back(b);
        };
        for (S32 slot = 0; slot < ALCineLightRigManager::SLOT_COUNT; ++slot)
        {
            const ALCineLightRig& rig = mgr.at(static_cast<ALCineLightRigManager::Slot>(slot));
            for (S32 i = 0; i < ALCineLightRigModel::LIGHT_COUNT; ++i)
            {
                add(rig.projectorId(i));
                add(rig.omniId(i));
            }
            add(rig.catchlightId());
        }
        return bits;
    }

    bool readMask(const LLPipeline& pipe, std::vector<U8>& out, std::string& error)
    {
        const LLRenderTarget& mask = pipe.mRT->actorMask;
        if (mask.getWidth() != mTest.mWidth || mask.getHeight() != mTest.mHeight)
        {
            error = "mask size differs from screen";
            return false;
        }
        out.assign(static_cast<size_t>(mTest.mWidth) * mTest.mHeight, 0);
        const U32 width = mTest.mWidth;
        return read_attachment0(mask, GL_RED, GL_UNSIGNED_BYTE, 1,
            [&](U32 y0, U32 rows, const U8* data)
            {
                std::memcpy(&out[static_cast<size_t>(y0) * width], data,
                            static_cast<size_t>(rows) * width);
            }, error);
    }

    // F1: flags from the mask (erosion, light coverage, recipient bounds).
    void buildMaskFlags(const std::vector<U8>& lights)
    {
        const S32 w = static_cast<S32>(mTest.mWidth);
        const S32 h = static_cast<S32>(mTest.mHeight);
        const std::vector<U8>& mask = mTest.mMask;
        std::vector<U8>& flags = mTest.mFlags;

        // Separable erosion of "mask != 0" by PARITY_ERODE_PX (out of image = 0).
        std::vector<U8> horizontal(mask.size(), 0);
        for (S32 y = 0; y < h; ++y)
        {
            S32 run = 0;
            for (S32 x = 0; x < w; ++x)
            {
                run = mask[static_cast<size_t>(y) * w + x] ? run + 1 : 0;
                // run counts consecutive non-zero pixels ending at x.
                const S32 centre = x - PARITY_ERODE_PX;
                if (centre >= 0 && run >= 2 * PARITY_ERODE_PX + 1)
                    horizontal[static_cast<size_t>(y) * w + centre] = 1;
            }
        }
        // [ActorMask] Review #4: one mask bounding box PER recipient bit, never a
        // single box over every recipient (the set between two actors must stay
        // inside the ISOLATION-FORWARD comparison).
        std::array<ScreenRect, 8> bit_box;
        for (ScreenRect& r : bit_box) r = { w, h, -1, -1 }; // empty until a pixel lands
        for (S32 x = 0; x < w; ++x)
        {
            S32 run = 0;
            for (S32 y = 0; y < h; ++y)
            {
                run = horizontal[static_cast<size_t>(y) * w + x] ? run + 1 : 0;
                const S32 centre = y - PARITY_ERODE_PX;
                if (centre < 0 || run < 2 * PARITY_ERODE_PX + 1) continue;
                const size_t p = static_cast<size_t>(centre) * w + x;
                const U8 byte = mask[p];
                bool covered = !lights.empty();
                for (U8 bits : lights) covered = covered && (byte & bits) != 0;
                if (covered) flags[p] |= PX_PARITY;
            }
        }
        for (S32 y = 0; y < h; ++y)
        {
            for (S32 x = 0; x < w; ++x)
            {
                const size_t p = static_cast<size_t>(y) * w + x;
                const U8 byte = mask[p];
                ++mTest.mHistogram[byte];
                if (!byte) continue;
                flags[p] |= PX_MASK_NZ;
                ++mTest.mMaskPixels;
                for (U32 bit = 0; bit < 8; ++bit)
                {
                    if (!(byte & (1u << bit))) continue;
                    ScreenRect& r = bit_box[bit];
                    r.x0 = std::min(r.x0, x); r.x1 = std::max(r.x1, x);
                    r.y0 = std::min(r.y0, y); r.y1 = std::max(r.y1, y);
                }
            }
        }

        // Recipient bounds, each dilated by 8 px on its own: every recipient's
        // projected animation extents (attachments included) plus the mask
        // bounding box of every recipient bit present in the mask.
        std::vector<ScreenRect> rects;
        for (const ScreenRect& r : bit_box)
        {
            if (r.x1 >= 0) rects.push_back({ r.x0, r.y0, r.x1 + 1, r.y1 + 1 }); // inclusive -> half-open
        }
        const glm::mat4 clip_from_agent = mTest.mProjection * mTest.mModelview;
        for (const auto& recipient : ALCineLightRigManager::instance().actorOnlyRecipients())
        {
            const LLVOAvatar* av = recipient.mAvatar.get();
            if (!av || av->isDead()) continue;
            const LLVector3* ext = av->getLastAnimExtents();
            F32 lo_x = 1.e30f, lo_y = 1.e30f, hi_x = -1.e30f, hi_y = -1.e30f;
            bool trusted = ext[0].isFinite() && ext[1].isFinite();
            for (U32 i = 0; i < 8 && trusted; ++i)
            {
                const glm::vec4 corner(ext[(i & 1) ? 1 : 0].mV[0], ext[(i & 2) ? 1 : 0].mV[1],
                                       ext[(i & 4) ? 1 : 0].mV[2], 1.f);
                const glm::vec4 clip = clip_from_agent * corner;
                if (!(clip.w > 1.e-4f)) { trusted = false; break; } // behind/at the eye
                const F32 px = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<F32>(w);
                const F32 py = (clip.y / clip.w * 0.5f + 0.5f) * static_cast<F32>(h);
                if (!std::isfinite(px) || !std::isfinite(py) ||
                    std::fabs(px) > 1.e6f || std::fabs(py) > 1.e6f) { trusted = false; break; }
                lo_x = std::min(lo_x, px); hi_x = std::max(hi_x, px);
                lo_y = std::min(lo_y, py); hi_y = std::max(hi_y, py);
            }
            if (!trusted)
            {
                mTest.mBoundsTrusted = false;
                continue;
            }
            rects.push_back({ static_cast<S32>(std::floor(lo_x)), static_cast<S32>(std::floor(lo_y)),
                              static_cast<S32>(std::ceil(hi_x)) + 1, static_cast<S32>(std::ceil(hi_y)) + 1 });
        }
        for (ScreenRect& r : rects)
        {
            r.x0 = llclamp(r.x0 - FORWARD_DILATE_PX, 0, w);
            r.y0 = llclamp(r.y0 - FORWARD_DILATE_PX, 0, h);
            r.x1 = llclamp(r.x1 + FORWARD_DILATE_PX, 0, w);
            r.y1 = llclamp(r.y1 + FORWARD_DILATE_PX, 0, h);
        }
        for (S32 y = 0; y < h; ++y)
        {
            for (S32 x = 0; x < w; ++x)
            {
                bool inside = false;
                for (const ScreenRect& r : rects)
                    inside = inside || (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1);
                if (!inside) flags[static_cast<size_t>(y) * w + x] |= PX_OUTSIDE;
            }
        }
    }

    void captureDeferred(LLPipeline& pipe, LLRenderTarget& screen)
    {
        if (!mTest.mRunning || mTest.mCaptureFrame != gFrameCount || mTest.mGotDeferred) return;
        std::string reason;
        if (!actor_mask_view_ready(pipe))
        {
            abortTest(pipe, "main view has no valid actor mask");
            return;
        }
        if (!checkCamera(screen, reason))
        {
            abortTest(pipe, reason);
            return;
        }
        const size_t n = static_cast<size_t>(mTest.mWidth) * mTest.mHeight;
        const U32 width = mTest.mWidth;
        constexpr U32 bpp = 4 * sizeof(F32);
        std::string error;
        bool ok = true;

        switch (mTest.mStep)
        {
        case 0: // F0: reference digests
            mTest.mFlags.assign(n, 0);
            mTest.mDigestD0.assign(n, 0);
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                    mTest.mDigestD0[static_cast<size_t>(y0) * width + i] = pixel_digest(data + i * bpp);
            }, error);
            break;

        case 1: // F1: mask, histogram, flags, F1-vs-F0 change, parity reference
        {
            ok = readMask(pipe, mTest.mMask, error);
            if (!ok) break;
            buildMaskFlags(actorLightBits());
            mTest.mShadowF1 = shadowSlots(pipe);
            mTest.mVolumetrics += forwardConfound();
            mTest.mParityRef.clear();
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                {
                    const size_t p = static_cast<size_t>(y0) * width + i;
                    const U8* texel = data + i * bpp;
                    if (pixel_digest(texel) != mTest.mDigestD0[p]) mTest.mFlags[p] |= PX_DEF_CHANGED;
                    if (mTest.mFlags[p] & PX_PARITY)
                    {
                        F32 rgb[3];
                        std::memcpy(rgb, texel, sizeof(rgb));
                        mTest.mParityRef.insert(mTest.mParityRef.end(), rgb, rgb + 3);
                    }
                }
            }, error);
            break;
        }

        case 2: // F2: the mask must not change
        {
            std::vector<U8> mask;
            ok = readMask(pipe, mask, error);
            if (!ok) break;
            mTest.mMaskChanged = mTest.mMaskChanged || mask != mTest.mMask;
            mTest.mVolumetrics += forwardConfound();
            break;
        }

        case 3: // F3: mask unchanged, slot assignment, path record, parity
        {
            std::vector<U8> mask;
            ok = readMask(pipe, mask, error);
            if (!ok) break;
            mTest.mMaskChanged = mTest.mMaskChanged || mask != mTest.mMask;
            mTest.mMask.clear();
            mTest.mMask.shrink_to_fit();
            mTest.mShadowSlotsDiffer = shadowSlots(pipe) != mTest.mShadowF1;
            // WP2's record of which path each accepted F3 actor point took.
            const char* path = pipe.actorMaskParityPathInconclusiveReason();
            mTest.mParityPath = path ? path : "";
            size_t k = 0;
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                {
                    const size_t p = static_cast<size_t>(y0) * width + i;
                    if (!(mTest.mFlags[p] & PX_PARITY)) continue;
                    if (k + 3 > mTest.mParityRef.size()) { mTest.mFlags[p] |= PX_PARITY_FAIL; continue; }
                    F32 b[3];
                    std::memcpy(b, data + i * bpp, sizeof(b));
                    for (U32 c = 0; c < 3; ++c)
                    {
                        const F32 a = mTest.mParityRef[k + c];
                        if (std::memcmp(&a, &b[c], sizeof(F32)) == 0) continue; // bit-identical
                        const F32 diff = std::fabs(a - b[c]);
                        const F32 tol = 1.e-3f + 0.01f * std::max(a, b[c]);
                        if (!(diff <= tol)) mTest.mFlags[p] |= PX_PARITY_FAIL; // NaN fails
                        if (std::isfinite(diff))
                            mTest.mParityMaxErr = std::max(mTest.mParityMaxErr, diff - tol);
                    }
                    k += 3;
                }
            }, error);
            mTest.mParityRef.clear();
            mTest.mParityRef.shrink_to_fit();
            break;
        }

        case 4: // F0': deferred stability
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                {
                    const size_t p = static_cast<size_t>(y0) * width + i;
                    if (pixel_digest(data + i * bpp) == mTest.mDigestD0[p]) mTest.mFlags[p] |= PX_STABLE_DEF;
                }
            }, error);
            mTest.mDigestD0.clear();
            mTest.mDigestD0.shrink_to_fit();
            break;

        default:
            break;
        }
        if (!ok)
        {
            abortTest(pipe, std::string("readback failed at C_deferred ") +
                      test_mode_name(mTest.mStep) + ": " + error);
            return;
        }
        mTest.mGotDeferred = true;
    }

    void capturePost(LLPipeline& pipe, LLRenderTarget& screen)
    {
        if (!mTest.mRunning || mTest.mCaptureFrame != gFrameCount) return;
        if (!mTest.mGotDeferred)
        {
            mTest.mCaptureFrame = 0; // C_deferred did not run this frame: retry
            return;
        }
        const size_t n = static_cast<size_t>(mTest.mWidth) * mTest.mHeight;
        const U32 width = mTest.mWidth;
        constexpr U32 bpp = 4 * sizeof(F32);
        std::string error;
        bool ok = true;
        if (screen.getWidth() != mTest.mWidth || screen.getHeight() != mTest.mHeight)
        {
            abortTest(pipe, "screen size changed");
            return;
        }
        const auto digest_into = [&](std::vector<U32>& out)
        {
            out.assign(n, 0);
            return read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                    out[static_cast<size_t>(y0) * width + i] = pixel_digest(data + i * bpp);
            }, error);
        };
        switch (mTest.mStep)
        {
        case 0: ok = digest_into(mTest.mDigestP0); break;
        case 1: ok = digest_into(mTest.mDigestP1); break;
        case 2:
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                {
                    const size_t p = static_cast<size_t>(y0) * width + i;
                    if (pixel_digest(data + i * bpp) != mTest.mDigestP1[p]) mTest.mFlags[p] |= PX_POST_CHANGED;
                }
            }, error);
            mTest.mDigestP1.clear();
            mTest.mDigestP1.shrink_to_fit();
            break;
        case 3: break;
        case 4:
            ok = read_attachment0(screen, GL_RGBA, GL_FLOAT, bpp, [&](U32 y0, U32 rows, const U8* data)
            {
                for (size_t i = 0, count = static_cast<size_t>(rows) * width; i < count; ++i)
                {
                    const size_t p = static_cast<size_t>(y0) * width + i;
                    if (pixel_digest(data + i * bpp) == mTest.mDigestP0[p]) mTest.mFlags[p] |= PX_STABLE_POST;
                }
            }, error);
            break;
        default: break;
        }
        if (!ok)
        {
            abortTest(pipe, std::string("readback failed at C_post ") +
                      test_mode_name(mTest.mStep) + ": " + error);
            return;
        }

        ++mTest.mStep;
        mTest.mCaptureFrame = 0;
        mTest.mGotDeferred = false;
        mTest.mStepFrame = gFrameCount + 1; // the next frame is the first in the new mode
        mTest.mStepTime = now_seconds();
        if (mTest.mStep < TEST_STEP_COUNT)
        {
            pipe.mActorMaskTestMode = test_mode_for(mTest.mStep);
            return;
        }
        evaluate();
        resetTest(pipe, true);
    }

    void evaluate()
    {
        const auto& t = mTest;
        U64 total = 0, stable_def = 0, stable_post = 0;
        U64 def_changed_out = 0, def_changed_in = 0, def_compared_out = 0;
        U64 post_compared = 0, post_changed_out = 0, post_changed_in = 0;
        U64 parity_compared = 0, parity_failed = 0;
        S64 def_first = -1, post_first = -1, parity_first = -1;
        for (size_t p = 0; p < t.mFlags.size(); ++p)
        {
            const U8 f = t.mFlags[p];
            ++total;
            if (f & PX_STABLE_DEF)
            {
                ++stable_def;
                if (!(f & PX_MASK_NZ))
                {
                    ++def_compared_out;
                    if (f & PX_DEF_CHANGED) { ++def_changed_out; if (def_first < 0) def_first = static_cast<S64>(p); }
                }
                else if (f & PX_DEF_CHANGED)
                {
                    ++def_changed_in;
                }
                if (f & PX_PARITY)
                {
                    ++parity_compared;
                    if (f & PX_PARITY_FAIL) { ++parity_failed; if (parity_first < 0) parity_first = static_cast<S64>(p); }
                }
            }
            if (f & PX_STABLE_POST)
            {
                ++stable_post;
                if (f & PX_OUTSIDE)
                {
                    ++post_compared;
                    if (f & PX_POST_CHANGED) { ++post_changed_out; if (post_first < 0) post_first = static_cast<S64>(p); }
                }
                else if (f & PX_POST_CHANGED)
                {
                    ++post_changed_in;
                }
            }
        }
        const auto at = [&](S64 p)
        {
            if (p < 0 || !t.mWidth) return std::string("none");
            return std::to_string(static_cast<U64>(p) % t.mWidth) + "," +
                   std::to_string(static_cast<U64>(p) / t.mWidth);
        };

        // Mask-byte histogram (Fable A1): non-empty bins only.
        {
            std::ostringstream hist;
            for (U32 b = 0; b < 256; ++b)
                if (t.mHistogram[b]) hist << ' ' << b << '=' << t.mHistogram[b];
            LL_INFOS("ActorMask") << "[ActorMask] ACTORMASK TEST mask-histogram" << hist.str()
                << " stable_deferred=" << percent(stable_def, total)
                << " stable_post=" << percent(stable_post, total) << LL_ENDL;
        }

        std::string general;
        if (t.mMaskPixels == 0) general = "recipient off screen (no mask pixel at F1)";
        else if (t.mMaskChanged) general = "mask changed across F1-F3";

        // ISOLATION-DEFERRED: F1 vs F0 at C_deferred, stable pixels with mask == 0.
        if (t.mHistogram[0] == 0)
            test_line("ISOLATION-DEFERRED", "FAIL", "mask clear (histogram has no zero bin)");
        else if (!general.empty())
            test_line("ISOLATION-DEFERRED", "INCONCLUSIVE", general);
        else if (stable_def * 2 < total)
            test_line("ISOLATION-DEFERRED", "INCONCLUSIVE", "only " + percent(stable_def, total) + " stable (F0 vs F0')");
        else if (def_changed_out)
            test_line("ISOLATION-DEFERRED", "FAIL", std::to_string(def_changed_out) + " of " +
                      std::to_string(def_compared_out) + " stable unmasked pixels changed, first=" + at(def_first));
        else if (!def_changed_in)
            test_line("ISOLATION-DEFERRED", "INCONCLUSIVE", "zero changed mask pixels (actor-only light had no visible effect)");
        else
            test_line("ISOLATION-DEFERRED", "PASS", std::to_string(def_compared_out) +
                      " stable unmasked pixels identical, " + std::to_string(def_changed_in) + " mask pixels lit");

        // ISOLATION-FORWARD: F2 vs F1 at C_post, stable pixels outside dilated bounds.
        if (!general.empty())
            test_line("ISOLATION-FORWARD", "INCONCLUSIVE", general);
        else if (!t.mVolumetrics.empty())
            test_line("ISOLATION-FORWARD", "INCONCLUSIVE", "volumetrics on: " + t.mVolumetrics);
        else if (!t.mBoundsTrusted)
            test_line("ISOLATION-FORWARD", "INCONCLUSIVE", "recipient bounds cross the near plane");
        else if (stable_post * 2 < total)
            test_line("ISOLATION-FORWARD", "INCONCLUSIVE", "only " + percent(stable_post, total) + " stable (F0 vs F0')");
        else if (!post_compared)
            test_line("ISOLATION-FORWARD", "INCONCLUSIVE", "recipient bounds cover every stable pixel");
        else if (post_changed_out)
            test_line("ISOLATION-FORWARD", "FAIL", std::to_string(post_changed_out) + " of " +
                      std::to_string(post_compared) + " stable pixels outside recipient bounds changed, first=" + at(post_first));
        else
            test_line("ISOLATION-FORWARD", "PASS", std::to_string(post_compared) +
                      " stable pixels outside recipient bounds identical, " +
                      std::to_string(post_changed_in) + " changed inside");

        // PARITY: F1 vs F3 at C_deferred on eroded mask pixels.
        if (!general.empty())
            test_line("PARITY", "INCONCLUSIVE", general);
        else if (!t.mParityPath.empty())
            test_line("PARITY", "INCONCLUSIVE", t.mParityPath);
        else if (t.mShadowSlotsDiffer)
            test_line("PARITY", "INCONCLUSIVE", "shadow-slot assignment differs between F1 and F3");
        else if (t.mAnimating)
            test_line("PARITY", "INCONCLUSIVE", "rig animating during F1-F3");
        else if (stable_def * 2 < total)
            test_line("PARITY", "INCONCLUSIVE", "only " + percent(stable_def, total) + " stable (F0 vs F0')");
        else if (!parity_compared)
            test_line("PARITY", "INCONCLUSIVE", "no stable eroded mask pixel is lit by every actor-only light");
        else if (parity_failed)
        {
            std::ostringstream out;
            out << parity_failed << " of " << parity_compared << " eroded mask pixels outside |a-b| <= 1e-3 + 0.01*max(a,b), first="
                << at(parity_first) << " worst_excess=" << t.mParityMaxErr;
            test_line("PARITY", "FAIL", out.str());
        }
        else
            test_line("PARITY", "PASS", std::to_string(parity_compared) + " eroded mask pixels within tolerance");
    }

    // -----------------------------------------------------------------------
    // Debug view (sec. 9.2): new program at the end of the main view's lighting.
    // -----------------------------------------------------------------------
    void drawDebugView(LLPipeline& pipe, LLRenderTarget& screen, U32 mode, U64* leaks)
    {
        if (!gActorMaskDebugProgram.isComplete())
        {
            LL_WARNS_ONCE("ActorMask") << "[ActorMask] ACTORMASK NO-PROGRAM debug view" << LL_ENDL;
            return;
        }
        GLint draw_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
        if (LLRenderTarget::getCurrentBoundTarget() != &screen || pipe.mScreenTriangleVB.isNull() ||
            static_cast<U32>(draw_fbo) != screen.getFBO() || !draw_fbo)
        {
            if (!mDebugViewSkippedLogged)
            {
                mDebugViewSkippedLogged = true;
                LL_WARNS("ActorMask") << "[ActorMask] ACTORMASK debug view skipped: screen target not bound" << LL_ENDL;
            }
            return;
        }

        LLScopedActorMaskInvariant companion("debug view");
        LLScopedGhostRenderInvariant guard("debug view", leaks, EGhostInvariantMode::ACTOR_MASK);
        gGL.flush();
        const ScopedVertexInputState vertex_state; // [ActorMask] review #3

        LLGLSLShader* const prev_shader = LLGLSLShader::sCurBoundShaderPtr;
        const U32 prev_unit = gGL.getCurrentTexUnitIndex();
        GLint prev_active = GL_TEXTURE0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
        GLint blend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
        glGetIntegerv(GL_BLEND_SRC_RGB, &blend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &blend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blend[3]);
        GLboolean color[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        glGetBooleanv(GL_COLOR_WRITEMASK, color);
        GLint max_buffers = 1;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_buffers);
        std::vector<GLenum> draw_buffers(static_cast<size_t>(std::max(max_buffers, 1)), GL_NONE);
        for (size_t i = 0; i < draw_buffers.size(); ++i)
        {
            GLint value = GL_NONE;
            glGetIntegerv(static_cast<GLenum>(GL_DRAW_BUFFER0 + i), &value);
            draw_buffers[i] = static_cast<GLenum>(value);
        }

        {
            LLGLDepthTest depth(GL_FALSE, GL_FALSE);
            LLGLDisable cull(GL_CULL_FACE);
            LLGLEnable blend_on(GL_BLEND);
            gGL.setColorMask(true, true);
            // Only the beauty attachment: never write the sidecar/coverage targets.
            const GLenum beauty = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &beauty);
            if (mode == 2)
                glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ZERO);           // mask only, glow 0
            else
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE); // tint, glow kept

            gActorMaskDebugProgram.bind();
            const S32 channel = gActorMaskDebugProgram.getTextureChannel(LLShaderMgr::ACTOR_MASK_MAP);
            std::optional<LLActorMaskTextureState> saved_unit;
            if (channel >= 0) saved_unit.emplace(*gGL.getTexUnit(static_cast<U32>(channel)));
            if (channel >= 0 && pipe.bindActorMask(gActorMaskDebugProgram) >= 0)
            {
                static const LLStaticHashedString debug_mode("actor_mask_debug_mode");
                gActorMaskDebugProgram.uniform1i(debug_mode, static_cast<GLint>(mode));
                pipe.mScreenTriangleVB->setBuffer();
                pipe.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }
            if (saved_unit) saved_unit->restore(*gGL.getTexUnit(static_cast<U32>(channel)));

            glBlendFuncSeparate(static_cast<GLenum>(blend[0]), static_cast<GLenum>(blend[1]),
                                static_cast<GLenum>(blend[2]), static_cast<GLenum>(blend[3]));
            glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
            gGL.setColorMask(color[0] != GL_FALSE, color[1] != GL_FALSE, color[2] != GL_FALSE, color[3] != GL_FALSE);
        }
        if (prev_shader) prev_shader->bind();
        else LLGLSLShader::unbind();
        vertex_state.restore(); // [ActorMask] review #3: after the program bind's client-array setup
        gGL.getTexUnit(prev_unit)->activate();
        glActiveTexture(static_cast<GLenum>(prev_active));
    }
};

// ===========================================================================
// [ActorMask] Public entry points.
// ===========================================================================
ALActorMaskDiagnostics& ALActorMaskDiagnostics::instance()
{
    static ALActorMaskDiagnostics diagnostics;
    return diagnostics;
}

ALActorMaskDiagnostics::ALActorMaskDiagnostics() : mImpl(std::make_unique<Impl>()) {}
ALActorMaskDiagnostics::~ALActorMaskDiagnostics() = default;

void ALActorMaskDiagnostics::connectSettings()
{
    static bool connected = false;
    if (connected) return;
    LLControlVariablePtr control = gSavedSettings.getControl("CineLightRigActorMaskTest");
    if (control.isNull()) return;
    connected = true;
    // The per-frame tick only runs while anyActorOnly(); answer an arm or a
    // cancel request made with no Actor-only light here, on the user action.
    control->getSignal()->connect([](LLControlVariable*, const LLSD& value, const LLSD&)
    {
        const S32 command = value.asInteger();
        // [ActorMask] Review #2: any value but 1 ends a running test at once,
        // whether or not anything renders; the idle reset clears a 2.
        const bool was_running = instance().mImpl->mTest.mRunning;
        instance().mImpl->cancelFromListener(command);
        if (was_running && command == 2)
        {
            reset_test_setting_on_idle();
            return;
        }
        if (command == 0 || ALCineLightRigManager::anyActorOnly()) return;
        if (command == 1) test_all_inconclusive("no active actor-only light");
        reset_test_setting_on_idle();
    });
}

void ALActorMaskDiagnostics::noteMaskDraws(U32 draws)
{
    mImpl->mMaskDraws += draws;
}

void ALActorMaskDiagnostics::onLightingView(LLPipeline& pipe, bool main_view)
{
    Impl& d = *mImpl;
    if (actor_mask_view_ready(pipe)) ++d.mViews;
    if (!main_view || d.mLastMainFrame == gFrameCount) return; // once per frame (tiled snapshots)

    // A main-view gap means Actor-only was off: start a clean window.
    if (!d.mActive || gFrameCount - d.mLastMainFrame > REACTIVATE_GAP_FRAMES)
    {
        if (d.mTest.mRunning)
            d.abortTest(pipe, "actor-only lights went off (or the main view paused) during the test");
        d.resetWindow(pipe);
        d.mViews = actor_mask_view_ready(pipe) ? 1 : 0;
        d.mActive = true;
    }
    d.mLastMainFrame = gFrameCount;

    static LLCachedControl<U32> debug(gSavedSettings, "CineLightRigActorMaskDebug", 0);
    d.drainQueries(pipe);
    d.settleVerdicts(debug);
    d.printVerdicts();
    d.maybeSummary(pipe);
    d.tickTest(pipe);
}

void ALActorMaskDiagnostics::captureDeferred(LLPipeline& pipe, LLRenderTarget& screen)
{
    mImpl->captureDeferred(pipe, screen);
}

void ALActorMaskDiagnostics::endMainView(LLPipeline& pipe, LLRenderTarget& screen)
{
    Impl& d = *mImpl;
    d.capturePost(pipe, screen);

    static LLCachedControl<U32> debug(gSavedSettings, "CineLightRigActorMaskDebug", 0);
    const U32 mode = debug;
    // Never during the instrument: its captures must see the plain scene.
    if (mode != 0 && !d.mTest.mRunning && actor_mask_view_ready(pipe))
        d.drawDebugView(pipe, screen, mode == 2 ? 2U : 1U, &mStateLeaks);
}
