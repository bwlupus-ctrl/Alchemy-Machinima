"""R1-verification fixtures for the relaxations introduced while fixing R1.

X*: xui_layout_check legacy baseline + transitive wiring.  Q*: parity per-page/pending.  L*: lint policy.
Runs on temp copies; nothing in the repo changes.  Run with: python -I fixtures_r1v.py (cwd = repo root).
"""
import importlib.util, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path
R = Path('I:/alchemy-machinima'); CT = R / 'scripts/code_tools'
XUI = R / 'indra/newview/skins/default/xui/en'; KIT = CT / 'director_kit'
os.chdir(R)

def load(name):
    spec = importlib.util.spec_from_file_location(name, CT / f'{name}.py')
    mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod; spec.loader.exec_module(mod)
    return mod
kl = load('kit_lint'); dp = load('director_parity')
res = []
def rec(fid, what, caught, detail):
    res.append((fid, 'CAUGHT' if caught else 'MISSED', what, detail))

def tree_copy():
    d = Path(tempfile.mkdtemp(prefix='wp3r1v_'))
    for p in list(XUI.glob('panel_dpage_*.xml')) + [XUI / 'floater_director_v2.xml', XUI / 'director_kit_index.xml']:
        shutil.copy(p, d / p.name)
    return d

def edit(d, fn, old, new):
    p = d / fn; s = p.read_text(encoding='utf-8'); assert old in s, (fn, old[:90]); p.write_text(s.replace(old, new, 1), encoding='utf-8')

def xui(path):
    r = subprocess.run([sys.executable, '-I', str(CT / 'xui_layout_check.py'), str(path)], capture_output=True, text=True, cwd=R)
    return r.returncode, r.stdout.strip().splitlines()[-3:]

# X1 baselined page gains a NEW clipped child -> must fail
d = tree_copy()
edit(d, 'panel_dpage_performance_gaze.xml', '<kit_page ', '<kit_page ')  # sanity
s = (d / 'panel_dpage_performance_gaze.xml').read_text(encoding='utf-8')
s = re.sub(r'(<kit_page [^>]*>)', r'\1<button name="probe_new_clip" label="X" left="600" top="500" width="400" height="200" follows="left|top" layout="topleft"/>', s, count=1)
(d / 'panel_dpage_performance_gaze.xml').write_text(s, encoding='utf-8')
rc, out = xui(d / 'panel_dpage_performance_gaze.xml')
rec('X1', 'baselined legacy page gains a new clipped/overflowing child', rc == 1, f'rc={rc} {out}')

# X2 baselined subject grows (director_gaze_panel taller) -> masked by subject-only key (accepted? report)
d = tree_copy()
s = (d / 'panel_dpage_performance_gaze.xml').read_text(encoding='utf-8')
m = re.search(r'<panel [^>]*name="director_gaze_panel"[^>]*>', s) or re.search(r'<panel[^>]*name="director_gaze_panel"[^>]*/>', s)
tag = m.group(0); tag2 = re.sub(r'height="\d+"', 'height="9000"', tag) if 'height=' in tag else tag.replace('name="director_gaze_panel"', 'name="director_gaze_panel" height="9000"')
(d / 'panel_dpage_performance_gaze.xml').write_text(s.replace(tag, tag2, 1), encoding='utf-8')
rc, out = xui(d / 'panel_dpage_performance_gaze.xml')
rec('X2', 'baselined clipped subject grows to 9000 px (key is kind+subject only)', rc == 1, f'rc={rc} {out}')

# X3 legacy kit_page root loses its size -> must fail (invalid-root / not silently skipped)
d = tree_copy()
s = (d / 'panel_dpage_cast_move.xml').read_text(encoding='utf-8')
s2 = re.sub(r'(<kit_page [^>]*?) height="\d+"', r'\1', s, count=1); s2 = re.sub(r'(<kit_page [^>]*?) width="\d+"', r'\1', s2, count=1)
(d / 'panel_dpage_cast_move.xml').write_text(s2, encoding='utf-8')
rc, out = xui(d / 'panel_dpage_cast_move.xml')
rec('X3', 'legacy kit_page root without width/height', rc == 1, f'rc={rc} {out}')

