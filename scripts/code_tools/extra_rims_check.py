"""Extra-rim production contracts and actual TUT model mutation tests.

Contract mutations are applied to an in-memory source bundle, never the checkout.
Every mutation must fail its own named assertion; an unrelated failure does not
count. Native model mutations must compile and reach the corresponding TUT oracle.
All generated products live below this checkout's .tmp directory on I:.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]
PATHS = {
    'avatar': 'indra/newview/alghostclonemenu.cpp',
    **{name: 'indra/newview/skins/default/xui/en/' + name for name in (
        'menu_avatar_other.xml', 'menu_avatar_self.xml',
        'menu_attachment_other.xml', 'menu_attachment_self.xml')},
    'manager': 'indra/newview/alcinelightrigmanager.cpp',
    'rig': 'indra/newview/alcinelightrig.cpp',
    'header': 'indra/newview/alcinelightrigmanager.h',
    'ui': 'indra/newview/alpanelrigrim.cpp',
    'director': 'indra/newview/llfloaterdirector.cpp',
    'preset': 'indra/newview/alkitpresets.cpp',
    'pipeline': 'indra/newview/pipeline.cpp',
    'menu': 'indra/newview/skins/default/xui/en/menu_director_cast.xml',
}


def body(bundle, key, signature):
    return function(bundle[key], signature)


def require(condition, name):
    assert condition, name


def mutation(bundle, key, old, new, occurrence=1):
    require(old in bundle[key], 'mutation anchor missing: ' + old)
    changed = dict(bundle)
    changed[key] = bundle[key].replace(old, new, occurrence)
    require(changed[key] != bundle[key], 'mutation did not apply')
    return changed


def menu_contracts(b):
    menus = [ET.fromstring(b[name]).find("context_menu[@name='Director']/*[@name='Avatar Rim light']")
             for name in PATHS if name.endswith('.xml')]
    cast = ET.fromstring(b['menu']).find("*[@name='extra_rim_toggle']")
    checked = body(b, 'avatar', 'bool avatarRimChecked()\n{')
    actor = body(b, 'avatar', 'LLUUID avatarRimActor()')
    action = body(b, 'avatar', 'void actAvatarRim()\n{')
    enroll = body(b, 'avatar', 'bool ensureAvatarGazeCast(')
    cast_action = body(b, 'director', 'void LLFloaterDirector::onCastExtraRimToggle()')
    record = body(b, 'ui', 'void ALExtraRimUI::recordEdit(')
    def toggle(item, click, check, enable=None):
        if item is None or item.tag != 'menu_item_check' or item.get('label') != 'Rim light':
            return False
        for event, callback in [('on_click', click), ('on_check', check)] + ([('on_enable', enable)] if enable else []):
            node = item.find('menu_item_check.' + event)
            if node is None or node.get('function') != callback:
                return False
        return True
    no_rig = not re.search(r'borrowSlot|borrowFor|setSubject|enableSlot|ensureRig|giveRig', action + enroll)
    return {
        'xr_avatar_menu_missing': len(menus) == 4 and all(item is not None for item in menus),
        'xr_avatar_menu_gives_rig': no_rig and 'cast.add(actor);' in enroll and
            'ensureAvatarGazeCast(actor)' in action and 'rigs.addExtraRim(actor);' in action,
        'xr_avatar_menu_no_undo': action.count('ALExtraRimUI::recordEdit(shell, before, "Extra rim light");') == 1 and
            'const LLSD before = rigs.captureExtraRims();' in action and
            'rigs.removeExtraRim(actor)' in action and 'light_group/light_rim_tab' in record and
            'if (before == after) return;' in record and
            'ensurePageBuilt("light_group/light_rim_tab")' in action,
        'xr_menu_not_toggle': all(item is None or toggle(item, 'Avatar.RimToggle', 'Avatar.CheckRim', 'Avatar.EnableRim') for item in menus) and
            toggle(cast, 'Director.ExtraRimToggle', 'Director.ExtraRimCheck') and
            'hasExtraRim(avatarRimActor())' in checked and
            'resolveBorrowActor(*sAvatarRimPick, sAvatarRimWasCast)' in actor and
            'retainBorrowActor(sMenuAvatar)' in b['avatar'] and
            'if (present) rigs.removeExtraRim(actor);' in action and
            'else rigs.addExtraRim(actor);' in action and
            b['director'].count('rigs.hasExtraRim(rigs.resolveBorrowActor(*mExtraRimMenuPick, true))') == 2 and
            'resolveBorrowActor(*mExtraRimMenuPick, true)' in cast_action and
            'if (rigs.hasExtraRim(actor)) rigs.removeExtraRim(actor);' in cast_action and
            'ALExtraRimUI::recordEdit(mShell, before, "Extra rim light");' in cast_action and
            'setItemLabel("extra_rim_toggle"' not in b['director'],
    }


def contracts(b):
    manager = lambda signature: body(b, 'manager', signature)
    persist = manager('void ALCineLightRigManager::persist() const')
    replace = manager('void ALCineLightRigManager::replaceExtraRims(')
    scene = manager('void ALCineLightRigManager::applySceneData(')
    saved = manager('LLSD ALCineLightRigManager::extraRimsSceneData() const')
    registry = manager('void ALCineLightRigManager::appendExtraRimActorOnly(')
    derived = body(b, 'rig', 'ALCineLightRig::ExtraRimResult ALCineLightRig::extraRimFrom(')
    area = manager('bool ALCineLightRigManager::isMirrorLight(')
    rim = manager('bool ALCineLightRigManager::rigRimParamsFor(')
    rekey = manager('void ALCineLightRigManager::rewriteExtraRimActorReferences(')
    prune = manager('void ALCineLightRigManager::pruneRemovedExtraRims()')
    add = manager('bool ALCineLightRigManager::canAddExtraRim(')
    update = manager('void ALCineLightRigManager::updateExtraRims(')
    draw = body(b, 'ui', 'void ALPanelRigRim::draw()')
    # Follow every local UI helper reachable from draw, including future
    # indirection through refresh. Idle is deliberately not a draw root.
    helpers = {}
    for match in re.finditer(r'void\s+(?:ALPanelRigRim|ALExtraRimUI)::(\w+)\([^)]*\)\s*\{', b['ui']):
        helpers[match[1]] = function(b['ui'], match[0][:-1].strip())
    reachable, pending = {}, ['draw']
    while pending:
        name = pending.pop()
        if name in reachable:
            continue
        reachable[name] = helpers[name]
        pending += [callee for callee in helpers if callee not in reachable and
                    re.search(r'\b' + callee + r'\s*\(', helpers[name].split('{', 1)[1])]
    draw_path = '\n'.join(reachable.values())
    custom = b['preset'].split('case RIM_CUSTOM:', 1)[1].split('break;', 1)[0]
    return {
        **menu_contracts(b),
        'xr_persisted_in_instances': 'extra_rims' not in persist and 'mExtraRim' not in persist,
        'xr_login_recovery_restores':
            'if (login_recovery) { clearExtraRims(); return; }' in replace and
            'replaceExtraRims(data["extra_rims"], mLoginRecovery);' in scene,
        'xr_clone_saved':
            'LLDirectorCast::savedSceneActorId(extra.mActor)' in saved and
            'if (saved.notNull()) state.mItems.push_back({saved, extra.mTrim});' in saved,
        'xr_legacy_scene_leak':
            'replaceExtraRims(data["extra_rims"], mLoginRecovery);' in scene and
            'if (data.has("extra_rims"))' not in scene and
            scene.index('replaceExtraRims(') < scene.index('if (!data.isMap() || !data.has("version"))'),
        'xr_world_light':
            'input.mActorOnlyAll = true;' in registry and
            'input.mProjectors[2] = extra.mRig->projectorId(2);' in registry and
            'input.mRecipients.push_back(identity);' in registry and
            'identity.mAvatarId = avatar->getID();' in registry and
            'if (!disabled) appendExtraRimActorOnly(rigs, avatars, used);' in b['manager'],
        'xr_casts_shadow':
            'if (!LLPipeline::isProjectorNoShadow(to)) LLPipeline::toggleProjectorCastShadows(to);' in derived,
        'xr_area_shadow':
            'if (extra.mRig && extra.mRig->projectorId(2) == id) return true;' in area and
            b['pipeline'].count('registry.isMirrorLight(volume->getID())') >= 2,
        'xr_no_rigrim': 'out = extra.mRig->rigRimParams(2);\n            return true;' in rim,
        'xr_rekey_stale':
            'rewriteExtraRimActorReferences(old_id, new_id);' in
                manager('void ALCineLightRigManager::rewriteActorReferences(') and
            'if (extra.mRig) extra.mRig->shutdown();' in rekey and
            'extra.mActor = new_id;' in rekey and 'it->mActor.isNull()' in rekey,
        'xr_premature_prune':
            'if (cast.contains(it->mActor)) it->mEnrolled = true;' in prune and
            '(!cast.contains(it->mActor) && it->mEnrolled)' in prune,
        'xr_double_rim':
            'if (hasOwnLitRig(id)) return refuse("Has own rig");' in add and
            'if (hasOwnLitRig(extra.mActor, &recipients)) state = ExtraRimState::OWN_RIG;' in update and
            'if (!isSlotLit(static_cast<Slot>(i)) || rig.getObjectTarget().notNull()) continue;' in
                manager('ALCineLightRigManager::ExtraRimRecipientCache ALCineLightRigManager::extraRimRecipientCache(') and
            'if (!(cache.mLit & (1u << i))) continue;' in manager('bool ALCineLightRigManager::hasOwnLitRig('),
        'xr_preset_owned':
            not re.search(r'extra_?rims?', custom, re.I) and 'keys = owned(RIM);' in custom,
        'xr_menu_one_registrar':
            body(b, 'director', 'bool LLFloaterDirector::postBuild()').count('"Director.ExtraRimToggle"') == 1 and
            body(b, 'director', 'void LLFloaterDirector::bindChrome(').count('"Director.ExtraRimToggle"') == 1 and
            'function="Director.ExtraRimToggle"' in b['menu'],
        'xr_draw_work':
            not re.search(r'getChild|findChild|gSavedSettings|asString\(|deleteAllItems|addElement|->setText\(', draw_path) and
            'if (mRefreshClock.due())' in draw and
            'panel->mExtras.refresh();' in body(b, 'ui', 'void ALPanelRigRim::idle(') and
            'if (mRevision != rigs.extraRimRevision())' in b['ui'] and
            '!isWidgetOnScreen(mRoot)' in b['ui'],
    }


def planted_negatives(b):
    persist = body(b, 'manager', 'void ALCineLightRigManager::persist() const')
    # Mutate the actual persistence writer, not the intentionally scene-only
    # serializer elsewhere in this file.
    # This is a source-contract mutant; use the real local store name so it
    # remains a concrete plausible persistence regression.
    local = re.search(r'LLSD\s+(\w+)\s*=', persist)
    require(local is not None, 'instance persistence writer drift')
    bad_persist = persist.replace('gSavedSettings.setLLSD(',
        local[1] + '["extra_rims"] = captureExtraRims();\n    gSavedSettings.setLLSD(', 1)
    area = body(b, 'manager', 'bool ALCineLightRigManager::isMirrorLight(')
    return {
        'xr_avatar_menu_missing': mutation(b, 'menu_avatar_other.xml', 'name="Avatar Rim light"', 'name="Planted missing rim"'),
        'xr_avatar_menu_gives_rig': mutation(b, 'avatar', '    cast.add(actor);', '    cast.add(actor);\n    rigs.borrowFor(actor);'),
        'xr_avatar_menu_no_undo': mutation(b, 'avatar', '    ALExtraRimUI::recordEdit(shell, before, "Extra rim light");', ''),
        'xr_menu_not_toggle': mutation(b, 'avatar', 'hasExtraRim(avatarRimActor())', 'hasExtraRim(LLUUID::null)'),
        'xr_persisted_in_instances': mutation(b, 'manager', persist, bad_persist),
        'xr_login_recovery_restores': mutation(b, 'manager', 'if (login_recovery) { clearExtraRims(); return; }',
                                              'if (false) { clearExtraRims(); return; }'),
        'xr_clone_saved': mutation(b, 'manager',
            'if (saved.notNull()) state.mItems.push_back({saved, extra.mTrim});',
            'state.mItems.push_back({extra.mActor, extra.mTrim});'),
        'xr_legacy_scene_leak': mutation(b, 'manager',
            '        replaceExtraRims(data["extra_rims"], mLoginRecovery);',
            '        if (data.has("extra_rims")) replaceExtraRims(data["extra_rims"], mLoginRecovery);'),
        'xr_world_light': mutation(b, 'manager',
            body(b, 'manager', 'void ALCineLightRigManager::appendExtraRimActorOnly('),
            body(b, 'manager', 'void ALCineLightRigManager::appendExtraRimActorOnly(').replace(
                'input.mActorOnlyAll = true;', 'input.mActorOnlyAll = false;')),
        'xr_casts_shadow': mutation(b, 'rig',
            'if (!LLPipeline::isProjectorNoShadow(to)) LLPipeline::toggleProjectorCastShadows(to);',
            'if (LLPipeline::isProjectorNoShadow(to)) LLPipeline::toggleProjectorCastShadows(to);'),
        'xr_area_shadow': mutation(b, 'manager', area, area.replace(
            'if (extra.mRig && extra.mRig->projectorId(2) == id) return true;',
            'if (extra.mRig && extra.mRig->projectorId(2) == id) return false;')),
        'xr_no_rigrim': mutation(b, 'manager', 'out = extra.mRig->rigRimParams(2);\n            return true;',
                                 'out = extra.mRig->rigRimParams(2);\n            return false;'),
        'xr_rekey_stale': mutation(b, 'manager', 'extra.mActor = new_id;', 'extra.mActor = old_id;'),
        'xr_premature_prune': mutation(b, 'manager',
            '(!cast.contains(it->mActor) && it->mEnrolled)', '(!cast.contains(it->mActor))'),
        'xr_double_rim': mutation(b, 'manager',
            'if (hasOwnLitRig(extra.mActor, &recipients)) state = ExtraRimState::OWN_RIG;',
            'if (false) state = ExtraRimState::OWN_RIG;'),
        'xr_preset_owned': mutation(b, 'preset', 'case RIM_CUSTOM:\n        keys = owned(RIM);',
                                    'case RIM_CUSTOM:\n        keys = owned(RIM);\n        keys.emplace_back("extra_rims");'),
        'xr_menu_one_registrar': mutation(b, 'director', '"Director.ExtraRimToggle"', '"Director.PlantedUnusedAction"'),
        'xr_draw_work': mutation(b, 'ui', '    mRim.tickFrame();',
                                 '    mRim.tickFrame();\n    mExtras.refresh();'),
    }


def check_contracts(bundle):
    # Filled with production-specific contracts below; each is deliberately
    # independent so the named mutation runner can identify the failed oracle.
    for name, predicate in contracts(bundle).items():
        require(predicate, name)


def run_contracts(root=ROOT):
    bundle = {key: (root / path).read_text(encoding='utf8') for key, path in PATHS.items()}
    check_contracts(bundle)
    for name, changed in planted_negatives(bundle).items():
        try:
            check_contracts(changed)
        except AssertionError as error:
            require(str(error) == name,
                    f'{name} failed a different assertion: {error}')
        else:
            raise AssertionError(name + ' planted bug escaped')
        print('PASS: rejected planted ' + name + ' at its named assertion', flush=True)
    variants = []
    for name in PATHS:
        if name.endswith('.xml'):
            variants.append(('xr_avatar_menu_missing', mutation(bundle, name, 'name="Avatar Rim light"', 'name="Missing"')))
            variants.append(('xr_menu_not_toggle', mutation(bundle, name, '<menu_item_check label="Rim light" name="Avatar Rim light">', '<menu_item_call label="Rim light" name="Avatar Rim light">').copy()))
            # Keep the planted XML syntactically valid.
            variants[-1][1][name] = variants[-1][1][name].replace('function="Avatar.EnableRim" />\n    </menu_item_check>', 'function="Avatar.EnableRim" />\n    </menu_item_call>', 1)
    variants.append(('xr_menu_not_toggle', mutation(bundle, 'director', 'rigs.hasExtraRim(rigs.resolveBorrowActor(*mExtraRimMenuPick, true))', 'false')))
    variants.append(('xr_menu_not_toggle', mutation(bundle, 'menu', '<menu_item_check label="Rim light"', '<menu_item_call label="Rim light"')))
    variants[-1][1]['menu'] = variants[-1][1]['menu'].replace('function="Director.ExtraRimCheck" />\n  </menu_item_check>', 'function="Director.ExtraRimCheck" />\n  </menu_item_call>', 1)
    for name, changed in variants:
        try:
            check_contracts(changed)
        except AssertionError as error:
            require(str(error) == name, f'{name} failed a different assertion: {error}')
        else:
            raise AssertionError(name + ' variant escaped')
        print('PASS: rejected planted ' + name + ' menu variant at its named assertion', flush=True)
    print('PASS: extra-rim production contracts', flush=True)


if __name__ == '__main__':
    run_contracts()
    from extra_rims_model_check import run_unit
    run_unit(ROOT)
    from extra_rims_ui_check import run_unit as run_ui
    run_ui(ROOT)
    from extra_rims_registry_check import run_unit as run_registry
    run_registry(ROOT)
    from extra_rims_engine_check import run_unit as run_engine
    run_engine(ROOT)

    from extra_rims_round2_check import run_unit as run_round2
    run_round2(ROOT)

    from extra_rims_round3_check import run_unit as run_round3
    run_round3(ROOT)

    from extra_rims_round4_check import run_unit as run_round4
    run_round4(ROOT)
