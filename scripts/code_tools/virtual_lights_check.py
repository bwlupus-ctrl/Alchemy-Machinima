"""Virtual lights: production contracts plus actual TUT model mutation tests.

A virtual light is a session-only, client-only light "prim" (kind LOCAL_OBJECT_VIRTUAL_LIGHT) that to the
renderer is an ordinary WORLD light. This gate pins the contracts that keep it that way:

  * never sent to the simulator, never persisted, never in the live output (marker: guide composite only);
  * no pipeline light-admission branch and never actor-only;
  * the eight-light cap, region change / logout removal, login recovery does not restore it;
  * Volumetric, VCam, Practicals and Track reach it; Director scenes save and restore it; every edit is undoable.

Contract mutations are applied to an in-memory source bundle, never the checkout. Every planted negative must
fail EXACTLY its own named assertion (a different or additional assertion tripping does not count). The model
negatives compile the real header and the real TUT natively and must fail the TUT assertion of the same name.
All generated products live below this checkout's .tmp directory on I:.
"""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
NEWVIEW = 'indra/newview/'
PATHS = {
    'model': NEWVIEW + 'alvirtuallightsmodel.h',
    'rth': NEWVIEW + 'alvirtuallights.h',
    'rt': NEWVIEW + 'alvirtuallights.cpp',
    'place': NEWVIEW + 'altoolvirtuallightplace.cpp',
    'edit': NEWVIEW + 'altoolvirtuallightedit.cpp',
    'ui': NEWVIEW + 'alpanelpracticals.cpp',
    'agent': NEWVIEW + 'llagent.cpp',
    'app': NEWVIEW + 'llappviewer.cpp',
    'dir': NEWVIEW + 'llfloaterdirector.cpp',
    'dirh': NEWVIEW + 'llfloaterdirector.h',
    'pipe': NEWVIEW + 'pipeline.cpp',
    'tracker': NEWVIEW + 'alprojectortracker.cpp',
    'obj': NEWVIEW + 'llviewerobject.h',
    'rmm': NEWVIEW + 'llreflectionmapmanager.cpp',
    'env': NEWVIEW + 'alenvintensity.cpp',
    'cast': NEWVIEW + 'lldirectorcast.cpp',
    'propmover': NEWVIEW + 'alpanelpropmover.cpp',
    'pathmover': NEWVIEW + 'alobjectpathmover.cpp',
    'select': NEWVIEW + 'llselectmgr.cpp',
    'cmake': NEWVIEW + 'CMakeLists.txt',
    'settings': NEWVIEW + 'app_settings/settings.xml',
    'xui': NEWVIEW + 'skins/default/xui/en/panel_dpage_light_practicals.xml',
    'mock': 'scripts/code_tools/director_kit/director_mockup_data.json',
    'pages': 'scripts/code_tools/director_kit/kit_pages.json',
    'tests': NEWVIEW + 'tests/alvirtuallightsmodel_test.cpp',
}
CARD_WIDGETS = [
    'vl_status', 'vl_place', 'vl_edit', 'vl_list', 'vl_delete', 'vl_on', 'vl_type', 'vl_color', 'vl_intensity',
    'vl_radius', 'vl_falloff', 'vl_fov', 'vl_focus', 'vl_ambiance', 'vl_x', 'vl_y', 'vl_z', 'vl_yaw',
    'vl_pitch', 'vl_aim_pick', 'vl_vcam',
]


def load(root=ROOT):
    root = Path(root)
    return {key: (root / path).read_text(encoding='utf8') for key, path in PATHS.items()}


def function(source, signature):
    """The text of one function: from its signature to the matching closing brace."""
    start = source.index(signature)
    brace = source.index('\n{', start) + 1
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def virtual_ui_part(ui):
    """Only the Virtual lights card code of alpanelpracticals.cpp (the rest legitimately uses settings)."""
    return ui[ui.index('// ALVirtualLightsUI: the "Virtual lights" card'):]


