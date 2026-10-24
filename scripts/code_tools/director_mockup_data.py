#!/usr/bin/env python3
"""Merge curated Director layouts with content-map metadata into one compact
mockup data file (director_mockup_data.json).

Usage: director_mockup_data.py [--map PATH] [--curated DIR] [--out PATH]

Each curated row gets the control's real range/default/items/tooltip/setting
from the content map. Widgets that share a default XUI name (e.g. several
"check_box") are matched by control_name when the row carries one, else in
map order. Keys are short to keep the file small:
  rows: l label, k kind, a advanced, h help, w widget, s setting, d default,
        mn/mx/st/dc range, it items, ty widget type
  selector opts: label, value (the v1 combo item's LLSD value), rows; v is only
        the matched source-item ordinal and is never written to the setting
"""
import argparse
import json
from collections import defaultdict
from pathlib import Path

CUR = Path(r"C:\Users\xianw\cxs\content_curated")
MAP = Path(r"C:\Users\xianw\cxs\director_content_map.json")
OUT = Path(r"C:\Users\xianw\cxs\director_mockup\director_mockup_data.json")
GROUP_ORDER = ["cast_group", "performance_group", "camera_group", "lens_group",
               "light_group", "look_group", "world_group", "capture_group"]
ICONS = {"cast_group": "cast", "performance_group": "perf", "camera_group": "camera",
         "lens_group": "lens", "light_group": "light", "look_group": "look",
         "world_group": "world", "capture_group": "capture"}

# Binding user decision 2026-10-10: pages listed here are excluded from the
# generated Director data rather than deleted from the generated JSON by hand.
# Snapshot remains available through its existing floater.
PAGE_EXCLUSIONS = {
    "capture_group/capture_settings_tab": "Capture page dropped; use the Snapshot floater",
}

# Writable duplicates that are intentionally kept in an Easy card or a
# selector's mode-specific block. The named home remains canonical for search
# and undo attribution; kit_lint rejects every unlisted second writer.
SHORTCUTS = {
    ("cine_light_rig_tab", "cine_easy_cone_feather"): "projector_shafts_tab/ps_BDMergeProjectorVolumetricsFeather_slider",
    ("cine_light_rig_tab", "cine_easy_shaft_length"): "projector_shafts_tab/ps_BDMergeProjectorVolumetricsMaxDistance_slider",
    ("shot_tab", "CinematicCamJoint"): "frame_orbit_tab/CinematicCamJoint",
    ("shot_tab", "CinematicCamVertigoDuration"): "moves_tab/zolly_duration",
    ("shot_tab", "CinematicCamVertigoDirection"): "moves_tab/zolly_direction",
    ("shot_tab", "CinematicCamArcDuration"): "moves_tab/hero_arc_duration",
    ("shot_tab", "CinematicCamArcGeneratorAngle"): "moves_tab/hero_arc_angle",
    ("shot_tab", "CinematicCamArcHeightDrift"): "moves_tab/hero_arc_height_drift",
}


def apply_shortcuts(groups, problems):
    seen = set()
    for group in groups:
        for page in group["pages"]:
            for card in page["cards"]:
                rows = card.get("rows", []) + [row for option in card.get("opts", []) for row in option["rows"]]
                for row in rows:
                    key = (page["name"], row.get("w"))
                    if key in SHORTCUTS:
                        row["shortcut_of"] = SHORTCUTS[key]
                        seen.add(key)
    missing = sorted(set(SHORTCUTS) - seen)
    if missing:
        problems.append(f"shortcut rows not found: {missing}")


