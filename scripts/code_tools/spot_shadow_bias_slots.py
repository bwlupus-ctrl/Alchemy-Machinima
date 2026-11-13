"""Combined spot16/surface-bias readback, with independent map/matrix isolation."""
import numpy as np


def check(r, defaults, negatives=True):
    import relight_spot_bias_gl_check as B
    import relight_3d_gl as G

    source = r.visibility_source
    assert source.count(',0,tc)') == 1
    source = 'uniform int probe_slot;\n' + source.replace(',0,tc)', ',probe_slot,tc)')

    def link(tree, extended, label):
        saved = r.tree
        r.tree = tree
        try:
            return r.link_text(source, label, extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                               defines='#define SPOT_SHADOW 1\n' + ('#define SPOT_SHADOWS_16 1\n' if extended else ''))
        finally:
            r.tree = saved

    legacy = link(G.SourceTree('46ec4444fff'), False, 'reviewed bias ten-slot reference')
    extended = link(r.tree, True, 'combined sixteen-slot surface receiver')
    scene, cam, g, d, n, _, _ = B.strap_fixture()
    e = B.entry(B.R.CENTRE + np.array([3., 0, 0]), 1., False)
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    lit, dark = B.truth(scene, g, e['pos'], 4. * B.texel_at(e, 2048))
    assert min(lit.sum(), dark.sum()) >= B.LIMITS['expected_pixels']
    # All unused maps are lit, and every unused matrix projects outside the map.
    # A wrong sampler OR wrong matrix therefore loses the independently known shadow.
    poison = r._texture('slot_poison', 13, 0x8CAC, 1, 1, 0x1902, 0x1406, np.ones((1, 1), np.float32))
    gl = r.gl
    for key, value in ((0x884C, 0x884E), (0x884D, 0x0203), (0x2801, 0x2601), (0x2800, 0x2601)):
        gl.param(0x0DE1, key, value)

    def render(pr, slot, count, model, detail, soft, vogel, airborne=False, mutant=False):
        def setup(pr):
            tex, ncam, _ = r.native
            bias = np.eye(4); bias[:3, :3] *= .5; bias[:3, 3] = .5
            selected = bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view)
            # Upload all 20 matrix entries for the extended variant, including sun slots.
            for j in range(count + 4):
                matrix = np.eye(4)
                matrix[0, 3] = 100. + j
                if j == 4 + slot:
                    matrix = selected
                r._mat(pr, ('shadow_matrix[%d]' % j).encode(), matrix)
            locations = []
            for j in range(count):
                unit = 13 + j
                gl.active(0x84C0 + unit)
                gl.bind_tex(0x0DE1, tex if j == slot else poison)
                location = gl.loc(pr, ('shadowMap%d' % (4 + j)).encode())
                if not mutant:
                    assert location >= 0
                if location >= 0:
                    locations.append(location)
                gl.ui(location, unit)
                r.uniform(pr, 'spot_shadow_detail[%d]' % j, float(detail))
                r.uniform(pr, 'spot_shadow_softness[%d]' % j, 0.)
            assert len(set(locations)) == len(locations)
            gl.ui(gl.loc(pr, b'probe_slot'), slot)
        return B.visibility(r, pr, cam, d, n, e, dict(defaults, BDMergeSoftProjectorShadows=soft,
                            RenderSpotShadowDetailBias=True), model, vogel=vogel, airborne=airborne, setup=setup)

    rows, failures = [], []
    for kernel, soft, vogel in B.KERNELS:
        for model, detail, airborne in ((0, False, False), (1, False, False), (1, True, False),
                                        (2, True, False), (0, False, True), (1, False, True), (2, False, True)):
            reference = render(legacy, 0, 10, model, detail, soft, vogel, airborne)
            for slot in range(16):
                actual = render(extended, slot, 16, model, detail, soft, vogel, airborne)
                row = dict(slot=slot, matrix=4+slot, sampler='shadowMap%d' % (4+slot), model=model,
                           detail=detail, airborne=airborne, kernel=kernel,
                           reviewed_bias_bytes=actual.tobytes() == reference.tobytes(), **B.measure(actual, lit, dark))
                if not row['reviewed_bias_bytes']:
                    failures.append('slot %d %s model %d detail %s airborne %s differs from reviewed ten-slot bias' %
                                    (slot, kernel, model, detail, airborne))
                if model == 1 and not detail and not airborne and (
                    row['retention'] < B.LIMITS['retention_on'] or
                    row['mean_lit_visibility'] < B.LIMITS['mean_visibility'] or
                    row['false_dark_percent'] > B.LIMITS['false_dark_percent']):
                    failures.append('slot %d %s model 1 loses strap or acnes' % (slot, kernel))
                rows.append(row)

    prefix = 'vec4 spotSurfaceReceiver(vec3 pos, vec3 norm, int index)\n{\n    mat4 m = shadow_matrix[4+index];'
    mutants = {
        'surface_matrix_clamped_at_9': [(prefix, prefix.replace('4+index', '4+min(index,9)'))],
        'high_slots_skip_bias': [(B.GATE, B.GATE[:-1] + ' && index < 10)')],
        'high_slots_wrong_sampler': [('pcfSpotShadow(shadowMap%d, lpos,' % (4+j),
                                     'pcfSpotShadow(shadowMap4, lpos,') for j in range(10, 16)],
    }
    rejected = {}
    for name, edits in (mutants.items() if negatives else ()):
        mutant = link(B.MutantTree(edits), True, name)
        rejected[name] = []
        for slot in range(10, 16):
            actual = render(mutant, slot, 16, 1, False, False, False, mutant=True)
            caught = B.measure(actual, lit, dark)['retention'] < B.LIMITS['retention_on']
            rejected[name].append(bool(caught))
            if not caught:
                failures.append('%s survived at slot %d' % (name, slot))
        print('[spot-bias slots] rejected', name, rejected[name], flush=True)
    print('[spot-bias slots]', 'PASS' if not failures else 'FAIL', len(rows), 'readbacks', failures, flush=True)
    return dict(rows=rows, planted_negatives=rejected, failures=failures), failures
