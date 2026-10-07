#!/usr/bin/env python3
"""Check XUI geometry, scrolling, and static scroller wiring.

Usage::

    python scripts/code_tools/xui_layout_check.py path/to/file.xml
    python scripts/code_tools/xui_layout_check.py --all
    python scripts/code_tools/xui_layout_check.py --changed <git-rev> [--json]
    python scripts/code_tools/xui_layout_check.py --selftest

The checker models LLView's top-left and legacy bottom-left positioning.  It
also accounts for tab headers, scroll documents, layout stacks, and accordion
content.  Resource-only XUI documents with no root width/height are ignored.
Intentional sibling layering must be documented in the XML with::

    <!-- xui-check: allow-overlap first_widget second_widget -->

Exit status is 1 when any finding is reported, 0 otherwise, and 2 for command
line or XML errors.  It uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable
from xml.parsers import expat


VPAD = 4
DEFAULT_TAB_HEIGHT = 23
CONTAINERS = {
    "accordion",
    "accordion_tab",
    "floater",
    "flow_grid",
    "layout_panel",
    "layout_stack",
    "panel",
    "scroll_container",
    "tab_container",
}
NON_WIDGETS = {
    "axis",
    "columns",
    "combo_item",
    "first_tab",
    "floater.string",
    "icons_combo_box.item",
    "item.columns",
    "middle_tab",
    "radio_item",
    "last_tab",
    "combo_box.item",
    "scroll_list.columns",
    "scroll_list.item",
    "scroll_list.rows",
    "stat_bar",
    "string",
    "item",
}
ALLOW_RE = re.compile(
    r"^\s*xui-check:\s*allow-overlap\s+(\S+)\s+(\S+)\s*$", re.I
)


@dataclass
class Rect:
    x: int
    y: int
    width: int
    height: int

    @property
    def right(self) -> int:
        return self.x + self.width

    @property
    def bottom(self) -> int:
        return self.y + self.height

    def absolute(self, parent: "Rect") -> "Rect":
        return Rect(parent.x + self.x, parent.y + self.y, self.width, self.height)

    def intersects(self, other: "Rect") -> bool:
        return (
            self.width > 0
            and self.height > 0
            and other.width > 0
            and other.height > 0
            and self.x < other.right
            and other.x < self.right
            and self.y < other.bottom
            and other.y < self.bottom
        )

    def inside(self, parent: "Rect") -> bool:
        return (
            self.x >= parent.x
            and self.y >= parent.y
            and self.right <= parent.right
            and self.bottom <= parent.bottom
        )

    def __str__(self) -> str:
        return f"({self.x},{self.y},{self.width},{self.height})"


@dataclass
class Node:
    tag: str
    attrs: dict[str, str]
    line: int
    parent: "Node | None" = None
    children: list["Node"] = field(default_factory=list)
    text_parts: list[str] = field(default_factory=list)
    rect: Rect | None = None
    design_size: tuple[int, int] | None = None
    explicit_attrs: set[str] = field(default_factory=set)

    @property
    def name(self) -> str:
        return self.attrs.get("name", f"<{self.tag}>")

    @property
    def text(self) -> str:
        return "".join(self.text_parts)

    def integer(self, key: str, default: int = 0) -> int:
        raw = self.attrs.get(key)
        if raw is None:
            return default
        try:
            return int(float(raw))
        except ValueError:
            return default


@dataclass
class Finding:
    kind: str
    line: int
    message: str

    def as_dict(self) -> dict[str, str | int]:
        return {"kind": self.kind, "line": self.line, "message": self.message}


def parse_xml(path: Path) -> tuple[Node, set[frozenset[str]]]:
    parser = expat.ParserCreate()
    stack: list[Node] = []
    root: Node | None = None
    allowances: set[frozenset[str]] = set()

    def start(tag: str, attrs: dict[str, str]) -> None:
        nonlocal root
        node = Node(tag, dict(attrs), parser.CurrentLineNumber,
                    explicit_attrs=set(attrs))
        if stack:
            node.parent = stack[-1]
            stack[-1].children.append(node)
        else:
            root = node
        stack.append(node)

    def end(_tag: str) -> None:
        stack.pop()

    def text(data: str) -> None:
        if stack:
            stack[-1].text_parts.append(data)

    def comment(data: str) -> None:
        match = ALLOW_RE.match(data)
        if match:
            allowances.add(frozenset(match.groups()))

    parser.StartElementHandler = start
    parser.EndElementHandler = end
    parser.CharacterDataHandler = text
    parser.CommentHandler = comment
    try:
        with path.open("rb") as source:
            parser.ParseFile(source)
    except (OSError, expat.ExpatError) as exc:
        raise ValueError(str(exc)) from exc
    if root is None:
        raise ValueError("empty XML document")
    apply_widget_defaults(root, path)
    return root, allowances


def apply_widget_defaults(root: Node, path: Path) -> None:
    """Merge widgets/<tag>.xml root attributes, matching LLUICtrlFactory."""
    directory = next(
        (candidate / "widgets" for candidate in (path.parent, *path.parents)
         if (candidate / "widgets").is_dir()),
        None,
    )
    if directory is None:
        return
    cache: dict[str, dict[str, str]] = {}
    panel_cache: dict[str, dict[str, str]] = {}

    def defaults(tag: str) -> dict[str, str]:
        if tag not in cache:
            widget_path = directory / f"{tag}.xml"
            values: dict[str, str] = {}
            if widget_path.is_file():
                try:
                    values = {
                        key: value
                        for key, value in ElementTree.parse(widget_path).getroot().attrib.items()
                        if key != "name"
                    }
                except (OSError, ElementTree.ParseError):
                    pass
            cache[tag] = values
        return cache[tag]

    pending = [root]
    while pending:
        node = pending.pop()
        merged = defaults(node.tag).copy()
        filename = node.attrs.get("filename") if node.tag == "panel" else None
        if filename:
            if filename not in panel_cache:
                panel_path = path.parent / filename
                try:
                    panel_cache[filename] = dict(ElementTree.parse(panel_path).getroot().attrib)
                except (OSError, ElementTree.ParseError):
                    panel_cache[filename] = {}
            merged.update(panel_cache[filename])
        merged.update(node.attrs)
        node.attrs = merged
        pending.extend(node.children)


def is_widget(node: Node) -> bool:
    if node.tag in NON_WIDGETS or "." in node.tag:
        return False
    return bool(
        node.tag in CONTAINERS
        or node.attrs.get("name")
        or any(key in node.attrs for key in ("left", "top", "bottom", "width", "height"))
    )


def effective_layout(node: Node) -> str:
    current: Node | None = node
    while current is not None:
        if current.attrs.get("layout"):
            return current.attrs["layout"].lower()
        current = current.parent
    return "topleft"


def local_rect(node: Node, parent: Rect, previous: Rect | None) -> Rect:
    """Model LLView::applyXUILayout in top-left coordinates.

    Missing rectangle components inherit the preceding sibling's rectangle
    (or the parent rectangle for the first child).  This detail matters for
    panels with only ``top=`` and for left_pad/top_pad chains.
    """
    size_default = previous or Rect(0, 0, parent.width, parent.height)
    # LLView seeds relative positioning at the parent's top-left when there
    # is no preceding widget.  Treat it as a zero-height virtual sibling;
    # using the parent's full height incorrectly puts first-child top_pad at
    # the bottom of the parent.
    default = previous or Rect(0, 0, parent.width, 0)
    layout_topleft = effective_layout(node) == "topleft"

    width = node.integer("width", size_default.width)
    height = node.integer("height", size_default.height)
    if "width" not in node.attrs and "left" in node.attrs and "right" in node.attrs:
        left_edge = node.integer("left")
        right_edge = node.integer("right")
        left_edge = left_edge if left_edge >= 0 else parent.width + left_edge
        right_edge = right_edge if right_edge >= 0 else parent.width + right_edge
        width = right_edge - left_edge
    if "height" not in node.attrs and "top" not in node.attrs:
        # LLView's deprecated MIN_WIDGET_HEIGHT fallback precedes inheritance.
        height = 10
    elif "height" not in node.attrs and "top" in node.attrs and "bottom" in node.attrs:
        top_edge = node.integer("top")
        bottom_edge = node.integer("bottom")
        if layout_topleft:
            top_edge = top_edge if top_edge >= 0 else parent.height + top_edge
            bottom_edge = bottom_edge if bottom_edge >= 0 else parent.height + bottom_edge
            height = bottom_edge - top_edge
        else:
            top_edge = top_edge if top_edge >= 0 else parent.height + top_edge
            bottom_edge = bottom_edge if bottom_edge >= 0 else parent.height + bottom_edge
            height = top_edge - bottom_edge

    left = default.x
    if "left" in node.attrs:
        left = node.integer("left")
        if left < 0:
            left = parent.width + left
    elif "right" in node.attrs:
        right = node.integer("right")
        right = right if right >= 0 else parent.width + right
        left = right - width
    if "left_pad" in node.attrs:
        left = default.right + node.integer("left_pad")
    elif "left_delta" in node.attrs:
        left = default.x + node.integer("left_delta")
    if "right" in node.attrs and "width" in node.attrs:
        # LLRect is over-constrained deliberately: an explicit right+width
        # wins over the non-provided left synthesized for relative layout.
        right = node.integer("right")
        right = right if right >= 0 else parent.width + right
        left = right - width

    if "top_pad" in node.attrs:
        top = default.bottom + node.integer("top_pad")
    elif "top_delta" in node.attrs:
        top = default.y + node.integer("top_delta")
    elif "bottom_delta" in node.attrs:
        top = default.bottom - height - node.integer("bottom_delta")
    elif "top" in node.attrs:
        raw_top = node.integer("top")
        if layout_topleft:
            top = raw_top if raw_top >= 0 else parent.height + raw_top
        else:
            ll_top = raw_top if raw_top >= 0 else parent.height + raw_top
            top = parent.height - ll_top
    elif "bottom" in node.attrs:
        raw_bottom = node.integer("bottom")
        raw_bottom = raw_bottom if raw_bottom >= 0 else parent.height + raw_bottom
        top = parent.height - raw_bottom - height
    elif previous is None:
        top = 0
    elif "left_pad" in node.attrs or "left_delta" in node.attrs:
        top = default.y
    else:
        top = default.bottom + VPAD
    return Rect(left, top, width, height)


def reshape_for_follows(rect: Rect, node: Node, old_size: tuple[int, int],
                        new_size: tuple[int, int]) -> None:
    """Apply LLView parent-reshape anchoring to a direct child rectangle."""
    delta_x = new_size[0] - old_size[0]
    delta_y = new_size[1] - old_size[1]
    follows = {part.strip().lower() for part in
               node.attrs.get("follows", "left|top").split("|")}
    if "all" in follows:
        follows.update({"left", "right", "top", "bottom"})
    if "left" in follows and "right" in follows:
        rect.width += delta_x
    elif "right" in follows and "left" not in follows:
        rect.x += delta_x
    elif "left" not in follows:
        rect.x += delta_x // 2
    if "top" in follows and "bottom" in follows:
        rect.height += delta_y
    elif "bottom" in follows and "top" not in follows:
        rect.y += delta_y
    elif "top" not in follows:
        rect.y += delta_y // 2


def tab_page_rect(node: Node) -> Rect:
    """Return the rectangle LLTabContainer assigns to every page."""
    assert node.rect is not None
    if node.attrs.get("hide_tabs", "false").lower() == "true":
        return Rect(1, 0, max(0, node.rect.width - 2), max(0, node.rect.height - 1))
    position = node.attrs.get("tab_position", "top").lower()
    tab_height = node.integer("tab_height", DEFAULT_TAB_HEIGHT)
    overlap = 1  # UITabCntrButtonPanelOverlap
    if position == "left":
        tab_width = node.integer("tab_width", node.integer("tab_min_width", 100))
        left = tab_width + node.integer("tab_padding_right", 0) + 2
        return Rect(left, 1, max(0, node.rect.width - left - 1), max(0, node.rect.height - 2))
    if position == "bottom":
        return Rect(1, 1, max(0, node.rect.width - 2),
                    max(0, node.rect.height - tab_height))
    top = tab_height - overlap + 1
    return Rect(1, top, max(0, node.rect.width - 2),
                max(0, node.rect.height - top - 1))


def arrange_stack(node: Node, local_children: list[tuple[Node, Rect]]) -> None:
    panels = [(child, rect) for child, rect in local_children if child.tag == "layout_panel"]
    if not panels or node.rect is None:
        return
    horizontal = node.attrs.get("orientation", "horizontal").lower() == "horizontal"
    spacing = node.integer("border_size", node.integer("panel_spacing", 3))
    total = node.rect.width if horizontal else node.rect.height
    visible_panels = [item for item in panels if item[0].attrs.get("visible", "true").lower() != "false"]
    fixed = 0
    auto: list[tuple[Node, Rect]] = []
    for child, rect in panels:
        if child not in [item[0] for item in visible_panels]:
            continue
        dimension = rect.width if horizontal else rect.height
        minimum = child.integer("min_dim", child.integer("min_width" if horizontal else "min_height", 0))
        if child.attrs.get("auto_resize", "true").lower() == "false":
            fixed += max(dimension, minimum)
        else:
            auto.append((child, rect))
            fixed += minimum
    remaining = max(0, total - fixed - spacing * max(0, len(visible_panels) - 1))
    weights = [max(1, (rect.width if horizontal else rect.height) -
                   child.integer("min_dim", child.integer("min_width" if horizontal else "min_height", 0)))
               for child, rect in auto]
    weight_total = sum(weights)
    cursor = 0
    for child, rect in panels:
        minimum = child.integer("min_dim", child.integer("min_width" if horizontal else "min_height", 0))
        if child.attrs.get("auto_resize", "true").lower() == "false":
            dimension = max(minimum, rect.width if horizontal else rect.height)
        else:
            weight = weights[auto.index((child, rect))] if (child, rect) in auto else 0
            dimension = minimum + (remaining * weight // weight_total if weight_total else 0)
        maximum = child.integer("max_dim", child.integer("max_width" if horizontal else "max_height", -1))
        if maximum >= 0:
            dimension = min(dimension, maximum)
        if horizontal:
            rect.x, rect.y, rect.width, rect.height = cursor, 0, dimension, node.rect.height
        else:
            rect.x, rect.y, rect.width, rect.height = 0, cursor, node.rect.width, dimension
        if child.attrs.get("visible", "true").lower() != "false":
            cursor += dimension + spacing


def arrange_accordion(node: Node, local_children: list[tuple[Node, Rect]]) -> None:
    if node.rect is None:
        return
    cursor = 0
    spacing = node.integer("spacing", 1)
    for child, rect in local_children:
        if child.tag != "accordion_tab":
            continue
        rect.x = 0
        rect.y = cursor
        if not rect.width:
            rect.width = node.rect.width
        cursor += rect.height + spacing


def panel_content_height(node: Node, width: int, pad: int) -> int:
    """Mirror ALScrollFit::fitPanelToChildren for a flow-grid card."""
    parent = Rect(0, 0, width, max(node.integer("height", 0), 1))
    previous: Rect | None = None
    bottom = 0
    for child in node.children:
        if not is_widget(child):
            continue
        rect = local_rect(child, parent, previous)
        previous = rect
        bottom = max(bottom, rect.bottom)
    return max(node.integer("height", 0), bottom + pad)


def arrange_flow_grid(node: Node, local_children: list[tuple[Node, Rect]]) -> None:
    if node.rect is None:
        return
    cards = [(child, rect) for child, rect in local_children if child.tag == "panel"]
    if not cards:
        return
    pad = node.integer("pad", 8)
    column_gap = node.integer("column_gap", 12)
    row_gap = node.integer("row_gap", 12)
    card_width = max(1, node.integer("card_width", 340))
    available = max(1, node.rect.width - 2 * pad)
    columns = max(1, min(node.integer("max_columns", 4),
                         (available + column_gap) // (card_width + column_gap)))
    column_width = max(1, (available - (columns - 1) * column_gap) // columns)
    bottoms = [pad] * columns
    fit_cards = node.attrs.get("fit_cards", "false").lower() == "true"
    for child, rect in cards:
        child.design_size = (rect.width, rect.height)
        height = panel_content_height(child, column_width, pad) if fit_cards else rect.height
        if child.attrs.get("visible", "true").lower() == "false":
            rect.width, rect.height = column_width, height
            continue
        column = min(range(columns), key=bottoms.__getitem__)
        rect.x = pad + column * (column_width + column_gap)
        rect.y = bottoms[column]
        rect.width, rect.height = column_width, height
        bottoms[column] += height + row_gap
    natural_height = max(bottoms) - row_gap + pad
    node.rect.height = max(node.rect.height, natural_height)


def calculate(node: Node, parent_abs: Rect | None = None) -> None:
    if parent_abs is None:
        width = node.integer("width", 0)
        height = node.integer("height", 0)
        node.rect = Rect(0, 0, width, height)
    assert node.rect is not None
    layout_width, layout_height = node.design_size or (node.rect.width, node.rect.height)
    layout_parent = Rect(0, 0, layout_width, layout_height)
    previous: Rect | None = None
    local_children: list[tuple[Node, Rect]] = []
    for child in node.children:
        if not is_widget(child):
            continue
        if node.tag == "tab_container" and child.tag == "panel":
            # Pages are constructed against the tab container, then every
            # page is assigned the same runtime page rectangle.
            design = local_rect(child, layout_parent, None)
            local = tab_page_rect(node)
            child.design_size = (design.width, design.height)
        else:
            local = local_rect(child, layout_parent, previous)
            if node.design_size is not None:
                reshape_for_follows(
                    local,
                    child,
                    (layout_width, layout_height),
                    (node.rect.width, node.rect.height),
                )
        local_children.append((child, local))
        previous = local

    if node.tag == "layout_stack":
        arrange_stack(node, local_children)
    elif node.tag == "accordion":
        arrange_accordion(node, local_children)
    elif node.tag == "flow_grid":
        arrange_flow_grid(node, local_children)

    for child, local in local_children:
        child.rect = local.absolute(node.rect)
        calculate(child, node.rect)


def visible_children(node: Node) -> list[Node]:
    return [child for child in node.children if is_widget(child) and child.rect is not None]


def participates_in_overlap(node: Node) -> bool:
    return node.attrs.get("visible", "true").lower() != "false"


def display_text(node: Node) -> str:
    return " ".join((node.text or node.attrs.get("value", "")).split())


def overlap_rect(node: Node) -> Rect:
    """Approximate painted bounds, rather than a label's hit rectangle."""
    assert node.rect is not None
    if node.tag == "check_box" and node.attrs.get("label"):
        factor = 6.5 if node.attrs.get("font") == "SansSerifSmall" else 7.5
        width = min(node.rect.width, 20 + math.ceil(len(node.attrs["label"]) * factor))
        return Rect(node.rect.x, node.rect.y, width, node.rect.height)
    if node.tag != "text":
        return node.rect
    value = display_text(node)
    if not value:
        return node.rect
    factor = 6.5 if node.attrs.get("font") == "SansSerifSmall" else 7.5
    width = min(node.rect.width, math.ceil(len(value) * factor))
    halign = node.attrs.get("halign", "left").lower()
    if halign == "right":
        left = node.rect.right - width
    elif halign == "center":
        left = node.rect.x + (node.rect.width - width) // 2
    else:
        left = node.rect.x
    return Rect(left, node.rect.y, width, node.rect.height)


