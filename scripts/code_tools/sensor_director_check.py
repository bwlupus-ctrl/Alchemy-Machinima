"""WP-C Director sensor contracts; no viewer build. Run with PYTHONUTF8=1."""
import copy
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from types import SimpleNamespace
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
KIT = ROOT / 'scripts/code_tools/director_kit'
XUI = ROOT / 'indra/newview/skins/default/xui/en'
KEYWORDS = ('iso', 'sensor', 'noise', 'grain', 'gain', 'dual native',
            'headroom', 'read noise', 'camera')
ISO = (50, 64, 80, 100, 125, 160, 200, 250, 320, 400, 500, 640, 800,
       1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400, 8000,
       10000, 12800, 16000, 20000, 25600)


def check(xml=None, quiet=False):
    data = json.loads((KIT / 'director_mockup_data.json').read_text(encoding='utf-8'))
    page = next(p for g in data['groups'] for p in g['pages'] if p['name'] == 'exposure_tab')
    cards = {c['id']: c for c in page['cards']}
    assert 'sensor' in cards and 'custom_sensor' in cards, 'Sensor / Custom sensor cards missing'
    assert cards['sensor']['t'] == 'Sensor' and cards['custom_sensor']['t'] == 'Custom sensor'
    if xml is None:
        xml = ET.parse(XUI / 'panel_dpage_light_exposure.xml').getroot()
    for card_id in ('sensor', 'custom_sensor'):
        card = next(c for c in xml.iter('kit_card') if c.get('id') == card_id)
        rows = cards[card_id]['rows']
        assert len(list(card.iter('kit_row'))) == len(rows)
        for row in rows:
            assert row.get('h') and row.get('kw'), row['w'] + ': missing help/search words'
            node = next(n for n in card.iter('kit_row') if n.get('widget') == row['w'])
            setting = row.get('s') or row.get('code_setting')
            if setting:
                ctrl = next(n for n in node if n.get('name') == row['w'])
                if row.get('code_setting'):
                    assert node.get('bind') == 'code' and not ctrl.get('control_name')
                    source = (ROOT / 'indra/newview/alpanelexposurepage.cpp').read_text(encoding='utf-8')
                    assert (f'ALKitSettingBinding>("{setting}")' in source if setting == 'CineSensorISO'
                            else 'std::make_unique<SensorSeedBinding>()' in source), row['w'] + ': typed read-back'
                    assert f'alKitSetTypedControl(gSavedSettings, "{setting}", control->getValue())' in source
                    assert setting in xml.find('kit_page').get('watch_settings').split(',')
                else:
                    assert node.get('bind') == 'setting' and node.get('setting') == setting, row['w']
                    assert ctrl.get('control_name') == setting, row['w'] + ': no native read-back'
                for source, target in (('mn', 'min_val'), ('mx', 'max_val'), ('st', 'increment')):
                    if source in row:
                        assert float(ctrl.get(target)) == row[source], row['w'] + ': ' + target
                if row['k'] == 'combo':
                    values = [float(n.get('value')) for n in ctrl if n.tag == 'combo_box.item']
                    assert values == row['iv'], row['w'] + ': combo item values'
    iso = next(r for r in cards['sensor']['rows'] if r.get('code_setting') == 'CineSensorISO')
    assert tuple(iso['iv']) == ISO, 'third-stop ISO range'
    expected_ranges = {'sensor_amount': (0, 4), 'sensor_chroma': (0, 4),
                       'sensor_custom_base': (1, 102400), 'sensor_custom_switch': (0, 102400),
                       'sensor_custom_headroom': (0, 12)}
    for mode in ('lcg', 'hcg'):
        for name, bounds in {'fullwell': (1, 10000000), 'readnoise': (.001, 10000),
                             'analogbase': (.0001, 100), 'pedestal': (0, 65534),
                             'white': (1, 65535)}.items():
            expected_ranges[f'sensor_custom_{mode}_{name}'] = bounds
    for card in (cards['sensor'], cards['custom_sensor']):
        for row in card['rows']:
            if row['w'] in expected_ranges:
                assert (row['mn'], row['mx']) == expected_ranges[row['w']], row['w'] + ': consumer range'
            if row['w'] in {'sensor_preset', 'sensor_brightness'}:
                assert row['iv'] == ([0, 1, 2, 3] if row['w'] == 'sensor_preset' else [0, 1])
            if row['w'] == 'sensor_seed':
                assert row['k'] == 'text', 'U32 seed must survive beyond F32 integer precision'
    bound = {n.get('setting') for n in xml.iter('kit_row')}
    assert not bound & {'CineSensorNoiseEnabled', 'CineSensorPhysicalOrderEnabled'}, 'debug-only rows'
    if not quiet:
        print('PASS: cards, per-control native setting/read-back, numeric ranges and combo values')


