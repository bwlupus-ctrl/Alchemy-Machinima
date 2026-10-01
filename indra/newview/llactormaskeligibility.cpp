// [ActorMask]
#include "llviewerprecompiledheaders.h"
#include "llactormaskeligibility.h"
#include "alcinelightrigmanager.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llgl.h"
#include "pipeline.h"
#include "llglslshader.h"
#include <algorithm>
bool LLActorMaskViewStamp::operator==(const LLActorMaskViewStamp& r) const
{
    return frame == r.frame && pack == r.pack && scratch == r.scratch &&
        cube == r.cube && mirror == r.mirror && prism == r.prism;
}
bool LLActorMaskEligibility::sArmed = false;
LLActorMaskEligibility& LLActorMaskEligibility::instance()
{
    static LLActorMaskEligibility recorder;
    return recorder;
}
LLActorMaskEligibility::Scope::Scope(const LLActorMaskViewStamp& stamp)
{
    auto& r = instance();
    r.mStamp = stamp;
    r.mDraws.clear();
    r.mBodies.clear();
    r.mCoverageBits = 0;
    sArmed = true;
}
LLActorMaskEligibility::Scope::~Scope() { sArmed = false; }
// [ActorMask] Called only under the native shader while the G-buffer recorder is armed.
void LLActorMaskEligibility::coverageChanged(const LLUUID& owner)
{
    mCoverageBits = static_cast<U8>(mCoverageBits | ALCineLightRigManager::instance().byteForOwner(owner));
}
void LLActorMaskEligibility::coverageChanged(const LLDrawInfo& draw)
{
    mCoverageBits = static_cast<U8>(mCoverageBits | ALCineLightRigManager::instance().drawByte(draw));
}
bool LLActorMaskQueries::begin(const LLActorMaskQueryTag& tag)
{
    llassert(!mActive);
    // Shader profiling brackets GL_SAMPLES_PASSED across program binds. Never
    // nest our query inside its query or let its next bind end ours.
    if (LLGLSLShader::sProfileEnabled) return false;
    for (auto& e : mRing[tag.frame % 3])
    {
        if (e.pending) continue;
        if (!e.id) glGenQueries(1, &e.id);
        if (!e.id) return false;
        e.tag = tag;
        mActive = &e;
        glBeginQuery(GL_SAMPLES_PASSED, e.id);
        return true;
    }
    return false;
}
void LLActorMaskQueries::end(U32 draws, U32 skipped)
{
    if (!mActive) return;
    glEndQuery(GL_SAMPLES_PASSED);
    mActive->tag.drawsIssued = draws;
    mActive->tag.batchesSkippedLikeNative = skipped; // [ActorMask] Captured with this query.
    mActive->pending = true;
    mActive = nullptr;
}
void LLActorMaskQueries::poll()
{
    const auto& registry = ALCineLightRigManager::instance();
    for (auto& frame : mRing) for (auto& e : frame)
    {
        if (!e.pending) continue;
        U32 available = 0;
        glGetQueryObjectuiv(e.id, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) continue;
        U32 samples = 0;
        glGetQueryObjectuiv(e.id, GL_QUERY_RESULT, &samples);
        e.pending = false;
        for (const auto& r : registry.actorOnlyRecipients())
        {
            if (r.mAvatarId == e.tag.recipient && r.mBit == e.tag.recipientBit &&
                r.mAvatar.notNull() && !r.mAvatar->isDead())
            {
                if (mResults.size() >= 576) mResults.erase(mResults.begin());
                mResults.push_back({ e.tag, samples });
                break;
            }
        }
    }
}
void LLActorMaskQueries::release()
{
    llassert(!mActive);
    for (auto& frame : mRing) for (auto& e : frame)
    {
        if (e.id) glDeleteQueries(1, &e.id);
        e = Entry{};
    }
    mResults.clear();
}
std::vector<LLActorMaskQueryResult> LLActorMaskQueries::takeResults()
{
    std::vector<LLActorMaskQueryResult> result;
    result.swap(mResults);
    const auto& live = ALCineLightRigManager::instance().actorOnlyRecipients();
    result.erase(std::remove_if(result.begin(), result.end(), [&](const LLActorMaskQueryResult& q)
    {
        return std::none_of(live.begin(), live.end(), [&](const auto& r)
        {
            return r.mAvatarId == q.tag.recipient && r.mBit == q.tag.recipientBit &&
                r.mAvatar.notNull() && !r.mAvatar->isDead();
        });
    }), result.end());
    return result;
}
LLActorMaskTextureState::LLActorMaskTextureState(LLTexUnit& unit) : LLTexUnit(unit)
{
    unit.activate();
    glActiveTexture(GL_TEXTURE0 + unit.getIndex()); // [ActorMask] Also handle a raw/cache selector mismatch.
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &mBinding2D);
    // [ActorMask] LLTexUnit::enable may unbind the previously enabled target.
    const GLenum bindings[] = { GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_RECTANGLE,
        GL_TEXTURE_BINDING_CUBE_MAP, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY,
        GL_TEXTURE_BINDING_2D_MULTISAMPLE, GL_TEXTURE_BINDING_3D };
    static_assert(sizeof(bindings) / sizeof(bindings[0]) == LLTexUnit::TT_NONE,
                  "ActorMask texture target bindings");
    if (mCurrTexType != TT_NONE && mCurrTexType != TT_TEXTURE)
        glGetIntegerv(bindings[mCurrTexType], &mBindingOriginal);
}
void LLActorMaskTextureState::restore(LLTexUnit& unit) const
{
    unit.activate();
    glActiveTexture(GL_TEXTURE0 + unit.getIndex());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(mBinding2D));
    if (mCurrTexType != TT_NONE && mCurrTexType != TT_TEXTURE)
        glBindTexture(getInternalType(mCurrTexType), static_cast<GLuint>(mBindingOriginal));
    unit = static_cast<const LLTexUnit&>(*this); // [ActorMask] Includes TT_NONE and exact mip state.
}
LLScopedActorMaskInvariant::State LLScopedActorMaskInvariant::capture()
{
    State s;
    const GLenum singles[] = { GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING,
        GL_CURRENT_PROGRAM, GL_DEPTH_FUNC, GL_DEPTH_WRITEMASK, GL_BLEND_SRC_RGB,
        GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_CULL_FACE_MODE };
    size_t i = 0;
    for (GLenum key : singles) glGetIntegerv(key, &s.ints[i++]);
    const GLenum enables[] = { GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE,
        GL_POLYGON_OFFSET_FILL, GL_SCISSOR_TEST, GL_COLOR_LOGIC_OP };
    for (GLenum key : enables) s.ints[i++] = glIsEnabled(key);
    glGetIntegerv(GL_VIEWPORT, &s.ints[i]); i += 4;
    glGetIntegerv(GL_SCISSOR_BOX, &s.ints[i]); i += 4;
    glGetIntegerv(GL_COLOR_WRITEMASK, &s.ints[i]); i += 4;
    for (U32 b = 0; b < 4; ++b) glGetIntegerv(GL_DRAW_BUFFER0 + b, &s.ints[i++]);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, s.floats.data());
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &s.floats[4]);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &s.floats[5]);
    const GLenum extra[] = { GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA,
        GL_FRONT_FACE, GL_ACTIVE_TEXTURE, GL_ARRAY_BUFFER_BINDING,
        GL_ELEMENT_ARRAY_BUFFER_BINDING };
    for (GLenum key : extra) glGetIntegerv(key, &s.ints[i++]);
    s.ints[i++] = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    s.ints[i++] = static_cast<S32>(gGL.getMatrixMode());
    s.ints[i++] = static_cast<S32>(LLRenderTarget::sCurFBO);
    s.ints[i++] = static_cast<S32>(gGL.getCurrentTexUnitIndex());
    memcpy(&s.floats[6], &gGL.getModelviewMatrix(), sizeof(glm::mat4));
    for (U32 t = 0; t < 4; ++t)
        memcpy(&s.floats[22 + 16*t], &gGL.getMatrix(
            static_cast<LLRender::eMatrixMode>(LLRender::MM_TEXTURE0 + t)), sizeof(glm::mat4));
    s.pointers = { gGLLastMatrix, LLGLSLShader::sCurBoundShaderPtr,
        LLRenderTarget::getCurrentBoundTarget() };
    // [ActorMask] Observe the same texture state that the pass restores.
    const U32 unit = gGL.getCurrentTexUnitIndex();
    GLint active = 0, count = 1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    for (U32 t = 0; t < LL_NUM_TEXTURE_LAYERS; ++t)
        s.textures.push_back(LLActorMaskTextureState(*gGL.getTexUnit(t)).signature());
    gGL.getTexUnit(unit)->activate();
    glActiveTexture(static_cast<GLenum>(active));
    if (s.ints[0]) glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
    s.drawBuffers.resize(count);
    for (GLint b = 0; b < count; ++b)
        glGetIntegerv(GL_DRAW_BUFFER0 + b, &s.drawBuffers[b]);
    return s;
}
LLScopedActorMaskInvariant::LLScopedActorMaskInvariant(const char* scope) : mScope(scope)
{
    static LLCachedControl<U32> debug(gSavedSettings, "CineLightRigActorMaskDebug", 0);
    mEnabled = debug != 0;
    if (mEnabled) mBefore = capture();
}
LLScopedActorMaskInvariant::~LLScopedActorMaskInvariant()
{
    if (!mEnabled) return;
    const State after = capture();
    if (mBefore == after) return;
    const char* field = "matrices/pointers";
    static const char* const names[] = {
        "draw FBO", "read FBO", "program", "depth func", "depth write",
        "blend src RGB", "blend dst RGB", "blend src alpha", "blend dst alpha", "cull mode",
        "blend enable", "depth enable", "cull enable", "polygon offset enable", "scissor enable", "logic-op",
        "viewport", "viewport", "viewport", "viewport",
        "scissor box", "scissor box", "scissor box", "scissor box",
        "colour mask", "colour mask", "colour mask", "colour mask",
        "draw buffers", "draw buffers", "draw buffers", "draw buffers",
        "blend equation RGB", "blend equation alpha", "front face", "active texture",
        "array buffer", "element buffer", "framebuffer sRGB", "matrix mode", "wrapper FBO", "wrapper texture unit"
    };
    static_assert(sizeof(names) / sizeof(names[0]) == 42, "ActorMask state fields");
    for (size_t i = 0; i < mBefore.ints.size(); ++i)
        if (mBefore.ints[i] != after.ints[i]) { field = names[i]; break; }
    for (size_t i = 0; i < 6; ++i)
        if (mBefore.floats[i] != after.floats[i])
        {
            field = i < 4 ? "clear colour" : "polygon offset factor/units";
            break;
        }
    if (mBefore.textures != after.textures) field = "texture type/id/mips/binding";
    if (mBefore.drawBuffers != after.drawBuffers) field = "draw buffers";
    LL_WARNS_ONCE("ActorMask") << "[ActorMask] ACTORMASK STATE-LEAK " << mScope << ": " << field << LL_ENDL;
}
