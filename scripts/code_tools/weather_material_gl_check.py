"""WP2 real-WGL material inventory, temporal, energy, coverage and 4K paired cost gate.

Production fragment modules and their actual material entrypoints are compiled unchanged.
Only geometry, textures and uniforms are fixtures. No CPU wet-material implementation.
--source-ref d9eec7c85c5 must fail; --mutations must reject every control.
Native scene cost/acceptance remains provisional until the final viewer build/user test.
"""
import os
os.environ['PYTHONUTF8'] = '1'
import argparse
import ctypes as C
import json
from pathlib import Path
import re
import subprocess
import numpy as np
from relight_3d_gl import SourceTree, SHADERS
from weather_cpu_check import ROOT
from weather_rain_gl_check import function

INVENTORY = [
    ('class1/deferred/diffuseF.glsl', ''),
    ('class1/deferred/diffuseIndexedF.glsl', ''),
    ('class1/deferred/bumpF.glsl', ''),
    ('class1/deferred/diffuseAlphaMaskF.glsl', ''),
    ('class1/deferred/diffuseAlphaMaskIndexedF.glsl', ''),
    ('class1/deferred/diffuseAlphaMaskNoColorF.glsl', ''),
    ('class1/deferred/pbropaqueF.glsl', '#define HAS_ALPHA_MASK 1\n'),
    ('class1/deferred/pbropaqueIndexedF.glsl', '#define GLTF_INDEXED_CHANNELS 1\n'),
    ('class1/deferred/terrainF.glsl', ''),
    ('class1/deferred/pbrterrainF.glsl', '#define TERRAIN_PBR_DETAIL 0\n#define TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT 1\n#define TERRAIN_PAINT_TYPE 0\n'),
    ('class3/deferred/materialF.glsl', '#define DIFFUSE_ALPHA_MODE 2\n#define HAS_NORMAL_MAP 1\n#define HAS_SPECULAR_MAP 1\n'),
    ('class3/deferred/materialF.glsl', '#define DIFFUSE_ALPHA_MODE 1\n#define HAS_NORMAL_MAP 1\n#define HAS_SPECULAR_MAP 1\n'),
    ('class2/deferred/pbralphaF.glsl', ''),
    ('class2/deferred/alphaF.glsl', '#define USE_DIFFUSE_TEX 1\n#define USE_VERTEX_COLOR 1\n'),
    ('class1/deferred/materialIndexedF.glsl', '#define GLTF_INDEXED_CHANNELS 1\n#define DIFFUSE_ALPHA_MODE 2\n#define HAS_NORMAL_MAP 1\n#define HAS_SPECULAR_MAP 1\n'),
    ('class2/deferred/sharedActorFxPbrF.glsl', '#define SHARED_ACTOR_FX_ALPHA_MODE 0\n#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_REPLAY 1\n'),
    ('class2/deferred/sharedActorFxPbrF.glsl', '#define SHARED_ACTOR_FX_ALPHA_MODE 1\n#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_REPLAY 1\n'),
    ('class2/deferred/sharedActorFxPbrF.glsl', '#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_REPLAY 1\n'),
    ('class2/deferred/sharedActorFxPbrF.glsl', '#define SHARED_ACTOR_FX_ALPHA_MODE 0\n#define HAS_ACTOR_FX 1\n#define SHARED_ACTOR_FX_REPLAY 1\n#define SHARED_ACTOR_FX_SLOT_FILTER 1\n#define GLTF_INDEXED_CHANNELS 1\n'),
    ('class1/deferred/treeF.glsl', ''),
]


