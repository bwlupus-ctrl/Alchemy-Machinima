#!/usr/bin/env python3
"""Verify style_tokens.json against the Director mockup sources.

Checks (stdlib only):
  1. Every hex colour (#RRGGBB or #RGB) written in any string value of
     style_tokens.json appears literally (case-insensitive) in one of the
     mockup source files.
  2. Every rgba(...) literal appears literally in the sources.
  3. Every NNpx / NN.NNpx token appears literally in the sources.
  4. Every icons/*.svg file exists, is referenced from the JSON, and its
     full text appears verbatim in mockup_src/Director.dc.html.

Exit code 0 = pass, 1 = fail.
Run:  python -I verify_tokens.py
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOKENS = os.path.join(HERE, "style_tokens.json")
SRC_DIR = os.path.join(HERE, "mockup_src")
ICON_DIR = os.path.join(HERE, "icons")
SOURCES = ["Director.dc.html", "Main.dc.html", "Kit.dc.html", "VCamPage.dc.html"]
PRIMARY = "Director.dc.html"

HEX_RE = re.compile(r"#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{3})(?![0-9A-Fa-f])")
RGBA_RE = re.compile(r"rgba\([^)]*\)")
PX_RE = re.compile(r"(?<![\w.])(\d+(?:\.\d+)?)px\b")


def load_sources():
    texts = {}
    for name in SOURCES:
        path = os.path.join(SRC_DIR, name)
        with open(path, encoding="utf-8") as f:
            texts[name] = f.read()
    return texts


def walk_strings(node, path="$"):
    """Yield (path, string) for every string in the JSON tree."""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from walk_strings(v, path + "." + k)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from walk_strings(v, "%s[%d]" % (path, i))
    elif isinstance(node, str):
        yield path, node


def main():
    with open(TOKENS, encoding="utf-8") as f:
        data = json.load(f)
    texts = load_sources()
    all_src_lower = {k: v.lower() for k, v in texts.items()}
    any_src = lambda lit: any(lit.lower() in s for s in all_src_lower.values())

    failures = []
    counts = {"hex": set(), "rgba": set(), "px": set(), "strings": 0}

    for path, s in walk_strings(data):
        counts["strings"] += 1
        for m in HEX_RE.findall(s):
            counts["hex"].add(m.upper())
            if not any_src(m):
                failures.append("hex %s not in sources (at %s)" % (m, path))
        for m in RGBA_RE.findall(s):
            counts["rgba"].add(m)
            if not any_src(m):
                failures.append("rgba %s not in sources (at %s)" % (m, path))
        for num in PX_RE.findall(s):
            tok = num + "px"
            counts["px"].add(tok)
            if not any_src(tok):
                failures.append("px %s not in sources (at %s)" % (tok, path))

    # Also check the hex colour keys in the colors section have a hex field
    colors = data.get("colors", {})
    n_colour_entries = 0
    for name, entry in colors.items():
        if name.startswith("_") or not isinstance(entry, dict):
            continue
        if "hex" in entry:
            n_colour_entries += 1
            if not HEX_RE.fullmatch(entry["hex"]):
                failures.append("colors.%s.hex is not a hex colour: %r" % (name, entry["hex"]))
    # Nested lightmap_scene group
    for name, entry in colors.get("lightmap_scene", {}).items():
        if isinstance(entry, dict) and "hex" in entry:
            n_colour_entries += 1

    # Icons
    icons = data.get("icons", {})
    icon_files_json = set()
    director_text = texts[PRIMARY]
    n_icons = 0
    for key, entry in icons.items():
        if key.startswith("_") or not isinstance(entry, dict):
            continue
        n_icons += 1
        rel = entry.get("file")
        if not rel:
            failures.append("icons.%s has no file" % key)
            continue
        icon_files_json.add(os.path.basename(rel))
        full = os.path.join(HERE, rel.replace("/", os.sep))
        if not os.path.isfile(full):
            failures.append("icon file missing: %s" % rel)
            continue
        with open(full, encoding="utf-8") as f:
            svg = f.read().strip()
        if svg not in director_text:
            failures.append("icon %s text is not verbatim in %s" % (rel, PRIMARY))

    on_disk = set(f for f in os.listdir(ICON_DIR) if f.endswith(".svg")) if os.path.isdir(ICON_DIR) else set()
    for orphan in sorted(on_disk - icon_files_json):
        failures.append("icons/%s exists on disk but is not listed in style_tokens.json" % orphan)
    for missing in sorted(icon_files_json - on_disk):
        failures.append("icons/%s listed in JSON but not on disk" % missing)

    print("style_tokens.json checked against %d source files" % len(SOURCES))
    print("  string values scanned : %d" % counts["strings"])
    print("  distinct hex colours  : %d (colour entries with hex: %d)" % (len(counts["hex"]), n_colour_entries))
    print("  distinct rgba()       : %d" % len(counts["rgba"]))
    print("  distinct px tokens    : %d" % len(counts["px"]))
    print("  icons listed          : %d (files on disk: %d)" % (n_icons, len(on_disk)))
    if failures:
        print("FAIL: %d problem(s)" % len(failures))
        for line in failures:
            print("  - " + line)
        return 1
    print("PASS: every hex, rgba, px and icon literal appears in the mockup sources")
    return 0


if __name__ == "__main__":
    sys.exit(main())
