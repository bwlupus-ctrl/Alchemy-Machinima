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
  * KitKnobOn over KitAccent (ACTION label) and over KitText (active mode
    segment label, alkitshell.cpp): >= 4.5:1.
  * KitRecordTextOn over KitRecord (cut label): >= 4.5:1.
  * KitIconOnAccent over KitAccent and over KitRecord (icons on transport
    fills): >= 3:1.
  * KitIconTint and KitIconTintStrong over each BG_PAIRS background: >= 3:1.
  Icon pairs may be translucent; they are composited over the background
  first, so the judged colour is what is drawn.
  EXEMPT_TEXT names (KitFaint, KitTextDisabled, KitTextDisabledIcon) are
  decorative or disabled tiers; WCAG 1.4.3 exempts inactive components.
- inverted: every theme must also pass the contrast rules after the Invert
  theme switch. inverted_colors() mirrors ALDirectorTheme::invertColors;
  INVERT_VECTOR pins both to the same output (aldirectortheme_test.cpp).
- list selection: the selected row of a kit scroll list (ALKitStyle
  applyListColors) uses list_selection_colors(), the mirror of
  ALDirectorTheme::listSelectionColors. For every theme, plain and inverted,
  the selected background must reach LIST_SELECTION_MIN_BG:1 against both the
  plain row (KitInputBg) and the striped row (KitWindowBg), and the selected
  text LIST_SELECTION_MIN_TEXT:1 against the selected background.
  A selected row that is also highlighted (LLScrollListCtrl::drawItems) is
  judged as drawn: list_row_fill() mirrors the fill, and kit lists must keep
  the plain selected fill there (the stock blend with the KitAccent highlight
  colour drops the text below the minimum in most themes). The 3 px accent
  bar must have its gutter, so it never paints over the first cell.
- Runtime Director-capable C++: text setters/draws require Kit colour provenance;
  exact reviewed legacy/state/forwarder exceptions only. Kit member references
  must remain live; LLColor4 destinations and detached aliases fail.
- Director XUI: every explicit text_color in panel_dpage_*.xml and
  floater_director_v2.xml must name an existing Kit colour. No exemptions.

