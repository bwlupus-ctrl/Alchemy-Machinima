"""D56: execute production Key status/tint; reject ReShade UI or refusal consumers."""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET
from native_check_tools import compile_run, check_directory
from relight_area_policy_check import method

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / 'indra/newview/alpanelactorkey.cpp'
source = path.read_text(encoding='utf-8')
refresh = method(path, 'void ALPanelActorKey::refreshReadouts()')
# Execute the actual status/tint expressions before the widget write, with the
# model and overlay inputs instrumented. No duplicated policy implementation.
body = refresh[refresh.index('{')+1:refresh.index('    setTextIfChanged(mReadouts[0], status);')]
fixture = r'''
#include <string>
#include <cassert>
using U32=unsigned;
struct LLColor4 {float r,g,b,a;static const LLColor4 white;};
const LLColor4 LLColor4::white{1,1,1,1};
bool reshadePresent=false,effectsEnabled=true;
struct ALActorChromaKey {
 inline static bool sActive=false,keyed=false;
 inline static U32 selectedMode=0;
 inline static std::string why;
 static bool keyedThisFrame(){return keyed;}static U32 mode(){return selectedMode;}
 static const std::string& reason(){return why;}
};
std::string warnings="No capture overlays enabled.";
std::string captureWarnings(){return warnings;}
''' + method(path, 'const char* modeName(') + r'''
struct Readout{std::string status;LLColor4 tint;};
Readout readStatus(){
''' + body + r'''
 return {status,tint};
}
int main(){using K=ALActorChromaKey;
 for(bool present:{false,true}){reshadePresent=present;
  K::sActive=false;auto off=readStatus();assert(off.status=="Off (off at every login)"&&off.tint.r==1);
  K::sActive=true;K::keyed=true;
  for(unsigned m=0;m<3;++m){K::selectedMode=m;auto on=readStatus();
   assert(on.status==std::string("Keying: ")+modeName(m));assert(on.tint.g==1&&on.tint.r==.2f);}
  K::keyed=false;K::why.clear();assert(readStatus().status=="Arming");
  for(const char* why:{"RLVa sphere","diopter debug view","matte unavailable","program unavailable"}){
   K::why=why;auto suspended=readStatus();assert(suspended.status==std::string("Suspended: ")+why);
   assert(suspended.tint.g==.7f);}
  assert(effectsEnabled);
 }
}
'''
result = compile_run(check_directory('actor_key_ui'), fixture)
assert result.returncode == 0, result.stdout + result.stderr

def no_reshade_ui(text):
    assert not re.search(r'reshade|keyGuard|readGuard|ak_retry|ak_guarantee', text, re.I)

no_reshade_ui(source)
for mutation in ('\nLLReShadeBridge::instance().keyStatus();', '\n// ReShade refusal', '\nak_retry'):
    try:
        no_reshade_ui(source + mutation)
    except AssertionError:
        pass
    else:
        raise AssertionError('ReShade UI mutation escaped')
data = json.loads((ROOT/'scripts/code_tools/director_kit/director_mockup_data.json').read_text(encoding='utf-8'))
def pages(x):
    if isinstance(x,dict):
        if x.get('name')=='key_tab': yield x
        for v in x.values(): yield from pages(v)
    elif isinstance(x,list):
        for v in x: yield from pages(v)
key, = pages(data)
no_reshade_ui(json.dumps(key))
no_reshade_ui((ROOT/'indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml').read_text(encoding='utf-8'))
readouts = re.search(r'const char\* readouts\[\] = \{([^}]+)', source).group(1)
names = re.findall(r'"([^"]+)"', readouts)
assert names == ['ak_status','ak_codes','ak_count','ak_sstatus','ak_warn','ak_result','ak_limits']
header=path.with_suffix('.h').read_text(encoding='utf-8')
assert f'std::array<LLTextBox*, {len(names)}> mReadouts' in header
xml=ET.parse(ROOT/'indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml')
for name in names: assert xml.find(f'.//*[@name="{name}"]') is not None,name
print('PASS D56 production status/tint with ReShade absent/present, effects ON: off/keying/arming/non-ReShade suspension; no guard UI, exact readout census; restored UI consumer mutations rejected')