def intentional_paint_overlap(first: Node, second: Node, first_rect: Rect,
                              second_rect: Rect) -> bool:
    overlap_width = min(first_rect.right, second_rect.right) - max(first_rect.x, second_rect.x)
    overlap_height = min(first_rect.bottom, second_rect.bottom) - max(first_rect.y, second_rect.y)
    if min(overlap_width, overlap_height) <= 3:
        # Adjacent controls commonly share up to three border pixels.
        return True
    tags = {first.tag, second.tag}
    if "icon" in tags:
        icon = first if first.tag == "icon" else second
        host = second if icon is first else first
        assert icon.rect is not None and host.rect is not None
        if icon.rect.inside(host.rect):
            return True
    return False


def child_overflow_axes(node: Node) -> tuple[bool, bool]:
    """Return whether direct children exceed the node on either axis."""
    if node.rect is None:
        return False, False
    children = visible_children(node)
    if not children:
        return False, False
    left = min(child.rect.x for child in children if child.rect is not None)
    top = min(child.rect.y for child in children if child.rect is not None)
    right = max(child.rect.right for child in children if child.rect is not None)
    bottom = max(child.rect.bottom for child in children if child.rect is not None)
    return left < node.rect.x or right > node.rect.right, top < node.rect.y or bottom > node.rect.bottom


