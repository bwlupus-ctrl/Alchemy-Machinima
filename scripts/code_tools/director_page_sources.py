#!/usr/bin/env python3
"""Build the Director Kit page/source evidence index.

The index is intentionally derived only from the approved mockup data and the
current XUI, C++, and settings trees.  It does not import rollout prose.
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "scripts/code_tools/director_kit/director_mockup_data.json"
XUI_DIR = ROOT / "indra/newview/skins/default/xui/en"
CPP_DIRS = (ROOT / "indra/newview", ROOT / "indra/llui")
SETTINGS_DIR = ROOT / "indra/newview/app_settings"
SETTINGS_FILES = SETTINGS_DIR / "settings_files.xml"
JSON_OUT = ROOT / "doc/director_kit/page_sources.json"
MD_OUT = ROOT / "doc/director_kit/page_sources.md"
PAGES_DIR = ROOT / "doc/director_kit/pages"

NAME_RE = re.compile(r"\bname\s*=\s*(['\"])(.*?)\1")
STRING_RE = re.compile(r'"((?:\\.|[^"\\])*)"')
METHOD_RE = re.compile(
    r"^\s*(?P<prefix>(?:(?:[A-Za-z_]\w*|[:<>,~*&]+)\s+)*)"
    r"(?P<class>[A-Za-z_]\w*)::(?P<function>~?[A-Za-z_]\w*)\s*\("
)

ADAPTERS = {
    "move_tab": "ALActorMoverKitAdapter", "path_tab": "ALPathKitAdapter",
    "props_tab": "ALPropMoverKitAdapter", "ghosts_tab": "ALGhostStudioKitAdapter",
    "dance_tab": "ALDanceBallKitAdapter", "animate_tab": "ALAnimateKitAdapter",
    "switchboard_tab": "ALAnimSwitcherKitAdapter", "gaze_tab": "ALGazeKitAdapter",
    "polish_tab": "ALPosePolishKitAdapter", "gaze_cues_tab": "ALGazeCueKitAdapter",
    "shot_tab": "ALShotKitAdapter", "moves_tab": "ALCameraMoveKitAdapter",
    "operator_tab": "ALCameraOperatorKitAdapter", "frame_orbit_tab": "ALFrameOrbitKitAdapter",
    "takes_tab": "ALTakesKitAdapter", "vcam_tab": "ALVCamKitAdapter",
    "focus_tab": "ALFocusKitAdapter", "filters_tab": "ALLensFilterKitAdapter",
    "diopter_tab": "ALDiopterKitAdapter", "cine_light_rig_tab": "ALCineRigKitAdapter",
    "light_rim_tab": "ALRigRimKitAdapter", "exposure_tab": "ALExposureKitAdapter",
    "practicals_tab": "ALPracticalsKitAdapter", "actors_tab": "ALActorLightKitAdapter",
    "light_cues_tab": "ALLightCueKitAdapter", "image_tab": "ALImageLookKitAdapter",
    "bloom_glow_tab": "ALBloomGlowKitAdapter", "stylize_tab": "ALRotoKitAdapter",
    "tron_tab": "ALTronKitAdapter", "actor_style_tab": "ALActorStyleKitAdapter",
    "weather_tab": "ALWeatherKitAdapter", "atmosphere_tab": "ALAtmosphereKitAdapter",
    "shadows_probes_tab": "ALShadowProbeKitAdapter", "temporal_tab": "ALTemporalCaptureKitAdapter",
    "capture_settings_tab": "ALCaptureSettingsKitAdapter", "scene_options_tab": "ALSceneOptionsKitAdapter",
}

R2_FIRST = [
    "path_tab", "ghosts_tab", "dance_tab", "animate_tab", "switchboard_tab", "gaze_tab",
    "gaze_cues_tab", "shot_tab", "takes_tab", "diopter_tab", "exposure_tab", "practicals_tab",
    "actors_tab", "light_cues_tab", "image_tab", "stylize_tab", "tron_tab", "actor_style_tab",
    "atmosphere_tab", "capture_settings_tab", "frame_orbit_tab", "operator_tab", "move_tab",
]

SPECIAL_OWNERS = {
    "gaze_cues_tab": [
        "`floater_gaze_cues` in `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:2`; "
        "`ALFloaterGazeCues` in `indra/newview/alfloatergazecues.cpp:101-663`"
    ],
    "light_cues_tab": [
        "`floater_cine_light_cues` in `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:2`; "
        "`ALFloaterCineLightCues` in `indra/newview/alfloatercinelightcues.cpp:51-403`"
    ],
    "diopter_tab": [
        "`ultimate_diopter` in `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:8`; "
        "`ALFloaterUltimateDiopter` in `indra/newview/alfloaterultimatediopter.cpp:266-1565`"
    ],
    "capture_settings_tab": [
        "Snapshot owns capture/output behavior: `LLFloaterSnapshot`/`LLFloaterSnapshotBase` in "
        "`indra/newview/llfloatersnapshot.cpp:79-489` and the `panel_snapshot_*.xml` panels; "
        "the approved Director page currently has zero rows"
    ],
}

SPECIAL_OWNER_RECORDS = {
    "gaze_cues_tab": [{"file": "indra/newview/skins/default/xui/en/floater_gaze_cues.xml", "tag": "floater", "name": "floater_gaze_cues", "line": 2, "class": "ALFloaterGazeCues", "class_file": "indra/newview/alfloatergazecues.cpp", "class_line": 101}],
    "light_cues_tab": [{"file": "indra/newview/skins/default/xui/en/floater_cine_light_cues.xml", "tag": "floater", "name": "floater_cine_light_cues", "line": 2, "class": "ALFloaterCineLightCues", "class_file": "indra/newview/alfloatercinelightcues.cpp", "class_line": 51}],
    "diopter_tab": [{"file": "indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml", "tag": "floater", "name": "ultimate_diopter", "line": 8, "class": "ALFloaterUltimateDiopter", "class_file": "indra/newview/alfloaterultimatediopter.cpp", "class_line": 266}],
    "capture_settings_tab": [{"file": "indra/newview/skins/default/xui/en/floater_snapshot.xml", "tag": "floater", "name": "Snapshot", "line": 9, "class": "LLFloaterSnapshot", "class_file": "indra/newview/llfloatersnapshot.cpp", "class_line": 79}],
}

PRESENTATION_NOTES = {
    "path_tab": "Use the existing waypoint `scroll_list`, buttons, combos, and spinners as kit rows. There is no path-canvas class to embed.",
    "switchboard_tab": "Keep the twelve existing `anim_slot_01`…`anim_slot_12` buttons as measured kit button rows/cells; do not create a slot-grid widget.",
    "shot_tab": "Keep the existing switcher buttons and `panel_cinecam_params.xml` controls; no slot-grid or curve editor is authorized.",
    "takes_tab": "Use the recorder's existing list, buttons, combos, sliders, and scrubber; do not create a timeline/curve editor.",
    "exposure_tab": "Use `panel_exposure_hub.xml` controls as kit rows; no curve editor exists.",
    "gaze_cues_tab": "Extract the existing cue list/editor stock controls into a shared owner-token view only if inline editing is required. Embed size: full card width, 240 px preferred, 160 px minimum, 320 px maximum, no nested page scroller.",
    "light_cues_tab": "Extract the existing cue list/editor stock controls into a shared owner-token view only if inline editing is required. Embed size: full card width, 240 px preferred, 160 px minimum, 320 px maximum, no nested page scroller.",
    "diopter_tab": "The six approved rows are launcher/summary rows, while the real editor is a 560×720 nine-tab floater. Do not invent a compact editor or embed the whole floater; data expansion is a USER DECISION.",
    "capture_settings_tab": "The approved page has zero rows. Populate it from the Snapshot owner before conversion; control selection and scope are a USER DECISION.",
    "cine_light_rig_tab": "Reuse the existing gobo control and light-map view under their existing contracts; no replacement editor is introduced here.",
    "ghosts_tab": "Reuse the existing preview/diagnostics controls with a 240 px maximum and visible-only, revision-driven refresh.",
}

ACTOR_PAGES = {"move_tab", "path_tab", "ghosts_tab", "animate_tab", "gaze_tab", "polish_tab", "gaze_cues_tab", "actors_tab", "actor_style_tab"}
LIST_PAGES = {"path_tab", "props_tab", "ghosts_tab", "dance_tab", "animate_tab", "switchboard_tab", "gaze_tab", "gaze_cues_tab", "shot_tab", "takes_tab", "light_cues_tab", "atmosphere_tab", "vcam_tab"}
PERMISSION_PAGES = {"props_tab", "ghosts_tab", "practicals_tab", "actors_tab", "atmosphere_tab", "vcam_tab", "capture_settings_tab"}

SPECIAL_EXTRACTIONS = {
    "gaze_cues_tab": [
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshCueList", 235),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshCastCombo", 274),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshEditorVisibility", 303),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "loadEditor", 320),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "readEditor", 342),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onCueSelected", 395),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onAdd/onEdit/onDelete", 409),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onSetPoint/onPickObject/onClearObject", 460),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onQuickMacro", 486),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshInfluenceList", 545),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "loadInfluenceEditor/readInfluenceEditor", 584),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onInfluenceAdd/onInfluenceUpdate/onInfluenceDelete", 634),
    ],
    "light_cues_tab": [
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshListNames", 126),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshCueRows", 155),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshStatus", 203),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "loadEditor/readEditor", 241),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onListLoad/onListSave/onListDelete", 265),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onCueSelected/onCapture/onUpdate/onDeleteCue", 294),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onTimecodeMode", 353),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onGo/onBack/onGoto/onRelease", 362),
    ],
    "diopter_tab": [
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "populateState/buildSigKey", 552),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "applyRowState", 697),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "updateBandPanels", 807),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "refreshRelevance", 844),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "armFocusPick/onFocusPickCommitted", 996),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onSetFromCameraFocusClicked", 1141),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "initRangeCombos/syncRangesToCurrentValues", 1265),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onRangeComboCommit", 1360),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onShowDepthToggled", 1485),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onFilterChanged", 1531),
    ],
    "capture_settings_tab": [
        ("indra/newview/llfloatersnapshot.cpp", "LLFloaterSnapshot::Impl", "getActivePanel/getImageFormat", 79),
        ("indra/newview/llfloatersnapshot.cpp", "LLFloaterSnapshot::Impl", "getWidthSpinner/getHeightSpinner", 116),
        ("indra/newview/llfloatersnapshot.cpp", "LLFloaterSnapshotBase::ImplBase", "updateLayout", 180),
        ("indra/newview/llfloatersnapshot.cpp", "LLFloaterSnapshot::Impl", "updateControls", 292),
        ("indra/newview/llfloatersnapshot.cpp", "LLFloaterSnapshotBase::ImplBase", "setStatus/setNeedRefresh", 448),
    ],
}


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def tag_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def line_for_name(lines: list[str], name: str, start: int = 0) -> int:
    needle = re.compile(r"\bname\s*=\s*(['\"])" + re.escape(name) + r"\1")
    for number, line in enumerate(lines[start:], start + 1):
        if needle.search(line):
            return number
    return 1


def line_for_key(lines: list[str], name: str) -> int:
    needle = re.compile(r"<key>\s*" + re.escape(name) + r"\s*</key>")
    for number, line in enumerate(lines, 1):
        if needle.search(line):
            return number
    return 1


def scan_xui(widget_names: set[str]) -> tuple[dict[str, list[dict]], list[dict]]:
    by_widget: dict[str, list[dict]] = collections.defaultdict(list)
    all_controls: list[dict] = []
    for path in sorted(XUI_DIR.rglob("*.xml")):
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        try:
            root = ET.fromstring(text)
        except ET.ParseError as exc:
            print(f"warning: cannot parse {rel(path)}: {exc}", file=sys.stderr)
            continue
        root_info = {
            "file": rel(path),
            "tag": tag_name(root.tag),
            "name": root.attrib.get("name", ""),
            "line": line_for_name(lines, root.attrib.get("name", "")) if root.attrib.get("name") else 1,
        }
        search_from = 0
        for element in root.iter():
            name = element.attrib.get("name")
            if not name:
                continue
            number = line_for_name(lines, name, search_from)
            search_from = max(search_from, number - 1)
            item = {
                "file": rel(path),
                "line": number,
                "tag": tag_name(element.tag),
                "control_name": element.attrib.get("control_name"),
                "callbacks": sorted({
                    child.attrib["function"]
                    for child in element.iter()
                    if tag_name(child.tag).endswith("commit_callback") and child.attrib.get("function")
                }),
                "root": root_info,
            }
            all_controls.append({"name": name, **item})
            if name in widget_names:
                by_widget[name].append(item)
    return dict(by_widget), all_controls


def source_files() -> list[Path]:
    files: set[Path] = set()
    for directory in CPP_DIRS:
        files.update(directory.rglob("*.cpp"))
    return sorted(files)


def scan_cpp(widget_names: set[str]) -> dict[str, list[dict]]:
    hits: dict[str, list[dict]] = collections.defaultdict(list)
    for path in source_files():
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        current_class = ""
        current_function = ""
        pending: tuple[str, str] | None = None
        depth = 0
        method_depth: int | None = None
        for number, line in enumerate(lines, 1):
            match = METHOD_RE.search(line)
            if match:
                class_name = match.group("class")
                function_name = match.group("function")
                # A return type is required except for constructors and
                # destructors.  This keeps Class::method() calls and comments
                # from being mistaken for definitions.
                if match.group("prefix").strip() or function_name.lstrip("~") == class_name:
                    pending = (class_name, function_name)
            opens = line.count("{")
            closes = line.count("}")
            if pending and opens:
                current_class, current_function = pending
                method_depth = depth + 1
                pending = None
            for literal in STRING_RE.findall(line):
                name = literal.replace(r'\"', '"').replace(r"\\", "\\")
                if name in widget_names:
                    hits[name].append({
                        "file": rel(path),
                        "line": number,
                        "class": current_class or None,
                        "function": current_function or None,
                        "text": line.strip(),
                    })
            depth += opens - closes
            if method_depth is not None and depth < method_depth:
                current_class = ""
                current_function = ""
                method_depth = None
    return dict(hits)


def scan_callback_handlers(callback_names: set[str]) -> dict[str, list[dict]]:
    handlers: dict[str, list[dict]] = collections.defaultdict(list)
    target_re = re.compile(r"&([A-Za-z_]\w*)::([A-Za-z_]\w*)")
    for path in source_files():
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            literals = set(STRING_RE.findall(line))
            for callback in sorted(literals & callback_names):
                target = target_re.search(line)
                handlers[callback].append({
                    "callback": callback,
                    "file": rel(path),
                    "line": number,
                    "class": target.group(1) if target else None,
                    "function": target.group(2) if target else None,
                    "text": line.strip(),
                })
    return dict(handlers)


def llsd_map(element: ET.Element) -> dict[str, ET.Element]:
    children = list(element)
    result: dict[str, ET.Element] = {}
    index = 0
    while index + 1 < len(children):
        if tag_name(children[index].tag) == "key":
            result[children[index].text or ""] = children[index + 1]
            index += 2
        else:
            index += 1
    return result


def setting_bases() -> list[tuple[str, Path]]:
    root = ET.parse(SETTINGS_FILES).getroot()
    bases: list[tuple[str, Path]] = []
    seen: set[tuple[str, Path]] = set()
    for file_node in root.findall(".//file"):
        group = file_node.attrib.get("name", "")
        filename = file_node.attrib.get("file_name", "")
        if group not in {"Global", "PerAccount"} or not filename:
            continue
        path = SETTINGS_DIR / filename
        key = (group, path)
        if path.exists() and key not in seen:
            bases.append(key)
            seen.add(key)
    return bases


def scan_settings() -> dict[str, list[dict]]:
    declarations: dict[str, list[dict]] = collections.defaultdict(list)
    visited: set[tuple[str, Path]] = set()

    def load(group: str, path: Path) -> None:
        key = (group, path.resolve())
        if key in visited or not path.exists():
            return
        visited.add(key)
        root = ET.parse(path).getroot()
        map_node = next((node for node in root.iter() if tag_name(node.tag) == "map"), None)
        if map_node is None:
            return
        entries = llsd_map(map_node)
        include = entries.get("Include")
        if include is not None:
            for node in include:
                if tag_name(node.tag) == "string" and node.text:
                    load(group, path.parent / node.text)
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        for name, value in entries.items():
            if name == "Include" or tag_name(value.tag) != "map":
                continue
            fields = llsd_map(value)
            type_node = fields.get("Type")
            type_name = (type_node.text or "") if type_node is not None else ""
            declarations[name].append({
                "file": rel(path),
                "line": line_for_key(lines, name),
                "group": group,
                "type": type_name or None,
            })

    for group, path in setting_bases():
        load(group, path)
    return dict(declarations)


def flatten_data() -> tuple[list[dict], list[dict]]:
    data = json.loads(DATA.read_text(encoding="utf-8"))
    pages: list[dict] = []
    rows: list[dict] = []
    for group_index, group in enumerate(data["groups"]):
        for page_index, page in enumerate(group["pages"]):
            page_record = {
                "key": page["name"],
                "label": page["label"],
                "group": group["name"],
                "group_label": group["label"],
                "order": [group_index, page_index],
            }
            pages.append(page_record)
            for card_index, card in enumerate(page.get("cards", [])):
                for row_index, row in enumerate(card.get("rows", [])):
                    rows.append({
                        "page": page_record,
                        "card_id": card.get("id", ""),
                        "card_title": card.get("t", ""),
                        "card_index": card_index,
                        "row_index": row_index,
                        "label": row.get("l", ""),
                        "kind": row.get("k", ""),
                        "widget": row.get("w", ""),
                        "declared_setting": row.get("s"),
                        "items": row.get("it", []),
                    })
    return pages, rows


def unique_dicts(items: list[dict], keys: tuple[str, ...]) -> list[dict]:
    seen: set[tuple] = set()
    result: list[dict] = []
    for item in items:
        key = tuple(item.get(field) for field in keys)
        if key not in seen:
            seen.add(key)
            result.append(item)
    return result


def build_index() -> dict:
    pages, raw_rows = flatten_data()
    widgets = {row["widget"] for row in raw_rows if row["widget"]}
    xui, all_controls = scan_xui(widgets)
    cpp = scan_cpp(widgets)
    callback_names = {
        callback
        for hits in xui.values()
        for hit in hits
        for callback in hit.get("callbacks", [])
    }
    callback_handlers = scan_callback_handlers(callback_names)
    settings = scan_settings()

    page_rows: dict[str, list[dict]] = collections.defaultdict(list)
    setting_pages: dict[str, set[str]] = collections.defaultdict(set)
    resolved_rows: list[dict] = []
    for raw in raw_rows:
        widget = raw["widget"]
        xui_hits = xui.get(widget, []) if widget else []
        cpp_hits = cpp.get(widget, []) if widget else []
        row_callback_names = sorted({callback for hit in xui_hits for callback in hit.get("callbacks", [])})
        row_callback_handlers = [
            handler
            for callback in row_callback_names
            for handler in callback_handlers.get(callback, [])
        ]
        xml_settings = sorted({hit["control_name"] for hit in xui_hits if hit.get("control_name")})
        declared = raw["declared_setting"]
        candidates = ([declared] if declared else []) + [item for item in xml_settings if item != declared]
        setting = declared or (xml_settings[0] if len(xml_settings) == 1 else None)
        if setting:
            setting_pages[setting].add(raw["page"]["key"])
        reasons: list[str] = []
        if not widget:
            reasons.append("row has no widget name")
        if not xui_hits:
            reasons.append("widget not found in XUI")
        if not cpp_hits and not row_callback_handlers and not setting:
            reasons.append("no C++ reference or setting binding")
        if setting and setting not in settings:
            reasons.append("setting declaration not found")
        if not declared and len(xml_settings) > 1:
            reasons.append("ambiguous XUI control_name")
        row = {
            "card": raw["card_id"],
            "card_title": raw["card_title"],
            "index": raw["row_index"],
            "label": raw["label"],
            "kind": raw["kind"],
            "widget": widget or None,
            "declared_setting": declared,
            "resolved_setting": setting,
            "setting_candidates": candidates,
            "setting_declarations": settings.get(setting, []) if setting else [],
            "items": raw["items"],
            "xui": xui_hits,
            "cpp": cpp_hits,
            "callback_handlers": row_callback_handlers,
            "unresolved": reasons,
        }
        resolved_rows.append(row)
        page_rows[raw["page"]["key"]].append(row)

    # XUI controls outside the mockup pages are writable peers when they bind a
    # duplicated setting.  Page peers are added after every row is resolved.
    xui_setting_writers: dict[str, list[dict]] = collections.defaultdict(list)
    for control in all_controls:
        setting = control.get("control_name")
        if setting:
            xui_setting_writers[setting].append({
                "file": control["file"],
                "line": control["line"],
                "widget": control["name"],
                "root": control["root"],
            })

    output_pages: list[dict] = []
    all_unresolved = 0
    for page in pages:
        rows = page_rows[page["key"]]
        owner_counts: dict[tuple, int] = collections.Counter()
        owner_data: dict[tuple, dict] = {}
        class_counts: collections.Counter[str] = collections.Counter()
        for row in rows:
            for hit in row["xui"]:
                root = hit["root"]
                key = (root["file"], root["tag"], root["name"])
                owner_counts[key] += 1
                owner_data[key] = root
            for hit in row["cpp"]:
                if hit.get("class"):
                    class_counts[hit["class"]] += 1
            for hit in row["callback_handlers"]:
                if hit.get("class"):
                    class_counts[hit["class"]] += 1
        owners = []
        for key, count in sorted(owner_counts.items(), key=lambda pair: (-pair[1], pair[0])):
            owner = dict(owner_data[key])
            owner["matched_rows"] = count
            owner["evidence"] = "row-widget match"
            owners.append(owner)
        owner_keys = {(owner["file"], owner["name"]) for owner in owners}
        for special in SPECIAL_OWNER_RECORDS.get(page["key"], []):
            if (special["file"], special["name"]) not in owner_keys:
                owners.append({**special, "matched_rows": 0, "evidence": "full-editor owner; approved rows are summary/launcher rows"})
            class_counts[special["class"]] += 0
        setting_bound = sum(bool(row["resolved_setting"] and row["setting_declarations"]) for row in rows)
        cpp_rows = sum(bool(row["cpp"] or row["callback_handlers"]) for row in rows)
        coverage = (setting_bound / len(rows)) if rows else 0.0
        logic_class = "pure-setting" if rows and coverage >= 0.90 and cpp_rows == 0 else "code-driven"
        unresolved_count = sum(bool(row["unresolved"]) for row in rows)
        all_unresolved += unresolved_count
        output_pages.append({
            **{key: page[key] for key in ("key", "label", "group", "group_label")},
            "row_count": len(rows),
            "owner_roots": owners,
            "owner_classes": [
                {"class": name, "reference_count": count}
                for name, count in class_counts.most_common()
            ],
            "classification": {
                "logic_class": logic_class,
                "setting_bound_rows": setting_bound,
                "setting_coverage": round(coverage, 6),
                "cpp_referenced_rows": cpp_rows,
                "rule": "pure-setting iff >=90% rows have declared settings and zero rows have C++ references",
            },
            "unresolved_row_count": unresolved_count,
            "rows": rows,
        })

    conflicts = []
    for setting, page_set in sorted(setting_pages.items()):
        writers = unique_dicts(xui_setting_writers.get(setting, []), ("file", "line", "widget"))
        if len(page_set) > 1 or writers:
            conflicts.append({
                "setting": setting,
                "pages": sorted(page_set),
                "xui_writers": writers,
                "declarations": settings.get(setting, []),
            })
    for page in output_pages:
        for row in page["rows"]:
            setting = row["resolved_setting"]
            if not setting:
                row["other_writers"] = []
                continue
            others = []
            for other_page in sorted(setting_pages[setting] - {page["key"]}):
                others.append({"kind": "page", "page": other_page})
            own_xui = {(hit["file"], hit["line"]) for hit in row["xui"]}
            for writer in xui_setting_writers.get(setting, []):
                if (writer["file"], writer["line"]) not in own_xui:
                    others.append({"kind": "xui", **writer})
            row["other_writers"] = unique_dicts(others, ("kind", "page", "file", "line", "widget"))

    return {
        "schema_version": 1,
        "source": rel(DATA),
        "classification_rule": "pure-setting iff >=90% rows are setting-bound and no row widget is referenced in C++; otherwise code-driven",
        "summary": {
            "pages": len(output_pages),
            "rows": len(resolved_rows),
            "owner_roots": len({owner["file"] for page in output_pages for owner in page["owner_roots"]}),
            "unresolved_rows": all_unresolved,
            "multi_page_settings": sum(len(item["pages"]) > 1 for item in conflicts),
        },
        "setting_conflicts": conflicts,
        "pages": output_pages,
    }


def md_anchor(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def render_markdown(index: dict) -> str:
    lines = [
        "# Director Kit page source evidence",
        "",
        f"Generated by `scripts/code_tools/director_page_sources.py` from `{index['source']}`.",
        "Do not hand-edit. File and line references describe the scanned tree at generation time.",
        "",
        "## Summary",
        "",
        "| Pages | Rows | Distinct owner files | Unresolved rows | Multi-page settings |",
        "|---:|---:|---:|---:|---:|",
        f"| {index['summary']['pages']} | {index['summary']['rows']} | {index['summary']['owner_roots']} | {index['summary']['unresolved_rows']} | {index['summary']['multi_page_settings']} |",
        "",
        f"Classification: {index['classification_rule']}.",
        "",
        "## One-home conflicts",
        "",
        "| Setting | Director pages | Other XUI writers |",
        "|---|---|---|",
    ]
    for conflict in index["setting_conflicts"]:
        if len(conflict["pages"]) < 2:
            continue
        writers = ", ".join(
            f"`{item['root']['name'] or item['root']['tag']}` (`{item['file']}:{item['line']}` / `{item['widget']}`)"
            for item in conflict["xui_writers"]
        ) or "none"
        lines.append(f"| `{conflict['setting']}` | {', '.join(f'`{p}`' for p in conflict['pages'])} | {writers} |")
    lines += ["", "## Pages", ""]
    for page in index["pages"]:
        c = page["classification"]
        lines += [
            f"### {page['label']} (`{page['key']}`)",
            "",
            f"Real owner roots: " + (", ".join(
                f"`{owner['name'] or owner['tag']}` in `{owner['file']}:{owner['line']}` ({owner['matched_rows']} matched rows)"
                for owner in page["owner_roots"]
            ) or "none found") + ".",
            "",
            "C++ owner classes: " + (", ".join(
                f"`{item['class']}` ({item['reference_count']} references)" for item in page["owner_classes"]
            ) or "none found") + ".",
            "",
            f"Computed logic class: **{c['logic_class']}**; {c['setting_bound_rows']}/{page['row_count']} rows setting-bound, {c['cpp_referenced_rows']} rows referenced in C++.",
            "",
            "| Card / row | Widget | Kind | Setting declaration | XUI definitions | C++ handlers | Other writers | Unresolved |",
            "|---|---|---|---|---|---|---|---|",
        ]
        for row in page["rows"]:
            declarations = ", ".join(
                f"`{item['type']}` `{item['file']}:{item['line']}` ({item['group']})"
                for item in row["setting_declarations"]
            ) or (f"`{row['resolved_setting']}` (missing declaration)" if row["resolved_setting"] else "—")
            xui_text = ", ".join(
                f"`{hit['file']}:{hit['line']}` `{hit['root']['name'] or hit['root']['tag']}`/{hit['tag']}"
                for hit in row["xui"]
            ) or "—"
            cpp_text = ", ".join(
                f"`{hit['class'] or '?'}::{hit['function'] or '?'} {hit['file']}:{hit['line']}`"
                for hit in [*row["cpp"], *row["callback_handlers"]]
            ) or "—"
            other_text = ", ".join(
                (f"page `{item['page']}`" if item["kind"] == "page" else f"`{item['root']['name'] or item['root']['tag']}` `{item['file']}:{item['line']}`")
                for item in row["other_writers"]
            ) or "—"
            unresolved = "; ".join(row["unresolved"]) or "—"
            label = row["label"].replace("|", "\\|")
            lines.append(
                f"| `{row['card']}` / {label} | `{row['widget'] or ''}` | `{row['kind']}` | {declarations} | {xui_text} | {cpp_text} | {other_text} | {unresolved} |"
            )
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def page_audit_states(page: dict) -> list[tuple[str, str]]:
    states: list[tuple[str, str]] = [
        ("size-850x560", "Default data at the minimum supported shell size."),
        ("size-915x720", "Default data at the narrow 16:9 shell size."),
        ("size-1141x720", "Default data at the wide non-round shell size."),
    ]
    cards: dict[str, list[dict]] = collections.defaultdict(list)
    for row in page["rows"]:
        cards[row["card"]].append(row)
    for card, rows in cards.items():
        states.append((f"card-{card}-tallest-850", f"Only `{card}` is put in its tallest local state at 850 px; all other cards stay default."))
        states.append((f"card-{card}-tallest-1160", f"Only `{card}` is put in its tallest local state at 1160 px; all other cards stay default."))
        for row in rows:
            widget = row["widget"] or f"row-{row['index']}"
            if row["kind"] == "combo":
                for index, item in enumerate(row.get("items", [])):
                    states.append((f"{widget}-option-{index}", f"Only `{widget}` selects `{item}`; other selectors remain default."))
            elif row["kind"] == "toggle":
                states.append((f"{widget}-on", f"Only `{widget}` is on; dependent visibility is measured."))
            elif row["kind"] == "list":
                states.extend([
                    (f"{widget}-empty", f"`{widget}` is empty with no selection."),
                    (f"{widget}-one", f"`{widget}` has one selected item."),
                    (f"{widget}-overflow", f"`{widget}` has 51 items and a visible scrollbar."),
                ])
    key = page["key"]
    if key in ACTOR_PAGES:
        states.extend([
            ("actor-none", "Rail has no actor; target-dependent rows are disabled with an explicit empty state."),
            ("actor-selected", "One rail actor is selected and unpinned."),
            ("actor-pinned-rail-changes", "Page target is pinned while the rail changes to another actor."),
            ("actor-removed", "The active actor disappears while the page is visible."),
        ])
    if key in LIST_PAGES:
        states.append(("selection-deleted", "The selected model row is removed by another owner while visible."))
    if key in PERMISSION_PAGES:
        states.extend([
            ("permission-denied", "The current target exists but cannot be modified."),
            ("target-offline", "The current target/provider becomes unavailable."),
        ])
    if key in {"gaze_cues_tab", "light_cues_tab"}:
        states.extend([
            ("shared-owner-other-floater", "The standalone cue floater owns edit state; Director remains read-only."),
            ("shared-owner-director", "Director owns edit state; the standalone floater mirrors without claiming."),
            ("cue-playback", "Cue playback is active; history recording is suspended until stop."),
        ])
    if key == "vcam_tab":
        states.extend((f"captures-{count}", f"Registry contains {count} captures.") for count in (0, 1, 4, 5, 8))
        states.extend([
            ("preview-paused", "All preview tiles are paused."),
            ("preview-throttled", "Preview scheduler reports throttling."),
            ("display-raw", "A monitor shows its raw capture."),
            ("display-selected", "A monitor shows a selected display output."),
        ])
    if key == "capture_settings_tab":
        states.append(("data-missing", "Approved page remains empty; audit must fail closed until rows are approved."))
    # Preserve order while removing duplicate identifiers.
    seen: set[str] = set()
    result = []
    for state in states:
        if state[0] not in seen:
            seen.add(state[0])
            result.append(state)
    if len(result) > 256:
        raise RuntimeError(f"{key} has {len(result)} audit states; hard cap is 256")
    return result


def row_binding(row: dict, adapter: str) -> str:
    handlers = []
    for hit in [*row["cpp"], *row["callback_handlers"]]:
        handler = f"`{hit['class'] or '?'}::{hit['function'] or '?'}()` (`{hit['file']}:{hit['line']}`)"
        if handler not in handlers:
            handlers.append(handler)
    setting = row["resolved_setting"]
    if setting:
        declaration = row["setting_declarations"][0] if row["setting_declarations"] else None
        type_text = f" `{declaration['type']}`" if declaration and declaration.get("type") else ""
        binding = f"kit `{row['kind']}` ↔ `{setting}`{type_text}"
        if handlers:
            binding += f"; `{adapter}` preserves/calls " + ", ".join(handlers)
        return binding
    if handlers:
        return f"`{adapter}` command/model binding → " + ", ".join(handlers)
    if row["kind"] == "readout":
        return "kit readout ← approved static copy; no runtime writer"
    if row["kind"] in {"button", "link", "toggle"}:
        return f"**USER DECISION:** register an explicit `{adapter}` command; no matching setting or C++ literal was found"
    return "**USER DECISION:** no setting, C++ reference, or model accessor was found"


def render_page_spec(page: dict) -> str:
    key = page["key"]
    adapter = ADAPTERS[key]
    c = page["classification"]
    lines = [
        f"# {page['label']} — Director Kit page contract",
        "",
        f"Page key: `{key}`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).",
        f"Computed class: **{c['logic_class']}** ({c['setting_bound_rows']}/{page['row_count']} setting-bound rows; {c['cpp_referenced_rows']} C++-referenced rows).",
        "",
        "## Real owners",
        "",
    ]
    if page["owner_roots"]:
        for owner in page["owner_roots"]:
            lines.append(f"- `{owner['name'] or owner['tag']}` / `{owner['tag']}` at `{owner['file']}:{owner['line']}` ({owner['matched_rows']} matching rows).")
    else:
        lines.append("- No approved row widget resolves to XUI.")
    for owner in SPECIAL_OWNERS.get(key, []):
        lines.append(f"- {owner}.")
    if page["owner_classes"]:
        lines.append("- Referencing classes: " + ", ".join(f"`{item['class']}` ({item['reference_count']})" for item in page["owner_classes"]) + ".")
    lines += [
        "",
        "Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.",
        "",
        "## Presentation rule",
        "",
        PRESENTATION_NOTES.get(key, "Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized."),
        "",
        "## Row bindings",
        "",
        f"`{adapter}` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.",
        "",
        "| Card / row | Existing control evidence | Required binding | Other writable homes |",
        "|---|---|---|---|",
    ]
    for row in page["rows"]:
        evidence = ", ".join(
            f"`{hit['tag']}` `{hit['file']}:{hit['line']}`" for hit in row["xui"]
        ) or "none"
        binding = row_binding(row, adapter)
        others = ", ".join(
            f"page `{item['page']}`" if item["kind"] == "page" else f"`{item['file']}:{item['line']}` `{item['widget']}`"
            for item in row["other_writers"]
        ) or "none"
        label = row["label"].replace("|", "\\|")
        lines.append(f"| `{row['card']}` / `{row['widget'] or ''}` ({label}) | {evidence} | {binding} | {others} |")

    methods: dict[tuple, set[int]] = collections.defaultdict(set)
    for row in page["rows"]:
        for hit in [*row["cpp"], *row["callback_handlers"]]:
            methods[(hit["file"], hit.get("class") or "?", hit.get("function") or "?")].add(hit["line"])
    for file_name, class_name, function_name, number in SPECIAL_EXTRACTIONS.get(key, []):
        methods[(file_name, class_name, function_name)].add(number)
    lines += ["", "## Adapter extraction", ""]
    if methods:
        lines += [
            "| Existing function | Extraction contract |",
            "|---|---|",
        ]
        for (file_name, class_name, function_name), numbers in sorted(methods.items()):
            number_text = ", ".join(str(number) for number in sorted(numbers))
            lines.append(f"| `{class_name}::{function_name}()` — `{file_name}:{number_text}` | Move model-facing work into `{adapter}` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |")
    else:
        lines.append(f"No approved widget has a C++ literal reference. `{adapter}` owns only explicit actions and setting signals; it must not synthesize a polling mirror.")

    lines += [
        "",
        "## Live state and refresh",
        "",
        "- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.",
        "- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.",
        "- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.",
        "- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.",
        "",
        "## Bounded non-default audit states",
        "",
    ]
    states = page_audit_states(page)
    lines.append(f"Manifest size: **{len(states)} states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.")
    lines += ["", "| State key | Exact scenario |", "|---|---|"]
    for state, description in states:
        lines.append(f"| `{state}` | {description} |")

    unresolved = [row for row in page["rows"] if row["unresolved"]]
    decisions = [row for row in page["rows"] if "USER DECISION" in row_binding(row, adapter)]
    lines += [
        "",
        "## Parity check",
        "",
        "1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.",
        "2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.",
        "3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.",
        "4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.",
        "",
        "## New widget required",
        "",
        "None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.",
        "",
        "## Unresolved and USER DECISION items",
        "",
    ]
    if not unresolved and not decisions:
        lines.append("None in the current evidence index.")
    else:
        if key in {"diopter_tab", "capture_settings_tab"}:
            lines.append(f"- **USER DECISION:** {PRESENTATION_NOTES[key]}")
        for row in unresolved:
            reason = "; ".join(row["unresolved"])
            decision = " **USER DECISION required.**" if row in decisions else ""
            lines.append(f"- `{row['widget'] or '(unnamed)'}` ({row['label']}): {reason}.{decision}")

    lines += [
        "",
        "## Risks",
        "",
        "- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.",
        "- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.",
        "- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.",
        "- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.",
        "- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.",
    ]
    return "\n".join(lines).rstrip() + "\n"


def page_specs(index: dict) -> list[tuple[Path, str]]:
    by_key = {page["key"]: page for page in index["pages"]}
    order = R2_FIRST + [page["key"] for page in index["pages"] if page["key"] not in R2_FIRST]
    return [(PAGES_DIR / f"{key}.md", render_page_spec(by_key[key])) for key in order]


def summary_table(index: dict) -> str:
    rows = ["page                 rows owners class         cpp unresolved", "-------------------- ---- ------ ------------- --- ----------"]
    for page in index["pages"]:
        rows.append(
            f"{page['key']:<20} {page['row_count']:>4} {len(page['owner_roots']):>6} "
            f"{page['classification']['logic_class']:<13} {page['classification']['cpp_referenced_rows']:>3} {page['unresolved_row_count']:>10}"
        )
    s = index["summary"]
    rows.append(
        f"TOTAL {s['pages']} pages, {s['rows']} rows, {s['owner_roots']} distinct owner files, "
        f"{s['unresolved_rows']} unresolved rows, {s['multi_page_settings']} multi-page settings"
    )
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if committed outputs differ")
    args = parser.parse_args()
    index = build_index()
    json_text = json.dumps(index, indent=2, sort_keys=False, ensure_ascii=False) + "\n"
    md_text = render_markdown(index)
    stale = []
    outputs = [(JSON_OUT, json_text), (MD_OUT, md_text), *page_specs(index)]
    for path, content in outputs:
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                stale.append(rel(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
            if path.parent == PAGES_DIR:
                print(f"wrote {rel(path)}")
    print(summary_table(index))
    if stale:
        print("stale generated files: " + ", ".join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
