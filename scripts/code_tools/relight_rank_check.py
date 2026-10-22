"""Importance-ranking + stack-review round gates (10/22). Imported by relight_area_check; keep in the commit.

CPU side (no GPU): the selection source is the importance score with a continuous native weight,
the comparator ignores the native map, no allocation backoff exists, and the superseded forms are
rejected. GPU side, on the production gather (and, for the last block, the production spot-light
composition):

* housing is the committed GLOBAL light-bias sphere (RenderAreaShadowLightBias) and end gap
  floor + bias, byte-identical to HEAD (per-light housing shapes were removed: each one erased real
  blockers or left a seam). Regressions: the housed wall lamp keeps its pool, a big rig softbox
  (source radius counted once) still casts soft shadows, and independent blockers inside a large
  room prim shadow receivers both near the light and 3+ m from it (Codex's room-prim repro,
  4/8, 8/12 and 16/64 rays/steps). A LARGE emitting prim shadowing its own light is a documented
  limitation: the user raises Light bias;
* the native hand-over: the mapped light's screen term takes over from the native map (u .25 .. .45)
  before the native sampler's own fade-to-lit starts at u = .5, continuous across the cutoff, evaluated at the depth the
  native sampler itself compares (the receiver moved along its normal by spot_shadow_offset), and an
  independent combined-output oracle sweeps the whole transition through the production gather, the
  production bilateral filter (radius 2 = the default and 8 = the maximum) AND the production spot-light
  composition (min(native + fade, screen)) for every combination of who sees the blocker, receiver
  offsets 0 / 0.04 / 0.15 m and tilted normals.
Run directly it re-launches the GPU harness with --area-only --rank-only.
"""
from pathlib import Path
import subprocess
import sys


# Mirror of ALAreaShadow::kHandoverStart / kHandoverEnd; check_rank_sources pins them to the header AND the shader.
HANDOVER_START, HANDOVER_END = 0.25, 0.45


def native_strength(depth, clip_z, clip_w):
    """Mirror of ALAreaShadow::nativeShadowStrength (the C++ test pins the same literals)."""
    if not (clip_z > 0 and clip_w > clip_z) or depth != depth or depth in (float('inf'), float('-inf')):
        return 1.0
    u = (depth - clip_z) / (clip_w - clip_z)
    return 1.0 - min(max((u - HANDOVER_START) / (HANDOVER_END - HANDOVER_START), 0.0), 1.0)


def old_native_strength(depth, clip_z, clip_w):
    """The superseded hand-over: the screen term took over WITH the native ramp (u = .5 .. 1)."""
    if not (clip_z > 0 and clip_w > clip_z):
        return 1.0
    u = (depth - clip_z) / (clip_w - clip_z)
    return 1.0 - min(max(2.0 * u - 1.0, 0.0), 1.0)


def native_model(pcf, depth, clip_z, clip_w, blend=.25):
    """Independent model of shadowUtil.glsl sampleSpotShadow (ramp to lit and the /weight term)."""
    if not depth < clip_w:
        return 1.0
    u = (depth - clip_z) / (clip_w - clip_z)
    add = max(2.0 * u - 1.0, 0.0)
    weight = 1.0 - max(clip_z * (1.0 + blend) - depth, 0.0) / (2.0 * blend * clip_z)
    return (pcf * weight + add) / weight if abs(weight) > 1.e-9 else pcf


def link_mutant(g, replacements, label):
    """Link the production gather with the given (old, new) text replacements (each must match once)."""
    gather_path = Path('indra/newview/app_settings/shaders/class3/deferred/areaShadowF.glsl')
    original = g['read_shader']
    text = original(gather_path)
    for old, new in replacements:
        assert text.count(old) == 1, ('mutation anchor', old)
        text = text.replace(old, new)
    try:
        g['read_shader'] = lambda path: text if Path(path) == gather_path else original(path)
        return g['link_production']('class3/deferred/areaShadowF.glsl', '', label)
    finally:
        g['read_shader'] = original


def encode_normal(n):
    """Inverse of decodeNormal (globalF.glsl): octahedral, xy in [0,1]."""
    import numpy as np
    n = np.array(n, np.float64)
    n = n / np.abs(n).sum()
    p = n[:2].copy()
    if n[2] < 0:
        p = np.array([(1 - abs(p[1])) * (1.0 if p[0] >= 0 else -1.0), (1 - abs(p[0])) * (1.0 if p[1] >= 0 else -1.0)])
    return p * .5 + .5


def decode_normal(e):
    """Mirror of decodeNormal in globalF.glsl."""
    import numpy as np
    fx, fy = e[0] * 2 - 1, e[1] * 2 - 1
    x, y, z = fx, fy, 1 - abs(fx) - abs(fy)
    t = min(max(-z, 0.0), 1.0)
    x += -t if x >= 0 else t
    y += -t if y >= 0 else t
    v = np.array([x, y, z])
    return v / np.linalg.norm(v)


