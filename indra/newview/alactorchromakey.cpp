/**
 * @file alactorchromakey.cpp
 * @brief [ActorKey] WP0 key activation and recipient selection.
 */
#include "llviewerprecompiledheaders.h"

#include "alactorchromakey.h"
#include "alactorkeycapture.h"
#include "llactormaskeligibility.h"
#include "llglslshader.h"
#include "llvertexbuffer.h"
#include "llactormover.h"
extern bool gCubeSnapshot;
#include <array>
#include <memory>
#include "llreshadebridge.h"
#include "alactorkeyui.h" // WP5 global settings presentation listeners

#include "alcinelightrigmanager.h"
#include "alghoststudio.h"
#include "alcoverpolicy.h"
#include "alavatarlightresponse.h"
#include "pipeline.h"
#include "llagent.h"
#include "llcharacter.h"
#include "lldirectorcast.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "llnotificationsutil.h"
#include "llviewerdisplay.h"

#include <algorithm>

extern bool gSnapshot; // snapshot presents are blocked; preserve the live guard heartbeat

bool ALActorChromaKey::sActive = false;
ALActorKeySet<LLUUID> ALActorChromaKey::sKeyed;
U64 ALActorChromaKey::sRevision = 1;
U32 ALActorChromaKey::sSuppress = 0;

namespace
{
template <typename Callback>
void watchActorKeyControls(Callback callback)
{
    // Revision also covers edge/output controls whose values do not change membership.
    const char* controls[] = {"ActorKeySubjects", "ActorKeyIncludeClones", "ActorKeyIncludeAnimesh",
        "ActorKeyMode", "ActorKeyColorPreset", "ActorKeyColor", "ActorKeyMatteInvert",
        "ActorKeyFeather", "ActorKeyChoke", "ActorKeyEdgeMode", "ActorKeyAlphaCoverage",
        "ActorKeyHideHUD", "ActorKeyShadow", "ActorKeyShadowIsolation", "ActorKeyShadowStrength",
        "ActorKeyShadowCascades", "ActorKeyShadowSoftness", "ActorKeyMatteShadow",
        "ActorKeyCoverBias", "ActorKeyGlassSource", "ActorKeyDebugView", "ActorKeyDebugFailPreflight"};
    for (const char* name : controls)
        if (auto control = gSavedSettings.getControl(name))
            control->getSignal()->connect(callback);
}
}

// static
void ALActorChromaKey::initialize()
{
    // [ActorKey] LLAppViewer calls this once after settings exist.
    // Persist=0 and Value=0 make a fresh session inert.
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    LLControlVariable* enabled = gSavedSettings.getControl("ActorKeyEnabled");
    if (!enabled) return;
    sActive = enabled->getValue().asBoolean();
    enabled->getSignal()->connect(
        [](LLControlVariable*, const LLSD& value, const LLSD&)
        {
            ALActorChromaKey::sActive = value.asBoolean();
            ++sRevision;
            LLReShadeBridge::instance().retryKeyGuard();
            if (!sActive)
            {
                sKeyed = {};
                gPipeline.releaseActorKeyTargets();
            }
        });
    ALActorKeyUI::initialize();
    watchActorKeyControls([](LLControlVariable*, const LLSD&, const LLSD&) { ++sRevision; });
}

