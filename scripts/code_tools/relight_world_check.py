"""P1-lite static pins (no GPU): the face table, the constants and the shadow render-type mask have ONE meaning in the C++ header, the
GLSL resolve, the CPU raster mirror of the 3D harness and the pipeline, and section 10 of the plan (native shadow allocation, the
auction, generateSunShadow, allocateShadowBuffer) is untouched.

    py -3 -I scripts/code_tools/relight_world_check.py [--base <commit>]

Also exec'd by relight_shader_check.py (worktree runs).
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2] if '__file__' in globals() and Path(__file__).name == 'relight_world_check.py' else Path('.').resolve()
if not (REPO / 'indra').exists():
    REPO = Path('.').resolve()
BASE = sys.argv[sys.argv.index('--base') + 1] if '--base' in sys.argv else 'e150b927cf4'
NV = REPO / 'indra/newview'
GLSL = NV / 'app_settings/shaders/class3/deferred'


def text(path):
    return Path(path).read_text(encoding='utf-8')


def vec_list(s):
    return [tuple(int(float(x)) for x in m.split(',')) for m in re.findall(r'vec3\(([^)]*)\)', s)]


def check_face_tables():
    header = text(NV / 'alworldshadowmath.h')
    glsl = text(GLSL / 'areaShadowFilterF.glsl')
    # C++: kFaceTable rows {{ax,ay,az},{ux,uy,uz}}
    rows = re.findall(r'\{\{\s*(-?\d),\s*(-?\d),\s*(-?\d)\},\s*\{\s*(-?\d),\s*(-?\d),\s*(-?\d)\}\}', header)
    assert len(rows) == 6, 'kFaceTable rows'
    axis = [tuple(map(int, r[:3])) for r in rows]
    up_in = [tuple(map(int, r[3:])) for r in rows]
    # derive right/up' exactly as glm::lookAt does
    def cross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    right = [cross(a, u) for a, u in zip(axis, up_in)]
    up = [cross(r, a) for r, a in zip(right, axis)]
    g_axis = vec_list(re.search(r'AW_AXIS\[6\]\s*=\s*vec3\[6\]\((.*?)\);', glsl, re.S).group(1))
    g_right = vec_list(re.search(r'AW_RIGHT\[6\]\s*=\s*vec3\[6\]\((.*?)\);', glsl, re.S).group(1))
    g_up = vec_list(re.search(r'AW_UP\[6\]\s*=\s*vec3\[6\]\((.*?)\);', glsl, re.S).group(1))
    assert g_axis == axis, ('GLSL axis table', g_axis, axis)
    assert g_right == right, ('GLSL right table', g_right, right)
    assert g_up == up, ('GLSL up table', g_up, up)
    # CPU raster mirror of the harness
    sys.path.insert(0, str(REPO / 'scripts/code_tools'))
    import numpy as np
    src = text(REPO / 'scripts/code_tools/relight_3d_cube_source.py')
    py_axis = [tuple(int(x) for x in m.split(',')) for m in re.findall(r'\((-?\d, -?\d, -?\d)\)', re.search(r'FACE_AXIS = \[(.*?)\]\nFACE_UP_IN', src, re.S).group(1))]
    py_up = [tuple(int(x) for x in m.split(',')) for m in re.findall(r'\((-?\d, -?\d, -?\d)\)', re.search(r'FACE_UP_IN = \[(.*?)\]\n', src, re.S).group(1))]
    assert py_axis == axis and py_up == up_in, 'harness face table'
    print('[world] face table PASS: C++ kFaceTable == GLSL AW_AXIS/AW_RIGHT/AW_UP == harness mirror (right = axis x up, up = right x axis)', flush=True)


def check_constants():
    header = text(NV / 'alworldshadowmath.h')
    glsl = text(GLSL / 'areaShadowFilterF.glsl')
    pipe = text(NV / 'pipeline.cpp')
    def hc(name):
        return float(re.search(r'constexpr (?:unsigned|double|std::uint64_t) [^;]*\b' + name + r'\s*=\s*([0-9.]+)', header).group(1))
    usable, gutter = hc('kUsable'), hc('kGutter')
    tile = usable + 2 * gutter
    assert float(re.search(r'AW_TILE = ([0-9.]+)', glsl).group(1)) == tile
    assert float(re.search(r'AW_HALF_TILE = ([0-9.]+)', glsl).group(1)) == tile / 2
    assert float(re.search(r'AW_HALF_USABLE = ([0-9.]+)', glsl).group(1)) == usable / 2
    assert float(re.search(r'AW_USABLE = ([0-9.]+)', glsl).group(1)) == usable
    assert float(re.search(r'AW_MAX_RADIUS = ([0-9.]+)', glsl).group(1)) + 2 <= gutter, 'kernel + bilinear + slope must stay inside the gutter'
    m = re.search(r'AW_NORMAL_TEXELS = ([0-9.]+), AW_NORMAL_CAP = ([0-9.]+), AW_BIAS_CONST = ([0-9.]+), AW_BIAS_SLOPE = ([0-9.]+), AW_TAN_CAP = ([0-9.]+)', glsl)
    g = [float(x) for x in m.groups()]
    h = [hc('kNormalOffsetTexels'), hc('kNormalOffsetCap'), hc('kConstantBias'), hc('kSlopeTexels'), hc('kTanCap')]
    assert g == h, ('bias constants differ GLSL vs header', g, h)
    taps = re.search(r'AW_SEARCH_TAPS = (\d+), AW_FILTER_TAPS = (\d+)', glsl).groups()
    assert (int(taps[0]) + int(taps[1])) == hc('kResolveGathers'), 'resolve ledger gathers per pixel'
    # the CPU raster mirror's literals
    src = text(REPO / 'scripts/code_tools/relight_3d_cube_source.py')
    assert 'USABLE, GUTTER = 512, 16' in src and 'min(1.0 * texel_world(far), 0.05) + 0.003 + 1.0 * texel_world(far) * tan_cap' in src
    assert h[0] == 1.0 and h[1] == 0.05 and h[2] == 0.003 and h[3] == 1.0 and hc('kContactTanCap') == 2.0, 'harness blind band literal'
    # uniforms the pipeline uploads exist in the shaders; the reserved sampler is declared in the enum and the name list in order
    for u in ('area_world', 'area_world_light', 'area_world_info', 'area_world_seed', 'area_world_rot', 'area_world_atlas'):
        assert re.search(r'\buniform \w+ ' + u + r'\b', glsl), 'filter uniform ' + u
    assert re.search(r'\buniform vec4 area_world;', text(GLSL / 'areaShadowF.glsl')), 'gather uniform area_world'
    for u in ('area_world', 'area_world_light', 'area_world_info', 'area_world_seed', 'area_world_rot'):
        assert 'LLStaticHashedString("' + u + '")' in pipe, 'pipeline upload of ' + u
    mgr_h = text(REPO / 'indra/llrender/llshadermgr.h')
    mgr_c = text(REPO / 'indra/llrender/llshadermgr.cpp')
    enum_order = re.findall(r'^\s+(\w+),', mgr_h[mgr_h.index('AREA_SHADOW_SOURCE,'):mgr_h.index('END_RESERVED_UNIFORMS')], re.M)
    names = re.findall(r'push_back\("(\w+)"\);', mgr_c[mgr_c.index('"area_shadow_source"') if '"area_shadow_source"' in mgr_c else mgr_c.index('"area_source"'):mgr_c.index('llassert(mReservedUniforms.size() == END_RESERVED_UNIFORMS)')])
    assert enum_order[-1] == 'AREA_WORLD_ATLAS' and names[-1] == 'area_world_atlas' and len(enum_order) == len(names), (enum_order, names)
    print('[world] constants PASS: tile/usable/gutter, bias set, tap counts (16 + 32 = the ledger), gutter >= kernel + 2, uniforms, reserved sampler order', flush=True)


def check_mask_sync():
    pipe = text(NV / 'pipeline.cpp')
    def mask_after(marker, nth=0):
        i = pipe.index(marker)
        j = pipe.index('andRenderTypeMask(', i) + len('andRenderTypeMask(')
        k = pipe.index('END_RENDER_TYPES);', j)
        return re.sub(r'\s+', '', pipe[j:k])
    sun = mask_after('void LLPipeline::generateSunShadow(')
    world = mask_after('void LLPipeline::generateWorldShadowCubes(')
    assert sun == world, 'the world pass render-type mask drifted from generateSunShadow (KEEP IN SYNC)'
    print('[world] render-type mask PASS: generateWorldShadowCubes copies generateSunShadow\'s list verbatim (%d types)' % sun.count('RENDER_TYPE'), flush=True)


def check_camera_id(display_src=None):
    """generateWorldShadowCubes returns early unless sCurCameraID == CAMERA_WORLD, and generateSunShadow leaves a shadow ID: the call site
    must set CAMERA_WORLD immediately before the call and restore the saved ID right after it."""
    src = display_src if display_src is not None else text(NV / 'llviewerdisplay.cpp')
    call = 'gPipeline.generateWorldShadowCubes('
    assert src.count(call) == 1, 'exactly one generateWorldShadowCubes call site expected'
    i = src.index(call)
    before = re.sub(r'\s+', '', src[:i])
    assert before.endswith('LLViewerCamera::sCurCameraID=LLViewerCamera::CAMERA_WORLD;'), 'camera ID is not set to CAMERA_WORLD immediately before generateWorldShadowCubes'
    assert 'saved_camera_id=LLViewerCamera::sCurCameraID;LLViewerCamera::sCurCameraID=LLViewerCamera::CAMERA_WORLD;' in before[-200:], 'the previous camera ID is not saved before the override'
    after = re.sub(r'\s+', '', src[i:i + 400])
    assert 'LLViewerCamera::sCurCameraID=saved_camera_id;' in after.split('generateWeatherRainOcclusion')[0], 'the saved camera ID is not restored right after the call'
    pipe = text(NV / 'pipeline.cpp')
    guard = pipe[pipe.index('void LLPipeline::generateWorldShadowCubes('):]
    assert 'LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD' in guard[:3500], 'the CAMERA_WORLD guard in pipeline.cpp must stay'
    print('[world] camera id PASS: CAMERA_WORLD is set immediately before generateWorldShadowCubes and the saved ID restored; the pipeline guard stays', flush=True)


def check_near_and_state():
    """Round 4: the per-light near plane is computed ONCE (worldShadowLightNear) and used by the raster AND the resolve; the function restores the
    entry shader program and texture units on every exit through an RAII guard created right after the early returns."""
    pipe = text(NV / 'pipeline.cpp')
    header = text(NV / 'alworldshadowmath.h')
    assert 'inline double emitterNearPlane(' in header, 'emitterNearPlane missing'
    uses = pipe.count('worldShadowLightNear(')
    assert uses == 3, 'worldShadowLightNear: expected the definition, the raster job and the resolve (3 mentions), got %d' % uses
    assert 'world_info[i].set(static_cast<F32>(ALWorldShadow::kRowsPerLight*world_slot),worldShadowLightNear(' in pipe, 'the resolve must use the per-light near'
    assert 'ALWorldShadow::nearPlane())' not in pipe, 'a raw global near plane crept back in'
    # Round 5: the near is the Chebyshev reach of the emitter's WORLD AABB (+ small enclosing linkset siblings), never an inscribed radius / scale axis.
    fn = pipe[pipe.index('static F32 worldShadowLightNear('):]
    fn = fn[:fn.index('\n}\n')]
    for needle in ('world_shadow_extents(drawable, lo, hi)', 'ALWorldShadow::chebyshevReach(lo, hi, p)', 'ALWorldShadow::siblingReach(', 'getRootEdit()', 'getChildren()',
                   'ALWorldShadow::emitterNearPlane('):
        assert needle in fn, 'worldShadowLightNear lost ' + needle
    assert 'getSpatialExtents()' in pipe[pipe.index('static bool world_shadow_extents('):pipe.index('static F32 worldShadowLightNear(')], 'the AABB must come from the drawable extents'
    assert 'llmin(scale' not in fn and 'min_scale' not in fn, 'the round-4 inscribed (smallest scale axis) rule crept back'
    assert 'inline double emitterNearPlane(double setting, double emitterReach, double lightRadius)' in header and 'emitterReach + 0.01' in header, 'header rule'
    # raster and resolve: BOTH call it with the LIGHT'S OWN drawable (selected by the light's UUID) and the same getLightRadius(), so the values are identical.
    assert re.search(r'getVObj\(\)->getID\(\) == mWorldShadowResident\.id\[k\]\) \{ found = drawable; break; \}', pipe), 'raster: the job drawable must be selected by the resident UUID'
    assert 'const F32 radius = found->getVOVolume()->getLightRadius();' in pipe and 'radius, worldShadowLightNear(found, radius)}' in pipe, 'raster near call'
    assert re.search(r'volume->getID\(\)!=id\) continue;', pipe) and 'worldShadowLightNear(drawable,volume->getLightRadius())' in pipe, 'resolve near call'
    # Round 5 root cause: the rotation upload is packed explicitly (never a memcpy of a glm::mat3), by the SAME rule in C++, the unit test and the harness.
    assert 'std::memcpy(world_rot' not in pipe and 'ALWorldShadow::packViewRotation(view,world_rot)' in pipe, 'world_rot must use packViewRotation'
    assert 'out[3*column + row] = static_cast<float>(view[column][row]);' in header, 'packViewRotation rule'
    sys.path.insert(0, str(REPO / 'scripts/code_tools'))
    import numpy as np
    import importlib.util
    spec = importlib.util.spec_from_file_location('relight_3d_gl_pack', str(REPO / 'scripts/code_tools/relight_3d_gl.py'))
    src_gl = text(REPO / 'scripts/code_tools/relight_3d_gl.py')
    ns = {'np': np}
    exec(src_gl[src_gl.index('def pack_view_rotation('):src_gl.index('\nclass ', src_gl.index('def pack_view_rotation('))], ns)
    Rm = [[.8, -.48, .36], [.6, .64, -.48], [0, .6, .8]]
    view = np.eye(4)
    view[:3, :3] = Rm
    packed = ns['pack_view_rotation'](view)
    expected = [.8, .6, 0, -.48, .64, .6, .36, -.48, .8]
    assert np.allclose(packed, expected, atol=1e-6), ('harness packing differs from the pinned vector', packed)
    test = text(NV / 'tests/alrelightmath_test.cpp')
    assert 'const float expected[9] = {.8f,.6f,0.f, -.48f,.64f,.6f, .36f,-.48f,.8f};' in test, 'the C++ test lost the pinned vector'
    assert 'wrot = pack_view_rotation(cam.view)' in src_gl, 'the harness must upload the packed rotation'
    # the harness mirror
    src = text(REPO / 'scripts/code_tools/relight_3d_cube_source.py')
    mirror = src[src.index('def emitter_near('):src.index('def light_tiles(')]
    for needle in ('chebyshev_reach(lo, hi, light.pos)', 'sibling_encloses_light(lo, hi, light.pos)', 'reach + 0.01', 'min(n, 1.0)', '0.9 * float(light.range)'):
        assert needle in mirror, 'harness emitter_near lost ' + needle
    assert 'largest extent' in src and 'float(np.max(hi - lo)) < 1.0' in src, 'harness sibling rule'
    body = pipe[pipe.index('void LLPipeline::generateWorldShadowCubes('):]
    body = body[:body.index('void LLPipeline::renderGroups(')]
    g = body.index('WorldShadowStateGuard state_guard;')
    first_gl = body.index('const U32 width = ')
    assert g < first_gl, 'the state guard must exist before the first GL work (allocation, release, render)'
    assert 'if (mShader) mShader->bind(); else LLGLSLShader::unbind();' in pipe, 'the guard must rebind or unbind the entry program'
    print('[world] near + state PASS: one per-light near for raster and resolve, state guard before the first GL work', flush=True)


def check_rotation_independence():
    """User note 10/23: point lights are omnidirectional, projectors take their direction from the prim rotation. The cube is VISIBILITY only: its six
    faces stay WORLD-axis aligned for any projector rotation (the cone/texture belongs to the normal light path), and the only rotation that matters is
    the emitter prim's WORLD AABB (the near plane). Nothing in the cube path may read a light/prim rotation."""
    pipe = text(NV / 'pipeline.cpp')
    zones = [pipe[pipe.index('static bool world_shadow_extents('):pipe.index('// [P1-lite] Residency eligibility')],
             pipe[pipe.index('void LLPipeline::generateWorldShadowCubes('):pipe.index('void LLPipeline::renderGroups(')]]
    for z in zones:
        for forbidden in ('getRotation', 'getRenderRotation', 'getWorldRotation', 'getAngularVelocity', 'getSpotLightDir', 'getLightDirection', 'getProjectorDir'):
            assert forbidden not in z, 'the cube path reads a rotation: ' + forbidden
    sys.path.insert(0, str(REPO / 'scripts/code_tools'))
    import numpy as np
    import relight_3d_scene as S
    import importlib.util
    spec = importlib.util.spec_from_file_location('cube_source_rot', str(REPO / 'scripts/code_tools/relight_3d_cube_source.py'))
    sys.modules['__main__'].VisibilitySource = getattr(sys.modules['__main__'], 'VisibilitySource', object)
    cs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cs)
    sc = S.SCENES['WR']()
    base = sc.light_sets[0][0]
    # a projector pointing 30 degrees down at 45 degrees yaw, same position: identical near, identical six faces (the cone is not part of the cube)
    d = (np.cos(np.radians(-30)) * np.cos(np.radians(45)), np.cos(np.radians(-30)) * np.sin(np.radians(45)), np.sin(np.radians(-30)))
    proj = S.Light(base.name, base.pos, base.radius, base.range, kind='projector', dir=tuple(float(v) for v in d), cone=35.0, ignore=base.ignore, seed=base.seed)
    assert cs.emitter_near(sc, proj) == cs.emitter_near(sc, base), 'projector rotation changed the near plane'
    near = cs.emitter_near(sc, base)
    skip = {i for i, p in enumerate(sc.prims) if p.cull == 'back' and cs._inside(p, base.pos)}
    for face in range(6):
        a = cs.raster_face(sc, base, face, near, base.range * 1.5, skip)
        b = cs.raster_face(sc, proj, face, near, proj.range * 1.5, skip)
        assert np.array_equal(a, b), 'the projector cube face %d differs from the point light cube' % face
    # the rotated emitter prim: near = its WORLD AABB reach (a 0.12 m box turned 45 degrees reaches 6 sqrt(2) cm) + 1 cm
    prim = sc.prims[sc.prim_index('bulb')]
    lo, hi = prim.aabb()
    assert abs(float(np.max(hi - np.array(base.pos))) - 0.06 * 2 ** 0.5) < 1e-9, 'rotated prim world AABB'
    assert abs(near - (0.06 * 2 ** 0.5 + 0.01)) < 1e-9, ('rotated emitter near', near)
    print('[world] rotation independence PASS: the cube path reads no rotation; a rotated projector builds the same six world-axis faces and near as the point light; the rotated emitter near is its world AABB reach', flush=True)


