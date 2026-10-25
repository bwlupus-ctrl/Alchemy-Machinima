"""Execute the complete production sensor shader loader with recipe seams."""
import ctypes as C
import os
from pathlib import Path
import re
import subprocess


def run(root,out):
    source=(root/'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    start=source.index('bool LLViewerShaderMgr::loadSensorShaders()')
    loader=source[start:source.index('\n}',start)+2]
    declarations=re.findall(r'^LLGLSLShader (gSensor\w+)(\[\d+\])?;',source,re.M)
    legacy=sorted(set(re.findall(r'\bg(?:CG|Deferred|Ultimate|FXAA|SMAA|Blit|CineOutline|Volumetric)\w+',loader)))
    prefix=r'''
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <iostream>
using U32=unsigned;using std::make_pair;
#define GL_VERTEX_SHADER 1
#define GL_FRAGMENT_SHADER 2
#define LL_WARNS(x) std::cerr
#define LL_ENDL std::endl
enum{SHADER_DEFERRED};
struct LLShaderFeatures{bool isDeferred=false;};
struct LLGLSLShader{
 std::string mName;LLShaderFeatures mFeatures;
 std::vector<std::pair<std::string,int>> mShaderFiles;
 int mShaderLevel=1;bool mUniformsDirty=false,ready=false;
 std::map<std::string,std::string> mDefines;
 bool isComplete(){return ready;}
 void clearPermutations(){mDefines.clear();}
 void addPermutations(const std::map<std::string,std::string>& v){mDefines=v;}
 void addPermutation(std::string k,std::string v){mDefines[k]=v;}
 bool createShader(){ready=true;return true;}
};
'''
    globals_='\n'.join('LLGLSLShader '+name+(array or '')+';' for name,array in declarations)
    for name in legacy:
        globals_+='\nLLGLSLShader '+name+('[4]' if name.endswith('Program') and name in (
            'gUltimateDiopterGatherProgram','gFXAAProgram','gSMAAEdgeDetectProgram','gSMAANeighborhoodBlendProgram') else '')+';'
    suffix=r'''
struct LLViewerShaderMgr {
 std::vector<LLGLSLShader*> mShaderList;int mShaderLevel[1]={1};
 void unloadSensorShaders(){}bool loadSensorShaders();
};
'''+loader+r'''
extern "C" __declspec(dllexport) int recipes(){
 gVolumetricLightProgram.ready=true;
 for(auto& s:gFXAAProgram)s.ready=true;
 for(auto& s:gSMAANeighborhoodBlendProgram)s.ready=true;
 for(auto& s:gSMAAEdgeDetectProgram){s.ready=true;s.mDefines["SMAA_PREDICATION"]="1";}
 LLViewerShaderMgr mgr;if(!mgr.loadSensorShaders())return 0;
 for(auto& s:gSensorCameraProgram)if(!s.ready||s.mDefines["AL_SENSOR_HDR"]!="1")return 1;
 for(auto& s:gSensorSMAAEdgeProgram)if(!s.ready||s.mDefines["SMAA_PREDICATION"]!="0")return 2;
 for(auto& s:gSensorSMAABlendProgram)if(s.mDefines["AL_SENSOR_HDR"]!="1")return 3;
 for(unsigned i=0;i<2;++i)if(gSensorDoFCombineProgram[i].mDefines["FRONT_BLUR"]!=(i?"1":"0"))return 4;
 if(gSensorCameraProgram[6].mDefines["AL_SENSOR_GODRAYS"]!="1")return 5;
 if(gSensorGodrayProgram.mDefines["AL_SENSOR_HDR"]!="1")return 6;
 return 100;
}
'''
    out.mkdir(parents=True,exist_ok=True)
    vc=sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    results=[]
    for name,body in (('recipes',suffix),('predication_mutant',suffix.replace(
        'gSensorSMAAEdgeProgram[i].addPermutation("SMAA_PREDICATION", "0");',
        'gSensorSMAAEdgeProgram[i].addPermutation("SMAA_PREDICATION", "1");'))):
        path=out/(name+'.cpp');dll=out/(name+'.dll');cmd=out/(name+'.cmd')
        path.write_text(prefix+globals_+body,encoding='utf-8')
        cmd.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /O2 /LD "'+str(path)+'" /Fe:"'+str(dll)+'" /Fo"'+out.as_posix()+'/" /link /IMPLIB:"'+str(out/(name+'.lib'))+'"\n',encoding='utf-8')
        result=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=root,capture_output=True,text=True,timeout=90)
        (out/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        results.append(C.CDLL(str(dll)).recipes())
    assert results==[100,2],results
    return dict(full_loader=True,default_predication=1,sensor_predication=0,mutation_rejected=True)


def lpm_controls(root,out):
    """Compile the untouched production CPU control-block setup in isolation."""
    import numpy as np
    header=(root/'indra/newview/allpm.h').read_text(encoding='utf-8').replace('#include "stdtypes.h"','using U32=unsigned;using F32=float;')
    source=(root/'indra/newview/allpm.cpp').read_text(encoding='utf-8').replace('#include "llviewerprecompiledheaders.h"','').replace('#include "allpm.h"','')
    out.mkdir(parents=True,exist_ok=True);path=out/'lpm.cpp';dll=out/'lpm.dll';cmd=out/'lpm.cmd'
    path.write_text(header+source+'\nextern "C" __declspec(dllexport) void controls(unsigned* out){ALLPM::setup709(false,256.f,7.4f,.05f,1.f,0.f,0.f,0.f,1.f,.5f,1.f/32.f,out);}\n',encoding='utf-8')
    vc=sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    cmd.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /O2 /LD "'+str(path)+'" /Fe:"'+str(dll)+'" /Fo"'+out.as_posix()+'/" /link /IMPLIB:"'+str(out/'lpm.lib')+'"\n',encoding='utf-8')
    result=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=root,capture_output=True,text=True,timeout=90)
    assert result.returncode==0,result.stdout+result.stderr
    lib=C.CDLL(str(dll));lib.controls.argtypes=[C.POINTER(C.c_uint)]
    values=np.zeros(96,np.uint32);lib.controls(values.ctypes.data_as(C.POINTER(C.c_uint)))
    return values
