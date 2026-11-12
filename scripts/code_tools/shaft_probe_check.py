"""Compile actual shaft probe/menu/GL transaction and native slot recovery with mocked viewer/GL boundaries.

The fixture uses production bodies verbatim. Strict GL rejects the default-FBO
multi-draw restore regression. Planted regressions must compile and fail at runtime.
This is native fixture evidence, not an in-viewer or real-driver probe exercise.
"""
from pathlib import Path
import re
from native_check_tools import compile_run
ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
OUT = ROOT / '.tmp/shaftfix/probe-native'


def block(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    at = brace + 1
    while depth:
        if source[at] == '{': depth += 1
        if source[at] == '}': depth -= 1
        at += 1
    return source[start:at]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    pipe = (NV / 'pipeline.cpp').read_text(encoding='utf-8')
    menu = (NV / 'alviewermenu.cpp').read_text(encoding='utf-8')
    probe = (NV / 'alshaftselftest.cpp').read_text(encoding='utf-8')
    probe = re.sub(r'^#include[^\n]*\n', '', probe, flags=re.M)
    gl = (NV / 'alshaftglstate.h').read_text(encoding='utf-8')
    # Actual menu actions and checkmark, with only enable/picker APIs excluded.
    action = 'bool check_object_volumetric_shaft();\n' + '\n'.join(block(menu, sig) for sig in (
        'void sync_hero_with_shaft(', 'void handle_object_volumetric_shaft(',
        'bool check_object_volumetric_shaft()\n'))
    at = pipe.index('{ //for each current shadow')
    body = pipe[pipe.rfind('for (U32 i = 0; i < num_spots; i++)', 0, at):]
    body = block(body, 'for (U32 i = 0; i < num_spots; i++)')
    template = (Path(__file__).parent / 'fixtures/shaft_probe.cpp.in').read_text(encoding='utf-8')
    # Forward declarations normally supplied by alshaftselftest.h.
    probe = 'namespace ALShaftSelfTest { void start(LLView*); void cancel(const char*); }\n' + probe
    master = pipe[pipe.index('std::map<LLUUID, ALVCamAutoVol::EnableOverride>'):pipe.index('} // anonymous namespace', pipe.index('std::map<LLUUID, ALVCamAutoVol::EnableOverride>'))]
    master += '\n'.join(block(pipe, sig) for sig in (
        'void LLPipeline::acquireShaftTestMaster(', 'bool LLPipeline::acquireVCamVolumetrics(',
        'void LLPipeline::releaseVCamVolumetrics('))
    sources = {'GL_STATE': gl, 'MASTER': master, 'PROBE': probe,
        'RETENTION': block(pipe, 'void LLPipeline::retainSpotShadowOwners()'), 'FADE': body, 'MENU': action,
        'SELECTION': re.sub(r'^#include[^\n]*\n', '', (NV / 'alshaftselection.h').read_text(encoding='utf-8'), flags=re.M)}
    def compose(parts):
        source = template
        for key, value in parts.items(): source = source.replace('@@' + key + '@@', value)
        assert '@@' not in source
        return source
    flags = ['/MT', '/Od', '/UNDEBUG', f'/I"{NV}"']
    result = compile_run(OUT, compose(sources), 'shaft_probe', flags)
    print(result.stdout, flush=True)
    assert result.returncode == 0, result.stderr
    mutations = (
        ('default_fbo_drawbuffers', 'GL_STATE', 'if (!drawFBO) glDrawBuffer(drawBuffers[0]);', 'if (!drawFBO) glDrawBuffers(4, drawBuffers.data());'),
        ('default_fbo_hardcoded_back', 'GL_STATE', 'if (!drawFBO) glDrawBuffer(drawBuffers[0]);', 'if (!drawFBO) glDrawBuffer(GL_BACK);'),
        ('lost_draw_fbo', 'GL_STATE', 'glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);', 'glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);'),
        ('lost_read_fbo', 'GL_STATE', 'glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);', 'glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);'),
        ('lost_read_buffer', 'GL_STATE', 'glReadBuffer(readBuffer);', ';'),
        ('lost_scissor_box', 'GL_STATE', 'glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);', ';'),
        ('lost_scissor_enable', 'GL_STATE', 'if (scissorEnabled) glEnable(GL_SCISSOR_TEST);', 'if (scissorEnabled) glDisable(GL_SCISSOR_TEST);'),
        ('lost_color_mask', 'GL_STATE', 'gGL.setColorMask(mask[0], mask[1], mask[2], mask[3]);', ';'),
        ('lost_pack_buffer', 'GL_STATE', 'glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);', ';'),
        ('lost_pack_row_length', 'GL_STATE', 'glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);', ';'),
        ('lost_viewport', 'GL_STATE', 'glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);', ';'),
        ('cancel_deletes_probe', 'PROBE', '// Deferred: callbacks may still have a lease on this timer-owned probe.', 'delete probe;'),
        ('history_left_valid', 'PROBE', 'gPipeline.mProjVolHistoryValid = false;', 'gPipeline.mProjVolHistoryValid = true;'),
        ('temporary_master_changes_preference', 'MASTER', 'if (enable_master) setVCamVolumetricMaster(true);', 'if (enable_master) gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);'),
        ('master_hold_not_released', 'PROBE', 'if (holdsMaster) LLPipeline::releaseVCamVolumetrics(id);', ';'),
        ('master_raw_snapshot_restore', 'PROBE', 'if (holdsMaster) LLPipeline::releaseVCamVolumetrics(id);', 'if (holdsMaster) { LLPipeline::releaseVCamVolumetrics(id); setVCamVolumetricMaster(false); }'),
        ('master_user_edit_ignored', 'MASTER', 'sVCamVolumetricMaster.changed(value.asBoolean());', ';'),
        ('master_same_value_write_ignored', 'MASTER', 'sVCamVolumetricMaster.changed(control->getValue().asBoolean());', ';'),
        ('master_release_ignores_other_holders', 'MASTER', 'if (sVCamVolumetricMaster.release(', 'sVCamVolumetricMaster.mUsers = 1; if (sVCamVolumetricMaster.release('),
        ('hero_not_removed', 'PROBE', 'if (LLPipeline::isHeroProjector(id)) LLPipeline::toggleHeroProjector(id);', ';'),
        ('shaft_not_removed', 'PROBE', 'if (LLPipeline::isVolumetricShaftEnabled(id)) LLPipeline::toggleVolumetricShaft(id, false);', ';'),
        ('no_shadow_not_removed', 'PROBE', 'if (LLPipeline::isProjectorNoShadow(id)) LLPipeline::toggleProjectorCastShadows(id);', ';'),
        ('froxel_history_left_valid', 'PROBE', 'gPipeline.mFroxelHistoryValid = false;', 'gPipeline.mFroxelHistoryValid = true;'),
        ('override_not_removed', 'PROBE', 'LLPipeline::clearVolumetricShaftOverride(id);', ';'),
        ('menu_orphan_child', 'MENU', 'else sync_hero_with_shaft(child->getID());', 'else if (LLPipeline::isVolumetricShaftEnabled(child->getID())) sync_hero_with_shaft(child->getID());'),
    )
    for name, key, before, after in mutations:
        assert sources[key].count(before) == 1, (name, 'mutation drift')
        changed = dict(sources)
        changed[key] = changed[key].replace(before, after)
        result = compile_run(OUT, compose(changed), name, flags)
        assert result.returncode != 0, 'planted regression survived: ' + name
        print('PASS rejected planted probe regression:', name, flush=True)
    # Wiring requirements complement runtime mutants: actual renderer starts with
    # the cheap gate, every lifecycle allocation/reset entry cancels, both clients
    # use the same selected-projector helper, and inspected children reconcile.
    observer = block(pipe, 'void LLPipeline::observeProjectorShafts(')
    assert observer.index('ALShaftDecision::observe(') < observer.index('BDMergeProjectorVolumetrics ||')
    assert 'RenderShadowDetail <= 1' in observer
    for sig in ('void LLPipeline::releaseGLBuffers()', 'void LLPipeline::releaseScreenBuffers()', 'bool LLPipeline::allocateShadowBuffer('):
        assert 'ALShaftSelfTest::cancel(' in block(pipe, sig)
    for name in ('alviewermenu.cpp', 'alpanelprojectorshafts.cpp'):
        text = (NV / name).read_text(encoding='utf-8')
        assert '#include "alshaftselection.h"' in text and 'alSelectedProjector(' in text
    master = block(pipe, 'void LLPipeline::acquireShaftTestMaster(')
    assert 'acquireVCamVolumetrics(id, false);' in master
    writer = block(pipe, 'void setVCamVolumetricMaster(')
    assert 'sWritingVCamVolumetricMaster = true;' in writer and 'setValue(LLSD(enabled), false)' in writer
    print(f'PASS production observer/lifecycle/shared-selection/shared-master wiring; {len(mutations)} probe mutants rejected')
    from shaft_visibility_check import main as visibility_check
    visibility_check()


if __name__ == '__main__':
    main()
