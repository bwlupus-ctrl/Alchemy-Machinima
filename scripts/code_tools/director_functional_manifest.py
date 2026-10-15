#!/usr/bin/env python3
"""Director functional test manifest (read-only generator).

Builds the machine-readable test plan that a login-screen C++ self-test uses to
click every Director control (v1 and v2) and assert that it works.

Inputs (read only):
  generated panel_dpage_*.xml kit_rows       authoritative converted-page inventory
  doc/director_kit/wiring/wiring_table.csv   v1/legacy inventory and owner evidence
  indra/newview/app_settings/settings*.xml   setting type and default
  indra/newview/skins/default/xui/en/*.xml   widget tags, min/max/increment, items
  indra/newview/alrigrimcontroller.cpp       Rig Rim built-in preset table

Outputs:
  doc/director_kit/functional/functional_manifest.json
  doc/director_kit/functional/FUNCTIONAL_MANIFEST.md

The JSON also carries a "director_rules" block (page/link/popover registry and the list of
runtime rule checks R1-R6 that aluiperfselftest.cpp runs after the manifest rows).

Stdlib only. Run as:  python -I scripts/code_tools/director_functional_manifest.py
  --check      regenerate to a temp folder, validate the rule registry against the viewer source and
               the checked-in manifest, write nothing to the repo (exit 1 on any problem)
  --self-test  adversarial fixtures for the rule-registry validator
"""

import argparse
import collections
import copy
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
DIRECTOR_CPP = os.path.join(REPO, "indra", "newview", "llfloaterdirector.cpp")
VCAM_PAGE_JSON = os.path.join(HERE, "director_kit", "vcam_page.json")
KIT_INDEX_PATH = os.path.join(XUI_DIR, "director_kit_index.xml")
OUT_DIR = os.path.join(REPO, "doc", "director_kit", "functional")
HAZARD_RUN_ALLOW_PATH = os.path.join(OUT_DIR, "hazard_skip_run_allow.json")
JSON_PATH = os.path.join(OUT_DIR, "functional_manifest.json")
MD_PATH = os.path.join(OUT_DIR, "FUNCTIONAL_MANIFEST.md")
VERSIONS = ("v1", "v2")
GENERATED_KIT_PAGES = set()

# The row host is explicit so the viewer never guesses from a page name.  The
# launcher records mirror LLFloaterDirector::bindPage launchers.
LAUNCHER_HOSTS = {
    "diopter_tab": ("ultimate_diopter", "", "diopter_tab", "card_diopter_open"),
    "gaze_cues_tab": ("gaze_cues", "", "gaze_cues_tab", "card_gaze_cues_open"),
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

# Redirected pages can draw controls from several tabs in the same target
# floater.  The source panel identifies the tab more precisely than the broad
# Director page name.
LIGHTBOX_TAB_BY_XUI = {
    "panel_projector_shafts.xml": "projshaft_settings",
    "panel_atmospheric_volumes.xml": "atmospheric_volumes_settings",
}


def host_for(version, page):
    if version == "v2" and page in GENERATED_KIT_PAGES:
        return {"floater": "director", "page_view": page, "via": "tab"}
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
    "vcam_monitor_strip": "button", "vcam_monitor_pager": "button",
    "vcam_monitor_tile": "button", "vcam_camera_selector": "button",
    "vcam_face_list": "list",
}
# CSV kind -> widget type (fallback when the XUI element is not found).
KIND_TO_TYPE = {
    "slider": "slider", "number": "spinner", "dial": "dial", "arc": "dial",
    "combo": "combo", "toggle": "toggle", "button": "button", "link": "button",
    "color": "color", "list": "list", "text": "text", "vec2": "vec2",
    "vec3": "vec3", "readout": "readout", "custom": "custom", "viewport": "viewport", "gobo": "gobo",
}
# widget type -> exercise verb.
EXERCISE = {
    "toggle": "toggle", "slider": "set_value", "spinner": "set_value",
    "dial": "set_value", "vec2": "set_value", "vec3": "set_value",
    "color": "set_value", "text": "set_value", "combo": "select_item",
    "radio": "select_item", "list": "select_item", "button": "click",
    "readout": "readout",
}
# Pages whose actions operate on an in-world avatar/object/selection.
WORLD_PAGES = {"move_tab", "path_tab", "props_tab", "ghosts_tab", "dance_tab",
               "animate_tab", "actors_tab", "gaze_tab", "polish_tab",
               "gaze_cues_tab", "moves_tab", "vcam_tab"}
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

FORCED_SAFE_SKIPS = {
    "btn_load": ("opens a native file picker that cannot be safely automated at login",
                 ["file_picker"]),
    "look_at_camera_mode": ("body-turn on SELF can drive the AO (aoengine.cpp:654)",
                            ["agent_send"]),
}

# Controls that write the user's own saved data to disk. The harness never
# toggles or otherwise drives them: turning Remember on would persist the
# session's Avatar Light Response edits into the user's file. Keyed by
# (page, widget) because v1 and v2 both name it chk_persist. Enforced in classify().
PERSIST_WRITE_CONTROLS = {
    ("actors_tab", "chk_persist"): (
        "writes the session's Avatar Light Response edits to the user's saved file",
        ["persistent_setting"]),
}

