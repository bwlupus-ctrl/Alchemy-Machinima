"""Compile the production sampler mapper and weather feature selector for WGL tests.

Only GL entry points and viewer services are fixtures. Uniform discovery, sorting,
channel allocation and feature selection execute the unchanged native functions.
"""
import ctypes as C
import re

from weather_cpu_check import compiler_command, prepare
from weather_preparation_fixture import function


def build(tree, out, gl, mutation=None):
    prepare(out)
    shader = tree.read('indra/llrender/llglslshader.cpp')
    manager = tree.read('indra/llrender/llshadermgr.cpp')
    methods = '\n'.join(function(shader, sig) for sig in (
        'static bool is_sampler_type(', 'void LLGLSLShader::mapUniform(',
        'GLint LLGLSLShader::mapUniformTextureChannel(', 'bool LLGLSLShader::mapUniforms('))
    selector = function(manager, 'void LLShaderMgr::prepareWeatherFeatures(')
    if mutation == 'avatar-eyes':
        selector = selector.replace('file.first == "deferred/diffuseF.glsl" ||',
            'file.first == "deferred/diffuseAlphaMaskNoColorF.glsl" || file.first == "deferred/diffuseF.glsl" ||')
    reserved = re.findall(r'mReservedUniforms.push_back\("([^"]+)"\)',
                         function(manager, 'void LLShaderMgr::initAttribsAndUniforms('))
    if mutation == 'sampler-order':
        reserved.remove('weather_surface_cover_map')
        reserved.insert(0, 'weather_surface_cover_map')
    # Values are OpenGL's public enum values, including the vector types used by
    # production uniform-size bookkeeping. No GL header/library dependency.
    enums = dict(GL_SAMPLER_1D=0x8B5D, GL_SAMPLER_2D_RECT_SHADOW=0x8B64,
                 GL_SAMPLER_2D_MULTISAMPLE=0x9108, GL_SAMPLER_CUBE_MAP_ARRAY=0x900C,
                 GL_ACTIVE_UNIFORMS=0x8B86, GL_INVALID_INDEX=0xFFFFFFFF,
                 GL_FRAGMENT_SHADER=0x8B30, GL_DOUBLE=0x140A)
    for base, names in ((0x8B50, ['FLOAT_VEC2','FLOAT_VEC3','FLOAT_VEC4','INT_VEC2','INT_VEC3','INT_VEC4',
                                 'BOOL','BOOL_VEC2','BOOL_VEC3','BOOL_VEC4','FLOAT_MAT2','FLOAT_MAT3','FLOAT_MAT4']),
                        (0x8B65, ['FLOAT_MAT2x3','FLOAT_MAT2x4','FLOAT_MAT3x2','FLOAT_MAT3x4','FLOAT_MAT4x2','FLOAT_MAT4x3']),
                        (0x8DC6, ['UNSIGNED_INT_VEC2','UNSIGNED_INT_VEC3','UNSIGNED_INT_VEC4']),
                        (0x8FFC, ['DOUBLE_VEC2','DOUBLE_VEC3','DOUBLE_VEC4']),
                        (0x8F46, ['DOUBLE_MAT2','DOUBLE_MAT3','DOUBLE_MAT4','DOUBLE_MAT2x3','DOUBLE_MAT2x4',
                                 'DOUBLE_MAT3x2','DOUBLE_MAT3x4','DOUBLE_MAT4x2','DOUBLE_MAT4x3'])):
        enums.update(('GL_'+name, base+i) for i, name in enumerate(names))
    entries = [
        ('glGetUniformLocation', 'int', 'unsigned,const char*', C.c_int, [C.c_uint,C.c_char_p]),
        ('glUniform1i', 'void', 'int,int', None, [C.c_int,C.c_int]),
        ('glUniform1iv', 'void', 'int,int,const int*', None, [C.c_int,C.c_int,C.c_void_p]),
        ('glGetProgramiv', 'void', 'unsigned,unsigned,int*', None, [C.c_uint,C.c_uint,C.c_void_p]),
        ('glGetActiveUniform', 'void', 'unsigned,unsigned,int,int*,int*,unsigned*,char*', None,
         [C.c_uint,C.c_uint,C.c_int,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p]),
        ('glGetUniformBlockIndex', 'unsigned', 'unsigned,const char*', C.c_uint, [C.c_uint,C.c_char_p]),
        ('glUniformBlockBinding', 'void', 'unsigned,unsigned,unsigned', None, [C.c_uint]*3),
        ('glUseProgram', 'void', 'unsigned', None, [C.c_uint]),
    ]
    prefix = r'''
#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER
#define LL_DEBUGS(x) NullLog{}
#define LL_WARNS(x) NullLog{}
#define LL_ENDL 0
#define llassert(x) do { if(!(x)) throw std::runtime_error(#x); } while(0)
#define llassert_always_msg(x,y) llassert(x)
#define LL_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
struct NullLog {template<class T> NullLog& operator<<(const T&){return *this;}};
using U32=unsigned; using S32=int; using GLint=int; using GLenum=unsigned;
using GLuint=unsigned; using GLsizei=int; using GLchar=char;
using LLStaticHashedString=std::string;
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llmax(T a,T b){return std::max(a,b);}
'''
    prefix += '\n'.join(f'constexpr unsigned {name}={value}U;' for name,value in enums.items())+'\n'
    prefix += '\n'.join(f'{ret} (*{name})({params});' for name,ret,params,_,_ in entries)+'\n'
    prefix += r'''
struct LLShaderFeatures {bool hasWeatherMaterial=false,hasSkinning=false,hasObjectSkinning=false;
 int mIndexedTextureChannels=0,mIndexedMaterialChannels=0;};
struct LLGLSLShader;
struct LLShaderMgr {
 std::vector<std::string> mReservedUniforms;
 bool enabled=true;
 static LLShaderMgr* instance(){static LLShaderMgr s;return &s;}
 bool weatherMaterialsEnabled(){return enabled;}
 void prepareWeatherFeatures(LLGLSLShader*);
};
struct LLGLSLShader {
 struct gl_uniform_data_t {std::string name;GLenum type=(GLenum)-1;GLint size=-1;
 U32 texunit_priority=UINT_MAX;S32 forced_texunit=-1;};
 LLShaderFeatures mFeatures;std::map<std::string,std::string> mDefines;
 std::vector<std::pair<std::string,unsigned>> mShaderFiles;
 std::vector<int> mUniform,mTexture;std::map<std::string,int> mUniformMap;
 std::map<int,int> mValue;std::string mName="weather fixture";
 int mTotalUniformSize=0,mActiveTextureChannels=0;unsigned mProgramObject=0;
 static constexpr int sIndexedTextureChannels=4,NUM_UNIFORM_BLOCKS=4;
 void bind(){glUseProgram(mProgramObject);}void unbind(){glUseProgram(0);}
 void addPermutation(std::string n,std::string v){mDefines[n]=v;}
 void mapUniform(const gl_uniform_data_t&);GLint mapUniformTextureChannel(GLint,GLenum,GLint);
 bool mapUniforms();
};
'''
    suffix = '\nextern "C" __declspec(dllexport) void initialize(void** pointers){\n'
    suffix += '\n'.join(f'{name}=reinterpret_cast<decltype({name})>(pointers[{i}]);'
                        for i,(name,*_) in enumerate(entries))
    suffix += 'LLShaderMgr::instance()->mReservedUniforms={' + ','.join('"'+n+'"' for n in reserved) + '};}\n'
    suffix += r'''
extern "C" __declspec(dllexport) int map_program(unsigned program,int indexed,int materials){
 LLGLSLShader s;s.mProgramObject=program;s.mFeatures.mIndexedTextureChannels=indexed;
 s.mFeatures.mIndexedMaterialChannels=materials;s.mapUniforms();return s.mActiveTextureChannels;
}
extern "C" __declspec(dllexport) int feature(const char* file,int enabled,int excluded){
 LLShaderMgr::instance()->enabled=enabled!=0;LLGLSLShader s;
 s.mShaderFiles.push_back({file,GL_FRAGMENT_SHADER});
 const char* flags[]={"HAS_SKIN","AVATAR_GEOMETRY","IS_HUD","MATERIAL_VELOCITY"};
 if(excluded>0)s.mDefines[flags[excluded-1]]="1";
 LLShaderMgr::instance()->prepareWeatherFeatures(&s);
 return s.mFeatures.hasWeatherMaterial && s.mDefines.count("HAS_WEATHER_MATERIAL");
}
'''
    path = out/'native.cpp'
    path.write_text(prefix+methods+selector+suffix, encoding='utf-8')
    dll = C.CDLL(str(compiler_command(out,[path],'native',dll=True)))
    pointers = (C.c_void_p*len(entries))(*(C.cast(gl.fn(name,ret,*args),C.c_void_p).value
                                         for name,_,_,ret,args in entries))
    dll.initialize.argtypes=[C.POINTER(C.c_void_p)];dll.initialize(pointers)
    dll.map_program.argtypes=[C.c_uint,C.c_int,C.c_int]
    dll.feature.argtypes=[C.c_char_p,C.c_int,C.c_int]
    return dll
