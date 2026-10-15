#!/usr/bin/env python3
"""Generate the non-shippable Director Kit P0 XUI foundation.

The input JSON is the design authority.  Existing Director leaf panels remain
byte-independent semantic embeds; Rig, Rim, Image, Roto Ink, Tron and Actor FX
are hosted kit pages, and Bloom is a new pure-setting page.  This tool
intentionally uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import itertools
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from xml.etree import ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
import director_theme_check as theme_check  # noqa: E402  (same-directory checker: theme validity and labels)


ROOT = Path(__file__).resolve().parents[2]
XUI = ROOT / "indra/newview/skins/default/xui/en"
KIT = ROOT / "scripts/code_tools/director_kit"
DEFAULT_DATA = KIT / "director_mockup_data.json"
DEFAULT_CONFIG = KIT / "kit_pages.json"
VCAM_DATA = KIT / "vcam_page.json"
SYNONYMS_DATA = KIT / "synonyms.json"
EXTERNAL_SEARCH_DATA = KIT / "external_search.json"
DEFAULT_OUT = XUI
THEMES_DIR = ROOT / "indra/newview/app_settings/director_themes"
V1_FLOATER = XUI / "floater_director.xml"
DIRECTOR_CPP = ROOT / "indra/newview/llfloaterdirector.cpp"
REPORT = KIT / "gen_report.txt"
INDEX_FIELDS = 22
EXPECTED_SHORTCUTS = {
    ("shot_tab", "CinematicCamJoint", "frame_orbit_tab/fo_anchor_joint"),
    ("shot_tab", "CinematicCamVertigoDuration", "moves_tab/zolly_duration"),
    ("shot_tab", "CinematicCamVertigoDirection", "moves_tab/zolly_direction"),
    ("shot_tab", "CinematicCamArcDuration", "moves_tab/hero_arc_duration"),
    ("shot_tab", "CinematicCamArcGeneratorAngle", "moves_tab/hero_arc_angle"),
    ("shot_tab", "CinematicCamArcHeightDrift", "moves_tab/hero_arc_height_drift"),
    ("cine_light_rig_tab", "cine_easy_cone_feather", "atmosphere_tab/ps_BDMergeProjectorVolumetricsFeather_slider"),
    ("cine_light_rig_tab", "cine_easy_shaft_length", "atmosphere_tab/ps_BDMergeProjectorVolumetricsMaxDistance_slider"),
    # 10/13: the user approved 5 image_tab -> exposure_tab shortcuts; add them when
    # exposure_tab becomes a kit page (see kit_pages.json image_tab omit_rows_note).
}

STRIP = {"left", "top", "bottom", "width", "height", "follows", "layout",
         "label_width", "text_width", "tab_group"}
LABEL_STRIP = {"slider", "spinner", "check_box"}
PURE_KINDS = {"slider", "number", "toggle", "combo", "color", "dial", "arc", "vec2", "vec3"}
NATIVE_TAG = {"slider": "slider", "number": "spinner", "toggle": "kit_switch",
              "combo": "combo_box", "color": "color_swatch", "readout": "text",
              "button": "button", "text": "line_editor", "list": "scroll_list",
              "dial": "compass_dial", "arc": "compass_dial",
              "segmented": "radio_group"}
HOST_KIND = {"slider": "slider", "spinner": "number", "check_box": "toggle",
             "combo_box": "combo", "line_editor": "text", "text": "readout",
             "button": "button", "color_swatch": "color", "icon": "gobo",
             "scroll_list": "list", "view_border": "viewport"}
CONTROL_TAGS = set(HOST_KIND) | {"kit_switch", "compass_dial", "radio_group", "panel"}
CUSTOM_KIND = {"check": "toggle", "slider": "slider", "spinner": "number",
               "combo": "combo", "button": "button", "line_editor": "text",
               "list": "list"}
ICONS = {"cast": "KitIcon_rail_cast_16", "perf": "KitIcon_rail_performance_16",
         "camera": "KitIcon_rail_camera_16", "lens": "KitIcon_rail_lens_16",
         "light": "KitIcon_rail_light_16", "look": "KitIcon_rail_look_16",
         "world": "KitIcon_rail_world_16", "capture": "KitIcon_rail_capture_16"}


class GenError(RuntimeError):
    pass


def parser() -> ET.XMLParser:
    return ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))


def parse_xml(path: Path) -> ET.Element:
    try:
        return ET.parse(path, parser=parser()).getroot()
    except (OSError, ET.ParseError) as exc:
        raise GenError(f"cannot parse {path}: {exc}") from exc


def sha16(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()[:16]


def source_bundle_sha(config_path: Path) -> str:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    paths = [Path(__file__), V1_FLOATER, DIRECTOR_CPP, SYNONYMS_DATA, EXTERNAL_SEARCH_DATA]
    paths += sorted(THEMES_DIR.glob("*.xml"))  # theme list rows and labels come from these files
    paths += [XUI / name for page in config.get("p0_pages", {}).values()
              for name in page.get("v1_sources", [])]
    digest = hashlib.sha256()
    for path in sorted(set(paths), key=lambda item: item.as_posix()):
        digest.update(path.resolve().as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()[:16]


def header(data_path: Path, config_path: Path) -> str:
    return ("GENERATED by scripts/code_tools/director_kit_gen.py from "
            f"director_mockup_data.json sha256={sha16(data_path)} + "
            f"kit_pages.json sha256={sha16(config_path)} + "
            f"vcam_page.json sha256={sha16(VCAM_DATA)} + "
            f"source bundle sha256={source_bundle_sha(config_path)}. Do not edit.")


def page_filename(group: str, page: str) -> str:
    g = group.removesuffix("_group")
    slug = page.removesuffix("_tab")
    if slug.startswith(g + "_"):
        slug = slug[len(g) + 1:]
    return f"panel_dpage_{g}_{slug}.xml"


def attr_order(item: tuple[str, str]) -> tuple[int, str]:
    # name first; the remaining generated attributes have one stable order.
    return (0 if item[0] == "name" else 1, item[0])


def ordered_tree(node: ET.Element) -> None:
    if isinstance(node.tag, str):
        if node.tag in CONTROL_TAGS:
            # Control attributes retain source order; only lift name to the front.
            node.attrib = dict(([('name', node.attrib['name'])] if 'name' in node.attrib else []) +
                               [(key, value) for key, value in node.attrib.items() if key != 'name'])
        else:
            node.attrib = dict(sorted(node.attrib.items(), key=attr_order))
    for child in node:
        ordered_tree(child)


def xml_bytes(root: ET.Element, note: str) -> bytes:
    ordered_tree(root)
    ET.indent(root, space="    ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    return (f'<?xml version="1.0" encoding="utf-8"?>\n<!-- {note} -->\n{body}\n').encode("utf-8")


def canonical(node: ET.Element) -> tuple:
    """A layout/format independent representation, including comments."""
    tag = "#comment" if node.tag is ET.Comment else str(node.tag)
    return (tag, tuple(sorted(node.attrib.items())), (node.text or "").strip(),
            tuple(canonical(child) for child in node))


def children_canonical(node: ET.Element) -> tuple:
    return tuple(canonical(child) for child in node)


def rows_of(card: dict):
    yield from card.get("rows", [])
    for option in card.get("opts", []):
        yield from option.get("rows", [])


def all_rows(page: dict):
    for card in page.get("cards", []):
        yield from rows_of(card)


def is_pure_row(row: dict) -> bool:
    kind = row.get("k")
    return ((kind in PURE_KINDS and row.get("s") and row.get("bind") != "code")
            or kind == "link" or (kind == "button" and row.get("target")))


def classify(group: str, page: dict, config: dict, leaves: dict[str, ET.Element]) -> str:
    key = f"{group}/{page['name']}"
    if key in config.get("p0_pages", {}):
        return "kit (P0)"
    if page["name"] in leaves:
        return "legacy"
    if all(is_pure_row(row) for row in all_rows(page)):
        return "pure kit (new)"
    if key not in config.get("launcher_targets", {}):
        raise GenError(f"new non-pure page {key} has no launcher_targets entry")
    return "launcher"


def find_named(root: ET.Element, name: str) -> ET.Element | None:
    return next((node for node in root.iter() if node.get("name") == name), None)


def flatten_pages(data: dict) -> list[tuple[dict, dict]]:
    return [(group, page) for group in data.get("groups", []) for page in group.get("pages", [])]


def validate_action_rows(page: dict) -> None:
    for row in all_rows(page):
        target = row.get("target")
        if not target:
            continue
        action = (f"lightbox:{row.get('target_tab', '')}" if target == "lightbox"
                  else f"floater:{target}")
        prefix, separator, name = action.partition(":")
        if (not separator or prefix not in {"lightbox", "floater"}
                or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name)):
            raise GenError(f"invalid action target {action!r} on {page['name']}/{row.get('w')}")


def validate_data(data: dict) -> None:
    pages = flatten_pages(data)
    row_count = sum(1 for _group, page in pages for _row in all_rows(page))
    # 1959: VCam "Settings for" selector + Add camera + Remove capture merged into one row (mockup, 10/11).
    # 1960: + VCam Output 'Projectors' row (VP5, 10/12).
    # 2006: + 46 VCam Projector card rows (framing/light/film/signal/TV, 10/12).
    # 2010: + 4 Light Rig 'Projector feed' combos (key/fill/rim/bg, VP6 10/12).
    # 2011: + 1 VCam Projector 'Lamp power' row.
    # 2013: + VCam camera Name and no-projector-selection hint rows (10/12 polish).
    # 2059: stepped_tab +46 rows (10/14).
    # 10/13 look-group conversion: the mockup source is unchanged (still 2013) except for explicit iv values
    # on two combos (colour LUT filenames, Actor FX look ids) and five approved Image-to-Exposure shortcuts.
    # Image, Roto Ink, Tron and Actor FX are generated kit pages. The three Rig Rim tint source rows are not
    # generated on Tron because that page links to their Light > Rim home instead.
    # Quick favorites are runtime row proxies, so they intentionally do not change this source-data count.
    # Camera/capture completion adds recorder status, two temporal readouts and
    # the Scene login-self toggle.  This is source-data completeness, not a
    # generated/legacy page-classification pin.
    # 2065: + Step particles and Step texture animation toggles (10/15).
    # 2066: + Actors Remember-between-sessions toggle; Isolation test combo replaced by one trigger button (10/15)
    # 2071: Director Settings adds six rows and rehomes the Scene cast-at-login row.
    # 2072: Include ghost clones adds one Stepped row (10/15).
    # 2073: Capture > Scene regains the "Add me to the cast at login" home row (+1); Director
    #       Settings keeps a link row to it in place of its former toggle (net 0) (10/15).
    # 2073 (unchanged): Director Settings Appearance readout settings_appearance_info becomes the
    #       DirectorTheme combo settings_theme (1 row replaced, net 0) (10/15).
    # 2075: Director Settings 'Restore last unsaved scene' button becomes the crash-backup list: a code-bound
    #       combo (settings_crash_backups) plus Restore and Delete buttons (1 row replaced by 3, net +2) (10/15).
    if len(pages) != 37 or row_count != 2075:
        raise GenError(f"mockup completeness drift: {len(pages)} pages, {row_count} rows; expected 37/2075")
    shortcuts: set[tuple[str, str, str]] = set()
    for _group, page in pages:
        for card in page.get("cards", []):
            for option in card.get("opts", []):
                if "value" not in option:
                    raise GenError(f"selector {page['name']}/{card['id']} option {option.get('label')!r} lacks explicit value")
            for row in rows_of(card):
                if row.get("shortcut_of"):
                    shortcuts.add((page["name"], str(row.get("w", "")), str(row["shortcut_of"])))
    if shortcuts != EXPECTED_SHORTCUTS:
        raise GenError(f"shortcut contract drift: {sorted(shortcuts ^ EXPECTED_SHORTCUTS)}")
    for _group, page in pages:
        validate_action_rows(page)


def normalize_shortcuts(data: dict) -> None:
    """Make every shortcut's presentation metadata identical to its canonical home."""
    pages = {page["name"]: page for _group, page in flatten_pages(data)}
    rows = {(page["name"], str(row.get("w", ""))): row
            for _group, page in flatten_pages(data) for row in all_rows(page)}
    for _group, page in flatten_pages(data):
        for row in all_rows(page):
            target = row.get("shortcut_of")
            if not target:
                continue
            target_page, sep, target_widget = str(target).rpartition("/")
            home = rows.get((target_page, target_widget))
            if not sep or target_page not in pages or home is None:
                raise GenError(f"shortcut {page['name']}/{row.get('w')} has missing home {target!r}")
            for key in ("k", "mn", "mx", "st", "dc", "unit"):
                if key in home:
                    row[key] = copy.deepcopy(home[key])
                else:
                    row.pop(key, None)
            home_label = f"{pages[target_page].get('label', target_page)} > {home.get('l', target_widget)}"
            help_text = str(row.get("h", "")).strip()
            if home_label.lower() not in help_text.lower():
                row["h"] = f"{help_text} Home: {home_label}.".strip()


