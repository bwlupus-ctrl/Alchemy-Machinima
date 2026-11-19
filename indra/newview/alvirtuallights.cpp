/**
 * @file alvirtuallights.cpp
 * @brief Runtime for session-only, client-only virtual lights -- see alvirtuallights.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alvirtuallights.h"

#include "alcleanframe.h"
#include "alkeyframeplayer.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "alprojectortracker.h"
#include "altoolvirtuallightedit.h"
#include "alvcampagemodel.h"
#include "llagent.h"
#include "llapp.h"
#include "lldirectorcast.h"
#include "lldrawable.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llframetimer.h"
#include "llgl.h"
#include "llglslshader.h"
#include "lljoint.h"
#include "llprimitive.h"
#include "llprismlens.h"
#include "llrender.h"
#include "llselectmgr.h"
#include "llsdutil.h"
#include "lltimer.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewerdisplay.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvolume.h"
#include "llvolumemgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <algorithm>
#include <cmath>
#include <memory>

extern bool gSnapshot;
extern bool gCubeSnapshot;

namespace
{
using namespace ALVirtualLightsModel;

constexpr F32 EMITTER_SCALE = 0.25f;
constexpr F32 MARKER_RADIUS = 0.12f;
constexpr F32 AIM_LINE_M = 1.f;
constexpr F32 TIP_RADIUS = 0.07f;
constexpr char PRACTICALS_PAGE[] = "light_group/practicals_tab";

// Set once from the Practicals page; the pipeline hooks read it without constructing anything.
S32 sPlaced = 0;
bool sWantsGuides = false;      // the edit tool is active
bool sEditorStamped = false;    // the Practicals page stamped a frame; cleared synchronously on release
U32 sEditorFrame = 0;

// Draw-time truth: the marker is a Director editing aid, so it needs a live, visible Director.
bool directorLive()
{
    LLFloater* director = LLFloaterReg::findInstance("director");
    return director && director->getVisible() && !director->isMinimized() && director->isInVisibleChain();
}

LLSD gobo_to_llsd(const LLPipeline::GoboOverride& g)
{
    LLSD out = LLSD::emptyMap();
    out["pattern"] = g.mPattern;
    out["anim"] = g.mAnimMode;
    out["speed"] = LLSD::Real(g.mSpeed);
    out["zoom"] = LLSD::Real(g.mZoom);
    out["dispersion"] = LLSD::Real(g.mDispersion);
    const F32 tint[3] = {g.mTint.mV[0], g.mTint.mV[1], g.mTint.mV[2]};
    out["tint"] = vec3ToLLSD(tint);
    LLSD params = LLSD::emptyArray();
    for (S32 i = 0; i < 4; ++i) params.append(LLSD::Real(g.mPatternParams.mV[i]));
    out["params"] = params;
    return out;
}

LLPipeline::GoboOverride gobo_from_llsd(const LLSD& in)
{
    LLPipeline::GoboOverride g;
    g.mPattern = in["pattern"].asInteger();
    g.mAnimMode = in["anim"].asInteger();
    g.mSpeed = static_cast<F32>(in["speed"].asReal());
    g.mZoom = static_cast<F32>(in["zoom"].asReal());
    g.mDispersion = static_cast<F32>(in["dispersion"].asReal());
    F32 tint[3] = {1.f, 1.f, 1.f};
    vec3FromLLSD(in["tint"], tint);
    g.mTint = LLColor3(tint[0], tint[1], tint[2]);
    if (in["params"].isArray() && in["params"].size() == 4)
        for (S32 i = 0; i < 4; ++i) g.mPatternParams.mV[i] = static_cast<F32>(in["params"][i].asReal());
    return g;
}

LLSD shaft_to_llsd(const LLPipeline::VolumetricShaftOverride& s)
{
    LLSD out = LLSD::emptyMap();
    out["multiplier"] = LLSD::Real(s.multiplier);
    out["max_distance"] = LLSD::Real(s.maxDistance);
    out["feather"] = LLSD::Real(s.feather);
    out["anisotropy"] = LLSD::Real(s.anisotropy);
    out["density"] = LLSD::Real(s.density);
    const F32 tint[3] = {s.tint.mV[0], s.tint.mV[1], s.tint.mV[2]};
    out["tint"] = vec3ToLLSD(tint);
    out["tint_strength"] = LLSD::Real(s.tintStrength);
    out["rim_strength"] = LLSD::Real(s.rimStrength);
    out["rim_power"] = LLSD::Real(s.rimPower);
    out["rim_wrap"] = LLSD::Real(s.rimWrap);
    out["rim_softness"] = LLSD::Real(s.rimSoftness);
    return out;
}

LLPipeline::VolumetricShaftOverride shaft_from_llsd(const LLSD& in)
{
    LLPipeline::VolumetricShaftOverride s;
    s.multiplier = static_cast<F32>(in["multiplier"].asReal());
    s.maxDistance = static_cast<F32>(in["max_distance"].asReal());
    s.feather = static_cast<F32>(in["feather"].asReal());
    s.anisotropy = static_cast<F32>(in["anisotropy"].asReal());
    s.density = static_cast<F32>(in["density"].asReal());
    F32 tint[3] = {1.f, 1.f, 1.f};
    vec3FromLLSD(in["tint"], tint);
    s.tint = LLColor3(tint[0], tint[1], tint[2]);
    s.tintStrength = static_cast<F32>(in["tint_strength"].asReal());
    s.rimStrength = static_cast<F32>(in["rim_strength"].asReal());
    s.rimPower = static_cast<F32>(in["rim_power"].asReal());
    s.rimWrap = static_cast<F32>(in["rim_wrap"].asReal());
    s.rimSoftness = static_cast<F32>(in["rim_softness"].asReal());
    return s;
}

LLUUID default_cookie()
{
    LLUUID id;
    id.set(kDefaultCookie);
    return id;
}

LLVector3 vec_of(const F32 v[3]) { return LLVector3(v[0], v[1], v[2]); }

// The "settings" block of this object's Prism "prism_projectors" scene item (the existing codec,
// llprismlens.cpp sceneData), or undefined when it has no binding. Never stores the object id.
LLSD vcam_settings_of(const LLUUID& object_id)
{
    if (object_id.isNull() || !LLPrismLens::instanceExists()) return LLSD();
    const LLSD prism = LLPrismLens::sceneData();
    for (const LLSD& item : llsd::inArray(prism["prism_projectors"]))
        if (item["target"].asString() == "object" && item["object_id"].asUUID() == object_id &&
            item["settings"].isMap())
            return item["settings"];
    return LLSD();
}

// Reads that block back (same keys and clamping as the Prism scene loader). False = unusable, keep defaults;
// bindProjector / setProjectorSettings validate the result again.
bool vcam_settings_from(const LLSD& s, LLPrismLens::ProjectorSettings& out)
{
    if (!s.isMap()) return false;
    LLPrismLens::ProjectorSettings p;
    const std::string fit = s["fit"].asString();
    if (fit == "fit") p.mFitMode = LLPrismLens::EFitMode::FIT;
    else if (fit == "fill") p.mFitMode = LLPrismLens::EFitMode::FILL;
    else if (fit == "stretch") p.mFitMode = LLPrismLens::EFitMode::STRETCH;
    else return false;
    if (s["anchor"].isArray() && s["anchor"].size() == 2)
        for (S32 i = 0; i < 2; ++i) p.mAnchor[i] = static_cast<F32>(s["anchor"][i].asReal());
    if (s.has("feed_ev")) p.mFeedEV = static_cast<F32>(s["feed_ev"].asReal());
    if (!LLPrismLens::projectorLampPowerFromLLSD(s, p)) return false;
    if (s.has("follow_main_exposure")) p.mFollowMainExposure = s["follow_main_exposure"].asBoolean();
    if (!LLPrismLens::projectorResolutionFromLLSD(s, p)) return false;
    const std::string no_signal = s["no_signal"].asString();
    if (no_signal == "black") p.mNoSignal = LLPrismLens::ENoSignal::BLACK;
    else if (no_signal == "white") p.mNoSignal = LLPrismLens::ENoSignal::WHITE;
    else p.mNoSignal = LLPrismLens::ENoSignal::OWN_COOKIE;
    p.mKeepGobo = s["keep_gobo"].asBoolean();
    p.mFlipH = s["flip_h"].asBoolean();
    p.mFlipV = s["flip_v"].asBoolean();
    if (s.has("rotate")) p.mRotate = static_cast<U8>(s["rotate"].asInteger());
    if (s["screen_effects"].isMap())
    {
        const LLSD& e = s["screen_effects"];
        LLPrismLens::ScreenEffects& fx = p.mEffects;
        const struct { const char* mKey; F32* mValue; } reals[] = {
            {"scanlines", &fx.mScanlines}, {"scanline_count", &fx.mScanlineCount},
            {"scanline_depth", &fx.mScanlineDepth}, {"scanline_sharpness", &fx.mScanlineSharpness},
            {"scanline_thickness", &fx.mScanlineThickness}, {"pixelate", &fx.mPixelate},
            {"grayscale", &fx.mGrayscale}, {"sepia", &fx.mSepia}, {"static", &fx.mStatic},
            {"vertical_roll", &fx.mVerticalRoll}, {"roll_speed", &fx.mRollSpeed}, {"tracking", &fx.mTracking},
            {"flicker", &fx.mFlicker}, {"chroma_bleed", &fx.mChromaBleed}, {"vignette", &fx.mVignette},
            {"interlace", &fx.mInterlace}, {"dropout", &fx.mDropout}, {"brightness", &fx.mBrightness},
            {"sheen", &fx.mSheen}};
        for (const auto& item : reals)
        {
            if (!e.has(item.mKey) || (!e[item.mKey].isReal() && !e[item.mKey].isInteger())) continue;
            const F32 value = static_cast<F32>(e[item.mKey].asReal());
            if (std::isfinite(value)) *item.mValue = value;
        }
        if (e.has("flip_h")) fx.mFlipH = e["flip_h"].asBoolean();
        if (e.has("flip_v")) fx.mFlipV = e["flip_v"].asBoolean();
        if (e.has("rotate90")) fx.mRotate90 = e["rotate90"].asBoolean();
        fx.clampAndValidate();
    }
    if (s.has("look") && !LLPrismLens::projectorLookFromLLSD(s["look"], p.mLook)) return false;
    out = p;
    return true;
}

void octahedron(const LLVector3& c, F32 r)
{
    const LLVector3 px = c + LLVector3(r, 0.f, 0.f), nx = c - LLVector3(r, 0.f, 0.f);
    const LLVector3 py = c + LLVector3(0.f, r, 0.f), ny = c - LLVector3(0.f, r, 0.f);
    const LLVector3 pz = c + LLVector3(0.f, 0.f, r), nz = c - LLVector3(0.f, 0.f, r);
    const LLVector3* tri[8][3] = {
        {&px, &py, &pz}, {&py, &nx, &pz}, {&nx, &ny, &pz}, {&ny, &px, &pz},
        {&py, &px, &nz}, {&nx, &py, &nz}, {&ny, &nx, &nz}, {&px, &ny, &nz}};
    for (const auto& face : tri)
        for (const LLVector3* v : face) gGL.vertex3fv(v->mV);
}
} // namespace

ALVirtualLights& ALVirtualLights::instance()
{
    static ALVirtualLights lights;
    return lights;
}

ALVirtualLights::ALVirtualLights() = default;

bool ALVirtualLights::anyPlaced() { return sPlaced > 0; }

bool ALVirtualLights::wantsGuides()
{
    // Feature-off: one integer compare. With lights placed the marker shows only while a live, visible
    // Director editor owns it (the Practicals page stamped a recent frame, or the edit tool is active), and
    // never during a clean-frame take. releaseEditor() drops the ownership synchronously on every hide.
    if (sPlaced <= 0 || ALCleanFrame::active()) return false;
    const bool owned = sWantsGuides || (sEditorStamped && (LLFrameTimer::getFrameCount() - sEditorFrame) <= 2);
    return owned && directorLive();
}

void ALVirtualLights::setEditTool(bool active)
{
    sWantsGuides = active;
}

void ALVirtualLights::noteEditorVisible()
{
    sEditorStamped = true;
    sEditorFrame = LLFrameTimer::getFrameCount();
}

// Page hide, Director hide / minimise / close, teardown: the edit tool and the guide ownership end now, so
// not one more frame can carry a marker into the presented output.
void ALVirtualLights::releaseEditor()
{
    sWantsGuides = false;
    sEditorStamped = false;
    if (!ALToolVirtualLightEdit::instanceExists()) return;
    ALToolVirtualLightEdit* tool = ALToolVirtualLightEdit::getInstance();
    if (LLToolMgr::instanceExists() && LLToolMgr::getInstance()->getCurrentTool() == tool)
        LLToolMgr::getInstance()->clearTransientTool();   // handleDeselect cancels any drag
    else
        tool->cancelGesture();
}

void ALVirtualLights::logEvent(const char* event, const std::string& detail, const char* verdict)
{
    LL_INFOS("VirtualLight") << "#VirtualLight " << event << " " << detail << " verdict=" << verdict << LL_ENDL;
}

// ---- lookup -------------------------------------------------------------------------------
ALVirtualLights::Entry* ALVirtualLights::findEntry(const LLUUID& key)
{
    for (Entry& entry : mEntries)
        if (entry.mLight.mKey == key) return &entry;
    return nullptr;
}
const ALVirtualLights::Entry* ALVirtualLights::findEntry(const LLUUID& key) const
{
    for (const Entry& entry : mEntries)
        if (entry.mLight.mKey == key) return &entry;
    return nullptr;
}
ALVirtualLights::Entry* ALVirtualLights::findByObject(const LLUUID& object_id)
{
    if (object_id.isNull()) return nullptr;
    for (Entry& entry : mEntries)
        if (entry.mObject.notNull() && entry.mObject->getID() == object_id) return &entry;
    return nullptr;
}
std::vector<LLUUID> ALVirtualLights::keys() const
{
    std::vector<LLUUID> out;
    out.reserve(mEntries.size());
    for (const Entry& entry : mEntries) out.push_back(entry.mLight.mKey);
    return out;
}
const ALVirtualLights::Light* ALVirtualLights::find(const LLUUID& key) const
{
    const Entry* entry = findEntry(key);
    return entry ? &entry->mLight : nullptr;
}
bool ALVirtualLights::isVirtualObject(const LLUUID& object_id) const
{
    return keyForObject(object_id).notNull();
}
LLUUID ALVirtualLights::keyForObject(const LLUUID& object_id) const
{
    if (object_id.isNull()) return LLUUID::null;
    for (const Entry& entry : mEntries)
        if (entry.mObject.notNull() && entry.mObject->getID() == object_id) return entry.mLight.mKey;
    return LLUUID::null;
}
LLUUID ALVirtualLights::objectIdFor(const LLUUID& key) const
{
    const Entry* entry = findEntry(key);
    return entry && entry->mObject.notNull() ? entry->mObject->getID() : LLUUID::null;
}
LLVOVolume* ALVirtualLights::volumeFor(const LLUUID& key) const
{
    const Entry* entry = findEntry(key);
    return entry && entry->mObject.notNull() && !entry->mObject->isDead() ? entry->mObject.get() : nullptr;
}
std::string ALVirtualLights::label(const LLUUID& key) const
{
    for (size_t i = 0; i < mEntries.size(); ++i)
        if (mEntries[i].mLight.mKey == key) return "Virtual light " + std::to_string(i + 1);
    return "Virtual light";
}
LLVector3 ALVirtualLights::agentPositionOf(const LLUUID& key) const
{
    const Entry* entry = findEntry(key);
    if (!entry || !entry->mRegion) return LLVector3::zero;
    return gAgent.getPosAgentFromGlobal(
        entry->mRegion->getPosGlobalFromRegion(vec_of(entry->mLight.mPos)));
}

// ---- object lifecycle (mirrors ALCineLightRig::createEmitter / destroyEmitter) ----------
bool ALVirtualLights::createObject(Entry& entry)
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region || LLApp::isExiting()) return false;

    LLViewerObject* object = gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume)
    {
        if (object) object->markDead();
        return false;
    }
    volume->mbCanSelect = false;
    volume->mIsLocalOnly = true;
    volume->mLocalObjectKind = LLViewerObject::LOCAL_OBJECT_VIRTUAL_LIGHT;
    volume->setFlagsWithoutUpdate(
        FLAGS_OBJECT_YOU_OWNER | FLAGS_OBJECT_MODIFY | FLAGS_OBJECT_MOVE |
        FLAGS_OBJECT_COPY | FLAGS_OBJECT_TRANSFER, true);

    gPipeline.createObject(volume);
    volume->setLOD(LLVolumeLODGroup::NUM_LODS - 1);

    LLVolumeParams params;
    params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
    params.setBeginAndEndS(0.f, 1.f);
    params.setBeginAndEndT(0.f, 1.f);
    params.setRatio(1.f, 1.f);
    params.setShear(0.f, 0.f);
    if (!volume->setVolume(params, LLVolumeLODGroup::NUM_LODS - 1, true))
    {
        volume->markDead();
        return false;
    }
    volume->setScale(LLVector3(EMITTER_SCALE, EMITTER_SCALE, EMITTER_SCALE), false);
    volume->setIsLight(false);
    volume->setLightTextureID(LLUUID::null);
    volume->setLightIntensity(0.f);
    if (volume->mDrawable.notNull())
        volume->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    entry.mObject = volume;
    entry.mRegion = region;
    entry.mDirty = true;
    return true;
}

void ALVirtualLights::unbindVCam(const LLViewerObject* object)
{
    if (!object || !LLPrismLens::instanceExists()) return;
    const LLUUID id = object->getID();
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
    {
        const LLPrismLens::ProjectorDefinition& binding = snapshot.mProjectors[index];
        if (binding.mTarget == LLPrismLens::EProjectorTarget::OBJECT && binding.mObjectId == id)
        {
            std::string reason;
            LLPrismLens::removeProjector(binding.mHandle, &reason);
            break;
        }
    }
}

void ALVirtualLights::destroyEntry(Entry& entry)
{
    LLPointer<LLVOVolume> dying = entry.mObject;
    entry.mObject = nullptr;
    if (dying.isNull()) return;
    const LLUUID id = dying->getID();
    // Teardown order: VCam binding -> pipeline flags -> light membership -> markDead.
    unbindVCam(dying.get());
    if (LLPipeline::isVolumetricShaftEnabled(id))
        LLPipeline::toggleVolumetricShaft(id, false);   // not a manual decline of an auto flag
    if (LLPipeline::isHeroProjector(id)) LLPipeline::toggleHeroProjector(id);
    if (LLPipeline::isProjectorNoShadow(id)) LLPipeline::toggleProjectorCastShadows(id);
    LLPipeline::setRigWorldShadowPick(id, false);
    LLPipeline::setProjectorDetailShadows(id, false);
    LLPipeline::clearProjectorShadowSoftness(id);
    LLPipeline::clearGoboOverride(id);
    LLPipeline::clearVolumetricShaftOverride(id);
    // A region teardown may have marked the object dead first, but a surviving drawable must
    // never stay in LLPipeline::mLights.
    dying->setLightIntensity(0.f);
    dying->setIsLight(false);
    if (!dying->isDead()) dying->markDead();
}

// ---- Practicals state mirrored in the model --------------------------------------------
void ALVirtualLights::syncPracticals(Entry& entry)
{
    LLVOVolume* volume = entry.mObject.get();
    if (!volume || volume->isDead()) return;
    const LLUUID id = volume->getID();
    LLSD practicals = LLSD::emptyMap();
    if (LLPipeline::isProjectorNoShadow(id)) practicals[kPracticalNoShadow] = true;
    if (LLPipeline::isVolumetricShaftEnabled(id)) practicals[kPracticalVolumetric] = true;
    if (LLPipeline::hasGoboOverride(id)) practicals[kPracticalGobo] = gobo_to_llsd(LLPipeline::getGoboOverride(id));
    LLPipeline::VolumetricShaftOverride shaft;
    if (LLPipeline::getVolumetricShaftOverride(id, shaft)) practicals[kPracticalShaft] = shaft_to_llsd(shaft);
    entry.mLight.mPracticals = practicals;
}

void ALVirtualLights::applyPracticals(Entry& entry)
{
    LLVOVolume* volume = entry.mObject.get();
    if (!volume || volume->isDead()) return;
    const LLUUID id = volume->getID();
    const LLSD& p = entry.mLight.mPracticals;
    const bool no_shadow = p[kPracticalNoShadow].asBoolean();
    if (no_shadow != LLPipeline::isProjectorNoShadow(id)) LLPipeline::toggleProjectorCastShadows(id);
    const bool volumetric = p[kPracticalVolumetric].asBoolean();
    if (volumetric != LLPipeline::isVolumetricShaftEnabled(id)) LLPipeline::toggleVolumetricShaft(id, true);
    if (p[kPracticalGobo].isMap()) LLPipeline::setGoboOverride(id, gobo_from_llsd(p[kPracticalGobo]));
    else LLPipeline::clearGoboOverride(id);
    if (p[kPracticalShaft].isMap()) LLPipeline::setVolumetricShaftOverride(id, shaft_from_llsd(p[kPracticalShaft]));
    else LLPipeline::clearVolumetricShaftOverride(id);
}

LLUUID ALVirtualLights::vcamOf(const LLUUID& key) const
{
    const Entry* entry = findEntry(key);
    if (!entry || entry->mObject.isNull() || !LLPrismLens::instanceExists()) return LLUUID::null;
    const LLUUID id = entry->mObject->getID();
    const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
    for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
    {
        const LLPrismLens::ProjectorDefinition& binding = snapshot.mProjectors[index];
        if (binding.mTarget == LLPrismLens::EProjectorTarget::OBJECT && binding.mObjectId == id)
            return binding.mCapture.mId;
    }
    return LLUUID::null;
}

void ALVirtualLights::syncVCam(Entry& entry)
{
    // The registry is the truth (a capture can be removed from the VCam page).
    if (entry.mObject.isNull() || entry.mObject->isDead()) return;
    entry.mLight.mVCamCapture = vcamOf(entry.mLight.mKey);
    entry.mLight.mVCamSettings = entry.mLight.mVCamCapture.notNull()
        ? vcam_settings_of(entry.mObject->getID()) : LLSD();
}

bool ALVirtualLights::setVCam(const LLUUID& key, const LLUUID& capture_id, std::string* reason,
                              const LLSD& settings)
{
    Entry* entry = findEntry(key);
    if (!entry || entry->mObject.isNull() || entry->mObject->isDead())
    {
        if (reason) *reason = "That light is not ready.";
        return false;
    }
    if (capture_id.isNull())
    {
        unbindVCam(entry->mObject.get());
        entry->mLight.mVCamCapture.setNull();
        entry->mLight.mVCamSettings = LLSD();
        bump();
        return true;
    }
    std::string why;
    // A VCam target must be a live light: Off is intensity 0, never setIsLight(false).
    entry->mObject->setIsLight(true);
    if (ALVCamPageModel::dropOnProjector(capture_id, entry->mObject.get(), nullptr, &why) !=
        LLPrismLens::ERegistryResult::OK)
    {
        if (reason) *reason = why;
        return false;
    }
    entry->mLight.mVCamCapture = capture_id;
    entry->mLight.mVCamSettings = LLSD();
    // A restored binding keeps its authored settings (fit, exposure, lamp power, flips, effects, look).
    LLPrismLens::ProjectorSettings parsed;
    if (settings.isMap() && vcam_settings_from(settings, parsed))
    {
        const LLUUID id = entry->mObject->getID();
        const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
        for (U32 index = 0; index < snapshot.mProjectorCount; ++index)
        {
            const LLPrismLens::ProjectorDefinition& binding = snapshot.mProjectors[index];
            if (binding.mTarget != LLPrismLens::EProjectorTarget::OBJECT || binding.mObjectId != id) continue;
            std::string settings_reason;
            if (LLPrismLens::setProjectorSettings(binding.mHandle, parsed, &settings_reason))
                entry->mLight.mVCamSettings = settings;
            else
                logEvent("vcam-settings", "key=" + key.asString() + " " + settings_reason, "INCONCLUSIVE");
            break;
        }
    }
    bump();
    return true;
}

// ---- applying the model to the object -----------------------------------------------------
void ALVirtualLights::applyEntry(Entry& entry)
{
    LLVOVolume* volume = entry.mObject.get();
    if (!volume || volume->isDead() || !entry.mRegion) return;
    const Light& light = entry.mLight;

    volume->setPositionGlobal(entry.mRegion->getPosGlobalFromRegion(vec_of(light.mPos)), false);
    F32 dir[3];
    directionFromAngles(light.mYawDeg, light.mPitchDeg, dir);
    entry.mRot = ALProjectorTrackMath::aim(LLVector3(dir[0], dir[1], dir[2]), entry.mRot);
    volume->setRotation(entry.mRot, false);

    // Light mode FIRST: createObject() leaves it off, and while it is off getLightParams() is null, so every
    // setter below would silently do nothing (a recreated Off or non-default light read back defaults).
    volume->setIsLight(true);
    const LLUUID texture = light.mSpot
        ? (light.mTexture.notNull() ? light.mTexture : default_cookie()) : LLUUID::null;
    if (volume->getLightTextureID() != texture) volume->setLightTextureID(texture);
    volume->setLightSRGBColor(LLColor3(light.mColor[0], light.mColor[1], light.mColor[2]));
    volume->setLightIntensity(light.mOn ? light.mIntensity : 0.f);
    volume->setLightRadius(light.mRadius);
    volume->setLightFalloff(light.mFalloff);
    if (light.mSpot) volume->setSpotLightParams(LLVector3(light.mFov, light.mFocus, light.mAmbiance));
    if (volume->mDrawable.notNull())
    {
        gPipeline.updateMoveNormalAsync(volume->mDrawable);
        volume->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
    entry.mDirty = false;
}

// Reads the light back through the real volume getters (the verdict of a recreate: scene restore, undo).
bool ALVirtualLights::verifyApplied(const Entry& entry, const char* event)
{
    const LLVOVolume* volume = entry.mObject.get();
    const bool ok = volume && !volume->isDead() &&
        lightApplied(entry.mLight, volume->getIsLight(), volume->getLightIntensity(),
                     volume->getLightRadius(), volume->getLightFalloff());
    logEvent("verify", std::string(event) + " key=" + entry.mLight.mKey.asString(), ok ? "PASS" : "FAIL");
    return ok;
}

// ---- tracking (virtual backend of the Practicals Track card) -------------------------------
void ALVirtualLights::updateTracking(Entry& entry, F32 dt, F64 now)
{
    Light& light = entry.mLight;
    const LLVector3 here = gAgent.getPosAgentFromGlobal(
        entry.mRegion->getPosGlobalFromRegion(vec_of(light.mPos)));
    auto write_direction = [&](const LLVector3& direction)
    {
        const F32 d[3] = {direction.mV[0], direction.mV[1], direction.mV[2]};
        F32 yaw = light.mYawDeg, pitch = light.mPitchDeg;
        if (!anglesFromDirection(d, yaw, pitch)) return;
        if (std::fabs(yaw - light.mYawDeg) > 0.001f || std::fabs(pitch - light.mPitchDeg) > 0.001f)
        {
            light.mYawDeg = yaw;
            light.mPitchDeg = pitch;
            entry.mDirty = true;
        }
    };

    if (light.mTrack.mEnabled && light.mTrack.mHasSubject)
    {
        LLVOAvatar* avatar = LLDirectorCast::instance().resolve(light.mTrack.mSubject);
        LLJoint* joint = avatar && !avatar->isDead() && avatar->mDrawable.notNull()
            ? avatar->getJoint(ALProjectorTracker::jointName(light.mTrack.mJoint)) : nullptr;
        ALProjectorTrackMath::Pose frame;
        if (!ALProjectorTracker::subjectFrame(avatar, joint, frame))
        {
            if (!entry.mMissingSince) entry.mMissingSince = now;
            if (now - entry.mMissingSince > 2.)
            {
                light.mTrack.mEnabled = false;
                entry.mTrackStatus = "Subject unavailable";
                bump();
            }
            return;
        }
        entry.mMissingSince = 0.;
        F32 d[3];
        directionFromAngles(light.mYawDeg, light.mPitchDeg, d);
        const ALProjectorTrackMath::Pose now_pose{here, ALProjectorTrackMath::aim(LLVector3(d[0], d[1], d[2]), entry.mRot)};
        if (entry.mCapturePending || !entry.mOffsetCaptured)
        {
            entry.mOffset = ALProjectorTrackMath::relative(now_pose, frame);
            entry.mOffsetCaptured = true;
            entry.mCapturePending = false;
            entry.mCurrent = now_pose;
        }
        ALProjectorTrackMath::Pose desired = now_pose;
        if (light.mTrack.mMode == 1)
            desired = ALProjectorTrackMath::apply(entry.mOffset, frame);
        else
            desired.rotation = ALProjectorTrackMath::aim(frame.position - here, entry.mRot);
        entry.mCurrent = ALProjectorTrackMath::smooth(
            entry.mCurrent.rotation.isFinite() && entry.mCurrent.position.isFinite() ? entry.mCurrent : now_pose,
            desired, dt, light.mTrack.mSmoothing);
        if (light.mTrack.mMode == 1 && entry.mCurrent.position.isFinite())
        {
            const LLVector3d global = gAgent.getPosGlobalFromAgent(entry.mCurrent.position);
            const LLVector3 local = entry.mRegion->getPosRegionFromGlobal(global);
            if (std::fabs(local.mV[0] - light.mPos[0]) + std::fabs(local.mV[1] - light.mPos[1]) +
                std::fabs(local.mV[2] - light.mPos[2]) > 0.0005f)
            {
                for (S32 i = 0; i < 3; ++i) light.mPos[i] = local.mV[i];
                sanitize(light);
                entry.mDirty = true;
            }
        }
        write_direction(LLVector3(0.f, 0.f, -1.f) * entry.mCurrent.rotation);
        entry.mTrackStatus = light.mTrack.mMode == 1 ? "Following" : "Aiming";
        return;
    }
    if (light.mAimObject.notNull())
    {
        LLViewerObject* target = gObjectList.findObject(light.mAimObject);
        if (target && !target->isDead() && target->mDrawable.notNull())
            write_direction(target->getRenderPosition() - here);
    }
}

void ALVirtualLights::tick(F32 dt)
{
    if (mEntries.empty()) return;   // feature-off: nothing to do
    LL_PROFILE_ZONE_SCOPED;
    LLViewerRegion* region = gAgent.getRegion();
    const F64 now = LLTimer::getTotalSeconds();
    for (size_t index = 0; index < mEntries.size();)
    {
        Entry& entry = mEntries[index];
        if (entry.mObject.isNull() || entry.mObject->isDead())
        {
            // The object died under us (region teardown, disconnect). Never recreate it in whatever region
            // happens to be current: the light is simply gone, like a prim left behind in the old sim.
            const LLUUID key = entry.mLight.mKey;
            destroyEntry(entry);
            mEntries.erase(mEntries.begin() + index);
            sPlaced = count();
            if (mFocusKey == key) mFocusKey.setNull();
            ++mListRevision;
            bump();
            logEvent("lost", "key=" + key.asString() + " count=" + std::to_string(count()), "INCONCLUSIVE");
            continue;
        }
        ++index;
        if (entry.mRegion == region && (entry.mLight.mTrack.mEnabled || entry.mLight.mAimObject.notNull()))
            updateTracking(entry, dt, now);
        if (entry.mDirty) applyEntry(entry);
        else if (entry.mObject->mDrawable.notNull())
            entry.mObject->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
}

// ---- commands ---------------------------------------------------------------------------------
void ALVirtualLights::connectSelection()
{
    if (mSelectionConnected) return;
    mSelectionConnected = true;
    LLSelectMgr::getInstance()->mUpdateSignal.connect([this]()
    {
        // The signal fires on every selection refresh. The focus ends only when the primary selection CHANGES
        // to a new non-avatar object (a real prim picked in world), never on a repeated refresh of the same one.
        LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        const LLUUID primary = object ? object->getID() : LLUUID::null;
        const bool changed = primary != mLastPrimaryId;
        mLastPrimaryId = primary;
        if (!changed || mFocusKey.isNull()) return;
        if (object && !object->isAvatar()) clearFocus();
    });
}

void ALVirtualLights::setFocus(const LLUUID& key)
{
    if (key.notNull() && !findEntry(key)) return;
    connectSelection();
    if (mFocusKey == key) return;
    mFocusKey = key;
    bump();
}
void ALVirtualLights::clearFocus()
{
    if (mFocusKey.isNull()) return;
    mFocusKey.setNull();
    bump();
}

LLUUID ALVirtualLights::placeAtGlobal(const LLVector3d& point_global, const LLVector3& normal, std::string* reason)
{
    LLViewerRegion* region = gAgent.getRegion();
    const bool inside = region && region->pointInRegionGlobal(point_global);
    const Refusal refusal = placementRefusal(count(), region != nullptr, inside, ALCleanFrame::active());
    if (refusal != Refusal::NONE)
    {
        if (reason) *reason = refusalText(refusal);
        logEvent("place-refused", refusalText(refusal), "PASS");   // the refusal is the correct outcome
        return LLUUID::null;
    }
    const LLVector3 local = region->getPosRegionFromGlobal(point_global);
    const F32 hit[3] = {local.mV[0], local.mV[1], local.mV[2]};
    const F32 n[3] = {normal.mV[0], normal.mV[1], normal.mV[2]};
    return place(hit, n, reason);
}

LLUUID ALVirtualLights::place(const F32 hit[3], const F32 normal[3], std::string* reason)
{
    LLViewerRegion* region = gAgent.getRegion();
    const Refusal refusal = placementRefusal(count(), region != nullptr, true, ALCleanFrame::active());
    if (refusal != Refusal::NONE)
    {
        if (reason) *reason = refusalText(refusal);
        return LLUUID::null;
    }
    Entry entry;
    entry.mLight.mKey = LLUUID::generateNewID();
    liftedPosition(hit, normal, entry.mLight.mPos);
    // A new spot aims along the camera's forward direction (the light starts as a Point).
    const LLVector3 at = LLViewerCamera::getInstance()->getAtAxis();
    const F32 forward[3] = {at.mV[0], at.mV[1], at.mV[2]};
    anglesFromDirection(forward, entry.mLight.mYawDeg, entry.mLight.mPitchDeg);
    sanitize(entry.mLight);
    if (!createObject(entry))
    {
        if (reason) *reason = "Could not create the light.";
        logEvent("create", "key=" + entry.mLight.mKey.asString(), "FAIL");
        return LLUUID::null;
    }
    entry.mRot = LLQuaternion();
    applyEntry(entry);
    const LLUUID key = entry.mLight.mKey;
    const bool good = entry.mObject->isLocalOnly() && entry.mObject->isVirtualLight() &&
        entry.mObject->getIsLight() && entry.mObject->mDrawable.notNull();
    mEntries.push_back(std::move(entry));
    sPlaced = count();
    connectSelection();
    mFocusKey = key;
    ++mListRevision;
    bump();
    logEvent("create", "key=" + key.asString() + " count=" + std::to_string(count()), good ? "PASS" : "FAIL");
    return key;
}

bool ALVirtualLights::remove(const LLUUID& key)
{
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        if (mEntries[i].mLight.mKey != key) continue;
        const LLPointer<LLVOVolume> object = mEntries[i].mObject;
        destroyEntry(mEntries[i]);
        mEntries.erase(mEntries.begin() + i);
        sPlaced = count();
        if (mFocusKey == key) mFocusKey.setNull();
        ++mListRevision;
        bump();
        const bool gone = object.isNull() || object->isDead();
        logEvent("remove", "key=" + key.asString() + " count=" + std::to_string(count()), gone ? "PASS" : "FAIL");
        return true;
    }
    return false;
}

void ALVirtualLights::clearAll(const char* reason)
{
    ++mEpoch;   // pending undo steps must never replay into a different region/scene
    if (mEntries.empty())
    {
        mFocusKey.setNull();
        return;
    }
    const S32 total = count();
    S32 leaked = 0;
    for (Entry& entry : mEntries)
    {
        const LLPointer<LLVOVolume> object = entry.mObject;
        destroyEntry(entry);
        if (object.notNull() && !object->isDead()) ++leaked;
    }
    mEntries.clear();
    sPlaced = 0;
    mFocusKey.setNull();
    ++mListRevision;
    bump();
    logEvent(reason, "removed=" + std::to_string(total) + " leaked=" + std::to_string(leaked),
             leaked == 0 ? "PASS" : "FAIL");
}

bool ALVirtualLights::edit(const LLUUID& key, const std::function<void(Light&)>& change)
{
    Entry* entry = findEntry(key);
    if (!entry || !change) return false;
    Light copy = entry->mLight;
    change(copy);
    copy.mKey = key;
    sanitize(copy);
    if (sameLight(copy, entry->mLight)) return false;
    if (copy.mOn != entry->mLight.mOn || copy.mSpot != entry->mLight.mSpot) ++mListRevision;
    entry->mLight = copy;
    entry->mDirty = true;
    // A manual aim or move ends a previous track capture; Follow recaptures from the new pose.
    entry->mOffsetCaptured = false;
    bump();
    return true;
}

bool ALVirtualLights::setAimObject(const LLUUID& key, const LLUUID& object_id)
{
    return edit(key, [&](Light& light) { light.mAimObject = object_id; });
}

void ALVirtualLights::notePracticalsEdit(const LLUUID& key)
{
    if (findEntry(key)) bump();
}

void ALVirtualLights::configureTrack(const LLUUID& key, const LLUUID& subject, bool has_subject,
                                     S32 joint, S32 mode, F32 smoothing)
{
    Entry* entry = findEntry(key);
    if (!entry) return;
    Light& light = entry->mLight;
    const bool subject_changed = light.mTrack.mSubject != subject || light.mTrack.mJoint != joint ||
        light.mTrack.mMode != mode;
    light.mTrack.mSubject = subject;
    light.mTrack.mHasSubject = has_subject;
    light.mTrack.mJoint = joint;
    light.mTrack.mMode = mode;
    light.mTrack.mSmoothing = smoothing;
    sanitize(light);
    if (subject_changed) entry->mOffsetCaptured = false;
    if (!light.mTrack.mHasSubject) entry->mTrackStatus = "Choose a subject";
    bump();
}

void ALVirtualLights::enableTrack(const LLUUID& key, bool enabled)
{
    Entry* entry = findEntry(key);
    if (!entry) return;
    entry->mLight.mTrack.mEnabled = enabled && entry->mLight.mTrack.mHasSubject;
    entry->mOffsetCaptured = false;
    entry->mMissingSince = 0.;
    entry->mTrackStatus = entry->mLight.mTrack.mEnabled ? "Tracking" : "Track off";
    entry->mDirty = true;
    bump();
}

void ALVirtualLights::captureTrack(const LLUUID& key)
{
    Entry* entry = findEntry(key);
    if (!entry || entry->mLight.mTrack.mMode != 1) return;
    entry->mCapturePending = true;
    ++entry->mLight.mTrack.mCaptures;   // the press is data, so the Practicals edit wrapper records one undo step
    bump();
}

void ALVirtualLights::rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
{
    if (old_id.isNull() || old_id == new_id) return;
    bool any = false;
    for (Entry& entry : mEntries)
    {
        if (!rewriteActorInLight(entry.mLight, old_id, new_id)) continue;
        entry.mOffsetCaptured = false;
        entry.mMissingSince = 0.;
        if (new_id.isNull()) entry.mTrackStatus = "Choose a subject";
        entry.mDirty = true;
        any = true;
    }
    if (any) bump();
}

const std::string& ALVirtualLights::trackStatus(const LLUUID& key) const
{
    static const std::string off = "Track off";
    const Entry* entry = findEntry(key);
    return entry ? entry->mTrackStatus : off;
}

// ---- undo / scene snapshots -------------------------------------------------------------------
LLSD ALVirtualLights::capture()
{
    LLSD out = LLSD::emptyArray();
    for (Entry& entry : mEntries)
    {
        syncPracticals(entry);
        syncVCam(entry);
        out.append(toLLSD(entry.mLight));
    }
    return out;
}

bool ALVirtualLights::restore(const LLSD& snapshot, std::string& reason)
{
    if (!snapshot.isArray() || snapshot.size() > kMaxLights)
    {
        reason = "Virtual light history is malformed";
        return false;
    }
    if (!gAgent.getRegion())
    {
        reason = "Not in a region";
        return false;
    }
    const std::vector<Light> wanted = parseScene(snapshot, []() { return LLUUID::generateNewID(); });
    std::vector<Entry> next;
    next.reserve(wanted.size());
    std::vector<LLUUID> to_bind;
    for (const Light& light : wanted)
    {
        Entry* existing = findEntry(light.mKey);
        if (existing)
        {
            existing->mLight = light;
            existing->mDirty = true;
            existing->mOffsetCaptured = false;
            const LLUUID object_id = existing->mObject.notNull() ? existing->mObject->getID() : LLUUID::null;
            if (light.mVCamCapture != vcamOf(light.mKey) ||
                (light.mVCamCapture.notNull() && light.mVCamSettings.isMap() &&
                 light.mVCamSettings != vcam_settings_of(object_id)))
                to_bind.push_back(light.mKey);
            next.push_back(std::move(*existing));
            existing->mObject = nullptr;   // moved-from: keep destroy below from touching it
            existing->mLight.mKey.setNull();
            continue;
        }
        Entry fresh;
        fresh.mLight = light;
        if (!createObject(fresh))
        {
            reason = "Could not recreate a virtual light";
            continue;
        }
        applyEntry(fresh);
        verifyApplied(fresh, "undo-recreate");
        if (light.mVCamCapture.notNull()) to_bind.push_back(light.mKey);
        next.push_back(std::move(fresh));
    }
    // Whatever the snapshot no longer has goes away.
    for (Entry& entry : mEntries)
        if (entry.mLight.mKey.notNull()) destroyEntry(entry);
    mEntries = std::move(next);
    sPlaced = count();
    // Order: the VCam binding FIRST, then the Practicals state, so a bind-time auto-enable (light cone) can
    // never override a saved light-cone state.
    for (const LLUUID& key : to_bind)
    {
        std::string why;
        const Light* light = find(key);
        if (light) setVCam(key, light->mVCamCapture, &why, light->mVCamSettings);
    }
    for (const Light& light : wanted)
        if (Entry* entry = findEntry(light.mKey))
        {
            entry->mLight.mPracticals = light.mPracticals;
            applyPracticals(*entry);
        }
    if (mFocusKey.notNull() && !findEntry(mFocusKey)) mFocusKey.setNull();
    ++mListRevision;
    bump();
    return true;
}

LLSD ALVirtualLights::sceneData()
{
    // Positions are region-local; the block truncates to the cap. A VCam feed is stored as the
    // capture id and rebound after the lights exist (scene-load order: Prism, lights, rebind).
    return sceneBlock(parseScene(capture(), []() { return LLUUID::generateNewID(); }));
}

void ALVirtualLights::applySceneData(const LLSD& block, bool recovery)
{
    if (recovery) return;   // L7: login/crash recovery never brings virtual lights back
    std::vector<Light> lights = parseScene(block, []() { return LLUUID::generateNewID(); });
    if (lights.empty() && mEntries.empty()) return;
    clearAll("scene-clear");
    if (lights.empty()) return;
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        logEvent("scene-restore", "wanted=" + std::to_string(lights.size()) + " created=0 (no region)", "FAIL");
        return;
    }
    S32 created = 0;
    bool verified = true;
    for (Light& light : lights)
    {
        Entry entry;
        entry.mLight = light;
        if (!createObject(entry)) continue;
        applyEntry(entry);
        verified = verifyApplied(entry, "scene-restore") && verified;
        mEntries.push_back(std::move(entry));
        ++created;
        sPlaced = count();
        // VCam FIRST (Prism was applied before the lights), then the saved Practicals state on top.
        if (light.mVCamCapture.notNull())
        {
            std::string why;
            setVCam(light.mKey, light.mVCamCapture, &why, light.mVCamSettings);
        }
        if (Entry* placed = findEntry(light.mKey))
        {
            placed->mLight.mPracticals = light.mPracticals;
            applyPracticals(*placed);
        }
    }
    sPlaced = count();
    connectSelection();
    ++mListRevision;
    bump();
    logEvent("scene-restore", "wanted=" + std::to_string(lights.size()) + " created=" + std::to_string(created),
             created == static_cast<S32>(lights.size()) && verified ? "PASS" : (created ? "INCONCLUSIVE" : "FAIL"));
}

namespace
{
std::function<void()>& pendingFlusher()
{
    static std::function<void()> flusher;
    return flusher;
}
}

void ALVirtualLights::setPendingFlusher(std::function<void()> flusher)
{
    pendingFlusher() = std::move(flusher);
}

void ALVirtualLights::flushPending()
{
    if (pendingFlusher()) pendingFlusher()();
}

void ALVirtualLights::recordUndo(const LLSD& before, const std::string& label)
{
    ALVirtualLights& lights = instance();
    const LLSD after = lights.capture();
    if (before == after) return;
    LLFloater* director = LLFloaterReg::findInstance("director");
    ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
    LLPanel* page = shell ? shell->ensurePageBuilt(PRACTICALS_PAGE) : nullptr;
    if (!page) return;
    const U64 epoch = lights.mEpoch;
    auto edit = std::make_shared<std::pair<LLSD, LLSD>>(before, after);
    auto still_valid = [epoch](bool, std::string& reason)
    {
        if (instance().mEpoch != epoch)
        {
            reason = "The virtual lights were cleared since this edit";
            return false;
        }
        return true;
    };
    ALKitHistory::ScopedSource source(page);
    shell->history().recordCustom(page, "virtual_lights", label,
        [edit, still_valid](bool redo, std::string& reason)
        {
            if (!still_valid(redo, reason)) return false;
            return instance().restore(redo ? edit->second : edit->first, reason);
        },
        false, still_valid,
        // A clone replacement rewrites the retired actor inside the saved snapshots too.
        [edit](const LLUUID& old_id, const LLUUID& new_id)
        {
            rewriteActorInSnapshot(edit->first, old_id, new_id);
            rewriteActorInSnapshot(edit->second, old_id, new_id);
        });
}

// ---- marker ---------------------------------------------------------------------------------------
void ALVirtualLights::collectPicks(std::vector<Pick>& out) const
{
    for (const Entry& entry : mEntries)
    {
        if (!entry.mRegion) continue;
        const LLVector3 here = gAgent.getPosAgentFromGlobal(
            entry.mRegion->getPosGlobalFromRegion(vec_of(entry.mLight.mPos)));
        out.push_back({entry.mLight.mKey, 0, here});
        if (entry.mLight.mSpot)
        {
            F32 d[3];
            directionFromAngles(entry.mLight.mYawDeg, entry.mLight.mPitchDeg, d);
            out.push_back({entry.mLight.mKey, 1, here + LLVector3(d[0], d[1], d[2]) * AIM_LINE_M});
        }
    }
}

void ALVirtualLights::drawGuides()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // Same full clean-frame guard as the keyframe overlay: nothing here may reach a recording.
    if (!wantsGuides() || ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive() ||
        gSnapshot || gCubeSnapshot || LLPipeline::sPrismLensRender || LLPipeline::sReflectionRender ||
        LLPipeline::sShadowRender || !gPipeline.isTagRecordView() ||
        !gViewerWindow || !gViewerWindow->getUIVisibility() ||
        !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;

    ALVirtualLights& lights = instance();
    // Flush whatever the previous pass queued under ITS state, then remember the state this draw changes
    // (shader, texture unit 0, active unit) and put it back at the end, like the keyframe overlay.
    gGL.flush();
    LLGLSLShader* const previous_shader = LLGLSLShader::sCurBoundShaderPtr;
    const U32 previous_unit = gGL.getCurrentTexUnitIndex();
    LLTexUnit* const unit0 = gGL.getTexUnit(0);
    const LLTexUnit::eTextureType previous_type = unit0->getCurrType();
    const U32 previous_texture = unit0->getCurrTexture();
    LLGLEnable blend(GL_BLEND);
    LLGLDisable cull(GL_CULL_FACE);
    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
    gUIProgram.bind();
    unit0->unbind(LLTexUnit::TT_TEXTURE);
    for (const Entry& entry : lights.mEntries)
    {
        if (!entry.mRegion) continue;
        const bool selected = entry.mLight.mKey == lights.mFocusKey;
        const LLVector3 here = gAgent.getPosAgentFromGlobal(
            entry.mRegion->getPosGlobalFromRegion(vec_of(entry.mLight.mPos)));
        const F32 on = entry.mLight.mOn ? 1.f : 0.45f;
        const LLColor4 body = selected ? LLColor4(1.f, 0.82f, 0.1f, 0.95f)
                                       : LLColor4(0.2f * on + 0.1f, 0.8f * on, 1.f * on, 0.9f);
        gGL.begin(LLRender::TRIANGLES);
        gGL.color4fv(body.mV);
        octahedron(here, selected ? MARKER_RADIUS * 1.4f : MARKER_RADIUS);
        gGL.end();
        if (entry.mLight.mSpot)
        {
            F32 d[3];
            directionFromAngles(entry.mLight.mYawDeg, entry.mLight.mPitchDeg, d);
            const LLVector3 tip = here + LLVector3(d[0], d[1], d[2]) * AIM_LINE_M;
            gGL.begin(LLRender::LINES);
            gGL.color4fv(body.mV);
            gGL.vertex3fv(here.mV);
            gGL.vertex3fv(tip.mV);
            gGL.end();
            gGL.begin(LLRender::TRIANGLES);
            gGL.color4f(1.f, 1.f, 1.f, 0.95f);
            octahedron(tip, TIP_RADIUS);
            gGL.end();
        }
    }
    gGL.flush();
    // The guide composite after us expects its own state: white current colour, the shader, the bound texture.
    gGL.color4f(1.f, 1.f, 1.f, 1.f);
    if (previous_shader) previous_shader->bind(); else LLGLSLShader::unbind();
    if (previous_texture) unit0->bindManual(previous_type, previous_texture);
    gGL.getTexUnit(previous_unit)->activate();
}
