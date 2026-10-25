"""Immutable pre-WP7 full-consumer baseline, without inventing a shelter owner.

Only scene/view services are simulated. The old basic cover, publication,
material upload, particle owner, weather producer and rain shader execute.
"""
from weather_compute_fixture import function
from weather_particles_fixture import build as particles
from weather_shelter_fixture import COST_API
import ctypes as C
import os
import time
import numpy as np
from relight_3d_gl import SourceTree
from weather_cpu_check import ROOT


def build(tree,out,gl):
    def extension(prefix,methods,suffix,entries,sources):
        import re
        reserved=re.findall(r'mReservedUniforms.push_back\("([^"]+)"\)',
            function(tree.read('indra/llrender/llshadermgr.cpp'),'void LLShaderMgr::initAttribsAndUniforms('))
        names=('ENABLED','VIEW_TO_AGENT','ORIGIN','WET','TIME_RANGE')
        slots=','.join('WEATHER_MATERIAL_'+name+'='+str(reserved.index('weather_material_'+name.lower())) for name in names)
        prefix=prefix.replace('enum {DEFERRED_DEPTH=','enum {'+slots+',DEFERRED_DEPTH=')
        overloads='''
 void uniform1i(int n,int v){glUniform1i(mUniform[n],v);}
 void uniform2f(int n,float a,float b){glUniform2f(mUniform[n],a,b);}
 void uniform3f(int n,float a,float b,float c){glUniform3f(mUniform[n],a,b,c);}
 void uniform4f(int n,float a,float b,float c,float d){glUniform4f(mUniform[n],a,b,c,d);}
'''
        prefix=prefix.replace(' bool isComplete()const{return mProgramObject!=0;}',overloads+' bool isComplete()const{return mProgramObject!=0;}')
        methods+='\n'+function(tree.read('indra/newview/alweatherresources.cpp'),'void ALWeatherResources::bind(')
        methods+='\n'+API+'\n'+COST_API
        prefix+='\nvoid (*glGetUniformiv)(unsigned,int,int*);\n'
        suffix=suffix.replace('LLShaderMgr::instance()->root=root;',
            f'glGetUniformiv=reinterpret_cast<decltype(glGetUniformiv)>(p[{len(entries)}]);\nLLShaderMgr::instance()->root=root;')
        extra=('glGetUniformiv','void','unsigned,int,int*',None,[C.c_uint,C.c_int,C.c_void_p])
        return prefix,methods,suffix,entries+[extra],sources
    native=particles(tree,out,gl,extension=extension)
    return native


