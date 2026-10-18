#!/usr/bin/env python3
"""Check XUI geometry, scrolling, and static scroller wiring.

Usage::

    python scripts/code_tools/xui_layout_check.py path/to/file.xml
    python scripts/code_tools/xui_layout_check.py --all
    python scripts/code_tools/xui_layout_check.py --changed <git-rev> [--json]
    python scripts/code_tools/xui_layout_check.py --selftest

The checker models LLView's top-left and legacy bottom-left positioning.  It
also accounts for tab headers, scroll documents, layout stacks, accordion
content, and flow grids with ``pack="rows"``.  Director kit tags (``kit_*`` and
``light_rig_map``) are runtime-laid-out: their interiors are not positioned,
and their hosted control geometry is checked as ``kit-geometry``. Nested
action rows, multiline sibling resizing and label/value first-line alignment
also have a fail-closed runtime contract check against ALKitRow source.  A legacy
``kit_page`` is checked like the v1 leaf it replaces.  Resource-only XUI
documents with no root width/height are ignored.
Intentional sibling layering must be documented in the XML with::

    <!-- xui-check: allow-overlap first_widget second_widget -->
    <!-- xui-check: exclusive alternate_a alternate_b alternate_c -->
    <!-- xui-check: overlay empty_hint list_widget -->
    <!-- xui-check: independent conditional_a conditional_b -->

Exit status is 1 when any non-baselined finding is reported, 0 otherwise, and
2 for command-line or XML errors.  Generated legacy Director pages may carry
only the exact v1-inherited keys listed in gate_policy.json; their static
selftest makes additions, removals, or drift fatal.  Kit row labels and page
master labels are measured with the viewer's Kit12 font file; an overflow must
be shortened.  It uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import decimal
import functools
import json
import math
import re
import struct
import subprocess
import tempfile
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
EXCLUSIVE_RE = re.compile(r"^\s*xui-check:\s*exclusive\s+(.+?)\s*$", re.I)
OVERLAY_RE = re.compile(r"^\s*xui-check:\s*overlay\s+(\S+)\s+(\S+)\s*$", re.I)
INDEPENDENT_RE = re.compile(r"^\s*xui-check:\s*independent\s+(\S+)\s+(\S+)\s*$", re.I)
NON_STRETCH_TAGS = {"button", "icon_button", "icon", "image", "check_box", "spinner", "spin_ctrl"}


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
    # Authored tag before legacy kit_page is retagged to "panel" for v1 leaf semantics.
    source_tag: str = ""
    legacy_kit: bool = False
    # True for the accordion_tab child that LLAccordionCtrlTab reshapes to the
    # fit_panel rectangle (see accordion_fit_rect).
    fit_panel_container: bool = False

    def __post_init__(self) -> None:
        if not self.source_tag:
            self.source_tag = self.tag

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


@dataclass
class OverlapPolicy:
    allowances: set[frozenset[str]] = field(default_factory=set)
    exclusive_groups: list[set[str]] = field(default_factory=list)
    overlays: set[frozenset[str]] = field(default_factory=set)
    independents: set[frozenset[str]] = field(default_factory=set)

    def excluded(self, pair: frozenset[str]) -> bool:
        return (pair in self.allowances or pair in self.overlays
                or any(pair.issubset(group) for group in self.exclusive_groups))


def parse_xml(path: Path) -> tuple[Node, OverlapPolicy]:
    parser = expat.ParserCreate()
    stack: list[Node] = []
    root: Node | None = None
    policy = OverlapPolicy()

    def start(tag: str, attrs: dict[str, str]) -> None:
        nonlocal root
        node = Node(tag, dict(attrs), parser.CurrentLineNumber,
                    explicit_attrs=set(attrs))
        if tag == "kit_page" and attrs.get("legacy", "false").lower() == "true":
            # A legacy kit_page is the v1 leaf it replaces: same panel rules.
            node.legacy_kit = True
            node.tag = "panel"
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
            policy.allowances.add(frozenset(match.groups()))
        exclusive = EXCLUSIVE_RE.match(data)
        if exclusive:
            names = set(exclusive.group(1).split())
            if len(names) >= 2:
                policy.exclusive_groups.append(names)
        overlay = OVERLAY_RE.match(data)
        if overlay:
            policy.overlays.add(frozenset(overlay.groups()))
        independent = INDEPENDENT_RE.match(data)
        if independent:
            policy.independents.add(frozenset(independent.groups()))

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
    return root, policy


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
                panel_path = next(
                    (candidate / filename for candidate in (path.parent, *path.parents)
                     if (candidate / filename).is_file()),
                    path.parent / filename,
                )
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


ACCORDION_HEADER_HEIGHT = 25  # llaccordionctrltab.cpp HEADER_HEIGHT


def _xml_flag(node: Node, key: str, default: bool) -> bool:
    raw = node.attrs.get(key)
    if raw is None:
        return default
    return raw.strip().lower() not in {"false", "0"}


def accordion_fit_child(tab: Node) -> Node | None:
    """Return the child LLAccordionCtrlTab resizes as its fit_panel, if any.

    llaccordionctrltab.cpp findContainerView() (~:645) picks the first visible
    child that is not the header.  reshape() (~:426) returns early when the tab
    is collapsed (display_children = XML "expanded"), and fit_panel (default
    true, ~:354) selects adjustContainerPanel(childRect) (~:977) instead of the
    scrollbar path.
    """
    if not _xml_flag(tab, "expanded", True) or not _xml_flag(tab, "fit_panel", True):
        return None
    for child in tab.children:
        if is_widget(child) and child.attrs.get("visible", "true").lower() != "false":
            return child
    return None


def accordion_fit_rect(tab: Node) -> Rect:
    """Return the fit_panel rectangle in tab-local top-left coordinates.

    llaccordionctrltab.cpp reshape() (~:440-446) and adjustContainerPanel()
    (~:960-962):
        left   = padding_left
        top    = getHeaderHeight() + padding_top
        width  = tab_width - padding_left - padding_right
        height = tab_height - getHeaderHeight() - padding_top - padding_bottom
    getHeaderHeight() (~:674) is HEADER_HEIGHT (25) when header_visible (default
    true), else 0.  Paddings default to 2 (Params ~:341-344).  The header_height
    param is not read by the layout code, so the constant is used.  With the
    defaults the fit panel is the tab height minus 29, starting 27 px down.
    """
    assert tab.rect is not None
    header = ACCORDION_HEADER_HEIGHT if _xml_flag(tab, "header_visible", True) else 0
    left = tab.integer("padding_left", 2)
    right = tab.integer("padding_right", 2)
    top = tab.integer("padding_top", 2)
    bottom = tab.integer("padding_bottom", 2)
    return Rect(left, header + top,
                tab.rect.width - left - right,
                tab.rect.height - header - top - bottom)


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
        if "height" not in child.explicit_attrs and not _xml_flag(child, "expanded", True):
            # A collapsed tab is header-only (llaccordionctrltab.cpp ~:407).
            rect.height = ACCORDION_HEADER_HEIGHT
        fit = accordion_fit_child(child) if "height" not in child.explicit_attrs else None
        if fit is not None:
            # An unauthored tab height is not known to the checker; at runtime
            # the tab tracks its fit panel: max(panel height, 10) + header +
            # padding_top + padding_bottom (llaccordionctrltab.cpp notifyParent
            # "size_changes", ~:770).
            top_pad = child.integer("padding_top", 2)
            bottom_pad = child.integer("padding_bottom", 2)
            header = ACCORDION_HEADER_HEIGHT if _xml_flag(child, "header_visible", True) else 0
            rect.height = max(fit.integer("height", 0), 10) + header + top_pad + bottom_pad
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


def fitted_flow_card_width(node: Node) -> int:
    """Return the width ALFlowGrid assigns to every card at runtime."""
    assert node.rect is not None
    pad = node.integer("pad", 8)
    column_gap = node.integer("column_gap", 12)
    card_width = max(1, node.integer("card_width", 340))
    available = max(1, node.rect.width - 2 * pad)
    columns = max(1, min(node.integer("max_columns", 4),
                         (available + column_gap) // (card_width + column_gap)))
    return max(1, (available - (columns - 1) * column_gap) // columns)


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
    column_width = fitted_flow_card_width(node)
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


KIT_INDEX_NAME = "director_kit_index.xml"
GATE_POLICY = Path(__file__).with_name("director_kit") / "gate_policy.json"
KIT_HOSTING_TAGS = {"kit_row", "kit_preset_host", "kit_popover"}
POSITION_ATTRS = ("left", "top", "bottom", "follows", "layout")
GEOMETRY_ATTRS = POSITION_ATTRS + ("width", "height")


def is_kit_owned(node: Node) -> bool:
    """Kit-runtime element whose size and position the kit decides at runtime.

    ``kit_shell`` is excluded: the parent positions it like a v1 widget, but
    its interior is runtime-laid-out.  A legacy kit_page is a v1 leaf panel.
    """
    if node.legacy_kit or node.source_tag == "kit_shell":
        return False
    return node.source_tag.startswith("kit_") or node.source_tag == "light_rig_map"


def arrange_rows_grid(node: Node) -> list[tuple[Node, Rect]]:
    """Model ALFlowGrid ``pack="rows"``: row-major, top-aligned, hidden items skipped.

    Kit-owned items have a runtime height (ALFlowGridItem::heightForWidth), so
    they keep their column x and width but get zero height.  After a row that
    holds one, later rows have an unknown top and are placed at the grid pad
    with zero height, so they cannot create false overlaps.
    """
    assert node.rect is not None
    pad = node.integer("pad", 8)
    column_gap = node.integer("column_gap", 12)
    row_gap = node.integer("row_gap", 12)
    column_width = fitted_flow_card_width(node)
    available = max(1, node.rect.width - 2 * pad)
    card_width = max(1, node.integer("card_width", 340))
    columns = max(1, min(node.integer("max_columns", 4),
                         (available + column_gap) // (card_width + column_gap)))
    shown = [child for child in node.children
             if is_widget(child) and child.attrs.get("visible", "true").lower() != "false"]
    result: list[tuple[Node, Rect]] = [
        (child, Rect(pad, pad, 0, 0))
        for child in node.children
        if is_widget(child) and child.attrs.get("visible", "true").lower() == "false"
    ]
    top = pad
    top_known = True
    for start in range(0, len(shown), columns):
        row = shown[start:start + columns]
        heights: list[int | None] = []
        for child in row:
            heights.append(None if is_kit_owned(child) else local_rect(
                child, Rect(0, 0, column_width, max(node.integer("height", 0), 1)), None).height)
        for column, (child, height) in enumerate(zip(row, heights)):
            x = pad + column * (column_width + column_gap)
            if top_known:
                result.append((child, Rect(x, top, column_width, height or 0)))
            else:
                result.append((child, Rect(x, pad, column_width, 0)))
        if top_known:
            if all(height is not None for height in heights):
                top += max((height for height in heights if height is not None), default=0) + row_gap
            else:
                top_known = False
    if shown and top_known:
        node.rect.height = max(node.rect.height, top - row_gap + pad)
    return result


def kit_geometry_findings(root: Node) -> list[Finding]:
    """Kit rules from the Director kit spec, section 6.3 (kit-geometry).

    * A kit tag may not carry size or position attributes (the file root,
      the kit_shell, and direct children of kit_bar, which may carry width and
      height, are exempt).
    * A hosted control (direct child of kit_row, kit_preset_host or kit_popover,
      or any widget inside kit_shell that is not under another kit tag) may not
      carry size or position attributes.
    * Legacy kit_page takes width and height only.
    Kit-owned subtrees are not laid out.  Checks use authored attributes only,
    so widget-default merges cannot create findings.
    """
    findings: list[Finding] = []

    def check(node: Node, attrs: tuple[str, ...]) -> None:
        bad = [key for key in attrs if key in node.explicit_attrs]
        if bad:
            findings.append(Finding(
                "kit-geometry",
                node.line,
                f"{node.name}: <{node.source_tag}> sets {', '.join(bad)}; "
                "kit-owned geometry comes from the kit layout",
            ))

    def visit(node: Node, inherited: tuple[str, ...] | None, shell: bool) -> None:
        if "." in node.tag:
            return
        if node.parent is None:
            own = None
        elif node.legacy_kit:
            own = POSITION_ATTRS
        elif is_kit_owned(node):
            own = POSITION_ATTRS if node.parent.source_tag == "kit_bar" else GEOMETRY_ATTRS
        else:
            own = inherited
        if own is not None and (node.legacy_kit or is_kit_owned(node)
                                or (is_widget(node) and node.tag not in CONTAINERS)):
            check(node, own)
        for child in node.children:
            if node.legacy_kit:
                visit(child, None, False)
            elif node.source_tag == "kit_shell" or (shell and not is_kit_owned(node)):
                visit(child, GEOMETRY_ATTRS, True)
            elif is_kit_owned(node) and node.source_tag == "kit_bar":
                visit(child, POSITION_ATTRS, False)
            elif is_kit_owned(node) and node.source_tag in KIT_HOSTING_TAGS:
                visit(child, GEOMETRY_ATTRS, False)
            else:
                visit(child, None, False)

    visit(root, None, False)
    return findings


def finalize_findings(findings: list[Finding]) -> list[Finding]:
    # The wide pass may reproduce a width-independent finding.  Rectangles are
    # deliberately removed from the identity while widget names remain.
    unique: dict[tuple[str, int, str], Finding] = {}
    for finding in findings:
        normalized = re.sub(r"\(-?\d+,-?\d+,-?\d+,-?\d+\)", "(<rect>)", finding.message)
        unique.setdefault((finding.kind, finding.line, normalized), finding)
    result = list(unique.values())
    result.sort(key=lambda item: (item.line, item.kind, item.message))
    return result


def finding_subject(finding: Finding) -> str:
    """Widget/scroller identity used when comparing generated and v1 pages."""
    return finding.message.split(" ", 1)[0].rstrip(":")


def finding_key(filename: str, finding: Finding) -> str:
    """Exact legacy finding identity, including authored/calculated geometry.

    The message is intentionally not rectangle-normalized here: growth of an
    already-baselined widget must drift the key instead of inheriting the old
    widget's allowance.
    """
    return f"{filename}|{finding.kind}|{finding.message}"


def legacy_v1_baseline() -> set[str]:
    try:
        policy = json.loads(GATE_POLICY.read_text(encoding="utf-8"))
        entries = policy["xui_legacy_v1_findings"]
    except (OSError, KeyError, json.JSONDecodeError, TypeError) as exc:
        raise ValueError(f"invalid legacy XUI baseline in {GATE_POLICY}: {exc}") from exc
    if not isinstance(entries, list) or not all(isinstance(item, str) for item in entries):
        raise ValueError("xui_legacy_v1_findings must be a list of exact string keys")
    if len(entries) != len(set(entries)) or any(any(ch in item for ch in "*?[]") for item in entries):
        raise ValueError("xui_legacy_v1_findings must contain unique exact keys (no globs)")
    return set(entries)


def is_legacy_page(path: Path) -> bool:
    try:
        root = ElementTree.parse(path).getroot()
    except (OSError, ElementTree.ParseError):
        return False
    kit = root if root.tag == "kit_page" else root.find(".//kit_page")
    return kit is not None and kit.get("legacy", "false").lower() == "true"


def calculate(node: Node, parent_abs: Rect | None = None, root_width: int | None = None) -> None:
    if parent_abs is None:
        authored_width = node.integer("width", 0)
        width = root_width if root_width is not None else authored_width
        height = node.integer("height", 0)
        node.rect = Rect(0, 0, width, height)
        if root_width is not None:
            node.design_size = (authored_width, height)
    assert node.rect is not None
    if is_kit_owned(node) or node.source_tag == "kit_shell":
        # Runtime-laid-out: the kit positions everything inside these nodes.
        return
    if node.tag == "flow_grid" and node.attrs.get("pack", "masonry").lower() == "rows":
        for child, local in arrange_rows_grid(node):
            child.rect = local.absolute(node.rect)
            calculate(child, node.rect)
        return
    layout_width, layout_height = node.design_size or (node.rect.width, node.rect.height)
    layout_parent = Rect(0, 0, layout_width, layout_height)
    previous: Rect | None = None
    local_children: list[tuple[Node, Rect]] = []
    fit_child = accordion_fit_child(node) if node.tag == "accordion_tab" else None
    for child in node.children:
        if not is_widget(child) or is_kit_owned(child):
            continue
        if node.tag == "tab_container" and child.tag == "panel":
            # Pages are constructed against the tab container, then every
            # page is assigned the same runtime page rectangle.
            design = local_rect(child, layout_parent, None)
            local = tab_page_rect(node)
            child.design_size = (design.width, design.height)
        elif child is fit_child:
            # The authored panel is laid out at its XML size, then the tab
            # reshapes it to the fit rectangle; follows= anchoring applies
            # through the design_size path below (LLView::reshape).
            design = local_rect(child, layout_parent, previous)
            child.design_size = (design.width, design.height)
            child.fit_panel_container = True
            local = accordion_fit_rect(node)
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


def is_conditional(node: Node, dynamic_visibility: set[str]) -> bool:
    return (node.attrs.get("visible", "true").lower() == "false"
            or node.name in dynamic_visibility)


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
    for decoration, other in ((first, second), (second, first)):
        if decoration.tag in {"icon", "image"}:
            assert decoration.rect is not None and other.rect is not None
            # Small decoration painted inside its host, or a containing image
            # painted under another widget, is not a competing control.
            if decoration.rect.inside(other.rect) or other.rect.inside(decoration.rect):
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


def dynamic_visibility_names(source: str) -> set[str]:
    """Map C++ visibility calls, including setVisibleIfChanged member aliases."""
    def extract(scoped_source: str) -> set[str]:
        aliases = dict(re.findall(
            r"\b([A-Za-z_]\w*)\s*=\s*(?:getChild|getChildView|findChild)"
            r"(?:<[^;>]+>)?\s*\(\s*[\"']([^\"']+)[\"']",
            scoped_source,
        ))
        scoped_result: set[str] = set()
        for alias, name in aliases.items():
            if (re.search(rf"\b{re.escape(alias)}\s*->\s*setVisible\s*\(", scoped_source)
                    or re.search(
                        rf"\bsetVisibleIfChanged\s*\(\s*{re.escape(alias)}\s*,",
                        scoped_source,
                    )):
                scoped_result.add(name)
        scoped_result.update(re.findall(
            r"(?:getChild|getChildView|findChild)(?:<[^;>]+>)?\s*\(\s*"
            r"[\"']([^\"']+)[\"']\s*\)\s*->\s*setVisible\s*\(",
            scoped_source,
        ))
        scoped_result.update(re.findall(
            r"\bchildSetVisible\s*\(\s*[\"']([^\"']+)[\"']\s*,",
            scoped_source,
        ))
        return scoped_result

    # Member assignment and setVisibleIfChanged may occur in different
    # methods, but must belong to the same C++ class.  Group method spans by
    # their Class::method qualifier before resolving aliases.
    method_matches = list(re.finditer(
        r"\b([A-Za-z_]\w*)::[~A-Za-z_]\w*\s*\([^;{}]*\)\s*(?:const\s*)?\{",
        source,
    ))
    if not method_matches:
        return extract(source)
    class_sources: dict[str, list[str]] = {}
    for index, match in enumerate(method_matches):
        end = method_matches[index + 1].start() if index + 1 < len(method_matches) else len(source)
        class_sources.setdefault(match.group(1), []).append(source[match.start():end])
    result: set[str] = set()
    for chunks in class_sources.values():
        result.update(extract("\n".join(chunks)))
    return result


class SourceIndex:
    """Small, lazy index used by the scroller ownership/wiring check."""

    def __init__(self, repo_root: Path):
        self.files: dict[Path, str] = {}
        self.xui_sources: dict[str, list[str]] = {}
        self.class_sources: dict[str, list[str]] = {}
        self.panel_classes: dict[str, set[str]] = {}
        self.xui_parents: dict[str, set[str]] = {}
        self.lazy_page_files: set[str] = set()
        self._owner_cache: dict[tuple[str, tuple[str, ...]], list[str]] = {}
        self._visibility_cache: dict[str, set[str]] = {}
        self.all_dynamic_visibility: set[str] = set()
        source_dir = repo_root / "indra" / "newview"
        self.source_dir = source_dir
        xui_dir = source_dir / "skins" / "default" / "xui" / "en"
        for xui_path in xui_dir.glob("*.xml"):
            try:
                xui_root = ElementTree.parse(xui_path).getroot()
            except (OSError, ElementTree.ParseError):
                continue
            for node in xui_root.iter():
                child = node.get("page_file")
                if child:
                    self.xui_parents.setdefault(child, set()).add(xui_path.name)
                    if node.tag == "kit_lazy_page":
                        self.lazy_page_files.add(child)
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
            for match in re.finditer(
                r"LLPanelInjector\s*<\s*([A-Za-z_]\w*)\s*>[\s\S]{0,160}?"
                r"\(\s*[\"']([^\"']+)[\"']\s*\)",
                source,
            ):
                self.panel_classes.setdefault(match.group(2), set()).add(match.group(1))
            self.all_dynamic_visibility.update(dynamic_visibility_names(source))

    def owner_sources(self, xui_name: str, scroller_names: list[str]) -> list[str]:
        cache_key = (xui_name, tuple(scroller_names))
        if cache_key in self._owner_cache:
            return self._owner_cache[cache_key]
        owner_names = {xui_name}
        pending = [xui_name]
        while pending:
            for parent in self.xui_parents.get(pending.pop(), set()):
                if parent not in owner_names:
                    owner_names.add(parent)
                    pending.append(parent)
        direct = [source for name in owner_names for source in self.xui_sources.get(name, [])]
        classes: set[str] = set()
        for text in direct:
            for name in owner_names:
                for match in re.finditer(
                    rf'["\']{re.escape(name)}["\'][\s\S]{{0,500}}?build<\s*([A-Za-z_][A-Za-z0-9_:]*)',
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

    def dynamically_visible_names(self, xui_name: str, panel_class: str = "") -> set[str]:
        """Names whose visibility is controlled by the owning C++ UI class."""
        cache_key = f"{xui_name}\0{panel_class}"
        if cache_key in self._visibility_cache:
            return self._visibility_cache[cache_key]
        sources = self.owner_sources(xui_name, [])
        for cpp_class in self.panel_classes.get(panel_class, set()):
            sources.extend(self.class_sources.get(cpp_class, []))
        result: set[str] = set()
        for source in sources:
            result.update(dynamic_visibility_names(source))
        # Child names are effectively global in XUI.  Preserve the checker's
        # established cross-owner set for inline specialist panels whose C++
        # class is not declared on their XML root.
        result.update(self.all_dynamic_visibility)
        self._visibility_cache[cache_key] = result
        return result

    def fits_tabs_at_runtime(self, xui_name: str) -> bool:
        if xui_name in self.lazy_page_files:
            lazy = self.files.get(self.source_dir / "alkitlazypage.cpp", "")
            fit = self.files.get(self.source_dir / "alscrollfit.cpp", "")
            return ".adoptPage(" in lazy and "Controller::adoptPage(" in fit
        return any("installForTabs" in source for source in self.owner_sources(xui_name, []))

    def copies_fit_grid_height(self, panel_class: str, grid_names: list[str]) -> bool:
        """Whether the registered panel reshape copies its fit grid's height."""
        for cpp_class in self.panel_classes.get(panel_class, set()):
            for source in self.class_sources.get(cpp_class, []):
                if not any(re.search(rf'[\"\']{re.escape(name)}[\"\']', source)
                           for name in grid_names):
                    continue
                if (re.search(rf"\b{re.escape(cpp_class)}::reshape\s*\(", source)
                        and re.search(r"getRect\s*\(\s*\)\s*\.\s*getHeight\s*\(", source)
                        and re.search(r"\bsetRect\s*\(", source)):
                    return True
        return False


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
    lazy_hosted = path.name in source_index.lazy_page_files
    broad_fit = (source_index.fits_tabs_at_runtime(path.name) if lazy_hosted else bool(
        re.search(r"(?:\.installForTabs\s*\(|ALScrollFit::fitAll\s*\()", combined)))
    findings: list[Finding] = []
    for scroller in scrollers:
        named_fit = not lazy_hosted and bool(
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
    overlap_policy: OverlapPolicy,
    findings: list[Finding],
    dynamic_visibility: set[str],
    runtime_tab_fit: bool,
    copies_fit_grid_height: bool,
    static_rules: bool = True,
) -> None:
    children = visible_children(node)
    # A class-backed shared panel may use a direct fit_cards grid as its
    # runtime-sized scroll document.  The grid owns its packed height and the
    # panel class mirrors that height to its wrapper (panel_cine_light_rig is
    # the in-tree example).  Do not demand a hand-authored wrapper height;
    # still recurse into the grid/cards so fitted-width clipping is checked.
    runtime_fitted_flow_wrapper = (
        node.parent is None
        and bool(node.attrs.get("class"))
        and copies_fit_grid_height
        and any(child.tag == "flow_grid"
                and child.attrs.get("fit_cards", "false").lower() == "true"
                for child in children)
    )

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
        # A fit_panel accordion container has no scroller at runtime; its
        # bottom overflow is reported once, as accordion-clip, by the parent
        # clipped loop below.
        if (overflow_y and not runtime_fitted_flow_wrapper and not node.fit_panel_container
                and not enclosed_by_scrollable_axis(node, horizontal=False)):
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
        for second in children[index + 1 :]:
            assert second.rect is not None
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
            pair = frozenset((first.name, second.name))
            if overlap_policy.excluded(pair):
                continue
            if (is_conditional(first, dynamic_visibility)
                    and is_conditional(second, dynamic_visibility)
                    and pair not in overlap_policy.independents):
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
            runtime_fitted_flow_child = runtime_fitted_flow_wrapper and child.tag == "flow_grid"
            inside_scroll_document = scroll_document
            outside_x = child.rect.x < node.rect.x or child.rect.right > node.rect.right
            outside_y = child.rect.y < node.rect.y or child.rect.bottom > node.rect.bottom
            if runtime_fitted_flow_child:
                # The class copies only the packed grid HEIGHT.  Horizontal
                # overflow remains a real finding.
                outside_y = False
            if node.tag == "accordion":
                # LLAccordionCtrl owns a vertical scrollbar (llaccordionctrl.cpp
                # ~:246-249), so stacked tabs past its height scroll.  Width
                # overflow is still clipped.
                outside_y = False
            if node.tag == "accordion_tab" and not _xml_flag(node, "expanded", True):
                # A collapsed tab hides its panel (llaccordionctrltab.cpp
                # postBuild ~:735 setVisible(mDisplayChildren)); nothing paints.
                outside_x = outside_y = False
            if (
                not runtime_fitted_child
                and not inside_scroll_document
                and (scroll_document_clipped or not child_is_scroll_document)
                and (outside_x or outside_y)
            ):
                if node.fit_panel_container and node.parent is not None:
                    findings.append(
                        Finding(
                            "accordion-clip",
                            child.line,
                            f"{child.name} {child.rect} extends outside fit_panel {node.name} "
                            f"{node.rect}: accordion_tab {node.parent.name} height "
                            f"{node.parent.rect.height} leaves {node.rect.height} after the "
                            "header and padding (llaccordionctrltab.cpp reshape)",
                        )
                    )
                else:
                    findings.append(
                        Finding(
                            "clipped",
                            child.line,
                            f"{child.name} {child.rect} extends outside {node.name} {node.rect}",
                        )
                    )

    if static_rules and node.rect is not None:
        flags = {part.strip().lower() for part in node.attrs.get("follows", "left|top").split("|")}
        if "all" in flags:
            flags.update({"left", "right", "top", "bottom"})
        fit_card_child = bool(
            node.parent is not None
            and node.parent.tag in {"panel", "layout_panel"}
            and node.parent.parent is not None
            and node.parent.parent.tag == "flow_grid"
            and node.parent.parent.attrs.get("fit_cards", "false").lower() == "true"
        )
        invalid_spinner_stretch = (
            node.tag in {"spinner", "spin_ctrl"}
            and {"left", "right"}.issubset(flags)
        )
        invalid_card_stretch = (
            fit_card_child
            and node.tag in {"button", "icon_button", "icon", "image", "check_box"}
            and {"left", "right"}.issubset(flags)
        )
        if invalid_spinner_stretch or invalid_card_stretch:
            findings.append(
                Finding("invalid-stretch", node.line,
                        f"{node.name}: {node.tag} has unsupported horizontal stretch")
            )

    if static_rules and node.tag in {"spinner", "spin_ctrl"} and node.rect is not None:
        digits = max(0, node.integer("decimal_digits", 3))
        increment = node.attrs.get("increment")
        if increment is not None:
            try:
                normalized = decimal.Decimal(increment).normalize()
                required_digits = max(0, -normalized.as_tuple().exponent)
            except decimal.InvalidOperation:
                required_digits = 0
            if required_digits > digits:
                findings.append(
                    Finding("spinner-increment-precision", node.line,
                            f"{node.name} increment {increment} needs {required_digits} "
                            f"decimal digits, but decimal_digits is {digits}")
                )
        formatted: list[str] = []
        for key in ("min_val", "max_val"):
            try:
                formatted.append(f"{float(node.attrs.get(key, 0)):.{digits}f}")
            except ValueError:
                formatted.append(node.attrs.get(key, ""))
        endpoint = max(formatted, key=len)
        label_width = node.integer("label_width", 0) if node.attrs.get("label") else 0
        editor_width = node.rect.width - label_width - 17
        needed = math.ceil(len(endpoint) * 6.5)
        if editor_width < needed:
            findings.append(
                Finding("spinner-value-width", node.line,
                        f"{node.name} editor has {editor_width}px; needs about {needed}px "
                        f"for {endpoint!r}")
            )

    if static_rules and node.tag in {"text", "button"} and node.rect is not None:
        value = node.attrs.get("label", "") if node.tag == "button" else display_text(node)
        wraps = node.attrs.get("wrap", node.attrs.get("word_wrap", "false")).lower() == "true"
        skip_button = (node.tag == "button" and (
            node.attrs.get("visible", "true").lower() == "false"
            or node.rect.width <= 0
            or bool(re.search(r"\[[^\]]*\]", value))
        ))
        if value and not wraps and not skip_button:
            factor = 6.5 if node.attrs.get("font") == "SansSerifSmall" else 7.5
            estimated = math.ceil(max(len(line) for line in value.splitlines()) * factor)
            limit = node.rect.width * (1.1 if node.tag == "button" else 1.0)
            if estimated > limit:
                findings.append(
                    Finding(
                        "text-width",
                        node.line,
                        f"{node.name} {node.rect} needs about {estimated}px for "
                        f"{value!r}, has {node.rect.width}px",
                    )
                )

    for child in children:
        check_node(child, overlap_policy, findings, dynamic_visibility,
                   runtime_tab_fit, copies_fit_grid_height, static_rules)