def check_rank_sources():
    """Static source checks, each with a rejected mutation (no viewer, no GPU)."""
    nv = Path('indra/newview')
    from relight_area_policy_check import method
    pipeline = method(nv / 'pipeline.cpp', 'void LLPipeline::renderAreaShadows(').replace('\r\n', '\n')
    pipeline_all = (nv / 'pipeline.cpp').read_text(encoding='utf-8')
    pipeline_h = (nv / 'pipeline.h').read_text(encoding='utf-8')
    header = (nv / 'alareashadowmath.h').read_text(encoding='utf-8')

    def ranking_ok(render, math):
        select = render[render.index('    ALAreaShadow::Selection<LLUUID> selected;'):render.index('    take.selected = selected;')]
        before = math[math.index('inline bool before('):math.index('template<class ID> struct Selection')] \
            if 'inline bool before(' in math else math[math.index('bool before('):math.index('struct Selection')]
        return ('ALAreaShadow::importance(luminance,range,distance,native_weight)' in select
                and 'llclamp(native_fade,0.f,1.f) *' in select
                and 'ALAreaShadow::nativeShadowStrength(view_depth,selection_clip.mV[2],selection_clip.mV[3])' in select
                and 'native_fade = mSpotLightFade[i];\n                    break;' in select
                and 'ALRelight::legacy(' not in select and 'contribution(' not in select
                and 'shadowMap' not in before and 'unshadowed first' not in render
                and 'spotShadowMapSampled(i)' in select)

    assert ranking_ok(pipeline, header), 'selection must rank by importance only'
    mutations = {
        'legacy camera-point score': pipeline.replace(
            'ALAreaShadow::importance(luminance,range,distance,native_weight)',
            'ALAreaShadow::importance(luminance,range,distance,native_weight)*ALRelight::legacy(distance,range,.5)'),
        'old contribution score': pipeline.replace(
            'ALAreaShadow::importance(luminance,range,distance,native_weight)',
            'ALAreaShadow::contribution(range,distance,luminance,1.0)'),
        'stale mapped slot': pipeline.replace(' && spotShadowMapSampled(i)', ''),
        'binary unmapped bonus (ignores the slot fade)': pipeline.replace(
            'llclamp(native_fade,0.f,1.f) *', '1.0 *'),
        'slot loop keeps scanning after the owner (no break)': pipeline.replace(
            'native_fade = mSpotLightFade[i];\n                    break;', 'native_fade = mSpotLightFade[i];'),
    }
    for label, mutated in mutations.items():
        assert mutated != pipeline, label
        assert not ranking_ok(mutated, header), 'mutation survived: ' + label
    unmapped_first = header.replace('    if (a.score != b.score) return a.score > b.score;',
                                    '    if (a.shadowMap != b.shadowMap) return !a.shadowMap;\n    if (a.score != b.score) return a.score > b.score;', 1)
    assert unmapped_first != header and not ranking_ok(pipeline, unmapped_first), 'unmapped-first mutation survived'
    # Take records keep the per-light score (bounded: at most eight selected lights).
    record = (nv / 'alrelighttakerecord.h').read_text(encoding='utf-8')
    assert 'light["score"] = state.selected.lights[channel].score;' in record
    assert 'out["frame"] = static_cast<int>(state.frame);' in record
    # The upload plumbing points reach the gather.
    assert 'area_native_clip' in pipeline and 'nativeSpotShadowClip()' in pipeline
    assert 'const unsigned feed_slots = feeds() ? LLPrismLens::MAX_CAPTURES : 0;' in pipeline
    assert 'selected.size,budget(),static_cast<unsigned>(llclamp(cap(),1,8)),feed_slots)' in pipeline
    # No rendering may depend on failure history: no backoff type, member or retry window anywhere.
    for text in ('AllocBackoff', 'kAllocRetryFrames', 'mAreaAllocBackoff', 'allocation_backoff'):
        assert text not in pipeline_all and text not in header and text not in pipeline_h, \
            'allocation backoff must stay removed: ' + text
    assert 'ALAreaShadow::drawablePlanes(planes_wanted,plane_ok[0],plane_ok[1])' in pipeline
    assert 'mRT->areaAllocFailed' in pipeline and 'bool areaAllocFailed = false;' in pipeline_h
    # Housing is HEAD's: the global light-bias sphere and end gap floor + bias. No per-light housing of any
    # shape (bounding sphere, oriented box, capped box) survives anywhere, in any file.
    glsl_dir = nv / 'app_settings/shaders'
    glsl = (glsl_dir / 'class3/deferred/areaShadowF.glsl').read_text(encoding='utf-8')
    trace = (glsl_dir / 'class1/deferred/screenSpaceRayUtil.glsl').read_text(encoding='utf-8')
    housing_words = ('area_housing_radius', 'housingRadius', 'kHousingCap', '"area_housing"', 'area_box', 'areaBoxEndGap',
                     'areaInHousing', 'box_centre', 'box_axes', 'box_half', 'HousingBox', 'housingBox', 'housingForRay',
                     'housingEndGap', 'insideExpandedBox', 'insideHousing', 'kHousingMaxHalf', 'area_housing_box')
    for word in housing_words:
        for label, body in (('pipeline.cpp', pipeline_all), ('alareashadowmath.h', header), ('areaShadowF.glsl', glsl),
                            ('screenSpaceRayUtil.glsl', trace), ('pipeline.h', pipeline_h)):
            assert word not in body, 'per-light housing must stay removed: %s in %s' % (word, label)
    # The ray utility is byte-identical to HEAD, and the gather keeps HEAD's housing arithmetic verbatim.
    def head_text(path):
        return subprocess.check_output(['git', 'show', 'HEAD:' + path.as_posix()]).decode('utf-8').replace('\r\n', '\n')
    assert trace.replace('\r\n', '\n') == head_text(glsl_dir / 'class1/deferred/screenSpaceRayUtil.glsl'), \
        'screenSpaceRayUtil.glsl must be byte-identical to HEAD'
    head_gather = head_text(glsl_dir / 'class3/deferred/areaShadowF.glsl')
    housing_lines = ('            float end_gap = max(source_radius,0.1)+area_light_bias;\n'
                     '            vec2 traced = areaTrace(origin,target,area_lights[light].xyz,end_gap,\n'
                     '                area_light_bias,area_params.x,area_params.y,area_steps,offset,area_contact_dist[light]);\n')
    assert housing_lines in head_gather and housing_lines in glsl.replace('\r\n', '\n'), 'gather housing must be HEAD\'s'
    # The hand-over is evaluated at the native sampler's shifted receiver depth, from the same offset.
    assert 'areaNativeStrength(-(pos.z+normal.z*area_native_offset))' in glsl and 'uniform float area_native_offset;' in glsl
    assert 'LLStaticHashedString("area_native_offset")' in pipeline and 'RenderSpotShadowOffset' in pipeline
    # The hand-over window: header constants, shader slope, this file's mirror and the shader's NaN guard agree.
    assert 'constexpr double kHandoverStart = 0.25, kHandoverEnd = 0.45;' in header
    assert 'return 1.0-clamp((u-0.25)*5.0,0.0,1.0);' in glsl and (HANDOVER_START, HANDOVER_END) == (0.25, 0.45)
    assert 'if (isnan(depth) || isinf(depth)) return 1.0;' in glsl
    # Plane 0 failing leaves an allocated plane 1 alone.
    assert 'if (!plane_ok[0]) break;' in pipeline and 'release_plane(1)' not in pipeline
    for label, mutated_header, mutated_glsl, mutated_pipeline in (
            ('hand-over end back at .5', header, glsl.replace('(u-0.25)*5.0', '(u-0.25)*4.0'), pipeline),
            ('NaN guard dropped', header, glsl.replace('    if (isnan(depth) || isinf(depth)) return 1.0;\n', ''), pipeline),
            ('header end back at .5', header.replace('kHandoverEnd = 0.45', 'kHandoverEnd = 0.5'), glsl, pipeline),
            ('per-light housing back', header + 'inline bool insideHousing(){return false;}', glsl, pipeline),
            ('housing uniform back', header, glsl + 'uniform vec4 area_box[16];', pipeline),
            ('shifted depth dropped', header, glsl.replace('areaNativeStrength(-(pos.z+normal.z*area_native_offset))',
                                                           'areaNativeStrength(-pos.z)'), pipeline),
            ('offset upload dropped', header, glsl, pipeline.replace('LLStaticHashedString("area_native_offset")', 'LLStaticHashedString("x")')),
            ('plane 1 freed on plane 0 failure', header, glsl, pipeline.replace('if (!plane_ok[0]) break;', 'if (!plane_ok[0]) { release_plane(1); break; }'))):
        ok = ('constexpr double kHandoverStart = 0.25, kHandoverEnd = 0.45;' in mutated_header
              and 'return 1.0-clamp((u-0.25)*5.0,0.0,1.0);' in mutated_glsl
              and 'if (isnan(depth) || isinf(depth)) return 1.0;' in mutated_glsl
              and not any(word in mutated_header or word in mutated_glsl for word in ('insideHousing', 'area_box'))
              and 'areaNativeStrength(-(pos.z+normal.z*area_native_offset))' in mutated_glsl
              and 'LLStaticHashedString("area_native_offset")' in mutated_pipeline
              and 'if (!plane_ok[0]) break;' in mutated_pipeline and 'release_plane(1)' not in mutated_pipeline)
        assert not ok, 'source mutation survived: ' + label
    print('[RelightQ] rank sources PASS: importance score with a continuous native weight, no unmapped-first or legacy '
          'camera-point term, sampled-map rule, feed slots 0 with the VCam switch off, no allocation backoff, '
          'plane-1 fallback, HEAD housing (global bias sphere; ray utility byte-identical to HEAD), hand-over window .25 .. .45 '
          'with the NaN guard, shifted-depth hand-over, plane 0 failure keeps plane 1; mutations rejected', flush=True)


