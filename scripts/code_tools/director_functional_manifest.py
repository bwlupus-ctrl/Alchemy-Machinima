#!/usr/bin/env python3
"""Director functional test manifest (read-only generator).

Builds the machine-readable test plan that a login-screen C++ self-test uses to
click every Director control (v1 and v2) and assert that it works.

Inputs (read only):
  doc/director_kit/wiring/wiring_table.csv   per page x control, ORIG/V1/V2 status
  indra/newview/app_settings/settings*.xml   setting type and default
  indra/newview/skins/default/xui/en/*.xml   widget tags, min/max/increment, items
  indra/newview/alrigrimcontroller.cpp       Rig Rim built-in preset table

Outputs:
  doc/director_kit/functional/functional_manifest.json
  doc/director_kit/functional/FUNCTIONAL_MANIFEST.md

Stdlib only. Run as:  python -I scripts/code_tools/director_functional_manifest.py
"""

import argparse
import collections
import csv
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, os.pardir, os.pardir))
CSV_PATH = os.path.join(REPO, "doc", "director_kit", "wiring", "wiring_table.csv")
XUI_DIR = os.path.join(REPO, "indra", "newview", "skins", "default", "xui", "en")
SETTINGS_DIR = os.path.join(REPO, "indra", "newview", "app_settings")
SETTINGS_FILES = ("settings.xml", "settings_alchemy.xml",
                  "settings_per_account.xml", "settings_per_account_alchemy.xml")
RIM_PRESET_CPP = os.path.join(REPO, "indra", "newview", "alrigrimcontroller.cpp")
OUT_DIR = os.path.join(REPO, "doc", "director_kit", "functional")
JSON_PATH = os.path.join(OUT_DIR, "functional_manifest.json")
MD_PATH = os.path.join(OUT_DIR, "FUNCTIONAL_MANIFEST.md")
VERSIONS = ("v1", "v2")

# The row host is explicit so the viewer never guesses from a page name.  The
# launcher records mirror LLFloaterDirector::bindPage launchers.
LAUNCHER_HOSTS = {
    "diopter_tab": ("ultimate_diopter", "", "diopter_tab", "card_diopter_open"),
    "practicals_tab": ("lightbox", "sellight_settings", "practicals_tab", "card_practicals_open"),
    "actors_tab": ("lightbox", "actor_mask_settings", "actors_tab", "card_actor_mask_open"),
    "light_cues_tab": ("cine_light_cues", "", "light_cues_tab", "card_light_cues_open"),
    "image_tab": ("lightbox", "colortone_settings", "image_tab", "card_image_open"),
    "bloom_glow_tab": ("lightbox", "glow_settings", "image_tab", "card_image_open"),
    "stylize_tab": ("lightbox", "rotoink_settings", "stylize_tab", "card_rotoink_open"),
    "tron_tab": ("lightbox", "tron_settings", "stylize_tab", "card_tron_open"),
    "atmosphere_tab": ("lightbox", "cinehaze_settings", "atmosphere_tab", "card_haze_open"),
    "shadows_probes_tab": ("lightbox", "shadow_settings", "shadows_probes_tab", "card_shadows_probes_open"),
}


def host_for(version, page):
    if version == "v1" and page == "light_rim_tab":
        return {"floater": "director", "page_view": "cine_light_rig_tab",
                "via": "tab", "alias_of": "light_rim_tab"}
    if page in LAUNCHER_HOSTS:
        floater, tab, launcher_page, widget = LAUNCHER_HOSTS[page]
        host = {"floater": floater, "via": "launcher",
                "launcher": {"floater": "director", "page_view": launcher_page,
                             "widget": widget}}
        if tab:
            host["tab"] = tab
        return host
    return {"floater": "director", "page_view": page, "via": "tab"}

# XUI tag -> widget type used for exercise selection.
CONTROL_TYPE = {
    "check_box": "toggle", "kit_switch": "toggle", "menu_item_check": "toggle",
    "slider": "slider", "spinner": "spinner", "compass_dial": "dial",
    "combo_box": "combo", "icons_combo_box": "combo", "radio_group": "radio",
    "scroll_list": "list", "button": "button", "icon_button": "button",
    "line_editor": "text", "text_editor": "text", "color_swatch": "color",
    "texture_picker": "gobo", "viewport": "viewport",
}
# CSV kind -> widget type (fallback when the XUI element is not found).
KIND_TO_TYPE = {
    "slider": "slider", "number": "spinner", "dial": "dial", "arc": "dial",
    "combo": "combo", "toggle": "toggle", "button": "button", "link": "button",
    "color": "color", "list": "list", "text": "text", "vec2": "vec2",
    "vec3": "vec3", "custom": "custom", "viewport": "viewport", "gobo": "gobo",
}
# widget type -> exercise verb.
EXERCISE = {
    "toggle": "toggle", "slider": "set_value", "spinner": "set_value",
    "dial": "set_value", "vec2": "set_value", "vec3": "set_value",
    "color": "set_value", "text": "set_value", "combo": "select_item",
    "radio": "select_item", "list": "select_item", "button": "click",
}
# Pages whose actions operate on an in-world avatar/object/selection.
WORLD_PAGES = {"move_tab", "path_tab", "props_tab", "ghosts_tab", "dance_tab",
               "animate_tab", "actors_tab", "gaze_tab", "polish_tab",
               "gaze_cues_tab", "moves_tab"}
# Token sets, matched after splitting names on _, camelCase and punctuation.
DESTRUCTIVE_TOKENS = {"clear", "delete", "remove", "dissolve", "wipe", "purge",
                      "erase", "discard", "forget", "save", "export", "upload",
                      "import", "browse", "snapshot", "render", "record",
                      "teleport", "publish", "send", "share", "rename", "kick",
                      "disband", "overwrite", "write", "resetall"}
FILE_TOKENS = {"file", "path", "folder", "url", "uuid", "dir", "directory"}
MODAL_TOKENS = {"popout", "open", "manage", "animexp", "lightbox", "picker",
                "library", "sellight", "cues"}
