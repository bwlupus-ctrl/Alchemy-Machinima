"""Production caller fixtures for rig_sway_check; renderer/settings services are stubbed.

Function bodies for scene/cue/blob/multi-instance restore are extracted verbatim.
Only viewer lifecycle, settings sinks, UUID storage and presentation time use doubles.
"""
from pathlib import Path

from rig_shadow_presets_check import function
from rig_session_ui_check import fixture as ui_fixture
from rig_subject_assignment_check import assignment_fixture


SUPPORT = r'''
#include <functional>
#include <memory>
#define LL_WARNS(...) std::cerr
struct SettingsSink {
    std::map<std::string, LLSD> values;
    std::function<void(bool)> enabledChanged;
    void setBOOL(const std::string& k, bool v) {
        const bool changed = values[k].asBoolean() != v;
        values[k] = v;
        if (changed && k == "CineLightRigEnabled" && enabledChanged) enabledChanged(v);
    }
    void setF32(const std::string& k, F32 v) { values[k] = v; }
    void setS32(const std::string& k, S32 v) { values[k] = v; }
    void setU32(const std::string& k, U32 v) { values[k] = double(v); }
    void setString(const std::string& k, const std::string& v) { values[k] = v; }
    void setLLSD(const std::string& k, const LLSD& v) { values[k] = v; }
    bool getBOOL(const std::string& k) { return values[k].asBoolean(); }
    F32 getF32(const std::string& k) { return static_cast<F32>(values[k].asReal()); }
    S32 getS32(const std::string& k) { return values[k].asInteger(); }
    U32 getU32(const std::string& k) { return static_cast<U32>(values[k].asReal()); }
    std::string getString(const std::string& k) { return values[k].asString(); }
} gSavedSettings;
namespace LLStringUtil {
    bool convertToU32(const std::string& text, U32& out) {
        out = static_cast<U32>(std::stoul(text)); return true;
    }
}
namespace LLPresentationTime {
    struct Frame { F64 presentation_time = 1001.0; };
    Frame currentFrame() { return {}; }
}
struct OptionalSetupGlobals;
void applyOptionalSetupGlobals(const OptionalSetupGlobals&) {}
struct ALCineLightRigParamBlob;
class ALCineLightRig {
public:
    static inline U32 sActorOnlyRequestedSlots = 0;
    static constexpr U32 GROUP_SLOT_MASK = 31, GROUP_SLOT_A = 2, GROUP_SLOT_C = 8;
    bool mShaftEnabled[4] = {}, mHeroEnabled[4] = {};
    F64 mPendingFXPhase = -1, mLastPresentationTime = 1001, mFXStart = 0;
    S32 mPendingFXId = -1, mActiveFX = -1;
    bool mHaveTarget = false, mTransitionActive = false;
    Setup mStoredSetup;
    CueList mCueList;
    CueState mCueTransitionStart, mCueCurrent;
    Cue mCueRuntimeTarget;
    S32 mCueActiveIndex = -1;
    bool mCueRelease = false, mCueRuntimeActive = false;
    U64 mCueRevision = 0, mCueFXGeneration = 0;
    U32 mEmitters = 0;
    std::string mLoadedSetupName;
    LLUUID mAnchor, mObjectTarget;
    uuid_vec_t mLastResolvedMembers, mLastResolvedStoredMembers;
    const uuid_vec_t& lastResolvedMembers() const { return mLastResolvedMembers; }
    const uuid_vec_t& lastResolvedStoredMembers() const { return mLastResolvedStoredMembers; }
    std::string mLitGroup;
    const std::string& getLitGroup() const { return mLitGroup; }
    void setLitGroup(const std::string& name) { mLitGroup = name; }
    bool mGroupEnabled = false;
    U32 mGroupSlots = 0;
    S32 mSelectedTicks = 0, mBlobTicks = 0;
    void shutdown() { mEmitters = 0; }
    void tickSelected(F64, bool) { ++mSelectedTicks; mEmitters = gSavedSettings.getBOOL("CineLightRigEnabled") ? 0xffu : 0; }
    void tickFromBlob(const ALCineLightRigParamBlob&, F64, bool);
    void setLiveProbeBounceScale(F32) {}
    F64 mCueFXEpoch = 0, mCueGoTime = 0;
    const LLUUID& getAnchor() const { return mAnchor; }
    void setAnchor(const LLUUID& id) { mAnchor = id; }
    void setObjectTarget(const LLUUID& id) { mObjectTarget = id; }
    void setGroupEnabled(bool value) { mGroupEnabled = value; }
    void setGroupSlots(U32 value) { mGroupSlots = value; }
    void updateProjectorFlags() {}
    void writeSetupToSettings(const Setup& s) { mStoredSetup = s; }
    CueState captureCueState() const { CueState s; s.mSetup = mStoredSetup; return s; }
    static CueState releasedCueState(const CueState&);
    void setCueList(const CueList&);
    static bool validateCueListData(const LLSD&);
    LLSD cueListData() const;
    bool applyCueListData(const LLSD&);
    void applyCueSceneState(const LLSD&);
    void applySceneData(const LLSD&);
    void startCueTransition(S32, F64, bool);
    void cueRelease(F64);
};
'''

