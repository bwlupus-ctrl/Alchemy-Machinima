"""Real-WGL 6/8/10/16 spot sampler census, dispatch readback and base parity.

No viewer build/login. Frozen Classic sources stay byte-pinned by the existing
engine gate; this gate additionally exercises its registered 16-slot companion.
"""
from pathlib import Path
import ctypes as C
import re
import json
import subprocess
import sys
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
# Registration is positional; a stale last-sampler assertion crashes debug startup.
manager = (ROOT / 'indra/llrender/llshadermgr.cpp').read_text(encoding='utf-8')
header = (ROOT / 'indra/llrender/llshadermgr.h').read_text(encoding='utf-8')
assert [int(x) for x in re.findall(r'mReservedUniforms.push_back\("shadowMap(\d+)"\)', manager)] == list(range(20))
assert 'llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_SHADOW19+1);' in manager
assert [int(x) for x in re.findall(r'^        DEFERRED_SHADOW(\d+),', header, re.M)] == list(range(20))
camera = (ROOT / 'indra/newview/llviewercamera.h').read_text(encoding='utf-8')
assert [int(x) for x in re.findall(r'^        CAMERA_SPOT_SHADOW(\d+),', camera, re.M)] == list(range(16))
rig = (ROOT / 'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf-8')
assert 'std::clamp(static_cast<U32>(requested), 2u, 10u)' not in rig
assert 'std::clamp(requested, 2, 10)' not in rig
print('PASS contiguous sampler/camera registration and rig ceiling wiring', flush=True)

CHECK = Path(__file__).with_name('relight_shader_check.py')
s = {'__file__': str(CHECK), '__name__': 'spot16_gpu'}
exec(compile(CHECK.read_text(encoding='utf-8').split("if '--area-only' in sys.argv:")[0], str(CHECK), 'exec'), s)
import relight_3d_gl as G
from native_check_tools import compile_run, check_directory

from spot_shadow_registration import production_selection
from spot_shadow_census import inventory
from spot_shadow_classic_extension import check_classic_extension
selection = production_selection()
consumers, classification = inventory()
check_classic_extension(ROOT, subprocess.check_output(['git','show','c4d3614d3e7:indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl']))
I, U, F = s['I'], s['U'], s['F']
budget_rows = []
current_limit = 32
current_count = 2
get_int = s['gl']('glGetIntegerv', None, U, C.POINTER(I))
fragment_limit, vertex_limit = I(), I()
get_int(0x8872, C.byref(fragment_limit))
get_int(0x8B4C, C.byref(vertex_limit))
assert fragment_limit.value >= 32, '16-slot fixture requires >=32 fragment units'
print('stage limits: fragment', fragment_limit.value, 'vertex', vertex_limit.value, flush=True)
get_uniform = s['gl']('glGetActiveUniform', None, U, U, I, C.POINTER(I), C.POINTER(I), C.POINTER(U), C.c_char_p)
validate = s['gl']('glValidateProgram', None, U)

def census(pr, expected, label):
    n = I(); s['getprog'](pr, 0x8B86, C.byref(n))
    names = []
    # All these production modules use fragment samplers only; their vertex
    # modules have no texture access. Count sampler arrays by their active size.
    for j in range(n.value):
        name = C.create_string_buffer(256); length, size, kind = I(), I(), U()
        get_uniform(pr, j, 256, C.byref(length), C.byref(size), C.byref(kind), name)
        if kind.value in (0x8B5E, 0x8B5F, 0x8B60, 0x8B62, 0x8DC1, 0x8DC4, 0x8DC5):
            base = name.value.decode()
            if size.value > 1 or base.endswith('[0]'):
                assert base.endswith('[0]'), (label, 'unexpected sampler array name', base)
                names.extend(base[:-3] + '[%d]' % i for i in range(size.value))
            else:
                names.append(base)
    assert len(names) == expected, (label, names, expected)
    assert len(names) <= fragment_limit.value, (label, names)
    fits = len(names) <= current_limit
    # <=10 layouts are frozen, including the pre-existing 16-unit overflow.
    # Never label those programs safe merely because this GPU has 32 units.
    if not fits:
        assert current_limit == 16 and current_count == 6
    budget_rows.append(dict(program=label, requested=current_count, tier_limit=current_limit,
                            samplers=len(names), fits=fits, names=names,
                            fallback='none needed' if fits else
                            'rim retry drops RIG_RIM_AUX; optional shaders disable; required deferred load can fail (legacy limitation)'))
    # Mirror LLGLSLShader's one unit per active sampler. Validation rejects
    # mixed sampler types aliasing a unit; merely linking would not catch it.
    s['use_raw'](pr)
    locations = [s['loc'](pr, name.encode()) for name in names]
    assert all(location >= 0 for location in locations), (label, names, locations)
    assert len(set(locations)) == len(locations), (label, 'aliased sampler locations', names)
    for unit, location in enumerate(locations): s['ui'](location, unit)
    validate(pr); ok = I(); s['getprog'](pr, 0x8B83, C.byref(ok))
    assert ok.value, (label, 'sampler validation')
    print(label, 'fragment samplers', len(names), '/', current_limit, 'vertex samplers 0', 'FIT' if fits else 'OVER BUDGET (legacy failure/optional disable)', names, flush=True)
    return names