# ---------------------------------------------------------------------------
# Measured kit label fit (DIRECTOR_KIT_P0_SPEC.md 6.1, "Additional errors").
#
# A kit row label sits in the mockup's 100 px label track (alkitrow.cpp: LABEL_W),
# and a page master label sits in a box alkitpage.cpp sizes from its text,
# capped at MASTER_LABEL_MAX_W.  Both use the Kit12 size of IBM Plex Sans, so
# the width is summed from the viewer's own font file.  The viewer elides an
# overflowing label with an ellipsis and a tooltip (ALKitStyle::fitText), but
# that is only a safety net: every label must FIT, and there is no allowance list.
# ---------------------------------------------------------------------------
ROOT_DIR = Path(__file__).resolve().parents[2]
KIT_FONT_FILE = ROOT_DIR / "indra/newview/fonts/IBMPlexSansVar-Roman.ttf"
KIT_FONTS_XML = ROOT_DIR / "indra/newview/skins/default/xui/en/fonts.xml"
ALKIT_STYLE_H = ROOT_DIR / "indra/newview/alkitstyle.h"
ALKIT_ROW_CPP = ROOT_DIR / "indra/newview/alkitrow.cpp"
ALKIT_PAGE_CPP = ROOT_DIR / "indra/newview/alkitpage.cpp"
ALKIT_CARD_CPP = ROOT_DIR / "indra/newview/alkitcard.cpp"
KIT_LABEL_FONT_SIZE = "Kit12"
# A dial/arc label sits in a 2-up cell whose width comes from the card, not the column
# (ALKitRow::layoutDialCell): the viewer elides it at the narrowest card, so the gate
# measures it against that cell's label box (kit_dial_label_limit()).
DIAL_LABEL_KINDS = {"dial", "arc"}
RUNTIME_ELIDE_GUARDS = (
    (ALKIT_ROW_CPP, "ALKitStyle::fitText(", "kit row labels are no longer elided with a tooltip"),
    (ALKIT_PAGE_CPP, "ALKitStyle::masterLabelWidth(", "page master labels are no longer sized to their text"),
)