def enclosed_by_scrollable_axis(node: Node, horizontal: bool) -> bool:
    """Whether an ancestor scroll document is larger than its viewport on this axis."""
    ancestor = node.parent
    while ancestor is not None:
        if ancestor.tag == "scroll_container" and ancestor.rect is not None:
            documents = visible_children(ancestor)
            if documents:
                document = documents[0]
                assert document.rect is not None
                doc_size = document.rect.width if horizontal else document.rect.height
                view_size = ancestor.rect.width if horizontal else ancestor.rect.height
                if doc_size > view_size:
                    return True
        ancestor = ancestor.parent
    return False


def descendants(node: Node) -> Iterable[Node]:
    for child in node.children:
        yield child
        yield from descendants(child)


class SourceIndex:
    """Small, lazy index used by the scroller ownership/wiring check."""

    def __init__(self, repo_root: Path):
        self.files: dict[Path, str] = {}
        self.xui_sources: dict[str, list[str]] = {}
        self.class_sources: dict[str, list[str]] = {}
        self._owner_cache: dict[tuple[str, tuple[str, ...]], list[str]] = {}
        self._visibility_cache: dict[str, set[str]] = {}
        self.all_dynamic_visibility: set[str] = set()
        source_dir = repo_root / "indra" / "newview"
        for path in source_dir.glob("*.cpp"):
            try:
                self.files[path] = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
        for source in self.files.values():
            for match in re.finditer(r"[\"']([^\"'/\\]+\.xml)[\"']", source, re.I):
                self.xui_sources.setdefault(match.group(1), []).append(source)
            for class_name in set(re.findall(r"\b([A-Za-z_]\w*)::", source)):
                self.class_sources.setdefault(class_name, []).append(source)
            aliases = dict(re.findall(
                r"\b([A-Za-z_]\w*)\s*=\s*(?:getChild|getChildView|findChild)"
                r"(?:<[^;>]+>)?\s*\(\s*[\"']([^\"']+)[\"']",
                source,
            ))
            for alias, name in aliases.items():
                if re.search(rf"\b{re.escape(alias)}\s*->\s*setVisible\s*\(", source):
                    self.all_dynamic_visibility.add(name)
            self.all_dynamic_visibility.update(re.findall(
                r"(?:getChild|getChildView|findChild)(?:<[^;>]+>)?\s*\(\s*"
                r"[\"']([^\"']+)[\"']\s*\)\s*->\s*setVisible\s*\(",
                source,
            ))
            self.all_dynamic_visibility.update(re.findall(
                r"\bchildSetVisible\s*\(\s*[\"']([^\"']+)[\"']\s*,",
                source,
            ))

    def owner_sources(self, xui_name: str, scroller_names: list[str]) -> list[str]:
        cache_key = (xui_name, tuple(scroller_names))
        if cache_key in self._owner_cache:
            return self._owner_cache[cache_key]
        direct = self.xui_sources.get(xui_name, [])
        classes: set[str] = set()
        for text in direct:
            for match in re.finditer(
                rf'["\']{re.escape(xui_name)}["\'][\s\S]{{0,500}}?build<\s*([A-Za-z_][A-Za-z0-9_:]*)',
                text,
            ):
                classes.add(match.group(1).split("::")[-1])
        owners = list(direct)
        for class_name in classes:
            owners.extend(self.class_sources.get(class_name, []))
        for text in self.files.values():
            if any(re.search(rf'["\']{re.escape(name)}["\']', text) for name in scroller_names):
                owners.append(text)
        self._owner_cache[cache_key] = owners
        return owners

    def dynamically_visible_names(self, xui_name: str) -> set[str]:
        """Names whose visibility is controlled by the owning C++ UI class."""
        if xui_name in self._visibility_cache:
            return self._visibility_cache[xui_name]
        sources = self.owner_sources(xui_name, [])
        result: set[str] = set()
        for source in sources:
            aliases: dict[str, str] = {}
            for match in re.finditer(
                r"\b([A-Za-z_]\w*)\s*=\s*(?:getChild|getChildView|findChild)"
                r"(?:<[^;>]+>)?\s*\(\s*[\"']([^\"']+)[\"']",
                source,
            ):
                aliases[match.group(1)] = match.group(2)
            for alias, name in aliases.items():
                if re.search(rf"\b{re.escape(alias)}\s*->\s*setVisible\s*\(", source):
                    result.add(name)
            for match in re.finditer(
                r"(?:getChild|getChildView|findChild)(?:<[^;>]+>)?\s*\(\s*"
                r"[\"']([^\"']+)[\"']\s*\)\s*->\s*setVisible\s*\(",
                source,
            ):
                result.add(match.group(1))
        result.update(self.all_dynamic_visibility)
        self._visibility_cache[xui_name] = result
        return result

    def fits_tabs_at_runtime(self, xui_name: str) -> bool:
        return any("installForTabs" in source for source in self.owner_sources(xui_name, []))