def link_modules(file, defines, classic=False, classic_path=None):
    pr = s['program']()
    mods = list(G.AreaRunner.FRAGMENT_COMMON) + ['deferred/shadowUtil.glsl']
    if file == 'froxelInjectF.glsl': mods += ['deferred/froxelUtil.glsl']
    if file == 'sunLightSSAOF.glsl': mods += ['deferred/aoUtil.glsl']
    for mod in mods:
        path = s['production_file'](mod, 1 if mod in G.AreaRunner.LEVEL1 else 3)
        if classic and mod == 'deferred/shadowUtil.glsl':
            path = s['root'] / 'class1' / classic_path
        if classic and mod == 'deferred/deferredUtil.glsl':
            path = s['root'] / 'class1/deferred/projectorVolumetricClassicUtil.glsl'
        s['attach'](pr, s['compile_part'](path.read_text(encoding='utf-8'), defines, str(path)))
    for name, stage in [('deferred/' + file, 0x8B30), ('deferred/postDeferredNoTCV.glsl', 0x8B31)]:
        path = s['production_file'](name)
        s['attach'](pr, s['compile_part'](path.read_text(encoding='utf-8'), defines, name, stage))
    s['link'](pr); ok = I(); s['getprog'](pr, 0x8B82, C.byref(ok))
    if not ok.value:
        log = C.create_string_buffer(64000); s['plog'](pr, len(log), None, log)
        raise AssertionError(file + log.value.decode())
    return pr


# Recipes classify the consumers DISCOVERED from registrations + reachable GLSL.
# Adding a sampler/call to any other registered program fails this closed set.
recipes = {
    'deferred/spotLightF.glsl': 'surface',
    'deferred/sunLightF.glsl': 'sun',
    'deferred/sunLightSSAOF.glsl': 'sun-ssao',
    'deferred/projectorVolumetricF.glsl': 'shaft-new',
    'deferred/projectorVolumetricClassicF.glsl': 'shaft-classic',
    'deferred/froxelInjectF.glsl': 'froxel',
}
def require_classified(found):
    assert found == set(recipes), ('unclassified or stale spot consumer', found ^ set(recipes))
