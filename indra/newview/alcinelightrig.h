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
class LLVOAvatar;

enum class ALCineLightRigSlot : S32
{
    SELF = 0,
    A,
    B,
    C,
    D,
    COUNT
};

struct ALCineLightRigParamBlob;

// Optional volumetric block carried by a preset. Every field is individually
// present-flagged: a preset applies ONLY the keys it actually declares, so a
// preset that omits the whole block (mPresent == false) never touches the
// user's current volumetric tuning. Mirrors the OptionalSetupGlobals pattern,
// but is viewer-side (it drives gSavedSettings and per-fixture shaft flags,
// which the pure ALCineLightRigModel deliberately cannot reference).
struct CineVolumetricBlock
{
    struct OptF32  { bool mHas = false; F32  mValue = 0.f; };
    struct OptBool { bool mHas = false; bool mValue = false; };
    struct OptS32  { bool mHas = false; S32  mValue = 0; };
    struct OptColor
    {
        bool mHas = false;
        F32  mR = 1.f;
        F32  mG = 1.f;
        F32  mB = 1.f;
    };

    // Whether the preset declared a (map) `volumetric` block at all. A malformed
    // or absent block leaves this false and apply becomes a strict no-op.
    bool mPresent = false;

    // Per-fixture shaft enables (Key/Fill/Rim/Bg).
    bool mHasShafts = false;
    bool mShafts[ALCineLightRigModel::LIGHT_COUNT] = {};

    OptBool  mEnabled;        // BDMergeProjectorVolumetrics (master on/off)
    OptF32   mMultiplier;     // ...Multiplier
    OptF32   mDensity;        // ...Density
    OptF32   mAnisotropy;     // ...Anisotropy
    OptF32   mFeather;        // ...Feather
    OptS32   mShadowSamples;  // ...ShadowSamples

    OptF32   mFogStrength;       // ...FogStrength
    OptF32   mFogGroundDensity;  // ...FogGroundDensity
    OptF32   mFogFalloff;        // ...FogFalloff
    OptF32   mFogBase;           // ...FogBase

    OptF32   mNoiseStrength;  // ...NoiseStrength
    OptF32   mNoiseScale;     // ...NoiseScale
    OptF32   mNoiseSpeed;     // ...NoiseSpeed

    OptBool  mDust;           // ...Dust (master)
    OptF32   mDustIntensity;  // ...DustIntensity
    OptF32   mDustScale;      // ...DustScale
    OptF32   mDustDrift;      // ...DustDrift

    OptF32   mRimStrength;    // ...RimStrength
    OptF32   mRimPower;       // ...RimPower
    OptF32   mRimWrap;        // ...RimWrap
    OptF32   mRimThreshold;   // ...RimThreshold
    OptF32   mRimSoftness;    // ...RimSoftness

