#!/usr/bin/env python3
"""Summarize and compare Director functional self-test CSV files."""

from __future__ import annotations

import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path


KEY = ("page", "control", "kind", "action", "expected")
FAIL_STATUSES = {"FAIL", "KNOWN_GAP"}


def csv_path(value: str) -> Path:
    path = Path(value)
    return path / "functional.csv" if path.is_dir() else path


def load(value: str) -> list[dict[str, str]]:
    path = csv_path(value)
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    required = {*KEY, "ui_version", "PASS/FAIL/SKIP", "reason"}
    missing = required.difference(rows[0] if rows else ())
    if missing:
        raise SystemExit(f"{path}: missing columns: {', '.join(sorted(missing))}")
    bad_skip = [row for row in rows
                if row["PASS/FAIL/SKIP"] == "SKIP" and not row["reason"].strip()]
    if bad_skip:
        raise SystemExit(f"{path}: {len(bad_skip)} SKIP row(s) lack an explicit reason")
    unknown = sorted({row["PASS/FAIL/SKIP"] for row in rows}
                     - {"PASS", "FAIL", "SKIP", "KNOWN_GAP"})
    if unknown:
        raise SystemExit(f"{path}: unknown status values: {', '.join(unknown)}")
    return rows


def identity(row: dict[str, str]) -> tuple[str, ...]:
    return tuple(row[name] for name in KEY)


def counts(rows: list[dict[str, str]]) -> dict[tuple[str, str], Counter[str]]:
    result: dict[tuple[str, str], Counter[str]] = defaultdict(Counter)
    for row in rows:
        result[(row["ui_version"], row["page"])][row["PASS/FAIL/SKIP"]] += 1
    return result


def compare(left: list[dict[str, str]], right: list[dict[str, str]],
            left_name: str, right_name: str) -> list[str]:
    lmap = {identity(row): row for row in left}
    rmap = {identity(row): row for row in right}
    lines = [f"## {left_name} vs {right_name}", ""]
    changes = 0
    for key in sorted(lmap.keys() | rmap.keys()):
        lrow, rrow = lmap.get(key), rmap.get(key)
        lstatus = lrow["PASS/FAIL/SKIP"] if lrow else "MISSING"
        rstatus = rrow["PASS/FAIL/SKIP"] if rrow else "MISSING"
        if lstatus == rstatus:
            continue
        changes += 1
        reason = (rrow or lrow or {}).get("reason", "")
        lines.append(f"- `{' / '.join(key)}`: {lstatus} -> {rstatus}" +
                     (f" ({reason})" if reason else ""))
    if not changes:
        lines.append("- No status differences.")
    return lines


def render(rows: list[dict[str, str]], title: str) -> list[str]:
    lines = [f"# Director functional self-test: {title}", "",
             "| UI | Page | PASS | FAIL | GAP | SKIP |", "|---|---|---:|---:|---:|---:|"]
    for (version, page), count in sorted(counts(rows).items()):
        lines.append(f"| {version} | {page} | {count['PASS']} | {count['FAIL']} | {count['KNOWN_GAP']} | {count['SKIP']} |")
    total = Counter(row["PASS/FAIL/SKIP"] for row in rows)
    lines += ["", f"Total: {total['PASS']} PASS, {total['FAIL']} FAIL, "
             f"{total['KNOWN_GAP']} KNOWN_GAP (failing), {total['SKIP']} SKIP.", ""]
    versions = sorted({row["ui_version"] for row in rows})
    if {"v1", "v2"}.issubset(versions):
        lines += compare([r for r in rows if r["ui_version"] == "v1"],
                         [r for r in rows if r["ui_version"] == "v2"], "v1", "v2")
    else:
        lines += ["## v1 vs v2", "", "- Only " + ", ".join(versions) +
                  " is present in this run; pass a second run to compare."]
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", help="functional.csv or its run directory")
    parser.add_argument("right", nargs="?", help="optional comparison CSV/run directory")
    parser.add_argument("-o", "--output", help="write Markdown here (default: stdout)")
    args = parser.parse_args()
    left = load(args.left)
    right = load(args.right) if args.right else []
    lines = render(left, csv_path(args.left).parent.name)
    if args.right:
        lines += [""] + compare(left, right,
                                 csv_path(args.left).parent.name,
                                 csv_path(args.right).parent.name)
    text = "\n".join(lines).rstrip() + "\n"
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8", newline="\n")
    else:
        print(text, end="")
    failed = any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in left)
    if args.right:
        failed = failed or any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in right)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
