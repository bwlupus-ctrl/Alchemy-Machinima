#!/usr/bin/env python3
"""Fail-closed environment-only source contracts and actual C++ predicate.

--source-ref HEAD is the required fail-before control. No viewer build or GL.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
# Baseline = the commit this feature was built on (pre Environment EV); not HEAD, which
# includes the feature once committed/merged. Override with env var.
import os as _os_base
BASE_REF = _os_base.environ.get('ENV_ONLY_CHECK_BASE', 'b8a911596c5')

ROOT=Path(__file__).resolve().parents[2]
KEY='AlchemyEnvEnvironmentOnly'


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source-ref');ap.add_argument('--cpu',action='store_true');args=ap.parse_args()
    failures=[]
    def read(path,ref=args.source_ref):
        if ref:
            return subprocess.check_output(['git','show',f'{ref}:{path}'],cwd=ROOT,timeout=20).decode('utf-8')
        return (ROOT/path).read_text(encoding='utf-8')
    def check(name,ok):
        print(('PASS ' if ok else 'FAIL ')+name,flush=True)
        if not ok:failures.append(name)
    control=read('indra/newview/llviewercontrol.cpp');mgr=read('indra/newview/llviewershadermgr.cpp');env=read('indra/newview/alenvintensity.cpp')
    def function(text,signature):
        start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return text[start:end]
    # No registration for either preference may ever reach a shader rebuild.
    reloads=re.findall(r'setting_setup_signal_listener\([^;]*;',control,re.S)
    check('no-reload: env-only and tag have no shader-change listeners',all(
        not any(key in registration for key in (KEY,'AlchemyEnvEnvironmentEV','RenderGBufferAvatarTag')) for registration in reloads))
    check('no-reload: recipient mode is runtime, absent from shader-manager compile decisions',KEY not in mgr and
        not any(word in function(env,'bool envOnlyActive()') for word in ('setShaders','handleSetShaderChanged','addPermutation')) if 'bool envOnlyActive()' in env else False)
    check('startup tag follows HDR format only', 'tag_setting()' not in function(mgr,'bool LLViewerShaderMgr::gbufferAvatarTagWanted()') and
        'return hdr;' in function(mgr,'bool LLViewerShaderMgr::gbufferAvatarTagWanted()'))
    compute=function(env,'Factors compute(')
    # The master is the sole intentional math change: retain the previous
    # byte-for-byte contract for every other operation, order and branch.
    original_compute=compute.replace('        const F32 environment_ev = environmentEV();\n','').replace(' + environment_ev','')
    check('existing compute unchanged except additive Environment EV',original_compute==function(read('indra/newview/alenvintensity.cpp',BASE_REF),'Factors compute('))
    settings=ET.fromstring(read('indra/newview/app_settings/settings_alchemy.xml'));nodes=list(settings.find('map'));entries={nodes[i].text:nodes[i+1] for i in range(0,len(nodes),2)}
    fields=list(entries.get(KEY,[]));props={fields[i].text:fields[i+1].text for i in range(0,len(fields),2)}
    check('persistent boolean, default false',props.get('Type')=='Boolean' and props.get('Persist')=='1' and props.get('Value')=='0')
    ev_key='AlchemyEnvEnvironmentEV'
    fields=list(entries.get(ev_key,[]));props={fields[i].text:fields[i+1].text for i in range(0,len(fields),2)}
    check('Environment EV persistent F32, default zero',props.get('Type')=='F32' and props.get('Persist')=='1' and props.get('Value')=='0')
    check('Environment EV stays runtime-only',ev_key not in mgr and 'F32 environmentEV()' in env and
          not any(word in function(env,'F32 environmentEV()') for word in ('setShaders','handleSetShaderChanged','addPermutation')))
    probe=function(read('indra/newview/llreflectionmapmanager.cpp'),'ALProbeSched::EnvSample LLReflectionMapManager::gatherEnvSample()')
    check('probe source signatures follow effective master and mode',
          'const F32 environment_ev = ALEnvIntensity::environmentEV();' in probe and all(
              assignment in probe for assignment in ('st.mSunEV = (F32)sun_ev + environment_ev;',
              'st.mMoonEV = (F32)moon_ev + environment_ev;', 'st.mLocalLightEV = (F32)local_light_ev + environment_ev;')))
    preset=read('indra/newview/alenvintensitypresets.cpp');page=read('indra/newview/alpanelexposurepage.cpp')
    check('preset/Natural ownership excludes preference',KEY not in preset and KEY not in function(page,'const std::vector<std::string>& intensitySettings()'))
    check('preset/Natural ownership excludes Environment EV',ev_key not in preset and ev_key not in function(page,'const std::vector<std::string>& intensitySettings()'))
    kit=json.loads(read('scripts/code_tools/director_kit/kit_pages.json'))
    config=kit.get('pages',kit).get('light_group/exposure_tab',{})
    if not config:
        def walk(o):
            if isinstance(o,dict):
                if 'light_group/exposure_tab' in o:return o['light_group/exposure_tab']
                for v in o.values():
                    found=walk(v)
                    if found:return found
        config=walk(kit) or {}
    check('Director shared history and preference bulk-reset exclusion',KEY in config.get('own_history',[]) and
        f'setting="{KEY}"' in read('indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml') and
        'bulk_reset="false"' in read('indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml'))
    xui=ET.fromstring(read('indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml'))
    ev_rows=[n for n in xui.iter('kit_row') if n.get('setting')==ev_key]
    check('Environment EV shared history and preference bulk-reset exclusion',ev_key in config.get('own_history',[]) and
          len(ev_rows)==1 and ev_rows[0].get('bulk_reset')=='false')
    for file,name in (('floater_adjust_environment.xml','env_environment_ev'),('panel_exposure_src.xml','exp_environment_ev')):
        tree=ET.fromstring(read('indra/newview/skins/default/xui/en/'+file))
        node=next((n for n in tree.iter('slider') if n.get('name')==name),None)
        check(file+' Environment EV range/step/value',node is not None and all(node.get(k)==v for k,v in
              {'control_name':ev_key,'min_val':'-4','max_val':'4','increment':'0.05','initial_value':'0','can_edit_text':'true'}.items()))
    check('Environment EV both UI hosts gate on mode and HDR via signals',all(
        widget in function(read('indra/newview/'+file),signature) and '"AlchemyEnvEnvironmentOnly"' in function(read('indra/newview/'+file),signature)
        for file,signature,widget in (('llfloaterenvironmentadjust.cpp','void LLFloaterEnvironmentAdjust::updateEnvOnlyAvailability()','env_environment_ev'),
                                      ('alpanelexposurepage.cpp','void ALPanelExposurePage::updateEnvOnlyAvailability()','exp_environment_ev'))))
    check('Director gates native value editor as well as row actions',
          all(code in function(page,'void ALPanelExposurePage::updateEnvOnlyAvailability()') for code in
              ('setEnabledIfChanged(control->getParent(), enabled);','setEnabledIfChanged(control, enabled);')))
    history=read('indra/newview/alsettingsundohistory.cpp')
    check('exposure undo mirrors IncludeRig',KEY in history and 'AlchemyEnvLocalLightIncludeRig' in history)
    check('Environment EV exposure undo',ev_key in history)
    scene=function(read('indra/newview/llfloaterdirector.cpp'),'const std::vector<std::string>& LLFloaterDirector::sceneSettingsList(')
    check('scene ownership mirrors IncludeRig exclusion',KEY not in scene and 'AlchemyEnvLocalLightIncludeRig' not in scene and bool(scene))
    check('Environment EV scene ownership mirrors env-only exclusion',ev_key not in scene and bool(scene))
    pipe=read('indra/newview/pipeline.cpp')
    check('deferred fast and slow binds refresh view mode',pipe.count('uploadEnvOnlyView(shader);')==2)
    check('forward identity independent of Actor Mask and Prism snapshot key', 'entry.first | 0x100' in pipe and 'std::map<U16, ForwardLightSet>' in read('indra/newview/pipeline.h'))
    shaderdir='indra/newview/app_settings/shaders/'
    paths=subprocess.check_output(['git','ls-files',shaderdir+'*.glsl'],cwd=ROOT,timeout=20).decode().splitlines()
    rawguards=[]
    for path in paths:
        s=read(path)
        if re.search(r'^#if.*HAS_WEATHER_MATERIAL.*!defined\(GBUFFER_AVATAR_TAG\)',s,re.M):rawguards.append(path)
    check('no weather compile guard depends on avatar tag availability',not rawguards)
    if args.cpu and not args.source_ref:
        out=ROOT/'.tmp/env_only/cpu';out.mkdir(parents=True,exist_ok=True)
        adapter=r'''
#include <string>
struct Controls { bool enabled=false,hdr=true; } gSavedSettings;
struct GLManager { float mGLVersion=4.6f; } gGLManager;
template<class T> struct LLCachedControl { std::string key; LLCachedControl(Controls&,const char* k,T):key(k){} T operator()() const { return key=="AlchemyEnvEnvironmentOnly" ? gSavedSettings.enabled : gSavedSettings.hdr; } };
bool neutral=false,capture=false;
namespace ALEnvIntensity { bool isNeutral(){return neutral;} bool isSamplingCapture(){return capture;}
'''+function(env,'bool envOnlyActive()')+'\n}\n'
        (out/'env_only_adapter.h').write_text(adapter,encoding='utf-8')
        dev=Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat')
        cmd=out/'run.cmd';cmd.write_text(fr'''@echo off
call "{dev}" -arch=x64 -host_arch=x64 >nul
cl /nologo /std:c++17 /EHsc /O2 /DAL_ENV_ONLY_STANDALONE /I.tmp\env_only\cpu indra\newview\tests\alenvintensity_envonly_test.cpp /Fe.tmp\env_only\cpu\test.exe /Fo.tmp\env_only\cpu\test.obj
if errorlevel 1 exit /b %errorlevel%
.tmp\env_only\cpu\test.exe
''',encoding='utf-8')
        result=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=ROOT,capture_output=True,text=True,timeout=90)
        print(result.stdout,result.stderr);check('actual production envOnlyActive C++ truth table',result.returncode==0)
        from env_only_signal_test import run as run_signals
        check('real LLControlGroup signals never rebuild shaders; negative control fails',
              run_signals(ROOT.parent/'alchemy-machinima/build-Windows-vs2026-os'))
    print(f'{"FAIL" if failures else "PASS"}: {len(failures)} environment-only contract failures')
    return bool(failures)


if __name__=='__main__':raise SystemExit(main())