// static
std::vector<LLVOAvatar*> ALActorChromaKey::gatherKeyRecipients()
{
    std::vector<LLVOAvatar*> result;
    if (!sActive) return result;

    static LLCachedControl<U32> subjects(gSavedSettings, "ActorKeySubjects", 0);
    static LLCachedControl<bool> include_clones(gSavedSettings, "ActorKeyIncludeClones", true);
    static LLCachedControl<bool> include_animesh(gSavedSettings, "ActorKeyIncludeAnimesh", false);
    const auto add = [&](LLVOAvatar* avatar)
    {
        if (!avatar || avatar->isDead()) return;
        if (avatar->isGhostAvatar() && !include_clones) return;
        if (avatar->isControlAvatar() && (!include_animesh || !avatar->isVisible())) return;
        // Overlay helpers are private implementation actors, including their attachments.
        if (ALGhostStudio::instance().isPrivateOverlayRuntime(avatar->getID())) return;
        if (std::find(result.begin(), result.end(), avatar) == result.end())
            result.push_back(avatar);
    };

    switch (static_cast<U32>(subjects))
    {
    case 1: // [ActorKey] Director cast.
    {
        LLDirectorCast& cast = LLDirectorCast::instance();
        for (const LLUUID& id : cast.getIds()) add(cast.resolve(id));
        break;
    }
    case 2: // [ActorKey] Existing actor-only recipients; add no others.
        for (const auto& recipient :
             ALCineLightRigManager::instance().actorOnlyRecipients())
            add(recipient.mAvatar.get());
        break;
    case 3: // [ActorKey] Me only.
        add(gAgentAvatarp.get());
        break;
    case 0: // [ActorKey] All resident avatars and clones.
    default:
        for (LLCharacter* character : LLCharacter::sInstances)
        {
            LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
            if (avatar && !avatar->isControlAvatar()) add(avatar);
        }
        break;
    }

    // [ActorKey] Animesh is an opt-in modifier. Add visible control avatars to
    // the selected set; actor-only mode above remains limited to its registry.
    if (include_animesh && static_cast<U32>(subjects) != 2)
    {
        for (LLCharacter* character : LLCharacter::sInstances)
        {
            LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
            if (avatar && avatar->isControlAvatar()) add(avatar);
        }
    }
    return result;
}

// Called before object updates so keep-live sees the current selection.
void ALActorChromaKey::tick()
{
    if (!sActive) return;
    static LLCachedControl<U32> subjects(gSavedSettings, "ActorKeySubjects", 0);
    static LLCachedControl<bool> clones(gSavedSettings, "ActorKeyIncludeClones", true);
    ALActorKeySet<LLUUID> next;
    next.all = subjects() == 0 || subjects() > 3;
    next.includeClones = clones();
    for (LLVOAvatar* av : gatherKeyRecipients()) next.selected.insert(av->getID());
    // Cast may contain an overlay instance with no LLVOAvatar to resolve.
    if (subjects() == 1)
        for (const auto& id : LLDirectorCast::instance().getIds()) next.selected.insert(id);
    std::vector<ALActorKeySet<LLUUID>::Avatar> live;
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        auto* av = dynamic_cast<LLVOAvatar*>(character);
        if (!av || av->isDead()) continue;
        LLVOAvatar* wearer = av->isControlAvatar() ? av->getAttachedAvatar() : nullptr;
        live.push_back({av->getID(), av->getActorFxOwnerId(),
            wearer ? wearer->getActorFxOwnerId() : LLUUID::null, av->isGhostAvatar()});
    }
    std::vector<ALActorKeySet<LLUUID>::Instance> instances;
    for (const auto& inst : ALGhostStudio::instance().getInstances())
    {
        // Retiring instances stay visible during Clone QoL's fade-out. Keep
        // membership until Studio actually removes them; replay owns the fade.
        // Studio null-source means self. Resolve through its canonical avatar.
        LLVOAvatar* source = inst.mSourceRemoved ? nullptr : ALGhostStudio::resolveSourceAvatar(inst);
        instances.push_back({inst.mId, inst.mSourceRemoved ? LLUUID::null :
            (source ? source->getID() : inst.mSource), inst.mEntityId});
    }
    next.rebuild(live, instances);
    if (next.avatars != sKeyed.avatars || next.owners != sKeyed.owners ||
        next.instances != sKeyed.instances) ++sRevision;
    sKeyed = std::move(next);
}

bool ALActorChromaKey::keepsLive(const LLVOAvatar* av)
{
    return sActive && av && sKeyed.avatars.count(av->getID());
}
bool ALActorChromaKey::ownerKeyed(const LLUUID& owner, const LLUUID& fallback)
{
    return sActive && sKeyed.nativeKeyed(owner, fallback);
}
bool ALActorChromaKey::instanceKeyed(const LLUUID& instance)
{
    return sActive && sKeyed.instances.count(instance);
}

