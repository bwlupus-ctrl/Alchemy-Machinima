"""Compile the actual particle owner and cover/state methods against real WGL.

Only viewer services, GL dispatch and matrix containers are fixtures. Native
compute/program loading, settings reader, fixed clock and render method execute.
GL calls are recorded as well as executed, making missing barriers/depth enables
detectable on coherent drivers where a race may otherwise appear to work.
"""
import ctypes as C
import shutil
from weather_cpu_check import ROOT, SOURCES
from weather_compute_fixture import build as loader_build, function
from weather_preparation_fixture import source as preparation_source


def build(tree,out,gl,mutation=None,extension=None):
    def extend(prefix,methods,suffix,entries):
        # Historical runs compile a complete snapshot, including local headers.
        for name in SOURCES:
            for ext in ('.cpp','.h'):
                (out/(name+ext)).write_text(tree.read('indra/newview/'+name+ext),encoding='utf-8')
        (out/'stdtypes.h').write_text(tree.read('indra/llcommon/stdtypes.h'),encoding='utf-8')
        names=[
          ('glGenVertexArrays','void','int,unsigned*',None,[C.c_int,C.c_void_p]),
          ('glDeleteVertexArrays','void','int,const unsigned*',None,[C.c_int,C.c_void_p]),
          ('glBindVertexArray','void','unsigned',None,[C.c_uint]),
          ('glGenBuffers','void','int,unsigned*',None,[C.c_int,C.c_void_p]),
          ('glDeleteBuffers','void','int,const unsigned*',None,[C.c_int,C.c_void_p]),
          ('glBindBuffer','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glBufferData','void','unsigned,ptrdiff_t,const void*,unsigned',None,[C.c_uint,C.c_ssize_t,C.c_void_p,C.c_uint]),
          ('glGetBufferParameteriv','void','unsigned,unsigned,int*',None,[C.c_uint,C.c_uint,C.c_void_p]),
          ('glDrawArrays','void','unsigned,int,int',None,[C.c_uint,C.c_int,C.c_int]),
          ('glDrawArraysInstanced','void','unsigned,int,int,int',None,[C.c_uint,C.c_int,C.c_int,C.c_int]),
          ('glVertexAttribDivisor','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glVertexAttribPointer','void','unsigned,int,unsigned,unsigned char,int,const void*',None,[C.c_uint,C.c_int,C.c_uint,C.c_ubyte,C.c_int,C.c_void_p]),
          ('glEnableVertexAttribArray','void','unsigned',None,[C.c_uint]),
          ('glDisableVertexAttribArray','void','unsigned',None,[C.c_uint]),
          ('glGetUniformLocation','int','unsigned,const char*',C.c_int,[C.c_uint,C.c_char_p]),
          ('glUniform1i','void','int,int',None,[C.c_int]*2),
          ('glUniform1ui','void','int,unsigned',None,[C.c_int,C.c_uint]),
          ('glUniform1f','void','int,float',None,[C.c_int,C.c_float]),
          ('glUniform2f','void','int,float,float',None,[C.c_int,C.c_float,C.c_float]),
          ('glUniform4f','void','int,float,float,float,float',None,[C.c_int,*[C.c_float]*4]),
          ('glUniform3f','void','int,float,float,float',None,[C.c_int,C.c_float,C.c_float,C.c_float]),
          ('glUniform3i','void','int,int,int,int',None,[C.c_int]*4),
          ('glUniformMatrix4fv','void','int,int,unsigned char,const float*',None,[C.c_int,C.c_int,C.c_ubyte,C.c_void_p]),
          ('glBindFramebuffer','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glViewport','void','int,int,int,int',None,[C.c_int]*4),
          ('glActiveTexture','void','unsigned',None,[C.c_uint]),
          ('glBindTexture','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glGetBooleanv','void','unsigned,unsigned char*',None,[C.c_uint,C.c_void_p]),
          ('glGetBooleani_v','void','unsigned,unsigned,unsigned char*',None,[C.c_uint,C.c_uint,C.c_void_p]),
          ('glGetDoublev','void','unsigned,double*',None,[C.c_uint,C.c_void_p]),
          ('glIsEnabled','unsigned char','unsigned',C.c_ubyte,[C.c_uint]),
          ('glIsEnabledi','unsigned char','unsigned,unsigned',C.c_ubyte,[C.c_uint]*2),
          ('glEnable','void','unsigned',None,[C.c_uint]),
          ('glDisable','void','unsigned',None,[C.c_uint]),
          ('glEnablei','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glDisablei','void','unsigned,unsigned',None,[C.c_uint]*2),
          ('glBlendFuncSeparate','void','unsigned,unsigned,unsigned,unsigned',None,[C.c_uint]*4),
          ('glBlendFuncSeparatei','void','unsigned,unsigned,unsigned,unsigned,unsigned',None,[C.c_uint]*5),
          ('glBlendEquationSeparatei','void','unsigned,unsigned,unsigned',None,[C.c_uint]*3),
          ('glColorMask','void','unsigned char,unsigned char,unsigned char,unsigned char',None,[C.c_ubyte]*4),
          ('glColorMaski','void','unsigned,unsigned char,unsigned char,unsigned char,unsigned char',None,[C.c_uint,*[C.c_ubyte]*4]),
          ('glDrawBuffers','void','int,const unsigned*',None,[C.c_int,C.c_void_p]),
          ('glTextureBarrier','void','',None,[]),
          ('glGenFramebuffers','void','int,unsigned*',None,[C.c_int,C.c_void_p]),
          ('glDeleteFramebuffers','void','int,const unsigned*',None,[C.c_int,C.c_void_p]),
          ('glGenTextures','void','int,unsigned*',None,[C.c_int,C.c_void_p]),
          ('glDeleteTextures','void','int,const unsigned*',None,[C.c_int,C.c_void_p]),
          ('glTexParameteri','void','unsigned,unsigned,int',None,[C.c_uint,C.c_uint,C.c_int]),
          ('glTexImage2D','void','unsigned,int,int,int,int,int,unsigned,unsigned,const void*',None,[C.c_uint,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_void_p]),
          ('glFramebufferTexture2D','void','unsigned,unsigned,unsigned,unsigned,int',None,[C.c_uint,C.c_uint,C.c_uint,C.c_uint,C.c_int]),
          ('glCheckFramebufferStatus','unsigned','unsigned',C.c_uint,[C.c_uint]),
          ('glClear','void','unsigned',None,[C.c_uint]),
          ('glReadBuffer','void','unsigned',None,[C.c_uint]),
          ('glCullFace','void','unsigned',None,[C.c_uint]),
          ('glFrontFace','void','unsigned',None,[C.c_uint]),
          ('glDepthFunc','void','unsigned',None,[C.c_uint]),
          ('glDepthMask','void','unsigned char',None,[C.c_ubyte]),
          ('glClearDepth','void','double',None,[C.c_double]),
          ('glIsTexture','unsigned char','unsigned',C.c_ubyte,[C.c_uint]),
          ('glIsProgram','unsigned char','unsigned',C.c_ubyte,[C.c_uint]),
        ]
        names = [e for e in names if e[0] not in {item[0] for item in entries}]
        enum=dict(GL_SHADER_STORAGE_BUFFER=0x90D2,GL_DYNAMIC_DRAW=0x88E8,GL_BUFFER_SIZE=0x8764,
          GL_VERTEX_ARRAY_BINDING=0x85B5,GL_ARRAY_BUFFER_BINDING=0x8894,GL_SHADER_STORAGE_BUFFER_BINDING=0x90D3,
          GL_ARRAY_BUFFER=0x8892,GL_BLEND=0x0BE2,GL_CULL_FACE=0x0B44,GL_SCISSOR_TEST=0x0C11,
          GL_ELEMENT_ARRAY_BUFFER_BINDING=0x8895,GL_ELEMENT_ARRAY_BUFFER=0x8893,
          GL_TRIANGLES=4,GL_SHADER_STORAGE_BARRIER_BIT=0x2000,GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT=1,
          GL_LEQUAL=0x0203,GL_FLOAT=0x1406,GL_DRAW_FRAMEBUFFER_BINDING=0x8CA6,GL_READ_FRAMEBUFFER_BINDING=0x8CAA,
          GL_READ_BUFFER=0x0C02,GL_CULL_FACE_MODE=0x0B45,GL_FRONT_FACE=0x0B46,GL_VIEWPORT=0x0BA2,
          GL_DEPTH_FUNC=0x0B74,GL_DEPTH_WRITEMASK=0x0B72,GL_DEPTH_CLEAR_VALUE=0x0B73,GL_MAX_DRAW_BUFFERS=0x8824,
          GL_DRAW_BUFFER0=0x8825,GL_COLOR_WRITEMASK=0x0C23,GL_BLEND_SRC_RGB=0x80C9,GL_BLEND_DST_RGB=0x80C8,
          GL_BLEND_SRC_ALPHA=0x80CB,GL_BLEND_DST_ALPHA=0x80CA,GL_BLEND_EQUATION_RGB=0x8009,GL_BLEND_EQUATION_ALPHA=0x883D,
          GL_COLOR_ATTACHMENT0=0x8CE0,GL_ONE=1,GL_ONE_MINUS_SRC_ALPHA=0x0303,
          GL_TEXTURE_2D=0x0DE1,GL_TEXTURE_COMPARE_MODE=0x884C,GL_NONE=0,GL_CURRENT_PROGRAM=0x8B8D,
          GL_FRAMEBUFFER_BARRIER_BIT=0x400,GL_TEXTURE_FETCH_BARRIER_BIT=8,
          GL_DRAW_FRAMEBUFFER=0x8CA9,GL_READ_FRAMEBUFFER=0x8CA8,GL_DEPTH_TEST=0x0B71)
        extra='#define LL_WARNS_ONCE(x) NullLog{}\n#include <cstddef>\n#include <cmath>\n#include <array>\n#include <new>\n#include "alweatherbridge.h"\n#include "alweathershelter.h"\n'
        extra+='''
size_t hostAllocations=0;
void* operator new(size_t n){++hostAllocations;if(void* p=malloc(n))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{free(p);}void operator delete[](void* p)noexcept{free(p);}
'''
        extra+='using GLboolean=unsigned char;using GLdouble=double;using LLStaticHashedString=std::string;\n'
        extra+='\n'.join(f'constexpr unsigned {n}={v}U;' for n,v in enum.items())+'\n'
        extra+='\n'.join(f'{ret} (*{name})({args});' for name,ret,args,_,_ in names)+'\n'
        prefix=prefix.replace('hasWeatherMaterial=false,attachNothing=false;', 'hasWeatherMaterial=false,attachNothing=false,isDeferred=false;')
        extra+=MATRIX+BACKEND
        prefix=prefix.replace('struct LLGLSLShader {',extra+'\nstruct LLGLSLShader {')
        prefix=prefix.replace('struct LLRender {static constexpr int NUM_MATRIX_MODES=4;};','struct LLRender {static constexpr int NUM_MATRIX_MODES=4; using eBlendFactor=unsigned;enum {BF_ONE=1,BF_ONE_MINUS_SOURCE_ALPHA=0x0303};static bool indexedDrawBufferGuardSupported(){return true;}};')
        import re
        reserved = re.findall(r'mReservedUniforms.push_back\("([^"]+)"\)', function(tree.read('indra/llrender/llshadermgr.cpp'), 'void LLShaderMgr::initAttribsAndUniforms('))
        slots = ','.join(f'{n}={reserved.index(v)}' for n,v in (('DEFERRED_DEPTH','depthMap'),('WEATHER_SURFACE_COVER_MAP','weather_surface_cover_map'),('WEATHER_SURFACE_COVER_MATRIX','weather_surface_cover_matrix'),('WEATHER_SHELTER_CLOSE_MAP','weather_shelter_close_map'),('WEATHER_SHELTER_DISTANT_MAP','weather_shelter_distant_map')) if v in reserved)
        prefix=prefix.replace('std::string root;int features=', 'enum {'+slots+'};\n std::string root;int features=')
        shader_methods=r'''
 bool isComplete()const{return mProgramObject!=0;}
 void unload(){unloadInternal();mShaderFiles.clear();mDefines.clear();mFeatures={};}
 void bind();void placeProfileQuery(){}void readProfileQuery(){}
 int getUniformLocation(const LLStaticHashedString&);
 void uniform1i(const std::string& n,int v){glUniform1i(getUniformLocation(n),v);}
 void uniform1f(const std::string& n,float v){glUniform1f(getUniformLocation(n),v);}
 void uniform2f(const std::string& n,float x,float y){glUniform2f(getUniformLocation(n),x,y);}
 void uniform3f(const std::string& n,float x,float y,float z){glUniform3f(getUniformLocation(n),x,y,z);}
 void uniform4f(const std::string& n,float a,float b,float c,float d){glUniform4f(getUniformLocation(n),a,b,c,d);}
 void uniformMatrix4fv(const std::string& n,int c,bool t,const float* p){glUniformMatrix4fv(getUniformLocation(n),c,t,p);}
 void uniformMatrix4fv(int n,int c,bool t,const float* p){glUniformMatrix4fv(mUniform[n],c,t,p);}
 int getTextureChannel(S32 uniform)const;
 int bindTexture(S32,LLRenderTarget*,bool,LLTexUnit::eTextureFilterOptions,U32=0);
 void addPermutation(const std::string& n,const std::string& v){mDefines[n]=v;}
 void clearPermutations(){mDefines.clear();}
'''
        prefix=prefix.replace(' void bind(){glUseProgram(mProgramObject);}', '')
        prefix=prefix.replace(' bool mapUniforms();',shader_methods+'\n bool mapUniforms();')
        prefix = prefix.replace('using LLStaticHashedString=std::string;', 'using LLStaticHashedString=std::string;')
        prefix += '\ntemplate<class T>using LLStaticStringTable=std::map<std::string,T>;\nconstexpr bool gDebugGL=false;\n'
        methods += '\n'+function(tree.read('indra/llrender/llglslshader.cpp'), 'GLint LLGLSLShader::getUniformLocation(const LLStaticHashedString& uniform)').replace('uniform.String().c_str()', 'uniform.c_str()')
        prefix=prefix.replace('static void unbind(){glUseProgram(0);sCurBoundShaderPtr=nullptr;}', 'static void unbind();')
        prefix=prefix.replace('inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;',
            'inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;inline static GLuint sCurBoundShader=0;bool mUniformsDirty=false;')
        prefix=prefix.replace('void prepareWeatherFeatures(LLGLSLShader*){}',
            'void updateWeatherUniforms(LLGLSLShader*){}void updateShaderUniforms(LLGLSLShader*){}void prepareWeatherFeatures(LLGLSLShader*){}')
        shader=tree.read('indra/llrender/llglslshader.cpp')
        methods+='\n'+'\n'.join(function(shader,sig) for sig in ('void LLGLSLShader::bind()', 'void LLGLSLShader::unbind(void)',
            'S32 LLGLSLShader::getTextureChannel(S32 uniform)', 'S32 LLGLSLShader::bindTexture(S32 uniform, LLRenderTarget*'))
        resources=tree.read('indra/newview/alweatherresources.cpp')
        extra_class=function(tree.read('indra/newview/alweatherresources.h'),'class ALWeatherResources').replace('private:', 'public:')+';\n'
        extra_class+=function(tree.read('indra/newview/alweatherparticles.h'),'class ALWeatherParticles').replace('private:','public:')+';\n'
        if 'class ALWeatherShelterMaps\n{' in tree.read('indra/newview/alweatherresources.h'):
            extra_class = 'class ALWeatherShelterMaps;\n' + extra_class
            extra_class += function(tree.read('indra/newview/alweatherresources.h'),'class ALWeatherShelterMaps\n{').replace('private:','public:')+';\n'
        prefix+='\n'+extra_class+'\nALWeatherParticles particles;ALWeatherResources coverage;\n'
        from weather_cover_fixture import parts as cover_parts
        cp, cm, ca = cover_parts(tree,out)
        prefix += cp
        prefix+='#define LL_PROFILE_GPU_ZONE(x)\n'
        methods+='\n'+ '\n'.join(function(resources,s) for s in (
          'ALWeatherResources::State::State(', 'ALWeatherResources::State::~State(',
          'void ALWeatherResources::prepare(', 'bool ALWeatherResources::allocate(', 'void ALWeatherResources::release(',
          'void ALWeatherResources::publish(', 'void ALWeatherResources::fail(',
          'bool ALWeatherResources::precipitationCovers(', 'void ALWeatherResources::bindPrecipitation('))
        if 'void ALWeatherShelterMaps::prepare(' in resources:
            methods += '\n' + '\n'.join(function(resources,s) for s in ('void ALWeatherResources::bindMap(', 'void ALWeatherShelterMaps::prepare(', 'void ALWeatherShelterMaps::release(', 'void ALWeatherShelterMaps::fail(', 'bool ALWeatherShelterMaps::current(', 'bool ALWeatherShelterMaps::covers(', 'void ALWeatherShelterMaps::publish(', 'void ALWeatherShelterMaps::bind('))
        owner=tree.read('indra/newview/alweatherparticles.cpp')
        changes={
          'depth':('LLGLDepthTest depth_test(GL_TRUE, GL_FALSE, GL_LEQUAL);','LLGLDepthTest depth_test(GL_FALSE, GL_FALSE, GL_LEQUAL);'),
          'barrier':('glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);','(void)0;'),
          'reuse-barrier':('glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);','(void)0;'),
          'cap':('glDispatchCompute((count+127U)/128U, 1, 1);','glDispatchCompute((count+127U)/128U + 1U, 1, 1);'),
          'main-camera':('origin + glm::dvec3(inverse_view[3])','glm::dvec3(0.0)'),
          'frame-seed':('glUniform1ui(mSimulation.getUniformLocation(sSeed), s.seed);',
                        'glUniform1ui(mSimulation.getUniformLocation(sSeed), s.seed ^ frame);'),
          'veil-twice':('glDrawArrays(GL_TRIANGLES, 0, 3); // One extinction application per target view.',
                        'glDrawArrays(GL_TRIANGLES, 0, 3); glDrawArrays(GL_TRIANGLES, 0, 3); // Mutant'),
          'mrt':('glDrawBuffers(1, &beauty);','(void)beauty;'),
          'texture-barrier':('glTextureBarrier();','(void)0;'),
        }
        if mutation in changes:
            a,b=changes[mutation];assert a in owner;owner=owner.replace(a,b)
        if mutation=='cover-cache':
            assert 'if (reuse_cover && mWeatherSurfaces.ready())' in cm
            cm=cm.replace('if (reuse_cover && mWeatherSurfaces.ready())','if (false && reuse_cover && mWeatherSurfaces.ready())')
        if mutation=='raw-light':
            assert 'sky->getIsSunUp() ? linearColor3(sky->getSunDiffuse()) : LLColor3::black' in cm
            cm=cm.replace('sky->getIsSunUp() ? linearColor3(sky->getSunDiffuse()) : LLColor3::black','linearColor3(sky->getSunDiffuse())')
        if mutation=='cap-cache':
            assert 'if (generation == sContextGeneration) return supported;' in methods
            methods=methods.replace('if (generation == sContextGeneration) return supported;','')
        methods+='\n'+ '\n'.join(function(owner,s) for s in ('bool ALWeatherParticles::supported(',
            'void ALWeatherParticles::release(', 'bool ALWeatherParticles::initialize(', 'void ALWeatherParticles::prepare(', 'void ALWeatherParticles::render('))
        methods+='\n'+cm+'\n'+API+'\n'+ca
        # Redirect only the dispatch surface. Assertions observe the real GL
        # state at the production draw, never a compile-error stand-in mutant.
        prefix=prefix.replace('struct LLGLSLShader {',TRACE+'\nstruct LLGLSLShader {')
        tracked=('glDrawArraysInstanced','glDrawArrays','glMemoryBarrier','glDispatchCompute','glGenBuffers','glGenVertexArrays',
                 'glGetIntegerv','glGetIntegeri_v','glGetAttribLocation','glCreateProgram','glTextureBarrier')
        for n in tracked:
            methods=methods.replace(n+'(', 'record_'+n+'(')
        prefix+='\n'
        inject='\n'.join(f'{name}=reinterpret_cast<decltype({name})>(p[{len(entries)+i}]);' for i,(name,*_) in enumerate(names))
        suffix=suffix.replace('LLShaderMgr::instance()->root=root;',inject+'\nLLShaderMgr::instance()->root=root;LLShaderMgr::instance()->mReservedAttribs={"position","normal","texcoord0"};')
        preparation=preparation_source(tree,out,particles=True)
        result=(prefix,methods,suffix,entries+names,[preparation,*[out/(n+'.cpp') for n in SOURCES]])
        return extension(*result) if extension else result
    lib=loader_build(tree,out,gl,extend)
    shaderdir=out/'class1/deferred';shaderdir.mkdir(parents=True,exist_ok=True)
    for name in ('weatherSnowSim','weatherSnowV','weatherSnowF','weatherPrecipCoverF','weatherPrecipVeilF','weatherShelterF'):
        if not tree.exists('indra/newview/app_settings/shaders/class1/deferred/'+name+'.glsl'): continue;
        text=tree.read('indra/newview/app_settings/shaders/class1/deferred/'+name+'.glsl')
        if mutation=='view-seed' and name=='weatherSnowSim':
            a='^ snow_seed);';assert a in text;text=text.replace(a,'^ snow_seed ^ floatBitsToUint(snow_cell_relative.x));')
        if mutation=='ray-endpoint' and name=='weatherPrecipVeilF':
            text=tree.read('indra/newview/app_settings/shaders/class1/deferred/weatherPrecipVeilF.glsl')
            text=text.replace('float weatherPrecipExposedDistance(vec3 start, vec3 direction, float distance_m);','float weatherPrecipOpen(vec3 agent);')
            text=text.replace('weatherPrecipExposedDistance(start,direction,distance_m)','distance_m*max(weatherPrecipOpen(start+direction*(z>=0.999999?8.0:distance_m)),0.0)')
            text=text.replace('z >= 0.999999 ? 3000.0 : min(length(eye),3000.0)', 'min(length(eye),3000.0)')
        (shaderdir/(name+'.glsl')).write_text(text,encoding='utf-8')
    lib.fixture_weather_set.argtypes=[C.c_char_p,C.c_double];lib.fixture_weather_step.argtypes=[C.c_double]
    lib.render.argtypes=[C.c_uint,C.c_uint,C.c_uint,C.c_int,C.c_int,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p,C.c_int]
    lib.metrics.argtypes=[C.c_void_p];lib.status.restype=C.c_char_p
    lib.buffer.restype=C.c_uint
    lib.weather_values.argtypes=[C.c_void_p]
    lib.work_counts.argtypes=[C.c_void_p]
    lib.cover_prepare.argtypes=[C.c_double,C.c_double,C.c_double,C.c_int]
    lib.cover_counts.argtypes=[C.c_void_p]
    lib.evaluated_sky.argtypes=[C.c_int,C.c_int,C.c_float,C.c_float,C.c_float]
    return lib


