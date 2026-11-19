"""Virtual lights: production contracts plus actual TUT model mutation tests.

A virtual light is a session-only, client-only light "prim" (kind LOCAL_OBJECT_VIRTUAL_LIGHT) that to the
renderer is an ordinary WORLD light. This gate pins the contracts that keep it that way:

  * never sent to the simulator, never persisted, never in the live output (marker: guide composite only);
  * no pipeline light-admission branch and never actor-only;
  * the eight-light cap, region change / logout removal, login recovery does not restore it;
  * Volumetric, VCam, Practicals and Track reach it; Director scenes save and restore it; every edit is undoable;
  * round 1: recreated lights read their saved values back, the marker needs a live visible Director and dies with
    it, VCam binding settings survive scenes and undo, the async Track picker keeps its target, one edit is one undo
    step, guide drawing restores GL state, clone replacement reaches Track references, a texture can be picked;
  * round 2: undo recreation does not alias the settings it is rebinding, a Track capture undoes to the captured
    offset, clone replacement also reaches PENDING undo snapshots, the texture picker is never retargeted, wheel
    gestures settle, the focus baseline is seeded, guide drawing restores colour and mip state, the edit tool is
    released with the last light; outcome assertions (clean frame draws, final cone state, steps per commit) are
    evaluated from the source, not just pinned.

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
    'hist': NEWVIEW + 'alkithistory.cpp',
    'llrender': 'indra/llrender/llrender.h',
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
    'vl_pitch', 'vl_aim_pick', 'vl_vcam', 'vl_texture', 'vl_texture_name',
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


def clean_frame_would_draw(wants, draw):
    """Outcome: with a clean-frame take active, does ANY layer still let the marker be drawn? Evaluated from the
    source: eligibility (wantsGuides) refuses it, or the draw-time guard does."""
    guard = draw[:draw.index('LLGLEnable')]
    wants_refuses = re.search(r'sPlaced <= 0 \|\| ALCleanFrame::active\(\)\) return false;', wants) is not None
    draw_refuses = 'ALCleanFrame::active() ||' in guard
    return not (wants_refuses or draw_refuses)


def final_op_is_practicals(text):
    """Outcome: the LAST thing a restore does to a light is apply its saved Practicals state (a bind-time light-cone
    auto-enable from setVCam can then never win). False when a VCam bind comes after the last Practicals apply."""
    return 0 <= text.rfind('setVCam(') < text.rfind('applyPracticals(')


def recorded_steps_per_commit(b, suppressed):
    """Outcome: for every generated Practicals (sl_*) row, how many undo steps one commit records. The custom
    light-snapshot step is always one; the kit's own widget step is a second unless the row opts out
    (widget_history="false", or a row without a value) or the suppressor catches it."""
    steps = {}
    for attrs in re.findall(r'<kit_row\s([^>]*)>', b['xui']):
        match = re.search(r'widget="(sl_\w+)"', attrs)
        if not match:
            continue
        kind = re.search(r'kind="(\w+)"', attrs)
        # only value-bearing rows ever record a widget step (a button, readout or custom row carries no value)
        widget_records = (kind.group(1) if kind else '') in ('toggle', 'slider', 'combo', 'color') and             'widget_history="false"' not in attrs
        steps[match.group(1)] = 1 + (1 if widget_records and not suppressed else 0)
    return steps


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
    # The WHOLE guard statement, byte for byte (the same one the keyframe overlay uses): dropping, weakening or
    # reordering any term fails here.
    guard_statement = (
        '    if (!wantsGuides() || ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive() ||\n'
        '        gSnapshot || gCubeSnapshot || LLPipeline::sPrismLensRender || LLPipeline::sReflectionRender ||\n'
        '        LLPipeline::sShadowRender || !gPipeline.isTagRecordView() ||\n'
        '        !gViewerWindow || !gViewerWindow->getUIVisibility() ||\n'
        '        !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;')
    guides = function(pipe, 'void LLPipeline::renderActorPathGuides(')
    hook = 'if (ALVirtualLights::wantsGuides()) ALVirtualLights::drawGuides();'
    require(all(t in guard for t in guard_terms) and guard_statement in draw and
            pipe.count('ALVirtualLights::drawGuides()') == 1 and pipe.count('ALVirtualLights::wantsGuides()') == 2 and
            0 <= guides.find(hook) < guides.find('ALKeyframePlayer::instance().drawGuides()'),
            'vl_marker_in_frame')
    # Eligibility itself (wantsGuides) must refuse a clean-frame take: the exact refusal line, not just a mention.
    require('    if (sPlaced <= 0 || ALCleanFrame::active()) return false;\n' in wants and 'sPlaced <= 0' in wants,
            'vl_marker_eligibility')
    # The marker is a Director editing aid: it needs a LIVE, VISIBLE Director at draw time (edit tool included).
    live = function(rt, 'bool directorLive()')
    require('return owned && directorLive();' in wants and
            'director->getVisible() && !director->isMinimized() && director->isInVisibleChain()' in live and
            'const bool owned = sWantsGuides || (sEditorStamped &&' in wants,
            'vl_marker_after_hide')
    # ... and the two-frame stamp must not leave residual frames: release clears it synchronously.
    release = function(rt, 'void ALVirtualLights::releaseEditor()')
    require('    sEditorStamped = false;\n' in release and 'sWantsGuides = false;' in release and
            'clearTransientTool()' in release and
            'sEditorStamped = true;' in function(rt, 'void ALVirtualLights::noteEditorVisible()'),
            'vl_marker_residual_frames')
    # The edit tool and the guide ownership end on page hide, Director hide / minimise / close and teardown.
    d_close = function(b['dir'], 'void LLFloaterDirector::onClose(')
    d_visible = function(b['dir'], 'void LLFloaterDirector::setVisible(')
    d_min = function(b['dir'], 'void LLFloaterDirector::setMinimized(')
    d_dtor = function(b['dir'], 'LLFloaterDirector::~LLFloaterDirector()')
    on_hide = function(ui, 'void ALVirtualLightsUI::onHide()')
    ui_dtor = function(ui, 'ALVirtualLightsUI::~ALVirtualLightsUI()')
    page_vis = function(ui, 'void ALPanelPracticals::onVisibilityChange(')
    require(all('ALVirtualLights::releaseEditor();' in f for f in (d_close, d_visible, d_min, d_dtor, on_hide, ui_dtor)) and
            'mVirtualUI.onHide();' in page_vis and 'if (!visible)' in page_vis,
            'vl_edit_tool_survives_hide')
    require('ALVirtualLights::releaseEditor();' in function(b['edit'], 'bool ALToolVirtualLightEdit::handleKey(') and
            'key == KEY_ESCAPE' in function(b['edit'], 'bool ALToolVirtualLightEdit::handleKey('),
            'vl_edit_esc_stays')

    # --- an ordinary WORLD light: no pipeline admission branch, never actor-only --------------------------------------------
    actor_sites = [pipe[m.start():pipe.index(')\n', m.start()) + 1] for m in re.finditer(r'actorOnlyBits\(', pipe)]
    require('ctorOnly' not in rt and 'ActorOnly' not in rt and 'ALCineLightRigManager' not in rt and
            all('isVirtualLight' not in b[k] for k in ('pipe', 'rmm', 'env')) and
            all('isVirtual' not in site and 'ALVirtualLights' not in site for site in actor_sites) and
            len(actor_sites) >= 20 and
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
    handlers = [function(ui, sig) for sig in (
        'void ALPanelPracticals::onCastShadows()', 'void ALPanelPracticals::onVolumetric()',
        'void ALPanelPracticals::onCaptureOverride()', 'void ALPanelPracticals::onClearOverride()',
        'void ALPanelPracticals::onGoboChanged(', 'void ALPanelPracticals::onTrackingChanged(')]
    require(ui.count('getPrimaryObject()') == 1 and ui.count('resolveTarget()') >= 9 and
            'lights.volumeFor(lights.focusKey())' in ui and
            all('resolveTarget()' in h and 'LLSelectMgr' not in h and 'getPrimaryObject' not in h for h in handlers) and
            'toggleProjectorCastShadows(target.mId)' in handlers[0] and
            'toggleVolumetricShaft(target.mId)' in handlers[1],
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

    # --- a recreated light is read back through the real volume getters: light mode first, then the values -----------------
    apply_entry = function(rt, 'void ALVirtualLights::applyEntry(')
    verify = function(rt, 'bool ALVirtualLights::verifyApplied(')
    restore_fn = function(rt, 'bool ALVirtualLights::restore(')
    values = ('setLightTextureID', 'setLightSRGBColor', 'setLightIntensity', 'setLightRadius', 'setLightFalloff',
              'setSpotLightParams')
    require(apply_entry.count('setIsLight(') == 1 and
            all(apply_entry.index('volume->setIsLight(true);') < apply_entry.index(t) for t in values),
            'vl_recreated_params_lost')
    require('verifyApplied(fresh, "undo-recreate");' in restore_fn and
            'verified = verifyApplied(entry, "scene-restore") && verified;' in apply_scene and
            all(t in verify for t in ('getIsLight()', 'getLightIntensity()', 'getLightRadius()', 'getLightFalloff()',
                                      'lightApplied(')),
            'vl_recreate_unverified')

    # --- VCam binding settings survive scenes, delete -> undo, and are rebound BEFORE the Practicals state -----------------
    sync_vcam = function(rt, 'void ALVirtualLights::syncVCam(')
    require('vcam_settings_of(' in sync_vcam and
            'setVCam(light.mKey, light.mVCamCapture, &why, light.mVCamSettings);' in apply_scene and
            'setVCam(key, light->mVCamCapture, &why, light->mVCamSettings);' in restore_fn and
            'setProjectorSettings(binding.mHandle, parsed' in set_vcam and
            'out["vcam_settings"] = light.mVCamSettings;' in b['model'] and 'data["vcam_settings"]' in b['model'],
            'vl_vcam_settings_lost')
    require(0 <= apply_scene.index('setVCam(') < apply_scene.index('applyPracticals(') and
            0 <= restore_fn.index('setVCam(') < restore_fn.index('applyPracticals('),
            'vl_vcam_after_practicals')

    # --- the async Track picker keeps the target it was opened for ------------------------------------------------------------
    assign = function(ui, 'void ALPanelPracticals::assignTrackingSubject(')
    choose = ui[ui.index('mTrackChoose->setCommitCallback'):ui.index('mTrackSelected->setCommitCallback')]
    require('trackTargetValid(opened_for' in assign and 'resolveTarget()' not in assign and 'is_virtual_id' in assign and
            'const ALVirtualLightsModel::TrackTargetRef opened_for = trackTargetNow();' in choose and
            '[this, opened_for](const LLUUID& id) { assignTrackingSubject(opened_for, id, true); }' in choose and
            'assignTrackingSubject(mLastId' not in ui,
            'vl_track_picker_current')
    capture_handler = ui[ui.index('mTrackCapture->setCommitCallback'):ui.index('mTrackChoose->setCommitCallback')]
    require('edited(target, "Virtual light track capture", mTrackCapture, [&]()' in capture_handler and
            '++entry->mLight.mTrack.mCaptures;' in function(rt, 'void ALVirtualLights::captureTrack('),
            'vl_track_capture_unrecorded')

    # --- one edit is one undo step; steps end at real gesture boundaries ----------------------------------------------------------
    record_widget = function(b['hist'], 'void ALKitHistory::recordWidget(')
    suppressor = function(ui, 'bool practicalsWidgetOwnedByVirtual(')
    require('if (sWidgetSuppressor && sWidgetSuppressor(c))' in record_widget and
            record_widget.index('sWidgetSuppressor(c)') < record_widget.index('beginGesture(') and
            'ALKitHistory::setWidgetSuppressor(&practicalsWidgetOwnedByVirtual);' in ui and
            'compare(0, 3, "sl_") != 0' in suppressor and 'lights.volumeFor(lights.focusKey()) != nullptr' in suppressor,
            'vl_widget_suppressor_wired')
    begin = function(ui, 'LLSD ALVirtualLightsUI::beginEdit(')
    note = function(ui, 'void ALVirtualLightsUI::notePending(')
    require('control == mPendingControl' in begin and 'GestureKind::SLIDER && sliderHeld(control)' in begin and
            'if (gestureKind(control) == GestureKind::DISCRETE) finishPending(true);' in note and
            'edited(target, "Virtual light gobo", source,' in ui and 'edited(target, "Virtual light track", source,' in ui,
            'vl_undo_merges_gestures')
    require('finishPending' not in ui_dtor and 'recordUndo' not in ui_dtor and 'mPendingBefore = LLSD();' in ui_dtor and
            'finishPending(true);' in on_hide,
            'vl_destructor_records_undo')

    # --- clone replacement reaches virtual Track references, live and inside undo snapshots ----------------------------------------------
    cast_rewrite = function(b['cast'], 'bool LLDirectorCast::rewriteActorReferences(')
    require('if (ALVirtualLights::anyPlaced()) ALVirtualLights::instance().rewriteActorReferences(old_id, new_id);' in cast_rewrite and
            'rewriteActorInLight(entry.mLight, old_id, new_id)' in function(rt, 'void ALVirtualLights::rewriteActorReferences('),
            'vl_clone_replace_unwired')
    require('rewriteActorInSnapshot(edit->first, old_id, new_id);' in record and
            'rewriteActorInSnapshot(edit->second, old_id, new_id);' in record,
            'vl_undo_actor_rewrite')

    # --- guide drawing restores the GL state it changed -----------------------------------------------------------------------------------
    require(0 <= draw.index('gGL.flush();') < draw.index('gUIProgram.bind();') and
            'LLGLSLShader* const previous_shader = LLGLSLShader::sCurBoundShaderPtr;' in draw and
            'const U32 previous_texture = unit0->getCurrTexture();' in draw and
            0 <= draw.rindex('gGL.flush();') < draw.find('previous_shader->bind(); else LLGLSLShader::unbind();') and
            'unit0->bindManual(previous_type, previous_texture, previous_mips);' in draw and
            draw.find('previous_shader->bind(); else LLGLSLShader::unbind();') >= 0 and
            (draw.find('gGL.color4ubv(previous_color.mV);') < 0 or
             draw.find('previous_shader->bind(); else LLGLSLShader::unbind();') <
             draw.find('gGL.color4ubv(previous_color.mV);')),
            'vl_gl_state_unrestored')

    # --- the focus ends only when the primary selection CHANGES to a new non-avatar object ---------------------------------------------------
    connect = function(rt, 'void ALVirtualLights::connectSelection()')
    require('const bool changed = primary != mLastPrimaryId;' in connect and
            'if (!changed || mFocusKey.isNull()) return;' in connect,
            'vl_focus_clears_too_often')

    # --- a projector texture can be picked through the standard picker and is one undo step ----------------------------------------------
    pick = function(ui, 'void ALVirtualLightsUI::onTexturePick()')
    picked = function(ui, 'void ALVirtualLightsUI::onTexturePicked()')
    require('showPicker(true)' in pick and 'LLTextureCtrl::Params picker;' in ui_part and
            'editKey(mTextureKey, "Virtual light projector texture", mTexture,' in picked and
            '[id](ALVirtualLights::Light& light) { light.mTexture = id; });' in picked,
            'vl_texture_unwired')

    # === round 2 ===================================================================================================
    # Undo recreation rebinds with settings that live in the very light setVCam clears: they are taken BY VALUE.
    require('\n                              LLSD settings)' in set_vcam and 'const LLSD& settings' not in set_vcam and
            'std::string* reason, LLSD settings = LLSD());' in b['rth'] and 'const LLSD& settings' not in b['rth'],
            'vl_vcam_settings_aliased')

    # A Track capture undoes to the captured Follow offset: it rides in the snapshot and is put back with its state.
    capture_fn = function(rt, 'LLSD ALVirtualLights::capture()')
    restore_offset = function(rt, 'void ALVirtualLights::restoreOffset(')
    capture_track = function(rt, 'void ALVirtualLights::captureTrack(')
    require('trackOffsetToLLSD(' in capture_fn and 'item["track_offset"]' in capture_fn and
            'restoreOffset(*existing, offset_of(light.mKey));' in restore_fn and
            'restoreOffset(fresh, offset_of(light.mKey));' in restore_fn and
            'existing->mOffsetCaptured = false;' not in restore_fn and
            'trackOffsetFromLLSD(' in restore_offset and 'entry.mOffsetCaptured = true;' in restore_offset and
            'entry.mOffsetCaptured = false;' in restore_offset and
            'captureOffsetNow(*entry)' in capture_track and
            'bool trackOffsetFromLLSD(' in b['model'],
            'vl_track_offset_unrestored')

    # Clone replacement rewrites the snapshots that are not in the history yet (pending edit, drag in flight).
    rewrite_live = function(rt, 'void ALVirtualLights::rewriteActorReferences(')
    rewrite_pending_ui = function(ui, 'void ALVirtualLightsUI::rewritePendingAll(')
    rewrite_pending_tool = function(b['edit'], 'void ALToolVirtualLightEdit::rewritePending(')
    require('ALVirtualLightsUI::rewritePendingAll(old_id, new_id);' in rewrite_live and
            'ALToolVirtualLightEdit::getInstance()->rewritePending(old_id, new_id);' in rewrite_live and
            rewrite_live.index('rewritePendingAll(') < rewrite_live.index('for (Entry& entry : mEntries)') and
            'rewriteActorInSnapshot(editor->mPendingBefore, old_id, new_id);' in rewrite_pending_ui and
            'rewriteActorInSnapshot(mBefore, old_id, new_id);' in rewrite_pending_tool,
            'vl_pending_actor_rewrite')

    # The texture picker is never retargeted: a fresh one per open, Select/Cancel handled explicitly, no local / bake.
    create_picker = function(ui, 'void ALVirtualLightsUI::createTexturePicker()')
    require('createTexturePicker();' in pick and pick.index('createTexturePicker();') < pick.index('showPicker(true)') and
            'delete mTexturePicker;' in create_picker and 'setAllowLocalTexture(false);' in create_picker and
            'setBakeTextureEnabled(false);' in create_picker and 'no_commit_on_selection(true);' in create_picker and
            'setOnSelectCallback(' in create_picker and 'setOnCancelCallback(' in create_picker and
            'mTextureKey.isNull()' in picked and
            'void ALVirtualLightsUI::onTexturePickCancelled()' in ui and 'mTextureKey.setNull();' in ui_part,
            'vl_texture_retarget')

    # Wheel gestures settle: every slider the card or page edits through reports mouse-up / wheel settle.
    gesture_end = function(ui, 'void ALVirtualLightsUI::gestureEnd(')
    post_build = function(ui, 'bool ALPanelPracticals::postBuild()')
    require(ui.count('setSliderMouseUpCallback(') == 2 and
            'ctrl->setSliderMouseUpCallback([this, ctrl](LLUICtrl*, const LLSD&) { gestureEnd(ctrl); });' in ui_part and
            'slider->setSliderMouseUpCallback([this, slider](LLUICtrl*, const LLSD&) { mVirtualUI.gestureEnd(slider); });' in post_build and
            'control == mPendingControl' in gesture_end and 'finishPending(true);' in gesture_end,
            'vl_wheel_settle_unwired')

    # The focus baseline is the CURRENT primary selection, seeded where focus is claimed and where it is connected.
    seed = function(rt, 'void ALVirtualLights::seedPrimary()')
    set_focus = function(rt, 'void ALVirtualLights::setFocus(')
    require('getPrimaryObject()' in seed and 'mLastPrimaryId = ' in seed and
            'seedPrimary();' in connect and 'seedPrimary();' in set_focus and
            connect.index('seedPrimary();') < connect.index('mUpdateSignal.connect') and
            'seedPrimary();' in place,
            'vl_focus_unseeded')

    # Guide drawing puts back the incoming colour, the mip state and the zero-binding case, not just a binding.
    require('const LLColor4U previous_color = gGL.getCurrentColor();' in draw and
            'const bool previous_mips = unit0->hasMipMaps();' in draw and
            'unit0->bindManual(previous_type, previous_texture, previous_mips);' in draw and
            'else unit0->unbind(previous_type);' in draw and
            'unit0->setHasMipMaps(previous_mips);' in draw and
            draw.rindex('gGL.flush();') < draw.find('gGL.color4ubv(previous_color.mV);') and
            'LLColor4U getCurrentColor()' in b['llrender'],
            'vl_gl_colour_mips_unrestored')

    # The zero-binding restore never hands TT_NONE to unbind() (it indexes past the texture-type table): the disabled
    # state is put back with disable(), and only real targets reach unbind().
    require('else if (previous_type == LLTexUnit::TT_NONE) unit0->disable();' in draw and
            draw.index('previous_type == LLTexUnit::TT_NONE') < draw.index('else unit0->unbind(previous_type);'),
            'vl_gl_none_unbind')

    # Restoring Practicals applies the shaft override FIRST (it force-enables the shaft) and the saved volumetric
    # flag LAST, so a saved "Light cone off" survives save/load and undo/redo (both reach applyPracticals).
    require('setVolumetricShaftOverride(' in apply_pr and 'toggleVolumetricShaft(id, true)' in apply_pr and
            apply_pr.index('setVolumetricShaftOverride(') < apply_pr.index('toggleVolumetricShaft(id, true)') and
            apply_pr.index('clearVolumetricShaftOverride(') < apply_pr.index('toggleVolumetricShaft(id, true)'),
            'vl_shaft_flag_overwritten')

    # The edit tool is released with the last light (every way a light can go) and never swallows a world click
    # when there is nothing to pick.
    release_empty = function(rt, 'void ALVirtualLights::releaseEditorIfEmpty()')
    remove_fn = function(rt, 'bool ALVirtualLights::remove(')
    clear_fn = function(rt, 'void ALVirtualLights::clearAll(')
    mouse_down = function(b['edit'], 'bool ALToolVirtualLightEdit::handleMouseDown(')
    require('if (sPlaced > 0) return;' in release_empty and 'dropGesture()' in release_empty and
            'releaseEditor();' in release_empty and
            'releaseEditorIfEmpty();' in remove_fn and 'releaseEditorIfEmpty();' in clear_fn and
            'releaseEditorIfEmpty();' in restore_fn and 'if (lost_any) releaseEditorIfEmpty();' in tick and
            'if (picks.empty()) return false;' in mouse_down,
            'vl_edit_stuck_when_empty')

    # --- outcome assertions: evaluated from the source, so a plant has to produce the wrong BEHAVIOUR ------------
    require(not clean_frame_would_draw(wants, draw), 'vl_clean_frame_draws')
    require(final_op_is_practicals(apply_scene) and final_op_is_practicals(restore_fn), 'vl_vcam_final_state')
    suppressed = ('if (sWidgetSuppressor && sWidgetSuppressor(c))' in record_widget and
                  record_widget.index('sWidgetSuppressor(c)') < record_widget.index('beginGesture(') and
                  'ALKitHistory::setWidgetSuppressor(&practicalsWidgetOwnedByVirtual);' in ui and
                  'compare(0, 3, "sl_") != 0' in suppressor and
                  'lights.volumeFor(lights.focusKey()) != nullptr' in suppressor)
    steps = recorded_steps_per_commit(b, suppressed)
    require(len(steps) >= 20 and all(count == 1 for count in steps.values()), 'vl_double_undo_step')
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
        'vl_persisted': [('rt', 'sPlaced = count();\n    connectSelection();\n    seedPrimary();\n    mFocusKey = key;',
                          'sPlaced = count();\n    gSavedSettings.setBOOL("VirtualLightLast", true);\n    connectSelection();\n    seedPrimary();\n    mFocusKey = key;')],
        'vl_marker_in_frame': [('rt', '    if (!wantsGuides() || ALCleanFrame::active() ||', '    if (!wantsGuides() ||')],
        # eligibility with a FLIPPED clean-frame test: the marker would be drawn exactly during a clean-frame take
        'vl_marker_eligibility': [('rt', '    if (sPlaced <= 0 || ALCleanFrame::active()) return false;',
                                   '    if (sPlaced <= 0 || !ALCleanFrame::active()) return false;')],
        # Director hidden + edit tool active would still draw the marker
        'vl_marker_after_hide': [('rt', '    return owned && directorLive();', '    return owned;')],
        # the two-frame stamp survives a hide: residual marker frames
        'vl_marker_residual_frames': [('rt', '    sWantsGuides = false;\n    sEditorStamped = false;\n',
                                       '    sWantsGuides = false;\n')],
        'vl_edit_tool_survives_hide': [('dir', '        ALDirectorTagTargets::instance().withdraw(this);    // hidden without close or draw: no lingering highlight\n        ALVirtualLights::releaseEditor();\n',
                                        '        ALDirectorTagTargets::instance().withdraw(this);    // hidden without close or draw: no lingering highlight\n')],
        'vl_edit_esc_stays': [('edit', '        cancelGesture();\n        ALVirtualLights::releaseEditor();\n        return true;\n    }\n    if ((mask & MASK_CONTROL)',
                               '        cancelGesture();\n        return true;\n    }\n    if ((mask & MASK_CONTROL)')],
        # a REAL actor-only classification in a pipeline light site: the virtual light is skipped like a masked rig light
        'vl_actor_only': [('pipe', '            if (ALCineLightRigManager::anyActorOnly() &&\n                ALCineLightRigManager::instance().actorOnlyBits(light->getID()))\n            {\n                continue;\n            }\n            if (light->isAttachment())',
                           '            if (ALCineLightRigManager::anyActorOnly() &&\n                (ALCineLightRigManager::instance().actorOnlyBits(light->getID()) || light->isVirtualLight()))\n            {\n                continue;\n            }\n            if (light->isAttachment())')],
        'vl_cap_exceeded': [('model', 'if (count >= kMaxLights) return Refusal::AT_CAP;', 'if (count > kMaxLights) return Refusal::AT_CAP;')],
        'vl_volumetric_unwired': [('rt', '        LLPipeline::toggleVolumetricShaft(id, false);   // not a manual decline of an auto flag',
                                   '        (void)id;   // not a manual decline of an auto flag')],
        'vl_vcam_unwired': [('rt', '    unbindVCam(dying.get());\n', '')],
        'vl_survives_region_change': [('agent', '        ALVirtualLights::instance().clearAll("region-clear");\n', '')],
        # a real handler takes its WRITE target from the raw selection (the focused virtual light is ignored)
        'vl_practicals_ignores_virtual': [('ui', '[&]() { LLPipeline::toggleProjectorCastShadows(target.mId); });',
                                           '[&]() { LLPipeline::toggleProjectorCastShadows(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()->getID()); });')],
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
        # light mode switched on AFTER the values: the volume drops them (recreated Off / non-default lights read defaults)
        'vl_recreated_params_lost': [('rt', '    volume->setIsLight(true);\n    const LLUUID texture', '    const LLUUID texture'),
                                     ('rt', '    if (light.mSpot) volume->setSpotLightParams(LLVector3(light.mFov, light.mFocus, light.mAmbiance));\n',
                                      '    if (light.mSpot) volume->setSpotLightParams(LLVector3(light.mFov, light.mFocus, light.mAmbiance));\n    volume->setIsLight(true);\n')],
        'vl_recreate_unverified': [('rt', '        verified = verifyApplied(entry, "scene-restore") && verified;\n', '        verified = true;\n')],
        # the rebind forgets the authored binding settings (defaults come back)
        'vl_vcam_settings_lost': [('rt', 'setVCam(light.mKey, light.mVCamCapture, &why, light.mVCamSettings);',
                                   'setVCam(light.mKey, light.mVCamCapture, &why);')],
        # an extra early Practicals apply before the rebind: the order pin trips (the outcome plants are vl_vcam_final_state)
        'vl_vcam_after_practicals': [('rt', '        mEntries.push_back(std::move(entry));\n        ++created;\n        sPlaced = count();\n',
                                      '        mEntries.push_back(std::move(entry));\n        applyPracticals(mEntries.back());\n        ++created;\n        sPlaced = count();\n')],
        # the picker resolves the CURRENT target at completion (the real bug): a switch between backends edits the wrong one
        'vl_track_picker_current': [('ui', '        const ALVirtualLightsModel::TrackTargetRef opened_for = trackTargetNow();\n        ALAvatarSubjectPicker::choose(anchor, this, mTrackMenu, true,\n            [this, opened_for](const LLUUID& id) { assignTrackingSubject(opened_for, id, true); });',
                                     '        ALAvatarSubjectPicker::choose(anchor, this, mTrackMenu, true,\n            [this](const LLUUID& id) { assignTrackingSubject(trackTargetNow(), id, true); });')],
        'vl_track_capture_unrecorded': [('ui', '        edited(target, "Virtual light track capture", mTrackCapture, [&]()\n        {\n            if (target.mVirtual) ALVirtualLights::instance().captureTrack(target.mKey);\n            else ALProjectorTracker::instance().capture(target.mId);\n        });',
                                         '        if (target.mVirtual) ALVirtualLights::instance().captureTrack(target.mKey);\n        else ALProjectorTracker::instance().capture(target.mId);')],
        # the suppressor is gone: the pin that watches it trips (the outcome needs a row that records a widget step too)
        'vl_widget_suppressor_wired': [('hist', '    if (sWidgetSuppressor && sWidgetSuppressor(c))\n        return;\n', '')],
        # separate edits of different controls merge into one step
        'vl_undo_merges_gestures': [('ui', 'const bool same = control && control == mPendingControl && lights.sceneEpoch() == mPendingEpoch;',
                                     'const bool same = control && lights.sceneEpoch() == mPendingEpoch;')],
        # teardown records an undo step again
        'vl_destructor_records_undo': [('ui', '    mPendingBefore = LLSD();\n    mPendingControl = nullptr;\n    ALVirtualLights::releaseEditor();\n    sVirtualEditors.erase(this);',
                                        '    finishPending(true);\n    ALVirtualLights::releaseEditor();\n    sVirtualEditors.erase(this);')],
        'vl_clone_replace_unwired': [('cast', '    if (ALVirtualLights::anyPlaced()) ALVirtualLights::instance().rewriteActorReferences(old_id, new_id);\n', '')],
        'vl_undo_actor_rewrite': [('rt', '            rewriteActorInSnapshot(edit->second, old_id, new_id);\n', '')],
        # shader / texture / colour left changed after the guide draw
        'vl_gl_state_unrestored': [('rt', '    if (previous_shader) previous_shader->bind(); else LLGLSLShader::unbind();\n', '')],
        'vl_gl_none_unbind': [('rt', '    else if (previous_type == LLTexUnit::TT_NONE) unit0->disable();\n', '')],
        # the volumetric flag restored BEFORE the override: the override re-enables a saved "Light cone off"
        'vl_shaft_flag_overwritten': [('rt', '    if (volumetric != LLPipeline::isVolumetricShaftEnabled(id)) LLPipeline::toggleVolumetricShaft(id, true);\n', ''),
                                      ('rt', '    if (p[kPracticalGobo].isMap()) LLPipeline::setGoboOverride', '    if (volumetric != LLPipeline::isVolumetricShaftEnabled(id)) LLPipeline::toggleVolumetricShaft(id, true);\n    if (p[kPracticalGobo].isMap()) LLPipeline::setGoboOverride')],
        'vl_focus_clears_too_often': [('rt', '        if (!changed || mFocusKey.isNull()) return;', '        if (mFocusKey.isNull()) return;')],
        'vl_texture_unwired': [('ui', '[id](ALVirtualLights::Light& light) { light.mTexture = id; });',
                                '[id](ALVirtualLights::Light& light) { (void)id; (void)light; });')],
        # ---- round 2 -------------------------------------------------------------------------------------------------
        # the settings parameter is a reference to the member setVCam clears: delete -> undo binds defaults
        'vl_vcam_settings_aliased': [('rt', 'std::string* reason,\n                              LLSD settings)',
                                      'std::string* reason,\n                              const LLSD& settings)'),
                                     ('rth', 'std::string* reason, LLSD settings = LLSD());',
                                      'std::string* reason, const LLSD& settings = LLSD());')],
        # undo recreates the entry but forgets the captured offset: Follow measures a new one against the moved subject
        'vl_track_offset_unrestored': [('rt', '            restoreOffset(*existing, offset_of(light.mKey));\n',
                                        '            existing->mOffsetCaptured = false;\n')],
        # a Practicals edit in flight keeps the retired actor in its "before" snapshot
        'vl_pending_actor_rewrite@ui': [('ui', '        ALVirtualLightsModel::rewriteActorInSnapshot(editor->mPendingBefore, old_id, new_id);\n',
                                         '        (void)editor;\n')],
        # an edit-tool drag in flight keeps it too
        'vl_pending_actor_rewrite@tool': [('edit', '    ALVirtualLightsModel::rewriteActorInSnapshot(mBefore, old_id, new_id);\n', '')],
        # the picker is reused and retargeted: Cancel writes the first light's texture onto the second
        'vl_texture_retarget': [('ui', '    createTexturePicker();\n    if (!mTexturePicker) return;\n    mTextureKey = light->mKey;',
                                 '    if (!mTexturePicker) return;\n    mTextureKey = light->mKey;')],
        'vl_texture_retarget@local': [('ui', '    mTexturePicker->setAllowLocalTexture(false);\n', '')],
        # no mouse-up consumer on the card's sliders: a wheel gesture never settles and splits into many steps
        'vl_wheel_settle_unwired': [('ui', '        ctrl->setSliderMouseUpCallback([this, ctrl](LLUICtrl*, const LLSD&) { gestureEnd(ctrl); });\n', '')],
        'vl_focus_unseeded@setfocus': [('rt', '    // An already-selected real prim is the baseline, not a "change": the next refresh must not clear this focus.\n    seedPrimary();\n', '')],
        'vl_focus_unseeded@connect': [('rt', '    mSelectionConnected = true;\n    seedPrimary();\n', '    mSelectionConnected = true;\n')],
        # the incoming colour is no longer put back / the mip state is dropped
        'vl_gl_colour_mips_unrestored': [('rt', '    gGL.color4ubv(previous_color.mV);\n', '')],
        'vl_gl_colour_mips_unrestored@mips': [('rt', '    unit0->setHasMipMaps(previous_mips);\n', '')],
        # the last light goes and the edit tool stays armed (every world click swallowed)
        'vl_edit_stuck_when_empty': [('rt', '        releaseEditorIfEmpty();\n        return true;\n    }\n    return false;\n}', '        return true;\n    }\n    return false;\n}')],
        'vl_edit_stuck_when_empty@click': [('edit', '    if (picks.empty()) return false;\n', '')],
        # ---- outcome plants: the planted tree must DO the wrong thing --------------------------------------------------
        # a clean-frame take would really draw: the draw guard AND eligibility both lose the clean-frame refusal
        # (the two text pins that watch those lines necessarily trip with it)
        'vl_clean_frame_draws': {
            'edits': [('rt', '    if (!wantsGuides() || ALCleanFrame::active() ||', '    if (!wantsGuides() ||'),
                      ('rt', '    if (sPlaced <= 0 || ALCleanFrame::active()) return false;', '    if (sPlaced <= 0) return false;')],
            'also': ('vl_marker_in_frame', 'vl_marker_eligibility')},
        # the final cone state is wrong: the VCam rebind (bind-time light-cone auto-enable) runs AFTER the saved
        # Practicals state in a scene restore ...
        'vl_vcam_final_state': {
            'edits': [('rt', '''        // VCam FIRST (Prism was applied before the lights), then the saved Practicals state on top.
        if (light.mVCamCapture.notNull())
        {
            std::string why;
            setVCam(light.mKey, light.mVCamCapture, &why, light.mVCamSettings);
        }
        if (Entry* placed = findEntry(light.mKey))
        {
            placed->mLight.mPracticals = light.mPracticals;
            applyPracticals(*placed);
        }
''', '''        if (Entry* placed = findEntry(light.mKey))
        {
            placed->mLight.mPracticals = light.mPracticals;
            applyPracticals(*placed);
        }
        if (light.mVCamCapture.notNull())
        {
            std::string why;
            setVCam(light.mKey, light.mVCamCapture, &why, light.mVCamSettings);
        }
''')],
            'also': ('vl_vcam_after_practicals',)},
        # ... and in a delete -> undo restore
        'vl_vcam_final_state@undo': {
            'edits': [('rt', '''    for (const LLUUID& key : to_bind)
    {
        std::string why;
        const Light* light = find(key);
        if (light) setVCam(key, light->mVCamCapture, &why, light->mVCamSettings);
    }
    for (const Light& light : wanted)
        if (Entry* entry = findEntry(light.mKey))
        {
            entry->mLight.mPracticals = light.mPracticals;
            applyPracticals(*entry);
        }
''', '''    for (const Light& light : wanted)
        if (Entry* entry = findEntry(light.mKey))
        {
            entry->mLight.mPracticals = light.mPracticals;
            applyPracticals(*entry);
        }
    for (const LLUUID& key : to_bind)
    {
        std::string why;
        const Light* light = find(key);
        if (light) setVCam(key, light->mVCamCapture, &why, light->mVCamSettings);
    }
''')],
            'also': ('vl_vcam_after_practicals',)},
        # a Practicals commit really records two undo steps: one value row loses widget_history="false" AND the
        # suppressor is gone (the pin that watches the suppressor trips with it)
        'vl_double_undo_step': {
            'edits': [('hist', '    if (sWidgetSuppressor && sWidgetSuppressor(c))\n        return;\n', ''),
                      ('xui', 'widget="sl_gobo_speed" widget_history="false"', 'widget="sl_gobo_speed"')],
            'also': ('vl_widget_suppressor_wired',)},
    }


def run_contracts(root=ROOT):
    bundle = load(root)
    baseline = contracts(bundle)
    assert not baseline, 'production tree violates: ' + ', '.join(baseline)
    print('PASS: production virtual-light contracts hold (%d planted negatives)' % len(contract_negatives()), flush=True)
    for label, plant in contract_negatives().items():
        # A key may carry '@variant' (several plants of one assertion). An outcome plant is a dict that names the text
        # pins which necessarily trip with it (the lines it has to change); nothing else may trip.
        name = label.split('@')[0]
        edits, also = (plant['edits'], plant['also']) if isinstance(plant, dict) else (plant, ())
        planted = bundle
        for key, old, new in edits:
            planted = mutate(planted, key, old, new)
        failed = contracts(planted)
        assert name in failed, label + ': planted negative unexpectedly passed (failed: %s)' % failed
        assert sorted(failed) == sorted((name,) + tuple(also)), label + ': a different assertion also tripped: %s' % failed
        print('PASS: rejected planted ' + label + ' at its named assertion' +
              (' (with its pins: ' + ', '.join(also) + ')' if also else ''), flush=True)


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
    'vl_vcam_settings_lost': ('    if (data["vcam_settings"].isMap()) light.mVCamSettings = data["vcam_settings"];\n', ''),
    'vl_recreated_params_lost': (
        '    return is_light && std::fabs(intensity - want) < 1.e-4f && std::fabs(radius - light.mRadius) < 1.e-3f &&\n'
        '        std::fabs(falloff - light.mFalloff) < 1.e-3f;',
        '    return is_light;'),
    'vl_track_target_current': ('    if (target.mVirtual) return target.mKey.notNull() && virtual_alive(target.mKey);',
                                '    if (!target.mVirtual) return target.mKey.notNull() && virtual_alive(target.mKey);'),
    'vl_virtual_id_reaches_tracker': ('    return target.mId.notNull() && !is_virtual_id(target.mId) && real_alive(target.mId);',
                                      '    return target.mId.notNull() && real_alive(target.mId);'),
    'vl_track_reference_stale': ('    if (light.mTrack.mSubject == old_id)\n    {\n        light.mTrack.mSubject = new_id;',
                                 '    if (light.mTrack.mSubject == old_id)\n    {\n        light.mTrack.mSubject = old_id;'),
    'vl_snapshot_offset_dropped': (
        '        snapshot[i]["aim_object"] = light.mAimObject;\n        snapshot[i]["track"]["enabled"] = light.mTrack.mEnabled;\n'
        '        snapshot[i]["track"]["has_subject"] = light.mTrack.mHasSubject;\n'
        '        snapshot[i]["track"]["subject"] = light.mTrack.mSubject;\n',
        '        snapshot[i] = toLLSD(light);\n'),
    'vl_track_offset_lost': ('    for (S32 i = 0; i < 3; ++i) p[i] = static_cast<F32>(data["pos"][i].asReal());\n',
                             '    for (S32 i = 0; i < 3; ++i) p[i] = 0.f;\n'),
}


def native_source(root, header, tests):
    tests = tests.replace('#include "linden_common.h"\n', '')
    tests = tests.replace('#include "../test/lltut.h"\n', '')
    tests = tests.replace('#include "../alvirtuallightsmodel.h"\n', '')
    anchor = '    struct virtual_lights_data {};'
    assert tests.count(anchor) == 1, 'test file drifted'
    tests = tests.replace(anchor, '    test_runner_singleton runner;\n' + anchor)
    count = len(re.findall(r'void virtual_lights_object::test<(\d+)>\(\)', tests))
    assert count >= 14, 'the virtual-light TUT must keep at least fourteen tests'
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
