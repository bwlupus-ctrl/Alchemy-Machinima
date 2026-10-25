"""Standalone compilation of the production loader/program lifecycle, with real WGL.

Viewer logging, filesystem services and graphics-feature modules are fixtures.
Source assembly, capability gating, caches, stage selection, linking and cleanup
are the actual C++ methods. No shader version is rewritten by this fixture.
"""
import ctypes as C
import re
from weather_cpu_check import compiler_command, prepare

def function(text, signature):
    # Ignore braces in C++ strings and comments (loader-generated GLSL contains
    # braces that do not balance at the C++ method boundary).
    tokens = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]')
    start=text.index(signature);opening=text.index('{',start);depth=0
    for token in tokens.finditer(text,opening):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return text[start:token.end()]
    raise AssertionError('unclosed '+signature)


def declaration(text, symbol):
    """A production global must exist; never repair a missing declaration in a fixture."""
    match = re.search(r'^\s*(?:std::string|ALWeatherUIModel|inline static U32)\s+' + re.escape(symbol) + r'\s*[^;]*;', text, re.M)
    assert match, 'missing production declaration: ' + symbol
    return match.group().strip()


def build(tree, out, gl, extend=None):
    prepare(out)
    manager = tree.read('indra/llrender/llshadermgr.cpp')
    shader = tree.read('indra/llrender/llglslshader.cpp')
    methods = '\n'.join(function(manager, signature) for signature in (
        'bool LLShaderMgr::computeShadersSupported(', 'S32 LLShaderMgr::shaderLanguageVersion(',
        'bool LLShaderMgr::ownsShaderObject(', 'void LLShaderMgr::clearShaderObjects(',
        'GLuint LLShaderMgr::loadShaderFile(', 'bool LLShaderMgr::linkProgramObject('))
    methods += '\n' + '\n'.join(function(shader, signature) for signature in (
        'bool LLGLSLShader::isCompute(', 'void LLGLSLShader::unloadInternal(',
        'bool LLGLSLShader::createShader(', 'bool LLGLSLShader::link(',
        'void LLGLSLShader::attachObject(', 'bool LLGLSLShader::mapAttributes(',
        'LLUUID LLGLSLShader::hash('))
    entries = [
        ('glGetError', 'unsigned', '', C.c_uint, []),
        ('glGetIntegerv', 'void', 'unsigned,int*', None, [C.c_uint,C.c_void_p]),
        ('glGetIntegeri_v', 'void', 'unsigned,unsigned,int*', None, [C.c_uint,C.c_uint,C.c_void_p]),
        ('glDispatchCompute', 'void', 'unsigned,unsigned,unsigned', None, [C.c_uint]*3),
        ('glMemoryBarrier', 'void', 'unsigned', None, [C.c_uint]),
        ('glBindBufferBase', 'void', 'unsigned,unsigned,unsigned', None, [C.c_uint]*3),
        ('glBindImageTexture', 'void', 'unsigned,unsigned,int,unsigned char,int,unsigned,unsigned', None,
         [C.c_uint,C.c_uint,C.c_int,C.c_ubyte,C.c_int,C.c_uint,C.c_uint]),
        ('glShaderStorageBlockBinding', 'void', 'unsigned,unsigned,unsigned', None, [C.c_uint]*3),
        ('glCreateShader', 'unsigned', 'unsigned', C.c_uint, [C.c_uint]),
        ('glShaderSource', 'void', 'unsigned,int,const char**,const int*', None, [C.c_uint,C.c_int,C.c_void_p,C.c_void_p]),
        ('glCompileShader', 'void', 'unsigned', None, [C.c_uint]),
        ('glGetShaderiv', 'void', 'unsigned,unsigned,int*', None, [C.c_uint,C.c_uint,C.c_void_p]),
        ('glDeleteShader', 'void', 'unsigned', None, [C.c_uint]),
        ('glCreateProgram', 'unsigned', '', C.c_uint, []),
        ('glDeleteProgram', 'void', 'unsigned', None, [C.c_uint]),
        ('glAttachShader', 'void', 'unsigned,unsigned', None, [C.c_uint]*2),
        ('glDetachShader', 'void', 'unsigned,unsigned', None, [C.c_uint]*2),
        ('glGetAttachedShaders', 'void', 'unsigned,int,int*,unsigned*', None, [C.c_uint,C.c_int,C.c_void_p,C.c_void_p]),
        ('glIsShader', 'unsigned char', 'unsigned', C.c_ubyte, [C.c_uint]),
        ('glLinkProgram', 'void', 'unsigned', None, [C.c_uint]),
        ('glGetProgramiv', 'void', 'unsigned,unsigned,int*', None, [C.c_uint,C.c_uint,C.c_void_p]),
        ('glBindFragDataLocation', 'void', 'unsigned,unsigned,const char*', None, [C.c_uint,C.c_uint,C.c_char_p]),
        ('glBindAttribLocation', 'void', 'unsigned,unsigned,const char*', None, [C.c_uint,C.c_uint,C.c_char_p]),
        ('glGetAttribLocation', 'int', 'unsigned,const char*', C.c_int, [C.c_uint,C.c_char_p]),
        ('glDeleteQueries', 'void', 'int,const unsigned*', None, [C.c_int,C.c_void_p]),
        ('glUseProgram', 'void', 'unsigned', None, [C.c_uint]),
    ]
    enums = dict(GL_NO_ERROR=0, GL_TRUE=1, GL_FALSE=0, GL_VERTEX_SHADER=0x8B31,
                 GL_FRAGMENT_SHADER=0x8B30,GL_GEOMETRY_SHADER=0x8DD9,GL_COMPILE_STATUS=0x8B81,
                 GL_LINK_STATUS=0x8B82,GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS=0x90DB,
                 GL_MAX_IMAGE_UNITS=0x8F38,GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS=0x90EB,
                 GL_MAX_COMPUTE_WORK_GROUP_COUNT=0x91BE)
    prefix = r'''
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <stdexcept>
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER
#define LL_PROFILE_ZONE_NAMED_CATEGORY_SHADER(x)
#define LL_DEBUGS(x) NullLog{}
#define LL_INFOS(x) NullLog{}
#define LL_WARNS(x) NullLog{}
#define LL_ERRS() NullLog{}
#define LL_SHADER_LOADING_WARNS() NullLog{}
#define LL_ENDL 0
#define llassert(x) do {if(!(x)) throw std::runtime_error(#x);}while(0)
#define llassert_always(x) llassert(x)
#define BOOST_STATIC_ASSERT(x) static_assert(x)
#define LL_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define LLFILE_MODE(x) x
#define stop_glerror() ((void)0)
#define flush_glerror() ((void)0)
using U32=unsigned;using S32=int;using GLchar=char;using GLuint=unsigned;
using GLenum=unsigned;using GLint=int;using GLsizei=int;using LLFILE=FILE;
using std::vector;using std::pair;using std::string;using llifstream=std::ifstream;
struct NullLog {template<class T> NullLog& operator<<(const T&){return *this;}};
struct LLUUID {std::string s;std::string asString()const{return s;}};
struct HBXXH128 {std::string s;void update(const std::string& v){s+=std::to_string(v.size())+":"+v;}
 void update(const void* v,size_t n){update(std::string((const char*)v,n));}LLUUID digest(){return {s};}};
template<class... T> std::string llformat(const char* fmt,T... args){char b[4096];snprintf(b,sizeof(b),fmt,args...);return b;}
struct LLFile {static FILE* fopen(const std::string& s,const char* m){return ::fopen(s.c_str(),m);}
 static bool isfile(const std::string& s){return std::filesystem::is_regular_file(s);}};
struct Dir {std::string getDirDelimiter(){return "/";}bool fileExists(const std::string& s){return LLFile::isfile(s);}
 std::string getExpandedFilename(int,const char*,const std::string&){return "";}} dir;
Dir* gDirUtilp=&dir;constexpr int LL_PATH_APP_SETTINGS=0;
struct Manager {int mGLSLVersionMajor=4,mGLSLVersionMinor=60,mNumTextureImageUnits=32;
 float mGLVersion=4.6f;bool mIsAMD=false,mIsNVIDIA=true;
 std::string mGLVendor="fixture",mGLRenderer="real WGL",mGLVersionString="4.6";} gGLManager;
struct LLRender {static constexpr int NUM_MATRIX_MODES=4;};
struct LLShaderFeatures {int mIndexedTextureChannels=0;bool hasWeatherMaterial=false,attachNothing=false;};
'''
    prefix += '\n'.join(f'constexpr unsigned {n}={v}U;' for n,v in enums.items())+'\n'
    prefix += '\n'.join(f'{ret} (*{name})({args});' for name,ret,args,_,_ in entries)+'\n'
    prefix += r'''
struct LLGLSLShader;
struct LLShaderMgr {
 std::map<std::string,GLuint> mShaderObjects,mVertexShaderObjects,mFragmentShaderObjects;
 std::vector<std::string> mReservedUniforms,mReservedAttribs;
 std::string root;int features=0,attributes=0,frag_outputs=0;
 static LLShaderMgr* instance(){static LLShaderMgr s;return &s;}
 static bool computeShadersSupported();static S32 shaderLanguageVersion(GLenum);
 CONTEXT_DECLARATION
 bool ownsShaderObject(GLuint)const;void clearShaderObjects();
 GLuint loadShaderFile(const std::string&,S32&,GLenum,std::map<std::string,std::string>* =nullptr,S32=-1);
 bool linkProgramObject(GLuint,bool=false);
 std::string getShaderDirPrefix(){return root+"/class";}
 void prepareWeatherFeatures(LLGLSLShader*){}
 bool attachShaderFeatures(LLGLSLShader*){++features;return true;}
 bool loadCachedProgramBinary(LLGLSLShader*){return false;}
 bool saveCachedProgramBinary(LLGLSLShader*){return true;}
 void dumpObjectLog(GLuint,bool=true,const std::string& =""){}
 void dumpShaderSource(U32,GLchar**){}
};
struct LLGLSLShader {
 static constexpr GLenum COMPUTE_STAGE=0x91B9;
 inline static std::set<LLGLSLShader*> sInstances;
 inline static std::map<std::string,std::string> sGlobalDefines;
 inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;
 std::vector<std::pair<std::string,GLenum>> mShaderFiles;
 std::map<std::string,std::string> mDefines;LLShaderFeatures mFeatures;
 GLuint mProgramObject=0,mTimerQuery=0,mSamplesQuery=0,mPrimitivesQuery=0;
 U32 mAttributeMask=0,mMatHash[4]{},mLightHash=0;
 std::vector<int> mAttribute,mTexture,mUniform;std::map<int,int> mValue;
 std::map<std::string,int> mUniformMap;bool mUsingBinaryProgram=false;
 int mShaderLevel=1,mShaderGroup=0,mActiveTextureChannels=0;std::string mName="fixture";LLUUID mShaderHash;
 bool isCompute()const;bool createShader();void unloadInternal();bool link(bool=false);
 void attachObject(GLuint);bool mapAttributes();LLUUID hash();
 bool mapUniforms(){return true;}
 static void unbind(){glUseProgram(0);sCurBoundShaderPtr=nullptr;}
};
'''
    prefix = prefix.replace('CONTEXT_DECLARATION', declaration(tree.read('indra/llrender/llshadermgr.h'), 'sContextGeneration'))
    # Run the native uniform discovery, ordering and sampler-channel assignment.
    from weather_native_fixture import build as mapper_parts
    mp, mm, me, reserved = mapper_parts(tree, out, gl, parts=True)
    prefix = '#include <climits>\n' + prefix
    prefix = prefix.replace('struct LLShaderFeatures {int mIndexedTextureChannels=0;', 'struct LLShaderFeatures {int mIndexedMaterialChannels=0,mIndexedTextureChannels=0;')
    prefix = prefix.replace('bool mapUniforms(){return true;}', 'bool mapUniforms();')
    prefix = prefix.replace(' bool mapUniforms();', function(tree.read('indra/llrender/llglslshader.h'), 'struct gl_uniform_data_t') + ';\n static constexpr int sIndexedTextureChannels=4,NUM_UNIFORM_BLOCKS=4;\n int mTotalUniformSize=0;\n void bind(){glUseProgram(mProgramObject);}\n void mapUniform(const gl_uniform_data_t&);GLint mapUniformTextureChannel(GLint,GLenum,GLint);\n bool mapUniforms();')
    prefix = prefix.replace('using U32=unsigned;', 'using LLStaticHashedString=std::string;\n template<class T>T llmin(T a,T b){return std::min(a,b);}\n template<class T>T llmax(T a,T b){return std::max(a,b);}\n using U32=unsigned;')
    prefix = '#define llassert_always_msg(x,y) llassert(x)\n' + prefix
    for n,v in re.findall(r'constexpr unsigned (\w+)=(\d+)U;', mp):
        if 'constexpr unsigned '+n+'=' not in prefix: prefix = f'constexpr unsigned {n}={v}U;\n' + prefix
    for item in me:
        if item[0] not in {e[0] for e in entries}:
            n,ret,args,_,_ = item
            prefix = f'{ret} (*{n})({args});\n' + prefix
            entries.append(item)
    methods += '\n' + mm
    suffix = '\nextern "C" __declspec(dllexport) void initialize(void** p,const char* root){\n'
    suffix += '\n'.join(f'{name}=reinterpret_cast<decltype({name})>(p[{i}]);' for i,(name,*_) in enumerate(entries))
    suffix += 'LLShaderMgr::instance()->root=root;LLShaderMgr::instance()->mReservedUniforms={' + ','.join('"'+n+'"' for n in reserved) + '};}\n'
    attributes = re.findall(r'mReservedAttribs.push_back\("([^"]+)"\)', function(manager, 'void LLShaderMgr::initAttribsAndUniforms('))
    suffix = suffix.replace('mReservedUniforms={', 'mReservedAttribs={' + ','.join('"'+n+'"' for n in attributes) + '};LLShaderMgr::instance()->mReservedUniforms={')
    suffix += r'''
extern "C" __declspec(dllexport) void capability(int version,int entry){
 gGLManager.mGLVersion=version?4.6f:4.1f;gGLManager.mGLSLVersionMinor=version?60:10;
 if(!entry)glDispatchCompute=nullptr;}
extern "C" __declspec(dllexport) unsigned load(const char* file,unsigned stage,int permutation){
 int level=1;std::map<std::string,std::string> defs;
 if(permutation)defs["FIXTURE_PERMUTATION"]=std::to_string(permutation);
 return LLShaderMgr::instance()->loadShaderFile(file,level,stage,&defs,0);}
extern "C" __declspec(dllexport) unsigned program(const char* file,unsigned stage,int mixed){
 LLGLSLShader s;s.mShaderFiles.push_back({file,stage});
 if(mixed)s.mShaderFiles.push_back({file,GL_VERTEX_SHADER});
 if(!s.createShader())return 0;unsigned p=s.mProgramObject;s.mProgramObject=0;
 LLGLSLShader::sInstances.erase(&s);return p;}
extern "C" __declspec(dllexport) void clear(){LLShaderMgr::instance()->clearShaderObjects();}
extern "C" __declspec(dllexport) int feature_count(){return LLShaderMgr::instance()->features;}
extern "C" __declspec(dllexport) int instance_count(){return int(LLGLSLShader::sInstances.size());}
extern "C" __declspec(dllexport) void language(int major,int minor){gGLManager.mGLSLVersionMajor=major;gGLManager.mGLSLVersionMinor=minor;}
extern "C" __declspec(dllexport) unsigned graphics(const char* vertex,const char* fragment){
 LLGLSLShader s;s.mShaderFiles={{vertex,GL_VERTEX_SHADER},{fragment,GL_FRAGMENT_SHADER}};s.mDefines["COPY_DEPTH"]="1";
 if(!s.createShader())return 0;unsigned p=s.mProgramObject;s.mProgramObject=0;LLGLSLShader::sInstances.erase(&s);return p;}
'''
    sources=[]
    if extend is None:
        # Binding a compute program must never touch the graphics attribute
        # cache, including during uniform discovery. Execute native bind/unbind.
        prefix=prefix.replace('struct LLGLSLShader {', '''
struct FixtureRender {void flush(){}} gGL;
struct LLVertexBuffer {inline static int calls=0;static void unbind(){++calls;}
 static void setupClientArrays(unsigned){++calls;}};
struct LLGLSLShader {''')
        prefix=prefix.replace('void prepareWeatherFeatures(LLGLSLShader*){}',
            'void updateWeatherUniforms(LLGLSLShader*){} void updateShaderUniforms(LLGLSLShader*){} void prepareWeatherFeatures(LLGLSLShader*){}')
        prefix=prefix.replace('inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;',
            'inline static LLGLSLShader* sCurBoundShaderPtr=nullptr; inline static GLuint sCurBoundShader=0; bool mUniformsDirty=false;')
        prefix = prefix.replace(' void bind(){glUseProgram(mProgramObject);}', ' void bind();void unload();void placeProfileQuery(){}void readProfileQuery(){}')
        prefix=prefix.replace(' static void unbind(){glUseProgram(0);sCurBoundShaderPtr=nullptr;}','')
        prefix=prefix.replace(' void bind();void unload();', ' void bind();static void unbind();void unload();')
        methods+='\n'+function(shader,'void LLGLSLShader::bind()')+'\n'+function(shader,'void LLGLSLShader::unbind(void)')
        methods+='\n'+function(shader,'void LLGLSLShader::unload()')
        suffix+='''
extern "C" __declspec(dllexport) int bind_attributes(unsigned program,int compute){
 LLGLSLShader s;s.mProgramObject=program;s.mShaderFiles.push_back({"fixture",compute?LLGLSLShader::COMPUTE_STAGE:GL_VERTEX_SHADER});
 LLVertexBuffer::calls=0;s.bind();s.unbind();return LLVertexBuffer::calls;}
extern "C" __declspec(dllexport) int unload_attributes(unsigned program){
 LLGLSLShader s;s.mProgramObject=program;s.mShaderFiles.push_back({"fixture",LLGLSLShader::COMPUTE_STAGE});
 s.bind();LLVertexBuffer::calls=0;s.unload();return LLVertexBuffer::calls;}
'''
    if extend: prefix,methods,suffix,entries,sources=extend(prefix,methods,suffix,entries)
    path = out/'compute_loader.cpp'; path.write_text(prefix+methods+suffix,encoding='utf-8')
    dll = C.CDLL(str(compiler_command(out,[path,*sources],'compute_loader',dll=True)))
    pointers = (C.c_void_p*len(entries))(*(C.cast(gl.fn(name,ret,*args),C.c_void_p).value
        for name,_,_,ret,args in entries))
    dll.initialize.argtypes=[C.POINTER(C.c_void_p),C.c_char_p]
    dll.initialize(pointers,str(out).encode())
    dll.load.argtypes=[C.c_char_p,C.c_uint,C.c_int];dll.load.restype=C.c_uint
    dll.program.argtypes=[C.c_char_p,C.c_uint,C.c_int];dll.program.restype=C.c_uint
    dll.capability.argtypes=[C.c_int,C.c_int]
    return dll
