"""Native support for actor_key_lens_fixture_check.py --real-targets.

Links existing Release libraries; compiles only this fixture DLL. Executes the
complete production coverage method/RAII and actual LLRenderTarget implementation.
Platform context lookup, deterministic depth and externally linked programs are
explicit test seams. It never builds or launches the viewer.
"""
from pathlib import Path
import sys, subprocess, ctypes as C, os
from native_check_tools import compile_run,viewer_native_flags
ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'.tmp/ckey_20261102/lens_native';out.mkdir(parents=True,exist_ok=True)
flags,libs=viewer_native_flags()
# Compile this test DLL only. All viewer libraries are existing products; no viewer build.
flags+=['/LD'] # Disable profiling only in the fixture TU, retaining library behavior.
flags=[f for f in flags if 'TRACY_ENABLE' not in f]
build=Path(subprocess.check_output(['git','rev-parse','--git-common-dir'],cwd=ROOT,text=True).strip()).resolve().parent/'build-Windows-vs2026-os'
libs=[f'"{p}"' for p in build.glob('*/Release/*.lib') if p.parent.parent.name == p.stem]
libs += [f'"{p}"' for p in build.glob('vcpkg_installed/*/lib/*.lib') if p.open('rb').read(8)==b'!<arch>\n']
libs += ['/link','/OPT:REF','/FORCE:MULTIPLE','opengl32.lib','gdi32.lib','user32.lib','advapi32.lib','bcrypt.lib','shell32.lib','ws2_32.lib','dbghelp.lib','dbgeng.lib','winmm.lib','ole32.lib','version.lib','psapi.lib','synchronization.lib','iphlpapi.lib','rpcrt4.lib','imm32.lib','comdlg32.lib','oleaut32.lib','uuid.lib','dinput8.lib','dxguid.lib','shcore.lib','dwmapi.lib','wbemuuid.lib','dxgi.lib']
src=r'''
#include "linden_common.h"
#include "llgl.h"
#include "llcommon.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llshadermgr.h"
class LensShaderMgr:public LLShaderMgr{public:LensShaderMgr(){sInstance=this;}std::string getShaderDirPrefix()override{return "";}void updateShaderUniforms(LLGLSLShader*)override{}};
static LensShaderMgr lensShaderManager;
// Hidden-WGL context has no LLWindowWin32 instance; supply its platform loader seam.
class LLWindowWin32 {public:static PROC WINAPI getProcAddress(const char*);};
PROC WINAPI LLWindowWin32::getProcAddress(const char* name){PROC p=wglGetProcAddress(name);return p?p:GetProcAddress(GetModuleHandleW(L"opengl32.dll"),name);}
extern "C" {
__declspec(dllexport) int init(){LLCommon::initClass();gGLManager.initGL();gGL.init(false);LLRenderTarget::sUseFBO=true;return 1;}
__declspec(dllexport) void* make_target(unsigned w,unsigned h,unsigned n){
 auto* t=new LLRenderTarget;
 if(!t->allocate(w,h,GL_RGBA16F,false)){delete t;return nullptr;}
 for(unsigned i=1;i<n;++i)if(!t->addColorAttachment(i==2?GL_RG16F:GL_RGBA16F)){delete t;return nullptr;}
 return t;
}
__declspec(dllexport) unsigned texture(void* p,unsigned i){return static_cast<LLRenderTarget*>(p)->getTexture(i);}
__declspec(dllexport) unsigned fbo(void* p){return static_cast<LLRenderTarget*>(p)->getFBO();}
__declspec(dllexport) void lens_bind(void* p){static_cast<LLRenderTarget*>(p)->bindTarget();}
__declspec(dllexport) void lens_flush(void* p){static_cast<LLRenderTarget*>(p)->flush();}
__declspec(dllexport) void drop(void* p){delete static_cast<LLRenderTarget*>(p);}
__declspec(dllexport) unsigned bytes(){return LLRenderTarget::sBytesAllocated;}
}
'''
# Execute complete production lens coverage method and its real GL RAII scopes.
# Shader compiler is an external seam: Python supplies already linked production
# permutations; the production draw still copies all resolved uniforms/bindings.
from relight_area_policy_check import method
pipeline=(ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
scopes=pipeline[pipeline.index('struct ActorKeyTextureScope'):pipeline.index('// Copy from DRAW,')]
release=pipeline[pipeline.index('    mKeyDiopterCov.release();',pipeline.index('void LLPipeline::releaseActorKeyTargets()')):pipeline.index('    mActorKeyCoverage.release();',pipeline.index('void LLPipeline::releaseActorKeyTargets()'))]
src+="""
#include "llglslshader.h"
#include <map>
#include <memory>
struct ALActorChromaKey {inline static bool sActive=true;};
static U32 gFrameCount=42;
struct ActorKeyLensProgram{LLGLSLShader shader;LLUUID beautyHash;};
static std::map<LLGLSLShader*,std::unique_ptr<ActorKeyLensProgram>>sActorKeyLensPrograms;
struct Triangle
{
 GLuint vbo=0;
 void setBuffer(){
  if(!vbo){glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
   const float vertices[]={-1,-1,0,3,-1,0,-1,3,0};glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);}
  glBindBuffer(GL_ARRAY_BUFFER,vbo);
  GLint program;glGetIntegerv(GL_CURRENT_PROGRAM,&program);
  GLint ix=glGetAttribLocation(program,"position");glEnableVertexAttribArray(ix);glVertexAttribPointer(ix,3,GL_FLOAT,GL_FALSE,12,nullptr);
 }
 void drawArrays(U32,S32,S32){glDrawArrays(GL_TRIANGLES,0,3);}
};
struct LLPipeline
{
 LLRenderTarget mActorKeyMatte,mKeyDiopterCov,mKeyLensStage[3];
 LLRenderTarget* mKeyLensCurrent=nullptr;
 U32 mKeyLensFrame=42,mKeyLensRan=0,mKeyLensComplete=0;
 Triangle triangle;Triangle* mScreenTriangleVB=&triangle;
 bool matte=true;bool actorKeyMatteValid()const{return matte;}
 void renderActorKeyLens(U32,LLGLSLShader&,U32,U32);
 bool lensCoverageValid()const;
 void releaseLens(){
"""+release+"""}
};
"""+scopes+method(ROOT/'indra/newview/pipeline.cpp','bool LLPipeline::lensCoverageValid() const')+method(ROOT/'indra/newview/pipeline.cpp','void LLPipeline::renderActorKeyLens(')
src+="""
static LLPipeline* pipe=nullptr;
static LLGLSLShader beautyShaders[4];
extern "C" {
__declspec(dllexport) void lens_active(unsigned unit){gGL.getTexUnit(unit)->activate();}
__declspec(dllexport) void begin_lens(unsigned width,unsigned height,const float* input){
 if(!pipe)pipe=new LLPipeline;
 pipe->mKeyLensFrame=gFrameCount;pipe->mKeyLensRan=pipe->mKeyLensComplete=0;pipe->mKeyLensCurrent=nullptr;
 ALActorChromaKey::sActive=true;pipe->matte=true;
 if(!pipe->mActorKeyMatte.isComplete() || pipe->mActorKeyMatte.getWidth()!=width || pipe->mActorKeyMatte.getHeight()!=height){
  pipe->mActorKeyMatte.release();pipe->mActorKeyMatte.allocate(width,height,GL_RG16F,false);
 }
 gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE,pipe->mActorKeyMatte.getTexture());
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_FLOAT,input);
}
__declspec(dllexport) int run_lens(unsigned stage,unsigned beauty,unsigned coverage,unsigned width,unsigned height){
 auto& b=beautyShaders[stage];b.mProgramObject=beauty;b.mShaderHash=LLUUID::generateNewID();
 GLint count;glGetProgramiv(beauty,GL_ACTIVE_UNIFORMS,&count);b.mActiveTextureChannels=0;
 for(int i=0;i<count;++i){char name[256];GLsizei length;GLint size;GLenum type;
  glGetActiveUniform(beauty,i,256,&length,&size,&type,name);
  if(type==GL_SAMPLER_2D){GLint unit;glGetUniformiv(beauty,glGetUniformLocation(beauty,name),&unit);b.mActiveTextureChannels=llmax(b.mActiveTextureChannels,unit+1);}
 }
 auto& cached=sActorKeyLensPrograms[&b];if(!cached)cached=std::make_unique<ActorKeyLensProgram>();
 cached->beautyHash=b.mShaderHash;cached->shader.mProgramObject=coverage;cached->shader.mUniformsDirty=false;
 pipe->renderActorKeyLens(stage,b,width,height);
 return pipe->lensCoverageValid()?1:0;
}
__declspec(dllexport) unsigned lens_output(){return pipe->mKeyLensCurrent?pipe->mKeyLensCurrent->getFBO():0;}
__declspec(dllexport) unsigned lens_gather(unsigned i){return pipe->mKeyDiopterCov.getTexture(i);}
__declspec(dllexport) unsigned lens_output_texture(){return pipe->mKeyLensCurrent?pipe->mKeyLensCurrent->getTexture():0;}
__declspec(dllexport) int lens_valid(){return pipe->lensCoverageValid();}
__declspec(dllexport) void lens_off(){ALActorChromaKey::sActive=false;pipe->releaseLens();}
__declspec(dllexport) void lens_off_probe(){pipe->renderActorKeyLens(3,beautyShaders[3],128,128);}
__declspec(dllexport) void lens_stale(){pipe->mKeyLensFrame=41;}
__declspec(dllexport) void lens_missing(){pipe->mKeyLensComplete=0;}
__declspec(dllexport) void lens_finish(){
 if(pipe){pipe->releaseLens();delete pipe;pipe=nullptr;}
 for(auto& b:beautyShaders)b.mProgramObject=0;
}
__declspec(dllexport) void shutdown_lens(){gGL.shutdown();LLCommon::cleanupClass();}
}
"""

def build_targets():
    return compile_run(out.resolve(),src,'lens_targets',flags,libs,False)


def load_targets():
    binary=build_targets()
    handles=[]
    for path in [build/'sharedlibs/Release',build/'newview/Release',*build.glob('vcpkg_installed/*/bin')]:
        if path.exists():handles.append(os.add_dll_directory(str(path)))
    dll=C.CDLL(str(binary));dll._directories=handles
    dll.make_target.argtypes=[C.c_uint,C.c_uint,C.c_uint];dll.make_target.restype=C.c_void_p
    dll.texture.argtypes=[C.c_void_p,C.c_uint];dll.texture.restype=C.c_uint
    dll.fbo.argtypes=[C.c_void_p];dll.fbo.restype=C.c_uint
    for name in ('lens_bind','lens_flush','drop'):getattr(dll,name).argtypes=[C.c_void_p]
    dll.begin_lens.argtypes=[C.c_uint,C.c_uint,C.POINTER(C.c_float)]
    dll.run_lens.argtypes=[C.c_uint]*5
    assert dll.init()==1
    return dll
