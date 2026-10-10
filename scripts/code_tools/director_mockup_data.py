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
        out["it"] = [i.get("label") if isinstance(i, dict) else i for i in c["items"]][:40]
    if c.get("type"):
        out["ty"] = c["type"]
    return out


import re

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
            take(pool, cd["enable_widget"], None)
            o["en"] = 1
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
            opts.append({"label": o.get("label") or c["t"], "en": c.get("en", 0), "rows": c["rows"]})
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
    "world_group/atmosphere_tab": ("proj_shafts", ["ps_preset"]),
    "camera_group/operator_tab": ("@preset_combo", ["preset_combo", "btn_preset_save", "btn_preset_delete"]),
    "performance_group/gaze_tab": ("@gaze_performance_combo", ["gaze_performance_combo", "reset_gaze_performance"]),
    "lens_group/diopter_tab": ("diopter_bank", []),
}
# Card presets: a compact preset picker in the card header.
CARD_PRESET = {
    "cine_easy_flare_preset": "rig_flare", "cine_flare_preset": "rig_flare",
    "cine_easy_night_mask_preset": "rig_night_mask", "cine_night_mask_preset": "rig_night_mask",
    "cine_fixture_preset": None, "exp_gnd_preset": "exposure_hub_gnd", "gradnd_preset_combo": "exposure_hub_gnd",
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
            if key in STRIP:
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
    apply_extras(groups, problems)
    pre_path = a.curated / "presets.json"
    if pre_path.exists():
        apply_presets(groups, json.loads(pre_path.read_text(encoding="utf-8")), problems)
    for pr in problems:
        print("IA PROBLEM:", pr)
    data = {"groups": groups, "chrome": chrome}
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    n_rows = sum(len(c["rows"]) for g in groups for p in g["pages"] for c in p["cards"])
    print(f"wrote {a.out} ({a.out.stat().st_size} bytes): {len(groups)} groups, "
          f"{sum(len(g['pages']) for g in groups)} pages, {n_rows} rows")
    for g in groups:
        print(" ", g["label"], [p["label"] for p in g["pages"]])


if __name__ == "__main__":
    main()