def scroller_wiring_findings(root: Node, path: Path, source_index: SourceIndex) -> list[Finding]:
    scrollers = [node for node in (root, *descendants(root)) if node.tag == "scroll_container"]
    if not scrollers:
        return []
    names = [node.name for node in scrollers]
    owners = source_index.owner_sources(path.name, names)
    combined = "\n".join(owners)

    # CLAUDE.md rule 9: "Any scroller that moves content without going through
    # LLScrollbar::scrollByWheel MUST call LLSliderWheel::noteScroll(this) when it
    # really moves."  Ordinary LLScrollContainer wheel handling is therefore native;
    # this check requires ALScrollFit sizing/focus wiring, never the retired WheelGuard.
    broad_fit = bool(re.search(r"(?:\.installForTabs\s*\(|ALScrollFit::fitAll\s*\()", combined))
    findings: list[Finding] = []
    for scroller in scrollers:
        named_fit = bool(
            scroller.name in combined
            and re.search(
                r"(?:ALScrollFit::(?:fitContainer|fitAll)|\.installForTabs|\.installNamed|\.fit)\s*\(",
                combined,
            )
        )
        if not (broad_fit or named_fit):
            findings.append(
                Finding(
                    "scroller-wiring",
                    scroller.line,
                    f"{scroller.name}: scroller not wired (wheel/fit)",
                )
            )
    return findings


