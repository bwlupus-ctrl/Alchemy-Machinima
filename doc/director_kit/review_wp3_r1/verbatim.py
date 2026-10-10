"""Independent verbatim check: re-parse every generated legacy page FROM DISK and compare its
kit_page children with the v1 leaf children, canonically (attrs exact, comments, exact text
for leaf elements, child order)."""
import json
from pathlib import Path
from xml.etree import ElementTree as ET
R = Path('I:/alchemy-machinima')
XUI = R / 'indra/newview/skins/default/xui/en'

def parse(p):
    return ET.parse(p, parser=ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))).getroot()

def canon(n, strict_text):
    tag = '#c' if n.tag is ET.Comment else n.tag
    txt = n.text or ''
    if len(n) or not strict_text:
        txt = txt.strip()
    return (tag, tuple(sorted(n.attrib.items())), txt, tuple(canon(c, strict_text) for c in n))

v1 = parse(XUI / 'floater_director.xml')
leaves = {}
dups = {}
for n in v1.iter('panel'):
    nm = n.get('name')
    if nm:
        if nm in leaves:
            dups[nm] = dups.get(nm, 1) + 1
        leaves[nm] = n
print('duplicate panel names in v1 floater:', dups)
bad = 0; n_pages = 0
for p in sorted(XUI.glob('panel_dpage_*.xml')):
    root = parse(p)
    kp = next(root.iter('kit_page'))
    if kp.get('legacy') != 'true':
        continue
    n_pages += 1
    page = kp.get('page').split('/')[-1]
    leaf = leaves[page]
    for strict in (False, True):
        a = tuple(canon(c, strict) for c in leaf)
        b = tuple(canon(c, strict) for c in kp)
        if a != b:
            bad += 1
            print('MISMATCH', p.name, 'strict_text' if strict else 'canonical')
    if kp.get('width') != leaf.get('width') or kp.get('height') != leaf.get('height'):
        print('SIZE', p.name, kp.get('width'), kp.get('height'), 'leaf', leaf.get('width'), leaf.get('height'))
    if leaf.get('width') is None or leaf.get('height') is None:
        print('LEAF HAS NO SIZE (generator defaulted 711x525):', page)
print('legacy pages checked:', n_pages, 'mismatches:', bad)
