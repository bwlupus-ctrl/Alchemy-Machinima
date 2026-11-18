"""Execute the production Extra rims undo delta against real LLSD/LLUUID.

Only cast eligibility/identity services are stubbed. The test preserves unrelated
actor edits, checks conflict refusal, rekeys queued snapshots and enforces the
four-person cap. Generated C++/binaries/logs stay under this checkout on I:.
"""
from pathlib import Path
import re

from native_check_tools import check_directory, compile_run, viewer_native_flags
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    root = Path(root)
    ui = (root / 'indra/newview/alpanelrigrim.cpp').read_text(encoding='utf8')
    record = function(ui, 'void ALExtraRimUI::recordEdit(')
    assert 'extraRimSceneEpoch() != pending->sceneEpoch' in record, 'xr_ui_scene_epoch_fence'
    assert 'shell->history().revision()' not in record, 'xr_ui_unrelated_history_keeps_undo'
    assert record.index('const LLSD after =') < record.index('doOnIdleOneTime('), 'xr_ui_proxy_snapshot_resampled'
    assert 'pending->edits.push_back({before, after, label});' in record, 'xr_ui_proxy_snapshot_resampled'
    assert 'recordExtraRimSnapshot(shell, page, edit.before, edit.after, edit.label);' in record, 'xr_ui_proxy_wrong_page'
    assert 'ALKitHistory::ScopedSource source(page);' in function(ui, 'void recordExtraRimSnapshot('), 'xr_ui_proxy_wrong_page'
    assert 'LLFloaterReg::getInstance("director")' in record and 'if (!shell || !page)' in record, 'xr_ui_dead_host_recovery'
    trim = re.search(r'mTrim->setCommitCallback\(\[this\][^\n]*\n    \{(.*?)\n    \}\);', ui, re.S)
    assert trim, 'xr_ui_strength_gesture'
    assert 'setSliderMouseUpCallback' in ui and 'panel->mExtras.finishTrim();' in ui, 'xr_ui_strength_gesture'
    source = r'''
#include "linden_common.h"
#include "llsd.h"
#include "llsdutil.h"
#include "lluuid.h"
#include <map>
#include <set>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <memory>
struct LLDirectorCast {
    std::set<LLUUID> actors;
    static LLDirectorCast& instance() { static LLDirectorCast value; return value; }
    const void* getStoredMember(const LLUUID& id) const { return actors.count(id) ? this : nullptr; }
};
struct ALCineLightRigManager {
    std::map<LLUUID,LLUUID> aliases;
    U64 sceneEpoch = 1;
    LLSD live;
    ALCineLightRigManager() {
        live["source"]=0;live["items"]=LLSD::emptyArray();
        LLSD item;item["actor"]=LLUUID::null;item["trim"]=1.;live["items"].append(item);
    }
    LLSD captureExtraRims()const { return live; }
    std::shared_ptr<const LLUUID> retainBorrowActor(LLUUID id) { return std::make_shared<const LLUUID>(id); }
    void setExtraRimTrim(LLUUID,F32 value) { live["items"][0]["trim"]=value; }
    static ALCineLightRigManager& instance() { static ALCineLightRigManager value; return value; }
    U64 extraRimSceneEpoch() const { return sceneEpoch; }
    LLUUID resolveBorrowActor(const LLUUID& id, bool) const {
        auto found = aliases.find(id); return found == aliases.end() ? id : found->second;
    }
};
struct ALGhostStudio {
    enum { BACKING_ENTITY, BACKING_OVERLAY };
    struct Clone { int mKind = BACKING_OVERLAY; };
    std::set<LLUUID> overlays;
    static ALGhostStudio& instance() { static ALGhostStudio value; return value; }
    const Clone* getInstance(const LLUUID& id) const {
        static Clone clone; return overlays.count(id) ? &clone : nullptr;
    }
    const Clone* findInstanceByRuntime(const LLUUID&) const { return nullptr; }
};
using ExtraRimRows = std::map<LLUUID,F32>;
'''
    for signature in ('ExtraRimRows extraRimRows(', 'bool patchExtraRimDelta(',
                      'void rewriteExtraRimSnapshot(', 'void resolveExtraRimSnapshot('):
        source += function(ui, signature) + '\n'
    fence = re.search(r'if \((!shell \|\| !page \|\| [^\n]+)\) return;', record)
    assert fence, 'xr_ui_scene_epoch_fence'
    source += ('struct QueueProbe { U64 sceneEpoch; };\n'
               'bool queueAllowed(const void* shell, const void* page, const QueueProbe* pending) {\n'
               'if (' + fence[1] + ') return false;\nreturn true;\n}\n')
    source += r'''
struct TrimControl {
    bool held=false, wheel=false; F32 value=100;
    bool isMouseHeldDown()const{return held;} bool isWheelSettlePending()const{return wheel;}
    LLSD getValue()const{return value;}
};
struct ALKitShell {
    static ALKitShell* find(void*){static ALKitShell shell;return &shell;}
    void* ensurePageBuilt(const std::string&){return this;}
};
struct LLFloater {
    template<class T> T* findChild(const std::string&){return ALKitShell::find(nullptr);}
};
struct LLFloaterReg {
    static LLFloater* getInstance(const std::string&){static LLFloater director;return &director;}
};
struct ALExtraRimUI {
    TrimControl control; TrimControl* mTrim=&control; void* mRoot=nullptr;
    LLSD mTrimBefore; LLUUID mTrimActor; int steps=0; LLSD first,last;
    std::shared_ptr<const LLUUID> mTrimPick; U64 mTrimSceneEpoch=0;
    LLUUID selectedActor()const{return LLUUID::null;}
    void recordEdit(ALKitShell*,const LLSD& before,const std::string&) {
        const LLSD after=ALCineLightRigManager::instance().captureExtraRims();
        if(before!=after){++steps;first=before;last=after;}
    }
    static void finishPendingTrims();
    void finishTrim(bool force=false);
    void commit() {
'''
    source += trim[1] + '\n}\n};\n' + function(ui, 'void ALExtraRimUI::finishTrim(') + '\n'
    source += 'std::set<ALExtraRimUI*> extraRimEditors;\n' + function(ui, 'void ALExtraRimUI::finishPendingTrims(') + '\n'
    page = (root / 'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    boundary = function(page, 'void ALKitPage::undoOnPage(')
    source += 'void pageReplayBoundary(){\n' + boundary[boundary.index('{')+1:boundary.index('    if (ALKitShell* shell')] + '}\n'
    recovery = record[record.index('            if (!shell || !page)'):record.index('            // A scene/reset boundary')]
    source += 'void recoverHost(ALKitShell*& shell, void*& page) {\n' + recovery + '\n}\n'
    source += r'''
void require(bool ok, const char* name) { if (!ok) { std::cerr << name << "\n"; std::exit(1); } }
LLUUID id(int value) {
    return LLUUID(llformat("00000000-0000-0000-0000-%012d", value));
}
LLSD state(int source, std::initializer_list<std::pair<int,F32>> rows) {
    LLSD result; result["version"] = 1; result["source"] = source;
    result["items"] = LLSD::emptyArray();
    for (const auto& row : rows) {
        LLSD item; item["actor"] = id(row.first); item["trim"] = row.second;
        result["items"].append(item);
    }
    return result;
}
int main() {
    ALKitShell* recovered=nullptr;void* recoveredPage=nullptr;
    recoverHost(recovered,recoveredPage);
    require(recovered && recoveredPage,"xr_ui_dead_host_recovery");
    recoveredPage=nullptr;recoverHost(recovered,recoveredPage);
    require(recovered && recoveredPage,"xr_ui_dead_host_recovery");
    ALExtraRimUI strength;
    strength.control.held=true;
    for(int n=0;n<20;++n){strength.control.value=100-n;strength.commit();strength.finishTrim();}
    require(strength.steps==0,"xr_ui_strength_gesture");
    strength.control.held=false;strength.finishTrim();
    require(strength.steps==1 && strength.first["items"][0]["trim"].asReal()==1. &&
        fabs(strength.last["items"][0]["trim"].asReal()-.81)<1.e-6,"xr_ui_strength_gesture");
    strength.control.wheel=true;
    for(int n=0;n<10;++n){strength.control.value=80-n;strength.commit();strength.finishTrim();}
    require(strength.steps==1,"xr_ui_strength_gesture");
    ALCineLightRigManager::instance().live["source"]=3;
    LLSD unrelated;unrelated["actor"]=LLUUID("00000000-0000-0000-0000-000000000099");unrelated["trim"]=.5;
    ALCineLightRigManager::instance().live["items"].append(unrelated);
    strength.control.wheel=false;strength.finishTrim();
    require(strength.steps==2 && fabs(strength.first["items"][0]["trim"].asReal()-.81)<1.e-6 &&
        fabs(strength.last["items"][0]["trim"].asReal()-.71)<1.e-6,"xr_ui_strength_gesture");
    require(strength.first["source"].asInteger()==3 && strength.first["items"].size()==2 &&
        strength.first["items"][1]==strength.last["items"][1],"xr_ui_strength_unrelated");
    strength.control.value=50;strength.commit();strength.finishTrim();
    require(strength.steps==3,"xr_ui_strength_gesture");
    strength.control.wheel=true;strength.control.value=30;strength.commit();
    strength.finishTrim(true); // menu boundary, before membership disappears
    LLSD removed = ALCineLightRigManager::instance().live;
    ALCineLightRigManager::instance().live["items"]=LLSD::emptyArray();
    require(strength.steps==4 && fabs(strength.first["items"][0]["trim"].asReal()-.5)<1.e-6 &&
        fabs(strength.last["items"][0]["trim"].asReal()-.3)<1.e-6,"xr_ui_strength_removal");
    ALCineLightRigManager::instance().live=removed; // undo removal
    ALCineLightRigManager::instance().live=strength.first; // undo Strength
    require(fabs(ALCineLightRigManager::instance().live["items"][0]["trim"].asReal()-.5)<1.e-6,"xr_ui_strength_removal");
    strength.control.wheel=true;strength.control.value=20;strength.commit();
    extraRimEditors.insert(&strength);
    pageReplayBoundary(); // before page-level sequence selection and prevalidation
    require(strength.steps==5 && fabs(strength.last["items"][0]["trim"].asReal()-.2)<1.e-6,"xr_ui_strength_prevalidation");
    extraRimEditors.clear();
    strength.control.wheel=true;strength.control.value=25;strength.commit();
    ++ALCineLightRigManager::instance().sceneEpoch;
    strength.control.wheel=false;strength.finishTrim();
    require(strength.steps==5 && strength.mTrimBefore.isUndefined(),"xr_ui_strength_epoch");
    ALCineLightRigManager::instance().sceneEpoch=1;
    QueueProbe pending{1}; int owner=1, page=1, unrelatedHistoryRevision=1;
    require(queueAllowed(&owner,&page,&pending), "xr_ui_epoch_guard");
    ++unrelatedHistoryRevision;
    require(unrelatedHistoryRevision==2 && queueAllowed(&owner,&page,&pending), "xr_ui_epoch_guard");
    ALCineLightRigManager::instance().sceneEpoch=2;
    require(!queueAllowed(&owner,&page,&pending) && !queueAllowed(nullptr,&page,&pending) &&
        !queueAllowed(&owner,nullptr,&pending), "xr_ui_epoch_guard");
    ALCineLightRigManager::instance().sceneEpoch=1;
    for (int n=1;n<=8;++n) LLDirectorCast::instance().actors.insert(id(n));
    std::string reason;
    const LLSD before = state(0, {{1,1.f}}), after = state(0, {{1,.7f}});
    LLSD live = state(3, {{2,.4f},{1,.7f}});
    require(patchExtraRimDelta(live,after,before,reason), "xr_ui_delta_unrelated");
    require(live["source"].asInteger()==3 && live["items"].size()==2 &&
        live["items"][0]["actor"].asUUID()==id(2) && live["items"][1]["actor"].asUUID()==id(1) &&
        (F32)live["items"][0]["trim"].asReal()==.4f && (F32)live["items"][1]["trim"].asReal()==1.f,
        "xr_ui_delta_unrelated");
    const LLSD priority_before=state(0,{{1,1.f},{2,1.f},{3,1.f},{4,1.f}});
    const LLSD priority_after=state(0,{{2,1.f},{3,1.f},{4,1.f}});
    live=priority_after;
    require(patchExtraRimDelta(live,priority_after,priority_before,reason) && live==priority_before,
        "xr_ui_restore_priority");
    // A later actor keeps its position relative to all other live actors.
    // Restoring actor 1 still puts it before its original neighbor 2.
    live=state(0,{{5,.5f},{2,1.f},{3,1.f}});
    require(patchExtraRimDelta(live,priority_after,priority_before,reason) &&
        live==state(0,{{5,.5f},{1,1.f},{2,1.f},{3,1.f}}), "xr_ui_restore_priority");
    live=state(0,{{1,.8f}});
    require(!patchExtraRimDelta(live,after,before,reason), "xr_ui_row_conflict");
    live=state(2,{{1,1.f}});
    require(!patchExtraRimDelta(live,state(1,{{1,1.f}}),before,reason), "xr_ui_source_conflict");
    live=state(0,{{1,1.f},{2,1.f},{3,1.f},{4,1.f}});
    require(!patchExtraRimDelta(live,state(0,{}),state(0,{{5,1.f}}),reason), "xr_ui_cap_conflict");
    LLSD old_before=before, old_after=after;
    rewriteExtraRimSnapshot(old_before,id(1),id(6));
    rewriteExtraRimSnapshot(old_after,id(1),id(6));
    live=state(0,{{6,.7f},{2,.4f}});
    require(patchExtraRimDelta(live,old_after,old_before,reason) &&
        live["items"][0]["actor"].asUUID()==id(6) && (F32)live["items"][0]["trim"].asReal()==1.f,
        "xr_ui_rekey_snapshot");
    rewriteExtraRimSnapshot(old_before,id(6),LLUUID::null);
    require(old_before["items"].size()==0, "xr_ui_rekey_snapshot");
    old_before=before; ALCineLightRigManager::instance().aliases[id(1)]=id(7);
    resolveExtraRimSnapshot(old_before);
    require(old_before["items"][0]["actor"].asUUID()==id(7), "xr_ui_queued_rekey");
    ALCineLightRigManager::instance().aliases[id(1)]=LLUUID::null;
    old_before=before; resolveExtraRimSnapshot(old_before);
    require(old_before["items"].size()==0, "xr_ui_queued_rekey");
    live=state(0,{}); ALGhostStudio::instance().overlays.insert(id(3));
    require(!patchExtraRimDelta(live,state(0,{}),state(0,{{3,1.f}}),reason), "xr_ui_actor_eligibility");
    LLDirectorCast::instance().actors.erase(id(4));
    require(!patchExtraRimDelta(live,state(0,{}),state(0,{{4,1.f}}),reason), "xr_ui_actor_eligibility");
    std::cout << "PASS: production Extra rims undo delta preserves unrelated/order, rejects conflicts/cap/invalid actors, rewrites live and queued identities\n";
}
'''
    flags, libraries = viewer_native_flags()
    directory = check_directory('extra_rims_ui')
    baseline = compile_run(directory, source, 'baseline', flags, libraries)
    output = baseline.stdout + baseline.stderr
    (directory / 'baseline.log').write_bytes(output.encode('utf8'))
    assert baseline.returncode == 0, output
    print(baseline.stdout, end='', flush=True)
    mutations = {
        'xr_ui_strength_prevalidation': ('    ALExtraRimUI::finishPendingTrims();', ''),
        'xr_ui_strength_removal': ('strength.finishTrim(true); // menu boundary', 'strength.finishTrim(); // menu boundary'),
        'xr_ui_row_conflict': ('a->second != c->second', 'false'),
        'xr_ui_source_conflict': ('current["source"] != expected["source"]', 'false'),
        'xr_ui_cap_conflict': ('if (live.size() > 4)', 'if (live.size() > 5)'),
        'xr_ui_rekey_snapshot': ('row["actor"] = new_id;', 'row["actor"] = old_id;'),
        'xr_ui_epoch_guard': ('ALCineLightRigManager::instance().extraRimSceneEpoch() != pending->sceneEpoch', 'false'),
        'xr_ui_restore_priority': ('order.insert(insertion, id);', 'order.push_back(id);'),
        'xr_ui_strength_gesture': ('if (mTrimBefore.isUndefined() || mTrimSceneEpoch != rigs.extraRimSceneEpoch())', 'if (true)'),
        'xr_ui_strength_unrelated': ('LLSD before = rigs.captureExtraRims();', 'LLSD before = mTrimBefore;'),
        'xr_ui_strength_epoch': ('if (mTrimSceneEpoch != rigs.extraRimSceneEpoch())', 'if (false)'),
        'xr_ui_dead_host_recovery': ('if (!shell || !page)', 'if (false)'),
    }
    for name, (before, after) in mutations.items():
        assert source.count(before) == 1, (name, 'mutation anchor drift')
        planted = compile_run(directory, source.replace(before, after, 1), name, flags, libraries)
        output = planted.stdout + planted.stderr
        (directory / (name + '.log')).write_bytes(output.encode('utf8'))
        assert planted.returncode != 0, name + ': planted negative unexpectedly passed'
        assert name in output.splitlines(), (name, 'failed a different assertion', output)
        print('PASS: rejected planted ' + name + ' at its named assertion', flush=True)
    print('PASS: proxy history captures endpoints before idle, binds Rim, and uses scene epoch rather than unrelated history revision', flush=True)


if __name__ == '__main__':
    run_unit()