def contracts(b):
    """Every failed assertion name. A healthy tree returns []."""
    failed = []

    def require(condition, name):
        if not condition and name not in failed:
            failed.append(name)

    rt, ui, pipe = b['rt'], b['ui'], b['pipe']
    create = function(rt, 'bool ALVirtualLights::createObject(')
    destroy = function(rt, 'void ALVirtualLights::destroyEntry(')
    draw = function(rt, 'void ALVirtualLights::drawGuides()')
    wants = function(rt, 'bool ALVirtualLights::wantsGuides()')
    tick = function(rt, 'void ALVirtualLights::tick(')
    place = function(rt, 'LLUUID ALVirtualLights::place(')
    place_global = function(rt, 'LLUUID ALVirtualLights::placeAtGlobal(')
    apply_scene = function(rt, 'void ALVirtualLights::applySceneData(')
    record = function(rt, 'void ALVirtualLights::recordUndo(')
    set_vcam = function(rt, 'bool ALVirtualLights::setVCam(')
    unbind = function(rt, 'void ALVirtualLights::unbindVCam(')
    apply_pr = function(rt, 'void ALVirtualLights::applyPracticals(')
    sync_pr = function(rt, 'void ALVirtualLights::syncPracticals(')

    # --- never sent to the simulator --------------------------------------------------------------
    sim_tokens = ('gMessageSystem', 'LLMessageSystem', 'sendObject', 'ObjectAdd', 'sendRezObject',
                  'sendMultipleUpdate', 'sendBuy', 'sendDelete')
    require('volume->mIsLocalOnly = true;' in create and 'LOCAL_OBJECT_VIRTUAL_LIGHT' in create and
            'volume->mbCanSelect = false;' in create and
            not any(t in b[k] for k in ('rt', 'place', 'edit') for t in sim_tokens) and
            not any(t in virtual_ui_part(ui) for t in sim_tokens),
            'vl_sent_to_sim')

    # --- never persisted ----------------------------------------------------------------------------------
    persist_tokens = ('gSavedSettings', 'LLControlGroup', 'LLSDSerialize', 'LLFile::', 'ofstream', 'gDirUtilp')
    require(not any(t in b[k] for k in ('model', 'rth', 'rt', 'place', 'edit') for t in persist_tokens) and
            not any(t in virtual_ui_part(ui) for t in persist_tokens) and
            'VirtualLight' not in b['settings'],
            'vl_persisted')

    # --- the marker never reaches the live output ---------------------------------------------------------------
    guard = draw[:draw.index('LLGLEnable')]
    guard_terms = ('ALCleanFrame::active()', 'rlvHandbackActive()', 'gSnapshot', 'gCubeSnapshot',
                   'sPrismLensRender', 'sReflectionRender', 'sShadowRender', 'isTagRecordView()',
                   'getUIVisibility()', 'RENDER_DEBUG_FEATURE_UI')
    guides = function(pipe, 'void LLPipeline::renderActorPathGuides(')
    hook = 'if (ALVirtualLights::wantsGuides()) ALVirtualLights::drawGuides();'
    require(all(t in guard for t in guard_terms) and 'ALCleanFrame::active()' in wants and
            pipe.count('ALVirtualLights::drawGuides()') == 1 and pipe.count('ALVirtualLights::wantsGuides()') == 2 and
            0 <= guides.find(hook) < guides.find('ALKeyframePlayer::instance().drawGuides()'),
            'vl_marker_in_frame')

    # --- an ordinary WORLD light: no pipeline admission branch, never actor-only --------------------------------------------
    require('ctorOnly' not in rt and 'ActorOnly' not in rt and
            all('isVirtualLight' not in b[k] for k in ('pipe', 'rmm', 'env')) and
            'ALVirtualLights' not in b['rmm'] and 'ALVirtualLights' not in b['env'],
            'vl_actor_only')

    # --- the cap ----------------------------------------------------------------------------------------------------------------
    require('constexpr S32 kMaxLights = 8;' in b['model'] and
            'if (count >= kMaxLights) return Refusal::AT_CAP;' in b['model'] and
            'placementRefusal(count()' in place and 'placementRefusal(count()' in place_global,
            'vl_cap_exceeded')

    # --- Volumetric: the Practicals toggle reaches it and teardown clears the flag ----------------------------------------------------------
    on_volumetric = function(ui, 'void ALPanelPracticals::onVolumetric()')
    require('resolveTarget()' in on_volumetric and 'toggleVolumetricShaft(target.mId)' in on_volumetric and
            'toggleVolumetricShaft(id, false)' in destroy and
            'kPracticalVolumetric' in apply_pr and 'toggleVolumetricShaft(id, true)' in apply_pr and
            'isVolumetricShaftEnabled(id)' in sync_pr,
            'vl_volumetric_unwired')

    # --- VCam: bind through the page model, unbind explicitly before the object dies -----------------------------------------------------------------
    require('dropOnProjector(' in set_vcam and 'removeProjector(' in unbind and
            'unbindVCam(dying.get());' in destroy and
            destroy.index('unbindVCam(dying.get());') < destroy.index('markDead()') and
            'setIsLight(true)' in set_vcam,
            'vl_vcam_unwired')

    # --- a real region change removes every light, before the agent frame moves -------------------------------------------------------------------------------
    set_region = function(b['agent'], 'void LLAgent::setRegion(')
    clear = 'ALVirtualLights::instance().clearAll("region-clear");'
    require(clear in set_region and
            set_region.index('if (mRegionp != regionp)') < set_region.index(clear) < set_region.index('mAgentOriginGlobal ='),
            'vl_survives_region_change')

    # --- the Practicals page follows the virtual focus, not only the selection ---------------------------------------------------------------------------------------------
    require(ui.count('getPrimaryObject()') == 1 and ui.count('resolveTarget()') >= 9 and
            'lights.volumeFor(lights.focusKey())' in ui,
            'vl_practicals_ignores_virtual')

    # --- Track: a virtual light never enters the real tracker ----------------------------------------------------------------------------------------------------------------
    eligible = function(b['tracker'], 'bool ALProjectorTracker::eligible(')
    track_view = function(ui, 'ALPanelPracticals::TrackView ALPanelPracticals::trackView(')
    configure = function(ui, 'void ALPanelPracticals::configureTrackFor(')
    on_track = ui[ui.index('mTrack->setCommitCallback'):ui.index('mTrackCapture->setCommitCallback')]
    require('!object->isLocalOnly()' in eligible and 'ALProjectorTracker::instance()' not in rt and
            track_view.index('if (target.mVirtual)') < track_view.index('ALProjectorTracker::instance()') and
            configure.index('if (target.mVirtual)') < configure.index('ALProjectorTracker::instance()') and
            on_track.index('if (target.mVirtual) ALVirtualLights::instance().enableTrack') <
            on_track.index('else ALProjectorTracker::instance().enable'),
            'vl_tracker_gets_virtual')

    # --- logout / quit: gone before the volumetric flags and Prism designations are cleared ----------------------------------------------------------------------------------------------
    app = b['app']
    require('ALVirtualLights::instance().clearAll("logout-clear");' in app and
            app.index('ALVirtualLights::instance().clearAll("logout-clear");') < app.index('LLPipeline::clearVolumetricShafts();') and
            'ALVirtualLights::instance().clearAll("shutdown-clear");' in app and
            'ALVirtualLights::instance().tick(dt_raw);' in app,
            'vl_logout_leak')

    # --- scenes: a virtual object id never becomes a Prism projector binding ----------------------------------------------------------------------------------------------------------
    capture = function(b['dir'], 'LLSD LLFloaterDirector::captureSceneData()')
    require('isVirtualObject(item["object_id"].asUUID())) continue;' in capture,
            'vl_scene_stale_binding')

    # --- scenes: saved, restored after Prism, never restored by recovery -------------------------------------------------------------------------------------------------------------------
    load_scene = function(b['dir'], 'bool LLFloaterDirector::loadScene(')
    apply_call = 'ALVirtualLights::instance().applySceneData(scene["virtual_lights"], recovery);'
    require('scene["virtual_lights"] = virtual_lights;' in capture and apply_call in load_scene and
            load_scene.index('LLPrismLens::applySceneData(scene') < load_scene.index(apply_call) and
            load_scene.index('applySceneData(scene["light_rig"])') < load_scene.index(apply_call) and
            'mVirtualLights' in b['dir'] and 'mVirtualLights' in b['dirh'],
            'vl_scene_unrestored')
    require(0 <= apply_scene.find('if (recovery) return;') < apply_scene.index('clearAll('),
            'vl_recovery_restores')

    # --- every edit is one undo step, guarded by the scene epoch -----------------------------------------------------------------------------------------------------------------------------
    edit_tool = b['edit']
    ui_part = virtual_ui_part(ui)
    require('ALVirtualLights::recordUndo(before, "Place virtual light");' in b['place'] and
            'ALVirtualLights::recordUndo(before, "Aim virtual light");' in b['place'] and
            'ALVirtualLights::recordUndo(mBefore, mHandle == 0 ? "Move virtual light" : "Aim virtual light");' in edit_tool and
            'ALVirtualLights::recordUndo(before, "Delete virtual light");' in edit_tool and
            'ALVirtualLights::recordUndo(before, "Delete virtual light");' in ui_part and
            'if (lights.edit(key, change)) notePending(before, label, control);' in ui_part and
            'ALVirtualLights::recordUndo(before, label);' in ui_part and
            'recordCustom(' in record and 'instance().mEpoch != epoch' in record,
            'vl_undo_unrecorded')

    # --- feature-off identity: free when nothing is placed ---------------------------------------------------------------------------------------------------------------------------------------------
    require(0 <= tick.find('if (mEntries.empty()) return;') < tick.index('for (size_t index = 0; index < mEntries.size();)') and
            'sPlaced <= 0' in wants and
            0 <= pipe.index('const bool guide_work =') < pipe.index('ALVirtualLights::wantsGuides() ||') <
            pipe.index('show_heading', pipe.index('const bool guide_work =')),
            'vl_feature_off_cost')

    # --- the kind and the by-kind exclusions -------------------------------------------------------------------------------------------------------------------------------------------------------------
    require('LOCAL_OBJECT_VIRTUAL_LIGHT' in b['obj'] and 'isVirtualLight()' in b['obj'] and
            all('isVirtualLight()' in b[k] for k in ('cast', 'propmover', 'pathmover', 'select')),
            'vl_exclusions_missing')

    # --- the card: every widget the code binds exists on the generated page, is searchable and unlisted in history ------------------------------------------------------------------------------------------
    bound = set(re.findall(r'findChild<\w+>\("(vl_\w+)"\)', ui_part))
    mock = json.loads(b['mock'])
    rows = [row for group in mock['groups'] for page in group['pages'] if page['name'] == 'practicals_tab'
            for card in page['cards'] if card['id'] == 'virtual' for row in card['rows']]
    overrides = json.loads(b['pages'])['p0_pages']['light_group/practicals_tab'].get('row_overrides', {})
    require(bound == set(CARD_WIDGETS) and {r['w'] for r in rows} == set(CARD_WIDGETS) and
            all('virtual light' in r.get('kw', '') for r in rows) and
            all(overrides.get(w, {}).get('no_widget_history') for w in CARD_WIDGETS) and
            all('name="' + w + '"' in b['xui'] for w in CARD_WIDGETS) and
            'cards="virtual" id="virtual"' in b['xui'],
            'vl_card_unwired')

    # --- the build knows the new files --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    require(all(name in b['cmake'] for name in ('alvirtuallights.cpp', 'altoolvirtuallightplace.cpp',
                'altoolvirtuallightedit.cpp', 'alvirtuallightsmodel.h', 'LL_ADD_INTEGRATION_TEST(alvirtuallightsmodel')),
            'vl_not_in_build')
    return failed