def check_node(
    node: Node,
    allowances: set[frozenset[str]],
    findings: list[Finding],
    dynamic_visibility: set[str],
    runtime_tab_fit: bool,
) -> None:
    children = visible_children(node)

    if "bottom" in node.explicit_attrs:
        findings.append(
            Finding(
                "legacy-bottom",
                node.line,
                f"{node.name} uses legacy bottom= coordinates; use top= layout",
            )
        )

    tab_page = node.parent is not None and node.parent.tag == "tab_container"
    scroll_document = node.parent is not None and node.parent.tag == "scroll_container"
    if (
        node.tag in {"panel", "layout_panel"}
        and not scroll_document
        and not (runtime_tab_fit and tab_page)
    ):
        overflow_x, overflow_y = child_overflow_axes(node)
        unscrollable: list[str] = []
        if overflow_x and not enclosed_by_scrollable_axis(node, horizontal=True):
            unscrollable.append("width")
        if overflow_y and not enclosed_by_scrollable_axis(node, horizontal=False):
            unscrollable.append("height")
        if unscrollable:
            findings.append(
                Finding(
                    "no-scroller-overflow",
                    node.line,
                    f"{node.name}: children's union exceeds {' and '.join(unscrollable)} "
                    "without an enclosing scroll_container able to scroll that axis",
                )
            )

    if (
        node.tag == "scroll_container"
        and not runtime_tab_fit
        and children
        and children[0].tag in {"panel", "layout_panel"}
    ):
        document = children[0]
        overflow_x, overflow_y = child_overflow_axes(document)
        axes = [axis for axis, overflow in (("width", overflow_x), ("height", overflow_y)) if overflow]
        if axes:
            findings.append(
                Finding(
                    "scroll-content-overflow",
                    document.line,
                    f"{node.name}: content {document.name} is smaller than its children "
                    f"on {' and '.join(axes)} (not auto-fit)",
                )
            )

    for index, first in enumerate(children):
        assert first.rect is not None
        if not participates_in_overlap(first) or first.name in dynamic_visibility:
            continue
        for second in children[index + 1 :]:
            assert second.rect is not None
            if not participates_in_overlap(second) or second.name in dynamic_visibility:
                continue
            if node.tag == "tab_container" and first.tag == second.tag == "panel":
                # LLTabContainer makes exactly one page visible at a time.
                continue
            if first.tag == "view_border" or second.tag == "view_border":
                continue
            first_paint = overlap_rect(first)
            second_paint = overlap_rect(second)
            if not first_paint.intersects(second_paint):
                continue
            if intentional_paint_overlap(first, second, first_paint, second_paint):
                continue
            if frozenset((first.name, second.name)) in allowances:
                continue
            findings.append(
                Finding(
                    "overlap",
                    min(first.line, second.line),
                    f"{first.name} {first.rect} intersects {second.name} {second.rect}",
                )
            )

    if node.rect is not None:
        for child in children:
            assert child.rect is not None
            child_is_scroll_document = node.tag == "scroll_container" and child.tag in {
                "panel", "layout_panel", "flow_grid"
            }
            scroll_document_clipped = child_is_scroll_document and (
                child.rect.x < node.rect.x
                or child.rect.y < node.rect.y
                or child.rect.right > node.rect.right
            )
            runtime_fitted_child = runtime_tab_fit and tab_page
            inside_scroll_document = scroll_document
            if (
                not runtime_fitted_child
                and not inside_scroll_document
                and (scroll_document_clipped or not child_is_scroll_document)
                and not child.rect.inside(node.rect)
            ):
                findings.append(
                    Finding(
                        "clipped",
                        child.line,
                        f"{child.name} {child.rect} extends outside {node.name} {node.rect}",
                    )
                )

    if node.tag == "text" and node.rect is not None:
        value = display_text(node)
        wraps = node.attrs.get("wrap", node.attrs.get("word_wrap", "false")).lower() == "true"
        if value and not wraps:
            factor = 6.5 if node.attrs.get("font") == "SansSerifSmall" else 7.5
            estimated = math.ceil(max(len(line) for line in value.splitlines()) * factor)
            if estimated > node.rect.width:
                findings.append(
                    Finding(
                        "text-width",
                        node.line,
                        f"{node.name} {node.rect} needs about {estimated}px for "
                        f"{value!r}, has {node.rect.width}px",
                    )
                )

    for child in children:
        check_node(child, allowances, findings, dynamic_visibility, runtime_tab_fit)


