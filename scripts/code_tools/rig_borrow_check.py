"""Borrow policy TUT execution and precise UI/no-Subject-write contract, with planted bugs."""
from pathlib import Path
from rig_shadow_presets_check import function


def block(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def valid_contract(source, page, manager):
    import re
    post = function(source, 'bool ALPanelCineLightRig::postBuild()')
    if 'setWhoUnavailable(' not in post or 'return panel->pendingBorrowLabel(id);' not in post:
        return False
    if 'std::string ALPanelCineLightRig::pendingBorrowLabel(' not in source:
        return False
    pending = function(source, 'std::string ALPanelCineLightRig::pendingBorrowLabel(')
    if '"Give " + rigActorName(id, "chip") + " a rig"' not in pending:
        return False
    # The entire rig panel is forbidden from changing camera-driving Subjects.
    if re.search(r'\bsetSubject[ABCD]\s*\(', source + manager):
        return False
    menu = function(source, 'void ALPanelCineLightRig::showBorrowMenu(')
    if not (menu.index('add("replace_rig"') < menu.index('add("share_rig"') < menu.index('add("cancel"')):
        return False
    if 'add("follow_cast"' not in menu or 'add("edit_rig_' not in menu or '"Remove rig"' not in menu:
        return False
    if 'rigs.slotActor(slot) != (expected_was_cast ? rigs.resolveBorrowActor(expected, true) : expected)' not in menu:
        return False
    if menu.count('const LLUUID id = rigs.resolveBorrowActor(captured, was_cast);') != 2 or menu.count('if (id.isNull()) { panel->showRigNotice("Actor unavailable"); return; }') != 2:
        return False
    borrow = function(source, 'bool ALPanelCineLightRig::addAvatarToRigGroup(')
    if 'const bool follow = !mRigKitPage->whoPinned()' not in borrow or 'finishRigAction(static_cast<S32>(slot), follow)' not in borrow: return False
    # Extract the exact master construction branch, not unrelated adapter uses.
    start = page.find('if (mWhoAdapter == "cine_rig_slot")')
    if start < 0: return False
    branch = block(page[start:], 'if (mWhoAdapter == "cine_rig_slot")')
    if 'setControlName(' in branch or 'setCommitCallback(' not in branch: return False
    switch_guard = 'if (mWhoUnavailableTarget.notNull())'
    if switch_guard not in branch: return False
    interception = block(branch, switch_guard)
    if 'activateWhoUnavailable();' not in interception or 'return;' not in interception: return False
    if branch.index(switch_guard) > branch.index('gSavedSettings.setBOOL(mMasterSetting, mMaster->isOn())'): return False
    banner_start = page.index('turn->setCommitCallback(')
    banner_end = page.index('mBanner->addChild(turn);', banner_start)
    banner = page[banner_start:banner_end]
    preflight = 'mWhoUnavailableTarget.notNull() && activateWhoUnavailable()'
    if preflight not in banner or banner.index(preflight) > banner.index('mMaster->writeBoundValue(true)'): return False
    return True


def check_contract(root, source=None):
    source = source or (root/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    page = (root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    assert valid_contract(source, page, manager), 'borrow UI/Subject immutability contract'
    negatives = {
        'Subject write': source + '\nvoid bad(){cast.setSubjectA(id);}',
        'disabled borrowing chip': source.replace('setWhoUnavailable(', 'disabledWhoUnavailable('),
        'disabled cached borrowing label': source.replace('"Give " + rigActorName(id, "chip") + " a rig"', 'std::string()'),
        'Share default': source.replace('add("replace_rig"', 'add("share_rig"', 1).replace('add("share_rig", "Share', 'add("replace_rig", "Share', 1),
        'quick borrow pins': source.replace('finishRigAction(static_cast<S32>(slot), follow)', 'finishRigAction(static_cast<S32>(slot))'),
        'trapped chip': source.replace('add("follow_cast"', 'add("no_follow"'),
        'stale replacement': source.replace('rigs.slotActor(slot) != (expected_was_cast ? rigs.resolveBorrowActor(expected, true) : expected)', 'false'),
        'retired target': source.replace('if (id.isNull()) { panel->showRigNotice("Actor unavailable"); return; }', '', 1),
    }
    for label, bad in negatives.items():
        assert not valid_contract(bad, page, manager), 'planted ' + label + ' escaped'
    bad_pages = [
        page.replace('// Keep the switch unbound:', 'mMaster->setControlName(mMasterSetting, this); // Keep the switch unbound:',1),
        page.replace('                    activateWhoUnavailable();\n                    return;', '                    return;',1),
        page.replace('if (mWhoUnavailableTarget.notNull() && activateWhoUnavailable()) return;', '',1),
    ]
    for bad in bad_pages:
        assert not valid_contract(source, bad, manager), 'planted bound/unguarded On escaped'
    print('PASS: precise borrow chip/On, Replace-first, canonical lifetime checks, follow/edit/remove and no Subject writes; 11 planted controls')


def check_generator_contract(root):
    import copy, json
    import director_kit_gen as gen
    data = json.loads((root/'scripts/code_tools/director_kit/director_mockup_data.json').read_text(encoding='utf8'))
    # Match generate(): VCam has its own expanded source page.
    vcam = json.loads(gen.VCAM_DATA.read_text(encoding='utf8'))
    for group in data['groups']:
        if group['name']=='camera_group':
            group['pages']=[vcam if p['name']=='vcam_tab' else p for p in group['pages']]
    gen.validate_data(data)
    for mutation in ('missing', 'history', 'caption'):
        bad = copy.deepcopy(data)
        page = next(p for g in bad['groups'] for p in g['pages'] if p['name']=='cine_light_rig_tab')
        row = next(r for c in page['cards'] for r in c['rows'] if r['w']=='cine_remove_borrowed_rig')
        if mutation=='missing': row['w']='cine_planted_other_action' # census unchanged
        elif mutation=='history': row['no_widget_history']=False
        else: row['btn']='Planted wrong action'
        try: gen.validate_data(bad)
        except gen.GenError: pass
        else: raise AssertionError('planted removal-row '+mutation+' escaped')
    print('PASS: generator keeps exact searchable Remove action; 3 census-preserving planted controls')


def run_unit(root):
    from native_check_tools import check_directory, compile_run, viewer_native_flags
    header = (root/'indra/newview/alriggroupsubject.h').read_text(encoding='utf8')
    tests = (root/'indra/newview/tests/alriggroupsubject_test.cpp').read_text(encoding='utf8')
    tests = '\n'.join(line for line in tests.splitlines() if not line.startswith('#include'))
    prefix = '#include "linden_common.h"\n#include "lltut.h"\nnamespace tut { test_runner_singleton runner; }\n'
    main = '''
int main(){auto r=tut::rig_group_subject_tests.run_test(1);
 if(r.result!=tut::test_result::ok){std::cerr<<r.message;return 1;}
 std::cout<<"PASS: actual alriggroupsubject TUT borrow selection test\\n";}
'''
    flags, libs = viewer_native_flags()
    flags += ['/UNDEBUG',f'/I"{root/"indra/externals/tut"}"']
    out = check_directory('rig_borrow')
    def run(h, name):
        return compile_run(out, prefix + h + tests + main, name, flags=flags, libraries=libs)
    good = run(header, 'borrow')
    assert good.returncode == 0, good.stdout+good.stderr
    print(good.stdout.strip())
    negatives = {
        'borrow_writes_subject': ('    ALRigBorrowChoice result;', '    const_cast<std::array<LLUUID,4>&>(subjects)[0]=actor;\n    ALRigBorrowChoice result;'),
        'ignores_own_anchor': ('if (anchors[slot] == actor)', 'if (false)'),
        'takes_busy_rig': ('!enabled[slot] && subjects[slot].isNull() && anchors[slot].isNull()', 'subjects[slot].isNull()'),
        'silent_replace': ('    return result;\n}', '    result.mSlot=result.mReplacement; return result;\n}'),
    }
    for label, (old,new) in negatives.items():
        assert old in header
        bad = run(header.replace(old,new,1), label)
        assert bad.returncode != 0, 'planted '+label+' escaped TUT oracle'
        print('PASS: rejected planted '+label)


if __name__ == '__main__':
    root=Path(__file__).resolve().parents[2]
    check_contract(root)
    check_generator_contract(root)
    run_unit(root)
    from rig_borrow_round1 import run
    run(root)
