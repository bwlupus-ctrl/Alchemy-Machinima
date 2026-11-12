#include "llviewerprecompiledheaders.h"
#include "alshaftselftest.h"
#include "alshaftdecision.h"
#include "llagent.h"
#include "llapp.h"
#include "lldrawable.h"
#include "lleventtimer.h"
#include "llglslshader.h"
#include "llrender.h"
#include "alshaftglstate.h"
#include "llthread.h"
#include "llrendertarget.h"
#include "llvertexbuffer.h"
#include "llview.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llvolumemgr.h"
#include "llvovolume.h"
#include "pipeline.h"
#include <array>
#include <cmath>

namespace ALShaftSelfTest
{
bool active = false;
namespace
{
std::string line;
class Probe;
Probe* probe = nullptr;
LLHandle<LLView> pendingOwner;
class Probe final : public LLEventTimer
{
public:
    explicit Probe(LLView* owner) : LLEventTimer(.1f), view(owner->getHandle()),
        region(gAgent.getRegion()), started(gFrameCount), begin(LLTimer::getTotalSeconds()) {}
    ~Probe() override
    {
        llassert(assert_main_thread());
        if (light)
        {
            const LLUUID id = light->getID();
            // Same keyed hold as VCam: scene reset may already have released it.
            // Never restore a snapshot over another holder or a user's edit.
            if (holdsMaster) LLPipeline::releaseVCamVolumetrics(id);
            if (LLPipeline::isVolumetricShaftEnabled(id)) LLPipeline::toggleVolumetricShaft(id, false);
            if (LLPipeline::isHeroProjector(id)) LLPipeline::toggleHeroProjector(id);
            LLPipeline::clearVolumetricShaftOverride(id);
            if (LLPipeline::isProjectorNoShadow(id)) LLPipeline::toggleProjectorCastShadows(id);
            light->setLightIntensity(0.f);
            light->setIsLight(false); // also removes membership after region teardown
            if (!light->isDead()) light->markDead();
        }
        // Never restore validity bits: the textures may contain test-projector
        // samples. The first later beauty pass must seed clean histories.
        gPipeline.mProjVolHistoryValid = false;
        gPipeline.mFroxelHistoryValid = false;
        if (probe == this) probe = nullptr;
        active = false;
        LLView* restart = pendingOwner.get();
        pendingOwner = LLHandle<LLView>();
        if (restart && restart->isInVisibleChain()) start(restart);
    }
    void finish(const std::string& verdict)
    {
        if (lifetime.done) return;
        lifetime.done = true;
        // Remain active until timer-owned cleanup; never destroy in a render pass.
        line = verdict;
        LL_INFOS("ShaftSelfTest") << "#ShaftSelfTest " << line << LL_ENDL;
    }
    bool slot() const
    {
        return gPipeline.hasProjectorShaftShadowSlot(light);
    }
    bool valid() const
    {
        LLView* owner = view.get();
        return owner && owner->isInVisibleChain() && !LLApp::isExiting() &&
            gAgent.getRegion() == region && gAgent.getTeleportState() == LLAgent::TELEPORT_NONE &&
            light && !light->isDead();
    }
    bool tick() override
    {
        llassert(assert_main_thread());
        if (lifetime.canDestroy()) return true; // ONLY LLEventTimer deletes a Probe
        if (lifetime.done || lifetime.callbacks) return false;
        ALShaftDecision::ProbeLifetime::Lease callback(lifetime);
        if (!valid())
        {
            finish("FAIL - cancelled (page closed, teleport or logout)");
            return false;
        }
        if (LLTimer::getTotalSeconds() - begin > 10.)
        {
            if (!sawSlot) finish("FAIL step 1 - no native projector shadow slot");
            else if (!sawShader) finish("FAIL step 2 - active shaft shader unavailable or below class 3");
            else finish("FAIL step 3 - no shaft draw submitted (off-screen, blocked or filtered)");
            return false;
        }
        if (gFrameCount - started >= 4u)
        {
            // Shadow generation precedes idle/render depending on host scheduling;
            // submitted() checks the current frame again before measuring.
            sawSlot |= slot();
            static LLCachedControl<S32> engine(gSavedSettings, "BDMergeProjectorVolumetricsEngine", 1);
            auto& shader = engine == 0
                ? gDeferredProjectorVolumetricClassicProgram : gDeferredProjectorVolumetricProgram;
            sawShader = ALShaftDecision::shaderReady(shader.isComplete(), shader.mShaderLevel,
                engine != 0 && gPipeline.mProjVolVariantFailed);
        }
        return false;
    }
    LLHandle<LLView> view;
    LLViewerRegion* region;
    LLPointer<LLVOVolume> light;
    U32 started;
    F64 begin;
    ALShaftDecision::ProbeLifetime lifetime;
    bool sawSlot = false, sawShader = false;
    bool holdsMaster = false;
};
}

std::string result() { llassert(assert_main_thread()); return line; }
void cancel(const char* why)
{
    llassert(assert_main_thread());
    pendingOwner = LLHandle<LLView>();
    if (!probe) return;
    probe->finish(std::string("FAIL - cancelled (") + why + ")");
    // Deferred: callbacks may still have a lease on this timer-owned probe.
}

void start(LLView* owner)
{
    llassert(assert_main_thread());
    cancel("restarted");
    if (probe)
    {
        if (owner) pendingOwner = owner->getHandle();
        return; // old settings/light are cleaned BEFORE creating another probe
    }
    if (!owner || !gAgent.getRegion() || LLApp::isExiting())
    {
        line = "FAIL step 1 - no active region";
        LL_INFOS("ShaftSelfTest") << "#ShaftSelfTest " << line << LL_ENDL;
        return;
    }
    probe = new Probe(owner);
    Probe& p = *probe;
    ALShaftDecision::ProbeLifetime::Lease callback(p.lifetime);
    LLViewerObject* object = gObjectList.createObjectViewer(LL_PCODE_VOLUME, gAgent.getRegion());
    p.light = dynamic_cast<LLVOVolume*>(object);
    if (!p.light)
    {
        if (object) object->markDead();
        p.finish("FAIL step 1 - local projector creation failed");
        // Timer performs cleanup, including partially-created projectors.
        return;
    }
    LLVOVolume* light = p.light;
    light->mIsLocalOnly = true;
    light->mbCanSelect = false;
    // Ordinary local volume: do not claim rig priority, and never select or send updates.
    gPipeline.createObject(light);
    light->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
    LLVolumeParams shape;
    shape.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
    if (!light->setVolume(shape, LLVolumeLODGroup::NUM_LODS - 1, true))
    {
        p.finish("FAIL step 1 - local projector geometry failed");
        // Timer performs cleanup, including partially-created projectors.
        return;
    }
    const LLViewerCamera* camera = LLViewerCamera::getInstance();
    const LLVector3 forward = camera->getAtAxis();
    light->setPositionAgent(camera->getOrigin() + forward * 3.f, false);
    LLQuaternion rotation;
    rotation.shortestArc(LLVector3(0.f, 0.f, -1.f), forward);
    light->setRotation(rotation, false);
    light->setScale(LLVector3(.05f, .05f, .05f), false);
    ALShaftDecision::configureProbeLight(*light, LLColor3::white);
    light->setLightTextureID(IMG_WHITE);
    light->setSpotLightParams(LLVector3(1.5f, 0.f, 0.f));
    if (light->mDrawable) light->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    LLPipeline::VolumetricShaftOverride shaft;
    shaft.multiplier = 4.f;
    shaft.anisotropy = 0.f;
    // Override owns only this new object's ID; no existing projector is touched.
    LLPipeline::setVolumetricShaftOverride(light->getID(), shaft);
    if (!LLPipeline::isVolumetricShaftEnabled(light->getID())) LLPipeline::toggleVolumetricShaft(light->getID());
    LLPipeline::toggleHeroProjector(light->getID());
    LLPipeline::acquireShaftTestMaster(light->getID());
    p.holdsMaster = true;
    active = true;
    line = "Testing shafts - waiting for a native shadow slot";
    LL_INFOS("ShaftSelfTest") << "#ShaftSelfTest " << line << LL_ENDL;
}

void submitted(LLVOVolume* volume, LLGLSLShader& shader, LLRenderTarget& march)
{
    llassert(assert_main_thread());
    if (!probe || probe->lifetime.done || probe->light.get() != volume || gFrameCount - probe->started < 4u) return;
    Probe& p = *probe;
    ALShaftDecision::ProbeLifetime::Lease callback(p.lifetime);
    if (!p.valid())
    {
        p.finish("FAIL - cancelled (page closed, teleport or logout)");
        return;
    }
    if (!gPipeline.hasProjectorShaftShadowSlot(volume, true)) return;
    p.sawSlot = true;
    if (!ALShaftDecision::shaderReady(shader.isComplete(), shader.mShaderLevel))
    {
        p.finish("FAIL step 2 - submitted shaft shader is not class 3");
        return;
    }
    p.sawShader = true;
    // Run the unchanged active engine's direct aux path into black scratch with
    // only this light's native slot visible. Aux dispatch preserves main-view
    // history and never touches its accumulation targets. No second implementation
    // of shaft uniforms or shader math, and no other cone/history can pass the test.
    ScopedGLState state; // before allocation; outlives the scratch target on every exit
    const U32 activeUnit = gGL.getCurrentTexUnitIndex();
    LLTexUnit* unit = gGL.getTexUnit(0);
    const U32 texture = unit->getCurrTexture();
    const auto type = unit->getCurrType();
    const bool mips = unit->hasMipMaps();
    LLRenderTarget isolated;
    const bool allocated = isolated.allocate(march.getWidth(), march.getHeight(), GL_RGBA16F);
    // Allocation binds its texture on unit zero. Restore through LLTexUnit so
    // both GL and the viewer's cache agree before replaying the production draw.
    if (type != LLTexUnit::TT_NONE && texture) unit->bindManual(type, texture, mips);
    else if (type != LLTexUnit::TT_NONE)
    {
        unit->disable(); unit->enable(type); unit->unbind(type); unit->setHasMipMaps(mips);
    }
    else { unit->unbind(LLTexUnit::TT_TEXTURE); unit->disable(); }
    gGL.getTexUnit(activeUnit)->activate();
    if (!allocated)
    {
        isolated.release();
        p.finish("FAIL step 4 - shaft readback target allocation failed");
        return;
    }
    gGL.flush();
    isolated.bindTarget();
    {
        LLGLDisable noScissor(GL_SCISSOR_TEST);
        gGL.setColorMask(true, true);
        const GLfloat black[4] = {};
        glClearBufferfv(GL_COLOR, 0, black);
    }
    isolated.flush(); // balance the clear before the frozen pass binds its target
    struct SlotRestore
    {
        std::array<LLPointer<LLDrawable>, LLPipeline::MAX_SPOT_SHADOWS> owners;
        SlotRestore()
        {
            for (U32 i = 0; i < owners.size(); ++i) owners[i] = gPipeline.mShadowSpotLight[i];
        }
        ~SlotRestore()
        {
            for (U32 i = 0; i < owners.size(); ++i) gPipeline.mShadowSpotLight[i] = owners[i];
        }
    } slots;
    auto& owners = slots.owners;
    for (U32 i = 0; i < owners.size(); ++i)
    {
        if (owners[i] != volume->mDrawable) gPipeline.mShadowSpotLight[i] = nullptr;
    }
    if (LLPipeline::BDMergeProjectorVolumetricsEngine == 0)
        gPipeline.renderProjectorVolumetricClassic(&isolated, true);
    else
        gPipeline.renderProjectorVolumetricNew(&isolated, true);
    for (U32 i = 0; i < owners.size(); ++i) gPipeline.mShadowSpotLight[i] = owners[i];
    isolated.bindTarget();
    gGL.flush();
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    // This test projector is aimed along the camera axis; a central patch lies
    // inside its cone footprint. Scratch matches the main view's dimensions.
    const GLint width = llmin((S32)march.getWidth(), 16), height = llmin((S32)march.getHeight(), 16);
    std::array<GLfloat, 16*16*4> pixels{};
    const GLint x = ((S32)march.getWidth()-width)/2;
    const GLint y = ((S32)march.getHeight()-height)/2;
    glReadPixels(x, y, width, height, GL_RGBA, GL_FLOAT, pixels.data()); // exactly once on demand
    const GLenum readError = glGetError();
    isolated.flush();
    isolated.release();
    state.restore(); // verdict includes errors from framebuffer/buffer restoration
    float peak = 0.f;
    for (S32 i = 0; i < width*height; ++i)
        for (S32 c = 0; c < 3; ++c)
            if (std::isfinite(pixels[i*4+c])) peak = llmax(peak, pixels[i*4+c]);
    if (readError != GL_NO_ERROR || state.failed)
    {
        p.finish("FAIL step 4 - GPU readback reported an OpenGL error");
        return;
    }
    if (p.lifetime.done) return; // cancellation during a nested callback wins
    p.finish(peak > 0.0001f ? "PASS - shadow slot, class 3 shader, shaft draw and GPU light verified"
        : "FAIL step 4 - shaft pixels are black (camera view may be obstructed)");
}
}
