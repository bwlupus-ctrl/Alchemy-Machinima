/**
 * @file alcinelightrig.cpp
 * @brief Client-side cinematic light-rig controller and local emitters.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"

#include "llagent.h"
#include "llapp.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "lldirectorcast.h"
#include "lldrawable.h"
#include "llfile.h"
#include "llgl.h"
#include "llglslshader.h"
#include "lljoint.h"
#include "llnotificationsutil.h"
#include "llprimitive.h"
#include "llrender.h"
#include "llsdserialize.h"
#include "lluri.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llvoavatar.h"
#include "llvolume.h"
#include "llvolumemgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
using namespace ALCineLightRigModel;

constexpr char PRESET_SUBDIR[] = "cine_light_rig";
constexpr char CLASSIC_SETUP_NAME[] = "Classic 3-Point";
constexpr char MASTER_PRESET_FILE[] = "cine_light_rig_presets.xml";
constexpr char DEFAULT_COOKIE[] = "5748decc-f629-461c-9a36-a35a221fe21f";
constexpr F32 EMITTER_SCALE = 0.25f;
constexpr S32 GIZMO_SEGMENTS = 16;
constexpr S32 EMITTER_RETRY_TICKS = 60;

const LLColor4 GIZMO_COLORS[LIGHT_COUNT] = {
    LLColor4(1.f, 0.4f, 0.4f, 1.f),
    LLColor4(0.4f, 1.f, 0.4f, 1.f),
    LLColor4(0.4f, 0.7f, 1.f, 1.f),
    LLColor4(1.f, 1.f, 0.4f, 1.f)
};

const char* const ROLE_NAMES[LIGHT_COUNT] = {
    "Key", "Fill", "Rim", "Bg"
};

const char* const GOBO_FILES[GOBO_COUNT] = {
    nullptr,
    "cine_gobos/gobo_blinds.png",
    "cine_gobos/gobo_panes.png",
    "cine_gobos/gobo_bars.png",
    "cine_gobos/gobo_slats.png",
    "cine_gobos/gobo_grid.png",
    "cine_gobos/gobo_dapple.png",
    "cine_gobos/gobo_branches.png",
};

bool sameSetupName(const std::string& first, const std::string& second)
{
    return LLStringUtil::compareInsensitive(first, second) == 0;
}

LLVector3 scaledPoint(LLVOAvatar* avatar, const LLVector3& point, F32 scale)
{
    if (!avatar || scale == 1.f)
    {
        return point;
    }

    LLJoint* root = avatar->getRootJoint();
    if (!root)
    {
        return point;
    }
    LLVector3 foot = root->getWorldPosition();
    F32 pelvis_to_foot = avatar->getPelvisToFoot();
    if (!std::isfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    foot.mV[VZ] -= std::max(0.f, pelvis_to_foot);
    if (!foot.isFinite())
    {
        return point;
    }
    return foot + (point - foot) * scale;
}

S32 gatherGroupMembers(U32 slots,
                       LLVOAvatar* out_members[GROUP_MAX_MEMBERS],
                       U32& out_slots)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLVOAvatar* candidates[GROUP_MAX_MEMBERS] = {
        (slots & ALCineLightRig::GROUP_SLOT_SELF)
            ? cast.resolve(LLUUID::null) : nullptr,
        (slots & ALCineLightRig::GROUP_SLOT_A)
            ? cast.resolveSubjectA() : nullptr,
        (slots & ALCineLightRig::GROUP_SLOT_B)
            ? cast.resolveSubjectB() : nullptr,
        (slots & ALCineLightRig::GROUP_SLOT_C)
            ? cast.resolveSubjectC() : nullptr,
        (slots & ALCineLightRig::GROUP_SLOT_D)
            ? cast.resolveSubjectD() : nullptr,
    };
    const U32 slot_bits[GROUP_MAX_MEMBERS] = {
        ALCineLightRig::GROUP_SLOT_SELF,
        ALCineLightRig::GROUP_SLOT_A,
        ALCineLightRig::GROUP_SLOT_B,
        ALCineLightRig::GROUP_SLOT_C,
        ALCineLightRig::GROUP_SLOT_D,
    };

    out_slots = 0;
    S32 count = 0;
    for (S32 slot = 0; slot < GROUP_MAX_MEMBERS; ++slot)
    {
        LLVOAvatar* candidate = candidates[slot];
        if (!candidate)
        {
            continue;
        }
        bool duplicate = false;
        for (S32 i = 0; i < count; ++i)
        {
            if (out_members[i]->getID() == candidate->getID())
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            out_members[count++] = candidate;
            out_slots |= slot_bits[slot];
        }
    }
    return count;
}

bool memberTrackPoint(LLVOAvatar* avatar, S32 track_mode, F32 member_scale,
                      LLVector3& out_point)
{
    bool have_joint = false;
    if (track_mode == 0)
    {
        if (LLJoint* chest = avatar->getJoint("mChest"))
        {
            out_point = scaledPoint(
                avatar, chest->getWorldPosition(), member_scale);
            have_joint = out_point.isFinite();
        }
    }
    if (!have_joint)
    {
        out_point = scaledPoint(
            avatar,
            avatar->getRenderPosition() + LLVector3(0.f, 0.f, 1.2f),
            member_scale);
        if (!out_point.isFinite())
        {
            return false;
        }
    }
    return true;
}

F32 emitterBoxEdge(F32 nominal_radius, F32 subject_scale)
{
    if (subject_scale == 1.f)
    {
        return emitterBoxEdgeFromRatio(1.f);
    }
    const F32 effective_radius = std::clamp(
        nominal_radius * subject_scale,
        std::min(nominal_radius, SCALED_RADIUS_FLOOR),
        std::max(nominal_radius, SCALED_RADIUS_CEIL));
    return emitterBoxEdgeFromRatio(effective_radius / nominal_radius);
}

bool sameLight(const LightBase& first, const LightBase& second)
{
    return first.mYawDeg == second.mYawDeg &&
           first.mPitchDeg == second.mPitchDeg &&
           first.mProfile == second.mProfile &&
           first.mEV == second.mEV &&
           first.mBeam == second.mBeam &&
           first.mOn == second.mOn &&
           first.mGobo == second.mGobo &&
           first.mGel == second.mGel;
}

bool sameLights(const LightBase first[LIGHT_COUNT],
                const LightBase second[LIGHT_COUNT])
{
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (!sameLight(first[i], second[i]))
        {
            return false;
        }
    }
    return true;
}

void copyLights(const LightBase source[LIGHT_COUNT],
                LightBase destination[LIGHT_COUNT])
{
    std::memcpy(destination, source, sizeof(LightBase) * LIGHT_COUNT);
}

LLUUID rigCookie(const std::string& setting)
{
    LLUUID cookie;
    if (!cookie.set(setting, false) || cookie.isNull())
    {
        cookie.set(DEFAULT_COOKIE, false);
    }
    return cookie;
}

LLUUID rigGoboTexture(S32 index, const std::string& cookie_setting)
{
    if (index <= 0 || index >= GOBO_COUNT)
    {
        return rigCookie(cookie_setting);
    }

    static LLPointer<LLViewerFetchedTexture> cache[GOBO_COUNT];
    static bool attempted[GOBO_COUNT] = {};
    if (!attempted[index])
    {
        attempted[index] = true;
        if (!gDirUtilp->findSkinnedFilename(
                 "textures", GOBO_FILES[index]).empty())
        {
            cache[index] =
                LLViewerTextureManager::getFetchedTextureFromFile(
                    GOBO_FILES[index], FTT_LOCAL_FILE, MIPMAP_YES,
                    LLGLTexture::BOOST_NONE);
        }
        if (cache[index].isNull())
        {
            LL_WARNS("CineLightRig") << "Bundled gobo missing: "
                << GOBO_FILES[index] << "; using default cookie" << LL_ENDL;
        }
    }
    return cache[index].notNull()
        ? cache[index]->getID() : rigCookie(cookie_setting);
}

bool emittersReady(const LLPointer<LLVOVolume> emitters[LIGHT_COUNT],
                   LLViewerRegion* region)
{
    if (!region)
    {
        return false;
    }
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (emitters[i].isNull() || emitters[i]->isDead() ||
            emitters[i]->getRegion() != region)
        {
            return false;
        }
    }
    return true;
}

bool emitterReady(const LLPointer<LLVOVolume>& emitter,
                  LLViewerRegion* region)
{
    return region && emitter.notNull() && !emitter->isDead() &&
           emitter->getRegion() == region;
}

S32 profileIndex(const std::string& name)
{
    for (S32 i = 0; i < PROFILE_COUNT; ++i)
    {
        if (name == profileName(i))
        {
            return i;
        }
    }
    return -1;
}

S32 beamIndex(const std::string& name)
{
    for (S32 i = 0; i < BEAM_COUNT; ++i)
    {
        if (name == beamName(i))
        {
            return i;
        }
    }
    return -1;
}

S32 goboIndex(const std::string& name)
{
    for (S32 i = 0; i < GOBO_COUNT; ++i)
    {
        if (name == goboName(i))
        {
            return i;
        }
    }
    return -1;
}

S32 gelIndex(const std::string& name)
{
    for (S32 i = 0; i < GEL_COUNT; ++i)
    {
        if (name == gelName(i))
        {
            return i;
        }
    }
    return -1;
}

LLSD lightToLLSD(const LightBase& light)
{
    LLSD data = LLSD::emptyMap();
    data["yaw"] = light.mYawDeg;
    data["pitch"] = light.mPitchDeg;
    data["profile_idx"] = light.mProfile;
    data["profile_name"] = profileName(light.mProfile);
    data["ev"] = light.mEV;
    data["beam_idx"] = light.mBeam;
    data["beam_name"] = beamName(light.mBeam);
    data["on"] = light.mOn;
    data["gobo_idx"] = light.mGobo;
    data["gobo_name"] = goboName(light.mGobo);
    data["gel_idx"] = light.mGel;
    data["gel_name"] = gelName(light.mGel);
    return data;
}

bool setupFromLLSD(const LLSD& data, Setup& output)
{
    if (!data.isMap() || !data["lights"].isArray() ||
        data["lights"].size() != LIGHT_COUNT)
    {
        return false;
    }

    Setup setup;
    setup.mRadius = static_cast<F32>(data["radius"].asReal());
    setup.mRatioLock = data["ratio_lock"].asBoolean();
    setup.mRatioStops = data.has("ratio_stops")
        ? static_cast<F32>(data["ratio_stops"].asReal()) : 0.f;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const LLSD& item = data["lights"][i];
        if (!item.isMap())
        {
            return false;
        }
        LightBase& light = setup.mLights[i];
        light.mYawDeg = static_cast<F32>(item["yaw"].asReal());
        light.mPitchDeg = static_cast<F32>(item["pitch"].asReal());
        light.mProfile = item["profile_idx"].asInteger();
        if (item["profile_name"].isString())
        {
            const S32 named_profile = profileIndex(
                item["profile_name"].asString());
            if (named_profile >= 0 &&
                (light.mProfile < 0 || light.mProfile >= PROFILE_COUNT ||
                 item["profile_name"].asString() !=
                     profileName(light.mProfile)))
            {
                light.mProfile = named_profile;
            }
        }
        light.mEV = static_cast<F32>(item["ev"].asReal());
        light.mBeam = item["beam_idx"].asInteger();
        if (item["beam_name"].isString())
        {
            const S32 named_beam = beamIndex(item["beam_name"].asString());
            if (named_beam >= 0 &&
                (light.mBeam < 0 || light.mBeam >= BEAM_COUNT ||
                 item["beam_name"].asString() != beamName(light.mBeam)))
            {
                light.mBeam = named_beam;
            }
        }
        light.mOn = item["on"].asBoolean();
        light.mGobo = item["gobo_idx"].asInteger();
        if (item["gobo_name"].isString())
        {
            const S32 named_gobo = goboIndex(
                item["gobo_name"].asString());
            if (named_gobo >= 0 &&
                (light.mGobo < 0 || light.mGobo >= GOBO_COUNT ||
                 item["gobo_name"].asString() != goboName(light.mGobo)))
            {
                light.mGobo = named_gobo;
            }
        }
        light.mGel = item["gel_idx"].asInteger();
        if (item["gel_name"].isString())
        {
            const S32 named_gel = gelIndex(item["gel_name"].asString());
            if (named_gel >= 0 &&
                (light.mGel < 0 || light.mGel >= GEL_COUNT ||
                 item["gel_name"].asString() != gelName(light.mGel)))
            {
                light.mGel = named_gel;
            }
        }
    }
    output = sanitizeSetup(setup);
    return true;
}

LLSD setupToLLSD(const Setup& input)
{
    const Setup setup = sanitizeSetup(input);
    LLSD data = LLSD::emptyMap();
    data["radius"] = setup.mRadius;
    data["ratio_lock"] = setup.mRatioLock;
    data["ratio_stops"] = setup.mRatioStops;
    data["lights"] = LLSD::emptyArray();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        data["lights"].append(lightToLLSD(setup.mLights[i]));
    }
    return data;
}
} // namespace

const char ALCineLightRig::BUILT_IN_SETUP_CAPTION[] =
    "\xE2\x80\x94 Built-in \xE2\x80\x94";
const char ALCineLightRig::GENRE_SETUP_CAPTION[] =
    "\xE2\x80\x94 Genre / Mood \xE2\x80\x94";
const char ALCineLightRig::LOCAL_SETUP_CAPTION[] =
    "\xE2\x80\x94 My setups \xE2\x80\x94";

//static
bool ALCineLightRig::isSetupDecorationName(const std::string& name)
{
    return sameSetupName(name, BUILT_IN_SETUP_CAPTION) ||
           sameSetupName(name, GENRE_SETUP_CAPTION) ||
           sameSetupName(name, LOCAL_SETUP_CAPTION);
}

ALCineLightRig::ALCineLightRig(ALCineLightRigSlot slot)
  : mSlot(slot)
{
    std::memset(mTransitionStart, 0, sizeof(mTransitionStart));
    std::memset(mTransitionTarget, 0, sizeof(mTransitionTarget));
    std::memset(mCurrentLive, 0, sizeof(mCurrentLive));
    std::memset(&mLastFrame, 0, sizeof(mLastFrame));
}

ALCineLightRig::~ALCineLightRig() = default;

void ALCineLightRig::setAnchor(const LLUUID& id)
{
    mAnchor = id;
    mHaveSmoothedCentre = false;
    mSmoothedScale = 1.f;
}

void ALCineLightRig::setGroupEnabled(bool enabled)
{
    mGroupEnabled = enabled;
    mHaveSmoothedCentre = false;
    mSmoothedScale = 1.f;
}

void ALCineLightRig::setGroupSlots(U32 mask)
{
    mGroupSlots = mask & GROUP_SLOT_MASK;
    mHaveSmoothedCentre = false;
    mSmoothedScale = 1.f;
}

LLVOAvatar* ALCineLightRig::resolveSlotAvatar() const
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    switch (mSlot)
    {
        case ALCineLightRigSlot::A: return cast.resolveSubjectA();
        case ALCineLightRigSlot::B: return cast.resolveSubjectB();
        case ALCineLightRigSlot::C: return cast.resolveSubjectC();
        case ALCineLightRigSlot::D: return cast.resolveSubjectD();
        case ALCineLightRigSlot::SELF:
        default:
            // A non-null SELF anchor exists only for an arbitrary-anchor
            // pre-multi-instance scene. Normal SELF operation is resolve(null).
            return cast.resolve(mAnchor.notNull() ? mAnchor : LLUUID::null);
    }
}

LLUUID ALCineLightRig::projectorId(S32 light) const
{
    if (light < 0 || light >= LIGHT_COUNT || mProjectors[light].isNull() ||
        mProjectors[light]->isDead())
    {
        return LLUUID::null;
    }
    return mProjectors[light]->getID();
}

void ALCineLightRig::startFX(S32 fx_id, F64 presentation_time)
{
    if (fx_id < 0 || fx_id >= FX_COUNT ||
        !std::isfinite(presentation_time))
    {
        stopFX();
        return;
    }
    gSavedSettings.setS32("CineLightRigFX", fx_id);
    mActiveFX = fx_id;
    mFXStart = presentation_time;
    mPendingFXPhase = -1.0;
    mPendingFXId = -1;
    mTransitionActive = false;
    mHaveTarget = false;
}

void ALCineLightRig::stopFX()
{
    gSavedSettings.setS32("CineLightRigFX", -1);
    mActiveFX = -1;
    mPendingFXPhase = -1.0;
    mPendingFXId = -1;
    mTransitionActive = false;
    mHaveTarget = false;
}

void ALCineLightRig::setShaftEnabled(S32 light, bool enabled)
{
    if (light < 0 || light >= LIGHT_COUNT)
    {
        return;
    }
    mShaftEnabled[light] = enabled;
    updateProjectorFlags();
}

bool ALCineLightRig::isShaftEnabled(S32 light) const
{
    return light >= 0 && light < LIGHT_COUNT && mShaftEnabled[light];
}

void ALCineLightRig::setHeroEnabled(S32 light, bool enabled)
{
    if (light < 0 || light >= LIGHT_COUNT)
    {
        return;
    }
    mHeroEnabled[light] = enabled;
    updateProjectorFlags();
}

bool ALCineLightRig::isHeroEnabled(S32 light) const
{
    return light >= 0 && light < LIGHT_COUNT && mHeroEnabled[light];
}

void ALCineLightRig::readSettings(Setup& setup, Globals& globals,
                                  Transforms& transforms) const
{
    static LLCachedControl<F32> radius(gSavedSettings, "CineLightRigRadius");
    static LLCachedControl<F32> key_yaw(gSavedSettings, "CineLightRigKeyYaw");
    static LLCachedControl<F32> fill_yaw(gSavedSettings, "CineLightRigFillYaw");
    static LLCachedControl<F32> rim_yaw(gSavedSettings, "CineLightRigRimYaw");
    static LLCachedControl<F32> bg_yaw(gSavedSettings, "CineLightRigBgYaw");
    static LLCachedControl<F32> key_pitch(gSavedSettings, "CineLightRigKeyPitch");
    static LLCachedControl<F32> fill_pitch(gSavedSettings, "CineLightRigFillPitch");
    static LLCachedControl<F32> rim_pitch(gSavedSettings, "CineLightRigRimPitch");
    static LLCachedControl<F32> bg_pitch(gSavedSettings, "CineLightRigBgPitch");
    static LLCachedControl<S32> key_profile(gSavedSettings, "CineLightRigKeyProfile");
    static LLCachedControl<S32> fill_profile(gSavedSettings, "CineLightRigFillProfile");
    static LLCachedControl<S32> rim_profile(gSavedSettings, "CineLightRigRimProfile");
    static LLCachedControl<S32> bg_profile(gSavedSettings, "CineLightRigBgProfile");
    static LLCachedControl<F32> key_ev(gSavedSettings, "CineLightRigKeyEV");
    static LLCachedControl<F32> fill_ev(gSavedSettings, "CineLightRigFillEV");
    static LLCachedControl<F32> rim_ev(gSavedSettings, "CineLightRigRimEV");
    static LLCachedControl<F32> bg_ev(gSavedSettings, "CineLightRigBgEV");
    static LLCachedControl<S32> key_beam(gSavedSettings, "CineLightRigKeyBeam");
    static LLCachedControl<S32> fill_beam(gSavedSettings, "CineLightRigFillBeam");
    static LLCachedControl<S32> rim_beam(gSavedSettings, "CineLightRigRimBeam");
    static LLCachedControl<S32> bg_beam(gSavedSettings, "CineLightRigBgBeam");
    static LLCachedControl<S32> key_gobo(gSavedSettings, "CineLightRigKeyGobo");
    static LLCachedControl<S32> fill_gobo(gSavedSettings, "CineLightRigFillGobo");
    static LLCachedControl<S32> rim_gobo(gSavedSettings, "CineLightRigRimGobo");
    static LLCachedControl<S32> bg_gobo(gSavedSettings, "CineLightRigBgGobo");
    static LLCachedControl<S32> key_gel(gSavedSettings, "CineLightRigKeyGel");
    static LLCachedControl<S32> fill_gel(gSavedSettings, "CineLightRigFillGel");
    static LLCachedControl<S32> rim_gel(gSavedSettings, "CineLightRigRimGel");
    static LLCachedControl<S32> bg_gel(gSavedSettings, "CineLightRigBgGel");
    static LLCachedControl<bool> key_on(gSavedSettings, "CineLightRigKeyOn");
    static LLCachedControl<bool> fill_on(gSavedSettings, "CineLightRigFillOn");
    static LLCachedControl<bool> rim_on(gSavedSettings, "CineLightRigRimOn");
    static LLCachedControl<bool> bg_on(gSavedSettings, "CineLightRigBgOn");
    static LLCachedControl<F32> master_ev(gSavedSettings, "CineLightRigMasterEV");
    static LLCachedControl<F32> master_temp(
        gSavedSettings, "CineLightRigMasterTempMired");
    static LLCachedControl<F32> headroom(gSavedSettings, "CineLightRigHeadroomStops");
    static LLCachedControl<F32> bounce_ratio(gSavedSettings, "CineLightRigBounceRatio");
    static LLCachedControl<F32> transition(gSavedSettings, "CineLightRigTransitionSec");
    static LLCachedControl<bool> bounce_enabled(gSavedSettings, "CineLightRigBounceEnabled");
    static LLCachedControl<bool> power(gSavedSettings, "CineLightRigPower");
    static LLCachedControl<U32> seed(gSavedSettings, "CineLightRigSeed");
    static LLCachedControl<bool> mirror(gSavedSettings, "CineLightRigMirror");
    static LLCachedControl<F32> orbit_yaw(gSavedSettings, "CineLightRigOrbitYaw");
    static LLCachedControl<F32> orbit_pitch(gSavedSettings, "CineLightRigOrbitPitch");
    static LLCachedControl<bool> ratio_lock(
        gSavedSettings, "CineLightRigRatioLock");
    static LLCachedControl<F32> ratio(gSavedSettings, "CineLightRigRatio");

    const F32 yaws[LIGHT_COUNT] = { key_yaw, fill_yaw, rim_yaw, bg_yaw };
    const F32 pitches[LIGHT_COUNT] = {
        key_pitch, fill_pitch, rim_pitch, bg_pitch
    };
    const S32 profiles[LIGHT_COUNT] = {
        key_profile, fill_profile, rim_profile, bg_profile
    };
    const F32 evs[LIGHT_COUNT] = { key_ev, fill_ev, rim_ev, bg_ev };
    const S32 beams[LIGHT_COUNT] = { key_beam, fill_beam, rim_beam, bg_beam };
    const S32 gobos[LIGHT_COUNT] = {
        key_gobo, fill_gobo, rim_gobo, bg_gobo
    };
    const S32 gels[LIGHT_COUNT] = {
        key_gel, fill_gel, rim_gel, bg_gel
    };
    const bool on[LIGHT_COUNT] = { key_on, fill_on, rim_on, bg_on };

    setup.mRadius = radius;
    setup.mRatioLock = ratio_lock;
    setup.mRatioStops = ratio;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = yaws[i];
        setup.mLights[i].mPitchDeg = pitches[i];
        setup.mLights[i].mProfile = profiles[i];
        setup.mLights[i].mEV = evs[i];
        setup.mLights[i].mBeam = beams[i];
        setup.mLights[i].mOn = on[i];
        setup.mLights[i].mGobo = gobos[i];
        setup.mLights[i].mGel = gels[i];
    }

    globals.mMasterEV = master_ev;
    globals.mMasterTempMired = master_temp;
    globals.mHeadroomStops = headroom;
    globals.mBounceRatio = bounce_ratio;
    globals.mTransitionSec = transition;
    globals.mBounceEnabled = bounce_enabled;
    globals.mPower = power;
    globals.mSeed = static_cast<U64>(seed());

    transforms.mMirror = mirror;
    transforms.mYawDeg = orbit_yaw;
    transforms.mPitchDeg = orbit_pitch;

    setup = sanitizeSetup(setup);
    globals = sanitizeGlobals(globals);
    transforms = sanitizeTransforms(transforms);
}

void ALCineLightRig::readSettings(const ALCineLightRigParamBlob& blob,
                                  Setup& setup, Globals& globals,
                                  Transforms& transforms) const
{
    setup.mRadius = blob.mRadius;
    setup.mRatioLock = blob.mRatioLock;
    setup.mRatioStops = blob.mRatio;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        setup.mLights[i].mYawDeg = blob.mLights[i].mYaw;
        setup.mLights[i].mPitchDeg = blob.mLights[i].mPitch;
        setup.mLights[i].mProfile = blob.mLights[i].mProfile;
        setup.mLights[i].mEV = blob.mLights[i].mEV;
        setup.mLights[i].mBeam = blob.mLights[i].mBeam;
        setup.mLights[i].mOn = blob.mLights[i].mOn;
        setup.mLights[i].mGobo = blob.mLights[i].mGobo;
        setup.mLights[i].mGel = blob.mLights[i].mGel;
    }

    globals.mMasterEV = blob.mMasterEV;
    globals.mMasterTempMired = blob.mMasterTempMired;
    globals.mHeadroomStops = blob.mHeadroomStops;
    globals.mBounceRatio = blob.mBounceRatio;
    globals.mTransitionSec = blob.mTransitionSec;
    globals.mBounceEnabled = blob.mBounceEnabled;
    globals.mPower = blob.mPower;
    globals.mSeed = static_cast<U64>(blob.mSeed);

    transforms.mMirror = blob.mMirror;
    transforms.mYawDeg = blob.mOrbitYaw;
    transforms.mPitchDeg = blob.mOrbitPitch;

    setup = sanitizeSetup(setup);
    globals = sanitizeGlobals(globals);
    transforms = sanitizeTransforms(transforms);
}

void ALCineLightRig::writeSetupToSettings(const Setup& input) const
{
    const Setup setup = sanitizeSetup(input);
    gSavedSettings.setF32("CineLightRigRadius", setup.mRadius);
    gSavedSettings.setBOOL("CineLightRigRatioLock", setup.mRatioLock);
    gSavedSettings.setF32("CineLightRigRatio", setup.mRatioStops);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const std::string prefix = std::string("CineLightRig") + ROLE_NAMES[i];
        const LightBase& light = setup.mLights[i];
        gSavedSettings.setF32(prefix + "Yaw", light.mYawDeg);
        gSavedSettings.setF32(prefix + "Pitch", light.mPitchDeg);
        gSavedSettings.setS32(prefix + "Profile", light.mProfile);
        gSavedSettings.setF32(prefix + "EV", light.mEV);
        gSavedSettings.setS32(prefix + "Beam", light.mBeam);
        gSavedSettings.setBOOL(prefix + "On", light.mOn);
        gSavedSettings.setS32(prefix + "Gobo", light.mGobo);
        gSavedSettings.setS32(prefix + "Gel", light.mGel);
    }
}

bool ALCineLightRig::createEmitter(LLViewerRegion* region, bool projector,
                                   const std::string& cookie_setting,
                                   LLPointer<LLVOVolume>& output)
{
    if (!region || LLApp::isExiting())
    {
        return false;
    }

    LLViewerObject* object = gObjectList.createObjectViewer(
        LL_PCODE_VOLUME, region);
    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume)
    {
        if (object)
        {
            object->markDead();
        }
        return false;
    }

    volume->mbCanSelect = false;
    volume->mIsLocalOnly = true;
    volume->mLocalObjectKind =
        LLViewerObject::LOCAL_OBJECT_CINE_RIG_EMITTER;
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

    volume->setScale(LLVector3(EMITTER_SCALE, EMITTER_SCALE, EMITTER_SCALE),
                     false);
    volume->setIsLight(false);
    if (projector)
    {
        volume->setLightTextureID(rigCookie(cookie_setting));
        volume->setSpotLightParams(LLVector3(1.5f, 0.f, 0.f));
    }
    else
    {
        volume->setLightTextureID(LLUUID::null);
    }
    volume->setLightIntensity(0.f);
    if (volume->mDrawable.notNull())
    {
        volume->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
    output = volume;
    return true;
}

void ALCineLightRig::destroyEmitter(LLPointer<LLVOVolume>& emitter)
{
    LLPointer<LLVOVolume> dying = emitter;
    emitter = nullptr;
    if (dying.isNull())
    {
        return;
    }

    const LLUUID id = dying->getID();
    if (LLPipeline::isVolumetricShaftEnabled(id))
    {
        LLPipeline::toggleVolumetricShaft(id);
    }
    if (LLPipeline::isHeroProjector(id))
    {
        LLPipeline::toggleHeroProjector(id);
    }
    if (LLPipeline::isProjectorNoShadow(id))
    {
        LLPipeline::toggleProjectorCastShadows(id);
    }
    LLPipeline::clearProjectorShadowSoftness(id);
    // Always clear light membership while the strong pointer is held. A region
    // teardown may have marked the object dead before the controller observes
    // it, but a surviving drawable must never remain in LLPipeline::mLights.
    dying->setLightIntensity(0.f);
    dying->setIsLight(false);
    if (!dying->isDead())
    {
        dying->markDead();
    }
}

void ALCineLightRig::destroyEmitters()
{
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        destroyEmitter(mProjectors[i]);
    }
    destroyOmnis();
    destroyCatchlight();
    mRegion = nullptr;
}

void ALCineLightRig::destroyOmnis()
{
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        destroyEmitter(mOmnis[i]);
    }
}

void ALCineLightRig::destroyCatchlight()
{
    destroyEmitter(mCatchlight);
}

bool ALCineLightRig::ensureProjectors(const std::string& cookie_setting)
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region || LLApp::isExiting())
    {
        destroyEmitters();
        return false;
    }

    if (mRegion == region && emittersReady(mProjectors, region))
    {
        return true;
    }

    destroyEmitters();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (!createEmitter(region, true, cookie_setting, mProjectors[i]))
        {
            destroyEmitters();
            return false;
        }
    }
    mRegion = region;
    mOmniRetryTicks = 0;
    mCatchlightRetryTicks = 0;
    return true;
}

bool ALCineLightRig::ensureOmnis()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region || region != mRegion || LLApp::isExiting() ||
        !emittersReady(mProjectors, region))
    {
        destroyOmnis();
        return false;
    }
    if (emittersReady(mOmnis, region))
    {
        return true;
    }

    destroyOmnis();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (!createEmitter(region, false, DEFAULT_COOKIE, mOmnis[i]))
        {
            destroyOmnis();
            return false;
        }
    }
    return true;
}

bool ALCineLightRig::ensureCatchlight(
    const std::string& cookie_setting)
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region || region != mRegion || LLApp::isExiting() ||
        !emittersReady(mProjectors, region))
    {
        destroyCatchlight();
        return false;
    }
    if (emitterReady(mCatchlight, region))
    {
        return true;
    }

    destroyCatchlight();
    if (!createEmitter(region, true, cookie_setting, mCatchlight))
    {
        return false;
    }
    // A catchlight is a specular accent, never a shadow-map candidate.
    if (!LLPipeline::isProjectorNoShadow(mCatchlight->getID()))
    {
        LLPipeline::toggleProjectorCastShadows(mCatchlight->getID());
    }
    return true;
}

void ALCineLightRig::updateShadowPolicy(S32 shadow_mode)
{
    const S32 mode = std::clamp(shadow_mode, 0, 2);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (mProjectors[i].isNull() || mProjectors[i]->isDead())
        {
            continue;
        }
        const LLUUID id = mProjectors[i]->getID();
        const bool suppress = mode == 0 || (mode == 1 && i != 0);
        if (LLPipeline::isProjectorNoShadow(id) != suppress)
        {
            LLPipeline::toggleProjectorCastShadows(id);
        }
    }
}

void ALCineLightRig::updateProjectorFlags()
{
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (mProjectors[i].isNull() || mProjectors[i]->isDead())
        {
            continue;
        }
        const LLUUID id = mProjectors[i]->getID();
        if (LLPipeline::isVolumetricShaftEnabled(id) != mShaftEnabled[i])
        {
            LLPipeline::toggleVolumetricShaft(id);
        }
        if (LLPipeline::isHeroProjector(id) != mHeroEnabled[i])
        {
            LLPipeline::toggleHeroProjector(id);
        }
        const F32 softness = std::isfinite(mShadowSoftness[i])
            ? std::clamp(mShadowSoftness[i], 0.f, 8.f) : 0.f;
        if (softness > 0.f)
        {
            LLPipeline::setProjectorShadowSoftness(id, softness);
        }
        else
        {
            LLPipeline::clearProjectorShadowSoftness(id);
        }
    }

    // [DIAG cine-shadow] throttled runtime dump of the rig's shadow/shaft
    // intent, ~once/2s. Answers: does each projector exist, is it flagged
    // NoShadow (=> no shadow slot => no shaft), and is its shaft toggled on.
    static S32 s_diag = 0;
    if ((s_diag++ % 120) == 0)
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            const bool present = mProjectors[i].notNull() &&
                                 !mProjectors[i]->isDead();
            if (!present)
            {
                LL_INFOS("CineRigShadow") << "light " << i
                    << " projector ABSENT" << LL_ENDL;
                continue;
            }
            const LLUUID id = mProjectors[i]->getID();
            LL_INFOS("CineRigShadow") << "light " << i
                << " id=" << id.asString().substr(0, 8)
                << " noShadow=" << LLPipeline::isProjectorNoShadow(id)
                << " shaftPipe=" << LLPipeline::isVolumetricShaftEnabled(id)
                << " shaftWant=" << mShaftEnabled[i]
                << " hero=" << mHeroEnabled[i] << LL_ENDL;
        }
    }
}

void ALCineLightRig::setEmittersDark()
{
    std::memset(&mLastFrame, 0, sizeof(mLastFrame));
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        LLVOVolume* emitters[2] = {
            mProjectors[i].get(), mOmnis[i].get()
        };
        for (LLVOVolume* emitter : emitters)
        {
            if (!emitter || emitter->isDead())
            {
                continue;
            }
            emitter->setLightIntensity(0.f);
            emitter->setIsLight(false);
            emitter->setScale(
                LLVector3(EMITTER_SCALE, EMITTER_SCALE, EMITTER_SCALE), false);
            if (emitter->mDrawable.notNull())
            {
                emitter->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
            }
        }
    }
    if (mCatchlight.notNull() && !mCatchlight->isDead())
    {
        mCatchlight->setLightIntensity(0.f);
        mCatchlight->setIsLight(false);
        mCatchlight->setScale(
            LLVector3(EMITTER_SCALE, EMITTER_SCALE, EMITTER_SCALE), false);
        if (mCatchlight->mDrawable.notNull())
        {
            mCatchlight->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
        }
    }
}

void ALCineLightRig::evaluateTransition(F64 presentation_time)
{
    if (!mTransitionActive)
    {
        return;
    }
    if (!std::isfinite(presentation_time) ||
        presentation_time < mTransitionStartTime ||
        mTransitionDuration <= 0.f)
    {
        copyLights(mTransitionTarget, mCurrentLive);
        mCurrentRadius = mTransitionRadiusTarget;
        mTransitionActive = false;
        return;
    }
    const F32 t = static_cast<F32>(
        (presentation_time - mTransitionStartTime) / mTransitionDuration);
    if (t >= 1.f)
    {
        copyLights(mTransitionTarget, mCurrentLive);
        mCurrentRadius = mTransitionRadiusTarget;
        mTransitionActive = false;
        return;
    }
    const F32 eased = ease(t);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        mCurrentLive[i] = blendLight(
            mTransitionStart[i], mTransitionTarget[i], eased);
    }
    mCurrentRadius = mTransitionRadiusStart +
        (mTransitionRadiusTarget - mTransitionRadiusStart) * eased;
}

void ALCineLightRig::updateTransition(
    const LightBase target[LIGHT_COUNT], F32 target_radius, F32 duration,
    F64 presentation_time)
{
    if (!mHaveTarget)
    {
        copyLights(target, mTransitionTarget);
        copyLights(target, mCurrentLive);
        mTransitionRadiusTarget = target_radius;
        mCurrentRadius = target_radius;
        mHaveTarget = true;
        mTransitionActive = false;
        return;
    }

    if (!sameLights(target, mTransitionTarget) ||
        target_radius != mTransitionRadiusTarget)
    {
        evaluateTransition(presentation_time);
        copyLights(mCurrentLive, mTransitionStart);
        copyLights(target, mTransitionTarget);
        mTransitionRadiusStart = mCurrentRadius;
        mTransitionRadiusTarget = target_radius;
        mTransitionDuration = duration;
        mTransitionStartTime = presentation_time;
        mTransitionActive = duration > 0.f;
        if (!mTransitionActive)
        {
            copyLights(target, mCurrentLive);
            mCurrentRadius = target_radius;
        }
    }
    evaluateTransition(presentation_time);
}

void ALCineLightRig::applyFrame(const RigFrame& frame,
                                const LLVector3d& rig_centre,
                                const LLVector3d& aim_centre,
                                F32 nominal_radius, F32 subject_scale,
                                const std::string& cookie_setting)
{
    const F32 emitter_edge = emitterBoxEdge(
        nominal_radius, subject_scale);
    const LLVector3 emitter_scale(emitter_edge, emitter_edge, emitter_edge);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const EmitterState& state = frame.mProj[i];
        LLVOVolume* projector = mProjectors[i].get();
        if (projector && !projector->isDead())
        {
            LLVector3d position = rig_centre;
            position.mdV[VX] += state.mOffX;
            position.mdV[VY] += state.mOffY;
            position.mdV[VZ] += state.mOffZ;
            LLVector3 aim(
                static_cast<F32>(aim_centre.mdV[VX] - position.mdV[VX]),
                static_cast<F32>(aim_centre.mdV[VY] - position.mdV[VY]),
                static_cast<F32>(aim_centre.mdV[VZ] - position.mdV[VZ]));
            if (aim.normalize() <= F_APPROXIMATELY_ZERO)
            {
                aim.set(state.mAimX, state.mAimY, state.mAimZ);
            }
            LLQuaternion rotation;
            rotation.shortestArc(LLVector3(0.f, 0.f, -1.f), aim);

            projector->setPositionGlobal(position, false);
            projector->setRotation(rotation, false);
            projector->setScale(emitter_scale, false);
            const LLUUID cookie = rigGoboTexture(
                state.mGobo, cookie_setting);
            if (projector->getLightTextureID() != cookie)
            {
                projector->setLightTextureID(cookie);
            }
            projector->setLightSRGBColor(
                LLColor3(state.mSR, state.mSG, state.mSB));
            projector->setLightIntensity(state.mOn ? state.mIntensity : 0.f);
            projector->setLightRadius(state.mLightRadius);
            projector->setLightFalloff(state.mFalloff);
            projector->setSpotLightParams(
                LLVector3(state.mFovRad, 0.f, 0.f));
            projector->setIsLight(state.mOn);
            if (projector->mDrawable.notNull())
            {
                gPipeline.updateMoveNormalAsync(projector->mDrawable);
                projector->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
                if (state.mOn)
                {
                    // [Cine rig] Seed the spot-shadow priority in the rig tick,
                    // before the render's per-frame shadow-slot auction, so a
                    // freshly enabled projector never ranks with a stale 0 and
                    // lose its slot until a camera move (the zoom-to-activate
                    // bug). Pairs with the isCineRigEmitter() priority branch in
                    // LLVOVolume::updateSpotLightPriority.
                    projector->updateSpotLightPriority();
                }
            }
        }

        LLVOVolume* omni = mOmnis[i].get();
        const EmitterState& omni_state = frame.mOmni[i];
        if (omni && !omni->isDead())
        {
            LLVector3d position = rig_centre;
            position.mdV[VX] += omni_state.mOffX;
            position.mdV[VY] += omni_state.mOffY;
            position.mdV[VZ] += omni_state.mOffZ;
            omni->setPositionGlobal(position, false);
            omni->setRotation(LLQuaternion(), false);
            omni->setScale(emitter_scale, false);
            if (omni->getLightTextureID().notNull())
            {
                omni->setLightTextureID(LLUUID::null);
            }
            omni->setLightSRGBColor(
                LLColor3(omni_state.mSR, omni_state.mSG, omni_state.mSB));
            omni->setLightIntensity(
                omni_state.mOn ? omni_state.mIntensity : 0.f);
            omni->setLightRadius(omni_state.mLightRadius);
            omni->setLightFalloff(omni_state.mFalloff);
            omni->setIsLight(omni_state.mOn);
            if (omni->mDrawable.notNull())
            {
                gPipeline.updateMoveNormalAsync(omni->mDrawable);
                omni->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
            }
        }
    }
}

void ALCineLightRig::applyCatchlight(
    LLVOAvatar* avatar, F32 subject_scale, F32 master_temp_mired,
    F32 ev, F32 size, F32 angle_degrees,
    const std::string& cookie_setting)
{
    LLVOVolume* emitter = mCatchlight.get();
    if (!avatar || !emitter || emitter->isDead())
    {
        return;
    }

    LLVector3 eye;
    LLJoint* left_eye = avatar->getJoint("mEyeLeft");
    LLJoint* right_eye = avatar->getJoint("mEyeRight");
    if (left_eye && right_eye)
    {
        eye = (left_eye->getWorldPosition() + right_eye->getWorldPosition()) *
              0.5f;
        eye = scaledPoint(avatar, eye, subject_scale);
    }
    else if (LLJoint* head = avatar->getJoint("mHead"))
    {
        const LLVector3 forward =
            LLVector3::x_axis * head->getWorldRotation();
        eye = scaledPoint(avatar,
            head->getWorldPosition() + forward * 0.08f, subject_scale);
    }
    else
    {
        const LLVector3 forward =
            LLVector3::x_axis * avatar->getRenderRotation();
        eye = scaledPoint(avatar,
            avatar->getRenderPosition() + LLVector3(0.f, 0.f, 1.65f) +
                forward * 0.08f,
            subject_scale);
    }

    const LLVector3 camera = LLViewerCamera::getInstance()->getOrigin();
    LLVector3 eye_to_camera = camera - eye;
    if (!eye.isFinite() || !camera.isFinite() ||
        eye_to_camera.normalize() <= F_APPROXIMATELY_ZERO)
    {
        destroyCatchlight();
        return;
    }
    LLVector3 right = eye_to_camera % LLVector3::z_axis;
    if (right.normalize() <= F_APPROXIMATELY_ZERO)
    {
        right = LLVector3::x_axis;
    }
    LLVector3 up = right % eye_to_camera;
    if (up.normalize() <= F_APPROXIMATELY_ZERO)
    {
        up = LLVector3::z_axis;
    }

    F32 radial[2];
    catchlightRadialOffset(subject_scale, angle_degrees, radial);
    const F32 scale = sanitizeSubjectScale(subject_scale);
    // The source sits just camera-side of the eyes. Its optical axis is the
    // camera-to-eye half of the eye/camera axis, with the radial trim placing
    // the sparkle at the traditional upper key-side clock position.
    const LLVector3 position_agent = eye + eye_to_camera * (0.10f * scale) +
                                     right * radial[0] + up * radial[1];
    LLVector3 aim = eye - position_agent;
    if (!position_agent.isFinite() ||
        aim.normalize() <= F_APPROXIMATELY_ZERO)
    {
        destroyCatchlight();
        return;
    }
    LLQuaternion rotation;
    rotation.shortestArc(LLVector3(0.f, 0.f, -1.f), aim);

    F32 colour[3] = { 1.f, 1.f, 1.f };
    if (master_temp_mired != 0.f)
    {
        F32 gain[3];
        masterTempGain(master_temp_mired, gain);
        for (S32 channel = 0; channel < 3; ++channel)
        {
            const F32 linear = std::clamp(gain[channel], 0.f, 1.f);
            colour[channel] = linear <= 0.0031308f
                ? linear * 12.92f
                : 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
        }
    }

    emitter->setPositionGlobal(
        gAgent.getPosGlobalFromAgent(position_agent), false);
    emitter->setRotation(rotation, false);
    const F32 box_edge = std::clamp(0.04f * scale, 0.01f, 0.25f);
    emitter->setScale(LLVector3(box_edge, box_edge, box_edge), false);
    const LLUUID cookie = rigCookie(cookie_setting);
    if (emitter->getLightTextureID() != cookie)
    {
        emitter->setLightTextureID(cookie);
    }
    emitter->setLightSRGBColor(LLColor3(colour[0], colour[1], colour[2]));
    emitter->setLightIntensity(intensityFromEV(
        std::clamp(std::isfinite(ev) ? ev : -0.5f, -8.f, 8.f),
        0.f, nullptr));
    const F32 safe_size = std::clamp(
        std::isfinite(size) ? size : 0.35f, 0.15f, 2.f);
    // Keep the smallest supported source large enough to reach the eye from
    // the 10 cm axial + 10 cm radial placement, at every finalized scale.
    emitter->setLightRadius(std::clamp(safe_size * scale, 0.01f, 300.f));
    emitter->setLightFalloff(0.25f);
    emitter->setSpotLightParams(LLVector3(0.7f, 0.f, 0.f));
    emitter->setIsLight(true);
    if (!LLPipeline::isProjectorNoShadow(emitter->getID()))
    {
        LLPipeline::toggleProjectorCastShadows(emitter->getID());
    }
    if (emitter->mDrawable.notNull())
    {
        gPipeline.updateMoveNormalAsync(emitter->mDrawable);
        emitter->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
}

void ALCineLightRig::tickSelected(F64 presentation_time, bool owns_shadows)
{
    static LLCachedControl<bool> enabled(
        gSavedSettings, "CineLightRigEnabled", false);
    static LLCachedControl<S32> fx_setting(
        gSavedSettings, "CineLightRigFX");
    static LLCachedControl<F32> offset_z_setting(
        gSavedSettings, "CineLightRigOffsetZ");
    static LLCachedControl<F32> damping_setting(
        gSavedSettings, "CineLightRigDamping");
    static LLCachedControl<S32> track_mode_setting(
        gSavedSettings, "CineLightRigTrackMode");
    static LLCachedControl<bool> scale_aware_setting(
        gSavedSettings, "CineLightRigScaleAware", true);
    static LLCachedControl<S32> shadow_mode(
        gSavedSettings, "CineLightRigShadowMode");
    static LLCachedControl<std::string> cookie_setting(
        gSavedSettings, "CineLightRigCookieUUID");
    static LLCachedControl<bool> catchlight_enabled(
        gSavedSettings, "CineLightRigCatchlight");
    static LLCachedControl<F32> catchlight_ev(
        gSavedSettings, "CineLightRigCatchlightEV");
    static LLCachedControl<F32> catchlight_size(
        gSavedSettings, "CineLightRigCatchlightSize");
    static LLCachedControl<F32> catchlight_angle(
        gSavedSettings, "CineLightRigCatchlightAngle");
    static LLCachedControl<F32> key_shadow_soft(
        gSavedSettings, "CineLightRigKeyShadowSoft");
    static LLCachedControl<F32> fill_shadow_soft(
        gSavedSettings, "CineLightRigFillShadowSoft");
    static LLCachedControl<F32> rim_shadow_soft(
        gSavedSettings, "CineLightRigRimShadowSoft");
    static LLCachedControl<F32> bg_shadow_soft(
        gSavedSettings, "CineLightRigBgShadowSoft");
    if (LLApp::isExiting())
    {
        shutdown();
        return;
    }
    if (!enabled)
    {
        mLastResolvedGroupSlots = 0;
        shutdown();
        return;
    }
    if (!std::isfinite(presentation_time) || presentation_time < 0.0)
    {
        if (!catchlight_enabled)
        {
            destroyCatchlight();
            mCatchlightRetryTicks = 0;
        }
        setEmittersDark();
        return;
    }

    Setup setup;
    Globals globals;
    Transforms transforms;
    readSettings(setup, globals, transforms);
    if (!globals.mPower)
    {
        mLastResolvedGroupSlots = 0;
        destroyEmitters();
        std::memset(&mLastFrame, 0, sizeof(mLastFrame));
        mHaveSmoothedCentre = false;
        mSmoothedScale = 1.f;
        mProjectorRetryTicks = 0;
        mOmniRetryTicks = 0;
        mCatchlightRetryTicks = 0;
        mLastPresentationTime = presentation_time;
        return;
    }

    const F32 shadow_softness[LIGHT_COUNT] = {
        key_shadow_soft, fill_shadow_soft, rim_shadow_soft, bg_shadow_soft
    };
    tickShared(presentation_time, owns_shadows, fx_setting, offset_z_setting,
               damping_setting, track_mode_setting, scale_aware_setting,
               shadow_mode, catchlight_enabled, catchlight_ev,
               catchlight_size, catchlight_angle, shadow_softness,
               cookie_setting(), setup, globals, transforms);
}

void ALCineLightRig::tickFromBlob(const ALCineLightRigParamBlob& blob,
                                  F64 presentation_time, bool owns_shadows)
{
    const bool enabled = blob.mEnabled;
    const S32 fx_setting = blob.mFX;
    const F32 offset_z_setting = blob.mOffsetZ;
    const F32 damping_setting = blob.mDamping;
    const S32 track_mode_setting = blob.mTrackMode;
    const bool scale_aware_setting = blob.mScaleAware;
    const S32 shadow_mode = blob.mShadowMode;
    const bool catchlight_enabled = blob.mCatchlight;
    const F32 catchlight_ev = blob.mCatchlightEV;
    const F32 catchlight_size = blob.mCatchlightSize;
    const F32 catchlight_angle = blob.mCatchlightAngle;
    F32 shadow_softness[LIGHT_COUNT];
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        shadow_softness[i] = blob.mLights[i].mShadowSoft;
    }
    const std::string& cookie_setting = blob.mCookieUUID;
    if (LLApp::isExiting())
    {
        shutdown();
        return;
    }
    if (!enabled)
    {
        mLastResolvedGroupSlots = 0;
        shutdown();
        return;
    }
    if (!std::isfinite(presentation_time) || presentation_time < 0.0)
    {
        if (!catchlight_enabled)
        {
            destroyCatchlight();
            mCatchlightRetryTicks = 0;
        }
        setEmittersDark();
        return;
    }

    Setup setup;
    Globals globals;
    Transforms transforms;
    readSettings(blob, setup, globals, transforms);
    if (!globals.mPower)
    {
        mLastResolvedGroupSlots = 0;
        destroyEmitters();
        std::memset(&mLastFrame, 0, sizeof(mLastFrame));
        mHaveSmoothedCentre = false;
        mSmoothedScale = 1.f;
        mProjectorRetryTicks = 0;
        mOmniRetryTicks = 0;
        mCatchlightRetryTicks = 0;
        mLastPresentationTime = presentation_time;
        return;
    }

    tickShared(presentation_time, owns_shadows, fx_setting, offset_z_setting,
               damping_setting, track_mode_setting, scale_aware_setting,
               shadow_mode, catchlight_enabled, catchlight_ev,
               catchlight_size, catchlight_angle, shadow_softness,
               cookie_setting, setup, globals, transforms);
}

void ALCineLightRig::tickShared(
    F64 presentation_time, bool owns_shadows, S32 fx_setting,
    F32 offset_z_setting, F32 damping_setting, S32 track_mode_setting,
    bool scale_aware_setting, S32 shadow_mode,
    bool catchlight_enabled, F32 catchlight_ev, F32 catchlight_size,
    F32 catchlight_angle, const F32 shadow_softness[LIGHT_COUNT],
    const std::string& cookie_setting, Setup& setup, Globals& globals,
    Transforms& transforms)
{

    LLVOAvatar* group_members[GROUP_MAX_MEMBERS] = {};
    U32 resolved_group_slots = 0;
    S32 group_count = 0;
    if (mGroupEnabled)
    {
        group_count = gatherGroupMembers(
            mGroupSlots, group_members, resolved_group_slots);
        mLastResolvedGroupSlots = resolved_group_slots;
    }
    else
    {
        mLastResolvedGroupSlots = 0;
    }

    LLVOAvatar* avatar = mGroupEnabled
        ? (group_count > 0 ? group_members[0] : nullptr)
        : resolveSlotAvatar();
    if (!avatar)
    {
        destroyEmitters();
        std::memset(&mLastFrame, 0, sizeof(mLastFrame));
        mHaveSmoothedCentre = false;
        mSmoothedScale = 1.f;
        mProjectorRetryTicks = 0;
        mOmniRetryTicks = 0;
        mCatchlightRetryTicks = 0;
        mLastPresentationTime = presentation_time;
        return;
    }

    bool aggregate_group = false;
    LLVector3 aggregate_centre_agent;
    F32 aggregate_subject_scale = 1.f;
    if (mGroupEnabled && group_count > 1)
    {
        const S32 group_track_mode = track_mode_setting == 1 ? 1 : 0;
        F32 points[GROUP_MAX_MEMBERS][3];
        F32 member_scales[GROUP_MAX_MEMBERS];
        S32 valid_count = 0;
        U32 valid_slots = 0;
        S32 member_index = 0;
        for (S32 slot = 0; slot < GROUP_MAX_MEMBERS; ++slot)
        {
            const U32 slot_bit = 1u << slot;
            if (!(resolved_group_slots & slot_bit))
            {
                continue;
            }

            LLVOAvatar* member = group_members[member_index++];
            const F32 member_scale = sanitizeSubjectScale(
                scale_aware_setting ? member->getUniformScale() : 1.f);
            LLVector3 point;
            if (!memberTrackPoint(
                    member, group_track_mode, member_scale, point))
            {
                continue;
            }
            group_members[valid_count] = member;
            member_scales[valid_count] = member_scale;
            points[valid_count][0] = point.mV[VX];
            points[valid_count][1] = point.mV[VY];
            points[valid_count][2] = point.mV[VZ];
            valid_slots |= slot_bit;
            ++valid_count;
        }
        group_count = valid_count;
        resolved_group_slots = valid_slots;
        mLastResolvedGroupSlots = valid_slots;
        if (group_count == 0)
        {
            destroyEmitters();
            std::memset(&mLastFrame, 0, sizeof(mLastFrame));
            mHaveSmoothedCentre = false;
            mSmoothedScale = 1.f;
            mProjectorRetryTicks = 0;
            mOmniRetryTicks = 0;
            mCatchlightRetryTicks = 0;
            mLastPresentationTime = presentation_time;
            return;
        }
        avatar = group_members[0];
        if (group_count > 1)
        {
            F32 centre[3];
            groupBoundsCentre(points, group_count, centre);
            aggregate_centre_agent.set(centre[0], centre[1], centre[2]);
            aggregate_subject_scale = groupSubjectScale(
                points, member_scales, group_count, centre, setup.mRadius);
            aggregate_group = true;
        }
    }

    // The skeleton root is the rendered body facing. Actor Mover can override
    // it without changing the viewer-object rotation; its world axes are the
    // same region X/Y frame used by the rig's global emitter offsets.
    if (LLJoint* root = avatar->getRootJoint())
    {
        const LLVector3 forward =
            LLVector3(1.f, 0.f, 0.f) * root->getWorldRotation();
        F32 facing = atan2f(forward.mV[VY], forward.mV[VX]) * RAD_TO_DEG;
        if (!std::isfinite(facing))
        {
            facing = 0.f;
        }
        transforms.mFacingAzimuthDeg = facing;
    }

    if (aggregate_group)
    {
        globals.mSubjectScale = aggregate_subject_scale;
    }
    else
    {
        // Group-off and one-survivor shots stay on the shipped single-avatar
        // scale path; no aggregate model arithmetic reaches this branch.
        globals.mSubjectScale = scale_aware_setting
            ? avatar->getUniformScale() : 1.f;
    }
    globals = sanitizeGlobals(globals);
    const F32 subject_scale = globals.mSubjectScale;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        mShadowSoftness[i] = shadow_softness ? shadow_softness[i] : 0.f;
    }

    LLViewerRegion* region = gAgent.getRegion();
    if (mRegion && mRegion != region)
    {
        destroyEmitters();
        mProjectorRetryTicks = 0;
        mOmniRetryTicks = 0;
        mCatchlightRetryTicks = 0;
    }

    // Disable is an immediate lifecycle transition, even if the four main
    // projectors are currently in their retry window.
    if (!catchlight_enabled)
    {
        destroyCatchlight();
        mCatchlightRetryTicks = 0;
    }

    bool projectors_ready = emittersReady(mProjectors, region);
    if (!projectors_ready)
    {
        if (mProjectorRetryTicks > 0)
        {
            --mProjectorRetryTicks;
        }
        else
        {
            projectors_ready = ensureProjectors(cookie_setting);
            if (!projectors_ready)
            {
                mProjectorRetryTicks = EMITTER_RETRY_TICKS;
            }
        }
    }
    else
    {
        mProjectorRetryTicks = 0;
    }
    if (!projectors_ready)
    {
        setEmittersDark();
        mLastPresentationTime = presentation_time;
        return;
    }

    if (!globals.mBounceEnabled)
    {
        destroyOmnis();
        mOmniRetryTicks = 0;
    }
    else if (!emittersReady(mOmnis, region))
    {
        if (mOmniRetryTicks > 0)
        {
            --mOmniRetryTicks;
        }
        else if (!ensureOmnis())
        {
            mOmniRetryTicks = EMITTER_RETRY_TICKS;
        }
    }
    else
    {
        mOmniRetryTicks = 0;
    }

    if (catchlight_enabled && !emitterReady(mCatchlight, region))
    {
        if (mCatchlightRetryTicks > 0)
        {
            --mCatchlightRetryTicks;
        }
        else if (!ensureCatchlight(cookie_setting))
        {
            mCatchlightRetryTicks = EMITTER_RETRY_TICKS;
        }
    }
    else if (catchlight_enabled)
    {
        mCatchlightRetryTicks = 0;
    }

    const S32 requested_fx = std::clamp(
        static_cast<S32>(fx_setting), -1, FX_COUNT - 1);
    if (requested_fx != mActiveFX)
    {
        mTransitionActive = false;
        mHaveTarget = false;
        mActiveFX = requested_fx;
        if (mActiveFX >= 0)
        {
            const F64 phase = mPendingFXId == mActiveFX &&
                              mPendingFXPhase >= 0.0
                ? mPendingFXPhase : 0.0;
            mFXStart = presentation_time - phase;
        }
        mPendingFXPhase = -1.0;
        mPendingFXId = -1;
    }

    LLVector3 centre_agent;
    const S32 track_mode = track_mode_setting == 1 ? 1 : 0;
    if (aggregate_group)
    {
        centre_agent = aggregate_centre_agent;
    }
    else if (!memberTrackPoint(
                 avatar, track_mode, subject_scale, centre_agent))
    {
        if (mGroupEnabled)
        {
            mLastResolvedGroupSlots = 0;
            destroyEmitters();
            std::memset(&mLastFrame, 0, sizeof(mLastFrame));
            mHaveSmoothedCentre = false;
            mSmoothedScale = 1.f;
            mProjectorRetryTicks = 0;
            mOmniRetryTicks = 0;
            mCatchlightRetryTicks = 0;
            mLastPresentationTime = presentation_time;
        }
        return;
    }
    F32 offset_z = offset_z_setting;
    if (!std::isfinite(offset_z))
    {
        offset_z = 0.f;
    }
    centre_agent.mV[VZ] +=
        std::clamp(offset_z, -10.f, 10.f) * subject_scale;
    const LLVector3d true_centre = gAgent.getPosGlobalFromAgent(centre_agent);

    F32 damping = damping_setting;
    damping = std::isfinite(damping)
        ? std::clamp(damping, 0.f, 10.f) : 0.f;
    if (!mHaveSmoothedCentre || damping <= 0.f ||
        mLastPresentationTime < 0.0 ||
        presentation_time < mLastPresentationTime)
    {
        mSmoothedCentre = true_centre;
        mSmoothedScale = subject_scale;
        mHaveSmoothedCentre = true;
    }
    else
    {
        const F64 dt = presentation_time - mLastPresentationTime;
        const F64 alpha = 1.0 - std::exp(-dt / (F64)damping);
        mSmoothedCentre.mdV[VX] +=
            (true_centre.mdV[VX] - mSmoothedCentre.mdV[VX]) * alpha;
        mSmoothedCentre.mdV[VY] +=
            (true_centre.mdV[VY] - mSmoothedCentre.mdV[VY]) * alpha;
        mSmoothedCentre.mdV[VZ] +=
            (true_centre.mdV[VZ] - mSmoothedCentre.mdV[VZ]) * alpha;
        mSmoothedScale += static_cast<F32>(
            (subject_scale - mSmoothedScale) * alpha);
    }

    LightBase desired[LIGHT_COUNT];
    if (mActiveFX >= 0)
    {
        LightBase fx_base[LIGHT_COUNT];
        evalFX(mActiveFX, globals.mSeed,
               std::max(0.0, presentation_time - mFXStart), fx_base);
        Setup fx_setup;
        fx_setup.mRadius = setup.mRadius;
        fx_setup.mRatioLock = setup.mRatioLock;
        fx_setup.mRatioStops = setup.mRatioStops;
        copyLights(fx_base, fx_setup.mLights);
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            // FX owns the animated profile/pose, while the operator's gel is
            // a physical modifier on that light and remains layered above it.
            fx_setup.mLights[i].mGel = setup.mLights[i].mGel;
        }
        computeLive(fx_setup, transforms, desired);
        copyLights(desired, mCurrentLive);
        copyLights(desired, mTransitionTarget);
        mCurrentRadius = setup.mRadius;
        mTransitionRadiusTarget = setup.mRadius;
        mHaveTarget = true;
        mTransitionActive = false;
    }
    else
    {
        computeLive(setup, transforms, desired);
        updateTransition(desired, setup.mRadius, globals.mTransitionSec,
                         presentation_time);
    }

    globals.mSubjectScale = mSmoothedScale;
    render(mCurrentRadius, mCurrentLive, globals, mLastFrame);
    applyFrame(mLastFrame, mSmoothedCentre, true_centre,
               mCurrentRadius, globals.mSubjectScale, cookie_setting);
    if (catchlight_enabled && emitterReady(mCatchlight, region))
    {
        // A group rig's representative scale includes member spread and is not
        // the clone scale of the avatar whose eye joints anchor the catchlight.
        // Preserve the shipped single-anchor path exactly; only aggregate
        // groups substitute the chosen target avatar's own scale.
        const F32 catchlight_subject_scale = aggregate_group
            ? sanitizeSubjectScale(avatar->getUniformScale())
            : globals.mSubjectScale;
        applyCatchlight(avatar, catchlight_subject_scale,
                        globals.mMasterTempMired, catchlight_ev,
                        catchlight_size, catchlight_angle, cookie_setting);
    }
    // Every enabled rig manages its OWN projectors' shadow casting per its own
    // ShadowMode. The deferred pipeline already caps the total at
    // MAX_SPOT_SHADOWS (10), ranking rig emitters by radius ahead of world
    // projectors' existing on-screen-size order. The old owns_shadows gate
    // starved multi-light setups: volumetric shafts require a shadow slot,
    // so a suppressed projector loses BOTH its shadow and its shaft. Let the
    // pipeline arbitrate; focus-priority is a future soft bias, not a hard cut.
    (void)owns_shadows;
    updateShadowPolicy(shadow_mode);
    updateProjectorFlags();
    mLastPresentationTime = presentation_time;
}

void ALCineLightRig::renderGizmo() const
{
    static LLCachedControl<bool> gizmo(
        gSavedSettings, "CineLightRigGizmo", false);
    if (!gizmo)
    {
        return;
    }

    static LLCachedControl<bool> enabled(
        gSavedSettings, "CineLightRigEnabled", false);
    if (!enabled || !mHaveSmoothedCentre)
    {
        return;
    }

    bool any_light = false;
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        if (mLastFrame.mProj[i].mOn && mProjectors[i].notNull() &&
            !mProjectors[i]->isDead())
        {
            any_light = true;
            break;
        }
    }
    if (!any_light)
    {
        return;
    }

    const LLVector3 rig_centre =
        gAgent.getPosAgentFromGlobal(mSmoothedCentre);
    if (!rig_centre.isFinite())
    {
        return;
    }

    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.setLineWidth(2.f);

    gGL.begin(LLRender::LINES);
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        const EmitterState& state = mLastFrame.mProj[i];
        LLVOVolume* projector = mProjectors[i].get();
        if (!state.mOn || !projector || projector->isDead())
        {
            continue;
        }

        const LLVector3 origin = projector->getRenderPosition();
        const LLQuaternion rotation = projector->getRenderRotation();
        LLVector3 forward = LLVector3(0.f, 0.f, -1.f) * rotation;
        LLVector3 up = LLVector3::y_axis * rotation;
        LLVector3 right = forward % up;
        if (!origin.isFinite() || !forward.isFinite() || !up.isFinite() ||
            forward.normVec() <= F_ALMOST_ZERO ||
            up.normVec() <= F_ALMOST_ZERO ||
            right.normVec() <= F_ALMOST_ZERO)
        {
            continue;
        }
        up = right % forward;
        up.normVec();

        const F32 fov = state.mFovRad;
        if (!std::isfinite(fov) || fov <= 0.f || fov >= F_PI)
        {
            continue;
        }
        const F32 centre_distance = dist_vec(origin, rig_centre);
        const F32 cone_length = std::clamp(
            centre_distance * 0.35f, 0.35f, 1.5f);
        const F32 cone_radius = cone_length * tanf(fov * 0.5f);
        if (!std::isfinite(cone_radius))
        {
            continue;
        }
        const LLVector3 ring_centre = origin + forward * cone_length;

        gGL.color4fv(GIZMO_COLORS[i].mV);

        // The centre line makes the orbit relationship explicit; the cone
        // itself follows the live projector rotation and therefore its aim.
        gGL.vertex3fv(origin.mV);
        gGL.vertex3fv(rig_centre.mV);

        for (S32 segment = 0; segment < GIZMO_SEGMENTS; ++segment)
        {
            const F32 angle0 = F_TWO_PI *
                static_cast<F32>(segment) / GIZMO_SEGMENTS;
            const F32 angle1 = F_TWO_PI *
                static_cast<F32>(segment + 1) / GIZMO_SEGMENTS;
            const LLVector3 point0 = ring_centre +
                right * (cosf(angle0) * cone_radius) +
                up * (sinf(angle0) * cone_radius);
            const LLVector3 point1 = ring_centre +
                right * (cosf(angle1) * cone_radius) +
                up * (sinf(angle1) * cone_radius);
            gGL.vertex3fv(point0.mV);
            gGL.vertex3fv(point1.mV);
            if ((segment & 3) == 0)
            {
                gGL.vertex3fv(origin.mV);
                gGL.vertex3fv(point0.mV);
            }
        }
    }
    gGL.end();
    gGL.flush();
    gGL.setLineWidth(1.f);
}

bool ALCineLightRig::isClipped(S32 light) const
{
    return light >= 0 && light < LIGHT_COUNT &&
           mLastFrame.mProj[light].mClipped;
}

void ALCineLightRig::shutdown()
{
    mLastResolvedGroupSlots = 0;
    destroyEmitters();
    mHaveSmoothedCentre = false;
    mSmoothedScale = 1.f;
    mHaveTarget = false;
    mTransitionActive = false;
    mActiveFX = -1;
    mPendingFXPhase = -1.0;
    mPendingFXId = -1;
    mLastPresentationTime = -1.0;
    mProjectorRetryTicks = 0;
    mOmniRetryTicks = 0;
    mCatchlightRetryTicks = 0;
    std::memset(&mLastFrame, 0, sizeof(mLastFrame));
}

//static
std::string ALCineLightRig::presetsDir()
{
    const std::string directory = gDirUtilp->getExpandedFilename(
        LL_PATH_USER_SETTINGS, PRESET_SUBDIR);
    if (!gDirUtilp->fileExists(directory))
    {
        LLFile::mkdir(directory);
    }
    return directory;
}

//static
std::string ALCineLightRig::presetPath(const std::string& name)
{
    return gDirUtilp->add(presetsDir(), LLURI::escape(name) + ".xml");
}

//static
const std::vector<ALCineLightRig::MasterSetup>&
ALCineLightRig::masterSetups()
{
    static const std::vector<MasterSetup> masters = []()
    {
        std::vector<MasterSetup> loaded;
        loaded.push_back({
            CLASSIC_SETUP_NAME,
            "The LSL boot rig; neutral daylight coverage.",
            classicSetup()
        });

        const std::string path = gDirUtilp->getExpandedFilename(
            LL_PATH_APP_SETTINGS, MASTER_PRESET_FILE);
        llifstream input(path.c_str());
        if (!input.is_open())
        {
            LL_WARNS("CineLightRig")
                << "Could not open bundled master setup library " << path
                << "; using compiled Classic only" << LL_ENDL;
        }
        else
        {
            LLSD library;
            const S32 parsed = LLSDSerialize::fromXML(library, input);
            const bool stream_bad = input.bad();
            input.close();
            if (parsed == LLSDParser::PARSE_FAILURE || stream_bad ||
                !library.isMap())
            {
                LL_WARNS("CineLightRig")
                    << "Bundled master setup library is unreadable or malformed; "
                       "using compiled Classic only"
                    << LL_ENDL;
            }
            else if (!library["version"].isInteger() ||
                     library["version"].asInteger() != 1)
            {
                LL_WARNS("CineLightRig")
                    << "Bundled master setup library uses unsupported version '"
                    << library["version"].asString()
                    << "'; using compiled Classic only" << LL_ENDL;
            }
            else if (!library["presets"].isArray())
            {
                LL_WARNS("CineLightRig")
                    << "Bundled master setup library has no presets array; "
                       "using compiled Classic only"
                    << LL_ENDL;
            }
            else
            {
                const LLSD& presets = library["presets"];
                for (S32 index = 0; index < presets.size(); ++index)
                {
                    const LLSD& item = presets[index];
                    std::string name = item["name"].isString()
                        ? item["name"].asString() : std::string();
                    LLStringUtil::trim(name);
                    Setup setup;
                    const bool duplicate = std::any_of(
                        loaded.begin(), loaded.end(),
                        [&name](const MasterSetup& master)
                        {
                            return sameSetupName(name, master.mName);
                        });
                    if (!item.isMap() || name.empty() ||
                        isSetupDecorationName(name) || duplicate ||
                        !setupFromLLSD(item, setup))
                    {
                        LL_WARNS("CineLightRig")
                            << "Skipping malformed or duplicate bundled master "
                               "setup at index "
                            << index << " ('" << name << "')" << LL_ENDL;
                        continue;
                    }
                    loaded.push_back({
                        name,
                        item["intent"].isString()
                            ? item["intent"].asString() : std::string(),
                        setup,
                        item["category"].isString() &&
                            item["category"].asString() == "Genre / Mood"
                    });
                }
            }
        }

        // A newly-shipped master must never shadow user data. Rename collisions
        // once, preserving the local setup under a visible unique name.
        std::vector<std::string> local_names;
        LLDirIterator iterator(presetsDir(), "*.xml");
        std::string file;
        while (iterator.next(file))
        {
            local_names.emplace_back(LLURI::unescape(
                gDirUtilp->getBaseFileName(file, true)));
        }

        std::vector<std::string> reserved_local_names = local_names;
        S32 renamed_count = 0;
        for (const std::string& local_name : local_names)
        {
            const bool collides = std::any_of(
                loaded.begin(), loaded.end(),
                [&local_name](const MasterSetup& master)
                {
                    return sameSetupName(local_name, master.mName);
                });
            if (!collides)
            {
                continue;
            }

            std::string replacement = local_name + " (local)";
            S32 suffix = 2;
            const auto unavailable = [&loaded, &reserved_local_names](
                                         const std::string& candidate)
            {
                return isSetupDecorationName(candidate) ||
                    std::any_of(loaded.begin(), loaded.end(),
                        [&candidate](const MasterSetup& master)
                        {
                            return sameSetupName(candidate, master.mName);
                        }) ||
                    std::any_of(reserved_local_names.begin(),
                        reserved_local_names.end(),
                        [&candidate](const std::string& local)
                        {
                            return sameSetupName(candidate, local);
                        }) ||
                    gDirUtilp->fileExists(ALCineLightRig::presetPath(candidate));
            };
            while (unavailable(replacement))
            {
                replacement = local_name + llformat(" (local %d)", suffix++);
            }

            if (LLFile::rename(presetPath(local_name),
                               presetPath(replacement)) == 0)
            {
                ++renamed_count;
                reserved_local_names.push_back(replacement);
                LL_WARNS("CineLightRig")
                    << "Renamed local setup '" << local_name << "' to '"
                    << replacement << "' because its name is now built in"
                    << LL_ENDL;
            }
            else
            {
                LL_WARNS("CineLightRig")
                    << "Could not rename local setup '" << local_name
                    << "' that collides with a built-in setup; it will be "
                       "hidden but not deleted"
                    << LL_ENDL;
            }
        }
        if (renamed_count > 0)
        {
            LLNotificationsUtil::add(
                "GenericAlert",
                LLSD().with("MESSAGE", llformat(
                    "%d of your saved rig setups were renamed because this "
                    "viewer ships built-in presets with the same names.",
                    renamed_count)));
        }
        return loaded;
    }();
    return masters;
}

//static
const ALCineLightRig::MasterSetup*
ALCineLightRig::findMasterSetup(const std::string& name)
{
    const std::vector<MasterSetup>& masters = masterSetups();
    for (const MasterSetup& master : masters)
    {
        if (master.mName == name)
        {
            return &master;
        }
    }
    for (const MasterSetup& master : masters)
    {
        if (sameSetupName(master.mName, name))
        {
            return &master;
        }
    }
    return nullptr;
}

//static
bool ALCineLightRig::isMasterSetup(const std::string& name)
{
    return findMasterSetup(name) != nullptr;
}

std::vector<ALCineLightRig::SetupEntry>
ALCineLightRig::setupNamesGrouped() const
{
    std::vector<SetupEntry> entries;
    for (const MasterSetup& master : masterSetups())
    {
        if (!master.mGenre)
        {
            entries.push_back({ master.mName, true, false });
        }
    }
    for (const MasterSetup& master : masterSetups())
    {
        if (master.mGenre)
        {
            entries.push_back({ master.mName, true, true });
        }
    }

    std::vector<std::string> locals;
    LLDirIterator iterator(presetsDir(), "*.xml");
    std::string file;
    while (iterator.next(file))
    {
        const std::string name = LLURI::unescape(
            gDirUtilp->getBaseFileName(file, true));
        if (!isMasterSetup(name) && !isSetupDecorationName(name))
        {
            locals.emplace_back(name);
        }
    }
    std::sort(locals.begin(), locals.end());
    for (const std::string& name : locals)
    {
        entries.push_back({ name, false, false });
    }
    return entries;
}

bool ALCineLightRig::loadSetup(const std::string& name)
{
    Setup setup;
    if (const MasterSetup* master = findMasterSetup(name))
    {
        setup = master->mSetup;
    }
    else
    {
        llifstream input(presetPath(name).c_str());
        if (!input.is_open())
        {
            return false;
        }
        LLSD preset;
        const S32 parsed = LLSDSerialize::fromXML(preset, input);
        const bool stream_bad = input.bad();
        input.close();
        if (parsed == LLSDParser::PARSE_FAILURE || stream_bad ||
            !preset.isMap() || preset["version"].asInteger() != 1 ||
            !setupFromLLSD(preset, setup))
        {
            LL_WARNS("CineLightRig") << "Malformed setup " << name << LL_ENDL;
            return false;
        }
    }
    stopFX();
    writeSetupToSettings(setup);
    return true;
}

bool ALCineLightRig::saveSetup(const std::string& name)
{
    std::string clean_name = name;
    LLStringUtil::trim(clean_name);
    if (clean_name.empty() || isMasterSetup(clean_name) ||
        isSetupDecorationName(clean_name))
    {
        return false;
    }
    Setup setup;
    Globals globals;
    Transforms transforms;
    readSettings(setup, globals, transforms);
    LLSD preset = setupToLLSD(setup);
    preset["version"] = 1;
    preset["name"] = clean_name;

    const std::string path = presetPath(clean_name);
    const std::string temporary = path + "." +
        LLUUID::generateNewID().asString() + ".tmp";
    llofstream output(temporary.c_str());
    if (!output.is_open())
    {
        return false;
    }
    const S32 serialized = LLSDSerialize::toPrettyXML(preset, output);
    output.flush();
    const bool succeeded = serialized > 0 && output.good();
    output.close();
    if (!succeeded || output.fail() || LLFile::rename(temporary, path) != 0)
    {
        LLFile::remove(temporary);
        return false;
    }
    return true;
}

bool ALCineLightRig::deleteSetup(const std::string& name)
{
    return !name.empty() && !isMasterSetup(name) &&
           !isSetupDecorationName(name) &&
           LLFile::remove(presetPath(name)) == 0;
}

LLSD ALCineLightRig::sceneData() const
{
    Setup setup;
    Globals globals;
    Transforms transforms;
    readSettings(setup, globals, transforms);

    LLSD data = LLSD::emptyMap();
    data["version"] = 1;
    data["anchor"] = mAnchor;
    data["anchor_group"] = mGroupEnabled;
    data["anchor_group_slots"] = static_cast<S32>(mGroupSlots);
    data["mirror"] = transforms.mMirror;
    data["orbit_yaw"] = transforms.mYawDeg;
    data["orbit_pitch"] = transforms.mPitchDeg;
    const S32 requested_fx = std::clamp(
        gSavedSettings.getS32("CineLightRigFX"), -1, FX_COUNT - 1);
    data["fx"] = requested_fx;
    data["seed"] = llformat(
        "%u", gSavedSettings.getU32("CineLightRigSeed"));
    const F64 phase = requested_fx >= 0 && requested_fx == mActiveFX &&
                      mLastPresentationTime >= mFXStart
        ? mLastPresentationTime - mFXStart : 0.0;
    data["fx_phase"] = phase;
    data["shafts"] = LLSD::emptyArray();
    data["heroes"] = LLSD::emptyArray();
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        data["shafts"].append(mShaftEnabled[i]);
        data["heroes"].append(mHeroEnabled[i]);
    }
    data["base"] = setupToLLSD(setup);
    return data;
}

void ALCineLightRig::applySceneData(const LLSD& data)
{
    // No block means the scene says nothing about the rig, so start with clean
    // session intent. A versioned block says something; if it is unknown, leave
    // live state untouched rather than half-applying data we cannot understand.
    if (!data.isMap() || !data.has("version"))
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            mShaftEnabled[i] = false;
            mHeroEnabled[i] = false;
        }
        mPendingFXPhase = -1.0;
        mPendingFXId = -1;
        updateProjectorFlags();
        return;
    }
    if (!data["version"].isInteger() ||
        data["version"].asInteger() != 1)
    {
        LL_WARNS("CineLightRig")
            << "Director scene uses unsupported Cinematic Light Rig version '"
            << data["version"].asString()
            << "'; preserving live Cinematic Light Rig configuration"
            << LL_ENDL;
        return;
    }

    // A readable v1 block replaces session-only projector intent. Missing
    // arrays inside that block are therefore an explicit clean state.
    for (S32 i = 0; i < LIGHT_COUNT; ++i)
    {
        mShaftEnabled[i] = false;
        mHeroEnabled[i] = false;
    }
    mPendingFXPhase = -1.0;
    mPendingFXId = -1;
    if (data["shafts"].isArray() && data["shafts"].size() == LIGHT_COUNT)
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            mShaftEnabled[i] = data["shafts"][i].asBoolean();
        }
    }
    if (data["heroes"].isArray() && data["heroes"].size() == LIGHT_COUNT)
    {
        for (S32 i = 0; i < LIGHT_COUNT; ++i)
        {
            mHeroEnabled[i] = data["heroes"][i].asBoolean();
        }
    }
    updateProjectorFlags();
    if (data.has("anchor"))
    {
        setAnchor(data["anchor"].asUUID());
    }
    setGroupEnabled(data["anchor_group"].asBoolean());
    setGroupSlots(static_cast<U32>(
        data["anchor_group_slots"].asInteger()) & GROUP_SLOT_MASK);
    if (data.has("mirror"))
    {
        gSavedSettings.setBOOL("CineLightRigMirror", data["mirror"].asBoolean());
    }
    if (data.has("orbit_yaw"))
    {
        gSavedSettings.setF32("CineLightRigOrbitYaw",
            static_cast<F32>(data["orbit_yaw"].asReal()));
    }
    if (data.has("orbit_pitch"))
    {
        gSavedSettings.setF32("CineLightRigOrbitPitch",
            static_cast<F32>(data["orbit_pitch"].asReal()));
    }
    if (data["base"].isMap())
    {
        Setup setup;
        if (setupFromLLSD(data["base"], setup))
        {
            writeSetupToSettings(setup);
        }
    }
    if (data.has("seed"))
    {
        U32 seed = 0;
        bool valid_seed = false;
        if (data["seed"].isString())
        {
            const std::string seed_text = data["seed"].asString();
            const bool digits_only = !seed_text.empty() && std::all_of(
                seed_text.begin(), seed_text.end(), [](char character)
                {
                    return character >= '0' && character <= '9';
                });
            valid_seed = digits_only &&
                LLStringUtil::convertToU32(seed_text, seed) && seed != 0;
        }
        else if (data["seed"].isInteger())
        {
            const S32 integer_seed = data["seed"].asInteger();
            valid_seed = integer_seed != 0;
            seed = static_cast<U32>(integer_seed);
        }
        if (!valid_seed)
        {
            LL_WARNS("CineLightRig")
                << "Director scene has an invalid Cinematic Light Rig seed; "
                   "using the default seed"
                << LL_ENDL;
            seed = static_cast<U32>(DEFAULT_SEED);
        }
        gSavedSettings.setU32("CineLightRigSeed", seed);
    }
    if (data.has("fx"))
    {
        const S32 fx = std::clamp(
            data["fx"].asInteger(), -1, FX_COUNT - 1);
        gSavedSettings.setS32("CineLightRigFX", fx);
        const F64 phase = data.has("fx_phase")
            ? data["fx_phase"].asReal() : 0.0;
        mPendingFXPhase = std::isfinite(phase) && phase >= 0.0
            ? phase : 0.0;
        mPendingFXId = fx;
        mActiveFX = -1;
        mHaveTarget = false;
        mTransitionActive = false;
    }
}