def _read_const(text: str, name: str, source: Path) -> int:
    match = re.search(r"\b" + name + r"\s*=\s*(\d+)", text)
    if not match:
        raise ValueError(f"{source.name}: constant {name} not found")
    return int(match.group(1))


@functools.lru_cache(maxsize=None)
def kit_label_limits() -> tuple[int, int]:
    """Return (row label box width, master label maximum width) from the viewer source."""
    style = ALKIT_STYLE_H.read_text(encoding="utf-8")
    row_source = ALKIT_ROW_CPP.read_text(encoding="utf-8")
    if not re.search(r"mLabel->reshape\(\s*ALKitStyle::LABEL_W\s*,\s*20\s*,\s*true\s*\)", row_source):
        raise ValueError("alkitrow.cpp: the row label box is no longer ALKitStyle::LABEL_W wide")
    return _read_const(style, "LABEL_W", ALKIT_STYLE_H), _read_const(style, "MASTER_LABEL_MAX_W", ALKIT_STYLE_H)


@functools.lru_cache(maxsize=None)
def kit_full_toggle_label_limit() -> int:
    """The fill_width toggle's label uses the unused track at CARD_MIN_W.

    Pin the viewer formula and its application; ordinary rows retain the exact
    LABEL_W check above. This measures width, never exempts a long label.
    """
    style = ALKIT_STYLE_H.read_text(encoding="utf-8")
    source = re.sub(r"\s+", "", ALKIT_ROW_CPP.read_text(encoding="utf-8"))
    formula = ('constS32label_width=mFillWidth&&mMeta.kind=="toggle"'
               '?llmax(ALKitStyle::LABEL_W,w-2*ALKitStyle::ROW_INSET-ALKitStyle::SWITCH_W-'
               'value_width-ALKitStyle::RESET_W-kFavoriteW-4*ALKitStyle::COL_GAP)'
               ':ALKitStyle::LABEL_W;')
    if (formula not in source or
            'if(mFillWidth&&mMeta.kind=="toggle")mLabel->reshape(label_width,20,true);' not in source or
            'fitLabel(label_width);' not in source or 'constexprS32kFavoriteW=18;' not in source or
            'constS32value_width=mFillWidth&&mMeta.kind=="toggle"?24:ALKitStyle::VALUE_W;' not in source or
            'mValueText->reshape(value_width,20,true);' not in source):
        raise ValueError("alkitrow.cpp: full-width toggle label geometry changed")
    value = lambda key: _read_const(style, key, ALKIT_STYLE_H)
    body = value("CARD_MIN_W") - value("CARD_PAD_L") - value("CARD_PAD_R")
    return max(value("LABEL_W"), body - 2 * value("ROW_INSET") - value("SWITCH_W") -
               24 - value("RESET_W") - 18 - 4 * value("COL_GAP"))