MATRIX=r'''
namespace glm {
struct vec4 {float x=0,y=0,z=0,w=0;vec4()=default;vec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
 float& operator[](int i){return (&x)[i];}const float& operator[](int i)const{return (&x)[i];}};
template<class T>struct V3 {T x=0,y=0,z=0;V3()=default;explicit V3(T a):x(a),y(a),z(a){}
 T& operator[](int i){return (&x)[i];}const T& operator[](int i)const{return (&x)[i];}
 V3(T a,T b,T c):x(a),y(b),z(c){}explicit V3(const vec4& a):x(a.x),y(a.y),z(a.z){}
 template<class U>explicit V3(const V3<U>& a):x(T(a.x)),y(T(a.y)),z(T(a.z)){}
 bool operator==(const V3& a)const{return x==a.x&&y==a.y&&z==a.z;}
 bool operator!=(const V3& a)const{return !(*this==a);}};
using vec3=V3<float>;using dvec3=V3<double>;
template<class T> V3<T> operator+(V3<T> a,V3<T>b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
template<class T> V3<T> operator-(V3<T> a,V3<T>b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
template<class T> V3<T> operator*(V3<T> a,T b){return {a.x*b,a.y*b,a.z*b};}
template<class T> V3<T> operator/(V3<T> a,T b){return {a.x/b,a.y/b,a.z/b};}
template<class T>V3<T> floor(V3<T> a){return {std::floor(a.x),std::floor(a.y),std::floor(a.z)};}
template<class T>V3<T> abs(V3<T> a){return {std::abs(a.x),std::abs(a.y),std::abs(a.z)};}
inline bool lessThan(vec3 a,vec3 b){return a.x<b.x&&a.y<b.y&&a.z<b.z;}inline bool all(bool a){return a;}
struct mat4 {vec4 c[4];explicit mat4(float d=1){for(int i=0;i<4;++i)c[i][i]=d;}
 vec4& operator[](int i){return c[i];}const vec4& operator[](int i)const{return c[i];}};
inline vec4 operator*(const mat4&m,const vec4&v){vec4 r;for(int i=0;i<4;++i)for(int j=0;j<4;++j)r[i]+=m[j][i]*v[j];return r;}
inline mat4 inverse(const mat4& m){double a[4][8]{};for(int i=0;i<4;++i){for(int j=0;j<4;++j)a[i][j]=m[j][i];a[i][i+4]=1;}
 for(int i=0;i<4;++i){int p=i;for(int j=i+1;j<4;++j)if(std::abs(a[j][i])>std::abs(a[p][i]))p=j;
 for(int j=0;j<8;++j)std::swap(a[p][j],a[i][j]);double d=a[i][i];for(double& v:a[i])v/=d;
 for(int j=0;j<4;++j)if(j!=i){double b=a[j][i];for(int k=0;k<8;++k)a[j][k]-=b*a[i][k];}}
 mat4 r(0);for(int i=0;i<4;++i)for(int j=0;j<4;++j)r[j][i]=float(a[i][j+4]);return r;}
inline const float* value_ptr(const mat4& m){return &m.c[0].x;}
inline vec4::vec4(const vec3&,float); // removed below by Python
}
'''.replace('inline vec4::vec4(const vec3&,float); // removed below by Python','')
# The only production vec4(vec3,1) constructor is a straightforward backend seam.
MATRIX=MATRIX.replace('struct vec4 {','template<class T>struct V3;\nstruct vec4 {').replace(' vec4()=default;', ' vec4()=default;')
MATRIX=MATRIX.replace('float& operator[]', 'template<class T>vec4(const V3<T>& a,float b):x(float(a.x)),y(float(a.y)),z(float(a.z)),w(b){}\n float& operator[]',1)