def mutate(bundle, key, old, new):
    assert bundle[key].count(old) == 1, ('mutation anchor drift', key, old)
    changed = dict(bundle)
    changed[key] = bundle[key].replace(old, new, 1)
    return changed


def contract_negatives():
    """name -> list of (bundle key, old, new). Each must fail exactly that assertion."""
    return {
        'vl_sent_to_sim': [('rt', 'volume->mIsLocalOnly = true;', 'volume->mIsLocalOnly = false;')],
        'vl_persisted': [('rt', 'sPlaced = count();\n    connectSelection();',
                          'sPlaced = count();\n    gSavedSettings.setBOOL("VirtualLightLast", true);\n    connectSelection();')],
        'vl_marker_in_frame': [('rt', '    if (!wantsGuides() || ALCleanFrame::active() ||', '    if (!wantsGuides() ||')],
        'vl_actor_only': [('pipe', '        static LLCachedControl<bool> show_heading(gSavedSettings, "ActorMoverShowHeading", true);\n        const bool guide_work',
                           '        static LLCachedControl<bool> show_heading(gSavedSettings, "ActorMoverShowHeading", true);\n        if (false) { (void)isVirtualLight(); }\n        const bool guide_work')],
        'vl_cap_exceeded': [('model', 'if (count >= kMaxLights) return Refusal::AT_CAP;', 'if (count > kMaxLights) return Refusal::AT_CAP;')],
        'vl_volumetric_unwired': [('rt', '        LLPipeline::toggleVolumetricShaft(id, false);   // not a manual decline of an auto flag',
                                   '        (void)id;   // not a manual decline of an auto flag')],
        'vl_vcam_unwired': [('rt', '    unbindVCam(dying.get());\n', '')],
        'vl_survives_region_change': [('agent', '        ALVirtualLights::instance().clearAll("region-clear");\n', '')],
        'vl_practicals_ignores_virtual': [('ui', 'if (target.mObject && target.mId.notNull())\n    {\n        edited(target, "Virtual light shadows", mCastShadows,',
                                           'LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();\n    if (target.mObject && target.mId.notNull())\n    {\n        edited(target, "Virtual light shadows", mCastShadows,')],
        'vl_tracker_gets_virtual': [('tracker', '!object->isHUDAttachment() && !object->isLocalOnly() &&',
                                     '!object->isHUDAttachment() &&')],
        'vl_logout_leak': [('app', '    ALVirtualLights::instance().clearAll("logout-clear");\n', '')],
        'vl_scene_stale_binding': [('dir', 'if (ALVirtualLights::instance().isVirtualObject(item["object_id"].asUUID())) continue;',
                                    'if (false) continue;')],
        'vl_scene_unrestored': [('dir', '    ALVirtualLights::instance().applySceneData(scene["virtual_lights"], recovery);\n', '')],
        'vl_recovery_restores': [('rt', '    if (recovery) return;   // L7', '    // L7')],
        'vl_undo_unrecorded': [('place', '        ALVirtualLights::recordUndo(before, "Place virtual light");\n', '')],
        'vl_feature_off_cost': [('rt', '    if (mEntries.empty()) return;   // feature-off: nothing to do\n', '')],
        'vl_exclusions_missing': [('cast', '(candidate->isColorChart() || candidate->isVirtualLight())', '(candidate->isColorChart())')],
        'vl_card_unwired': [('xui', 'name="vl_place"', 'name="vl_placeX"')],
        'vl_not_in_build': [('cmake', '    alvirtuallights.cpp\n', '')],
    }


