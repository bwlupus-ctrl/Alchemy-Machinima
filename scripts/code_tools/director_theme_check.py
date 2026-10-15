#!/usr/bin/env python3
"""Check Director colour theme files (indra/newview/app_settings/director_themes/*.xml).

Usage::

    python scripts/code_tools/director_theme_check.py --check [--themes-dir DIR]
    python scripts/code_tools/director_theme_check.py --selftest

A theme is an LLSD XML map {version:1, id, label, colors:{KitName:[r,g,b,a]}}.
Colours are 0..1 reals.  A Kit name missing from a theme falls back to the
default skin value in indra/newview/skins/default/colors.xml, so contrast is
checked on effective colours (theme override first, skin default otherwise).

Rules (exit 1 on any finding):
- schema: version == 1, id == file stem, label is a non-empty string, colors is a map.
- names: every colour key starts with "Kit" and exists in the skin colour table.
- values: exactly four finite reals in 0..1 per colour.
- caps: at most 200 colour entries, file at most 64 KB (the C++ loader's caps).
- contrast (WCAG 2.x relative luminance, opaque colours):
  * TEXT_TIER names over each BG_PAIRS background: >= 4.5:1.
  * KitTextStrong over KitActiveFill (selected row label): >= 4.5:1.
  * KitAccentTextOn over KitAccent (text on accent fills): >= 4.5:1.
  * KitAccent over KitCardBg (accent strokes and focus): >= 3:1.
  EXEMPT_TEXT names (KitFaint, KitTextDisabled, KitTextDisabledIcon) are
  decorative or disabled tiers; WCAG 1.4.3 exempts inactive components.

Standard library only.  Exit 2 is for command-line or missing-input errors.
"""

from __future__ import annotations

import argparse
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SKIN_COLORS = os.path.join(REPO, "indra", "newview", "skins", "default", "colors.xml")
THEMES_DIR = os.path.join(REPO, "indra", "newview", "app_settings", "director_themes")

MAX_ENTRIES = 200
MAX_BYTES = 64 * 1024
TEXT_TIER = [
    "KitText", "KitTextPrimary", "KitTextStrong", "KitTextLabel", "KitTextSecondary",
    "KitTextMuted", "KitLabel", "KitDim", "KitMuted",
]
EXEMPT_TEXT = ["KitFaint", "KitTextDisabled", "KitTextDisabledIcon"]
BG_PAIRS = ["KitWindowBg", "KitCardBg", "KitPanelBg", "KitInputBg", "KitControlBg"]
TEXT_TIER_MIN = 4.5
ACCENT_PAIRS = [
    ("KitTextStrong", "KitActiveFill", 4.5),
    ("KitAccentTextOn", "KitAccent", 4.5),
    ("KitAccent", "KitCardBg", 3.0),
]


def load_skin_colors(path=SKIN_COLORS):
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    out = {}
    for m in re.finditer(r'<color name="(Kit\w+)" value="([^"]+)"', text):
        parts = [float(x) for x in m.group(2).split()]
        if len(parts) == 3:
            parts.append(1.0)
        out[m.group(1)] = tuple(parts)
    return out


def _llsd_value(elem, errors, where):
    tag = elem.tag
    if tag in ("real", "integer"):
        try:
            return float(elem.text) if tag == "real" else int(elem.text)
        except (TypeError, ValueError):
            errors.append(f"{where}: bad {tag} '{elem.text}'")
            return None
    if tag == "string":
        return elem.text or ""
    if tag == "boolean":
        return (elem.text or "").strip() in ("1", "true")
    if tag == "array":
        return [_llsd_value(c, errors, where) for c in elem]
    if tag == "map":
        return _llsd_map(elem, errors, where)
    errors.append(f"{where}: unsupported LLSD element <{tag}>")
    return None