@functools.lru_cache(maxsize=None)
def kit_dial_label_limit() -> int:
    """Label box of a dial/arc cell at the narrowest card (CARD_MIN_W), from the viewer source.

    alkitcard.cpp: body_w = card_w - CARD_PAD_L - CARD_PAD_R; cell = (body_w - GRID_GAP) / 2.
    alkitrow.cpp layoutDialCell: label box = cell - RESET_W - COL_GAP.
    """
    style = ALKIT_STYLE_H.read_text(encoding="utf-8")
    row_source = ALKIT_ROW_CPP.read_text(encoding="utf-8")
    card_source = ALKIT_CARD_CPP.read_text(encoding="utf-8")
    if not re.search(r"fitLabel\(\s*llmax\(\s*1\s*,\s*w\s*-\s*ALKitStyle::RESET_W\s*-\s*ALKitStyle::COL_GAP\s*\)\s*\)", row_source):
        raise ValueError("alkitrow.cpp: the dial label box is no longer cell - RESET_W - COL_GAP")
    if not re.search(r"cell\s*=\s*\(\s*body_w\s*-\s*ALKitStyle::GRID_GAP\s*\)\s*/\s*2", card_source):
        raise ValueError("alkitcard.cpp: the dial cell is no longer (body_w - GRID_GAP) / 2")
    if not re.search(r"body_w\s*=\s*w\s*-\s*ALKitStyle::CARD_PAD_L\s*-\s*ALKitStyle::CARD_PAD_R", card_source):
        raise ValueError("alkitcard.cpp: the card body width is no longer w - CARD_PAD_L - CARD_PAD_R")
    # The flow grid's card width is the narrowest card the viewer builds: it must be this constant,
    # not a literal, or a change to the page's card width would not reach this gate.
    page_source = ALKIT_PAGE_CPP.read_text(encoding="utf-8")
    if not re.search(r"gp\.card_width\s*=\s*ALKitStyle::CARD_MIN_W\s*;", page_source):
        raise ValueError("alkitpage.cpp: the flow grid card_width is no longer ALKitStyle::CARD_MIN_W")
    card_min = _read_const(style, "CARD_MIN_W", ALKIT_STYLE_H)
    body = card_min - _read_const(style, "CARD_PAD_L", ALKIT_STYLE_H) - _read_const(style, "CARD_PAD_R", ALKIT_STYLE_H)
    cell = (body - _read_const(style, "GRID_GAP", ALKIT_STYLE_H)) // 2
    return cell - _read_const(style, "RESET_W", ALKIT_STYLE_H) - _read_const(style, "COL_GAP", ALKIT_STYLE_H)


