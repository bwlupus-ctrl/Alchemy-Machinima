#!/usr/bin/env python3
"""Fit direct card children to the runtime width of ``fit_cards`` flow grids.

The default mode is read-only and prints a unified diff.  Pass ``--write`` to
replace the input file.  The transform deliberately edits start-tag attribute
values in place instead of serializing XML, preserving whitespace, attribute
order, comments, and LF line endings.
"""

from __future__ import annotations

import argparse
import difflib
import json
import math
import re
import subprocess
import sys
import xml.etree.ElementTree as ElementTree
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path


ATTR_RE = re.compile(r'([\w:.-]+)(\s*=\s*")([^"]*)(")')
STRETCH_TAGS = {"combo_box", "line_editor", "slider", "text"}
FIXED_RIGHT_TAGS = {"button", "check_box", "icon", "image"}
TEXT_FACTOR = 7.5
APPLIED_MARKER_RE = re.compile(
    r"<!-- xui-fit-card-children: one-shot transform from ([^;]+); "
    r"do not rerun; edit by hand -->"
)


@dataclass
class StartTag:
    start: int
    end: int
    text: str
    attrs: dict[str, str]
    updates: dict[str, str] = field(default_factory=dict)


@dataclass
class Box:
    element: ElementTree.Element
    tag: StartTag
    left: int
    top: int
    width: int
    height: int

    @property
    def right(self) -> int:
        return self.left + self.width

    @property
    def bottom(self) -> int:
        return self.top + self.height


def integer(attrs: dict[str, str], key: str, default: int = 0) -> int:
    try:
        return int(float(attrs.get(key, default)))
    except (TypeError, ValueError):
        return default


def follows(attrs: dict[str, str]) -> set[str]:
    result = {part.strip().lower() for part in attrs.get("follows", "left|top").split("|")}
    if "all" in result:
        result.update({"left", "right", "top", "bottom"})
    return result


def set_attr(tag: StartTag, key: str, value: int | str) -> None:
    value = str(value)
    tag.attrs[key] = value
    tag.updates[key] = value


def scan_start_tags(text: str) -> list[tuple[str, int, int, str]]:
    """Scan XML start tags without mistaking ``>`` inside an attribute for the close."""
    result: list[tuple[str, int, int, str]] = []
    cursor = 0
    while True:
        start = text.find("<", cursor)
        if start < 0:
            return result
        if text.startswith("<!--", start):
            cursor = text.find("-->", start + 4) + 3
            if cursor == 2:
                raise ValueError("unterminated XML comment")
            continue
        if text.startswith("<![CDATA[", start):
            cursor = text.find("]]>", start + 9) + 3
            if cursor == 2:
                raise ValueError("unterminated XML CDATA section")
            continue
        if text.startswith("<?", start):
            cursor = text.find("?>", start + 2) + 2
            if cursor == 1:
                raise ValueError("unterminated XML declaration/instruction")
            continue
        if text.startswith(("<!", "</"), start):
            cursor = text.find(">", start + 2) + 1
            if cursor == 0:
                raise ValueError("unterminated XML declaration/end tag")
            continue
        name_match = re.match(r"<([A-Za-z_][\w:.-]*)", text[start:])
        if not name_match:
            cursor = start + 1
            continue
        quote = ""
        end = start + len(name_match.group(0))
        while end < len(text):
            char = text[end]
            if quote:
                if char == quote:
                    quote = ""
            elif char in {'"', "'"}:
                quote = char
            elif char == ">":
                end += 1
                break
            end += 1
        else:
            raise ValueError(f"unterminated <{name_match.group(1)}> tag")
        result.append((name_match.group(1), start, end, text[start:end]))
        cursor = end


def parse_start_tags(text: str, root: ElementTree.Element) -> dict[int, StartTag]:
    matches = scan_start_tags(text)
    elements = list(root.iter())
    if len(matches) != len(elements):
        raise ValueError(f"could not map XML elements to start tags ({len(elements)} elements, {len(matches)} tags)")
    result: dict[int, StartTag] = {}
    for element, match in zip(elements, matches):
        name, start, end, raw = match
        if element.tag != name:
            raise ValueError(f"start-tag map lost sync at <{element.tag}> / <{name}>")
        attrs = {item.group(1): item.group(3) for item in ATTR_RE.finditer(raw)}
        result[id(element)] = StartTag(start, end, raw, attrs)
    return result