# X4 transitive wiring is page_file-scoped: same content under a name no kit_lazy_page references
d = tree_copy()
shutil.copy(d / 'panel_dpage_camera_shot.xml', d / 'panel_orphan_shot_copy.xml')
rc, out = xui(d / 'panel_orphan_shot_copy.xml')
rec('X4', 'unreferenced copy of a legacy page (no kit_lazy_page owner) still reports unwired scroller', rc == 1, f'rc={rc} {out}')

# X5 injected 5000x9000 button into a legacy page (R1 B2 probe, re-run through CLI incl. baseline)
d = tree_copy()
s = (d / 'panel_dpage_cast_move.xml').read_text(encoding='utf-8')
s = re.sub(r'(<kit_page [^>]*>)', r'\1<button name="probe_huge" label="X" left="0" top="0" width="5000" height="9000" follows="left|top" layout="topleft"/>', s, count=1)
(d / 'panel_dpage_cast_move.xml').write_text(s, encoding='utf-8')
rc, out = xui(d / 'panel_dpage_cast_move.xml')
rec('X5', 'R1 B2 probe via CLI: 5000x9000 button on Move', rc == 1, f'rc={rc} {out}')

# Q1 parity: C++-owned name moved to the WRONG page (rim status onto the Rig page)
def parity(xd):
    return dp.evaluate(dp.Paths(xui_dir=xd), ['light_group'], p0_scope=True, rollout_status=False)
d = tree_copy()
edit(d, 'panel_dpage_light_rim.xml', 'name="cine_rimq_status"', 'name="cine_rimq_status_GONE"')
edit(d, 'panel_dpage_light_cine_light_rig.xml', '<kit_card name="kc_rig_main"',
     '<kit_card name="kc_rimq_misplaced" id="rimq_misplaced" kind="rows" title="x"><kit_row kind="readout" widget="cine_rimq_status" label="x" bind="code"><text name="cine_rimq_status"/></kit_row></kit_card><kit_card name="kc_rig_main"')
r = parity(d)
rec('Q1', 'ALRigRimController target moved from Rim page to Rig page', r['status'] == 'FAIL', f"{r['status']} {[f['subject'] for f in r['findings']][:4]} pending={[p['subject'] for p in r['pending']]}")

# Q2 parity: ALPanelCineLightRig target moved from Rig page to Rim page
d = tree_copy()
edit(d, 'panel_dpage_light_cine_light_rig.xml', 'name="cine_shadow_hint"', 'name="cine_shadow_hint_GONE"')
edit(d, 'panel_dpage_light_rim.xml', '<kit_card name="kc_rim_quality"',
     '<kit_card name="kc_misplaced" id="misplaced" kind="rows" title="x"><kit_row kind="readout" widget="cine_shadow_hint" label="x" bind="code"><text name="cine_shadow_hint"/></kit_row></kit_card><kit_card name="kc_rim_quality"')
r = parity(d)
rec('Q2', 'ALPanelCineLightRig target (cine_shadow_hint) moved to Rim page', r['status'] == 'FAIL', f"{r['status']} {[f['subject'] for f in r['findings']][:4]}")

# Q3 the WP4 pending record must not mask a missing name
d = tree_copy()
edit(d, 'panel_dpage_light_rim.xml', 'name="cine_rimq_enable_tag"', 'name="cine_rimq_enable_tag_GONE"')
r = parity(d)
rec('Q3', 'pending_cpp_targets name (cine_rimq_enable_tag) removed from Rim page', r['status'] == 'FAIL', f"{r['status']} {[f['subject'] for f in r['findings']][:4]}")

# L1 lint: stale pending_settings (WP2 landed; the three DirectorV2* settings now exist) are not reported
rep = kl.lint(kl.Inputs(XUI, KIT, kl.SETTINGS_DIR, kl.PAGE_SOURCES))
pol = __import__('json').loads((KIT / 'gate_policy.json').read_text(encoding='utf-8'))
types, _ = kl.load_settings_graph(kl.SETTINGS_DIR)
stale = [k for k in pol.get('pending_settings', {}) if k in types]
rec('L1', 'stale pending_settings entries (setting now exists) are flagged', not stale or any('stale' in f.message.lower() for f in rep.findings),
    f'stale entries still in gate_policy.json: {stale}')

for r in res:
    print(f'{r[0]:3} {r[1]:6} {r[2]}\n          {r[3]}')