def _llsd_map(elem, errors, where):
    out = {}
    children = list(elem)
    if len(children) % 2:
        errors.append(f"{where}: map has a key without a value")
        return out
    for i in range(0, len(children), 2):
        key_el, val_el = children[i], children[i + 1]
        if key_el.tag != "key":
            errors.append(f"{where}: map key expected, found <{key_el.tag}>")
            continue
        out[key_el.text or ""] = _llsd_value(val_el, errors, f"{where}.{key_el.text}")
    return out


def check_theme_text(stem, text, skin, byte_len=None):
    """Return (errors, effective_colours). Pure: no filesystem access."""
    errors = []
    size = len(text.encode("utf-8")) if byte_len is None else byte_len
    if size > MAX_BYTES:
        errors.append(f"file is {size} bytes (cap {MAX_BYTES})")
    try:
        root = ET.fromstring(text)
    except ET.ParseError as exc:
        return errors + [f"XML parse error: {exc}"], {}
    if root.tag != "llsd" or len(root) != 1 or root[0].tag != "map":
        return errors + ["root must be <llsd><map>...</map></llsd>"], {}
    data = _llsd_map(root[0], errors, "theme")
    if data.get("version") != 1 or isinstance(data.get("version"), bool):
        errors.append(f"version must be integer 1, found {data.get('version')!r}")
    if not isinstance(data.get("id"), str) or data.get("id") != stem:
        errors.append(f"id must equal file stem '{stem}', found {data.get('id')!r}")
    if not isinstance(data.get("label"), str) or not data.get("label"):
        errors.append("label must be a non-empty string")
    colors = data.get("colors")
    if not isinstance(colors, dict):
        errors.append("colors must be a map")
        return errors, {}
    if len(colors) > MAX_ENTRIES:
        errors.append(f"{len(colors)} colour entries (cap {MAX_ENTRIES})")
    overrides = {}
    for name, value in colors.items():
        where = f"colors.{name}"
        if not name.startswith("Kit"):
            errors.append(f"{where}: name must start with 'Kit'")
            continue
        if name not in skin:
            errors.append(f"{where}: unknown colour name (not in skin colors.xml)")
            continue
        if not isinstance(value, list) or len(value) != 4:
            errors.append(f"{where}: must be [r,g,b,a]")
            continue
        ok = True
        for comp in value:
            if not isinstance(comp, float) or not math.isfinite(comp):
                errors.append(f"{where}: non-finite or non-real component {comp!r}")
                ok = False
                break
            if comp < 0.0 or comp > 1.0:
                errors.append(f"{where}: component {comp} outside 0..1")
                ok = False
                break
        if ok:
            overrides[name] = tuple(value)
    effective = dict(skin)
    effective.update(overrides)
    return errors, effective