def check_file(path: Path, source_index: SourceIndex | None = None) -> list[Finding]:
    root, allowances = parse_xml(path)
    calculate(root)
    if root.rect is None or root.rect.width <= 0 or root.rect.height <= 0:
        return []
    findings: list[Finding] = []
    # Visibility names are indexed once across the viewer sources.  XUI names
    # are effectively global child identifiers in these owner classes, and
    # this also covers inline specialist panels (for example LLPanelVolume in
    # floater_tools.xml) whose class is not named in the XML.
    dynamic_visibility = source_index.all_dynamic_visibility if source_index is not None else set()
    runtime_tab_fit = source_index.fits_tabs_at_runtime(path.name) if source_index is not None else False
    check_node(root, allowances, findings, dynamic_visibility, runtime_tab_fit)
    if source_index is not None:
        findings.extend(scroller_wiring_findings(root, path, source_index))
    findings.sort(key=lambda item: (item.line, item.kind, item.message))
    return findings


def default_xui_dir() -> Path:
    return Path("indra/newview/skins/default/xui/en")


def changed_xui_files(revision: str) -> list[Path]:
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMR", revision, "--"],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or str(exc)
        raise ValueError(f"git diff failed: {detail.strip()}") from exc
    return sorted(
        Path(name)
        for name in result.stdout.splitlines()
        if name.lower().endswith(".xml") and "/xui/" in name.replace("\\", "/")
    )