MANAGER = r'''
using Slot = ALCineLightRigSlot;
using ParamBlob = ALCineLightRigParamBlob;
using namespace ALCineLightRigManagerModel;
void ALCineLightRig::tickFromBlob(const ParamBlob& blob, F64, bool) {
    ++mBlobTicks; mEmitters = blob.mEnabled ? 0xffu : 0;
}
constexpr S32 INSTANCE_STORE_VERSION = 1;
constexpr char INSTANCE_SETTING[] = "CineLightRigInstances";
class ALCineLightRigManager {
public:
    using Slot = ALCineLightRigSlot;
    using ParamBlob = ALCineLightRigParamBlob;
    static constexpr S32 SLOT_COUNT = 5;
    static constexpr Slot SLOT_SELF = Slot::SELF;
    static constexpr Slot SLOT_A = Slot::A, SLOT_B = Slot::B, SLOT_C = Slot::C, SLOT_D = Slot::D;
    static inline ALCineLightRigManager* testInstance=nullptr;
    static bool instanceExists() { return testInstance!=nullptr; }
    static ALCineLightRigManager& instance() { assert(testInstance); return *testInstance; }
    ParamBlob mBlobs[SLOT_COUNT];
    ALCineLightRig mInstances[SLOT_COUNT];
    Slot mSelected = SLOT_SELF;
    ALCineLightRigManagerModel::RigSession mRigSession;
    bool mWritingSelectedSettings = false;
    U64 mConfigurationRevision = 1;
    U64 mUseClock = 0; std::array<U64,4> mLastUsed{};
    std::array<LLUUID,4> mEnrolledAnchors{};
    std::map<LLUUID,LLUUID> mBorrowRekeys;std::map<LLUUID,std::weak_ptr<const LLUUID>> mBorrowPicks;U64 mBorrowCastRevision=~U64(0);bool mBorrowAnchorsDirty=true;void refreshBorrowAnchors();
    bool mLoginRecovery=false;  // [LoginSolo] production member read by restore()
    std::array<U64,5> mHistoryEpoch{};
    void touchSlot(Slot); void pruneRemovedAnchors();
    void rewriteActorReferences(const LLUUID&,const LLUUID&);
    void removeMirror(const LLUUID&) {}
    // Renderer-derived session lighting is covered by extra_rims_check.py;
    // these adapters retain the existing scene/cue/recovery assertions.
    std::vector<int> mExtraRims;
    S32 mExtraRimSource=0;
    U64 mExtraRimSceneEpoch=1;
    void updateExtraRims(F64) {}
    void replaceExtraRims(const LLSD&,bool) {}
    void shutdownExtraRims() {}
    void clearExtraRims() {mExtraRims.clear();mExtraRimSource=0;}
    void pruneRemovedExtraRims() {}
    void rewriteExtraRimActorReferences(const LLUUID&,const LLUUID&) {}
    bool extraRimRegistryNeedsRebuild()const{return true;}
    LLUUID slotActor(Slot) const; Slot actorSlot(const LLUUID&) const;
    Slot borrowSlot(const LLUUID&) const; Slot leastRecentlyUsedSlot() const;
    void giveActorRig(Slot,const LLUUID&); void removeBorrowedRig(Slot);
    ALCineLightRig& at(Slot s) { return mInstances[static_cast<S32>(s)]; }
    const ALCineLightRig& at(Slot s) const { return mInstances[static_cast<S32>(s)]; }
    bool isSlotEnabled(Slot s) const { return enabledMask() & (1u<<static_cast<S32>(s)); }
    std::map<S32, std::unique_ptr<ALCineLightRig>> mMirrors;
    struct { Slot mFocus, mChallenger; S32 mChallengeTicks; } mFocusState{};
    static bool validSlot(Slot);
    U32 enabledMask() const;
    bool restore(const LLSD&, bool scene_load = false);
    static bool validateInstanceStore(const LLSD&);
    bool isRigGroup() const { return mRigSession.mGroup; }
    ALCineLightRig& selected() { return mInstances[static_cast<S32>(mSelected)]; }
    const ALCineLightRig& selected() const { return mInstances[static_cast<S32>(mSelected)]; }
    ParamBlob captureSelected() const;
    bool mNeedsSessionRestore = false;
    static inline bool sAnyActorOnly = false;
    struct ActorOnlyRegistry {};
    ActorOnlyRegistry mActorOnlyRegistry;
    std::vector<S32> mActorOnlyRecipients;
    U32 mActorOnlyObjectSlots = 0;
    S32 mLiveProbeRetryTicks = 0;
    void setLitGroup(Slot, const std::string&);
    void applySceneData(const LLSD&);
    void applySceneDataStale(const LLSD&);
    void tick(F64);
    void tickSoloMask(F64);
    void shutdown();
    void migrateLegacyScene(const LLSD&);
    void updateAutoShadowSlots() {}
    void updateRandomCycle(F64) {}
    void resolveCameraFocus() {}
    F32 liveProbeBounceScaleFor(Slot) { return 1.f; }
    void updateMirrors() {}
    void rebuildActorOnlyRegistry() {}
    void updateLiveProbe() {}
    void restoreAutoShadowSlots() {}
    void destroyLiveProbe() {}
    void clearShadowSuppression() {}
    void logSelectedTarget() {}
    void writeSelectedSettings();
    void setSelectedSlot(Slot);
    void onEnabledChanged(bool);
    void shutdownSlot(S32);
    void subjectRigOff(Slot);
    void subjectRigsOff();
    void allOff();
    void restoreEnabledMask(U32, Slot, bool);
    void persist() const;
};
void ALCineLightRigParamBlob::toSettings() const { toSettingsStore(gSavedSettings); }
'''