Standard library only.  Exit 2 is for command-line or missing-input errors.
"""

from __future__ import annotations

import argparse
import glob
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SKIN_COLORS = os.path.join(REPO, "indra", "newview", "skins", "default", "colors.xml")
THEMES_DIR = os.path.join(REPO, "indra", "newview", "app_settings", "director_themes")
SKINS_DIR = os.path.join(REPO, "indra", "newview", "skins")

MAX_ENTRIES = 200
MAX_BYTES = 64 * 1024
TEXT_TIER = [
    "KitText", "KitTextPrimary", "KitTextStrong", "KitTextLabel", "KitTextSecondary",
    "KitTextMuted", "KitLabel", "KitDim", "KitMuted",
]
SEMANTIC_TEXT_NAMES = ["KitWarnBandText", "KitRecordText"]
EXEMPT_TEXT = ["KitFaint", "KitTextDisabled", "KitTextDisabledIcon"]
BG_PAIRS = ["KitWindowBg", "KitCardBg", "KitPanelBg", "KitInputBg", "KitControlBg"]
TEXT_TIER_MIN = 4.5
ACCENT_PAIRS = [
    ("KitTextStrong", "KitActiveFill", 4.5),
    ("KitAccentTextOn", "KitAccent", 4.5),
    ("KitAccent", "KitCardBg", 3.0),
    ("KitKnobOn", "KitAccent", 4.5),        # ACTION label on the accent fill (alkitstyle.cpp styleTree)
    ("KitKnobOn", "KitText", 4.5),          # active mode-segment label on its KitText fill (alkitshell.cpp)
    ("KitRecordTextOn", "KitRecord", 4.5),  # cut label on the record fill (director_kit_gen.py btn_cut)
]
# Icons are drawn with their own tint; translucent tints are composited over the fill first.
ICON_PAIRS = [
    ("KitIconOnAccent", "KitAccent", 3.0),  # transport icons drawn on the accent fill
    ("KitIconOnAccent", "KitRecord", 3.0),  # ... and on the record fill (ACTION while recording)
]
ICON_TINT_NAMES = ["KitIconTint", "KitIconTintStrong"]
ICON_MIN = 3.0
NEEDLE_PAIRS = [("KitDialNeedle", "KitDialFace", 3.0), ("KitDialNeedle", "KitCardBg", 3.0),
                ("KitDialNeedle", "KitRowHover", 3.0)]


def contrast_pairs():
    """Ordered visitor shared by judging and correction, mirrored in the C++ table."""
    return ([(fg, bg, TEXT_TIER_MIN, False) for fg in TEXT_TIER + SEMANTIC_TEXT_NAMES
             for bg in BG_PAIRS + (["KitRowHover"] if fg in SEMANTIC_TEXT_NAMES else [])]
            + [("KitKnobOn", "KitText", 4.5, False), ("KitAccent", "KitKnobOn", 4.5, False)]
            + [(fg, bg, minimum, False) for fg, bg, minimum in ACCENT_PAIRS + NEEDLE_PAIRS]
            + [(fg, bg, minimum, True) for fg, bg, minimum in ICON_PAIRS]
            + [(fg, bg, ICON_MIN, True) for fg in ICON_TINT_NAMES for bg in BG_PAIRS])


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


def composite_over(fg, bg):
    """Source-over blend of a translucent fg onto an opaque bg (both RGBA 0..1)."""
    a = fg[3]
    return tuple(fg[i] * a + bg[i] * (1.0 - a) for i in range(3)) + (1.0,)


def map_caption_findings(effective):
    """Bound every gradientDisc/frontHemisphere surface, including interpolation.

    RGB and alpha interpolate independently in the hemisphere fan. Enclose their
    product with all endpoint RGB/alpha combinations over both backdrop endpoints.
    Channel minima bound minimum luminance; convexity of sRGB luminance bounds
    its maximum by the vertices. This conservative enclosure cannot miss an
    interior worst case (or a surface crossing the caption's luminance).
    Judging only: do not silently add custom-theme failures to viewer correction.
    """
    names = ('KitText', 'KitMapBackdropMid', 'KitMapBackdropEdge',
             'KitMapHemisphereMid', 'KitMapHemisphereLow')
    if not all(name in effective for name in names):
        return []
    if any(effective[name][3] < 1.0 for name in names[:3]):
        return ['KitText over map surfaces: translucent text/backdrop, cannot judge contrast']
    surfaces = [effective[name] for name in names[1:3]]
    for backdrop in tuple(surfaces):
        for name in names[3:]:
            # Draw code overrides theme alpha with .62/.32; include the cross
            # combinations to enclose interpolation of both RGB and alpha.
            for alpha in (.62, .32):
                surfaces.append(composite_over(effective[name][:3] + (alpha,), backdrop))
    lower = rel_luminance(tuple(min(c[i] for c in surfaces) for i in range(3)))
    upper = max(rel_luminance(c) for c in surfaces)
    text = rel_luminance(effective['KitText'])
    ratio = ((text + .05) / (upper + .05) if text > upper else
             (lower + .05) / (text + .05) if text < lower else 1.0)
    if ratio < 4.5:
        return [f'KitText over map surfaces (worst-case bound): {ratio:.2f}:1 < 4.5:1']
    return []


def contrast_findings(effective):
    findings = []

    def pair(fg, bg, minimum, composite=False):
        if fg not in effective or bg not in effective:
            return
        f, b = effective[fg], effective[bg]
        if b[3] < 1.0:
            findings.append(f"{fg} over {bg}: translucent background, cannot judge contrast")
            return
        if f[3] < 1.0:
            if not composite:
                findings.append(f"{fg} over {bg}: translucent colour, cannot judge contrast")
                return
            f = composite_over(f, b)
        ratio = contrast(f, b)
        if ratio < minimum:
            findings.append(f"{fg} over {bg}: {ratio:.2f}:1 < {minimum}:1")

    for fg, bg, minimum, composite in contrast_pairs():
        pair(fg, bg, minimum, composite)
    findings += map_caption_findings(effective)
    return findings


KEPT_WHEN_INVERTED = {
    "KitRecord", "KitRecordTextOn", "KitErrorRed", "KitFlash", "KitRedTint22",
    "KitDimBlack65", "KitOverlayScrim", "KitMapArcBackFill",
}


# Kit scroll lists: selected row. Mirrors ALDirectorTheme::listSelectionColors.
LIST_SELECTION_MIN_BG = 1.8
LIST_SELECTION_MIN_TEXT = 4.5
LIST_SELECTION_STRENGTHS = (0.45, 0.6, 0.75, 0.9, 1.0)
LIST_SELECTION_NAMES = ("KitAccent", "KitInputBg", "KitWindowBg", "KitTextStrong")


def mix_opaque(start, end, amount):
    return tuple(start[i] + (end[i] - start[i]) * amount for i in range(3)) + (1.0,)


def list_selection_judge(bg, fg, row_bg, stripe_bg):
    """Findings for one selected-row colour pair; empty when it is clearly visible."""
    findings = []
    for label, other in (("plain row", row_bg), ("striped row", stripe_bg)):
        ratio = contrast(bg, other)
        if ratio < LIST_SELECTION_MIN_BG:
            findings.append(f"list selection background vs {label}: {ratio:.2f}:1 < {LIST_SELECTION_MIN_BG}:1")
    ratio = contrast(fg, bg)
    if ratio < LIST_SELECTION_MIN_TEXT:
        findings.append(f"list selection text: {ratio:.2f}:1 < {LIST_SELECTION_MIN_TEXT}:1")
    return findings


def list_selection_colors(accent, row_bg, stripe_bg, text, strengths=LIST_SELECTION_STRENGTHS):
    """(bg, fg, ok): accent blended into the row colour, stronger until it stands out."""
    black, white = (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0, 1.0)
    strong = tuple(text[:3]) + (1.0,)
    bg = fg = None
    for toward in (accent, strong):
        for strength in strengths:
            bg = mix_opaque(row_bg, toward, strength)
            fg = (strong if contrast(strong, bg) >= LIST_SELECTION_MIN_TEXT
                  else black if contrast(black, bg) >= contrast(white, bg) else white)
            if not list_selection_judge(bg, fg, row_bg, stripe_bg):
                return bg, fg, True
    return bg, fg, False


def list_row_fill(selected_bg, highlight, highlighted, keeps_selected_fill):
    """Fill of a selected row as LLScrollListCtrl::drawItems draws it.

    Stock lists average the selected fill with the highlight colour on a highlighted
    row; a list that opted in with setSelectedBar() keeps the selected fill.
    """
    if highlighted and not keeps_selected_fill:
        return mix_opaque(selected_bg, highlight, 0.5)
    return tuple(selected_bg[:3]) + (1.0,)


def list_selection_findings(effective, keeps_selected_fill=True):
    if not all(name in effective for name in LIST_SELECTION_NAMES):
        return ["list selection: missing " + ", ".join(n for n in LIST_SELECTION_NAMES if n not in effective)]
    accent, row_bg, stripe_bg, text = (effective[name] for name in LIST_SELECTION_NAMES)
    if row_bg[3] < 1.0 or stripe_bg[3] < 1.0:
        return ["list selection: translucent row background, cannot judge contrast"]
    bg, fg, ok = list_selection_colors(accent, row_bg, stripe_bg, text)
    findings = list_selection_judge(bg, fg, row_bg, stripe_bg) if not ok else []
    # Selected and highlighted: the kit highlight colour is KitAccent (applyListColors).
    drawn = list_row_fill(bg, accent, True, keeps_selected_fill)
    ratio = contrast(fg, drawn)
    if ratio < LIST_SELECTION_MIN_TEXT:
        findings.append(f"list selection text on a highlighted selected row: {ratio:.2f}:1 < {LIST_SELECTION_MIN_TEXT}:1")
    return findings


def list_draw_source_findings(ctrl_cpp, ctrl_h, item_cpp, style):
    """Pins of the opted-in (kit) row drawing: fill kept when highlighted, gutter for the bar."""
    ctrl_cpp, ctrl_h, item_cpp, style = (compact_cpp(strip_cpp_comments(text)) for text in (ctrl_cpp, ctrl_h, item_cpp, style))
    errors = []
    for text, needle, message in (
            (ctrl_cpp, "if(item->getHighlighted()&&mSelectedBarWidth<=0){select_color=lerp(mBgSelectedColor.get(),"
                       "mHighlightedColor.get(),0.5f);}else{select_color=mBgSelectedColor.get();}",
             "llscrolllistctrl.cpp: an opted-in list no longer keeps the selected fill on a highlighted row"),
            (ctrl_cpp, "highlight_color%alpha,mColumnPadding,getSelectedBarGutter());",
             "llscrolllistctrl.cpp: rows are no longer drawn with the selected-bar gutter"),
            (ctrl_cpp, "gl_rect_2d(item_rect.mLeft,item_rect.mTop,item_rect.mLeft+mSelectedBarWidth,item_rect.mBottom,",
             "llscrolllistctrl.cpp: the selected bar is no longer drawn inside the gutter"),
            (ctrl_h, "S32getSelectedBarGutter()const{returnmSelectedBarWidth>0?mSelectedBarWidth+1:0;}",
             "llscrolllistctrl.h: the gutter is no longer wider than the bar (or exists without one)"),
            (item_cpp, "S32cur_x=rect.mLeft+left_gutter;",
             "llscrolllistitem.cpp: cells no longer start after the gutter"),
            (style, "list->setSelectedBar(LIST_SELECTED_BAR_W,color(ACCENT).get());",
             "alkitstyle.cpp: kit lists no longer opt in to the selected bar / kept fill"),
            (style, "list->setHighlightedColor(color(ACCENT).get());",
             "alkitstyle.cpp: the kit highlight colour is no longer KitAccent (update list_selection_findings)")):
        if needle not in text:
            errors.append(message)
    return errors


def list_selection_source_findings(header, style):
    """The viewer must compute the same colours, from the same inputs, for every kit list."""
    header, style = compact_cpp(strip_cpp_comments(header)), compact_cpp(strip_cpp_comments(style))
    errors = []
    numbers = lambda values: "{" + ",".join(f"{v:g}f".replace("1f", "1.f") for v in values) + "}"
    for needle, label in (
            (f"kListSelectionMinBg={LIST_SELECTION_MIN_BG:g}f;", "background minimum"),
            (f"kListSelectionMinText={LIST_SELECTION_MIN_TEXT:g}f;", "text minimum"),
            ("kListSelectionStrengths[]=" + numbers(LIST_SELECTION_STRENGTHS) + ";", "blend strengths"),
            ("for(constLLColor4*toward:{&accent,&strong})", "accent-then-text order"),
            ("out.bg=mixOpaque(row_bg,*toward,strength);", "blend"),
            ("out.fg=contrastRatio(strong,out.bg)>=kListSelectionMinText?strong:"
             "contrastRatio(black,out.bg)>=contrastRatio(white,out.bg)?black:white;", "text choice"),
            ("returncontrastRatio(bg,row_bg)>=kListSelectionMinBg&&contrastRatio(bg,stripe_bg)>=kListSelectionMinBg&&"
             "contrastRatio(fg,bg)>=kListSelectionMinText;", "pass rule")):
        if needle not in header:
            errors.append(f"aldirectortheme.h: list selection {label} differs from Python")
    for needle, label in (
            ("ALDirectorTheme::listSelectionColors(color(ACCENT).get(),color(INPUT_BG).get(),"
             "color(WINDOW_BG).get(),color(TEXT_STRONG).get());", "inputs"),
            ("list->setBgSelectedColor(selected.bg);", "selected background"),
            ("list->setFgSelectedColor(selected.fg);", "selected text"),
            ("list->setBgWriteableColor(color(INPUT_BG).get());", "row background"),
            ("list->setBgStripeColor(color(WINDOW_BG).get());", "stripe background"),
            ("voidrestyleLists(){for(constLLHandle<LLScrollListCtrl>&handle:sStyledLists)"
             "if(LLScrollListCtrl*list=handle.get())applyListColors(list);}", "theme-change restyle")):
        if needle not in style:
            errors.append(f"alkitstyle.cpp: kit list {label} no longer comes from the checked rule")
    if style.count("setBgSelectedColor(") != 1 or style.count("setFgSelectedColor(") != 1:
        errors.append("alkitstyle.cpp: a second selected-colour write bypasses the checked rule")
    return errors


def keeps_when_inverted(name):
    return name in KEPT_WHEN_INVERTED or name.startswith("KitShadow")


def inverted_color(color):
    shift = 1.0 - (max(color[:3]) + min(color[:3]))
    return tuple(min(1.0, max(0.0, v + shift)) for v in color[:3]) + (color[3],)


def nudge_for_contrast(colors, fg, bg, minimum):
    if fg not in colors or bg not in colors or colors[bg][3] < 1.0:
        return
    back = colors[bg]
    target = 0.0 if rel_luminance(back) > 0.18 else 1.0
    for _step in range(40):
        if contrast(composite_over(colors[fg], back), back) >= minimum:
            return
        colors[fg] = tuple(v + (target - v) * 0.08 for v in colors[fg][:3]) + (colors[fg][3],)


def make_readable(colors):
    for _pass in range(4):
        for fg, bg, minimum, _composite in contrast_pairs():
            nudge_for_contrast(colors, fg, bg, minimum)


def inverted_colors(effective, nudge=True):
    colors = {name: (value if keeps_when_inverted(name) else inverted_color(value))
              for name, value in effective.items()}
    if nudge:
        make_readable(colors)
    return colors


def check_theme_file(path, skin):
    stem = os.path.splitext(os.path.basename(path))[0]
    with open(path, "rb") as fh:
        raw = fh.read()
    if len(raw) > MAX_BYTES:
        return [f"file is {len(raw)} bytes (cap {MAX_BYTES})"]
    errors, effective = check_theme_text(stem, raw.decode("utf-8"), skin, byte_len=len(raw))
    if not errors:
        errors += contrast_findings(effective) + list_selection_findings(effective)
        inverted = inverted_colors(effective)
        errors += ["inverted: " + item for item in contrast_findings(inverted) + list_selection_findings(inverted)]
    return errors


def director_text_findings(text, skin):
    """Check generated and hand-written Director text; no literal allowlist."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as exc:
        return [f"XML parse error: {exc}"]
    findings = []
    for node in root.iter():
        if node.tag == "text" and not any(
                attr.split(".")[-1] == "text_color" for attr in node.attrib) and not any(
                child.tag.split(".")[-1] == "text_color" for child in node):
            findings.append(f"text {node.get('name', '<unnamed>')}: missing explicit Kit text_color")
        colors = [value for attr, value in node.attrib.items()
                  if attr.split(".")[-1] == "text_color"]
        for attr in node.attrib:
            parts = attr.split(".")
            if "text_color" in parts and parts[-1] != "text_color":
                findings.append(f"{node.tag}: literal text colour component {attr}")
        parts = node.tag.split(".")
        if "text_color" in parts and parts[-1] != "text_color":
            findings.append(f"{node.tag}: literal text colour component")
        if node.tag.split(".")[-1] == "text_color":
            if list(node) or any(attr != "value" for attr in node.attrib):
                findings.append(f"{node.tag}: literal/nested text colour components")
            colors.append(node.get("value", (node.text or "").strip()))
        for color in colors:
            if not color.startswith("Kit") or color not in skin:
                findings.append(f"{node.tag} {node.get('name', '<unnamed>')}: "
                                f"text_color={color!r} must name an existing Kit colour")
    return findings


# Review exceptions are exact calls/initializers, never whole-file exclusions.
# A changed argument or a new call must be reviewed again.
CPP_TEXT_ALLOWLIST = {
    ('alkitstyle.cpp', 'list->setFgSelectedColor(selected.fg)'):
        ('Kit list selected text: KitTextStrong, or black/white when the accent fill needs it. '
         'list_selection_source_findings pins the rule and every theme is gated on its contrast.',
         'list->setBgStripeColor(color(WINDOW_BG).get());'),
    ('alpanelactorkey.cpp', 'font->renderUTF8(label,0,gViewerWindow->getWorldViewRectScaled().mRight-12,gViewerWindow->getWorldViewRectScaled().mTop-42,LLColor4(1.f,.7f,.15f,1.f),LLFontGL::RIGHT,LLFontGL::TOP,LLFontGL::NORMAL,LLFontGL::DROP_SHADOW)'):
        ('World-view key badge with drop shadow, outside Director surfaces.', ''),
    ('alghostnameplates.cpp', 'tag->setColor(color)'):
        ('World-space HUD nameplate selection/user colour and distance fade, outside Director.', ''),
    ('alpanelavatarlightresponse.cpp', 'cell->setColor(color)'):
        ('State column contains only filled/hollow dot glyphs; real upload/visibility state.', ''),
    ('alpanelavatarlightresponse.cpp', 'mBudget->setColor(findChild<ALKitPage>("actors_kit")?warning:LLUIColor(ALR_COLOR_AMBER))'):
        ('Kit warning reference on the generated Director page; original amber only in standalone panel.', ''),
    ('llfloaterdirector.cpp', 'cell->setColor(!in_world?LLColor4::grey:(m&&!m->mGroup.empty())?group_tint(m->mGroup):LLColor4::white)'):
        ('Legacy v1 cast-list text only; V2 inherits themed list text, with no cell override.', 'if(!mIsV2)'),
    ('allightrigmap.cpp', 'f->renderUTF8(s,0,ll_round(x),ll_round(y),c,a,LLFontGL::VCENTER)'):
        ('Map label forwarder; label/place_label call colour arguments are also scanned.', ''),
    ('allightrigmap.cpp', 'label(this,text,candidate.x,candidate.y,color,candidate.align)'):
        ('Map placement forwarder; place_label call colour arguments are also scanned.', ''),
    ('allightrigmap.cpp', 'label(this,text,alternate.x,alternate.y,color,alternate.align)'):
        ('Map alternate-placement forwarder; place_label arguments are scanned.', ''),
    ('allightrigmap.cpp', 'label(this,text,fallback.x,fallback.y,color,fallback.align)'):
        ('Map fallback-placement forwarder; place_label arguments are scanned.', ''),
    ('alvcammonitorstrip.cpp', 'font->renderUTF8(text,0,static_cast<F32>(x),static_cast<F32>(y),color,LLFontGL::LEFT,LLFontGL::BASELINE,LLFontGL::NORMAL,LLFontGL::DROP_SHADOW_SOFT,S32_MAX,llmax(1,max_pixels),nullptr,max_pixels!=S32_MAX)'):
        ('Monitor text forwarder; every drawText call colour argument is also scanned.', ''),
}
for _call in (
        'complexity->setColor(heatColor(rec.mRenderCost,100000.f,250000.f))',
        'complexity->setColor(mutedColor())', 'note->setColor(cautionColor())',
        'note->setColor(mutedColor())', 'desc_box->setColor(cautionColor())',
        'desc_box->setColor(labelColor())', 'box->setColor(muted?mutedColor():labelColor())',
        'box->setColor(color)'):
    CPP_TEXT_ALLOWLIST[('alfloatersceneexplorer.cpp', _call)] = (
        'Standalone Scene Explorer floater uses skin label/caution/cost colours; never embedded in Director.', '')

# Exact shared-controller initializers: only their non-Director arms use old colours.
CPP_SHARED_COLORS = {
    ('llfloaterdirector.cpp', 'rec_color'):
        'mIsV2?ALKitStyle::color(rec?ALKitStyle::RECORD_TEXT_ON:ALKitStyle::MUTED):LLUIColor(rec?LLColor4(0.85f,0.25f,0.25f,1.f):LLColor4(0.5f,0.5f,0.5f,1.f))',
    ('alpanelcinelightrig.cpp', 'mRadiusDefaultColor'):
        'mRigKitPage?LLUIColorTable::instance().getColor("KitTextSecondary"):LLUIColorTable::instance().getColor("LabelTextColor",LLColor4::white)',
    ('alpanelcinelightrig.cpp', 'mRadiusWarningColor'):
        'mRigKitPage?LLUIColorTable::instance().getColor("KitWarnBandText"):LLUIColor(LLColor4(1.f,0.75f,0.25f,1.f))',
}


def strip_cpp_comments(source):
    """Preserve strings and line numbers while removing both C++ comment forms."""
    token = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/'
    return re.sub(token, lambda m: ('\n' * m[0].count('\n') if m[0].startswith('/')
                                    else m[0]), source, flags=re.S)


def compact_cpp(source):
    # Retain spaces inside strings (including numeric colour strings).
    return ''.join(m[0] for m in re.finditer(r'"(?:\\.|[^"\\])*"|\S', source))


def cpp_arguments(source, opening):
    """Balanced C++ argument reader; strings and nested calls/braces are opaque."""
    args, start, stack = [], opening + 1, ['(']
    tokens = re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[(){}\[\],]', source[opening+1:])
    for token in tokens:
        c, pos = token[0], opening + 1 + token.start()
        if c.startswith(('"', "'")): continue
        if c in '({[': stack.append(c)
        elif c in ')}]':
            stack.pop()
            if not stack:
                args.append(source[start:pos].strip())
                return args, pos + 1
        elif c == ',' and len(stack) == 1:
            args.append(source[start:pos].strip()); start = pos + 1
    return [], opening + 1


def color_definitions(source):
    """Collect colour aliases, including constructor initializers and assignments.

    Multiple writes remain visible: a good initializer cannot hide a later literal.
    Destination member declarations are provided by the companion header.
    """
    defs = {}
    for m in re.finditer(r'\b(\w+)\s*=\s*(?!=)', source):
        start, depth = m.end(), 0
        for token in re.finditer(r'"(?:\\.|[^"\\])*"|[(){}\[\],;]', source[start:]):
            c = token[0]
            if c.startswith('"'): continue
            if c in '({[': depth += 1
            elif c in ')}]':
                if depth == 0: break
                depth -= 1
            elif c in ',;' and depth == 0:
                defs.setdefault(m[1], []).append(source[start:start+token.start()].strip())
                break
    for m in re.finditer(r'\b(m[A-Z]\w*)\s*\(', source):
        args, end = cpp_arguments(source, m.end()-1)
        if len(args) == 1 and source[end:].lstrip().startswith((',', '{')):
            defs.setdefault(m[1], []).append(args[0])
    # LLUIColor::set overwrites provenance just like operator=. Keep every
    # write so a Kit initializer cannot conceal a subsequent literal mutation.
    for m in re.finditer(r'\b(\w+)\s*(?:\.|->)\s*set\s*\(', source):
        args, _end = cpp_arguments(source, m.end()-1)
        defs.setdefault(m[1], []).extend(args or ['UNPROVEN_MUTATION'])
    return defs


_KIT_LOOKUP = re.compile(r'(?:\b\w+(?:::\w+)*(?:\([^()]*\))?\s*(?:\.|->)\s*)?getColor\s*\(\s*"Kit\w+"')
_KIT_STYLE = re.compile(r'ALKitStyle\s*::\s*color\s*\(')


def kit_expression(expression, definitions, path='', seen=frozenset()):
    """Prove the colour operands come from live Kit roles, following local aliases.

    Conditions select roles but do not themselves supply colour operands. Unknown
    arguments fail closed, so new helpers need either Kit provenance or review.
    """
    expression = expression.strip()
    # Replace complete Kit calls before inspecting ternaries or aliases.
    for pattern in (_KIT_STYLE, _KIT_LOOKUP):
        while m := pattern.search(expression):
            name = 'color' if pattern is _KIT_STYLE else 'getColor'
            opening = expression.index('(', expression.index(name, m.start()))
            _args, end = cpp_arguments(expression, opening)
            if end <= opening + 1: return False
            expression = expression[:m.start()] + ' KIT ' + expression[end:]
    # Unqualified wrapper within ALKitStyle's own implementation.
    if path == 'alkitstyle.cpp':
        expression = re.sub(r'\bcolor\(\w+\)', ' KIT ', expression)
    expression = re.sub(r'(?:\.|->)\s*get\s*\(\s*\)', '', expression).strip()
    expression = re.sub(r'^\s*[&*]\s*', '', expression)
    while expression.startswith('(') and expression.endswith(')'):
        _args, end = cpp_arguments(expression, 0)
        if end != len(expression): break
        expression = expression[1:-1].strip()
    # Find the outermost ternary, ignoring namespace :: tokens and nested calls.
    depth, question = 0, None
    for i, c in enumerate(expression):
        if c in '([{': depth += 1
        elif c in ')]}': depth -= 1
        elif c == '?' and depth == 0:
            question = i; break
    if question is not None:
        nested, depth = 0, 0
        for i in range(question+1, len(expression)):
            c = expression[i]
            if c in '([{': depth += 1
            elif c in ')]}': depth -= 1
            elif depth == 0:
                if c == '?': nested += 1
                elif c == ':' and expression[i-1:i] != ':' and expression[i+1:i+2] != ':':
                    if nested: nested -= 1
                    else:
                        return (kit_expression(expression[question+1:i], definitions, path, seen)
                                and kit_expression(expression[i+1:], definitions, path, seen))
        return False
    if expression.strip() == 'KIT': return True
    if re.fullmatch(r'\w+', expression) and expression not in seen:
        values = definitions.get(expression, [])
        reviewed = CPP_SHARED_COLORS.get((path, expression))
        if reviewed and [compact_cpp(v) for v in values] == [reviewed]: return True
        return bool(values) and all(kit_expression(v, definitions, path, seen | {expression}) for v in values)
    return False


def cpp_text_findings(source, path='', header=''):
    """Scan all text setters/draws, including helper call sites and colour aliases."""
    source, header = strip_cpp_comments(source), strip_cpp_comments(header)
    definitions = color_definitions(header + '\n' + source)
    pattern = r'(?:(?:\b\w+)\s*(?:->|\.)\s*)?\b(setColor|setTextColor|setFgColor|setReadOnlyColor|setEnabledColor|setReadOnlyFgColor|setTentativeFgColor|setUnselectedLabelColor|setSelectedLabelColor|setDisabledLabelColor|setFgSelectedColor|setFgUnselectedColor|setFgDisableColor|renderUTF8|renderSimple|render|drawText|place_label|label)\s*\('
    findings = []
    for m in re.finditer(pattern, source):
        method = m[1]
        if method == 'render' and not re.search(r'->|\.', m[0]): continue
        if method in ('label', 'place_label') and path != 'allightrigmap.cpp': continue
        if method == 'drawText' and path != 'alvcammonitorstrip.cpp': continue
        args, end = cpp_arguments(source, m.end()-1)
        index = (4 if method in ('renderUTF8', 'renderSimple', 'render', 'drawText')
                 else 4 if method == 'label' else 1 if method == 'place_label' else 0)
        if index >= len(args): continue
        # Definitions of drawing helpers are checked at their actual sinks/calls.
        if method in ('drawText', 'label') and ('const LLFontGL*' in args[0] or 'LLView*' in args[0]): continue
        call = compact_cpp(source[m.start():end])
        exception = CPP_TEXT_ALLOWLIST.get((path, call))
        if path == 'alpanelavatarlightresponse.cpp' and call.startswith('mBudget->setColor('):
            if not kit_expression('warning', definitions, path): exception = None
        if exception and (not exception[1] or compact_cpp(source[:m.start()]).endswith(exception[1])):
            continue
        if not kit_expression(args[index], definitions, path):
            findings.append(f'line {source.count(chr(10), 0, m.start())+1}: non-Kit/unproven {method} colour: {args[index]}')
    return findings


def director_cpp_paths():
    base = os.path.join(REPO, 'indra', 'newview')
    return sorted({p for pattern in ('al*.cpp', 'al*.h', 'llfloaterdirector*.cpp', 'llfloaterdirector*.h')
                   for p in glob.glob(os.path.join(base, pattern))})


def check_cpp_text():
    paths = [p for p in director_cpp_paths() if p.endswith('.cpp')]
    findings = []
    for path in paths:
        header_path = os.path.splitext(path)[0] + '.h'
        header = open(header_path, encoding='utf-8').read() if os.path.isfile(header_path) else ''
        with open(path, encoding='utf-8') as fh:
            findings += [f'{os.path.relpath(path, REPO)}: {item}'
                         for item in cpp_text_findings(fh.read(), os.path.basename(path), header)]
    print(f'{len(paths)} Director-capable C++ implementation(s), {len(findings)} runtime text colour finding(s)')
    return findings


def stale_member_findings(source, header=''):
    """Reject explicit AND implicit Kit-to-LLColor4 member copies.

    Header member types are joined with constructor/assignment expressions. Local
    LLUIColor/LLColor4 aliases carry Kit provenance into subsequent member writes.
    Draw-local values are allowed; only a persistent destination is rejected.
    """
    source, header = strip_cpp_comments(source), strip_cpp_comments(header)
    definitions = color_definitions(header + '\n' + source)
    copied = set(re.findall(r'\bLLColor4\s+(m[A-Z]\w*)\b', header + '\n' + source))
    live = set(re.findall(r'\bLLUIColor\s+(m[A-Z]\w*)\b', header + '\n' + source))

    tainted = {name for name, values in definitions.items()
               if any(_KIT_LOOKUP.search(v) or _KIT_STYLE.search(v) for v in values)}
    while True:
        more = {name for name, values in definitions.items()
                if any(set(re.findall(r'\b\w+\b', v)) & tainted for v in values)}
        if more <= tainted: break
        tainted.update(more)

    def from_kit(expr):
        return bool(_KIT_LOOKUP.search(expr) or _KIT_STYLE.search(expr)
                    or set(re.findall(r'\b\w+\b', expr)) & tainted)

    value_types = set(re.findall(r'\bLLColor4\s+(\w+)\b', header + '\n' + source))
    detached = {name for name in tainted if name in value_types or any(
        re.search(r'\.\s*get\s*\(', v) for v in definitions.get(name, []))}
    while True:
        more = {name for name in tainted if any(
            set(re.findall(r'\b\w+\b', v)) & detached for v in definitions.get(name, []))}
        if more <= detached: break
        detached.update(more)

    findings = []
    for m in re.finditer(r'\b(m[A-Z]\w*)\s*(=(?!=)|\(|\{)\s*', source):
        if m[2] == '=':
            end = source.find(';', m.end()); expr = source[m.end():end]
        else:
            # Braced construction uses the same nesting parser.
            opening = m.start(2)
            parse_source = source if m[2] == '(' else source[:opening] + '(' + source[opening+1:]
            args, end = cpp_arguments(parse_source, opening)
            expr = ','.join(args)
        # Explicit .get() discards the live reference even for LLUIColor destinations.
        if from_kit(expr) and (m[1] in copied or (re.search(r'\.\s*get\s*\(', expr) and (_KIT_LOOKUP.search(expr) or _KIT_STYLE.search(expr)))):
            findings.append(f'line {source.count(chr(10),0,m.start())+1}: stale Kit colour copy into {m[1]} (LLColor4 destination or detached value)')
        elif from_kit(expr) and m[1] in live and (re.search(r'\.\s*get\s*\(', expr)
                or set(re.findall(r'\b\w+\b', expr)) & detached):
            findings.append(f'line {source.count(chr(10),0,m.start())+1}: stale Kit colour copy into {m[1]} (detached value assigned to LLUIColor)')
    return findings


def check_stale_members():
    paths = director_cpp_paths()
    findings = []
    for path in paths:
        header_path = os.path.splitext(path)[0] + '.h'
        header = open(header_path, encoding='utf-8').read() if path.endswith('.cpp') and os.path.isfile(header_path) else ''
        with open(path, encoding='utf-8') as fh:
            findings += [f"{os.path.relpath(path, REPO)}: {item}" for item in stale_member_findings(fh.read(), header)]
    print(f"{len(paths)} Director C++ file(s), {len(findings)} stale Kit member finding(s)")
    return findings


def cpp_parity_findings(header):
    """Parse the production C++ kept names and complete ordered pair table."""
    header = strip_cpp_comments(header)
    kept = re.search(r'kKept\[\]\s*=\s*\{(.*?)\};', header, re.S)
    table = re.search(r'kContrastPairs\[\]\s*=\s*\{(.*?)\n\s*\};', header, re.S)
    if not kept or not table:
        return ['missing C++ kept names or contrast table']
    errors = []
    if set(re.findall(r'"(Kit\w+)"', kept[1])) != KEPT_WHEN_INVERTED:
        errors.append('C++ kept names differ from Python')
    if not re.search(r'name\.compare\(0, 9, "KitShadow"\) == 0', header):
        errors.append('C++ shadow prefix differs from Python')
    pairs = [(fg, bg, float(n), comp == 'true') for fg, bg, n, comp in re.findall(
        r'\{\s*"(Kit\w+)",\s*"(Kit\w+)",\s*([0-9.]+)f,\s*(true|false)\s*\}', table[1])]
    if pairs != contrast_pairs():
        errors.append('C++ ordered contrast pairs/thresholds differ from Python')
    return errors


def check_director_text(skin):
    paths = sorted(glob.glob(os.path.join(SKINS_DIR, "*", "xui", "*", "panel_dpage_*.xml"))
                   + glob.glob(os.path.join(SKINS_DIR, "*", "xui", "*", "floater_director_v2.xml")))
    if not paths:
        return ["no Director XUI files found"]
    findings = []
    for path in paths:
        with open(path, encoding="utf-8") as fh:
            findings += [f"{os.path.relpath(path, REPO)}: {item}"
                         for item in director_text_findings(fh.read(), skin)]
    print(f"{len(paths)} Director XUI file(s), {len(findings)} text colour finding(s)")
    return findings


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
    text_errors = check_director_text(skin)
    for error in text_errors:
        print(f"FAIL  {error}")
    stale_errors = check_stale_members()
    for error in stale_errors:
        print(f"FAIL  {error}")
    cpp_errors = check_cpp_text()
    for error in cpp_errors:
        print(f"FAIL  {error}")
    with open(os.path.join(REPO, 'indra/newview/aldirectortheme.h'), encoding='utf-8') as fh:
        parity_errors = cpp_parity_findings(fh.read())
    for error in parity_errors:
        print(f'FAIL  {error}')
    print(f'C++/Python contrast parity: {len(parity_errors)} finding(s)')
    with open(os.path.join(REPO, 'indra/newview/aldirectortheme.h'), encoding='utf-8') as fh:
        header = fh.read()
    with open(os.path.join(REPO, 'indra/newview/alkitstyle.cpp'), encoding='utf-8') as fh:
        selection_errors = list_selection_source_findings(header, fh.read())
    selection_errors += list_draw_source_findings(*_list_draw_sources())
    for error in selection_errors:
        print(f'FAIL  {error}')
    print(f'kit list selection rule: {len(selection_errors)} finding(s)')
    return 1 if (failed or text_errors or stale_errors or cpp_errors or parity_errors
                 or selection_errors) else 0


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


INVERT_VECTOR_IN = {
    "KitWindowBg": (0.10, 0.05, 0.10, 1.0),
    "KitText": (0.55, 0.45, 0.50, 1.0),
    "KitIconTint": (0.90, 0.80, 0.85, 0.75),
    "KitAccent": (1.00, 0.41, 0.71, 1.0),
    "KitAccentTextOn": (0.12, 0.04, 0.10, 1.0),
    "KitRecord": (0.77, 0.28, 0.28, 1.0),
    "KitShadowWindow": (0.0, 0.0, 0.0, 0.5),
}
INVERT_VECTOR_OUT = {
    "KitWindowBg": (0.9500, 0.9000, 0.9500, 1.0000),
    "KitText": (0.4655, 0.3809, 0.4232, 1.0000),
    "KitIconTint": (0.2000, 0.1000, 0.1500, 0.7500),
    "KitAccent": (0.5900, 0.0000, 0.3000, 1.0000),
    "KitAccentTextOn": (0.9600, 0.8800, 0.9400, 1.0000),
    "KitRecord": (0.7700, 0.2800, 0.2800, 1.0000),
    "KitShadowWindow": (0.0000, 0.0000, 0.0000, 0.5000),
}


def _list_draw_sources():
    texts = []
    for rel in ('indra/llui/llscrolllistctrl.cpp', 'indra/llui/llscrolllistctrl.h',
                'indra/llui/llscrolllistitem.cpp', 'indra/newview/alkitstyle.cpp'):
        with open(os.path.join(REPO, rel), encoding='utf-8', errors='replace') as fh:
            texts.append(fh.read())
    return texts


# Same input and output as test<11> in indra/newview/tests/aldirectortheme_test.cpp.
LIST_SELECTION_VECTOR_IN = ((1.00, 0.41, 0.71, 1.0), (0.10, 0.05, 0.10, 1.0),
                            (0.08, 0.04, 0.08, 1.0), (0.95, 0.93, 0.95, 1.0))
LIST_SELECTION_VECTOR_OUT = ((0.5050, 0.2120, 0.3745, 1.0), (0.9500, 0.9300, 0.9500, 1.0))


def list_selection_selftest(skin):
    failures = []
    row_bg, stripe_bg, text = skin["KitInputBg"], skin["KitWindowBg"], skin["KitTextStrong"]
    # Planted negative 1: the colours the kit lists used before this rule (selected
    # row = KitControlBg) must be rejected, or the gate would not have caught the report.
    if not list_selection_judge(skin["KitControlBg"], text, row_bg, stripe_bg):
        failures.append("old KitControlBg list selection passes the visibility rule")
    # Planted negative 2: a blend too weak to stand out is rejected and never returned as ok.
    weak = list_selection_colors(skin["KitAccent"], row_bg, stripe_bg, text, strengths=(0.04,))
    if not list_selection_judge(mix_opaque(row_bg, skin["KitAccent"], 0.04), text, row_bg, stripe_bg) or weak[2]:
        failures.append("a 4% accent blend passes the visibility rule")
    # Planted negative 3: unreadable selected text is rejected.
    bg, _fg, ok = list_selection_colors(skin["KitAccent"], row_bg, stripe_bg, text)
    if not ok or not list_selection_judge(bg, bg, row_bg, stripe_bg):
        failures.append("selected text equal to its background passes the visibility rule")
    # An accent equal to the row colour cannot separate at any strength: the text fallback must.
    bg, fg, ok = list_selection_colors(row_bg, row_bg, stripe_bg, text)
    if not ok or list_selection_judge(bg, fg, row_bg, stripe_bg):
        failures.append("accent equal to the row colour has no visible fallback")
    # A theme whose accent equals its row colour and whose text does too is reported, not passed.
    if list_selection_colors(row_bg, row_bg, stripe_bg, row_bg)[2]:
        failures.append("an impossible palette is reported as visible")
    hopeless = dict(skin, KitAccent=row_bg, KitTextStrong=row_bg)
    if not list_selection_findings(hopeless):
        failures.append("an impossible palette produces no finding")
    bg, fg, ok = list_selection_colors(*LIST_SELECTION_VECTOR_IN)
    for got, expected, label in ((bg, LIST_SELECTION_VECTOR_OUT[0], "bg"), (fg, LIST_SELECTION_VECTOR_OUT[1], "fg")):
        if not ok or any(abs(a - b) > 0.0005 for a, b in zip(got, expected)):
            failures.append(f"list selection vector {label}: {got} != {expected}")
    for label, colors in (("skin", skin), ("inverted skin", inverted_colors(skin))):
        if list_selection_findings(colors):
            failures.append(f"{label} list selection fails: {list_selection_findings(colors)}")
    with open(os.path.join(REPO, 'indra/newview/aldirectortheme.h'), encoding='utf-8') as fh:
        header = fh.read()
    with open(os.path.join(REPO, 'indra/newview/alkitstyle.cpp'), encoding='utf-8') as fh:
        style = fh.read()
    if list_selection_source_findings(header, style):
        failures.append(f"production list selection source fails: {list_selection_source_findings(header, style)}")
    for label, bad_header, bad_style in (
            ("weaker background minimum", header.replace("kListSelectionMinBg = 1.8f", "kListSelectionMinBg = 1.1f"), style),
            ("weaker text minimum", header.replace("kListSelectionMinText = 4.5f", "kListSelectionMinText = 3.f"), style),
            ("old selected fill", header, style.replace("list->setBgSelectedColor(selected.bg);",
                                                         "list->setBgSelectedColor(color(CONTROL_BG).get());")),
            ("unchecked selected text", header, style.replace("list->setFgSelectedColor(selected.fg);",
                                                               "list->setFgSelectedColor(color(TEXT).get());")),
            ("second selected fill", header, style.replace("list->setBgSelectedColor(selected.bg);",
                "list->setBgSelectedColor(selected.bg); list->setBgSelectedColor(color(CONTROL_BG).get());")),
            ("theme change not restyled", header, style.replace("applyListColors(list);\n}", "{}\n}"))):
        if (bad_header, bad_style) == (header, style):
            failures.append(f"list selection mutation '{label}' did not apply")
        elif not list_selection_source_findings(bad_header, bad_style):
            failures.append(f"list selection mutation '{label}' passes")
    # Review fix: selected + highlighted rows. The stock blend must be rejected for the
    # shipped skin (it was 3.29:1), the kept fill must pass, plain and inverted.
    if not any("highlighted" in item for item in list_selection_findings(skin, keeps_selected_fill=False)):
        failures.append("blended highlighted selected row passes the text minimum for the skin")
    blended_bad = 0
    for name in sorted(os.listdir(THEMES_DIR)):
        if not name.endswith(".xml"):
            continue
        with open(os.path.join(THEMES_DIR, name), "rb") as fh:
            _errors, effective = check_theme_text(name[:-4], fh.read().decode("utf-8"), skin)
        for colors in (effective, inverted_colors(effective)):
            blended_bad += bool(list_selection_findings(colors, keeps_selected_fill=False))
            if list_selection_findings(colors):
                failures.append(f"{name}: highlighted selected row fails with the kept fill")
    if blended_bad < 2:
        failures.append("the blended highlighted state fails in fewer than 2 theme states; the negative is weak")
    ctrl_cpp, ctrl_h, item_cpp, style_cpp = _list_draw_sources()
    if list_draw_source_findings(ctrl_cpp, ctrl_h, item_cpp, style_cpp):
        failures.append(f"production list drawing fails: {list_draw_source_findings(ctrl_cpp, ctrl_h, item_cpp, style_cpp)}")
    for label, args in (
            ("highlight blends the selected fill again",
             (ctrl_cpp.replace("item->getHighlighted() && mSelectedBarWidth <= 0", "item->getHighlighted()"), ctrl_h, item_cpp, style_cpp)),
            ("rows drawn without the gutter",
             (ctrl_cpp.replace("mColumnPadding, getSelectedBarGutter());", "mColumnPadding);"), ctrl_h, item_cpp, style_cpp)),
            ("gutter no wider than the bar",
             (ctrl_cpp, ctrl_h.replace("mSelectedBarWidth + 1 : 0", "mSelectedBarWidth : 0"), item_cpp, style_cpp)),
            ("cells ignore the gutter",
             (ctrl_cpp, ctrl_h, item_cpp.replace("rect.mLeft + left_gutter;", "rect.mLeft;"), style_cpp)),
            ("kit lists not opted in",
             (ctrl_cpp, ctrl_h, item_cpp, style_cpp.replace("list->setSelectedBar(LIST_SELECTED_BAR_W, color(ACCENT).get());", "")))):
        if args == (ctrl_cpp, ctrl_h, item_cpp, style_cpp):
            failures.append(f"list draw mutation '{label}' did not apply")
        elif not list_draw_source_findings(*args):
            failures.append(f"list draw mutation '{label}' passes")
    if not failures:
        print("ok    list selection: 3 planted low-contrast negatives, fallback, vector, 6 source mutations")
        print(f"ok    highlighted selected rows: blended fill rejected in {blended_bad} theme state(s), kept fill passes; 5 draw mutations")
    return failures


def selftest():
    skin = load_skin_colors()
    fixtures = [
        ("good control passes", "good",
         _theme("good", colors=_color("KitAccent", 0.2, 0.8, 0.7)), False),
        ("low-contrast text fails", "low",
         _theme("low", colors=_color("KitText", 0.10, 0.12, 0.15)), True),
        ("semantic warning text fails", "warn",
         _theme("warn", colors=_color("KitWarnBandText", 0.10, 0.12, 0.15)), True),
        ("semantic record text fails", "record",
         _theme("record", colors=_color("KitRecordText", 0.10, 0.12, 0.15)), True),
        ("needle on dial face fails", "needleface",
         _theme("needleface", colors=_color("KitDialNeedle", *skin["KitDialFace"])), True),
        ("needle on card fails", "needlecard",
         _theme("needlecard", colors=_color("KitDialNeedle", *skin["KitCardBg"])), True),
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
        # Contrast pairs added with the transport and mode-segment fixes.
        ("ACTION knob on accent fails (the 1.0:1 defect)", "knobaccent",
         _theme("knobaccent", colors=_color("KitKnobOn", 0.0, 0.749, 1.0)), True),
        ("record text on record fails", "recordtext",
         _theme("recordtext", colors=_color("KitRecordTextOn", 0.851, 0.651, 1.0)), True),
        ("white icon on accent fails", "iconaccent",
         _theme("iconaccent", colors=_color("KitIconOnAccent", 1.0, 1.0, 1.0)), True),
        ("icon tint too dark on window fails", "icontint",
         _theme("icontint", colors=_color("KitIconTint", 0.10, 0.11, 0.12)), True),
        ("translucent icon tint passes once composited", "tintalpha",
         _theme("tintalpha", colors=_color("KitIconTint", 1.0, 1.0, 1.0, "0.75")), False),
    ]
    failures = []
    # Planted regressions use the same check as --check, including the inherited
    # global widget colour and numeric warning cases that broke light themes.
    text_fixtures = [
        ("Kit Director text passes", '<panel><text text_color="KitTextMuted" /></panel>', False),
        ("inherited Director text colour fails", '<panel><text text_color="LtGray_50" /></panel>', True),
        ("literal Director warning fails", '<panel><text text_color="1 0.75 0.25 1" /></panel>', True),
        ("unknown Kit Director text fails", '<panel><text text_color="KitNotARealColour" /></panel>', True),
        ("nested Director text colour fails", '<panel><text.text_color value="White" /></panel>', True),
        ("qualified Director text colour fails", '<panel text.text_color="White" />', True),
        ("missing Director text attribute fails", '<panel><text name="recovery_help" /></panel>', True),
        ("dotted literal components fail", '<panel><text text_color.red="1" text_color.green="0" text_color.blue="0" text_color.alpha="1"/></panel>', True),
        ("nested literal components fail", '<panel><text><text.text_color><red>1</red><green>0</green></text.text_color></text></panel>', True),
        ("qualified component element fails", '<panel><text.text_color.red value="1" /></panel>', True),
        ("multiline Kit element passes", '<panel><text><text.text_color>\n  KitText \n</text.text_color></text></panel>', False),
    ]
    for label, text, must_fail in text_fixtures:
        errors = director_text_findings(text, skin)
        if bool(errors) != must_fail:
            failures.append(f"{label}: expected {'failure' if must_fail else 'pass'}, got {errors}")
        else:
            print(f"ok    {label}")
    for label, source, must_fail in [
            ('table member copy fails', 'mText(LLUIColorTable::instance().getColor("KitText").get())', True),
            ('named table member assignment fails', 'mText = table.getColor("KitText").get();', True),
            ('table pointer with fallback member copy fails', 'mText(LLUIColorTable::getInstance()->getColor("KitText", LLColor4(1,1,1,1)).get())', True),
            ('Kit wrapper member copy fails', 'mText(ALKitStyle::color(ALKitStyle::TEXT).get())', True),
            ('live member reference passes', 'mText(LLUIColorTable::instance().getColor("KitText"))', False),
            ('draw local copy passes', 'LLColor4 text = table.getColor("KitText").get();', False)]:
        if bool(stale_member_findings(source)) != must_fail:
            failures.append(label)
        else:
            print(f'ok    {label}')
    with open(os.path.join(REPO, 'indra/newview/aldirectortheme.h'), encoding='utf-8') as fh:
        header = fh.read()
    failures += cpp_parity_findings(header)
    drifted = re.sub(r'^.*\{ "KitTextStrong", "KitActiveFill".*\n', '', header, flags=re.M)
    if drifted == header or not cpp_parity_findings(drifted):
        failures.append('C++-only pair removal escaped parity parser')
    else:
        print('ok    C++ kept names and ordered pairs match; in-memory C++ pair removal fails')
    for label, transform in [
            ('line-commented C++ pair fails', lambda line: '// ' + line),
            ('block-commented C++ pair fails', lambda line: '/* ' + line + ' */')]:
        commented = re.sub(r'^.*\{ "KitTextStrong", "KitActiveFill".*$',
                           lambda m: transform(m[0]), header, flags=re.M)
        if commented == header or not cpp_parity_findings(commented):
            failures.append(label)
        else:
            print('ok    ' + label)
    # A comment marker in a string must survive stripping.
    if strip_cpp_comments('"http://fixture/*keep*/" // remove\n/* remove */') != '"http://fixture/*keep*/" \n':
        failures.append('comment stripping changed string contents')

    monitor = os.path.join(REPO, 'indra/newview/alvcammonitorstrip')
    with open(monitor + '.cpp', encoding='utf-8') as fh:
        monitor_source = fh.read()
    with open(monitor + '.h', encoding='utf-8') as fh:
        monitor_header = fh.read()
    reverted_header = re.sub(r'\bLLUIColor(\s+m[A-Z]\w*)', r'LLColor4\1', monitor_header)
    if (reverted_header == monitor_header or stale_member_findings(monitor_source, monitor_header)
            or not stale_member_findings(monitor_source, reverted_header)):
        failures.append('header-only monitor LLColor4 reversion escaped stale scan')
    else:
        print('ok    unchanged monitor constructors + header-only LLColor4 reversion fail')
    for label, source, member_header, must_fail in [
            ('implicit member constructor copy fails', 'Widget():mText(table.getColor("KitText")) {}', 'LLColor4 mText;', True),
            ('implicit member assignment fails', 'mText = table.getColor("KitText");', 'LLColor4 mText;', True),
            ('indirect live local to value member fails', 'const LLUIColor& local = table.getColor("KitText"); const auto& alias = local; mText = alias;', 'LLColor4 mText;', True),
            ('indirect detached local to live member fails', 'LLColor4 local = table.getColor("KitText"); mText = local;', 'LLUIColor mText;', True),
            ('braced implicit value member fails', 'Widget():mText{table.getColor("KitText")} {}', 'LLColor4 mText;', True),
            ('live reference local to live member passes', 'const LLUIColor& local = table.getColor("KitText"); mText = local;', 'LLUIColor mText;', False)]:
        if bool(stale_member_findings(source, member_header)) != must_fail:
            failures.append(label)
        else:
            print('ok    ' + label)

    # Every setter is exercised with both literal and legacy table regressions.
    for setter in ('setColor', 'setTextColor', 'setFgColor', 'setReadOnlyColor', 'setEnabledColor'):
        for argument in ('LLColor4(1.f, .7f, .15f, 1.f)', 'table.getColor("LabelTextColor")'):
            planted = f'control->{setter}({argument});'
            if not cpp_text_findings(planted, 'alfixture.cpp'):
                failures.append('runtime setter escaped: ' + planted)
        if cpp_text_findings(f'control->{setter}(table.getColor("KitText"));', 'alfixture.cpp'):
            failures.append('Kit setter control failed: ' + setter)
        else:
            print(f'ok    runtime {setter}: literal/non-Kit fail, Kit control passes')
    runtime_fixtures = [
        ('literal direct text draw fails', 'font->renderUTF8("caption", 0, 0, 0, LLColor4(1,.7,.1,1));', 'alfixture.cpp', True),
        ('legacy table direct text draw fails', 'font->renderUTF8("caption", 0, 0, 0, table.getColor("LabelTextColor"));', 'alfixture.cpp', True),
        ('indirect literal status caption fails', 'LLColor4 status_dot(.5,.8,.6,1); font->renderUTF8("caption",0,0,0,status_dot);', 'alfixture.cpp', True),
        ('literal replaces Kit alias fails', 'LLUIColor c = table.getColor("KitText"); c = LLColor4::white; control->setColor(c);', 'alfixture.cpp', True),
        ('set literal mutates Kit alias fails', 'LLUIColor c = ALKitStyle::color(ALKitStyle::TEXT); c.set(LLColor4::white); box->setColor(c);', 'alfixture.cpp', True),
        ('set live Kit alias passes', 'LLUIColor c = ALKitStyle::color(ALKitStyle::TEXT); c.set(table.getColor("KitText")); box->setColor(c);', 'alfixture.cpp', False),
        ('monitor helper literal caption fails', 'drawText(font,"caption",0,0,LLColor4(1,.7,.1,1));', 'alvcammonitorstrip.cpp', True),
        ('map helper light colour caption fails', 'place_label("Key",light.color,0,{},true,true);', 'allightrigmap.cpp', True),
        ('live Kit local and draw pass', 'const LLUIColor& c = table.getColor("KitText"); font->renderUTF8("caption",0,0,0,c.get());', 'alfixture.cpp', False),
        ('state dot fill stays outside text gate', 'LLColor4 status_dot(.5,.8,.6,1); gGL.color4fv(status_dot.mV); gl_circle_2d(0,0,3,10,true);', 'alfixture.cpp', False),
    ]
    # The original production defects must also fail when planted in memory.
    key_path = os.path.join(REPO, 'indra/newview/alpanelactorkey.cpp')
    with open(key_path, encoding='utf-8') as fh:
        key_source = fh.read()
    runtime_fixtures.append(('review ak_status white setter fails', key_source.replace('mReadouts[0]->setColor(tint)', 'mReadouts[0]->setColor(LLColor4::white)'), 'alpanelactorkey.cpp', True))
    runtime_fixtures.append(('review monitor status_dot caption fails', monitor_source.replace('caption->get(), rect.getWidth() - 16)', 'status_dot, rect.getWidth() - 16)'), 'alvcammonitorstrip.cpp', True))
    rig_path = os.path.join(REPO, 'indra/newview/alpanelcinelightrig.cpp')
    with open(rig_path, encoding='utf-8') as fh:
        rig_source = fh.read()
    runtime_fixtures.append(('review cached LabelTextColor fails', rig_source.replace('mGroupStatus->setColor(mRadiusDefaultColor)', 'mGroupStatus->setColor(LLUIColorTable::instance().getColor("LabelTextColor"))'), 'alpanelcinelightrig.cpp', True))
    runtime_fixtures.append(('review group amber setter fails', rig_source.replace('mGroupStatus->setColor(mRadiusWarningColor)', 'mGroupStatus->setColor(LLUIColor(LLColor4(1.f,.75f,.25f,1.f)))'), 'alpanelcinelightrig.cpp', True))
    # Removing a legacy-only guard invalidates its exact allowlist entry.
    legacy_call = next(call for (path, call) in CPP_TEXT_ALLOWLIST if path == 'llfloaterdirector.cpp')
    runtime_fixtures.append(('unguarded legacy cast setter fails', legacy_call + ';', 'llfloaterdirector.cpp', True))
    for label, source, path, must_fail in runtime_fixtures:
        if bool(cpp_text_findings(source, path)) != must_fail:
            failures.append(label)
        else:
            print('ok    ' + label)
    for fg, minimum in [('KitDialNeedle', 3.0), ('KitWarnBandText', 4.5), ('KitRecordText', 4.5)]:
        hover = dict(skin)
        hover['KitRowHover'] = hover[fg]
        if not any(f'{fg} over KitRowHover:' in error for error in contrast_findings(hover)):
            failures.append(f'{fg} hover contrast escaped')
        elif (fg, 'KitRowHover', minimum, False) not in contrast_pairs():
            failures.append(f'{fg} hover threshold drifted')
        else:
            print(f'ok    {fg} over KitRowHover fails at {minimum}:1')
    map_fixture = dict(skin)
    map_fixture['KitText'] = (1., 1., 1., 1.)
    for name in ('KitMapBackdropMid', 'KitMapBackdropEdge'):
        map_fixture[name] = (0., 0., 0., 1.)
    for name in ('KitMapHemisphereMid', 'KitMapHemisphereLow'):
        map_fixture[name] = (1., 1., 1., 0.)  # draw alpha must override this
    if not map_caption_findings(map_fixture):
        failures.append('composited map hemisphere contrast escaped')
    else:
        print('ok    map hemisphere composite fails despite readable backdrop endpoints')
    map_fixture = inverted_colors(skin)
    map_fixture['KitText'] = map_fixture['KitDim']
    if not map_caption_findings(map_fixture):
        failures.append('review inverted OFF caption contrast escaped')
    else:
        print('ok    review inverted map KitDim caption fails at 4.5:1')
    custom = dict(skin)
    custom['KitControlBg'] = (0., 0., 1., 1.)
    for name in TEXT_TIER:
        custom[name] = (.95, .95, .95, 1.)
    # M3 now catches one extra normal failure in the exact review reproduction:
    # the inherited red text is 4.45:1 on blue. Make it readable too, then prove
    # that a normal-valid custom file can still have an impossible inversion.
    exact_errors = contrast_findings(custom)
    if len(exact_errors) != 1 or not exact_errors[0].startswith('KitRecordText over KitControlBg:'):
        failures.append('exact blue reproduction no longer has the expected semantic finding')
    custom['KitRecordText'] = (.95, .95, .95, 1.)
    if contrast_findings(custom) or not contrast_findings(inverted_colors(custom)):
        failures.append('blue custom palette reproduction failed')
    else:
        print('ok    blue custom palette passes normal and fails completed inversion (viewer fallback case)')
    for label, stem, text, must_fail in fixtures:
        errors, effective = check_theme_text(stem, text, skin)
        if not errors:
            errors = contrast_findings(effective)
        if bool(errors) != must_fail:
            failures.append(f"{label}: expected {'failure' if must_fail else 'pass'}, got {errors}")
        else:
            print(f"ok    {label}")
    # Same input and output as test<9> in indra/newview/tests/aldirectortheme_test.cpp.
    vector = {name: tuple(value) for name, value in INVERT_VECTOR_IN.items()}
    vector = inverted_colors(vector)
    for name, expected in INVERT_VECTOR_OUT.items():
        if any(abs(a - b) > 0.0005 for a, b in zip(vector[name], expected)):
            failures.append(f"invert vector {name}: {vector[name]} != {expected}")
    print("ok    invert vector checked")
    failures += list_selection_selftest(skin)
    _errors, plain = check_theme_text("plain", _theme("plain"), skin)
    if not contrast_findings(inverted_colors(plain, nudge=False)):
        failures.append("raw inverted skin passes, so the readability step is untested")
    if contrast_findings(inverted_colors(plain)):
        failures.append("inverted skin fails after the readability step")
    if inverted_colors(plain)["KitRecord"] != plain["KitRecord"]:
        failures.append("KitRecord changed under Invert")
    print("ok    invert keeps Record and the readability step does real work")
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
    parser.add_argument("--check", action="store_true", help="check themes, C++ parity, Director XUI and runtime text colours")
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
