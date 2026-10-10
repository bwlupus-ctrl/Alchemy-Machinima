"""Adversarial fixtures for the WP3 gate relaxations (kit_lint, director_parity).

Each fixture copies the REAL generated tree into a temp dir, applies one bad mutation that the
spec says the gate must catch, and reports whether the gate catches it.  Nothing in the repo is
modified.  Run: python -I fixtures.py
"""
import importlib.util, re, shutil, sys, tempfile
from pathlib import Path
R = Path('I:/alchemy-machinima')
CT = R / 'scripts/code_tools'
XUI = R / 'indra/newview/skins/default/xui/en'
KIT = CT / 'director_kit'
sys.path.insert(0, str(CT))

def load(name):
    spec = importlib.util.spec_from_file_location(name, CT / f'{name}.py')
    mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod; spec.loader.exec_module(mod)
    return mod

kl = load('kit_lint')
dp = load('director_parity')

def tree_copy():
    d = Path(tempfile.mkdtemp(prefix='wp3r1_'))
    for p in list(XUI.glob('panel_dpage_*.xml')) + [XUI / 'floater_director_v2.xml', XUI / 'director_kit_index.xml']:
        shutil.copy(p, d / p.name)
    return d

def edit(d, fn, old, new, count=1):
    p = d / fn
    s = p.read_text(encoding='utf-8')
    assert old in s, (fn, old[:80])
    p.write_text(s.replace(old, new, count), encoding='utf-8')

def lint(d):
    rep = kl.lint(kl.Inputs(d, KIT, kl.SETTINGS_DIR, kl.PAGE_SOURCES))
    errs = [f for f in rep.findings if f.severity == 'error']
    warns = [f for f in rep.findings if f.severity == 'warn']
    return errs, warns

results = []
def record(fid, rule, caught, detail):
    results.append((fid, rule, 'CAUGHT' if caught else 'MISSED', detail))

# ---- baseline
d = tree_copy(); e, w = lint(d)
base_warn = len(w)
record('K0', 'baseline real tree', not e, f'{len(e)} errors / {len(w)} warns (expected 0/4)')

# K1 rule 4 widget-name: two kit-created switches with the same name (the real tree already has two:
#    cine_enabled master + rig_status card enable; cine_rig_rim_enable master + rig_rim card enable)
d = tree_copy()
edit(d, 'panel_dpage_light_cine_light_rig.xml', '<kit_card name="kc_rig_main" ', '<kit_card name="kc_rig_main" enable_widget="cine_catchlight" ')
e, w = lint(d)
record('K1', '6.1 rule 4 unique names (kit-created enable/master switches)',
       any(f.rule == 'widget-name' for f in e), f'{len(e)} errors after giving kc_rig_main enable_widget=cine_catchlight (already rig_catchlight\'s switch); real tree already ships cine_enabled x2 and cine_rig_rim_enable x2 with 0 errors')

# K2 rule 9: same-page second writer (unhomed setting) with drifting range, in an Advanced-only card
d = tree_copy()
row = ('<kit_row bind="setting" kind="slider" label="Probe radius 2" max_val="999" min_val="0" '
       'setting="CineLightRigLiveProbeRadius" widget="probe_dup_radius"><slider name="probe_dup_radius" '
       'control_name="CineLightRigLiveProbeRadius" max_val="999" min_val="0" show_text="true" text_width="52" /></kit_row>')
edit(d, 'panel_dpage_light_cine_light_rig.xml', '<kit_card name="kc_rig_main" id="rig_main" kind="rows" title="Rig master">',
     '<kit_card name="kc_rig_main" id="rig_main" kind="rows" title="Rig master">' + row)
e, w = lint(d)
record('K2', '6.1 rule 9 second writer w/o shortcut (same page, Advanced card, range 0..999 vs 0.5..32)',
       any(f.rule == 'setting-home' for f in e), f'{len(e)} errors')

# K3 rule 9: a code-bound row that writes a homed setting on a non-home page, no shortcut_of
d = tree_copy()
row = ('<kit_row bind="code" kind="slider" label="Beam edge" setting="BDMergeProjectorVolumetricsFeather" '
       'widget="bloom_feather_dup"><slider name="bloom_feather_dup" show_text="true" text_width="52" /></kit_row>')
