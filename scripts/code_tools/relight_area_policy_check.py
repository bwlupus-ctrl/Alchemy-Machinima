"""Compile production CPU policy methods with minimal objects; no viewer build."""
from pathlib import Path
import json
import os
import subprocess


def method(path, signature):
    source = Path(path).read_text(encoding='utf-8')
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def check_policy():
    nv = Path('indra/newview')
    pipeline = method(nv / 'pipeline.cpp', 'void LLPipeline::renderAreaShadows(')
    assert 'const ALAreaShadow::Quality uniform{samples(),steps(),half(),0,0,' in pipeline
    assert 'cube_on() ? ALAreaShadow::lookReach(look_reach()) : 0.f' in pipeline
    assert 'thin() ? ALAreaShadow::segmented(uniform,preset()) : uniform' in pipeline
    # Main and feed passes both carry the authored uniform plan as their fallback.
    assert 'ALAreaShadow::workloadOrUniform(requested,uniform,width,height,' in pipeline
    assert 'ALAreaShadow::feedWorkload(requested,uniform,' in pipeline
    # The shader mode follows the EFFECTIVE plan, so a fallback is the OFF march.
    assert 'uniform1i(LLStaticHashedString("area_thin_objects"),group.march.nearSteps > 0)' in pipeline
    take = method(nv / 'pipeline.cpp', 'void LLPipeline::logRelightTake(')
    assert '"RenderAreaShadowThinObjects"' in take
    assert pipeline.index('registry.areaShadowSuppressed(volume)') < pipeline.index('selected.add(')
    assert 'selected.size,budget(),static_cast<unsigned>(llclamp(cap(),1,8))' in pipeline
    director = method(nv / 'llfloaterdirector.cpp', 'void LLFloaterDirector::refreshLauncherCards()')
    assert 'gSavedSettings.getS32("RenderShadowDetail")' in director
    assert 'ALKitRow::setControlVisible(status, detail == 0 || detail == 1)' in director
    for text in ('SL shadows are off in Preferences; Soft shadows is on',
                 'SL shadows are off in Preferences; Soft shadows is off',
                 'SL projector shadows are off in Preferences; Soft shadows is on',
                 'SL projector shadows are off in Preferences; Soft shadows is off'):
        assert text in director
    assert 'setS32' not in director and 'getSignal' not in director
    fixture = r'''
#include "alareashadowmath.h"
#include <cassert>
#include <iostream>
#include <set>
using S32 = int;
using U32 = unsigned;
struct LLUUID {
    int value=0; LLUUID(int n=0):value(n) {} operator int() const { return value; }
    bool notNull() const { return value!=0; }
};
struct LLViewerObject { int id; bool isDead() const { return false; } int getID() const { return id; } };
struct LLVOVolume : LLViewerObject {
    LLViewerObject* root = nullptr;
    bool dead = false;
    bool isDead() const { return dead; }
    LLViewerObject* getRootEdit() { return root; }
};
struct Ptr { LLVOVolume* p = nullptr; LLVOVolume* get() const { return p; } };
struct Objects {
    LLVOVolume* key=nullptr;
    LLVOVolume* findObject(int id) const { return key && key->id==id && !key->isDead() ? key : nullptr; }
} gObjectList;
struct Settings { int mode=0; int getS32(const char*) const { return mode; } } gSavedSettings;
struct ALCineLightRigModel { static constexpr int LIGHT_COUNT=4; };
struct LLPipeline {
    static inline std::set<int> sNoShadowProjectors;
    static bool isProjectorNoShadow(int id) { return sNoShadowProjectors.count(id); }
    static void toggleProjectorCastShadows(int id) {
        if (!sNoShadowProjectors.erase(id)) sNoShadowProjectors.insert(id);
    }
    static bool isProjectorShadowSuppressed(LLVOVolume*);
};
struct ALCineLightRig {
    static constexpr int LIGHT_COUNT = 4;
    Ptr mProjectors[4], mOmnis[4], mCatchlight;
    int catchlightId() const { return mCatchlight.p ? mCatchlight.p->id : 0; }
    LLUUID projectorId(int i) const { return mProjectors[i].p ? mProjectors[i].p->id : 0; }
    LLUUID omniId(int i) const { return mOmnis[i].p ? mOmnis[i].p->id : 0; }
    void updateShadowPolicy(S32);
};
struct ALCineLightRigManager {
    enum Slot { FIRST };
    static constexpr int SLOT_COUNT = 2;
    ALCineLightRig mInstances[2]; ALCineLightRig& rig=mInstances[0];
    Slot mSelected=FIRST;
    struct Blob { int mShadowMode=0; } mBlobs[2];
    struct Focus { int mFocus=0; } mFocusState;
    unsigned enabledMask() const { return 1; }
    unsigned suppressedSlotMask(unsigned,int focus) const { return focus ? 1 : 0; }
    unsigned slotToGroupBit(Slot) const { return 1; }
    const ALCineLightRig& at(Slot slot) const { return mInstances[slot]; }
    bool areaShadowSuppressed(LLVOVolume*) const;
    void applyShadowSuppression(); void clearShadowSuppression();
};
'''
    fixture += '\n'.join([
        method(nv / 'pipeline.cpp', 'bool LLPipeline::isProjectorShadowSuppressed('),
        method(nv / 'alcinelightrig.cpp', 'void ALCineLightRig::updateShadowPolicy('),
        method(nv / 'alcinelightrigmanager.cpp', 'bool ALCineLightRigManager::areaShadowSuppressed('),
        method(nv / 'alcinelightrigmanager.cpp', 'void ALCineLightRigManager::applyShadowSuppression('),
        method(nv / 'alcinelightrigmanager.cpp', 'void ALCineLightRigManager::clearShadowSuppression('),
    ])
    fixture += r'''
int main() {
    ALCineLightRigManager manager;
    LLViewerObject root{100};
    LLVOVolume child{{10}, &root}, next{{20}}, third{{30}};
    auto select = [&]() {
        ALAreaShadow::Selection<int> out;
        for (auto* v : {&third, &child, &next})
            if (!manager.areaShadowSuppressed(v)) out.add({v->id,false,1},2);
        return out;
    };
    assert(select().lights[0].id == 10); // ordinary point stays eligible
    for (int id : {10,100}) {
        LLPipeline::toggleProjectorCastShadows(id);
        auto selected = select();
        assert(selected.size == 2 && selected.candidates == 2);
        assert(selected.lights[0].id == 20 && selected.lights[1].id == 30);
        LLPipeline::toggleProjectorCastShadows(id);
        assert(select().lights[0].id == 10); // live on/off/on, same objects
    }
    assert(!LLPipeline::isProjectorShadowSuppressed(nullptr));
    LLVOVolume projectors[4], omnis[4], catchlight{{99}};
    for (int i=0; i<4; ++i) {
        projectors[i].id=40+i; omnis[i].id=50+i;
        manager.rig.mProjectors[i].p=&projectors[i];
        manager.rig.mOmnis[i].p=&omnis[i];
    }
    manager.rig.mCatchlight.p=&catchlight;
    gObjectList.key=&projectors[0];
    LLPipeline::toggleProjectorCastShadows(99); // permanent native-map opt-out
    for (int mode : {0,1,2,0,2,1}) {
        manager.rig.updateShadowPolicy(mode);
        gSavedSettings.mode=mode;
        for (int i=0; i<4; ++i) {
            const bool off = mode==0 || (mode==1 && i!=0);
            assert(manager.areaShadowSuppressed(&projectors[i]) == off);
            assert(manager.areaShadowSuppressed(&omnis[i]) == off);
        }
        assert(manager.areaShadowSuppressed(&catchlight) == (mode==0));
        assert(LLPipeline::isProjectorNoShadow(99)); // native policy unchanged
    }
    // An absent projector must not prevent its surviving bounce from updating.
    manager.rig.mProjectors[1].p=nullptr;
    manager.rig.updateShadowPolicy(0);
    assert(manager.areaShadowSuppressed(&omnis[1]));
    manager.rig.updateShadowPolicy(2);
    assert(!manager.areaShadowSuppressed(&omnis[1]));
    // Key absent/dead: the rig mode, rather than a missing UUID flag, wins.
    for (bool dead : {false,true}) {
        manager.rig.mProjectors[0].p=dead ? &projectors[0] : nullptr;
        projectors[0].dead=dead;
        for (int mode : {0,1,2,0}) {
            gSavedSettings.mode=mode;
            assert(manager.areaShadowSuppressed(&catchlight)==(mode==0));
        }
    }
    manager.mSelected=static_cast<ALCineLightRigManager::Slot>(1);
    gSavedSettings.mode=2;
    manager.mBlobs[0].mShadowMode=0;
    assert(manager.areaShadowSuppressed(&catchlight));
    manager.mBlobs[0].mShadowMode=1;
    assert(!manager.areaShadowSuppressed(&catchlight));
    manager.mSelected=ALCineLightRigManager::FIRST;
    // Focus suppression and clearing must reach the surviving bounce as well.
    projectors[0].dead=false;manager.rig.mProjectors[0].p=&projectors[0];
    manager.rig.updateShadowPolicy(2);
    manager.mFocusState.mFocus=1;manager.applyShadowSuppression();
    for(auto& omni:omnis) assert(manager.areaShadowSuppressed(&omni));
    assert(manager.areaShadowSuppressed(&projectors[0]));
    manager.clearShadowSuppression();
    for(auto& omni:omnis) assert(!manager.areaShadowSuppressed(&omni));
    assert(!manager.areaShadowSuppressed(&projectors[0]));
    auto seven=ALAreaShadow::workload(ALAreaShadow::quality(3,true),1920,1080,7,1500,8);
    auto eight=ALAreaShadow::workload(ALAreaShadow::quality(3,true),1920,1080,8,1500,8);
    assert(seven.q.samples==5 && eight.q.samples==5); // ray-floor ladder (was 4 rays x 80 steps)
    assert(seven.q.steps==64 && eight.q.steps==64 && seven.q.half && eight.q.half);
    assert(seven.admitted==8 && eight.admitted==8);
    assert(seven.iterations<=seven.budget && eight.iterations<=eight.budget);
    assert(seven.pixels*seven.channels*seven.q.samples*seven.q.steps<=seven.drawCap);
    assert(eight.pixels*eight.channels*eight.q.samples*eight.q.steps<=eight.drawCap);
    std::cout << "{\"samples\":" << seven.q.samples << ",\"steps\":" << seven.q.steps << ",\"presets\":[";
    for (int preset=0;preset<4;++preset) {
        auto q=ALAreaShadow::quality(preset,true);
        auto plan=ALAreaShadow::workload(q,1920,1080,4,1500,4);
        if (preset) std::cout << ",";
        std::cout << "[" << plan.q.samples << "," << plan.q.steps-q.nearSteps << ","
            << plan.q.half << "," << q.nearSteps << "]";
    }
    std::cout << "],\"off_presets\":[";
    for (int preset=0;preset<4;++preset) for (bool half:{false,true}) {
        auto q=ALAreaShadow::quality(preset); q.half=half;
        auto plan=ALAreaShadow::workload(q,1920,1080,4,1500,4);
        if (preset || half) std::cout << ",";
        std::cout << "[" << plan.q.samples << "," << plan.q.steps << "," << plan.q.half << "]";
    }
    std::cout << "],\"grid\":[";
    // [thin, height, K, preset, feeds, samples, steps, near, half, admitted, channels, iterations, lead, rest samples,
    // rest steps, rest channels]: the first lead lights march the plan columns, the others the rest columns.
    // The production planner for every preset x height x K x Thin x feed reservation, exactly as
    // renderAreaShadows calls it (main view, default 1500M budget). The P0 grain check re-derives
    // each row with an independent ladder implementation and compares.
    bool firstRow=true;
    for (int thin=0; thin<2; ++thin) for (unsigned height : {720u,1080u,1440u,2160u})
    for (unsigned cap=1; cap<=8; ++cap) for (int preset=0; preset<4; ++preset) for (int feeds=0; feeds<2; ++feeds) {
        const auto uniform=ALAreaShadow::quality(preset);
        const auto requested=thin ? ALAreaShadow::segmented(uniform,preset) : uniform;
        const auto plan=ALAreaShadow::workloadOrUniform(requested,uniform,height*16/9,height,cap,1500,cap,
            feeds ? ALAreaShadow::kFeedSlots : 0);
        if (!firstRow) std::cout << ",";
        firstRow=false;
        std::cout << "[" << thin << "," << height << "," << cap << "," << preset << "," << feeds << ","
            << plan.q.samples << "," << plan.q.steps << "," << plan.q.nearSteps << "," << plan.q.half << ","
            << plan.admitted << "," << plan.channels << "," << plan.iterations << ","
            << plan.lead << "," << plan.rest.samples << "," << plan.rest.steps << "," << plan.restChannels << "]";
    }
    std::cout << "],\"radius\":[";
    {
        const float settings[]={0.f,1.f,2.f,4.f,8.f};
        bool firstRadius=true;
        for (float setting : settings) for (float scale : {.333f,1.f,2.f}) for (bool half : {false,true}) {
            if (!firstRadius) std::cout << ",";
            firstRadius=false;
            std::cout << "[" << setting << "," << scale << "," << half << ","
                << ALAreaShadow::filterRadius(setting,scale,half) << "]";
        }
    }
    std::cout << "]}";
}
'''
    directory = Path('.tmp/relight_step1_policy').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'policy.cpp').write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory / 'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /I"{nv.resolve()}" '
        f'/Fo"{directory / "policy.obj"}" /Fe"{directory / "policy.exe"}" '
        f'"{directory / "policy.cpp"}"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(directory / 'compile.cmd')], check=True)
    result = json.loads(subprocess.check_output([str(directory / 'policy.exe')], text=True))
    print('[RelightQ] CPU policy PASS: K7/K8 Cinematic 1080p; suppression before top-K, '
          'replacement channels, live own/root flags, rig Off/Key/All, bounce/catchlight; status refresh', flush=True)
    return result