@functools.lru_cache(maxsize=None)
def _kit_font_metrics() -> tuple[int, dict[int, int], float, float]:
    """Return (units per em, advance by code point, pixels per em, advance for a missing glyph)."""
    data = KIT_FONT_FILE.read_bytes()
    tables: dict[bytes, int] = {}
    for index in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _checksum, offset, _length = struct.unpack_from(">4sIII", data, 12 + 16 * index)
        tables[tag] = offset
    upem = struct.unpack_from(">H", data, tables[b"head"] + 18)[0]
    metric_count = struct.unpack_from(">H", data, tables[b"hhea"] + 34)[0]
    hmtx = tables[b"hmtx"]
    advance_of = [struct.unpack_from(">H", data, hmtx + 4 * glyph)[0] for glyph in range(metric_count)]
    cmap = tables[b"cmap"]
    glyph_for: dict[int, int] = {}
    for index in range(struct.unpack_from(">H", data, cmap + 2)[0]):
        _pid, _eid, offset = struct.unpack_from(">HHI", data, cmap + 4 + 8 * index)
        sub = cmap + offset
        fmt = struct.unpack_from(">H", data, sub)[0]
        if fmt == 4 and not glyph_for:
            seg_x2 = struct.unpack_from(">H", data, sub + 6)[0]
            seg = seg_x2 // 2
            ends = struct.unpack_from(f">{seg}H", data, sub + 14)
            starts = struct.unpack_from(f">{seg}H", data, sub + 16 + seg_x2)
            deltas = struct.unpack_from(f">{seg}h", data, sub + 16 + 2 * seg_x2)
            range_at = sub + 16 + 3 * seg_x2
            ranges = struct.unpack_from(f">{seg}H", data, range_at)
            for i in range(seg):
                for code in range(starts[i], ends[i] + 1):
                    if code == 0xFFFF:
                        continue
                    if ranges[i] == 0:
                        glyph = (code + deltas[i]) & 0xFFFF
                    else:
                        glyph = struct.unpack_from(
                            ">H", data, range_at + 2 * i + ranges[i] + 2 * (code - starts[i]))[0]
                        if glyph:
                            glyph = (glyph + deltas[i]) & 0xFFFF
                    if glyph:
                        glyph_for[code] = glyph
        elif fmt == 12 and not glyph_for:
            for group in range(struct.unpack_from(">I", data, sub + 12)[0]):
                first, last, glyph0 = struct.unpack_from(">III", data, sub + 16 + 12 * group)
                for code in range(first, last + 1):
                    glyph_for[code] = glyph0 + code - first
    if not glyph_for:
        raise ValueError(f"{KIT_FONT_FILE.name}: no usable cmap subtable")
    advances = {code: advance_of[min(glyph, metric_count - 1)] for code, glyph in glyph_for.items()}
    fonts = ElementTree.parse(KIT_FONTS_XML).getroot()
    point = next((float(node.get("size")) for node in fonts.iter("font_size")
                  if node.get("name") == KIT_LABEL_FONT_SIZE), None)
    if point is None:
        raise ValueError(f"{KIT_FONTS_XML.name}: font_size {KIT_LABEL_FONT_SIZE} not found")
    # fonts.xml sizes are points at the viewer's 96 dpi baseline.
    return upem, advances, point * 96.0 / 72.0, 0.6 * upem


