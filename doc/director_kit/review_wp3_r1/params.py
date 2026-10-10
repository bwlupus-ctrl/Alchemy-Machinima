"""Generated kit-tag attributes vs the WP1 C++ Params blocks."""
import re
from collections import defaultdict
from pathlib import Path
from xml.etree import ElementTree as ET
R = Path('I:/alchemy-machinima'); XUI = R / 'indra/newview/skins/default/xui/en'; NV = R / 'indra/newview'
BASE = {'name', 'label', 'follows', 'visible', 'enabled', 'tool_tip', 'width', 'height', 'left', 'top', 'layout',
        'mouse_opaque', 'background_visible', 'class', 'filename', 'help_topic', 'enabled_control', 'visibility_control'}

def params_of(header, cls_marker):
    t = (NV / header).read_text(encoding='utf-8')
    i = t.find(cls_marker)
    block = t[i:t.find('Params();', i)]
    names = set()
    for m in re.finditer(r'(?:Optional|Mandatory|Multiple)<[^>]+>\s*([\w,\s]+);', block):
        names |= {x.strip() for x in m.group(1).split(',') if x.strip()}
    return names

P = {
    'kit_row': params_of('alkitrow.h', 'class ALKitRow'),
    'kit_card': params_of('alkitcard.h', 'class ALKitCard'),
    'kit_option': params_of('alkitcard.h', 'class ALKitOption'),
    'kit_page': params_of('alkitpage.h', 'class ALKitPage'),
    'kit_lazy_page': params_of('alkitlazypage.h', 'class ALKitLazyPage'),
    'kit_shell': params_of('alkitshell.h', 'class ALKitShell'),
    'light_rig_map': params_of('allightrigmap.h', 'class ALLightRigMap'),
}
P['kit_page'] |= {'section', 'mode'}
P['kit_card'] |= {'summary'}
cd = (R / 'indra/llui/alcompassdial.h').read_text(encoding='utf-8')
P['compass_dial'] = set(re.findall(r'(\w+)\s*[,;]', cd[cd.find('struct Params'):cd.find('Params();')])) | {'value', 'control_name', 'initial_value', 'tab_stop', 'commit_callback'}
for k, v in P.items():
    print(k, sorted(v))
bad = defaultdict(set)
for p in sorted(XUI.glob('panel_dpage_*.xml')) + [XUI / 'floater_director_v2.xml']:
    for n in ET.parse(p).getroot().iter():
        if n.tag in P:
            for a in n.attrib:
                if a not in P[n.tag] and a not in BASE:
                    bad[(n.tag, a)].add(p.name)
for (tag, a), files in sorted(bad.items()):
    print('UNKNOWN PARAM', tag, a, sorted(files)[:3], len(files))