namespace
{
bool silhouetteCover(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, LLVector2& cover)
{
    const ALLightResponse::Packed* p = ALAvatarLightResponse::instance().peek(k1, k2, k3);
    if (!p || p->mSil.mV[VW] <= 0.f) return false;
    const F32 amount = p->mSil.mV[VW];
    cover.set(p->mSilAux.mV[VY] < 0.5f ? amount : 0.f,
              p->mSilAux.mV[VZ] < 0.5f ? amount : 0.f);
    return cover.mV[VX] > 0.f || cover.mV[VY] > 0.f;
}
}
bool CoverPolicy::nativeCover(const LLUUID& owner, const LLUUID& fallback, LLVector2& cover) const
{
    if (!key) return silhouetteCover(owner, fallback, LLUUID::null, cover);
    cover.set(1.f, 0.f);
    return ALActorChromaKey::ownerKeyed(owner, fallback);
}
bool CoverPolicy::proxyCover(const LLUUID& instance, const LLUUID& owner,
                            const LLUUID& fallback, LLVector2& cover) const
{
    if (!key) return silhouetteCover(instance, owner, fallback, cover);
    cover.set(1.f, 0.f);
    return ALActorChromaKey::instanceKeyed(instance);
}
bool CoverPolicy::sharedCover(const LLUUID& id, const LLUUID& owner,
                             const LLUUID& fallback, LLVector2& cover) const
{
    // D54: WP1b alone will replay shared beauty commands. No generic key contribution.
    return !key && silhouetteCover(id, owner, fallback, cover);
}


namespace
{
U64 sFinalizeSerial = 0;
U32 sKeyFrame = ~0u;
S32 sKeyWidth = 0, sKeyHeight = 0;
bool sComplete = false, sStudioFailed = false;
std::string sKeyReason = "no keyed finalize";
bool sExplicitRequest = true, sPendingExplicit = false;
struct SnapshotRefusalLatch
{
    bool latched = false, connected = false;
    U64 revision = 0;
    S32 width = 0, height = 0, windowWidth = 0, windowHeight = 0;
    S32 settledWidth = 0, settledHeight = 0;
    std::string suspension, placeholder;
} sRefusal;
}

bool ALActorChromaKey::keyGuardAllows()
{
    // Snapshot presents are blocked by WP2: do not consume the live heartbeat.
    if (gSnapshot) return true;
    return LLReShadeBridge::instance().keyGuardAllows(gFrameCount);
}