def run(ref='WORKTREE', mutation=None, cost=True):
    tree = SourceTree(ref)
    helper = tree.read(SHADERS + '/class1/deferred/weatherMaterialF.glsl')
    manager=tree.read('indra/newview/llreflectionmapmanager.cpp')
    for consumer in ('mWeatherProbeRevision.update(', 'mWeatherCinematicRevision.update(',
                     'sig.addExact(mWeatherCinematicRevision.signature())'):
        assert consumer in manager, 'missing weather probe consumer '+consumer
    loader=tree.read('indra/llrender/llglslshader.cpp')
    create=function(loader,'bool LLGLSLShader::createShader(')
    assert create.index('prepareWeatherFeatures(this)')<create.index('mShaderHash = hash()'), 'weather permutation omitted from binary cache key'
    attach=function(tree.read('indra/llrender/llshadermgr.cpp'),'bool LLShaderMgr::attachShaderFeatures(')
    color_attachment=attach.index('attachFragmentObject("environment/srgbF.glsl")')
    assert 'features->hasWeatherMaterial' in attach[color_attachment-250:color_attachment], 'native wet legacy permutations omit the colour-space module'
    feature_prepare=function(tree.read('indra/llrender/llshadermgr.cpp'),'void LLShaderMgr::prepareWeatherFeatures(')
    assert all('"'+flag+'"' in feature_prepare for flag in ('HAS_SKIN','AVATAR_GEOMETRY','IS_HUD','MATERIAL_VELOCITY')), 'excluded native permutations use weather features'
    for file in ('indra/newview/lldrawpool.cpp','indra/newview/llactormover.cpp'):
        source=tree.read(file)
        pos=source.index('LLShaderMgr::WEATHER_MATERIAL_EXCLUDED')
        assert 'mHasGlow' in source[pos:pos+300], 'authored glowing draw lacks weather exclusion '+file
    pipe=tree.read('indra/newview/pipeline.cpp')
    legacy=function(pipe,'void LLPipeline::generateWeatherRainOcclusion(')
    assert legacy==function(SourceTree('330d012e161').read('indra/newview/pipeline.cpp'),'void LLPipeline::generateWeatherRainOcclusion('), 'legacy cover predicate/path changed'
    assert 'ALWeatherResources::State saved_state' in function(pipe,'void LLPipeline::generateWeatherSurfaceCover(')
    # Negative controls mutate a scratch string, never a repository file.
    changes = {
        'off': ('if (!weatherMaterialActive()) return;', 'if (false) return;'),
        'screen': ('agent.xy + weather_material_origin.xy', 'gl_FragCoord.xy'),
        'cover': ('return step(q.z - 0.12 / weather_material_time_range.y, depth);', 'return 1.0;'),
        'normal': ('n = normalize(n + vec3(slope, 0.0));', 'n = n + vec3(slope * 40.0, 0.0);'),
        'energy': ('color *= 1.0 - wet *', 'color *= 1.0 + 4.0 * wet *'),
        'gloss': ('gloss = 1.0 - roughness;', 'gloss = roughness;'),
    }
    if mutation in changes:
        a, b = changes[mutation]
        assert a in helper
        helper = helper.replace(a, b)
        if mutation == 'normal': helper = helper.replace('normal_eye = normalize(transpose(mat3(weather_material_view_to_agent)) * n);', 'normal_eye = transpose(mat3(weather_material_view_to_agent)) * n;')
    # Reuse the established neutral native lighting fixture and real GL context.
    import alpha_blend_parity_gl_check as A
    gl = A.gl
    getstr = gl.fn('glGetString', C.c_char_p, C.c_uint)
    device = {k: getstr(v).decode() for k, v in [('vendor',0x1F00),('renderer',0x1F01),('version',0x1F02)]}
    print('WGL', device, flush=True)
    r = A.Runner(ref)
    old = A.Runner('330d012e161')
    all_programs = []
    # 4 float MRTs also expose flags, emissive and alpha for exact off comparisons.
    textures = []
    def texture(w, h, data=None, internal=0x8814, fmt=0x1908):
        tex = C.c_uint(); gl.gen_tex(1, C.byref(tex)); textures.append(tex)
        gl.active(0x84C0 + 15); gl.bind_tex(0x0DE1, tex)
        for p, v in [(0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)]: gl.param(0x0DE1,p,v)
        gl.teximage(0x0DE1,0,internal,w,h,0,fmt,0x1406,data.ctypes.data if data is not None else None)
        return tex
    def target_bind(ids):
        gl.bind_fbo(0x8D40,A.fbo)
        A.target_bind(ids)
    outputs = [texture(48,32) for _ in range(4)]
    cover_open = texture(16,16,np.ones((16,16),np.float32),0x822E,0x1903)
    cover_roof = texture(16,16,np.full((16,16),0.2,np.float32),0x822E,0x1903)
    flat_z = A.tex((.5,1.,.5,1.)) # native scene fixture has z-up rather than eye-z-up
    # Constant lookup fixtures represent the native indexed texture dispatch only.
    lookup = 'uniform sampler2D fixtureDiffuse; vec4 diffuseLookup(vec2 uv){return texture(fixtureDiffuse,uv);}\n'

    def vertex(source, defs):
        # Derive the fixture interface from each unmodified production fragment.
        names = {}
        for qualifier, ty, name, count in re.findall(r'(?:(flat)\s+)?\bin\s+(vec[234]|float|int)\s+(\w+)\s*(?:\[(\d+)\])?\s*;', source):
            names[name] = (qualifier, ty, int(count) if count else 0)
        declarations = []; assignments = []
        for name, (q, ty, count) in names.items():
            declarations.append((q+' ' if q else '')+'out '+ty+' '+name+(f'[{count}]' if count else '')+';')
            if name in ('vary_position','pos'): value = 'P'
            elif name in ('vary_normal','vary_norm'): value = 'mat3(fixtureWorldToView)*vec3(0,0,1)'
            elif name == 'vary_sign' or name == 'vary_signs': value = '1.0'
            elif name == 'vary_tangent' or name == 'vary_tangents': value = ('vec4(mat3(fixtureWorldToView)*vec3(1,0,0),1)' if ty=='vec4' else 'mat3(fixtureWorldToView)*vec3(1,0,0)')
            elif name == 'vertex_color': value = 'vec4(0.8,0.9,0.7,0.65)'
            elif name == 'vary_cur_clip': value = 'vec4(.4,.2,0,1)'
            elif name == 'vary_last_clip': value = 'vec4(.1,.1,0,1)'
            elif name in ('vary_mat0','vary_mat1','vary_mat2'): value = 'transpose(mat3(fixtureWorldToView))['+name[-1]+']'
            elif 'fragcoord' in name: value = ty+'(1.0)'
            elif 'texcoord' in name or 'coords' in name: value = ty+'(0.5)'
            elif ty == 'int': value = '0'
            else: value = ty+'(0.0)'
            assignments.append((f'for(int i=0;i<{count};++i) {name}[i]={value};' if count else f'{name}={value};'))
        return '\n'.join(declarations)+'''
uniform vec3 fixtureCamera;
uniform vec2 fixtureShift;
uniform mat4 fixtureWorldToView;
void main(){vec2 xy=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
 gl_Position=vec4(xy*2.0-1.0,0,1);
 vec3 P=mat3(fixtureWorldToView)*(vec3(xy*2.0+fixtureShift,0.0)-fixtureCamera);
'''+ '\n'.join(assignments)+'\n}'

    def program(file, defs, baseline=False, extra='', weather=True):
        owner = old if baseline else r
        src = owner.source(file)
        pr = gl.program()
        defines = '#define REFMAP_LEVEL 1\n#define HAS_EMISSIVE 1\n' + defs + extra
        if weather and not baseline: defines += '#define HAS_WEATHER_MATERIAL 1\n'
        for module in A.COMMON:
            gl.attach(pr, owner.compile(owner.source(module), defines, module))
        if '#define HAS_ACTOR_FX 1' in defines:
            for module in ('alchemy/avatarLightResponseF.glsl','alchemy/actorFxDissolveF.glsl','alchemy/actorFxF.glsl'):
                gl.attach(pr,owner.compile(owner.source(module),defines,module))
        if not baseline and weather: gl.attach(pr, owner.compile(helper, defines, 'weatherMaterial'))
        if 'diffuseLookup(' in src: gl.attach(pr, owner.compile(lookup, defines, 'lookup fixture'))
        if 'pbrterrainF' in file:
            gl.attach(pr, owner.compile(owner.source('class1/deferred/pbrterrainUtilF.glsl'), defines, 'terrain utility'))
        gl.attach(pr, owner.compile(src, defines, file))
        gl.attach(pr, owner.compile(vertex(src, defines), defines, file+' vertex',0x8B31))
        for i in range(4): A.bindout(pr,i,('frag_data[%d]'%i).encode())
        A.bindout(pr,0,b'frag_color')
        gl.link(pr); ok = C.c_int(); gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);gl.plog(pr,len(log),None,log);raise AssertionError(file+' LINK '+log.value.decode())
        all_programs.append(pr)
        return pr

    def matrix(pr, name, value):
        packed=np.ascontiguousarray(np.array(value,np.float32).T)
        gl.matrix(gl.loc(pr,name.encode()),1,0,packed.ctypes.data_as(C.POINTER(C.c_float)))
    def configure(pr, wet=0, covered=False, camera=(0.,0.,0.), shift=(0.,0.), puddles=.7, ripple=1., time=0., excluded=0, rotation=None, emission=0.):
        r.defaults(pr)
        r.set(pr,'fixtureCamera',*camera); r.set(pr,'fixtureShift',*shift)
        r.set(pr,'weather_material_enabled',int(wet != 0));r.set(pr,'weather_material_excluded',excluded)
        r.set(pr,'weather_material_wet',float(wet),float(puddles),float(ripple),1.)
        r.set(pr,'weather_material_origin',0.,0.,0.)
        r.set(pr,'weather_material_time_range',float(time),128.)
        r.set(pr,'emissiveColor',emission,emission,emission)
        r.set(pr,'emissive_brightness',emission)
        r.bind(pr,'emissiveMap',11,A.diffuse if emission else A.black)
        r.set(pr,'sharedActorFxOpacity',1.)
        r.set(pr,'specular_color',.1,.1,.1,.25)
        r.set(pr,'fixtureDiffuse',1)
        r.set(pr,'terrain_texture_scale',1.)
        r.set(pr,'terrain_paint_type',0)
        for i in range(4):
            r.set(pr,f'baseColorFactors[{i}]',1.,1.,1.,1.)
            r.set(pr,f'orm_factors[{i}]',1.,.7,0.)
            r.set(pr,f'emissiveColors[{i}]',0.,0.,0.)
            r.set(pr,f'minimum_alphas[{i}]',0.)
        for i in range(1):
            r.set(pr,f'gltf_roughness_factor[{i}]',.7)
            r.set(pr,f'gltf_metallic_factor[{i}]',0.)
            r.set(pr,f'gltf_emissive_color[{i}]',emission,emission,emission)
            r.set(pr,f'gltf_base_color_factor[{i}]',1.,1.,1.,1.)
            r.set(pr,f'gltf_minimum_alpha[{i}]',0.)
            r.set(pr,f'mat_specular_color[{i}]',.1,.1,.1,.25)
            r.set(pr,f'mat_env_intensity[{i}]',0.)
            r.set(pr,f'mat_emissive_brightness[{i}]',emission)
            r.set(pr,f'mat_minimum_alpha[{i}]',0.)
        for key in ['detail_0','detail_1','detail_2','detail_3','alpha_ramp','detail_0_base_color','detail_1_base_color','detail_2_base_color','detail_3_base_color','basecolor0','diffuse0']:
            r.bind(pr,key,1,A.diffuse)
        for key in ['detail_0_normal','detail_1_normal','detail_2_normal','detail_3_normal','normalmap0','bump0']:
            r.bind(pr,key,9,A.flat_normal)
        for key in ['detail_0_metallic_roughness','detail_1_metallic_roughness','detail_2_metallic_roughness','detail_3_metallic_roughness','ormmap0','spec0']:
            r.bind(pr,key,10,A.orm)
        for key in ['detail_0_emissive','detail_1_emissive','detail_2_emissive','detail_3_emissive','emissivemap0']:
            r.bind(pr,key,11,A.diffuse if emission else A.black)
        r.bind(pr,'weather_surface_cover_map',14,cover_roof if covered else cover_open)
        rot=np.eye(3) if rotation is None else rotation
        fixture_view=np.eye(4);fixture_view[:3,:3]=rot
        matrix(pr,'fixtureWorldToView',fixture_view)
        r.set(pr,'fixtureEyeNormal',*map(float,rot@np.array([0.,0.,1.])))
        view = np.eye(4); view[:3,:3]=rot.T;view[:3,3]=camera
        matrix(pr,'weather_material_view_to_agent',view)
        matrix(pr,'weather_surface_cover_matrix',np.diag([1/64,1/64,-1/64,1]))
    def draw(pr, wet=0, size=(48,32), targets=None, **kw):
        configure(pr,wet,**kw)
        ids=targets or outputs
        target_bind(ids)
        gl.viewport(0,0,*size)
        A.clearcolor(-5.,-5.,-5.,-5.); A.clear(0x4000)
        gl.draw(4,0,3)
        result=[]
        for i in range(4):
            A.readbuffer(0x8CE0+i)
            pixels=np.empty((size[1],size[0],4),np.float32)
            gl.read(0,0,*size,0x1908,0x1406,pixels.ctypes.data)
            assert np.all(np.isfinite(pixels)), 'non-finite '+str(i)
            result.append(pixels)
        assert A.err()==0, 'GL error'
        return np.array(result)

    identity=0; effects=[]
    for file,defs in INVENTORY:
        before, after = program(file,defs,True), program(file,defs)
        off_program=program(file,defs,weather=False)
        a,b=draw(before),draw(after)
        assert np.array_equal(a,b), 'weather-off byte identity '+file
        assert np.array_equal(a,draw(off_program)), 'surface-off permutation byte identity '+file
        identity+=2
        wet=draw(after,1)
        assert np.max(np.abs(wet[0,:,:,:3]-b[0,:,:,:3]))>1e-4, 'dead material consumer '+file
        # Surface writes leave all carriers and coverage intact.
        forward = file.startswith('class2/') or '#define DIFFUSE_ALPHA_MODE 1\n' in defs
        if not forward:
            assert np.array_equal(wet[0,:,:,3],b[0,:,:,3]), 'G-buffer alpha changed '+file
            assert np.array_equal(wet[2,:,:,2:],b[2,:,:,2:]), 'flags/env changed '+file
            assert np.array_equal(wet[3],b[3]), 'emission changed '+file
        else:
            assert np.array_equal(wet[0,:,:,3],b[0,:,:,3]), 'forward alpha changed '+file
        covered=draw(after,1,covered=True)
        assert np.max(np.abs(covered-b))<2e-6, 'sheltered interior wetted '+file+' '+str([(i,float(np.max(np.abs(covered[i]-b[i]))),covered[i,0,0].tolist(),b[i,0,0].tolist()) for i in range(4)])
        excluded=draw(after,1,excluded=1)
        assert np.array_equal(excluded,b), 'avatar/HUD/fullbright draw mask '+file
        if file in ('class1/deferred/pbropaqueF.glsl','class1/deferred/pbropaqueIndexedF.glsl',
                    'class3/deferred/materialF.glsl','class1/deferred/materialIndexedF.glsl',
                    'class2/deferred/pbralphaF.glsl','class2/deferred/sharedActorFxPbrF.glsl'):
            emitting=draw(after,emission=.5)
            assert not np.array_equal(emitting,b), 'emission fixture is empty '+file
            assert np.array_equal(emitting,draw(after,1,emission=.5)), 'emitting surface is coated '+file
        effects.append(file)
        print('material PASS',file,flush=True)
    for file,defs in [INVENTORY[0],INVENTORY[6],INVENTORY[10],INVENTORY[11],INVENTORY[12],INVENTORY[13]]:
        avatar=program(file,defs,extra='#define HAS_SKIN 1\n#define GBUFFER_AVATAR_TAG 1\n')
        assert np.array_equal(draw(avatar),draw(avatar,1)), 'WP2 coats avatar permutation '+file
    hud=program('class1/deferred/pbropaqueF.glsl','#define IS_HUD 1\n')
    assert np.array_equal(draw(hud),draw(hud,1)), 'HUD changed'
    velocity=program('class3/deferred/materialF.glsl','#define MATERIAL_VELOCITY 1\n#define DIFFUSE_ALPHA_MODE 1\n',weather=False)
    assert np.array_equal(draw(velocity),draw(velocity,1)), 'wetness changes velocity coverage'
    for file in ('class1/deferred/fullbrightF.glsl','class1/environment/waterF.glsl','class3/environment/waterF.glsl'):
        assert tree.read(SHADERS+'/'+file)==SourceTree('330d012e161').read(SHADERS+'/'+file), 'excluded shader modified '+file

    # Direct production helper fixture gives decoded fields before native packing.
    fixture='''in vec3 vary_position; out vec4 frag_data[4];uniform vec3 fixtureEyeNormal;
uniform float fixtureRoughness; uniform float fixtureMetal; uniform int fixtureLegacy;
uniform mat4 weather_material_view_to_agent; uniform vec3 weather_material_origin;
void weatherMaterialLinear(vec3 p,inout vec3 n,inout vec3 c,inout float r,float m);
void weatherMaterialLegacyLinear(vec3 p,inout vec3 n,inout vec3 c,inout vec3 s,inout float g);
float weatherPuddle(vec2 p,float up);
void main(){vec3 n=fixtureEyeNormal,c=vec3(.6),s=vec3(.04);float r=fixtureRoughness;
 if(fixtureLegacy!=0) weatherMaterialLegacyLinear(vary_position,n,c,s,r);
 else weatherMaterialLinear(vary_position,n,c,r,fixtureMetal);
 frag_data[0]=vec4(c,r);frag_data[1]=vec4(n,weatherPuddle((weather_material_view_to_agent*vec4(vary_position,1)).xy+weather_material_origin.xy,1.));
 frag_data[2]=vec4(s,0);frag_data[3]=vec4(0);}
'''
    pr=gl.program()
    gl.attach(pr,r.compile(helper,'','helper'))
    gl.attach(pr,r.compile(r.source('environment/srgbF.glsl'),'','srgb'))
    gl.attach(pr,r.compile(fixture,'','decoded fixture'))
    gl.attach(pr,r.compile(vertex(fixture,''),'','decoded vertex',0x8B31))
    for i in range(4): A.bindout(pr,i,('frag_data[%d]'%i).encode())
    gl.link(pr);ok=C.c_int();gl.getprog(pr,0x8B82,C.byref(ok));assert ok.value
    # Defaults set by configure don't override these decoded fixture inputs.
    gl.use(pr);r.set(pr,'fixtureRoughness',.8);r.set(pr,'fixtureMetal',0.);r.set(pr,'fixtureLegacy',0)
    a=draw(pr,1,ripple=1.,time=.25)
    b=draw(pr,1,ripple=1.,time=.25,camera=(4.,-2.,3.))
    temporal=float(np.max(np.abs(a-b)))
    shifted=draw(pr,1,ripple=1.,time=.25,camera=(4.,-2.,3.),shift=(2./48,0.))
    temporal=max(temporal,float(np.max(np.abs(a[:,:,1:]-shifted[:,:,:-1]))))
    yaw,pitch=np.deg2rad([37.,23.])
    rz=np.array([[np.cos(yaw),-np.sin(yaw),0.],[np.sin(yaw),np.cos(yaw),0.],[0.,0.,1.]])
    rx=np.array([[1.,0.,0.],[0.,np.cos(pitch),-np.sin(pitch)],[0.,np.sin(pitch),np.cos(pitch)]])
    rotation=rx@rz
    rotated=draw(pr,1,ripple=1.,time=.25,camera=(4.,-2.,3.),rotation=rotation)
    temporal=max(temporal,float(np.max(np.abs(a[0]-rotated[0]))),float(np.max(np.abs(a[1,:,:,:3]-rotated[1,:,:,:3]@rotation))),float(np.max(np.abs(a[1,:,:,3]-rotated[1,:,:,3]))))
    micro_yaw=np.deg2rad(.02)
    micro_rotation=np.array([[np.cos(micro_yaw),-np.sin(micro_yaw),0.],[np.sin(micro_yaw),np.cos(micro_yaw),0.],[0.,0.,1.]])
    micro=draw(pr,1,ripple=1.,time=.25,camera=(.001,0.,0.),rotation=micro_rotation)
    temporal=max(temporal,float(np.max(np.abs(a[0]-micro[0]))),float(np.max(np.abs(a[1,:,:,:3]-micro[1,:,:,:3]@micro_rotation))),float(np.max(np.abs(a[1,:,:,3]-micro[1,:,:,3]))))
    assert temporal<2e-5, 'camera-anchored puddle shimmer '+str(temporal)
    lengths=np.linalg.norm(a[1,:,:,:3],axis=-1)
    assert np.max(np.abs(lengths-1.))<1e-5, 'ripple normals not normalized'
    assert a[0,:,:,3].min()>=.12-1e-6, 'roughness below native floor'
    assert a[0,:,:,:3].max()<=.6+1e-6 and a[0,:,:,:3].min()>=.6*.72-1e-6, 'dielectric energy bound'
    # Decoded PBR and legacy share linear darkening but encode opposite gloss/roughness.
    r.set(pr,'fixtureRoughness',.2);r.set(pr,'fixtureLegacy',1)
    legacy=draw(pr,1,ripple=0.)
    r.set(pr,'fixtureRoughness',.8);r.set(pr,'fixtureLegacy',0)
    pbr=draw(pr,1,ripple=0.)
    assert np.max(np.abs(legacy[0,:,:,:3]-pbr[0,:,:,:3]))<2e-6
    assert np.max(np.abs(legacy[0,:,:,3]+pbr[0,:,:,3]-1))<2e-6, 'PBR roughness stored in legacy gloss'
    assert np.all(legacy[2,:,:,:3]<=.04+1e-6), 'specular energy increased beyond 0.02 absolute film bound'
    # Metal F0 tracks base color and cannot gain integrated specular energy here.
    r.set(pr,'fixtureMetal',1.)
    metal=draw(pr,1)
    assert metal[0,:,:,:3].max()<=.6+1e-6 and metal[0,:,:,:3].min()>=.588-1e-6

    # Integrate the actual native punctual BRDF over incident directions, rather
    # than using peak highlights (which legitimately sharpen as a film smooths).
    energy_src='''out vec4 frag_data[4]; uniform float energyMetal; uniform float energyRough;
uniform vec3 energyView;
void weatherMaterialLinear(vec3 p,inout vec3 n,inout vec3 c,inout float r,float m);
void pbrPunctual(vec3 dc,vec3 sc,float r,float m,vec3 n,vec3 v,vec3 l,out float nl,out vec3 diff,out vec3 spec);
void main(){float u=gl_FragCoord.x/1024.0;float z=1.0-u*u;
 float phi=6.28318530718*gl_FragCoord.y/256.0;
 vec3 l=vec3(sqrt(1-z*z)*vec2(cos(phi),sin(phi)),z);
 vec3 n=vec3(0,0,1),c=vec3(.6);float r=energyRough;
 weatherMaterialLinear(vec3(0),n,c,r,energyMetal);
 vec3 specColor=mix(vec3(.04),c,energyMetal),d,s;float nl;
 pbrPunctual(c*(1-energyMetal),specColor,r,energyMetal,n,energyView,l,nl,d,s);
 frag_data[0]=vec4(s*nl*(2*u),1);frag_data[1]=frag_data[2]=frag_data[3]=vec4(0);}
'''
    ep=gl.program()
    for module in A.COMMON:gl.attach(ep,r.compile(r.source(module),'#define REFMAP_LEVEL 1\n',module))
    gl.attach(ep,r.compile(helper,'','energy helper'))
    gl.attach(ep,r.compile(energy_src,'','native BRDF integral'))
    gl.attach(ep,r.compile('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);}','','energy vertex',0x8B31))
    for i in range(4):A.bindout(ep,i,('frag_data[%d]'%i).encode())
    gl.link(ep);ok=C.c_int();gl.getprog(ep,0x8B82,C.byref(ok));assert ok.value
    energy_targets=[texture(1024,256) for _ in range(4)];energy=[]
    for metallic in (0.,1.):
        for roughness in (.2,.5,.8):
            for nv in (.1,.5,1.):
                pair=[]
                for wet in (0,1):
                    configure(ep,wet,ripple=0.)
                    r.set(ep,'energyMetal',metallic);r.set(ep,'energyRough',roughness)
                    r.set(ep,'energyView',float(np.sqrt(1-nv*nv)),0.,nv)
                    target_bind(energy_targets);gl.viewport(0,0,1024,256);gl.draw(4,0,3)
                    A.readbuffer(0x8CE0);pixels=np.empty((256,1024,4),np.float32)
                    gl.read(0,0,1024,256,0x1908,0x1406,pixels.ctypes.data)
                    assert np.all(np.isfinite(pixels))
                    pair.append(float(pixels[:,:,:3].mean()*2*np.pi))
                assert pair[1]-pair[0]<=.10, 'integrated specular gain exceeds 0.10 absolute '+str((metallic,roughness,nv,pair))
                assert pair[1]<=1.01, 'specular reflectance exceeds incident energy'
                energy.append(dict(metallic=metallic,roughness=roughness,nv=nv,off=pair[0],on=pair[1]))

    # Every new Director row reaches a setting/command and changes actual decoded GL output.
    from weather_binding_check import build, settings_table
    native=build(ROOT/'.tmp/weather_port/material'/('bindings_'+(mutation or 'baseline')+'_'+str(os.getpid())),mutation)
    settings=settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    rows={row.get('setting'):row for row in __import__('xml.etree.ElementTree',fromlist=['ElementTree']).fromstring(
        tree.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml')).iter('kit_row')}
    def set_(suffix,value):native.set(('AlchemyWeather'+suffix).encode(),value)
    def defaults():
        for key,info in settings.items():
            if key.startswith('AlchemyWeather') and info['Type'] in ('Boolean','U32','F32'):
                native.set(key.encode(),float({'true':'1','false':'0'}.get(info['Value'],info['Value'])))
        for suffix,value in {'Enabled':1,'ControlMode':1,'LocalEnabled':1,'PersistentWetEnabled':1,'PuddlesEnabled':1,
                             'RainRate':5,'PrecipVariation':0,'TransitionSeconds':0,'WetSeconds':120,'DrySeconds':300}.items():set_(suffix,value)
    def production_surface(phase=0):
        fields=np.zeros(8,np.float64);native.surface(fields.ctypes.data_as(C.POINTER(C.c_double)),phase)
        upload=np.zeros(9,np.float64);native.surface_uniforms(upload.ctypes.data_as(C.POINTER(C.c_double)))
        gl.use(pr);r.set(pr,'fixtureRoughness',.8);r.set(pr,'fixtureLegacy',0);r.set(pr,'fixtureMetal',0.)
        configure(pr,upload[0] if upload[8] else 0,puddles=upload[1],ripple=upload[2],time=.25)
        r.set(pr,'weather_material_enabled',int(upload[8]))
        # Cover dimensions/extent are the same native settings used by the producer.
        # The fixture puts a roof edge within coverage so these rows visibly change sampling.
        resolution=int(fields[5]);extent=float(fields[6])
        roof=np.ones((resolution,resolution),np.float32);roof[:,:int(resolution*.503)]=.2
        cov=texture(resolution,resolution,roof,0x822E,0x1903)
        r.bind(pr,'weather_surface_cover_map',14,cov)
        matrix(pr,'weather_surface_cover_matrix',np.array([[1/extent,0,0,-.006],[0,1/extent,0,0],[0,0,-1/64,0],[0,0,0,1]]))
        target_bind(outputs);gl.viewport(0,0,48,32);gl.draw(4,0,3)
        result=[]
        for i in (0,1):
            A.readbuffer(0x8CE0+i);pixels=np.empty((32,48,4),np.float32)
            gl.read(0,0,48,32,0x1908,0x1406,pixels.ctypes.data);result.append(pixels)
        return np.array(result)
    defaults()
    native.set(b'RenderTerrainPBRDetail',0)
    assert native.surface_caps(32,1)==1 and native.surface_caps(16,1)==0 and native.surface_caps(32,0)==0
    native.surface_caps(16,1)
    assert native.surface_row_enabled(b'weather_surface_persistentWet')==0, 'unsupported row remains enabled'
    native.set(b'RenderTerrainPBRDetail',-1)
    assert native.surface_caps(16,1)==1, 'low-detail raster material capability rejected'
    set_('PersistentWetEnabled',0)
    assert native.surface_caps(32,1)==0, 'off capability gate is active'
    assert native.surface_row_enabled(b'weather_surface_persistentWet')==1 and native.surface_row_enabled(b'weather_surface_wetAmount')==0
    native.set(b'RenderTerrainPBRDetail',0)
    set_('PersistentWetEnabled',1);set_('PuddlesEnabled',0)
    assert native.surface_row_enabled(b'weather_surface_wetAmount')==1 and native.surface_row_enabled(b'weather_surface_rippleStrength')==0
    set_('PuddlesEnabled',1)
    assert native.surface_row_enabled(b'weather_surface_rippleStrength')==1
    for suffix,value in [('PersistentWetEnabled',0),('PuddlesEnabled',0),('WetAmount',1),('PuddleAmount',1),
                         ('RippleStrength',1),('WetSeconds',1),('DrySeconds',1),('StartWetness',.7),
                         ('SurfaceCoverResolution',256),('SurfaceCoverExtent',32)]:
        row=rows['AlchemyWeather'+suffix]
        assert row.get('bind')=='setting' and row[0].get('control_name')=='AlchemyWeather'+suffix
        defaults()
        if suffix in ('DrySeconds','StartWetness'):
            set_('RainRate',0);set_('StartWetness',.5);native.command(0,3,b'')
        before=production_surface(2 if suffix in ('DrySeconds','StartWetness') else 0)
        defaults();set_(suffix,value)
        if suffix in ('DrySeconds','StartWetness'):
            set_('RainRate',0)
            if suffix=='DrySeconds':set_('StartWetness',.5)
            native.command(0,3,b'')
        after=production_surface(2 if suffix in ('DrySeconds','StartWetness') else 0)
        assert not np.array_equal(before,after), 'dead Director surface row '+suffix
        print('surface binding GL PASS',suffix,flush=True)
    defaults();set_('RainRate',0);set_('StartWetness',.7)
    applied=production_surface(2);native.command(0,4,b'');cleared=production_surface(1)
    assert applied[0,:,:,:3].min()<.59 and np.max(np.abs(cleared[0,:,:,:3]-.6))<1e-6,'Clear surfaces has no GL consumer'
    stamp=np.zeros(9,np.float64)
    native.surface_generation(16);native.surface_uniforms(stamp.ctypes.data_as(C.POINTER(C.c_double)))
    assert stamp[8]==0, 'stale cover generation is consumed'
    native.surface_generation(17);native.surface_uniforms(stamp.ctypes.data_as(C.POINTER(C.c_double)))
    assert stamp[8]==1, 'current complete cover is rejected'

    # Draw real depth with the same native shadow fragment used by the producer.
    # Terrain-only/no-roof have zero object caster batches but valid traversal.
    shadow=gl.program()
    gl.attach(shadow,r.compile(r.source('class1/deferred/shadowF.glsl'),'','native cover fragment'))
    gl.attach(shadow,r.compile(r.source('alchemy/actorFxDissolveF.glsl'),'','native cover coverage'))
    shadow_vertex='''out vec3 vary_actor_fx_position;out vec4 post_pos;out float target_pos_x;
out vec4 vertex_color;out vec2 vary_texcoord0;
uniform float coverDepth;uniform int coverShape;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
 if(coverShape==3){vec2 c[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
 p=c[gl_VertexID%6];p.x=p.x*.35+(gl_VertexID/6==0?0.:.65);}
 float z=coverDepth+(coverShape==1?p.x*.2:0.);
 if(coverShape==2){z=p.x*1.6-.8;p.x=.5;}
 gl_Position=vec4(p*2-1,z,1);post_pos=gl_Position;target_pos_x=p.x*512.;
 vary_actor_fx_position=vec3(p,z);vertex_color=vec4(1);vary_texcoord0=p;}'''
    gl.attach(shadow,r.compile(shadow_vertex,'','cover fixture vertex',0x8B31))
    gl.link(shadow);ok=C.c_int();gl.getprog(shadow,0x8B82,C.byref(ok));assert ok.value
    masked_shadow=gl.program()
    for source,label,stage in [(r.source('class1/deferred/shadowAlphaMaskF.glsl'),'native masked cover',0x8B30),
                               (r.source('alchemy/actorFxDissolveF.glsl'),'native dissolve cover',0x8B30),
                               (lookup,'native cover texture lookup',0x8B30),
                               (shadow_vertex,'canopy fixture vertex',0x8B31)]:
        gl.attach(masked_shadow,r.compile(source,'',label,stage))
    gl.link(masked_shadow);gl.getprog(masked_shadow,0x8B82,C.byref(ok));assert ok.value, 'native masked-cover link'
    canopy=np.ones((16,16,4),np.float32);canopy[:,8:,3]=0.
    canopy_texture=texture(16,16,canopy)
    cover_fbo=C.c_uint();gl.gen_fbo(1,C.byref(cover_fbo))
    depth_maps=[texture(512,512,internal=0x8CAC,fmt=0x1902) for _ in range(2)]
    depth_func=gl.fn('glDepthFunc',None,C.c_uint);depth_mask=gl.fn('glDepthMask',None,C.c_ubyte)
    clear_depth=gl.fn('glClearDepth',None,C.c_double)
    def cover_pass(index,depth=None,shape=0,masked=False):
        gl.bind_fbo(0x8D40,cover_fbo)
        gl.fbo_tex(0x8D40,0x8D00,0x0DE1,depth_maps[index],0)
        draw_none=(C.c_uint*1)(0);A.drawbuffers(1,draw_none);A.readbuffer(0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        gl.viewport(0,0,512,512);A.enable(0x0B71);depth_func(0x0201);depth_mask(1)
        clear_depth(1.);A.clear(0x0100)
        if depth is not None:
            caster=masked_shadow if masked else shadow
            gl.use(caster);r.set(caster,'coverDepth',depth);r.set(caster,'coverShape',shape)
            if masked:
                r.set(caster,'minimum_alpha',.5);r.set(caster,'shadow_alpha_mode',0)
                r.bind(caster,'fixtureDiffuse',1,canopy_texture)
            gl.draw(4,0,12 if shape==3 else 3)
        gl.disable(0x0B71)
    defaults();set_('RainRate',0);set_('StartWetness',1.);production_surface(2)
    for label,depth,flags,expected in [
        ('terrain-only',0.,(1,1,1,1,1),1),('no-roof',.4,(1,1,1,1,1),1),
        ('empty-map',None,(1,1,1,1,0),1),('roof',-.6,(1,1,1,1,1),0),
        ('failed',None,(0,1,1,1,0),0),('failed-clear',None,(1,0,1,1,0),0),
        ('incomplete',None,(1,1,0,1,1),0)]:
        cover_pass(0,depth)
        depth_sample=np.empty(1,np.float32)
        gl.read(256,256,1,1,0x1902,0x1406,depth_sample.ctypes.data)
        expected_depth=1. if depth is None else depth*.5+.5
        assert abs(float(depth_sample[0])-expected_depth)<1e-5, 'native shadow depth '+label+' '+str(depth_sample[0])
        state=native.surface_cover(*flags)
        observed=native.cover_readout(state).decode()
        assert ('Open sky' if state==1 else 'ready' if state==2 else 'failed' if state==3 else 'incomplete') in observed, 'Director cover readout has no observable state'
        upload=np.zeros(9,np.float64);native.surface_uniforms(upload.ctypes.data_as(C.POINTER(C.c_double)))
        configure(pr,upload[0] if upload[8] else 0,puddles=0.,ripple=0.)
        r.set(pr,'weather_material_enabled',int(upload[8]))
        r.bind(pr,'weather_surface_cover_map',14,depth_maps[0])
        target_bind(outputs);gl.viewport(0,0,48,32);gl.draw(4,0,3)
        A.readbuffer(0x8CE0);pixels=np.empty((32,48,4),np.float32)
        gl.read(0,0,48,32,0x1908,0x1406,pixels.ctypes.data)
        changed=bool(pixels[0,0,0]<.59)
        assert changed==bool(expected), 'surface coverage fixture '+label+' '+str((state,depth_sample[0],pixels[0,0].tolist(),upload.tolist()))
        print('cover GL PASS',label,'state',state,flush=True)
    # Native depth fragments rasterize sloped geometry, an edge-on vertical wall,
    # a genuinely alpha-cut canopy and a roof opening. Both material families
    # consume the resulting depth, rather than a CPU exposure approximation.
    geometric_materials=[program('class1/deferred/diffuseF.glsl',''),program('class1/deferred/pbropaqueF.glsl','')]
    cover_matrix=np.diag([1.,1.,-1/64,1.]);cover_matrix[:2,3]=-1.
    for label,shape,masked,open_samples in [('sloped-roof',1,False,(0,0,0)),
            ('vertical-wall',2,False,(1,1,1)),('masked-canopy',0,True,(0,1,1)),
            ('doorway',3,False,(0,1,0))]:
        cover_pass(0,-.6,shape,masked)
        for material in geometric_materials:
            references=[draw(material,w,puddles=0.,ripple=0.)[0] for w in (0,1)]
            configure(material,1,puddles=0.,ripple=0.)
            matrix(material,'weather_surface_cover_matrix',cover_matrix)
            r.bind(material,'weather_surface_cover_map',14,depth_maps[0])
            target_bind(outputs);gl.viewport(0,0,48,32);gl.draw(4,0,3)
            A.readbuffer(0x8CE0);pixels=np.empty((32,48,4),np.float32)
            gl.read(0,0,48,32,0x1908,0x1406,pixels.ctypes.data)
            for x,exposed in zip((7,24,40),open_samples):
                assert np.max(np.abs(pixels[16,x]-references[exposed][16,x]))<2e-6, 'rasterized geometry exposure '+label
        print('cover geometry GL PASS',label,'legacy/PBR',flush=True)
    native.surface_cover(1,1,1,1,0)

    # Paired GPU queries include a real D32F cover clear/raster draw in the workload.
    timing={}
    if cost:
        big=[texture(3840,2160,internal=0x881A) for _ in range(4)]
        query=C.c_uint();gl.gen_queries(1,C.byref(query))
        main=program('class1/deferred/pbropaqueF.glsl','')
        values={(views,enabled):[] for views in (1,2) for enabled in (0,1)}
        for views in (1,2):
            for iteration in range(12):
                for enabled in (iteration%2,1-iteration%2):
                    configure(main,enabled);target_bind(big);gl.viewport(0,0,3840,2160)
                    gl.begin_query(0x88BF,query)
                    if enabled:
                        cover_pass(iteration%2,0.)
                        target_bind(big);gl.viewport(0,0,3840,2160);gl.use(main)
                        r.bind(main,'weather_surface_cover_map',14,depth_maps[iteration%2])
                    for view in range(views):gl.draw(4,0,3)
                    gl.end_query(0x88BF);gl.finish()
                    elapsed=C.c_uint64();gl.query_u64(query,0x8866,C.byref(elapsed))
                    if iteration>=2:values[views,enabled].append(elapsed.value*1e-6)
        off,on=(float(np.median(values[1,k])) for k in (0,1))
        aux_off,aux_on=(float(np.median(values[2,k])) for k in (0,1))
        timing=dict(status='PENDING-QUIET',resolution=[3840,2160],off_ms=off,on_ms=on,delta_ms=on-off,
                    two_views_off_ms=aux_off,two_views_on_ms=aux_on,two_views_delta_ms=aux_on-aux_off,
                    second_view_incremental_weather_ms=(aux_on-aux_off)-(on-off),
                    new_peak_bytes=2097152+4096,budget_ms=.60,budget_bytes=4*1024**2,
                    reason='Paired native material and 512-square D32F cover fixture. Quiet scene-density traversal and whole-pipeline p95 still require the final viewer test; no acceptance claim.')
        assert timing['new_peak_bytes']<=timing['budget_bytes']
        print('4K cost',json.dumps(timing),flush=True)
    result=dict(device=device,identity_inventory=identity,wet_inventory=effects,temporal_max=temporal,brdf_integrals=energy,
                cover_fixtures=['terrain-only','no-roof','empty-map','roof','failed','failed-clear','incomplete',
                                'sloped-roof','vertical-wall','masked-canopy','doorway'],
                energy_bound='Native integrated specular gain <=0.10 absolute, total <=1.01; PBR F0 never increases; dielectric albedo 0.72..1, metal 0.98..1; legacy film <=0.02 absolute F0; roughness floor 0.12',cost=timing)
    out=ROOT/'.tmp/weather_port/material';out.mkdir(parents=True,exist_ok=True)
    (out/('result_'+(mutation or ref)+'.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('weather material GL: PASS',json.dumps(result),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    mutations=['off','screen','cover','normal','energy','gloss','surface-setting','surface-command','surface-apply','upload','generation']
    p.add_argument('--source-ref',default='WORKTREE');p.add_argument('--mutation',choices=mutations)
    p.add_argument('--mutations',action='store_true');p.add_argument('--no-cost',action='store_true')
    args=p.parse_args()
    run(args.source_ref,args.mutation,not args.no_cost)
    if args.mutations:
        expected={
            'off':'weather-off byte identity','screen':'camera-anchored puddle shimmer',
            'cover':'sheltered interior wetted','normal':'ripple normals not normalized',
            'energy':'dielectric energy bound','gloss':'PBR roughness stored in legacy gloss',
            'surface-setting':'dead Director surface row WetAmount',
            'surface-command':'Clear surfaces has no GL consumer',
            'surface-apply':'dead Director surface row DrySeconds','upload':'dead Director surface row WetAmount',
            'generation':'stale cover generation is consumed'}
        for mutation in mutations:
            r=subprocess.run([os.sys.executable,__file__,'--mutation',mutation,'--no-cost'],capture_output=True,text=True,encoding='utf-8')
            assert r.returncode!=0,'SURVIVING MUTATION '+mutation
            assert expected[mutation] in r.stderr,'control failed to execute its behavioural assertion '+r.stderr
            print('mutation rejected',mutation,r.stderr.strip().splitlines()[-1],flush=True)
