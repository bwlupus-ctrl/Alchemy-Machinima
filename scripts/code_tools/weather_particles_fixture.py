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


def build(tree,out,gl,mutation=None):
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
          ('glReadBuffer','void','unsigned',None,[C.c_uint]),
          ('glCullFace','void','unsigned',None,[C.c_uint]),
          ('glFrontFace','void','unsigned',None,[C.c_uint]),
          ('glDepthFunc','void','unsigned',None,[C.c_uint]),
          ('glDepthMask','void','unsigned char',None,[C.c_ubyte]),
          ('glClearDepth','void','double',None,[C.c_double]),
          ('glIsTexture','unsigned char','unsigned',C.c_ubyte,[C.c_uint]),
        ]
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
          GL_DRAW_FRAMEBUFFER=0x8CA9,GL_READ_FRAMEBUFFER=0x8CA8,GL_DEPTH_TEST=0x0B71)
        extra='#include <cstddef>\n#include <cmath>\n#include <array>\n#include <new>\n#include "alweatherbridge.h"\n'
        extra+='''
size_t hostAllocations=0;
void* operator new(size_t n){++hostAllocations;if(void* p=malloc(n))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{free(p);}void operator delete[](void* p)noexcept{free(p);}
'''
        extra+='using GLboolean=unsigned char;using GLdouble=double;using LLStaticHashedString=std::string;\n'
        extra+='\n'.join(f'constexpr unsigned {n}={v}U;' for n,v in enum.items())+'\n'
        extra+='\n'.join(f'{ret} (*{name})({args});' for name,ret,args,_,_ in names)+'\n'
        extra+=MATRIX+BACKEND
        prefix=prefix.replace('struct LLGLSLShader {',extra+'\nstruct LLGLSLShader {')
        prefix=prefix.replace('struct LLRender {static constexpr int NUM_MATRIX_MODES=4;};','struct LLRender {static constexpr int NUM_MATRIX_MODES=4; using eBlendFactor=unsigned;enum {BF_ONE=1,BF_ONE_MINUS_SOURCE_ALPHA=0x0303};static bool indexedDrawBufferGuardSupported(){return true;}};')
        prefix=prefix.replace('std::string root;int features=', 'enum {DEFERRED_DEPTH, WEATHER_SURFACE_COVER_MAP, WEATHER_SURFACE_COVER_MATRIX};\n std::string root;int features=')
        shader_methods=r'''
 void unload(){unloadInternal();mShaderFiles.clear();mDefines.clear();mFeatures={};}
 void bind(){if(!isCompute())LLVertexBuffer::unbind();glUseProgram(mProgramObject);sCurBoundShaderPtr=this;if(!isCompute())LLVertexBuffer::setupClientArrays(mAttributeMask);}
 int getUniformLocation(const std::string& name){return glGetUniformLocation(mProgramObject,name.c_str());}
 void uniform1i(const std::string& n,int v){glUniform1i(getUniformLocation(n),v);}
 void uniform1f(const std::string& n,float v){glUniform1f(getUniformLocation(n),v);}
 void uniform2f(const std::string& n,float x,float y){glUniform2f(getUniformLocation(n),x,y);}
 void uniform3f(const std::string& n,float x,float y,float z){glUniform3f(getUniformLocation(n),x,y,z);}
 void uniformMatrix4fv(const std::string& n,int c,bool t,const float* p){glUniformMatrix4fv(getUniformLocation(n),c,t,p);}
 void uniformMatrix4fv(int n,int c,bool t,const float* p){uniformMatrix4fv("weather_surface_cover_matrix",c,t,p);}
 void bindTexture(int unit,LLRenderTarget* target,bool,int){
  glActiveTexture(0x84C0+unit);glBindTexture(0x0DE1,target->texture);
  uniform1i(unit==0?"depthMap":"weather_surface_cover_map",unit);}
 void addPermutation(const std::string& n,const std::string& v){mDefines[n]=v;}
 void clearPermutations(){mDefines.clear();}
'''
        prefix=prefix.replace(' bool mapUniforms(){return true;}',shader_methods+'\n bool mapUniforms(){return true;}')
        prefix=prefix.replace('static void unbind(){glUseProgram(0);sCurBoundShaderPtr=nullptr;}',
            'static void unbind(){if(!sCurBoundShaderPtr||!sCurBoundShaderPtr->isCompute())LLVertexBuffer::unbind();glUseProgram(0);sCurBoundShaderPtr=nullptr;}')
        resources=tree.read('indra/newview/alweatherresources.cpp')
        state_class=function(tree.read('indra/newview/alweatherresources.h'),'class State')+';'
        extra_class='struct ALWeatherResources {\n'+state_class+'''
 unsigned mPlace=0;glm::dvec3 mOrigin;glm::mat4 mMatrix{1.f};LLRenderTarget mCover[2];int mRead=0;
 bool readyForFrame(unsigned frame)const{return frame==1;}
 bool precipitationCovers(const glm::vec3&,const glm::dvec3&,unsigned)const;
 void bindPrecipitation(LLGLSLShader*,const glm::dvec3&,unsigned);
};
'''
        extra_class+=function(tree.read('indra/newview/alweatherparticles.h'),'class ALWeatherParticles').replace('private:','public:')+';\n'
        prefix+='\n'+extra_class
        prefix+='#define LL_PROFILE_GPU_ZONE(x)\n'
        methods+='\n'+ '\n'.join(function(resources,s) for s in (
          'ALWeatherResources::State::State(', 'ALWeatherResources::State::~State(',
          'bool ALWeatherResources::precipitationCovers(', 'void ALWeatherResources::bindPrecipitation('))
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
        }
        if mutation in changes:
            a,b=changes[mutation];assert a in owner;owner=owner.replace(a,b)
        methods+='\n'+ '\n'.join(function(owner,s) for s in ('bool ALWeatherParticles::supported(',
            'void ALWeatherParticles::release(', 'bool ALWeatherParticles::initialize(', 'void ALWeatherParticles::render('))
        methods+='\n'+API
        # Redirect only the dispatch surface. Assertions observe the real GL
        # state at the production draw, never a compile-error stand-in mutant.
        prefix=prefix.replace('struct LLGLSLShader {',TRACE+'\nstruct LLGLSLShader {')
        tracked=('glDrawArraysInstanced','glDrawArrays','glMemoryBarrier','glDispatchCompute','glGenBuffers','glGenVertexArrays')
        for n in tracked:
            methods=methods.replace(n+'(', 'record_'+n+'(')
        prefix+='\n'
        inject='\n'.join(f'{name}=reinterpret_cast<decltype({name})>(p[{len(entries)+i}]);' for i,(name,*_) in enumerate(names))
        suffix=suffix.replace('LLShaderMgr::instance()->root=root;',inject+'\nLLShaderMgr::instance()->root=root;LLShaderMgr::instance()->mReservedAttribs={"position","normal","texcoord0"};')
        preparation=preparation_source(tree,out)
        return prefix,methods,suffix,entries+names,[preparation,*[out/(n+'.cpp') for n in SOURCES]]
    lib=loader_build(tree,out,gl,extend)
    shaderdir=out/'class1/deferred';shaderdir.mkdir(parents=True,exist_ok=True)
    for name in ('weatherSnowSim','weatherSnowV','weatherSnowF','weatherPrecipCoverF','weatherPrecipVeilF'):
        text=tree.read('indra/newview/app_settings/shaders/class1/deferred/'+name+'.glsl')
        if mutation=='view-seed' and name=='weatherSnowSim':
            a='^ snow_seed);';assert a in text;text=text.replace(a,'^ snow_seed ^ floatBitsToUint(snow_cell_relative.x));')
        (shaderdir/(name+'.glsl')).write_text(text,encoding='utf-8')
    lib.fixture_weather_set.argtypes=[C.c_char_p,C.c_double];lib.fixture_weather_step.argtypes=[C.c_double]
    lib.render.argtypes=[C.c_uint,C.c_uint,C.c_uint,C.c_int,C.c_int,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p,C.c_int]
    lib.metrics.argtypes=[C.c_void_p];lib.status.restype=C.c_char_p
    lib.buffer.restype=C.c_uint
    lib.weather_values.argtypes=[C.c_void_p]
    return lib