U32 ALActorChromaKey::mode()
{
    static LLCachedControl<U32> value(gSavedSettings, "ActorKeyMode", 0);
    return llmin(value(), 2u);
}
LLColor3 ALActorChromaKey::colour()
{
    static LLCachedControl<LLColor3> value(gSavedSettings, "ActorKeyColor", LLColor3(0.f, 177.f/255.f, 64.f/255.f));
    LLColor3 code = value();
    for (S32 i = 0; i < 3; ++i) code.mV[i] = ll_round(llclamp(code.mV[i], 0.f, 1.f) * 255.f) / 255.f;
    return code;
}
bool ALActorChromaKey::keyedThisFrame()
{
    return sActive && sKeyFrame == gFrameCount;
}
bool ALActorChromaKey::alphaMatte() { return keyedThisFrame() && mode() == 2; }
namespace
{
bool keyHideHUDSetting()
{
    static LLCachedControl<bool> hide(gSavedSettings, "ActorKeyHideHUD", true);
    return hide();
}
}
bool ALActorChromaKey::hideHUD() { return keyedThisFrame() && keyHideHUDSetting(); }
bool ALActorChromaKey::matteInvert()
{
    static LLCachedControl<bool> invert(gSavedSettings, "ActorKeyMatteInvert", false);
    return invert();
}
void ALActorChromaKey::preflightControls()
{
    // Prime the lazy control subscriptions before any destructive write.
    (void)mode(); (void)colour(); (void)sceneGlass(); (void)matteInvert(); (void)keyHideHUDSetting();
}
bool ALActorChromaKey::sceneGlass()
{
    static LLCachedControl<U32> source(gSavedSettings, "ActorKeyGlassSource", 0);
    return source() == 0;
}
ALActorChromaKey::StudioPolicy ALActorChromaKey::requestedStudioPolicy(const LLUUID& id)
{
    const bool included = instanceKeyed(id);
    switch (mode())
    {
    case 1: return included ? StudioPolicy::Fill : StudioPolicy::Look;
    case 2: return included ? StudioPolicy::Fill : StudioPolicy::Skip;
    default: return included ? StudioPolicy::Look : StudioPolicy::Skip;
    }
}
ALActorChromaKey::StudioPolicy ALActorChromaKey::studioPolicy(const LLUUID& id)
{
    return keyedThisFrame() ? requestedStudioPolicy(id) : StudioPolicy::Look;
}
U64 ALActorChromaKey::finalizeSerial() { return sFinalizeSerial; }
void ALActorChromaKey::beginFinalize()
{
    ++sFinalizeSerial;
    sKeyFrame = ~0u;
    sComplete = sStudioFailed = false;
    sKeyReason = "incomplete finalize";
}
void ALActorChromaKey::refuse(const char* why)
{
    sKeyFrame = ~0u;
    sComplete = false;
    sKeyReason = why;
}
const std::string& ALActorChromaKey::reason() { return sKeyReason; }
void ALActorChromaKey::provision(S32 width, S32 height)
{
    sKeyFrame = gFrameCount;
    sKeyWidth = width; sKeyHeight = height;
    sComplete = false;
    sKeyReason.clear();
}
void ALActorChromaKey::studioFailure(const char* why)
{
    sStudioFailed = true;
    sComplete = false;
    sKeyReason = why;
    LL_WARNS_ONCE("ActorKey") << "ACTORKEY STUDIO-FAILURE " << why << LL_ENDL;
}
void ALActorChromaKey::finishStudioFrame(U64 serial)
{
    // R41: the caller invokes this even when Studio took an early return.
    if (serial == sFinalizeSerial && keyedThisFrame() && !sStudioFailed) sComplete = true;
}
bool ALActorChromaKey::snapshotComplete(U64 before, S32 width, S32 height)
{
    return sFinalizeSerial > before && keyedThisFrame() && sComplete &&
        sKeyWidth == width && sKeyHeight == height;
}
ALActorChromaKey::SnapshotScope::SnapshotScope() : mActive(sActive), mPrevious(false)
{
    if (mActive) mPrevious = LLWindow::sPresentBlocked.exchange(true);
}
ALActorChromaKey::SnapshotScope::~SnapshotScope()
{
    if (mActive) LLWindow::sPresentBlocked.store(mPrevious);
}
ALActorChromaKey::SnapshotRequestScope::SnapshotRequestScope(bool explicit_request)
    : mPrevious(sExplicitRequest) { sExplicitRequest = explicit_request; }
ALActorChromaKey::SnapshotRequestScope::~SnapshotRequestScope() { sExplicitRequest = mPrevious; }
bool ALActorChromaKey::snapshotRequestExplicit() { return sExplicitRequest; }
void ALActorChromaKey::explicitSnapshotRequest() { sPendingExplicit = true; }
bool ALActorChromaKey::snapshotRequestAllowed(S32 width, S32 height)
{
    if (!sActive) { sRefusal.latched = false; return true; }
    const auto& window = gViewerWindow->getWindowRectRaw();
    const bool connected = !gDisconnected;
    const bool explicit_request = sExplicitRequest || sPendingExplicit;
    sPendingExplicit = false;
    const bool cleared = explicit_request || sRefusal.revision != revision() ||
        sRefusal.width != width || sRefusal.height != height ||
        (!gWindowResized && (sRefusal.settledWidth != window.getWidth() || sRefusal.settledHeight != window.getHeight())) ||
        (!sRefusal.connected && connected) || (!sRefusal.suspension.empty() && reason().empty());
    sRefusal.revision = revision(); sRefusal.width = width; sRefusal.height = height;
    sRefusal.windowWidth = window.getWidth(); sRefusal.windowHeight = window.getHeight();
    if (!gWindowResized) { sRefusal.settledWidth = window.getWidth(); sRefusal.settledHeight = window.getHeight(); }
    sRefusal.connected = connected; sRefusal.suspension = reason();
    if (cleared) sRefusal.latched = false;
    if (sRefusal.latched)
        sRefusal.placeholder = "Keyed snapshot unavailable: " + reason() + " - press Refresh";
    return !sRefusal.latched;
}
void ALActorChromaKey::snapshotRefused(S32 width, S32 height)
{
    // snapshotRequestAllowed has already captured the request inputs.
    (void)width; (void)height;
    if (sRefusal.latched) return;
    sRefusal.latched = true;
    sRefusal.suspension = reason();
    sRefusal.placeholder = "Keyed snapshot unavailable: " + reason() + " - press Refresh";
    LL_WARNS("ActorKey") << "ACTORKEY SNAPSHOT-REFUSED " << reason() << LL_ENDL;
    LLSD args; args["REASON"] = reason();
    LLNotificationsUtil::add("ActorKeySnapshotRefused", args);
}
const std::string& ALActorChromaKey::snapshotPlaceholder()
{
    static const std::string empty;
    return sActive && sRefusal.latched ? sRefusal.placeholder : empty;
}

