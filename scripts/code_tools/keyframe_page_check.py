"""Camera Keyframes page, timeline, tool and guide integration. Four named source negatives."""
from pathlib import Path
import copy
import json
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
NV=ROOT/'indra/newview'
XUI=NV/'skins/default/xui/en'

def assertions(files):
    errors=[]
    def need(name,value):
        if not value:errors.append(name)
    panel=files['alpanelkeyframes.cpp'];timeline=files['alkeyframetimeline.cpp'];overlay=files['alkeyframeoverlay.cpp'];tool=files['altoolkeyframeedit.cpp']
    data=json.loads(files['mockup']);camera=next(g for g in data['groups'] if g['name']=='camera_group');pages=[p['name'] for p in camera['pages']]
    page=next(p for p in camera['pages'] if p['name']=='keyframes_tab');rows={r['w']:r for c in page['cards'] for r in c['rows']}
    xml=ET.fromstring(files['page_xml']);index=files['index']
    need('P1 page/search',pages.index('keyframes_tab')==pages.index('takes_tab')+1 and len(rows)==57 and all(r.get('kw') for r in rows.values()) and 'name="keyframes_tab" label="Keyframes" page_file="panel_dpage_camera_keyframes.xml"' in files['floater'] and all(w in index for w in rows) and 'panel_keyframes' in files['page_xml'] and '"keyframe_timeline": "viewport"' in files['manifest'])
    need('P2 owned history',all(('bind("'+w+'",') in panel for w in rows if w!='kf_timeline') and all(x in panel for x in ('mHistory->sequence(r)','mHistory->replay(r)','mHistory->discard_redo()','mHistory->ready(r,s)','mHistory->discard_step(r,s)','mHistory->group_sequence(r)','mPage->historyEdited(true)','Stop playback to undo keyframe edits','Stop playback to redo keyframe edits','connection.disconnect()')) and '"Stop the walk to undo path edits"' in files['kit_h'] and '"Stop the walk to redo path edits"' in files['kit_h'] and 'redo ? mOwnedRedoBlocked : mOwnedUndoBlocked' in files['kit_cpp'])
    need('P3 gestures/clean guides',all(x in timeline for x in ('c.departure[i-1]+.05','track->keys[i].hold-.05','gFocusMgr.setMouseCapture(this)','history().cancelGesture','onMouseCaptureLost','if(restored) {refresh();onCommit();}','MASK_CONTROL|MASK_SHIFT','LLSliderWheel::noteScroll(this)','mUndo(k==\'Y\')','mRevision == p.sequence().revision','LL_PROFILE_ZONE_SCOPED_CATEGORY_UI')) and all(x in overlay for x in ('ALCleanFrame::active()','rlvHandbackActive()','gSnapshot || gCubeSnapshot','sPrismLensRender','sReflectionRender','getUIVisibility()','hasRenderDebugFeatureMask','glBindVertexArray(vao)','gGL.popMatrix()','if(shader) shader->bind()')) and 'new ' not in overlay[overlay.index('void ALKeyframeOverlay::draw()'):] and all(x in tool for x in ('beginGesture(mBefore)','cancelGesture(ALKeyframePlayer::instance().sequence())','mHistory->commit(mBefore','mousePointOnPlaneGlobal','HandleMode::Aligned')) and 'setCancelEdit([this]{cancelEdits();})' in panel and 'p.setGuideDraw(ALKeyframeOverlay::draw)' in panel)
    need('P4 controls/targets/fit',all(rows[w]['k']=='toggle' for w in ('kf_playing','kf_show')) and 'ALKitRow::notifyValueChanged(c)' in panel and all(x in panel for x in ('getActorChoices()','getJointNum()','if(!canLock()) return','restricted?why','FitVerdict::Approximate','result.worst.position*100','result.worst.rotation/ALKf::degree','result.worst.fov/ALKf::degree','ALKf::Sequence next;next.name="Fitted take"','ALKf::appendFit(next,result,id)','switchSequence(std::move(next))','rotation exceeds 170 degrees','saveVerified(file,next,why)')) and all('Keyframes.CanLock' in files[f] and 'Lock keyframe camera on this' in files[f] for f in ('menu_avatar_other.xml','menu_avatar_self.xml','menu_object.xml','menu_attachment_other.xml','menu_attachment_self.xml')) and 'Keyframes.PlayStop' in files['menu_viewer.xml'] and 'keyframe_play_stop' in rows['kf_playing']['kw'] and 'name="keyframe_play_stop"' in files['controls'] and 'name="keyframe_play_stop"' not in files['bindings'])
    return errors

def main():
    names=['alpanelkeyframes.cpp','alkeyframetimeline.cpp','alkeyframeoverlay.cpp','altoolkeyframeedit.cpp']
    f={n:(NV/n).read_text(encoding='utf-8') for n in names}
    for name in ('menu_avatar_other.xml','menu_avatar_self.xml','menu_object.xml','menu_attachment_other.xml','menu_attachment_self.xml','menu_viewer.xml'):f[name]=(XUI/name).read_text(encoding='utf-8')
    for key,path in {'mockup':ROOT/'scripts/code_tools/director_kit/director_mockup_data.json','page_xml':XUI/'panel_dpage_camera_keyframes.xml','index':XUI/'director_kit_index.xml','floater':XUI/'floater_director_v2.xml','manifest':ROOT/'scripts/code_tools/director_functional_manifest.py','kit_h':NV/'alkitpage.h','kit_cpp':NV/'alkitpage.cpp','controls':XUI/'control_table_contents_machinima.xml','bindings':NV/'app_settings/key_bindings.xml'}.items():f[key]=path.read_text(encoding='utf-8')
    failed=assertions(f);assert not failed,failed
    print('PASS P1-P4 Keyframes page/search, owned history, gestures/clean guides, controls/targets/fit')
    mutants=[('P1 page/search','missing_index','index','kf_timeline','removed_timeline'),('P2 owned history','missing_discard_step','alpanelkeyframes.cpp','mHistory->discard_step(r,s)','void(0)'),('P3 gestures/clean guides','guide_during_Play','alkeyframeoverlay.cpp','ALCleanFrame::active()','false'),('P4 controls/targets/fit','Playing_not_bool','mockup','"w":"kf_playing","l":"Playing","k":"toggle"','"w":"kf_playing","l":"Playing","k":"button"')]
    for assertion,name,file,before,after in mutants:
        assert before in f[file],name
        planted=dict(f);planted[file]=f[file].replace(before,after)
        failed=assertions(planted);assert assertion in failed,(name,failed)
        print('PASS negative [src] '+name+': FAIL '+assertion)
    assert not assertions(f)
    print('PASS clean page after 4 named source negatives')
if __name__=='__main__':main()
