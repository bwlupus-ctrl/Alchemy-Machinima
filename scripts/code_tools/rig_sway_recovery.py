"""Production caller fixtures for rig_sway_check; renderer/settings services are stubbed.

Function bodies for scene/cue/blob/multi-instance restore are extracted verbatim.
Only viewer lifecycle, settings sinks, UUID storage and presentation time use doubles.
"""
from pathlib import Path

from rig_shadow_presets_check import function
from rig_session_ui_check import fixture as ui_fixture


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
    static constexpr U32 GROUP_SLOT_MASK = 31;
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
    bool mGroupEnabled = false;
    U32 mGroupSlots = 0;
    S32 mSelectedTicks = 0, mBlobTicks = 0;
    void shutdown() { mEmitters = 0; }
    void tickSelected(F64, bool) { ++mSelectedTicks; mEmitters = gSavedSettings.getBOOL("CineLightRigEnabled") ? 0xffu : 0; }
    void tickFromBlob(const ALCineLightRigParamBlob&, F64, bool);
    void setLiveProbeBounceScale(F32) {}
    F64 mCueFXEpoch = 0, mCueGoTime = 0;
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
    using ParamBlob = ALCineLightRigParamBlob;
    static constexpr S32 SLOT_COUNT = 5;
    static constexpr Slot SLOT_SELF = Slot::SELF;
    ParamBlob mBlobs[SLOT_COUNT];
    ALCineLightRig mInstances[SLOT_COUNT];
    Slot mSelected = SLOT_SELF;
    ALCineLightRigManagerModel::RigSession mRigSession;
    bool mWritingSelectedSettings = false;
    U64 mConfigurationRevision = 1;
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
    void tick(F64);
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
    void createRigGroup();
    void leaveRigGroup();
    void allOff();
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
    assert(manager.mSelected == Slot::B);
    assert(manager.isRigGroup());
    for (S32 slot = 0; slot < 5; ++slot) {
        assert(manager.mBlobs[slot].mMasterEV == slot + .25f);
        assert(manager.mBlobs[slot].mLights[1].mEV == -3.25f);
        checkRecovered(manager.mInstances[slot].mCueList.mCues[0].mSetup);
    }
    // Exercise the actual manager restore/control bodies, with emitter and
    // settings services doubled. Internal buffer swaps must not act as power
    // switches; every external setting setter shares the same callback.
    gSavedSettings.enabledChanged = [&manager](bool enabled) {
        if (!manager.mWritingSelectedSettings) manager.onEnabledChanged(enabled);
    };
    assert(manager.restore(store));
    assert(!manager.isRigGroup() && manager.mSelected == Slot::SELF);
    assert(manager.enabledMask() == 0);
    for (S32 slot = 0; slot < 5; ++slot) {
        assert(!manager.mBlobs[slot].mEnabled);
        assert(manager.mBlobs[slot].mMasterEV == slot + .25f);
        manager.setSelectedSlot(static_cast<Slot>(slot));
        assert(manager.mSelected == Slot::SELF);
    }
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    assert(manager.enabledMask() == 1);
    manager.mMirrors[1] = std::make_unique<ALCineLightRig>();
    manager.mMirrors[1]->mEmitters = 0xffu;
    for (S32 slot = 0; slot < 5; ++slot) {
        manager.mBlobs[slot].mEnabled = true; // adversarial stale hidden state
        manager.mInstances[slot].mEmitters = 0xffu;
    }
    gSavedSettings.setBOOL("CineLightRigEnabled", false);
    assert(manager.enabledMask() == 0 && manager.mMirrors[1]->mEmitters == 0);
    for (const auto& rig_slot : manager.mInstances) assert(rig_slot.mEmitters == 0);
    manager.createRigGroup();
    assert(manager.isRigGroup() && manager.enabledMask() == 0);
    manager.setSelectedSlot(Slot::B);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    manager.mInstances[2].mEmitters = 0xffu;
    manager.setSelectedSlot(Slot::A); // writes false to the editing buffer
    assert(manager.enabledMask() == 4 && manager.mInstances[2].mEmitters == 0xffu);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    gSavedSettings.setBOOL("CineLightRigEnabled", false);
    assert(manager.enabledMask() == 4 && manager.mInstances[2].mEmitters == 0xffu);
    // Leave while still editing a Subject, after a fresh edit in the buffer.
    gSavedSettings.setF32("CineLightRigMasterEV", 7.75f);
    manager.leaveRigGroup();
    assert(manager.mSelected == Slot::SELF && !manager.isRigGroup());
    assert(manager.mBlobs[1].mMasterEV == 7.75f);
    assert(gSavedSettings.getF32("CineLightRigMasterEV") == .25f);
    manager.createRigGroup();
    manager.setSelectedSlot(Slot::SELF);
    gSavedSettings.setBOOL("CineLightRigEnabled", true);
    manager.leaveRigGroup();
    assert(!manager.isRigGroup() && manager.mSelected == Slot::SELF);
    assert(manager.enabledMask() == 1 && manager.mInstances[2].mEmitters == 0);
    assert(manager.mBlobs[2].mMasterEV == 2.25f);
    manager.createRigGroup();
    manager.allOff();
    assert(manager.isRigGroup() && manager.enabledMask() == 0);
    assert(manager.restore(store, true)); // no mode field: infer old saved group
    assert(manager.isRigGroup() && manager.enabledMask() == 31);
    store["rig_group"] = true;
    assert(manager.restore(store)); // login ignores even an explicit saved mode
    assert(!manager.isRigGroup() && manager.enabledMask() == 0);
    assert(manager.mSelected == Slot::SELF);
    LLSD bad_mode = store;
    bad_mode["rig_group"] = "invalid";
    assert(!ALCineLightRigManager::validateInstanceStore(bad_mode));
    assert(!manager.restore(bad_mode, true));
    assert(!manager.isRigGroup() && manager.enabledMask() == 0);
    // The actual dispatcher must shut stale Subject flags/emitters down even
    // while the selected master is already false (there is no setting signal).
    for (S32 i = 1; i < 5; ++i) {
        manager.mBlobs[i].mEnabled = true;
        manager.mInstances[i].mEmitters = 0xffu;
    }
    manager.tick(1002.0);
    for (S32 i = 1; i < 5; ++i) {
        assert(manager.mInstances[i].mEmitters == 0);
        assert(manager.mInstances[i].mBlobTicks == 0);
    }
    // Even an adversarial stale selection cannot take the selected fast path
    // around the mask in solo. Ordinary selection APIs already reject this.
    manager.mSelected = Slot::A;
    manager.tick(1002.5);
    assert(manager.mInstances[1].mSelectedTicks == 0);
    assert(manager.mInstances[1].mEmitters == 0);
    manager.mSelected = Slot::SELF;
    manager.mBlobs[1].mEnabled = true;
    manager.createRigGroup();
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
    manager.shutdown(); // idempotent; must not capture the wrong buffer again
    assert(manager.mBlobs[3].mMasterEV == 6.5f);
    manager.tick(1004.0); // actual session-restore latch, then normal dispatch
    assert(!manager.mNeedsSessionRestore && manager.enabledMask() == 0);
    assert(gSavedSettings.getF32("CineLightRigMasterEV") == .25f);
    assert(manager.restore(saved_off) && manager.enabledMask() == 0);
    // A cast-Subject legacy anchor is still a single solo rig, on or off.
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
        assert(gSavedSettings.getF32("CineLightRigMasterEV") == -4.5f);
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
    std::cout << "PASS: production manager login/scene restore, solo selection and power, group create/leave/all-off, buffer-swap guards and mirror emitter shutdown\n";
    std::cout << "PASS: actual manager tick/shutdown/restore masks stale solo Subjects, persists all off, restores SELF and retains edits; legacy Subject scene remains solo/reachable\n";
    RigSessionUI::test();
    RigTargetUI::test();
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
};
const LLUUID LLUUID::null{};
struct LLSD {''')
    shim = shim.replace('INTEGER, BOOL }', 'INTEGER, BOOL, UUID }')
    shim = shim.replace('LLSD()=default;', '''LLSD()=default;
    LLSD(const LLUUID& v):type(UUID),text(v.value){}
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
    lines.append(MANAGER)
    for signature in ('inline U32 slotToGroupBit(', 'inline S32 enabledCount(',
                      'inline ALCineLightRigSlot lowestEnabled(',
                      'inline ALCineLightRigSlot normalizeSelected(', 'inline TickPath pathFor('):
        lines.append(function(header, signature))
    for signature in ('bool validSlotValue(', 'S32 slotIndex(', 'bool blobMapWellFormed(',
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
                      'void ALCineLightRigManager::createRigGroup()',
                      'void ALCineLightRigManager::leaveRigGroup()',
                      'void ALCineLightRigManager::allOff()',
                      'void ALCineLightRigManager::persist() const',
                      'void ALCineLightRigManager::tick(',
                      'void ALCineLightRigManager::shutdown()',
                      'void ALCineLightRigManager::migrateLegacyScene(',
                      'bool ALCineLightRigManager::validateInstanceStore(',
                      'bool ALCineLightRigManager::restore('):
        lines.append(function(manager, signature))
    lines.append(ui_fixture(root))
    lines.append(TESTS)
    return lines