def apply_user_decisions(groups, problems):
    """Apply the small, explicit R4 page contract decided on 2026-10-10."""
    pages = {f"{g['name']}/{p['name']}": p for g in groups for p in g["pages"]}

    diopter = pages.get("lens_group/diopter_tab")
    if diopter:
        diopter.update({
            "sum": "Ultimate Diopter everyday controls: on/off, preset, strength and focus split. Detailed editing stays in the full editor.",
            "cards": [{"id": "diopter_summary", "t": "Ultimate Diopter", "k": "rows", "rows": [
                {"w": "diopter_enabled", "k": "toggle", "l": "On / off", "h": "Master switch. Off allocates no render targets and adds no draw calls.", "s": "CineDiopterEnabled", "d": False},
                {"w": "diopter_preset", "k": "combo", "l": "Preset", "h": "Sets the optical character while leaving framing and focus controls editable.", "s": "CineDiopterPreset", "d": 0,
                 "it": ["Custom (start from scratch)", "Split Diopter (classic)", "Dream Spot", "Anamorphic Cinema", "Vintage Swirl", "Hand Lens (free glass)", "Halo FX - Subtle", "Halo FX - Full", "Halo FX - Half Moon", "Halo FX - Split Moon", "Halo FX - Crescent", "Halo FX - Broken Ring", "Halo FX - Handheld", "Faceted - Radial Mirror", "Concentric Echo", "Prism Split", "Heart Bokeh", "Star Dream", "Moon Bite (crescent)", "Petal Dream (flower window)", "Macro Pulse"]},
                {"w": "diopter_blend", "k": "slider", "l": "Strength", "h": "Master A/B strength against the untouched frame. 0 shows the original image.", "s": "CineDiopterBlend", "mn": 0, "mx": 1, "st": 0.01, "dc": 2, "d": 1.0},
                {"w": "diopter_power", "k": "slider", "l": "Focus split", "h": "Optical power in diopters; positive values pull the lens-side focus plane nearer.", "s": "CineDiopterPower", "mn": -10, "mx": 10, "st": 0.25, "dc": 2, "d": 2.0},
                {"w": "diopter_open_editor", "k": "button", "l": "Full editor", "h": "Open the existing Ultimate Diopter window for detailed shape, glass, focus and blur editing.", "btn": "Open full editor", "target": "ultimate_diopter"},
            ]}],
            "sections": [{"id": "summary", "label": "Everyday controls", "cards": [0]}],
        })
    else:
        problems.append("user decisions: lens_group/diopter_tab not found")

    gaze = pages.get("performance_group/gaze_cues_tab")
    if gaze:
        gaze.update({
            "sum": "Gaze cue list and transport for the selected actor. Detailed keyframe editing stays in the existing cue window.",
            "cards": [{"id": "cues", "t": "Gaze cues", "k": "rows", "rows": [
                {"w": "cue_list", "k": "list", "l": "Cues", "h": "Time | Target | Macro. Selecting a row opens the existing Gaze Cues window at that cue."},
                {"w": "btn_cue_add", "k": "button", "l": "Add", "h": "Append a cue from the current editor defaults and select it.", "btn": "Add"},
                {"w": "btn_cue_delete", "k": "button", "l": "Remove", "h": "Remove the selected cue as one undoable list edit.", "btn": "Remove"},
                {"w": "gaze_cue_play", "k": "button", "l": "Play", "h": "Preview the selected cue through the scoped Actor Mover preview owned by WP8.", "btn": "Play"},
                {"w": "gaze_cue_stop", "k": "button", "l": "Stop", "h": "Clear the scoped cue preview and restore the actor's live authored gaze target.", "btn": "Stop"},
            ]}],
            "sections": [{"id": "cues", "label": "Cues", "cards": [0]}],
        })
    else:
        problems.append("user decisions: performance_group/gaze_cues_tab not found")

    light = pages.get("light_group/light_cues_tab")
    if light:
        light.update({
            "sum": "Lighting cue list and transport. Detailed snapshot and timing editing stays in the existing cue window.",
            "cards": [{"id": "cues", "t": "Light cues", "k": "rows", "rows": [
                {"w": "cue_rows", "k": "list", "l": "Cues", "h": "# | Label | At | Fade | Delay | Curve | Follow. Selecting a row opens the existing Lighting Cue Stack at that cue."},
                {"w": "cue_capture", "k": "button", "l": "Add", "h": "Append a complete snapshot of the live rig using the editor timing.", "btn": "Add"},
                {"w": "cue_delete", "k": "button", "l": "Remove", "h": "Remove the selected cue as one undoable list edit.", "btn": "Remove"},
                {"w": "cue_goto", "k": "button", "l": "Play", "h": "Run the selected cue through the existing GOTO transport.", "btn": "Play"},
                {"w": "cue_release", "k": "button", "l": "Stop", "h": "Release the current cue playback through the existing transport.", "btn": "Stop"},
            ]}],
            "sections": [{"id": "cues", "label": "Cues", "cards": [0]}],
        })
    else:
        problems.append("user decisions: light_group/light_cues_tab not found")

    for key in PAGE_EXCLUSIONS:
        group_name, page_name = key.split("/", 1)
        group = next((g for g in groups if g["name"] == group_name), None)
        page = next((p for p in (group or {}).get("pages", []) if p["name"] == page_name), None)
        if page is None:
            problems.append(f"page exclusion {key}: not found")
        else:
            group["pages"].remove(page)


def index_map(cmap):
    by_group = defaultdict(lambda: defaultdict(list))
    labels, page_order = {}, defaultdict(list)
    for g in cmap["groups"]:
        labels[g["name"]] = g.get("label") or g["name"]
        for p in g["pages"]:
            page_order[g["name"]].append(p["name"])
            for s in p.get("sections", []):
                for c in s.get("controls", []):
                    by_group[g["name"]][c["widget"]].append(c)
    chrome = defaultdict(list)
    for s in cmap.get("persistent_sections", []) or []:
        for c in s.get("controls", []):
            chrome[c["widget"]].append(c)
    return by_group, labels, page_order, chrome


def take(pool, widget, setting):
    cands = pool.get(widget) or []
    if not cands:
        return {}
    if setting:
        for i, c in enumerate(cands):
            if c.get("control_name") == setting:
                return cands.pop(i)
    return cands.pop(0)


def conv_row(r, pool):
    c = take(pool, r.get("widget"), r.get("control_name") or r.get("setting"))
    st = c.get("setting") or {}
    out = {"l": r.get("label") or c.get("label") or r.get("widget"), "k": r.get("kind", "custom"),
           "w": r.get("widget")}
    if r.get("advanced"):
        out["a"] = 1
    h = r.get("help") or c.get("tooltip") or st.get("comment")
    if h:
        out["h"] = h
    if r.get("keywords"):
        out["kw"] = r["keywords"]
    if c.get("control_name"):
        out["s"] = c["control_name"]
    if "default" in st:
        out["d"] = st["default"]
    for src, dst in (("min", "mn"), ("max", "mx"), ("increment", "st"), ("decimals", "dc")):
        if c.get(src) is not None:
            out[dst] = c[src]
    if c.get("items"):
        out["it"] = [i.get("label") if isinstance(i, dict) else i for i in c["items"]]
        # Kept only while IA is applied. Selector options must inherit the
        # combo's authored LLSD value, never manufacture a value from its index.
        out["_items"] = [
            {"label": i.get("label"), "value": _item_value(i.get("value"))}
            if isinstance(i, dict) else {"label": i, "value": index}
            for index, i in enumerate(c["items"])
        ]
    if c.get("type"):
        out["ty"] = c["type"]
    return out


import re


def _item_value(value):
    """Preserve a v1 combo item's scalar value in compact JSON-native form."""
    if not isinstance(value, str):
        return value
    if re.fullmatch(r"[-+]?\d+", value):
        return int(value)
    if re.fullmatch(r"[-+]?(?:\d+\.\d*|\d*\.\d+)(?:[eE][-+]?\d+)?", value):
        return float(value)
    return value