def check_rank(g):
    check_rank_sources()
    np, C, F, U = (g[n] for n in ('np', 'C', 'F', 'U'))
    loc, ui, uf = (g[n] for n in ('loc', 'ui', 'uf'))
    pr = g['area_gather']
    size = 32
    projection = np.eye(4, dtype=np.float32)
    projection[2, 2] = -.1
    inverse = np.eye(4, dtype=np.float32)
    inverse[2, 2] = -10
    coords = (np.arange(size) + .5) / size * 2 - 1
    xx, yy = np.meshgrid(coords, coords)
    normal = np.zeros((size, size, 4), np.float32)
    normal[:, :, 0:2] = .5                      # encoded +Z
    g['viewport'](0, 0, size, size)
    output = g['area_texture'](0, None, 0x8058)
    g['bindfbo'](0x8D40, g['area_fbo'])
    g['attachment'](0x8D40, 0x8CE0, 0x0DE1, output, 0)
    assert g['status'](0x8D40) == 0x8CD5

    def depth_of(z):
        return (1 - .1 * z) / 2                 # view z -> stored depth for this orthographic fixture

    def scene(light, slab_z=-3.0, slab=None, samples=8, steps=12, count=1, wall_z=-4.0, bias=.25, program=None):
        p = program or pr
        depth = np.full((size, size, 4), depth_of(wall_z), np.float32)
        mask = (xx >= -.15) & (xx <= .05) if slab is None else slab
        depth[mask] = depth_of(slab_z)
        g['area_texture'](1, depth)
        g['area_texture'](2, normal)
        g['area_use'](p)
        for name, value in ((b'projection_matrix', projection), (b'inv_proj', inverse)):
            g['matrix'](loc(p, name), 1, 0, value.ctypes.data_as(C.POINTER(F)))
        ui(loc(p, b'depthMap'), 1)
        ui(loc(p, b'normalMap'), 2)
        ui(loc(p, b'area_count'), count)
        ui(loc(p, b'area_first'), 0)
        ui(loc(p, b'area_samples'), samples)
        ui(loc(p, b'area_steps'), steps)
        uf[3](loc(p, b'area_params'), 8, .35, 1)
        uf[2](loc(p, b'area_trace_res'), size, size)
        uf[1](loc(p, b'area_light_bias'), bias)
        uf[4](loc(p, b'area_contact_dist'), 0, 0, 0, 0)
        uf[4](loc(p, b'area_contact_fade'), 0, 0, 0, 0)
        uf[2](loc(p, b'area_native_clip'), 0, 0)
        uf[1](loc(p, b'area_native_offset'), 0)
        lights = np.array([light] * 4, np.float32)
        ids = np.array([12345, 9999, 7, 54321], np.uint32)
        g['uniform4fv'](loc(p, b'area_lights'), 4, lights.ctypes.data_as(C.POINTER(F)))
        g['uniform4uiv'](loc(p, b'area_seeds'), 1, ids.ctypes.data_as(C.POINTER(U)))

    def read():
        return g['area_read']()[:, :, 0].astype(np.float32) / 255

    lit = 1 - 1.5 / 255

    # ---- Housing = HEAD's global light-bias sphere (no per-light shape) ----------------------------
    # Per-light housing (bounding sphere, oriented box, capped box) was removed: each form erased real
    # blockers or left a seam. A large emitting prim shadowing its own light is a documented limitation
    # (raise Light bias). These are the regressions the sphere must keep.
    # Wall at Z -4, an independent slab at Z -3 (x -.15 .. .05), lamp at (.3, 0, -2.5): the slab
    # shadows the wall for x < -.3.
    shadow = (xx < -.3) & (abs(yy) < .3)
    light = [.3, 0, -2.5, 0]
    scene(light)
    base = read()
    assert np.min(base[shadow]) < .5, 'positive control: the slab shadows the wall'
    # (1) The housed wall lamp (relight_area_check.py's original case): the pool on the wall stays lit, the
    # lamp's own casing is inside the bias sphere and never shadows it.
    scene([.8, 0, -3.95, 0], slab=(xx >= .65) & (xx <= .95) & (abs(yy) <= .15), slab_z=-3.55, count=4)
    uf[3](loc(pr, b'area_params'), 8, .35, .5)
    wall_region = (xx >= -.5) & (xx <= .5) & (abs(yy) < .3)
    lamp = read()
    assert np.min(lamp[wall_region]) >= 1 - 1 / 8 - 1 / 255, 'wall-lamp pool stays lit'
    # (2) A big rig softbox (source diameter 2 and 4 m, i.e. radius 1 .. 2) on a 3 m orbit: a synthetic
    # emitter, the source radius counted ONCE (end gap = max(radius, .1) + bias). It must keep casting a SOFT shadow.
    soft = (xx >= -.05)
    for radius in (1.0, 1.5, 2.0):
        scene([0, 0, -1.0, radius], slab_z=-3.5, slab=soft, samples=16, steps=24)
        shade = read()
        partial = np.count_nonzero((shade > .05) & (shade < .95))
        assert np.min(shade) < .65 and partial > 8 and np.max(shade) > .95, \
            ('rig softbox keeps a soft shadow', radius, float(np.min(shade)), partial)
    # (3) Large room prim (8 x 8 x 3 m, light at its centre), independent actor-sized blockers. The sphere shadows
    # receivers both NEAR the light and 3+ m from it. Codex's repro: light (1, 0, -2.5), receivers z = -4, x < -.78,
    # blocker z = -3, x = .2 .. .45; with a capped-box housing the receivers outside the cap went 0.000 -> 1.000 at
    # 4/8, 8/12 and 16/64 rays/steps. Both receiver sets must show the blocker at every one of those workloads.
    room_slab = (xx >= .2) & (xx <= .45)
    outside = (xx < -.8) & (abs(yy) < .3)
    inside_slab = (xx >= -.15) & (xx <= .05)
    inside = (xx < -.3) & (abs(yy) < .3)
    for samples, steps in ((4, 8), (8, 12), (16, 64)):
        scene([1.0, 0, -2.5, 0], slab=room_slab, samples=samples, steps=steps)
        room_outside = read()
        assert np.min(room_outside[outside]) < .05 and np.count_nonzero(room_outside[outside] < .5) >= 8, \
            ('room prim: a blocker shadows a receiver far from the light', samples, steps, float(np.min(room_outside[outside])))
        scene([.3, 0, -2.5, 0], slab=inside_slab, samples=samples, steps=steps)
        room_inside = read()
        assert np.min(room_inside[inside]) < .05 and np.count_nonzero(room_inside[inside] < .5) >= 8, \
            ('room prim: a blocker shadows a receiver near the light', samples, steps, float(np.min(room_inside[inside])))
    # Discriminating fixture: a housing as large as the superseded caps (a 1.5 m sphere here) erases the outside
    # blocker (it lies 0.74 m from the light), so these assertions would fail under any oversized housing.
    scene([1.0, 0, -2.5, 0], slab=room_slab, bias=1.5)
    erased = float(np.min(read()[outside]))
    assert erased >= lit, 'discriminating fixture: an oversized housing erases the blocker'
    scene(light)
    print('[RelightQ] housing PASS: global bias sphere only (HEAD arithmetic); wall lamp pool stays lit, rig softbox radius '
          '1/1.5/2 m on a 3 m orbit stays soft, independent blockers shadow receivers near (min %.3f) and far (min %.3f) from the '
          'light of a room prim at 4/8, 8/12 and 16/64 rays/steps (an oversized housing erases them: %.3f)' %
          (float(np.min(room_inside[inside])), float(np.min(room_outside[outside])), erased), flush=True)

    # ---- Native hand-over, gather level ---------------------------------------------------------
    # Mapped light (contact distance .15, fade 1) with a blocker ~1 m from the wall: the screen
    # shadow is beyond the contact zone, so with the native map valid only the contact term
    # remains (lit), and with no native map (fade 0) the full trace shadows the wall.
    scene(light)
    uf[4](loc(pr, b'area_contact_dist'), .15, 0, 0, 0)
    uf[4](loc(pr, b'area_contact_fade'), 1, 0, 0, 0)
    contact = read()
    uf[4](loc(pr, b'area_contact_fade'), 0, 0, 0, 0)
    full = read()
    uf[4](loc(pr, b'area_contact_fade'), 1, 0, 0, 0)
    assert np.min(contact[shadow]) >= lit and np.min(full[shadow]) < .5, \
        'positive controls: contact-only is lit, the full trace shadows'
    receiver_depth = 4.0                                        # the wall is at view Z -4
    # (a) Clip unset (GL default): the old behaviour, byte for byte.
    assert np.array_equal(read(), contact), 'unset clip keeps the contact-only result'
    # (b0) A non-finite receiver depth means strength 1.0 (the CPU isfinite rule), on both sides.
    assert native_strength(float('nan'), 2, 6) == 1.0 and native_strength(float('inf'), 2, 6) == 1.0
    # (b) Receivers well inside the native map (u <= .25): unchanged.
    uf[2](loc(pr, b'area_native_clip'), 2, 10)
    assert native_strength(receiver_depth, 2, 10) == 1.0
    assert np.array_equal(read(), contact), 'inside the native map nothing changes'
    # (c) Receivers past the hand-over (u >= .45): the full soft-shadow trace, byte for byte.
    uf[2](loc(pr, b'area_native_clip'), 2, 6)                   # u = .5
    assert native_strength(receiver_depth, 2, 6) == 0.0
    uf[2](loc(pr, b'area_native_clip'), 2, 2 + 2 / .45)          # u = .45, where the hand-over ends
    assert abs(native_strength(receiver_depth, 2, 2 + 2 / .45)) < 1.e-9
    assert np.array_equal(read(), full), 'the hand-over is complete at u = .45'
    uf[2](loc(pr, b'area_native_clip'), 2, 6)
    assert np.array_equal(read(), full), 'past the hand-over the full trace is kept'
    uf[2](loc(pr, b'area_native_clip'), 2, 3)
    assert np.array_equal(read(), full), 'beyond shadow_clip.w the full trace is kept'
    # (d) Across the hand-over: the result follows the native strength and never steps. Sweeping
    # shadow_clip.w moves the receivers through u = .15 .. .70, which spans the whole hand-over.
    previous = None
    worst = 0.0
    swept = 0
    for index in range(401):
        clip_w = 2 + (receiver_depth - 2) / (.15 + .55 * index / 400)     # u = (4-2)/(w-2)
        uf[2](loc(pr, b'area_native_clip'), 2, clip_w)
        strength = native_strength(receiver_depth, 2, clip_w)
        measured = read()
        expected = full + (contact - full) * strength
        worst = max(worst, float(np.max(abs(measured[shadow] - expected[shadow]))))
        if previous is not None:
            assert np.max(abs(measured[shadow] - previous[shadow])) <= 2.5 / 255 + 1.e-6, \
                ('no step across the hand-over', index)
        previous = measured
        swept += 1
    assert worst <= 2.0 / 255, ('visibility follows the native strength', worst)
    # (d2) The native sampler moves the receiver along its normal by spot_shadow_offset before it ramps;
    # the gather evaluates the hand-over at that SHIFTED depth. The normal here is +Z, so the shift is
    # `offset` toward the camera. A gather that ignores the offset (mutation) follows the wrong depth.
    raw = link_mutant(g, [('areaNativeStrength(-(pos.z+normal.z*area_native_offset))', 'areaNativeStrength(-pos.z)')],
                      'unshifted hand-over (mutation)')
    shifted_worst, raw_worst = 0.0, 0.0
    for offset in (.04, .15):
        uf[1](loc(pr, b'area_native_offset'), offset)
        for index in range(0, 401, 8):
            clip_w = 2 + (receiver_depth - 2) / (.15 + .55 * index / 400)
            uf[2](loc(pr, b'area_native_clip'), 2, clip_w)
            expected = full + (contact - full) * native_strength(receiver_depth - offset, 2, clip_w)
            shifted_worst = max(shifted_worst, float(np.max(abs(read()[shadow] - expected[shadow]))))
        scene(light, program=raw)
        uf[4](loc(raw, b'area_contact_dist'), .15, 0, 0, 0)
        uf[4](loc(raw, b'area_contact_fade'), 1, 0, 0, 0)
        uf[1](loc(raw, b'area_native_offset'), offset)
        for index in range(0, 401, 8):
            clip_w = 2 + (receiver_depth - 2) / (.15 + .55 * index / 400)
            uf[2](loc(raw, b'area_native_clip'), 2, clip_w)
            expected = full + (contact - full) * native_strength(receiver_depth - offset, 2, clip_w)
            raw_worst = max(raw_worst, float(np.max(abs(read()[shadow] - expected[shadow]))))
        scene(light)
        uf[4](loc(pr, b'area_contact_dist'), .15, 0, 0, 0)
        uf[4](loc(pr, b'area_contact_fade'), 1, 0, 0, 0)
    assert shifted_worst <= 2.0 / 255, ('the hand-over follows the shifted receiver depth', shifted_worst)
    assert raw_worst > 12.0 / 255, ('a gather that ignores the offset is rejected (mutation)', raw_worst)
    uf[1](loc(pr, b'area_native_offset'), 0)
    # (e) Mutations: the superseded hand-over (with the native ramp) differs from the new one inside
    # the window where it matters, and a hard switch steps; the sweep catches both.
    assert abs(old_native_strength(4.0, 2, 4.0 + 2 / 1.5) - native_strength(4.0, 2, 4.0 + 2 / 1.5)) > .3
    steps = [native_strength(receiver_depth, 2, 2 + (receiver_depth - 2) / (.15 + .55 * i / 400)) for i in range(401)]
    assert max(abs(a - b) for a, b in zip(steps, steps[1:])) < .02, 'strength itself is continuous'
    hard = [1.0 if (.15 + .55 * i / 400) < .375 else 0.0 for i in range(401)]      # a switch at the middle of the window
    assert max(abs(a - b) for a, b in zip(hard, hard[1:])) == 1.0, 'a hard cutoff would step (mutation rejected)'
    # (f) Unmapped lights are untouched by the clip (their fade is already zero).
    uf[4](loc(pr, b'area_contact_fade'), 0, 0, 0, 0)
    uf[2](loc(pr, b'area_native_clip'), 2, 3)
    assert np.array_equal(read(), full), 'unmapped light ignores the native clip'
    uf[2](loc(pr, b'area_native_clip'), 0, 0)
    uf[4](loc(pr, b'area_contact_dist'), 0, 0, 0, 0)
    print('[RelightQ] native hand-over (gather) PASS: unset/inside/past exact, %d-point sweep within %.2f/255 of the '
          'native strength, no step; unmapped lights untouched; receiver offsets .04/.15 follow the shifted depth within '
          '%.2f/255 (a gather that ignores the offset is off by %.0f/255)' % (swept, worst * 255, shifted_worst * 255,
                                                                              raw_worst * 255), flush=True)

    check_combined(g)


