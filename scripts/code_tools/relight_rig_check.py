"""Production caller readbacks; executed by relight_shader_check.py.

Only surface/G-buffer inputs are fixtures. Deferred main bodies and forward
per-light functions, their utility modules, LUT sampling and output clamps run
unchanged. --rig-measure-only records failures for before-fix measurements.
--rig-json PATH preserves all samples, including zero-denominator raw values.
--shader-ref COMMIT reads that revision's production modules for reproducible A/B.
"""
import json
import sys
import re

measure_only = '--rig-measure-only' in sys.argv
rig_failures = []
rig_rows = []
rig_extra = []

def rig_require(condition, label):
    if not condition:
        rig_failures.append(label)

rig_inputs = '''
uniform vec3 testNormal;
uniform vec3 testLight;
uniform vec4 testShape;
uniform float testRoughness;
uniform float testGlossiness;
uniform int testComponent; // 0 diffuse; 1 specular; 2 rim only
uniform int testPbr;
uniform float testSilKeep;
uniform float testLightPower;
uniform float testFalloff;
const vec3 testPos=vec3(0,0,-4);
GBufferInfo testBuffer() {
    GBufferInfo gb;
    gb.albedo=vec4(testComponent==1 ? vec3(0) : vec3(.6),1);
    gb.specular=testPbr!=0 ? vec4(1,testRoughness,0,0) :
        vec4(testComponent==1 ? vec3(.5) : vec3(0),testGlossiness);
    gb.normal=testNormal; gb.emissive=vec4(0);
    gb.gbufferFlag=testPbr!=0 ? GBUFFER_FLAG_HAS_PBR : 0.0;
    gb.envIntensity=testComponent==0 ? 1.0 : 0.0;
    return gb;
}
'''

# Reproduce pipeline.cpp createLUTBuffers, using the default settings, storage
# precision, address and filtering. No constant/zero mock specular LUT.
import xml.etree.ElementTree as ET
settings_xml=ET.parse('indra/newview/app_settings/settings.xml').getroot().find('map')
settings_items=list(settings_xml)
def rig_setting(key):
    node=settings_items[next(i for i,n in enumerate(settings_items) if n.tag=='key' and n.text==key)+1]
    children=list(node)
    return float(children[next(i for i,n in enumerate(children) if n.tag=='key' and n.text=='Value')+1].text)
lut_x=int(rig_setting('RenderSpecularResX')); lut_y=int(rig_setting('RenderSpecularResY'))
lut_exp=rig_setting('RenderSpecularExponent')
sa=np.linspace(0,1,lut_x,dtype=np.float32)[None,:]
exponent=np.linspace(0,1,lut_y,dtype=np.float32)[:,None]**2*lut_exp
lut=np.ascontiguousarray(sa**exponent*((exponent+2)*(exponent+4)/(8*np.pi*(2**(-exponent/2)+exponent))))
rig_gloss=U();genTex(1,C.byref(rig_gloss));active(0x84C1);bindTex(0x0DE1,rig_gloss)
param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2601)
param(0x0DE1,0x2802,0x812F);param(0x0DE1,0x2803,0x812F)
teximage(0x0DE1,0,0x822D,lut_x,lut_y,0,0x1903,0x1406,lut.ctypes.data)
active(0x84C0)
clear=gl('glClear',None,U);clearcolor=gl('glClearColor',None,F,F,F,F)
clearcolor(0,0,0,0)

def rig_uniform(name,*values):
    uf[len(values)](loc(pr,name.encode()),*values)

def rig_int(name,value):
    ui(loc(pr,name.encode()),value)

