import importlib.util, sys
from pathlib import Path
R = Path('I:/alchemy-machinima')
spec = importlib.util.spec_from_file_location('xlc', R / 'scripts/code_tools/xui_layout_check.py')
xlc = importlib.util.module_from_spec(spec); sys.modules['xlc'] = xlc; spec.loader.exec_module(xlc)
XUI = R / 'indra/newview/skins/default/xui/en'
si = xlc.SourceIndex(R)
si_flat = xlc.SourceIndex(R); si_flat.xui_parents = {}
n = 0
for p in sorted(XUI.glob('panel_dpage_*.xml')):
    if not xlc.is_legacy_page(p):
        continue
    a = {xlc.finding_key(p.name, f): f for f in xlc.check_file(p, si)}
    b = {xlc.finding_key(p.name, f): f for f in xlc.check_file(p, si_flat)}
    for k in sorted(set(b) - set(a)):
        n += 1
        print('CLEARED-BY-TRANSITIVE', k, '|', b[k].message[:170])
print('total cleared by transitive owner walk:', n)