_DIR = re.compile(r"(heading|yaw|bearing|axis|azimuth|\bangle\b|\bfrom\b|\bto\b|offset|aim|roll)", re.I)
_TILT = re.compile(r"(pitch|tilt|elevation|dutch|eyeline|spin|roll)", re.I)
_RATE = re.compile(r"(speed|rate|per turn|angular|span|amount|period|limit|threshold|radial)", re.I)


def classify_angle(row):
    """Promote direction-angle sliders to dials (ALCompassDial) and signed tilt
    angles to arc dials (half-circle ALCompassDial mode, new). Rates, spans and
    magnitude limits stay sliders."""
    if row.get("k") not in ("slider", "number"):
        return
    mn, mx = row.get("mn"), row.get("mx")
    if not isinstance(mn, (int, float)) or not isinstance(mx, (int, float)):
        return
    text = " ".join(str(row.get(k, "")) for k in ("l", "s", "w"))
    if _RATE.search(text):
        return
    span = mx - mn
    if span >= 270 and _DIR.search(text):
        row["k"] = "dial"
    elif mn < 0 < mx and 45 <= max(-mn, mx) <= 180 and abs(mx + mn) <= 0.25 * span and _TILT.search(text):
        row["k"] = "arc"


def conv_cards(cards, pool):
    res = []
    for cd in cards or []:
        o = {"id": cd.get("id"), "t": cd.get("title") or cd.get("id"), "k": cd.get("kind", "rows")}
        if cd.get("advanced"):
            o["a"] = 1
        if cd.get("enable_widget"):
            ec = take(pool, cd["enable_widget"], None)
            o["en"] = 1
            # Kept for the IA v2 page master lookup and emitted in the data (WP0 snapshot).
            o["ew"] = cd["enable_widget"]
            if ec.get("control_name"):
                o["es"] = ec["control_name"]
        for key in ("target", "target_tab", "target_widget", "button_label", "help"):
            if cd.get(key):
                o[key] = cd[key]
        o["rows"] = [conv_row(r, pool) for r in cd.get("rows", []) or []]
        for r in o["rows"]:
            classify_angle(r)
        res.append(o)
    return res


def apply_ia(page, spec, problems, key):
    """Apply one page's ia_overlay entry (see content_curated/IA_SCHEMA.md):
    merge -> selector card, combine -> combined card with row sub-headings,
    rename, then sections as card-index lists. Problems are collected, not raised."""
    cards = page["cards"]
    by_id = {c["id"]: c for c in cards}
    used = set()

    def grab(cid, what):
        if cid not in by_id:
            problems.append(f"{key}: {what} references unknown card '{cid}'")
            return None
        if cid in used:
            problems.append(f"{key}: card '{cid}' used twice")
            return None
        used.add(cid)
        return by_id[cid]

    new_cards = {}   # first-source id -> replacement card
    for m in spec.get("merge", []) or []:
        opts, first = [], None
        for o in m.get("options", []):
            c = grab(o.get("card"), f"merge {m.get('id')}")
            if not c:
                continue
            first = first or c["id"]
            opt = {"label": o.get("label") or c["t"], "value": o.get("value", len(opts)),
                   "en": c.get("en", 0), "rows": c["rows"]}
            for k in ("ew", "es"):
                if c.get(k) is not None:
                    opt[k] = c[k]
            opts.append(opt)
        if first:
            new_cards[first] = {"id": m["id"], "t": m.get("title", m["id"]), "k": "selector",
                                "sel_label": m.get("selector_label", ""), "opts": opts, "rows": []}
    for cb in spec.get("combine", []) or []:
        rows, first = [], None
        for cid in cb.get("cards", []):
            c = grab(cid, f"combine {cb.get('id')}")
            if not c:
                continue
            first = first or c["id"]
            for r in c["rows"]:
                r2 = dict(r)
                if cb.get("subheads", True):
                    r2["sub"] = c["t"]
                rows.append(r2)
        if first:
            new_cards[first] = {"id": cb["id"], "t": cb.get("title", cb["id"]), "k": "combined", "rows": rows}
    out = []
    for c in cards:
        if c["id"] in new_cards:
            out.append(new_cards[c["id"]])
        elif c["id"] not in used:
            out.append(c)
    for cid, title in (spec.get("rename") or {}).items():
        for c in out:
            if c["id"] == cid:
                c["t"] = title
    page["cards"] = out
    if spec.get("summary"):
        page["sum"] = spec["summary"]
    if spec.get("sections"):
        idx = {c["id"]: i for i, c in enumerate(out)}
        placed, secs = set(), []
        for s in spec["sections"]:
            ids = []
            for cid in s.get("cards", []):
                if cid not in idx:
                    problems.append(f"{key}: section {s.get('id')} references unknown card '{cid}'")
                elif cid in placed:
                    problems.append(f"{key}: card '{cid}' in two sections")
                else:
                    placed.add(cid)
                    ids.append(idx[cid])
            secs.append({"id": s.get("id"), "label": s.get("label"), "cards": ids})
        for c in out:
            if c["id"] not in placed:
                problems.append(f"{key}: card '{c['id']}' is in no section")
        page["sections"] = secs