def rig_read(angle,distance,radius,component,pbr,roughness=.3,glossiness=.5,
             enabled=1,physical=0,sil_keep=1,power=1,falloff=.5,channel=-1,highlight_scale=1):
    theta=np.deg2rad(angle)
    rig_uniform('testNormal',np.sin(theta),0,np.cos(theta))
    rig_uniform('testLight',0,0,distance)
    rig_uniform('testShape',radius,physical,1,channel)
    rig_uniform('relight_highlight_scale',highlight_scale)
    rig_uniform('testRoughness',roughness);rig_uniform('testGlossiness',glossiness)
    rig_uniform('testSilKeep',sil_keep);rig_uniform('testLightPower',power)
    rig_uniform('testFalloff',falloff)
    rig_uniform('color',power,power,power)
    rig_int('testComponent',component);rig_int('testPbr',pbr)
    rig_int('relight_enabled',enabled)
    rig_int('rig_rim_mode',2 if component==2 else 0)
    clear(0x4000);draw(4,0,3)
    pixels=np.empty(4,np.float32);read(0,0,1,1,0x1908,0x1406,pixels.ctypes.data)
    assert np.all(np.isfinite(pixels)),(rig_path,angle,distance,radius,component,pixels)
    return pixels.copy()

def rig_pair(*args,**kw):
    return [rig_read(*args,enabled=master,**kw) for master in (0,1)]

def rig_ratio(pair):
    return float(pair[1][0]/pair[0][0]) if pair[0][0]>1.e-12 else None

