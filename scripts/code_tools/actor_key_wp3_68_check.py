"""I-68GREP: exact upstream v6.8.0 C++ sources; never fetch shader files."""
from pathlib import Path
import hashlib,json,re,urllib.request
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.tmp/ckrs/reshade_v680'
FILES=('source/runtime.cpp','source/runtime.hpp','source/runtime_api.cpp','source/runtime_gui.cpp','source/addon_manager.cpp','source/opengl/opengl_hooks_wgl.cpp')

def run():
    OUT.mkdir(parents=True,exist_ok=True)
    evidence={}
    sources={}
    for name in FILES:
        url='https://raw.githubusercontent.com/crosire/reshade/v6.8.0/'+name
        path=OUT/name.replace('/','_')
        data=urllib.request.urlopen(url,timeout=30).read()
        path.write_bytes(data);sources[name]=data.decode('utf-8-sig')
        evidence[name]={'url':url,'sha256':hashlib.sha256(data).hexdigest()}
    writes={}
    for name,source in sources.items():
        writes[name]=[(i+1,line.strip()) for i,line in enumerate(source.splitlines()) if re.search(r'\b_effects_enabled\s*(?:=(?!=)|\^=|\|=|&=)',line)]
    # Full write census: shortcut, public setter, declaration initializer only.
    assert [line for _,line in writes['source/runtime.cpp']]==['_effects_enabled = !_effects_enabled;'],writes
    assert [line for _,line in writes['source/runtime_api.cpp']]==['_effects_enabled = enabled;'],writes
    assert len(writes['source/runtime.hpp'])==1 and '= true' in writes['source/runtime.hpp'][0][1],writes
    assert not writes['source/runtime_gui.cpp'] and not writes['source/addon_manager.cpp'] and not writes['source/opengl/opengl_hooks_wgl.cpp'],writes
    api=sources['source/runtime_api.cpp']
    technique=api[api.index('void reshade::runtime::render_technique('):api.index('bool reshade::runtime::get_effects_state')]
    assert 'reshade_begin_effects' in technique and 'reshade_finish_effects' in technique and '_effects_enabled' not in technique
    evidence['writes']=writes
    (OUT/'evidence.json').write_text(json.dumps(evidence,indent=2))
    print('I-68GREP PASS v6.8.0: only shortcut / public setter / initializer write Effects; no reload-path write; public render_technique raises begin/finish regardless of Effects')
    for name,items in writes.items():
        for line,text in items:print(name+':'+str(line)+' '+text)
if __name__=='__main__':run()