V2_LEGACY_OPERATOR_ROWS = {
    "FlycamOperatorModeBlendTime", "FlycamOperatorSimulationHz",
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
    anchor = lo if lo is not None else 0.0
    # Sliders quantize to an increment grid anchored at min_val. Defaults in
    # legacy settings are sometimes off that grid (for example .325 with a
    # .01 step, or 45 with a 4-unit step); default +/- step is then impossible
    # to reach by pointer or arrow keys. Emit an actual grid point.
    candidates = []
    for raw in (d + step, d - step):
        candidates.append(anchor + round((raw - anchor) / step) * step)
    for cand in candidates:
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


def load_generated_kit_pages(xi):
    """Return non-legacy kit pages and their generated ``kit_row`` elements."""
    index_root = ET.parse(KIT_INDEX_PATH).getroot()
    index_pages = set()
    for line in (index_root.text or "").splitlines():
        fields = line.split("\t")
        if len(fields) > 1 and fields[1]:
            index_pages.add(fields[1])

    pages = collections.OrderedDict()
    for base in sorted(name for name in xi.paths if name.startswith("panel_dpage_")):
        path, root, _names, _any_names, _controls = xi.load(base)
        if root is None:
            continue
        kit_page = next(root.iter("kit_page"), None)
        if kit_page is None or kit_page.get("legacy") == "true":
            continue
        page = (kit_page.get("page") or "").rsplit("/", 1)[-1]
        # Shell-only Quick has no indexed rows and is not a functional page.
        if not page or page not in index_pages:
            continue
        xml_rows = []
        for card in kit_page.iter("kit_card"):
            for row in card.iter("kit_row"):
                xml_rows.append((base, path, kit_page, card, row))
        pages[page] = xml_rows
    return pages


def build_kit_entries(kit_pages, rows, settings, page_meta):
    """Build the converted-page v2 inventory solely from generated kit XML."""
    with open(DIRECTOR_CPP, encoding="utf-8") as source:
        director_cpp = source.read()
    binder_by_page = dict(re.findall(
        r'\{\s*"([^"]+)"\s*,\s*&LLFloaterDirector::(bind[A-Za-z0-9_]+Page)\s*\}',
        director_cpp))
    binder_by_page.update({
        "cine_light_rig_tab": "ALPanelCineLightRig::postBuild",
        "light_rim_tab": "ALRigRimController::attach",
        "vcam_tab": "ALPanelDirectorVCam::postBuild",
        "takes_tab": "ALPanelFlycamRecorder::postBuild",
        "temporal_tab": "ALPanelTemporalCapture::postBuild",
        "actors_tab": "ALPanelAvatarLightResponse::postBuild",
    })
    out = []
    for page, xml_rows in kit_pages.items():
        for ordinal, (_base, path, _kit_page, card, row) in enumerate(xml_rows):
            widget = row.get("widget", "")
            kind = row.get("kind", "")
            binding = row.get("bind", "")
            if not widget:
                sys.exit("kit coverage check failed: %s has a kit_row without widget" % page)
            etype = KIND_TO_TYPE.get(kind, kind)
            child = next((child for child in row if child.get("name") == widget), None)
            if child is not None and child.tag in CONTROL_TYPE:
                etype = CONTROL_TYPE[child.tag]
            exercise = EXERCISE.get(etype, "none")
            setting = row.get("setting", "")
            sinfo = settings.get(setting, {}) if setting else {}
            stype = sinfo.get("type", "")
            default = sinfo.get("default")
            lo = fnum(row.get("min_val"))
            hi = fnum(row.get("max_val"))
            step = fnum(row.get("increment"))
            items = xui_items(child) if etype in ("combo", "radio") else []
            test_value = test_resolved = None
            if exercise == "toggle":
                test_value = test_resolved = bool_test(default)
            elif exercise == "set_value" and etype in ("slider", "spinner", "dial"):
                test_value = test_resolved = number_test(default, lo, hi, step, stype)
            elif exercise == "set_value" and etype == "color":
                test_value = test_resolved = color_test(default, stype)
            elif exercise == "set_value" and etype == "text":
                test_value = test_resolved = (number_test(default, lo, hi, step, stype)
                                              if stype in ("S32", "U32", "F32")
                                              else text_test(default))
            elif exercise == "select_item":
                test_value, test_resolved = combo_test(default, items, stype)

            owner = ""
            callbacks = ""
            if page == "vcam_tab" and kind != "readout":
                owner = "ALPanelDirectorVCam::onControl"
            if binding == "code":
                if kind == "readout" and child is not None and (child.text or "").strip():
                    owner = "ALKitRow::postBuild (authored static text)"
                elif row.get("action") or row.get("link_to") or kind == "link":
                    owner = "ALKitRow::postBuild"
                else:
                    binder = binder_by_page.get(page, "")
                    if binder.startswith("bind"):
                        binder = "LLFloaterDirector::" + binder
                    owner = binder
                if exercise != "none" and not owner:
                    sys.exit("kit coverage check failed: no code binder for %s/%s" %
                             (page, widget))
            if page == "actors_tab" and widget in {
                    "light_response_popout", "am_reset_CineLightRigActorMaskBias", "am_test_trigger"}:
                owner = "LLFloaterDirector::bindLightActorsPage"
            action, reason, hazards = classify({
                "status": "GENERATED_KIT", "exercise": exercise, "etype": etype,
                "name": widget, "label": row.get("label", ""), "page": page,
                "reset_of": None, "version": "v2",
            })
            if exercise == "none":
                action = "NO_EXERCISE"
                reason = "no generic exercise for generated kit kind '%s'" % kind
            c = {"status": "GENERATED_KIT", "exercise": exercise, "etype": etype,
                 "name": widget, "label": row.get("label", ""), "page": page,
                 "version": "v2", "action": action, "setting": setting,
                 "setting_type": stype, "default": default, "reset_of": None,
                 "callbacks": callbacks, "owner": owner, "known_gap": False,
                 "hazards": hazards}
            expect = build_expect(c, test_resolved)
            if exercise == "readout":
                expect = {"ui": "non-empty text after page refresh"}
            if row.get("link_to"):
                expect = {"ui": "navigates to " + row.get("link_to")}
            restore = ({"setting": setting, "value": default}
                       if setting and exercise in ("toggle", "set_value", "select_item")
                       else {"ui": "restore the widget's pre-test value"}
                       if exercise in ("toggle", "set_value", "select_item")
                       else {"note": "restore any viewer-local action state"}
                       if exercise == "click" else None)
            entry = base_entry(page_meta, page, "v2", "generated_kit_xml")
            entry.update({
                "id": "v2:%s:%s:kit:%d" % (page, widget, ordinal),
                "csv_index": None, "card": card.get("id", card.get("name", "")),
                "card_title": card.get("title", ""), "row_label": row.get("label", ""),
                "xui_file": rel(path), "xui_line": None, "widget_name": widget,
                "widget_type": etype, "csv_kind": kind, "wiring_status": "GENERATED_KIT",
                "resolution": "generated_kit_xml", "action": action, "reason": reason,
                "known_gap": False, "suspect": False, "hazards": sorted(set(hazards)),
                "exercise": exercise, "test_value": test_value, "expect": expect,
                "restore": restore, "setting": setting or None,
                "setting_source": "kit_row setting" if setting else None,
                "setting_type": stype or None, "default": default,
                "min": lo, "max": hi, "increment": step,
                "combo_items": [{"label": label, "value": value} for label, value in items],
                "binding": binding, "link_to": row.get("link_to") or None,
                "shortcut_of": row.get("shortcut_of") or None,
                "callbacks": callbacks or None, "owner": owner or None,
                "evidence": rel(path),
            })
            if kind in ("vec2", "vec3"):
                component_count = 2 if kind == "vec2" else 3
                setting_size = len(default) if isinstance(default, list) else component_count
                for component in range(component_count):
                    component_entry = copy.deepcopy(entry)
                    axis = "xyz"[component]
                    component_default = (default[component]
                                         if isinstance(default, list) and component < len(default)
                                         else 0.0)
                    component_test = number_test(component_default, lo, hi, step, "F32")
                    component_entry.update({
                        "id": "%s:%s" % (entry["id"], axis),
                        "widget_name": "%s_%s" % (widget, axis),
                        "widget_type": "spinner",
                        "row_label": "%s %s" % (row.get("label", ""), axis.upper()),
                        "exercise": "set_value",
                        "test_value": component_test,
                        "expect": {"setting": setting, "type": "F32",
                                   "value": component_test},
                        "vector_component": component,
                        "vector_size": component_count,
                        "vector_setting_size": setting_size,
                        "default": component_default,
                    })
                    out.append(component_entry)
            else:
                out.append(entry)
    return out


# ---------------------------------------------------------------- classify

def classify(c):
    """Return (action, reason, hazards). action in RUN, SKIP, DEFER_INWORLD,
    EXPECT_MISSING, NO_EXERCISE."""
    hazards = []
    if c["status"] == "MISSING":
        return "EXPECT_MISSING", "no element with this name in this version", hazards
    if (c["page"], c["name"]) in PERSIST_WRITE_CONTROLS:
        reason, persist_hazards = PERSIST_WRITE_CONTROLS[(c["page"], c["name"])]
        return "SKIP", reason, list(persist_hazards)
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
    name = r["widget"]
    source_version = "v1" if v == "v2" and name in V2_LEGACY_OPERATOR_ROWS else v
    status = r[source_version + "_status"]
    base, line = parse_xui_field(r[source_version + "_xui"])
    label = r["label"]
    el = None if status == "MISSING" else xi.find(base, name)
    if status != "MISSING" and el is None:
        xi.warnings.append("element '%s' not found for %s on %s" % (name, v, r["page"]))

    setting, setting_src = "", ""
    if el is not None and el.get("control_name"):
        setting, setting_src = el.get("control_name"), "xui control_name"
    if not setting and r[source_version + "_bound"]:
        setting, setting_src = r[source_version + "_bound"], "csv bound"
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

    cb = r[source_version + "_callbacks"]
    owner = owner_from_evidence(r[source_version + "_evidence"])
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
    if name in FORCED_SAFE_SKIPS:
        reason, hazards = FORCED_SAFE_SKIPS[name]
        action = "SKIP"
        test_value = test_resolved = None
    if v == "v1" and name == "btn_prism_manage":
        action, reason, hazards = "RUN", "", ["modal"]

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
    if v == "v2" and r["page"] in LAUNCHER_HOSTS and \
            not r["v2_source"].startswith("redirect"):
        # Actors/Atmosphere contain native v2 controls as well as a launcher;
        # only rows explicitly sourced through redirect belong in Lightbox.
        entry["host"] = {"floater": "director", "page_view": r["page"],
                         "via": "tab"}
    elif entry["host"].get("floater") == "lightbox" and base in LIGHTBOX_TAB_BY_XUI:
        entry["host"]["tab"] = LIGHTBOX_TAB_BY_XUI[base]
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
        "resolution": r[source_version + "_source"],
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
        "evidence": r[source_version + "_evidence"] or None,
    })
    if name == "btn_load":
        entry["expect"] = {"callback": owner, "commit": True}
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
        # Runtime item matching must use the same declared type as the expected
        # setting.  In particular, radio XUI serializes Boolean values as "0"
        # and "1" strings; leaving those raw made false target the true segment.
        entry["options"] = [{"label": a, "value": coerce_item(b, stype)} for a, b in items]
    if test_resolved is not None and test_resolved != test_value:
        entry["test_value_resolved"] = test_resolved
    if name == "am_test_combo" or (v == "v2" and r["page"] == "actors_tab" and name == "am_test_trigger"):
        # This is a command, not durable state: its diagnostics listener either
        # arms the in-world capture sequence or reports it inconclusive, then
        # resets CineLightRigActorMaskTest to Idle on the next idle callback.
        # The login harness has no stable command-effect oracle (active actor-only
        # light plus rendered captures), so comparing the combo/setting to 1 is
        # intrinsically racy and incorrect.
        entry["action"] = "SKIP"
        entry["reason"] = (
            "command self-resets to Idle; requires an active actor-only light and "
            "rendered diagnostic captures for an effect oracle")
        entry["expect"] = {}
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
    if name in V2_LEGACY_OPERATOR_ROWS and v == "v2":
        entry["setting_source"] = (entry.get("setting_source") or "setting") + \
            " (v2 embedded legacy operator panel)"
        entry.setdefault("notes", []).append(
            "The v2 Operator orchestrator embeds the legacy operator panel; exercise its shared setting here.")
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
        owner = ("ALPanelDirectorVCam::onControl"
                 if version == "v2" and page == "vcam_tab" else None)
        e.update({"id": "%s:%s:%s:synthetic" % (version, page, widget),
                  "csv_index": None, "card": page_card, "card_title": title,
                  "row_label": title, "xui_file": rel(xi.path_of(base)) if base else "",
                  "xui_line": None, "widget_name": widget, "widget_type": "toggle",
                  "csv_kind": "", "wiring_status": "SYNTHETIC", "resolution": "synthetic",
                  "exercise": "toggle", "setting": setting, "setting_source": "probe",
                  "setting_type": stype or None, "default": default,
                  "min": None, "max": None, "increment": None, "callbacks": None,
                  "owner": owner, "evidence": None, "probe": family})
        hz = []
        if family in {"weather_master", "rain_gate"}:
            hz.append("requires_deferred_rendering")
        expect = {"setting": setting, "type": stype, "value": bool_test(default)}
        if family == "rain_gate" and bool_test(default) is False:
            expect["state"] = "effective rain intensity = 0"
        finish_synthetic(e, "RUN", "", hz, bool_test(default),
                         expect,
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
    # Compatibility path for old trees without a generated VCam kit page.  A
    # converted page is always inventoried later from its generated XML.
    if "vcam_tab" in GENERATED_KIT_PAGES:
        vcam = {"cards": [], "sections": []}
    else:
        with open(VCAM_PAGE_JSON, encoding="utf-8") as f:
            vcam = json.load(f)
    vcam_settings = {
        "vc_previews": "PrismMonitorPreviewsEnabled",
        "vc_preview_speed": "PrismMonitorPreviewFPS",
        "vc_lens_zoom": "PrismLensZoom",
        "vc_protect": "PrismAdaptivePerformance",
        "vc_main_fps": "PrismProtectedMainFPS",
        "vc_capture_budget": "PrismCaptureRefreshCeilingHz",
        "vc_quality_ceiling": "PrismLensResolutionScale",
    }
    kind_type = {"toggle": "toggle", "slider": "slider", "number": "spinner",
                 "combo": "combo", "segmented": "radio", "button": "button",
                 "color": "color"}
    custom_type = {"vcam_monitor_header": "button", "vcam_monitor_strip": "button", "vcam_monitor_pager": "button",
                   "vcam_monitor_tile": "button", "vcam_camera_selector": "button",
                   "vcam_face_list": "list", "vcam_projector_list": "list"}
    for card in vcam["cards"]:
        for row in card.get("rows", []):
            widget = row.get("w", "")
            etype = custom_type.get(row.get("custom_tag"), kind_type.get(row.get("k")))
            if not widget or not etype:
                continue
            exercise = EXERCISE.get(etype, "none")
            setting = vcam_settings.get(widget)
            sinfo = settings.get(setting, {}) if setting else {}
            default = sinfo.get("default", row.get("d"))
            test = None
            if exercise == "toggle":
                test = bool_test(default)
            elif etype == "color":
                test = color_test(default, "Color4")
            elif exercise == "set_value":
                test = number_test(default, row.get("mn"), row.get("mx"), row.get("st"),
                                   sinfo.get("type", "F32"))
            elif exercise == "select_item":
                values = row.get("iv", row.get("it", []))
                labels = row.get("it", [])
                items = list(zip(labels, values))
                test, _resolved = combo_test(default, items, sinfo.get("type", ""))
            elif exercise == "click":
                test = True
            c = {"status": "WIRED_CPP", "exercise": exercise, "etype": etype,
                 "name": widget, "label": row.get("l", ""), "page": "vcam_tab",
                 "reset_of": None, "version": "v2"}
            action, reason, hazards = classify(c)
            if widget == "vc_add_camera":
                action, reason, hazards = ("DEFER_INWORLD",
                    "camera creation needs a live view or in-world selection", ["requires_world"])
            if widget == "vc_remove_capture":
                action, reason, hazards = "SKIP", "destructive (remove capture)", ["requires_world"]
            if widget == "vc_source_type":
                action, reason, hazards = ("DEFER_INWORLD",
                    "test-pattern source needs the isolated in-world VCam fixture",
                    ["requires_world"])
                test = {"item_index": 3, "label": "Test pattern", "value": 3}
            expect = ({"setting": setting, "type": sinfo.get("type", ""), "value":
                       test.get("value") if isinstance(test, dict) and "value" in test else test}
                      if setting else {"ui": "VCam owner-installed handler fires"})
            if action in ("SKIP", "NO_EXERCISE"):
                test, expect = None, None
            e = base_entry(page_meta, "vcam_tab", "v2", "vcam_page.json")
            e.update({"id": "v2:vcam_tab:%s:vcam" % widget, "csv_index": None,
                      "card": card["id"], "card_title": card["t"],
                      "row_label": row.get("l", ""),
                      "xui_file": "indra/newview/skins/default/xui/en/panel_dpage_camera_vcam.xml",
                      "xui_line": None, "widget_name": widget, "widget_type": etype,
                      "csv_kind": row.get("k", ""), "wiring_status": "WIRED_CPP",
                      "resolution": "generated_vcam", "exercise": exercise,
                      "setting": setting, "setting_source": "vcam adapter" if setting else None,
                      "setting_type": sinfo.get("type"), "default": default,
                      "min": row.get("mn"), "max": row.get("mx"), "increment": row.get("st"),
                      "callbacks": "ALPanelDirectorVCam::onControl", "owner": "ALPanelDirectorVCam",
                      "evidence": "scripts/code_tools/director_kit/vcam_page.json",
                      "probe": "vcam_test_pattern" if widget == "vc_source_type" else "vcam_row",
                      "known_gap": False, "suspect": False})
            if widget == "vc_source_type":
                e["inworld_allowed"] = True
                expect = {"state": "selected capture source is test pattern"}
            finish_synthetic(e, action, reason, hazards, test, expect,
                             {"setting": setting, "value": default} if setting else
                             {"ui": "restore VCam row pre-test state"})
            out.append(e)
    # The rig feed combo is generated from director_mockup_data.json rather than
    # vcam_page.json.  Keep an explicit row with a registry oracle so a commit
    # can never PASS merely because its handler ran.
    e = base_entry(page_meta, "cine_light_rig_tab", "v2",
                   "synthetic:rig_projector_feed")
    e.update({"id": "v2:cine_light_rig_tab:cine_key_projector_feed:synthetic",
              "csv_index": None, "card": "cine_light_role_selector",
              "card_title": "Key", "row_label": "Projector feed",
              "xui_file": "indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml",
              "xui_line": None, "widget_name": "cine_key_projector_feed",
              "widget_type": "combo", "csv_kind": "combo",
              "wiring_status": "WIRED_CPP", "resolution": "generated_rig",
              "exercise": "select_item", "setting": None,
              "setting_source": "VCam projector registry", "setting_type": None,
              "default": None, "min": None, "max": None, "increment": None,
              "callbacks": "ALPanelCineLightRig::onProjectorFeedCommit",
              "owner": "ALPanelCineLightRig",
              "evidence": "scripts/code_tools/director_kit/director_mockup_data.json",
              "probe": "rig_projector_binding", "inworld_allowed": True})
    finish_synthetic(e, "DEFER_INWORLD",
                     "rig projector binding needs the isolated in-world VCam fixture",
                     ["requires_world"], {"item_index": 1, "resolve": "runtime"},
                     {"state": "stable rig-light registry binding is present"},
                     {"state": "isolated VCam fixture is restored after the run"})
    out.append(e)
    for index, section in enumerate(vcam.get("sections", [])):
        label = section.get("label", section.get("id", ""))
        widget = "vcam_chip_%d" % index
        e = base_entry(page_meta, "vcam_tab", "v2", "vcam_page.json:sections")
        e.update({"id": "v2:vcam_tab:%s:vcam" % widget, "csv_index": None,
                  "card": "signal_chain", "card_title": "Signal chain", "row_label": label,
                  "xui_file": "indra/newview/skins/default/xui/en/panel_dpage_camera_vcam.xml",
                  "xui_line": None, "widget_name": widget, "widget_type": "button",
                  "csv_kind": "chip", "wiring_status": "WIRED_CPP", "resolution": "kit",
                  "exercise": "click", "setting": None, "setting_source": None,
                  "setting_type": None, "default": None, "min": None, "max": None,
                  "increment": None, "callbacks": "ALKitPage::setChipIndex",
                  "owner": "ALKitPage", "evidence": "vcam_page.json sections",
                  "probe": "vcam_step_chip", "known_gap": False, "suspect": False})
        finish_synthetic(e, "RUN", "", [], True,
                         {"ui": "matching VCam signal-chain section is visible"}, None)
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
    w("- Coverage check: every interactive legacy CSV row yields one entry per applicable version; every converted v2 page is instead inventoried from its generated `panel_dpage_*.xml` `kit_row` metadata.")
    w("")
    return "\n".join(lines).rstrip() + "\n"


def enforce_hazard_skip_run_guard(entries):
    """Require review for new in-world rows and hazardous SKIP promotions."""
    if not os.path.exists(JSON_PATH):
        return
    with open(JSON_PATH, encoding="utf-8") as stream:
        previous = json.load(stream)
    if not os.path.exists(HAZARD_RUN_ALLOW_PATH):
        sys.exit("hazard SKIP->RUN guard failed: missing " + rel(HAZARD_RUN_ALLOW_PATH))
    with open(HAZARD_RUN_ALLOW_PATH, encoding="utf-8") as stream:
        allow_doc = json.load(stream)
    allowed = allow_doc.get("allow", {})
    if not isinstance(allowed, dict) or any(not isinstance(reason, str) or not reason.strip()
                                            for reason in allowed.values()):
        sys.exit("hazard SKIP->RUN guard failed: allow must map row keys to reviewed reasons")

    def key(row):
        return ":".join(str(row.get(field) or "")
                        for field in ("version", "page", "widget_name"))

    old = {key(row): row for row in previous.get("entries", [])}
    violations = []
    for row in entries:
        before = old.get(key(row))
        row_key = key(row)
        now_inworld = row.get("inworld_allowed") is True and \
            row.get("action") in {"RUN", "DEFER_INWORLD"}
        was_inworld = bool(before and before.get("inworld_allowed") is True and
                           before.get("action") in {"RUN", "DEFER_INWORLD"})
        needs_review = now_inworld and not was_inworld
        detail = "new in-world-allowed row"
        if before and before.get("action") == "SKIP" and \
                row.get("action") in {"RUN", "DEFER_INWORLD"}:
            reason = str(before.get("reason") or "").lower()
            hazard_skip = bool(before.get("hazards")) or any(token in reason for token in (
                "hazard", "destructive", "file picker", "server", "simulator",
                "animation request", "world action", "send", "delete", "remove"))
            if hazard_skip:
                needs_review = True
                detail = "was hazardous SKIP: " + str(before.get("reason") or "hazard")
        if needs_review and row_key not in allowed:
            violations.append(row_key + " (" + detail + ")")
    if violations:
        sys.exit("hazard SKIP->RUN guard failed; add reviewed entries to %s: %s" %
                 (rel(HAZARD_RUN_ALLOW_PATH), ", ".join(violations)))




def add_light_response_oracles(entries, page_meta):
    """Use isolated UUIDs and store fields, never widget echo as the value oracle."""
    fields = {
        "sld_diffuse": ("diffuse", "F32", 50, 0.5),
        "sld_tame": ("tame", "F32", 25, 0.25),
        "sld_brightness": ("ev", "F32", -1, -1),
        "sld_glow": ("glow", "F32", 150, 1.5),
        "sld_sil_amount": ("sil_amount", "F32", 40, 0.4),
        "chk_bypass": ("bypass", "Boolean", True, True),
        "chk_sil_haze": ("sil_haze", "Boolean", False, False),
        "chk_sil_keep_rim": ("sil_keep_rim", "Boolean", True, True),
        "combo_sil_fill": ("sil_fill", "Color3", 2, [1, 1, 1]),
        "swatch_sil_fill": ("sil_fill", "Color3", [0.2, 0.4, 0.6, 1], [0.2, 0.4, 0.6]),
    }
    fixtures = set(fields) | {
        "target_list", "chk_only_adjusted", "btn_refresh", "txt_brightness_ev",
        "btn_reset_selected", "btn_inherit_selected", "btn_copy", "btn_paste",
        "combo_preset", "btn_apply_preset",
    }
    # Preserve the legacy native-picker hazard when Takes is inventoried from kit XML.
    for e in entries:
        if e["version"] == "v2" and e["page"] == "takes_tab" and e["widget_name"] == "btn_load":
            e.update(action="SKIP", reason="opens a native file picker that cannot be safely automated at login")
    actors = [e for e in entries if e["version"] == "v2" and e["page"] == "actors_tab"]
    # The bar widgets are deliberately outside kit_row coverage, but still exercised.
    template = next(e for e in actors if e["widget_name"] == "btn_copy")
    for widget, kind, exercise in (("combo_preset", "combo", "select_item"),
                                    ("btn_apply_preset", "button", "click")):
        e = copy.deepcopy(template)
        e.update(id="v2:actors_tab:%s:bar" % widget, widget_name=widget,
                 widget_type=kind, exercise=exercise, source="synthetic:light_response_bar",
                 resolution="hosted_preset_bar", test_value=1)
        entries.append(e)
        actors.append(e)
    for e in actors:
        widget = e["widget_name"]
        if widget == "am_test_trigger":
            e.update(action="SKIP", expect={}, reason=(
                "command self-resets to Idle; requires an active actor-only light and "
                "rendered diagnostic captures for an effect oracle"))
        if widget == "am_reset_CineLightRigActorMaskBias":
            e.update(action="RUN", reason="", reset_widget="am_bias_slider", reset_pre_value=-20,
                     expect={"setting": "CineLightRigActorMaskBias", "type": "F32", "value": -8},
                     restore={"setting": "CineLightRigActorMaskBias", "value": "snapshot-before-test"})
        if widget not in fixtures:
            continue
        e.update(prereq="light_response_fixture", probe="light_response_store",
                 action="RUN", reason="", hazards=[], inworld_allowed=True,
                 restore={"state": "inherit each isolated fixture UUID without saving"})
        e["expect"] = {"mode": widget, "value": None}
        if widget in fields:
            field, stype, test, value = fields[widget]
            e.update(test_value=test, expect={"field": field, "type": stype, "value": value})
        if widget == "btn_inherit_selected":
            e.update(action="SKIP", reason="resident fixture has no wearer/source fallback; Inherit is disabled")
    diffuse = next(e for e in actors if e["widget_name"] == "sld_diffuse")
    multi = copy.deepcopy(diffuse)
    multi.update(id="v2:actors_tab:sld_diffuse:multi", source="synthetic:light_response_multi",
                 resolution="light_response_multiselect", fixture_count=2)
    entries.append(multi)
    negative = copy.deepcopy(diffuse)
    negative.update(id="v2:actors_tab:sld_diffuse:wrong_field", source="synthetic:light_response_negative",
                    resolution="light_response_negative_control", negative_control=True)
    negative["expect"]["field"] = "tame"
    entries.append(negative)

# ---------------------------------------------------------------- Director rule registry
#
# The viewer self-test (indra/newview/aluiperfselftest.cpp, "Director rule engine")
# enforces six user rules at runtime.  This section builds the page/link/popover
# registry it drives, lists every check so a new one cannot be added in C++
# without being declared here (and vice versa), and validates the registry.

FLOATER_V2_PATH = os.path.join(XUI_DIR, "floater_director_v2.xml")
SELFTEST_CPP = os.path.join(REPO, "indra", "newview", "aluiperfselftest.cpp")
GEN_REPORT_PATH = os.path.join(HERE, "director_kit", "gen_report.txt")
# Shrink-only pin (user-approved 10/13: lower as pages convert, never raise).  Mirrors the
# "legacy" count director_kit_gen.py pins in its classification table.
KNOWN_LEGACY_MAX = 13

DIRECTOR_RULES = collections.OrderedDict([
    ("R1_REACHABILITY", (
        "Every page and link is reachable through the real navigation code",
        ["rail click then tab click (gear click for Settings) selects the page: right group, "
         "tab toggled, visible in chain, non-zero size, inside the floater",
         "ALKitShell::navigateTo(page key) selects the same page from another page",
         "every link row and card link click lands on its target page (widget visible and highlighted)",
         "the generated registry matches the live shell (pages, rail groups)"])),
    ("R2_POPOVERS", (
        "Every shell popover opens correctly and closes both ways",
        ["anchor click opens exactly one popover of the expected name",
         "visible, front-most child of the shell at its centre, inside the shell rect, taller than the "
         "10 px stand-in, buttons inside its rect",
         "a second anchor click closes it; Esc closes it",
         "covers ACTION options, legend, hotkeys, scene menu, cast, search results, and per page "
         "Reset and Who popovers"])),
    ("R3_LABEL_FIT", (
        "No visible label is clipped",
        ["font text width against the drawn box for kit row labels, page-bar master labels, who chips, "
         "tab and rail labels, button labels, transport and popover labels",
         "clipped without elision = FAIL; elided = WARN listing the label",
         "every section chip of every page is walked so rows behind the active chip are measured"])),
    ("R4_RAW_NAMES", (
        "No internal id is ever shown to the user",
        ["help column text for every row, card and page (help column update called per item)",
         "every tooltip and visible text on each page, the shell chrome and the open popovers",
         "visible search result labels for a fixed query set, and all drawn kit text (help strip, card titles)",
         "detector self-test with planted names runs first, so a relaxed detector fails the run"])),
    ("R5_THEMED_PAGES", (
        "Every Director page is a themed kit page",
        ["plain checkbox, slider, spinner, tab container and button outside kit rows and cards are counted per page",
         "known-legacy pages are reported as 'legacy (not themed)' (WARN); any other page with one FAILS",
         "a page built as legacy that is not in the known-legacy list FAILS; the list can only shrink"])),
    ("R6_CONNECTED", (
        "Every interactive kit control has an observable effect",
        ["each interactive kit row maps to a RUN oracle that changes its setting, handler state, opens or navigates "
         "(the existing effect oracles stay as they were)",
         "no oracle, an oracle that proves nothing, or an unexplained exclusion FAILS; a SKIP with a reason is "
         "listed as WARN"])),
])

# Popovers the shell owns.  "anchor" is the button the user clicks; "via" selects the opening flow.
DIRECTOR_POPOVERS = [
    {"name": "kit_arm_popover", "anchor": "btn_arm", "via": "anchor", "xml": True},
    {"name": "legend_panel", "anchor": "btn_help", "via": "anchor", "xml": True},
    {"name": "kit_hotkeys_popover", "anchor": "btn_help", "via": "hotkeys", "xml": True},
    {"name": "kit_scene_popover", "anchor": "btn_scene_menu", "via": "anchor", "xml": True},
    {"name": "kit_cast_popover", "anchor": "btn_cast_chip", "via": "anchor", "xml": False},
    {"name": "kit_search_results", "anchor": "page_search_editor", "via": "search", "xml": False},
]
DIRECTOR_SEARCH_QUERIES = ["light", "exposure", "reset", "rim", "tron", "gaze", "step", "camera", "color", "shadow"]


def _load_floater_registry():
    """Rail items, groups and lazy pages of floater_director_v2.xml."""
    root = ET.parse(FLOATER_V2_PATH).getroot()
    rail = [(item.get("group"), item.get("label")) for item in root.iter()
            if item.tag.endswith("rail_item")]
    tabs = next((e for e in root.iter("tab_container") if e.get("name") == "director_tabs"), None)
    groups = []
    if tabs is not None:
        for panel in tabs.findall("panel"):
            inner = next(iter(panel.iter("tab_container")), None)
            pages = []
            if inner is not None:
                for lazy in inner.findall("kit_lazy_page"):
                    pages.append({"page": lazy.get("name"), "label": lazy.get("label", ""),
                                  "file": lazy.get("page_file", ""),
                                  "master_setting": lazy.get("master_setting", "")})
            groups.append({"group": panel.get("name"), "label": panel.get("label", ""), "pages": pages})
    return root, rail, groups


def _gen_report_legacy():
    legacy = set()
    if os.path.isfile(GEN_REPORT_PATH):
        with open(GEN_REPORT_PATH, encoding="utf-8") as stream:
            for line in stream:
                match = re.match(r"^(\w+)/(\w+): legacy;", line)
                if match:
                    legacy.add(match.group(2))
    return legacy


def build_director_rules():
    """Build the registry the viewer rule engine drives from the generated XUI."""
    floater, rail, groups = _load_floater_registry()
    rail_groups = [group for group, _label in rail]
    pages, links, widget_names = [], [], set()
    page_targets = {}
    errors = []
    for element in floater.iter():
        name = element.get("name")
        if name and "_" in name and len(name) >= 5:
            widget_names.add(name)
    for group in groups:
        for lazy in group["pages"]:
            key = "%s/%s" % (group["group"], lazy["page"])
            path = os.path.join(XUI_DIR, lazy["file"])
            entry = {"key": key, "group": group["group"], "page": lazy["page"], "label": lazy["label"],
                     "file": lazy["file"], "rail": group["group"] in rail_groups, "legacy": False}
            if not os.path.isfile(path):
                errors.append("page file missing for %s: %s" % (key, lazy["file"]))
                pages.append(entry)
                continue
            root = ET.parse(path).getroot()
            kit_page = next(root.iter("kit_page"), None)
            if kit_page is None:
                errors.append("%s has no kit_page element" % lazy["file"])
                pages.append(entry)
                continue
            if kit_page.get("page") != key:
                errors.append("%s declares page=%r but the shell lists it as %r" % (lazy["file"], kit_page.get("page"), key))
            entry["legacy"] = kit_page.get("legacy") == "true"
            targets = set()
            for element in root.iter():
                name = element.get("name")
                if name and "_" in name and len(name) >= 5:
                    widget_names.add(name)
                widget = element.get("widget")
                if widget:
                    targets.add(widget)
                    if "_" in widget and len(widget) >= 5:
                        widget_names.add(widget)
                if element.tag == "kit_card" and element.get("id"):
                    targets.add(element.get("id"))
            page_targets[key] = targets
            for element in root.iter():
                if element.tag not in ("kit_row", "kit_card") or element.get("kind") != "link":
                    continue
                link_to = element.get("link_to", "")
                if not link_to:
                    errors.append("%s has a link with no link_to" % key)
                    continue
                parts = [part for part in link_to.split("/") if part]
                target = "/".join(parts[:2])
                target_widget = parts[2] if len(parts) > 2 else ""
                links.append({"source": key, "kind": "card" if element.tag == "kit_card" else "row",
                              "id": element.get("id") if element.tag == "kit_card" else element.get("widget", ""),
                              "link_to": link_to, "target": target, "target_widget": target_widget})
            pages.append(entry)
    keys = {page["key"] for page in pages}
    for link in links:
        if link["target"] not in keys:
            errors.append("%s link %s points at %r, which is not a registered page" % (link["source"], link["id"], link["link_to"]))
        elif link["target_widget"] and link["target_widget"] not in page_targets.get(link["target"], set()):
            errors.append("%s link %s targets widget %r missing from %s" % (link["source"], link["id"], link["target_widget"], link["target"]))
        if not link["id"]:
            errors.append("%s has a link without a widget/id" % link["source"])
    legacy = sorted(page["page"] for page in pages if page["legacy"])
    if len(legacy) > KNOWN_LEGACY_MAX:
        errors.append("%d legacy pages exceed the shrink-only pin of %d: no new unthemed page may appear" % (len(legacy), KNOWN_LEGACY_MAX))
    report_legacy = _gen_report_legacy()
    if report_legacy and set(legacy) != report_legacy:
        errors.append("legacy pages in XUI %s differ from director_kit_gen classification %s" % (legacy, sorted(report_legacy)))
    shell_buttons = {e.get("name") for e in floater.iter() if e.tag == "button"}
    popover_xml = {e.get("name"): e.get("anchor") for e in floater.iter() if e.tag == "kit_popover"}
    for popover in DIRECTOR_POPOVERS:
        if popover["xml"]:
            if popover_xml.get(popover["name"]) != popover["anchor"]:
                errors.append("popover %s anchor mismatch in floater_director_v2.xml" % popover["name"])
        if popover["via"] != "search" and popover["anchor"] not in shell_buttons:
            errors.append("popover %s anchor %s is not a button in floater_director_v2.xml" % (popover["name"], popover["anchor"]))
    rules = {
        "version": 1,
        "rules": [{"id": rule_id, "title": title, "checks": checks}
                  for rule_id, (title, checks) in DIRECTOR_RULES.items()],
        "rail_groups": rail_groups,
        "groups": [{"group": g["group"], "label": g["label"], "rail": g["group"] in rail_groups,
                    "pages": [p["page"] for p in g["pages"]]} for g in groups],
        "pages": pages,
        "links": links,
        "known_legacy_pages": legacy,
        "legacy_pin": KNOWN_LEGACY_MAX,
        "popovers": [dict(popover) for popover in DIRECTOR_POPOVERS],
        "search_queries": list(DIRECTOR_SEARCH_QUERIES),
        "widget_names": sorted(widget_names),
    }
    return rules, errors


def validate_director_rules(rules, entries, cpp_text=None):
    """Return a list of problems with the rule registry; empty means the gate passes."""
    errors = []
    ids = [rule["id"] for rule in rules.get("rules", [])]
    if len(ids) != len(set(ids)) or not ids:
        errors.append("rule ids are missing or duplicated")
    pages = rules.get("pages", [])
    if not pages:
        errors.append("registry has no pages")
    leaves = {page["page"] for page in pages}
    covered = {e["page"] for e in entries if e.get("version") == "v2"}
    for page in pages:
        if page["key"] == "quick_group/quick_tab":
            continue
        if page["page"] not in covered:
            errors.append("page %s has no v2 functional entries (R6 would fail every control)" % page["key"])
    for link in rules.get("links", []):
        if link["target"] not in {page["key"] for page in pages}:
            errors.append("link %s -> %s has no registered target page" % (link["id"], link["link_to"]))
    if len(rules.get("known_legacy_pages", [])) > rules.get("legacy_pin", KNOWN_LEGACY_MAX):
        errors.append("known-legacy list exceeds its shrink-only pin")
    for popover in rules.get("popovers", []):
        if not popover.get("name") or not popover.get("anchor"):
            errors.append("popover entry without name/anchor: %r" % popover)
    if cpp_text is not None:
        for rule_id in ids:
            if '"%s"' % rule_id not in cpp_text:
                errors.append("%s is declared here but never emitted by aluiperfselftest.cpp" % rule_id)
        for used in sorted(set(re.findall(r'"(R[0-9]+_[A-Z_]+)"', cpp_text))):
            if used not in ids:
                errors.append("aluiperfselftest.cpp emits %s, which is not declared in DIRECTOR_RULES" % used)
        if "director_rules" not in cpp_text:
            errors.append("aluiperfselftest.cpp does not read the director_rules manifest block")
    _ = leaves
    return errors


def render_rules_md(rules):
    lines = ["", "## Director rule checks (runtime, `-Functional -DirectorV2` and `-InWorld`)", "",
             "These run after the manifest rows inside the same session. They are viewer-local UI checks "
             "(real clicks on shell chrome, navigation and help APIs, widget-tree reads) and never press a "
             "control that sends, saves, deletes or opens a file picker. Each emits PASS/FAIL/WARN rows "
             "(`kind=rule`) into `functional.csv`; any FAIL fails the run.", ""]
    for rule in rules["rules"]:
        lines.append("### %s - %s" % (rule["id"], rule["title"]))
        lines.append("")
        for check in rule["checks"]:
            lines.append("- %s" % check)
        lines.append("")
    legacy = rules["known_legacy_pages"]
    lines.append("Registry: %d pages in %d groups (%d rail groups, Settings via the gear), %d link rows/cards, "
                 "%d popovers, %d search queries." % (
                     len(rules["pages"]), len(rules["groups"]), len(rules["rail_groups"]),
                     len(rules["links"]), len(rules["popovers"]), len(rules["search_queries"])))
    lines.append("")
    lines.append("Known-legacy pages (%d of at most %d; shrink-only): %s." % (
        len(legacy), rules["legacy_pin"], ", ".join(legacy)))
    lines.append("")
    return "\n".join(lines)


def rules_self_test():
    """Adversarial fixtures: each planted fault must be reported by validate_director_rules."""
    good = {
        "rules": [{"id": "R1_REACHABILITY"}], "pages": [{"key": "a_group/a_tab", "page": "a_tab"}],
        "links": [], "known_legacy_pages": [], "legacy_pin": 1,
        "popovers": [{"name": "p", "anchor": "btn"}],
    }
    entries = [{"version": "v2", "page": "a_tab"}]
    cpp = '"R1_REACHABILITY" director_rules'
    assert not validate_director_rules(good, entries, cpp), "baseline fixture must pass"
    faults = {
        "link to a missing page": dict(good, links=[{"id": "x", "link_to": "a/b", "target": "a/b"}]),
        "legacy list over its pin": dict(good, known_legacy_pages=["x", "y"]),
        "page without functional entries": dict(good, pages=[{"key": "a_group/b_tab", "page": "b_tab"}]),
        "popover without an anchor": dict(good, popovers=[{"name": "p", "anchor": ""}]),
        "duplicate rule ids": dict(good, rules=[{"id": "R1_REACHABILITY"}, {"id": "R1_REACHABILITY"}]),
    }
    for label, fixture in faults.items():
        assert validate_director_rules(fixture, entries, cpp), "fixture not detected: " + label
    assert validate_director_rules(good, entries, cpp.replace("R1_REACHABILITY", "R9_X")), \
        "C++ rule drift not detected"
    assert validate_director_rules(good, entries, '"R1_REACHABILITY" "R7_UNDECLARED" director_rules'), \
        "undeclared C++ rule not detected"
    print("director_functional_manifest rule self-test: PASS (%d fixtures)" % (len(faults) + 3))


# ---------------------------------------------------------------- main

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json-output", default=JSON_PATH,
                        help="generated JSON path (default: checked-in manifest)")
    parser.add_argument("--markdown-output", default=MD_PATH,
                        help="generated Markdown path (default: checked-in report)")
    parser.add_argument("--check", action="store_true",
                        help="regenerate into a temp folder, validate the Director rule registry against "
                             "aluiperfselftest.cpp and the checked-in manifest, write nothing to the repo")
    parser.add_argument("--self-test", action="store_true",
                        help="run the rule-registry adversarial fixtures and exit")
    args = parser.parse_args()
    if args.self_test:
        rules_self_test()
        return
    if args.check:
        import tempfile
        scratch = tempfile.mkdtemp(prefix="director_manifest_check_")
        args.json_output = os.path.join(scratch, "functional_manifest.json")
        args.markdown_output = os.path.join(scratch, "FUNCTIONAL_MANIFEST.md")
    json_path = os.path.abspath(args.json_output)
    markdown_path = os.path.abspath(args.markdown_output)
    rows = load_csv()
    settings = load_settings()
    xi = XuiIndex()
    kit_pages = load_generated_kit_pages(xi)
    GENERATED_KIT_PAGES.update(kit_pages)
    presets, preset_written = load_rim_presets()

    page_meta, page_order = {}, []
    for r in rows:
        if r["page"] not in page_meta:
            page_meta[r["page"]] = (r["page_label"], r["group"])
            page_order.append(r["page"])
    for page in kit_pages:
        if page not in page_meta:
            page_meta[page] = (page, "")
            page_order.append(page)

    csv_pairs = set()
    for r in rows:
        if r["interactive"] == "Y":
            csv_pairs.add((r["page"], r["widget"]))

    entries = []
    for idx, r in enumerate(rows):
        if r["interactive"] != "Y":
            continue
        for v in VERSIONS:
            if v == "v2" and r["page"] in kit_pages:
                continue  # converted coverage comes from generated kit XML
            entries.append(build_csv_entry(idx, r, v, xi, settings, page_meta))

    synth = synthetic_entries(xi, settings, csv_pairs, page_meta, presets, preset_written)
    kit_widgets = {(page, row.get("widget", "")) for page, xml_rows in kit_pages.items()
                   for _base, _path, _kit, _card, row in xml_rows}
    synth = [entry for entry in synth
             if not (entry.get("version") == "v2" and entry.get("page") in kit_pages and
                     ((entry.get("page"), entry.get("widget_name")) in kit_widgets or
                      str(entry.get("source", "")).startswith("vcam_page.json")))]
    entries.extend(synth)
    entries.extend(build_kit_entries(kit_pages, rows, settings, page_meta))
    add_light_response_oracles(entries, page_meta)

    # Coverage check: every interactive CSV row must appear once per version.
    expected = {(i, v) for i, r in enumerate(rows) if r["interactive"] == "Y" for v in VERSIONS
                if not (v == "v2" and r["page"] in kit_pages)}
    got = [(e["csv_index"], e["version"]) for e in entries if e.get("csv_index") is not None]
    if len(got) != len(set(got)) or set(got) != expected:
        sys.exit("coverage check failed: expected %d csv entries, got %d" % (len(expected), len(got)))
    if len(set(e["page"] for e in entries)) != len(page_order):
        sys.exit("coverage check failed: page count mismatch")
    stale = [e["id"] for e in entries
             if e.get("version") == "v2" and e.get("page") in kit_pages and
             (e.get("host", {}).get("via") == "launcher" or
              str(e.get("source", "")).startswith("csv:") or
              str(e.get("resolution", "")).startswith("redirect"))]
    if stale:
        sys.exit("kit coverage check failed: launcher/legacy entries remain for generated kit pages: " +
                 ", ".join(stale[:20]))
    expected_kit_rows = []
    for page, xml_rows in kit_pages.items():
        for _base, _path, _kit, _card, row in xml_rows:
            widget = row.get("widget", "")
            if row.get("kind") in ("vec2", "vec3"):
                count = 2 if row.get("kind") == "vec2" else 3
                expected_kit_rows.extend((page, widget + "_" + "xyz"[component])
                                         for component in range(count))
            else:
                expected_kit_rows.append((page, widget))
    expected_kit = set(expected_kit_rows)
    got_kit_rows = [(e["page"], e["widget_name"]) for e in entries
                    if e.get("version") == "v2" and
                    e.get("resolution") == "generated_kit_xml"]
    got_kit = set(got_kit_rows)
    if len(expected_kit_rows) != len(expected_kit):
        sys.exit("kit coverage check failed: duplicate page/widget in generated kit XML")
    if got_kit != expected_kit or len(got_kit_rows) != len(expected_kit_rows):
        sys.exit("kit coverage check failed: expected %d generated rows, got %d" %
                 (len(expected_kit_rows), len(got_kit_rows)))
    legacy_owned_kit = [e["id"] for e in entries
                        if e.get("resolution") == "generated_kit_xml" and
                        e.get("binding") == "setting" and
                        (e.get("owner") or e.get("callbacks"))]
    if legacy_owned_kit:
        sys.exit("kit coverage check failed: bind=setting row carries a legacy-owner callback: " +
                 ", ".join(legacy_owned_kit[:20]))

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
        "generated_kit_pages": list(kit_pages),
        "generated_kit_rows": len(expected_kit),
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
    rules, rule_errors = build_director_rules()
    with open(SELFTEST_CPP, encoding="utf-8", errors="replace") as stream:
        cpp_text = stream.read()
    rule_errors += validate_director_rules(rules, entries, cpp_text)
    if rule_errors:
        sys.exit("Director rule registry check failed:\n  " + "\n  ".join(rule_errors))
    meta["director_rules"] = {"pages": len(rules["pages"]), "links": len(rules["links"]),
                              "popovers": len(rules["popovers"]), "known_legacy": len(rules["known_legacy_pages"])}
    doc = {"meta": meta, "entries": entries, "director_rules": rules}

    enforce_hazard_skip_run_guard(entries)

    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    os.makedirs(os.path.dirname(markdown_path), exist_ok=True)
    with open(json_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, ensure_ascii=True)
        f.write("\n")
    with open(json_path, encoding="utf-8") as f:
        check = json.load(f)
    assert len(check["entries"]) == len(entries)

    md = write_md(entries, meta, skip_rows, page_order, page_meta, synth) + render_rules_md(rules)
    with open(markdown_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(md)

    cnt = collections.Counter((e["version"], bucket(e)) for e in entries)
    print("entries=%d synthetic=%d pages=%d" % (len(entries), len(synth), len(page_order)))
    for v in VERSIONS:
        print(v, {a: cnt[(v, a)] for a in ACTION_ORDER})
    print("warnings=%d" % len(xi.warnings))
    print("director rules: %d pages, %d links, %d popovers, %d known-legacy (pin %d)" % (
        len(rules["pages"]), len(rules["links"]), len(rules["popovers"]),
        len(rules["known_legacy_pages"]), rules["legacy_pin"]))
    if args.check:
        # A stale checked-in manifest would feed the viewer an old registry.
        stale = []
        try:
            with open(JSON_PATH, encoding="utf-8") as stream:
                checked_in = json.load(stream)
            if checked_in.get("director_rules") != json.loads(json.dumps(rules)):
                stale.append("checked-in functional_manifest.json director_rules block is stale: "
                             "run python -I scripts/code_tools/director_functional_manifest.py")
            if len(checked_in.get("entries", [])) != len(entries):
                stale.append("checked-in functional_manifest.json has %d entries, regeneration has %d" % (
                    len(checked_in.get("entries", [])), len(entries)))
        except (OSError, ValueError) as exc:
            stale.append("cannot read checked-in manifest: %s" % exc)
        if stale:
            sys.exit("manifest check failed:\n  " + "\n  ".join(stale))
        print("manifest check: PASS (registry valid, checked-in manifest current)")
        return
    print("wrote", rel(json_path), rel(markdown_path))


if __name__ == "__main__":
    main()