def selected_files(path: str | None, all_files: bool, changed: str | None) -> Iterable[Path]:
    if all_files:
        yield from sorted(default_xui_dir().glob("*.xml"))
    elif changed:
        yield from changed_xui_files(changed)
    elif path:
        yield Path(path)


def run_selftest(json_output: bool) -> int:
    fixture_dir = Path(__file__).with_name("xui_fixtures")
    cases = (
        ("joystick_broken.xml", True),
        ("joystick_current.xml", False),
    )
    reports: list[dict[str, object]] = []
    passed = True
    for filename, expects_findings in cases:
        path = fixture_dir / filename
        try:
            findings = check_file(path)
            overlap_count = sum(item.kind == "overlap" for item in findings)
            case_passed = bool(findings) == expects_findings
            if expects_findings:
                case_passed = case_passed and overlap_count > 0
            error = None
        except ValueError as exc:
            findings = []
            overlap_count = 0
            case_passed = False
            error = str(exc)
        passed = passed and case_passed
        report = {
            "fixture": filename,
            "expected": "findings" if expects_findings else "0 findings",
            "count": len(findings),
            "overlaps": overlap_count,
            "passed": case_passed,
        }
        if error is not None:
            report["error"] = error
        reports.append(report)
        if not json_output:
            detail = f"{len(findings)} finding(s), {overlap_count} overlap(s)"
            print(f"{'PASS' if case_passed else 'FAIL'} {filename}: {detail}")
    if json_output:
        print(json.dumps({"passed": passed, "fixtures": reports}, indent=2))
    elif passed:
        print("Self-test passed: broken joystick detected; current joystick clean")
    return 0 if passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("file", nargs="?", help="XUI XML file to check")
    choice.add_argument("--all", action="store_true", help="check default English XUI files")
    choice.add_argument("--changed", metavar="GIT_REV", help="check XUI files changed since GIT_REV")
    choice.add_argument("--selftest", action="store_true", help="run bundled regression fixtures")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    if args.selftest:
        return run_selftest(args.json)

    total = 0
    failed_files = 0
    reports: list[dict[str, object]] = []
    source_index = SourceIndex(Path.cwd())
    try:
        files = list(selected_files(args.file, args.all, args.changed))
    except ValueError as exc:
        if args.json:
            print(json.dumps({"error": str(exc)}))
        else:
            print(str(exc), file=sys.stderr)
        return 2
    for path in files:
        try:
            findings = check_file(path, source_index)
        except ValueError as exc:
            if args.json:
                print(json.dumps({"error": f"{path}: XML error: {exc}"}))
            else:
                print(f"{path}: XML error: {exc}", file=sys.stderr)
            return 2
        counts: dict[str, int] = {}
        for finding in findings:
            counts[finding.kind] = counts.get(finding.kind, 0) + 1
            if not args.json:
                print(f"{path}:{finding.line}: {finding.kind}: {finding.message}")
        reports.append(
            {
                "file": path.as_posix(),
                "count": len(findings),
                "counts": counts,
                "findings": [finding.as_dict() for finding in findings],
            }
        )
        if not args.json:
            detail = ", ".join(f"{count} {kind}" for kind, count in sorted(counts.items()))
            print(f"{path}: {len(findings)} finding(s)" + (f" ({detail})" if detail else ""))
        total += len(findings)
        failed_files += bool(findings)
    if args.json:
        print(json.dumps({"total": total, "failed_files": failed_files, "files": reports}, indent=2))
    elif args.all:
        print(f"Checked default English XUI: {total} finding(s) in {failed_files} file(s)")
    elif args.changed:
        print(f"Checked {len(files)} changed XUI file(s): {total} finding(s) in {failed_files} file(s)")
    return 1 if total else 0


if __name__ == "__main__":
    raise SystemExit(main())
