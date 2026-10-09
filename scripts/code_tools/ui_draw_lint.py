#!/usr/bin/env python3
"""Reject new expensive operations reachable from per-frame UI paths."""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = Path(__file__).with_name("ui_draw_lint_baseline.json")
FIXTURES = Path(__file__).with_name("ui_draw_fixtures")
RULES = {"lookup", "lazy-find", "setting", "setting-write", "raw-write", "missing-zone"}
ROOT_NAMES = {"draw", "onIdle", "idle", "tick", "doIdle", "onIdleUpdate"}
GATE_RE = re.compile(
    r"\.due\s*\(|\.changed\s*\(|\.isStale\s*\(|hasExpired|"
    r"checkExpirationAndReset|m\w*(?:Dirty|Pending|NeedsRefresh|NeedsRebuild|Stale)|"
    r"\w*Revision\s*(?:==|!=|<|>)|mLast\w+\s*!=|getElapsedTimeF32\s*\(.*[<>]"
)
LOOKUP_RE = re.compile(
    r"(?:getChild|findChild|getChildRef)\s*<|"
    r"(?:getChildView|findChildView|hasChild|childSet\w*|childGet\w*|"
    r"childIs\w*|childHasFocus)\s*\(|childHasKeyboardFocus\s*\(\s*\""
)
SETTING_RE = re.compile(
    r"gSaved(?:PerAccount)?Settings\s*\.\s*(?:get\w+|getControl)\s*\(\s*\""
)
SETTING_WRITE_RE = re.compile(
    r"gSaved(?:PerAccount)?Settings\s*\.\s*set\w+\s*\(|"
    r"(?:control|ctrl|setting|variable)\s*->\s*setValue\s*\(", re.I
)
RAW_RE = re.compile(
    r"(?:->|\.)(?:setText|setValue|setToolTip|clearRows|deleteAllItems|removeall|"
    r"setLabel|setTextArg|setLabelArg)\s*\("
)
SUP_RE = re.compile(
    r"//\s*ui-draw-lint:\s*(allow(?:-function)?)\(([^)]*)\)\s*(.*)$"
)


@dataclass
class Function:
    cls: str
    name: str
    start: int
    body: int
    end: int
    start_line: int

    @property
    def full(self) -> str:
        return f"{self.cls}::{self.name}" if self.cls else self.name


@dataclass
class Finding:
    file: str
    line: int
    rule: str
    severity: str
    root: str
    via: str
    text: str
    function: str
    source: str
    ordinal: int = 0
    baselined: bool = False

    def fingerprint(self) -> str:
        normalized = " ".join(self.source.split())
        return f"{self.file} | {self.function} | {self.rule} | {normalized} | {self.ordinal}"


def blank_comments_and_strings(text: str) -> str:
    out = list(text)
    i, n = 0, len(text)
    state = "code"
    while i < n:
        if state == "code" and text.startswith("//", i):
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif state == "code" and text.startswith("/*", i):
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and not text.startswith("*/", i):
                if text[i] != "\n": out[i] = " "
                i += 1
            if i < n:
                out[i] = out[i + 1] = " "
                i += 2
        elif state == "code" and text[i] in "\"'":
            quote = text[i]
            i += 1  # retain opening quote for by-name recognition
            while i < n:
                if text[i] == "\\":
                    if text[i] != "\n": out[i] = " "
                    i += 1
                    if i < n and text[i] != "\n": out[i] = " "
                    i += 1
                elif text[i] == quote:
                    out[i] = " "
                    i += 1
                    break
                else:
                    if text[i] != "\n": out[i] = " "
                    i += 1
        else:
            i += 1
    return "".join(out)


def matching_brace(clean: str, opening: int) -> int | None:
    depth = 0
    for i in range(opening, len(clean)):
        if clean[i] == "{": depth += 1
        elif clean[i] == "}":
            depth -= 1
            if depth == 0: return i
    return None


