"""Production caller fixtures for rig_sway_check; renderer/settings services are stubbed.

Function bodies for scene/cue/blob/multi-instance restore are extracted verbatim.
Only viewer lifecycle, settings sinks, UUID storage and presentation time use doubles.
"""
from pathlib import Path

from rig_shadow_presets_check import function


SUPPORT = r'''
#define LL_WARNS(...) std::cerr
struct SettingsSink {
    std::map<std::string, LLSD> values;
    void setBOOL(const std::string& k, bool v) { values[k] = v; }
    void setF32(const std::string& k, F32 v) { values[k] = v; }
    void setS32(const std::string& k, S32 v) { values[k] = v; }
    void setU32(const std::string& k, U32 v) { values[k] = double(v); }
    void setString(const std::string& k, const std::string& v) { values[k] = v; }
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
class ALCineLightRig {
public:
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
    F64 mCueFXEpoch = 0, mCueGoTime = 0;
    void setAnchor(const LLUUID&) {}
    void setObjectTarget(const LLUUID&) {}
    void setGroupEnabled(bool) {}
    void setGroupSlots(U32) {}
    void updateProjectorFlags() {}
    void writeSetupToSettings(const Setup& s) { mStoredSetup = s; }
    CueState captureCueState() const { CueState s; s.mSetup = mStoredSetup; return s; }
    static CueState releasedCueState(const CueState&);
    void setCueList(const CueList&);
    static bool validateCueListData(const LLSD&);
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
constexpr S32 INSTANCE_STORE_VERSION = 1;
class ALCineLightRigManager {
public:
    static constexpr S32 SLOT_COUNT = 5;
    static constexpr Slot SLOT_SELF = Slot::SELF;
    ParamBlob mBlobs[SLOT_COUNT];
    ALCineLightRig mInstances[SLOT_COUNT];
    Slot mSelected = SLOT_SELF;
    struct { Slot mFocus, mChallenger; S32 mChallengeTicks; } mFocusState{};
    static bool validSlot(Slot);
    U32 enabledMask() const;
    bool restore(const LLSD&);
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
    ALCineLightRigManager manager; assert(manager.restore(store));
    assert(manager.mSelected == Slot::B);
    for (S32 slot = 0; slot < 5; ++slot) {
        assert(manager.mBlobs[slot].mMasterEV == slot + .25f);
        assert(manager.mBlobs[slot].mLights[1].mEV == -3.25f);
        checkRecovered(manager.mInstances[slot].mCueList.mCues[0].mSetup);
    }
    // Required structure is still rejected: graceful recovery is optional-only.
    setup["lights"] = LLSD::emptyArray();
    assert(!setupFromLLSD(setup, loaded));
    store["instances"].values[0]["lights"] = LLSD::emptyArray();
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
}
'''


def extended_shim(shim):
    """Extend the existing standalone LLSD double for the actual caller bodies."""
    shim = shim.replace('struct LLSD {', r'''
struct LLUUID {
    std::string value;
    std::string asString() const { return value; }
    void set(const std::string& v) { value = v; }
};
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
    for signature in ('Globals globalsFromLLSD(', 'Transforms transformsFromLLSD(',
                      'bool cueFromLLSD(', 'bool cueListFromLLSD('):
        lines.append(function(controller, signature))
    for signature in ('CueState ALCineLightRig::releasedCueState(',
                      'void ALCineLightRig::setCueList(',
                      'bool ALCineLightRig::validateCueListData(',
                      'bool ALCineLightRig::applyCueListData(',
                      'void ALCineLightRig::startCueTransition(',
                      'void ALCineLightRig::cueRelease(',
                      'void ALCineLightRig::applyCueSceneState(',
                      'void ALCineLightRig::applySceneData('):
        lines.append(function(controller, signature))
    lines.append(function(header, 'struct ALCineLightRigParamBlob') + ';')
    lines.append(function(header, 'inline LLSD ALCineLightRigParamBlob::toLLSD()'))
    lines.append(function(header, 'inline ALCineLightRigParamBlob ALCineLightRigParamBlob::fromLLSD('))
    lines.append(MANAGER)
    for signature in ('inline U32 slotToGroupBit(', 'inline S32 enabledCount(',
                      'inline ALCineLightRigSlot lowestEnabled(',
                      'inline ALCineLightRigSlot normalizeSelected('):
        lines.append(function(header, signature))
    for signature in ('bool validSlotValue(', 'S32 slotIndex(', 'bool blobMapWellFormed(',
                      'void ALCineLightRigParamBlob::applyRigState(',
                      'bool ALCineLightRigManager::validSlot(',
                      'U32 ALCineLightRigManager::enabledMask() const',
                      'bool ALCineLightRigManager::restore('):
        lines.append(function(manager, signature))
    lines.append(TESTS)
    return lines