BACKEND=r'''
struct LLRenderTarget;
struct LLTexUnit {enum eTextureType {TT_NONE,TT_TEXTURE};enum eTextureFilterOptions {TFO_POINT,TFO_TRILINEAR,TFO_ANISOTROPIC};enum {TMG_NONE,TAM_CLAMP};unsigned index=0;
 eTextureType getCurrType(){return TT_TEXTURE;}unsigned getCurrTexture(){int n;activate();glGetIntegerv(0x8069,&n);return n;}
 bool hasMipMaps(){return false;}void activate(){glActiveTexture(0x84C0+index);}
 void bindManual(eTextureType,unsigned n,bool){activate();glBindTexture(0x0DE1,n);}
 void bind(LLRenderTarget*,bool);void setTextureFilteringOption(int){}void setTextureAddressMode(int){}
 void unbind(eTextureType){bindManual(TT_TEXTURE,0,false);}
 void disable(){bindManual(TT_TEXTURE,0,false);}};
struct GLBackend {LLTexUnit units[32];GLBackend(){for(unsigned i=0;i<32;++i)units[i].index=i;}
 void flush(){}unsigned getCurrentTexUnitIndex(){int n;glGetIntegerv(0x84E0,&n);return n-0x84C0;}
 LLTexUnit* getTexUnit(unsigned n){return &units[n];}
 void getBlendFunc(unsigned&a,unsigned&b,unsigned&c,unsigned&d){int n;glGetIntegerv(GL_BLEND_SRC_RGB,&n);a=n;glGetIntegerv(GL_BLEND_DST_RGB,&n);b=n;glGetIntegerv(GL_BLEND_SRC_ALPHA,&n);c=n;glGetIntegerv(GL_BLEND_DST_ALPHA,&n);d=n;}
 void blendFunc(unsigned a,unsigned b){glBlendFuncSeparate(a,b,a,b);}
 void blendFunc(unsigned a,unsigned b,unsigned c,unsigned d){glBlendFuncSeparate(a,b,c,d);}
 void setColorMask(bool a,bool b){glColorMask(a,a,a,b);}
 void setColorMask(bool a,bool b,bool c,bool d){glColorMask(a,b,c,d);}} gGL;
struct LLVertexBuffer {inline static U32 sLastMask=0,sGLRenderBuffer=0,sGLRenderIndices=0;
 static void unbind(){glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);sGLRenderBuffer=sGLRenderIndices=0;}
 static void setupClientArrays(unsigned mask){for(unsigned i=0;i<16;++i){if(mask&(1U<<i))glEnableVertexAttribArray(i);else glDisableVertexAttribArray(i);}sLastMask=mask;}};
struct LLRenderTarget {unsigned fbo=0,texture=0,w=0,h=0;
 bool isComplete()const{return fbo!=0&&texture!=0;}
 unsigned getTexture(U32=0)const{return texture;}LLTexUnit::eTextureType getUsage()const{return LLTexUnit::TT_TEXTURE;}
 enum {DEPTH_FMT_32F};bool owned=false;
 bool allocate(unsigned width,unsigned height,int,bool,bool,LLTexUnit::eTextureType,int,int){
  w=width;h=height;owned=true;glGenFramebuffers(1,&fbo);glGenTextures(1,&texture);glBindTexture(0x0DE1,texture);
  glTexImage2D(0x0DE1,0,0x8CAC,w,h,0,0x1902,GL_FLOAT,nullptr);
  for(unsigned p:{0x2801U,0x2800U})glTexParameteri(0x0DE1,p,0x2600);
  glBindFramebuffer(0x8D40,fbo);glFramebufferTexture2D(0x8D40,0x8D00,0x0DE1,texture,0);
  const unsigned none=0;glDrawBuffers(1,&none);glReadBuffer(0);return glCheckFramebufferStatus(0x8D40)==0x8CD5;}
 void release(){if(owned){glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);}fbo=texture=w=h=0;owned=false;}
 unsigned getNumTextures()const{return owned?0:3;}
 void bindTarget(){glBindFramebuffer(0x8D40,fbo);const unsigned buffers[]={0x8CE0,0x8CE1,0x8CE2},none=0;glDrawBuffers(owned?1:3,owned?&none:buffers);glReadBuffer(owned?0:0x8CE0);glViewport(0,0,w,h);}
 void flush(){glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);}unsigned getWidth()const{return w;}unsigned getHeight()const{return h;}};
void LLTexUnit::bind(LLRenderTarget* t,bool){bindManual(TT_TEXTURE,t->texture,false);}
struct Enable {unsigned cap;bool was;Enable(unsigned c,bool on):cap(c),was(glIsEnabled(c)!=0){if(on)glEnable(c);else glDisable(c);}
 ~Enable(){if(was)glEnable(cap);else glDisable(cap);}};
struct LLGLEnable:Enable {LLGLEnable(unsigned c):Enable(c,true){}};
struct LLGLDisable:Enable {LLGLDisable(unsigned c):Enable(c,false){}};
struct LLGLDepthTest:Enable {int func;unsigned char mask;LLGLDepthTest(bool on,bool write=false,unsigned f=GL_LEQUAL):Enable(GL_DEPTH_TEST,on){glGetIntegerv(GL_DEPTH_FUNC,&func);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glDepthFunc(f);glDepthMask(write);}
 ~LLGLDepthTest(){glDepthFunc(func);glDepthMask(mask);}};
'''

