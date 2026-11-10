#!/usr/bin/env python3
"""Check that Director multi-select kit lists keep their multi-selection.

Usage::

    python scripts/code_tools/director_list_select_check.py --check
    python scripts/code_tools/director_list_select_check.py --selftest

Rules (exit 1 on any finding):
- Every scroll list with multi_select="true" on a generated Director page
  (panel_dpage_*.xml, floater_director_v2.xml) is listed in REVIEWED_LISTS with
  the panel source and member that own it. A new multi-select list must be
  reviewed for the rules below before it is added here.
- No owner restores a selection with a loop of selectByID(): in this viewer
  selectByID() clears every other row, so such a loop keeps only the last row.
- The Clone list (ALPanelGhostStudio) uses the pure rules of
  indra/newview/alkitlistselection.h at its three selection points:
  * refreshList() restores every selected row after a rebuild
    (restoreRows + selectMultiple, never selectByID) and puts the list's
    Shift-click anchor back on its row (anchorAfterRebuild + setLastSelectedItem);
  * draw() mirrors the shared primary through mirrorAction(), with the
    mouse-capture guard, so a click in progress is not collapsed;
  * onListSelect() makes the clicked row the primary (primaryAfterCommit) and
    selection() derives the primary row from it (primaryRow).
- A list that opts into the selected-row bar draws its cells a gutter in from the row
  edge; LLScrollListCtrl moves hit testing / hover (getColumnIndexFromOffset), cell
  rectangles (getColumnOffsetFromIndex), header positions and the width the columns share
  with it, through the pure rules of indra/llui/llscrolllistgutter.h. Lists without the
  bar (gutter 0) are unchanged.

Standard library only. Exit 2 is for command-line or missing-input errors.
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
NEWVIEW = os.path.join(REPO, "indra", "newview")
XUI = os.path.join(NEWVIEW, "skins", "default", "xui", "en")
LLUI = os.path.join(REPO, "indra", "llui")
CTRL_CPP = "llscrolllistctrl.cpp"
GUTTER_H = "llscrolllistgutter.h"
GHOST_CPP = "alpanelghoststudio.cpp"

# list name -> (owning source, list member). Reviewed for the rules in the module docstring.
REVIEWED_LISTS = {
    "ghost_list": (GHOST_CPP, "mList"),
    "db_dancer_list": ("alpaneldanceball.cpp", "mDancerList"),
    "db_blocking_list": ("alpaneldanceball.cpp", "mBlockingList"),
    "target_list": ("alpanelavatarlightresponse.cpp", "mTargetList"),
    "cast_list": ("llfloaterdirector.cpp", "mCastList"),
}

GHOST_PINS = (
    ("const std::vector<LLUUID> prev_rows = selectedRowValues(); std::vector<LLUUID> rebuilt_rows; mList->deleteAllItems();",
     "refreshList() no longer captures every selected row before the rebuild"),
    ("rebuilt_rows.push_back(inst.mId);", "refreshList() no longer records rebuilt clone rows"),
    ("rebuilt_rows.push_back(group->mId);", "refreshList() no longer records rebuilt group rows"),
    ("const std::vector<LLUUID> kept_rows = ALKitListSelection::restoreRows(prev_rows, rebuilt_rows); "
     "if (!kept_rows.empty()) { mList->selectMultiple(kept_rows); }",
     "refreshList() no longer restores every selected row"),
    ("LLScrollListItem* anchor_item = mList->getLastSelectedItem(); "
     "const LLUUID prev_anchor = anchor_item ? anchor_item->getValue().asUUID() : LLUUID::null;",
     "refreshList() no longer remembers the Shift-click anchor"),
    ("const LLUUID kept_anchor = ALKitListSelection::anchorAfterRebuild(prev_anchor, kept_rows); "
     "if (kept_anchor.notNull()) { mList->setLastSelectedItem(LLSD(kept_anchor)); }",
     "refreshList() no longer restores the Shift-click anchor"),
    ("const bool external_selection = shared_selection_revision != mObservedSelectionRevision;",
     "draw() no longer separates an external change of the shared primary"),
    ("switch (ALKitListSelection::mirrorAction( external_selection, mList->hasMouseCapture(), desired_row, "
     "listed_row, listed.mRows))",
     "draw() no longer mirrors the shared primary through mirrorAction with the mouse-capture guard"),
    ("case ALKitListSelection::MIRROR_ADOPT: mPrimaryRow = desired_row; noteListSelectionChanged(); break; "
     "case ALKitListSelection::MIRROR_KEEP: break;",
     "draw() mirror no longer keeps or adopts without touching the list"),
    ("mPrimaryRow = ALKitListSelection::primaryAfterCommit( selectedRowValues(), "
     "hit ? hit->getValue().asUUID() : LLUUID::null, last ? last->getValue().asUUID() : LLUUID::null, mPrimaryRow);",
     "onListSelect() no longer makes the clicked row the primary"),
    ("cache.mRows = selectedRowValues(); cache.mRow = ALKitListSelection::primaryRow(cache.mRows, mPrimaryRow);",
     "selection() no longer derives the primary row from the remembered click"),
)


GUTTER_PINS = {
    CTRL_CPP: (
        ("x = LLScrollListGutter::columnLocalX(x, getSelectedBarGutter());",
         "getColumnIndexFromOffset() no longer shifts hit testing by the gutter"),
        ("S32 column_offset = LLScrollListGutter::columnOffset(0, getSelectedBarGutter());",
         "getColumnOffsetFromIndex() no longer starts after the gutter"),
        ("return LLScrollListGutter::availableWidth(mItemListRect.getWidth(), getSelectedBarGutter());",
         "the column area width no longer excludes the gutter"),
        ("new_width = (getColumnAreaWidth() - mTotalStaticColumnWidth - mTotalColumnPadding) / mNumDynamicWidthColumns;",
         "dynamic column widths no longer use the column area width"),
        ("new_width = (S32)ll_round(column->mRelWidth*getColumnAreaWidth());",
         "relative column widths no longer use the column area width"),
        ("S32 left = mItemListRect.mLeft + getSelectedBarGutter(); LLScrollColumnHeader* last_header = NULL;",
         "column headers no longer start after the gutter"),
        ("S32 left = mItemListRect.mLeft + getSelectedBarGutter(); for (column_map_t::iterator itor = mColumns.begin();",
         "new column headers no longer start after the gutter"),
        ("item->draw(item_rect, fg_color % alpha, hover_color% alpha, select_color% alpha, highlight_color % alpha, "
         "mColumnPadding, getSelectedBarGutter());",
         "rows no longer draw their cells after the gutter"),
    ),
    GUTTER_H: (
        ("return std::max(0, x - gutter);", "a click in the gutter can fall off the column walk"),
        ("return columns_before + gutter;", "column offsets no longer include the gutter"),
        ("return std::max(0, list_width - gutter);", "the available width no longer excludes the gutter"),
    ),
}


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    return re.sub(r"//[^\n]*", " ", source)


def normalise(source):
    return re.sub(r"\s+", " ", strip_comments(source)).strip()


def function_body(source, signature):
    """Text of the function whose definition starts with signature (comments stripped)."""
    start = source.find(signature)
    if start < 0:
        return ""
    brace = source.find("{", start)
    depth, index = 0, brace
    while index < len(source):
        depth += {"{": 1, "}": -1}.get(source[index], 0)
        if depth == 0:
            return source[brace:index + 1]
        index += 1
    return ""


def multi_select_lists(xui_texts):
    names = {}
    for path, text in sorted(xui_texts.items()):
        for node in ET.fromstring(text.encode("utf-8")).iter():
            if node.get("multi_select") == "true":
                names.setdefault(node.get("name", "<unnamed>"), path)
    return names


def findings(xui_texts, sources):
    out = []
    lists = multi_select_lists(xui_texts)
    for name, path in sorted(lists.items()):
        if name not in REVIEWED_LISTS:
            out.append(f"{path}: multi-select list {name!r} is not reviewed (REVIEWED_LISTS)")
    for name, (source_name, member) in sorted(REVIEWED_LISTS.items()):
        if name not in lists:
            out.append(f"reviewed list {name!r} is no longer a multi-select Director list")
            continue
        source = sources.get(source_name)
        if source is None:
            out.append(f"{source_name}: missing owner of {name!r}")
            continue
        plain = normalise(source)
        if not re.search(rf'{member} = [^;]*"{name}"', plain):
            out.append(f"{source_name}: {member} no longer binds {name!r}")
        if re.search(rf"\bfor \([^;{{}}]*\) \{{? ?(?:\w+\.)?{member}->selectByID\(", plain):
            out.append(f"{source_name}: {member} restores a selection with a selectByID() loop "
                       "(each call clears the other rows)")
    ghost = sources.get(GHOST_CPP)
    if ghost is not None:
        plain = normalise(ghost)
        for needle, message in GHOST_PINS:
            if normalise(needle) not in plain:
                out.append(f"{GHOST_CPP}: {message}")
        rebuild = normalise(function_body(strip_comments(ghost), "void ALPanelGhostStudio::refreshList(bool force_now)"))
        if not rebuild:
            out.append(f"{GHOST_CPP}: refreshList() not found")
        elif "selectByID(" in rebuild or "getFirstSelected(" in rebuild:
            out.append(f"{GHOST_CPP}: refreshList() restores a single row (selectByID/getFirstSelected)")
        header = sources.get("alkitlistselection.h")
        if header is None or any(rule not in header for rule in ("restoreRows", "mirrorAction", "anchorAfterRebuild")):
            out.append("alkitlistselection.h: missing the pure selection rules")
    for name, pins in GUTTER_PINS.items():
        text = sources.get(name)
        if text is None:
            out.append(f"{name}: missing")
            continue
        plain = normalise(text)
        for needle, message in pins:
            if normalise(needle) not in plain:
                out.append(f"{name}: {message}")
    return out


def load_inputs():
    xui_texts = {}
    for path in sorted(glob.glob(os.path.join(XUI, "panel_dpage_*.xml"))
                       + [os.path.join(XUI, "floater_director_v2.xml")]):
        with open(path, encoding="utf-8") as fh:
            xui_texts[os.path.relpath(path, REPO).replace(os.sep, "/")] = fh.read()
    sources = {}
    for name in sorted({owner for owner, _member in REVIEWED_LISTS.values()} | {"alkitlistselection.h"}):
        path = os.path.join(NEWVIEW, name)
        if os.path.isfile(path):
            with open(path, encoding="utf-8", errors="replace") as fh:
                sources[name] = fh.read()
    for name in (CTRL_CPP, GUTTER_H):
        path = os.path.join(LLUI, name)
        if os.path.isfile(path):
            with open(path, encoding="utf-8", errors="replace") as fh:
                sources[name] = fh.read()
    return xui_texts, sources


def run_check():
    xui_texts, sources = load_inputs()
    if len(xui_texts) < 2:
        print("no Director XUI files found", file=sys.stderr)
        return 2
    found = findings(xui_texts, sources)
    for item in found:
        print(f"FAIL  {item}")
    print(f"{len(multi_select_lists(xui_texts))} multi-select Director list(s), {len(found)} finding(s)")
    return 1 if found else 0


def selftest():
    xui_texts, sources = load_inputs()
    failures = []
    if findings(xui_texts, sources):
        failures.append(f"production sources fail: {findings(xui_texts, sources)}")
    ghost = sources.get(GHOST_CPP, "")
    dance = sources.get("alpaneldanceball.cpp", "")
    some_page = next(iter(sorted(xui_texts)))
    ctrl = sources.get(CTRL_CPP, "")
    gutter = sources.get(GUTTER_H, "")

    def ctrl_swap(old, new):
        return dict(sources, **{CTRL_CPP: ctrl.replace(old, new)})

    def gutter_swap(old, new):
        return dict(sources, **{GUTTER_H: gutter.replace(old, new)})

    def ghost_swap(old, new):
        return dict(sources, **{GHOST_CPP: ghost.replace(old, new)})

    # Planted negatives. Each restores one way a refresh or the mirror collapsed the selection.
    mutations = [
        ("rebuild restores only the first selected row",
         ghost_swap("        mList->selectMultiple(kept_rows);", "        mList->selectByID(kept_rows.front());"), xui_texts),
        ("rebuild restores from getFirstSelected",
         ghost_swap("    const std::vector<LLUUID> prev_rows = selectedRowValues();",
                    "    const std::vector<LLUUID> prev_rows = { mList->getFirstSelected() ? "
                    "mList->getFirstSelected()->getValue().asUUID() : LLUUID::null };"), xui_texts),
        ("rebuild forgets the rebuilt rows",
         ghost_swap("        rebuilt_rows.push_back(inst.mId);\n", ""), xui_texts),
        ("rebuild loses the Shift-click anchor",
         ghost_swap("        mList->setLastSelectedItem(LLSD(kept_anchor));", ""), xui_texts),
        ("rebuild restores the anchor before the rows exist",
         ghost_swap("    LLScrollListItem* anchor_item = mList->getLastSelectedItem();",
                    "    LLScrollListItem* anchor_item = mList->getFirstSelected();"), xui_texts),
        ("mirror ignores a click in progress",
         ghost_swap("external_selection, mList->hasMouseCapture(), desired_row,",
                    "external_selection, false, desired_row,"), xui_texts),
        ("mirror treats every frame as an external change",
         ghost_swap("        shared_selection_revision != mObservedSelectionRevision;\n    const SelectionCache& listed",
                    "        true;\n    const SelectionCache& listed"), xui_texts),
        ("mirror compares against the first selected row",
         ghost_swap("    cache.mRow = ALKitListSelection::primaryRow(cache.mRows, mPrimaryRow);",
                    "    cache.mRow = cache.mRows.empty() ? LLUUID::null : cache.mRows.front();"), xui_texts),
        ("adopt collapses the list",
         ghost_swap("    case ALKitListSelection::MIRROR_ADOPT:\n        mPrimaryRow = desired_row;",
                    "    case ALKitListSelection::MIRROR_ADOPT:\n        mList->deselectAllItems(true);\n"
                    "        mPrimaryRow = desired_row;"), xui_texts),
        ("commit publishes the top row instead of the clicked row",
         ghost_swap("        mPrimaryRow = ALKitListSelection::primaryAfterCommit(", "        mPrimaryRow = first_row_only("),
         xui_texts),
        ("selectByID loop restore in the Dance Ball blocking list",
         dict(sources, **{"alpaneldanceball.cpp": dance.replace(
             "    if (!selected.empty()) mBlockingList->selectMultiple(selected);",
             "    for (const LLUUID& id : selected) mBlockingList->selectByID(id);")}), xui_texts),
        ("braced selectByID loop restore",
         dict(sources, **{"alpaneldanceball.cpp": dance.replace(
             "    if (!selected.empty()) mBlockingList->selectMultiple(selected);",
             "    for (const LLUUID& id : selected)\n    {\n        mBlockingList->selectByID(id);\n    }")}), xui_texts),
        ("unreviewed multi-select list",
         sources, dict(xui_texts, **{some_page: xui_texts[some_page].replace(
             "</panel>", '<scroll_list name="new_unreviewed_list" multi_select="true" /></panel>', 1)})),
        ("pure rules header missing", {k: v for k, v in sources.items() if k != "alkitlistselection.h"}, xui_texts),
        # Gutter hit testing (review round 2): the bar moves what is drawn, so it must move what is hit.
        ("hit testing ignores the gutter",
         ctrl_swap("    x = LLScrollListGutter::columnLocalX(x, getSelectedBarGutter());\n", ""), xui_texts),
        ("cell rectangles ignore the gutter",
         ctrl_swap("LLScrollListGutter::columnOffset(0, getSelectedBarGutter())", "0"), xui_texts),
        ("columns share the width under the gutter",
         ctrl_swap("    return LLScrollListGutter::availableWidth(mItemListRect.getWidth(), getSelectedBarGutter());",
                   "    return mItemListRect.getWidth();"), xui_texts),
        ("dynamic columns ignore the column area",
         ctrl_swap("new_width = (getColumnAreaWidth() - mTotalStaticColumnWidth",
                   "new_width = (mItemListRect.getWidth() - mTotalStaticColumnWidth"), xui_texts),
        ("headers ignore the gutter",
         ctrl_swap("    S32 left = mItemListRect.mLeft + getSelectedBarGutter();\n    LLScrollColumnHeader* last_header",
                   "    S32 left = mItemListRect.mLeft;\n    LLScrollColumnHeader* last_header"), xui_texts),
        ("gutter click falls off the column walk",
         gutter_swap("return std::max(0, x - gutter);", "return x - gutter;"), xui_texts),
        ("gutter helper loses the width",
         gutter_swap("return std::max(0, list_width - gutter);", "return list_width;"), xui_texts),
    ]
    for label, mutated_sources, mutated_xui in mutations:
        if mutated_sources == sources and mutated_xui == xui_texts:
            failures.append(f"mutation '{label}' did not apply")
        elif not findings(mutated_xui, mutated_sources):
            failures.append(f"mutation '{label}' passes")
        else:
            print(f"ok    rejected: {label}")
    if failures:
        for item in failures:
            print("SELFTEST FAIL:", item)
        return 1
    print("selftest passed")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="check the Director multi-select lists")
    parser.add_argument("--selftest", action="store_true", help="run planted must-fail mutations")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.check:
        return run_check()
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
