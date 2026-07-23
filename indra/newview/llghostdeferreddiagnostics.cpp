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

#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

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
