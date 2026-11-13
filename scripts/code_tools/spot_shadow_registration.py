"""Compile the actual production layout registrations and setting callback."""
from pathlib import Path
import re
import subprocess
from native_check_tools import compile_run, check_directory

ROOT=Path(__file__).resolve().parents[2]


def body(source, marker):
    a=source.index(marker); start=source.index('{',a); depth=1; end=start+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[a:end]


def production_selection():
    # Inactive extra reservations must not alter legacy sampler-unit assignment.
    relative='indra/llrender/llshadermgr.cpp'
    current=(ROOT/relative).read_text(encoding='utf-8')
    base=subprocess.check_output(['git','show','fb616f2519c:'+relative],cwd=ROOT,encoding='utf-8')
    reserved=lambda text: re.findall(r'mReservedUniforms.push_back\("([^"\n]+)"\)',text)
    added={'shadowMap'+str(i) for i in range(14,20)}
    assert [name for name in reserved(current) if name not in added] == reserved(base)
    relative='indra/llrender/llglslshader.cpp'
    assert (ROOT/relative).read_bytes() == subprocess.check_output(['git','show','fb616f2519c:'+relative],cwd=ROOT)
    header=(ROOT/'indra/newview/pipeline.h').read_text(encoding='utf-8')
    manager=(ROOT/'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    marker='    if (LLPipeline::use16SpotShadowShaders('
    assert manager.count(marker)==2, 'both production registration sites must use the shared selector'
    define=body(manager,marker)
    a=manager.index(marker,manager.index(marker)+1)
    # Include the complete if/else, not a fixture equivalent of the selection.
    classic=body(manager[a:],marker)+ '\n'+body(manager[a+len(body(manager[a:],marker)):], '        else')
    control=(ROOT/'indra/newview/llviewercontrol.cpp').read_text(encoding='utf-8')
    handler=body(control,'static bool handleMaxSpotShadowsChanged(')
    assert control.count('"BDMergeMaxSpotShadows", handleMaxSpotShadowsChanged)')==1
    source=r'''
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <iostream>
using U32=unsigned; using std::make_pair;
struct LLPipeline {
HEADER
};
struct { U32 mNumTextureImageUnits=32; } gGLManager;
struct {U32 count=2; U32 getU32(const char*){return count;}} gSavedSettings;
struct LLGLSLShader {inline static std::map<std::string,std::string> sGlobalDefines;};
struct {std::vector<std::pair<std::string,int>> mShaderFiles;} gDeferredProjectorVolumetricClassicProgram;
constexpr int GL_FRAGMENT_SHADER=1;
void registration(){
 std::map<std::string,std::string> attribs;
DEFINE
 LLGLSLShader::sGlobalDefines=attribs;
 gDeferredProjectorVolumetricClassicProgram.mShaderFiles.clear();
CLASSIC
}
struct LLSD {U32 count; int asInteger()const{return count;}};
int resizes=0,reloads=0;
static bool handleShadowsResized(const LLSD&){++resizes;return true;}
static bool handleSetShaderChanged(const LLSD&){++reloads;registration();return true;}
HANDLER
int main(){
 for(U32 units: {16u,28u,32u}) for(U32 count: {2u,6u,8u,10u,11u,16u}){
  gGLManager.mNumTextureImageUnits=units;gSavedSettings.count=count;registration();
  bool large=units==32 && count>10;
  assert((LLGLSLShader::sGlobalDefines.count("SPOT_SHADOWS_16")!=0)==large);
  const auto& files=gDeferredProjectorVolumetricClassicProgram.mShaderFiles;
  assert(files.size()==1);
  assert(files[0].first==std::string("deferred/projectorVolumetricClassicShadowUtil")+(large?"16.glsl":".glsl"));
  std::cout<<count<<" "<<units<<" "<<large<<" "<<files[0].first<<"\n";
 }
 for(U32 units: {16u,28u,32u}){
  gGLManager.mNumTextureImageUnits=units;gSavedSettings.count=2;registration();resizes=reloads=0;
  for(U32 count: {2u,10u,11u,16u,11u,10u,2u}){
   gSavedSettings.count=count;assert(handleMaxSpotShadowsChanged({count}));
   // Repeated notifications cannot trigger a per-frame/repeated layout reload.
   for(int frame=0;frame<100;++frame) assert(handleMaxSpotShadowsChanged({count}));
  }
  assert(reloads==(units==32?2:0));assert(resizes==707);
 }
}
'''
    for key,value in [('HEADER',header[header.index('    static constexpr U32    MAX_SPOT_SHADOWS ='):header.index('    LLRenderTarget          mSpotShadow')]),('DEFINE',define),('CLASSIC',classic),('HANDLER',handler)]:
        source=source.replace(key,value)
    result=compile_run(check_directory('spot_shadows_16_registration'),source,'registration')
    assert result.returncode==0,result.stdout+result.stderr
    rows={}
    for line in result.stdout.splitlines():
        n,units,large,path=line.split();rows[(int(n),int(units))]=(large=='1',path)
    assert len(rows)==18, result.stdout
    print('PASS actual global/Classic registration N=2/6/8/10/11/16, units=16/28/32; crossing reloads exactly once in each direction',flush=True)
    return rows
