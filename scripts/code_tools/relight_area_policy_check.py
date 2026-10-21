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
    assert pipeline.index('registry.areaShadowSuppressed(volume)') < pipeline.index('selected.add(')
    assert 'selected.size,budget(),static_cast<unsigned>(llclamp(cap(),1,8))' in pipeline
    director = method(nv / 'llfloaterdirector.cpp', 'void LLFloaterDirector::refreshLauncherCards()')
    assert 'gSavedSettings.getS32("RenderShadowDetail")' in director
    assert 'ALKitRow::setControlVisible(status, detail == 0 || detail == 1)' in director
    for text in ('SL shadows are off in Preferences; soft shadows still on',
                 'SL projector shadows are off in Preferences; soft shadows still on'):
        assert text in director
    assert 'setS32' not in director and 'getSignal' not in director
    fixture = r'''
#include "alareashadowmath.h"
#include <cassert>
#include <iostream>
#include <set>
using S32 = int;
using LLUUID = int;
struct LLViewerObject { int id; int getID() const { return id; } };
struct LLVOVolume : LLViewerObject {
    LLViewerObject* root = nullptr;
    bool dead = false;
    bool isDead() const { return dead; }
    LLViewerObject* getRootEdit() { return root; }
};
struct Ptr { LLVOVolume* p = nullptr; LLVOVolume* get() const { return p; } };
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
    int projectorId(int i) const { return mProjectors[i].p ? mProjectors[i].p->id : 0; }
    void updateShadowPolicy(S32);
};
struct ALCineLightRigManager {
    enum Slot { FIRST };
    static constexpr int SLOT_COUNT = 1;
    ALCineLightRig rig;
    const ALCineLightRig& at(Slot) const { return rig; }
    bool areaShadowSuppressed(LLVOVolume*) const;
};
'''
    fixture += '\n'.join([
        method(nv / 'pipeline.cpp', 'bool LLPipeline::isProjectorShadowSuppressed('),
        method(nv / 'alcinelightrig.cpp', 'void ALCineLightRig::updateShadowPolicy('),
        method(nv / 'alcinelightrigmanager.cpp', 'bool ALCineLightRigManager::areaShadowSuppressed('),
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
    LLPipeline::toggleProjectorCastShadows(99); // permanent native-map opt-out
    for (int mode : {0,1,2,0,2,1}) {
        manager.rig.updateShadowPolicy(mode);
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
    auto seven=ALAreaShadow::workload(ALAreaShadow::quality(3),1920,1080,7,1500,8);
    auto eight=ALAreaShadow::workload(ALAreaShadow::quality(3),1920,1080,8,1500,8);
    assert(seven.q.samples==2 && eight.q.samples==2);
    assert(seven.q.steps==32 && eight.q.steps==32 && !seven.q.half && !eight.q.half);
    assert(seven.iterations<=seven.budget && eight.iterations<=eight.budget);
    assert(seven.pixels*seven.channels*seven.q.samples*seven.q.steps<=seven.drawCap);
    assert(eight.pixels*eight.channels*eight.q.samples*eight.q.steps<=eight.drawCap);
    std::cout << "{\"samples\":" << seven.q.samples << ",\"steps\":" << seven.q.steps << "}";
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