require_classified(consumers)
try: require_classified(consumers | {'deferred/plantedUnknownF.glsl'})
except AssertionError: pass
else: raise AssertionError('unclassified program escaped census')
print('PASS registration-derived inventory:', len(classification), 'roots/classes; consumers', sorted(consumers), flush=True)
for count, tier_limit in ((2,32),(6,16),(8,28),(10,32),(11,32),(16,32)):
    current_count, current_limit = count, tier_limit
    # These results come from compiled actual global/Classic registration blocks.
    large, classic_path = selection[(count,tier_limit)]
    extra = '#define SPOT_SHADOWS_16 1\n' if large else ''
    samplers = 16 if large else 10
    for name in sorted(consumers):
        file = Path(name).name
        if recipes[name] == 'surface':
            for multi in (False,True):
                for emissive in (False,True):
                    for rim in (False,True):
                        for masked in ((False,True) if multi else (False,)):
                            defines = extra
                            for enabled,key,value in ((multi,'MULTI_SPOTLIGHT','1'),(emissive,'HAS_EMISSIVE','1'),
                                    (rim,'RIG_RIM_AUX','1'),(masked,'main','actorMaskedMain')):
                                if enabled: defines += '#define %s %s\n'%(key,value)
                            label='surface multi=%s emissive=%s rim=%s mask=%s N=%d'%(multi,emissive,rim,masked,count)
                            census(s['link_production']('class3/' + name, defines, label), samplers + 8 + emissive + 2*rim + masked, label)
        else:
            defines = '#define SPOT_SHADOW 1\n'
            if recipes[name] in ('sun','sun-ssao'):
                defines += '#define SUN_SHADOW 1\n'
                # Four sun maps + constant spot slots 0/1 + normal/depth; SSAO
                # adds noiseMap. This is a fixed count, not cancelling algebra.
                expected = 9 if recipes[name] == 'sun-ssao' else 8
            elif recipes[name] == 'shaft-new':
                defines += '#define PROJVOL_INTEGRATION_AB 1\n#define PROJVOL_DUST_ENABLE 1\n#define HAS_EMISSIVE 1\n'
                expected = samplers + 5
            elif recipes[name] == 'shaft-classic':
                defines += '#define PROJVOL_DUST_ENABLE 1\n#define PROJVOL_CONSERVATIVE_SHADOW 1\n#define HAS_EMISSIVE 1\n'
                expected = samplers + 5
            else: expected = samplers + 2
            names = census(link_modules(file, extra + defines, 'Classic' in file, classic_path), expected, file + ' N=' + str(count))
            if recipes[name] in ('sun','sun-ssao'):
                assert sorted(n for n in names if n.startswith('shadowMap')) == ['shadowMap'+str(i) for i in range(6)]
                assert not any(n in names for n in ('shadowMap6','shadowMap19'))

out = ROOT / '.tmp/spot16/r2'
out.mkdir(parents=True,exist_ok=True)
(out/'sampler-census.json').write_text(json.dumps(dict(classification=classification, budgets=budget_rows),indent=2)+'\n',encoding='utf-8')

# With the production <=10 selection, preprocessing the three widened modules
# must reproduce every token of the reviewed bias branch (including the old
# terminal else). The projector shader itself still compares to the common base.
def legacy_tokens(text):
    def branch(m):
        return m[1].split('#else\n',1)[1] if '#else\n' in m[1] else ''
    text = re.sub(r'#ifdef SPOT_SHADOWS_16\n(.*?)#endif\n', branch, text, flags=re.S)
    text = re.sub(r'^#define SPOT_SHADOW_(?:COUNT|MATRIX_COUNT|MAX_INDEX) .*\n', '', text, flags=re.M)
    text = text.replace('SPOT_SHADOW_MAX_INDEX','9')
    text = text.replace('SPOT_SHADOW_MATRIX_COUNT','14').replace('SPOT_SHADOW_COUNT','10')
    text = re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
    return re.findall(r'\w+|[^\s]',text)
for name in ('class1/deferred/shadowUtil.glsl','class3/deferred/projectorVolumetricF.glsl'):
    path=s['root']/name
    reference = '46ec4444fff' if name == 'class1/deferred/shadowUtil.glsl' else 'fb616f2519c'
    baseline=subprocess.check_output(['git','show',reference+':'+path.as_posix()],encoding='utf-8')
    assert legacy_tokens(path.read_text(encoding='utf-8')) == legacy_tokens(baseline), name
print('PASS <=10 production selection preserves reviewed bias GLSL tokens and base shaft layout',flush=True)

# Compile the production dispatch itself for every supported GLSL version.
# Both dark-map and matrix-isolation probes discriminate all sixteen slots.
vertex = 'void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}'
fragment = '''uniform int test_slot;
float sampleSpotShadow(vec3 pos,vec3 norm,int index,vec2 screen);
float sampleSpotShadowConservative(vec3 pos,int index);
out vec4 frag_color;
void main(){frag_color=vec4(sampleSpotShadow(vec3(0,0,-6),vec3(0),test_slot,vec2(0)),
 sampleSpotShadowConservative(vec3(0,0,-6),test_slot),0,1);}
'''

def shader_function(source, name):
    start = source.index('float ' + name + '(')
    return source[start:source.index('\n}', start) + 2]