def apply_extras(groups, problems):
    """Render surfaces the XUI extractor skips (C++-drawn view_border / icon
    previews) plus approved NEW widgets. Rows are located by widget name."""
    def find_card(page, widget):
        for i, c in enumerate(page["cards"]):
            for r in c.get("rows", []) + [x for o in c.get("opts", []) for x in o["rows"]]:
                if r.get("w") == widget:
                    return i, c
        return None, None

    for g in groups:
        for p in g["pages"]:
            # Performance > Animate: the looping avatar preview viewport.
            i, c = find_card(p, "preview_hint")
            if c is not None and not any(r.get("w") == "animation_preview" for r in c["rows"]):
                c["rows"].insert(0, {"l": "Preview", "k": "viewport", "w": "animation_preview", "ty": "view_border",
                                     "h": "Previews the selected animation on loop. Drag to orbit, wheel to zoom."})
            if p["name"] != "cine_light_rig_tab":
                continue
            # Light > Rig: gobo preview swatch beside the gobo library.
            i, c = find_card(p, "cine_gobo_library") if True else (None, None)
            if c is None:
                for cc in p["cards"]:
                    if "gobo" in cc.get("t", "").lower():
                        c = cc
                        break
            if c is not None:
                c["rows"].insert(0, {"l": "Gobo", "k": "gobo", "w": "CineGobo01", "ty": "icon",
                                     "h": "Preview of the selected cookie/gobo pattern projected by the light."})
            else:
                problems.append("extras: Light > Rig gobo card not found")
            # Light > Rig: NEW 2.5D light map (user request 2026-10-09), first in "Lights".
            lm = {"id": "light_map", "t": "Light map", "k": "rigmap", "new": 1, "rows": [],
                  "help": "NEW: 2.5D view of the rig around the subject. Drag a light to set its yaw and pitch."}
            p["cards"].append(lm)
            new_idx = len(p["cards"]) - 1
            sec = next((s for s in p.get("sections", []) if s.get("id") == "lights"), None)
            if sec is not None:
                sec["cards"].insert(0, new_idx)
            else:
                problems.append("extras: Light > Rig 'lights' section not found")


# Page presets: the page's primary preset system, shown in the preset strip.
# widgets listed are the old in-card controls the strip replaces.
STRIP = {
    "light_group/cine_light_rig_tab": ("cine_rig_setups", ["cine_setup_combo", "cine_setup_save", "cine_setup_delete"]),
    "light_group/light_rim_tab": ("rig_rim", ["cine_rig_rim_preset"]),
    "light_group/exposure_tab": ("env_intensity", []),
    "light_group/actors_tab": ("@combo_preset", ["combo_preset", "btn_apply_preset"]),
    "world_group/weather_tab": ("weather", ["weather_preset_combo", "weather_preset_save", "weather_preset_delete"]),
    "light_group/projector_shafts_tab": ("proj_shafts", ["ps_preset"]),
    "camera_group/operator_tab": ("@preset_combo", ["preset_combo", "btn_preset_save", "btn_preset_delete"]),
    "performance_group/gaze_tab": ("@gaze_performance_combo", ["gaze_performance_combo", "reset_gaze_performance"]),
    "lens_group/diopter_tab": ("diopter_bank", []),
}
# Card presets: a compact preset picker in the card header.
CARD_PRESET = {
    "cine_easy_flare_preset": "rig_flare", "cine_flare_preset": "rig_flare",
    "cine_easy_night_mask_preset": "rig_night_mask", "cine_night_mask_preset": "rig_night_mask",
    "cine_fixture_preset": "rig_fixtures", "exp_gnd_preset": "exposure_hub_gnd", "gradnd_preset_combo": "exposure_hub_gnd",
    "exp_pol_preset": "exposure_hub_pol", "polarizer_preset_combo": "exposure_hub_pol",
    "exp_ir_preset": "lens_looks_ir", "ir_preset_action": "lens_looks_ir",
    "exp_softedge_preset": "lens_looks_soft", "softedge_preset_action": "lens_looks_soft",
    "lens_filter_preset": None, "shake_preset_combo": "cinecam",
    "FlycamDronePreset": "flycam_drone", "FlycamWeightedPreset": "flycam_weighted",
    "gaze_range_preset_combo": None, "gaze_ik_lean_combo": None,
}


def apply_presets(groups, presets, problems):
    prov = presets.get("providers", {})

    def all_rows(page):
        for c in page["cards"]:
            for r in c.get("rows", []):
                yield c, c["rows"], r
            for o in c.get("opts", []):
                for r in o["rows"]:
                    yield c, o["rows"], r

    for g in groups:
        for p in g["pages"]:
            key = f"{g['name']}/{p['name']}"
            inline = p.pop("inline_presets", False)  # inline_pages presets win over the provider
            if key in STRIP and not inline:
                src, widgets = STRIP[key]
                names = []
                if src.startswith("@"):
                    for _, _, r in all_rows(p):
                        if r.get("w") == src[1:]:
                            names = list(r.get("it") or [])
                else:
                    names = list((prov.get(src) or {}).get("names") or [])
                p["presets"] = names
                p["preset_user"] = bool((prov.get(src.lstrip("@")) or {}).get("user_folder")) or src in ("weather", "@preset_combo")
                for c, rows, r in list(all_rows(p)):
                    if r.get("w") in widgets:
                        rows.remove(r)
            for c, rows, r in list(all_rows(p)):
                w = r.get("w")
                if w in CARD_PRESET:
                    src = CARD_PRESET[w]
                    names = list((prov.get(src) or {}).get("names") or []) if src else list(r.get("it") or [])
                    r["cp"] = 1
                    r["it"] = names or r.get("it") or []
                    c.setdefault("card_presets", 0)
                    c["card_presets"] += 1