MATRIX=r'''
namespace glm {
struct vec4 {float x=0,y=0,z=0,w=0;vec4()=default;vec4(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
 float& operator[](int i){return (&x)[i];}const float& operator[](int i)const{return (&x)[i];}};
template<class T>struct V3 {T x=0,y=0,z=0;V3()=default;explicit V3(T a):x(a),y(a),z(a){}
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
struct LLTexUnit {enum eTextureType {TT_NONE,TT_TEXTURE};enum {TFO_POINT};unsigned index=0;
 eTextureType getCurrType(){return TT_TEXTURE;}unsigned getCurrTexture(){int n;activate();glGetIntegerv(0x8069,&n);return n;}
 bool hasMipMaps(){return false;}void activate(){glActiveTexture(0x84C0+index);}
 void bindManual(eTextureType,unsigned n,bool){activate();glBindTexture(0x0DE1,n);}
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
struct LLRenderTarget {unsigned fbo=0,texture=0,w=1,h=1;
 void bindTarget(){glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);glViewport(0,0,w,h);}
 void flush(){glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);}unsigned getWidth()const{return w;}unsigned getHeight()const{return h;}};
struct Enable {unsigned cap;bool was;Enable(unsigned c,bool on):cap(c),was(glIsEnabled(c)!=0){if(on)glEnable(c);else glDisable(c);}
 ~Enable(){if(was)glEnable(cap);else glDisable(cap);}};
struct LLGLEnable:Enable {LLGLEnable(unsigned c):Enable(c,true){}};
struct LLGLDisable:Enable {LLGLDisable(unsigned c):Enable(c,false){}};
struct LLGLDepthTest:Enable {int func;unsigned char mask;LLGLDepthTest(bool on,bool write=false,unsigned f=GL_LEQUAL):Enable(GL_DEPTH_TEST,on){glGetIntegerv(GL_DEPTH_FUNC,&func);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glDepthFunc(f);glDepthMask(write);}
 ~LLGLDepthTest(){glDepthFunc(func);glDepthMask(mask);}};
'''