TESTS = r'''
LLSD cueWithSetup(const LLSD& setup) {
    LLSD cue = LLSD::emptyMap();
    cue["setup"] = setup; cue["globals"] = LLSD::emptyMap();
    cue["transforms"] = LLSD::emptyMap(); cue["label"] = "Recovery sentinel";
    cue["fade_sec"] = 0.0; cue["delay_sec"] = 0.0; cue["profile"] = 0;
    cue["at_sec"] = 0.0; return cue;
}
LLSD listWithSetup(const LLSD& setup) {
    LLSD list = LLSD::emptyMap(); list["version"] = 1;
    list["name"] = "Preserved list"; list["timecode_mode"] = false;
    list["cues"] = LLSD::emptyArray(); list["cues"].append(cueWithSetup(setup));
    return list;
}
void checkRecovered(const Setup& setup) {
    assert(setup.mRadius == 2.25f);
    assert(setup.mLights[0].mSwayTermCount == 0);
    assert(setup.mLights[0].mSwayAmount == .45f);
    assert(setup.mLights[0].mSwayPeriod == 2.4f);
    assert(setup.mLights[1].mEV == -3.25f);
    assert(setup.mLights[1].mOn);
    assert(setup.mLights[1].mSwayTermCount == 1);
}
void checkCallerRecovery(LLSD setup, const LLSD& malformed) {
    setup["radius"] = 2.25;
    setup["lights"].values[0]["sway_mix"] = malformed;
    setup["lights"].values[1]["ev"] = -3.25;
    setup["lights"].values[1]["sway_mix"] = LLSD::emptyArray();
    LLSD good = LLSD::emptyArray(); good.append(.2); good.append(3.0); good.append(.4);
    setup["lights"].values[1]["sway_mix"].append(good);
    Setup loaded; assert(setupFromLLSD(setup, loaded)); checkRecovered(loaded);
    CueList list; const LLSD encoded = listWithSetup(setup);
    assert(cueListFromLLSD(encoded, list)); assert(list.mCues.size() == 1);
    checkRecovered(list.mCues[0].mSetup);
    assert(list.mCues[0].mLabel == "Recovery sentinel");
    ALCineLightRig rig;
    LLSD scene = LLSD::emptyMap(); scene["version"] = 1;
    scene["base"] = setup; scene["cue_list"] = encoded;
    scene["cue_active"] = 0;
    rig.applySceneData(scene);
    checkRecovered(rig.mStoredSetup);
    assert(rig.mCueList.mName == "Preserved list");
    checkRecovered(rig.mCueList.mCues[0].mSetup);
    checkRecovered(rig.mCueCurrent.mSetup);
    ALCineLightRigParamBlob blob;
    blob.mEnabled = true; blob.mRadius = 2.25f;
    blob.mLights[1].mEV = -3.25f; blob.mCueList = encoded;
    LLSD entry = blob.toLLSD(); entry["slot"] = 0;
    assert(blobMapWellFormed(entry));
    const auto restored = ALCineLightRigParamBlob::fromLLSD(entry);
    assert(restored.mRadius == 2.25f && restored.mLights[1].mEV == -3.25f);
    ALCineLightRig from_blob; restored.applyRigState(from_blob);
    checkRecovered(from_blob.mCueList.mCues[0].mSetup);
    LLSD store = LLSD::emptyMap(); store["version"] = 1;
    store["selected_slot"] = 2; store["instances"] = LLSD::emptyArray();
    for (S32 slot = 0; slot < 5; ++slot) {
        LLSD row = entry; row["slot"] = slot;
        row["CineLightRigMasterEV"] = double(slot) + .25;
        store["instances"].append(row);
    }
    assert(ALCineLightRigManager::validateInstanceStore(store));
    ALCineLightRigManager manager; assert(manager.restore(store, true));
    for(S32 i=0;i<5;++i)manager.setLitGroup(static_cast<Slot>(i),"guards");
    auto clear=[](const ALCineLightRigManager& m){for(S32 i=0;i<5;++i)if(!m.mInstances[i].mLitGroup.empty() || !m.mBlobs[i].mLitGroup.empty())return false;return true;};
    LLSD unsupported;unsupported["version"]=99;manager.applySceneData(unsupported);assert(!clear(manager));
    for(S32 i=0;i<5;++i)assert(manager.mInstances[i].mLitGroup=="guards" && manager.mBlobs[i].mLitGroup=="guards");
    unsupported["version"]="future";manager.applySceneData(unsupported);
    for(S32 i=0;i<5;++i)assert(manager.mInstances[i].mLitGroup=="guards" && manager.mBlobs[i].mLitGroup=="guards");
    manager.applySceneDataStale(LLSD());assert(!clear(manager));
    manager.applySceneData(LLSD());assert(clear(manager));
    assert(manager.restore(store,true));
    assert(manager.mSelected == Slot::B);
    assert(!manager.isRigGroup()); // old scenes need no group field
    for (S32 slot = 0; slot < 5; ++slot) {
        assert(manager.mBlobs[slot].mMasterEV == slot + .25f);
        assert(manager.mBlobs[slot].mLights[1].mEV == -3.25f);
        checkRecovered(manager.mInstances[slot].mCueList.mCues[0].mSetup);
    }
    gSavedSettings.enabledChanged = [&manager](bool enabled) {
        if (!manager.mWritingSelectedSettings) manager.onEnabledChanged(enabled);
    };
    assert(manager.restore(store));
    assert(!manager.isRigGroup() && manager.mSelected == Slot::SELF);
    assert(manager.enabledMask() == 0);
    for (S32 slot = 0; slot < 5; ++slot) {
        manager.setSelectedSlot(static_cast<Slot>(slot));
        assert(manager.mSelected == static_cast<Slot>(slot));
        assert(!manager.mBlobs[slot].mEnabled);
        assert(manager.mBlobs[slot].mMasterEV == slot + .25f);
    }
    manager.setSelectedSlot(Slot::B);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    manager.tick(1002.0);
    assert(manager.mInstances[2].mSelectedTicks == 1 && manager.mInstances[2].mEmitters == 0xffu);
    manager.setSelectedSlot(Slot::A);
    manager.tick(1002.5);
    assert(manager.enabledMask() == 4 && manager.mInstances[2].mBlobTicks == 1);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    gSavedSettings.setBOOL("CineLightRigEnabled", false);
    assert(manager.enabledMask() == 4 && manager.mInstances[2].mEmitters == 0xffu);
    gSavedSettings.setF32("CineLightRigMasterEV", 7.75f);
    manager.subjectRigsOff(); // no legacy group flag needed
    assert(manager.mSelected == Slot::SELF && manager.enabledMask() == 0);
    assert(manager.mBlobs[1].mMasterEV == 7.75f && manager.mBlobs[2].mMasterEV == 2.25f);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    manager.setSelectedSlot(Slot::B);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    manager.subjectRigsOff();
    assert(manager.mSelected == Slot::SELF && manager.enabledMask() == 1);
    manager.mMirrors[1] = std::make_unique<ALCineLightRig>();
    manager.mMirrors[1]->mEmitters = 0xffu;
    for (auto& blob : manager.mBlobs) blob.mEnabled = true;
    for (auto& instance : manager.mInstances) instance.mEmitters = 0xffu;
    manager.allOff();
    assert(manager.enabledMask() == 0 && manager.mMirrors[1]->mEmitters == 0);
    for (const auto& instance : manager.mInstances) assert(instance.mEmitters == 0);
    // ALL OFF restore must retain a Subject selection/mask with the legacy flag false.
    manager.restoreEnabledMask(14, Slot::B, false);
    assert(manager.enabledMask() == 14 && manager.mSelected == Slot::B && !manager.isRigGroup());
    manager.allOff();
    manager.restoreEnabledMask(1, Slot::A, false);
    assert(manager.enabledMask() == 1 && manager.mSelected == Slot::A);
    for (bool group : {false, true}) {
        store["rig_group"] = group;
        assert(manager.restore(store, true));
        assert(manager.enabledMask() == 31 && manager.mSelected == Slot::B);
        assert(manager.restore(store));
        assert(manager.enabledMask() == 0 && manager.mSelected == Slot::SELF);
    }
    LLSD bad_mode = store; bad_mode["rig_group"] = "invalid";
    assert(!manager.restore(bad_mode, true));
    // Stale enabled Subjects dispatch independently even with the selected rig off.
    for (S32 i = 1; i < 5; ++i) manager.mBlobs[i].mEnabled = true;
    manager.tick(1003.0);
    for (S32 i = 1; i < 5; ++i) assert(manager.mInstances[i].mEmitters == 0xffu);
    manager.setSelectedSlot(Slot::C);
    gSavedSettings.setF32("CineLightRigMasterEV", 6.5f);
    manager.shutdown();
    assert(!manager.isRigGroup() && manager.mSelected == Slot::SELF);
    assert(manager.enabledMask() == 0 && manager.mNeedsSessionRestore);
    const LLSD saved_off = gSavedSettings.values["CineLightRigInstances"];
    assert(saved_off["selected_slot"].asInteger() == 0);
    for (const LLSD& saved_slot : saved_off["instances"].values)
        assert(!saved_slot["CineLightRigEnabled"].asBoolean());
    assert(manager.mBlobs[3].mMasterEV == 6.5f);
    for (const auto& slot : manager.mInstances) assert(slot.mEmitters == 0);
    manager.shutdown();
    assert(manager.mBlobs[3].mMasterEV == 6.5f);
    manager.tick(1004.0);
    assert(!manager.mNeedsSessionRestore && manager.enabledMask() == 0);
    assert(gSavedSettings.getF32("CineLightRigMasterEV") == .25f);
    assert(manager.restore(saved_off) && manager.enabledMask() == 0);
    for (bool enabled : {false, true}) {
        gSavedSettings.setBOOL("CineLightRigEnabled", enabled);
        gSavedSettings.setF32("CineLightRigRadius", 8.25f);
        gSavedSettings.setF32("CineLightRigMasterEV", -4.5f);
        LLSD legacy = LLSD::emptyMap(); legacy["anchor"] = LLUUID{"subject-A"};
        manager.migrateLegacyScene(legacy);
        assert(!manager.isRigGroup() && manager.mSelected == Slot::SELF);
        assert(manager.mBlobs[0].mAnchor.asString() == "subject-A");
        assert(manager.mBlobs[0].mEnabled == enabled);
        assert(gSavedSettings.getF32("CineLightRigRadius") == 8.25f);
        for (S32 i = 1; i < 5; ++i) assert(!manager.mBlobs[i].mEnabled);
    }
    gSavedSettings.enabledChanged = {};
    // Required structure is still rejected: graceful recovery is optional-only.
    setup["lights"] = LLSD::emptyArray();
    assert(!setupFromLLSD(setup, loaded));
    store["instances"].values[0]["lights"] = LLSD::emptyArray();
    assert(!ALCineLightRigManager::validateInstanceStore(store));
    assert(!manager.restore(store));
}
void testRecovery(const LLSD& authored) {
    std::vector<LLSD> bad;
    bad.emplace_back("bad");
    LLSD row = LLSD::emptyArray(); row.append(.5); row.append(2.4); row.append(.5);
    LLSD mix = LLSD::emptyArray();
    for (S32 i = 0; i <= MAX_SWAY_TERMS; ++i) mix.append(row);
    bad.push_back(mix); // oversized
    mix = LLSD::emptyArray(); mix.append("bad row"); bad.push_back(mix);
    for (F64 value : {-1.0, 2.0, std::numeric_limits<F64>::infinity(),
                      std::numeric_limits<F64>::quiet_NaN()}) {
        LLSD invalid = row; invalid.values[2] = value;
        mix = LLSD::emptyArray(); mix.append(invalid); bad.push_back(mix);
    }
    for (S32 column = 0; column < 3; ++column) {
        LLSD invalid = row; invalid.values[column] = "not numeric";
        mix = LLSD::emptyArray(); mix.append(invalid); bad.push_back(mix);
        invalid = row; invalid.values[column] = std::numeric_limits<F64>::quiet_NaN();
        mix = LLSD::emptyArray(); mix.append(invalid); bad.push_back(mix);
    }
    mix = LLSD::emptyArray(); mix.append(row); mix.append(row); mix.append(row);
    bad.push_back(mix); // individually valid weights, total > 1 + tolerance
    for (const LLSD& malformed : bad) checkCallerRecovery(authored, malformed);
    // Small overshoots load and normalize down; do not reject or scale up.
    for (F64 excess : {0.0, .000001, .00005, .0001}) {
        LLSD valid = row; valid.values[2] = 1.0 + excess;
        mix = LLSD::emptyArray(); mix.append(valid);
        LightBase light; assert(swayMixFromLLSD(mix, light));
        assert(light.mSwayTermCount == 1 && light.mSwayTerms[0].mWeight <= 1.f);
    }
    LightBase a, b; a.mSwayAmount = b.mSwayAmount = .5f;
    a.mSwayPeriod = 2.4f; b.mSwayPeriod = 4.f;
    LightBase current = a;
    for (S32 step = 0; step < 600; ++step) {
        current = blendLight(current, (step & 1) ? a : b, .001f);
        LLSD saved = authored; saved["lights"].values[0] = lightToLLSD(current);
        Setup loaded; assert(setupFromLLSD(saved, loaded));
        assert(sameSwayMix(loaded.mLights[0], current));
        F32 x[3], y[3];
        evalLightSway(current, flickerLightSeed(12345,0), 1001.0, .56f, x);
        evalLightSway(loaded.mLights[0], flickerLightSeed(12345,0), 1001.0, .56f, y);
        assert(std::memcmp(x,y,sizeof(x)) == 0);
    }
    std::cout << "PASS: 600 production blend/LLSD round trips; recovery through scene, cue list, blob and five-slot restore\n";
    std::cout << "PASS: production manager independent selection/power, Subject rigs off/all-off/restore, buffer-swap guards and mirrors\n";
    std::cout << "PASS: actual manager ticks Subjects without a group; startup off, scene enables retained and shutdown retains edits\n";
    {
        const auto setup_only=setupToLLSD(Setup(),Globals());
        assert(!setup_only.has("lit_group") && !setup_only.has("anchor_group"));
        ParamBlob blob; blob.mLitGroup="guards";blob.mGroupEnabled=true;
        auto entry=blob.toLLSD(); assert(!entry.has("lit_group") && !entry["group"].asBoolean());
        entry["lit_group"]="guards";
        LLSD scene; scene["version"]=1;scene["selected_slot"]=0;scene["instances"]=LLSD::emptyArray();
        for(S32 i=0;i<5;++i){entry["slot"]=i;scene["instances"].append(entry);}
        ALCineLightRigManager group_manager;
        assert(group_manager.restore(scene,true));
        for(const auto& rig:group_manager.mInstances)assert(rig.getLitGroup()=="guards");
        assert(group_manager.restore(scene,false));
        for(const auto& rig:group_manager.mInstances)assert(rig.getLitGroup().empty());
    }
    {
        ALCineLightRigManager borrowed;
        auto& cast=LLDirectorCast::instance();
        const LLUUID actor{"borrowed-clone"}; cast.members.insert(actor);
        borrowed.mBlobs[0]=ParamBlob::fromSettings(nullptr);
        borrowed.writeSelectedSettings();
        borrowed.giveActorRig(Slot::B,actor);
        assert(borrowed.slotActor(Slot::SELF)==gAgentID);
        assert(borrowed.slotActor(Slot::B)==actor && borrowed.mBlobs[2].mEnabled);
        borrowed.subjectRigOff(Slot::B); assert(borrowed.mBlobs[2].mEnabled);
        cast.members.erase(actor);cast.members.insert(LLUUID{"new-clone-body"});
        borrowed.rewriteActorReferences(actor,LLUUID{"new-clone-body"});
        assert(borrowed.slotActor(Slot::B)==LLUUID{"new-clone-body"} && borrowed.mBlobs[2].mEnabled);
        cast.members.erase(LLUUID{"new-clone-body"});
        borrowed.rewriteActorReferences(LLUUID{"new-clone-body"},LLUUID::null);
        assert(borrowed.mInstances[2].mAnchor.isNull() && !borrowed.mBlobs[2].mEnabled);
        cast.members.insert(actor);borrowed.giveActorRig(Slot::B,actor);
        borrowed.persist();
        LLSD scene=gSavedSettings.values["CineLightRigInstances"]; scene["version"]=1;
        borrowed.mBorrowRekeys[LLUUID{"old-scene-alias"}]=actor;
        borrowed.applySceneData(scene);
        assert(borrowed.mBorrowRekeys.empty());
        assert(borrowed.slotActor(Slot::B)==actor && borrowed.mBlobs[2].mEnabled);
        cast.members.erase(actor); borrowed.pruneRemovedAnchors();
        assert(borrowed.mInstances[2].mAnchor.isNull() && !borrowed.mBlobs[2].mEnabled);
        assert(borrowed.restore(scene,true));
        borrowed.subjectRigsOff(); assert(borrowed.enabledMask()==0);
        assert(borrowed.mInstances[2].mAnchor==actor); // Off retains the borrowed identity
        assert(borrowed.restore(scene,true));
        borrowed.allOff(); assert(borrowed.enabledMask()==0);
        assert(borrowed.mInstances[2].mAnchor==actor);
        assert(borrowed.restore(scene,true));
        borrowed.removeBorrowedRig(Slot::B);
        assert(borrowed.mBlobs[2].mAnchor.isNull() && borrowed.mInstances[2].mAnchor.isNull());
        assert(!borrowed.mBlobs[2].mEnabled);
        borrowed.mBorrowRekeys[LLUUID{"legacy-scene-alias"}]=actor;
        borrowed.applySceneData(LLSD());assert(borrowed.mBorrowRekeys.empty());
        std::cout<<"PASS: borrowed anchor scene round trip, Subject-change immunity, removal, Subject rigs off and All off (borrowed rigs included)\n";
    }
    RigLitGroupModel::test();
    RigSessionUI::test();
    RigTargetUI::test();
    RigLightingUI::test();
    testSubjectAssignment();
    RigPolicyUI::test();
    testSession128(); testSession129(); testSession130();
    testSession131(); testSession132(); testSession133();
    std::cout << "PASS: production TUT model tests 128-133 executed\n";
    for (bool negative : {false, true}) {
        ALCineLightRigManager probe;
        gSavedSettings.setBOOL("CineLightRigEnabled", false);
        probe.mBlobs[1].mEnabled = true;
        if (negative) probe.tickSoloMask(2000.0); else probe.tick(2000.0);
        const bool subject_tickable = probe.mInstances[1].mBlobTicks == 1 && probe.mInstances[1].mEmitters != 0;
        assert(subject_tickable == !negative);
    }
    std::cout << "PASS: dispatcher oracle rejects planted solo mask on enabled Subject rigs\n";
}
'''