TRACE=r'''
unsigned draws=0,veilDraws=0,dispatches=0,barriers=0,allocations=0,instances=0,depthEnabled=0,barrierBeforeDraw=0,lastBuffer=0,groups=0,reuseBarrier=0;
unsigned queries=0,attributeQueries=0,programCreates=0,textureBarriers=0,limitQueries=0,drawContract=0;
void record_glGetIntegerv(unsigned name,int* value){++queries;if(name==GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS||name==GL_MAX_IMAGE_UNITS||name==GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS)++limitQueries;glGetIntegerv(name,value);}
void record_glGetIntegeri_v(unsigned name,unsigned index,int* value){++queries;if(name==GL_MAX_COMPUTE_WORK_GROUP_COUNT)++limitQueries;glGetIntegeri_v(name,index,value);}
int record_glGetAttribLocation(unsigned p,const char* name){++attributeQueries;return glGetAttribLocation(p,name);}
unsigned record_glCreateProgram(){++programCreates;return glCreateProgram();}
void record_glTextureBarrier(){++textureBarriers;glTextureBarrier();}
void checkDrawContract(){int b1,b2;glGetIntegerv(GL_DRAW_BUFFER0+1,&b1);glGetIntegerv(GL_DRAW_BUFFER0+2,&b2);
 unsigned char mask=1;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
 drawContract=(!b1&&!b2?1:0)|(textureBarriers?2:0)|(!mask?4:0);}
void record_glDispatchCompute(unsigned x,unsigned y,unsigned z){++dispatches;groups=x;reuseBarrier=barriers;barriers=0;glDispatchCompute(x,y,z);}
void record_glMemoryBarrier(unsigned bits){barriers|=bits;glMemoryBarrier(bits);}
void record_glDrawArraysInstanced(unsigned mode,int start,int n,int count){checkDrawContract();++draws;instances=count;depthEnabled=glIsEnabled(GL_DEPTH_TEST);barrierBeforeDraw=barriers;int b;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&b);lastBuffer=b;glDrawArraysInstanced(mode,start,n,count);barriers=0;}
void record_glDrawArrays(unsigned mode,int start,int n){++veilDraws;glDrawArrays(mode,start,n);}
void record_glGenBuffers(int n,unsigned* p){allocations+=n;glGenBuffers(n,p);}
void record_glGenVertexArrays(int n,unsigned* p){allocations+=n;glGenVertexArrays(n,p);}
'''