def compare(gl,current,program,vertex,wrappers,caster,objects,values,projection,inverse_projection,views,inverse_views,width,height):
    from weather_rain_gl_check import RainRunner,native_adapter,UNIFORMS
    from weather_fix2_cost_check import distribution,profile
    tree=SourceTree('80abb6c19d6')
    native=build(tree,ROOT/'.tmp/weather_port/shelter/baseline80'/str(os.getpid()),gl)
    material=program(vertex,tree.read('indra/newview/app_settings/shaders/class1/deferred/weatherMaterialF.glsl')+wrappers+'''
vec3 srgb_to_linear(vec3 a){return a;}vec3 linear_to_srgb(vec3 a){return a;}
void main(){float a=weatherSurfaceOpen(sampleAgent());result=vec4(a,a,a,1);}''')
    rain=RainRunner(tree,gl)
    upload=native_adapter(tree,ROOT/'.tmp/weather_port/shelter/baseline80_rain'/str(os.getpid()))
    baseline_values=np.asarray(list(upload(1,20,12.5).values()),np.float32)
    assert np.array_equal(baseline_values,values),'paired rain producer uniforms differ'
    old_objects=objects.copy();old_objects[3]=material;old_objects[4:6]=rain.program
    uniform_names=[*UNIFORMS,'depthMap','inv_proj','weather_inv_modelview']
    locations=np.asarray([[gl.loc(rain.program,name.encode()) for name in uniform_names]]*2,np.int32)
    native.cover_program(caster)
    native.shelter_cost_setup.argtypes=[C.c_void_p]*7+[C.c_int]*2
    native.shelter_cost_frame.argtypes=[C.c_int]*3
    native.shelter_cost_setup(old_objects.ctypes.data,locations.ctypes.data,baseline_values.ctypes.data,
        projection.ctypes.data,inverse_projection.ctypes.data,views.ctypes.data,inverse_views.ctypes.data,width,height)
    for name,value in {'Enabled':1,'ControlMode':1,'LocalEnabled':1,'TransitionSeconds':0,'PrecipVariation':0,
        'WindSource':1,'WindDirection':270,'WindSpeed':14,'PersistentWetEnabled':1,'StartWetness':1,
        'RainRate':20,'SnowEnabled':1,'PrecipType':2,'SnowRate':5,'PrecipVisibilityEnabled':1,'PrecipVisibilityAmount':1}.items():
        native.fixture_weather_set(('AlchemyWeather'+name).encode(),value)
    contexts={'80abb6c19d6':native,'WORKTREE_WP7':current};records=[]
    for dual in (False,True):
        for owner in contexts.values():owner.fixture_weather_restart()
        for tick in range(128):
            for ref,owner in contexts.items():owner.fixture_sync();owner.shelter_cost_frame(int(ref!='80abb6c19d6'),int(dual),tick)
        gl.finish();queries=(C.c_uint*480)();gl.gen_queries(480,queries)
        cpu={ref:[] for ref in contexts};gpu={ref:[] for ref in contexts};order=[]
        for tick in range(240):
            for ref in (list(contexts) if tick%2==0 else list(reversed(contexts))):
                owner=contexts[ref];owner.fixture_sync();q=queries[len(order)];order.append(ref)
                gl.begin_query(0x88BF,q);start=time.perf_counter();owner.shelter_cost_frame(int(ref!='80abb6c19d6'),int(dual),tick+128)
                cpu[ref].append((time.perf_counter()-start)*1000);gl.end_query(0x88BF)
        gl.finish()
        for q,ref in zip(queries,order):
            value=C.c_uint64();gl.query_u64(q,0x8866,C.byref(value));gpu[ref].append(value.value/1e6)
        gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(480,queries)
        totals={ref:np.asarray(cpu[ref])+gpu[ref] for ref in contexts}
        record={'dual':dual,'samples':240,'baseline_ref':'80abb6c19d6',
            'runs':{ref:{'cpu_ms':distribution(cpu[ref]),'gpu_ms':distribution(gpu[ref]),'total_ms':distribution(totals[ref])} for ref in contexts},
            'paired_ratio':distribution(totals['WORKTREE_WP7']/totals['80abb6c19d6']),
            'paired_increment_ms':distribution(np.maximum(0,totals['WORKTREE_WP7']-totals['80abb6c19d6']))}
        record['passes']=profile({'native':current,'gl':gl},lambda context,t:context['native'].shelter_cost_frame(1,int(dual),400+t))
        required={'weather close map','weather distant map','fixture cull and sort','caster draw',
            'weather interval dirty updates','weather cover publication','weather rain consumer',
            'weather material consumer','weather veil consumer','weather snow consumer'}
        assert required <= record['passes'].keys(), 'paired WP7 workload omitted production passes: '+str(required-record['passes'].keys())
        records.append(record)
    return records


API=r'''
extern "C" __declspec(dllexport) void shelter_prepare(double,double x,double y,double z,int dirty){cover_prepare(x,y,z,dirty);}
extern "C" __declspec(dllexport) void shelter_scene(int kind,float x,float z,float alpha){
 oldRoofOffset=roofOffset;oldRoofWidth=(sceneKind==1||sceneKind==4)?200.f:8.f;
 sceneKind=kind;roofOffset=x;roofHeight=z;roofAlpha=alpha;
}
extern "C" __declspec(dllexport) void shelter_sync(){fixture_sync();}
extern "C" __declspec(dllexport) void shelter_bind(unsigned p,int kind){
 static std::map<unsigned,LLGLSLShader> programs;auto& shader=programs[p];
 shader.mProgramObject=p;if(shader.mUniform.empty())shader.mapUniforms();shader.bind();
 if(kind==2)coverage.bind(&shader,glm::mat4(1),glm::dvec3(0),gFrameCount);
 shader.mProgramObject=0;LLGLSLShader::sCurBoundShaderPtr=nullptr;LLGLSLShader::sCurBoundShader=0;
}
extern "C" __declspec(dllexport) void shelter_depth(unsigned unit,unsigned texture){gGL.getTexUnit(unit)->bindManual(LLTexUnit::TT_TEXTURE,texture,false);}
extern "C" __declspec(dllexport) void shelter_rain_state(float* out){
 auto& weather=ALWeatherBridge::Controller::instance();auto rain=ALWeatherBridge::rain(weather.state(),.55f,.65f);
 out[0]=rain.windX;out[1]=rain.windY;out[2]=float(std::fmod(weather.clock().seconds(),4096.));
}
'''