edit(d, 'panel_dpage_look_bloom_glow.xml', '<kit_card name="kc_bloom" id="bloom" kind="rows" title="HDR bloom">',
     '<kit_card name="kc_bloom" id="bloom" kind="rows" title="HDR bloom">' + row)
e, w = lint(d)
record('K3', '6.1 rule 9 code-bound writer of a homed setting (Atmosphere home) on Bloom',
       any(f.rule == 'setting-home' for f in e), f'{len(e)} errors (code rows are not modelled as writers; the real cine_easy_cone_feather shortcut is never checked either)')

# K4 rule 9: shortcut placed in an Advanced-only card (must be Easy card / selector block)
d = tree_copy()
src = (XUI / 'panel_dpage_look_bloom_glow.xml').read_text(encoding='utf-8')
m = re.search(r'(<kit_row [^>]*widget="set_RenderBloomStrength">.*?</kit_row>)', src, re.S)
home_row = m.group(1)
sc = home_row.replace('widget="set_RenderBloomStrength"', 'shortcut_of="bloom_glow_tab/set_RenderBloomStrength" widget="rig_bloom_sc"', 1)
sc = sc.replace('name="set_RenderBloomStrength"', 'name="rig_bloom_sc"')
edit(d, 'panel_dpage_light_cine_light_rig.xml', '<kit_card name="kc_rig_main" id="rig_main" kind="rows" title="Rig master">',
     '<kit_card name="kc_rig_main" id="rig_main" kind="rows" title="Rig master">' + sc)
e, w = lint(d)
record('K4', '6.1 rule 9 shortcut outside Easy card (rig_main is Advanced-mode only)',
       any('outside an Easy card' in f.message for f in e), f'{len(e)} errors; any card listed in ANY kit_page.mode counts as "Easy"')

# K5 rule 9: pending home suppresses the shortcut drift check entirely
d = tree_copy()
edit(d, 'panel_dpage_light_cine_light_rig.xml',
     'max_val="64" min_val="0" reset_settings="BDMergeProjectorVolumetricsMaxDistance"',
     'max_val="99" min_val="0" reset_settings="BDMergeProjectorVolumetricsMaxDistance"')
edit(d, 'panel_dpage_light_cine_light_rig.xml', 'increment="0.1" max_val="64" min_val="0" show_text="true" tool_tip="Volumetric shaft',
     'increment="0.1" max_val="99" min_val="0" show_text="true" tool_tip="Volumetric shaft')
e, w = lint(d)
record('K5', '6.1 rule 9 shortcut range drift (cine_easy_shaft_length 0..99 vs home 0..64)',
       bool(e), f'{len(e)} errors, {len(w)} warns (pending_homes skips the comparison; data home row is available)')

# K6 rule 5: code-bound row type no longer checked
d = tree_copy()
row = ('<kit_row bind="code" kind="toggle" label="Bad type" setting="RenderBloomStrength" widget="bad_type_toggle">'
       '<kit_switch name="bad_type_toggle" /></kit_row>')
edit(d, 'panel_dpage_look_bloom_glow.xml', '<kit_card name="kc_bloom" id="bloom" kind="rows" title="HDR bloom">',
     '<kit_card name="kc_bloom" id="bloom" kind="rows" title="HDR bloom">' + row)
e, w = lint(d)
record('K6', '6.1 rule 5 kind/type fit on a code-bound row (toggle over F32)',
       any(f.rule == 'setting-graph' for f in e), f'{len(e)} errors (also yields a second writer? {any(f.rule=="setting-home" for f in e)})')

# K7 rule 5: light_rig_map view_setting is never resolved
d = tree_copy()
edit(d, 'panel_dpage_light_cine_light_rig.xml', 'view_setting="DirectorV2LightMapView"', 'view_setting="NoSuchSettingXYZ"')
e, w = lint(d)
record('K7', '6.1 rule 5 settings graph for light_rig_map view_setting',
       any('NoSuchSettingXYZ' in f.message for f in e + w), f'{len(e)} errors; DirectorV2LightMapView is also absent from settings today with no finding')

