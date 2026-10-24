"""FIX1 regressions against production GLSL/C++ and the reviewed commit.

Invoked by relight_volumetric_gl_check.py --review-fix-only or --performance-only.
The frozen bad source is a negative control, never an alternate implementation.
"""
from pathlib import Path
import subprocess

import numpy as np

REVIEWED = 'dbcec8d2b36'


def review_checks(V, args, gl, host, report):
    def check(name, passed, **data):
        report['checks'].append(dict(name='fix1/'+name,passed=bool(passed),**data))
        print('fix1/'+name,'PASS' if passed else 'FAIL',data,flush=True)

    looks=V.parse_looks()['looks'];defaults=V.settings_defaults()
    def look(lid):
        return V.look_spec(V.look_state(next(l for l in looks if l['id']==lid),defaults))
    # This exact tuple is also emitted by Runner.run for a Clean Beam budget.
    # Explicitly link it, rather than assuming a runtime false branch compiles.
    saved=V.G.PREAMBLE
    try:
        V.G.PREAMBLE+='\n#define PROJVOL_INTEGRATION_AB 0\n#define PROJVOL_BUDGET_ENABLE 1\n#define PROJVOL_HARD_BEAM 0\n'
        startup=V.Runner(V.G.SourceTree('WORKTREE'),gl)
        reviewed_startup=V.Runner(V.G.SourceTree(REVIEWED),gl)
    finally:V.G.PREAMBLE=saved
    check('startup_AB0_BUDGET1_links',bool(startup.shaft and reviewed_startup.shaft),defines=dict(PROJVOL_INTEGRATION_AB=0,PROJVOL_BUDGET_ENABLE=1,PROJVOL_HARD_BEAM=0),
          existing_runner_tuple=[False,True,False,False])

    rig=V.LookRig(gl,host)
    before=V.Runner(V.G.SourceTree(REVIEWED),gl);before.host=host
    after=rig.runners(False)['after']
    f=V.backlit_fixture(args.width,args.height)
    sp=look('b:silhouette');sp['kwargs'].update(rim=0.,steps=64)
    startup.host=host
    zero=V.render_look(startup,f,sp,steps=0,budget_steps=0.)[1]
    check('startup_budget_zero_share',np.count_nonzero(zero)==0,nonzero_values=int(np.count_nonzero(zero)))
    transitions={}
    moving_frames=V.micro_frames(f)
    moving_bands=[V.LaneBand(frame,V.render_look(after,frame,sp,shadows=False)[0]) for frame in moving_frames]
    common=np.logical_and.reduce([b.cols for b in moving_bands])
    # Include the reported pair and all previous branch joins. The overlapping
    # entrants change the PRIMARY cone's share, with and without camera motion.
    for center in (4.,6.,7.,8.,8.06,12.,16.,'overlap','entry'):
        for moving in (False,True):
            frames=moving_frames if moving else [f]*9
            if isinstance(center,str):
                counts=[]
                for k,frame in enumerate(frames):
                    cam=frame[0];light=frame[4]
                    c=(cam.view@np.array([*light['pos'],1.]))[:3];w,h=cam.W//2,cam.H//2
                    a=V.host_request(host,'A',[w,h,12,*c,*cam.proj.T.ravel()])
                    coverage=float(np.clip((a[3]-a[1])/w,0,1)*np.clip((a[4]-a[2])/h,0,1)) if a[0] else 1.
                    # Five existing cones plus a growing sixth, or six full
                    # cones plus a seventh entering. Shares are computed by the
                    # production allocator at the 4K half-target height.
                    total=coverage*((5.91+.01*k) if center=='overlap' else (6.+.002*k))
                    counts.append(float(V.host_request(host,'P',[0,1,coverage,1080,6,32,0,.6,48,total])[0]))
                report.setdefault('backlit_overlap_allocations',{})[f'{center}/{moving}']=counts
            else:counts=[center+(k-4)*.005 for k in range(9)]
            for pre,label in ((None,'raw'),(0,'cold'),(10,'warm')):
                def sequence(runner):
                    history=np.zeros((args.height//2,args.width//2,4),np.float32)
                    previous=frames[0][0];valid=False;ims=[]
                    for k in ([0]*pre if pre else [])+list(range(9)):
                        im=V.render_look(runner,frames[k],sp,budget_steps=counts[k],half=pre is not None)[0]
                        if pre is not None:
                            history=runner.temporal_resolve(frames[k],history,previous,valid)
                            previous=frames[k][0];valid=True;im=history.copy()
                        ims.append(im[:,:,:3])
                    return ims[pre or 0:]
                a=sequence(after)
                if moving:
                    # Transport the same air lanes, as in the existing backlit
                    # motion gate; screen silhouettes really move under dolly.
                    profiles=[b.column_mean(b.samples(im))[common]/np.maximum(b.reference[common],1e-9)
                              for b,im in zip(moving_bands,a)]
                    delta=max(float(np.mean(np.abs(b-a)>.05*np.maximum(a,1e-4))) for a,b in zip(profiles,profiles[1:]))
                else:delta=max(V.motion_fractions(a))
                transitions[f'{center}/{moving}/{label}']=delta
    for runner,name in ((before,'before'),(after,'after')):
        pair=[V.render_look(runner,f,sp,half=False,budget_steps=n)[0] for n in (8.05,8.07)]
        transitions[name+'_reported_pair']=V.shaft_changes(*pair)
    maxima=[v for v in transitions.values() if isinstance(v,float)]
    old=transitions['before_reported_pair'];new=transitions['after_reported_pair']
    check('backlit_overlap_budget_motion',max(maxima)<=V.MOTION_PIXEL_BOUND and new[0]/max(new[1],1)<=V.MOTION_PIXEL_BOUND,
          bound=V.MOTION_PIXEL_BOUND,results=transitions)
    check('backlit_budget_negative_control',old[0]/max(old[1],1)>V.MOTION_PIXEL_BOUND,changed_lit=old)

    leaks={}
    for dust in (False,True):
        sp=look('b:dust_storm' if dust else 'b:silhouette')
        sp['kwargs'].update(rim=0.,steps=64,maximum=100.)
        sp['uniforms'].update(projvol_shadow_tint=0.)
        if not dust:
            sp['uniforms'].update(projvol_noise_strength=.75,projvol_noise_scale=2.,projvol_fog_strength=1.,
                                  projvol_fog_ground_density=8.,projvol_fog_falloff=.15,projvol_fog_base=.7)
        runner=rig.runners(dust)['after']
        old=V.Runner(V.G.SourceTree(REVIEWED),gl) if not dust else None
        if dust:
            saved=V.G.PREAMBLE
            try:
                V.G.PREAMBLE+='\n#define PROJVOL_DUST_ENABLE 1\n'
                old=V.DustRunner(V.G.SourceTree(REVIEWED),gl);old.volume=rig.volume
            finally:V.G.PREAMBLE=saved
        old.host=host
        for tint in (0.,.2):
            sp['uniforms']['projvol_shadow_tint']=tint
            ref=V.render_look(runner,f,sp,half=False,hard_beam=True)[0]
            clear=V.render_look(runner,f,sp,half=False,hard_beam=True,shadows=False)[0]
            mask=clear[:,:,:3].max(2)>1e-4
            for count in (4.,8.05):
                result={}
                for name,r in (('before',old),('after',runner)):
                    im=V.render_look(r,f,sp,half=False,budget_steps=count,hard_beam=True)[0]
                    delta=np.max(np.maximum(im[:,:,:3]-ref[:,:,:3],0),2)/np.maximum(clear[:,:,:3].max(2),1e-4)
                    result[name]=dict(max_leak=float(delta[mask].max()),pixels_over_5=int(np.sum(mask & (delta>.05))))
                leaks[f'{dust}/{tint}/{count}']=result
    check('heterogeneous_hard_shadow',all(v['after']['max_leak']<=.05 for v in leaks.values()),
          bound=.05,reference='64-step Full Hard, identical medium and tint',results=leaks)
    check('heterogeneous_negative_control',any(v['before']['max_leak']>.05 for v in leaks.values()))

    # FIX2 also approximates weak noise. Protect that field against the same
    # Full Hard reference, including high spatial frequencies and its runtime
    # blend joins; a cheap homogeneous-only timing fixture cannot prove this.
    weak={}
    sp=look('b:silhouette');sp['kwargs'].update(rim=0.,steps=64,maximum=100.)
    sp['uniforms']['projvol_shadow_tint']=0.
    for scale in (1.2,8.):
        sp['uniforms']['projvol_noise_scale']=scale
        for strength in (.124,.126,.49,.51):
            sp['uniforms']['projvol_noise_strength']=strength
            ref=V.render_look(after,f,sp,half=False,hard_beam=True)[0]
            clear=V.render_look(after,f,sp,half=False,hard_beam=True,shadows=False)[0]
            mask=clear[:,:,:3].max(2)>1e-4
            for count in (4.,8.05):
                im=V.render_look(after,f,sp,half=False,budget_steps=count,hard_beam=True)[0]
                delta=np.max(np.maximum(im[:,:,:3]-ref[:,:,:3],0),2)/np.maximum(clear[:,:,:3].max(2),1e-4)
                weak[f'{scale}/{strength}/{count}']=dict(max_leak=float(delta[mask].max()),pixels_over_5=int(np.sum(mask & (delta>.05))))
    check('weak_medium_hard_shadow',all(v['max_leak']<=.05 and v['pixels_over_5']==0 for v in weak.values()),
          bound=.05,reference='64-step Full Hard, weak-noise blend and frequency sweep',results=weak)

    sp=look('b:silhouette');sp['kwargs'].update(rim=0.,steps=64)
    jitter={}
    for name,r in (('before',before),('after',after)):
        jitter[name]={}
        for count in (0.,4.25):
            sp['uniforms'].update(projvol_shadow_jitter_tap=1,projvol_shadow_samples=4)
            a=V.render_look(r,f,sp,budget_steps=count,hard_beam=True)[1]
            sp['uniforms'].update(projvol_shadow_jitter_tap=0,projvol_shadow_samples=1)
            b=V.render_look(r,f,sp,budget_steps=count,hard_beam=True)[1]
            jitter[name][str(count)]=a.tobytes()==b.tobytes()
    check('hard_jitter_override',all(jitter['after'].values()),results=jitter)
    check('hard_jitter_negative_control',not any(jitter['before'].values()))

    conservative={}
    for ref in (REVIEWED,'WORKTREE'):
        saved=V.G.PREAMBLE
        try:
            V.G.PREAMBLE+='\n#define PROJVOL_CONSERVATIVE_SHADOW 1\n'
            r=V.Runner(V.G.SourceTree(ref),gl);r.host=host
        finally:V.G.PREAMBLE=saved
        conservative[ref]={}
        for count in (0.,4.25):
            a=V.render_look(r,f,sp,budget_steps=count,hard_beam=True,conservative=0)[0]
            b=V.render_look(r,f,sp,budget_steps=count,hard_beam=True,conservative=1)[0]
            conservative[ref][str(count)]=float(np.max(np.abs(a-b)))
    check('hard_conservative_control',all(v>0 for v in conservative['WORKTREE'].values()),results=conservative)
    check('hard_conservative_negative_control',all(v==0 for v in conservative[REVIEWED].values()))

    # Compile the reservation's actual filter, not a port of its predicates.
    from relight_volumetric_engine_check import compile_host
    reservation={}
    for ref in (REVIEWED,'WORKTREE'):
        pipe=V.G.SourceTree(ref).read('indra/newview/pipeline.cpp')
        start=pipe.index('        for (LLDrawable* candidate : mLights)',pipe.index('F64 total_coverage ='))
        body=pipe[start:pipe.index('            glm::vec4 sphere;',start)]+'            ++accepted;\n        }'
        source=r'''
#include <set>
#include <vector>
#include <iostream>
struct LLUUID {int n; bool operator<(const LLUUID& x)const{return n<x.n;} };
struct LLVector3{};
struct LLViewerObject {LLUUID getID(){return {7};}};
struct LLVOVolume {bool native=false,prism=true; bool isLightSpotlight(){return native;}
LLUUID getID(){return {1};} LLViewerObject* getRootEdit(){return nullptr;}};
struct LLDrawable {LLVOVolume v;LLVOVolume* getVOVolume(){return &v;}};
bool isVolumetricShaftEnabled(LLUUID){return true;}
namespace LLPrismLens {bool projectorRenderParams(LLVOVolume* v,LLVector3&){return v && (v->native||v->prism);}}
struct ALCineLightRigManager {static bool anyActorOnly(){return false;}
static ALCineLightRigManager& instance(){static ALCineLightRigManager a;return a;}bool actorOnlyBits(LLUUID){return false;}};
int main(){LLDrawable d[6];std::vector<LLDrawable*> mLights;for(auto& x:d)mLights.push_back(&x);
bool BDMergeFroxelVolumetrics=false,BDMergeFroxelLights=false;std::set<LLUUID> mFroxelInjectedProjectors;
int accepted=0;
'''+body+'\nstd::cout<<accepted;return 0;}\n'
        folder=args.out/('reserve_'+ref);folder.mkdir(exist_ok=True)
        exe=compile_host(folder,source)
        accepted=int(subprocess.check_output([str(exe)],text=True))
        total=accepted*1.
        steps=float(V.host_request(host,'P',[0,1,1,1080,6,32,0,.6,48,total])[0])
        reservation[ref]=dict(accepted=accepted,total_steps=6*steps)
    check('Prism_frame_reservation',reservation['WORKTREE']['accepted']==6 and reservation['WORKTREE']['total_steps']<=48.0001,
          budget=48,results=reservation)
    check('Prism_negative_control',reservation[REVIEWED]['total_steps']>48.)

    # A smooth control cannot participate in a compile/history key. This checks
    # the complete production selector, including the startup registration.
    for ref in (REVIEWED,'WORKTREE'):
        tree=V.G.SourceTree(ref);pipe=tree.read('indra/newview/pipeline.cpp')
        selector=pipe[pipe.index('    const U32 quality ='):pipe.index('    if (!projvolVariantReady',pipe.index('    const U32 quality ='))]
        report.setdefault('noise_variant',{})[ref]=selector.count('BDMergeProjectorVolumetricsNoiseStrength')
    check('noise_slider_no_compile_key',report['noise_variant']['WORKTREE']==0,counts=report['noise_variant'])
    check('noise_compile_negative_control',report['noise_variant'][REVIEWED]>0)

    noise={}
    sp=look('b:clean');sp['kwargs'].update(rim=0.)
    for ref in (REVIEWED,'WORKTREE'):
        r=V.Runner(V.G.SourceTree(ref),gl);r.host=host;ims=[];programs=[]
        for strength in (.49,.51):
            sp['uniforms']['projvol_noise_strength']=strength
            ims.append(V.render_look(r,f,sp,budget_steps=4.25)[0]);programs.append(len(r.variants))
        noise[ref]=dict(programs=programs,changed_lit=V.shaft_changes(*ims))
    check('noise_half_continuous',noise['WORKTREE']['programs'][0]==noise['WORKTREE']['programs'][1] and
          noise['WORKTREE']['changed_lit'][0]/max(noise['WORKTREE']['changed_lit'][1],1)<=V.MOTION_PIXEL_BOUND,results=noise)
    check('noise_half_negative_control',noise[REVIEWED]['programs'][1]>noise[REVIEWED]['programs'][0])

    ui={}
    for ref in (REVIEWED,'WORKTREE'):
        tree=V.G.SourceTree(ref)
        src=tree.read('indra/newview/alpanelprojectorshafts.cpp')
        start=src.index('void ALPanelProjectorShafts::refreshPresets()')
        body=src[start:src.index('\n}',start)+2]
        director=tree.read('indra/newview/llfloaterdirector.cpp')
        scene_start=director.index('const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()')
        scene_body=director[scene_start:director.index('\n}',scene_start)+2]
        start=director.index('    if (!mLauncherClock.due()) return;',director.index('void LLFloaterDirector::refreshLauncherCards()'))
        # Include the complete independent clock block, whichever its order.
        beginning=director.index('    if (LLTextBox* cost = mShaftCost.get())',director.index('void LLFloaterDirector::refreshLauncherCards()'))
        end=director.index('    // Read live Preferences on refresh;',beginning)
        timing=director[min(start,beginning):end]
        source=r'''
#include <string>
#include <vector>
#include <iostream>
using LLSD=std::string;
struct LLComboBox {std::vector<std::string> rows;std::string selected;
void clearRows(){rows.clear();}void add(const std::string&,const LLSD& id){rows.push_back(id);}
void selectFirstItem(){selected=rows[0];}bool setSelectedByValue(const LLSD& id,bool){for(auto& row:rows)if(row==id){selected=id;return true;}return false;}};
struct {std::string getString(const char*){return "2";}} gSavedSettings;
namespace ALProjectorShaftPresets {bool classicEngine(){return true;}
std::vector<std::string> classicLabels(){return std::vector<std::string>(8,"preset");}
const std::vector<std::string>& trackedSettings(){static std::vector<std::string> v={"BDMergeProjectorVolumetricsEngine","BDMergeProjectorVolumetricsPreset"};return v;}}
namespace ALProjectorShaftLooks {struct Look {std::string label,id;};std::vector<Look> looks(){return {};}}
namespace ALRotoInk {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
namespace ALTron {const std::vector<std::string>& settings(){static std::vector<std::string> v;return v;}}
struct LLFloaterDirector {static const std::vector<std::string>& sceneSettingsList();};
struct ALPanelProjectorShafts {LLComboBox* mPreset;void refreshPresets();};
struct LLTextBox{};struct Clock {bool value;bool due(){return value;}};
struct Handle {LLTextBox text;LLTextBox* get(){return &text;}};
struct {int projectorShaftCostReadout(){return 1;}} gPipeline;
bool isWidgetOnScreen(LLTextBox*){return true;}int writes=0;void setTextIfChanged(LLTextBox*,int){++writes;}
void tick(){Clock mLauncherClock{false},mShaftCostClock{true};Handle mShaftCost;
'''+timing+'\n}\n'+body+'\n'+scene_body+r'''
int main(){LLComboBox combo;ALPanelProjectorShafts panel{&combo};panel.refreshPresets();tick();
int duplicate=0;for(auto& name:LLFloaterDirector::sceneSettingsList())
if(name=="BDMergeProjectorVolumetricsEngine" || name=="BDMergeProjectorVolumetricsPreset")++duplicate;
std::cout<<(combo.selected=="2")<<" "<<writes<<" "<<duplicate;}
'''
        folder=args.out/('ui_'+ref);folder.mkdir(exist_ok=True)
        exe=compile_host(folder,source)
        ui[ref]=list(map(int,subprocess.check_output([str(exe)],text=True).split()))
    check('preset_combo_and_independent_clock',ui['WORKTREE'][:2]==[1,1],preset_selected_readout_updates_duplicates=ui)
    check('scene_selectors_one_owner',ui['WORKTREE'][2]==0,results=ui)
    check('UI_negative_control',ui[REVIEWED]==[0,0,2])
