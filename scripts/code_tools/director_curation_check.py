#!/usr/bin/env python3
"""Validate curated Director layouts against the extracted content map.

Usage: director_curation_check.py [--map PATH] [--curated DIR]

Every control in director_content_map.json must appear exactly once per group,
either in a card's rows or in `dropped` (with a reason). Controls may move
between pages of the same group (e.g. Light > Rig -> Light > Rim). The
persistent Director-chrome section may be placed under any curated file's
top-level "chrome" key. Also checks the brief's label/help/card-size rules.
Exit code 1 on any error; warnings don't fail.
"""
import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

DEF_MAP = Path(r"C:\Users\xianw\cxs\director_content_map.json")
DEF_CUR = Path(r"C:\Users\xianw\cxs\content_curated")


def map_widgets(cmap):
    """group name -> set of widget names; plus chrome widgets."""
    # Multisets: unnamed XUI widgets share default names (e.g. 7 x "check_box"),
    # so parity is checked by COUNT per name, not by unique names.
    groups = defaultdict(Counter)
    chrome = set()
    for g in cmap.get("groups", []):
        for p in g.get("pages", []):
            for s in p.get("sections", []):
                for c in s.get("controls", []):
                    groups[g["name"]][c["widget"]] += 1
            for l in p.get("launchers", []):
                if isinstance(l, dict) and l.get("name"):
                    groups[g["name"]][l["name"]] += 1
    for key in ("persistent_sections", "chrome", "persistent", "director_chrome"):
        sec = cmap.get(key)
        if isinstance(sec, list):
            for s in sec:
                for c in s.get("controls", []):
                    chrome.add(c["widget"])
        elif isinstance(sec, dict):
            for c in sec.get("controls", []):
                chrome.add(c["widget"])
    return groups, chrome


def collect_rows(cards, out, where):
    for card in cards or []:
        for key in ("enable_widget", "badge_widget", "target_widget", "widget"):
            if card.get(key):
                out.append((card[key], where + "/" + card.get("id", "?") + ":" + key))
        for r in card.get("rows", []) or []:
            out.append((r.get("widget"), where + "/" + card.get("id", "?")))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", type=Path, default=DEF_MAP)
    ap.add_argument("--curated", type=Path, default=DEF_CUR)
    a = ap.parse_args()
    cmap = json.loads(a.map.read_text(encoding="utf-8"))
    groups, chrome = map_widgets(cmap)
    errors, warns = [], []
    seen_groups = set()
    chrome_placed = []
    placed_by_group = defaultdict(list)
    for f in sorted(a.curated.glob("*.json")):
        cur = json.loads(f.read_text(encoding="utf-8"))
        gname = cur.get("group")
        if not gname:
            continue
        seen_groups.add(gname)
        placed = placed_by_group[gname]   # a group may span several files (e.g. a big page split out)
        for p in cur.get("pages", []):
            where = f"{gname}/{p.get('name')}"
            collect_rows(p.get("cards"), placed, where)
            for d in p.get("dropped", []) or []:
                if not d.get("reason"):
                    errors.append(f"{where}: dropped {d.get('widget')} has no reason")
                placed.append((d.get("widget"), where + "/dropped"))
            for c in p.get("cards", []) or []:
                rows = c.get("rows", []) or []
                prim = [r for r in rows if not r.get("advanced")]
                if c.get("kind", "rows") == "rows" and not (3 <= len(prim) <= 7):
                    warns.append(f"{where}/{c.get('id')}: {len(prim)} primary rows (want 3-7)")
                if len(c.get("title", "")) > 22:
                    warns.append(f"{where}/{c.get('id')}: title > 22 chars")
                for r in rows:
                    if len(r.get("label", "")) > 14:
                        warns.append(f"{where}/{c.get('id')}/{r.get('widget')}: label '{r.get('label')}' > 14 chars")
                    if len(r.get("help", "")) > 140:
                        warns.append(f"{where}/{c.get('id')}/{r.get('widget')}: help > 140 chars")
                    if r.get("kind") != "custom" and not r.get("help"):
                        warns.append(f"{where}/{c.get('id')}/{r.get('widget')}: no help")
        if isinstance(cur.get("chrome"), dict):
            collect_rows(cur["chrome"].get("cards"), chrome_placed, f"{gname}/chrome")
            for d in cur["chrome"].get("dropped", []) or []:
                chrome_placed.append((d.get("widget"), f"{gname}/chrome/dropped"))
    for gname, placed in sorted(placed_by_group.items()):
        expected = groups.get(gname, Counter())
        counts = Counter(w for w, _ in placed if w)
        for w, n in counts.items():
            if w in expected and n > expected[w]:
                errors.append(f"{gname}: {w} placed {n} times (map has {expected[w]})")
        missing = {w for w in expected if counts.get(w, 0) < expected[w]}
        extra = set(counts) - set(expected) - chrome
        for w in sorted(missing):
            have = counts.get(w, 0)
            errors.append(f"{gname}: MISSING {w}" + (f" ({have}/{expected[w]} placed)" if expected[w] > 1 else ""))
        for w in sorted(extra):
            errors.append(f"{gname}: UNKNOWN widget {w} (not in content map for this group)")
        print(f"{gname}: expected {len(expected)}, placed {len(counts)}, missing {len(missing)}, unknown {len(extra)}")
    for g in sorted(set(groups) - seen_groups):
        errors.append(f"group {g}: no curated file")
    if chrome:
        cc = Counter(w for w, _ in chrome_placed if w)
        for w in sorted(chrome - set(cc)):
            errors.append(f"chrome: MISSING {w}")
        print(f"chrome: expected {len(chrome)}, placed {len(cc)}")
    for w in warns[:60]:
        print("warn:", w)
    if len(warns) > 60:
        print(f"... {len(warns) - 60} more warnings")
    for e in errors[:80]:
        print("ERROR:", e)
    if len(errors) > 80:
        print(f"... {len(errors) - 80} more errors")
    print(f"{len(errors)} errors, {len(warns)} warnings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
