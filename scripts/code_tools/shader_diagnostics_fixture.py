"""Execute extracted production create/load/attach and record producers with GL spies.

No viewer target, GL context, external shader source or recipe reconstruction.
The GL driver and bind services are deterministic fixtures; the uniform mapper is real. All template
algorithms and diagnostic serializers are extracted verbatim from production.
"""
import re
from native_check_tools import ROOT, compile_run, viewer_native_flags
from shader_load_check import function
from shader_equivalence_diff import compare


def run(directory):
    mgr = (ROOT / 'indra/llrender/llshadermgr.cpp').read_text(encoding='utf-8')
    shader = (ROOT / 'indra/llrender/llglslshader.cpp').read_text(encoding='utf-8')
    mh = (ROOT / 'indra/llrender/llshadermgr.h').read_text(encoding='utf-8')
    sh = (ROOT / 'indra/llrender/llglslshader.h').read_text(encoding='utf-8')
    methods = '\n'.join(function(mgr, 'LLShaderMgr::' + name + '(') for name in (
        'shaderLanguageVersion', 'ownsShaderObject', 'clearShaderObjects',
        'loadShaderFileInternal', 'linkProgramObjectInternal', 'attachShaderFeaturesInternal',
        'loadCachedProgramBinaryInternal', 'saveCachedProgramBinary', 'rejectProgramBinary',
        'readShaderSource', 'configureShaderDiagnostics', 'openShaderEquivalence',
        'beginShaderAttempt', 'snapshotShaderAttempt', 'beginShaderStage',
        'assembledShaderStage', 'finishShaderStage', 'traceFeatureAttachment',
        'traceAttachment', 'traceLink', 'injectShaderFailure', 'injectShaderOverflow',
        'finishShaderAttempt', 'writeShaderRecord', 'beginShaderPass', 'finishShaderPass'))
    methods += '\n' + '\n'.join(function(shader, 'LLGLSLShader::' + name + '(') for name in (
        'isCompute', 'unloadInternal', 'createShaderInternal', 'createShader',
        'attachVertexObjectInternal', 'attachFragmentObjectInternal', 'attachObjectInternal',
        'mapAttributesInternal', 'linkInternal', 'hash', 'hashWithEpoch',
        'mapUniform', 'mapUniformTextureChannel', 'mapUniforms'))
    methods = function(shader, 'static bool is_sampler_type(') + '\n' + methods
    # Plant exactly the review's early return after the request trace, confined
    # to <true>. The entire warm creation must now have a different GL trace.
    vertex = function(shader, 'LLGLSLShader::attachVertexObjectInternal(')
    planted = vertex.replace('attachVertexObjectInternal(', 'attachVertexObjectPlanted(', 1)
    anchor = '    if constexpr (diagnostics) LLShaderMgr::instance()->traceFeatureAttachment(object_path, GL_VERTEX_SHADER);'
    assert anchor in planted
    planted = planted.replace(anchor, anchor + '\n    if constexpr (diagnostics) if (mUsingBinaryProgram) return true;', 1)
    methods = methods.replace(vertex, planted + '\n' + vertex.replace(anchor,
        '    if (plantRegression) return attachVertexObjectPlanted<diagnostics>(object_path);\n' + anchor, 1))

    # Only diagnostic reflection reads are excluded from the differential trace.
    # A state-changing GL call inside an observer must still be detected.
    finish = function(mgr, 'LLShaderMgr::finishShaderAttempt(')
    observed = finish.replace('{\n', '{\n    ObserverScope observer;\n', 1)
    sampler = 'if (location >= 0) glGetUniformiv(shader->mProgramObject, location, &unit);'
    assert sampler in observed
    observed = observed.replace(sampler, sampler + '\n                        if (plantReflection) glUniform1i(location, 7);')
    methods = methods.replace(finish, observed)
    gamma = 'if (features->hasGamma || features->isDeferred)'
    assert gamma in methods
    methods = methods.replace(gamma, 'if constexpr (diagnostics) if (plantGamma && features->hasGamma) glProgramParameteri(shader->mProgramObject, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_FALSE);\n    ' + gamma, 1)

    prefix = r'''
#include "linden_common.h"
#include "llsd.h"
#include "llsdjson.h"
#include "llstaticstringtable.h"
#include "llfile.h"
#include "lltimer.h"
#include "hbxxh.h"
#include "fsyspath.h"
#include "llglheaders.h"
#include "llshaderdiagnostics.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#undef LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER
#undef LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER
#undef LL_WARNS
#undef LL_INFOS
#undef LL_DEBUGS
#undef LL_ENDL
#undef LL_ERRS
#undef llassert
#undef llassert_always
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER
#define LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER(x)
#define LL_WARNS(...) NullLog{}
#define LL_INFOS(...) NullLog{}
#define LL_DEBUGS(...) NullLog{}
#define LL_SHADER_LOADING_WARNS() NullLog{}
#define LL_ENDL 0
#define LL_ERRS(...) NullLog{}
#define llassert(x) CHECK(x)
#define llassert_always(x) CHECK(x)
#define stop_glerror() ((void)0)
#define flush_glerror() ((void)0)
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
using std::vector;using std::pair;using std::string;
struct NullLog{template<class T> NullLog& operator<<(const T&){return *this;}};
std::vector<std::string> calls;
GLuint nextHandle=1;
std::map<GLuint,std::string> sources;
std::map<GLuint,std::vector<GLuint>> attached;
bool plantRegression=false, plantReflection=false, plantGamma=false, failLink=false, rejectBinary=false, observing=false;
struct ObserverScope {ObserverScope(){CHECK(!observing);observing=true;}~ObserverScope(){observing=false;}};
void query(const std::string& s){if(!observing) calls.push_back(s);}
std::map<GLint,GLint> units;
GLint blockBinding=0;
void call(const std::string& s){calls.push_back(s);}
struct FixtureDir{
 std::string root;
 std::string getDirDelimiter(){return "/";}
 bool fileExists(const std::string& p){return std::filesystem::exists(p);}
 std::string getExpandedFilename(int,const std::string& name){return root+"/"+name;}
 std::string add(const std::string& a,const std::string& b){return a+"/"+b;}
} fixtureDir;
#define gDirUtilp (&fixtureDir)
constexpr int LL_PATH_LOGS=0;
struct FixtureGLManager{int mGLSLVersionMajor=4,mGLSLVersionMinor=60,mNumTextureImageUnits=32;
 float mGLVersion=4.6f;bool mIsAMD=false,mIsNVIDIA=true;
 std::string mGLVendor="fixture",mGLRenderer="GL call spy",mGLVersionString="4.6";} gGLManager;
struct LLRender{static constexpr int NUM_MATRIX_MODES=4;};

'''
    # Rename only GL symbols in the fixture translation unit, never a body.
    glnames = sorted(set(re.findall(r'\bgl[A-Z]\w*', methods) + ['glUseProgram']))
    prefix += '\n'.join(f'#define {n} spy_{n}' for n in glnames) + '\n'
    prefix += r'''
GLenum glGetError(){call("glGetError");return GL_NO_ERROR;}
GLuint glCreateProgram(){call("glCreateProgram");return nextHandle++;}
GLuint glCreateShader(GLenum t){call("glCreateShader:"+std::to_string(t));return nextHandle++;}
void glShaderSource(GLuint h,GLsizei n,const GLchar** s,const GLint* lengths){
 std::string event="glShaderSource:"+std::to_string(h)+":"+std::to_string(n);
 for(int i=0;i<n;++i){std::string text=lengths&&lengths[i]>=0?std::string(s[i],lengths[i]):s[i];
 sources[h]+=text;event+=":"+std::to_string(text.size())+":"+text;}
 call(event);}
void glCompileShader(GLuint h){call("glCompileShader:"+std::to_string(h));}
void glGetShaderiv(GLuint h,GLenum k,GLint* p){call("glGetShaderiv:"+std::to_string(h)+":"+std::to_string(k));*p=sources[h].find("FAIL")==std::string::npos;}
void glDeleteShader(GLuint h){call("glDeleteShader:"+std::to_string(h));}
void glDeleteProgram(GLuint h){call("glDeleteProgram:"+std::to_string(h));attached.erase(h);}
void glAttachShader(GLuint p,GLuint s){call("glAttachShader:"+std::to_string(p)+":"+std::to_string(s));attached[p].push_back(s);}
void glDetachShader(GLuint p,GLuint s){call("glDetachShader:"+std::to_string(p)+":"+std::to_string(s));}
void glGetAttachedShaders(GLuint h,GLsizei max,GLsizei* n,GLuint* p){call("glGetAttachedShaders:"+std::to_string(h)+":"+std::to_string(max));*n=(GLsizei)attached[h].size();std::copy(attached[h].begin(),attached[h].end(),p);}
GLboolean glIsShader(GLuint h){call("glIsShader:"+std::to_string(h));return GL_TRUE;}
void glDeleteQueries(GLsizei n,const GLuint* h){std::string e="glDeleteQueries:";for(int i=0;i<n;++i)e+=std::to_string(h[i])+":";call(e);}
void glLinkProgram(GLuint h){call("glLinkProgram:"+std::to_string(h));}
void glGetProgramiv(GLuint h,GLenum k,GLint* p){
 // Only queries made exclusively by finishShaderAttempt are observers.
 if(k==GL_LINK_STATUS){call("glGetProgramiv:link:"+std::to_string(h));*p=!(failLink || rejectBinary);rejectBinary=false;}
 else if(k==GL_PROGRAM_BINARY_LENGTH){call("glGetProgramiv:binary_length:"+std::to_string(h));*p=0;}
 else {
   CHECK(k==GL_ACTIVE_ATTRIBUTES || k==GL_ACTIVE_ATTRIBUTE_MAX_LENGTH ||
         k==GL_ACTIVE_UNIFORM_BLOCKS || k==GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH ||
         k==GL_ACTIVE_UNIFORMS || k==GL_ACTIVE_UNIFORM_MAX_LENGTH);
   query("glGetProgramiv:"+std::to_string(k));
   *p=(k==GL_ACTIVE_ATTRIBUTES || k==GL_ACTIVE_UNIFORM_BLOCKS || k==GL_ACTIVE_UNIFORMS)?1:64;
 }
}
void glBindAttribLocation(GLuint h,GLuint i,const GLchar* n){call("glBindAttribLocation:"+std::to_string(h)+":"+std::to_string(i)+":"+n);}
GLint glGetAttribLocation(GLuint h,const GLchar* n){query("glGetAttribLocation:"+std::to_string(h)+":"+n);return 0;}
void glBindFragDataLocation(GLuint h,GLuint i,const GLchar* n){call("glBindFragDataLocation:"+std::to_string(h)+":"+std::to_string(i)+":"+n);}
void glProgramParameteri(GLuint h,GLenum k,GLint v){call("glProgramParameteri:"+std::to_string(h)+":"+std::to_string(k)+":"+std::to_string(v));}
void glProgramBinary(GLuint h,GLenum f,const void* p,GLsizei n){call("glProgramBinary:"+std::to_string(h)+":"+std::to_string(f)+":"+std::string((const char*)p,n));}
void glGetProgramBinary(GLuint h,GLsizei n,GLsizei*,GLenum*,void*){call("glGetProgramBinary:"+std::to_string(h)+":"+std::to_string(n));}
void reflect(const char* text,GLsizei max,GLsizei* length,GLchar* name){
 CHECK(max>(GLsizei)strlen(text));strcpy(name,text);*length=(GLsizei)strlen(text);}
void glGetActiveAttrib(GLuint,GLuint,GLsizei max,GLsizei* length,GLint* size,GLenum* type,GLchar* name){
 query("glGetActiveAttrib");*size=1;*type=GL_FLOAT_VEC3;reflect("position",max,length,name);}
void glGetActiveUniformBlockName(GLuint,GLuint,GLsizei max,GLsizei* length,GLchar* name){
 query("glGetActiveUniformBlockName");reflect("GLTFJoints",max,length,name);}
void glGetActiveUniformBlockiv(GLuint,GLuint,GLenum,GLint* value){query("glGetActiveUniformBlockiv");*value=blockBinding;}
void glGetActiveUniform(GLuint,GLuint,GLsizei max,GLsizei* length,GLint* size,GLenum* type,GLchar* name){
 query("glGetActiveUniform");*size=2;*type=GL_SAMPLER_2D;reflect("diffuseMap[0]",max,length,name);}
GLint glGetUniformLocation(GLuint,const GLchar* name){query(std::string("glGetUniformLocation:")+name);return strstr(name,"[1]")?3:2;}
void glGetUniformiv(GLuint,GLint loc,GLint* value){query("glGetUniformiv");*value=units[loc];}
void glUniform1i(GLint loc,GLint value){call("glUniform1i:"+std::to_string(loc)+":"+std::to_string(value));units[loc]=value;}
void glUniform1iv(GLint loc,GLsizei n,const GLint* value){for(int i=0;i<n;++i)glUniform1i(loc+i,value[i]);}
GLuint glGetUniformBlockIndex(GLuint,const GLchar* name){call(std::string("glGetUniformBlockIndex:")+name);return std::string(name)=="GLTFJoints"?0:GL_INVALID_INDEX;}
void glUniformBlockBinding(GLuint,GLuint,GLuint binding){call("glUniformBlockBinding:"+std::to_string(binding));blockBinding=binding;}
void glUseProgram(GLuint h){call("glUseProgram:"+std::to_string(h));}

'''
    prefix += function(sh, 'class LLShaderFeatures') + ';\n'
    diagnostic = mh[mh.index('    // Configured by the viewer,'):mh.index('\nprotected:', mh.index('    // Configured by the viewer,'))]
    prefix += r'''
struct LLGLSLShader;
struct LLShaderMgr{
 static LLShaderMgr* instance();
 inline static U32 sContextGeneration=0,sAvatarLightResponseAttachCount=0;
 std::map<std::string,GLuint> mShaderObjects,mVertexShaderObjects,mFragmentShaderObjects;
 std::vector<std::string> mReservedAttribs={"position"},mReservedUniforms={"diffuseMap"};
 std::string root,mShaderCacheDir;
 bool mShaderCacheEnabled=true,mShaderCacheReadOnly=false;
 LLUUID mSharedShaderFingerprint,mShaderCacheFeatureHash;
'''
    prefix += function(mh, 'struct ProgramBinaryData') + ';\nstd::map<LLUUID,ProgramBinaryData> mShaderBinaryCache;\n'
    prefix += re.search(r'static constexpr U32 SHADER_CACHE_EPOCH\s*=\s*\d+;', mh)[0] + '\n'
    prefix += r'''
 bool ownsShaderObject(GLuint)const;void clearShaderObjects();
 static bool computeShadersSupported(){return true;}
 static S32 shaderLanguageVersion(GLenum);
 static bool readShaderSource(const std::string&,std::string&);
 std::string getShaderDirPrefix(){return root+"/class";}
 void prepareWeatherFeatures(LLGLSLShader*){}
 template<bool diagnostics> GLuint loadShaderFileInternal(const std::string&,S32&,GLenum,std::map<std::string,std::string>*,S32);
 template<bool diagnostics> bool linkProgramObjectInternal(GLuint,bool,const std::string&);
 template<bool diagnostics> bool attachShaderFeaturesInternal(LLGLSLShader*);
 template<bool diagnostics> bool loadCachedProgramBinaryInternal(LLGLSLShader*);
 bool saveCachedProgramBinary(LLGLSLShader*);
 void dumpObjectLog(GLuint,bool,const std::string&){}
 void dumpShaderSource(U32,GLchar**){}
'''
    prefix += diagnostic.replace('private:', 'public:') + '\n};\n'
    prefix += r'''
LLShaderMgr* currentMgr=nullptr;
LLShaderMgr* LLShaderMgr::instance(){return currentMgr;}
struct LLGLSLShader{
 static constexpr GLenum COMPUTE_STAGE=0x91B9;
 inline static std::set<LLGLSLShader*> sInstances;
 inline static std::map<std::string,std::string> sGlobalDefines;
 inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;
 inline static int sIndexedTextureChannels=4;
 std::vector<std::pair<std::string,GLenum>> mShaderFiles;
 std::map<std::string,std::string> mDefines;
 LLShaderFeatures mFeatures;
 LLGLSLShader* mRiggedVariant=nullptr;
 GLuint mProgramObject=0,mTimerQuery=0,mSamplesQuery=0,mPrimitivesQuery=0;
 U32 mAttributeMask=0,mMatHash[4]{},mLightHash=0;
 std::vector<int> mAttribute,mTexture,mUniform;
 LLStaticStringTable<GLint> mUniformMap;
 std::map<int,int> mValue;
 bool mUsingBinaryProgram=false,mShaderCacheEligible=true;
 int mShaderLevel=3,mShaderGroup=0,mActiveTextureChannels=0;
 std::string mName="fixture";LLUUID mShaderHash;
 bool isCompute()const;bool createShader();void unloadInternal();LLUUID hash();LLUUID hashWithEpoch(U32);
 template<bool diagnostics> bool createShaderInternal();
 template<bool diagnostics> bool attachVertexObjectInternal(std::string);
 template<bool diagnostics> bool attachVertexObjectPlanted(std::string);
 template<bool diagnostics> bool attachFragmentObjectInternal(std::string);
 template<bool diagnostics> void attachObjectInternal(GLuint);
 template<bool diagnostics> bool mapAttributesInternal();
 template<bool diagnostics> bool linkInternal(bool=false);
 UNIFORM_DATA
 static constexpr int NUM_UNIFORM_BLOCKS=4;
 int mTotalUniformSize=0;
 void mapUniform(const gl_uniform_data_t&);GLint mapUniformTextureChannel(GLint,GLenum,GLint);
 bool mapUniforms();void bind(){glUseProgram(mProgramObject);}
 static void unbind(){call("unbind service");sCurBoundShaderPtr=nullptr;}
};
'''
    prefix = prefix.replace('UNIFORM_DATA', function(sh, 'struct gl_uniform_data_t') + ';')
    suffix = r'''
struct Result{std::vector<std::string> gl;};
void writeFile(const std::string& p,const std::string& text){
 std::filesystem::create_directories(std::filesystem::path(p).parent_path());
 std::ofstream(p,std::ios::binary)<<text;
}
template<bool diagnostics> Result scenario(const std::string& mode,const std::string& path){
 calls.clear();sources.clear();attached.clear();nextHandle=1;LLGLSLShader::sInstances.clear();
 failLink=false;rejectBinary=false;units.clear();blockBinding=0;
 LLShaderMgr mgr;currentMgr=&mgr;mgr.root=path;mgr.mShaderCacheDir=path;
 fixtureDir.root=path;
 std::filesystem::create_directories(path);
 writeFile(path+"/shader_equivalence.jsonl","STALE EARLIER RUN\n");
 mgr.configureShaderDiagnostics(diagnostics,"",false);
 std::unique_ptr<LLShaderMgr::ShaderDiagnosticPass> pass;
 if(mode!="startup" && mode!="startup_inject")pass=std::make_unique<LLShaderMgr::ShaderDiagnosticPass>(&mgr);
 // Every file is fixture-owned. No installed/viewer shader files are opened.
 for(const auto& file:{"rootV.glsl","rootF.glsl","compute.glsl","deferred/textureUtilV.glsl","deferred/globalF.glsl","objects/nonindexedTextureV.glsl","windlight/gammaF.glsl","windlight/atmosphericsVarsF.glsl","windlight/atmosphericsHelpersF.glsl","windlight/atmosphericsVarsV.glsl","windlight/atmosphericsHelpersV.glsl"})
   writeFile(path+"/class2/"+file,"void fixture(){}\n");
 if(mode=="all_fail"){
   writeFile(path+"/class2/rootV.glsl","FAIL\n");writeFile(path+"/class1/rootV.glsl","FAIL\n");
 }
 if(mode=="file_fallback"){
   writeFile(path+"/class1/only.glsl","void fixture(){}\n");
 }
 if(mode=="shared_fallback")writeFile(path+"/class3/deferred/textureUtilV.glsl","FAIL\n");
 for(const auto& file:{"deferred/textureUtilV.glsl","deferred/globalF.glsl","objects/nonindexedTextureV.glsl","windlight/gammaF.glsl","windlight/atmosphericsVarsF.glsl","windlight/atmosphericsHelpersF.glsl","windlight/atmosphericsVarsV.glsl","windlight/atmosphericsHelpersV.glsl"}){
   int level=mode=="shared_direct2" && std::string(file)=="deferred/textureUtilV.glsl"?2:3;
   auto t=std::string(file).find("F.glsl")!=std::string::npos?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER;
   CHECK(mgr.loadShaderFileInternal<diagnostics>(file,level,t,nullptr,0));
   if(diagnostics){
     std::ifstream journal(path+"/shader_equivalence.jsonl");
     std::string text((std::istreambuf_iterator<char>(journal)),{});
     CHECK(text.find("STALE EARLIER RUN")==std::string::npos);
     CHECK(text.find("module_load")!=std::string::npos || text.find("pass_begin")!=std::string::npos);
   }
 }
 LLGLSLShader s;
 if(mode=="gamma")s.mFeatures.hasGamma=true;
 s.mShaderFiles={{mode=="file_fallback"?"only.glsl":"rootV.glsl",GL_VERTEX_SHADER},{"rootF.glsl",GL_FRAGMENT_SHADER}};
 if(mode=="compute")s.mShaderFiles={{"compute.glsl",LLGLSLShader::COMPUTE_STAGE}};
 if(mode=="startup" || mode=="startup_inject")s.mName="Benchmark Shader";
 if(mode=="startup_inject")mgr.configureShaderDiagnostics(diagnostics,"link:Benchmark Shader",false);
 if(mode=="accepted_binary" || mode=="driver_rejected" || mode=="read_failure"){
   s.mShaderHash=s.hash();mgr.mShaderBinaryCache[s.mShaderHash]={4,1,0};
   writeFile(path+"/"+s.mShaderHash.asString()+".shaderbin",mode=="read_failure"?"A":"ABCD");
   rejectBinary=mode=="driver_rejected";
 }
 if(mode=="synthetic_rejected")mgr.configureShaderDiagnostics(diagnostics,"binary:fixture",false);
 if(mode=="disabled")mgr.mShaderCacheEnabled=false;
 bool success=s.createShader();
 CHECK(success==(mode!="all_fail" && mode!="startup_inject"));
 if(mode=="reload")CHECK(s.createShader());
 pass.reset();
 if(mode=="all_fail")CHECK(mgr.mVertexShaderObjects.count("rootV.glsl")==0);
 Result result;result.gl=calls;
 if(diagnostics){CHECK(mgr.mShaderDumpOpened && mgr.mShaderEquivalence.is_open());}

 s.unloadInternal();
 return result;
}
int main(int argc,char** argv){
 CHECK(argc==2);std::string root=argv[1];
 for(const auto& mode:{"cold","all_fail","file_fallback","shared_fallback","shared_direct2","reload","compute","startup","accepted_binary","gamma","disabled","read_failure","driver_rejected","synthetic_rejected"}){
   // Identical filesystem inputs and reset driver state for both algorithms.
   std::string path=root+"/"+mode;
   auto off=scenario<false>(mode,path);
   auto on=scenario<true>(mode,path);
   CHECK(off.gl==on.gl);
   std::cout<<"producer/differential "<<mode<<" PASS ("<<on.gl.size()<<" calls)\n";
 }
 auto baseline=scenario<false>("accepted_binary",root+"/planted");
 plantRegression=true;
 auto broken=scenario<true>("accepted_binary",root+"/planted");
 CHECK(baseline.gl!=broken.gl);
 plantRegression=false;
 std::cout<<"rejected: early return after traceFeatureAttachment in full warm create/load/attach path\n";
 plantReflection=true;
 auto reflected=scenario<true>("cold",root+"/planted_reflection");
 plantReflection=false;
 auto plain=scenario<false>("cold",root+"/planted_reflection");
 CHECK(reflected.gl!=plain.gl);
 std::cout<<"rejected: state-changing GL call inside active sampler-array observer loop\n";
 plantGamma=true;
 auto gammaOn=scenario<true>("gamma",root+"/planted_gamma");
 plantGamma=false;
 auto gammaOff=scenario<false>("gamma",root+"/planted_gamma");
 CHECK(gammaOn.gl!=gammaOff.gl);
 std::cout<<"rejected: diagnostics-only hasGamma regression\n";
 auto injection=scenario<true>("startup_inject",root+"/startup_inject");
 LLShaderMgr mgr;currentMgr=&mgr;
 mgr.configureShaderDiagnostics(false,"link:Program",false);
 CHECK(mgr.injectShaderFailure("link","Program") && !mgr.mShaderCacheEnabled);
 mgr.configureShaderDiagnostics(false,"",false);
 CHECK(!mgr.injectShaderFailure("link","Program") && !mgr.mShaderDiagnosticsEnabled);
 CHECK(mgr.mShaderInjectionActive && !mgr.mShaderCacheEnabled);
 CHECK(mgr.linkProgramObjectInternal<true>(1,false,"Program"));
 std::cout<<"production configure: clearing disarms entries, cache-disable latch survives PASS\n";
 LLShaderMgr second;currentMgr=&second;fixtureDir.root=root+"/second";
 writeFile(fixtureDir.root+"/shader_equivalence.jsonl","FIRST INSTANCE\n");
 second.configureShaderDiagnostics(true,"",true);
 second.beginShaderPass("second_instance");second.finishShaderPass();
 CHECK(!second.mShaderDumpOpened && second.mShaderDumpSecondWarned);
 std::ifstream protectedFile(fixtureDir.root+"/shader_equivalence.jsonl");std::string protectedText;
 std::getline(protectedFile,protectedText);CHECK(protectedText=="FIRST INSTANCE");
 std::cout<<"startup journal: stale file replaced at first operation, early records flushed, second instance preserves file PASS\n";
}
'''
    flags, libraries = viewer_native_flags()
    # Assertions use CHECK and remain enabled with the viewer's Release flags.
    exe = compile_run(directory, prefix + methods + suffix, name='producer_paths', flags=flags,
                      libraries=libraries, run=False)
    import subprocess
    result = subprocess.run([str(exe), str(directory)], cwd=ROOT, capture_output=True, text=True)
    print(result.stdout, end='')
    assert result.returncode == 0, result.stderr
    from shader_equivalence_diff import load
    dumps = {}
    for mode in ('cold', 'all_fail', 'file_fallback', 'shared_fallback', 'shared_direct2',
                 'reload', 'compute', 'startup', 'accepted_binary', 'planted', 'startup_inject',
                 'gamma', 'disabled', 'read_failure', 'driver_rejected', 'synthetic_rejected'):
        dumps[mode] = load(directory / mode / 'shader_equivalence.jsonl')
    def programs(mode):
        return [r for r in dumps[mode] if r['type'] == 'program']
    assert not programs('all_fail')[-1]['success']
    assert len(programs('reload')) == 2
    assert all(s['compile_outcome'] == 'reused' for s in programs('reload')[1]['stages'])
    assert programs('startup')[0]['name'] == 'Benchmark Shader'
    assert not programs('startup_inject')[-1]['success']
    assert programs('compute')[0]['shader_files'][0]['stage'] == 0x91B9
    assert programs('file_fallback')[0]['stages'][0]['source_level'] == 1
    assert programs('file_fallback')[0]['stages'][0]['requested_level'] == 3
    assert programs('accepted_binary')[0]['stages'] == []
    assert programs('accepted_binary')[0]['root_load_outcome'] == 'skipped'
    assert programs('accepted_binary')[0]['link_outcome'] == 'not_attempted'
    for mode, outcome, ran in [('cold', 'miss', False), ('disabled', 'disabled', False),
                               ('read_failure', 'read_failure', False), ('driver_rejected', 'driver_rejected', True),
                               ('synthetic_rejected', 'synthetic_rejected', False), ('accepted_binary', 'accepted', True)]:
        assert programs(mode)[0]['binary_outcome'] == outcome, mode
        assert programs(mode)[0]['program_binary_ran'] == ran, mode
    for mode in ('cold', 'gamma', 'accepted_binary'):
        record = programs(mode)[0]
        assert record['attributes'] == {'position': 0}
        assert record['samplers'] == {'diffuseMap[0]': 0, 'diffuseMap[1]': 1}
        assert record['ubo_bindings'] == {'GLTFJoints': 1}
        assert record['texture'] == [0] and record['active_texture_channels'] == 2
        assert record['uniform_map'] == {'diffuseMap': 2}
        assert record['epoch_neutral_key'] != record['cache_key']
    assert programs('gamma')[0]['features']['hasGamma']
    assert any(a['outcome'] == 'skipped' for a in programs('accepted_binary')[0]['attachments'])
    assert compare(dumps['shared_fallback'], dumps['shared_direct2'])
    assert programs('shared_fallback') == programs('shared_direct2'), 'history fixture must converge to identical program records'
    history = [r for r in dumps['shared_fallback'] if r['type'] == 'module_load' and r['file'] == 'deferred/textureUtilV.glsl']
    assert [(r['requested_level'], r['resolved_level'], r['source_level'], r['compile_outcome']) for r in history] == [(3, 3, 3, 'failed'), (2, 2, 2, 'compiled')]
    assert compare(dumps['accepted_binary'], dumps['planted'])
    from shader_equivalence_diff import load
    startup = load(directory / 'startup/shader_equivalence.jsonl')
    assert any(r.get('name') == 'Benchmark Shader' for r in startup)
    print('production records: binary outcomes and glProgramBinary execution, real mapper, attributes/sampler arrays/UBOs/gamma;  fallback history, levels, binary skips, reload, compute, startup injection and planted attachment regression PASS')
