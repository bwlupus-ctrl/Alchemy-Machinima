#!/usr/bin/env python3
"""Extract the current Director UI into a JSON content map.

Run from the repository root.  This intentionally reads the XUI rather than
maintaining a second, hand-written inventory of Director controls.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Optional


DEFAULT_OUT = Path(r"C:\Users\xianw\cxs\director_content_map.json")
XUI_REL = Path("indra/newview/skins/default/xui/en")
SETTINGS_REL = Path("indra/newview/app_settings")
DIRECTOR_XML = "floater_director.xml"
DIRECTOR_CPP = Path("indra/newview/llfloaterdirector.cpp")

TYPE_BY_TAG = {
    "slider": "slider",
    "slider_bar": "slider",
    "spinner": "spinner",
    "check_box": "check",
    "combo_box": "combo",
    "color_swatch": "color",
    "color_picker": "color",
    "text_editor": "text",
    "button": "button",
    "line_editor": "line_editor",
    "radio_group": "radio",
    "scroll_list": "list",
    "flat_list_view": "list",
    "list": "list",
}


@dataclass
class Node:
    elem: ET.Element
    source: str
    parent: Optional["Node"] = None
    children: list["Node"] = field(default_factory=list)
    embedded: bool = False

    @property
    def tag(self) -> str:
        return self.elem.tag

    def get(self, key: str, default: Optional[str] = None) -> Optional[str]:
        return self.elem.get(key, default)

    def ancestors(self) -> Iterable["Node"]:
        node = self.parent
        while node is not None:
            yield node
            node = node.parent

    def walk(self) -> Iterable["Node"]:
        yield self
        for child in self.children:
            yield from child.walk()


class ExtractError(RuntimeError):
    pass


def clean_text(value: Optional[str]) -> str:
    return " ".join((value or "").split())


def bool_value(value: Optional[str], default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes"}


def number(value: Optional[str]) -> Optional[int | float]:
    if value is None or not value.strip():
        return None
    try:
        result = float(value)
    except ValueError:
        return None
    return int(result) if result.is_integer() else result


def pretty_name(name: str) -> str:
    words = re.sub(r"(?:^|_)(?:card|panel|tab)(?:_|$)", "_", name)
    words = re.sub(r"_+", " ", words).strip()
    return words.title() if words else name


def element_text(elem: ET.Element) -> str:
    return clean_text(elem.get("value") or elem.get("label") or elem.text)


def parse_strings(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    root = ET.parse(path).getroot()
    return {
        node.get("name", ""): clean_text(node.text)
        for node in root.iter()
        if node.get("name") and clean_text(node.text)
    }


STRING_TOKEN = re.compile(r"\[([A-Za-z0-9_.-]+)\]")


def resolve_string(value: Optional[str], strings: dict[str, str]) -> str:
    text = clean_text(value)
    if not text or not strings:
        return text
    return STRING_TOKEN.sub(lambda m: strings.get(m.group(1), m.group(0)), text)


def build_tree(
    path: Path,
    xui_dir: Path,
    warnings: list[str],
    stack: tuple[str, ...] = (),
    parent: Optional[Node] = None,
    embedded: bool = False,
) -> Node:
    name = path.name
    if name in stack:
        raise ExtractError("recursive filename= cycle: " + " -> ".join(stack + (name,)))
    try:
        elem = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        raise ExtractError(f"cannot parse {path}: {exc}") from exc

    def wrap(xml_elem: ET.Element, owner: str, owner_parent: Optional[Node], is_embedded: bool) -> Node:
        node = Node(xml_elem, owner, owner_parent, embedded=is_embedded)
        for xml_child in list(xml_elem):
            child = wrap(xml_child, owner, node, False)
            node.children.append(child)
        filename = xml_elem.get("filename")
        if filename:
            include_path = xui_dir / filename
            if include_path.exists():
                included = build_tree(
                    include_path,
                    xui_dir,
                    warnings,
                    stack + (name,),
                    node,
                    True,
                )
                node.children.append(included)
            else:
                warnings.append(f"missing embedded XUI: {filename} (from {owner})")
        return node

    return wrap(elem, name, parent, embedded)


def llsd_value(elem: ET.Element) -> Any:
    tag = elem.tag
    if tag in {"string", "uuid", "uri", "date"}:
        return elem.text or ""
    if tag == "integer":
        return int((elem.text or "0").strip() or 0)
    if tag == "real":
        return float((elem.text or "0").strip() or 0)
    if tag == "boolean":
        return bool_value(elem.text)
    if tag == "undef":
        return None
    if tag == "array":
        return [llsd_value(child) for child in elem]
    if tag == "map":
        result: dict[str, Any] = {}
        children = list(elem)
        index = 0
        while index < len(children):
            if children[index].tag != "key" or index + 1 >= len(children):
                index += 1
                continue
            result[children[index].text or ""] = llsd_value(children[index + 1])
            index += 2
        return result
    return elem.text or ""


def load_settings(settings_dir: Path) -> tuple[dict[str, dict[str, Any]], list[str]]:
    files = [settings_dir / "settings.xml"]
    for extra in ("settings_alchemy.xml", "settings_per_account.xml"):
        if (settings_dir / extra).exists():
            files.append(settings_dir / extra)
    settings: dict[str, dict[str, Any]] = {}
    loaded: list[str] = []
    for path in files:
        if not path.exists():
            continue
        xml_root = ET.parse(path).getroot()
        payload = next(iter(xml_root), xml_root) if xml_root.tag == "llsd" else xml_root
        parsed = llsd_value(payload)
        if isinstance(parsed, dict):
            for key, value in parsed.items():
                if isinstance(value, dict):
                    settings[key] = value
        loaded.append(path.name)
    return settings, loaded


def setting_record(raw: dict[str, Any]) -> dict[str, Any]:
    return {
        "type": raw.get("Type"),
        "default": raw.get("Value"),
        "comment": raw.get("Comment", ""),
    }


def is_reset(node: Node) -> bool:
    if node.tag != "button":
        return False
    overlay = (node.get("image_overlay") or "").lower()
    return "refresh_off" in overlay and bool(node.get("commit_callback.parameter"))


def is_visible(node: Node) -> bool:
    if node.get("visible") is not None and not bool_value(node.get("visible"), True):
        return False
    return all(
        ancestor.get("visible") is None or bool_value(ancestor.get("visible"), True)
        for ancestor in node.ancestors()
    )


def is_launcher_descendant(node: Node) -> bool:
    return any((ancestor.get("name") or "").startswith("card_") for ancestor in node.ancestors())


def direct_bold_title(node: Node, strings: dict[str, str]) -> str:
    for child in node.children:
        if child.tag == "text" and "bold" in (child.get("font") or "").lower():
            title = resolve_string(element_text(child.elem), strings)
            if title:
                return title
    return ""


def section_node(control: Node, page: Node) -> Optional[Node]:
    for ancestor in control.ancestors():
        if ancestor is page:
            break
        if ancestor.tag in {"panel", "layout_panel"} and ancestor.get("name"):
            return ancestor
    return None


def section_title(node: Optional[Node], page: Node, strings: dict[str, str]) -> str:
    if node is None:
        return resolve_string(page.get("label"), strings) or pretty_name(page.get("name") or "page")
    return (
        direct_bold_title(node, strings)
        or resolve_string(node.get("label"), strings)
        or pretty_name(node.get("name") or "section")
    )


def adjacent_label(node: Node, strings: dict[str, str]) -> str:
    parent = node.parent
    if parent is None:
        return ""
    left = number(node.get("left"))
    top = number(node.get("top"))
    if left is None or top is None:
        return ""
    best: tuple[float, str] | None = None
    for sibling in parent.children:
        if sibling is node or sibling.tag != "text":
            continue
        text = resolve_string(element_text(sibling.elem), strings)
        text_left = number(sibling.get("left"))
        text_top = number(sibling.get("top"))
        width = number(sibling.get("width")) or 0
        if not text or text_left is None or text_top is None:
            continue
        gap = float(left) - (float(text_left) + float(width))
        if abs(float(top) - float(text_top)) <= 4 and -4 <= gap <= 200:
            score = abs(float(top) - float(text_top)) * 201 + max(0.0, gap)
            if best is None or score < best[0]:
                best = (score, text)
    return best[1] if best else ""


def first_clause(tooltip: str) -> str:
    text = clean_text(tooltip)
    if not text:
        return ""
    return re.split(r"(?<=[.!?])\s+|;\s+", text, maxsplit=1)[0].rstrip(".!?; ")


def control_label(node: Node, strings: dict[str, str]) -> tuple[str, str]:
    own_value = node.get("label")
    if node.tag == "text" and not own_value:
        own_value = node.get("value") or node.elem.text
    own = resolve_string(own_value, strings)
    if own:
        return own, "label"
    adjacent = adjacent_label(node, strings)
    if adjacent:
        return adjacent, "adjacent_text"
    tooltip = first_clause(resolve_string(node.get("tool_tip"), strings))
    if tooltip:
        return tooltip, "tooltip"
    return node.get("name") or node.tag, "name"


def item_records(node: Node, strings: dict[str, str]) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for child in node.walk():
        if child is node or child.tag not in {"combo_box.item", "radio_item", "item"}:
            continue
        label = resolve_string(child.get("label") or child.get("value") or child.elem.text, strings)
        value = child.get("value")
        records.append({"label": label, "value": "" if value is None else value})
    return records


def reset_targets(nodes: Iterable[Node]) -> tuple[set[str], list[Node]]:
    targets: set[str] = set()
    resets: list[Node] = []
    for node in nodes:
        if not is_reset(node):
            continue
        resets.append(node)
        targets.update(
            part.strip()
            for part in (node.get("commit_callback.parameter") or "").split(",")
            if part.strip()
        )
    return targets, resets


def reset_for_custom(node: Node, resets: list[Node]) -> bool:
    name = node.get("name") or ""
    if not name:
        return False
    for reset in resets:
        reset_name = reset.get("name") or ""
        if reset_name in {name + "_reset", re.sub(r"_(slider|spinner|combo|check)$", "", name) + "_reset"}:
            return True
    return False


def make_control(
    node: Node,
    settings: dict[str, dict[str, Any]],
    strings: dict[str, str],
    reset_names: set[str],
    resets: list[Node],
) -> dict[str, Any]:
    label, label_source = control_label(node, strings)
    control_name = node.get("control_name") or ""
    result: dict[str, Any] = {
        "widget": node.get("name") or node.tag,
        "type": "text" if node.tag == "text" else TYPE_BY_TAG.get(node.tag, "other"),
        "label": label,
        "control_name": control_name or None,
    }
    attr_map = {
        "min": "min_val",
        "max": "max_val",
        "increment": "increment",
        "decimals": "decimal_digits",
    }
    for output_key, attr in attr_map.items():
        value = number(node.get(attr))
        if value is not None:
            result[output_key] = value
    items = item_records(node, strings)
    if items:
        result["items"] = items
    tooltip = resolve_string(node.get("tool_tip"), strings)
    if tooltip:
        result["tooltip"] = tooltip
    if control_name and control_name in settings:
        result["setting"] = setting_record(settings[control_name])
    result["label_source"] = label_source
    if node.get("enabled_control"):
        result["enabled_control"] = node.get("enabled_control")
    visibility_control = node.get("visibility_control") or node.get("visible_control")
    if visibility_control:
        result["visibility_control"] = visibility_control
    # Static LLTextBox content is part of the mockup, but is not C++ behavior.
    result["custom"] = node.tag != "text" and not bool(control_name)
    if not is_visible(node):
        result["initially_visible"] = False
    if (control_name and control_name in reset_names) or (not control_name and reset_for_custom(node, resets)):
        result["has_reset"] = True
    return result


def text_is_structural(node: Node, interactive: list[Node]) -> bool:
    if "bold" in (node.get("font") or "").lower():
        return True
    for control in interactive:
        if control.parent is node.parent and adjacent_label(control, {}) == element_text(node.elem):
            return True
    return False


def page_controls(
    page: Node,
    settings: dict[str, dict[str, Any]],
    strings: dict[str, str],
    exclude: Optional[Node] = None,
) -> tuple[list[tuple[Optional[Node], dict[str, Any]]], list[str]]:
    all_nodes = [
        node for node in page.walk()
        if node is not exclude and not any(ancestor is exclude for ancestor in node.ancestors())
    ]
    reset_names, resets = reset_targets(all_nodes)
    interactive = [
        node for node in all_nodes
        if node.tag in TYPE_BY_TAG and not is_reset(node) and not is_launcher_descendant(node)
    ]
    controls = list(interactive)
    for node in all_nodes:
        if node.tag != "text" or not node.get("name") or is_launcher_descendant(node):
            continue
        if not text_is_structural(node, interactive):
            controls.append(node)
    controls.sort(key=all_nodes.index)
    records = [
        (section_node(node, page), make_control(node, settings, strings, reset_names, resets))
        for node in controls
    ]
    referenced = sorted({node.get("control_name") for node in interactive if node.get("control_name")})
    return records, referenced


def source_files(page: Node) -> list[str]:
    embedded = list(OrderedDict.fromkeys(node.source for node in page.walk() if node.source != DIRECTOR_XML))
    return embedded or [DIRECTOR_XML]


def launcher_targets(cpp_path: Path) -> dict[str, dict[str, str]]:
    if not cpp_path.exists():
        return {}
    text = cpp_path.read_text(encoding="utf-8", errors="replace")
    matches = list(re.finditer(r'addLauncherCard\("([^"]+)"', text))
    result: dict[str, dict[str, str]] = {}
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        block = text[match.start():end]
        target: dict[str, str] = {}
        tab = re.search(r'ALFloaterLightBox::showTab\("([^"]+)"\)', block)
        floater = re.search(r'LLFloaterReg::showInstance\("([^"]+)"', block)
        if tab:
            target = {"target": "lightbox", "target_tab": tab.group(1)}
        elif floater:
            target = {"target": floater.group(1)}
        result[match.group(1)] = target
    return result


def page_launchers(page: Node, targets: dict[str, dict[str, str]], strings: dict[str, str]) -> list[dict[str, Any]]:
    launchers: list[dict[str, Any]] = []
    for node in page.walk():
        name = node.get("name") or ""
        if node.tag != "panel" or not name.startswith("card_"):
            continue
        launcher_id = name[5:]
        title = ""
        button: Optional[Node] = None
        for child in node.walk():
            child_name = child.get("name") or ""
            if child_name == f"card_{launcher_id}_title":
                title = resolve_string(element_text(child.elem), strings)
            elif child_name == f"card_{launcher_id}_open":
                button = child
        if button is None:
            continue
        record: dict[str, Any] = {
            "name": launcher_id,
            "label": title or pretty_name(launcher_id),
            "button_label": resolve_string(button.get("label"), strings),
        }
        tooltip = resolve_string(button.get("tool_tip"), strings)
        if tooltip:
            record["tooltip"] = tooltip
        record.update(targets.get(launcher_id, {}))
        launchers.append(record)
    return launchers


def extract(repo: Path) -> dict[str, Any]:
    xui_dir = repo / XUI_REL
    warnings: list[str] = []
    root = build_tree(xui_dir / DIRECTOR_XML, xui_dir, warnings)
    strings = parse_strings(xui_dir / "strings.xml")
    settings, settings_files = load_settings(repo / SETTINGS_REL)
    targets = launcher_targets(repo / DIRECTOR_CPP)

    director_tabs = next(
        (node for node in root.walk() if node.tag == "tab_container" and node.get("name") == "director_tabs"),
        None,
    )
    if director_tabs is None:
        raise ExtractError("director_tabs not found")

    persistent_flat, persistent_referenced = page_controls(
        root, settings, strings, exclude=director_tabs
    )
    missing: set[str] = {
        name for name in persistent_referenced if name not in settings
    }
    persistent_map: OrderedDict[str, dict[str, Any]] = OrderedDict()
    for section, control in persistent_flat:
        section_name = section.get("name") if section is not None else "director_chrome"
        if section_name not in persistent_map:
            persistent_map[section_name] = {
                "name": section_name,
                "title": section_title(section, root, strings) if section is not None else "Director chrome",
                "controls": [],
            }
        persistent_map[section_name]["controls"].append(control)

    groups: list[dict[str, Any]] = []
    for group_node in (node for node in director_tabs.children if node.tag == "panel"):
        tabs = next((node for node in group_node.children if node.tag == "tab_container"), None)
        if tabs is None:
            continue
        pages: list[dict[str, Any]] = []
        for page_node in (node for node in tabs.children if node.tag == "panel"):
            flat_controls, referenced = page_controls(page_node, settings, strings)
            missing.update(name for name in referenced if name not in settings)
            section_map: OrderedDict[str, dict[str, Any]] = OrderedDict()
            for section, control in flat_controls:
                section_name = section.get("name") if section is not None else page_node.get("name") + "_section"
                if section_name not in section_map:
                    section_map[section_name] = {
                        "name": section_name,
                        "title": section_title(section, page_node, strings),
                        "controls": [],
                    }
                section_map[section_name]["controls"].append(control)
            page_warning: list[str] = []
            if not section_map and not page_launchers(page_node, targets, strings):
                page_warning.append("page contains no extracted controls or launcher cards")
            pages.append({
                "name": page_node.get("name"),
                "label": resolve_string(page_node.get("label"), strings) or pretty_name(page_node.get("name") or ""),
                "source_files": source_files(page_node),
                "sections": list(section_map.values()),
                "launchers": page_launchers(page_node, targets, strings),
                "warnings": page_warning,
            })
        groups.append({
            "name": group_node.get("name"),
            "label": resolve_string(group_node.get("label"), strings) or pretty_name(group_node.get("name") or ""),
            "pages": pages,
        })

    page_count = sum(len(group["pages"]) for group in groups)
    section_count = sum(len(page["sections"]) for group in groups for page in group["pages"])
    controls = [
        control
        for group in groups
        for page in group["pages"]
        for section in page["sections"]
        for control in section["controls"]
    ]
    persistent_controls = [
        control for section in persistent_map.values() for control in section["controls"]
    ]
    all_controls = persistent_controls + controls
    return {
        "persistent_sections": list(persistent_map.values()),
        "groups": groups,
        "summary": {
            "groups": len(groups),
            "pages": page_count,
            "sections": section_count + len(persistent_map),
            "controls": len(all_controls),
            "with_control_name": sum(bool(control.get("control_name")) for control in all_controls),
            "custom": sum(bool(control.get("custom")) for control in all_controls),
            "launchers": sum(len(page["launchers"]) for group in groups for page in group["pages"]),
            "page_sections": section_count,
            "page_controls": len(controls),
            "persistent_sections": len(persistent_map),
            "persistent_controls": len(persistent_controls),
            "persistent_with_control_name": sum(
                bool(control.get("control_name")) for control in persistent_controls
            ),
            "persistent_custom": sum(bool(control.get("custom")) for control in persistent_controls),
        },
        "missing_control_names": sorted(missing),
        "metadata": {
            "settings_files": settings_files,
            "warnings": warnings,
        },
    }


def print_summary(data: dict[str, Any], out_path: Path) -> None:
    persistent = data["summary"]["persistent_controls"]
    print(
        f'Persistent Director chrome: {data["summary"]["persistent_sections"]} sections, '
        f'{persistent} controls'
    )
    for group in data["groups"]:
        group_controls = 0
        print(f'{group["label"]} ({group["name"]})')
        for page in group["pages"]:
            count = sum(len(section["controls"]) for section in page["sections"])
            group_controls += count
            print(
                f'  {page["label"]} ({page["name"]}): '
                f'{len(page["sections"])} sections, {count} controls, {len(page["launchers"])} launchers'
            )
        print(f"  total: {group_controls} controls")
    summary = data["summary"]
    print(
        "Summary: "
        f'{summary["groups"]} groups, {summary["pages"]} pages, '
        f'{summary["sections"]} sections, {summary["controls"]} controls, '
        f'{summary["with_control_name"]} with control_name, '
        f'{summary["custom"]} custom, {summary["launchers"]} launchers'
    )
    print(f'Missing settings: {len(data["missing_control_names"])}')
    print(f"Wrote {out_path}")


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help=f"output JSON (default: {DEFAULT_OUT})")
    args = parser.parse_args(argv)
    repo = Path.cwd()
    required = repo / XUI_REL / DIRECTOR_XML
    if not required.exists():
        parser.error(f"run from the repository root; missing {required}")
    try:
        data = extract(repo)
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    except (ExtractError, OSError, ET.ParseError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print_summary(data, args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