def coordinate_code(source):
    # Extract both assignments and the actual march consumer, rather than
    # supplying shadow_matrix[4+test_slot] from the test (which hid the clamp).
    assignments = re.findall(r'^        shadow_(?:origin|rate) = .*;', source, re.M)
    assert len(assignments) == 2, assignments
    clip = re.search(r'vis = projvolAirborneShadow\((shadow_origin .*?)\);', source)[1]
    a = source.find('#ifdef SPOT_SHADOWS_16')
    defines = source[a:source.index('#endif', a) + len('#endif')] + '\n' if a >= 0 else ''
    return defines, '\n'.join(assignments), clip


def probe_program(text, extra, shaft, count, classic=False):
    pr = s['program']()
    declarations = 'uniform int proj_shadow_idx;\n' + ''.join('uniform sampler2DShadow shadowMap%d;\n' % j for j in range(4, count+4))
    declarations += 'uniform mat4 shadow_matrix[%d];\n' % (count+4)
    if classic:
        # Classic passes view-space positions into the utility; its production
        # dispatch computes each coordinate. Use the actual two march calls.
        calls = [re.search(r'vis = (sampleSpotShadowConservative\(spos, proj_shadow_idx\));', shaft)[1],
                 re.search(r'vis = (sampleSpotShadow\(spos, vec3\(0.0\), proj_shadow_idx, tc\));', shaft)[1]]
        body = 'vec3 spos=vec3(0,0,-6); vec2 tc=vec2(0);'
        helpers = ''
    else:
        defines, assignments, clip = coordinate_code(shaft)
        declarations = defines + declarations
        helpers = shader_function(shaft, 'projvolAirborneShadow') + shader_function(shaft, 'projvolHardShadow')
        body = 'vec3 d=vec3(0,0,-1); float shadow_t=6.; vec4 shadow_origin,shadow_rate;\n' + assignments
        calls = ['projvolAirborneShadow(' + clip + ')', 'projvolHardShadow(d*shadow_t,' + clip + ')']
    test = declarations + helpers + fragment[:fragment.index('void main()')] + 'void main(){' + body + """
    frag_color=vec4(sampleSpotShadow(vec3(0,0,-6),vec3(0),test_slot,vec2(0)),
      sampleSpotShadowConservative(vec3(0,0,-6),test_slot),%s,%s);}
""" % tuple(calls)
    for source, stage in [(text, 0x8B30), (test, 0x8B30), (vertex, 0x8B31)]:
        s['attach'](pr, s['compile_part'](source, '#define SPOT_SHADOW 1\n' + extra, 'dispatch', stage))
    s['link'](pr); ok = I(); s['getprog'](pr, 0x8B82, C.byref(ok)); assert ok.value, 'probe link'
    return pr


# One float pixel preserves exact comparison bytes (no 8-bit quantization).
target = U(); s['genTex'](1, C.byref(target)); s['active'](0x84C0 + 31); s['bindTex'](0x0DE1, target)
s['teximage'](0x0DE1, 0, 0x8814, 1, 1, 0, 0x1908, 0x1406, None)
s['param'](0x0DE1, 0x2801, 0x2600); s['param'](0x0DE1, 0x2800, 0x2600)
fbo = U(); s['genfbo'](1, C.byref(fbo)); s['bindfbo'](0x8D40, fbo); s['attachment'](0x8D40, 0x8CE0, 0x0DE1, target, 0)
assert s['status'](0x8D40) == 0x8CD5
s['viewport'](0, 0, 1, 1)
for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0BD0): s['disable'](cap)
textures = (U * 16)(); s['genTex'](16, textures)

