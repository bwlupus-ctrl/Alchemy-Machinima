"""1080p SL-map/contact proof using production gather, filter and hybrid blocks.

Loaded by the area gate in its WGL context. No viewer or reference implementation.
"""
from pathlib import Path
import os
import hashlib
import subprocess
import xml.etree.ElementTree as ET
from relight_area_policy_check import method


def check_contact_cpu():
    pipeline = method(Path('indra/newview/pipeline.cpp'), 'void LLPipeline::renderAreaShadows(')
    classify = pipeline[pipeline.index('        bool shadow_map = false;'):
                        pipeline.index('        const F32 distance = (drawable->')]
    packing = pipeline[pipeline.index('                // Sample the same current slots/fades'):
                       pipeline.index('                const glm::vec3 centre =')]
    setup = method(Path('indra/newview/pipeline.cpp'), 'F32 LLPipeline::setupSpotLight(')
    assert '1.f-mSpotLightFade[s_idx]' in setup
    # The native generation/fade update precedes deferred lighting; the gather
    # and setupSpotLight consume the same state, with no auction/fade mutation.
    lighting = method(Path('indra/newview/pipeline.cpp'), 'void LLPipeline::renderDeferredLighting(')
    assert 'generateSunShadow(' not in lighting and 'mSpotLightFade[' not in lighting
    source = Path('indra/newview/pipeline.cpp').read_text()
    save = method(Path('indra/newview/pipeline.cpp'), 'bool LLPipeline::beginPrismAuxiliaryState()')
    restore = method(Path('indra/newview/pipeline.cpp'), 'void LLPipeline::endPrismAuxiliaryState()')
    save = save[save.index('        mPrismSavedShadowSpotLight[i] ='):save.index('        // [Prism spot shadows Stage 1]')]
    restore = restore[restore.index('        mShadowSpotLight[i] ='):restore.index('        // [Prism spot shadows Stage 1]')]
    aux = source[source.index('        mShadowSpotLight[i] = drawable;', source.index('void LLPipeline::generatePrismSpotShadows(')):]
    aux = aux[:aux.index('        set_current_modelview(view);')]
    take = method(Path('indra/newview/pipeline.cpp'), 'void LLPipeline::logRelightTake(')
    keys = take[take.index('    for (const char* key : {'):take.index('    std::array<LLUUID, MAX_SPOT_SHADOWS> slots;')]
    fixture = r'''
#include "alareashadowmath.h"
#include <cassert>
#include <limits>
#include <iostream>
#include <map>
#include <string>
using U32=unsigned; using F32=float;
template<class T> T llclamp(T x,T a,T b) { return std::clamp(x,a,b); }
struct Vector { float mV[4]={}; };
struct LLSD {
    enum Type { TypeBoolean, TypeInteger, TypeReal };
    double value=0; std::map<std::string,double> fields;
    Type type() const { return TypeReal; }
    bool asBoolean() const { return value!=0; }
    int asInteger() const { return int(value); }
    double asReal() const { return value; }
    double& operator[](const char* key) { return fields[key]; }
};
struct LLControlVariable { LLSD value; LLSD getValue() const { return value; } };
struct Settings {
    LLControlVariable control;
    LLControlVariable* getControl(const char*) { return &control; }
} gSavedSettings;
int main() {
    constexpr unsigned MAX_SPOT_SHADOWS=10;
    int objects[11]={}; int* mShadowSpotLight[10]={};
    int* mTargetShadowSpotLight[10]={}; float mSpotLightFade[10]={};
    int* mPrismSavedShadowSpotLight[10]={}; int* mPrismSavedTargetShadowSpotLight[10]={};
    float mPrismSavedSpotLightFade[10]={};
    auto mapped=[&](int* drawable,bool projector) {
''' + classify + r'''
        return shadow_map;
    };
    ALAreaShadow::Selection<int> selected; unsigned plane=0;
    float setting=.15f;
    auto mapped_contact=[&]() { return setting; };
    auto pack=[&](int* drawable) {
        Vector contact, contact_fade;
        for(unsigned i=0;i<4;++i) {
''' + packing + r'''
        }
        return std::make_pair(contact,contact_fade);
    };
    for(unsigned slot=0;slot<MAX_SPOT_SHADOWS;++slot) {
        for(auto& owner:mShadowSpotLight) owner=nullptr;
        mShadowSpotLight[slot]=objects+slot;
        assert(mapped(objects+slot,true));
        assert(!mapped(objects+slot,false));
        assert(!mapped(objects+10,true));
        for(unsigned i=0;i<8;++i) selected.lights[i].shadowMap=i==slot%8;
        // Acquisition, retained zero owner (no pending target), eviction.
        for(float fade:{0.f,.25f,.5f,.75f,1.f,.75f,.5f,.25f,0.f,0.f}) {
            for(auto& f:mSpotLightFade) f=.23f; // wrong fade index must fail
            mSpotLightFade[slot]=fade;
            for(plane=0;plane<2;++plane) {
                setting=.15f; auto contact=pack(objects+slot);
                for(unsigned i=0;i<4;++i) {
                    bool owned=plane*4+i==slot%8;
                    assert(contact.first.mV[i]==(owned ? .15f:0.f));
                    assert(contact.second.mV[i]==(owned ? fade:0.f));
                }
            }
            // Execute actual native save/swap/restore assignments across VCam.
            for(unsigned i=0;i<MAX_SPOT_SHADOWS;++i) {
''' + save + r'''
                mShadowSpotLight[i]=nullptr;
            }
            unsigned i=(slot+3)%10; int* drawable=objects+10;
''' + aux + r'''
            plane=0; selected.lights[0].shadowMap=mapped(drawable,true);
            assert(pack(drawable).first.mV[0]==.15f && pack(drawable).second.mV[0]==1.f);
            assert(pack(objects+slot).second.mV[0]==0.f); // main owner has no aux map
            for(unsigned i=0;i<MAX_SPOT_SHADOWS;++i) {
''' + restore + r'''
            }
            assert(mShadowSpotLight[slot]==objects+slot && mSpotLightFade[slot]==fade);
            assert(!mapped(objects+10,true));
            for(unsigned i=0;i<8;++i) selected.lights[i].shadowMap=i==slot%8;
        }
    }
    plane=0; selected.lights[0].shadowMap=true;
    mShadowSpotLight[9]=objects+9; mSpotLightFade[9]=1;
    for(float value:{0.f,-1.f,.01f,.15f,8.f,100.f,
                     std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        setting=value;
        assert(pack(objects+9).first.mV[0]==(std::isfinite(value) ? std::clamp(value,.001f,64.f):.15f));
    }
    // Emit production-packed contact distance/fade for GPU handoff sweeps.
    setting=.15f;
    for(float fade:{0.f,.25f,.5f,.75f,1.f}) {
        mSpotLightFade[9]=fade;
        auto packed=pack(objects+9);
        std::cout << packed.first.mV[0] << " " << packed.second.mV[0] << " ";
    }
    std::cout << "\n";
    for(double cap:{0.,.15}) {
        gSavedSettings.control.value.value=cap;
        LLSD settings;
''' + keys + r'''
        assert(settings.fields.count("RenderAreaShadowMappedContact"));
        assert(settings["RenderAreaShadowMappedContact"]==cap);
    }
}
'''
    directory = Path('.tmp/relight_contact_cpu').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory/'contact.cpp').write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory/'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc '
        f'/I"{Path("indra/newview").resolve()}" /Fo"{directory / "contact.obj"}" '
        f'/Fe"{directory / "contact.exe"}" "{directory / "contact.cpp"}"\nexit /b %ERRORLEVEL%\n')
    subprocess.run(['cmd','/d','/c',str(directory/'compile.cmd')], check=True)
    packed = [float(x) for x in subprocess.check_output([str(directory/'contact.exe')], text=True).split()]
    transitions = list(zip(packed[::2],packed[1::2]))
    assert transitions == [(.15,f) for f in (0.,.25,.5,.75,1.)], transitions
    # Mutate the actual CPU upload. Each executable must fail the same assertions.
    for name, before, after in (
        ('binary_cap', 'contact_fade.mV[i] = fade', 'contact_fade.mV[i] = 1.f'),
        ('wrong_fade_slot', 'mSpotLightFade[slot]', 'mSpotLightFade[0]')):
        assert before in fixture
        mutant = fixture.replace(packing, packing.replace(before, after))
        (directory/'contact.cpp').write_text(mutant, encoding='utf-8')
        subprocess.run(['cmd','/d','/c',str(directory/'compile.cmd')], check=True, capture_output=True)
        result = subprocess.run([str(directory/'contact.exe')], capture_output=True)
        assert result.returncode != 0, name + ' mutation escaped'
        print('[RelightQ] contact CPU mutation rejected: ' + name, flush=True)
    (directory/'contact.cpp').write_text(fixture, encoding='utf-8')
    settings = ET.parse('indra/newview/app_settings/settings.xml').getroot().find('map')
    children = list(settings)
    value = children[next(i for i,x in enumerate(children) if x.text=='RenderAreaShadowMappedContact')+1]
    fields = {list(value)[i].text:list(value)[i+1].text for i in range(0,len(value),2)}
    assert fields['Type']=='F32' and fields['Persist']=='1' and float(fields['Value'])==.15
    print('[RelightQ] contact CPU PASS all 10 slots/fades/planes, main/VCam save-swap-restore, live caps and logged 0/.15', flush=True)
    return transitions