def extended_shim(shim):
    """Extend the existing standalone LLSD double for the actual caller bodies."""
    shim = shim.replace('struct LLSD {', r'''
struct LLUUID {
    std::string value;
    static const LLUUID null;
    std::string asString() const { return value; }
    void set(const std::string& v, bool = false) { value = v; }
    void setNull() { value.clear(); }
    bool isNull() const { return value.empty(); }
    bool notNull() const { return !value.empty(); }
    bool operator==(const LLUUID& other) const { return value == other.value; }
    bool operator<(const LLUUID& other) const { return value < other.value; }
};
const LLUUID LLUUID::null{};
using uuid_vec_t = std::vector<LLUUID>;
struct LLSD {''')
    shim = shim.replace('INTEGER, BOOL }', 'INTEGER, BOOL, UUID }')
    shim = shim.replace('LLSD()=default;', '''LLSD()=default;
    LLSD(const LLUUID& v):type(UUID),text(v.value){}
    bool operator==(const LLSD& b) const { return type==b.type && number==b.number && text==b.text && fields==b.fields && values==b.values; }
    LLSD& with(const char* k, const LLSD& v) { (*this)[k]=v; return *this; }
    LLSD(const std::string& v):type(STRING),text(v){}
    bool isUUID()const{return type==UUID;}
    bool isBoolean()const{return type==BOOL;}
    LLUUID asUUID()const{return {text};}
    using array_const_iterator=std::vector<LLSD>::const_iterator;
    array_const_iterator beginArray()const{return values.begin();}
    array_const_iterator endArray()const{return values.end();}''')
    return shim