for rig_path in ('multiPoint','point','spot','alpha','material','pbralpha','pbrsplit','actorPbr'):
    deferred=rig_path in ('multiPoint','point','spot')
    material=rig_path=='material'
    alpha=rig_path=='alpha'
    if deferred:
        file='class3/deferred/'+rig_path+'LightF.glsl'
        defines='#define LIGHT_COUNT 1\n'
        original=read_shader(root/file)
        original=original.replace('getScreenCoord(vary_fragcoord)','vec2(.5)')
        original=original.replace('getPosition(tc).xyz','testPos')
        original=original.replace('getGBuffer(tc)','testBuffer()')
        # Light uniforms are fixture expressions so the real loop/spot code is
        # exercised with each sample without a second CPU lighting evaluator.
        if rig_path=='multiPoint':
            original=original.replace('light_shapes[light_idx]','testShape').replace('light_shapes[i]','testShape')
            original=original.replace('light[light_idx]','vec4(testPos+testLight,15)').replace('light[i]','vec4(testPos+testLight,15)')
            original=original.replace('light_col[light_idx]','vec4(vec3(testLightPower),testFalloff)').replace('light_col[ light_idx ]','vec4(vec3(testLightPower),testFalloff)').replace('light_col[i]','vec4(vec3(testLightPower),testFalloff)')
            original=re.sub(r'\blight\[\s*(?:light_idx|i)\s*\]', 'vec4(testPos+testLight,15)',original)
        else:
            # Keep declarations; replace references in main only.
            header,body=original.split('void main()',1)
            for name,value in {'light_shape':'testShape','trans_center':'(testPos+testLight)',
                               'proj_origin':'(testPos+testLight)','falloff':'testFalloff',
                               'color':'vec3(testLightPower)'}.items():
                body=re.sub(r'\b'+name+r'\b',value,body)
            original=header+'void main()'+body
        fragment=original.replace('/*[EXTRA_CODE_HERE]*/','/*[EXTRA_CODE_HERE]*/\n'+rig_inputs)
    else:
        file=('class3/deferred/materialF.glsl' if material else 'class2/deferred/alphaF.glsl' if alpha else
              'class2/deferred/sharedActorFxPbrF.glsl' if rig_path=='actorPbr' else 'class2/deferred/pbralphaF.glsl')
        defines='#define HAS_ACTOR_FX 1\n#define DIFFUSE_ALPHA_MODE 1\n#define USE_DIFFUSE_TEX 1\n#define ALR_RIMSPLIT_PARITY 1\n'
        if rig_path=='actorPbr': defines+='#define SHARED_ACTOR_FX_ALPHA_MODE 2\n'
        original=read_shader(root/file).replace('void main()','void unusedProductionMain()')
        if alpha or material:
            args=('vec3(testLightPower),vec3(0,0,1),' if material else 'vec3(testLightPower),')
            args+='(testComponent==1 ? vec3(0) : vec3(.6)),'
            if material: args+='vec4(testComponent==1 ? vec3(.21404114) : vec3(0),testGlossiness),'
            args+='testPos,testNormal,vec4(testPos+testLight,1),vec3(0,0,-1),(12*(1+testFalloff)-9)/15,1+testFalloff,1,'
            if material: args+='glare,'
            args+='0,testShape,vec4(1,2,.5,-2),rigRimNeutralPixel(testNormal),true,testSilKeep,1'
            call='vec3 c=calcPointLightOrSpotLight('+args+');'
        else:
            args='(testComponent==1 ? vec3(0) : vec3(.576)),vec3(.04),testRoughness,0,testNormal,testPos,vec3(0,0,1),testPos+testLight,vec3(0,0,-1),vec3(testLightPower),15,testFalloff,1,0,testShape,vec4(1,2,.5,-2),(testComponent==0 ? 0.0 : 1.0),rigRimNeutralPixel(testNormal)'
            call=('vec3 c,r;bool on;float scale;pbrCalcPointLightOrSpotLightRimSplit('+args+',c,r,on,scale);c=(c+r)*scale;' if rig_path in ('pbrsplit','actorPbr') else
                  'vec3 c=pbrCalcPointLightOrSpotLightRimKeepRef('+args+');')
        fragment=original.replace('/*[EXTRA_CODE_HERE]*/','/*[EXTRA_CODE_HERE]*/\n'+rig_inputs)
        fragment+='\nvoid main(){float glare=0;'+call+'frag_color=vec4(c,glare);}\n'
    pr=link_production(file,defines,'rig key '+rig_path,fragment);use(pr)
    rig_uniform('relight_limits',.1,4)
    rig_uniform('rig_rim_globals',1,0,.5,0)
    rig_uniform('rig_rim_light',1,2,.5,-2)
    rig_uniform('rig_rim_lights[0]',1,2,.5,-2)
    rig_uniform('gobo_tint',1,1,1)
    rig_uniform('size',15);rig_uniform('far_z',-100)
    rig_int('lightFunc',1);rig_int('proj_shadow_idx',-1)
    if rig_path=='spot':
        # Projector coordinates from production clipProjectedLightVars. Constant
        # white cookie, zero ambiance, no shadow; centre == projection origin.
        rig_uniform('proj_n',0,0,1);rig_uniform('proj_p',0,0,-5)
        rig_uniform('proj_range',15);rig_uniform('proj_focus',0);rig_uniform('proj_lod',1)
        m=(F*16)(0,0,0,0, 0,0,0,0, 0,0,0,0, .5,.5,1,1)
        matrix(loc(pr,b'proj_mat'),1,0,m)
        cookie=U();genTex(1,C.byref(cookie));active(0x84C2);bindTex(0x0DE1,cookie)
        param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
        teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,one.ctypes.data)
        rig_int('projectionMap',2);active(0x84C0)
        rig_int('gobo_pattern',-1);rig_uniform('gobo_anim_params',0,0,1,0)
    for pbr in ([0,1] if deferred else [0] if material or alpha else [1]):
        # Radii come from executed production CPU functions (not Python maths).
        # The sphere branch must reproduce explicit-radius shading on every path,
        # and the large source must wrap further than the tiny bulb.
        for name, radius in prim_source_radii.items():
            expected = {'tiny':.01,'housing':1,'panel':.01,'child':.2,
                        'attachment':.06,'fixture':1.6,'mirror':1.6,'catchlight':.37}[name]
            actual = rig_read(85,1.5,radius,0,pbr)
            explicit = rig_read(85,1.5,expected,0,pbr)
            rig_require(np.array_equal(actual,explicit),(rig_path,pbr,name,'prim sphere upload'))
            off = rig_read(85,1.5,radius,0,pbr,enabled=0)
            baseline = rig_read(85,1.5,0,0,pbr,enabled=0)
            rig_require(np.array_equal(off,baseline),(rig_path,pbr,name,'OFF sphere bytes'))
        tiny = rig_read(85,1.5,prim_source_radii['tiny'],0,pbr)
        housing = rig_read(85,1.5,prim_source_radii['housing'],0,pbr)
        rig_require(housing[0]>tiny[0],(rig_path,pbr,'prim radius sphere wrap positive control'))
        print('[RelightQ] prim sphere runtime PASS',rig_path,'PBR' if pbr else 'legacy',flush=True)
        for roughness,glossiness in ([(.3,.5),(.6,.5)] if pbr else [(.3,.25),(.3,.75)] if not alpha else [(.3,0)]):
            for distance in (1.,2.):
                for radius in (.05,.3,.6):
                    values=[]
                    for angle in (0,30,60,85,95):
                        diffuse=rig_pair(angle,distance,radius,0,pbr,roughness,glossiness)
                        specular=rig_pair(angle,distance,radius,1,pbr,roughness,glossiness)
                        rim=rig_pair(angle,distance,radius,2,pbr,roughness,glossiness)
                        # Float readbacks allow last-bit differences between
                        # normalize and inverse-sqrt; exact mathematical identity.
                        if np.cos(np.deg2rad(angle))>radius/distance:
                            rig_require(np.allclose(diffuse[0][:3],diffuse[1][:3],rtol=2.e-6,atol=2.e-7),
                                        (rig_path,'diffuse',pbr,distance,radius,angle))
                        rig_require(np.allclose(rim[0][:3],rim[1][:3],rtol=3.e-6,atol=2.e-7),
                                    (rig_path,'rim',pbr,distance,radius,angle))
                        values.append(dict(angle=angle,diffuse=[float(x[0]) for x in diffuse],
                                           diffuse_ratio=rig_ratio(diffuse),specular=[float(x[0]) for x in specular],
                                           specular_ratio=rig_ratio(specular),rim=[float(x[0]) for x in rim]))
                    # Dense normal sweep finds each peak independently, avoiding
                    # the false assertion that broadening can't brighten a tail.
                    peaks=np.zeros(2)
                    peak_samples=[]
                    for angle in np.linspace(-89,89,357):
                        pair=[float(x[0]) for x in rig_pair(angle,distance,radius,1,pbr,roughness,glossiness)]
                        peak_samples.append(pair)
                        peaks=np.maximum(peaks,pair)
                    # Half-degree sampling resolution. Width is measured at half
                    # of each lobe's own peak, so normalization cannot fake it.
                    fwhm=(np.sum(np.array(peak_samples)>peaks*.5,axis=0)*.5).tolist()
                    if not alpha:
                        rig_require(peaks[1]>0,(rig_path,'shaped specular positive control',pbr))
                        rig_require(fwhm[1]>=fwhm[0],(rig_path,'highlight width',pbr,distance,radius))
                    rig_require(peaks[1]<=peaks[0]*(1+2.e-5)+2.e-7,
                                (rig_path,'peak',pbr,distance,radius,peaks.tolist()))
                    rig_require(values[0]['diffuse'][0]>0,(rig_path,'diffuse positive control',pbr))
                    if not alpha: rig_require(peaks[0]>0,(rig_path,'specular positive control',pbr))
                    rig_require(values[3]['rim'][0]>0,(rig_path,'rim positive control',pbr))
                    rig_rows.append(dict(path=rig_path,pbr=pbr,roughness=roughness,glossiness=glossiness,
                                         distance=distance,radius=radius,values=values,peaks=peaks.tolist(),fwhm=fwhm,
                                         peak_ratio=float(peaks[1]/peaks[0]) if peaks[0]>1.e-12 else None))
        physical_rim=rig_pair(85,1.5,.3,2,pbr,physical=1)
        rig_extra.append(dict(path=rig_path,pbr=pbr,kind='physical_rim',values=[x.tolist() for x in physical_rim]))
        d,range_,falloff=1.5,15.,.5
        physical_atten=(1-(d/range_)**4)**2/(d*d)
        legacy_atten=2*(1-(d/range_+falloff)/(1+falloff))**2
        expected_rim=physical_rim[0][:3]*(physical_atten/legacy_atten)
        rig_require(np.allclose(expected_rim,physical_rim[1][:3],rtol=3.e-6,atol=2.e-7),(rig_path,'physical rim',pbr))
        # Execute every production caller at zero, not just the helper: physical
        # mode must not disappear at zero source radius (the old cliff).
        zero=rig_read(0,1.5,0,0,pbr,physical=1)
        small=rig_read(0,1.5,.01,0,pbr,physical=1)
        rig_require(np.allclose(zero,small,rtol=2.e-5,atol=2.e-7),(rig_path,'zero physical continuity',pbr))
        if not alpha:
            hard=rig_read(0,2,.45,1,pbr,highlight_scale=0)
            point=rig_read(0,2,0,1,pbr)
            rig_require(np.allclose(hard,point,rtol=3.e-5,atol=2.e-7),(rig_path,'highlight scale zero',pbr))
        diffuse_hard=rig_read(85,1.5,.45,0,pbr,highlight_scale=0)
        diffuse_soft=rig_read(85,1.5,.45,0,pbr,highlight_scale=1)
        rig_require(np.array_equal(diffuse_hard,diffuse_soft),(rig_path,'diffuse scale identity',pbr))
    if material:
        for angle in (0,60,85):
            for sil_keep in (0,.25,1):
                pair=rig_pair(angle,1,1.e-6,1,0,glossiness=.05,sil_keep=sil_keep)
                rig_extra.append(dict(path=rig_path,kind='tiny_legacy',angle=angle,sil_keep=sil_keep,values=[x.tolist() for x in pair]))
                rig_require(np.allclose(pair[0],pair[1],rtol=2.e-3,atol=1.e-9),('material','tiny radius / glare',angle,sil_keep))
        pair=rig_pair(0,1,.3,1,0,glossiness=.75,power=8,falloff=0)
        rig_extra.append(dict(path=rig_path,kind='legacy_clamp',values=[x.tolist() for x in pair]))
        rig_require(pair[1][0]<=1,('material','per-light clamp',pair[1].tolist()))
    if deferred and not shader_ref:
        # Production light main bodies consume both RGBA planes, including when
        # Phase 1 is disabled. OFF and unselected channels must remain identical.
        shadow_values=np.array([[0,64,128,192],[255,204,153,102]],np.uint8)
        for plane in range(2):
            mask=U();genTex(1,C.byref(mask));active(0x84C0+6+plane);bindTex(0x0DE1,mask)
            param(0x0DE1,0x2801,0x2600);param(0x0DE1,0x2800,0x2600)
            teximage(0x0DE1,0,0x8058,1,1,0,0x1908,0x1401,shadow_values[plane].ctypes.data)
            rig_int('area_shadow_map'+str(plane),6+plane)
        for pbr in (0,1):
            for shape_master in (0,1):
                baseline=rig_read(0,1,.3,0,pbr,enabled=shape_master)
                for channel in range(8):
                    shaded=rig_read(0,1,.3,0,pbr,enabled=shape_master|2,channel=channel)
                    expected=baseline*float(shadow_values[channel//4,channel%4])/255
                    assert np.allclose(shaded,expected,rtol=3.e-6,atol=2.e-7),(rig_path,pbr,shape_master,channel,shaded,expected)
                unselected=rig_read(0,1,.3,0,pbr,enabled=shape_master|2)
                assert unselected.tobytes()==baseline.tobytes(),(rig_path,'unselected identity')
                off=rig_read(0,1,.3,0,pbr,enabled=shape_master,channel=0)
                assert off.tobytes()==baseline.tobytes(),(rig_path,'stale channel OFF identity')
        active(0x84C0)
        print('[RelightQ] area lighting runtime PASS',rig_path,'eight channels, legacy/PBR, shape on/off, stale master and unselected identity',flush=True)
    print('[RelightQ] rig measured',rig_path,flush=True)
    if '--rig-json' in sys.argv:
        Path(sys.argv[sys.argv.index('--rig-json')+1]).write_text(json.dumps(
            dict(lut=dict(width=lut_x,height=lut_y,exponent=lut_exp),rows=rig_rows,failures=rig_failures),indent=2)+'\n')

rig_result=dict(shader_ref=shader_ref or 'worktree',lut=dict(width=lut_x,height=lut_y,exponent=lut_exp),rows=rig_rows,extra=rig_extra,failures=rig_failures)
if '--rig-json' in sys.argv:
    Path(sys.argv[sys.argv.index('--rig-json')+1]).write_text(json.dumps(rig_result,indent=2)+'\n')
print('[RelightQ] rig matrix',len(rig_rows),'rows;',len(rig_failures),'failures',flush=True)
assert measure_only or not rig_failures,rig_failures[:20]