def check_contact(g):
    transitions = check_contact_cpu()
    # Share the gate's real GL helpers, with local names for readability.
    np, C, F = (g[n] for n in ('np','C','F'))
    loc, ui, uf = (g[n] for n in ('loc','ui','uf'))
    use, matrix = (g[n] for n in ('area_use','camera_matrix'))
    delete_texture = g['gl']('glDeleteTextures',None,g['I'],C.POINTER(g['U']))
    textures = {}
    def texture(unit,data,internal=0x8814):
        if unit not in textures:
            textures[unit]=g['area_texture'](unit,data,internal)
        else:
            g['active'](0x84C0+unit);g['bindTex'](0x0DE1,textures[unit])
            g['teximage'](0x0DE1,0,internal,data.shape[1],data.shape[0],0,0x1908,0x1406,data.ctypes.data)
        return textures[unit]
    # Dense continuity sweeps reuse their render targets. Every draw overwrites
    # the full target; this changes allocation overhead only, not shader inputs.
    outputs={}
    def draw(pr,width,height,probe=False,floating=False):
        key=(width,height,floating)
        if key not in outputs:
            outputs[key]=g['area_texture'](0,np.zeros((height,width,4),np.float32),0x8814 if floating else 0x8058)
        else:
            g['active'](0x84C0);g['bindTex'](0x0DE1,outputs[key])
        g['attachment'](0x8D40,0x8CE0,0x0DE1,outputs[key],0)
        assert g['status'](0x8D40)==0x8CD5
        g['viewport'](0,0,width,height)
        g['use'](pr) if probe else use(pr)
        g['draw'](4,0,3)
        result=np.empty((height,width,4),np.float32 if floating else np.uint8)
        g['read'](0,0,width,height,0x1908,0x1406 if floating else 0x1401,result.ctypes.data)
        return result
    uniform = g['uniform4fv']
    width,height = 1920,1080
    projection = np.eye(4,dtype=np.float32); projection[0,0]=.5; projection[2,2]=-.1
    inverse = np.linalg.inv(projection)
    # Independently compiled committed baseline, not a test-edited current shader.
    old_ref=g['shader_ref']
    try:
        g['shader_ref']='a63161d0028'; g['read_shader'].cache_clear()
        baseline=g['link_production']('class3/deferred/areaShadowF.glsl','','HEAD contact baseline')
    finally:
        g['shader_ref']=old_ref; g['read_shader'].cache_clear()
    gather=g['area_gather']; filt=g['area_filter']
    gather_source=Path('indra/newview/app_settings/shaders/class3/deferred/areaShadowF.glsl').read_text()
    length_source=gather_source.replace('vec2 areaTrace(vec3 origin', 'float areaTrace(vec3 origin')
    # Use scalar HEAD trace signature and round 1's fade-dependent max length.
    begin=length_source.index('float areaTrace(')
    end=length_source.index('uint areaHash',begin)
    length_source=length_source[:begin]+'''float areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
        float light_bias, float max_distance, float thickness, int steps, float offset);
'''+length_source[end:]
    begin=length_source.index('            vec2 traced =')
    end=length_source.index('        }',begin)
    length_source=length_source[:begin]+'''            visibility += areaTrace(origin,target,area_lights[light].xyz,end_gap,
                area_light_bias,min(area_params.x,area_contact_dist[light]),area_params.y,area_steps,0.25+0.5*jitter);
'''+length_source[end:]
    begin=length_source.index('        // Filter the blended')
    length_source=length_source[:begin]+'''        frag_color[light] = visibility/float(area_samples);
    }
}
'''
    # link_production uses a gl_VertexID vertex for test fragments.
    length_source='uniform vec2 area_trace_res;\n'+length_source
    length_source=length_source.replace('areaTexelUV(vary_fragcoord)','areaTexelUV(gl_FragCoord.xy/area_trace_res)')
    length_mutation=g['link_production']('class3/deferred/areaShadowF.glsl','','rejected length interpolation',test_fragment=length_source)

    source=Path('indra/newview/app_settings/shaders/class3/deferred/spotLightF.glsl').read_text()
    visibility=source[source.index('uniform int relight_enabled;'):source.index('out vec4 frag_color;')]
    hybrid=source[source.index('    float shadow = 1.0;'):source.index('    GBufferInfo gb =')]
    fragment=visibility+'''
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
'''+hybrid+'''
    frag_color=vec4(shadow);
}
'''
    compose=g['link_production']('class3/deferred/spotLightF.glsl','','production SL contact hybrid',test_fragment=fragment)
    product_fragment=fragment.replace('min(shadow, areaShadowVisibility(tc, light_shape.w))',
                                      'shadow * areaShadowVisibility(tc, light_shape.w)')
    assert product_fragment!=fragment,'mutation must replace production min'
    multiplied=g['link_production']('class3/deferred/spotLightF.glsl','','rejected product hybrid',
                                   test_fragment=product_fragment)
    normal=np.zeros((height,width,4),np.float32); normal[:,:,:2]=.5
    depth=np.full_like(normal,.7) # receiver Z=-4, X in [-2,2]
    x=(np.arange(width)+.5)/width*4-2
    # A foreground half-plane; its screen shadow begins near X=-1.
    blocker=x>=0
    def spatial(raw,half,floating=False):
        w,h=(width//2,height//2) if half else (width,height)
        # Production spatial passes and full-resolution upsample.
        for axis in ((1,0),(0,1),(0,0)):
            texture(3,raw.astype(np.float32)/(255 if raw.dtype==np.uint8 else 1),0x8814 if floating else 0x8058);use(filt)
            matrix(filt,b'inv_proj',inverse)
            ui(loc(filt,b'depthMap'),1);ui(loc(filt,b'normalMap'),2);ui(loc(filt,b'area_source'),3)
            ui(loc(filt,b'area_debug_channel'),0)
            uf[1](loc(filt,b'area_radius'),1 if half else 2)
            uf[2](loc(filt,b'area_axis'),*axis)
            raw=draw(filt,width if axis==(0,0) else w,height if axis==(0,0) else h,floating=floating)
        return raw

    screen_gap=None
    def screen(gap,cap,pr=gather,radius=.04,half=False,seed=12345,fade=1.,steps=12,with_gather=False):
        nonlocal screen_gap
        if screen_gap!=gap:
            depth[:]=.7
            # .7 is -4; blocker toward camera is -4+gap.
            depth[:,blocker]=(.4-gap*.1+1)/2
            texture(1,depth); texture(2,normal)
            screen_gap=gap
        g['use'](pr) if pr==length_mutation else use(pr)
        matrix(pr,b'projection_matrix',projection); matrix(pr,b'inv_proj',inverse)
        ui(loc(pr,b'depthMap'),1);ui(loc(pr,b'normalMap'),2)
        ui(loc(pr,b'area_count'),1);ui(loc(pr,b'area_first'),0)
        ui(loc(pr,b'area_samples'),8);ui(loc(pr,b'area_steps'),steps)
        uf[3](loc(pr,b'area_params'),8,.35,.5 if half else 1)
        uf[1](loc(pr,b'area_light_bias'),.25)
        w,h=(width//2,height//2) if half else (width,height)
        uf[2](loc(pr,b'area_trace_res'),w,h)
        lights=np.array([[1,0,-2,radius]]*4,np.float32)
        ids=np.array([seed]*4,np.uint32)
        uniform(loc(pr,b'area_lights'),4,lights.ctypes.data_as(C.POINTER(F)))
        g['uniform4uiv'](loc(pr,b'area_seeds'),1,ids.ctypes.data_as(C.POINTER(g['U'])))
        uf[4](loc(pr,b'area_contact_dist'),cap,0,0,0)
        uf[4](loc(pr,b'area_contact_fade'),fade if cap>0 else 0,0,0,0)
        raw=draw(pr,w,h,probe=pr==length_mutation)
        filtered=spatial(raw,half)
        return (raw,filtered) if with_gather else filtered

    # Ten independently identifiable depth maps AND matrices. A forced index 0
    # executable is used as a one-map reference by remapping each slot to zero.
    natives=[]; shadow_matrices=[]
    for slot in range(10):
        native=g['U']();g['genTex'](1,C.byref(native));g['active'](0x84C0+6+slot);g['bindTex'](0x0DE1,native)
        for key,value in ((0x2801,0x2601),(0x2800,0x2601),(0x2802,0x812F),(0x2803,0x812F),
                          (0x884C,0x884E),(0x884D,0x0203)):
            g['param'](0x0DE1,key,value)
        # Native shadow lies left of the screen edge, leaving a lit gap. The
        # full-distance negative must show both edges even at native fade one.
        native_depth=np.where(x[None,:]<-1.4+.025*slot,.1,.9).astype(np.float32)
        g['teximage'](0x0DE1,0,0x81A6,width,1,0,0x1902,0x1406,native_depth.ctypes.data)
        shadow_matrix=np.zeros((4,4),np.float32)
        shadow_matrix[0,0]=.25;shadow_matrix[0,3]=.5+.002*slot
        shadow_matrix[1,1]=.5;shadow_matrix[1,3]=.5
        shadow_matrix[2,3]=.5;shadow_matrix[3,3]=1
        natives.append(native);shadow_matrices.append(shadow_matrix)
    wrong_fragment=fragment.replace('spot_norm, proj_shadow_idx, tc)', 'spot_norm, 0, tc)')
    assert wrong_fragment!=fragment
    wrong_slot=g['link_production']('class3/deferred/spotLightF.glsl','','rejected hardcoded slot 0',test_fragment=wrong_fragment)
    # A separate native all-lit depth map isolates the near-contact positive
    # control at full native fade, independently of the displaced main edge.
    lit_native=g['U']();g['genTex'](1,C.byref(lit_native));g['active'](0x84C0+16);g['bindTex'](0x0DE1,lit_native)
    for key,value in ((0x2801,0x2601),(0x2800,0x2601),(0x884C,0x884E),(0x884D,0x0203)):
        g['param'](0x0DE1,key,value)
    lit_depth=np.full((1,1),.9,np.float32)
    g['teximage'](0x0DE1,0,0x81A6,1,1,0,0x1902,0x1406,lit_depth.ctypes.data)
    def hybrid_read(visibility,slot=0,enabled=True,fade=.6,pr=compose,remap=None,lit_map=False,floating=False):
        compose=pr
        texture(4,visibility.astype(np.float32)/(255 if visibility.dtype==np.uint8 else 1),
                0x8814 if floating else 0x8058);g['use'](compose)
        matrix(compose,b'inv_proj',inverse)
        uf[2](loc(compose,b'screen_res'),width,height)
        ui(loc(compose,b'depthMap'),1);ui(loc(compose,b'normalMap'),2)
        ui(loc(compose,b'area_shadow_map0'),4);ui(loc(compose,b'area_shadow_map1'),4)
        for n in range(10):
            source_slot = remap if n==0 and remap is not None else n
            ui(loc(compose,('shadowMap%d'%(4+n)).encode()),16 if lit_map else 6+source_slot)
            matrix(compose,('shadow_matrix[%d]'%(4+n)).encode(),shadow_matrices[source_slot])
        uf[2](loc(compose,b'proj_shadow_res'),width,height)
        uf[4](loc(compose,b'shadow_clip'),10,20,40,80)
        uf[4](loc(compose,b'light_shape'),0,0,0,0)
        uf[1](loc(compose,b'spot_shadow_bias'),0);uf[1](loc(compose,b'spot_shadow_offset'),0)
        ui(loc(compose,b'proj_shadow_idx'),slot);uf[1](loc(compose,b'shadow_fade'),1-fade)
        ui(loc(compose,b'relight_enabled'),2 if enabled else 0)
        return draw(compose,width,height,probe=True,floating=floating)

    roi=(x> -1.8)&(x<-.15) # only receiver, clear of the visible blocker
    for half in (False,True):
        capped=screen(1,.15,half=half)
        assert np.all(capped[:,roi,0]==255),'far blocker must add no screen edge'
        map_only=hybrid_read(capped,enabled=False,fade=1)
        row=map_only[height//2,roi,0].astype(int)
        changes=np.flatnonzero(np.diff(row)!=0)
        assert len(changes)>0 and np.max(np.diff(changes),initial=0)<=1,'native map must have exactly one boundary'
        slot_edges=[]
        for slot in range(10):
            result=hybrid_read(capped,slot)
            expected=hybrid_read(capped,pr=wrong_slot,remap=slot)
            assert np.array_equal(result[:,roi],expected[:,roi]),('slot map AND matrix',slot,half)
            native_only=hybrid_read(capped,slot,enabled=False)
            assert np.array_equal(result[:,roi],native_only[:,roi]),('one map-only edge',slot,half)
            slot_edges.append(np.flatnonzero(np.diff(native_only[height//2,:,0].astype(int)))[0])
            if slot:
                mutated=hybrid_read(capped,slot,pr=wrong_slot)
                assert not np.array_equal(mutated[:,roi],expected[:,roi]),('hardcoded slot 0 escaped',slot)
        assert len(set(slot_edges))==10,slot_edges
        full=screen(1,8,half=half)
        negative=hybrid_read(full,fade=1)
        delta=np.abs(np.diff(negative[height//2,roi,0].astype(int)))
        groups=np.flatnonzero(delta>0)
        assert np.any(np.diff(groups)>20),'full-distance negative control must reproduce two separated boundaries'
        assert np.any(negative[:,roi,0]<map_only[:,roi,0]),'negative control must change hybrid'
        assert np.array_equal(full,screen(1,0,half=half)),'full-distance cap reproduces uncapped gather'
        # Independent coverage-transfer envelope: no adjacent image may move
        # faster than the visibility weight. It does not use the current trace
        # as its expected result. Test both displaced and independently lit maps.
        def continuity(previous,current,df,label,quantized_passes=4):
            change=np.max(abs(current.astype(np.int16)-previous.astype(np.int16)))
            # Gather has its own strict 1-LSB oracle below. Each subsequent
            # RGBA8 H/V/upsample/native write can add a quantization LSB;
            # floating point near rounding ties can otherwise amplify it.
            bound=np.ceil(255*df)+quantized_passes
            assert change<=bound,('coverage transfer',label,half,df,change,bound)
            return int(change)

        frames=[]
        for cap,fade in transitions:
            traced=screen(1,cap,half=half,fade=fade)
            pair=[]
            for lit_map in (False,True):
                current=hybrid_read(traced,fade=fade,lit_map=lit_map)
                if frames:
                    continuity(frames[-1][int(lit_map)],current,.25,'quarter acquisition')
                pair.append(current)
            frames.append(pair)
            for slot in range(10):
                current=hybrid_read(traced,slot,fade=fade)
                native_only=hybrid_read(traced,slot,enabled=False,fade=fade)
                expected=np.minimum(native_only,traced[:,:,:1])
                assert np.max(abs(current.astype(int)-expected.astype(int)))<=1,('fade min oracle',slot,fade)
                if fade==0:
                    assert np.array_equal(traced,screen(1,0,pr=baseline,half=half)),('fade zero HEAD bytes',slot,half)
                    assert np.array_equal(current,hybrid_read(full,slot=-1)),('acquire/evict endpoint',slot,half)
                if fade==1:
                    assert np.array_equal(current[:,roi],native_only[:,roi]),('settled single map boundary',slot,half)
        for index in range(len(transitions)-1,-1,-1):
            cap,fade=transitions[index]
            traced=screen(1,cap,half=half,fade=fade)
            for lit_map in (False,True):
                replay=hybrid_read(traced,fade=fade,lit_map=lit_map)
                assert np.array_equal(replay,frames[index][int(lit_map)]),'eviction/repeat current-frame identity'
                if index<len(transitions)-1:
                    continuity(frames[index+1][int(lit_map)],replay,.25,'quarter eviction')
        binary=hybrid_read(capped,fade=0)
        assert np.any(binary[:,roi]!=frames[0][0][:,roi]),'binary cap mutation must fail acquisition oracle'

        for kind,seed in (('point',7),('unmapped projector',54321)):
            current=screen(1,0,half=half,seed=seed)
            head=screen(1,0,pr=baseline,half=half,seed=seed)
            assert np.array_equal(current,head),(kind,'HEAD output bytes',half)
        # Near blockers outside the unchanged receiver normal bias: 3/5/10 cm.
        # Use the same 64-step density as the HEAD control: shortened rays no
        # longer concentrate 12 steps inside 15 cm. Medium may miss thin gaps.
        # Native map is independently lit so the screen contribution matters.
        for gap in (.03,.05,.10):
            medium=screen(gap,.15,half=half)
            medium_head=screen(gap,0,pr=baseline,half=half)
            assert np.all(medium>=medium_head),'Medium contact is a subset of HEAD hits'
            assert np.array_equal(screen(gap,.15,half=half,fade=0),medium_head),'near-gap fade zero HEAD bytes'
            print('[RelightQ] Medium %.0fcm gap %s: contact/HEAD minimum %d/%d at unchanged 12 steps' %
                  (gap*100,'half' if half else 'full',medium[:,(x>-.09)&(x<-.005),0].min(),
                   medium_head[:,(x>-.09)&(x<-.005),0].min()),flush=True)
            contact=screen(gap,.15,half=half,steps=64)
            near_roi=(x>-.09)&(x<-.005)
            assert np.any(contact[:,near_roi,0]<255),('near contact positive',gap,half)
            head=screen(gap,0,pr=baseline,half=half,steps=64)
            assert np.any(head[:,near_roi,0]<255),'HEAD near-contact positive control'
            assert np.all(contact[:,near_roi,0]>=head[:,near_roi,0]),'contact is a subset of HEAD hits'
            lit=hybrid_read(contact,fade=1,enabled=False,lit_map=True)
            added=hybrid_read(contact,fade=1,lit_map=True)
            assert np.all(lit[:,near_roi]==255) and np.any(added[:,near_roi,0]<255),'contact adds to independently lit map'
            assert np.array_equal(contact,screen(gap,.15,half=half,steps=64)),'contact repeat bytes'
        assert np.all(screen(.10,.001,half=half)[:,roi,0]==255),'zero setting disables mapped contact'
        print('[RelightQ] SL map + contact endpoints PASS 1920x1080 %s: 10 distinct maps/matrices; slot-0 and binary-cap mutations rejected; fades 0/.25/.5/.75/1 acquire/evict, '
              'far-blocker/full-distance double-edge negative, 3/5/10cm contacts at HEAD step density, unmapped/point HEAD bytes' %
              ('Medium half' if half else 'full'),flush=True)
        # Preserve round 1's exact length-interpolation mechanism as a mutation.
        # It must fail the SAME adjacent-fade envelope with a lit native map.
        previous=None; previous_gather=None; rejected=False
        for index in range(1001):
            fade=index/1000
            gathered,traced=screen(1,(1-fade)*8+fade*.15,pr=length_mutation,half=half,with_gather=True)
            current=hybrid_read(traced,fade=fade,lit_map=True)
            if previous is not None:
                try:
                    continuity(previous_gather,gathered,.001,'length interpolation mutation gather',quantized_passes=0)
                    continuity(previous,current,.001,'length interpolation mutation')
                except AssertionError as error:
                    print('[RelightQ] length interpolation mutation rejected:',error,flush=True)
                    rejected=True
                    break
            previous=current
            previous_gather=gathered
        assert rejected,'length-interpolation mutation escaped continuity gate'

        # Dense adjacent fades, including exact .25/.75, exercise the actual
        # gather and ALL spatial passes. Reverse replay tests eviction too.
        previous=None; previous_gather=None; dense=[]; maximum=0; gather_maximum=0; quantization_proved=False
        for index in range(1001):
            fade=index/1000
            gathered,traced=screen(1,.15,half=half,fade=fade,with_gather=True)
            pair=[hybrid_read(traced,fade=fade,lit_map=lit) for lit in (False,True)]
            if previous is not None:
                # An independent coverage-transfer bound BEFORE spatial
                # quantization: at .001, no gather byte may move by >1.
                gather_maximum=max(gather_maximum,continuity(previous_gather,gathered,.001,
                    ('gather acquisition',fade),quantized_passes=0))
                for lit in (0,1):
                    maximum=max(maximum,continuity(previous[lit],pair[lit],.001,('fine acquisition',fade)))
                if not quantization_proved and any(np.max(abs(pair[lit].astype(np.int16)-previous[lit].astype(np.int16)))>2 for lit in (0,1)):
                    # Prove the extra bytes come from spatial quantization:
                    # replay the SAME RGBA8 gathers through the SAME filter
                    # and hybrid shaders in FP32 targets (no intermediate
                    # rounding). Keep a strict <=1-LSB visibility bound here.
                    before=spatial(previous_gather,half,floating=True)
                    after=spatial(gathered,half,floating=True)
                    fp_maximum=0.
                    for lit in (False,True):
                        a=hybrid_read(before,fade=fade-.001,lit_map=lit,floating=True)
                        b=hybrid_read(after,fade=fade,lit_map=lit,floating=True)
                        fp_maximum=max(fp_maximum,float(np.max(abs(a-b))))
                    assert fp_maximum<=1/255+2.e-6,('unrounded spatial continuity',half,fade,fp_maximum)
                    quantization_proved=True
                    print('[RelightQ] RGBA8 quantization isolated at fade %.3f %s: FP32 replay max %.6f/255 (strict <=1 plus float epsilon)' %
                          (fade,'half' if half else 'full',fp_maximum*255),flush=True)
            # Store image hashes rather than 16 GB of full images.
            dense.append([hashlib.sha256(frame).digest() for frame in pair])
            previous=pair
            previous_gather=gathered
            if index%250==0:
                print('[RelightQ] fine acquisition %s fade %.3f max %d/255' % ('half' if half else 'full',fade,maximum),flush=True)
        previous=None; previous_gather=None
        for index in range(1000,-1,-1):
            fade=index/1000
            gathered,traced=screen(1,.15,half=half,fade=fade,with_gather=True)
            pair=[hybrid_read(traced,fade=fade,lit_map=lit) for lit in (False,True)]
            assert [hashlib.sha256(frame).digest() for frame in pair]==dense[index],'fine eviction identity'
            if previous is not None:
                continuity(previous_gather,gathered,.001,('gather eviction',fade),quantized_passes=0)
                for lit in (0,1):
                    continuity(previous[lit],pair[lit],.001,('fine eviction',fade))
            previous=pair
            previous_gather=gathered
            if index%250==0:
                print('[RelightQ] fine eviction %s fade %.3f' % ('half' if half else 'full',fade),flush=True)
        print('[RelightQ] independent continuity %s: 1001 fades 0..1 / .001 both directions, displaced/lit native maps; gather max %d/255, final max %d/255' %
              ('half' if half else 'full',gather_maximum,maximum),flush=True)

    # Both visibilities fractional: a production multiplication mutation must
    # fail exact min semantics, independent of the boundary assertion.
    fractional=np.full((height,width,4),.6,np.float32)
    # hybrid_read accepts byte images.
    result=hybrid_read((fractional*255).astype(np.uint8))
    native_result=hybrid_read((fractional*255).astype(np.uint8),enabled=False)
    expected=np.minimum(native_result,153)
    assert np.max(abs(result.astype(int)-expected.astype(int)))<=1,'production min composition'
    product=hybrid_read((fractional*255).astype(np.uint8),pr=multiplied)
    assert np.any(abs(product.astype(int)-expected.astype(int))>20),'production multiplication mutation must fail min oracle'
    print('[RelightQ] contact hybrid mutation rejected: product with both visibilities fractional',flush=True)
    for handle in list(outputs.values())+list(textures.values())+[n.value for n in natives]+[lit_native.value]:
        texture_id=g['U'](getattr(handle,'value',handle))
        delete_texture(1,C.byref(texture_id))