WORLD_TOKENS = {"selected", "pick", "target", "anchor", "play", "stop", "start",
                "capture", "fixit", "hero", "zolly", "ots", "walk", "run",
                "influence", "avatar", "inherit", "paste", "copy", "dance",
                "anim", "prop", "ghost", "group", "set", "refresh", "tag",
                "probe", "loco", "locomotion", "actor", "object", "selection"}
SELECTION_TOKENS = {"selected", "pick", "target", "anchor", "avatar", "actor",
                    "object", "selection"}

RIG_RIM_SENTINEL = "CineRigRimMasterGain"

# Deliberately non-interactive controls whose settings exist only as reserved
# schema.  Keep this narrow and versioned: dynamically disabled controls are
# still handled by explicit enable_when/visible_when prerequisites.
DECLARED_NON_INTERACTIVE = {
    ("v2", "temporal_tab", "drive_camera_check"):
        "reserved: camera deliberately stays real-time in this version",
}

# The temporal preset buttons are callback-owned rather than setting-bound.
# Give each real click an exact two-setting oracle and a deliberately different
# scale pre-state so even Realtime has to produce an observable transition.
TEMPORAL_PRESET_ORACLES = {
    ("v2", "temporal_tab", "btn_freeze"):   (0.0, 1, 1.0),
    ("v2", "temporal_tab", "btn_quarter"):  (0.25, 1, 1.0),
    ("v2", "temporal_tab", "btn_half"):     (0.5, 1, 1.0),
    ("v2", "temporal_tab", "btn_realtime"): (1.0, 0, 0.5),
    ("v2", "temporal_tab", "btn_double"):   (2.0, 1, 1.0),
}

# A one-increment target (1.01) is not pointer-reachable on the compact 0..8
# temporal slider: it rounds to the same pixel as 1.0.  The minimum is both a
# real drag target and an exact, layout-independent setting oracle.
POINTER_SLIDER_ENDPOINT_TESTS = {
    ("v2", "temporal_tab", "scale_slider"): 0.0,
}

# These two controls display whole-percent darkness but deliberately store the
# inverse 0..1 brightness floor (alpanelcinelightrig.cpp:964-995).
TRANSFORMED_VALUE_TESTS = {
    ("v2", "cine_light_rig_tab", "cine_easy_night_mask_darkness"): (1.0, 0.99),
    ("v2", "cine_light_rig_tab", "cine_night_mask_darkness"): (1.0, 0.99),
}


# ---------------------------------------------------------------- helpers

def tokens_of(*parts):
    out = set()
    for part in parts:
        if not part:
            continue
        s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", part)
        s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
        for t in re.split(r"[^a-z0-9]+", s.lower()):
            if t:
                out.add(t)
    return out


def destructive_hit(name, toks):
    """Return a short description of the destructive token matched, or None.

    Per-control reset buttons (reset_<control>) only count when they are a
    reset-all, so 'reset_gaze_chest_share' is not mistaken for a share action."""
    reset_all = {"reset", "all"} <= toks or "resetall" in toks
    if name.startswith("reset_") and not reset_all:
        return None
    hits = sorted(toks & DESTRUCTIVE_TOKENS)
    if reset_all:
        hits.append("reset all")
    return ", ".join(hits) or None


def fnum(text):
    if text is None or text == "":
        return None
    try:
        return float(text)
    except ValueError:
        return None


def rel(path):
    return os.path.relpath(path, REPO).replace("\\", "/")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def parse_xui_field(field):
    """'panel_actor_mover.xml:22 <radio_group>' -> ('panel_actor_mover.xml', 22)."""
    field = (field or "").strip()
    if not field:
        return "", None
    m = re.match(r"^([^\s:]+\.xml)(?::(\d+))?", field)
    if not m:
        return "", None
    return m.group(1), int(m.group(2)) if m.group(2) else None


def owner_from_evidence(evidence):
    m = re.search(r"([A-Za-z_]\w*::\w+)", evidence or "")
    return m.group(1) if m else ""


# ---------------------------------------------------------------- settings

def _pairs(map_el):
    if map_el is None:
        return []
    kids = list(map_el)
    out = []
    i = 0
    while i + 1 < len(kids):
        if kids[i].tag == "key":
            out.append(((kids[i].text or "").strip(), kids[i + 1]))
            i += 2
        else:
            i += 1
    return out


def _llsd_value(el):
    tag = el.tag
    txt = (el.text or "").strip()
    if tag == "integer":
        try:
            return int(txt)
        except ValueError:
            return txt
    if tag == "real":
        try:
            return float(txt)
        except ValueError:
            return txt
    if tag == "boolean":
        return txt.lower() in ("true", "1")
    if tag == "array":
        return [_llsd_value(c) for c in el]
    if tag == "map":
        return {k: _llsd_value(v) for k, v in _pairs(el)}
    return txt


def load_settings():
    decl = {}
    for fn in SETTINGS_FILES:
        path = os.path.join(SETTINGS_DIR, fn)
        if not os.path.isfile(path):
            continue
        root = ET.parse(path).getroot()
        for name, body in _pairs(root.find("map")):
            if name in decl or body.tag != "map":
                continue
            d = dict(_pairs(body))
            if "Type" not in d and "Value" not in d:
                continue
            stype = _llsd_value(d["Type"]) if "Type" in d else ""
            default = _llsd_value(d["Value"]) if "Value" in d else None
            if stype == "Boolean" and default is not None:
                default = bool(default)
            decl[name] = {"type": stype, "default": default, "file": fn}
    return decl


# ---------------------------------------------------------------- XUI index

