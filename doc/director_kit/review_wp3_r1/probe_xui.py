"""Adversarial probe: does xui_layout_check actually lay out generated legacy pages?

1. Take the real generated panel_dpage_cast_move.xml (outer <panel> wrapper, no size).
2. Inject a blatant overlap/clip: a button at left=0 top=0 width=5000 inside the kit_page.
3. Run check_file on (a) the as-generated wrapper form and (b) the same kit_page as the root.
"""
import importlib.util, re, sys, tempfile
from pathlib import Path
R = Path('I:/alchemy-machinima')
spec = importlib.util.spec_from_file_location('xlc', R / 'scripts/code_tools/xui_layout_check.py')
xlc = importlib.util.module_from_spec(spec); sys.modules['xlc'] = xlc; spec.loader.exec_module(xlc)
XUI = R / 'indra/newview/skins/default/xui/en'
tmp = Path(tempfile.mkdtemp())

def run(name, text):
    p = tmp / name
    p.write_text(text, encoding='utf-8')
    f = xlc.check_file(p)
    return len(f), [x.kind for x in f][:6]

src = (XUI / 'panel_dpage_cast_move.xml').read_text(encoding='utf-8')
bad = '<button name="probe_huge" label="X" left="0" top="0" width="5000" height="9000" follows="left|top" layout="topleft"/>'
# insert as first child of kit_page
injected = re.sub(r'(<kit_page [^>]*>)', r'\1' + bad, src, count=1)
print('A generated wrapper, clean      :', run('panel_dpage_a.xml', src))
print('B generated wrapper + injected  :', run('panel_dpage_b.xml', injected))
# same but with kit_page as root (spec 4.3 example form)
inner = re.search(r'(<kit_page .*</kit_page>)', injected, re.S).group(1)
print('C kit_page as root  + injected  :', run('panel_dpage_c.xml', '<?xml version="1.0" encoding="utf-8"?>\n' + inner))
inner_clean = re.search(r'(<kit_page .*</kit_page>)', src, re.S).group(1)
print('D kit_page as root, clean       :', run('panel_dpage_d.xml', '<?xml version="1.0" encoding="utf-8"?>\n' + inner_clean))

# Now: every legacy page, wrapper form vs kit_page-as-root form
tot_wrap = tot_root = 0
per = []
for p in sorted(XUI.glob('panel_dpage_*.xml')):
    s = p.read_text(encoding='utf-8')
    if 'legacy="true"' not in s:
        continue
    a = len(xlc.check_file(p))
    inner = re.search(r'(<kit_page .*</kit_page>)', s, re.S).group(1)
    q = tmp / ('root_' + p.name)
    q.write_text('<?xml version="1.0" encoding="utf-8"?>\n' + inner, encoding='utf-8')
    b = xlc.check_file(q)
    tot_wrap += a; tot_root += len(b)
    if b:
        per.append((p.name, len(b), sorted({x.kind for x in b})))
print('legacy pages: findings as generated =', tot_wrap, '; with kit_page as layout root =', tot_root)
for x in per:
    print('   ', x)
# v1 floater findings for comparison
v1 = xlc.check_file(XUI / 'floater_director.xml')
print('floater_director.xml (v1) findings:', len(v1))