# K8 kit_geometry/show_readout on generated dials (spec 3.3 / 3.13) - real tree
dials = re.findall(r'<compass_dial [^>]*>', (XUI / 'panel_dpage_light_cine_light_rig.xml').read_text(encoding='utf-8'))
ok = all('kit_geometry="true"' in x and 'show_readout="false"' in x for x in dials)
record('K8', '3.3/3.13 generated compass_dial carries kit_geometry=true show_readout=false',
       ok, f'{len(dials)} dials, {sum("kit_geometry" in x for x in dials)} with kit_geometry, {sum("show_readout" in x for x in dials)} with show_readout; kit_lint has no rule')

# ------------------------------------------------------------------ parity
def parity(xui_dir, gen_report=None, groups=['light_group'], rollout=False):
    paths = dp.Paths(xui_dir=xui_dir, gen_report=gen_report or dp.GEN_REPORT)
    return dp.evaluate(paths, None if rollout else groups, p0_scope=True, rollout_status=rollout)

d = tree_copy()
rep = parity(d)
record('P0', 'baseline light_group', rep['status'] == 'PASS', f"{rep['status']} {len(rep['findings'])} findings")

# P1 check 3 + glob allow: a C++-referenced label (getChild<LLTextBox>("cine_radius_label")) vanishes
d = tree_copy()
rp = d / 'gen_report.txt'
txt = dp.GEN_REPORT.read_text(encoding='utf-8')
txt = re.sub(r'(KITNAMES cine_light_rig_tab:.*?)\bcine_radius_label, ', r'\1', txt)
assert 'cine_radius_label' not in txt.split('KITNAMES cine_light_rig_tab:')[1].split('\n')[0]
rp.write_text(txt, encoding='utf-8')
rep = parity(d, rp)
allowed = [a for a in rep['allowed'] if a['subject'] == 'cine_radius_label']
record('P1', '6.2 check 3: C++ getChild<LLTextBox>("cine_radius_label") target missing from v2',
       any(f['subject'] == 'cine_radius_label' for f in rep['findings']),
       f"{rep['status']}; allowed as {[(a['check'], a['reason'][:40]) for a in allowed]} by the cine_*_label glob whose own reason says referenced ones must exist")

# P2 check 2: kit_lazy_page.master_setting (observer) satisfies the v1 control binding
d = tree_copy()
edit(d, 'panel_dpage_light_cine_light_rig.xml', 'master_setting="CineLightRigEnabled" ', '')
rep = parity(d)
record('P2', '6.2 check 2: Rig page no longer binds CineLightRigEnabled (only the floater kit_lazy_page pip observes it)',
       any(f['subject'] == 'CineLightRigEnabled' for f in rep['findings']), f"{rep['status']} {len(rep['findings'])} findings")

# P3 check 3 scope: a C++ target must exist on the one page owned by that C++ controller.
d = tree_copy()
edit(d, 'panel_dpage_light_rim.xml', 'name="cine_rimq_status"', 'name="cine_rimq_status_RENAMED"')
rep = parity(d)
record('P3', '5.1 "every name in its getChild list must exist on this page" (ALRigRimController::attach cine_rimq_status)',
       rep['status'] != 'PASS' and any(f['subject'] == 'cine_rimq_status' for f in rep['findings']),
       f"removed cine_rimq_status from Rim page; parity {rep['status']}")

# P4 check 4: v1 combo without control_name -> option values never compared
items = [dp.ComboItem('Bone Lock', '1', 'v1.xml', 'cam_mode_combo')]
opts = [dp.OptionRef('Bone Lock', '0', 'v2.xml', 'CinematicCamMode')]
f4 = dp.check_options(items, opts, dp.Allow({}), [])
record('P4', '6.2 check 4: wrong option value when the v1 combo is code-populated by name (no control_name)',
       bool(f4), f'{len(f4)} findings for value 0 vs v1 1')

# P5 --all masks a regression on a non-P0 page
d = tree_copy()
edit(d, 'panel_dpage_cast_move.xml', 'name="btn_group_dissolve"', 'name="btn_group_dissolve_RENAMED"')
base = parity(tree_copy(), rollout=True)
rep = parity(d, rollout=True)
record('P5', '6.2 all-at-once: a legacy page loses a v1 widget (btn_group_dissolve)',
       len(rep['findings']) != len(base['findings']),
       f"--all findings {len(base['findings'])} -> {len(rep['findings'])}; the injected removal adds one")

for r in results:
    print(f'{r[0]:3} {r[2]:6} {r[1]}\n          {r[3]}')