class XuiIndex:
    def __init__(self):
        self.paths = {}
        for dp, _dn, fns in os.walk(XUI_DIR):
            for fn in fns:
                if fn.lower().endswith(".xml"):
                    self.paths.setdefault(fn, os.path.join(dp, fn))
        self.cache = {}
        self.warnings = []
        self.unknown_tags = collections.Counter()

    def load(self, base):
        if base in self.cache:
            return self.cache[base]
        path = self.paths.get(base)
        names, any_names, controls = {}, {}, {}
        root = None
        if path is None:
            self.warnings.append("XUI file not found: %s" % base)
        else:
            try:
                root = ET.parse(path).getroot()
            except ET.ParseError as exc:
                self.warnings.append("XUI parse error %s: %s" % (base, exc))
        if root is not None:
            for el in root.iter():
                n = el.get("name")
                if n:
                    if el.tag in CONTROL_TYPE:
                        names.setdefault(n, el)
                    any_names.setdefault(n, el)
                cn = el.get("control_name")
                if cn:
                    controls.setdefault(cn, el)
                    if el.tag not in CONTROL_TYPE:
                        self.unknown_tags[el.tag] += 1
        self.cache[base] = (path, root, names, any_names, controls)
        return self.cache[base]

    def find(self, base, name):
        if base:
            _p, _r, names, any_names, _c = self.load(base)
            if name in names:
                return names[name]
            if name in any_names:
                return any_names[name]
        # Fallback: search every XUI file (only reached on a miss).
        for other in sorted(self.paths):
            _p, _r, names, _a, _c = self.load(other)
            if name in names:
                return names[name]
        return None

    def find_by_control(self, base, setting):
        if not base:
            return None
        _p, _r, _n, _a, controls = self.load(base)
        return controls.get(setting)

    def path_of(self, base):
        return self.paths.get(base, "") if base else ""


def xui_items(el):
    items = []
    if el is None:
        return items
    for c in el:
        if c.tag in ("combo_box.item", "radio_item"):
            value = c.get("value")
            if value is None:
                value = c.get("name", "")
            items.append((c.get("label") or c.get("name") or "", value))
    return items


# ---------------------------------------------------------------- test values

def bool_test(default):
    return not bool(default)


def number_test(default, lo, hi, step, stype):
    integer = stype in ("S32", "U32")
    d = default if isinstance(default, (int, float)) and not isinstance(default, bool) else None
    if d is None:
        d = lo if lo is not None else 0.0
    if step is None or step <= 0:
        if lo is not None and hi is not None and hi > lo:
            step = (hi - lo) / 10.0
        else:
            step = 1.0 if integer else max(0.1, abs(d) * 0.1)
    if integer:
        step = max(1.0, float(round(step)))
    for cand in (d + step, d - step):
        if lo is not None and cand < lo - 1e-12:
            continue
        if hi is not None and cand > hi + 1e-12:
            continue
        if abs(cand - d) > 1e-9:
            return int(round(cand)) if integer else round(cand, 6)
    for cand in (lo, hi):
        if cand is not None and abs(cand - d) > 1e-9:
            return int(cand) if integer else round(cand, 6)
    return None


def coerce_item(raw, stype):
    """Convert an XUI item value to the declared setting type where possible."""
    if stype == "Boolean":
        return str(raw).strip().lower() in ("1", "true")
    if stype in ("S32", "U32"):
        try:
            return int(raw)
        except (TypeError, ValueError):
            return raw
    if stype == "F32":
        try:
            return float(raw)
        except (TypeError, ValueError):
            return raw
    return raw


def combo_test(default, items, stype):
    """Return (test_value, expect_value) for a combo or radio group."""
    if not items:
        marker = {"item_index": 1, "resolve": "runtime"}
        return marker, marker
    dv = None if default is None else str(default)
    order = [1] + [i for i in range(len(items)) if i not in (0, 1)] + [0]
    for i in order:
        if i >= len(items):
            continue
        label, raw = items[i]
        value = coerce_item(raw, stype)
        if dv is not None and str(value) == dv:
            continue
        test = {"item_index": i, "label": label, "value": value}
        return test, value
    return None, None


def color_test(default, stype):
    n = 3 if stype == "Color3" else 4
    cand = [0.2, 0.6, 1.0, 1.0][:n]
    if isinstance(default, list) and [round(float(x), 6) for x in default] == cand:
        cand = [1.0, 0.5, 0.0, 1.0][:n]
    return cand


def text_test(default):
    return "selftest2" if default == "selftest" else "selftest"


# ---------------------------------------------------------------- rim preset

