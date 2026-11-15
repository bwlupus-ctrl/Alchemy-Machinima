"""Sized local lights: the diffuse term is continuous through the source sphere (RenderLocalLightContinuousSource).

Runs inside relight_shader_check's WGL host (run alone, it starts that host with --shape-only). Production
deferredUtil.glsl helper bodies are executed on the GPU; no lighting formula is copied here except the old
relightRatio body, which is the planted negative and the reference for the A/B switch.

  1. continuity   left/right limits at d == radius agree (several N.L, back-facing included), for the raw ratio
                  and through the production PBR and legacy diffuse paths; the centre is finite, 1/2 and the
                  limit of its neighbourhood.
  2. negative     the same checks FAIL on a program whose relightRatio is the pre-11/14 body (the step).
  3. switch       relight_continuous_source = 0 is byte-identical to that old body everywhere.
  4. scope        with the switch ON, every unsized light (radius 0, physical on or off) and every receiver
                  outside the source is byte-identical to the old body; so is the production forward evaluator
                  with the master off. --continuous-base-ref REF repeats 3 and 4 against REF's own shader text.
"""
if 'link_production' not in globals():
    import subprocess
    import sys
    from pathlib import Path
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name(
        'relight_shader_check.py')), '--shape-only', *sys.argv[1:]]))

CS_OLD_BODY = '''float relightRatio(vec3 n, vec3 lv, float radius)
{
    float d2 = dot(lv,lv);
    if (d2 < radius*radius) return 1.0; // full hemisphere, no centre singularity
    float s2 = min(1.0, radius*radius/max(d2, 1.e-12));
    float c = dot(n, relightUnit(lv,n));
    // Fully visible sphere: return N.L directly, without multiply/divide roundoff.
    if (c >= sqrt(s2)) return c;
    return relightSphere(c, s2) / (M_PI * max(s2, 1.e-20));
}'''

cs_wrapper = """
uniform vec3 csNormal;
uniform vec3 csLight;
uniform vec4 csShape;
uniform sampler2D csGloss;
out vec4 result;
void main()
{
    vec3 n=csNormal, v=vec3(0,0,1), highlight;
    result=vec4(relightRatio(n,csLight,csShape.x),
                relightPbr(vec3(.6),vec3(0),.3,0.0,n,v,csLight,15.0,.5,csShape,0.0).r,
                relightLegacy(csGloss,vec3(.6),vec4(0.0),n,v,csLight,15.0,.5,csShape,highlight).r,
                relightPbr(vec3(.6),vec3(.04),.2,0.0,n,v,csLight,15.0,.5,csShape,1.0).r);
}
"""


def cs_function(text, name):
    start = text.index(name)
    start = text.rfind('\n', 0, start)+1
    end = text.index('{', start)+1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def cs_math(text):
    return ('const float M_PI=3.14159265;\nconst float MIN_PBR_ROUGHNESS=.045;\n' +
            cs_function(text, 'float calcLegacyDistanceAttenuation(') +
            text[text.index('struct PBRInfo'):text.index('// [RigRim] ---')])


def cs_program(text, label):
    built = program()
    attach(built, compile_part(cs_math(text)+cs_wrapper, '', label))
    attach(built, vs); link(built); getprog(built, 0x8B82, C.byref(ok))
    if not ok.value:
        out = C.create_string_buffer(32000); plog(built, len(out), None, out)
        raise AssertionError(label+' '+out.value.decode())
    return built


def cs_sample(built, light, radius, switch, physical=0, normal=(0, 0, 1)):
    use(built)
    uniform2(loc(built, b'relight_limits'), .1, 4.0)
    ui(loc(built, b'csGloss'), 1)  # the host's constant nonzero LUT
    uf[1](loc(built, b'relight_continuous_source'), switch)  # location -1 (old body): ignored by GL
    uf[3](loc(built, b'csLight'), *light); uf[3](loc(built, b'csNormal'), *normal)
    uf[4](loc(built, b'csShape'), radius, physical, 128/9 if physical else 1, -1)
    draw(4, 0, 3); pixels = np.empty(4, np.float32)
    read(0, 0, 1, 1, 0x1908, 0x1406, pixels.ctypes.data)
    return pixels


CS_RADII = (.03, .1, .45)
CS_COSINES = (-.9, -.5, -.2, 0.0, .3, .5, .9)
CS_EDGE = 5.e-5   # relative distance either side of the surface
CS_TOLERANCE = .02  # the old step is (1 - N.L)/2: 0.05 at N.L 0.9, 0.95 at N.L -0.9
CS_SWEEP_TOLERANCE = .1  # largest neighbour-to-neighbour change of a sliding receiver's diffuse ratio
cs_sweeps = []  # one entry per cs_continuity() call: the largest sweep jump measured


def cs_direction(cosine):
    return np.array([np.sqrt(1-cosine*cosine), 0, cosine])