def pattern_names(config_path: Path) -> set[str]:
    path = config_path.with_name("cpp_name_patterns.json")
    if not path.is_file():
        return set()
    raw = json.loads(path.read_text(encoding="utf-8"))
    result: set[str] = set()
    for pattern in raw.get("patterns", []):
        holes = re.findall(r"\{(\w+)\}", pattern["template"])
        for values in itertools.product(*(pattern.get(hole, []) for hole in holes)):
            name = pattern["template"]
            for hole, value in zip(holes, values):
                name = name.replace("{" + hole + "}", str(value))
            result.add(name)
    return result


def cpp_names(config_path: Path) -> set[str]:
    names = pattern_names(config_path)
    literal = re.compile(r'"([A-Za-z_][A-Za-z0-9_]*)"')
    for path in itertools.chain((ROOT / "indra/newview").glob("*.cpp"),
                                (ROOT / "indra/newview").glob("*.h")):
        names.update(literal.findall(path.read_text(encoding="utf-8", errors="replace")))
    return names


def source_index(paths: list[Path]) -> tuple[dict[str, ET.Element], dict[str, list[ET.Element]]]:
    by_name: dict[str, ET.Element] = {}
    by_control: dict[str, list[ET.Element]] = defaultdict(list)
    for path in paths:
        root = parse_xml(path)
        for node in root.iter():
            name = node.get("name")
            if name:
                if name in by_name:
                    raise GenError(f"duplicate widget name {name!r} in P0 v1 sources")
                by_name[name] = node
            if node.get("control_name"):
                by_control[node.get("control_name", "")].append(node)
    return by_name, by_control


def apply_config(page: dict, cfg: dict) -> dict:
    page = copy.deepcopy(page)
    cards = {card["id"]: card for card in page.get("cards", [])}
    for key, changes in cfg.get("row_overrides", {}).items():
        hits = [row for card in page.get("cards", []) for row in rows_of(card)
                if row.get("w") == key or row.get("s") == key]
        if len(hits) != 1:
            raise GenError(f"row_overrides key {key!r} matched {len(hits)} rows on {page['name']}")
        hits[0].update(changes)
    for addition in cfg.get("add_rows", []):
        card = cards.get(addition["card"])
        if card is None:
            raise GenError(f"add_rows card {addition['card']!r} not found on {page['name']}")
        target = card.setdefault("rows", [])
        if "option" in addition:
            options = [o for o in card.get("opts", []) if o.get("label") == addition["option"]]
            if len(options) != 1:
                raise GenError(f"add_rows option {addition['option']!r} not found uniquely")
            target = options[0].setdefault("rows", [])
        row = copy.deepcopy(addition["row"])
        if addition.get("first"):
            target.insert(0, row)
        elif addition.get("after"):
            indexes = [i for i, old in enumerate(target) if old.get("w") == addition["after"]]
            if len(indexes) != 1:
                raise GenError(f"add_rows after {addition['after']!r} matched {len(indexes)} rows")
            target.insert(indexes[0] + 1, row)
        else:
            target.append(row)
    # omit_rows: mockup rows that may not be generated on this page. Each name must match exactly
    # one row and the page must provide a link to the rows' canonical home.
    for name in cfg.get("omit_rows", []):
        removed = 0
        for card in page.get("cards", []):
            for holder in [card] + list(card.get("opts", [])):
                if "rows" not in holder:
                    continue
                before = len(holder["rows"])
                holder["rows"] = [row for row in holder["rows"] if row.get("w") != name]
                removed += before - len(holder["rows"])
        if removed != 1:
            raise GenError(f"omit_rows {name!r} matched {removed} rows on {page['name']}")
    for card_id in cfg.get("omit_empty_cards", []):
        card = cards.get(card_id)
        if card is None or any(rows_of(card)):
            raise GenError(f"omit_empty_cards {card_id!r} is missing or non-empty on {page['name']}")
    return page


def matched_node(row: dict, by_name: dict[str, ET.Element],
                 by_control: dict[str, list[ET.Element]]) -> ET.Element | None:
    node = by_name.get(str(row.get("source_widget", row.get("w", ""))))
    if node is not None and (row.get("w") != "check_box" or not row.get("s")):
        return node
    candidates = by_control.get(str(row.get("s", "")), []) if row.get("s") else []
    if len(candidates) == 1:
        return candidates[0]
    if node is not None:
        return node
    return None


def stripped_copy(node: ET.Element, strip_label: bool = True) -> ET.Element:
    clone = copy.deepcopy(node)
    for attr in STRIP:
        clone.attrib.pop(attr, None)
    if strip_label and clone.tag in LABEL_STRIP:
        clone.attrib.pop("label", None)
    return clone


# Transport buttons whose fill is accent or record colour; their icons use KitIconOnAccent.
ACCENT_FILL_BUTTONS = ("btn_action", "btn_cut", "btn_arm")


def apply_kit_xui_style(root: ET.Element) -> None:
    """Stamp only generated Director-v2 controls; v1/global widget defaults stay untouched."""
    for node in list(root.iter()):
        if node.get("image_overlay", "").startswith("KitIcon_") and "image_overlay_color" not in node.attrib:
            # Icons on an accent or record fill take KitIconOnAccent; other kit icons follow the
            # colour theme (KitIconTint = white at 0.75 in the Default skin).
            on_fill = node.tag == "button" and node.get("name", "") in ACCENT_FILL_BUTTONS
            node.set("image_overlay_color", "KitIconOnAccent" if on_fill else "KitIconTint")
        if node.tag == "button":
            for attr in ("image_unselected", "image_selected", "image_pressed",
                         "image_hover_unselected", "image_hover_selected"):
                node.set(attr, "Kit_RoundRect4")
            name = node.get("name", "")
            node.set("image_color", "KitAccent" if name == "btn_action" else
                     "KitRecord" if name == "btn_cut" else "KitControlBg")
            node.set("label_color", "KitKnobOn" if name == "btn_action" else
                     "KitRecordTextOn" if name == "btn_cut" else "KitLabel")
            node.set("label_color_selected", "KitTextStrong")
            node.set("label_color_disabled", "KitFaint")
            node.set("label_shadow", "false")
            if name.startswith(("switcher_slot_", "anim_slot_")):
                node.set("image_color_selected", "KitAccent")
                node.set("image_color_pressed", "KitAccentHi")
                node.set("label_color_selected", "KitAccentTextOn")
        elif node.tag == "line_editor":
            node.set("use_bg_color", "true")
            node.set("bg_color", "KitInputBg")
            node.set("text_color", "KitText")
            node.set("text_readonly_color", "KitDim")
            node.set("text_tentative_color", "KitMuted")
            node.set("cursor_color", "KitAccent")
        elif node.tag == "combo_box":
            drop = next((child for child in node if child.tag == "combo_box.drop_down_button"), None)
            if drop is None:
                drop = ET.SubElement(node, "combo_box.drop_down_button")
            for attr in ("image_unselected", "image_selected", "image_pressed"):
                drop.set(attr, "Kit_RoundRect4")
            drop.set("image_color", "KitControlBg")
            drop.set("image_overlay", "KitIcon_chevron_down_10_16")
            drop.set("image_overlay_color", "KitIconTint")
            drop.set("image_overlay_alignment", "right")
            drop.set("pad_right", "6")
            editor = next((child for child in node if child.tag == "combo_box.combo_editor"), None)
            if editor is None:
                editor = ET.SubElement(node, "combo_box.combo_editor")
            editor.set("use_bg_color", "true")
            editor.set("bg_color", "KitInputBg")
            editor.set("text_color", "KitText")
        elif node.tag == "slider":
            bar = next((child for child in node if child.tag == "slider.slider_bar"), None)
            if bar is None:
                bar = ET.SubElement(node, "slider.slider_bar")
            bar.set("track_image_horizontal", "Kit_SliderTrack")
            bar.set("track_highlight_horizontal_image", "Kit_SliderTrack")
            bar.set("track_color", "KitSliderTrackEmpty")
            bar.set("track_highlight_color", "KitSliderTrackFill")
            bar.set("thumb_image", "Kit_SliderThumb")
            bar.set("thumb_image_pressed", "Kit_SliderThumb")
            bar.set("thumb_image_disabled", "Kit_SliderThumb")
            bar.set("thumb_center_color", "KitSliderThumb")
            bar.set("thumb_outline_color", "KitSliderThumb")
        elif node.tag == "spinner":
            node.set("text_enabled_color", "KitText")
            node.set("text_disabled_color", "KitMuted")
        elif node.tag == "color_swatch":
            node.set("border_color", "KitSwatchBorder")