def check_combined(g):
    """Independent combined-output oracle: production gather -> production spot-light composition."""
    np, C, F, U = (g[n] for n in ('np', 'C', 'F', 'U'))
    loc, ui, uf = (g[n] for n in ('loc', 'ui', 'uf'))
    width, height = 256, 8
    shader_dir = Path('indra/newview/app_settings/shaders')
    gather_path = shader_dir / 'class3/deferred/areaShadowF.glsl'
    import itertools

    source = (shader_dir / 'class3/deferred/spotLightF.glsl').read_text()
    visibility = source[source.index('uniform int relight_enabled;'):source.index('out vec4 frag_color;')]
    hybrid = source[source.index('    float shadow = 1.0;'):source.index('    GBufferInfo gb =')]
    fragment = visibility + '''
out vec4 frag_color;
uniform int proj_shadow_idx;
uniform float shadow_fade;
uniform vec4 light_shape;
uniform vec2 screen_res;
vec4 getNorm(vec2 uv);
vec4 getPosition(vec2 uv);
float sampleSpotShadow(vec3 pos,vec3 norm,int index,vec2 uv);
void main() {
    vec2 tc=gl_FragCoord.xy/screen_res;
    vec3 pos=getPosition(tc).xyz;
''' + hybrid + '''
    frag_color=vec4(shadow);
}
'''
    compose = g['link_production']('class3/deferred/spotLightF.glsl', '', 'combined oracle: production composition',
                                   test_fragment=fragment)
    # The superseded hand-over, linked from the same gather with only its strength line changed.
    original = g['read_shader']
    current_text = original(gather_path)
    old_line = 'return 1.0-clamp((u-0.25)*5.0,0.0,1.0);'
    assert current_text.count(old_line) == 1, 'hand-over line'
    mutant_text = current_text.replace(old_line, 'return 1.0-clamp(2.0*u-1.0,0.0,1.0);')
    # The previous window (.25 .. .5): strength reaches 0 exactly where the native ramp starts, so the bilateral
    # filter leaks a few /255 of the lit side across it.
    tight_text = current_text.replace(old_line, 'return 1.0-clamp((u-0.25)*4.0,0.0,1.0);')
    try:
        g['read_shader'] = lambda path: mutant_text if Path(path) == gather_path else original(path)
        old_gather = g['link_production']('class3/deferred/areaShadowF.glsl', '', 'superseded hand-over (mutation)')
        g['read_shader'] = lambda path: tight_text if Path(path) == gather_path else original(path)
        tight_gather = g['link_production']('class3/deferred/areaShadowF.glsl', '', 'hand-over ending at u = .5 (mutation)')
    finally:
        g['read_shader'] = original
    gather = g['area_gather']
    area_filter = g['area_filter']              # the production bilateral filter program

    projection = np.eye(4, dtype=np.float32)
    projection[2, 2] = -.1
    inverse = np.eye(4, dtype=np.float32)
    inverse[2, 2] = -10
    xs = (np.arange(width) + .5) / width * 2 - 1
    z_wall = -(4 + 5.9 * np.arange(width) / (width - 1))           # receivers 4 .. 9.9 m deep
    depth_wall = (1 - .1 * z_wall) / 2
    post = np.abs(xs) <= .35                                        # a post at z -1.5 between receivers and the light
    analysis = (np.abs(xs) > .4) & (np.abs(xs) < .9)              # clear of the post and of the screen edge
    normal = np.zeros((height, width, 4), np.float32)
    normal[:, :, 0:2] = .5
    textures = {}

    def upload(unit, data, internal=0x8814):
        if unit not in textures:
            textures[unit] = g['area_texture'](unit, data, internal)
        else:
            g['active'](0x84C0 + unit)
            g['bindTex'](0x0DE1, textures[unit])
            g['teximage'](0x0DE1, 0, internal, data.shape[1], data.shape[0], 0, 0x1908, 0x1406, data.ctypes.data)

    targets = {}

    def draw(pr, probe=False):
        if 'out' not in targets:
            targets['out'] = g['area_texture'](0, np.zeros((height, width, 4), np.float32), 0x8058)
        g['active'](0x84C0)
        g['bindTex'](0x0DE1, targets['out'])
        g['attachment'](0x8D40, 0x8CE0, 0x0DE1, targets['out'], 0)
        assert g['status'](0x8D40) == 0x8CD5
        g['viewport'](0, 0, width, height)
        g['use'](pr) if probe else g['area_use'](pr)
        g['draw'](4, 0, 3)
        pixels = np.empty((height, width, 4), np.uint8)
        g['read'](0, 0, width, height, 0x1908, 0x1401, pixels.ctypes.data)
        return pixels

    # Native spot map: one texel, compare-to-texture. Value .1 = every receiver shadowed (pcf 0);
    # .9 = every receiver lit (pcf 1). All ten sampler uniforms share the unit.
    native = U()
    g['genTex'](1, C.byref(native))
    g['active'](0x84C0 + 6)
    g['bindTex'](0x0DE1, native)
    for key, value in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F),
                       (0x884C, 0x884E), (0x884D, 0x0203)):
        g['param'](0x0DE1, key, value)
    shadow_matrix = np.zeros((4, 4), np.float32)
    shadow_matrix[0, 0] = .25
    shadow_matrix[0, 3] = .5
    shadow_matrix[1, 1] = .5
    shadow_matrix[1, 3] = .5
    shadow_matrix[2, 3] = .5
    shadow_matrix[3, 3] = 1

    def set_native(value):
        g['active'](0x84C0 + 6)
        g['bindTex'](0x0DE1, native)
        texel = np.full((1, 1), value, np.float32)
        g['teximage'](0x0DE1, 0, 0x81A6, 1, 1, 0, 0x1902, 0x1406, texel.ctypes.data)

    def run_filter(pixels, radius):
        """The production filter chain (renderAreaShadows): blur x, blur y, then the full-resolution upsample."""
        current = pixels
        for axis in (((1, 0), (0, 1)) if radius > 0 else ()) + ((0, 0),):
            upload(3, current.astype(np.float32) / 255, 0x8058)
            g['area_use'](area_filter)
            g['matrix'](loc(area_filter, b'inv_proj'), 1, 0, inverse.ctypes.data_as(C.POINTER(F)))
            ui(loc(area_filter, b'depthMap'), 1)
            ui(loc(area_filter, b'normalMap'), 2)
            ui(loc(area_filter, b'area_source'), 3)
            ui(loc(area_filter, b'area_debug_channel'), 0)
            uf[2](loc(area_filter, b'area_axis'), *axis)
            uf[1](loc(area_filter, b'area_radius'), radius)
            current = draw(area_filter)
        return current

    def gather_visibility(program, clip, fade, blocker, normal_image=None, offset=0.0, radius=None):
        depth = np.tile(depth_wall.astype(np.float32)[None, :, None], (height, 1, 4))
        if blocker:
            depth[:, post, :] = (1 - .1 * -1.5) / 2
        upload(1, depth)
        upload(2, normal if normal_image is None else normal_image)
        g['area_use'](program)
        for name, value in ((b'projection_matrix', projection), (b'inv_proj', inverse)):
            g['matrix'](loc(program, name), 1, 0, value.ctypes.data_as(C.POINTER(F)))
        ui(loc(program, b'depthMap'), 1)
        ui(loc(program, b'normalMap'), 2)
        ui(loc(program, b'area_count'), 1)
        ui(loc(program, b'area_first'), 0)
        ui(loc(program, b'area_samples'), 8)
        ui(loc(program, b'area_steps'), 64)
        uf[3](loc(program, b'area_params'), 14, .35, 1)
        # A production-like receiver bias (the 2 cm floor): the footprint of a 256 x 8 trace grid would be ~0.4 m,
        # and a tilted receiver would then start its ray off screen or behind its own surface.
        uf[2](loc(program, b'area_trace_res'), width * 8, height * 32)
        uf[1](loc(program, b'area_light_bias'), .25)
        uf[4](loc(program, b'area_contact_dist'), .15, 0, 0, 0)
        uf[4](loc(program, b'area_contact_fade'), fade, 0, 0, 0)
        uf[2](loc(program, b'area_native_clip'), *clip)
        uf[1](loc(program, b'area_native_offset'), offset)
        lights = np.array([[0, 0, -1.0, 0]] * 4, np.float32)
        ids = np.array([12345, 9999, 7, 54321], np.uint32)
        g['uniform4fv'](loc(program, b'area_lights'), 4, lights.ctypes.data_as(C.POINTER(F)))
        g['uniform4uiv'](loc(program, b'area_seeds'), 1, ids.ctypes.data_as(C.POINTER(U)))
        pixels = draw(program)
        # radius None: the raw gather. Otherwise through the production bilateral filter at that radius
        # (2 = the default RenderAreaShadowDenoise, 8 = its maximum), which the lighting shaders actually sample.
        return pixels[:, :, 0] if radius is None else run_filter(pixels, radius)[:, :, 0]

    def composed(visibility_bytes, clip, fade, blend, offset=0.0):
        upload(4, np.repeat(visibility_bytes[:, :, None], 4, axis=2).astype(np.float32) / 255, 0x8058)
        g['use'](compose)
        g['matrix'](loc(compose, b'inv_proj'), 1, 0, inverse.ctypes.data_as(C.POINTER(F)))
        uf[2](loc(compose, b'screen_res'), width, height)
        ui(loc(compose, b'depthMap'), 1)
        ui(loc(compose, b'normalMap'), 2)
        ui(loc(compose, b'area_shadow_map0'), 4)
        ui(loc(compose, b'area_shadow_map1'), 4)
        for n in range(10):
            ui(loc(compose, ('shadowMap%d' % (4 + n)).encode()), 6)
        matrix = np.ascontiguousarray(shadow_matrix.T, dtype=np.float32)
        g['matrix'](loc(compose, b'shadow_matrix[4]'), 1, 0, matrix.ctypes.data_as(C.POINTER(F)))
        uf[2](loc(compose, b'proj_shadow_res'), width, height)
        uf[4](loc(compose, b'shadow_clip'), clip[0] / 4, clip[0] / 2, clip[0], clip[1])
        uf[4](loc(compose, b'light_shape'), 0, 0, 0, 0)
        uf[1](loc(compose, b'spot_shadow_bias'), 0)
        uf[1](loc(compose, b'spot_shadow_offset'), offset)
        uf[1](loc(compose, b'shadow_split_blend'), blend)
        ui(loc(compose, b'proj_shadow_idx'), 0)
        uf[1](loc(compose, b'shadow_fade'), 1 - fade)
        ui(loc(compose, b'relight_enabled'), 2)
        return draw(compose, probe=True)[:, :, 0]

    depth_columns = -z_wall
    # Receiver normals (view space) and the native sampler's receiver offsets. sampleSpotShadow moves the
    # receiver by norm * spot_shadow_offset before it ramps to lit, so the depth the native model sees is
    # depth - n.z * offset; the gather must hand over at that same shifted depth.
    normal_images = {}
    for n in ((0, 0, 1), (.7071, 0, -.7071), (.7071, 0, .7071)):
        encoded = encode_normal(n)
        unit = np.array(n, np.float64) / np.linalg.norm(n)
        assert float(np.max(abs(decode_normal(encoded) - unit))) < 1.e-6, ('normal codec', n)
        image = np.zeros((height, width, 4), np.float32)
        image[:, :, 0] = encoded[0]
        image[:, :, 1] = encoded[1]
        # Only the RECEIVER is tilted: the post is a flat z = const surface, and the trace validates its hits
        # against the normal of the surface it hits, which must stay consistent with that surface (+Z).
        image[:, post, 0:2] = .5
        normal_images[n] = (image, float(unit[2]))
    cases = 0
    sweeps = 0
    worst_shared_peak = 0.0
    peaks = {}
    narrow_peak = 0.0
    scenes = (('blocker seen by both', .1, 0.0, True),
              ('only the screen trace sees it', .9, 1.0, True),
              ('only the native map sees it', .1, 0.0, False),
              ('nothing blocks', .9, 1.0, False))
    # (4, 5) puts the receivers where the cascade-blend weight W is below 1 while the ramp is running.
    for n, (nimage, nz) in normal_images.items():
      # A facing receiver also runs with NEGATIVE offsets: n.z * offset < 0 is the deeper shift of a normal that
      # leans away, without starting the ray behind its own surface (a back-facing normal is not a visible receiver).
      for offset in ((0.0, .04, .15, -.04, -.15) if n == (0, 0, 1) else (0.0, .04, .15)):
        shifted_columns = depth_columns - nz * offset
        for clip, blend in (((5.0, 9.0), .25), ((4.5, 6.5), .1), ((4.0, 5.0), .25)):
            for fade in (1.0, .5, 0.0):
                for radius, (name, native_value, pcf, blocker) in itertools.product((None, 2.0, 8.0), scenes):
                    set_native(native_value)
                    v = gather_visibility(gather, clip, fade, blocker, nimage, offset, radius)
                    row = v[height // 2].astype(np.float32) / 255
                    final = composed(v, clip, fade, blend, offset)[height // 2].astype(np.float32) / 255
                    d = shifted_columns
                    # The screen term itself: mix(full, contact, fade * strength) with the strength taken at the
                    # SHIFTED depth; a blocker the trace sees at every receiver is full 0 / contact 1.
                    strength = np.array([native_strength(x, *clip) for x in d])
                    expected_screen = fade * strength if blocker else np.ones_like(strength)
                    error = float(np.max(abs(row[analysis] - expected_screen[analysis])))
                    worst_column = int(np.argmax(np.where(analysis, abs(row - expected_screen), 0)))
                    # The raw gather follows the strength exactly. A filtered gather is smoothed on purpose (a gaussian
                    # of up to 8 px rounds the kinks of the hand-over), so it is judged below by what it must never do
                    # (light a shared blocker, step) and by the composition arithmetic, not by the unfiltered curve.
                    if radius is None:
                        assert error <= 3 / 255, ('gather visibility follows the native strength', name, clip, fade, n, offset, error,
                                                  'column', worst_column, 'depth', float(d[worst_column]),
                                                  'measured', float(row[worst_column]), 'expected', float(expected_screen[worst_column]))
                    else:
                        # The bilateral blur is a convex combination of the neighbours it reaches (all weights are positive),
                        # so a filtered column lies within the range of the unfiltered curve over +-radius columns.
                        reach = int(radius)
                        low = np.array([expected_screen[max(0, i - reach):i + reach + 1].min() for i in range(width)])
                        high = np.array([expected_screen[max(0, i - reach):i + reach + 1].max() for i in range(width)])
                        outside = float(np.max(np.maximum(low - row, row - high)[analysis]))
                        assert outside <= 3 / 255, ('filtered visibility stays within the unfiltered neighbourhood', name, clip,
                                                   fade, n, offset, radius, outside)
                    # The composition: min(native + shadow_fade, screen), the native from the independent model.
                    native_total = np.clip(np.array([native_model(pcf, x, clip[0], clip[1], blend) for x in d]) + (1 - fade), 0, 1)
                    expected_final = np.minimum(native_total, row)
                    error = float(np.max(abs(final[analysis] - expected_final[analysis])))
                    assert error <= 3 / 255, ('production composition equals min(native + fade, screen)', name, clip, fade, n, offset, radius, error)
                    steps = np.abs(np.diff(final))[analysis[:-1] & analysis[1:]]   # neighbours only, not across the post
                    # Neighbouring columns are 5.9/255 m apart; nothing may move faster than 1.5x the hand-over
                    # (the steepest term: its width is a quarter of the clip range) plus quantization.
                    bound = 1.5 * (5.9 / 255) / ((HANDOVER_END - HANDOVER_START) * (clip[1] - clip[0])) + 4 / 255
                    assert np.max(steps) <= bound, ('final composite has no step', name, clip, fade, n, offset, radius, float(np.max(steps)), bound)
                    if pcf == 0.0 and blocker:
                        start, end = final[analysis][0], final[analysis][-1]
                        peak = float(np.max(final[analysis]))
                        assert peak <= max(start, end) + 3 / 255, \
                            ('never brighter than both endpoints for a shared blocker', clip, fade, n, offset, radius, peak, start, end)
                        if fade == 1.0:
                            # Fully faded in, both endpoints are dark: the whole transition transmits nothing.
                            # The hand-over ends 0.05 of the clip range before the native ramp; the filter reaches about three
                            # columns here (the bilateral depth weight cuts the rest), so the margin must cover that reach. It
                            # does for every realistic clip range and for the default radius; a 1 m clip range at the
                            # maximum radius 8 has only about two columns of margin and may leak 2/255 (documented).
                            allowed = 1 / 255 if (clip[1] - clip[0] >= 2.0 or (radius or 0.0) <= 2.0) else 3 / 255
                            assert peak <= allowed + 1.e-6, ('shared blocker peak through the production filter', clip, n, offset, radius, peak)
                            peaks[(clip, radius)] = max(peaks.get((clip, radius), 0.0), peak)
                            if allowed == 1 / 255: worst_shared_peak = max(worst_shared_peak, peak)
                            else: narrow_peak = max(narrow_peak, peak)
                    if pcf == 1.0 and blocker and fade == 1.0:
                        first = int(np.flatnonzero(analysis)[0])
                        near = strength[max(0, first - int(radius or 0)):first + int(radius or 0) + 1]
                        assert final[analysis][-1] <= 3 / 255 and near.min() - 3 / 255 <= final[analysis][0] <= near.max() + 3 / 255, \
                            ('past the native map the full trace decides; inside it only contact adds', clip, fade, n, offset, radius,
                             float(final[analysis][-1]), float(final[analysis][0]), float(strength[first]))
                    if pcf == 0.0 and not blocker:
                        assert final[analysis][-1] >= 1 - 3 / 255, 'a native-only blocker ramps to lit as stock'
                    cases += 1
                    sweeps += 1
    # Mutation: the superseded hand-over (screen term takes over WITH the native ramp) meets it mid-way.
    set_native(.1)
    v_old = gather_visibility(old_gather, (5.0, 9.0), 1.0, True, radius=8.0)
    final_old = composed(v_old, (5.0, 9.0), 1.0, .25)[height // 2].astype(np.float32) / 255
    assert float(np.max(final_old[analysis])) > .35, 'the superseded hand-over transmits a bright band (mutation rejected)'
    v_new = gather_visibility(gather, (5.0, 9.0), 1.0, True, radius=8.0)
    final_new = composed(v_new, (5.0, 9.0), 1.0, .25)[height // 2].astype(np.float32) / 255
    assert float(np.max(final_new[analysis])) <= 1 / 255 + 1.e-6, 'the new hand-over transmits nothing for a shared blocker'
    # Mutation: the window ending exactly where the native ramp starts (u = .5). Through the radius-8 production
    # filter it leaks a few /255 of the lit side over the shared blocker; the 0.05 margin removes it.
    tight_peaks = []
    for clip in ((5.0, 9.0), (4.5, 6.5), (4.0, 5.0)):
        v_tight = gather_visibility(tight_gather, clip, 1.0, True, radius=8.0)
        tight_peaks.append(float(np.max(composed(v_tight, clip, 1.0, .25)[height // 2].astype(np.float32)[analysis] / 255)))
    assert max(tight_peaks) > 1 / 255, ('a window ending at u = .5 leaks through the filter (mutation rejected)', tight_peaks)
    # Mutation: the hand-over taken at the UNSHIFTED depth (the previous round). A tilted receiver with the
    # default 0.04 m offset, and with the 0.15 m UI maximum, lights a shared blocker between two dark ends.
    raw_gather = link_mutant(g, [('areaNativeStrength(-(pos.z+normal.z*area_native_offset))', 'areaNativeStrength(-pos.z)')],
                             'unshifted hand-over (mutation)')
    tilted_image, _ = normal_images[(.7071, 0, -.7071)]
    set_native(.1)
    raw_peaks = []
    # The 0.05 hand-over margin heals a shift of up to 0.05 of the clip range, so the unshifted form is only wrong where the
    # shift exceeds it: the 0.04 m default needs a clip range of 0.4 m, the 0.15 m maximum is wrong even on a 1 m range.
    # (A 0.4 m range leaves the fixed gather about two columns of margin only, so it may leak 2/255 there.)
    for clip, offset, floor_peak, fixed_allowed in (((4.0, 5.0), .15, 10 / 255, 1 / 255), ((4.3, 4.7), .04, 5 / 255, 3 / 255)):
        v_raw = gather_visibility(raw_gather, clip, 1.0, True, tilted_image, offset, 2.0)
        peak_raw = float(np.max(composed(v_raw, clip, 1.0, .25, offset)[height // 2].astype(np.float32)[analysis] / 255))
        v_fix = gather_visibility(gather, clip, 1.0, True, tilted_image, offset, 2.0)
        peak_fix = float(np.max(composed(v_fix, clip, 1.0, .25, offset)[height // 2].astype(np.float32)[analysis] / 255))
        assert peak_raw > floor_peak and peak_raw > peak_fix + 2 / 255, ('the unshifted hand-over transmits a bright band (mutation rejected)', clip, offset, peak_raw)
        assert peak_fix <= fixed_allowed + 1.e-6, ('the shifted hand-over transmits nothing', clip, offset, peak_fix)
        raw_peaks.append(peak_raw)
    print('[RelightQ] shared-blocker peak per (clip, filter radius): ' + ', '.join(
        '%s r=%s: %.1f/255' % (clip, radius, peak * 255) for (clip, radius), peak in sorted(peaks.items(), key=str)), flush=True)
    print('[RelightQ] native hand-over (combined output) PASS: production gather raw and through the production bilateral filter (radius 2 and 8) + '
          'production spot-light composition, %d sweeps over 3 normals x offsets 0/.04/.15 (and -.04/-.15 facing) x 3 clips/blends x 3 fades x '
          '4 scenes x 3 filter settings (%d columns each); shared blocker peak %.4f at every offset and normal for clip ranges >= 2 m at any radius and any range at radius <= 2 (limit 1/255 = %.4f; a 1 m range at radius 8 peaks %.4f; superseded peak %.3f; a window ending '
          'at u = .5 peaks %.4f through the filter; unshifted %.3f at .15 m, %.3f at .04 m on a 0.4 m range)' %
          (cases, int(np.count_nonzero(analysis)), worst_shared_peak, 1 / 255, narrow_peak, float(np.max(final_old[analysis])),
           max(tight_peaks), raw_peaks[0], raw_peaks[1]), flush=True)
    g['area_use'](gather)
    uf[2](loc(gather, b'area_native_clip'), 0, 0)
    uf[1](loc(gather, b'area_native_offset'), 0)


if __name__ == '__main__':
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name('relight_shader_check.py')),
                                      '--area-only', '--rank-only']))