def cs_continuity(built):
    """Failures of the continuity contract on one linked program, and the largest surface step measured."""
    failures, worst, sweep = [], 0.0, 0.0
    for radius in CS_RADII:
        for cosine in CS_COSINES:
            direction = cs_direction(cosine)
            for physical in (0, 1):
                left = cs_sample(built, direction*radius*(1-CS_EDGE), radius, 1, physical)
                right = cs_sample(built, direction*radius*(1+CS_EDGE), radius, 1, physical)
                if not (np.all(np.isfinite(left)) and np.all(np.isfinite(right))):
                    failures.append(('finite at the surface', radius, cosine, physical)); continue
                step = abs(float(left[0])-float(right[0]))
                worst = max(worst, step)
                if step > CS_TOLERANCE: failures.append(('ratio step at d == radius', radius, cosine, step))
                if abs(float(left[0])-.5*(1+cosine)) > CS_TOLERANCE:
                    failures.append(('surface value is (1 + N.L)/2', radius, cosine, float(left[0])))
                # Production diffuse paths (PBR, legacy): the same limits. Each is ratio * (a factor that is
                # continuous here), so its step is bounded by the tolerance times the larger side / ratio.
                for channel, name in ((1, 'PBR diffuse'), (2, 'legacy diffuse')):
                    a, b = float(left[channel]), float(right[channel])
                    scale = max(a, b)/max(float(left[0]), float(right[0]), 1.e-6)
                    if abs(a-b) > CS_TOLERANCE*scale+1.e-6:
                        failures.append((name+' step at d == radius', radius, cosine, physical, a, b))
            # Sources smaller than the 0.1 mm core (Codex r1): the core is capped at half the radius, so
            # the surface limits still agree. A core fixed at 0.1 mm steps here (0.75 -> 1 at N.L 1).
            for tiny in (5.e-5, 1.e-5, 2.e-6, 5.e-7):  # the last two skip the inside branch (under 10 um)
                left = cs_sample(built, direction*tiny*(1-CS_EDGE), tiny, 1)
                right = cs_sample(built, direction*tiny*(1+CS_EDGE), tiny, 1)
                if not (np.all(np.isfinite(left)) and np.all(np.isfinite(right))) or \
                        abs(float(left[0])-float(right[0])) > CS_TOLERANCE:
                    failures.append(('tiny source step at d == radius', tiny, cosine, float(left[0]), float(right[0])))
            # The centre and its neighbourhood: finite, 1/2, continuous.
            centre = cs_sample(built, (0, 0, 0), radius, 1)
            if not np.all(np.isfinite(centre)) or centre[0] != .5:
                failures.append(('centre is finite and 1/2', radius, centre.tolist()))
            # The 0.1 mm core: N.L fades linearly to 0, so the term tends to 1/2 from every side.
            for metres in (1.e-9, 1.e-7, 1.e-6, 1.e-5, 5.e-5):
                near = cs_sample(built, direction*metres, radius, 1)
                if not np.all(np.isfinite(near)) or abs(float(near[0])-(.5+.5*cosine*metres/1.e-4)) > 1.e-5:
                    failures.append(('continuous toward the centre', radius, cosine, metres, near.tolist()))
            # Everywhere else inside: the half-covered hemisphere for this normal.
            for part in (.01, .3, .7, .99):
                inner = cs_sample(built, direction*radius*part, radius, 1)
                if abs(float(inner[0])-.5*(1+cosine)) > 2.e-6:
                    failures.append(('inside is (1 + N.L)/2', radius, cosine, part, float(inner[0])))
        # Temporal proxy (a sub-pixel pan): one receiver slides along a flat surface that cuts the source, in
        # steps of 2 % of the radius, physical falloff on. The largest change between neighbouring positions,
        # in the diffuse ratio (range 0..1), bounds the frame-to-frame jump of a pixel. The old body jumps by
        # (1 - N.L)/2 where the path leaves the sphere (0.2 to 0.8 on these paths); the continuous term only
        # drifts. Its steepest part (about 0.07 per step, centre behind the surface) is the unchanged
        # outside formula: a source sinking below the horizon falls off like a square root next to the sphere.
        for height in (-.5, -.3, .3, .6):
            samples = [cs_sample(built, (radius*.02*i, 0, radius*height), radius, 1, 1) for i in range(101)]
            path = [float(pixel[0]) for pixel in samples]
            jump = max(abs(a-b) for a, b in zip(path, path[1:]))
            if not np.all(np.isfinite(samples)) or jump > CS_SWEEP_TOLERANCE:
                failures.append(('sweep jump across the source surface', radius, height, jump))
            sweep = max(sweep, jump)
    cs_sweeps.append(sweep)
    return failures, worst


def cs_grid():
    """(light, radius, physical, inside the source) over unsized, inside, surface and outside receivers."""
    for radius in (0, .005, .03, .1, .45, 2.0):
        for cosine in (-1.0, -.9, -.2, 0.0, .3, 1.0):
            for distance in (0, .001, .02, .09, .3, .44, .46, 1.0, 2.5, 7.5, 14.0, 16.0):
                for physical in (0, 1):
                    light = cs_direction(cosine)*distance
                    # "inside" exactly as the shader decides it, in float32
                    lv = np.float32(light)
                    yield light, radius, physical, float(lv@lv) < float(np.float32(radius)*np.float32(radius))