    OptColor mTint;           // ...Tint
    OptF32   mTintStrength;   // ...TintStrength
};

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
        bool mGenre = false;
        bool mHasMasterEV = false;
        F32 mMasterEV = 0.f;
        bool mHasMasterTempMired = false;
        F32 mMasterTempMired = 0.f;
        // Optional per-preset volumetric overrides. Default-constructed
        // (mPresent == false) for every preset that omits the block.
        CineVolumetricBlock mVolumetric;
    };

    struct SetupEntry
    {
        std::string mName;
        bool mMaster = false;
        bool mGenre = false;
    };

    // Non-explicit so the manager's `mInstances{ {SLOT_SELF}, {SLOT_A}, ... }`
    // aggregate init can construct each element in place from its braced slot
    // (copy-list-initialization cannot call an explicit constructor).
    ALCineLightRig(
        ALCineLightRigSlot slot = ALCineLightRigSlot::SELF);
    ~ALCineLightRig();

    static const char BUILT_IN_SETUP_CAPTION[];
    static const char GENRE_SETUP_CAPTION[];
    static const char LOCAL_SETUP_CAPTION[];
    static bool isSetupDecorationName(const std::string& name);

    void tickSelected(F64 presentation_time, bool owns_shadows);
    void tickFromBlob(const ALCineLightRigParamBlob& blob,
                      F64 presentation_time, bool owns_shadows);
    void renderGizmo() const;

    ALCineLightRigSlot slot() const { return mSlot; }
    LLVOAvatar* resolveSlotAvatar() const;
    LLUUID projectorId(S32 light) const;

    void setAnchor(const LLUUID& id);
    const LLUUID& getAnchor() const { return mAnchor; }
    void setObjectTarget(const LLUUID& id);
    const LLUUID& getObjectTarget() const { return mObjectTarget; }
    void setGroupEnabled(bool enabled);
    bool isGroupEnabled() const { return mGroupEnabled; }
    void setGroupSlots(U32 mask);
    U32 getGroupSlots() const { return mGroupSlots; }
    U32 lastResolvedGroupSlots() const
    {
        return mLastResolvedGroupSlots;
    }

    void startFX(S32 fx_id, F64 presentation_time);
    // preserve_target keeps the transition target and current lit state so a
    // following updateTransition eases (used by setup loads) instead of
    // snapping. Default false performs the full FX/transition reset.
    void stopFX(bool preserve_target = false);
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
    // Name of the most recently loaded setup (manual or random). Empty until
    // the first successful loadSetup. Reflects the currently-lit named preset.
    const std::string& loadedSetupName() const { return mLoadedSetupName; }

    // Lighting-console cue stack.  Lists contain full snapshots rather than
    // diffs, so playback is independent of later setup-preset edits.
    const ALCineLightRigModel::CueList& cueList() const { return mCueList; }
    void setCueList(const ALCineLightRigModel::CueList& list);
    ALCineLightRigModel::Cue captureCue(const std::string& label) const;
    std::vector<std::string> cueListNames() const;
    bool loadCueList(const std::string& name);
    bool saveCueList(const std::string& name);
    bool deleteCueList(const std::string& name);
    void cueGo(F64 presentation_time);
    void cueBack(F64 presentation_time);
    void cueGoto(S32 index, F64 presentation_time, bool snap = false);
    void cueRelease(F64 presentation_time);
    S32 activeCue() const { return mCueActiveIndex; }
    bool cuePlaybackActive() const { return mCueRuntimeActive; }
    U64 cueRevision() const { return mCueRevision; }
    LLSD cueListData() const;
    static bool validateCueListData(const LLSD& data);
    bool applyCueListData(const LLSD& data);
    void applyCueSceneState(const LLSD& data);

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
    friend struct ALCineLightRigParamBlob;

    bool ensureProjectors(const std::string& cookie_setting);
    bool ensureOmnis();
    bool ensureCatchlight(const std::string& cookie_setting);
    bool createEmitter(LLViewerRegion* region, bool projector,
                       const std::string& cookie_setting,
                       LLPointer<LLVOVolume>& output);
    void destroyEmitter(LLPointer<LLVOVolume>& emitter);
    void destroyOmnis();
    void destroyCatchlight();
    void destroyEmitters();
    void setEmittersDark();

    void readSettings(ALCineLightRigModel::Setup& setup,
                      ALCineLightRigModel::Globals& globals,
                      ALCineLightRigModel::Transforms& transforms) const;
    void readSettings(const ALCineLightRigParamBlob& blob,
                      ALCineLightRigModel::Setup& setup,
                      ALCineLightRigModel::Globals& globals,
                      ALCineLightRigModel::Transforms& transforms) const;
    void tickShared(
        F64 presentation_time, bool owns_shadows, S32 fx_setting,
        F32 offset_z_setting, F32 damping_setting, S32 track_mode_setting,
        bool scale_aware_setting, S32 shadow_mode,
        bool catchlight_enabled, F32 catchlight_ev, F32 catchlight_size,
        F32 catchlight_angle, const F32 shadow_softness[
            ALCineLightRigModel::LIGHT_COUNT],
        const std::string& cookie_setting,
        ALCineLightRigModel::Setup& setup,
        ALCineLightRigModel::Globals& globals,
        ALCineLightRigModel::Transforms& transforms);
    void writeSetupToSettings(const ALCineLightRigModel::Setup& setup) const;
    // Applies a preset's optional volumetric block (clamped to each setting's
    // documented range) and toggles the four per-fixture shafts. When any shaft
    // is enabled it also raises the rig shadow policy to "all projectors" so the
    // shaft is not silently starved of a shadow slot. A no-op when !mPresent.
    void applyVolumetricBlock(const CineVolumetricBlock& block);
    void updateTransition(const ALCineLightRigModel::LightBase target[
                              ALCineLightRigModel::LIGHT_COUNT],
                          F32 target_radius,
                          const ALCineLightRigModel::Globals& target_globals,
                          F32 duration,
                          F64 presentation_time);
    void evaluateTransition(F64 presentation_time);
    ALCineLightRigModel::CueState captureCueState() const;
    static ALCineLightRigModel::CueState releasedCueState(
        const ALCineLightRigModel::CueState& basis);
    void startCueTransition(S32 index, F64 presentation_time, bool snap);
    bool evaluateCuePlayback(
        F64 presentation_time, ALCineLightRigModel::Setup& setup,
        ALCineLightRigModel::Globals& globals,
        ALCineLightRigModel::Transforms& transforms,
        F32 shadow_softness[ALCineLightRigModel::LIGHT_COUNT],
        S32& fx_setting, F64& fx_epoch);
    void applyFrame(const ALCineLightRigModel::RigFrame& frame,
                    const LLVector3d& rig_centre,
                    const LLVector3d& aim_centre, F32 nominal_radius,
                    F32 subject_scale,
                    const std::string& cookie_setting);
    void applyCatchlight(LLVOAvatar* avatar, F32 subject_scale,
                         F32 master_temp_mired, F32 ev, F32 size,
                         F32 angle_degrees,
                         const std::string& cookie_setting);
    void updateShadowPolicy(S32 shadow_mode);
    void updateProjectorFlags();

    static std::string presetsDir();
    static std::string presetPath(const std::string& name);
    static std::string cueListsDir();
    static std::string cueListPath(const std::string& name);

    LLPointer<LLVOVolume> mProjectors[
        ALCineLightRigModel::LIGHT_COUNT];
    LLPointer<LLVOVolume> mOmnis[
        ALCineLightRigModel::LIGHT_COUNT];
    LLPointer<LLVOVolume> mCatchlight;
    LLViewerRegion* mRegion = nullptr;

    ALCineLightRigSlot mSlot = ALCineLightRigSlot::SELF;
    LLUUID mAnchor;
    LLUUID mObjectTarget;
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
    S32 mCatchlightRetryTicks = 0;

    ALCineLightRigModel::LightBase mTransitionStart[
        ALCineLightRigModel::LIGHT_COUNT];
    ALCineLightRigModel::LightBase mTransitionTarget[
        ALCineLightRigModel::LIGHT_COUNT];
    ALCineLightRigModel::LightBase mCurrentLive[
        ALCineLightRigModel::LIGHT_COUNT];
    F32 mTransitionRadiusStart = 1.5f;
    F32 mTransitionRadiusTarget = 1.5f;
    F32 mCurrentRadius = 1.5f;
    ALCineLightRigModel::Globals mTransitionGlobalsStart;
    ALCineLightRigModel::Globals mTransitionGlobalsTarget;
    ALCineLightRigModel::Globals mCurrentGlobals;
    F32 mTransitionDuration = 0.9f;
    F64 mTransitionStartTime = 0.0;
    bool mHaveTarget = false;
    bool mTransitionActive = false;
    std::string mLoadedSetupName;

    ALCineLightRigModel::CueList mCueList;
    ALCineLightRigModel::CueState mCueTransitionStart;
    ALCineLightRigModel::CueState mCueCurrent;
    ALCineLightRigModel::Cue mCueRuntimeTarget;
    F64 mCueGoTime = 0.0;
    F64 mCueFXEpoch = 0.0;
    S32 mCueActiveIndex = -1;
    bool mCueRuntimeActive = false;
    bool mCueRelease = false;
    U64 mCueRevision = 0;
    U64 mCueFXGeneration = 0;
    U64 mAppliedCueFXGeneration = 0;

    LLVector3d mSmoothedCentre;
    F32 mSmoothedScale = 1.f;
    bool mHaveSmoothedCentre = false;
    bool mShaftEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    bool mHeroEnabled[ALCineLightRigModel::LIGHT_COUNT] = {};
    F32 mShadowSoftness[ALCineLightRigModel::LIGHT_COUNT] = {};
    ALCineLightRigModel::RigFrame mLastFrame;
};

#endif // AL_CINE_LIGHT_RIG_H
