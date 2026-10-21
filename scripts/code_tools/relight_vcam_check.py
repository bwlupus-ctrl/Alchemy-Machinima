"""Production C++ view/transaction/budget fixture; no viewer build or launch."""
from pathlib import Path
import os
import subprocess
import xml.etree.ElementTree as ET
from relight_area_policy_check import method


def check_vcam():
    nv = Path('indra/newview')
    pipeline = (nv / 'pipeline.cpp').read_text(encoding='utf-8')
    begin = method(nv / 'pipeline.cpp', 'bool LLPipeline::beginPrismAuxiliaryState()')
    begin = begin[begin.index('    static LLCachedControl<bool> area('):begin.index('    mPrismSavedNearbyLights =')]
    end = method(nv / 'pipeline.cpp', 'void LLPipeline::endPrismAuxiliaryState()')
    end = end[end.index('    if (mPrismAreaStateSaved)'):end.index('    // Discard the temporary')]
    # No alternate binding route: dispatch and consumers use the same active pack.
    render = method(nv / 'pipeline.cpp', 'void LLPipeline::renderAreaShadows(')
    assert 'sPrismLensRender ? ALAreaShadow::feedWorkload(' in render
    assert 'mPrismAreaPasses.admit(gFrameCount,LLPrismLens::MAX_CAPTURES)' in render
    assert 'const glm::mat4 view = get_current_modelview();' in render
    assert 'mul_mat4_vec3(view,glm::make_vec3(drawable->getPositionAgent().mV))' in render
    assert 'LLViewerCamera& camera = LLViewerCamera::instance();' in render
    assert 'const bool measure = !sPrismLensRender && !mAreaShadowQueryPending;' in render
    assert 'if (!sPrismLensRender && mAreaShadowQueryPending)' in render
    shape = method(nv / 'pipeline.cpp', 'static bool light_shape_enabled()')
    assert '!LLPipeline::sPrismLensRender' in shape
    header = (nv / 'pipeline.h').read_text(encoding='utf-8')
    comparator = method(nv / 'pipeline.h', '            bool operator()(const Light& a, const Light& b) const')
    assert 'stable_area_feed ? light->getID() : LLUUID::null' in pipeline
    assert 'const bool stable_area_feed = area() && areaShadowsAllowedThisView();' in pipeline
    fields = header[header.index('    bool mPrismAreaStateSaved'):header.index('    LLRenderTarget          mSceneMap;')]
    main_fields = header[header.index('    bool mAreaShadowValid'):header.index('    // Opt-in Prism transaction;')]
    main_fields = main_fields[0:main_fields.index('    GLuint mAreaShadowQueries')] + main_fields[main_fields.index('    std::array<LLUUID, 8> mAreaShadowIDs;'):]
    fixture = r'''
#include "alareashadowmath.h"
#include "baseline.h"
#include <cassert>
#include <map>
#include <string>
#include <iostream>
using U32=unsigned; using LLUUID=int; using GLint=int; using F32=float;
struct Settings { std::map<std::string,bool> values; } gSavedSettings;
template<class T> struct LLCachedControl {
    const char* key;
    LLCachedControl(Settings&,const char* k,T) : key(k) {}
    T operator()() const { return gSavedSettings.values[key]; }
};
struct LLVector4 { float mV[4]={0,0,1,-1}; };
struct LLVOVolume { int id; int getID() const { return id; } };
LLVector4 light_shape_for_volume(const LLVOVolume*) { return {}; }
struct LLViewerCamera {
    float zoom=1;
    static LLViewerCamera& instance() { static LLViewerCamera c; return c; }
    float getZoomFactor() const { return zoom; }
};
bool gCubeSnapshot=false;
int drawFBO=41,readFBO=42;
constexpr int GL_DRAW_FRAMEBUFFER_BINDING=1,GL_READ_FRAMEBUFFER_BINDING=2;
constexpr int GL_DRAW_FRAMEBUFFER=3,GL_READ_FRAMEBUFFER=4;
void glGetIntegerv(int what,int* v) { *v=what==1?drawFBO:readFBO; }
void glBindFramebuffer(int what,int v) { (what==3?drawFBO:readFBO)=v; }
struct LLRenderTarget {
    int releases=0;
    void release() { ++releases; drawFBO=readFBO=99; }
};
namespace ALRelight { struct AreaTakeState { int fingerprint=0; }; }
struct LLPipeline {
    struct RenderTargetPack { LLRenderTarget areaGather[2],areaScratch[2],areaVisibility[2]; };
    struct Scratch { RenderTargetPack rt; } scratch;
    RenderTargetPack mMainRT;
    RenderTargetPack* mRT=&mMainRT;
    Scratch* mActivePrismLensScratch=nullptr;
    static constexpr unsigned MAX_SPOT_SHADOWS=10;
    bool sPrismLensRender=false,sPrismPreviewRender=false,sRenderingHUDs=false;
    bool sImpostorRender=false,sReflectionRender=false;
    bool areaShadowsAllowedThisView() const;
    bool areaShadowsActive() const;
    LLVector4 deferredLightShape(const LLVOVolume*) const;
    void begin(); void end();
'''+main_fields+fields+'};\n'
    fixture += '\n'.join(method(nv / 'pipeline.cpp', signature) for signature in (
        'bool LLPipeline::areaShadowsAllowedThisView() const',
        'bool LLPipeline::areaShadowsActive() const',
        'LLVector4 LLPipeline::deferredLightShape('))
    fixture += '\nvoid LLPipeline::begin() {\n'+begin+'}\nvoid LLPipeline::end() {\n'+end+'}\n'
    fixture += '\nstruct Light { float dist; int drawable; int areaFeedID; };\nstruct Compare {\n'+comparator+'\n};\n'
    fixture += r'''
int main() {
    // Execute the production collector comparator. Distance still wins; UUID
    // resolves equal-distance feed admission, before the unchanged draw limit.
    // Main/OFF entries all carry null IDs and retain the exact pointer order.
    Compare compare;
    for (int a=1;a<8;++a) for (int b=1;b<8;++b)
    for (float da:{1.f,2.f}) for (float db:{1.f,2.f}) {
        Light left{da,a,0},right{db,b,0};
        assert(compare(left,right)==(da<db || (da==db && a<b)));
    }
    std::array<int,4> order={0,1,2,3};
    do {
        for (bool reversed:{false,true}) {
            std::array<Light,4> lights;
            for (int i=0;i<4;++i) {
                int id=order[i]+1;
                lights[i]={1.f,reversed?5-id:id,id};
            }
            std::sort(lights.begin(),lights.end(),compare);
            ALAreaShadow::Selection<int> selected;
            for (int i=0;i<2;++i) selected.add({lights[i].areaFeedID,false,1.0},2);
            assert(selected.lights[0].id==1 && selected.lights[1].id==2);
        }
    } while(std::next_permutation(order.begin(),order.end()));
    gSavedSettings.values["RenderAreaShadows"]=true;
    LLPipeline p;
    p.mAreaShadowValid=true; p.mAreaShadowCount=2; p.mAreaShadowIDs={11,22};
    p.mRelightAreaTake.fingerprint=71; p.mRelightAreaSlotIDs[0]=81;
    LLVOVolume mainLight{11},feedLight{99};
    for (bool enabled : {false,true}) {
        gSavedSettings.values["RenderAreaShadowsVCam"]=enabled;
        for (int feeds : {1,2,4}) for (int i=0;i<feeds;++i) {
            p.begin();
            p.sPrismLensRender=true; p.mRT=&p.scratch.rt; p.mActivePrismLensScratch=&p.scratch;
            assert(p.areaShadowsAllowedThisView()==enabled);
            if (enabled) {
                assert(!p.areaShadowsActive() && p.mAreaShadowCount==0);
                p.mRelightAreaTake.fingerprint=72; p.mRelightAreaSlotIDs[0]=82;
                // Before production dispatch, neither eye has channels here.
                // Publication and feed routing run through the GL fixture.
                assert(p.deferredLightShape(&feedLight).mV[3]==-1);
                assert(p.deferredLightShape(&mainLight).mV[3]==-1);
                for (int exclude=0;exclude<7;++exclude) {
                    switch(exclude) {
                    case 0:gCubeSnapshot=true;break;case 1:p.sRenderingHUDs=true;break;
                    case 2:p.sImpostorRender=true;break;case 3:p.sReflectionRender=true;break;
                    case 4:p.sPrismPreviewRender=true;break;case 5:p.mPrismAreaTiled=true;break;
                    case 6:p.mRT=&p.mMainRT;break;
                    }
                    assert(!p.areaShadowsAllowedThisView());
                    gCubeSnapshot=p.sRenderingHUDs=p.sImpostorRender=p.sReflectionRender=false;
                    p.sPrismPreviewRender=p.mPrismAreaTiled=false;p.mRT=&p.scratch.rt;
                }
            } else {
                assert(p.deferredLightShape(&feedLight).mV[3]==-1);
                assert(!p.mPrismAreaStateSaved);
            }
            p.sPrismLensRender=false;p.mRT=&p.mMainRT;p.end();
            assert(p.mAreaShadowValid && p.mAreaShadowCount==2);
            assert(p.mAreaShadowIDs[0]==11 && p.mAreaShadowIDs[1]==22);
            assert(p.mRelightAreaTake.fingerprint==71 && p.mRelightAreaSlotIDs[0]==81);
            assert(p.deferredLightShape(&mainLight).mV[3]==0);
            assert(p.deferredLightShape(&feedLight).mV[3]==-1);
            assert(drawFBO==41 && readFBO==42);
            assert(!p.mPrismAreaStateSaved);
            for (const auto& t:p.mMainRT.areaVisibility) assert(t.releases==0);
        }
    }
    for (const auto& t:p.scratch.rt.areaVisibility) assert(t.releases==0);
    // Tiled MAIN snapshots stay excluded even if a feed camera resets its zoom.
    LLViewerCamera::instance().zoom=2;p.begin();LLViewerCamera::instance().zoom=1;
    p.sPrismLensRender=true;p.mRT=&p.scratch.rt;
    assert(!p.areaShadowsAllowedThisView());p.sPrismLensRender=false;p.mRT=&p.mMainRT;p.end();
    // No new capture state, release or channel advertisement with the master OFF.
    gSavedSettings.values["RenderAreaShadows"]=false;p.begin();assert(!p.mPrismAreaStateSaved);
    assert(!p.areaShadowsActive());p.end();
    // Compare unchanged main planning against the independent committed header.
    for (int preset=0;preset<4;++preset) for (int budget:{1,100,1500})
    for (unsigned width:{1280u,1920u,3840u}) for (unsigned cap:{1u,4u,8u})
    for (unsigned selected=1;selected<=cap;++selected) {
        auto q=ALAreaShadow::quality(preset);
        auto main=ALAreaShadow::workload(q,width,width*9/16,selected,budget,cap);
        auto old=ALAreaShadowBaseline::workload({q.samples,q.steps,q.half},width,width*9/16,selected,budget,cap);
        assert(main.q.samples==old.q.samples && main.q.steps==old.q.steps && main.q.half==old.q.half);
        assert(main.channels==old.channels && main.pixels==old.pixels && main.iterations==old.iterations);
        assert(main.budget==old.budget && main.drawCap==old.drawCap);
        for (unsigned feeds:{1u,2u,4u,8u}) {
            auto total=main.channels?main.iterations:0;
            for (unsigned f=0;f<feeds;++f) {
                auto feed=ALAreaShadow::feedWorkload(q,width,width*9/16,511+f*127,287+f*71,cap,budget,cap,8);
                assert(feed.iterations<=feed.budget);
                total+=feed.channels?feed.iterations:0;
            }
            assert(total<=main.budget);
        }
    }
    std::cout << "[RelightQ] VCam CPU PASS: production view/transaction/comparator, UUID feed admission across permutations and reversed pointers, main/OFF pointer ordering preserved, 1/2/4 feed channel and diagnostic restoration, prepass invalidation, retained targets; HEAD main-plan identity and shared budget for 1/2/4/8 feeds\n";
}
'''
    directory = Path('.tmp/relight_step6_cpu').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    baseline = subprocess.check_output(['git','show','HEAD:indra/newview/alareashadowmath.h'],text=True)
    baseline = baseline.replace('AL_AREA_SHADOW_MATH_H','AL_AREA_SHADOW_BASELINE_H').replace(
        'namespace ALAreaShadow','namespace ALAreaShadowBaseline')
    (directory / 'baseline.h').write_text(baseline,encoding='utf-8')
    (directory / 'vcam.cpp').write_text(fixture,encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory / 'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /I"{nv.resolve()}" '
        f'/Fo"{directory / "vcam.obj"}" /Fe"{directory / "vcam.exe"}" '
        f'"{directory / "vcam.cpp"}"\nexit /b %ERRORLEVEL%\n',encoding='utf-8')
    subprocess.run(['cmd','/d','/c',str(directory / 'compile.cmd')],check=True)
    subprocess.run([str(directory / 'vcam.exe')],check=True)
    page = ET.parse(nv / 'skins/default/xui/en/panel_dpage_light_rim.xml')
    row = page.find('.//kit_card[@id="area_soft_shadows"]/kit_row[@widget="area_shadow_vcam"]')
    assert row is not None and row.find('kit_switch').get('control_name')=='RenderAreaShadowsVCam'
    settings = ET.parse(nv / 'app_settings/settings.xml').getroot().find('map')
    entries = list(settings)
    index = next(i for i,e in enumerate(entries) if e.tag=='key' and e.text=='RenderAreaShadowsVCam')
    values = list(entries[index+1])
    assert next(values[i+1].text for i,e in enumerate(values) if e.text=='Value')=='0'
    print('[RelightQ] VCam UI PASS: real Soft shadows switch, persistent default OFF',flush=True)


if __name__=='__main__':
    check_vcam()