TRACE=r'''
unsigned draws=0,veilDraws=0,dispatches=0,barriers=0,allocations=0,instances=0,depthEnabled=0,barrierBeforeDraw=0,lastBuffer=0,groups=0,reuseBarrier=0;
void record_glDispatchCompute(unsigned x,unsigned y,unsigned z){++dispatches;groups=x;reuseBarrier=barriers;barriers=0;glDispatchCompute(x,y,z);}
void record_glMemoryBarrier(unsigned bits){barriers|=bits;glMemoryBarrier(bits);}
void record_glDrawArraysInstanced(unsigned mode,int start,int n,int count){++draws;instances=count;depthEnabled=glIsEnabled(GL_DEPTH_TEST);barrierBeforeDraw=barriers;int b;glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&b);lastBuffer=b;glDrawArraysInstanced(mode,start,n,count);barriers=0;}
void record_glDrawArrays(unsigned mode,int start,int n){++veilDraws;glDrawArrays(mode,start,n);}
void record_glGenBuffers(int n,unsigned* p){allocations+=n;glGenBuffers(n,p);}
void record_glGenVertexArrays(int n,unsigned* p){allocations+=n;glGenVertexArrays(n,p);}
'''

API=r'''
ALWeatherParticles particles;ALWeatherResources coverage;
extern "C" __declspec(dllexport) void render(unsigned fbo,unsigned depth,unsigned cover,int w,int h,
 const float* view,const float* projection,const float* coverMatrix,const double* origin,int frame){
 LLRenderTarget target;target.fbo=fbo;target.w=w;target.h=h;
 LLRenderTarget depthTarget;depthTarget.texture=depth;
 coverage.mCover[0].texture=cover;coverage.mPlace=ALWeatherBridge::Controller::instance().placeSerial();
 std::memcpy(&coverage.mMatrix,coverMatrix,64);coverage.mOrigin={origin[0],origin[1],origin[2]};
 glm::mat4 v,p;std::memcpy(&v,view,64);std::memcpy(&p,projection,64);
 particles.render(target,depthTarget,v,p,coverage.mOrigin,glm::vec3(1.f),coverage,frame);
}
extern "C" __declspec(dllexport) void metrics(double* v){v[0]=draws;v[1]=veilDraws;v[2]=dispatches;v[3]=allocations;
 v[4]=instances;v[5]=depthEnabled;v[6]=barrierBeforeDraw;v[7]=particles.bytes();v[8]=groups;v[9]=double(hostAllocations);v[10]=reuseBarrier;}
extern "C" __declspec(dllexport) const char* status(){return particles.status();}
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