def apply_overrides(groups, cur_dir, problems):
    """content_curated/overrides/*.json: real kinds/ranges/items for rows the XUI
    extractor could only see as 'custom' (resolved from C++ by the override pass)."""
    od = cur_dir / "overrides"
    if not od.exists():
        return
    by_group = defaultdict(dict)
    for f in sorted(od.glob("*.json")):
        try:
            j = json.loads(f.read_text(encoding="utf-8"))
        except Exception as e:  # keep going; report the bad file
            problems.append(f"overrides: {f.name} unreadable: {e}")
            continue
        by_group[j.get("group")].update(j.get("rows", {}))
    keys = ("k", "l", "mn", "mx", "st", "dc", "it", "s", "h", "btn", "unit", "sample", "who")
    applied = 0
    for g in groups:
        ov = by_group.get(g["name"])
        if not ov:
            continue
        for p in g["pages"]:
            for c in p["cards"]:
                for r in c.get("rows", []) + [x for o in c.get("opts", []) for x in o["rows"]]:
                    # Unnamed widgets share default names ("check_box"): overrides may key
                    # them as "<widget>|<label>".
                    o = ov.get(f"{r.get('w')}|{r.get('l')}") or ov.get(r.get("w"))
                    if not o:
                        continue
                    for k in keys:
                        if k in o and o[k] not in (None, ""):
                            r[k] = o[k]
                    applied += 1
    # Line editors are real text inputs, not placeholders.
    texts = 0
    for g in groups:
        for p in g["pages"]:
            for c in p["cards"]:
                for r in c.get("rows", []) + [x for o in c.get("opts", []) for x in o["rows"]]:
                    if r.get("k") == "custom" and str(r.get("ty", "")).replace(" ", "_") in ("line_editor", "text_editor", "search_editor"):
                        r["k"] = "text"
                        texts += 1
    print(f"overrides applied to {applied} rows; {texts} line editors -> text")


IA2_KEYS = ("links", "master", "who", "modes", "add_rows", "remove_rows",
            "promote_rows", "reorder", "sections", "selector_binds", "rename")


def _as_list(x):
    """A spec value that may be one dict, a list, or absent."""
    if not x:
        return []
    return [x] if isinstance(x, dict) else list(x)


def _row_lists(card, option=None):
    """Row lists of a card: its own rows and every option's rows (option=None),
    or only the named option's rows (empty when that option is missing)."""
    if option is None:
        return [card.get("rows", [])] + [o["rows"] for o in card.get("opts", [])]
    return [o["rows"] for o in card.get("opts", []) if o["label"] == option]


def _page_rows(page):
    for c in page["cards"]:
        for rows in _row_lists(c):
            yield from rows


def _find_card(page, cid):
    return next((c for c in page["cards"] if c.get("id") == cid), None)


def _build_sections(page, sec_list, key, problems):
    """Sections given by card id -> page["sections"] as card-index lists."""
    idx = {c["id"]: i for i, c in enumerate(page["cards"])}
    placed, secs = set(), []
    for s in sec_list:
        ids = []
        for cid in s.get("cards", []):
            if cid not in idx:
                problems.append(f"{key}: section '{s.get('id')}' references unknown card '{cid}'")
            elif cid in placed:
                problems.append(f"{key}: card '{cid}' in two sections")
            else:
                placed.add(cid)
                ids.append(idx[cid])
        secs.append({"id": s.get("id"), "label": s.get("label"), "cards": ids})
    for c in page["cards"]:
        if c["id"] not in placed:
            problems.append(f"{key}: card '{c['id']}' is in no section")
    page["sections"] = secs


def _norm_item(s):
    return str(s).lower().replace("-the-", "-").split(" (")[0].strip()


def _match_item(label, items):
    """Index of the combo item an option names: exact, then base name (text before
    '('), then word prefix, then word suffix. None unless one tier gives one match."""
    n = _norm_item(label)
    tiers = [
        [i for i, it in enumerate(items) if it == label],
        [i for i, it in enumerate(items) if _norm_item(it) == n],
        [i for i, it in enumerate(items) if _norm_item(it).startswith(n + " ")],
        [i for i, it in enumerate(items) if _norm_item(it).endswith(" " + n)],
    ]
    for hits in tiers:
        if len(hits) == 1:
            return hits[0]
        if len(hits) > 1:
            return None
    return None


def _combo_items(page, widget, key, problems):
    row = next((r for r in _page_rows(page) if widget in (r.get("w"), r.get("s"))), None)
    if row is None or not row.get("it"):
        problems.append(f"{key}: selector_binds items_from '{widget}' is not a combo row with items")
        return None
    return row.get("_items") or [
        {"label": i[0] if isinstance(i, (list, tuple)) else i, "value": index}
        for index, i in enumerate(row["it"])
    ]


def _conv_v2_row(r):
    """Schema row (label/kind/help/widget/advanced) -> mockup row keys."""
    names = {"label": "l", "kind": "k", "help": "h", "widget": "w"}
    out = {}
    for k, v in r.items():
        if k.startswith("_"):
            continue
        if k == "advanced":
            if v:
                out["a"] = 1
        else:
            out[names.get(k, k)] = v
    return out


def _set_master(page, widget, key, problems):
    """The card header enable (en/ew) or a plain enable row becomes the page master.
    Returns (found, setting)."""
    for c in page["cards"]:
        if c.get("ew") == widget:
            c.pop("en", None)
            return True, c.get("es")
    for c in page["cards"]:
        for rows in _row_lists(c):
            for r in rows:
                if r.get("w") == widget:
                    rows.remove(r)
                    return True, r.get("s")
    problems.append(f"{key}: master widget '{widget}' not found")
    return False, None


def _set_who(page, spec, key, problems):
    pickers = list(spec.get("picker_widgets") or [])
    marked = 0
    for w in pickers:
        hits = [r for r in _page_rows(page) if r.get("w") == w]
        if not hits:
            problems.append(f"{key}: who picker '{w}' not found")
        for r in hits:
            r["who_picker"] = 1
            marked += 1
    page["who"] = {"label": spec.get("label"), "pickers": pickers}
    return marked


