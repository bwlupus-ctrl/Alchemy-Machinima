"""Verify disabled production GLSL token streams against HEAD; no viewer build."""
import difflib
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
compiler=next(Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))
files=['class1/deferred/deferredUtil.glsl','class3/deferred/multiPointLightF.glsl','class3/deferred/pointLightF.glsl','class3/deferred/spotLightF.glsl','class2/deferred/alphaF.glsl','class3/deferred/materialF.glsl','class2/deferred/pbralphaF.glsl','class2/deferred/sharedActorFxPbrF.glsl']
with tempfile.TemporaryDirectory(dir=root/'.tmp',prefix='relight_off_') as temporary:
    for file in files:
        path=Path('indra/newview/app_settings/shaders')/file
        old=subprocess.check_output(['git','show','HEAD:'+path.as_posix()],cwd=root,text=True)
        new=(root/path).read_text()
        for flags in [[],['HAS_ACTOR_FX','ALR_RIMSPLIT_PARITY','MULTI_SPOTLIGHT','RIG_RIM_AUX']]:
            streams=[]
            for text in [old,new]:
                source=Path(temporary)/'input.glsl'
                source.write_text('#define LIGHT_COUNT 16\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define USE_DIFFUSE_TEX 1\n'+''.join('#define '+f+' 1\n' for f in flags)+text)
                result=subprocess.run([str(compiler),'/nologo','/EP','/TC',str(source)],capture_output=True,text=True)
                assert result.returncode==0,result.stderr
                streams.append(re.findall(r'\w+|[^\s]',result.stdout))
            assert streams[0]==streams[1],file+' disabled token drift: '+str(next((i for i,(a,b) in enumerate(zip(*streams)) if a!=b), 'length'))
        print('[RelightQ] OFF token identity PASS',file,flush=True)
