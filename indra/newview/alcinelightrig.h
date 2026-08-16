/**
 * @file alcinelightrig.h
 * @brief Client-side cinematic light-rig controller and local emitters.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_CINE_LIGHT_RIG_H
#define AL_CINE_LIGHT_RIG_H

#include "alcinelightrigmodel.h"
#include "llpointer.h"
#include "llsd.h"
#include "lluuid.h"
#include "v3dmath.h"

#include <string>
#include <vector>

class LLViewerRegion;
class LLVOVolume;

class ALCineLightRig
{
public:
    enum : U32
    {
        GROUP_SLOT_SELF = 1 << 0,
        GROUP_SLOT_A    = 1 << 1,
        GROUP_SLOT_B    = 1 << 2,
        GROUP_SLOT_C    = 1 << 3,
        GROUP_SLOT_D    = 1 << 4,
        GROUP_SLOT_MASK = 0x1F,
    };

    struct MasterSetup
    {
        std::string mName;
        std::string mIntent;
        ALCineLightRigModel::Setup mSetup;
    };

    struct SetupEntry
    {
        std::string mName;
        bool mMaster = false;
    };

    ALCineLightRig();
    ~ALCineLightRig();

    static ALCineLightRig& instance();

    static const char BUILT_IN_SETUP_CAPTION[];
    static const char LOCAL_SETUP_CAPTION[];
    static bool isSetupDecorationName(const std::string& name);

    void tick(F64 presentation_time);
    void renderGizmo() const;

    void setAnchor(const LLUUID& id);
    const LLUUID& getAnchor() const { return mAnchor; }
    void setGroupEnabled(bool enabled);
    bool isGroupEnabled() const { return mGroupEnabled; }
    void setGroupSlots(U32 mask);
    U32 getGroupSlots() const { return mGroupSlots; }
    U32 lastResolvedGroupSlots() const
    {
        return mLastResolvedGroupSlots;
    }

    void startFX(S32 fx_id, F64 presentation_time);
    void stopFX();
    S32 activeFX() const { return mActiveFX; }

    void setShaftEnabled(S32 light, bool enabled);
    bool isShaftEnabled(S32 light) const;
    void setHeroEnabled(S32 light, bool enabled);
    bool isHeroEnabled(S32 light) const;

    static const std::vector<MasterSetup>& masterSetups();
    static const MasterSetup* findMasterSetup(const std::string& name);
    static bool isMasterSetup(const std::string& name);
    std::vector<SetupEntry> setupNamesGrouped() const;
    bool loadSetup(const std::string& name);
    bool saveSetup(const std::string& name);
    bool deleteSetup(const std::string& name);

    LLSD sceneData() const;
    void applySceneData(const LLSD& data);

    bool isClipped(S32 light) const;
    const ALCineLightRigModel::RigFrame& lastFrame() const
    {
        return mLastFrame;
    }

    // Idempotent and safe on a partial emitter set.
    void shutdown();

private:
    bool ensureProjectors();
    bool ensureOmnis();
    bool createEmitter(LLViewerRegion* region, bool projector,
                       LLPointer<LLVOVolume>& output);
    void destroyEmitter(LLPointer<LLVOVolume>& emitter);
    void destroyOmnis();
    void destroyEmitters();
    void setEmittersDark();

    void readSettings(ALCineLightRigModel::Setup& setup,
                      ALCineLightRigModel::Globals& globals,
                      ALCineLightRigModel::Transforms& transforms) const;
    void writeSetupToSettings(const ALCineLightRigModel::Setup& setup) const;
    void updateTransition(const ALCineLightRigModel::LightBase target[
                              ALCineLightRigModel::LIGHT_COUNT],
                          F32 target_radius, F32 duration,
                          F64 presentation_time);
    void evaluateTransition(F64 presentation_time);
    void applyFrame(const ALCineLightRigModel::RigFrame& frame,
                    const LLVector3d& rig_centre,
                    const LLVector3d& aim_centre, F32 nominal_radius,
                    F32 subject_scale);
    void updateShadowPolicy();
    void updateProjectorFlags();

    static std::string presetsDir();
    static std::string presetPath(const std::string& name);

    LLPointer<LLVOVolume> mProjectors[
        ALCineLightRigModel::LIGHT_COUNT];
    LLPointer<LLVOVolume> mOmnis[
        ALCineLightRigModel::LIGHT_COUNT];
    LLViewerRegion* mRegion = nullptr;

    LLUUID mAnchor;
    bool mGroupEnabled = false;
    U32 mGroupSlots = 0;
    U32 mLastResolvedGroupSlots = 0;
    S32 mActiveFX = -1;
    F64 mFXStart = 0.0;
    F64 mPendingFXPhase = -1.0;
    S32 mPendingFXId = -1;
    F64 mLastPresentationTime = -1.0;
    S32 mProjectorRetryTicks = 0;
    S32 mOmniRetryTicks = 0;

    ALCineLightRigModel::LightBase mTransitionStart[
        ALCineLightRigModel::LIGHT_COUNT];
    ALCineLightRigModel::LightBase mTransitionTarget[
        ALCineLightRigModel::LIGHT_COUNT];
    ALCineLightRigModel::LightBase mCurrentLive[
        ALCineLightRigModel::LIGHT_COUNT];
    F32 mTransitionRadiusStart = 1.5f;
    F32 mTransitionRadiusTarget = 1.5f;
    F32 mCurrentRadius = 1.5f;
    F32 mTransitionDuration = 0.9f;
    F64 mTransitionStartTime = 0.0;
    bool mHaveTarget = false;
    bool mTransitionActive = false;

    LLVector3d mSmoothedCentre;
    F32 mSmoothedScale = 1.f;
    bool mHaveSmoothedCentre = false;
    bool mShaftEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    bool mHeroEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    ALCineLightRigModel::RigFrame mLastFrame;
};

#endif // AL_CINE_LIGHT_RIG_H