def reset_setting(name: str, by_name: dict[str, ET.Element], setting: str | None) -> str | None:
    reset = by_name.get(name + "_reset")
    if reset is not None:
        if reset.get("commit_callback.parameter"):
            return reset.get("commit_callback.parameter")
        cb = next((n for n in reset.iter() if n.tag == "commit_callback" and n.get("parameter")), None)
        if cb is not None:
            return cb.get("parameter")
    return setting


def data_kind(row: dict) -> tuple[str, bool]:
    if row.get("k") == "custom":
        mapped = CUSTOM_KIND.get(row.get("ty", ""), "custom")
        return mapped, row.get("ty") in {"check", "slider", "spinner", "combo", "line_editor"}
    return str(row.get("k", "custom")), row.get("bind") == "code"


def control_for(row: dict, hosted: ET.Element | None, referenced: set[str], report: list[str]) -> tuple[str, ET.Element | None, str]:
    requested, custom_code = data_kind(row)
    widget = str(row.get("w", ""))
    if hosted is not None:
        tag = hosted.tag
        kind = HOST_KIND.get(tag, "custom")
        child = stripped_copy(hosted)
        child.set("name", widget)
        for node in child.iter():
            if node.get("name") in row.get("child_names", {}):
                node.set("name", str(row["child_names"][node.get("name")]))
        if tag == "check_box":
            child.tag = "kit_switch"
        elif tag in {"slider", "spinner"} and requested in {"dial", "arc"} and widget not in referenced:
            child.tag = "compass_dial"
            child.set("mode", "compass" if requested == "dial" else "arc")
            child.set("kit_geometry", "true")
            child.set("show_readout", "false")
            for attr in ("can_edit_text", "show_text", "text_width", "label_width"):
                child.attrib.pop(attr, None)
            kind = requested
            report.append(f"UPGRADE {widget}: {tag} -> compass_dial/{child.get('mode')}")
        elif tag == "spinner" and requested == "slider" and widget not in referenced:
            child.tag = "slider"
            kind = "slider"
            report.append(f"UPGRADE {widget}: spinner -> slider")
        if child.tag == "slider":
            child.set("show_text", "true")
            # P0 spec 3.3 gives sliders the complete VALUE_W slot.  This is
            # the one intentional exception to the generic hosted-control
            # geometry stripping rule in 4.5.
            child.set("text_width", "52")
        if child.tag == "compass_dial":
            child.set("kit_geometry", "true")
            child.set("show_readout", "false")
        for src, dst in (("mn", "min_val"), ("mx", "max_val"),
                         ("st", "increment"), ("dc", "decimal_digits")):
            if src in row:
                child.set(dst, scalar(row[src]))
        if row.get("allow_text_entry") and child.tag == "combo_box":
            child.set("allow_text_entry", "true")
        if row.get("toggle_button") and child.tag == "button":
            child.set("is_toggle", "true")
        if row.get("disabled"):
            child.set("enabled", "false")
        if row.get("tip"):
            child.set("tool_tip", str(row["tip"]))
        if row.get("enabled_control"):
            child.set("enabled_control", str(row["enabled_control"]))
        if kind == "readout" and row.get("row_height"):
            child.set("word_wrap", "true")
        return kind, child, "setting" if hosted.get("control_name") else "code"
    kind, _ = data_kind(row)
    if kind in {"vec2", "vec3", "link"}:
        return kind, None, "setting" if row.get("s") else "code"
    custom_tag = row.get("custom_tag")
    if custom_tag:
        child = ET.Element(str(custom_tag), {"name": widget})
        return kind, child, "code"
    tag = NATIVE_TAG.get(kind)
    if tag is None:
        if kind in {"viewport", "gobo", "custom"}:
            raise GenError(f"kit-native {kind} row {widget!r} is forbidden in P0")
        raise GenError(f"no native tag for row {widget!r} kind {kind!r}")
    attrs = {"name": widget}
    setting = row.get("s")
    bind = "code" if custom_code else ("setting" if setting else "code")
    if bind == "setting":
        attrs["control_name"] = str(setting)
    if kind in {"dial", "arc"}:
        attrs["mode"] = "compass" if kind == "dial" else "arc"
        attrs["kit_geometry"] = "true"
        attrs["show_readout"] = "false"
    if kind == "slider":
        attrs["show_text"] = "true"
        attrs["text_width"] = "52"
    if kind == "button":
        attrs["label"] = str(row.get("btn") or row.get("l", ""))
    for src, dst in (("mn", "min_val"), ("mx", "max_val"), ("st", "increment"), ("dc", "decimal_digits")):
        if src in row:
            attrs[dst] = scalar(row[src])
    if row.get("allow_text_entry") and tag == "combo_box":
        attrs["allow_text_entry"] = "true"
    if row.get("toggle_button") and tag == "button":
        attrs["is_toggle"] = "true"
    if row.get("disabled"):
        attrs["enabled"] = "false"
    if row.get("tip"):  # [Connectedness B9] widget tooltip, e.g. "Not connected yet" on a disabled row
        attrs["tool_tip"] = str(row["tip"])
    if row.get("enabled_control"):
        attrs["enabled_control"] = str(row["enabled_control"])
    if kind == "readout" and row.get("row_height"):
        attrs["word_wrap"] = "true"
    child = ET.Element(tag, attrs)
    if kind == "readout" and row.get("text") is not None:
        child.text = str(row["text"])
    if tag in {"combo_box", "radio_group"}:
        for index, label in enumerate(row.get("it", [])):
            values = row.get("iv", [])
            value = values[index] if index < len(values) else index
            item_tag = "combo_box.item" if tag == "combo_box" else "radio_item"
            item_attrs = {"name": f"{widget}_item_{index}", "label": str(label), "value": scalar(value)}
            if scalar(value) == "-1":
                # A value of -1 is a divider label (e.g. Actor FX "-- Live Actor Exclusives --"),
                # not a selectable enum; the v1 combo disables it the same way.
                item_attrs["enabled"] = "false"
            if tag == "radio_group":
                # LLRadioGroup otherwise applies the default top-left layout to
                # every geometry-less item and stacks the segments vertically.
                # Kit rule: top + height (never bottom=).
                item_attrs.update({"layout": "topleft", "left": str(index * 80), "top": "0",
                                   "width": "80", "height": "24"})
            ET.SubElement(child, item_tag, item_attrs)
    return kind, child, bind


