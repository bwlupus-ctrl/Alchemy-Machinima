"""Entity clone "Stay in place": native fixture over the production placement code,
structural pins, and planted negatives.

The switch never writes the skeleton. It shifts the clone's PRESENTED root on top
of the authored placement, so the fixture runs the real methods (spliced out of the
viewer sources) against small doubles and proves: OFF is inert; ON keeps the drawn
hips over the spot while an animation drifts the pelvis; height and pose are kept;
turned / tilted / scaled / floor-reflected roots; drag, path, undo and the floor hold
keep authored values; no skeleton writes; Stepped's held pelvis and held root, and a
snapshot between frames; delete / respawn / overlay registry; multi-select history.
"""
import json
import subprocess
from pathlib import Path
from native_check_tools import ROOT, compile_run

BASE = '8c350a71b4e'


def read(path):
    return path.read_text(encoding='utf-8').replace('\r\n', '\n')


def method(source, name):
    start = source.index(name)
    start = source.rfind('\n', 0, start) + 1
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def line_with(source, text):
    lines = [line.strip() for line in source.splitlines() if text in line]
    assert len(lines) == 1, (text, len(lines))
    return lines[0]


def statement_after(source, text):
    """A two-line `if (...)` + `{ ... }` statement, as the Stepped restore writes them."""
    lines = source.splitlines()
    hits = [i for i, line in enumerate(lines) if text in line]
    assert len(hits) == 1, (text, len(hits))
    return lines[hits[0]].strip() + '\n' + lines[hits[0] + 1].strip()


def base(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT).decode('utf-8').replace('\r\n', '\n')


# The skeleton is never written: the whole Stay in place block may touch joints
# only through these calls (the root joint's world position, and reads).
def no_skeleton_writes(block):
    for token in ('setPosition(', 'setRotation(', 'setScale(', 'setWorldRotation(', 'setSkinOffset(',
                  'addAttachmentPosOverride', 'm_posBeforeOverrides', 'applyHeldPose', 'stampTrueMirror'):
        assert token not in block, token
    pelvis = [line.strip() for line in block.splitlines() if 'mPelvisp' in line]
    for line in pelvis:
        ok = ('mPelvisp->m_attachmentPosOverrides.getMap()' in line or 'mPelvisp->getPosition()' in line or
              '!mPelvisp' in line or '!ghost->mPelvisp' in line)
        assert ok and 'mPelvisp->set' not in line, line
    for line in block.splitlines():
        if 'mRoot->' in line:
            assert any(call in line for call in ('mRoot->setWorldPosition(', 'mRoot->getWorldPosition()', 'mRoot->getScale()',
                'mRoot->getWorldRotation()', 'mRoot->updateWorldMatrixChildren()', 'mRoot->mDirtyFlags & LLJoint::MATRIX_DIRTY')), line