def line_no(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def functions(text: str, clean: str) -> list[Function]:
    found: list[Function] = []
    occupied: list[tuple[int, int]] = []
    outside = re.compile(
        r"(?m)^[ \t]*(?:template\s*<[^;{}]+>\s*)?(?:[\w:<>,~*&]+[ \t]+)+"
        r"(?P<class>[A-Za-z_]\w*)::(?P<name>~?[A-Za-z_]\w*)\s*\([^;{}]*\)"
        r"(?:\s*(?:const|override|noexcept|final))*\s*\{"
    )
    for match in outside.finditer(clean):
        opening = match.end() - 1
        end = matching_brace(clean, opening)
        if end is not None:
            found.append(Function(match["class"], match["name"], match.start(), opening, end,
                                  line_no(text, match.start())))
            occupied.append((match.start(), end))

    class_re = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)[^;{]*\{")
    method_re = re.compile(
        r"(?m)^[ \t]*(?:virtual\s+|static\s+|inline\s+|explicit\s+)*"
        r"(?:[\w:<>,~*&]+[ \t]+)*(?P<name>~?[A-Za-z_]\w*)\s*\([^;{}]*\)"
        r"(?:\s*(?:const|override|noexcept|final))*\s*\{"
    )
    for cm in class_re.finditer(clean):
        class_end = matching_brace(clean, cm.end() - 1)
        if class_end is None: continue
        for mm in method_re.finditer(clean, cm.end(), class_end):
            if any(a <= mm.start() <= b for a, b in occupied): continue
            opening = mm.end() - 1
            end = matching_brace(clean, opening)
            if end is None or end > class_end: continue
            found.append(Function(cm.group(1), mm["name"], mm.start(), opening, end,
                                  line_no(text, mm.start())))
    free_re = re.compile(
        r"(?m)^[ \t]*(?:static\s+|inline\s+)*(?:[\w:<>,~*&]+[ \t]+)+"
        r"(?P<name>[A-Za-z_]\w*)\s*\([^;{}]*\)"
        r"(?:\s*(?:const|noexcept))*\s*\{"
    )
    for match in free_re.finditer(clean):
        if any(a <= match.start() <= b for a, b in occupied): continue
        if any(fn.start == match.start() for fn in found): continue
        opening = match.end() - 1
        end = matching_brace(clean, opening)
        if end is not None:
            found.append(Function("", match["name"], match.start(), opening, end,
                                  line_no(text, match.start())))
    return found


def suppression_map(lines: list[str]) -> tuple[dict[int, set[str]], dict[int, set[str]], list[str]]:
    site: dict[int, set[str]] = {}
    func: dict[int, set[str]] = {}
    errors: list[str] = []
    for i, line in enumerate(lines, 1):
        match = SUP_RE.search(line)
        if not match: continue
        rules = {item.strip() for item in match.group(2).split(",") if item.strip()}
        reason = match.group(3).strip()
        unknown = rules - RULES
        if unknown or len(reason) < 12:
            errors.append(f"line {i}: malformed suppression (unknown={sorted(unknown)}, reason must be at least 12 characters)")
            continue
        (func if match.group(1) == "allow-function" else site)[i] = rules
    return site, func, errors


def structural_gates(body: str) -> tuple[list[bool], list[int]]:
    """Return gate/depth state at every source position in one function body."""
    gated_at = [False] * (len(body) + 1)
    depth_at = [0] * (len(body) + 1)
    stack: list[bool] = []
    delimiter = 0
    for i, char in enumerate(body):
        gated_at[i] = any(stack)
        depth_at[i] = len(stack)
        if char == "{":
            header = body[delimiter:i]
            stack.append(bool(re.search(r"\bif\s*\(", header) and GATE_RE.search(header)))
            delimiter = i + 1
        elif char == "}":
            if stack: stack.pop()
            delimiter = i + 1
        elif char == ";":
            delimiter = i + 1
    gated_at[-1] = any(stack)
    depth_at[-1] = len(stack)
    return gated_at, depth_at


def early_return_gates(body: str, site_offset: int, depth_at: list[int]) -> bool:
    """Recognize only function-top-level rate-gated early returns."""
    prefix = body[:site_offset]
    for match in re.finditer(r"\bif\s*\(", prefix):
        if depth_at[match.start()] != 1:
            continue
        opening = prefix.find("(", match.start())
        depth = 0
        closing = None
        for offset in range(opening, len(prefix)):
            if prefix[offset] == "(":
                depth += 1
            elif prefix[offset] == ")":
                depth -= 1
                if depth == 0:
                    closing = offset
                    break
        if closing is None or not GATE_RE.search(prefix[opening + 1:closing]):
            continue
        tail = prefix[closing + 1:]
        if re.match(r"\s*(?:\{\s*)?return\s*;", tail):
            return True
    return False


def braceless_gate(body: str, site_offset: int) -> bool:
    """Recognize a rate-gated braceless if controlling this statement."""
    statement_start = max(body.rfind(";", 0, site_offset),
                          body.rfind("{", 0, site_offset),
                          body.rfind("}", 0, site_offset)) + 1
    prefix = body[statement_start:site_offset]
    return bool(re.search(r"\bif\s*\([^;{}]*\)\s*$", prefix) and GATE_RE.search(prefix))


def brace_error(clean: str) -> str | None:
    depth = 0
    for offset, char in enumerate(clean):
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth < 0:
                return f"line {line_no(clean, offset)}: unmatched closing brace"
    if depth:
        return "unmatched opening brace"
    return None


def first_preprocessor_branch(clean: str) -> str:
    """Select the first branch of each conditional while preserving offsets."""
    output: list[str] = []
    stack: list[bool] = []
    active = True
    for line in clean.splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b", line)
        if directive:
            kind = directive.group(1)
            if kind in {"if", "ifdef", "ifndef"}:
                stack.append(active)
                active = active
            elif kind in {"elif", "else"} and stack:
                active = False
            elif kind == "endif" and stack:
                active = stack.pop()
            output.append("".join("\n" if char == "\n" else " " for char in line))
        elif active:
            output.append(line)
        else:
            output.append("".join("\n" if char == "\n" else " " for char in line))
    return "".join(output)


def scan(path: Path, changed_mode: bool = False) -> tuple[list[Finding], list[str]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    clean = blank_comments_and_strings(text)
    lines = text.splitlines()
    clean_lines = clean.splitlines()
    parse_error = brace_error(first_preprocessor_branch(clean))
    funcs = functions(text, clean)
    site_supp, func_supp, bad = suppression_map(lines)
    if parse_error:
        bad.append(parse_error)
    rel = path.relative_to(ROOT).as_posix()

    idle_names: set[str] = set()
    for match in re.finditer(r"gIdleCallbacks\s*\.\s*addFunction\s*\(\s*&?(?:(\w+)::)?(\w+)", clean):
        idle_names.add(f"{match.group(1)}::{match.group(2)}" if match.group(1) else match.group(2))

    roots: list[Function] = []
    for fn in funcs:
        before = "\n".join(lines[max(0, fn.start_line - 4):fn.start_line])
        if fn.name in ROOT_NAMES or fn.full in idle_names or fn.name in idle_names or "ui-draw-lint: per-frame" in before:
            roots.append(fn)
    by_class_name = {(fn.cls, fn.name): fn for fn in funcs}
    findings: list[Finding] = []
    seen_sites: dict[tuple[str, int, str], int] = {}
    control_names = set(re.findall(r"LLControlVariable\s*\*\s*(\w+)", clean))

    for root in roots:
        queue = [(root, [root.full], False, 0)]
        visited_edges: set[tuple[str, bool, int]] = set()
        while queue:
            fn, via, inherited_gate, depth = queue.pop(0)
            key = (fn.full, inherited_gate, depth)
            if key in visited_edges: continue
            visited_edges.add(key)
            start_line = line_no(text, fn.body) - 1
            end_line = line_no(text, fn.end) - 1
            body_lines = lines[start_line:end_line + 1]
            body_clean = clean_lines[start_line:end_line + 1]
            body_clean_text = "\n".join(body_clean)
            gated_at, depth_at = structural_gates(body_clean_text)
            line_offsets: list[int] = []
            cursor = 0
            for body_line in body_clean:
                line_offsets.append(cursor)
                cursor += len(body_line) + 1
            allowed_function: set[str] = set()
            for sup_line, rules in func_supp.items():
                if fn.start_line - 3 <= sup_line <= fn.start_line:
                    allowed_function |= rules
            for j, (source, code) in enumerate(zip(body_lines, body_clean)):
                absolute = start_line + j + 1
                allowed = set(allowed_function)
                allowed |= site_supp.get(absolute, set())
                allowed |= site_supp.get(absolute - 1, set())
                candidates: list[tuple[str, str, int]] = []
                context = "\n".join(body_clean[max(0, j - 3):j + 1])
                lazy_get_safe = bool(
                    re.search(r"if\s*\(\s*!\s*m\w+(?:\.\w+)?\s*\)", context) and
                    re.search(r"=\s*(?:\w+->|\w+\.)?getChild\s*<", code)
                )
                lazy_find = bool(
                    re.search(r"=\s*(?:\w+->|\w+\.)?findChild(?:View)?(?:\s*<[^>]+>)?\s*\(", code) and
                    re.search(r"if\s*\(\s*!\s*m\w+(?:\.\w+)?\s*\)", context) and
                    not re.search(r"m\w*(?:Looked|Resolved|Searched)", context)
                )
                if not lazy_get_safe and not lazy_find:
                    for match in LOOKUP_RE.finditer(code):
                        if not re.search(r"gFocusMgr\s*\.\s*childHas(?:KeyboardFocus|MouseCapture)\s*\(\s*[^\"]", code):
                            candidates.append(("lookup", "by-name child lookup on a per-frame path",
                                               match.start()))
                for match in SETTING_RE.finditer(code):
                    candidates.append(("setting", "by-name setting read on a per-frame path",
                                       match.start()))
                typed_setting_write = any(re.search(rf"\b{re.escape(name)}\s*->\s*setValue\s*\(", code)
                                          for name in control_names)
                write_columns = {match.start() for match in SETTING_WRITE_RE.finditer(code)}
                if typed_setting_write:
                    write_columns.update(match.start() for match in re.finditer(r"\bsetValue\s*\(", code))
                for column in write_columns:
                    candidates.append(("setting-write", "setting write on a per-frame path", column))
                if "IfChanged(" not in code and not typed_setting_write:
                    for match in RAW_RE.finditer(code):
                        candidates.append(("raw-write", "raw widget write on a per-frame path",
                                           match.start()))
                if lazy_find:
                    match = re.search(r"findChild(?:View)?", code)
                    candidates.append(("lazy-find", "nullable lazy find repeats a miss",
                                       match.start() if match else 0))
                for rule, message, column in candidates:
                    if rule in allowed: continue
                    site_offset = line_offsets[j] + max(column, 0)
                    is_gated = inherited_gate or gated_at[min(site_offset, len(gated_at) - 1)] or \
                        braceless_gate(body_clean_text, site_offset) or \
                        early_return_gates(body_clean_text, site_offset, depth_at)
                    site_key = (rel, absolute, rule)
                    severity = "warn" if is_gated and rule in {"lookup", "setting", "raw-write"} else "error"
                    if site_key in seen_sites:
                        old = findings[seen_sites[site_key]]
                        if old.severity == "warn" and severity == "error":
                            findings[seen_sites[site_key]] = Finding(
                                rel, absolute, rule, severity, root.full,
                                " > ".join(via), message, fn.full, source.strip())
                        continue
                    seen_sites[site_key] = len(findings)
                    findings.append(Finding(rel, absolute, rule, severity, root.full,
                                            " > ".join(via), message, fn.full, source.strip()))

            if depth < 3:
                joined = body_clean_text
                for match in re.finditer(r"(?<![:\w])([A-Za-z_]\w*)\s*\(", joined):
                    callee = by_class_name.get((fn.cls, match.group(1)))
                    if callee and callee is not fn:
                        call_gate = inherited_gate or gated_at[min(match.start(), len(gated_at) - 1)] or \
                            braceless_gate(joined, match.start()) or \
                            early_return_gates(joined, match.start(), depth_at)
                        queue.append((callee, via + [callee.full], call_gate, depth + 1))

        if root.name == "draw" and changed_mode and re.match(r"(?:al|llfloater|llpanel)", path.name):
            first = clean[root.body + 1:root.end].strip().split("\n", 1)[0].strip()
            if not re.match(r"LL_PROFILE_ZONE_(?:SCOPED|NAMED)_CATEGORY_UI\b", first):
                findings.append(Finding(rel, root.start_line, "missing-zone", "error", root.full,
                                        root.full, "draw() does not begin with a UI Tracy zone",
                                        root.full, lines[root.start_line - 1].strip()))

    ordinals: dict[tuple[str, str, str], int] = {}
    for finding in sorted(findings, key=lambda f: (f.line, f.rule)):
        key = (finding.function, finding.rule, " ".join(finding.source.split()))
        finding.ordinal = ordinals.get(key, 0)
        ordinals[key] = finding.ordinal + 1
    return findings, bad


def git_lines(*args: str) -> list[str]:
    proc = subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True)
    if proc.returncode:
        raise RuntimeError(proc.stderr.strip() or "git failed")
    return [line for line in proc.stdout.splitlines() if line]


def git_rename_map(revision: str) -> dict[str, str]:
    """Map current paths to their names in REV for fingerprint continuity."""
    renamed: dict[str, str] = {}
    for line in git_lines("diff", "--name-status", "--find-renames", revision, "--"):
        fields = line.split("\t")
        if len(fields) == 3 and fields[0].startswith("R"):
            renamed[fields[2]] = fields[1]
    return renamed


def remap_fingerprint(fingerprint: str, renamed: dict[str, str]) -> str:
    path, separator, rest = fingerprint.partition(" | ")
    old_path = renamed.get(path)
    return f"{old_path}{separator}{rest}" if old_path else fingerprint


def baseline_additions(current: set[str], revision: set[str],
                       renamed: dict[str, str]) -> list[str]:
    return sorted(
        fingerprint for fingerprint in current
        if remap_fingerprint(fingerprint, renamed) not in revision
    )


def eligible(path: Path) -> bool:
    try: rel = path.relative_to(ROOT).as_posix()
    except ValueError: return False
    return (rel.startswith("indra/newview/") or rel.startswith("indra/llui/")) and path.suffix in {".cpp", ".h"} and "/tests/" not in rel and path.name != "aluirefresh.h"


def select_files(args: argparse.Namespace) -> tuple[list[Path], bool]:
    if args.all:
        files = list((ROOT / "indra/newview").glob("*.cpp")) + list((ROOT / "indra/newview").glob("*.h"))
        files += list((ROOT / "indra/llui").glob("*.cpp")) + list((ROOT / "indra/llui").glob("*.h"))
        return sorted(filter(eligible, files)), False
    if args.changed:
        names = git_lines("diff", "--name-only", "--diff-filter=ACMR", args.changed, "--")
        names += git_lines("ls-files", "--others", "--exclude-standard", "indra/newview", "indra/llui")
        return sorted({ROOT / name for name in names if eligible(ROOT / name)}), True
    return [Path(name).resolve() for name in args.files], True


def load_baseline() -> set[str]:
    if not BASELINE.exists(): return set()
    data = json.loads(BASELINE.read_text(encoding="utf-8"))
    return set(data.get("fingerprints", []))


def load_revision_baseline(revision: str) -> set[str] | None:
    """Return REV's baseline, or None when that revision predates the file."""
    rel = BASELINE.relative_to(ROOT).as_posix()
    proc = subprocess.run(
        ["git", "show", f"{revision}:{rel}"], cwd=ROOT,
        text=True, capture_output=True,
    )
    if proc.returncode:
        verify = subprocess.run(
            ["git", "rev-parse", "--verify", f"{revision}^{{commit}}"],
            cwd=ROOT, text=True, capture_output=True,
        )
        if verify.returncode:
            raise RuntimeError(verify.stderr.strip() or f"invalid revision: {revision}")
        return None
    data = json.loads(proc.stdout)
    return set(data.get("fingerprints", []))


def run_selftest() -> int:
    expected = {
        "lookup_in_draw_broken.cpp": (1, "lookup"),
        "lookup_gated_current.cpp": (0, None),
        "lazy_findchild_broken.cpp": (1, "lazy-find"),
        "lazy_getchild_current.cpp": (0, None),
        "menu_holder_miss_broken.cpp": (1, "lookup"),
        "setting_in_tick_broken.cpp": (1, "setting"),
        "cached_control_current.cpp": (0, None),
        "raw_write_broken.cpp": (1, "raw-write"),
        "ifchanged_current.cpp": (0, None),
        "early_return_gate_current.cpp": (0, None),
        "suppression_current.cpp": (0, None),
        "inclass_draw_broken.h": (1, "lookup"),
        "idle_callback_root_broken.cpp": (1, "lookup"),
        "completed_gate_broken.cpp": (1, "lookup"),
        "free_idle_callback_broken.cpp": (1, "lookup"),
        "bare_lookup_broken.cpp": (1, "lookup"),
        "mixed_call_paths_broken.cpp": (1, "lookup"),
        "nested_early_return_broken.cpp": (1, "lookup"),
        "nested_gate_current.cpp": (0, None),
        "typed_control_write_broken.cpp": (1, "setting-write"),
        "early_return_body_broken.cpp": (1, "lookup"),
        "same_line_mixed_broken.cpp": (1, "lookup"),
    }
    failed = False
    for name, (errors, rule) in expected.items():
        findings, bad = scan(FIXTURES / name, changed_mode=False)
        actual = [f for f in findings if f.severity == "error"]
        ok = not bad and len(actual) == errors and (rule is None or any(f.rule == rule for f in actual))
        print(f"{'PASS' if ok else 'FAIL'} {name}: errors={len(actual)} warnings={sum(f.severity == 'warn' for f in findings)}")
        failed |= not ok
    _, bad = scan(FIXTURES / "suppression_bad.cpp", changed_mode=False)
    ok = bool(bad)
    print(f"{'PASS' if ok else 'FAIL'} suppression_bad.cpp: malformed={len(bad)}")
    _, parse_bad = scan(FIXTURES / "parse_failure_broken.cpp", changed_mode=False)
    parse_ok = bool(parse_bad)
    print(f"{'PASS' if parse_ok else 'FAIL'} parse_failure_broken.cpp: malformed={len(parse_bad)}")
    rename_ok = remap_fingerprint(
        "indra/newview/new.cpp | C::draw | lookup | x | 0",
        {"indra/newview/new.cpp": "indra/newview/old.cpp"},
    ) == "indra/newview/old.cpp | C::draw | lookup | x | 0"
    print(f"{'PASS' if rename_ok else 'FAIL'} rename fingerprint continuity")
    shrink_ok = baseline_additions({"new"}, set(), {}) == ["new"]
    print(f"{'PASS' if shrink_ok else 'FAIL'} shrink-only baseline enforcement")
    return 1 if failed or not ok or not parse_ok or not rename_ok or not shrink_ok else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--changed", metavar="REV")
    mode.add_argument("--all", action="store_true")
    mode.add_argument("--selftest", action="store_true")
    mode.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--prune", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()
    if args.selftest: return run_selftest()
    if not (args.changed or args.all or args.write_baseline or args.files):
        parser.error("choose --changed, --all, --selftest, --write-baseline, or files")
    if args.write_baseline:
        args.all = True
    try:
        files, changed_mode = select_files(args)
        all_findings: list[Finding] = []
        malformed: list[str] = []
        for path in files:
            findings, bad = scan(path, changed_mode)
            all_findings.extend(findings)
            malformed.extend(f"{path.relative_to(ROOT).as_posix()}: {item}" for item in bad)
        if malformed:
            for item in malformed: print(f"bad-suppression: {item}", file=sys.stderr)
            return 2
        current = {f.fingerprint() for f in all_findings if f.severity == "error"}
        if args.write_baseline:
            if BASELINE.exists() and not args.prune:
                print(f"baseline already exists: {BASELINE}; use --prune", file=sys.stderr)
                return 2
            if args.prune:
                fingerprints = sorted(load_baseline() & current)
            else:
                fingerprints = sorted(current)
            BASELINE.write_text(json.dumps({"fingerprints": fingerprints}, indent=2) + "\n", encoding="utf-8")
            print(f"wrote {len(fingerprints)} baseline fingerprints")
            return 0
        baseline = load_baseline()
        baseline_added: list[str] = []
        renamed = git_rename_map(args.changed) if args.changed else {}
        if args.changed:
            revision_baseline = load_revision_baseline(args.changed)
            if revision_baseline is not None:
                baseline_added = baseline_additions(
                    baseline, revision_baseline, renamed)
        for finding in all_findings:
            fingerprint = finding.fingerprint()
            finding.baselined = finding.severity == "error" and (
                fingerprint in baseline or
                remap_fingerprint(fingerprint, renamed) in baseline
            )
        new_errors = [f for f in all_findings if f.severity == "error" and not f.baselined]
        if args.json:
            grouped: dict[str, list[dict]] = {}
            for f in all_findings:
                grouped.setdefault(f.file, []).append({
                    "line": f.line, "rule": f.rule, "severity": f.severity,
                    "root": f.root, "via": f.via, "text": f.text,
                    "baselined": f.baselined,
                })
            print(json.dumps({"total_errors": sum(f.severity == "error" for f in all_findings),
                              "new_errors": len(new_errors),
                              "baseline_added": baseline_added,
                              "files": [{"file": name, "findings": values} for name, values in sorted(grouped.items())]}, indent=2))
        else:
            for fingerprint in baseline_added:
                print(f"baseline-growth: {fingerprint}", file=sys.stderr)
            for f in all_findings:
                suffix = " [baselined]" if f.baselined else ""
                print(f"{f.file}:{f.line}: {f.rule}: {f.text} [root={f.root} via={f.via}]{suffix}")
            print(f"UI draw lint: {sum(f.severity == 'error' for f in all_findings)} errors, {len(new_errors)} new, {sum(f.severity == 'warn' for f in all_findings)} warnings, {len(baseline_added)} baseline additions")
        return 1 if new_errors or baseline_added else 0
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
        print(f"ui_draw_lint: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
