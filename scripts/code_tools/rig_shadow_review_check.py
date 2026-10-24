"""Source contracts supplement production-GL/model/menu regression checks."""
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from relight_3d_gl import SourceTree
from rig_shadow_presets_check import BASE, function


def check(out, source_ref='WORKTREE'):
    tree=SourceTree(source_ref)
    detail=tree.read('scripts/code_tools/rig_shadow_detail_check.py')
    presets=tree.read('scripts/code_tools/rig_shadow_presets_check.py')
    shader=tree.read('indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl')
    settings=ET.fromstring(tree.read('indra/newview/app_settings/settings.xml'))
    nodes=list(settings.find('map'))
    setting=next(nodes[i+1] for i,n in enumerate(nodes[:-1]) if n.tag=='key' and n.text=='RenderSpotShadowDetailBias')
    values=list(setting);value=next(values[i+1].text for i,n in enumerate(values[:-1]) if n.text=='Value')
    pipeline=tree.read('indra/newview/pipeline.cpp')
    pcf=function(shader,'float pcfSpotShadow(')
    shafts=tree.read('indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl')
    froxel=tree.read('indra/newview/app_settings/shaders/class1/deferred/froxelInjectF.glsl')
    header=tree.read('indra/newview/alcinelightrigmanager.h')
    xui=ET.fromstring(tree.read('indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml'))
    rows={n.get('setting'):n for n in xui.iter('kit_row')}
    flags={f'CineLightRig{role}{flag}' for role in ('Key','Fill','Rim','Bg') for flag in ('DetailShadows','WorldShadowPick')}
    checks={
        '2_cold_default':value=='1' and '"RenderSpotShadowDetailBias", true' in pipeline and "assert defaults['RenderSpotShadowDetailBias'] is True" in presets and "defaults['RenderSpotShadowDetailBias'] = True" not in presets,
        '3_airborne_bypass':'spot_shadow_detail[index] > 0.5 && dot(norm,norm) > 0.0' in shader and 'sampleSpotShadow(shadow_pos, vec3(0.0)' in shafts and 'sampleSpotShadow(sp, vec3(0.0)' in shafts and 'sampleSpotShadow(vpos, vec3(0.0)' in froxel,
        '5_identity_BASE':"G.SourceTree(BASE)" in detail and "G.SourceTree('HEAD')" not in detail and "base_e=dict(e,detail_shadows=False)" in detail and "base_defaults=dict(defaults,RenderSpotShadowDetailBias=False)" in detail and BASE=='13028c968b4',
        '6_existing_presets_BASE_model':'def baseline_model_outputs(' in presets and "((BASE,baseline),('WORKTREE',presets[:len(baseline)]))" in presets and "tree.read('indra/newview/'+name)" in presets and 'outputs[0]==outputs[1]' in presets,
        '7_unflagged_no_plane_ALU':'clamp(dot(plane' not in pcf and 'if (detail)' in pcf and 'detail ? detailSpotTap' in pcf,
        '10_trailing_flags_comment':'Appended last' not in header and 'These trailing actor/detail/world flags are omitted and default false.' in header,
        '11_Director_coverage':all(s in rows and rows[s].get('kind')=='toggle' and rows[s].find('kit_switch').get('control_name')==s for s in flags) and rows['RenderSpotShadowDetailBias'].get('label')=='Detail shadow bias',
    }
    result=dict(passed=all(checks.values()),source_ref=source_ref,checks=checks)
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    (out/'review_contracts.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('[shadow-review]',result,flush=True)
    return result


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--source-ref',default='WORKTREE');ap.add_argument('--out',default='.tmp/rig_shadow/fix2/review')
    args=ap.parse_args();raise SystemExit(0 if check(args.out,args.source_ref)['passed'] else 1)
