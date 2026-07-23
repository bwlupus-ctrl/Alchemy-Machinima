/**
 * @file llghostdeferreddiagnostics.cpp
 * @brief GL-state invariant checking for Ghost Studio deferred rendering.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llghostdeferreddiagnostics.h"

#include "llglheaders.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "llactormover.h"      // [GhostDeferred] proxy queue + counters
#include "llcamera.h"
#include "llviewercamera.h"    // sCurCameraID

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr U32 GHOST_TEXTURE_MATRIX_COUNT = 4;

template <typename T>
bool scalar_changed(const T& before, const T& after)
{
    return before != after;
}

template <typename T, std::size_t N>
bool array_changed(const std::array<T, N>& before,
                   const std::array<T, N>& after)
{
    return std::memcmp(before.data(), after.data(), sizeof(T) * N) != 0;
}

bool matrix_changed(const glm::mat4& before, const glm::mat4& after)
{
    return std::memcmp(&before, &after, sizeof(glm::mat4)) != 0;
}

U64 hash_matrix(const glm::mat4& matrix)
{
    // FNV-1a over the exact cached representation. This is for compact,
    // deterministic before/after diagnostics; equality itself still uses
    // an exact byte comparison above.
    const U8* bytes = reinterpret_cast<const U8*>(&matrix);
    U64 hash = 1469598103934665603ULL;

    for (std::size_t i = 0; i < sizeof(glm::mat4); ++i)
    {
        hash ^= static_cast<U64>(bytes[i]);
        hash *= 1099511628211ULL;
    }

    return hash;
}

template <typename T>
std::string scalar_string(const T& value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string bool_string(GLboolean value)
{
    return value == GL_TRUE ? "true" : "false";
}

template <typename T, std::size_t N>
std::string array_string(const std::array<T, N>& value)
{
    std::ostringstream out;
    out << '[';

    for (std::size_t i = 0; i < N; ++i)
    {
        if (i != 0)
        {
            out << ',';
        }
        out << value[i];
    }

    out << ']';
    return out.str();
}

std::string boolean_array_string(const std::array<GLboolean, 4>& value)
{
    std::ostringstream out;
    out << '['
        << bool_string(value[0]) << ','
        << bool_string(value[1]) << ','
        << bool_string(value[2]) << ','
        << bool_string(value[3]) << ']';
    return out.str();
}

std::string matrix_string(const glm::mat4& value)
{
    std::ostringstream out;
    out << "hash:0x"
        << std::hex << std::setw(16) << std::setfill('0')
        << hash_matrix(value);
    return out.str();
}

std::string pointer_string(const void* value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

void append_change(std::ostringstream& changes,
                   bool& any_change,
                   const char* field,
                   const std::string& before,
                   const std::string& after)
{
    if (any_change)
    {
        changes << "; ";
    }

    changes << field << ' ' << before << "->" << after;
    any_change = true;
}
} // anonymous namespace

struct LLScopedGhostRenderInvariant::Snapshot
{
    GLint mDrawFBO = 0;
    GLint mReadFBO = 0;
    U32 mWrapperFBO = 0;
    LLRenderTarget* mBoundRenderTarget = nullptr;

    std::array<GLint, 4> mViewport{};
    GLboolean mScissorEnabled = GL_FALSE;
    std::array<GLint, 4> mScissorBox{};

    LLRender::eMatrixMode mMatrixMode = LLRender::MM_MODELVIEW;
    glm::mat4 mModelview{1.f};
    std::array<glm::mat4, GHOST_TEXTURE_MATRIX_COUNT> mTextureMatrices{};

    std::array<GLboolean, 4> mColorMask{};

    GLboolean mDepthTestEnabled = GL_FALSE;
    GLboolean mDepthWriteMask = GL_FALSE;
    GLint mDepthFunc = GL_LESS;

    GLboolean mBlendEnabled = GL_FALSE;
    GLint mBlendSrcRGB = GL_ONE;
    GLint mBlendDstRGB = GL_ZERO;
    GLint mBlendSrcAlpha = GL_ONE;
    GLint mBlendDstAlpha = GL_ZERO;
    GLint mBlendEquationRGB = GL_FUNC_ADD;
    GLint mBlendEquationAlpha = GL_FUNC_ADD;

    GLboolean mCullEnabled = GL_FALSE;
    GLint mCullMode = GL_BACK;
    GLint mFrontFace = GL_CCW;

    GLboolean mFramebufferSRGB = GL_FALSE;

    GLint mCurrentProgram = 0;
    LLGLSLShader* mCurrentShader = nullptr;

    GLint mActiveTexture = GL_TEXTURE0;
    GLint mArrayBuffer = 0;
    GLint mElementArrayBuffer = 0;

    const LLMatrix4* mGLLastMatrix = nullptr;

    void capture()
    {
        // All raw queries here are valid in the viewer's OpenGL core-ish
        // profile. Deliberately do not query GL_TEXTURE_MATRIX.
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mDrawFBO);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFBO);
        mWrapperFBO = LLRenderTarget::sCurFBO;
        mBoundRenderTarget = LLRenderTarget::getCurrentBoundTarget();

        glGetIntegerv(GL_VIEWPORT, mViewport.data());
        mScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, mScissorBox.data());

        mMatrixMode = gGL.getMatrixMode();
        mModelview = gGL.getModelviewMatrix();
        mTextureMatrices[0] = gGL.getMatrix(LLRender::MM_TEXTURE0);
        mTextureMatrices[1] = gGL.getMatrix(LLRender::MM_TEXTURE1);
        mTextureMatrices[2] = gGL.getMatrix(LLRender::MM_TEXTURE2);
        mTextureMatrices[3] = gGL.getMatrix(LLRender::MM_TEXTURE3);

        glGetBooleanv(GL_COLOR_WRITEMASK, mColorMask.data());

        mDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthWriteMask);
        glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc);

        mBlendEnabled = glIsEnabled(GL_BLEND);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mBlendSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &mBlendDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mBlendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mBlendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &mBlendEquationRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &mBlendEquationAlpha);

        mCullEnabled = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &mCullMode);
        glGetIntegerv(GL_FRONT_FACE, &mFrontFace);

        mFramebufferSRGB = glIsEnabled(GL_FRAMEBUFFER_SRGB);

        glGetIntegerv(GL_CURRENT_PROGRAM, &mCurrentProgram);
        mCurrentShader = LLGLSLShader::sCurBoundShaderPtr;

        glGetIntegerv(GL_ACTIVE_TEXTURE, &mActiveTexture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &mArrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &mElementArrayBuffer);

        mGLLastMatrix = gGLLastMatrix;
    }
};

LLScopedGhostRenderInvariant::LLScopedGhostRenderInvariant(
    const char* scope,
    U64* violation_counter)
:   mScope(scope ? scope : "(unnamed)"),
    mViolationCounter(violation_counter)
{
    static LLCachedControl<bool> enabled(
        gSavedSettings, "GhostDeferredDebugLog", false);

    if (!enabled)
    {
        return;
    }

    mBefore = std::make_unique<Snapshot>();
    mBefore->capture();
}

LLScopedGhostRenderInvariant::~LLScopedGhostRenderInvariant()
{
    finish();
}

void LLScopedGhostRenderInvariant::finish()
{
    if (mFinished)
    {
        return;
    }

    mFinished = true;

    // A null snapshot means GhostDeferredDebugLog was disabled when this
    // invariant was constructed. The normal-play path did no GL queries.
    if (!mBefore)
    {
        return;
    }

    Snapshot after;
    after.capture();

    bool changed = false;
    std::ostringstream changes;

#define GHOST_COMPARE_SCALAR(member, label)                                  \
    do                                                                       \
    {                                                                        \
        if (scalar_changed(mBefore->member, after.member))                   \
        {                                                                    \
            append_change(changes, changed, label,                           \
                          scalar_string(mBefore->member),                     \
                          scalar_string(after.member));                      \
        }                                                                    \
    } while (false)

#define GHOST_COMPARE_BOOL(member, label)                                    \
    do                                                                       \
    {                                                                        \
        if (scalar_changed(mBefore->member, after.member))                   \
        {                                                                    \
            append_change(changes, changed, label,                           \
                          bool_string(mBefore->member),                       \
                          bool_string(after.member));                        \
        }                                                                    \
    } while (false)

#define GHOST_COMPARE_ARRAY(member, label)                                   \
    do                                                                       \
    {                                                                        \
        if (array_changed(mBefore->member, after.member))                    \
        {                                                                    \
            append_change(changes, changed, label,                           \
                          array_string(mBefore->member),                      \
                          array_string(after.member));                       \
        }                                                                    \
    } while (false)

#define GHOST_COMPARE_MATRIX(member, label)                                  \
    do                                                                       \
    {                                                                        \
        if (matrix_changed(mBefore->member, after.member))                   \
        {                                                                    \
            append_change(changes, changed, label,                           \
                          matrix_string(mBefore->member),                     \
                          matrix_string(after.member));                      \
        }                                                                    \
    } while (false)

#define GHOST_COMPARE_POINTER(member, label)                                 \
    do                                                                       \
    {                                                                        \
        if (scalar_changed(mBefore->member, after.member))                   \
        {                                                                    \
            append_change(changes, changed, label,                           \
                          pointer_string(mBefore->member),                    \
                          pointer_string(after.member));                     \
        }                                                                    \
    } while (false)

    GHOST_COMPARE_SCALAR(mDrawFBO, "draw_fbo");
    GHOST_COMPARE_SCALAR(mReadFBO, "read_fbo");
    GHOST_COMPARE_SCALAR(mWrapperFBO, "LLRenderTarget::sCurFBO");
    GHOST_COMPARE_POINTER(mBoundRenderTarget, "bound_render_target");

    GHOST_COMPARE_ARRAY(mViewport, "viewport");
    GHOST_COMPARE_BOOL(mScissorEnabled, "scissor_enable");
    GHOST_COMPARE_ARRAY(mScissorBox, "scissor_box");

    GHOST_COMPARE_SCALAR(mMatrixMode, "matrix_mode");
    GHOST_COMPARE_MATRIX(mModelview, "modelview");
    GHOST_COMPARE_MATRIX(mTextureMatrices[0], "texture_matrix_0");
    GHOST_COMPARE_MATRIX(mTextureMatrices[1], "texture_matrix_1");
    GHOST_COMPARE_MATRIX(mTextureMatrices[2], "texture_matrix_2");
    GHOST_COMPARE_MATRIX(mTextureMatrices[3], "texture_matrix_3");

    if (array_changed(mBefore->mColorMask, after.mColorMask))
    {
        append_change(changes, changed, "color_mask",
                      boolean_array_string(mBefore->mColorMask),
                      boolean_array_string(after.mColorMask));
    }

    GHOST_COMPARE_BOOL(mDepthTestEnabled, "depth_test");
    GHOST_COMPARE_BOOL(mDepthWriteMask, "depth_write");
    GHOST_COMPARE_SCALAR(mDepthFunc, "depth_func");

    GHOST_COMPARE_BOOL(mBlendEnabled, "blend_enable");
    GHOST_COMPARE_SCALAR(mBlendSrcRGB, "blend_src_rgb");
    GHOST_COMPARE_SCALAR(mBlendDstRGB, "blend_dst_rgb");
    GHOST_COMPARE_SCALAR(mBlendSrcAlpha, "blend_src_alpha");
    GHOST_COMPARE_SCALAR(mBlendDstAlpha, "blend_dst_alpha");
    GHOST_COMPARE_SCALAR(mBlendEquationRGB, "blend_equation_rgb");
    GHOST_COMPARE_SCALAR(mBlendEquationAlpha, "blend_equation_alpha");

    GHOST_COMPARE_BOOL(mCullEnabled, "cull_enable");
    GHOST_COMPARE_SCALAR(mCullMode, "cull_mode");
    GHOST_COMPARE_SCALAR(mFrontFace, "front_face");

    GHOST_COMPARE_BOOL(mFramebufferSRGB, "framebuffer_srgb");

    GHOST_COMPARE_SCALAR(mCurrentProgram, "current_program");
    GHOST_COMPARE_POINTER(mCurrentShader, "current_shader");

    GHOST_COMPARE_SCALAR(mActiveTexture, "active_texture");
    GHOST_COMPARE_SCALAR(mArrayBuffer, "array_buffer");
    GHOST_COMPARE_SCALAR(mElementArrayBuffer, "element_array_buffer");

    GHOST_COMPARE_POINTER(mGLLastMatrix, "gGLLastMatrix");

#undef GHOST_COMPARE_POINTER
#undef GHOST_COMPARE_MATRIX
#undef GHOST_COMPARE_ARRAY
#undef GHOST_COMPARE_BOOL
#undef GHOST_COMPARE_SCALAR

    if (!changed)
    {
        return;
    }

    // Exactly one verdict line per violating scope. Do not restore anything:
    // restoration here would conceal the contamination from the caller.
    LL_WARNS("GhostDeferredInvariant")
        << mScope << " GL STATE LEAK: " << changes.str()
        << LL_ENDL;

    if (mViolationCounter)
    {
        ++*mViolationCounter;
    }

    // Debug builds stop at the leak. Release builds count it and continue/drop.
    llassert(false);
}

// ===========================================================================
// [GhostDeferred] G-buffer OFF-vs-ON contamination test (P0 slice 4).
// ===========================================================================
namespace
{
constexpr U32 GHOST_STABLE_FRAMES = 3;
constexpr F32 GHOST_CLIP_EPSILON = 1.e-6f;

EGhostDeferredSubmissionOverride sGhostSubmissionOverride =
    EGhostDeferredSubmissionOverride::NONE;

enum class EContaminationPhase : U8
{
    IDLE,
    WAIT_STABLE,
    CAPTURE_OFF,
    CAPTURE_ON,
    COMPARE
};

enum class ESampleKind : U8
{
    U8_NORMALIZED,
    U16_NORMALIZED,
    PACKED_2_10_10_10,
    FLOAT32
};

struct GhostAttachmentCapture
{
    std::string mName;
    GLenum mAttachment = GL_NONE;
    GLenum mInternalFormat = GL_NONE;
    GLenum mReadFormat = GL_NONE;
    GLenum mReadType = GL_NONE;
    ESampleKind mKind = ESampleKind::U8_NORMALIZED;
    U32 mComponents = 0;
    U32 mBytesPerPixel = 0;
    std::vector<U8> mBytes;
};

struct GhostProxyMetadata
{
    LLUUID mInstanceId;
    LLVector3 mFootAgent;
    LLQuaternion mRotation;
    F32 mScale = 1.f;
    LLVector3 mPivotFootAgent;
    LLVector3 mBoundsCenter;
    LLVector3 mBoundsHalfExtent;
    bool mVisible = false;
};

struct GhostCapture
{
    const LLCamera* mCamera = nullptr;
    S32 mCameraId = 0;
    U32 mViewStamp = 0;
    U32 mQueueViewStamp = 0;
    const LLCamera* mQueueCamera = nullptr;

    std::array<GLint, 4> mViewport{};
    glm::mat4 mModelview{1.f};
    glm::mat4 mProjection{1.f};

    U32 mWidth = 0;
    U32 mHeight = 0;
    U32 mColorCount = 0;

    std::vector<GLenum> mInternalFormats;
    std::vector<GhostProxyMetadata> mProxies;
    std::vector<GhostAttachmentCapture> mAttachments;

    U64 mDrawCalls = 0;   // clone draw calls issued in the captured frame (ON must be > 0)
    bool mReadOK = false;
    std::string mReadError;
};

bool exact_matrix_equal(const glm::mat4& lhs, const glm::mat4& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(glm::mat4)) == 0;
}

bool exact_vec3_equal(const LLVector3& lhs, const LLVector3& rhs)
{
    return std::memcmp(lhs.mV, rhs.mV, sizeof(lhs.mV)) == 0;
}

bool exact_quat_equal(const LLQuaternion& lhs, const LLQuaternion& rhs)
{
    return std::memcmp(lhs.mQ, rhs.mQ, sizeof(lhs.mQ)) == 0;
}

GhostProxyMetadata proxy_metadata(const LLActorMover::GhostProxy& proxy)
{
    GhostProxyMetadata result;
    result.mInstanceId = proxy.mInstanceId;
    result.mFootAgent = proxy.mFootAgent;
    result.mRotation = proxy.mRotation;
    result.mScale = proxy.mScale;
    result.mPivotFootAgent = proxy.mPivotFootAgent;
    result.mBoundsCenter = proxy.mWorldBoundsCenter;
    result.mBoundsHalfExtent = proxy.mWorldBoundsHalfExtent;
    result.mVisible = proxy.mFrustumVisible;
    return result;
}

bool proxy_equal(const GhostProxyMetadata& lhs,
                 const GhostProxyMetadata& rhs)
{
    return lhs.mInstanceId == rhs.mInstanceId
        && exact_vec3_equal(lhs.mFootAgent, rhs.mFootAgent)
        && exact_quat_equal(lhs.mRotation, rhs.mRotation)
        && lhs.mScale == rhs.mScale
        && exact_vec3_equal(lhs.mPivotFootAgent, rhs.mPivotFootAgent)
        && exact_vec3_equal(lhs.mBoundsCenter, rhs.mBoundsCenter)
        && exact_vec3_equal(lhs.mBoundsHalfExtent, rhs.mBoundsHalfExtent)
        && lhs.mVisible == rhs.mVisible;
}

bool proxy_vectors_equal(const std::vector<GhostProxyMetadata>& lhs,
                         const std::vector<GhostProxyMetadata>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (U32 i = 0; i < lhs.size(); ++i)
    {
        if (!proxy_equal(lhs[i], rhs[i]))
        {
            return false;
        }
    }
    return true;
}

void capture_metadata(const LLCamera& camera,
                      U32 view_stamp,
                      LLRenderTarget& target,
                      GhostCapture& out)
{
    out.mCamera = &camera;
    out.mCameraId = static_cast<S32>(LLViewerCamera::sCurCameraID);
    out.mViewStamp = view_stamp;
    out.mModelview = gGL.getModelviewMatrix();
    out.mProjection = gGL.getProjectionMatrix();
    glGetIntegerv(GL_VIEWPORT, out.mViewport.data());

    out.mWidth = target.getWidth();
    out.mHeight = target.getHeight();
    out.mColorCount = target.getNumTextures();

    out.mInternalFormats.clear();
    for (U32 i = 0; i < out.mColorCount; ++i)
    {
        out.mInternalFormats.push_back(
            static_cast<GLenum>(target.getInternalFormat(i)));
    }

    const LLActorMover::GhostProxyQueue& queue =
        LLActorMover::instance().getGhostDeferredQueue(camera, view_stamp);

    out.mQueueViewStamp = queue.mViewStamp;
    out.mQueueCamera = queue.mCameraIdentity;
    out.mProxies.clear();
    out.mProxies.reserve(queue.mProxies.size());

    for (const LLActorMover::GhostProxy& proxy : queue.mProxies)
    {
        if (proxy.mFrustumVisible)
        {
            out.mProxies.push_back(proxy_metadata(proxy));
        }
    }
}

bool metadata_equal(const GhostCapture& lhs,
                    const GhostCapture& rhs,
                    std::string& reason)
{
#define GHOST_METADATA_REQUIRE(condition, text) \
    do { if (!(condition)) { reason = text; return false; } } while (false)

    GHOST_METADATA_REQUIRE(lhs.mCamera == rhs.mCamera,
                           "camera address changed");
    GHOST_METADATA_REQUIRE(lhs.mCameraId == rhs.mCameraId,
                           "sCurCameraID changed");
    GHOST_METADATA_REQUIRE(lhs.mViewStamp == rhs.mViewStamp,
                           "view stamp changed");
    GHOST_METADATA_REQUIRE(lhs.mQueueViewStamp == rhs.mQueueViewStamp,
                           "queue view stamp changed");
    GHOST_METADATA_REQUIRE(lhs.mQueueCamera == rhs.mQueueCamera,
                           "queue camera identity changed");
    GHOST_METADATA_REQUIRE(lhs.mViewport == rhs.mViewport,
                           "viewport changed");
    GHOST_METADATA_REQUIRE(exact_matrix_equal(lhs.mModelview,
                                              rhs.mModelview),
                           "modelview changed");
    GHOST_METADATA_REQUIRE(exact_matrix_equal(lhs.mProjection,
                                              rhs.mProjection),
                           "projection changed");
    GHOST_METADATA_REQUIRE(lhs.mWidth == rhs.mWidth
                           && lhs.mHeight == rhs.mHeight,
                           "render-target dimensions changed");
    GHOST_METADATA_REQUIRE(lhs.mColorCount == rhs.mColorCount,
                           "attachment count changed");
    GHOST_METADATA_REQUIRE(lhs.mInternalFormats == rhs.mInternalFormats,
                           "attachment formats changed");
    GHOST_METADATA_REQUIRE(proxy_vectors_equal(lhs.mProxies, rhs.mProxies),
                           "visible proxy transforms or bounds changed");

#undef GHOST_METADATA_REQUIRE
    return true;
}

bool describe_color_attachment(U32 index,
                               GLenum internal_format,
                               GhostAttachmentCapture& out)
{
    out.mName = "A" + std::to_string(index);
    out.mAttachment = GL_COLOR_ATTACHMENT0 + index;
    out.mInternalFormat = internal_format;

    switch (internal_format)
    {
    case GL_SRGB8_ALPHA8:
    case GL_RGBA8:
    case GL_RGBA:
        out.mReadFormat = GL_RGBA;
        out.mReadType = GL_UNSIGNED_BYTE;
        out.mKind = ESampleKind::U8_NORMALIZED;
        out.mComponents = 4;
        out.mBytesPerPixel = 4;
        return true;

    case GL_RGB8:
    case GL_RGB:
        out.mReadFormat = GL_RGB;
        out.mReadType = GL_UNSIGNED_BYTE;
        out.mKind = ESampleKind::U8_NORMALIZED;
        out.mComponents = 3;
        out.mBytesPerPixel = 3;
        return true;

    case GL_RGBA16:
        out.mReadFormat = GL_RGBA;
        out.mReadType = GL_UNSIGNED_SHORT;
        out.mKind = ESampleKind::U16_NORMALIZED;
        out.mComponents = 4;
        out.mBytesPerPixel = 8;
        return true;

    case GL_RGB10_A2:
        out.mReadFormat = GL_RGBA;
        out.mReadType = GL_UNSIGNED_INT_2_10_10_10_REV;
        out.mKind = ESampleKind::PACKED_2_10_10_10;
        out.mComponents = 1;
        out.mBytesPerPixel = 4;
        return true;

    case GL_RGB16F:
        out.mReadFormat = GL_RGB;
        out.mReadType = GL_FLOAT;
        out.mKind = ESampleKind::FLOAT32;
        out.mComponents = 3;
        out.mBytesPerPixel = 12;
        return true;

    case GL_RGBA16F:
        out.mReadFormat = GL_RGBA;
        out.mReadType = GL_FLOAT;
        out.mKind = ESampleKind::FLOAT32;
        out.mComponents = 4;
        out.mBytesPerPixel = 16;
        return true;

    default:
        return false;
    }
}

bool read_deferred_target(LLRenderTarget& target, GhostCapture& out)
{
    GLint saved_read_fbo = 0;
    GLint saved_read_buffer = GL_NONE;
    GLint saved_pack_alignment = 4;

    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saved_read_fbo);
    glGetIntegerv(GL_READ_BUFFER, &saved_read_buffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_pack_alignment);

    while (glGetError() != GL_NO_ERROR)
    {
        // Clear pre-existing errors so readback reports only its own failure.
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFBO());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    out.mAttachments.clear();

    for (U32 i = 0; i < target.getNumTextures(); ++i)
    {
        GhostAttachmentCapture attachment;
        const GLenum internal_format =
            static_cast<GLenum>(target.getInternalFormat(i));

        if (!describe_color_attachment(i, internal_format, attachment))
        {
            out.mReadError =
                "unsupported color internal format 0x"
                + scalar_string(static_cast<U32>(internal_format));
            goto restore;
        }

        attachment.mBytes.resize(
            static_cast<size_t>(target.getWidth())
            * target.getHeight()
            * attachment.mBytesPerPixel);

        glReadBuffer(attachment.mAttachment);
        glReadPixels(0, 0,
                     static_cast<GLsizei>(target.getWidth()),
                     static_cast<GLsizei>(target.getHeight()),
                     attachment.mReadFormat,
                     attachment.mReadType,
                     attachment.mBytes.data());

        if (glGetError() != GL_NO_ERROR)
        {
            out.mReadError =
                "glReadPixels failed for " + attachment.mName;
            goto restore;
        }

        out.mAttachments.push_back(std::move(attachment));
    }

    {
        GhostAttachmentCapture depth;
        depth.mName = "depth";
        depth.mAttachment = GL_DEPTH_ATTACHMENT;
        // Read format (getDepthFormat() is LLRenderTarget's 0/1 enum, not a GL
        // internal format; both captures share it so the layout check is unaffected).
        depth.mInternalFormat = GL_DEPTH_COMPONENT;
        depth.mReadFormat = GL_DEPTH_COMPONENT;
        depth.mReadType = GL_FLOAT;
        depth.mKind = ESampleKind::FLOAT32;
        depth.mComponents = 1;
        depth.mBytesPerPixel = 4;
        depth.mBytes.resize(
            static_cast<size_t>(target.getWidth())
            * target.getHeight() * sizeof(F32));

        glReadPixels(0, 0,
                     static_cast<GLsizei>(target.getWidth()),
                     static_cast<GLsizei>(target.getHeight()),
                     GL_DEPTH_COMPONENT, GL_FLOAT,
                     depth.mBytes.data());

        if (glGetError() != GL_NO_ERROR)
        {
            out.mReadError = "glReadPixels failed for depth";
            goto restore;
        }

        out.mAttachments.push_back(std::move(depth));
    }

    out.mReadOK = true;

restore:
    glPixelStorei(GL_PACK_ALIGNMENT, saved_pack_alignment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(saved_read_fbo));
    glReadBuffer(static_cast<GLenum>(saved_read_buffer));

    return out.mReadOK;
}

bool capture_frame(const LLCamera& camera,
                   U32 view_stamp,
                   LLRenderTarget& target,
                   GhostCapture& out)
{
    out = GhostCapture();
    capture_metadata(camera, view_stamp, target, out);
    return read_deferred_target(target, out);
}

std::vector<U8> build_clone_mask(const GhostCapture& capture,
                                S32 pad,
                                bool& trustworthy)
{
    const U32 width = capture.mWidth;
    const U32 height = capture.mHeight;
    std::vector<U8> mask(static_cast<size_t>(width) * height, 0);
    trustworthy = true;

    static constexpr U8 EDGES[12][2] =
    {
        {0,1}, {0,2}, {0,4}, {1,3}, {1,5}, {2,3},
        {2,6}, {3,7}, {4,5}, {4,6}, {5,7}, {6,7}
    };

    const glm::mat4 clip_from_agent =
        capture.mProjection * capture.mModelview;

    for (const GhostProxyMetadata& proxy : capture.mProxies)
    {
        glm::vec4 corners[8];

        for (U32 i = 0; i < 8; ++i)
        {
            const F32 x = proxy.mBoundsCenter.mV[0]
                + ((i & 1) ? 1.f : -1.f)
                    * proxy.mBoundsHalfExtent.mV[0];
            const F32 y = proxy.mBoundsCenter.mV[1]
                + ((i & 2) ? 1.f : -1.f)
                    * proxy.mBoundsHalfExtent.mV[1];
            const F32 z = proxy.mBoundsCenter.mV[2]
                + ((i & 4) ? 1.f : -1.f)
                    * proxy.mBoundsHalfExtent.mV[2];

            corners[i] = clip_from_agent * glm::vec4(x, y, z, 1.f);
        }

        std::vector<glm::vec4> clipped;
        clipped.reserve(32);

        for (const glm::vec4& corner : corners)
        {
            if (corner.z >= -corner.w && corner.w > GHOST_CLIP_EPSILON)
            {
                clipped.push_back(corner);
            }
        }

        bool crossed_near = false;

        for (const auto& edge : EDGES)
        {
            const glm::vec4& a = corners[edge[0]];
            const glm::vec4& b = corners[edge[1]];
            const F32 da = a.z + a.w;
            const F32 db = b.z + b.w;

            if ((da < 0.f) != (db < 0.f))
            {
                crossed_near = true;
                const F32 denominator = da - db;

                if (!std::isfinite(denominator)
                    || std::fabs(denominator) <= GHOST_CLIP_EPSILON)
                {
                    std::fill(mask.begin(), mask.end(), 1);
                    trustworthy = false;
                    return mask;
                }

                const F32 t = da / denominator;
                const glm::vec4 point = a + t * (b - a);

                if (!std::isfinite(point.x)
                    || !std::isfinite(point.y)
                    || !std::isfinite(point.w)
                    || point.w <= GHOST_CLIP_EPSILON)
                {
                    std::fill(mask.begin(), mask.end(), 1);
                    trustworthy = false;
                    return mask;
                }

                clipped.push_back(point);
            }
        }

        if (clipped.empty())
        {
            // Completely behind the near plane or outside the clip volume.
            // A frustum-visible proxy with no trustworthy projected points must
            // never be under-masked.
            if (proxy.mVisible)
            {
                std::fill(mask.begin(), mask.end(), 1);
                trustworthy = false;
                return mask;
            }
            continue;
        }

        F32 min_x = std::numeric_limits<F32>::infinity();
        F32 min_y = std::numeric_limits<F32>::infinity();
        F32 max_x = -std::numeric_limits<F32>::infinity();
        F32 max_y = -std::numeric_limits<F32>::infinity();

        for (const glm::vec4& point : clipped)
        {
            if (point.w <= GHOST_CLIP_EPSILON)
            {
                std::fill(mask.begin(), mask.end(), 1);
                trustworthy = false;
                return mask;
            }

            const F32 ndc_x = point.x / point.w;
            const F32 ndc_y = point.y / point.w;

            if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y))
            {
                std::fill(mask.begin(), mask.end(), 1);
                trustworthy = false;
                return mask;
            }

            const F32 pixel_x = capture.mViewport[0]
                + (ndc_x * 0.5f + 0.5f) * capture.mViewport[2];
            const F32 pixel_y = capture.mViewport[1]
                + (ndc_y * 0.5f + 0.5f) * capture.mViewport[3];

            min_x = std::min(min_x, pixel_x);
            min_y = std::min(min_y, pixel_y);
            max_x = std::max(max_x, pixel_x);
            max_y = std::max(max_y, pixel_y);
        }

        // Near-plane crossing is safe only because every crossing edge was
        // clipped above. Any failure already promoted the mask to full-screen.
        (void)crossed_near;

        // Guard the float->int conversion: an out-of-range cast is UB and could
        // undermine the never-under-mask guarantee. Any wild coordinate -> full mask.
        constexpr F32 GHOST_PIXEL_LIMIT = 1.e6f;
        if (min_x < -GHOST_PIXEL_LIMIT || max_x > GHOST_PIXEL_LIMIT
            || min_y < -GHOST_PIXEL_LIMIT || max_y > GHOST_PIXEL_LIMIT)
        {
            std::fill(mask.begin(), mask.end(), 1);
            trustworthy = false;
            return mask;
        }

        S32 x0 = static_cast<S32>(std::floor(min_x)) - pad;
        S32 y0 = static_cast<S32>(std::floor(min_y)) - pad;
        S32 x1 = static_cast<S32>(std::ceil(max_x)) + pad;
        S32 y1 = static_cast<S32>(std::ceil(max_y)) + pad;

        x0 = llclamp(x0, 0, static_cast<S32>(width));
        y0 = llclamp(y0, 0, static_cast<S32>(height));
        x1 = llclamp(x1, 0, static_cast<S32>(width));
        y1 = llclamp(y1, 0, static_cast<S32>(height));

        for (S32 y = y0; y < y1; ++y)
        {
            for (S32 x = x0; x < x1; ++x)
            {
                mask[static_cast<size_t>(y) * width + x] = 1;
            }
        }
    }

    return mask;
}

struct GhostDiffResult
{
    U64 mPixelsCompared = 0;
    U64 mPixelsDifferent = 0;
    U64 mComponentsDifferent = 0;
    bool mHasFirst = false;
    U32 mFirstX = 0;
    U32 mFirstY = 0;
    F64 mMaxDifference = 0.0;
};

template<typename T>
void compare_typed(const GhostAttachmentCapture& off,
                   const GhostAttachmentCapture& on,
                   U32 width,
                   U32 height,
                   const std::vector<U8>* exclusion_mask,
                   F64 epsilon,
                   GhostDiffResult& result)
{
    // std::vector<U8> guarantees only byte alignment, so load each sample via
    // memcpy rather than reinterpret_cast (which would be unaligned access +
    // aliasing UB). Bit-exact for the U32 float-bit-pattern path.
    const U8* a_bytes = off.mBytes.data();
    const U8* b_bytes = on.mBytes.data();
    const U32 components = off.mComponents;

    for (U32 y = 0; y < height; ++y)
    {
        for (U32 x = 0; x < width; ++x)
        {
            const size_t pixel = static_cast<size_t>(y) * width + x;

            if (exclusion_mask && (*exclusion_mask)[pixel])
            {
                continue;
            }

            ++result.mPixelsCompared;
            bool pixel_differs = false;

            for (U32 c = 0; c < components; ++c)
            {
                const size_t sample = pixel * components + c;
                T av_raw;
                T bv_raw;
                std::memcpy(&av_raw, a_bytes + sample * sizeof(T), sizeof(T));
                std::memcpy(&bv_raw, b_bytes + sample * sizeof(T), sizeof(T));
                const F64 av = static_cast<F64>(av_raw);
                const F64 bv = static_cast<F64>(bv_raw);
                const F64 difference = std::fabs(av - bv);

                result.mMaxDifference =
                    std::max(result.mMaxDifference, difference);

                if (difference > epsilon)
                {
                    ++result.mComponentsDifferent;
                    pixel_differs = true;
                }
            }

            if (pixel_differs)
            {
                ++result.mPixelsDifferent;
                if (!result.mHasFirst)
                {
                    result.mHasFirst = true;
                    result.mFirstX = x;
                    result.mFirstY = y;
                }
            }
        }
    }
}

GhostDiffResult compare_attachment(
    const GhostAttachmentCapture& off,
    const GhostAttachmentCapture& on,
    U32 width,
    U32 height,
    const std::vector<U8>* exclusion_mask)
{
    GhostDiffResult result;

    switch (off.mKind)
    {
    case ESampleKind::U8_NORMALIZED:
        compare_typed<U8>(off, on, width, height,
                          exclusion_mask, 0.0, result);
        break;

    case ESampleKind::U16_NORMALIZED:
        compare_typed<U16>(off, on, width, height,
                           exclusion_mask, 0.0, result);
        break;

    case ESampleKind::PACKED_2_10_10_10:
        compare_typed<U32>(off, on, width, height,
                           exclusion_mask, 0.0, result);
        break;

    case ESampleKind::FLOAT32:
        // Bitwise-exact: reinterpret the float samples as U32 and compare bit
        // patterns (epsilon 0). A fabs float compare would miss NaN differences
        // and treat +0.0/-0.0 as equal; the same content read twice is bit-identical.
        compare_typed<U32>(off, on, width, height,
                           exclusion_mask,
                           0.0, result);
        break;
    }

    return result;
}

void log_diff(const std::string& name,
              const char* scope,
              const GhostDiffResult& diff)
{
    LL_INFOS("GhostDeferredContamination")
        << name << ' ' << scope
        << " pixels-compared=" << diff.mPixelsCompared
        << " pixels-different=" << diff.mPixelsDifferent
        << " components-different=" << diff.mComponentsDifferent
        << " first-diff="
        << (diff.mHasFirst
                ? std::to_string(diff.mFirstX) + ","
                    + std::to_string(diff.mFirstY)
                : std::string("none"))
        << " max-diff=" << diff.mMaxDifference
        << LL_ENDL;
}

void terminal_verdict(const char* verdict, const std::string& reason)
{
    LL_INFOS("GhostDeferredContamination")
        << "GhostDeferred contamination verdict: "
        << verdict << " - " << reason
        << LL_ENDL;
}
} // anonymous namespace

EGhostDeferredSubmissionOverride getGhostDeferredSubmissionOverride()
{
    return sGhostSubmissionOverride;
}

bool ghostDeferredSubmissionEnabled(bool user_enabled)
{
    switch (sGhostSubmissionOverride)
    {
    case EGhostDeferredSubmissionOverride::FORCE_OFF:
        return false;
    case EGhostDeferredSubmissionOverride::FORCE_ON:
        return true;
    case EGhostDeferredSubmissionOverride::NONE:
    default:
        return user_enabled;
    }
}

struct LLGhostDeferredContaminationTest::Impl
{
    EContaminationPhase mPhase = EContaminationPhase::IDLE;
    U32 mStableFrames = 0;
    GhostCapture mStable;
    GhostCapture mOff;
    GhostCapture mOn;

    void reset()
    {
        mPhase = EContaminationPhase::IDLE;
        mStableFrames = 0;
        mStable = GhostCapture();
        mOff = GhostCapture();
        mOn = GhostCapture();
        sGhostSubmissionOverride =
            EGhostDeferredSubmissionOverride::NONE;
        gSavedSettings.setS32("GhostDeferredContaminationTest", 0);
    }

    void inconclusive(const std::string& reason)
    {
        terminal_verdict("INCONCLUSIVE", reason);
        ++LLActorMover::instance().ghostDeferredCounters()
              .mContaminationInconclusive;
        reset();
    }

    void compare()
    {
        std::string reason;

        if (!mOff.mReadOK || !mOn.mReadOK)
        {
            inconclusive(!mOff.mReadOK
                ? mOff.mReadError : mOn.mReadError);
            return;
        }

        if (!metadata_equal(mOff, mOn, reason))
        {
            inconclusive(reason);
            return;
        }

        if (mOff.mAttachments.size() != mOn.mAttachments.size())
        {
            inconclusive("captured attachment count changed");
            return;
        }

        if (mOn.mDrawCalls == 0)
        {
            inconclusive("ON frame submitted zero clone draws -- OFF==ON proves nothing");
            return;
        }

        bool trustworthy = false;
        static LLCachedControl<S32> mask_pad(
            gSavedSettings, "GhostDeferredDiffMaskPad", 2);

        const std::vector<U8> mask =
            build_clone_mask(mOff, llclamp(mask_pad(), 2, 4096), trustworthy);

        if (!trustworthy)
        {
            inconclusive("clone bounds crossed the near plane without a trustworthy mask");
            return;
        }

        U64 outside_pixels_different = 0;
        U64 outside_pixels_compared = 0;

        for (U32 i = 0; i < mOff.mAttachments.size(); ++i)
        {
            const GhostAttachmentCapture& off = mOff.mAttachments[i];
            const GhostAttachmentCapture& on = mOn.mAttachments[i];

            if (off.mName != on.mName
                || off.mInternalFormat != on.mInternalFormat
                || off.mReadFormat != on.mReadFormat
                || off.mReadType != on.mReadType
                || off.mComponents != on.mComponents
                || off.mBytes.size() != on.mBytes.size())
            {
                inconclusive("readback attachment layout changed");
                return;
            }

            const GhostDiffResult whole =
                compare_attachment(off, on, mOff.mWidth, mOff.mHeight,
                                   nullptr);
            const GhostDiffResult outside =
                compare_attachment(off, on, mOff.mWidth, mOff.mHeight,
                                   &mask);

            log_diff(off.mName, "whole", whole);
            log_diff(off.mName, "outside-mask", outside);

            outside_pixels_different += outside.mPixelsDifferent;
            outside_pixels_compared += outside.mPixelsCompared;
        }

        if (outside_pixels_compared == 0)
        {
            inconclusive("clone mask covered the entire viewport");
            return;
        }

        if (outside_pixels_different == 0)
        {
            terminal_verdict(
                "PASS",
                "all compared pixels outside clone bounds are identical");
            ++LLActorMover::instance().ghostDeferredCounters()
                  .mContaminationPass;
            reset();
            return;
        }

        // A widespread frame-to-frame change is more plausibly changing world
        // content than a clone-local write escaping its bounds. Never call that
        // a proven renderer failure.
        const U64 broad_threshold =
            llmax<U64>(64, outside_pixels_compared / 100);

        if (outside_pixels_different >= broad_threshold)
        {
            inconclusive(
                "broad outside-mask change indicates unstable world content");
            return;
        }

        terminal_verdict(
            "FAIL",
            std::to_string(outside_pixels_different)
                + " outside-mask pixels changed");
        ++LLActorMover::instance().ghostDeferredCounters()
              .mContaminationFail;
        reset();
    }
};

LLGhostDeferredContaminationTest&
LLGhostDeferredContaminationTest::instance()
{
    static LLGhostDeferredContaminationTest test;
    return test;
}

LLGhostDeferredContaminationTest::
LLGhostDeferredContaminationTest()
: mImpl(std::make_unique<Impl>())
{
}

LLGhostDeferredContaminationTest::
~LLGhostDeferredContaminationTest() = default;

void LLGhostDeferredContaminationTest::tick(
    const LLCamera& camera,
    U32 view_stamp,
    LLRenderTarget& deferred_screen)
{
    const S32 command =
        gSavedSettings.getS32("GhostDeferredContaminationTest");

    if (command == 2)
    {
        if (mImpl->mPhase != EContaminationPhase::IDLE)
        {
            terminal_verdict("INCONCLUSIVE", "cancelled");
        }
        mImpl->reset();
        return;
    }

    if (mImpl->mPhase == EContaminationPhase::IDLE)
    {
        if (command != 1)
        {
            return;
        }

        mImpl->mPhase = EContaminationPhase::WAIT_STABLE;
        mImpl->mStableFrames = 0;
        sGhostSubmissionOverride =
            EGhostDeferredSubmissionOverride::NONE;
    }

    switch (mImpl->mPhase)
    {
    case EContaminationPhase::WAIT_STABLE:
    {
        GhostCapture current;
        capture_metadata(camera, view_stamp, deferred_screen, current);

        std::string unused;
        if (mImpl->mStableFrames == 0
            || metadata_equal(mImpl->mStable, current, unused))
        {
            mImpl->mStable = std::move(current);
            ++mImpl->mStableFrames;
        }
        else
        {
            mImpl->mStable = std::move(current);
            mImpl->mStableFrames = 1;
        }

        if (mImpl->mStableFrames >= GHOST_STABLE_FRAMES)
        {
            // This tick occurs after today's draw. FORCE_OFF therefore applies
            // to the next display() call, whose post-draw tick captures OFF.
            sGhostSubmissionOverride =
                EGhostDeferredSubmissionOverride::FORCE_OFF;
            mImpl->mPhase = EContaminationPhase::CAPTURE_OFF;
        }
        break;
    }

    case EContaminationPhase::CAPTURE_OFF:
        if (!capture_frame(camera, view_stamp,
                           deferred_screen, mImpl->mOff))
        {
            mImpl->inconclusive(mImpl->mOff.mReadError);
            return;
        }

        // Applies to the immediately following display() call.
        sGhostSubmissionOverride =
            EGhostDeferredSubmissionOverride::FORCE_ON;
        mImpl->mPhase = EContaminationPhase::CAPTURE_ON;
        break;

    case EContaminationPhase::CAPTURE_ON:
        if (!capture_frame(camera, view_stamp,
                           deferred_screen, mImpl->mOn))
        {
            mImpl->inconclusive(mImpl->mOn.mReadError);
            return;
        }

        // Prove the ON frame actually submitted clone draws -- otherwise OFF==ON is
        // trivially true and a PASS would prove nothing. Read the counter now, in
        // the same tick, before the next frame's buildGhostDeferredQueue resets it.
        mImpl->mOn.mDrawCalls =
            LLActorMover::instance().ghostDeferredCounters().mActualDrawCalls;

        // Restore normal user-controlled submission before comparison.
        sGhostSubmissionOverride =
            EGhostDeferredSubmissionOverride::NONE;
        mImpl->mPhase = EContaminationPhase::COMPARE;
        [[fallthrough]];

    case EContaminationPhase::COMPARE:
        mImpl->compare();
        break;

    case EContaminationPhase::IDLE:
        break;
    }
}
