"""Verify disabled GLSL preserves HEAD lighting expressions; no viewer build."""
import difflib
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
settings=('RenderLightShapeEnable','RenderLocalLightPhysicalFalloff',
          'RenderLocalLightDefaultRadius','RenderLocalLightSizeFromPrim','RenderLocalLightNearFloor',
          'RenderLocalLightPhysicalMaxBoost')
control=(root/'indra/newview/llviewercontrol.cpp').read_text()
manager=(root/'indra/newview/llviewershadermgr.cpp').read_text()
for setting in settings:
    assert not re.search(r'setting_setup_signal_listener\([^;]*"'+setting+r'"',control),setting+' reload listener'
    assert setting not in manager,setting+' shader-manager dependency'
assert '"RELIGHT_SHAPE"' not in manager, 'global shape permutation'
print('[RelightQ] six live settings PASS no reload listeners or shader-manager dependencies',flush=True)
compiler=next(Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))

def disabled_source(text):
    # Bit 0 preserves Phase 1. Bit 1 is area visibility on the existing upload.
    text=text.replace('(relight_enabled & 1)', 'relight_enabled')
    text=re.sub(r'float areaShadowVisibility\([^{}]+\)\s*\{.*?\n\}', '', text, flags=re.S)
    text=re.sub(r'uniform sampler2D area_shadow_map[01];', '', text)
    text=re.sub(r'if \(\(relight_enabled & 2\) != 0 && [^\n]+\)\s*[^;]+;', '', text)
    for index in ('light_idx','i'):
        alias='vec4 area_col = light_col['+index+'];'
        if alias not in text: continue
        start=text.index(alias)
        end=text.find('for (int ',start)
        if end < 0: end=len(text)
        body=text[start:end].replace(alias,'').replace('area_col','light_col['+index+']')
        text=text[:start]+body+text[end:]

    # Discard only runtime-enabled blocks, and unreachable shaped helpers.
    # Preserve the complete original point branches, including nested else,
    # Actor FX, rim, clamps and final output expressions.
    patterns=[r'if\s*\(relight_enabled != 0 && [^\n]+\)\s*\{',
              r'^(?:vec3|float|void) relight\w+\([^;{}]*\)\s*\{']
    for pattern in patterns:
        while True:
            match=re.search(pattern,text,re.M)
            if not match: break
            a=match.end(); end=a; depth=1
            while depth:
                depth+=(text[end]=='{')-(text[end]=='}');end+=1
            # Some callers use else for the point path; remove only the else
            # keyword, retaining its scope and every original statement.
            following=re.match(r'\s*else\b',text[end:])
            if following: end+=following.end()
            text=text[:match.start()]+text[end:]
    text=re.sub(r'if\s*\(relight_enabled == 0 \|\| light_shape.x == 0.0\)\s*','',text)
    # An added forward declaration has no executable expression.
    text=re.sub(r'float calcLegacyDistanceAttenuation\([^;{}]*\);','',text)
    return text
files=['class1/deferred/deferredUtil.glsl','class3/deferred/multiPointLightF.glsl','class3/deferred/pointLightF.glsl','class3/deferred/spotLightF.glsl','class2/deferred/alphaF.glsl','class3/deferred/materialF.glsl','class2/deferred/pbralphaF.glsl','class2/deferred/sharedActorFxPbrF.glsl']
with tempfile.TemporaryDirectory(dir=root/'.tmp',prefix='relight_off_') as temporary:
    for file in files:
        path=Path('indra/newview/app_settings/shaders')/file
        old=subprocess.check_output(['git','show','HEAD:'+path.as_posix()],cwd=root,text=True)
        new=(root/path).read_text()
        old=disabled_source(old)
        new=disabled_source(new)

        for flags in [[],['HAS_ACTOR_FX','ALR_RIMSPLIT_PARITY','MULTI_SPOTLIGHT','RIG_RIM_AUX']]:
            streams=[]
            for text in [old,new]:
                source=Path(temporary)/'input.glsl'
                # Blend mode includes materialF's forward per-light evaluator;
                # leaving DIFFUSE_ALPHA_MODE undefined would omit that body.
                source.write_text('#define RELIGHT_SHAPE 1\n#define LIGHT_COUNT 16\n#define DIFFUSE_ALPHA_MODE 1\n#define SHARED_ACTOR_FX_ALPHA_MODE 2\n#define USE_DIFFUSE_TEX 1\n'+''.join('#define '+f+' 1\n' for f in flags)+text)
                result=subprocess.run([str(compiler),'/nologo','/EP','/TC',str(source)],capture_output=True,text=True)
                assert result.returncode==0,result.stderr
                streams.append(re.findall(r'\w+|[^\s]',result.stdout))
            assert streams[0]==streams[1],file+' runtime expression drift: '+str(next((i for i,(a,b) in enumerate(zip(*streams)) if a!=b), 'length'))
        print('[RelightQ] HEAD disabled expression identity PASS',file,flush=True)

area_settings=('RenderAreaShadowThinObjects','RenderAreaShadows','RenderAreaShadowsVCam','RenderAreaShadowLights','RenderAreaShadowSamples',
    'RenderAreaShadowSteps','RenderAreaShadowMaxDist','RenderAreaShadowThickness',
    'RenderAreaShadowHalfRes','RenderAreaShadowDenoise','RenderAreaShadowQuality',
    'RenderAreaShadowLightBias','RenderAreaShadowDebugView','RenderAreaShadowMappedContact',
    'RenderAreaShadowCubeMaps','RenderAreaShadowCubeLights',  # P1-lite world cube maps: runtime uniforms only, no reload
    'RenderAreaShadowStrength','RenderAreaShadowContrast','RenderAreaShadowWorldScreenReach','RenderAreaShadowWorldAmount','RenderAreaShadowScreenAmount')  # soft shadow LOOK: runtime uniforms only, no reload
for setting in area_settings:
    assert setting not in manager, setting+' shader-manager dependency'
    listener=re.search(r'setting_setup_signal_listener\([^;]*"'+setting+r'"[^;]*;',control,re.S)
    if listener:
        assert setting=='RenderAreaShadowQuality' and 'handleSetShaderChanged' not in listener.group(),setting
for name in ('areaShadowF.glsl','areaShadowFilterF.glsl'):
    text=(root/'indra/newview/app_settings/shaders/class3/deferred'/name).read_text()
    for forbidden in ('noiseSine','modelview_delta','gFrameCount','uniform float time','random('):
        assert forbidden not in text, (name, forbidden)
text=(root/'indra/newview/app_settings/shaders/class1/deferred/screenSpaceRayUtil.glsl').read_text()
for forbidden in ('noiseSine','modelview_delta','gFrameCount','uniform float time','random('):
    assert forbidden not in text, ('screenSpaceRayUtil.glsl',forbidden)
print('[RelightQ] area settings/current-frame source PASS no reload dependencies, temporal terms or implicit visibility samples')