API=r'''
extern "C" __declspec(dllexport) void fixture_prepare_basic(unsigned resolution){coverage.prepare(resolution);}
extern "C" __declspec(dllexport) void fixture_prepare_shelter(){coverage.advanced=&fixturePipeline.mWeatherShelter;fixturePipeline.mWeatherShelter.prepare();}
extern "C" __declspec(dllexport) void fixture_suspend_shelter(){fixturePipeline.mWeatherShelter.suspend();}
extern "C" __declspec(dllexport) void fixture_prepare_particles(){particles.prepare();}
extern "C" __declspec(dllexport) int fixture_particle_supported(){return ALWeatherParticles::supported();}
extern "C" __declspec(dllexport) void render(unsigned fbo,unsigned depth,unsigned cover,int w,int h,
 const float* view,const float* projection,const float* coverMatrix,const double* origin,int frame){
 LLRenderTarget target;target.fbo=fbo;target.w=w;target.h=h;
 LLRenderTarget depthTarget;depthTarget.texture=depth;
 if(frame>=0){coverage.mRead=0;coverage.mCover[0].texture=cover;coverage.mPlace=ALWeatherBridge::Controller::instance().placeSerial();
 coverage.mCoverage=ALWeatherShelter::Coverage::COMPLETE_EMPTY;coverage.mPublishedFrame=1;
 std::memcpy(&coverage.mMatrix,coverMatrix,64);coverage.mOrigin={origin[0],origin[1],origin[2]};gFrameCount=frame;}
 glm::mat4 v,p;std::memcpy(&v,view,64);std::memcpy(&p,projection,64);
 currentView=v;currentProjection=p;std::memcpy(gGLLastModelView,view,64);std::memcpy(gGLLastProjection,projection,64);
 gAgent.origin={{origin[0],origin[1],origin[2]}};fixturePipeline.targets.deferredScreen=depthTarget;
 fixturePipeline.renderWeatherParticles(&target,true);
}
extern "C" __declspec(dllexport) void metrics(double* v){v[0]=draws;v[1]=veilDraws;v[2]=dispatches;v[3]=allocations;
 v[4]=instances;v[5]=depthEnabled;v[6]=barrierBeforeDraw;v[7]=particles.bytes();v[8]=groups;v[9]=double(hostAllocations);v[10]=reuseBarrier;}
extern "C" __declspec(dllexport) const char* status(){return particles.status();}
extern "C" __declspec(dllexport) unsigned draw_contract(){return drawContract;}
extern "C" __declspec(dllexport) void work_counts(double* v){v[0]=queries;v[1]=attributeQueries;v[2]=programCreates;v[3]=textureBarriers;v[4]=limitQueries;}
extern "C" __declspec(dllexport) void texture_capability(int core,int entry){gGLManager.mGLVersion=core?4.6f:4.3f;if(!entry)glTextureBarrier=nullptr;}
extern "C" __declspec(dllexport) unsigned draw_program(){return particles.mDraw.mProgramObject;}
extern "C" __declspec(dllexport) void attribute_mask(unsigned mask){LLVertexBuffer::setupClientArrays(mask);}
extern "C" __declspec(dllexport) unsigned buffer(){
 // The last dispatch's indexed buffer binding is restored by production, so
 // retain the handle via the VAO instance attribute captured during drawing.
 return lastBuffer;}
extern "C" __declspec(dllexport) void weather_values(double* v){auto& c=ALWeatherBridge::Controller::instance();
 v[0]=c.clock().seconds();v[1]=c.state().rainFall;v[2]=c.state().snowFall;v[3]=c.snowDrift()[0];v[4]=c.snowDrift()[1];v[5]=c.snowDrift()[2];
 v[6]=ALWeatherBridge::rain(c.state(),1,1).intensity;v[7]=ALWeatherBridge::snowAmount(c.state());}
'''