def _apply_v2_page(page, spec, key, page_keys, problems):
    stats = {"links": 0, "master": 0, "who": 0, "modes": 0, "added": 0}
    link_ids = {}  # replaced card id -> its link card id

    def resolve(cid):
        return link_ids.get(cid, cid)

    def card(cid):
        c = _find_card(page, resolve(cid))
        if c is None:
            problems.append(f"{key}: unknown card '{cid}'")
        return c

    # links: the card is swapped in place (same section slot and index) for a link card
    for ln in _as_list(spec.get("links")):
        cid, to = ln.get("card"), ln.get("to")
        idx = next((i for i, c in enumerate(page["cards"]) if c.get("id") == cid), None)
        if idx is None:
            problems.append(f"{key}: link card '{cid}' is not on this page")
            continue
        if to not in page_keys:
            problems.append(f"{key}: link '{cid}' targets unknown page '{to}'")
            continue
        old = page["cards"][idx]
        summary = []
        for w in ln.get("summary_rows") or []:
            hit = next((r for rows in _row_lists(old) for r in rows if r.get("w") == w), None)
            if hit is None:
                problems.append(f"{key}: link '{cid}' summary row '{w}' is not on that card")
            else:
                summary.append(dict(hit))
        link_ids[cid] = cid + "_link"
        page["cards"][idx] = {"id": cid + "_link", "t": ln.get("label") or old.get("t"), "k": "link",
                              "to": to, "summary": summary, "rows": []}
        stats["links"] += 1

    # selector_binds: take the combo's items now, before remove_rows deletes that row
    binds = []
    for sb in _as_list(spec.get("selector_binds")):
        sc = card(sb.get("card"))
        items = _combo_items(page, sb.get("items_from"), key, problems)
        if sc is None or items is None:
            continue
        if "opts" not in sc:
            problems.append(f"{key}: selector_binds card '{sc.get('id')}' is not a selector")
            continue
        binds.append((sc, sb.get("s"), items))

    if spec.get("master"):
        m = spec["master"]
        found, setting = _set_master(page, m.get("widget"), key, problems)
        if found:
            page["master"] = {"w": m.get("widget"), "l": m.get("label"), "s": setting}
            stats["master"] = 1

    if spec.get("who"):
        stats["who"] = _set_who(page, spec["who"], key, problems)

    for ar in _as_list(spec.get("add_rows")):
        c = card(ar.get("card"))
        if c is None:
            continue
        row = _conv_v2_row(ar.get("row") or {})
        row["added"] = 1
        if row.get("w") and any(r.get("w") == row["w"] for r in _page_rows(page)):
            problems.append(f"{key}: add_rows widget '{row['w']}' already exists on this page")
        if ar.get("option"):
            if "opts" not in c:
                problems.append(f"{key}: add_rows option '{ar['option']}' on non-selector '{c.get('id')}'")
                continue
            opt = next((o for o in c["opts"] if o["label"] == ar["option"]), None)
            if opt is None:  # the option does not exist yet: create it
                opt = {"label": ar["option"], "en": 0, "rows": []}
                c["opts"].append(opt)
            opt["rows"].append(row)
        else:
            c["rows"].append(row)
        stats["added"] += 1

    for pr in _as_list(spec.get("promote_rows")):
        c = card(pr.get("card"))
        if c is None:
            continue
        hit = next((r for rows in _row_lists(c, pr.get("option"))
                    for r in rows if r.get("w") == pr.get("w")), None)
        if hit is None:
            problems.append(f"{key}: promote row '{pr.get('w')}' not found on '{c.get('id')}'")
        else:
            hit.pop("a", None)

    for rr in _as_list(spec.get("remove_rows")):
        if rr.get("card"):
            c = card(rr["card"])
            scope = [c] if c is not None else []
        else:
            scope = page["cards"]
        removed = 0
        for c in scope:
            for rows in _row_lists(c):
                for r in list(rows):
                    if r.get("w") == rr.get("w"):
                        rows.remove(r)
                        removed += 1
        if scope and removed == 0:
            problems.append(f"{key}: remove_rows '{rr.get('w')}' not found")

    # Sections as card-id lists, snapshotted after links and before any reorder.
    sec_list = None
    if "sections" in page:
        sec_list = [{"id": s.get("id"), "label": s.get("label"),
                     "cards": [page["cards"][i]["id"] for i in s["cards"]]}
                    for s in page["sections"]]
    if spec.get("sections"):
        sec_list = [{"id": s.get("id"), "label": s.get("label"),
                     "cards": [resolve(c) for c in s.get("cards", [])]} for s in spec["sections"]]
    if spec.get("reorder"):
        by_id = {c["id"]: c for c in page["cards"]}
        order = []
        for cid in (resolve(c) for c in spec["reorder"].get("cards", [])):
            if cid not in by_id:
                problems.append(f"{key}: reorder card '{cid}' is not on this page")
            elif cid in order:
                problems.append(f"{key}: reorder card '{cid}' is listed twice")
            else:
                order.append(cid)
        for c in page["cards"]:
            if c["id"] not in order:
                problems.append(f"{key}: card '{c['id']}' is not in reorder; kept at the end")
                order.append(c["id"])
        page["cards"] = [by_id[cid] for cid in order]
    if sec_list is not None:
        _build_sections(page, sec_list, key, problems)

    for sc, setting, items in binds:
        sc["sel_setting"] = setting
        labels = [item["label"] for item in items]
        sc["it"] = labels
        for o in sc["opts"]:
            v = _match_item(o["label"], labels)
            if v is None:
                problems.append(f"{key}: option '{o['label']}' of '{sc.get('id')}' matches no item")
            else:
                o["v"] = v
                o["value"] = items[v]["value"]

    for ro, title in (spec.get("rename") or {}).items():
        c = card(ro)
        if c is not None:
            c["t"] = title

    if spec.get("modes"):
        page["modes"] = {k: v for k, v in spec["modes"].items() if not k.startswith("_")}
        stats["modes"] = 1
        for k, v in page["modes"].items():
            if k == "options" or not isinstance(v, list):
                continue
            for cid in v:
                if cid != "*" and _find_card(page, cid) is None:
                    problems.append(f"{key}: modes '{k}' lists unknown card '{cid}'")
    return stats


