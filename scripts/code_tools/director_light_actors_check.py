#!/usr/bin/env python3
"""Static Light Actors conversion contract (SPEC tests T7/T8 and value wiring)."""
from pathlib import Path
import re
import subprocess
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
XUI = ROOT / "indra/newview/skins/default/xui/en"
root = ET.parse(XUI / "panel_dpage_light_actors.xml").getroot()
page = next(root.iter("kit_page"))
rows = {r.get("widget"): r for r in root.iter("kit_row")}
assert root.get("class") == "panel_avatar_light_response"
assert len(rows) == 31
panel = (ROOT / "indra/newview/alpanelavatarlightresponse.cpp").read_text()
post = panel.split("bool ALPanelAvatarLightResponse::postBuild()", 1)[1].split("void ALPanelAvatarLightResponse::focusTarget", 1)[0]
lookups = re.findall(r'getChild<(\w+)>\("([^" ]+)"\)', post)
assert len(lookups) == 24
compatible = {"LLScrollListCtrl": "scroll_list", "LLCheckBoxCtrl": "kit_switch",
              "LLSliderCtrl": "slider", "LLColorSwatchCtrl": "color_swatch",
              "LLComboBox": "combo_box", "LLTextBox": "text", "LLButton": "button"}
for cls, name in lookups:
    matches = [n for n in root.iter() if n.get("name") == name]
    assert len(matches) == 1 and matches[0].tag == compatible[cls], (cls, name)
code_rows = "target_list chk_only_adjusted sld_diffuse sld_tame sld_brightness sld_glow chk_bypass sld_sil_amount combo_sil_fill swatch_sil_fill chk_sil_haze chk_sil_keep_rim".split()
for name in code_rows:
    row = rows[name]
    assert row.get("widget_history") == "false", name
    assert not any(key in row.attrib for key in ("default_value", "reset_settings", "setting")), name
assert all(not any(key.startswith("who_") for key in n.attrib) for n in root.iter())
assert page.get("preset_mode") == "widgets"
host = next(root.iter("kit_preset_host"))
assert [n.get("name") for n in host] == ["combo_preset", "btn_apply_preset"]
for name in ("txt_sil_note", "txt_budget"):
    assert rows[name].get("visible") == "false"
assert "am_test_combo" not in rows
assert rows["am_test_trigger"][0].get("control_name") is None
assert "CineLightRigActorMaskTest" not in page.get("watch_settings", "")
assert rows["chk_persist"].get("bulk_reset") == "false"
assert rows["chk_persist"].get("setting") == "AvatarLightResponsePersist"
model = (ROOT / "indra/newview/alavatarlightresponsemodel.h").read_text()
constants = dict(re.findall(r'constexpr F32 (\w+) = ([-\d.]+)f;', model))
for widget, lo, hi, scale, prefix in (
    ("sld_diffuse", 0, 100, 100, "DIFFUSE"), ("sld_tame", 0, 100, 100, "TAME"),
    ("sld_brightness", -2, 1, 1, "EV"), ("sld_glow", 0, 200, 100, "GLOW"),
    ("sld_sil_amount", 0, 100, 100, None), ("am_bias_slider", -64, 0, 1, None)):
    child = rows[widget][0]
    assert (float(child.get("min_val")), float(child.get("max_val"))) == (lo, hi), widget
    if prefix:
        assert (lo / scale, hi / scale) == (float(constants[prefix + "_MIN"]), float(constants[prefix + "_MAX"])), widget
settings = ET.parse(ROOT / "indra/newview/app_settings/settings.xml").getroot()[0]
setting_fields = {}
children = list(settings)
for i in range(0, len(children), 2):
    fields = list(children[i + 1])
    setting_fields[children[i].text] = {key.text: value.text for key, value in zip(fields[::2], fields[1::2])}
for widget, name, ty in (
    ("chk_persist", "AvatarLightResponsePersist", "Boolean"),
    ("chk_debug_tint", "AvatarLightResponseDebugTint", "Boolean"),
    ("am_disable_check", "CineLightRigActorMaskDisable", "Boolean"),
    ("am_debug_combo", "CineLightRigActorMaskDebug", "U32"),
    ("am_bias_slider", "CineLightRigActorMaskBias", "F32")):
    assert rows[widget].get("setting") == name and setting_fields[name]["Type"] == ty
assert setting_fields["AvatarLightResponseEnabled"]["Type"] == "Boolean"
assert setting_fields["AvatarLightResponsePersist"]["Value"] == "0"
for name, values in (("combo_sil_fill", range(6)), ("combo_preset", range(9)), ("am_debug_combo", range(3))):
    child = next(n for n in root.iter("combo_box") if n.get("name") == name)
    assert [int(n.get("value")) for n in child if n.tag == "combo_box.item"] == list(values), name
old_rig = subprocess.check_output(["git", "show", "HEAD:indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml"], cwd=ROOT)
new_rig = (XUI / "panel_dpage_light_cine_light_rig.xml").read_bytes()
assert old_rig.splitlines()[2:] == new_rig.splitlines()[2:], "Light Rig changed beyond generated hash"
old_panel = subprocess.check_output(["git", "show", "HEAD:indra/newview/skins/default/xui/en/panel_avatar_light_response.xml"], cwd=ROOT)
assert ET.tostring(ET.fromstring(old_panel)) == ET.tostring(ET.parse(XUI / "panel_avatar_light_response.xml").getroot()), "standalone/v1 panel XML changed"
print("PASS: 24 compatible lookups; 12 unresettable/no-history rows; 31 rows; preset order; conditional rows; preference reset exclusion; types/ranges/combo values; Rig unchanged except header")