def scalar(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def validate_vcam_page(page: dict) -> None:
    """Keep the mockup's Settings-for selector/actions as one custom row."""
    rows = [row for card in page.get("cards", []) for row in card.get("rows", [])]
    selector = [row for row in rows if row.get("w") == "vc_camera_selector"]
    if len(selector) != 1 or selector[0].get("custom_tag") != "vcam_camera_selector":
        raise GenError("VCam requires exactly one custom vc_camera_selector row")
    obsolete = {row.get("w") for row in rows} & {"vc_add_camera", "vc_remove_capture"}
    if obsolete:
        raise GenError("VCam add/remove actions are integrated into vc_camera_selector")


def row_element(row: dict, by_name: dict[str, ET.Element], by_control: dict[str, list[ET.Element]],
                referenced: set[str], report: list[str], names: set[str],
                label_names: dict[str, str]) -> ET.Element:
    hosted = matched_node(row, by_name, by_control)
    kind, child, bind = control_for(row, hosted, referenced, report)
    if row.get("bind") == "none":
        bind = "none"
    widget = str(row.get("w", ""))
    attrs = {"kind": kind, "widget": widget, "label": str(row.get("l", ""))}
    label_name = label_names.get(widget)
    if label_name:
        attrs["label_name"] = label_name
    setting = hosted.get("control_name") if hosted is not None and hosted.get("control_name") else row.get("s")
    if setting:
        attrs["setting"] = str(setting)
    if kind in {"vec2", "vec3"} and bind == "setting":
        # Composite rows have no control child: ALKitRow builds its spinners and an
        # ALKitVecBinding over the row's setting itself (alkitrow.cpp resolveBinding), so
        # kit_lint's bind="setting" control-child rule cannot apply to them.
        bind = "none"
    attrs["bind"] = bind
    if row.get("bulk_reset") is False:
        attrs["bulk_reset"] = "false"
    if row.get("no_widget_history"):
        attrs["widget_history"] = "false"
    if row.get("disabled"):
        # [Connectedness B9] A disabled row is not connected to anything: the runtime skips it
        # in Reset page/card, paste, Quick favourites and Advanced reveal.
        attrs["unconnected"] = "true"
    mapping = (("h", "help"), ("kw", "keywords"), ("unit", "unit"), ("sub", "sub"),
               ("to", "link_to"), ("shortcut_of", "shortcut_of"), ("mirror_setting", "mirror_setting"))
    for src, dst in mapping:
        if row.get(src) not in (None, ""):
            attrs[dst] = scalar(row[src])
    for src, dst in (("a", "advanced"), ("added", "added"), ("cp", "card_preset"),
                     ("who_picker", "who_picker"), ("hidden", "hidden")):
        if row.get(src):
            attrs[dst] = "true"
    for src, dst in (("mn", "min_val"), ("mx", "max_val"), ("st", "increment"), ("dc", "decimals"),
                      ("row_height", "row_height")):
        if src in row:
            attrs[dst] = scalar(row[src])
    # Conditional rows are created once and hidden until their page adapter
    # applies the named condition. The expression remains authoritative in the
    # generator data; the adapter caches every affected row in postBuild().
    if row.get("show_if"):
        attrs["visible"] = "false"
    if row.get("bind") == "code" and "d" in row:
        attrs["default_value"] = scalar(row["d"])
    if kind == "button" and row.get("target"):
        attrs["action"] = (f"lightbox:{row.get('target_tab', '')}" if row["target"] == "lightbox"
                           else f"floater:{row['target']}")
    reset = row.get("reset_settings") or reset_setting(widget, by_name, str(setting) if setting and bind == "setting" else None)
    if reset:
        attrs["reset_settings"] = reset
    if hosted is not None and row.get("s") and hosted.get("control_name") != row.get("s"):
        report.append(f"WARNING CONTROL_NAME {widget}: data={row.get('s')} v1={hosted.get('control_name') or '<none>'}; v1 trusted")
    if hosted is not None and hosted.tag == "text" and hosted.get("word_wrap", "false").lower() == "true":
        attrs["row_height"] = str(int(hosted.get("height", "22")) + 6)
    result = ET.Element("kit_row", attrs)
    if child is not None:
        result.append(child)
    names.add(widget)
    names.add(label_name or widget + "_label")
    if reset:
        names.add(widget + "_reset")
    elif row.get("bind") == "code" and "d" in row:
        names.add(widget + "_reset")
    if kind in {"slider", "number", "dial", "arc"}:
        names.add(widget + "_value")
        if row.get("unit"):
            names.add(widget + "_unit")
    if kind == "toggle":
        names.add(widget + "_state")
    return result


def page_attrs(group: dict, page: dict, cfg: dict) -> dict[str, str]:
    attrs = {"name": page["name"].removesuffix("_tab") + "_kit",
             "page": f"{group['name']}/{page['name']}", "summary": str(page.get("sum", ""))}
    master = page.get("master") or {}
    if master.get("w"):
        attrs["master_widget"] = str(master["w"])
        attrs["master_label"] = str(master.get("l", ""))
        # A master with no setting (Actor FX: per-actor state in llfloaterdirector) must not
        # serialize Python's None as the setting name "None", nor an empty setting name.
        if master.get("s"):
            attrs["master_setting"] = str(master["s"])
    if page.get("who"):
        attrs["who_label"] = str(page["who"].get("label", ""))
        if page["who"].get("pickers"):
            attrs["who_picker"] = str(page["who"]["pickers"][0])
        if cfg.get("who_adapter"):
            attrs["who_adapter"] = str(cfg["who_adapter"])
    modes = page.get("modes") or {}
    if modes:
        attrs["modes"] = "|".join(str(x) for x in modes.get("options", []))
        if modes.get("setting"):
            attrs["mode_setting"] = str(modes["setting"])
        if modes.get("widget"):
            attrs["mode_widget"] = str(modes["widget"])
        if cfg.get("mode_on"):
            attrs["mode_on"] = str(cfg["mode_on"])
        if cfg.get("mode_adapter"):
            attrs["mode_adapter"] = str(cfg["mode_adapter"])
    preset = cfg.get("preset")
    if preset:
        attrs["preset_mode"] = str(preset["mode"])
        if preset.get("provider"):
            attrs["preset_provider"] = str(preset["provider"])
    elif page.get("presets"):
        attrs["preset_mode"] = "none"
    if cfg.get("undo_context"):
        attrs["undo_context"] = str(cfg["undo_context"])
    if cfg.get("chip_mode"):
        attrs["chip_mode"] = str(cfg["chip_mode"])
    settings = [] if cfg.get("no_history") else sorted(
        {str(row["s"]) for row in all_rows(page) if row.get("s")} | set(cfg.get("watch_extra", [])))
    if settings:
        attrs["watch_settings"] = ",".join(settings)
    return attrs


def kit_page(group: dict, raw_page: dict, cfg: dict, source_paths: list[Path],
             referenced: set[str], report: list[str]) -> tuple[ET.Element, set[str]]:
    page = apply_config(raw_page, cfg)
    validate_action_rows(page)
    if cfg.get("no_history"):
        for row in all_rows(page):
            row["bind"] = "none"
    by_name, by_control = source_index(source_paths) if source_paths else ({}, defaultdict(list))
    names: set[str] = set()
    root_attrs = {"follows": "all", "name": cfg.get("root_name", page["name"] + "_logic")}
    if cfg.get("root_class"):
        root_attrs["class"] = str(cfg["root_class"])
    root = ET.Element("panel", root_attrs)
    body = ET.SubElement(root, "kit_page", page_attrs(group, page, cfg))
    omitted_cards = set(cfg.get("omit_empty_cards", []))
    if cfg.get("actor_target"):
        # Same flag the retired legacy path emitted from page_features: the page styles the
        # selected cast member through llfloaterdirector's bindActorStylePage.
        body.set("actor_target", "true")
    modes = page.get("modes") or {}
    for mode in modes.get("options", []):
        ET.SubElement(body, "kit_page.mode", {"name": str(mode),
                      "cards": ",".join(str(x) for x in modes.get(mode, []))})
    for section in page.get("sections", []):
        ids = [page["cards"][int(i)]["id"] for i in section.get("cards", [])
               if page["cards"][int(i)]["id"] not in omitted_cards]
        ET.SubElement(body, "kit_page.section", {"id": str(section["id"]),
                      "label": str(section.get("label", "")), "cards": ",".join(ids)})
    for card in page.get("cards", []):
        if card["id"] in omitted_cards:
            continue
        attrs = {"name": "kc_" + str(card["id"]), "id": str(card["id"]),
                 "title": str(card.get("t", "")), "kind": str(card.get("k", "rows"))}
        copy_type = card.get("copy_type") or cfg.get("card_copy_types", {}).get(card["id"])
        if copy_type:
            attrs["copy_type"] = str(copy_type)
        for src, dst in (("a", "advanced"), ("new", "badge"), ("help", "help"),
                         ("sel_label", "sel_label"), ("to", "link_to")):
            if card.get(src):
                attrs[dst] = "NEW" if src == "new" else scalar(card[src])
        if card.get("badge"):
            attrs["badge"] = scalar(card["badge"])
        enable_widget = card.get("ew") or cfg.get("card_enables", {}).get(card["id"])
        enable_setting = card.get("es")
        master = page.get("master") or {}
        if enable_widget == master.get("w"):
            enable_widget = None
            enable_setting = None
        if enable_widget:
            attrs["enable_widget"] = str(enable_widget)
            names.add(str(enable_widget))
            source = by_name.get(str(enable_widget))
            if source is not None and source.get("tool_tip"):
                attrs["enable_tooltip"] = source.get("tool_tip", "")
            if not enable_setting and source is not None and source.get("control_name"):
                enable_setting = source.get("control_name")
        if enable_setting:
            attrs["enable_setting"] = str(enable_setting)
        if card.get("opts"):
            attrs["kind"] = "selector"
            setting = card.get("sel_setting") or cfg.get("selector_settings", {}).get(card["id"])
            if setting:
                attrs["sel_setting"] = str(setting)
        card_node = ET.SubElement(body, "kit_card", attrs)
        if card.get("k") == "rigmap":
            ET.SubElement(card_node, "light_rig_map", {"name": "cine_light_map",
                          "selector_card": "kc_lights", "view_setting": "DirectorV2LightMapView",
                          "side_from_setting": "DirectorV2LightMapSideFrom"})
            names.add("cine_light_map")
        for row in card.get("rows", []):
            # A show_if value of "never" marks manifest-only metadata. Unlike
            # runtime conditions, it must not create a row: ALKitCard owns row
            # visibility during layout and would otherwise reveal it.
            if row.get("show_if") == "never":
                continue
            card_node.append(row_element(row, by_name, by_control, referenced, report, names,
                                         cfg.get("label_names", {})))
        for option_index, option in enumerate(card.get("opts", [])):
            if "value" not in option:
                raise GenError(f"selector {card['id']} option {option.get('label')} lacks explicit value")
            option_attrs = {"label": str(option.get("label", "")), "value": scalar(option["value"])}
            ew = option.get("ew") or cfg.get("option_enables", {}).get(card["id"], {}).get(option.get("label"))
            if ew:
                option_attrs["enable_widget"] = str(ew)
                names.add(str(ew))
            option_setting = option.get("es")
            option_source = by_name.get(str(ew)) if ew else None
            if not option_setting and option_source is not None:
                option_setting = option_source.get("control_name")
            if option_setting:
                option_attrs["enable_setting"] = str(option_setting)
            option_node = ET.SubElement(card_node, "kit_option", option_attrs)
            for row in option.get("rows", []):
                if row.get("show_if") == "never":
                    continue
                option_node.append(row_element(row, by_name, by_control, referenced, report, names,
                                               cfg.get("label_names", {})))
    preset = cfg.get("preset", {})
    if preset.get("mode") == "widgets":
        host = ET.SubElement(body, "kit_preset_host", {
            "name": page["name"].removesuffix("_tab") + "_preset_host"})
        for key in ("combo", "apply", "save", "delete"):
            name = preset.get(key)
            if not name and key != "combo":
                continue
            if not name or name not in by_name:
                raise GenError(f"preset widget {name!r} not found")
            host.append(stripped_copy(by_name[name], strip_label=False))
            names.add(name)
    master = page.get("master") or {}
    if master.get("w"):
        names.add(str(master["w"]))
    if modes.get("widget"):
        names.add(str(modes["widget"]))
    return root, names


def apply_page_features(root: ET.Element, key: str, features: dict) -> None:
    feature = features.get(key, {})
    if feature.get("bulk_subject_assignment"):
        card = next((node for node in root.iter()
                     if node.get("name") in {"shot_subjects_card", "kc_subjects"}), None)
        if card is None:
            raise GenError("bulk subject assignment requires shot_subjects_card")
        if not any(node.get("name") == "btn_assign_subjects_ad" for node in card.iter()):
            ET.SubElement(card, "button", {"name": "btn_assign_subjects_ad", "label": "Assign A-D from selection",
                          "tool_tip": "Assign the selected roster rows to A, B, C and D in roster order",
                          "follows": "left|top", "layout": "topleft", "left": "10", "top": "108",
                          "width": "318", "height": "20"})
        for subject in "abcd":
            setter = next((node for node in card.iter() if node.get("name") == f"btn_set_{subject}"), None)
            if setter is None:
                raise GenError(f"bulk subject assignment requires btn_set_{subject}")
            setter.tag = "dnd_button"
            # On a kit page the drop-target button sits in a kit_row; a dnd_button is not
            # the plain <button> a kind="button" row requires, so the row is a custom host.
            for row in card.iter("kit_row"):
                if setter in list(row):
                    row.set("kind", "custom")


def legacy_page(group: dict, page: dict, leaf: ET.Element, features: dict) -> tuple[ET.Element, set[str]]:
    attrs = {"name": page["name"] + "_content", "page": f"{group['name']}/{page['name']}",
             "legacy": "true", "summary": str(page.get("sum", "")),
             "width": leaf.get("width", "711"), "height": leaf.get("height", "525")}
    if features.get(f"{group['name']}/{page['name']}", {}).get("actor_target"):
        attrs["actor_target"] = "true"
    if features.get(f"{group['name']}/{page['name']}", {}).get("actor_target_synthetic"):
        attrs["actor_target_synthetic"] = "true"
    settings = sorted({str(row["s"]) for row in all_rows(page) if row.get("s")})
    if settings:
        attrs["watch_settings"] = ",".join(settings)
    root = ET.Element("kit_page", attrs)
    body = root
    originals = children_canonical(leaf)
    for child in leaf:
        body.append(copy.deepcopy(child))
    key = f"{group['name']}/{page['name']}"
    apply_page_features(body, key, features)
    if not features.get(key) and children_canonical(body) != originals:
        raise GenError(f"canonical v1 children copy failed for {page['name']}")
    names = {node.get("name") for node in body.iter() if node.get("name")}
    return root, {str(x) for x in names}


def launcher_page(group: dict, page: dict, target: dict) -> tuple[ET.Element, set[str]]:
    root = ET.Element("panel", {"follows": "all", "name": page["name"] + "_launcher_logic"})
    body = ET.SubElement(root, "kit_page", {"name": page["name"] + "_kit",
                         "page": f"{group['name']}/{page['name']}", "summary": str(page.get("sum", ""))})
    ET.SubElement(body, "kit_page.section", {"id": "open", "label": page.get("label", ""), "cards": "open"})
    card = ET.SubElement(body, "kit_card", {"name": "kc_open", "id": "open", "title": page.get("label", ""),
                         "kind": "launcher", "target": target["target"], "target_tab": target["target_tab"],
                         "button_label": target["button_label"]})
    # ALKitCard creates kc_open_launch for kind=launcher.  A serialized row
    # here would draw a second, identical button.
    return root, {"kc_open", "kc_open_launch"}


def quick_page() -> ET.Element:
    """Quick rows are runtime proxies; this generated shell owns their layout host."""
    root = ET.Element("panel", {"follows": "all", "name": "quick_tab_logic"})
    body = ET.SubElement(root, "kit_page", {"name": "quick_kit", "page": "quick_group/quick_tab",
                         "summary": "Live shortcuts to starred Director controls."})
    ET.SubElement(body, "kit_page.section", {"id": "quick", "label": "Quick", "cards": "quick"})
    ET.SubElement(body, "kit_card", {"name": "kc_quick", "id": "quick", "title": "Quick", "kind": "quick"})
    return root


def synonym_aliases(value: str, synonym_groups: list[list[str]]) -> str:
    """Expand matching film/SL vocabulary into a low-priority index field."""
    lowered = value.lower()
    aliases: list[str] = []
    for group in synonym_groups:
        if any(re.search(r"(?<![a-z0-9])" + re.escape(term.lower()) + r"(?![a-z0-9])", lowered)
               for term in group):
            for term in group:
                if term.lower() not in lowered and term not in aliases:
                    aliases.append(term)
    return " ".join(aliases)


EXTERNAL_CONTROL_TAGS = {
    "button", "check_box", "color_swatch", "combo_box", "filter_editor",
    "flyout_button", "line_editor", "radio_group", "scroll_list", "slider",
    "slider_bar", "spinner", "texture_picker"
}


# [Connectedness B6] External floater controls often carry no label of their own. The index used to
# fall back to the widget name ("ps reset BDMergeFroxelWindAzimuth"), which then showed in search
# results. Build a readable label from the widget name instead: drop the short layout prefix and the
# control-type suffix, split camelCase, and turn reset/default buttons into "Reset <thing>".
_LABEL_PREFIXES = {"cine", "rig", "ps", "ri", "tr", "ch", "sd", "pol", "exp", "fx", "lpm", "gradnd"}
_LABEL_SETTING_WORDS = {"bd", "merge", "alchemy", "render", "cine"}
_LABEL_SUFFIXES = {"slider", "spinner", "btn", "button", "check", "checkbox", "combo", "swatch", "picker",
                   "editor", "list", "bar", "field", "radio", "text", "label"}
_LABEL_ACRONYMS = {"ev", "fps", "ir", "hdr", "rgb", "gi", "ao", "ca", "ui", "lut", "fov", "nd", "dof", "bg",
                   "ots", "iso", "lod", "sdr", "rtgi", "vram", "z", "x", "y", "cas", "ssao", "pov", "eep", "tm",
                   "fx", "sl", "coc", "cof", "ssr", "ior", "amd"}
# Abbreviations and run-together words seen in widget names; the value replaces the token as-is
# (so acronyms keep their capitals).
_LABEL_WORD_MAP = {
    "sat": "saturation", "satr": "red saturation", "satg": "green saturation", "satb": "blue saturation",
    "hdrmax": "HDR max", "toneexp": "tone exposure", "exp": "exposure", "amt": "amount",
    "glarethr": "glare threshold", "glaredepth": "glare depth", "skysat": "sky saturation",
    "skydark": "sky darkening", "bandrad": "band radius", "shadowres": "shadow resolution",
    "shadowtype": "shadow type", "colorlut": "colour LUT", "anisotropicfilter": "anisotropic filter",
    "blurdist": "blur distance", "spotbias": "spot bias", "spotoffset": "spot offset",
    "localfog": "local fog", "projvol": "projector volumetric", "rimq": "rim quality", "rigrim": "rig rim",
    "res": "resolution", "thr": "threshold", "dist": "distance", "rad": "radius", "val": "value",
    "deg": "degrees", "inten": "intensity", "mult": "multiplier", "freq": "frequency", "temp": "temperature",
    "desat": "desaturation", "anim": "animation", "env": "environment", "pos": "position",
    "min": "min", "max": "max", "lo": "low", "hi": "high", "pow": "power", "var": "variation",
    "kal": "kaleidoscope", "ior": "IOR", "rot": "rotation", "tex": "texture", "src": "source",
    "cx": "centre X", "cy": "centre Y", "rx": "radius X", "ry": "radius Y", "db": "dB", "tron": "Tron",
    "n": "N", "sil": "silhouette", "am": "amount", "amp": "amplitude", "fstop": "f-stop", "focaltime": "focal time",
}


def _label_tokens(widget: str) -> tuple[list[str], bool]:
    parts = [part for part in widget.split("_") if part]
    if len(parts) > 1 and parts[0].lower() in _LABEL_PREFIXES:
        parts = parts[1:]
    words: list[str] = []
    for part in parts:
        pieces = re.findall(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+|\d+", part) or [part]
        if len(pieces) > 1:  # camelCase token: it is a setting-style name, drop its internal prefix words
            while len(pieces) > 1 and pieces[0].lower() in _LABEL_SETTING_WORDS:
                pieces = pieces[1:]
        words.extend(piece.lower() for piece in pieces)
    # "reset"/"default"/"rst" tokens anywhere (reset_X, X_default_btn, cas_reset_X) make it a Reset label.
    reset = any(word in ("reset", "default", "rst") for word in words)
    words = [word for word in words if word not in ("reset", "default", "rst")]
    while len(words) > 1 and words[-1] in _LABEL_SUFFIXES:
        words = words[:-1]
    while len(words) > 1 and words[0] in _LABEL_SETTING_WORDS:
        words = words[1:]
    # A control id often repeats its own subject (cas_reset_RenderCASSharpness): keep the last mention.
    words = [word for index, word in enumerate(words) if word not in words[index + 1:]]
    return words, reset


def humanize_widget_name(widget: str) -> str:
    words, reset = _label_tokens(widget)
    if not words:
        words = ["control"]
    # A trailing single-letter token is a unit (focus m, spin delay s): show it as "(m)" / "(s)".
    if len(words) > 1 and words[-1] in ("m", "s"):
        words[-1] = "(" + words[-1] + ")"
    # "S Log" is the S-Log curve name, split by the camelCase rule.
    for index in range(len(words) - 1):
        if words[index] == "s" and words[index + 1] == "log":
            words[index:index + 2] = ["S-Log"]
            break
    words = [_LABEL_WORD_MAP.get(word, word) for word in words]
    words = [word.upper() if word in _LABEL_ACRONYMS else word for word in words]
    text = " ".join(words)
    if reset:
        return "Reset " + text
    return text[:1].upper() + text[1:]


_LABEL_SIBLING_TAGS = {"text", "text_box", "label"}


def sibling_label(parent: ET.Element | None, node: ET.Element) -> str:
    """The floater's own label for a bare control: the nearest preceding text/label sibling.

    Floater XML pairs a control with a caption (``<text value="Sharpen (CAS):"/>`` then the slider)
    rather than a label= attribute. Scan back over siblings that belong to the same control (slider
    then spinner on one setting, or its reset button) and stop at any other control.
    """
    if parent is None:
        return ""
    siblings = list(parent)
    try:
        index = siblings.index(node)
    except ValueError:
        return ""
    stem = node.get("control_name", "")
    for previous in reversed(siblings[:index]):
        if not isinstance(previous.tag, str):
            continue
        if previous.tag in _LABEL_SIBLING_TAGS:
            caption = (previous.get("value") or previous.get("label") or previous.text or "").strip()
            caption = caption.rstrip(":").strip()
            if 1 < len(caption) <= 40 and not caption.startswith("$") and chr(10) not in caption:
                return caption
            return ""
        if previous.tag in EXTERNAL_CONTROL_TAGS and stem and previous.get("control_name") == stem:
            continue
        if previous.tag in ("view_border", "icon", "layout_panel"):
            continue
        return ""
    return ""


def tree_names(root: ET.Element, seen: set[str] | None = None) -> set[str]:
    """Every name= / widget= in a page tree, following filename= includes (B8: index rows must resolve)."""
    seen = set() if seen is None else seen
    names: set[str] = set()
    for node in root.iter():
        for attr in ("name", "widget"):
            if node.get(attr):
                names.add(node.get(attr))
        include = node.get("filename", "")
        if include and include not in seen and (XUI / include).exists():
            seen.add(include)
            names |= tree_names(parse_xml(XUI / include), seen)
    return names


def external_index_rows(spec: dict, synonym_groups: list[list[str]]) -> list[list[str]]:
    rows: list[list[str]] = []

    def visit(node: ET.Element, floater: dict, tab_name: str = "", tab_label: str = "",
              parent: ET.Element | None = None) -> None:
        name = node.get("name", "")
        setting = node.get("control_name", "")
        widget = name or setting
        if isinstance(node.tag, str) and widget and (node.tag in EXTERNAL_CONTROL_TAGS or setting):
            label = (node.get("label", "").strip() or (node.text or "").strip()
                     or sibling_label(parent, node) or humanize_widget_name(widget))
            help_text = node.get("tool_tip", "")
            direct = " ".join((label, name, setting, help_text, tab_label, floater["label"]))
            rows.append([
                "external", floater["name"], floater["label"], "External",
                tab_name, tab_name, tab_label, "", widget, label, str(node.tag), setting,
                node.get("decimal_digits", "3"), "e", str(node.tag), help_text,
                synonym_aliases(direct, synonym_groups), floater["name"], floater["label"],
                tab_name, tab_label, "external"
            ])
        filename = node.get("filename", "")
        if filename:
            included = parse_xml(XUI / filename)
            visit(included, floater, tab_name, tab_label, None)
        for child in node:
            child_tab_name = tab_name
            child_tab_label = tab_label
            if node.tag == "tab_container" and child.tag == "panel":
                child_tab_name = child.get("name", "")
                child_tab_label = child.get("label", "") or child_tab_name.replace("_", " ").title()
            visit(child, floater, child_tab_name, child_tab_label, node)

    for floater in spec.get("floaters", []):
        visit(parse_xml(XUI / floater["xui"]), floater)
    return rows


def index_bytes(data: dict, config: dict, classes: dict[str, str], note: str,
                generated_rows: dict[str, set[str]], page_names: dict[str, set[str]] | None = None) -> bytes:
    lines: list[str] = []
    synonym_groups = json.loads(SYNONYMS_DATA.read_text(encoding="utf-8")).get("groups", [])
    def esc(value) -> str:
        return str(value or "").replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")
    indexed_rows: dict[str, set[str]] = defaultdict(set)
    manifest_only_rows: dict[str, set[str]] = defaultdict(set)
    for group, raw_page in flatten_pages(data):
        key = f"{group['name']}/{raw_page['name']}"
        classification = classes[key]
        page = apply_config(raw_page, config.get("p0_pages", {}).get(key, {}))
        page = apply_config(page, config.get("layout_overrides", {}).get(key, {}))
        legacy = classification == "legacy"
        if classification == "launcher":
            page = copy.deepcopy(raw_page)
            page["cards"] = [{"id": "open", "t": page.get("label", ""), "rows": []}]
            page["sections"] = [{"id": "open", "cards": [0]}]
        section_for = {}
        for section in page.get("sections", []):
            for i in section.get("cards", []):
                section_for[int(i)] = section.get("id", "")
        for ci, card in enumerate(page.get("cards", [])):
            entries = [(None, row) for row in card.get("rows", [])]
            entries += [(oi, row) for oi, opt in enumerate(card.get("opts", [])) for row in opt.get("rows", [])]
            for oi, row in entries:
                flags = ("a" if row.get("a") else "") + ("l" if row.get("k") == "link" else "") + ("x" if legacy else "")
                fields = [group["name"], page["name"], page.get("label", ""), group.get("label", ""),
                          section_for.get(ci, ""), card.get("id", ""), card.get("t", ""),
                          "" if oi is None else oi, row.get("w", ""), row.get("l", ""), row.get("k", ""),
                          row.get("s", ""), str(row.get("dc") if row.get("dc") is not None
                                                   else (0 if row.get("k") in ("dial", "arc") else 3)),
                          flags, row.get("kw", ""), row.get("h", "")]
                # [Connectedness B8] Only index widgets that exist on their page: a row naming a widget the
                # page tree lacks opens the page, finds nothing and highlights nothing. The matching floater
                # rows are already indexed as external entries.
                if page_names is not None and row.get("w", "") not in page_names.get(key, set()):
                    continue
                direct = " ".join(str(value or "") for value in fields[8:16])
                fields += [synonym_aliases(direct, synonym_groups), "", "", "", "", "director"]
                if len(fields) != INDEX_FIELDS:
                    raise GenError("internal index field count error")
                lines.append("\t".join(esc(value) for value in fields))
                indexed_rows[key].add(str(row.get("w", "")))
                if row.get("show_if") == "never":
                    manifest_only_rows[key].add(str(row.get("w", "")))
    external_spec = json.loads(EXTERNAL_SEARCH_DATA.read_text(encoding="utf-8"))
    indexed_floaters = {item["name"] for item in external_spec.get("floaters", [])}
    launcher_floaters = set(re.findall(r'LLFloaterReg::showInstance\(\s*"([^"]+)"',
                                       DIRECTOR_CPP.read_text(encoding="utf-8")))
    launcher_floaters.update(target["target"] for target in config.get("launcher_targets", {}).values())
    missing_floaters = launcher_floaters - indexed_floaters
    if missing_floaters:
        raise GenError(f"Director launcher floaters missing from external search: {sorted(missing_floaters)}")
    for fields in external_index_rows(external_spec, synonym_groups):
        if len(fields) != INDEX_FIELDS:
            raise GenError("internal external index field count error")
        lines.append("\t".join(esc(value) for value in fields))
    for key, widgets in generated_rows.items():
        runtime_indexed = indexed_rows.get(key, set()) - manifest_only_rows.get(key, set())
        if classes[key] != "legacy" and runtime_indexed != widgets:
            raise GenError(f"search index/generated row drift on {key}: "
                           f"index-only={sorted(runtime_indexed - widgets)}, "
                           f"page-only={sorted(widgets - runtime_indexed)}")
    payload = "\n".join(lines) + "\n"
    return (f'<?xml version="1.0" encoding="utf-8"?>\n<!-- {note} -->\n'
            f'<director_kit_index version="3" fields="{INDEX_FIELDS}"><![CDATA[\n{payload}]]>'
            '</director_kit_index>\n').encode("utf-8")


def copy_chrome(name: str, v1: ET.Element, *, label: bool = True) -> ET.Element:
    node = find_named(v1, name)
    if node is None:
        raise GenError(f"v1 chrome widget {name!r} not found")
    return stripped_copy(node, strip_label=not label)


def floater_tree(data: dict, page_files: dict[str, str], v1: ET.Element) -> ET.Element:
    root = ET.Element("floater", {"name": "floater_director", "title": "Director", "width": "1160",
                      "height": "720", "min_width": "850", "min_height": "560", "can_resize": "true",
                      "save_rect": "true", "single_instance": "true", "legacy_header_height": "30",
                      "layout": "topleft", "bg_opaque_color": "KitWindowBg", "bg_alpha_color": "KitWindowBg"})
    shell = ET.SubElement(root, "kit_shell", {"name": "director_shell", "follows": "all", "left": "0",
                          "top": "30", "width": "1160", "height": "690", "index_file": "director_kit_index.xml"})
    for group in data.get("groups", []):
        if group.get("rail") is False:
            button = group["button"]
            ET.SubElement(shell, "button", {"name": button["name"], "label": "",
                          "image_overlay": button["icon"], "tool_tip": button["help"]})
            continue
        masters = sorted({p.get("master", {}).get("s", "") for p in group.get("pages", []) if p.get("master", {}).get("s")})
        ET.SubElement(shell, "kit_shell.rail_item", {"group": group["name"], "label": group["label"],
                      "icon": ICONS[group["icon"]], "master_settings": ",".join(masters)})
    roster = ET.SubElement(shell, "panel", {"name": "kit_cast_roster", "background_visible": "false"})
    header = ET.SubElement(roster, "text", {"name": "cast_roster_header", "font": "SansSerifSmall",
                                            "text_color": "KitMuted"})
    header.text = "CAST"
    for name in ("cast_list", "cast_hint", "group_assign_combo", "btn_add_you", "btn_add_animesh", "btn_focus", "btn_remove"):
        node = copy_chrome(name, v1)
        if name == "cast_list":
            # Intentional: agent cargo exposes ordered multi-row DAD_PERSON drops,
            # which Subject A consumes to fill A-D. A plain scroll_list cannot.
            node.tag = "radar_list"
            node.set("content_type", "Agents")
            # The name column takes the roster's spare width (the rail roster is 176 px and the
            # popover 280 px). The v1 fixed 72 px clipped names to about 11 characters.
            for column in node.iter("columns"):
                if column.get("name") == "name":
                    column.attrib.pop("width", None)
                    column.set("dynamic_width", "true")
        if name == "cast_hint":
            node.set("clip", "true")
        tool_icons = {"btn_add_you": "KitIcon_cast_tool_you_16", "btn_add_animesh": "KitIcon_cast_tool_animesh_16",
                      "btn_focus": "KitIcon_cast_tool_focus_16", "btn_remove": "KitIcon_cast_tool_remove_16"}
        if name in tool_icons:
            node.set("label", "")
            node.set("image_overlay", tool_icons[name])
        roster.append(node)
    tabs = ET.SubElement(shell, "tab_container", {"name": "director_tabs", "hide_tabs": "true",
                         "tab_position": "left", "follows": "all"})
    for group in data.get("groups", []):
        gp = ET.SubElement(tabs, "panel", {"name": group["name"], "label": group["label"]})
        tc = ET.SubElement(gp, "tab_container", {"name": group["name"].removesuffix("_group") + "_tabs",
                           "hide_tabs": "true", "tab_position": "top", "follows": "all"})
        for page in group.get("pages", []):
            attrs = {"name": page["name"], "label": page["label"],
                     "page_file": page_files[f"{group['name']}/{page['name']}"]}
            master = page.get("master") or {}
            if master.get("s"):
                attrs["master_setting"] = str(master["s"])
            ET.SubElement(tc, "kit_lazy_page", attrs)
    bar = ET.SubElement(shell, "kit_bar", {"name": "kit_transport", "flex": "transport_spacer",
                        "collapse_order": "transport_toast,btn_reset_marks,btn_set_marks,show_paths_global_check,"
                                          "btn_help,btn_scene_load,status_strip,btn_scene_save,btn_scene_menu"})
    widths = {"btn_action": 92, "btn_cut": 92, "btn_arm": 22, "btn_take_play": 28, "btn_take_stop": 28,
              "take_timecode": 100, "scene_combo": 130, "scene_name_editor": 130, "btn_scene_load": 44,
              "btn_scene_save": 50, "btn_scene_menu": 28, "btn_set_marks": 28, "btn_reset_marks": 28,
              "show_paths_global_check": 28, "btn_help": 28}
    action_group = ET.SubElement(bar, "panel", {"name": "transport_action_group", "width": "114", "height": "26",
                                                 "background_visible": "false"})
    for name in ("btn_action", "btn_cut"):
        node = copy_chrome(name, v1)
        node.set("image_color", "KitAccent" if name == "btn_action" else "KitRecord")
        node.set("label_color", "Black" if name == "btn_action" else "White")
        node.set("image_overlay", "KitIcon_transport_action_play_16" if name == "btn_action" else
                 "KitIcon_transport_action_stop_16")
        node.set("width", str(widths[name])); node.set("height", "26")
        node.set("left", "0"); node.set("top", "0"); action_group.append(node)
    ET.SubElement(action_group, "button", {"name": "btn_arm", "label": "", "image_overlay": "KitIcon_armed_chevron_up_16",
                                            "left": "92", "top": "0", "width": "22", "height": "26",
                                            "tool_tip": "Choose what ACTION starts"})
    for name, image, tip in (("btn_take_play", "KitIcon_transport_play_16", "Play or pause the current camera take"),
                             ("btn_take_stop", "KitIcon_transport_stop_16", "Stop the current camera take")):
        ET.SubElement(bar, "button", {"name": name, "label": "", "image_overlay": image,
                                      "width": str(widths[name]), "height": "26", "tool_tip": tip})
    timecode = ET.SubElement(bar, "text", {"name": "take_timecode", "width": str(widths["take_timecode"]),
                                           "height": "26", "halign": "center", "text_color": "KitTextStrong",
                                           "tool_tip": "Camera-take timecode"})
    timecode.text = "00:00:00:00"
    ET.SubElement(bar, "panel", {"name": "transport_divider_timecode", "width": "1", "height": "18",
                                  "background_visible": "false"})
    for name in ("scene_combo", "scene_name_editor"):
        node = copy_chrome(name, v1); node.set("width", str(widths[name])); node.set("height", "24"); bar.append(node)
    dirty = ET.SubElement(bar, "text", {"name": "scene_dirty_indicator", "width": "10", "height": "28",
                                         "halign": "center", "text_color": "KitAccent",
                                         "tool_tip": "Unsaved Director scene changes"})
    dirty.text = ""
    ET.SubElement(bar, "button", {"name": "btn_scene_load", "label": "Load",
                                   "tool_tip": "Load the selected scene", "width": str(widths["btn_scene_load"]),
                                   "height": "24"})
    save = copy_chrome("btn_scene_save", v1); save.set("width", str(widths["btn_scene_save"])); save.set("height", "24"); bar.append(save)
    ET.SubElement(bar, "button", {"name": "btn_scene_menu", "label": "...", "width": str(widths["btn_scene_menu"]),
                                   "height": "24", "tool_tip": "Save as, Rename, or Delete the selected scene"})
    ET.SubElement(bar, "panel", {"name": "transport_divider_scene", "width": "1", "height": "18",
                                  "background_visible": "false"})
    for name in ("btn_set_marks", "btn_reset_marks"):
        node = copy_chrome(name, v1); node.set("width", str(widths[name])); node.set("height", "26"); bar.append(node)
        if name in {"btn_set_marks", "btn_reset_marks"}:
            node.set("label", "")
            node.set("image_overlay", "KitIcon_transport_marks_set_16" if name == "btn_set_marks" else
                     "KitIcon_transport_marks_reset_16")
    rec = copy_chrome("rec_indicator", v1)
    rec.set("width", "48"); rec.set("height", "20"); rec.set("halign", "right"); rec.set("h_pad", "7")
    rec.set("text_color", "KitFaint")
    rec.text = "REC"
    bar.append(rec)
    show = copy_chrome("show_paths_global_check", v1); show.tag = "button"; show.set("label", ""); show.set("is_toggle", "true")
    show.set("image_overlay", "KitIcon_transport_paths_16"); show.set("width", "28"); show.set("height", "26"); bar.append(show)
    help_button = copy_chrome("btn_help", v1); help_button.set("label", ""); help_button.set("image_overlay", "KitIcon_transport_legend_16"); help_button.set("width", "28"); help_button.set("height", "26"); bar.append(help_button)
    ET.SubElement(bar, "text", {"name": "status_strip", "use_ellipses": "true",
                                "width": "190", "height": "24", "h_pad": "14", "text_color": "KitRecordText",
                                "visible": "false"})
    ET.SubElement(bar, "panel", {"name": "transport_spacer", "width": "1", "height": "1",
                                  "background_visible": "false"})
    ET.SubElement(bar, "text", {"name": "transport_toast", "width": "180", "height": "22", "halign": "center",
                                "use_ellipses": "true", "visible": "false"})
    ET.SubElement(bar, "button", {"name": "btn_cast_chip", "label": "Cast 0  ^", "width": "82", "height": "24",
                                   "tool_tip": "Open the cast tools and member list"})
    arm = ET.SubElement(shell, "kit_popover", {"name": "kit_arm_popover", "anchor": "btn_arm", "side": "above", "visible": "false"})
    for name in ("arm_moves_check", "arm_camera_check", "arm_play_check", "arm_capture_check", "delay_spinner"):
        src = find_named(v1, name)
        if src is None:
            raise GenError(f"arm widget {name} missing")
        kind = "toggle" if src.tag == "check_box" else "number"
        child = stripped_copy(src); child.tag = "kit_switch" if src.tag == "check_box" else src.tag
        row = ET.SubElement(arm, "kit_row", {"kind": kind, "widget": name,
                            "label": src.get("label", ""), "setting": src.get("control_name", ""),
                            "bind": "setting" if src.get("control_name") else "code"})
        row.append(child)
    legend = ET.SubElement(shell, "kit_popover", {"name": "legend_panel", "anchor": "btn_help", "side": "above", "visible": "false"})
    legend_body = copy_chrome("legend_body", v1)
    if legend_body.text:
        legend_body.text = legend_body.text.replace("see Capture > Scene options", "use F1 or ?, or choose Hotkeys below")
        # [Connectedness B7] The per-actor Look-at-camera flag and its master toggle live on
        # Performance > Gaze, not Camera > Shot (the A/B subjects are the Camera > Shot part).
        if "override (Camera > Shot)" not in legend_body.text:
            raise GenError("legend L-column text changed: update the B7 replacement")
        legend_body.text = legend_body.text.replace("override (Camera > Shot)", "override (Performance > Gaze)")
    legend.append(legend_body)
    ET.SubElement(legend, "button", {"name": "btn_legend_hotkeys", "label": "Hotkeys (F1 / ?)"})
    legend.append(copy_chrome("btn_legend_close", v1))
    scene_menu = ET.SubElement(shell, "kit_popover", {"name": "kit_scene_popover", "anchor": "btn_scene_menu", "side": "above", "visible": "false"})
    for name, label in (("btn_scene_save_as", "Save as..."), ("btn_scene_rename", "Rename..."),
                        ("btn_scene_delete", "Delete...")):
        ET.SubElement(scene_menu, "button", {"name": name, "label": label})
    hot = ET.SubElement(shell, "kit_popover", {"name": "kit_hotkeys_popover", "anchor": "btn_help", "side": "above", "visible": "false"})
    # [Connectedness B7] Every unmodified key aldirectorhotkeys.cpp handles (F2-F8, 1-9, Alt+1-9)
    # plus the in-console keys handled by LLFloaterDirector::handleKeyHere.
    ET.SubElement(hot, "text", {"name": "kit_hotkeys_text", "word_wrap": "true"}).text = (
        "F2: show / hide the Director console\nF3: ACTION (becomes CUT while running)\nF4: CUT\n"
        "F5: reset to marks\nF6: pose ghosts on / off\nF7: take play / pause\nF8: set marks\n"
        "Enter: ACTION\nEsc: close, clear or CUT\n/: search\nCtrl+Z/Y: undo / redo\n"
        "PgUp/PgDn: chip\nShift+PgUp/PgDn: page\nAlt+PgUp/PgDn: group\nArrows: adjust\n"
        "Home: reset row\nSpace/Enter: activate\n1-9: camera slots (when armed)\nAlt+1-9: animation slots (when armed)")
    return root


def theme_choices(themes_dir: Path = THEMES_DIR) -> tuple[list[str], list[str]]:
    """(labels, ids) of the colour themes, Default first then by file name (ALDirectorTheme::listThemes order).

    Every theme file must pass director_theme_check, so a bad file stops generation.
    """
    skin = theme_check.load_skin_colors()
    entries = []
    for path in sorted(themes_dir.glob("*.xml")):
        errors = theme_check.check_theme_file(str(path), skin)
        if errors:
            raise GenError(f"theme {path.name} fails director_theme_check: {errors[0]}")
        root = ET.fromstring(path.read_text(encoding="utf-8"))
        label = theme_check._llsd_map(root[0], [], path.name)["label"]
        entries.append((path.stem, label))
    if not any(stem == "default" for stem, _label in entries):
        raise GenError("app_settings/director_themes/default.xml is required")
    entries.sort(key=lambda entry: (entry[0] != "default", entry[0]))
    return [label for _stem, label in entries], [stem for stem, _label in entries]


def fill_theme_rows(data: dict) -> None:
    """Rows with theme_list take their combo items from the theme files.

    These generated items are the FALLBACK and the default order only: at runtime
    LLFloaterDirector::bindDirectorSettingsPage repopulates the combo from
    ALDirectorTheme::listThemes(), so a theme file added after generation still appears.
    """
    labels, ids = theme_choices()
    for _group, page in flatten_pages(data):
        for row in all_rows(page):
            if row.get("theme_list"):
                row["it"] = labels
                row["iv"] = ids


def generate(data_path: Path, config_path: Path, out_dir: Path) -> tuple[dict[Path, bytes], dict[str, str]]:
    data = json.loads(data_path.read_text(encoding="utf-8"))
    if VCAM_DATA.exists():
        vcam = json.loads(VCAM_DATA.read_text(encoding="utf-8"))
        validate_vcam_page(vcam)
        for group in data.get("groups", []):
            if group.get("name") == "camera_group":
                group["pages"] = [vcam if page.get("name") == "vcam_tab" else page
                                  for page in group.get("pages", [])]
                break
    config = json.loads(config_path.read_text(encoding="utf-8"))
    validate_data(data)
    normalize_shortcuts(data)
    fill_theme_rows(data)
    if config.get("version") != 1:
        raise GenError("kit_pages.json version must be 1")
    v1 = parse_xml(V1_FLOATER)
    leaves = {node.get("name", ""): node for node in v1.iter("panel") if node.get("name")}
    classes = {f"{g['name']}/{p['name']}": classify(g["name"], p, config, leaves)
               for g, p in flatten_pages(data)}
    expected = {"light_group/cine_light_rig_tab": "kit (P0)", "light_group/light_rim_tab": "kit (P0)",
                "light_group/actors_tab": "kit (P0)",
                "camera_group/vcam_tab": "kit (P0)", "look_group/stylize_tab": "kit (P0)",
                "look_group/tron_tab": "kit (P0)", "look_group/image_tab": "kit (P0)",
                "look_group/actor_style_tab": "kit (P0)",
                "lens_group/focus_tab": "kit (P0)", "lens_group/filters_tab": "kit (P0)",
                "lens_group/diopter_tab": "kit (P0)", "world_group/weather_tab": "kit (P0)",
                "world_group/atmosphere_tab": "kit (P0)", "world_group/shadows_probes_tab": "kit (P0)",
                "look_group/bloom_glow_tab": "pure kit (new)",
                "look_group/stepped_tab": "kit (P0)",
                "camera_group/shot_tab": "kit (P0)", "camera_group/moves_tab": "kit (P0)",
                "camera_group/operator_tab": "kit (P0)", "camera_group/frame_orbit_tab": "kit (P0)",
                "camera_group/takes_tab": "kit (P0)", "capture_group/temporal_tab": "kit (P0)",
                "capture_group/scene_options_tab": "kit (P0)",
                "settings_group/director_settings_tab": "kit (P0)"}
    # Pinned per conversion batch (user-approved 10/13: lower as pages convert, never raise).
    # World/Lens batch: kit 7 -> 13, legacy 27 -> 21. 10/14: new Stepped page (kit_pages
    # entry => kit (P0)) kit 13 -> 14; legacy unchanged at 21. 10/14 Camera+Capture
    # batch: kit 14 -> 21, legacy 21 -> 14.
    # 10/15 Light Actors: kit 21 -> 22, legacy 14 -> 13
    # 10/15 Director Settings: kit 22 -> 23, legacy unchanged at 13.
    if any(classes.get(key) != value for key, value in expected.items()) or Counter(classes.values()) != Counter({"kit (P0)": 23, "pure kit (new)": 1, "legacy": 13}):
        raise GenError(f"classification table drift: {Counter(classes.values())}, special={[(k, classes.get(k)) for k in expected]}")
    note = header(data_path, config_path)
    referenced = cpp_names(config_path)
    outputs: dict[Path, bytes] = {}
    page_files: dict[str, str] = {}
    generated_rows: dict[str, set[str]] = {}
    page_names: dict[str, set[str]] = {}
    report_lines = [note, "", "CLASSIFICATION"]
    for group, raw_page in flatten_pages(data):
        key = f"{group['name']}/{raw_page['name']}"
        classification = classes[key]
        details: list[str] = []
        if classification == "legacy":
            root, names = legacy_page(group, raw_page, leaves[raw_page["name"]], config.get("page_features", {}))
            hosted = len(list(leaves[raw_page["name"]]))
            kit_rows = 0
        elif classification in {"kit (P0)", "pure kit (new)"}:
            cfg = config.get("p0_pages", {}).get(key, {})
            sources = [XUI / name for name in cfg.get("v1_sources", [])]
            root, names = kit_page(group, raw_page, cfg, sources, referenced, details)
            apply_page_features(root, key, config.get("page_features", {}))
            hosted = sum(1 for node in root.iter() if node.tag in HOST_KIND or node.tag == "kit_switch") if sources else 0
            kit_rows = sum(1 for node in root.iter("kit_row"))
        else:
            root, names = launcher_page(group, raw_page, config["launcher_targets"][key])
            hosted, kit_rows = 0, 0
        apply_kit_xui_style(root)
        file_name = page_filename(group["name"], raw_page["name"])
        page_files[key] = file_name
        outputs[out_dir / file_name] = xml_bytes(root, note)
        generated_rows[key] = {str(node.get("widget")) for node in root.iter("kit_row") if node.get("widget")}
        page_names[key] = tree_names(root)
        data_rows = sum(1 for _ in all_rows(raw_page))
        pure_kit = all(is_pure_row(row) for row in all_rows(raw_page))
        predicate = f"pure-kit-eligible:{str(pure_kit).lower()}"
        report_lines.append(f"{key}: {classification}; predicate={predicate}; rows={data_rows}; kit_rows={kit_rows}; "
                            f"hosted_v1_nodes={hosted}; upgrades={sum(x.startswith('UPGRADE') for x in details)}; "
                            f"warnings={sum(x.startswith('WARNING') for x in details)}")
        report_lines.extend("  " + line for line in details)
        # Legacy controls are serialized XML names, not runtime-created kit
        # names. Re-listing them here would let parity's supplemental index
        # mask a removed legacy widget (review fixture P5).
        runtime_names = names if classification != "legacy" else set()
        names_text = ", ".join(sorted(runtime_names))
        report_lines.append(f"KITNAMES {raw_page['name']}:" + (" " + names_text if names_text else ""))
    quick = quick_page()
    apply_kit_xui_style(quick)
    page_files["quick_group/quick_tab"] = "panel_dpage_quick.xml"
    outputs[out_dir / "panel_dpage_quick.xml"] = xml_bytes(quick, note)
    floater_data = copy.deepcopy(data)
    floater_data["groups"].insert(0, {"name": "quick_group", "label": "Quick", "icon": "camera",
                                      "pages": [{"name": "quick_tab", "label": "Quick"}]})
    floater = floater_tree(floater_data, page_files, v1)
    apply_kit_xui_style(floater)
    outputs[out_dir / "floater_director_v2.xml"] = xml_bytes(floater, note)
    outputs[out_dir / "director_kit_index.xml"] = index_bytes(data, config, classes, note, generated_rows, page_names)
    report_lines += ["", "ICON_TEXTURES KitIcon_*_16 Kit_RoundRect4 Kit_RoundRect6 Kit_Pill9 Kit_Circle Kit_SliderTrack Kit_SliderThumb",
                     "TRANSPORT take_timecode uses LLFlycamRecorder playhead at 2 Hz; generated visible",
                     "CONTROL_NAME cine_rimq_actors_alpha: data widget spelling differs; v1 setting CineRigRimActorsOnlyAlpha is authoritative"]
    report_path = REPORT if out_dir.resolve() == DEFAULT_OUT.resolve() else out_dir / "gen_report.txt"
    outputs[report_path] = ("\n".join(report_lines) + "\n").encode("utf-8")
    return outputs, classes


def selftest(data_path: Path, config_path: Path, out_dir: Path) -> int:
    try:
        outputs, classes = generate(data_path, config_path, out_dir)
        checks = []
        checks.append(("classification table", Counter(classes.values()) == Counter({"kit (P0)": 23, "pure kit (new)": 1, "legacy": 13})
                       and classes["light_group/cine_light_rig_tab"] == "kit (P0)"
                       and classes["light_group/light_rim_tab"] == "kit (P0)"
                       and classes["camera_group/vcam_tab"] == "kit (P0)"
                       and classes["look_group/bloom_glow_tab"] == "pure kit (new)"
                       and classes["look_group/stepped_tab"] == "kit (P0)"
                       and classes["look_group/stylize_tab"] == "kit (P0)"
                       and classes["look_group/tron_tab"] == "kit (P0)"
                       and classes["look_group/image_tab"] == "kit (P0)"
                       and classes["look_group/actor_style_tab"] == "kit (P0)"))
        expected = {"slider": "slider", "number": "spinner", "toggle": "kit_switch", "combo": "combo_box",
                    "color": "color_swatch", "readout": "text", "button": "button", "text": "line_editor",
                    "list": "scroll_list", "dial": "compass_dial", "arc": "compass_dial",
                    "segmented": "radio_group"}
        hosted = {"slider": "slider", "spinner": "number", "check_box": "toggle",
                  "combo_box": "combo", "line_editor": "text", "text": "readout",
                  "button": "button", "color_swatch": "color", "icon": "gobo",
                  "scroll_list": "list", "view_border": "viewport"}
        custom = {"check": "toggle", "slider": "slider", "spinner": "number",
                  "combo": "combo", "button": "button", "line_editor": "text", "list": "list"}
        checks.append(("kind mapping", all(NATIVE_TAG.get(k) == v for k, v in expected.items())
                       and HOST_KIND == hosted and CUSTOM_KIND == custom))
        v1 = parse_xml(V1_FLOATER)
        config = json.loads(config_path.read_text(encoding="utf-8"))
        legacy_ok = True
        legacy_count = 0
        for group, page in flatten_pages(json.loads(data_path.read_text(encoding="utf-8"))):
            key = f"{group['name']}/{page['name']}"
            if classes[key] != "legacy":
                continue
            legacy_count += 1
            generated = ET.fromstring(outputs[out_dir / page_filename(group["name"], page["name"])], parser=parser())
            expected_legacy = copy.deepcopy(find_named(v1, page["name"]))
            # Same order as generate(): page features first, then the kit style stamp.
            apply_page_features(expected_legacy, key, config.get("page_features", {}))
            apply_kit_xui_style(expected_legacy)
            legacy_ok = legacy_ok and generated.tag == "kit_page" and children_canonical(generated) == children_canonical(expected_legacy)
        checks.append(("13 serialized legacy pages copy v1 children", legacy_ok and legacy_count == 13))
        dials = [node for path, payload in outputs.items() if path.name.startswith("panel_dpage_")
                 for node in ET.fromstring(payload, parser=parser()).iter("compass_dial")]
        checks.append(("all compass dials use kit geometry without duplicate readout",
                       bool(dials) and all(node.get("kit_geometry") == "true" and node.get("show_readout") == "false" for node in dials)))
        rig = ET.fromstring(outputs[out_dir / "panel_dpage_light_cine_light_rig.xml"], parser=parser())
        rig_ev_rows = [node for node in rig.iter("kit_row") if node.get("widget") in {
            "cine_key_ev", "cine_fill_ev", "cine_rim_ev", "cine_bg_ev", "cine_catchlight_ev"}]
        checks.append(("Rig EV rows retain setting-bound numeric spinners",
                       len(rig_ev_rows) == 5 and all(
                           row.get("kind") == "number" and row.get("bind") == "setting" and
                           len(list(row)) == 1 and list(row)[0].tag == "spinner" and
                           list(row)[0].get("control_name") == row.get("setting")
                           for row in rig_ev_rows)))
        tron = ET.fromstring(outputs[out_dir / "panel_dpage_look_tron.xml"], parser=parser())
        launcher_cards = [node for node in tron.iter("kit_card") if node.get("kind") == "launcher"]
        checks.append(("look Tron page is a generated kit page (no launcher)",
                       not launcher_cards and len(list(tron.iter("kit_row"))) > 100
                       and any(node.get("page") == "look_group/tron_tab" for node in tron.iter("kit_page"))))
        checks.append(("38 page files including runtime Quick host", sum(path.name.startswith("panel_dpage_") for path in outputs) == 38))
        synonyms = json.loads(SYNONYMS_DATA.read_text(encoding="utf-8")).get("groups", [])
        exposure_aliases = synonym_aliases("Exposure", synonyms).split()
        checks.append(("synonym expansion is bidirectional and index-ready",
                       "ev" in exposure_aliases and "brightness" in exposure_aliases and
                       "exposure" in synonym_aliases("EV", synonyms).split()))
        externals = external_index_rows(
            json.loads(EXTERNAL_SEARCH_DATA.read_text(encoding="utf-8")), synonyms)
        checks.append(("external controls carry lower-rank metadata",
                       bool(externals) and all(row[13] == "e" and row[17] and row[18]
                                               for row in externals)))
        for label, ok in checks:
            print(("ok: " if ok else "FAIL: ") + label)
        return 0 if all(ok for _, ok in checks) else 1
    except (GenError, OSError, ValueError, KeyError) as exc:
        print(f"selftest error: {exc}", file=sys.stderr)
        return 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, default=DEFAULT_DATA)
    ap.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest(args.data, args.config, args.out_dir)
    try:
        outputs, _ = generate(args.data, args.config, args.out_dir)
    except (GenError, OSError, ValueError, KeyError) as exc:
        print(f"director_kit_gen: {exc}", file=sys.stderr)
        return 2
    stale: list[Path] = []
    for path, content in outputs.items():
        if not path.is_file() or path.read_bytes() != content:
            stale.append(path)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
    if args.check:
        for path in stale:
            print(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)
        return 1 if stale else 0
    for path in outputs:
        print(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