def _strip_enable_keys(groups):
    for g in groups:
        for p in g["pages"]:
            for c in p["cards"]:
                c.pop("ew", None)
                c.pop("es", None)


def apply_ia_v2(groups, cur_dir, problems):
    """content_curated/ia_overlay_v2.json, applied after ia_overlay.json.
    Schema: content_curated/IA_V2_SCHEMA.md; extensions are listed in its _meta."""
    path = cur_dir / "ia_overlay_v2.json"
    spec_all = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    pages = {f"{g['name']}/{p['name']}": p for g in groups for p in g["pages"]}
    for key, spec in spec_all.items():
        if key.startswith("_"):
            continue
        if key not in pages:
            problems.append(f"ia_overlay_v2 key '{key}' matches no page")
            continue
        for k in spec:
            if not k.startswith("_") and k not in IA2_KEYS:
                problems.append(f"{key}: unhandled ia_overlay_v2 key '{k}'")
        s = _apply_v2_page(pages[key], spec, key, set(pages), problems)
        print(f"  ia_v2 {key}: links={s['links']} master={s['master']} who={s['who']} "
              f"modes={s['modes']} added={s['added']}")


def _conv_inline_row(r):
    """inline_pages.json row -> mockup row. Vec3/vec2/link kinds and bind/range_assumed kept."""
    o = {"w": r.get("widget"), "k": r.get("kind", "custom"), "l": r.get("label") or r.get("widget")}
    if r.get("advanced"):
        o["a"] = 1
    if r.get("help"):
        o["h"] = r["help"]
    for k in ("s", "mn", "mx", "st", "dc", "d", "it", "to", "btn", "target", "target_tab",
              "range_assumed", "bind"):
        if k in r:
            o[k] = r[k]
    classify_angle(o)
    return o


def _conv_inline_card(c):
    rows = [_conv_inline_row(r) for r in c.get("rows", [])]
    o = {"id": c["id"], "t": c.get("title") or c["id"], "k": "rows", "rows": rows}
    en = c.get("enable_widget")
    if en and not any(r.get("w") == en for r in rows):
        o["en"] = 1
        o["ew"] = en
    return o


def _new_inline_page(name, entry, key, problems):
    page = {"name": name, "label": entry.get("label") or name, "sum": entry.get("summary", ""),
            "pre": "none", "cards": [_conv_inline_card(c) for c in entry.get("cards", [])]}
    if "presets" in entry:
        page["presets"] = entry["presets"]
        page["inline_presets"] = True  # apply_presets leaves these alone (popped there)
        if entry["presets"]:
            page["preset_user"] = True
    sec_list = entry.get("sections") or [{"id": "main", "label": "",
                                          "cards": [c["id"] for c in page["cards"]]}]
    _build_sections(page, sec_list, key, problems)
    return page


def _replace_inline_card(page, old_id, entry, key, problems):
    """Swap one card for the entry's cards. New cards take the old card's slot: the
    section with the same id as the entry gives them, else the old card's section."""
    old_ids = [c["id"] for c in page["cards"]]
    if old_id not in old_ids:
        problems.append(f"{key}: replace_card '{old_id}' is not on this page")
        return False
    entry_sec = {cid: s.get("id") for s in entry.get("sections") or [] for cid in s.get("cards", [])}
    secs = [{"id": s.get("id"), "label": s.get("label"), "cards": [old_ids[i] for i in s["cards"]]}
            for s in page.get("sections", [])]
    old_sec = next((s for s in secs if old_id in s["cards"]), None)
    pos = old_sec["cards"].index(old_id) if old_sec else 0
    if old_sec:
        old_sec["cards"].pop(pos)
    new_cards = [_conv_inline_card(c) for c in entry.get("cards", [])]
    for c in new_cards:
        target = next((s for s in secs if s["id"] == entry_sec.get(c["id"])), None) or old_sec
        if target is old_sec and old_sec is not None:
            old_sec["cards"].insert(pos, c["id"])
            pos += 1
        elif target is not None:
            target["cards"].append(c["id"])
    i = old_ids.index(old_id)
    page["cards"][i:i + 1] = new_cards
    if secs:
        _build_sections(page, secs, key, problems)
    return True