def caller_fixtures(root: Path, controller: str):
    manager = (root / 'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf-8')
    header = (root / 'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf-8')
    rig_header = (root / 'indra/newview/alcinelightrig.h').read_text(encoding='utf-8')
    start = rig_header.index('namespace ALCineLightRigPersistence')
    end = rig_header.index('struct ALCineLightRigParamBlob;', start)
    lines = [rig_header[start:end], SUPPORT]
    lines.append(function(controller, 'struct OptionalSetupGlobals') + ';')
    lines.append(function(controller, 'OptionalSetupGlobals optionalSetupGlobalsFromLLSD('))
    for signature in ('LLSD setupToLLSD(', 'LLSD globalsToLLSD(', 'LLSD transformsToLLSD(',
                      'LLSD cueToLLSD(', 'LLSD cueListToLLSD(',
                      'Globals globalsFromLLSD(', 'Transforms transformsFromLLSD(',
                      'bool cueFromLLSD(', 'bool cueListFromLLSD('):
        lines.append(function(controller, signature))
    for signature in ('CueState ALCineLightRig::releasedCueState(',
                      'void ALCineLightRig::setCueList(',
                      'bool ALCineLightRig::validateCueListData(',
                      'LLSD ALCineLightRig::cueListData() const',
                      'bool ALCineLightRig::applyCueListData(',
                      'void ALCineLightRig::startCueTransition(',
                      'void ALCineLightRig::cueRelease(',
                      'void ALCineLightRig::applyCueSceneState(',
                      'void ALCineLightRig::applySceneData('):
        lines.append(function(controller, signature))
    lines.append(function(header, 'struct ALCineLightRigParamBlob') + ';')
    lines.append(function(header, 'inline LLSD ALCineLightRigParamBlob::toLLSD()'))
    lines.append(function(header, 'inline ALCineLightRigParamBlob ALCineLightRigParamBlob::fromLLSD('))
    lines.append('namespace ALCineLightRigManagerModel {\n' +
                 function(header, 'struct RigSession') + ';\n' +
                 function(header, 'enum class TickPath') + ';\n' +
                 function(header, 'inline void clearActorOnlyForLegacyMigration(') + '\n}')
    lines.append(r'''
#include <set>
struct ALKitHistory {static inline int depth=0;struct ScopedSuspend {ScopedSuspend(){++depth;}~ScopedSuspend(){--depth;}};};
S32 slotIndex(ALCineLightRigSlot);
const LLUUID gAgentID{"self"};
struct LLDirectorCast {
    LLUUID subjects[4]={{"subject-A"},{"subject-B"},{"subject-C"},{"subject-D"}};
    std::set<LLUUID> members;
    static LLDirectorCast& instance(){static LLDirectorCast cast;return cast;}
    static LLUUID canonicalActorId(const LLUUID& id){return id;}
    LLUUID getSubjectA() const{return subjects[0];} LLUUID getSubjectB() const{return subjects[1];}
    LLUUID getSubjectC() const{return subjects[2];} LLUUID getSubjectD() const{return subjects[3];}
    U64 getCastRevision()const{return 1;}bool contains(const LLUUID& id) const{return members.count(id)!=0;}
};
''')
    lines.append(MANAGER)
    lines.append(r'''
namespace ALCineLightRigManagerModel {struct LiveProbeConfig {};bool liveProbeConfigFromLLSD(const LLSD&,LiveProbeConfig&){return true;}}
void liveProbeConfigToSettings(const ALCineLightRigManagerModel::LiveProbeConfig&){}
struct ALGhostStudio {struct Instance{bool mMirrorLightRig=false;};std::vector<Instance> instances;
static ALGhostStudio& instance(){static ALGhostStudio s;return s;}std::vector<Instance>& getInstances(){return instances;}};
''')
    for signature in ('inline U32 slotToGroupBit(', 'inline S32 enabledCount(',
                      'inline ALCineLightRigSlot lowestEnabled(',
                      'inline ALCineLightRigSlot normalizeSelected(', 'inline TickPath pathFor('):
        lines.append(function(header, signature))
    lines.append((root / 'indra/newview/alriggroupsubject.h').read_text(encoding='utf8').replace('#include "lluuid.h"', ''))
    for signature in ('LLUUID legacyAnchorForSlot(',
                      'LLUUID ALCineLightRigManager::slotActor(',
                      'ALCineLightRigManager::Slot ALCineLightRigManager::actorSlot(',
                      'ALCineLightRigManager::Slot ALCineLightRigManager::borrowSlot(',
                      'ALCineLightRigManager::Slot ALCineLightRigManager::leastRecentlyUsedSlot(',
                      'void ALCineLightRigManager::touchSlot(',
                      'void ALCineLightRigManager::giveActorRig(',
                      'void ALCineLightRigManager::removeBorrowedRig(',
                      'void ALCineLightRigManager::pruneRemovedAnchors(', 'void ALCineLightRigManager::refreshBorrowAnchors(',
                      'void ALCineLightRigManager::rewriteActorReferences(',
                      'bool validSlotValue(', 'S32 slotIndex(', 'bool blobMapWellFormed(',
                      'ALCineLightRigParamBlob ALCineLightRigParamBlob::fromSettings(',
                      'void ALCineLightRigParamBlob::captureRigState(',
                      'void ALCineLightRigParamBlob::applyRigState(',
                      'ALCineLightRigManager::ParamBlob ALCineLightRigManager::captureSelected() const',
                      'bool ALCineLightRigManager::validSlot(',
                      'U32 ALCineLightRigManager::enabledMask() const',
                      'void ALCineLightRigManager::writeSelectedSettings()',
                      'void ALCineLightRigManager::setSelectedSlot(',
                      'void ALCineLightRigManager::shutdownSlot(',
                      'void ALCineLightRigManager::onEnabledChanged(',
                      'void ALCineLightRigManager::subjectRigOff(',
                      'void ALCineLightRigManager::subjectRigsOff()',
                      'void ALCineLightRigManager::allOff()',
                      'void ALCineLightRigManager::restoreEnabledMask(',
                      'void ALCineLightRigManager::persist() const',
                      'void ALCineLightRigManager::tick(',
                      'void ALCineLightRigManager::shutdown()',
                      'void ALCineLightRigManager::migrateLegacyScene(',
                      'bool ALCineLightRigManager::validateInstanceStore(',
                      'bool ALCineLightRigManager::restore(',
                      'void ALCineLightRigManager::setLitGroup(',
                      'void ALCineLightRigManager::applySceneData('):
        lines.append(function(manager, signature))
    scene = function(manager, 'void ALCineLightRigManager::applySceneData(')
    assert 'setLitGroup(static_cast<Slot>(i), std::string());' in scene
    lines.append(scene.replace('::applySceneData(', '::applySceneDataStale(', 1).replace('setLitGroup(static_cast<Slot>(i), std::string());', '(void)i;'))
    tick = function(manager, 'void ALCineLightRigManager::tick(')
    assert 'const U32 enabled_mask = enabledMask();' in tick
    lines.append(tick.replace('ALCineLightRigManager::tick(', 'ALCineLightRigManager::tickSoloMask(', 1).replace(
        'const U32 enabled_mask = enabledMask();', 'const U32 enabled_mask = enabledMask() & 1u;'))
    from rig_lit_group_check import fixture as lit_fixture
    lines.append(lit_fixture(root))
    lines.append(ui_fixture(root))
    lines.append(assignment_fixture(root))
    model_tests = (root / 'indra/newview/tests/alcinelightrigmodel_test.cpp').read_text(encoding='utf8')
    lines.append(function(model_tests, 'ALCineLightRigParamBlob distinctiveBlob()'))
    for number in range(128, 134):
        signature = f'void cine_light_rig_model_object::test<{number}>()'
        lines.append(function(model_tests, signature).replace(signature, f'void testSession{number}()'))
    lines.append(TESTS)
    return lines


if __name__ == '__main__':
    # The recovery fixtures are linked into the production sway executable;
    # running this module must execute those assertions, not just define them.
    from rig_sway_check import main
    raise SystemExit(main())
