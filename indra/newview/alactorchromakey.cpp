/**
 * @file alactorchromakey.cpp
 * @brief [ActorKey] WP0 key activation and recipient selection.
 */
#include "llviewerprecompiledheaders.h"

#include "alactorchromakey.h"

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

bool ALActorChromaKey::sActive = false;
ALActorKeySet<LLUUID> ALActorChromaKey::sKeyed;
U64 ALActorChromaKey::sRevision = 1;
U32 ALActorChromaKey::sSuppress = 0;

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
            if (!sActive)
            {
                sKeyed = {};
                gPipeline.releaseActorKeyTargets();
            }
        });
    // Revision also covers edge/output controls whose values do not change membership.
    const char* controls[] = {"ActorKeySubjects", "ActorKeyIncludeClones", "ActorKeyIncludeAnimesh",
        "ActorKeyMode", "ActorKeyColorPreset", "ActorKeyColor", "ActorKeyMatteInvert",
        "ActorKeyFeather", "ActorKeyChoke", "ActorKeyEdgeMode", "ActorKeyAlphaCoverage",
        "ActorKeyHideHUD", "ActorKeyShadow", "ActorKeyShadowIsolation", "ActorKeyShadowStrength",
        "ActorKeyShadowCascades", "ActorKeyShadowSoftness", "ActorKeyMatteShadow",
        "ActorKeyCoverBias", "ActorKeyGlassSource", "ActorKeyDebugView", "ActorKeyDebugFailPreflight"};
    for (const char* name : controls)
        if (auto control = gSavedSettings.getControl(name))
            control->getSignal()->connect([](LLControlVariable*, const LLSD&, const LLSD&) { ++sRevision; });
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
    // WP3 owns the ReShade handshake. No guard bookkeeping on snapshot renders.
    return true;
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