cs_new = cs_program(util, 'continuous source: production')
cs_old = cs_program(util.replace(cs_function(util, 'float relightRatio('), CS_OLD_BODY, 1), 'continuous source: old body')
cs_body = cs_function(util, 'float relightRatio(')
if 'relight_continuous_source' in cs_body:
    assert loc(cs_new, b'relight_continuous_source') >= 0, 'production relightRatio reads the live switch'
    assert not any('relight_continuous_source' in line or 'CONTINUOUS_SOURCE' in line
                   for line in util.splitlines() if line.lstrip().startswith('#')), 'the switch is a uniform, never a #define'

# 1. Continuity on production.
cs_failures, cs_worst = cs_continuity(cs_new)
assert not cs_failures, ('continuous source', len(cs_failures), cs_failures[:6])
# 2. Planted negative: the pre-11/14 body must fail the same contract.
cs_negative, cs_old_worst = cs_continuity(cs_old)
assert any(item[0] == 'ratio step at d == radius' for item in cs_negative) and \
    any(item[0] == 'centre is finite and 1/2' for item in cs_negative) and \
    any(item[0] == 'PBR diffuse step at d == radius' for item in cs_negative) and \
    any(item[0] == 'legacy diffuse step at d == radius' for item in cs_negative) and \
    any(item[0] == 'sweep jump across the source surface' for item in cs_negative) and cs_old_worst > .9, \
    ('the old step must be detected', len(cs_negative), cs_old_worst)

# 3 + 4. The switch and its scope, byte for byte against the old body (and optionally a git ref).
cs_references = [('old body', cs_old)]
if '--continuous-base-ref' in sys.argv:
    cs_ref = sys.argv[sys.argv.index('--continuous-base-ref')+1]
    cs_references.append((cs_ref, cs_program(subprocess.check_output(
        ['git', 'show', cs_ref+':'+(root/'class1/deferred/deferredUtil.glsl').as_posix()], text=True), 'continuous source: '+cs_ref)))
cs_counts = {}
for cs_name, cs_reference in cs_references:
    off_checks = scope_checks = inside_changed = 0
    for light, radius, physical, inside in cs_grid():
        expected = cs_sample(cs_reference, light, radius, 1, physical)
        off = cs_sample(cs_new, light, radius, 0, physical)
        assert off.tobytes() == expected.tobytes(), ('switch off is the old value exactly', cs_name, light.tolist(), radius, physical, off, expected)
        off_checks += 1
        on = cs_sample(cs_new, light, radius, 1, physical)
        if inside:
            inside_changed += on.tobytes() != expected.tobytes()
        else:
            assert on.tobytes() == expected.tobytes(), ('unsized/outside is untouched', cs_name, light.tolist(), radius, physical, on, expected)
            scope_checks += 1
    assert inside_changed > 100, ('positive control: the switch changes receivers inside a source', inside_changed)
    cs_counts[cs_name] = (off_checks, scope_checks, inside_changed)

# Production forward evaluator (pbralphaF + its linked modules): the uniform reaches it, radius 0 and the master
# ignore it. `pr` is the runtime-master program linked above.
use(pr)
cs_live = loc(pr, b'relight_continuous_source'); assert cs_live >= 0, 'forward program links the switch'
uniform2(loc(pr, b'relight_limits'), .1, 4.0); ui(loc(pr, b'rig_rim_mode'), 0)
cs_forward = 0
for light in ((.05, 0, .02), (.2, 0, -.1), (.1, 0, .3), (1, 0, 2)):
    inside = float(np.linalg.norm(light)) < .45
    for physical in (0, 1):
        results = {}
        for master in (1, 0):
            for radius in (.45, 0):
                for switch in (1, 0):
                    ui(loc(pr, b'relight_enabled'), master); uf[1](cs_live, switch)
                    results[master, radius, switch] = sample(light, (radius, physical, 128/9 if physical else 1, -1), 1).tobytes()
        assert results[1, 0, 1] == results[1, 0, 0], ('forward unsized ignores the switch', light, physical)
        assert results[0, .45, 1] == results[0, .45, 0] == results[0, 0, 1] == results[0, 0, 0], ('forward master off ignores the switch', light, physical)
        assert (results[1, .45, 1] != results[1, .45, 0]) == inside, ('forward: the switch acts inside a source only', light, physical)
        cs_forward += 1
uf[1](cs_live, 0); ui(loc(pr, b'relight_enabled'), 1)
print('[RelightQ] continuous source PASS: worst surface step %.5f (tolerance %.2f; old body %.3f, %d failures on the planted step); '
      'sliding receiver ratio jump %.3f (tolerance %.2f; old body %.3f); '
      'switch off byte-identical and unsized/outside byte-identical vs %s; %d forward production cases'
      % (cs_worst, CS_TOLERANCE, cs_old_worst, len(cs_negative), cs_sweeps[0], CS_SWEEP_TOLERANCE, cs_sweeps[1],
         ', '.join('%s (%d off, %d untouched, %d inside changed)' % ((name,)+cs_counts[name]) for name in cs_counts), cs_forward), flush=True)