def check():
    nv = ROOT / 'indra/newview'
    runtime = read(nv / 'llghostavatar.cpp')
    header = read(nv / 'llghostavatar.h')
    avatar = read(nv / 'llvoavatar.cpp')
    ui = read(nv / 'alpanelghoststudio.cpp')
    studio = read(nv / 'alghoststudio.cpp')
    studio_h = read(nv / 'alghoststudio.h')
    app = read(nv / 'llappviewer.cpp')
    stepped = read(nv / 'alsteppedanimruntime.cpp')
    proxy = read(nv / 'alghostmanipproxy.cpp')
    reflection = read(nv / 'alreflectmath.h')
    math_header = read(nv / 'alghoststayinplace.h')
    fixture = read(Path(__file__).parent / 'fixtures/clone_stay_in_place.cpp.in')

    production = runtime[runtime.index('void ALGhostStudio::setInstanceStayInPlace('):
                         runtime.index('void LLGhostAvatar::setDirectedMirror(')]
    slam = method(runtime, 'void LLGhostAvatar::ghostSlamPosition(') + '\n' + method(runtime, 'void LLGhostAvatar::slamGhostRoot(')
    placement = '\n'.join(method(runtime, f'void LLGhostAvatar::{name}(') for name in (
        'setGhostPosition', 'setGhostFootPosition', 'setGhostFloorPosition',
        'applyDesiredGhostFootPosition', 'syncGhostObjectToMovingRoot'))
    setters = method(runtime, 'void LLGhostAvatar::setGhostRotation(') + '\n' + method(runtime, 'void LLGhostAvatar::setEntityScale(')
    reflect_source = method(studio, 'LLVector3d reflectSourceFoot(')
    base_root = (method(avatar, 'LLVector3 LLVOAvatar::getBaseRootPosition(') + '\n' +
                 method(avatar, 'LLVector3 LLVOAvatar::getPlacedRootPosition('))
    mover = read(nv / 'llactormover.cpp')
    rig = read(nv / 'alcinelightrigmanager.cpp')
    mover_facing = line_with(mover, 'LLVector3 d = tgt->getPlacedRootPosition() - agent;')
    mover_origin = line_with(mover, 'LLVector3 origin = av->getPlacedRootPosition();')
    rig_foot = line_with(rig, 'LLVector3 foot = source->getPlacedRootPosition();')
    idle_update = method(runtime, 'void LLGhostAvatar::idleUpdate(')
    idle_tail = idle_update[idle_update.index('    if (!LLActorMover::instance().applyOverride(this))'):]
    handler = method(ui, 'void ALPanelGhostStudio::onStayInPlaceCommit(')
    reflect_stamp = method(runtime, 'void LLGhostAvatar::applyReflectStamp(')
    capture_start = reflect_stamp.index('            if (floor && mGhostFloorPosition)')
    floor_capture = reflect_stamp[capture_start:reflect_stamp.index(';', capture_start) + 1]
    floor_hold = method(reflection, 'struct FloorRootHold') + ';'

    # ---- structural pins -------------------------------------------------------
    no_skeleton_writes(production)
    for bad in (production + '\n    mPelvisp->setPosition(LLVector3::zero, false);',
                production.replace('    mStayOffset = offset;', '    mStayOffset = offset; mRoot->setWorldRotation(LLQuaternion());'),
                production + '\n    getJoint("mTorso")->setRotation(LLQuaternion());',
                production + '\n    ghost->mPelvisp->setScale(LLVector3::zero);',
                production + '\n    mRoot->setSkinOffset(LLVector3::zero);'):
        try:
            no_skeleton_writes(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('skeleton-write negative survived')
    print('PASS 5 planted skeleton-write negatives rejected (static)', flush=True)

    # The pose machinery v1 had to entangle is byte-identical to base, and the
    # byte-pinned Studio file carries no Stay in place code at all.
    base_runtime = base('indra/newview/llghostavatar.cpp')
    for name in ('void LLGhostAvatar::stampTrueMirrorSkeleton(', 'void LLGhostAvatar::applyDirectedMirrorStamp(',
                 'ALGhostClearAnim::HeldPose LLGhostAvatar::captureHeldPose(', 'void LLGhostAvatar::applyHeldPose(',
                 'void LLGhostAvatar::copyHeldPoseFrom(', 'void LLGhostAvatar::applyOffsetHistory(',
                 'void LLGhostAvatar::setEntityDriveMode(', 'void LLGhostAvatar::setEntityAnimationsCleared(',
                 'void LLGhostAvatar::setStartOffset(', 'void LLGhostAvatar::setDirectedMirror(',
                 'void LLGhostAvatar::moveStampedAttachments(', 'void LLGhostAvatar::publishGhostTransform(',
                 'void LLGhostAvatar::restoreDirectedMirrorPose(', 'bool LLGhostAvatar::buildSkeleton('):
        assert method(runtime, name) == method(base_runtime, name), name
    new_capture = ('            // [StayInPlace] The hold is an AUTHORED root: idleUpdate hands it back\n'
                   '            // to ghostSlamPosition(), which presents the offset on top again.\n' + floor_capture)
    old_capture = ('            if (floor && mGhostFloorPosition)\n'
                   '                mReflectFloorRootHold.capture(gAgent.getPosGlobalFromAgent(mRoot->getWorldPosition()), q);')
    base_stamp = method(base_runtime, 'void LLGhostAvatar::applyReflectStamp(')
    assert reflect_stamp.count(new_capture) == 1 and base_stamp.count(old_capture) == 1
    assert reflect_stamp.replace(new_capture, '@') == base_stamp.replace(old_capture, '@'), \
        'only the floor hold capture may differ in applyReflectStamp'
    for path in ('indra/newview/alghoststudio.cpp', 'indra/newview/llviewerpartsim.cpp', 'indra/newview/llviewerwindow.cpp',
                 'indra/newview/llactormover.h', 'indra/newview/lldirectorcast.cpp', 'indra/newview/alghostclearanim.h'):
        assert read(ROOT / path) == base(path), path + ' must stay at base'
    # The byte-pinned mover differs from base by exactly its reviewed allowance layer: two reads of
    # where an avatar stands. The layer file is pinned by digest in director_actor_bar_check.py.
    import hashlib
    layer_raw = (Path(__file__).parent / 'clone_stay_in_place_mover_hunks.json').read_bytes().replace(b'\r\n', b'\n')
    layer = json.loads(layer_raw)
    assert f'CLONE_STAY_IN_PLACE_MOVER_DIGEST = "{hashlib.sha256(layer_raw).hexdigest()}"' in read(Path(__file__).parent / 'director_actor_bar_check.py')
    assert layer['base'] == BASE and list(layer['files']) == ['llactormover.cpp'] and len(layer['files']['llactormover.cpp']) == 2
    peeled = mover
    for hunk in layer['files']['llactormover.cpp']:
        assert peeled.count(hunk['after']) == 1 and hunk['after'].count('getPlacedRootPosition()') == 1
        assert hunk['before'].count('getRootJoint()->getWorldPosition()') == 1
        peeled = peeled.replace(hunk['after'], hunk['before'], 1)
    assert peeled == base('indra/newview/llactormover.cpp'), 'llactormover.cpp differs from base beyond its allowance layer'
    assert mover.count('StayInPlace') == 2 and rig.count('StayInPlace') == 1 and rig.count('getPlacedRootPosition()') == 1
    assert 'StayInPlace' not in studio
    # Copies (duplicate, crowd, strip) take the Instance wholesale, so they carry the switch.
    assert studio.count('const Instance proto = *src;') == 1 and studio.count('const Instance proto = *source;') == 1
    assert 'bool        mStayInPlace = false;' in studio_h and 'void      setInstanceStayInPlace(' in studio_h

    # Stepped's own writer/restorer bodies are untouched; only the three hooks were added.
    base_stepped = base('indra/newview/alsteppedanimruntime.cpp')
    for name in ('void ALSteppedAnimRuntime::writeOverlay(', 'void ALSteppedAnimRuntime::restoreOverlay('):
        assert method(stepped, name) == method(base_stepped, name), name
    step_begin = 'if (LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::beginStayInPlaceStepped(avatar);'
    step_end = 'if (LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::endStayInPlaceStepped();'
    apply_held = method(stepped, 'void ALSteppedAnimRuntime::applyHeld(')
    step_present = method(apply_held[apply_held.index('    // [StayInPlace] Every overlay is written'):],
                          'if (LLGhostAvatar::hasStayInPlaceAvatars())')
    assert 'LLGhostAvatar::presentStayInPlaceStepped(stay_verify);' in step_present
    assert 'static LLCachedControl<bool> stay_verify(gSavedSettings, "StepAnimDebugVerify");' in step_present
    import difflib
    delta = [line for line in difflib.unified_diff(base_stepped.splitlines(), stepped.splitlines(), lineterm='', n=0)
             if line[:1] in '+-' and line[:3] not in ('+++', '---')]
    old_verify = '((entry.mVerifyRootMask & WRITE_POS) && !vecEqual(root->getPosition(), entry.mVerifyPose.rootPos))))'
    new_verify = '((entry.mVerifyRootMask & WRITE_POS) && !vecEqual(verify_root, entry.mVerifyPose.rootPos))))'
    assert [line[1:].strip() for line in delta if line.startswith('-')] == [old_verify], 'Stepped lost base lines'
    added = [line[1:].strip() for line in delta if line.startswith('+')]
    step_verify_root = ('LLVector3 verify_root = root ? root->getPosition() : LLVector3::zero;\n'
                        'if (root && LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::steppedVerifyRoot(avatar, verify_root);')
    verify_restored = method(stepped, 'void ALSteppedAnimRuntime::verifyRestored(')
    assert verify_restored.index(step_verify_root.splitlines()[0]) < verify_restored.index(step_verify_root.splitlines()[1]) < verify_restored.index(new_verify)
    assert sorted(added) == sorted([
        "// [StayInPlace] Capture and hold the clone's AUTHORED root, never its",
        '// presented one. One empty-registry test when every switch is off.',
        step_begin,
        '// [StayInPlace] Every overlay is written: keep the hips over the spot for',
        '// the pelvis and root rotation Stepped actually presents.',
        'if (LLGhostAvatar::hasStayInPlaceAvatars())',
        '{',
        '// StepAnimDebugVerify then expects what is really rendered, not the',
        '// state writeOverlay() captured before this presentation.',
        'static LLCachedControl<bool> stay_verify(gSavedSettings, "StepAnimDebugVerify");',
        'LLGhostAvatar::presentStayInPlaceStepped(stay_verify);',
        '}',
        '// [StayInPlace] Mirror applyHeld(): the smooth pose is back, present it.',
        step_end,
        '// [StayInPlace] Stepped held and restored the AUTHORED root; the root joint',
        '// then carries the presented offset again. Compare what Stepped owns.',
        step_verify_root.splitlines()[0], step_verify_root.splitlines()[1], new_verify,
    ]), added
    assert (apply_held.index('if (!eligible(avatar)) continue;') < apply_held.index(step_begin) <
            apply_held.index('readPose(avatar, entry, entry.mSmooth);') <
            apply_held.index('writeOverlay(avatar, entry, entry.mWritten);') < apply_held.index(step_present) <
            apply_held.index('applyStepAnimNameplateOffsets('))
    # writeOverlay() captures the verifier's expectation; the presentation that follows re-captures it.
    assert 'if (verify) avatar->captureStepAnimRenderState();' in method(stepped, 'void ALSteppedAnimRuntime::writeOverlay(')
    end_frame = method(stepped, 'void ALSteppedAnimRuntime::endFrameRestore(')
    assert end_frame.index('restoreOverlay(avatar, entry);') < end_frame.index(step_end) < end_frame.index('sHasOverlay = false;')

    # Frame order: the main point follows the last ordinary pose writer and
    # precedes every pose consumer and Stepped.
    idle = method(app, 'void LLAppViewer::idle(')
    app_hook = 'if (LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::applyStayInPlaceAvatars();'
    pre_hook = 'if (LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::beginStayInPlacePostPass();'
    assert app.count('StayInPlace') == 9 and idle.count(app_hook) == 2 and idle.count(pre_hook) == 1
    main_point = idle.index(app_hook)
    late_point = idle.index(app_hook, main_point + 1)
    # writers (W) and placement consumers (C), in idle() order
    order = [
        idle.index('gObjectList.update(gAgent);'),                               # W controllers, mover, foot lock
        idle.index(pre_hook),                                                    #   source roots authored for Studio
        idle.index('ALGhostStudio::instance().updatePostObjectList();'),         # W stamps, Reflect placement, tilt
        main_point,                                                              #   PRESENT
        idle.index('ALAvatarLightResponse::tick();'),                            # C
        idle.index('gObjectList.cleanDeadObjects();'),
        idle.index('LLSelectMgr::getInstance()->updateEffects();'),              # C HUD effects
        idle.index('gPipeline.updateMove();'),                                   # C drawable moves
        idle.index('LLWorld::getInstance()->updateParticles();'),                # C particles
        idle.index('ALToolGhostEdit::getInstance()->getManipProxy().tick();'),   # W transform setters (self-presenting)
        idle.index('ALDirectorSwitcher::instance().tick('),
        idle.index('ALDirectorAnimSwitcher::instance().tick('),                  # W Switchboard forced pose cut
        late_point,                                                              #   PRESENT
        idle.index('ALLocalFogManager::instance().tick('),                       # C
        idle.index('ALCineLightRigManager::instance().tick('),                   # C
        idle.index('ALVCamBonePov::instance().tick('),                           # C
        idle.index('gAgentPilot.moveCamera();'),                                 # C camera dispatch
        idle.index('LLCinematicCamera::instance().updateCamera();'),             # C
        idle.index('gAgentCamera.updateCamera();'),                              # C
        idle.index('ALSteppedAnimRuntime::instance().applyHeld();'),             # W held overlay (own PRESENT hooks)
        idle.index('applyHotGazes();'),                                          # W rotations only
        idle.index('ALProjectorTracker::instance().update(dt_raw);'),            # C
    ]
    assert order == sorted(order) and len(set(order)) == len(order), order
    # The transform setters a late edit reaches present for the NEW rotation / scale themselves.
    for name in ('setGhostRotation', 'setGhostTiltRotation', 'setEntityScale'):
        assert method(runtime, f'void LLGhostAvatar::{name}(').rstrip().endswith('if (mStayInPlace) applyStayInPlace();\n}'), name
    # Studio's Reflect "Mirror position" reads (pinned file): the source's root joint and base root.
    assert 'LLVector3 foot = floor ? source->getBaseRootPosition() : source->getRootJoint()->getWorldPosition();' in reflect_source
    assert 'gAgent.getPosGlobalFromAgent(source->getRootJoint()->getWorldPosition()), plane.point.mdV[VZ]);' in studio
    assert 'inst.mFootGlobal = mirrored;' in studio
    base_app = base('indra/newview/llappviewer.cpp')
    assert method(app, 'bool LLAppViewer::doFrame(') == method(base_app, 'bool LLAppViewer::doFrame(')

    # Lifetime, respawn, authoring space, proxy, UI.
    for name in ('LLGhostAvatar::~LLGhostAvatar(', 'void LLGhostAvatar::markForDeath(', 'void LLGhostAvatar::markDead('):
        body = method(runtime, name)
        assert body.index('clearStayInPlace();') < body.index('sOffsetHotGazes.erase(this);'), name
    assert 'void setClientHeightKey(const LLUUID& key);' in header
    assert 'new_ghost->setClientHeightKey(stable_id);\n        applyEntityRuntimeState(inst, new_ghost);' in studio
    assert 'rendered_root -= ghost->getStayInPlaceRenderedOffset();' in proxy and proxy.count('StayInPlace') == 3
    assert avatar.count('StayInPlace') == 3 and 'getStayInPlaceAuthoredRoot(pos)' in base_root
    assert '"Entity clones only"' in ui and 'selectedInstances()' in handler and 'recordCustom(' in handler
    data = json.loads(read(ROOT / 'scripts/code_tools/director_kit/director_mockup_data.json'))
    page = next(p for g in data['groups'] for p in g['pages'] if p['name'] == 'ghosts_tab')
    rows = next(c for c in page['cards'] if c['id'] == 'pose')['rows']
    row = next(r for r in rows if r.get('w') == 'stay_in_place_check')
    assert rows.index(row) == next(i for i, r in enumerate(rows) if r.get('w') == 'drive_mode_combo') + 1
    assert rows[rows.index(row) + 1]['w'] == 'mirror_hold_check'
    assert row['l'] == 'Stay in place' and row['d'] == 0 and row['k'] == 'toggle' and 's' not in row
    help_text = ("Keep this clone's hips over its spot when an animation moves them forward or sideways. "
                 "Height is kept, so sits and crouches still work. Entity clones only.")
    assert row['h'] == help_text and ui.count('"' + help_text + '"') == 1
    print('PASS structural pins (pose machinery at base, Stepped hooks only, frame order, lifetime, kit row)', flush=True)

    # ---- native fixture ----------------------------------------------------------
    write_overlay = method(stepped, 'void ALSteppedAnimRuntime::writeOverlay(')
    restore_overlay = method(stepped, 'void ALSteppedAnimRuntime::restoreOverlay(')
    scoped = lambda text: '{ StayScope scope; ' + text + ' }'
    main_hook, late_hook = scoped(app_hook) + ' // main point', scoped(app_hook) + ' // late point'
    replacements = {
        '@PRODUCTION@': production, '@SLAM@': slam, '@PLACEMENT@': placement, '@BASE_ROOT@': base_root,
        '@SETTERS@': setters, '@REFLECT_SOURCE@': reflect_source,
        '@MOVER_FACING@': mover_facing, '@MOVER_ORIGIN@': mover_origin, '@RIG_FOOT@': rig_foot,
        '@STEP_VERIFY_ROOT@': step_verify_root,
        '@IDLE_TAIL@': idle_tail, '@UI@': handler, '@FLOOR_HOLD@': floor_hold, '@FLOOR_CAPTURE@': floor_capture.strip(),
        '@DEATH_CLEAR@': 'clearStayInPlace();',
        '@FRAME_PRE@': scoped(pre_hook), '@FRAME_MAIN@': main_hook, '@FRAME_LATE@': late_hook,
        '@STEP_BEGIN@': scoped(step_begin), '@STEP_PRESENT@': scoped(step_present), '@STEP_RESTORE_END@': scoped(step_end),
        '@STEP_WRITE@': line_with(write_overlay, 'joint->setPosition(target.pos, false)'),
        '@STEP_ROOT_WRITE@': line_with(write_overlay, 'root->setPosition(render.rootPos, false)'),
        '@STEP_RESTORE@': statement_after(restore_overlay, 'if ((mask & WRITE_POS) && vecEqual(joint->getPosition(), entry.mWritten.joints[i].pos))'),
        '@STEP_ROOT_RESTORE@': statement_after(restore_overlay, 'if ((entry.mRootWrittenMask & WRITE_POS) && vecEqual(root->getPosition(), entry.mWritten.rootPos))'),
    }
    source = fixture
    for key, value in replacements.items():
        assert key in source, key
        source = source.replace(key, value)
    assert '@' not in source, sorted({part.split('@')[0] for part in source.split('@')[1::2]})
    directory = ROOT / '.tmp/native_checks/clone_stay_in_place'
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'alghoststayinplace.h').write_text(math_header, encoding='utf-8')
    flags = (f'/I"{directory}"', '/W4', '/WX')
    result = compile_run(directory, source, flags=flags)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='', flush=True)

    def swap(text, old, new, count=1):
        assert text.count(old) == count, (old, text.count(old))
        return text.replace(old, new)
    negatives = {
        # the math
        'height_removed': (source, swap(math_header, 'pelvis_local.mV[1], 0.f);', 'pelvis_local.mV[1], pelvis_local.mV[2]);')),
        'world_frame_not_root_frame': (source, swap(math_header, 'Vector offset = drift * root_rotation;', 'Vector offset = drift; (void)root_rotation;')),
        'sideways_kept': (source, swap(math_header, 'Vector drift(pelvis_local.mV[0] - rest_x, pelvis_local.mV[1], 0.f);', 'Vector drift(pelvis_local.mV[0] - rest_x, 0.f, 0.f);')),
        'compensation_sign': (source, swap(math_header, 'offset.mV[axis] *= -rendered_scale;', 'offset.mV[axis] *= rendered_scale;')),
        'rest_forward_discarded': (swap(source, 'overrides.empty() ? 0.f : overrides.rbegin()->second.mV[VX];', 'overrides.empty() ? 0.f : 0.f * overrides.rbegin()->second.mV[VX];'), math_header),
        'lowest_override_wins': (swap(source, 'overrides.rbegin()->second.mV[VX];', 'overrides.begin()->second.mV[VX];'), math_header),
        'outer_scale_ignored': (swap(source, 'mRoot->getWorldRotation(), mGhostFloorPosition ? 1.f : mEntityScale);', 'mRoot->getWorldRotation(), 1.f);'), math_header),
        'floor_pivot_scaled_twice': (swap(source, 'mRoot->getWorldRotation(), mGhostFloorPosition ? 1.f : mEntityScale);', 'mRoot->getWorldRotation(), mEntityScale);'), math_header),
        # OFF and scope
        'applied_when_off': (swap(source, 'setStayInPlace(inst && inst->mStayInPlace);', 'setStayInPlace(inst != nullptr);'), math_header),
        'applied_to_overlay': (swap(source, 'enabled = enabled && inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE && !inst->mDeleting &&', 'enabled = enabled && inst && !inst->mDeleting &&'), math_header),
        'model_accepts_overlay': (swap(source, 'if (!inst || inst->mDeleting || inst->mKind != BACKING_ENTITY_CLONE || inst->mStayInPlace == enabled) return;', 'if (!inst || inst->mDeleting || inst->mStayInPlace == enabled) return;'), math_header),
        'toggle_off_keeps_offset': (swap(source, '    slamGhostRoot(authored, authored);\n', ''), math_header),
        'off_without_region_drops_restore': (swap(source, 'if (!mRoot || !getRegion()) return; // not placeable now: retried from the frame hook', 'if (!mRoot || !getRegion()) { clearStayInPlace(); return; }'), math_header),
        'owed_restore_never_paid': (swap(source, '        finishStayInPlaceDisable();\n        return;', '        return;'), math_header),
        'owed_restore_keeps_presenting': (swap(source, 'if (mStayDisabling || !isBuilt() || !mRoot || !mPelvisp) return LLVector3::zero;', 'if (!isBuilt() || !mRoot || !mPelvisp) return LLVector3::zero;').replace('    if (mStayDisabling)\n    {\n        finishStayInPlaceDisable();\n        return;\n    }\n', ''), math_header),
        'toggle_not_applied_at_once': (swap(source, '        // A toggle is visible at once, even to a snapshot taken before the next idle.\n        ghost->applyStayInPlace();\n', ''), math_header),
        # authored placement
        'authored_records_presented': (swap(source, '    slamGhostRoot(presented, mStayRootAuthored ? pos_agent : presented);\n',
            '    slamGhostRoot(presented, mStayRootAuthored ? pos_agent : presented);\n    mStayAuthoredRootGlobal = gAgent.getPosGlobalFromAgent(presented);\n'), math_header),
        # late transform and pose writers
        'slam_reuses_cached_offset': (swap(source, '    mStayOffset = measureStayInPlaceOffset();\n', ''), math_header),
        'rotate_reuses_old_rotation': (swap(source, '    // [StayInPlace] The offset turns with the root: present it for THIS rotation.\n    if (mStayInPlace) applyStayInPlace();\n', ''), math_header),
        'stretch_reuses_old_scale': (swap(source, '    // [StayInPlace] The offset scales with the drawn body: present it for THIS scale.\n    if (mStayInPlace) applyStayInPlace();\n', ''), math_header),
        'late_pose_writer_not_presented': (swap(source, late_hook, ''), math_header),
        # a Stay in place clone as a Reflect source
        'studio_reads_presented_source': (swap(source, scoped(pre_hook), ''), math_header),
        'source_slam_breaks_authored_pass': (swap(source, 'slamGhostRoot(presented, mStayRootAuthored ? pos_agent : presented);', 'slamGhostRoot(presented, presented);'), math_header),
        'source_setter_breaks_authored_pass': (swap(source, 'const LLVector3 root_agent = mStayRootAuthored ? root_base_agent : root_base_agent + offset;', 'const LLVector3 root_agent = root_base_agent + offset;'), math_header),
        'source_republishes_every_frame': (swap(source, '    mRoot->setWorldPosition(mStayPassRoot);\n', ''), math_header),
        'pass_matrix_propagation_undetected': (swap(source, 'const bool resolved = mStayPassDirtied && !(mRoot->mDirtyFlags & LLJoint::MATRIX_DIRTY);', 'const bool resolved = mStayPassDirtied && false;'), math_header),
        'pass_dirty_witness_never_armed': (swap(source, 'ghost->mStayPassDirtied = (ghost->mRoot->mDirtyFlags & LLJoint::MATRIX_DIRTY) != 0;', 'ghost->mStayPassDirtied = false;'), math_header),
        'restore_verify_blind_to_real_change': (swap(source, '        root_agent -= ghost->mStayOffset;', '        root_agent = authored;'), math_header),
        'restore_verify_exact_bits_only': (swap(source, 'if (dist_vec_squared(root_agent, authored + ghost->mStayOffset) <= ALGhostStayInPlace::PUBLISH_EPSILON_SQUARED)', 'if (false)'), math_header),
        'source_foot_lock_fights_pass': (swap(source, 'dist_vec_squared(mRoot->getWorldPosition(), mStayRootAuthored ? root_agent : presented_root) <=', 'dist_vec_squared(mRoot->getWorldPosition(), presented_root) <='), math_header),
        # readers of where a clone stands
        'arrival_facing_reads_presented_root': (swap(source, 'LLVector3 d = tgt->getPlacedRootPosition() - agent;', 'LLVector3 d = tgt->getRootJoint()->getWorldPosition() - agent;'), math_header),
        'heading_preview_reads_presented_root': (swap(source, 'LLVector3 origin = av->getPlacedRootPosition();', 'LLVector3 origin = av->getRootJoint()->getWorldPosition();'), math_header),
        'rig_bisector_reads_presented_root': (swap(source, 'LLVector3 foot = source->getPlacedRootPosition();', 'LLVector3 foot = source->getRootJoint()->getWorldPosition();'), math_header),
        'stepped_restore_verify_reads_presented_root': (swap(source, 'if (root && LLGhostAvatar::hasStayInPlaceAvatars()) LLGhostAvatar::steppedVerifyRoot(avatar, verify_root);', '(void)avatar;'), math_header),
        'slam_drops_offset': (swap(source, 'const LLVector3 presented = pos_agent + mStayOffset;', 'const LLVector3 presented = pos_agent;'), math_header),
        'base_root_reads_presented': (swap(source, 'static_cast<const LLGhostAvatar*>(this)->getStayInPlaceAuthoredRoot(pos);', '(void)0;'), math_header),
        'floor_hold_captures_presented': (swap(source, 'mReflectFloorRootHold.capture(mStayInPlace && mStayAuthoredValid ? mStayAuthoredRootGlobal :', 'mReflectFloorRootHold.capture(false ? mStayAuthoredRootGlobal :'), math_header),
        'foot_lock_republishes': (swap(source, '        presented_root += mStayOffset;\n', ''), math_header),
        'slow_walk_skips_mover_root': (swap(source, '    if (!mStayInPlace &&\n        dist_vec_squared(getPositionAgent(), root_agent) <=', '    if (dist_vec_squared(getPositionAgent(), root_agent) <='), math_header),
        'object_left_behind': (swap(source, '    const LLVector3 object_agent = authored + offset;', '    const LLVector3 object_agent = authored;'), math_header),
        'held_pose_republishes': (swap(source, '        return; // a held pose: nothing to publish\n', ''), math_header),
        # frame order and Stepped
        'measured_before_final_pose': (swap(source, 'postObjectList(ghosts);\n    ' + main_hook, main_hook + '\n    postObjectList(ghosts);'), math_header),
        'main_point_missing': (swap(source, 'postObjectList(ghosts);\n    ' + main_hook, 'postObjectList(ghosts);'), math_header),
        'stepped_captures_presented_root': (swap(source, scoped(step_begin), ''), math_header),
        'stepped_held_pelvis_not_presented': (swap(source, scoped(step_present), ''), math_header),
        'verify_expects_unpresented_clone': (swap(source, '        ghost->captureStepAnimRenderState();\n', ''), math_header),
        'verify_expects_unpresented_animesh': (swap(source, 'if (worn && !worn->isDead()) worn->captureStepAnimRenderState();', '(void)worn;'), math_header),
        'verify_recapture_ignores_setting': (swap(source, 'LLGhostAvatar::presentStayInPlaceStepped(stay_verify);', 'LLGhostAvatar::presentStayInPlaceStepped(!stay_verify);'), math_header),
        'stale_stepped_window_kept': (swap(source, '        if (ghost->mStayRootAuthored) ghost->endStayInPlacePostPass();\n        ghost->mStaySteppedWindow = false;\n', '        if (ghost->mStayRootAuthored) ghost->endStayInPlacePostPass();\n'), math_header),
        'stepped_restore_not_mirrored': (swap(source, scoped(step_end), ''), math_header),
        'stepped_ignores_held_root': (swap(source, 'ghost->presentStayInPlace(ghost->measureStayInPlaceOffset(), stepped_root);', '(void)stepped_root; ghost->applyStayInPlace();'), math_header),
        # lifetime and history
        'delete_keeps_registry': (swap(source, 'void markForDeath() {\n        clearStayInPlace();', 'void markForDeath() {\n        '), math_header),
        'respawn_loses_switch': (swap(source, '    setStayInPlace(inst && inst->mStayInPlace);\n', '    (void)inst;\n'), math_header),
        'no_history': (swap(source, 'if (before->empty()) return;', 'if (!before->empty()) return;'), math_header),
        'undo_follows_selection': (swap(source, 'ALGhostStudio::instance().setInstanceStayInPlace(saved.first, redo ? enabled : saved.second);', 'for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().setInstanceStayInPlace(id, redo ? enabled : saved.second);').replace('[before, enabled](bool redo, std::string&)', '[this, before, enabled](bool redo, std::string&)'), math_header),
        # the skeleton
        'pelvis_clamped': (swap(source, '    mStayOffset = offset;\n', '    mStayOffset = offset;\n    mPelvisp->setPosition(LLVector3(0.f, 0.f, mPelvisp->getPosition().mV[VZ]), false);\n'), math_header),
    }
    for name, (bad_source, bad_header) in negatives.items():
        assert (bad_source, bad_header) != (source, math_header), name
        (directory / 'alghoststayinplace.h').write_text(bad_header, encoding='utf-8')
        try:
            result = compile_run(directory, bad_source, name=name, flags=flags)
            failed = result.returncode != 0
        except RuntimeError:
            raise AssertionError(f'negative does not compile (it must fail at run time): {name}')
        assert failed, f'negative survived: {name}'
        print(f'PASS planted negative rejected: {name}', flush=True)
    (directory / 'alghoststayinplace.h').write_text(math_header, encoding='utf-8')
    (directory / 'results.json').write_text(json.dumps(
        {'production': 'PASS', 'negatives_rejected': list(negatives)}, indent=2) + '\n', encoding='utf-8')
    print(f'PASS clone_stay_in_place_check: production fixture + {len(negatives)} native negatives', flush=True)


if __name__ == '__main__':
    check()
