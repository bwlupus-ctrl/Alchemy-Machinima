"""Execute production clone FX commits and audit every model/uniform copy site."""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET

from native_check_tools import check_directory, compile_run
from relight_area_policy_check import method

ROOT=Path(__file__).resolve().parents[2]
NV=ROOT/'indra/newview'
FIELDS=('Slice','Dropout','PowerFlicker','Wipe','Interlace','PositionJitter')
WIDGETS=('slice','dropout','power_flicker','wipe','interlace','position_jitter')


def check():
    from clone_fx_fix1_check import source_assertions
    from relight_3d_gl import SourceTree
    assert source_assertions(SourceTree('WORKTREE')), 'FX fix round 1 source assertions'
    from clone_fx_safety_check import pre_fx_shader
    from clone_looks_gl_check import F,V,SH
    for shader in (F,V,SH+'avatar/avatarActorGhostV.glsl',SH+'avatar/eyeballActorGhostV.glsl'):
        source=(ROOT/shader).read_text(encoding='utf-8')
        pre_fx_shader(source,shader)
        try:pre_fx_shader(source+'\n// unrelated edit\n',shader)
        except AssertionError:pass
        else:raise AssertionError(('unrelated shader edit escaped exact layer pin',shader))
    mover=(NV/'llactormover.cpp').read_text(encoding='utf-8')
    panel=(NV/'alpanelghoststudio.cpp').read_text(encoding='utf-8')
    cast=(NV/'lldirectorcast.cpp').read_text(encoding='utf-8')
    for field,widget in zip(FIELDS,WIDGETS):
        for file,count in [('alghoststudio.h',1),('lldirectorcast.h',1),('llactormover.h',2)]:
            text=(NV/file).read_text(encoding='utf-8')
            assert len(re.findall(r'F32\s+m'+field+r'\s*=\s*0.f;',text))==count,(field,file,'default/copy structures')
        for target,source in [('gp','inst'),('gp','style'),('proxy','style'),('params','proxy')]:
            assert f'{target}.m{field} = llclamp({source}.m{field}, 0.f, 1.f);' in mover,(field,target,source)
        assert f'setValueIfChanged(m{field}Slider, LLSD((F64)inst->m{field}), force);' in panel
        assert f'{{ "reset_{widget}", "{widget}_slider" }}' in panel
        assert f'setEnabledIfChanged(m{field}Slider, overlay);' in panel
        assert f'F32)m{field}Slider->getValue().asReal(), 0.f, 1.f)' in panel
        assert f'data["{widget}"]' not in cast, 'clone session-only effect leaked into scene LLSD'
        for file,kind in [('panel_ghost_studio.xml','slider'),('panel_dpage_cast_ghosts.xml','kit_row')]:
            tree=ET.parse(NV/'skins/default/xui/en'/file)
            control=tree.find(".//*[@"+('name' if kind=='slider' else 'widget')+"='"+widget+"_slider']")
            assert control is not None,(file,widget)
            assert control.get('min_val')=='0'
            assert control.get('max_val')=='1'
    for upload in ('sh->uniform4f','shader.uniform4f'):
        assert upload+'(sGhostFx2, gp.mSlice, gp.mDropout, gp.mPowerFlicker, gp.mWipe);' in mover
        assert upload+'(sGhostFx3, gp.mInterlace, gp.mPositionJitter,' in mover
    assert 'gp.mFxBodyRotation = inst.mRotation;' in mover
    assert mover.count('actorFxFrameTime(gp.mEffectFps, 6144.0)')==2
    assert mover.count('actor_ghost_studio_frame_time(gp.mEffectFps, 6144.0)')==1
    assert mover.count('fx_height, fx_now);')==2
    assert mover.count('const F32 fx_height = ghost_fx_viewport_height(gp.mInterlace);')==2
    assert 'if (!(interlace > 0.f)) return 0.f;' in mover
    assert 'glGetIntegerv(GL_VIEWPORT, viewport);' in mover
    assert 'gp.mFxBodyRotationValid ? &gp.mFxBodyRotation : nullptr' in mover
    assert mover.index('const glm::vec3 fx_lateral = fx_displaces') < mover.index('// placement * this face\'s model matrix')
    bodies='\n'.join(method(NV/'alpanelghoststudio.cpp',f'void ALPanelGhostStudio::on{f}Commit(') for f in FIELDS)
    fields=''.join(f'float m{f}=0;' for f in FIELDS)
    slots=''.join(f'Slider storage{f}; Slider* m{f}Slider=&storage{f}; void on{f}Commit();' for f in FIELDS)
    tests=''
    for field in FIELDS:
        tests+=f'''
        for(float v : {{0.f,.5f,1.f,-.2f,1.2f}}){{
            p.m{field}Slider->value=v; p.on{field}Commit();
            assert(studio.values[0].m{field}==std::clamp(v,0.f,1.f));
            assert(studio.values[1].m{field}==std::clamp(v,0.f,1.f));
            assert(studio.values[2].m{field}==0.f);
        }}
        p.m{field}Slider->value=0.f;p.on{field}Commit();
        assert(studio.values[0].m{field}==0.f && studio.values[1].m{field}==0.f);
'''
    source='''#include <algorithm>
#include <cassert>
#include <vector>
#include <iostream>
using F32=float;using LLUUID=int;
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
struct Value {float v;double asReal(){return v;}};
struct Slider {float value=0;Value getValue(){return {value};}};
struct ALGhostStudio {struct Instance {'''+fields+'''};Instance values[3];
static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
Instance* getInstance(int id){return id>=0&&id<3?&values[id]:nullptr;}};
struct ALPanelGhostStudio {std::vector<int> selectedInstances(){return {0,1,9};}'''+slots+'''};
'''+bodies+'''
int main(){ALPanelGhostStudio p;auto& studio=ALGhostStudio::instance();'''+tests+'''
std::cout<<"PASS six production clone FX callbacks: defaults, .5/1, bounds, multi-selection, missing id, reset to zero\\n";}
'''
    directory=check_directory('clone_fx')
    run=compile_run(directory,source)
    assert run.returncode==0,run.stdout+run.stderr
    print(run.stdout,flush=True)
    result=dict(callbacks=list(FIELDS),copy_paths=4,packed_uniform_upload_paths=2,
                defaults=True,reset=True,readback=True,session_only=True,exit=run.returncode)
    (directory/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':check()