def render(pr, slot, count, dark=True, matrix_probe=False, soft=False, patterned=False):
    s['use_raw'](pr)
    for j in range(count):
        s['active'](0x84C0 + j); s['bindTex'](0x0DE1, textures[j])
        depth = np.array([.5 if matrix_probe else (0. if (j == slot) == dark else 1.)], np.float32)
        if patterned:
            y, x = np.indices((4,4))
            depth = np.asarray(((x+y+j)%2)*.5+.25, np.float32)
        size = 4 if patterned else 1
        s['teximage'](0x0DE1, 0, 0x8CAC, size, size, 0, 0x1902, 0x1406, depth.ctypes.data)
        for key, value in [(0x2801, 0x2600), (0x2800, 0x2600), (0x2802, 0x812F), (0x2803, 0x812F), (0x884C, 0x884E), (0x884D, 0x0203)]: s['param'](0x0DE1, key, value)
        s['ui'](s['loc'](pr, ('shadowMap%d' % (j + 4)).encode()), j)
    mats = np.zeros((count + 4, 4, 4), np.float32)
    for j in range(count):
        mats[j + 4, 3] = [.5, .5, .25 if matrix_probe == 'rate' or matrix_probe and j == slot else .75 if matrix_probe else .5, 1.]
        # Nonzero, independently discriminating origin/rate terms: leaving
        # either production clamp at 9 must fail every slot 10..15.
        if matrix_probe:
            mats[j + 4, 2, 2] = -.1 if matrix_probe == 'rate' and j != slot else -.01
    s['matrix'](s['loc'](pr, b'shadow_matrix'), count + 4, 0, mats.ctypes.data_as(C.POINTER(F)))
    s['ui'](s['loc'](pr, b'test_slot'), slot)
    s['ui'](s['loc'](pr, b'proj_shadow_idx'), slot)
    s['uf'][4](s['loc'](pr, b'shadow_clip'), 1, 2, 4, 8)
    s['uf'][2](s['loc'](pr, b'proj_shadow_res'), *( (4,4) if patterned else (4096,4096)))
    for key, value in [('soft_shadow_enable', int(soft)), ('soft_shadow_vogel', int(soft)), ('soft_shadow_taps', 12)]: s['ui'](s['loc'](pr, key.encode()), value)
    for key, value in [('soft_shadow_scale', 1.), ('soft_shadow_max', 4.), ('soft_shadow_fill', .2)]: s['uf'][1](s['loc'](pr, key.encode()), value)
    s['draw'](4, 0, 3)
    pixel = np.empty(4, np.float32); s['read'](0, 0, 1, 1, 0x1908, 0x1406, pixel.ctypes.data)
    return pixel

# A real sized sampler array must resolve/bind every element individually.
array_pr = s['program']()
array_fragment = """uniform sampler2D censusArray[3];
uniform samplerCube censusCube;
uniform int censusIndex;
out vec4 frag_color;
void main(){frag_color=texture(censusArray[censusIndex],vec2(.5))
    +texture(censusCube,vec3(1,0,0));}
"""
for source, stage in ((array_fragment, 0x8B30), (vertex, 0x8B31)):
    s['attach'](array_pr, s['compile_part'](source, '', 'sampler-array census', stage))
s['link'](array_pr); ok = I(); s['getprog'](array_pr, 0x8B82, C.byref(ok)); assert ok.value
array_names = census(array_pr, 4, 'sampler-array fixture')
assert set(array_names) == {'censusArray[%d]' % i for i in range(3)} | {'censusCube'}
get_uniform_int = s['gl']('glGetUniformiv', None, U, I, C.POINTER(I))
for unit, name in enumerate(array_names):
    actual = I(-1)
    get_uniform_int(array_pr, s['loc'](array_pr, name.encode()), C.byref(actual))
    assert actual.value == unit, (name, actual.value, unit)
print('PASS sampler-array individual locations and binding readback', flush=True)

