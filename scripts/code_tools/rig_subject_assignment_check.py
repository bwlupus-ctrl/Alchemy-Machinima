"""Exercise production Subject setters and per-slot shutdown, with negatives."""
from pathlib import Path
from rig_shadow_presets_check import function


def assignment_fixture(root: Path) -> str:
    cast = (root / 'indra/newview/lldirectorcast.cpp').read_text(encoding='utf8')
    # Scene restore and same-actor clone rekeys intentionally bypass manual setters.
    rekey = function(cast, 'void LLDirectorCast::replaceActorId(')
    assert 'if (*subject == old_id) { *subject = new_id; ++mSubjectRevision; }' in rekey
    load = function(cast, 'void LLDirectorCast::applySceneData(')
    assert 'mSubjectA = a;' in load and 'mSubjectD = d;' in load
    lines = [r'''
namespace RigSubjectAssignment {
struct LLDirectorCast {
    LLUUID mSubjectA, mSubjectB, mSubjectC, mSubjectD;
    U64 mSubjectRevision=0;
    U32 mSubjectAssignmentDepth=0;
    LLUUID mSubjectAssignmentBefore[4];
    void beginSubjectAssignment(); void endSubjectAssignment();
    const LLUUID& getSubjectA() const { return mSubjectA; }
    const LLUUID& getSubjectB() const { return mSubjectB; }
    const LLUUID& getSubjectC() const { return mSubjectC; }
    const LLUUID& getSubjectD() const { return mSubjectD; }
    static LLUUID canonicalActorId(const LLUUID& id) {
        return id.value=="alice-runtime" ? LLUUID{"alice"} : id;
    }
    void touchScene() {}
    void setSubjectA(const LLUUID&); void setSubjectB(const LLUUID&);
    void setSubjectC(const LLUUID&); void setSubjectD(const LLUUID&);
};
''']
    lines.append('#define llassert assert')
    for method in ('beginSubjectAssignment', 'endSubjectAssignment'):
        lines.append(function(cast, f'void LLDirectorCast::{method}(').replace('static_cast<ALCineLightRigManager::Slot>', 'static_cast<Slot>'))
    for letter in 'ABCD':
        lines.append(function(cast, f'void LLDirectorCast::setSubject{letter}('))
    floater = (root / 'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf8')
    lines.append('enum { SUBJECT_A=1, SUBJECT_B, SUBJECT_C, SUBJECT_D };')
    lines.append(function(floater, 'void clear_subject('))
    lines.append(function(floater, 'void assign_subject('))
    bulk = function(floater, 'void LLFloaterDirector::onClickAssignSubjects(')
    bulk = bulk[bulk.index('    cast.beginSubjectAssignment();'):bulk.index('    LLSD args;', bulk.index('    cast.beginSubjectAssignment();'))]
    lines.append('void bulkAssign(LLDirectorCast& cast, const std::vector<LLUUID>& ordered) {\n' + bulk + '}')
    drag = floater[floater.index('                                if (batch && index == 0)'):]
    drag = drag[:drag.index('                            }\n                            return true;')]
    lines.append('struct Tool { S32 count; S32 getCargoCount() const { return count; } };')
    lines.append('void dropAssign(LLDirectorCast& cast, const std::vector<LLUUID>& ids) {'
                 'Tool storage{(S32)ids.size()}; Tool* tool=&storage; bool batch=true; S32 subject=SUBJECT_A;'
                 'for(S32 index=0;index<(S32)ids.size();++index) { void* cargo=(void*)&ids[index];\n' +
                 drag.replace('                                    cast.add(id);\n', '') + '}}')
    # Same oracle must reject premature retirement during temporary clears.
    bad = '\n'.join(lines).replace('namespace RigSubjectAssignment {', 'namespace RigSubjectAssignmentBad {')
    bad = bad.replace('if (--mSubjectAssignmentDepth != 0) return;', '--mSubjectAssignmentDepth;')
    no_retirement = '\n'.join(lines).replace('namespace RigSubjectAssignment {', 'namespace RigSubjectAssignmentArmed {')
    no_retirement = no_retirement.replace('if (mSubjectAssignmentBefore[i] != canonicalActorId(subjects[i]))', 'if (false)')
    lines.append('} // RigSubjectAssignment')
    lines.append(bad + '\n} // RigSubjectAssignmentBad')
    lines.append(no_retirement + '\n} // RigSubjectAssignmentArmed')
    lines.append(r'''
template<class Cast> bool assignmentOracle() {
    Cast cast;
    ALCineLightRigManager manager;
    ALCineLightRigManager::testInstance=&manager;
    gSavedSettings.enabledChanged=[&](bool enabled) {
        if(!manager.mWritingSelectedSettings) manager.onEnabledChanged(enabled);
    };
    const auto setters = { &Cast::setSubjectA, &Cast::setSubjectB,
                           &Cast::setSubjectC, &Cast::setSubjectD };
    int slot=0;
    bool ok=true;
    for(auto setter : setters) {
        ++slot;
        (cast.*setter)({"alice"});
        for(bool selected : {false,true}) {
            manager.mSelected=static_cast<Slot>(selected ? slot : 0);
            for(int i=0;i<5;++i) {
                manager.mBlobs[i].mEnabled=true;
                manager.mBlobs[i].mMasterEV=i+.25f;
                manager.mInstances[i].mEmitters=0xffu;
            }
            manager.writeSelectedSettings();
            (cast.*setter)({"alice-runtime"}); // same canonical actor retains power
            ok = ok && manager.enabledMask()==31u && manager.mInstances[slot].mEmitters==0xffu;
            (cast.*setter)(LLUUID::null); // manual clear immediately disarms/shuts down
            ok = ok && manager.enabledMask()==(31u & ~(1u<<slot)) && manager.mInstances[slot].mEmitters==0;
            (cast.*setter)({"bob"});
            manager.tick(2001.);
            ok = ok && !(manager.enabledMask() & (1u<<slot)) && manager.mInstances[slot].mEmitters==0;
            ok = ok && manager.mBlobs[slot].mMasterEV==slot+.25f;
            manager.mBlobs[slot].mEnabled=true;
            if(selected) manager.writeSelectedSettings();
            manager.mInstances[slot].mEmitters=0xffu;
            (cast.*setter)({"alice"}); // direct reassignment also disarms
            ok = ok && !(manager.enabledMask() & (1u<<slot)) && manager.mInstances[slot].mEmitters==0;
        }
    }
    gSavedSettings.enabledChanged={};
    ALCineLightRigManager::testInstance=nullptr;
    return ok;
}
void testSubjectAssignment() {
    assert(assignmentOracle<RigSubjectAssignment::LLDirectorCast>());
    assert(!assignmentOracle<RigSubjectAssignmentArmed::LLDirectorCast>());

    auto operationOracle=[](auto& cast, auto assign, auto bulk) {
        ALCineLightRigManager manager;
        ALCineLightRigManager::testInstance=&manager;
        assign(cast,1,LLUUID{"you"});
        manager.mBlobs[1].mEnabled=true;
        manager.mInstances[1].mEmitters=0xffu;
        assign(cast,1,LLUUID{"you"});
        bool ok=manager.mBlobs[1].mEnabled && manager.mInstances[1].mEmitters==0xffu;
        bulk(cast, std::vector<LLUUID>{{"you"},{"alice"}});
        ok=ok && manager.mBlobs[1].mEnabled;
        manager.mBlobs[2].mEnabled=true;
        bulk(cast, std::vector<LLUUID>{{"you"},{"alice-runtime"}});
        ok=ok && manager.mBlobs[1].mEnabled && manager.mBlobs[2].mEnabled;
        assign(cast,2,LLUUID{"bob"});
        ok=ok && !manager.mBlobs[2].mEnabled && manager.mBlobs[1].mEnabled;
        ALCineLightRigManager::testInstance=nullptr;
        return ok;
    };
    RigSubjectAssignment::LLDirectorCast good;
    RigSubjectAssignmentBad::LLDirectorCast bad;
    RigSubjectAssignment::LLDirectorCast drag;
    assert(operationOracle(drag, RigSubjectAssignment::assign_subject, RigSubjectAssignment::dropAssign));
    RigSubjectAssignmentBad::LLDirectorCast badDrag;
    assert(!operationOracle(badDrag, RigSubjectAssignmentBad::assign_subject, RigSubjectAssignmentBad::dropAssign));
    assert(operationOracle(good, RigSubjectAssignment::assign_subject, RigSubjectAssignment::bulkAssign));
    assert(!operationOracle(bad, RigSubjectAssignmentBad::assign_subject, RigSubjectAssignmentBad::bulkAssign));
    std::cout<<"PASS: production single/bulk/multi-drop reselect You keeps A on, Bob over Alice retires B; premature retirement negative rejected\n";
    std::cout<<"PASS: production A-D manual clear/reassignment shuts down selected/unselected rigs, retains edits and other enables; same-actor unchanged; planted armed-slot negative rejected\n";
}
''')
    return '\n'.join(lines)
