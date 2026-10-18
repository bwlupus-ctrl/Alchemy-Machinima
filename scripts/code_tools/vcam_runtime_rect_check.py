#!/usr/bin/env python3
"""Audit ACTUAL viewer ui_vcam_source_rects.csv output; never synthesize layout."""
import argparse
import csv
import copy
import json
from collections import defaultdict
from pathlib import Path


def audit(records, require_media_modes=False):
    findings = []
    groups = defaultdict(list)
    modes = defaultdict(set)
    for record in records:
        groups[tuple(record[k] for k in ("size", "state", "sample", "card"))].append(record)
        if record["card"] == "media_source":
            modes[record["size"]].add(record["mode"])
    if not groups:
        findings.append("missing runtime Source card rectangles")
    if require_media_modes:
        sizes = {record["size"] for record in records}
        for size in sizes:
            if not {"url", "face"} <= modes[size]:
                findings.append(f"{size}: missing runtime media Source url/face coverage")
    for group, widgets in groups.items():
        prefix = "/".join(group)
        rows = {w["row"]: w for w in widgets if w["role"] == "row"}

        def rect(widget):
            return tuple(float(widget[k]) for k in ("left", "bottom", "right", "top"))

        def overlap(a, b):
            return min(a[2], b[2]) > max(a[0], b[0]) and min(a[3], b[3]) > max(a[1], b[1])

        row_list = list(rows.values())
        for i, row in enumerate(row_list):
            for other in row_list[i + 1:]:
                if overlap(rect(row), rect(other)):
                    findings.append(f"{prefix}: sibling-row overlap {row['row']} / {other['row']}")
        by_row = defaultdict(dict)
        for widget in widgets:
            owner = rows.get(widget["row"])
            if owner is None:
                findings.append(f"{prefix}: missing owner for {widget['widget']}")
                continue
            box, bounds = rect(widget), rect(owner)
            if box[2] <= box[0] or box[3] <= box[1]:
                findings.append(f"{prefix}: empty rect {widget['widget']}")
            if widget["role"] != "row":
                if box[0] < bounds[0] or box[1] < bounds[1] or box[2] > bounds[2] or box[3] > bounds[3]:
                    findings.append(f"{prefix}: widget escapes row {widget['widget']}")
                for other in row_list:
                    if other["row"] != widget["row"] and overlap(box, rect(other)):
                        findings.append(f"{prefix}: widget overlaps sibling row {widget['widget']} / {other['row']}")
            by_row[widget["row"]][widget["role"]] = widget
        for row, roles in by_row.items():
            if "label" not in roles or "value" not in roles:
                continue
            label, value = roles["label"]["baseline"], roles["value"]["baseline"]
            if not label or not value:
                findings.append(f"{prefix}: missing text baseline {row}")
            elif abs(float(label) - float(value)) > 3:
                findings.append(f"{prefix}: label/value baseline mismatch >3px {row}")
    return findings


def selftest():
    fixture = Path(__file__).with_name("xui_fixtures") / "vcam_runtime_rects_good.csv"
    good = list(csv.DictReader(fixture.open(encoding="utf-8")))
    assert not audit(good, True), audit(good, True)
    cases = []
    for role in ("action", "value"):
        broken = copy.deepcopy(good)
        widget = next(w for w in broken if w["role"] == role)
        for coordinate in ("bottom", "top"):
            widget[coordinate] = str(float(widget[coordinate]) + 100)
        assert any("escapes row" in f for f in audit(broken)), role
        cases.append(f"{role} moved 100px: rejected")
    broken = copy.deepcopy(good)
    label = next(w for w in broken if w["role"] == "label")
    label["baseline"] = str(float(label["baseline"]) + 4)
    assert any("baseline mismatch" in f for f in audit(broken))
    cases.append("baseline moved 4px: rejected")
    label["baseline"] = str(float(label["baseline"]) - 1)
    assert not any("baseline mismatch" in f for f in audit(broken))
    broken = copy.deepcopy(good)
    row = next(w for w in broken if w["row"] == "status" and w["role"] == "row")
    row["top"] = "200"
    assert any("sibling-row overlap" in f for f in audit(broken))
    cases.append("sibling row overlap: rejected")
    assert audit([], True)
    print("PASS good runtime rect fixture; " + "; ".join(cases) + "; 3px baseline allowed; missing dump rejected")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, nargs="?")
    parser.add_argument("--require-media-modes", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.dump:
        parser.error("provide the viewer CSV dump or --selftest")
    findings = audit(list(csv.DictReader(args.dump.open(encoding="utf-8-sig"))), args.require_media_modes)
    print(json.dumps({"dump": str(args.dump), "findings": findings}, indent=2))
    return int(bool(findings))


if __name__ == "__main__":
    raise SystemExit(main())