def static_mutations():
    root = ET.parse(XUI / 'panel_dpage_light_exposure.xml').getroot()
    tested = 0
    for card in root.iter('kit_card'):
        if card.get('id') not in {'sensor', 'custom_sensor'}:
            continue
        for row in card.iter('kit_row'):
            widget = row.get('widget')
            mutations = []
            if row.get('setting') or row.get('bind') == 'code' and widget in {'sensor_iso', 'sensor_seed'}:
                mutations.append(('bind', row.get('bind'), 'none'))
            ctrl = next((n for n in row if n.get('name') == widget), None)
            if ctrl is not None:
                mutations += [(attr, ctrl.get(attr), '99999999')
                              for attr in ('min_val', 'max_val', 'increment') if ctrl.get(attr) is not None]
                if ctrl.tag == 'combo_box':
                    mutations.append(('value', ctrl[0].get('value'), '-999'))
            for attr, _old, replacement in mutations:
                mutant = copy.deepcopy(root)
                target = next(n for n in mutant.iter('kit_row') if n.get('widget') == widget)
                if attr != 'bind':
                    target = next(n for n in target if n.get('name') == widget)
                    if attr == 'value':
                        target = target[0]
                target.set(attr, replacement)
                try:
                    check(mutant, quiet=True)
                except AssertionError:
                    tested += 1
                else:
                    raise AssertionError(widget + ': escaped ' + attr + ' mutation')
    print(f'PASS: rejected {tested} per-control binding/range/combo mutations')
    import director_wiring_table as wiring
    registry = SimpleNamespace(panel_reg={'panel_exposure_page': 'ALPanelExposurePage'})
    for attrs, expected in (({'class': 'panel_exposure_page'}, True), ({}, False),
                            ({'class': 'unknown_injector'}, False)):
        context = SimpleNamespace(cpp=registry, closure=lambda page: ['panel_dpage_light_exposure.xml'],
                                  elems=lambda rel: [('panel', 3, attrs)])
        assert ('ALPanelExposurePage' in wiring.Ctx.inst_classes(context, 'exposure_tab')) == expected
    print('PASS: explicit kit injector ownership; missing/unknown injectors remain uninstantiated')


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    level = 1
    end = brace + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def native():
    """Compile production model, settings resolver, UI refresh seams and search engine.

    Only viewer services/widgets are stubbed. No renderer or viewer build is run.
    Production numerical formulas, query scoring and UI predicates are unchanged.
    """
    out = ROOT / '.tmp/iso_sensor/wpc/native'
    out.mkdir(parents=True, exist_ok=True)
    view = ROOT / 'indra/newview'
    page = (view / 'alpanelexposurepage.cpp').read_text(encoding='utf-8')
    methods = '\n'.join(function(page, 'void ALPanelExposurePage::' + name + '(')
                        for name in ('updateSensor', 'showSensorReadout', 'updateSensorStatus', 'updateSensorGating', 'syncSensorISO'))
    control = (view / 'llviewercontrol.cpp').read_text(encoding='utf-8')
    listener = re.search(r'    setting_setup_signal_listener\(gSavedSettings, "CineSensorPreset",.*?\n    \}\);',
                         function(control, 'void settings_setup_listeners()'), re.S).group()
    seed_validation = function(page, 'bool sensorSeedTextValid(') + '\n' + function(page, 'std::string sensorSeedText(')
    binding = (view / 'alkitbinding.cpp').read_text(encoding='utf-8')
    seed_conversion = '\n'.join(function(binding, signature) for signature in
                                ('U32 llsdToU32(', 'LLSD unsignedControlValue('))
    pipeline_status = function((view / 'pipeline.cpp').read_text(encoding='utf-8'),
                               'const char* LLPipeline::sensorUnavailableReason(')
    # Include the production header verbatim beside a minimal settings service.
    (out / 'alsensorsettings.h').write_bytes((view / 'alsensorsettings.h').read_bytes())
    (out / 'linden_common.h').write_text('', encoding='utf-8')
    (out / 'llviewercontrol.h').write_text('', encoding='utf-8')
    search_header = (view / 'alkitsearch.h').read_text(encoding='utf-8')
    search = (view / 'alkitsearch.cpp').read_text(encoding='utf-8')
    search_header = re.sub(r'^#include "[^"]+"\s*$', '', search_header, flags=re.M)
    search = re.sub(r'^#include "[^"]+"\s*$', '', search, flags=re.M)
    qol = (view / 'aldirectorqol.cpp').read_text(encoding='utf-8')
    helpers = '\n'.join(function(qol, sig) for sig in (
        'S32 editDistance(', 'bool isSubsequence(', 'S32 ALDirectorQol::fuzzyTokenScore(',
        'S32 ALDirectorQol::searchTokenScore(', 'bool ALDirectorQol::searchResultBefore('))
    header = (view / 'aldirectorqol.h').read_text(encoding='utf-8')
    enum = header[header.index('enum class SearchField'):header.index('};', header.index('enum class SearchField')) + 2]
    prefix = r'''
#include "alsensormodel.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#define REQUIRE(value) do { if (!(value)) { std::cerr << "contract failed: " << #value << '\n'; std::exit(2); } } while (0)
#undef assert
#define assert(value) REQUIRE(value)
using F32=float; using F64=double; using U32=unsigned; using S32=int;
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llmax(T a,T b){return std::max(a,b);}
template<class T> T llclamp(T v,T lo,T hi){return std::clamp(v,lo,hi);}
struct LLSD {
    using Real=double;
    double value=0; std::string text; bool string=false,integer=false;
    LLSD()=default; LLSD(double x):value(x){} LLSD(int x):value(x),integer(true){}
    LLSD(std::string x):text(x),string(true){}
    bool isReal() const{return !string&&!integer;}bool isString() const{return string;}
    bool isInteger() const{return integer;}double asReal() const{return value;}
    int asInteger() const{return static_cast<int>(value);}
    std::string asString() const {if(string)return text;std::ostringstream s;s<<value;return s.str();}
};
enum {TYPE_U32};
struct LLControlVariable {LLSD value; LLSD get() const {return value;}int type(){return TYPE_U32;}};
struct Settings {
    std::map<std::string,LLControlVariable> values;
    std::map<std::string,std::function<void()>> listeners;
    F32 getF32(const std::string& key){return static_cast<F32>(values[key].value.value);}
    U32 getU32(const std::string& key){return static_cast<U32>(values[key].value.value);}
    bool getBOOL(const std::string& key){return getU32(key)!=0;}
    void setF32(const std::string& key,F32 value){bool changed=values[key].value.value!=value;values[key].value=LLSD(value);if(changed&&listeners.count(key))listeners[key]();}
    void setU32(const std::string& key,U32 value){values[key].value=LLSD(static_cast<double>(value));}
    LLControlVariable* getControl(const std::string& key){return &values[key];}
} gSavedSettings;
#include "alsensorsettings.h"
struct LLView {bool enabled=true;};
struct LLTextBox {std::string text;};
struct LLComboBox {
    std::vector<std::string> values; std::string selected;
    bool findItemByValue(const std::string& value){return std::find(values.begin(),values.end(),value)!=values.end();}
    void add(const std::string&,const LLSD& value){values.push_back(value.asString());}
};
void setTextIfChanged(LLTextBox* c,const std::string& s){c->text=s;}
void setEnabledIfChanged(LLView* c,bool value){c->enabled=value;}
void setValueIfChanged(LLComboBox* c,const LLSD& value){c->selected=value.asString();}
bool gCubeSnapshot=false,sPrismLensRender=false,sPrismPreviewRender=false,sImpostorRender=false,sRenderingHUDs=false;
struct LLPipeline {
    bool hdr=true,exposure=true,mSensorFailed=false;
    int mMainRT=0;int* mRT=&mMainRT;
    bool worldHDREnabled() const{return hdr;}bool colorCorrectWillApplyExposure(bool) const{return exposure;}
    const char* sensorUnavailableReason() const;
} gPipeline;
struct ALKitHistory {static bool replay;static bool replaying(){return replay;}};
bool ALKitHistory::replay=false;
bool alDiopterIsPresetMaterializing(){return false;}
template<class F> void setting_setup_signal_listener(Settings& s,const std::string& key,F cb){s.listeners[key]=cb;}
struct ALPanelExposurePage {
    LLTextBox readout,status; LLComboBox iso; LLView seed,custom;
    LLTextBox* mSensorReadout=&readout; LLTextBox* mSensorStatus=&status;
    LLComboBox* mSensorISO=&iso; LLView* mSensorSeedRow=&seed; LLView* mCustomSensorCard=&custom;
    LLSD mSensorStoredISO,mSensorStoredSeed;
    bool mSensorDirty=true;
    bool mSensorEnabled=false;std::string mSensorReadoutText;
    bool isInVisibleChain() const{return true;}
    void updateSensor();void showSensorReadout();void updateSensorStatus();void updateSensorGating();void syncSensorISO();
};
struct LLTimer {void reset(){} double getElapsedTimeF64(){return 0;}};
struct LLStringUtil {static void toLower(std::string& s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));}};
struct LLDir {enum {XUI};std::string findSkinnedFilename(int,const std::string& s){return s;}} dir;
LLDir* gDirUtilp=&dir;using llifstream=std::ifstream;
#define LL_WARNS(x) std::cerr
#define LL_INFOS(x) std::cerr
#define LL_ENDL std::endl
struct ALUIPerfSelfTest {static bool isActive(){return false;}};
'''
    main = r'''
int main(int argc,char** argv){
    tut::sensor_model_object unit;
    unit.test<1>();unit.test<2>();unit.test<3>();unit.test<4>();unit.test<5>();
    unit.test<6>();unit.test<7>();unit.test<8>();unit.test<9>();
    for(const auto seed:{0u,1u,16777217u,2147483648u,4294967295u}){
        const auto text=std::to_string(seed);assert(sensorSeedTextValid(text));
        const U32 written=llsdToU32(LLSD(text));assert(written==seed);
        LLControlVariable control;
        const auto shown=unsignedControlValue(&control,LLSD(static_cast<S32>(written)));
        assert(shown.value==static_cast<double>(seed));
        assert(sensorSeedText(shown)==text);
    }
    for(const auto& invalid:{"","-1","1.5","1e3","4294967296","99999999999","10abc"})
        assert(!sensorSeedTextValid(invalid));
    using namespace ALSensorModel;
    gSavedSettings.setF32("CineSensorCustomBaseISO",250.f);
    gSavedSettings.setF32("CineSensorCustomSwitchISO",1600.f);
    gSavedSettings.setF32("CineSensorCustomHeadroom",4.25f);
    for(const auto& mode:{"LCG","HCG"}){
        std::string prefix=std::string("CineSensorCustom")+mode;
        gSavedSettings.setF32(prefix+"FullWell",12000.f);
        gSavedSettings.setF32(prefix+"ReadNoise",1.7f);
        gSavedSettings.setF32(prefix+"AnalogBase",.4f);
        gSavedSettings.setF32(prefix+"Pedestal",256.f);
        gSavedSettings.setF32(prefix+"White",16383.f);
    }
    ALPanelExposurePage ui;
    for(const auto seed:{0u,1u,16777217u,2147483648u,4294967295u}){
        gSavedSettings.setU32("CineSensorSeed",seed);ui.updateSensor();
        assert(ui.mSensorStoredSeed.asString()==std::to_string(seed));
    }
    for(U32 index=0;index<4;++index){
        gSavedSettings.setF32("CineSensorPreset",static_cast<F32>((index+1)%4));
        gSavedSettings.setF32("CineSensorISO",12345.f);
        gSavedSettings.setF32("CineSensorPreset",static_cast<F32>(index));
        const auto preset=ALSensorSettings::preset();
        assert(gSavedSettings.getF32("CineSensorISO")==preset.baseISO);
        ALKitHistory::replay=true;
        gSavedSettings.setF32("CineSensorISO",12345.f);
        gSavedSettings.setF32("CineSensorPreset",static_cast<F32>((index+1)%4));
        gSavedSettings.setF32("CineSensorPreset",static_cast<F32>(index));
        assert(gSavedSettings.getF32("CineSensorISO")==12345.f);
        ui.syncSensorISO();ui.updateSensor();ui.showSensorReadout();assert(ui.iso.selected=="12345");
        const auto count=ui.iso.values.size();ui.syncSensorISO();assert(ui.iso.values.size()==count);
        ALKitHistory::replay=false;
        for(double iso:{100.,800.,3200.,25600.})for(U32 bright:{0u,1u})for(U32 fixed:{0u,1u}){
            gSavedSettings.setF32("CineSensorISO",static_cast<F32>(iso));
            gSavedSettings.setF32("CineSensorBrightnessMode",static_cast<F32>(bright));
            gSavedSettings.setF32("CineSensorSeedFixed",static_cast<F32>(fixed));
            ui.syncSensorISO();ui.updateSensor();ui.showSensorReadout();
            const auto p=derive(preset,iso,bright?BrightnessMode::Brightens:BrightnessMode::Neutral);
            assert(ui.readout.text.substr(0,3)==(p.mode==GainMode::HCG?"HCG":"LCG"));
            const auto head=ui.readout.text.find("Headroom: ")+10;
            const auto floor=ui.readout.text.find("Noise floor: ")+13;
            assert(std::abs(std::stod(ui.readout.text.substr(head))-p.headroomStops)<=.00501);
            assert(std::abs(std::stod(ui.readout.text.substr(floor))-p.noiseFloorStops)<=.00501);
            assert(ui.seed.enabled==(fixed!=0));assert(ui.custom.enabled==(index==3));
            assert(ui.iso.selected==gSavedSettings.getControl("CineSensorISO")->get().asString());
        }
    }
    gSavedSettings.setF32("CineSensorPhysicalOrderEnabled",1.f);
    gSavedSettings.setF32("CineSensorNoiseEnabled",1.f);
    gPipeline.hdr=false;ui.updateSensorStatus();assert(ui.status.text.find("not HDR")!=std::string::npos);
    gPipeline.hdr=true;gPipeline.exposure=false;ui.updateSensorStatus();assert(ui.status.text.find("bypasses exposure")!=std::string::npos);
    gPipeline.exposure=true;gPipeline.mSensorFailed=true;ui.updateSensorStatus();assert(ui.status.text.find("unsupported")!=std::string::npos);
    gPipeline.mSensorFailed=false;
    for(bool* excluded:{&gCubeSnapshot,&sPrismLensRender,&sPrismPreviewRender,&sImpostorRender,&sRenderingHUDs}){
        *excluded=true;ui.updateSensorStatus();assert(ui.status.text.find("capture context")!=std::string::npos);*excluded=false;
    }
    gPipeline.mRT=nullptr;ui.updateSensorStatus();assert(ui.status.text.find("capture context")!=std::string::npos);
    gPipeline.mRT=&gPipeline.mMainRT;
    assert(std::string(gPipeline.sensorUnavailableReason()).empty());
    ui.updateSensorStatus();assert(!ui.status.text.empty());
    ALKitSearch search;assert(argc==2 && search.load(argv[1]));while(search.tokenizeBatch()){}
    for(const auto& keyword:{"iso","sensor","noise","grain","gain","dual native","headroom","read noise","camera"}){
        const auto hits=search.query(keyword);bool found=false;
        for(const auto& hit:hits)if(hit.page_key=="light_group/exposure_tab" && hit.card_id=="sensor")found=true;
        assert(found);
    }
    std::cout<<"PASS: 4 presets x 4 ISOs x 2 modes; model readout, preset reset, replay, seed/custom enable, stored ISO, status, 9 production search queries\n";
}
'''
    declarations = 'namespace ALDirectorQol {\n' + enum + r'''
S32 fuzzyTokenScore(const std::string&,const std::string&);
S32 searchTokenScore(const std::string&,const std::string&,SearchField);
bool searchResultBefore(S32,bool,U32,S32,bool,U32);
}
'''
    source = out / 'director_native.cpp'
    unit_source = (view / 'tests/alsensormodel_test.cpp').read_text(encoding='utf-8')
    unit_source = re.sub(r'^#include "[^"]+"\s*$', '', unit_source, flags=re.M)
    tut = r'''
namespace tut {
template<class Data> struct test_group {
    struct object {template<int Number> void test();};
    explicit test_group(const char*){}
};
void ensure(const char*,bool value){REQUIRE(value);}
template<class A,class B> void ensure_equals(const char*,const A& a,const B& b){REQUIRE(a==b);}
}
'''
    main = main.replace('    tut::sensor_model_object unit;', '    settings_setup_listeners();\n    tut::sensor_model_object unit;')
    native_source = '\n'.join((prefix, 'void settings_setup_listeners(){' + listener + '}', seed_validation, seed_conversion, pipeline_status, methods, declarations,
                               helpers, search_header, search, tut, unit_source, main))
    source.write_text(native_source, encoding='utf-8')
    candidates = sorted((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    assert candidates, 'MSVC required for native sensor UI seam checks'
    exe = out / 'director_native.exe'
    command = out / 'build.cmd'
    command.write_text('@echo off\ncall "' + str(candidates[-1]) + '" >nul 2>&1\n' +
        'cl /nologo /std:c++17 /EHsc /O2 /I"' + str(out) + '" /I"' + str(view) +
        '" "' + str(view / 'alsensormodel.cpp') + '" "' + str(source) + '" /Fe:"' + str(exe) +
        '" /Fo"' + out.as_posix() + '/"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(command)], cwd=ROOT,
                            capture_output=True, text=True, timeout=90)
    (out / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    no_sensor = out / 'index_without_sensor.xml'
    index = (XUI / 'director_kit_index.xml').read_text(encoding='utf-8')
    no_sensor.write_text('\n'.join(line for line in index.splitlines()
                                   if not ('\texposure_tab\t' in line and '\tsensor\tSensor\t' in line)), encoding='utf-8')
    result = subprocess.run([str(exe), str(no_sensor)], cwd=ROOT,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 2, 'search must reject an index without the Sensor card'
    print('PASS: rejected missing-Sensor production search index')
    result = subprocess.run([str(exe), str(XUI / 'director_kit_index.xml')], cwd=ROOT,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip())
    print('PASS: all 9 production sensor-model unit cases executed with a standalone TUT adapter')
    mutations = {
        'readout_model': ('ALSensorModel::readout(preset, iso, mode)', 'ALSensorModel::readout(preset, 50., mode)'),
        'seed_enable': ('setEnabledIfChanged(mSensorSeedRow, gSavedSettings.getBOOL("CineSensorSeedFixed"))',
                        'setEnabledIfChanged(mSensorSeedRow, !gSavedSettings.getBOOL("CineSensorSeedFixed"))'),
        'custom_enable': ('setEnabledIfChanged(mCustomSensorCard, gSavedSettings.getU32("CineSensorPreset") == 3)',
                          'setEnabledIfChanged(mCustomSensorCard, gSavedSettings.getU32("CineSensorPreset") == 2)'),
        'preset_iso': ('static_cast<F32>(ALSensorSettings::preset().baseISO)',
                       'static_cast<F32>(ALSensorSettings::preset().baseISO * 2.)'),
        'hdr_status': ('if (!worldHDREnabled())', 'if (false)'),
        'seed_precision': ('return std::to_string(static_cast<U32>(value.asReal()));',
                           'return value.asString();'),
    }
    for name, (original, replacement) in mutations.items():
        assert native_source.count(original) == 1, name + ': mutation seam drift'
        source.write_text(native_source.replace(original, replacement), encoding='utf-8')
        result = subprocess.run(['cmd', '/d', '/c', str(command)], cwd=ROOT,
                                capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, name + ': a compile failure does not kill a mutation'
        result = subprocess.run([str(exe), str(XUI / 'director_kit_index.xml')], cwd=ROOT,
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 2, name + ': mutation escaped the native predicate'
        print('PASS: rejected production mutation ' + name)
    # Restore the reviewable fixture and binary to the passing production source.
    source.write_text(native_source, encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(command)], cwd=ROOT,
                            capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr


if __name__ == '__main__':
    try:
        check()
        static_mutations()
        native()
        import sensor_director_restore_check
        sensor_director_restore_check.check()
    except (AssertionError, StopIteration) as exc:
        print('FAIL:', exc)
        sys.exit(1)