def render_tag(tag: StartTag) -> str:
    rendered = tag.text
    for key, value in tag.updates.items():
        pattern = re.compile(rf'((?<![\w:.-]){re.escape(key)}\s*=\s*")[^"]*(")')
        if pattern.search(rendered):
            rendered = pattern.sub(lambda match: match.group(1) + value + match.group(2), rendered, count=1)
        else:
            close = rendered.rfind("/>")
            if close < 0:
                close = rendered.rfind(">")
            rendered = rendered[:close].rstrip() + f' {key}="{value}"' + rendered[close:]
    return rendered


def fitted_width(grid: StartTag, host_width: int | None = None) -> int:
    width = max(1, host_width if host_width is not None else integer(grid.attrs, "width", 1))
    pad = max(0, integer(grid.attrs, "pad", 8))
    gap = max(0, integer(grid.attrs, "column_gap", 12))
    nominal = max(1, integer(grid.attrs, "card_width", 340))
    columns = max(1, min(max(1, integer(grid.attrs, "max_columns", 4)),
                         (max(1, width - 2 * pad) + gap) // (nominal + gap)))
    return max(1, (max(1, width - 2 * pad) - (columns - 1) * gap) // columns)


def boxes(card: ElementTree.Element, tags: dict[int, StartTag]) -> list[Box]:
    result: list[Box] = []
    for child in card:
        tag = tags[id(child)]
        if not {"left", "top", "width", "height"}.issubset(tag.attrs):
            continue
        result.append(Box(child, tag, integer(tag.attrs, "left"), integer(tag.attrs, "top"),
                          integer(tag.attrs, "width"), integer(tag.attrs, "height")))
    return result


def same_row(first: Box, second: Box) -> bool:
    return first.top < second.bottom and second.top < first.bottom


def existing_right_margin(items: list[Box], design_width: int) -> int:
    margins = [design_width - item.right for item in items
               if 0 <= design_width - item.right <= 32]
    return Counter(margins).most_common(1)[0][0] if margins else 0


def natural_width(item: Box) -> int:
    """Smallest safe authored width for a fixed control."""
    if item.element.tag == "button":
        label = item.tag.attrs.get("label", "")
        # Label-less buttons are icons; preserve their authored dimensions.
        return item.width if not label else math.ceil(len(label) * TEXT_FACTOR)
    if item.element.tag == "check_box":
        label = item.tag.attrs.get("label", "")
        return max(20, 20 + math.ceil(len(label) * TEXT_FACTOR)) if label else item.width
    if item.element.tag in {"icon", "image"}:
        return item.width
    if item.element.tag in {"spinner", "spin_ctrl"}:
        return spinner_width_floor(item.tag.attrs)
    return 1


def formatted_spinner_extreme(attrs: dict[str, str]) -> str:
    """Return the longer fixed-precision endpoint LLSpinCtrl must display."""
    digits = max(0, integer(attrs, "decimal_digits", 3))
    values: list[str] = []
    for key in ("min_val", "max_val"):
        try:
            values.append(f"{float(attrs.get(key, 0)):.{digits}f}")
        except (TypeError, ValueError):
            values.append(str(attrs.get(key, "")))
    return max(values, key=len)


def spinner_width_floor(attrs: dict[str, str]) -> int:
    """Minimum width for label, left-hand arrows, endpoint text, and padding."""
    label_width = integer(attrs, "label_width", 0) if attrs.get("label") else 0
    value_width = math.ceil(len(formatted_spinner_extreme(attrs)) * 6.5)
    return label_width + 17 + value_width + 6


def make_room_on_left(items: list[Box], target: Box, old_left: int, overflow: int,
                      counts: Counter[str]) -> None:
    """Preserve a row's gaps while moving its right edge inward.

    Tiny fixed suffixes (for example a percent unit between a slider and its
    reset button) move as a unit.  The first sizeable control then gives up
    the same width as the right-edge overflow.
    """
    candidates = sorted(
        (item for item in items if item is not target and same_row(item, target)
         and item.right <= old_left),
        key=lambda item: item.right,
        reverse=True,
    )
    cursor = old_left - overflow
    for neighbour in candidates:
        if neighbour.right <= cursor:
            break
        if neighbour.element.tag == "text" and neighbour.width <= overflow:
            set_attr(neighbour.tag, "follows", "right|top")
            cursor = neighbour.left - overflow
            counts["row_suffix_anchors"] += 1
            continue
        if neighbour.element.tag in STRETCH_TAGS:
            set_attr(neighbour.tag, "follows", "left|right|top")
            counts["row_stretchers"] += 1
            break

        gap = max(0, old_left - neighbour.right)
        desired_right = old_left - overflow - gap
        new_width = desired_right - neighbour.left
        minimum = natural_width(neighbour)
        if new_width >= minimum:
            neighbour.width = new_width
            set_attr(neighbour.tag, "width", new_width)
            set_attr(neighbour.tag, "follows", "left|top")
            counts["row_static_shrinks"] += 1
        else:
            # Preserve the fixed control's readable/icon width.  Panel-specific
            # overrides can re-pack a denser multi-control row around it.
            shift = neighbour.right - desired_right
            neighbour.left -= shift
            set_attr(neighbour.tag, "left", neighbour.left)
            set_attr(neighbour.tag, "follows", "left|top")
            counts["row_static_moves"] += 1
        break


def fit_right_edges(items: list[Box], inner_right: int, counts: Counter[str]) -> None:
    design_width = max((item.right for item in items), default=inner_right)
    runtime_delta = inner_right - design_width
    for item in sorted(items, key=lambda value: (value.top, value.left)):
        overflow = item.right - inner_right
        if overflow <= 0:
            continue
        status_text = item.element.tag == "text" and (
            item.tag.attrs.get("halign", "").lower() == "right"
            or item.tag.attrs.get("name", "").endswith("status")
            or (item.width <= 64 and bool(item.tag.attrs.get("name"))))
        fixed_right = item.element.tag in FIXED_RIGHT_TAGS or status_text
        if not fixed_right or item.width > inner_right:
            if item.element.tag not in STRETCH_TAGS:
                item.width = max(natural_width(item), item.width - overflow)
                set_attr(item.tag, "width", item.width)
                set_attr(item.tag, "follows", "left|top")
                counts["static_shrinks"] += 1
                continue
            set_attr(item.tag, "follows", "left|right|top")
            counts["stretched"] += 1
            continue

        old_left = item.left
        # Keep the authored rectangle against the design edge.  LLView applies
        # runtime_delta once when the fitted card is reshaped.
        design_right = inner_right - runtime_delta
        item.left = design_right - item.width
        set_attr(item.tag, "left", item.left)
        set_attr(item.tag, "follows", "right|top")
        counts["pinned"] += 1

        make_room_on_left(items, item, old_left, overflow, counts)


def displayed_text(item: Box) -> str:
    return " ".join((item.element.text or item.tag.attrs.get("value", "")).split())


def runtime_box(item: Box, runtime_delta: int) -> Box:
    result = Box(item.element, item.tag, item.left, item.top, item.width, item.height)
    flags = follows(item.tag.attrs)
    if "left" in flags and "right" in flags:
        result.width += runtime_delta
    elif "right" in flags and "left" not in flags:
        result.left += runtime_delta
    return result


def widen_text(items: list[Box], inner_right: int, counts: Counter[str],
               text_replacements: dict[str, str]) -> None:
    design_right = max((item.right for item in items), default=inner_right)
    runtime_delta = inner_right - design_right
    for item in items:
        if item.element.tag != "text" or item.tag.attrs.get("wrap", item.tag.attrs.get("word_wrap", "false")).lower() == "true":
            continue
        value = displayed_text(item)
        if not value:
            continue
        if value in text_replacements:
            value = text_replacements[value]
            set_attr(item.tag, "value", value)
            counts["text_shortened"] += 1
        factor = 6.5 if item.tag.attrs.get("font") == "SansSerifSmall" else TEXT_FACTOR
        needed = math.ceil(max(len(line) for line in value.splitlines()) * factor)
        runtime_items = {id(other): runtime_box(other, runtime_delta) for other in items}
        runtime_item = runtime_items[id(item)]
        if needed <= runtime_item.width:
            continue
        delta = needed - runtime_item.width
        item_flags = follows(item.tag.attrs)
        if "right" in item_flags and "left" not in item_flags and needed <= runtime_item.right:
            item.left -= delta
            item.width += delta
            set_attr(item.tag, "left", item.left)
            set_attr(item.tag, "width", item.width)
            counts["text_widened"] += 1
            continue
        right_items = [other for other in items if other is not item
                       and same_row(runtime_item, runtime_items[id(other)])
                       and runtime_items[id(other)].left >= runtime_item.right]
        neighbour = min(right_items, key=lambda other: runtime_items[id(other)].left, default=None)
        available = (runtime_items[id(neighbour)].left if neighbour else inner_right) - runtime_item.left
        if available >= needed:
            if "right" in item_flags and "left" not in item_flags:
                item.left -= delta
                set_attr(item.tag, "left", item.left)
            item.width += delta
            set_attr(item.tag, "width", item.width)
            counts["text_widened"] += 1
            continue
        if neighbour is not None and neighbour.element.tag in STRETCH_TAGS and neighbour.width > delta:
            neighbour.left += delta
            neighbour.width -= delta
            set_attr(neighbour.tag, "left", neighbour.left)
            set_attr(neighbour.tag, "width", neighbour.width)
            item.width += delta
            set_attr(item.tag, "width", item.width)
            counts["text_widened"] += 1
            counts["text_neighbour_shrinks"] += 1
            continue
        # A later overlap pass may free this row (for example two header
        # texts).  The layout checker remains the authority for any text that
        # is still undersized after the transform.


def resolve_overlaps(items: list[Box], inner_right: int, counts: Counter[str]) -> None:
    design_right = max((item.right for item in items), default=inner_right)
    runtime_delta = inner_right - design_right
    for index, first in enumerate(items):
        for second in items[index + 1:]:
            first_runtime = runtime_box(first, runtime_delta)
            second_runtime = runtime_box(second, runtime_delta)
            if not same_row(first_runtime, second_runtime):
                continue
            horizontal_overlap = (first_runtime.right > second_runtime.left
                                  and second_runtime.right > first_runtime.left)
            if not horizontal_overlap:
                continue
            left, right = sorted((first, second), key=lambda item: runtime_box(item, runtime_delta).left)
            right_flags = follows(right.tag.attrs)
            if "right" in right_flags and "left" not in right_flags:
                if left.element.tag in STRETCH_TAGS:
                    set_attr(left.tag, "follows", "left|right|top")
                    counts["overlaps_stretched"] += 1
                else:
                    target_right = runtime_box(right, runtime_delta).left
                    new_width = target_right - runtime_box(left, runtime_delta).left
                    if new_width >= natural_width(left):
                        left.width = new_width
                        set_attr(left.tag, "width", new_width)
                        set_attr(left.tag, "follows", "left|top")
                        counts["overlaps_static_shrinks"] += 1
                continue
            if first.element.tag == second.element.tag == "text":
                new_left = left.right + 4
                if new_left < inner_right and right.left < new_left:
                    right.left = new_left
                    right.width = inner_right - new_left
                    set_attr(right.tag, "left", right.left)
                    set_attr(right.tag, "width", right.width)
                    set_attr(right.tag, "follows", "left|top")
                    counts["overlaps_reflowed"] += 1
            elif (left.element.tag == "text"
                  and left.tag.attrs.get("wrap", left.tag.attrs.get("word_wrap", "false")).lower() == "true"
                  and right.element.tag == "button"
                  and right.top >= left.top):
                overlap = runtime_box(left, runtime_delta).bottom - runtime_box(right, runtime_delta).top
                if overlap > 0:
                    right.top += overlap
                    set_attr(right.tag, "top", right.top)
                    counts["overlaps_moved_down"] += 1


def apply_overrides(root: ElementTree.Element, tags: dict[int, StartTag],
                    overrides: dict[str, object], counts: Counter[str]) -> None:
    widgets = overrides.get("widgets", {})
    values = overrides.get("values", {})
    if not isinstance(widgets, dict) or not isinstance(values, dict):
        raise ValueError("overrides widgets/values must be objects")
    for element in root.iter():
        attrs: object | None = widgets.get(element.get("name", ""))
        if attrs is None:
            current_value = element.get("value", " ".join((element.text or "").split()))
            attrs = values.get(current_value)
        if attrs is None:
            continue
        if not isinstance(attrs, dict):
            raise ValueError("each override must be an attribute object")
        for key, value in attrs.items():
            set_attr(tags[id(element)], str(key), str(value))
            counts["panel_overrides"] += 1


def transform(text: str, overrides: dict[str, object] | None = None,
              base_sha: str = "unversioned") -> tuple[str, Counter[str]]:
    overrides = overrides or {}
    if APPLIED_MARKER_RE.search(text):
        return text, Counter()
    root = ElementTree.fromstring(text)
    tags = parse_start_tags(text, root)
    counts: Counter[str] = Counter()
    text_replacements = overrides.get("text_replacements", {})
    if not isinstance(text_replacements, dict):
        raise ValueError("text_replacements must be an object")
    host_widths = overrides.get("host_widths", [])
    if not isinstance(host_widths, list) or any(not isinstance(value, int) for value in host_widths):
        raise ValueError("host_widths must be an array of integers")
    grid_overrides = overrides.get("grids", {})
    if not isinstance(grid_overrides, dict):
        raise ValueError("grids must be an object")
    for grid in root.iter("flow_grid"):
        attrs = grid_overrides.get(grid.get("name", ""), {})
        if not isinstance(attrs, dict):
            raise ValueError("each grid override must be an attribute object")
        for key, value in attrs.items():
            set_attr(tags[id(grid)], str(key), str(value))
            counts["grid_overrides"] += 1
    for grid in root.iter("flow_grid"):
        grid_tag = tags[id(grid)]
        if grid_tag.attrs.get("fit_cards", "false").lower() != "true":
            continue
        candidate_widths = [fitted_width(grid_tag, width) for width in host_widths]
        runtime_width = min(candidate_widths or [fitted_width(grid_tag)])
        for card in (child for child in grid if child.tag == "panel"):
            card_tag = tags[id(card)]
            design_width = max(1, integer(card_tag.attrs, "width", runtime_width))
            card_items = boxes(card, tags)
            inner_right = runtime_width - existing_right_margin(card_items, design_width)
            fit_right_edges(card_items, inner_right, counts)
            widen_text(card_items, inner_right, counts, text_replacements)
            resolve_overlaps(card_items, inner_right, counts)

    # Explicit per-panel exceptions are applied last.  They document rows
    # whose density cannot be resolved by the generic one-suffix rule.
    apply_overrides(root, tags, overrides, counts)

    replacements = [(tag.start, tag.end, render_tag(tag)) for tag in tags.values() if tag.updates]
    result = text
    for start, end, replacement in sorted(replacements, reverse=True):
        result = result[:start] + replacement + result[end:]
    newline = "\r\n" if "\r\n" in result else "\n"
    declaration_end = result.find("?>")
    marker = (f"<!-- xui-fit-card-children: one-shot transform from {base_sha}; "
              "do not rerun; edit by hand -->")
    if declaration_end >= 0:
        insert_at = declaration_end + 2
        result = result[:insert_at] + newline + marker + result[insert_at:]
    else:
        result = marker + newline + result
    return result, counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="XUI panel containing a fit_cards flow_grid")
    parser.add_argument("--write", action="store_true", help="replace the file (default: print a diff)")
    parser.add_argument("--overrides", type=Path,
                        help="optional JSON host widths, text replacements, and documented panel exceptions")
    parser.add_argument("--source-revision",
                        help="transform this file as stored at a git revision (for reproducible one-shot output)")
    args = parser.parse_args()
    try:
        destination = args.file.read_bytes().decode("utf-8")
        base_sha = subprocess.run(
            ["git", "rev-parse", args.source_revision or "HEAD"], check=True,
            capture_output=True, text=True,
        ).stdout.strip()
        if args.source_revision:
            original = subprocess.run(
                ["git", "show", f"{args.source_revision}:{args.file.as_posix()}"], check=True,
                capture_output=True,
            ).stdout.decode("utf-8")
        else:
            original = destination
        overrides = json.loads(args.overrides.read_text(encoding="utf-8")) if args.overrides else {}
        updated, counts = transform(original, overrides, base_sha)
    except (OSError, UnicodeError, subprocess.CalledProcessError, json.JSONDecodeError,
            ElementTree.ParseError, ValueError) as exc:
        print(f"{args.file}: {exc}", file=sys.stderr)
        return 2

    if updated == destination:
        print(f"{args.file}: no changes")
        return 0
    if args.write:
        args.file.write_bytes(updated.encode("utf-8"))
        print(f"{args.file}: wrote {sum(counts.values())} adjustment(s): "
              + ", ".join(f"{key}={value}" for key, value in sorted(counts.items())))
    else:
        print("".join(difflib.unified_diff(original.splitlines(True), updated.splitlines(True),
                                           fromfile=str(args.file), tofile=str(args.file))))
        print("Dry run; pass --write to apply. "
              + ", ".join(f"{key}={value}" for key, value in sorted(counts.items())), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