// [ActorKey][WP1b] Capture resolved program inputs at base beauty emission.
// Uniform reflection intentionally includes matrix arrays, indexed material arrays,
// cloth, time and sampler units: the vec4-only LLGLSLShader cache is insufficient.
namespace
{
struct KeyUniform
{
    std::string name;
    GLenum type;
    std::array<GLfloat, 16> f{};
    std::array<GLint, 4> i{};
};
struct KeyCommand
{
    const LLDrawInfo* info = nullptr;
    SystemComponentKey system{};
    bool isSystem = false, bearing = false, blended = false;
    LLUUID owner;
    LLGLSLShader* beauty = nullptr;
    LLUUID hash;
    std::vector<KeyUniform> uniforms;
    std::vector<LLActorMaskTextureState> textures;
    std::array<glm::mat4, LLRender::NUM_MATRIX_MODES> matrices;
    std::array<GLfloat, 4> colour{};
    GLint polygon[2] = {GL_FILL, GL_FILL}, cullMode = GL_BACK;
    bool cull = false;
    std::function<void(LLGLSLShader&)> draw;
};
struct KeyProgram { LLGLSLShader shader; LLUUID hash; };
std::vector<KeyCommand> sKeyCommands;
std::map<LLGLSLShader*, std::unique_ptr<KeyProgram>> sKeyPrograms;
U32 sKeyCaptureFrame = ~0u;
bool sKeyCaptureValid = true;

bool key_integer(GLenum type)
{
    switch (type)
    {
    case GL_INT: case GL_BOOL: case GL_INT_VEC2: case GL_INT_VEC3: case GL_INT_VEC4:
    case GL_BOOL_VEC2: case GL_BOOL_VEC3: case GL_BOOL_VEC4:
    case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_SHADOW: case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_CUBE_MAP_ARRAY: case GL_SAMPLER_2D_MULTISAMPLE: case GL_SAMPLER_3D:
        return true;
    default: return false;
    }
}
bool key_upload(GLuint program, const KeyUniform& u)
{
    const GLint loc = glGetUniformLocation(program, u.name.c_str());
    if (loc < 0) return true; // Colour/lighting input optimized out of coverage variant.
    switch (u.type)
    {
    case GL_INT_VEC2: case GL_BOOL_VEC2: glUniform2iv(loc, 1, u.i.data()); break;
    case GL_INT_VEC3: case GL_BOOL_VEC3: glUniform3iv(loc, 1, u.i.data()); break;
    case GL_INT_VEC4: case GL_BOOL_VEC4: glUniform4iv(loc, 1, u.i.data()); break;
    case GL_FLOAT: glUniform1fv(loc, 1, u.f.data()); break;
    case GL_FLOAT_VEC2: glUniform2fv(loc, 1, u.f.data()); break;
    case GL_FLOAT_VEC3: glUniform3fv(loc, 1, u.f.data()); break;
    case GL_FLOAT_VEC4: glUniform4fv(loc, 1, u.f.data()); break;
    case GL_FLOAT_MAT2: glUniformMatrix2fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT3: glUniformMatrix3fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT4: glUniformMatrix4fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT2x3: glUniformMatrix2x3fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT2x4: glUniformMatrix2x4fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT3x2: glUniformMatrix3x2fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT3x4: glUniformMatrix3x4fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT4x2: glUniformMatrix4x2fv(loc, 1, GL_FALSE, u.f.data()); break;
    case GL_FLOAT_MAT4x3: glUniformMatrix4x3fv(loc, 1, GL_FALSE, u.f.data()); break;
    default:
        if (!key_integer(u.type)) return false;
        glUniform1iv(loc, 1, u.i.data()); break;
    }
    return true;
}
void key_snapshot(KeyCommand& command)
{
    command.beauty = LLGLSLShader::sCurBoundShaderPtr;
    if (!command.beauty) { sKeyCaptureValid = false; return; }
    auto& beauty = *command.beauty;
    command.hash = beauty.mShaderHash;
    gGL.syncMatrices();
    GLint count = 0, max_name = 0;
    glGetProgramiv(beauty.mProgramObject, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(beauty.mProgramObject, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name);
    std::vector<GLchar> name(llmax(max_name, 1));
    for (GLint index = 0; index < count; ++index)
    {
        GLsizei length; GLint size; GLenum type;
        glGetActiveUniform(beauty.mProgramObject, index, (GLsizei)name.size(), &length, &size, &type, name.data());
        const std::string base(name.data(), length);
        for (GLint element = 0; element < size; ++element)
        {
            KeyUniform u; u.name = base; u.type = type;
            const auto zero = base.find("[0]");
            if (zero != std::string::npos) u.name.replace(zero, 3, "[" + std::to_string(element) + "]");
            const GLint loc = glGetUniformLocation(beauty.mProgramObject, u.name.c_str());
            if (loc < 0) continue; // Uniform blocks are managed by the original features.
            if (key_integer(type)) glGetUniformiv(beauty.mProgramObject, loc, u.i.data());
            else glGetUniformfv(beauty.mProgramObject, loc, u.f.data());
            command.uniforms.push_back(std::move(u));
        }
    }
    const U32 active = gGL.getCurrentTexUnitIndex();
    GLint actual_active; glGetIntegerv(GL_ACTIVE_TEXTURE, &actual_active);
    for (U32 unit = 0; unit < LL_NUM_TEXTURE_LAYERS; ++unit)
        command.textures.emplace_back(*gGL.getTexUnit(unit));
    gGL.getTexUnit(active)->activate();
    glActiveTexture(actual_active);
    for (U32 mode = 0; mode < LLRender::NUM_MATRIX_MODES; ++mode)
        command.matrices[mode] = gGL.getMatrix(static_cast<LLRender::eMatrixMode>(mode));
    const GLint colour = beauty.getAttribLocation(LLVertexBuffer::TYPE_COLOR);
    if (colour >= 0) glGetVertexAttribfv(colour, GL_CURRENT_VERTEX_ATTRIB, command.colour.data());
    glGetIntegerv(GL_POLYGON_MODE, command.polygon);
    glGetIntegerv(GL_CULL_FACE_MODE, &command.cullMode);
    command.cull = glIsEnabled(GL_CULL_FACE) != GL_FALSE;
}
}

bool ALActorKeyCapture::sArmed = false;
ALActorKeyCapture& ALActorKeyCapture::instance() { static ALActorKeyCapture capture; return capture; }
void ALActorKeyCapture::beginBeauty()
{
    mBeautySuppressed.clear(); mSystemSuppressed.clear();
    mDrawTrace.clear(); mSystemTrace.clear(); sKeyCommands.clear();
    sKeyCaptureFrame = gFrameCount; sKeyCaptureValid = true; sArmed = true;
}
void ALActorKeyCapture::release()
{
    sArmed = false; std::vector<KeyCommand>().swap(sKeyCommands); sKeyPrograms.clear();
    mBeautySuppressed.clear(); mSystemSuppressed.clear();
    mDrawTrace.clear(); mSystemTrace.clear();
    std::unordered_set<const LLDrawInfo*>().swap(mBeautySuppressed); sKeyCaptureFrame = ~0u;
}
bool ALActorKeyCapture::native(const LLDrawInfo* info)
{
    auto& trace = mDrawTrace[info];
    if (mBeautySuppressed.count(info)) return false;
    return true;
}
bool ALActorKeyCapture::native(const SystemComponentKey& key)
{
    auto& trace = mSystemTrace[key];
    if (mSystemSuppressed.count(key)) return false;
    return true;
}
void ALActorKeyCapture::capture(const LLDrawInfo* info, const LLUUID& owner, bool bearing,
    LLVertexBuffer* vb, U32 start, U32 end, U32 count, U32 offset, bool double_sided, bool blended)
{
    if (!sArmed || ALActorChromaKey::suppressed() || sKeyCaptureFrame != gFrameCount || !info || !vb || !count) return;
    GLboolean mask[4]; glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    if (!mask[0]) return; // The same helper also emits depth-only primes.
    KeyCommand command; command.info = info; command.owner = owner; command.bearing = bearing; command.blended = blended;
    // A9 records every actual shared emission, including non-bearing ordinary Layer.
    key_snapshot(command);
    if (double_sided) command.cull = false; // Indexed binder's temporary cull scope has already restored.
    // Geometry and material/palette/matrix inputs belong to this beauty emission.
    LLPointer<LLVertexBuffer> buffer(vb);
    command.draw = [buffer, start, end, count, offset](LLGLSLShader&) mutable
    { buffer->setBuffer(); buffer->drawRange(LLRender::TRIANGLES, start, end, count, offset); };
    sKeyCommands.push_back(std::move(command));
}
void ALActorKeyCapture::capture(const SystemComponentKey& key, bool bearing, U32 drawn,
    std::function<void(LLGLSLShader&)> draw)
{
    if (!sArmed || ALActorChromaKey::suppressed() || sKeyCaptureFrame != gFrameCount || !drawn) return;
    KeyCommand command; command.system = key; command.isSystem = true;
    command.owner = key.id; command.bearing = bearing; command.blended = key.part == SystemComponentKey::BodyTransparent;
    key_snapshot(command); command.draw = std::move(draw);
    sKeyCommands.push_back(std::move(command));
}
bool ALActorKeyCapture::replay(bool include_alpha)
{
    sArmed = false; // Coverage never records beauty or recursively captures itself.
    if (!sKeyCaptureValid || sKeyCaptureFrame != gFrameCount) return false;
    struct RestoreReplayInputs
    {
        LLRender::eMatrixMode mode = gGL.getMatrixMode();
        GLint polygon[2];
        std::array<glm::mat4, LLRender::NUM_MATRIX_MODES> matrices;
        std::vector<std::array<GLfloat, 4>> attributes;
        RestoreReplayInputs()
        {
            glGetIntegerv(GL_POLYGON_MODE, polygon);
            for (U32 i = 0; i < LLRender::NUM_MATRIX_MODES; ++i)
                matrices[i] = gGL.getMatrix(static_cast<LLRender::eMatrixMode>(i));
            GLint count; glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count); attributes.resize(count);
            for (GLint i = 0; i < count; ++i) glGetVertexAttribfv(i, GL_CURRENT_VERTEX_ATTRIB, attributes[i].data());
        }
        ~RestoreReplayInputs()
        {
            for (U32 i = 0; i < LLRender::NUM_MATRIX_MODES; ++i)
            { gGL.matrixMode(static_cast<LLRender::eMatrixMode>(i)); gGL.loadMatrix(glm::value_ptr(matrices[i])); }
            gGL.matrixMode(mode);
            glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
            for (U32 i = 0; i < attributes.size(); ++i) glVertexAttrib4fv(i, attributes[i].data());
        }
    } restore;
    for (auto& command : sKeyCommands)
    {
        if (!ALActorChromaKey::ownerKeyed(command.owner, command.info ? command.info->mActorFxFallbackOwner : LLUUID::null)) continue;
        const bool suppressed = command.isSystem ? mSystemSuppressed.count(command.system) != 0
                                                 : mBeautySuppressed.count(command.info) != 0;
        auto& trace = command.isSystem ? mSystemTrace[command.system] : mDrawTrace[command.info];
        if (!suppressed || !command.bearing || (!include_alpha && command.blended)) continue;
        if (!command.beauty || command.hash != command.beauty->mShaderHash) return false;
        auto& cached = sKeyPrograms[command.beauty];
        if (!cached) cached = std::make_unique<KeyProgram>();
        auto& shader = cached->shader;
        if (!shader.isComplete() || cached->hash != command.hash)
        {
            shader.unload();
            shader.mName = command.beauty->mName + " Key component coverage";
            shader.mFeatures = command.beauty->mFeatures;
            shader.mShaderFiles = command.beauty->mShaderFiles;
            shader.mShaderLevel = command.beauty->mShaderLevel;
            shader.mShaderGroup = command.beauty->mShaderGroup;
            shader.clearPermutations(); shader.mDefines = command.beauty->mDefines;
            shader.addPermutation("AL_KEY_COVERAGE_OUT", "1");
            if (!shader.createShader()) return false;
            cached->hash = command.hash;
        }
        // The coverage linker can optimize colour-only samplers out. Keep the
        // original unit map for system mesh binders and restore sampler values by name.
        shader.mTexture = command.beauty->mTexture;
        for (U32 mode = 0; mode < LLRender::NUM_MATRIX_MODES; ++mode)
        {
            gGL.matrixMode(static_cast<LLRender::eMatrixMode>(mode));
            gGL.loadMatrix(glm::value_ptr(command.matrices[mode]));
        }
        gGL.matrixMode(LLRender::MM_MODELVIEW); gGLLastMatrix = nullptr;
        shader.bind(); gGL.syncMatrices();
        for (const auto& u : command.uniforms) if (!key_upload(shader.mProgramObject, u)) return false;
        for (U32 unit = 0; unit < command.textures.size(); ++unit)
            command.textures[unit].restore(*gGL.getTexUnit(unit));
        shader.vertexAttrib4fv(LLVertexBuffer::TYPE_COLOR, command.colour.data());
        LLGLState cull(GL_CULL_FACE, command.cull ? LLGLState::ENABLED_STATE : LLGLState::DISABLED_STATE);
        glCullFace(command.cullMode);
        glPolygonMode(GL_FRONT_AND_BACK, command.polygon[0]);
        command.draw(shader);
        trace.shared = 1; // One path per component; disjoint faces/slots remain separate commands.
    }
    // Include suppressed-without-capture components, rather than silently omitting zero Cover/hidden.
    for (const auto* info : mBeautySuppressed) mDrawTrace[info];
    for (const auto& key : mSystemSuppressed) mSystemTrace[key];
    static LLCachedControl<U32> debug(gSavedSettings, "ActorKeyDebugView", 0);
    if (debug)
    {
        for (const auto& entry : mDrawTrace)
            LL_INFOS("ActorKey") << "CONTRIBUTOR draw=" << entry.first
                << " suppressed=" << (mBeautySuppressed.count(entry.first) != 0)
                << " native=" << entry.second.native << " WP1b=" << entry.second.shared
                << " count=" << entry.second.native + entry.second.shared << LL_ENDL;
        for (const auto& entry : mSystemTrace)
            LL_INFOS("ActorKey") << "CONTRIBUTOR system=" << entry.first.id << " part=" << entry.first.part
                << " suppressed=" << (mSystemSuppressed.count(entry.first) != 0)
                << " native=" << entry.second.native << " WP1b=" << entry.second.shared
                << " count=" << entry.second.native + entry.second.shared << LL_ENDL;
    }
    return true;
}


bool ALActorKeyCapture::beautyContext()
{
    return ALActorChromaKey::sActive && !ALActorChromaKey::suppressed() &&
        LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
        !LLPipeline::sShadowRender && !LLPipeline::sVelocityRender &&
        !LLPipeline::sReflectionRender && !LLPipeline::sImpostorRender &&
        !LLPipeline::sRenderingHUDs && !LLPipeline::sPrismLensRender &&
        !LLPipeline::sPrismPreviewRender && !gCubeSnapshot &&
        !gPipeline.mHeroProbeManager.isMirrorPass() &&
        !LLActorMover::instance().isRenderingSharedActorStyle();
}