def kit_label_width(text: str) -> float:
    """Advance-sum width in pixels (no kerning). A character the font lacks counts as 0.6 em."""
    upem, advances, pixels_per_em, missing = _kit_font_metrics()
    return sum(advances.get(ord(ch), missing) for ch in text) * pixels_per_em / upem


@functools.lru_cache(maxsize=None)
def _runtime_elide_findings() -> tuple[str, ...]:
    messages = []
    for source, guard, message in RUNTIME_ELIDE_GUARDS:
        if guard not in source.read_text(encoding="utf-8"):
            messages.append(f"{source.name}: {message}")
    return tuple(messages)


def label_fit_findings(root: Node, path: Path) -> list[Finding]:
    kit_nodes = [node for node in (root, *descendants(root)) if node.tag in {"kit_row", "kit_page"}]
    if not kit_nodes:
        return []
    row_box, master_max = kit_label_limits()
    findings: list[Finding] = []
    for node in kit_nodes:
        if node.tag == "kit_row":
            label = node.attrs.get("label", "")
            if not label:
                continue
            if node.attrs.get("kind", "") in DIAL_LABEL_KINDS:
                role, allotted = "dial", kit_dial_label_limit()
            elif node.attrs.get("kind") == "toggle" and node.attrs.get("fill_width") == "true":
                role, allotted = "full-width toggle", kit_full_toggle_label_limit()
            else:
                role, allotted = "row", row_box
        else:
            label = node.attrs.get("master_label", "")
            if not label or not node.attrs.get("master_widget"):
                continue
            role, allotted = "master", master_max
        width = kit_label_width(label)
        if width <= allotted:
            continue
        findings.append(Finding(
            "label-fit", node.line,
            f"{role} label {label!r} needs about {math.ceil(width)}px; its box is {allotted}px, "
            "so the viewer elides it. Shorten the label and keep the full wording in its help text",
        ))
    findings.extend(Finding("label-fit-runtime", 1, message) for message in _runtime_elide_findings())
    return findings


@dataclass(frozen=True)
class KitSourcePins:
    """Source-string pins only; these do not measure runtime geometry.

    Actual post-layout VCam rects are audited by vcam_runtime_rect_check.py.
    Unknown source implementations fail closed until the pins are reviewed.
    """
    nested_actions: bool
    top_aligned_readouts: bool
    atomic_row_resize: bool
    card_stacks_rows: bool

    @classmethod
    def from_source(cls, source: str) -> "KitSourcePins":
        card_source = ALKIT_CARD_CPP.read_text(encoding="utf-8")
        return cls(
            all(part in source for part in (
                "if (mActionRow) height = h - 4;",
                "action.first->reshape(button_width, 24, true);",
                "action.first->setOrigin(left, top - 24);",
                "if (left && left + button_width > width) { left = 0; top -= 28; }",
                "return llmax(mRowHeight, lines * 28);")),
            "top_aligned ? h - 24 : (h - 20) / 2" in source,
            "else layoutRow(width);" in source and
            "if (ALKitCard* owner = card()) owner->requestLayout();" in source,
            all(part in card_source for part in (
                "const S32 rh = row->heightForWidth(body_w);",
                "row->layoutRow(body_w);",
                "row->setOrigin(ALKitStyle::CARD_PAD_L, top - rh);",
                "top -= rh;")),
        )


def kit_source_pin_findings(root: Node, rules: KitSourcePins | None = None) -> list[Finding]:
    """Pin row ownership/reflow source contracts; make no geometry claims."""
    if rules is None:
        rules = KitSourcePins.from_source(ALKIT_ROW_CPP.read_text(encoding="utf-8"))
    findings: list[Finding] = []
    for card in (node for node in (root, *descendants(root)) if node.tag == "kit_card"):
        rows = [node for node in card.children if node.tag == "kit_row"]
        if rows and not rules.card_stacks_rows:
            findings.append(Finding("kit-source-stacking", card.line, f"{card.name}: measured-height stacking source pin missing"))
        for row in rows:
            control = next((child for child in row.children if child.name == row.attrs.get("widget")), None)
            buttons = [] if control is None else [child for child in control.children if child.tag == "button"]
            if row.attrs.get("kind") == "custom" and buttons:
                if row.attrs.get("action_row", "false") != "true" or not rules.nested_actions:
                    findings.append(Finding("kit-source-actions", row.line, f"{row.name}: action ownership source pin missing"))
            if row.attrs.get("kind") == "readout" and control is not None and control.attrs.get("word_wrap") == "true":
                if not rules.atomic_row_resize:
                    findings.append(Finding("kit-source-reflow", row.line, f"{row.name}: atomic row reflow source pin missing"))
                if row.attrs.get("label") and not rules.top_aligned_readouts:
                    findings.append(Finding("kit-source-readout", row.line, f"{row.name}: first-line alignment source pin missing"))
    return finalize_findings(findings)