def rel_luminance(rgba):
    def lin(v):
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(c) for c in rgba[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = rel_luminance(a), rel_luminance(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


def contrast_findings(effective):
    findings = []

    def pair(fg, bg, minimum):
        if fg not in effective or bg not in effective:
            return
        f, b = effective[fg], effective[bg]
        if f[3] < 1.0 or b[3] < 1.0:
            findings.append(f"{fg} over {bg}: translucent colour, cannot judge contrast")
            return
        ratio = contrast(f, b)
        if ratio < minimum:
            findings.append(f"{fg} over {bg}: {ratio:.2f}:1 < {minimum}:1")

    for fg in TEXT_TIER:
        for bg in BG_PAIRS:
            pair(fg, bg, TEXT_TIER_MIN)
    for fg, bg, minimum in ACCENT_PAIRS:
        pair(fg, bg, minimum)
    return findings


def check_theme_file(path, skin):
    stem = os.path.splitext(os.path.basename(path))[0]
    with open(path, "rb") as fh:
        raw = fh.read()
    if len(raw) > MAX_BYTES:
        return [f"file is {len(raw)} bytes (cap {MAX_BYTES})"]
    errors, effective = check_theme_text(stem, raw.decode("utf-8"), skin, byte_len=len(raw))
    if not errors:
        errors += contrast_findings(effective)
    return errors


def run_check(themes_dir):
    if not os.path.isdir(themes_dir):
        print(f"no themes directory: {themes_dir}", file=sys.stderr)
        return 2
    skin = load_skin_colors()
    if not skin:
        print("no Kit colours found in skin colors.xml", file=sys.stderr)
        return 2
    files = sorted(f for f in os.listdir(themes_dir) if f.endswith(".xml"))
    if "default.xml" not in files:
        print("default.xml is required", file=sys.stderr)
        return 1
    failed = 0
    for name in files:
        errors = check_theme_file(os.path.join(themes_dir, name), skin)
        status = "FAIL" if errors else "ok"
        print(f"{status}  {name}")
        for err in errors:
            print(f"      {err}")
        failed += bool(errors)
    print(f"{len(files)} theme(s), {failed} failing")
    return 1 if failed else 0


# Planted fixtures. Each must-fail fixture carries exactly the defect its label names.
_HEAD = '<?xml version="1.0" encoding="utf-8"?>\n<llsd>\n<map>\n'
_TAIL = '</map>\n</llsd>\n'


def _theme(stem, version="1", colors=""):
    return (_HEAD
            + f'  <key>version</key><integer>{version}</integer>\n'
            + f'  <key>id</key><string>{stem}</string>\n'
            + '  <key>label</key><string>Fixture</string>\n'
            + f'  <key>colors</key><map>{colors}</map>\n'
            + _TAIL)


def _color(name, r, g, b, a="1"):
    return (f'<key>{name}</key><array><real>{r}</real><real>{g}</real>'
            f'<real>{b}</real><real>{a}</real></array>')


def selftest():
    skin = load_skin_colors()
    fixtures = [
        ("good control passes", "good",
         _theme("good", colors=_color("KitAccent", 0.2, 0.8, 0.7)), False),
        ("low-contrast text fails", "low",
         _theme("low", colors=_color("KitText", 0.10, 0.12, 0.15)), True),
        ("unknown name fails", "unknown",
         _theme("unknown", colors=_color("KitNotARealColour", 0.5, 0.5, 0.5)), True),
        ("non-finite value fails", "nonfinite",
         _theme("nonfinite", colors=_color("KitAccent", "nan", 0.5, 0.5)), True),
        ("bad schema version fails", "badver",
         _theme("badver", version="2", colors=_color("KitAccent", 0.2, 0.8, 0.7)), True),
        ("id not matching stem fails", "realstem",
         _theme("wrongid", colors=_color("KitAccent", 0.2, 0.8, 0.7)), True),
        ("out-of-range component fails", "range",
         _theme("range", colors=_color("KitAccent", 1.5, 0.5, 0.5)), True),
        ("three-component array fails", "three",
         _theme("three", colors='<key>KitAccent</key><array><real>0.1</real><real>0.2</real><real>0.3</real></array>'), True),
    ]
    failures = []
    for label, stem, text, must_fail in fixtures:
        errors, effective = check_theme_text(stem, text, skin)
        if not errors:
            errors = contrast_findings(effective)
        if bool(errors) != must_fail:
            failures.append(f"{label}: expected {'failure' if must_fail else 'pass'}, got {errors}")
        else:
            print(f"ok    {label}")
    default_path = os.path.join(THEMES_DIR, "default.xml")
    if os.path.isfile(default_path):
        if check_theme_file(default_path, skin):
            failures.append("shipped default.xml does not pass")
        else:
            print("ok    shipped default.xml passes")
    if failures:
        for f in failures:
            print("SELFTEST FAIL:", f)
        return 1
    print("selftest passed")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="check every theme file")
    parser.add_argument("--selftest", action="store_true", help="run planted must-fail fixtures")
    parser.add_argument("--themes-dir", default=THEMES_DIR)
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.check:
        return run_check(args.themes_dir)
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
