"""Source wiring checks complementing the real-GL alpha fixtures.

Run with PYTHONUTF8=1. --ref reads committed inputs without moving HEAD.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ref')
    args = ap.parse_args()

    def read(path):
        if args.ref:
            return subprocess.check_output(['git', 'show', args.ref+':'+path],
                                           cwd=ROOT, encoding='utf-8')
        return (ROOT/path).read_text(encoding='utf-8')

    def objects(value):
        if isinstance(value, dict):
            yield value
            for item in value.values():
                yield from objects(item)
        elif isinstance(value, list):
            for item in value:
                yield from objects(item)

    data = json.loads(read('scripts/code_tools/director_kit/director_mockup_data.json'))
    rows = [row for row in objects(data) if row.get('s') == 'BDMergeAlphaAttachmentSort']
    assert len(rows) == 1
    row = rows[0]
    help_text = row['h']
    checks = {
        'sort row exposes legacy dependency': '(legacy)' in row['l'] and
            all(s in help_text for s in ('RenderInterleavedAlpha', 'on by default',
                                         'shared Actor FX', 'no effect')) and
            'Off = stock order' not in help_text,
        'sort setting remains default off': row['d'] is False,
    }
    alpha = read('indra/newview/lldrawpoolalpha.cpp')
    checks['simple alpha rim receives the oriented normal'] = (
        'rigRimForwardPixel(norm, norm, rim_fw)' in read(
            'indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl'))
    checks['particle flag uploaded for each beauty draw'] = (
        'uniform1i(sParticleNormal, is_particle_or_hud_particle ? 1 : 0)' in alpha)
    checks['material velocity uses beauty family and material inputs'] = all(s in alpha for s in (
        'bind_program(gVelocityMaterialProgram[params.mShaderMask], true)',
        'uniform1f(sVelocityAlphaCutoff, VELOCITY_ALPHA_CUTOFF)',
        'shader->bindTexture(LLShaderMgr::BUMP_MAP, params.mNormalMap',
        'shader->bindTexture(LLShaderMgr::SPECULAR_MAP, params.mSpecularMap'))
    velocity = alpha.split('void LLDrawPoolAlpha::renderVelocity(S32 pass)', 1)[1].split(
        'void LLDrawPoolAlpha::renderVelocityGroups', 1)[0]
    checks['material velocity enables forward lights and beauty view uniforms'] = all(s in velocity for s in (
        'gPipeline.enableLightsDynamic();',
        'prepare_alpha_shader(&gVelocityMaterialProgram[mask], true, water_sign)'))

    # Every fullbright IS_ALPHA program is atmospheric, including rigged
    # variants inherited by make_rigged_variant. Atmospheric draws sync the
    # sun-up flag through LLRender and receive both eye-space directions from
    # the common deferred bind. HUD bypasses the IS_ALPHA atmospheric block.
    manager = read('indra/newview/llviewershadermgr.cpp')
    names = ('gDeferredFullbrightAlphaMaskAlphaProgram',
             'gHUDFullbrightAlphaMaskAlphaProgram')
    for name in names:
        start = manager.index(name+'.mName =')
        end = manager.index('llassert(success);', start)
        block = manager[start:end]
        checks[name+' atmospheric binding'] = all(s in block for s in (
            name+'.mFeatures.calculatesAtmospherics = true;',
            name+'.addPermutation("IS_ALPHA", "1")'))
    render = read('indra/llrender/llrender.cpp')
    pipeline = read('indra/newview/pipeline.cpp')
    checks['fullbright shared sun uniforms'] = all(s in render for s in (
        'shader->mFeatures.calculatesAtmospherics)', 'syncLightState();',
        'uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_primary[0] ? 1 : 0)')) and all(
        s in pipeline for s in ('uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR',
                               'uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR'))
    for name, passed in checks.items():
        print(('PASS ' if passed else 'FAIL ')+name)
    return not all(checks.values())


if __name__ == '__main__':
    raise SystemExit(main())