def load_rim_presets():
    try:
        with open(RIM_PRESET_CPP, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return [], []
    presets = []
    m = re.search(r"RIG_RIM_PRESETS\[\d+\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if m:
        presets = [(n, float(v)) for n, v in
                   re.findall(r'\{\s*"([^"]+)",\s*([-+]?[\d.]+)f', m.group(1))]
    written = []
    fn = re.search(r"void applyRigRimPreset\(.*?\n\}", text, re.S)
    if fn:
        body = fn.group(0)
        written = re.findall(r'setF32\("(\w+)"', body) + re.findall(r'setBOOL\("(\w+)"', body)
    return presets, written


# ---------------------------------------------------------------- CSV rows

def load_csv():
    with open(CSV_PATH, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


# ---------------------------------------------------------------- classify

def classify(c):
    """Return (action, reason, hazards). action in RUN, SKIP, DEFER_INWORLD,
    EXPECT_MISSING, NO_EXERCISE."""
    hazards = []
    if c["status"] == "MISSING":
        return "EXPECT_MISSING", "no element with this name in this version", hazards
    if c["exercise"] == "none":
        return "NO_EXERCISE", "no generic exercise for widget type '%s'" % c["etype"], hazards
    text_like = c["exercise"] == "set_value" and c["etype"] == "text"
    toks = tokens_of(c["name"], c["label"])
    hit = destructive_hit(c["name"], toks) if c["exercise"] == "click" and c.get("reset_of") is None else None
    if hit:
        return "SKIP", "destructive (%s) in action name or label" % hit, hazards
    if text_like and toks & FILE_TOKENS:
        return "SKIP", "file/path text input (would write a host path)", hazards
    if c["etype"] == "list":
        hazards.append("requires_world")
        return "DEFER_INWORLD", "list contents are populated in-world (avatars, anims, groups, presets)", hazards
    if c["exercise"] == "click" and c.get("reset_of") is None:
        if c["page"] in WORLD_PAGES:
            hazards.append("requires_world")
            if toks & SELECTION_TOKENS:
                hazards.append("requires_selection")
            return "DEFER_INWORLD", "page acts on in-world actor/object/selection", hazards
        if toks & WORLD_TOKENS:
            hazards.append("requires_world")
            if toks & SELECTION_TOKENS:
                hazards.append("requires_selection")
            return "DEFER_INWORLD", "action needs an active in-world avatar, object or running take", hazards
    if c["exercise"] == "click" and toks & MODAL_TOKENS:
        hazards.append("modal")
    if c["etype"] == "color":
        hazards.append("opens_color_picker")
    if text_like:
        hazards.append("text_input")
    return "RUN", "", hazards


# ---------------------------------------------------------------- entries

def base_entry(page_meta, page, version, source):
    label, group = page_meta.get(page, ("", ""))
    return {"page": page, "page_label": label, "group": group, "version": version,
            "source": source, "host": host_for(version, page)}


def build_expect(c, test_resolved):
    if c["status"] == "MISSING":
        e = {"ui": "MISSING in %s" % c["version"]}
        if c.get("relocated_to"):
            e["relocated_to"] = c["relocated_to"]
        return e
    if c.get("known_gap"):
        return {"ui": "no observable effect (UNWIRED: known gap)", "known_gap": True}
    if c["action"] == "SKIP" or c["action"] == "NO_EXERCISE":
        return None
    e = {}
    setting = c.get("setting")
    if setting and c.get("exercise") in ("toggle", "set_value", "select_item"):
        e["setting"] = setting
        e["type"] = c.get("setting_type", "")
        e["value"] = test_resolved
    elif setting and c.get("reset_of"):
        e["setting"] = setting
        e["type"] = c.get("setting_type", "")
        e["value"] = c.get("default")
    elif setting:
        e["setting"] = setting
        e["type"] = c.get("setting_type", "")
        e["value"] = "changes"
    if c.get("owner") or c.get("callbacks"):
        e["callback"] = c.get("callbacks") or c.get("owner")
        e["commit"] = True
    if "modal" in c.get("hazards", []):
        e["ui"] = "floater or dialog opens; runner closes it"
    if not e:
        e["ui"] = "owner-installed handler fires (no generic probe)"
    return e


def build_csv_entry(idx, r, v, xi, settings, page_meta):
    status = r[v + "_status"]
    base, line = parse_xui_field(r[v + "_xui"])
    name = r["widget"]
    label = r["label"]
    el = None if status == "MISSING" else xi.find(base, name)
    if status != "MISSING" and el is None:
        xi.warnings.append("element '%s' not found for %s on %s" % (name, v, r["page"]))

    setting, setting_src = "", ""
    if el is not None and el.get("control_name"):
        setting, setting_src = el.get("control_name"), "xui control_name"
    if not setting and r[v + "_bound"]:
        setting, setting_src = r[v + "_bound"], "csv bound"
    if not setting and status != "MISSING" and r["mockup_setting"] in settings:
        setting, setting_src = r["mockup_setting"], "csv mockup_setting"

    if el is not None and el.tag in CONTROL_TYPE:
        etype = CONTROL_TYPE[el.tag]
    else:
        etype = KIND_TO_TYPE.get(r["kind"], r["kind"])
    exercise = EXERCISE.get(etype, "none")

    reset_of = None
    if exercise == "click" and name.startswith("reset_") and name[6:] in settings:
        reset_of = name[6:]
        setting, setting_src = reset_of, "reset_ prefix"

    sinfo = settings.get(setting) if setting else None
    stype = sinfo["type"] if sinfo else ""
    default = sinfo["default"] if sinfo else None
    lo = fnum(el.get("min_val")) if el is not None else None
    hi = fnum(el.get("max_val")) if el is not None else None
    step = fnum(el.get("increment")) if el is not None else None
    items = xui_items(el) if etype in ("combo", "radio") else []

    test_value, test_resolved = None, None
    if exercise == "toggle":
        test_value = test_resolved = bool_test(default)
    elif exercise == "set_value" and etype in ("slider", "spinner", "dial"):
        test_value = test_resolved = number_test(default, lo, hi, step, stype)
    elif exercise == "set_value" and etype == "color":
        test_value = test_resolved = color_test(default, stype)
    elif exercise == "set_value" and etype == "text" and stype in ("S32", "U32", "F32"):
        test_value = test_resolved = number_test(default, lo, hi, step, stype)
    elif exercise == "set_value" and etype == "text":
        test_value = test_resolved = text_test(default)
    elif exercise == "select_item":
        test_value, test_resolved = combo_test(default, items, stype)

    pointer_endpoint = POINTER_SLIDER_ENDPOINT_TESTS.get((v, r["page"], name))
    if pointer_endpoint is not None:
        test_value = test_resolved = pointer_endpoint
    transformed = TRANSFORMED_VALUE_TESTS.get((v, r["page"], name))
    if transformed is not None:
        test_value, test_resolved = transformed

    cb = r[v + "_callbacks"]
    owner = owner_from_evidence(r[v + "_evidence"])
    action, reason, hazards = classify({
        "status": status, "exercise": exercise, "etype": etype, "name": name,
        "label": label, "page": r["page"], "reset_of": reset_of, "version": v,
    })
    if (v, r["page"], name) in DECLARED_NON_INTERACTIVE:
        action = "NO_EXERCISE"
        reason = DECLARED_NON_INTERACTIVE[(v, r["page"], name)]
        hazards = []
    suspect = status == "V2_SUSPECT_FLOATER_SCOPED"
    known_gap = status == "UNWIRED"
    if v == "v2" and r["v2_source"].startswith("redirect"):
        hazards.append("v2_launcher_redirect")
    if action == "DEFER_INWORLD" and not reason:
        reason = "world-dependent"
    if reset_of:
        test_value = None
        test_resolved = None
    if action == "SKIP" and exercise == "set_value" and etype == "text":
        test_value = test_resolved = None
    if status == "MISSING":
        action, reason, hazards = "EXPECT_MISSING", "no element with this name in this version", []
        test_value = test_resolved = None
    if exercise == "none" and status != "MISSING":
        action = "NO_EXERCISE"
        reason = "no generic exercise for widget type '%s'" % etype
        test_value = test_resolved = None

    c = {"status": status, "exercise": exercise, "etype": etype, "name": name,
         "label": label, "page": r["page"], "version": v, "action": action,
         "setting": setting, "setting_type": stype, "default": default,
         "reset_of": reset_of, "callbacks": cb, "owner": owner, "known_gap": known_gap,
         "hazards": hazards, "relocated_to": r.get(v + "_relocated_to", "")}
    expect = build_expect(c, test_resolved)
    if (v, r["page"], name) in DECLARED_NON_INTERACTIVE:
        expect = None

    if action in ("SKIP", "NO_EXERCISE", "EXPECT_MISSING"):
        test_value = test_resolved = None
    if exercise == "select_item" and etype == "radio" and not items:
        pass

    restore = None
    if setting and exercise in ("toggle", "set_value", "select_item") and not reset_of:
        restore = {"setting": setting, "value": default if default is not None else "snapshot-before-test"}
    elif reset_of:
        restore = {"setting": reset_of, "value": "snapshot-before-test"}
    elif exercise in ("toggle", "set_value", "select_item") and test_value is not None:
        restore = {"ui": "restore the widget's pre-test value (no bound setting)"}
    elif exercise == "click":
        restore = {"note": "close any floater opened; undo state the action creates"}

    entry = base_entry(page_meta, r["page"], v, "csv:%d" % idx)
    entry.update({
        "id": "%s:%s:%s:%d" % (v, r["page"], name, idx),
        "csv_index": idx,
        "card": r["card"], "card_title": r["card_title"],
        "row_label": label,
        "xui_file": rel(xi.path_of(base)) if base and xi.path_of(base) else "",
        "xui_line": line,
        "widget_name": name,
        "widget_type": etype,
        "csv_kind": r["kind"],
        "wiring_status": status,
        "resolution": r[v + "_source"],
        "action": action,
        "reason": reason,
        "known_gap": known_gap,
        "suspect": suspect,
        "hazards": sorted(set(hazards)),
        "exercise": exercise,
        "test_value": test_value,
        "expect": expect,
        "restore": restore,
        "setting": setting or None,
        "setting_source": setting_src or None,
        "setting_type": stype or None,
        "default": default,
        "min": lo, "max": hi, "increment": step,
        "callbacks": cb or None,
        "owner": owner or None,
        "evidence": r[v + "_evidence"] or None,
    })
    temporal_oracle = TEMPORAL_PRESET_ORACLES.get((v, r["page"], name))
    if temporal_oracle:
        scale, mode, pre_scale = temporal_oracle
        entry.update({
            "probe": "temporal_preset",
            "expect": {"setting": "TemporalWorldScale", "type": "F32", "value": scale,
                       "callback": owner, "commit": True},
            "oracle_pre_value": pre_scale,
            "temporal_mode": mode,
            "restore": {"snapshot_settings": ["TemporalWorldScale", "TemporalMode"]},
        })
    if pointer_endpoint is not None:
        entry["pointer_drag"] = True
    if items:
        entry["options"] = [{"label": a, "value": b} for a, b in items]
    if test_resolved is not None and test_resolved != test_value:
        entry["test_value_resolved"] = test_resolved
    if r.get("v2_loss") and v == "v2" and r["v2_loss"] not in ("", "0"):
        entry["probe"] = "v2_loss"
    if v == "v2":
        # These code-bound controls do not own a setting with the widget's
        # name.  Point each functional row at state its shipping callback
        # actually writes (alpanelcinelightrig.cpp).
        code_oracles = {
            "cine_easy_rim": ("CineLightRigRimEV", "F32", -2.5, -1.0),
            "cine_easy_bg": ("CineLightRigBgOn", "Boolean", True, False),
            "cine_easy_cone_width": ("CineLightRigKeyBeam", "S32", 0, 2),
            "cine_easy_flare_preset": ("RenderCineLensFlarePreset", "S32", 1, 0),
            "cine_easy_night_mask_preset": ("CineLightRigNightMaskShape", "S32", 1, 0),
        }
        if name in code_oracles:
            oracle_setting, oracle_type, oracle_value, oracle_pre_value = code_oracles[name]
            entry["expect"] = {"setting": oracle_setting, "type": oracle_type,
                               "value": oracle_value, "callback": owner,
                               "commit": True}
            entry["oracle_pre_value"] = oracle_pre_value
            entry["restore"] = {"setting": oracle_setting,
                                "value": settings.get(oracle_setting, {}).get("default")}
        if name == "cine_fixture_role":
            # Selecting Fill must refresh the role-owned fixture editors.  Give
            # that role a distinct Kelvin witness so visible-state comparison
            # proves onFixtureRoleCommit updated the panel's selected fixture.
            entry["reset_prestate"] = {"CineLightRigFillKelvin": 6500.0}
            entry["probe"] = "fixture_role"
    if name in ("FlycamOperatorModeBlendTime", "FlycamOperatorSimulationHz") and v == "v2":
        entry["probe"] = "operator_v2_missing"
    if suspect:
        entry.setdefault("notes", []).append("V2_SUSPECT_FLOATER_SCOPED: run and confirm in-world")
    if known_gap:
        entry.setdefault("notes", []).append("UNWIRED in %s: expected to fail until wired" % v)
    if status == "MISSING" and r.get(v + "_relocated_to"):
        entry.setdefault("notes", []).append("relocated to " + r[v + "_relocated_to"])
    return entry


# ---------------------------------------------------------------- synthetic probes

def finish_synthetic(entry, action, reason, hazards, test, expect, restore):
    entry.update({"action": action, "reason": reason, "known_gap": False,
                  "suspect": False, "hazards": sorted(set(hazards)),
                  "test_value": test, "expect": expect, "restore": restore})
    return entry


def synthetic_entries(xi, settings, csv_pairs, page_meta, presets, preset_written):
    out = []
    seen = set()

    def add_toggle_setting(page, page_card, widget, setting, base, family, version, title):
        key = (version, page, widget)
        if key in seen or (page, widget) in csv_pairs:
            return
        seen.add(key)
        sinfo = settings.get(setting, {})
        stype = sinfo.get("type", "")
        default = sinfo.get("default")
        e = base_entry(page_meta, page, version, "synthetic:" + family)
        e.update({"id": "%s:%s:%s:synthetic" % (version, page, widget),
                  "csv_index": None, "card": page_card, "card_title": title,
                  "row_label": title, "xui_file": rel(xi.path_of(base)) if base else "",
                  "xui_line": None, "widget_name": widget, "widget_type": "toggle",
                  "csv_kind": "", "wiring_status": "SYNTHETIC", "resolution": "synthetic",
                  "exercise": "toggle", "setting": setting, "setting_source": "probe",
                  "setting_type": stype or None, "default": default,
                  "min": None, "max": None, "increment": None, "callbacks": None,
                  "owner": None, "evidence": None, "probe": family})
        hz = []
        if family == "weather_master":
            hz.append("requires_deferred_rendering")
        finish_synthetic(e, "RUN", "", hz, bool_test(default),
                         {"setting": setting, "type": stype, "value": bool_test(default)},
                         {"setting": setting, "value": default})
        out.append(e)

    # Page masters and enable switches (v2 kit pages and v1 legacy panel).
    for fn in sorted(os.listdir(XUI_DIR)):
        if not (fn.startswith("panel_dpage_") and fn.endswith(".xml")):
            continue
        path, root, _n, _a, _c = xi.load(fn)
        if root is None:
            continue
        for kp in root.iter("kit_page"):
            page = (kp.get("page") or "").split("/")[-1]
            if not page:
                continue
            if kp.get("master_widget") and kp.get("master_setting"):
                add_toggle_setting(page, kp.get("name", ""), kp.get("master_widget"),
                                   kp.get("master_setting"), fn, "page_master", "v2",
                                   "Page master: " + kp.get("master_label", ""))
            for el in kp.iter():
                if el.tag in ("kit_card", "kit_option") and el.get("enable_widget") and el.get("enable_setting"):
                    add_toggle_setting(page, el.get("name", el.get("label", "")),
                                       el.get("enable_widget"), el.get("enable_setting"),
                                       fn, "enable_switch", "v2", el.get("title", el.get("label", "")))
            for card in kp.iter("kit_card"):
                if card.get("kind") != "selector" or not card.get("sel_setting"):
                    continue
                sel = card.get("sel_setting")
                sinfo = settings.get(sel, {})
                for opt in card.findall("kit_option"):
                    try:
                        value = int(opt.get("value"))
                    except (TypeError, ValueError):
                        continue
                    widget = "%s:%s" % (card.get("name"), opt.get("label"))
                    key = ("v2", page, widget)
                    if key in seen:
                        continue
                    seen.add(key)
                    e = base_entry(page_meta, page, "v2", "synthetic:selector_segment")
                    e.update({"id": "v2:%s:%s:synthetic" % (page, widget), "csv_index": None,
                              "card": card.get("id", ""), "card_title": card.get("title", ""),
                              "row_label": opt.get("label", ""), "xui_file": rel(xi.path_of(fn)),
                              "xui_line": None, "widget_name": widget,
                              "widget_type": "segment", "csv_kind": "", "wiring_status": "SYNTHETIC",
                              "resolution": "kit", "exercise": "click", "setting": sel,
                              "setting_source": "sel_setting", "setting_type": sinfo.get("type"),
                              "default": sinfo.get("default"), "min": None, "max": None,
                              "increment": None, "callbacks": None, "owner": None,
                              "evidence": None, "probe": "selector_segment"})
                    finish_synthetic(
                        e, "RUN", "", [], value,
                        {"setting": sel, "type": sinfo.get("type", ""), "value": value},
                        {"setting": sel, "value": sinfo.get("default")})
                    e["notes"] = ["Click every segment in turn; assert %s == %d and the matching option is shown." % (sel, value)]
                    out.append(e)
            if kp.get("preset_mode") == "provider" and kp.get("preset_provider") == "rig_rim" and presets:
                default_master = settings.get(RIG_RIM_SENTINEL, {}).get("default")
                pick = next((p for p in presets if p[1] != default_master), presets[0])
                key = ("v2", page, "preset_strip:rig_rim")
                if key not in seen:
                    seen.add(key)
                    e = base_entry(page_meta, page, "v2", "synthetic:preset_strip")
                    e.update({"id": "v2:%s:preset_strip:rig_rim:synthetic" % page, "csv_index": None,
                              "card": "preset_strip", "card_title": "Rim preset strip",
                              "row_label": "Preset", "xui_file": rel(xi.path_of(fn)),
                              "xui_line": None, "widget_name": "preset_strip:rig_rim",
                              "widget_type": "combo", "csv_kind": "", "wiring_status": "SYNTHETIC",
                              "resolution": "provider", "exercise": "select_item",
                              "setting": RIG_RIM_SENTINEL, "setting_source": "rig_rim preset table",
                              "setting_type": settings.get(RIG_RIM_SENTINEL, {}).get("type"),
                              "default": default_master, "min": None, "max": None, "increment": None,
                              "callbacks": None, "owner": "ALPanelRigRim", "evidence": None,
                              "probe": "preset_strip"})
                    test = {"preset": pick[0], "id": "b:" + pick[0]}
                    finish_synthetic(
                        e, "RUN", "", [], test,
                        {"setting": RIG_RIM_SENTINEL, "type": "F32", "value": pick[1]},
                        {"snapshot_settings": preset_written})
                    e["oracle_pre_value"] = default_master if default_master != pick[1] else (
                        pick[1] + 0.25)
                    e["notes"] = [
                        "Select the built-in '%s' from the strip; sentinel %s must equal %s." % (pick[0], RIG_RIM_SENTINEL, pick[1]),
                        "The preset does not write CineRigRimEnabled; a disabled Rim master makes the result visually inert, not a failure.",
                        "Restore snapshot covers all %d settings applyRigRimPreset writes." % len(preset_written)]
                    out.append(e)

    # v1 legacy panels: page masters, Rim preset combo and the Weather master.
    v1_base = "panel_cine_light_rig.xml"
    for setting, page in (("CineLightRigEnabled", "cine_light_rig_tab"),
                          ("CineRigRimEnabled", "light_rim_tab")):
        el = xi.find_by_control(v1_base, setting)
        if el is not None:
            add_toggle_setting(page, "", el.get("name") or setting, setting, v1_base, "page_master", "v1",
                               "Page master (v1 Cine Light Rig panel)")
        else:
            xi.warnings.append("v1 master for %s not found in %s" % (setting, v1_base))
    for version in VERSIONS:
        el = xi.find_by_control("panel_weather_settings.xml", "AlchemyWeatherEnabled")
        if el is not None:
            add_toggle_setting("weather_tab", "", el.get("name") or "weather_enabled",
                               "AlchemyWeatherEnabled", "panel_weather_settings.xml",
                               "weather_master", version, "Cinematic weather master")
        rain = xi.find_by_control("panel_weather_settings.xml", "AlchemyWeatherRainEnabled")
        if rain is not None:
            add_toggle_setting("weather_tab", "weather_rain_group",
                               rain.get("name") or "weather_rain_enabled",
                               "AlchemyWeatherRainEnabled", "panel_weather_settings.xml",
                               "rain_gate", version, "Rain enabled")
    rr = xi.find("panel_cine_light_rig.xml", "cine_rig_rim_preset")
    if rr is not None and presets:
        default_master = settings.get(RIG_RIM_SENTINEL, {}).get("default")
        pick = next((p for p in presets if p[1] != default_master), presets[0])
        key = ("v1", "light_rim_tab", "cine_rig_rim_preset")
        if key not in seen:
            seen.add(key)
            e = base_entry(page_meta, "light_rim_tab", "v1", "synthetic:preset_strip")
            e.update({"id": "v1:light_rim_tab:cine_rig_rim_preset:synthetic", "csv_index": None,
                      "card": "rim_preset", "card_title": "Rim preset", "row_label": "Preset",
                      "xui_file": rel(xi.path_of("panel_cine_light_rig.xml")), "xui_line": None,
                      "widget_name": "cine_rig_rim_preset", "widget_type": "combo", "csv_kind": "",
                      "wiring_status": "SYNTHETIC", "resolution": "xui", "exercise": "select_item",
                      "setting": RIG_RIM_SENTINEL, "setting_source": "rig_rim preset table",
                      "setting_type": settings.get(RIG_RIM_SENTINEL, {}).get("type"),
                      "default": default_master, "min": None, "max": None, "increment": None,
                      "callbacks": None, "owner": "ALRigRimController", "evidence": None,
                      "probe": "preset_strip"})
            finish_synthetic(e, "RUN", "", [], {"preset": pick[0], "id": pick[0]},
                             {"setting": RIG_RIM_SENTINEL, "type": "F32", "value": pick[1]},
                             {"snapshot_settings": preset_written})
            e["oracle_pre_value"] = default_master if default_master != pick[1] else (pick[1] + 0.25)
            out.append(e)
    return out


# ---------------------------------------------------------------- summary

ACTION_ORDER = ["RUN", "RUN_KNOWN_GAP", "DEFER_INWORLD", "SKIP", "EXPECT_MISSING", "NO_EXERCISE"]


def bucket(e):
    if e["action"] == "RUN" and e.get("known_gap"):
        return "RUN_KNOWN_GAP"
    return e["action"]


def write_md(entries, meta, skip_rows, page_order, page_meta, synth):
    lines = []
    w = lines.append
    w("# Director functional manifest (login-screen self-test plan)")
    w("")
    w("Generated by `scripts/code_tools/director_functional_manifest.py` (stdlib, `python -I`). Read-only over source and XUI.")
    w("Machine-readable plan: `doc/director_kit/functional/functional_manifest.json`.")
    w("")
    w("Inputs: `wiring_table.csv` sha256 `%s` (%d interactive rows); %d settings declared; %d XUI files indexed; rim presets parsed from `alrigrimcontroller.cpp`: %d built-ins." % (
        meta["csv_sha256"][:16], meta["interactive_rows"], meta["settings_declared"], meta["xui_files"], meta["rim_presets"]))
    w("")
    w("## Action legend")
    w("")
    w("- **RUN**: exercise at the login screen and assert `expect`. Rows with `known_gap` (UNWIRED) are counted separately; a failure there is the expected finding.")
    w("- **DEFER_INWORLD**: needs an in-world avatar, object, selection or running take. Also defer at runtime if the widget is disabled.")
    w("- **SKIP**: destructive (delete, clear, save, export, reset-all, file or path input). Reason is listed below.")
    w("- **EXPECT_MISSING**: the control is absent in that version; the test asserts absence.")
    w("- **NO_EXERCISE**: widget type has no generic exercise (viewport, texture picker, custom).")
    w("")
    w("## Totals per version")
    w("")
    w("| Version | RUN | RUN (known gap) | DEFER_INWORLD | SKIP | EXPECT_MISSING | NO_EXERCISE | Total |")
    w("|---|---:|---:|---:|---:|---:|---:|---:|")
    for v in VERSIONS:
        cnt = collections.Counter(bucket(e) for e in entries if e["version"] == v)
        tot = sum(cnt.values())
        w("| %s | %d | %d | %d | %d | %d | %d | %d |" % (
            v, cnt["RUN"], cnt["RUN_KNOWN_GAP"], cnt["DEFER_INWORLD"], cnt["SKIP"],
            cnt["EXPECT_MISSING"], cnt["NO_EXERCISE"], tot))
    w("")
    w("Suspect (V2_SUSPECT_FLOATER_SCOPED) entries are RUN and flagged `suspect`: %d." % sum(1 for e in entries if e.get("suspect")))
    w("")
    w("## Counts per page and version")
    w("")
    w("| Page | Group | v1 RUN | v1 gap | v1 defer | v1 skip | v1 missing | v1 no-ex | v2 RUN | v2 gap | v2 defer | v2 skip | v2 missing | v2 no-ex |")
    w("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for page in page_order:
        cells = []
        for v in VERSIONS:
            cnt = collections.Counter(bucket(e) for e in entries if e["page"] == page and e["version"] == v)
            cells += [cnt["RUN"], cnt["RUN_KNOWN_GAP"], cnt["DEFER_INWORLD"], cnt["SKIP"],
                      cnt["EXPECT_MISSING"], cnt["NO_EXERCISE"]]
        label, group = page_meta[page]
        w("| %s (%s) | %s | %s |" % (page, label, group, " | ".join(str(x) for x in cells)))
    w("")
    w("## Special probes")
    w("")
    w("| Probe | Version | Page | Widget | Exercise | Expect |")
    w("|---|---|---|---|---|---|")
    for e in synth + [x for x in entries if x.get("probe") == "operator_v2_missing"]:
        exp = e["expect"]
        if e.get("probe") == "operator_v2_missing":
            exp_s = "MISSING in v2"
        elif isinstance(exp, dict) and "setting" in exp:
            exp_s = "%s = %s" % (exp["setting"], json.dumps(exp["value"]))
        else:
            exp_s = json.dumps(exp)
        w("| %s | %s | %s | %s | %s | %s |" % (e.get("probe"), e["version"], e["page"],
                                             e["widget_name"], e["exercise"], exp_s))
    w("")
    w("Selector segments (both v2 cards, 8 total) expect `DirectorV2SelectedLight` to equal 0 to 3 in turn. Synthetic rows are programmatic (ALKitCard, ALKitPage, ALKitPresets) and have no CSV row.")
    w("")
    w("## SKIP list (destructive) with reasons")
    w("")
    w("| Page | Widget | Label | Version(s) | Reason |")
    w("|---|---|---|---|---|")
    for page, widget, label, vers, reason in skip_rows:
        w("| %s | %s | %s | %s | %s |" % (page, widget, label, ",".join(vers), reason))
    w("")
    w("## Caveats")
    w("")
    w("- Deferral and destructive classification is name and label based (token rules in the script). The runtime must also defer any widget that is disabled at login.")
    w("- Combos and radio groups without XUI items (kit combos) carry `item_index: 1` with `resolve: runtime`; the runner reads the item list and picks the second value.")
    w("- Setting min, max and increment come from the XUI element only. Settings XML carries no range.")
    w("- v2 launcher stubs (`v2_launcher_redirect`) host their controls in the v1 owner floater that they open; test them through the launcher.")
    w("- XUI lookup warnings: %d (widgets whose CSV name has no named XUI element, usually unnamed `check_box`/`slider` rows, or settings-only names). The setting still comes from the CSV; the full list is in `meta.warnings` of the JSON." % meta["warning_count"])
    w("- Coverage check: every interactive CSV row yields one entry per version (verified by the script before writing).")
    w("")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- main

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json-output", default=JSON_PATH,
                        help="generated JSON path (default: checked-in manifest)")
    parser.add_argument("--markdown-output", default=MD_PATH,
                        help="generated Markdown path (default: checked-in report)")
    args = parser.parse_args()
    json_path = os.path.abspath(args.json_output)
    markdown_path = os.path.abspath(args.markdown_output)
    rows = load_csv()
    settings = load_settings()
    xi = XuiIndex()
    presets, preset_written = load_rim_presets()

    page_meta, page_order = {}, []
    for r in rows:
        if r["page"] not in page_meta:
            page_meta[r["page"]] = (r["page_label"], r["group"])
            page_order.append(r["page"])

    csv_pairs = set()
    for r in rows:
        if r["interactive"] == "Y":
            csv_pairs.add((r["page"], r["widget"]))

    entries = []
    for idx, r in enumerate(rows):
        if r["interactive"] != "Y":
            continue
        for v in VERSIONS:
            entries.append(build_csv_entry(idx, r, v, xi, settings, page_meta))

    synth = synthetic_entries(xi, settings, csv_pairs, page_meta, presets, preset_written)
    entries.extend(synth)

    # Coverage check: every interactive CSV row must appear once per version.
    expected = {(i, v) for i, r in enumerate(rows) if r["interactive"] == "Y" for v in VERSIONS}
    got = [(e["csv_index"], e["version"]) for e in entries if e.get("csv_index") is not None]
    if len(got) != len(set(got)) or set(got) != expected:
        sys.exit("coverage check failed: expected %d csv entries, got %d" % (len(expected), len(got)))
    if len(set(e["page"] for e in entries)) != len(page_order):
        sys.exit("coverage check failed: page count mismatch")

    skip_map = collections.OrderedDict()
    for e in entries:
        if e["action"] == "SKIP":
            key = (e["page"], e["widget_name"], e["row_label"])
            if key not in skip_map:
                skip_map[key] = ([], e["reason"])
            vers = skip_map[key][0]
            if e["version"] not in vers:
                vers.append(e["version"])
    skip_rows = [(p, w, l, vs, rs) for (p, w, l), (vs, rs) in skip_map.items()]

    meta = {
        "generator": "scripts/code_tools/director_functional_manifest.py",
        "csv": rel(CSV_PATH),
        "csv_sha256": sha256_file(CSV_PATH),
        "interactive_rows": sum(1 for r in rows if r["interactive"] == "Y"),
        "entries": len(entries),
        "pages": len(page_order),
        "settings_declared": len(settings),
        "xui_files": len(xi.paths),
        "rim_presets": len(presets),
        "rim_preset_written_settings": preset_written,
        "versions": list(VERSIONS),
        "actions": ACTION_ORDER,
        "warnings": xi.warnings[:200],
        "warning_count": len(xi.warnings),
        "unknown_control_tags": dict(xi.unknown_tags),
    }
    doc = {"meta": meta, "entries": entries}

    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    os.makedirs(os.path.dirname(markdown_path), exist_ok=True)
    with open(json_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, ensure_ascii=True)
        f.write("\n")
    with open(json_path, encoding="utf-8") as f:
        check = json.load(f)
    assert len(check["entries"]) == len(entries)

    md = write_md(entries, meta, skip_rows, page_order, page_meta, synth)
    with open(markdown_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(md)

    cnt = collections.Counter((e["version"], bucket(e)) for e in entries)
    print("entries=%d synthetic=%d pages=%d" % (len(entries), len(synth), len(page_order)))
    for v in VERSIONS:
        print(v, {a: cnt[(v, a)] for a in ACTION_ORDER})
    print("warnings=%d" % len(xi.warnings))
    print("wrote", rel(json_path), rel(markdown_path))


if __name__ == "__main__":
    main()