def run_contracts(root=ROOT):
    bundle = load(root)
    baseline = contracts(bundle)
    assert not baseline, 'production tree violates: ' + ', '.join(baseline)
    print('PASS: production virtual-light contracts hold (%d named assertions)' % len(contract_negatives()), flush=True)
    for name, edits in contract_negatives().items():
        planted = bundle
        for key, old, new in edits:
            planted = mutate(planted, key, old, new)
        failed = contracts(planted)
        assert name in failed, name + ': planted negative unexpectedly passed (failed: %s)' % failed
        assert failed == [name], name + ': a different assertion also tripped: %s' % failed
        print('PASS: rejected planted ' + name + ' at its named assertion', flush=True)


MODEL_NEGATIVES = {
    'vl_cap_exceeded': ('if (count >= kMaxLights) return Refusal::AT_CAP;', 'if (count > kMaxLights) return Refusal::AT_CAP;'),
    'vl_prim_range_radius': ('light.mRadius = clampTo(light.mRadius, kMinRadius, kMaxRadius, kDefaultRadius);',
                             'light.mRadius = clampTo(light.mRadius, kMinRadius, 1.0e9f, kDefaultRadius);'),
    'vl_prim_range_intensity': ('light.mIntensity = clampTo(light.mIntensity, 0.f, kMaxIntensity, kDefaultIntensity);',
                                'light.mIntensity = clampTo(light.mIntensity, 0.f, 1.0e9f, kDefaultIntensity);'),
    'vl_scene_unrestored': ('    vec3FromLLSD(data["pos"], light.mPos);\n', ''),
    'vl_scene_cap_exceeded': ('i < block.size() && static_cast<S32>(lights.size()) < kMaxLights;', 'i < block.size();'),
    'vl_scene_write_cap': ('i < lights.size() && static_cast<S32>(i) < kMaxLights;', 'i < lights.size();'),
    'vl_scene_duplicate_key': ('if (light.mKey.isNull() || containsKey(lights, light.mKey)) light.mKey = mint();',
                               'if (light.mKey.isNull()) light.mKey = mint();'),
    'vl_track_without_subject': ('    if (!light.mTrack.mHasSubject) light.mTrack.mEnabled = false;\n', ''),
}