base_compile = s['compile_part']
for version in (330, 400, 420):
    # The shared compiler's sole version directive is substituted in memory.
    code = CHECK.read_text(encoding='utf-8'); code = code[code.index('def compile_part('):code.index('files=[')]
    exec(compile(code.replace('#version 420', '#version %d' % version), 'spot-version', 'exec'), s)
    for relative, base_relative in [('class1/deferred/shadowUtil.glsl', 'class1/deferred/shadowUtil.glsl'),
            ('class1/deferred/projectorVolumetricClassicShadowUtil16.glsl', 'class1/deferred/projectorVolumetricClassicShadowUtil.glsl')]:
        text = (s['root'] / relative).read_text(encoding='utf-8')
        base = subprocess.check_output(['git', 'show', 'fb616f2519c:' + (s['root'] / base_relative).as_posix()], encoding='utf-8')
        air_path = s['root'] / 'class3/deferred/projectorVolumetricF.glsl'
        classic = 'Classic' in relative
        shaft_path = s['root'] / ('class3/deferred/projectorVolumetricClassicF.glsl' if classic else 'class3/deferred/projectorVolumetricF.glsl')
        air_now = shaft_path.read_text(encoding='utf-8')
        air_base = subprocess.check_output(['git','show','fb616f2519c:'+shaft_path.as_posix()],encoding='utf-8')
        pr = probe_program(text, '#define SPOT_SHADOWS_16 1\n', air_now, 16, classic)
        reference = probe_program(base, '', air_base, 10, classic)
        for slot in range(16):
            for dark in (True, False):
                pixel = render(pr, slot, 16, dark)
                assert np.array_equal(pixel, [0., 0., 0., 0.] if dark else [1., 1., 1., 1.]), (relative, version, slot, dark, pixel)
            for term in ('origin', 'rate'):
                pixel = render(pr, slot, 16, matrix_probe=term)
                assert np.array_equal(pixel, [1., 1., 1., 1.]), ('production matrix coordinate', relative, version, slot, term, pixel)
            if slot < 10:
                for dark in (True, False):
                    for soft in (False, True):
                        assert render(pr, slot, 16, dark, soft=soft).tobytes() == render(reference, slot, 10, dark, soft=soft).tobytes(), ('base parity', relative, version, slot, dark, soft)
                for soft in (False, True):
                    actual = render(pr, slot, 16, soft=soft, patterned=True)
                    expected = render(reference, slot, 10, soft=soft, patterned=True)
                    assert actual.tobytes() == expected.tobytes(), ('filtered base parity', relative, version, slot, soft, actual, expected)
                    if not soft: assert 0. < actual[0] < 1., ('filter probe must be fractional', actual)
        if not classic:
            for term in ('origin', 'rate', 'both'):
                mutant = air_now
                for name in (('origin', 'rate') if term == 'both' else (term,)):
                    pattern = r'(shadow_' + name + r' = .*?)SPOT_SHADOW_MAX_INDEX'
                    mutant, n = re.subn(pattern, r'\g<1>9', mutant)
                    assert n == 1
                broken = probe_program(text, '#define SPOT_SHADOWS_16 1\n', mutant, 16)
                for slot in range(10, 16):
                    pixel = render(broken, slot, 16, matrix_probe='rate' if term == 'rate' else 'origin')
                    assert not np.array_equal(pixel, [1., 1., 1., 1.]), ('clamp-9 negative escaped', version, term, slot, pixel)
            print('PASS planted production origin/rate/both clamp-9 negatives: slots 10..15', flush=True)
        print(relative, 'GLSL', version, 'slots 1..16 maps/matrices + 1..10 hard/soft byte parity PASS', flush=True)
s['compile_part'] = base_compile

# Exercise the exact production ceiling/tier/clamp/map helpers with MSVC.
header = (ROOT / 'indra/newview/pipeline.h').read_text(encoding='utf-8')
a = header.index('    static constexpr U32    MAX_SPOT_SHADOWS =')
b = header.index('    LLRenderTarget          mSpotShadow', a)
source = '#include <cassert>\nusing U32=unsigned;\nstruct Pipeline {\n' + header[a:b] + '''};
int main(){
 for(unsigned units=0;units<=64;++units){
  unsigned cap=units>=32?16:units>=28?8:6;
  assert(Pipeline::maxSpotShadowsForTextureUnits(units)==cap);
  for(unsigned request=0;request<=20;++request)
   assert(Pipeline::clampSpotShadowCount(request,units)==(request<2?2:request>cap?cap:request));
 }
 for(unsigned slot=0;slot<16;++slot) assert(Pipeline::spotShadowMapIndex(slot)==slot+4);
}
'''
result = compile_run(check_directory('spot_shadows_16'), source, 'tiers')
assert result.returncode == 0, result.stdout + result.stderr
from director_actor_bar_check import reverse_spot_shadows_16, reverse_softshadow_bias
for name in ('pipeline.h', 'pipeline.cpp'):
    current = (ROOT / 'indra/newview' / name).read_text(encoding='utf-8')
    baseline = subprocess.check_output(['git','show','fb616f2519c:indra/newview/' + name], encoding='utf-8').replace('\r\n','\n')
    assert reverse_spot_shadows_16(reverse_softshadow_bias(current, name), name) == baseline
    if name == 'pipeline.h':
        try: reverse_spot_shadows_16(current + '\n// unrelated edit\n', name)
        except AssertionError: pass
        else: raise AssertionError('unrelated edit escaped allowance')
print('PASS ceiling/tier/clamp/map native checks and exact newest allowance', flush=True)