def apply_inline_pages(groups, cur_dir, problems):
    """content_curated/inline_pages.json (optional). Entries replace a page (replace_page),
    replace one card of a page (replace_card), or insert a new page after one (insert_after)."""
    path = cur_dir / "inline_pages.json"
    if not path.exists():
        return
    data = json.loads(path.read_text(encoding="utf-8"))
    entries = data.get("pages", []) if isinstance(data, dict) else data
    n_rep = n_ins = n_card = 0
    for e in entries:
        g = next((g for g in groups if g["name"] == e.get("group")), None)
        if g is None:
            problems.append(f"inline_pages: unknown group '{e.get('group')}'")
            continue
        names = [p["name"] for p in g["pages"]]
        if e.get("replace_card"):
            key = f"{g['name']}/{e.get('replace_page')}"
            if e.get("replace_page") not in names:
                problems.append(f"inline_pages: replace_card page '{key}' not found")
                continue
            page = g["pages"][names.index(e["replace_page"])]
            if _replace_inline_card(page, e["replace_card"], e, key, problems):
                n_card += 1
        elif e.get("insert_after"):
            key = f"{g['name']}/{e.get('insert_after')}"
            if e["insert_after"] not in names:
                problems.append(f"inline_pages: insert_after page '{key}' not found")
                continue
            name = re.sub(r"[^a-z0-9]+", "_", str(e.get("label", "")).lower()).strip("_") + "_tab"
            if name in names:
                problems.append(f"inline_pages: page name '{g['name']}/{name}' already exists")
                continue
            page = _new_inline_page(name, e, f"{g['name']}/{name}", problems)
            g["pages"].insert(names.index(e["insert_after"]) + 1, page)
            n_ins += 1
        else:
            key = f"{g['name']}/{e.get('replace_page')}"
            if e.get("replace_page") not in names:
                problems.append(f"inline_pages: replace_page '{key}' not found")
                continue
            old = g["pages"][names.index(e["replace_page"])]
            page = _new_inline_page(old["name"], e, key, problems)
            page["pre"] = old.get("pre", "none")
            g["pages"][names.index(e["replace_page"])] = page
            n_rep += 1
    print(f"inline_pages: {n_rep} pages replaced, {n_ins} inserted, {n_card} cards replaced")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", type=Path, default=MAP)
    ap.add_argument("--curated", type=Path, default=CUR)
    ap.add_argument("--out", type=Path, default=OUT)
    a = ap.parse_args()
    cmap = json.loads(a.map.read_text(encoding="utf-8"))
    by_group, labels, page_order, chrome_pool = index_map(cmap)
    pages_by_group = defaultdict(list)
    chrome = None
    for f in sorted(a.curated.glob("*.json")):
        cur = json.loads(f.read_text(encoding="utf-8"))
        g = cur.get("group")
        if not g:
            continue
        pool = by_group[g]
        for p in cur.get("pages", []):
            pages_by_group[g].append({"name": p.get("name"), "label": p.get("label"),
                                      "sum": p.get("summary", ""), "pre": p.get("preset_provider", "none"),
                                      "cards": conv_cards(p.get("cards"), pool)})
        if isinstance(cur.get("chrome"), dict):
            chrome = {"cards": conv_cards(cur["chrome"].get("cards"), chrome_pool)}
    groups = []
    for g in GROUP_ORDER:
        order = page_order.get(g, [])
        pages = sorted(pages_by_group.get(g, []),
                       key=lambda p: (order.index(p["name"]) if p["name"] in order else
                                      (order.index("cine_light_rig_tab") + 0.5 if "cine_light_rig_tab" in order else 99)))
        groups.append({"name": g, "label": labels.get(g, g), "icon": ICONS[g], "pages": pages})
    ia_path = a.curated / "ia_overlay.json"
    problems = []
    if ia_path.exists():
        ia = json.loads(ia_path.read_text(encoding="utf-8"))
        for g in groups:
            for p in g["pages"]:
                key = f"{g['name']}/{p['name']}"
                if key in ia:
                    apply_ia(p, ia[key], problems, key)
        for k in ia:
            if not any(f"{g['name']}/{p['name']}" == k for g in groups for p in g["pages"]):
                problems.append(f"ia_overlay key '{k}' matches no page")
    # Page moves (user 2026-10-09: "VCam is a camera and should be under camera").
    # The moved page renders a dedicated sub-panel component (built separately).
    PAGE_MOVES = {"lens_group/vcam_tab": ("camera_group", "VCamPage")}
    for key, (dest, component) in PAGE_MOVES.items():
        src_g, page_name = key.split("/")
        sg = next((g for g in groups if g["name"] == src_g), None)
        dg = next((g for g in groups if g["name"] == dest), None)
        page = next((p for p in (sg or {}).get("pages", []) if p["name"] == page_name), None)
        if page is None or dg is None:
            problems.append(f"page move {key} -> {dest}: not found")
            continue
        sg["pages"].remove(page)
        page["component"] = component
        dg["pages"].append(page)
    apply_extras(groups, problems)
    apply_overrides(groups, a.curated, problems)
    apply_ia_v2(groups, a.curated, problems)
    apply_inline_pages(groups, a.curated, problems)
    apply_user_decisions(groups, problems)
    apply_shortcuts(groups, problems)
    pre_path = a.curated / "presets.json"
    if pre_path.exists():
        apply_presets(groups, json.loads(pre_path.read_text(encoding="utf-8")), problems)
    for pr in problems:
        print("IA PROBLEM:", pr)
    # Normalise item/preset lists to plain label strings (inline Lightbox pages
    # carry [label, value] pairs and preset dicts).
    def _lab(x):
        if isinstance(x, dict):
            return x.get("label") or x.get("name") or str(x.get("value", ""))
        if isinstance(x, (list, tuple)) and x:
            return str(x[0])
        return x
    for g in groups:
        for pg in g["pages"]:
            if isinstance(pg.get("presets"), list):
                pg["presets"] = [_lab(x) for x in pg["presets"]]
            for c in pg["cards"]:
                for r in c.get("rows", []) + [x for o in c.get("opts", []) for x in o["rows"]] + c.get("summary", []):
                    r.pop("_items", None)
                    if isinstance(r.get("it"), list):
                        r["it"] = [_lab(x) for x in r["it"]]
    data = {"groups": groups, "chrome": chrome}
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    n_rows = sum(len(c["rows"]) for g in groups for p in g["pages"] for c in p["cards"])
    n_opt = sum(len(o["rows"]) for g in groups for p in g["pages"] for c in p["cards"] for o in c.get("opts", []))
    print(f"wrote {a.out} ({a.out.stat().st_size} bytes): {len(groups)} groups, "
          f"{sum(len(g['pages']) for g in groups)} pages, {n_rows} rows "
          f"(+{n_opt} selector option rows = {n_rows + n_opt})")
    for g in groups:
        print(" ", g["label"], [p["label"] for p in g["pages"]])


if __name__ == "__main__":
    main()