def native_source(root, header, tests):
    tests = tests.replace('#include "linden_common.h"\n', '')
    tests = tests.replace('#include "../test/lltut.h"\n', '')
    tests = tests.replace('#include "../alvirtuallightsmodel.h"\n', '')
    anchor = '    struct virtual_lights_data {};'
    assert tests.count(anchor) == 1, 'test file drifted'
    tests = tests.replace(anchor, '    test_runner_singleton runner;\n' + anchor)
    count = len(re.findall(r'void virtual_lights_object::test<(\d+)>\(\)', tests))
    assert count >= 9, 'the virtual-light TUT must keep at least nine tests'
    main = ('int main(){for(int n=1;n<=%d;++n){auto result=tut::virtual_lights_tests.run_test(n);'
            'if(result.result!=tut::test_result::ok){std::cerr<<result.message<<"\\n";return 1;}}'
            'std::cout<<"PASS: actual TUT registration and execution of the virtual light model tests 1-%d\\n";return 0;}'
            % (count, count))
    return '\n'.join(['#include "linden_common.h"', '#include "lltut.h"', '#include <iostream>', '#include <limits>',
                      header, tests, main])


def run_model(root=ROOT):
    from native_check_tools import check_directory, compile_run, viewer_native_flags
    root = Path(root)
    header = (root / PATHS['model']).read_text(encoding='utf8')
    tests = (root / PATHS['tests']).read_text(encoding='utf8')
    source = native_source(root, header, tests)
    flags, libraries = viewer_native_flags()
    flags += ['/UNDEBUG', f'/I"{root / "indra/externals/tut"}"']
    directory = check_directory('virtual_lights_model')
    baseline = compile_run(directory, source, 'baseline', flags, libraries)
    (directory / 'baseline.log').write_bytes((baseline.stdout + baseline.stderr).encode('utf8'))
    assert baseline.returncode == 0, baseline.stdout + baseline.stderr
    print(baseline.stdout, end='', flush=True)
    for name, (before, after) in MODEL_NEGATIVES.items():
        assert source.count(before) == 1, (name, 'mutation anchor drift')
        planted = compile_run(directory, source.replace(before, after, 1), name, flags, libraries)
        output = planted.stdout + planted.stderr
        (directory / (name + '.log')).write_bytes(output.encode('utf8'))
        assert planted.returncode != 0, name + ': planted negative unexpectedly passed'
        assert name in output.splitlines(), (name, 'failed a different assertion', output)
        print('PASS: rejected planted ' + name + ' at its named TUT assertion', flush=True)


def main():
    run_contracts()
    run_model()
    print('PASS: virtual_lights_check')


if __name__ == '__main__':
    sys.exit(main())