def function_range(src, signature):
    i = src.index(signature)
    j = src.index('\n}\n', i) + 3
    return src[:i].count('\n') + 1, src[:j].count('\n') + 1


def check_section10():
    pipe = text(NV / 'pipeline.cpp')
    diff = subprocess.check_output(['git', 'diff', '-U0', BASE, '--', 'indra/newview/pipeline.cpp'], cwd=REPO, text=True, encoding='utf-8')
    # include the committed work AFTER the base too: diff of the base against the working tree
    hunks = [(int(m.group(1)), int(m.group(2) or 1)) for m in re.finditer(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@', diff, re.M)]
    protected = ['bool LLPipeline::allocateShadowBuffer(', 'void LLPipeline::generateSunShadow(', 'void LLPipeline::generatePrismSpotShadows(']
    for sig in protected:
        lo, hi = function_range(pipe, sig)
        for start, count in hunks:
            end = start + max(count, 1) - 1
            assert end < lo or start > hi, '%s was modified (section 10 is untouched): hunk %d-%d inside %d-%d' % (sig, start, end, lo, hi)
    for needle in ('bdmergeMaxSpotShadows()', 'mTargetShadowSpotLight'):
        # the auction/slot logic: no line of the diff outside the world sections may touch these
        for line in diff.splitlines():
            if line.startswith('+') and not line.startswith('+++') and needle in line and 'world' not in line.lower():
                raise AssertionError('section 10 symbol touched: ' + line)
    print('[world] section 10 PASS: allocateShadowBuffer, generateSunShadow, generatePrismSpotShadows have no diff against %s; no auction/slot-count line added' % BASE, flush=True)


check_face_tables()
check_constants()
check_camera_id()
check_near_and_state()
check_rotation_independence()
check_mask_sync()
check_section10()
print('[world] static pins PASS', flush=True)