def check_file(path: Path, source_index: SourceIndex | None = None,
               dynamic_visibility_override: set[str] | None = None) -> list[Finding]:
    if path.name == KIT_INDEX_NAME:
        return []
    root, overlap_policy = parse_xml(path)
    calculate(root)
    kit_findings = kit_geometry_findings(root) + label_fit_findings(root, path) + kit_source_pin_findings(root)
    if root.rect is None or root.rect.width <= 0 or root.rect.height <= 0:
        if any(node.legacy_kit for node in (root, *descendants(root))):
            kit_findings.append(Finding(
                "invalid-root", root.line,
                f"{path.name} contains a legacy kit_page under a zero-size root; layout cannot be skipped",
            ))
        return finalize_findings(kit_findings)
    findings: list[Finding] = list(kit_findings)
    dynamic_visibility = (dynamic_visibility_override if dynamic_visibility_override is not None
                          else (source_index.dynamically_visible_names(
                              path.name, root.attrs.get("class", ""))
                                if source_index is not None else set()))
    runtime_tab_fit = source_index.fits_tabs_at_runtime(path.name) if source_index is not None else False
    grid_names = [node.name for node in (root, *descendants(root))
                  if node.tag == "flow_grid"
                  and node.attrs.get("fit_cards", "false").lower() == "true"]
    explicit_height_copy = root.attrs.get("xui_check_copies_grid_height", "false").lower() == "true"
    copies_fit_grid_height = explicit_height_copy or bool(
        source_index is not None
        and source_index.copies_fit_grid_height(root.attrs.get("class", ""), grid_names)
    )
    check_node(root, overlap_policy, findings, dynamic_visibility, runtime_tab_fit,
               copies_fit_grid_height)

    if grid_names:
        # A 690px card is the normal single-column Director host.  Reparse so
        # the narrow calculation's rect mutations cannot leak into this pass.
        wide_root, wide_policy = parse_xml(path)
        wide_grid = next(node for node in (wide_root, *descendants(wide_root))
                         if node.tag == "flow_grid"
                         and node.attrs.get("fit_cards", "false").lower() == "true")
        wide_grid_width = 690 + 2 * wide_grid.integer("pad", 8)
        wide_root_width = wide_root.integer("width", 0) + (
            wide_grid_width - wide_grid.integer("width", wide_grid_width)
        )
        calculate(wide_root, root_width=wide_root_width)
        wide_findings: list[Finding] = []
        check_node(wide_root, wide_policy, wide_findings, dynamic_visibility,
                   runtime_tab_fit, copies_fit_grid_height, static_rules=False)
        findings.extend(wide_findings)
    if source_index is not None:
        findings.extend(scroller_wiring_findings(root, path, source_index))
    return finalize_findings(findings)


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
        untracked = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard", "indra/newview/skins"],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, "stderr", "") or str(exc)
        raise ValueError(f"git diff failed: {detail.strip()}") from exc
    return sorted({
        Path(name)
        for name in result.stdout.splitlines() + untracked.stdout.splitlines()
        if name.lower().endswith(".xml") and "/xui/" in name.replace("\\", "/")
    })


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
        ("joystick_broken.xml", True, "overlap"),
        ("joystick_current.xml", False, None),
        ("fit_card_broken.xml", True, "clipped"),
        ("spinner_stretch_broken.xml", True, "invalid-stretch"),
        ("spinner_value_width_broken.xml", True, "spinner-value-width"),
        ("spinner_increment_precision_broken.xml", True, "spinner-increment-precision"),
        ("fixed_stretch_broken.xml", True, "invalid-stretch"),
        ("fixed_stretch_outside_current.xml", False, None),
        ("button_text_broken.xml", True, "text-width"),
        ("hidden_overlap_broken.xml", True, "overlap"),
        ("conditional_independent_broken.xml", True, "overlap"),
        ("exclusive_overlap_broken.xml", True, "overlap"),
        ("overlay_current.xml", False, None),
        ("fit_card_wide_broken.xml", True, "overlap"),
        ("kit_page_clean_current.xml", False, None),
        ("kit_row_stray_width_broken.xml", True, "kit-geometry"),
        ("kit_tag_geometry_broken.xml", True, "kit-geometry"),
        ("kit_bar_geometry_broken.xml", True, "kit-geometry"),
        ("kit_shell_current.xml", False, None),
        ("kit_shell_copied_width_broken.xml", True, "kit-geometry"),
        ("director_kit_index.xml", False, None),
        ("flow_rows_fit_current.xml", False, None),
        ("flow_rows_clipped_broken.xml", True, "clipped"),
        ("kit_legacy_position_broken.xml", True, "kit-geometry"),
        ("kit_legacy_page_broken.xml", True, "overlap"),
        ("kit_legacy_zero_root_broken.xml", True, "invalid-root"),
        ("accordion_fit_broken.xml", True, "accordion-clip"),
        ("accordion_fit_current.xml", False, None),
        ("kit_label_fit_broken.xml", True, "label-fit"),
        ("kit_label_master_broken.xml", True, "label-fit"),
        ("kit_label_fit_current.xml", False, None),
        ("kit_label_toggle_current.xml", False, None),
        ("kit_label_toggle_broken.xml", True, "label-fit"),
        ("kit_label_dial_broken.xml", True, "label-fit"),
        ("kit_label_dial_current.xml", False, None),
    )
    reports: list[dict[str, object]] = []
    passed = True
    for filename, expects_findings, required_kind in cases:
        path = fixture_dir / filename
        try:
            findings = check_file(path)
            overlap_count = sum(item.kind == "overlap" for item in findings)
            case_passed = bool(findings) == expects_findings
            if required_kind is not None:
                case_passed = case_passed and any(item.kind == required_kind for item in findings)
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
    runtime_root, _ = parse_xml(fixture_dir / "kit_media_runtime.xml")
    old_rules = KitSourcePins.from_source((fixture_dir / "kit_media_runtime_old.txt").read_text(encoding="utf-8"))
    old_findings = kit_source_pin_findings(runtime_root, old_rules)
    current_findings = kit_source_pin_findings(runtime_root)
    old_kinds = {item.kind for item in old_findings}
    runtime_passed = {"kit-source-actions", "kit-source-readout"} <= old_kinds and not current_findings
    passed = passed and runtime_passed
    reports.append({"fixture": "kit media old/current source pins", "passed": runtime_passed,
                    "old_findings": len(old_findings), "current_findings": len(current_findings)})
    if not json_output:
        print(f"{'PASS' if runtime_passed else 'FAIL'} kit media old/current source pins: "
              f"old={len(old_findings)} ({', '.join(sorted(old_kinds))}); current={len(current_findings)}")
    unmanaged_root, _ = parse_xml(fixture_dir / "kit_media_runtime.xml")
    for node in descendants(unmanaged_root):
        node.attrs.pop("action_row", None)
    unmanaged = kit_source_pin_findings(unmanaged_root)
    unmanaged_passed = any(item.kind == "kit-source-actions" for item in unmanaged)
    passed = passed and unmanaged_passed
    reports.append({"fixture": "kit media missing action ownership", "passed": unmanaged_passed})
    if not json_output:
        print(f"{'PASS' if unmanaged_passed else 'FAIL'} kit media missing action ownership: "
              f"{len(unmanaged)} finding(s)")
    alias_source = (fixture_dir / "visibility_alias_fixture.cpp").read_text(encoding="utf-8")
    alias_passed = dynamic_visibility_names(alias_source) == {"member_alternate"}
    passed = passed and alias_passed
    reports.append({
        "fixture": "visibility_alias_fixture.cpp",
        "expected": "member alias classified as C++-toggled",
        "passed": alias_passed,
    })
    if not json_output:
        print(f"{'PASS' if alias_passed else 'FAIL'} visibility_alias_fixture.cpp: "
              "setVisibleIfChanged member alias")
    conditional_findings = check_file(
        fixture_dir / "conditional_alternates_current.xml",
        dynamic_visibility_override={"conditional_a", "conditional_b"},
    )
    conditional_passed = not conditional_findings
    passed = passed and conditional_passed
    reports.append({
        "fixture": "conditional_alternates_current.xml",
        "expected": "0 findings for two C++-toggled siblings",
        "count": len(conditional_findings),
        "passed": conditional_passed,
    })
    if not json_output:
        print(f"{'PASS' if conditional_passed else 'FAIL'} conditional_alternates_current.xml: "
              f"{len(conditional_findings)} finding(s)")
    # A legacy kit_page must report exactly what the v1 leaf it replaces reports.
    leaf_findings = check_file(fixture_dir / "kit_legacy_leaf_v1_broken.xml")
    legacy_findings = check_file(fixture_dir / "kit_legacy_page_broken.xml")
    legacy_pair = [(item.kind, item.message) for item in leaf_findings]
    legacy_passed = bool(leaf_findings) and sorted(legacy_pair) == sorted(
        (item.kind, item.message) for item in legacy_findings)
    passed = passed and legacy_passed
    reports.append({
        "fixture": "kit_legacy_page_broken.xml",
        "expected": "same findings as kit_legacy_leaf_v1_broken.xml",
        "count": len(legacy_findings),
        "passed": legacy_passed,
    })
    if not json_output:
        print(f"{'PASS' if legacy_passed else 'FAIL'} kit_legacy_page_broken.xml: "
              f"legacy kit_page matches v1 leaf findings ({len(legacy_findings)})")
    baseline_a = finding_key("fixture.xml", Finding(
        "clipped", 1, "widget (0,0,10,10) extends outside root (0,0,5,5)"))
    baseline_b = finding_key("fixture.xml", Finding(
        "clipped", 1, "widget (0,0,10,9000) extends outside root (0,0,5,5)"))
    geometry_key_passed = baseline_a != baseline_b
    passed = passed and geometry_key_passed
    reports.append({"fixture": "legacy baseline geometry key", "expected": "growth changes key",
                    "passed": geometry_key_passed})
    if not json_output:
        print(f"{'PASS' if geometry_key_passed else 'FAIL'} legacy baseline geometry key: "
              "baselined widget growth changes the exact key")
    # Measured kit labels: the font-file widths must match what the viewer clips
    # ("Include ghost c" was the visible prefix of the old 82 px box), and the row
    # box must be the mockup's 100 px label track.
    clip_width = kit_label_width("Include ghost c")
    label_metrics_passed = (
        kit_label_limits()[0] == 100
        and 100 < kit_label_width("Include ghost clones")
        and kit_label_width("Ghost clones") <= 82
        and 80 <= clip_width <= 86
    )
    passed = passed and label_metrics_passed
    reports.append({"fixture": "kit label metrics and row track",
                    "expected": "Kit12 font-file widths; row label track is 100 px",
                    "passed": label_metrics_passed})
    if not json_output:
        print(f"{'PASS' if label_metrics_passed else 'FAIL'} kit label metrics and row track: "
              f"row box {kit_label_limits()[0]}px, 'Include ghost c' {clip_width:.1f}px")
    # Real-tree regression: each of the 30 serialized legacy pages must have
    # the same finding keys as the v1 leaf it copied.  This prevents both
    # wrapper-size vacuity and serialization-only drift.
    repo = Path(__file__).resolve().parents[2]
    xui = repo / "indra/newview/skins/default/xui/en"
    v1_path = xui / "floater_director.xml"
    real_ok = True
    real_count = 0
    matching_count = 0
    observed_baseline: set[str] = set()
    if v1_path.is_file():
        v1_xml = ElementTree.parse(
            v1_path, parser=ElementTree.XMLParser(
                target=ElementTree.TreeBuilder(insert_comments=True))).getroot()
        leaves = {node.get("name"): node for node in v1_xml.iter("panel") if node.get("name")}
        with tempfile.TemporaryDirectory(prefix=".xui_legacy_parity_", dir=xui) as temp_name:
            temp = Path(temp_name)
            for generated_path in sorted(xui.glob("panel_dpage_*.xml")):
                generated_xml = ElementTree.parse(generated_path).getroot()
                kit = generated_xml if generated_xml.tag == "kit_page" else generated_xml.find(".//kit_page")
                if kit is None or kit.get("legacy") != "true":
                    continue
                real_count += 1
                page_name = str(kit.get("page", "")).rsplit("/", 1)[-1]
                leaf = leaves.get(page_name)
                if leaf is None:
                    real_ok = False
                    continue
                leaf_path = temp / generated_path.name
                ElementTree.ElementTree(leaf).write(leaf_path, encoding="utf-8", xml_declaration=True)
                def keys(items, generated=False):
                    keyed = []
                    for item in items:
                        subject = finding_subject(item)
                        if generated and subject == page_name + "_content":
                            subject = page_name
                        keyed.append((item.kind, subject))
                    return sorted(keyed)
                generated_findings = check_file(generated_path)
                pair_ok = keys(generated_findings, generated=True) == keys(check_file(leaf_path))
                matching_count += int(pair_ok)
                real_ok = real_ok and pair_ok
                observed_baseline.update(finding_key(generated_path.name, item)
                                         for item in generated_findings)
    try:
        declared_baseline = legacy_v1_baseline()
        baseline_ok = observed_baseline == declared_baseline
    except ValueError:
        baseline_ok = False
    source_index = SourceIndex(repo)
    # 10/15: Path became a generated kit page with no scroll_container, so the probe is a fixture
    # with the retired legacy Path page's shape, registered as a lazily hosted page.
    lazy_probe = "lazy_scroll_probe.xml"
    source_index.lazy_page_files.add(lazy_probe)
    lazy_path = source_index.source_dir / "alkitlazypage.cpp"
    fit_path = source_index.source_dir / "alscrollfit.cpp"
    adopt_present = source_index.fits_tabs_at_runtime(lazy_probe)
    original_lazy = source_index.files.get(lazy_path, "")
    original_fit = source_index.files.get(fit_path, "")
    source_index.files[lazy_path] = original_lazy.replace(".adoptPage(", ".removedAdoptPage(")
    missing_lazy_rejected = not source_index.fits_tabs_at_runtime(lazy_probe)
    missing_lazy_kinds = {item.kind for item in check_file(fixture_dir / lazy_probe, source_index)}
    source_index.files[lazy_path] = original_lazy
    source_index.files[fit_path] = original_fit.replace("Controller::adoptPage(", "Controller::removedAdoptPage(")
    missing_fit_rejected = not source_index.fits_tabs_at_runtime(lazy_probe)
    source_index.files[fit_path] = original_fit
    adopt_passed = (adopt_present and missing_lazy_rejected and missing_fit_rejected
                    and {"scroller-wiring", "scroll-content-overflow"} <= missing_lazy_kinds)
    passed = passed and adopt_passed
    reports.append({"fixture": "lazy-page adoptPage wiring", "expected": "both call and implementation required",
                    "passed": adopt_passed})
    if not json_output:
        print(f"{'PASS' if adopt_passed else 'FAIL'} lazy-page adoptPage wiring: "
              "alkitlazypage.cpp call and alscrollfit.cpp implementation are both required")
    runtime_observed = {
        finding_key(path.name, item)
        for path in sorted(xui.glob("panel_dpage_*.xml")) if is_legacy_page(path)
        for item in check_file(path, source_index)
    }
    runtime_ok = runtime_observed <= declared_baseline if baseline_ok else False
    # Legacy (embedded v1) Director pages still pinned to v1 findings. Lower this as
    # pages become real kit pages (user-approved 10/13, Look batch: 30 -> 27,
    # World/Lens batch: 27 -> 21, Camera+Capture: 21 -> 14); never raise it.
    LEGACY_PAGE_COUNT = 0  # Light Actors: 14 -> 13; Animate: 13 -> 12; polish_tab: 12 -> 11; switchboard_tab: 11 -> 10; light_cues_tab: 10 -> 9; practicals_tab: 9 -> 8; gaze_cues_tab: 8 -> 7; exposure_tab: 7 -> 6; move_tab: 6 -> 5; props_tab: 5 -> 4; path_tab: 4 -> 3; gaze_tab: 3 -> 2; dance_tab: 2 -> 1; ghosts_tab: 1 -> 0
    real_passed = real_ok and real_count == LEGACY_PAGE_COUNT and baseline_ok and runtime_ok
    passed = passed and real_passed
    reports.append({"fixture": "real legacy pages", "expected": f"{LEGACY_PAGE_COUNT} pages match v1 leaf findings",
                    "count": real_count, "passed": real_passed})
    if not json_output:
        print(f"{'PASS' if real_passed else 'FAIL'} real legacy pages: "
              f"{matching_count}/{LEGACY_PAGE_COUNT} match v1 finding keys; strict baseline "
              f"{'matches' if baseline_ok else 'differs'}; transitive wiring "
              f"{'passes' if runtime_ok else 'differs'}")
    if json_output:
        print(json.dumps({"passed": passed, "fixtures": reports}, indent=2))
    elif passed:
        print("Self-test passed: overlap policy, C++ visibility, fitted-card narrow/wide, "
              "stretch, spinner-value, button-text, kit geometry, kit shell, flow rows, "
              "and legacy kit_page parity rules detected")
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
        baseline = legacy_v1_baseline()
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
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
        if is_legacy_page(path):
            expected = {key for key in baseline if key.startswith(path.name + "|")}
            observed = {finding_key(path.name, item) for item in findings}
            drift = sorted(observed - expected)
            findings = [item for item in findings if finding_key(path.name, item) not in expected]
            findings.extend(Finding("legacy-baseline-drift", 1, f"unexpected finding key {key}")
                            for key in drift)
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
