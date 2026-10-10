#!/usr/bin/env python3
"""Director kit v1-to-v2 parity gate.

Checks (DIRECTOR_KIT_P0_SPEC.md section 6.2 unless noted):

  0  the machine authority lists exactly 35 pages (USER_DECISIONS_20261010.md drops Capture)
  1  every v1 widget name is a v2 name, or matches a parity_allow glob with a reason
  2  every v1 control_name is bound by some control_name in v2; declared shortcuts
     (shortcut_of) are ordinary bindings and duplicate bindings are not reported here
  3  every getChild/findChild literal and expanded prefix/suffix pattern exists in v2 with a
     class compatible with the requested type (section 4.4, cpp_name_patterns.json)
  4  every selector kit_option carries a value that equals a v1 combo item value with the
     same label (sections 3.4 and 6.1 rule 10); unmatched labels need a parity_allow entry
     keyed "option:<label>"

v1 scope comes from doc/director_kit/page_sources.json owner roots (ROLLOUT spec section 6).
A page's owner is scoped to its leaf element when one is named after the page (for example
floater_director.xml's exposure_tab); a shared panel root is taken whole; a floater owner
such as floater_lightbox_settings.xml contributes only the widgets the page's rows name.

Usage::

    python scripts/code_tools/director_parity.py --groups light_group [--json]
    python scripts/code_tools/director_parity.py --all [--json]
    python scripts/code_tools/director_parity.py --selftest

When floater_director_v2.xml does not exist yet, the result is "v2 not generated" and the
exit status is 0 (unless the v1 side already has findings, which is exit 1). Exit status is 1
on FAIL and 2 for command line or input errors. It uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import fnmatch
import itertools
import json
import re
import sys
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator, Optional


REPO_ROOT = Path(__file__).resolve().parents[2]
XUI_DIR = REPO_ROOT / "indra" / "newview" / "skins" / "default" / "xui" / "en"
NEWVIEW_DIR = REPO_ROOT / "indra" / "newview"
KIT_DIR = REPO_ROOT / "scripts" / "code_tools" / "director_kit"
FIXTURE_DIR = KIT_DIR / "parity_fixtures"
PAGE_SOURCES = REPO_ROOT / "doc" / "director_kit" / "page_sources.json"
KIT_PAGES = KIT_DIR / "kit_pages.json"
CPP_PATTERNS = KIT_DIR / "cpp_name_patterns.json"
GEN_REPORT = KIT_DIR / "gen_report.txt"
V2_FLOATER = "floater_director_v2.xml"
EXPECTED_PAGES = 35

NAME_TAGS = frozenset({
    "slider", "spinner", "check_box", "combo_box", "button", "line_editor",
    "color_swatch", "icon", "radio_group", "scroll_list", "text", "compass_dial",
})
V2_EXTRA_NAME_TAGS = frozenset({"kit_switch"})
# Requested getChild<T> type -> tags the kit may generate for it. Types not listed are
# existence-checked only.
CLASS_TAGS = {
    "LLSliderCtrl": frozenset({"slider"}),
    "LLSpinCtrl": frozenset({"spinner"}),
    "LLCheckBoxCtrl": frozenset({"check_box", "kit_switch"}),
    "LLComboBox": frozenset({"combo_box"}),
    "LLLineEditor": frozenset({"line_editor"}),
    "LLButton": frozenset({"button"}),
    "LLTextBox": frozenset({"text"}),
    "LLRadioGroup": frozenset({"radio_group"}),
    "LLScrollListCtrl": frozenset({"scroll_list"}),
    "LLColorSwatchCtrl": frozenset({"color_swatch"}),
    "LLIconCtrl": frozenset({"icon"}),
    "ALCompassDial": frozenset({"compass_dial"}),
}

GETCHILD_CALL = re.compile(r"\b(?:getChild|findChild)\s*<\s*([\w:]+)\s*>\s*\(")
LITERAL = re.compile(r'"([^"]*)"')
KITNAMES_LINE = re.compile(r"^KITNAMES\s+([^:\s]+):\s*(.*)$")
CPP_LITERAL = re.compile(r'"([A-Za-z_][A-Za-z0-9_]*)"')
TEMPLATE_HOLE = re.compile(r"\{(\w+)\}")


class ParityError(Exception):
    """Bad command line or unreadable input (exit status 2)."""


@dataclass(frozen=True)
class Paths:
    page_sources: Path = PAGE_SOURCES
    xui_dir: Path = XUI_DIR
    kit_pages: Path = KIT_PAGES
    patterns: Path = CPP_PATTERNS
    gen_report: Path = GEN_REPORT
    ref_sources: Optional[tuple] = None  # None: indra/newview/*.cpp and *.h


@dataclass(frozen=True)
class Widget:
    name: str
    tag: str
    control: Optional[str]
    source: str


@dataclass(frozen=True)
class ComboItem:
    label: str
    value: str
    source: str


@dataclass(frozen=True)
class OptionRef:
    label: str
    value: Optional[str]
    source: str


@dataclass(frozen=True)
class GetChildRequest:
    source: str
    line: int
    requested: str
    names: tuple
    text: str
    unresolved: bool


@dataclass(frozen=True)
class Finding:
    check: str
    subject: str
    message: str

    def to_dict(self) -> dict:
        return {"check": self.check, "subject": self.subject, "message": self.message}


@dataclass
class Allow:
    rules: dict

    def match(self, subject: str) -> Optional[str]:
        for pattern in sorted(self.rules):
            if fnmatch.fnmatchcase(subject, pattern):
                return str(self.rules[pattern])
        return None


@dataclass
class V2Index:
    names: set = field(default_factory=set)
    controls: set = field(default_factory=set)
    tags: dict = field(default_factory=dict)
    options: list = field(default_factory=list)


def _rel(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def _resolve(rel: str) -> Path:
    path = Path(rel)
    return path if path.is_absolute() else REPO_ROOT / path


def _load_json(path: Path, required: bool = True):
    if not path.is_file():
        if required:
            raise ParityError(f"missing {_rel(path)}")
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ParityError(f"cannot read {_rel(path)}: {exc}")


def load_pages(path: Path) -> list:
    data = _load_json(path)
    pages = data.get("pages") if isinstance(data, dict) else None
    if not isinstance(pages, list):
        raise ParityError(f"{_rel(path)}: no pages list")
    return pages


def load_allow(paths: Paths) -> Allow:
    data = _load_json(paths.kit_pages, required=False) or {}
    rules = data.get("parity_allow", {})
    if not isinstance(rules, dict):
        raise ParityError("parity_allow must be an object")
    return Allow({str(k): v for k, v in rules.items()})


def load_expansions(path: Path) -> list:
    """Concrete C++-referenced names from cpp_name_patterns.json templates."""
    data = _load_json(path, required=False)
    if not data:
        return []
    names = set()
    for pattern in data.get("patterns", []):
        template = str(pattern["template"])
        holes = TEMPLATE_HOLE.findall(template)
        values = [pattern.get(hole, []) for hole in holes]
        for combo in itertools.product(*values):
            name = template
            for hole, value in zip(holes, combo):
                name = name.replace("{" + hole + "}", str(value))
            names.add(name)
    return sorted(names)


def load_kitnames(path: Path, keys: Optional[set]) -> dict:
    """Parse `KITNAMES <page>: name[:tag], ...` lines. Untagged names map to '*'."""
    out: dict = {}
    if not path.is_file():
        return out
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = KITNAMES_LINE.match(line.strip())
        if not match:
            continue
        if keys is not None and match.group(1) not in keys:
            continue
        for token in re.split(r"[,\s]+", match.group(2)):
            if not token:
                continue
            name, _, tag = token.partition(":")
            out.setdefault(name, set()).add(tag or "*")
    return out


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def cpp_literals(files) -> set:
    names = set()
    for path in files:
        if path.is_file():
            names.update(CPP_LITERAL.findall(read_text(path)))
    return names


def default_ref_sources() -> list:
    return sorted(NEWVIEW_DIR.glob("*.cpp")) + sorted(NEWVIEW_DIR.glob("*.h"))


class XmlCache:
    def __init__(self) -> None:
        self.roots: dict = {}
        self.parents: dict = {}

    def root(self, path: Path) -> ElementTree.Element:
        key = str(path.resolve())
        if key not in self.roots:
            try:
                root = ElementTree.parse(path).getroot()
            except (OSError, ElementTree.ParseError) as exc:
                raise ParityError(f"cannot parse {_rel(path)}: {exc}")
            self.roots[key] = root
            for parent in root.iter():
                for child in parent:
                    self.parents[child] = parent
        return self.roots[key]

    def resolve_embed(self, host: Path, embed: str) -> Optional[Path]:
        for candidate in (host.parent / embed, XUI_DIR / embed):
            if candidate.is_file():
                return candidate
        return None


def expand(cache: XmlCache, element, source: Path, stack: tuple = ()) -> Iterator[tuple]:
    """Yield (element, source file) for a subtree, following filename= and page_file= embeds."""
    yield element, source
    embed = element.get("filename") or element.get("page_file")
    if embed:
        target = cache.resolve_embed(source, embed)
        if target is None:
            raise ParityError(f"{_rel(source)}: unresolved embed {embed!r}")
        if str(target) not in stack:
            yield from expand(cache, cache.root(target), target, stack + (str(target),))
    for child in element:
        yield from expand(cache, child, source, stack)


def find_named(root, name: str):
    for element in root.iter():
        if element.get("name") == name:
            return element
    return None


def scope_elements(cache: XmlCache, owner: Path, page_key: str,
                   row_names: frozenset = frozenset()) -> tuple:
    root = cache.root(owner)
    leaf = find_named(root, page_key)
    if leaf is not None:
        return list(expand(cache, leaf, owner)), "leaf"
    if root.tag == "panel":
        return list(expand(cache, root, owner)), "panel"
    if not row_names:
        return [], "none"
    kept = []
    for element, source in expand(cache, root, owner):
        parent = cache.parents.get(element)
        parent_name = parent.get("name") if parent is not None else None
        if element.get("name") in row_names or (
                element.tag == "combo_box.item" and parent_name in row_names):
            kept.append((element, source))
    return kept, "rows"


def widgets_from(elements) -> list:
    out = []
    for element, source in elements:
        name = (element.get("name") or "").strip()
        if name and element.tag in NAME_TAGS:
            control = (element.get("control_name") or "").strip() or None
            out.append(Widget(name, element.tag, control, _rel(source)))
    return out


def combo_items(elements) -> list:
    return [
        ComboItem((element.get("label") or "").strip(), (element.get("value") or "").strip(),
                  _rel(source))
        for element, source in elements if element.tag == "combo_box.item"
    ]


def row_names(page: dict, owner_rel: str) -> frozenset:
    names = set()
    for row in page.get("rows", []) or []:
        if not row.get("matched_widget"):
            continue
        if any(x.get("file") == owner_rel for x in row.get("xui", []) or []):
            names.add(row["matched_widget"])
    return frozenset(names)


def page_file_name(page: dict) -> str:
    """panel_dpage_<g>_<slug>.xml per section 4.1 of the P0 spec."""
    group = str(page["group"])
    g = group[:-len("_group")] if group.endswith("_group") else group
    name = str(page["key"])
    name = name[:-len("_tab")] if name.endswith("_tab") else name
    if name.startswith(g + "_"):
        name = name[len(g) + 1:]
    return f"panel_dpage_{g}_{name}.xml"


def build_v2_index(cache: XmlCache, files, kitnames: dict) -> V2Index:
    index = V2Index()
    for path in files:
        for element, source in expand(cache, cache.root(path), path):
            name = (element.get("name") or "").strip()
            if name:
                index.tags.setdefault(name, set()).add(element.tag)
                if element.tag in NAME_TAGS or element.tag in V2_EXTRA_NAME_TAGS:
                    index.names.add(name)
            control = (element.get("control_name") or "").strip()
            if control:
                index.controls.add(control)
            if element.tag == "kit_option":
                value = element.get("value")
                index.options.append(OptionRef((element.get("label") or "").strip(),
                                               value.strip() if value is not None else None,
                                               _rel(source)))
    for name, tags in kitnames.items():
        index.tags.setdefault(name, set()).update(tags)
        if "*" in tags or tags & (NAME_TAGS | V2_EXTRA_NAME_TAGS):
            index.names.add(name)
    return index


def _balanced_args(text: str, start: int) -> Optional[str]:
    depth = 1
    for i in range(start, len(text)):
        char = text[i]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return text[start:i].strip()
    return None


def _classify(requested: str, args: str, source: str, line: int, expansions: list) -> GetChildRequest:
    parts = [part.strip() for part in args.split("+")]
    literals = [LITERAL.fullmatch(part) for part in parts]
    if all(literals):
        name = "".join(match.group(1) for match in literals)
        return GetChildRequest(source, line, requested, (name,), args, False)
    prefix = literals[0].group(1) if literals[0] else ""
    suffix = literals[-1].group(1) if literals[-1] else ""
    if not prefix and not suffix:
        return GetChildRequest(source, line, requested, (), args, True)
    names = tuple(n for n in expansions if n.startswith(prefix) and n.endswith(suffix))
    return GetChildRequest(source, line, requested, names, args, not names)


def extract_getchild(text: str, source: str, expansions: list) -> list:
    out = []
    for match in GETCHILD_CALL.finditer(text):
        args = _balanced_args(text, match.end())
        if args is None:
            continue
        line = text.count("\n", 0, match.start()) + 1
        out.append(_classify(match.group(1).split("::")[-1], args, source, line, expansions))
    return out


def cpp_sources(pages: list) -> list:
    files = set()
    for page in pages:
        for row in page.get("rows", []) or []:
            for entry in row.get("cpp", []) or []:
                if entry.get("file"):
                    files.add(entry["file"])
    out = []
    for rel in sorted(files):
        path = _resolve(rel)
        if not path.is_file():
            raise ParityError(f"cpp source missing: {rel}")
        out.append(path)
    return out


def page_count_findings(count: int, expected: int = EXPECTED_PAGES) -> list:
    if count == expected:
        return []
    return [Finding("0", "page_sources.json",
                    f"{count} pages, expected {expected} (Capture dropped per USER_DECISIONS_20261010)")]


def check_names(a_names: set, b_names: set, allow: Allow, allowed: list) -> list:
    out = []
    for name in sorted(a_names - b_names):
        reason = allow.match(name)
        if reason is not None:
            allowed.append({"check": "1", "subject": name, "reason": reason})
        else:
            out.append(Finding("1", name, "v1 name is not a v2 name and has no parity_allow entry"))
    return out


def check_controls(a_controls: set, b_controls: set, allow: Allow, allowed: list) -> list:
    out = []
    for control in sorted(a_controls - b_controls):
        reason = allow.match(control)
        if reason is not None:
            allowed.append({"check": "2", "subject": control, "reason": reason})
        else:
            out.append(Finding("2", control, "v1 control_name is not bound by any v2 control_name"))
    return out


def check_getchild(requests: list, tags: dict) -> list:
    out = []
    for request in requests:
        where = f"{request.source}:{request.line} getChild<{request.requested}>"
        if request.unresolved:
            out.append(Finding("3", f"{request.source}:{request.line}",
                               f"dynamic lookup {request.text!r} is not expanded by cpp_name_patterns.json"))
            continue
        for name in request.names:
            actual = tags.get(name)
            if actual is None:
                out.append(Finding("3", name, f"{where} name is missing from v2"))
            elif (request.requested in CLASS_TAGS and "*" not in actual
                  and not (actual & CLASS_TAGS[request.requested])):
                out.append(Finding("3", name, f"{where} but v2 tag is {sorted(actual)}"))
    return out


def check_options(items: list, options: list, allow: Allow, allowed: list) -> list:
    values_by_label: dict = {}
    for item in items:
        values_by_label.setdefault(item.label, set()).add(item.value)
    out = []
    for option in options:
        if not option.value:
            out.append(Finding("4", option.label, f"selector option has no value ({option.source})"))
            continue
        values = values_by_label.get(option.label)
        if not values:
            reason = allow.match("option:" + option.label)
            if reason is not None:
                allowed.append({"check": "4", "subject": option.label, "reason": reason})
            else:
                out.append(Finding("4", option.label, "no v1 combo item carries this label"))
        elif option.value not in values:
            out.append(Finding("4", option.label,
                               f"value {option.value!r} is not a v1 combo value for this label "
                               f"(v1 values {sorted(values)})"))
    return out


def _report(label: str, pages: list, status: str, findings: list, allowed: list, counts: dict) -> dict:
    ordered = sorted(findings, key=lambda f: (f.check, f.subject, f.message))
    return {
        "groups": label,
        "pages": pages,
        "status": status,
        "counts": counts,
        "allowed": sorted(allowed, key=lambda a: (a["check"], a["subject"])),
        "findings": [f.to_dict() for f in ordered],
    }


def evaluate(paths: Paths, groups: Optional[list]) -> dict:
    pages_all = load_pages(paths.page_sources)
    known_groups = {str(page.get("group")) for page in pages_all}
    if groups is None:
        selected = list(pages_all)
    else:
        unknown = sorted(g for g in groups if g not in known_groups)
        if unknown:
            raise ParityError("unknown group(s): " + ", ".join(unknown))
        selected = [page for page in pages_all if page.get("group") in groups]
    keys = [str(page["key"]) for page in selected]
    label = "all" if groups is None else ",".join(groups)
    findings: list = []
    if groups is None:
        findings += page_count_findings(len(pages_all))
    allow = load_allow(paths)
    cache = XmlCache()

    v1_widgets: list = []
    v1_items: list = []
    for page in selected:
        for owner in page.get("owner_roots", []) or []:
            owner_path = _resolve(owner["file"])
            if not owner_path.is_file():
                raise ParityError(f"owner file missing: {owner['file']}")
            elements, _scope = scope_elements(cache, owner_path, str(page["key"]),
                                              row_names(page, owner["file"]))
            v1_widgets.extend(widgets_from(elements))
            v1_items.extend(combo_items(elements))

    expansions = load_expansions(paths.patterns)
    refs = set(expansions)
    refs |= cpp_literals(paths.ref_sources if paths.ref_sources is not None else default_ref_sources())
    kept = [w for w in v1_widgets if not (w.name.endswith("_reset") and w.name not in refs)]
    a_names = {w.name for w in kept}
    a_controls = {w.control for w in kept if w.control}
    counts = {"v1_names": len(a_names), "v1_controls": len(a_controls),
              "v1_combo_items": len(v1_items)}

    v2_floater = paths.xui_dir / V2_FLOATER
    if not v2_floater.is_file():
        status = "FAIL" if findings else "V2_NOT_GENERATED"
        return _report(label, keys, status, findings, [], counts)

    page_files = []
    for page in selected:
        name = page_file_name(page)
        if (paths.xui_dir / name).is_file():
            page_files.append(paths.xui_dir / name)
        else:
            findings.append(Finding("0", name, "v2 page file is missing"))
    kitnames = load_kitnames(paths.gen_report, set(keys) if groups is not None else None)
    index = build_v2_index(cache, [v2_floater] + page_files, kitnames)
    counts.update(v2_names=len(index.names), kit_options=len(index.options))

    allowed: list = []
    findings += check_names(a_names, index.names, allow, allowed)
    findings += check_controls(a_controls, index.controls, allow, allowed)
    requests: list = []
    for path in cpp_sources(selected):
        requests += extract_getchild(read_text(path), _rel(path), expansions)
    counts["getchild_requests"] = len(requests)
    findings += check_getchild(requests, index.tags)
    findings += check_options(v1_items, index.options, allow, allowed)
    status = "FAIL" if findings else "PASS"
    return _report(label, keys, status, findings, allowed, counts)


def exit_code(status: str) -> int:
    return 1 if status == "FAIL" else 0


def _print_text(report: dict) -> None:
    counts = report["counts"]
    print(f"director_parity: groups={report['groups']} pages={len(report['pages'])} "
          f"v1_names={counts['v1_names']} v1_controls={counts['v1_controls']} "
          f"v1_combo_items={counts['v1_combo_items']}")
    if report["status"] == "V2_NOT_GENERATED":
        print("v2 not generated: floater_director_v2.xml is absent; v1 side loaded, parity not evaluated")
        return
    print(f"v2_names={counts['v2_names']} kit_options={counts['kit_options']} "
          f"getchild_requests={counts['getchild_requests']} allowed={len(report['allowed'])}")
    for finding in report["findings"]:
        print(f"  [check {finding['check']}] {finding['subject']}: {finding['message']}")
    print(f"{report['status']}: {len(report['findings'])} finding(s)")


def selftest() -> int:
    results: list = []

    def expect(name: str, ok: bool, detail: object = "") -> None:
        results.append(bool(ok))
        print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f" -- {detail}"))

    def fx(name: str) -> Path:
        return FIXTURE_DIR / name

    cache = XmlCache()
    allow = Allow({})
    e2e = fx("e2e")

    # Scope and extraction
    leaf, _ = scope_elements(cache, fx("v1_leaf.xml"), "fixture_tab")
    leaf_widgets = widgets_from(leaf)
    expected_leaf = {"fx_slider", "fx_check", "fx_combo", "fx_cine_mode", "fx_check_reset",
                     "fx_combo_reset", "fx_readout", "fx_embedded"}
    expect("scope: leaf subtree with embed expanded, sibling tab excluded",
           {w.name for w in leaf_widgets} == expected_leaf,
           sorted({w.name for w in leaf_widgets} ^ expected_leaf))
    expansions = load_expansions(fx("c3_patterns.json"))
    expect("patterns: fx_{side} expands to fx_left, fx_right", expansions == ["fx_left", "fx_right"], expansions)
    refs = cpp_literals([fx("c3_pass.cpp")]) | set(expansions)
    a_widgets = [w for w in leaf_widgets if not (w.name.endswith("_reset") and w.name not in refs)]
    a_names = {w.name for w in a_widgets}
    a_controls = {w.control for w in a_widgets if w.control}
    expect("reset filter: unreferenced _reset dropped, referenced _reset kept",
           "fx_check_reset" not in a_names and "fx_combo_reset" in a_names)
    expect("control names collected from the leaf",
           a_controls == {"FxSliderSetting", "FxCheckSetting", "FxComboSetting", "FxCineSetting", "FxEmbedded"},
           sorted(a_controls))
    rows, _ = scope_elements(cache, fx("v1_leaf.xml"), "not_a_leaf", frozenset({"fx_slider", "fx_cine_mode"}))
    expect("rows scope (floater owner): named widgets and their combo items only",
           {w.name for w in widgets_from(rows)} == {"fx_slider", "fx_cine_mode"}
           and {i.label for i in combo_items(rows)} == {"Cine 39", "Cine 41"})
    panel, _ = scope_elements(cache, fx("fx_embed.xml"), "not_a_leaf")
    expect("panel-root owner taken whole", {w.name for w in widgets_from(panel)} == {"fx_embedded"})
    kitnames = load_kitnames(fx("kitnames_gen_report.txt"), None)
    expect("KITNAMES: name, name:tag and untagged forms", kitnames == {
        "fx_kit_label": {"text"}, "fx_kit_state": {"kit_switch"},
        "fx_kit_reset": {"*"}, "fx_other_kit": {"text"}}, kitnames)
    expect("KITNAMES: page filter", load_kitnames(fx("kitnames_gen_report.txt"), {"other_tab"})
           == {"fx_other_kit": {"text"}})
    expect("page file name: cine_light_rig_tab -> panel_dpage_light_cine_light_rig.xml",
           page_file_name({"key": "cine_light_rig_tab", "group": "light_group"})
           == "panel_dpage_light_cine_light_rig.xml")
    expect("page file name: light_rim_tab -> panel_dpage_light_rim.xml",
           page_file_name({"key": "light_rim_tab", "group": "light_group"}) == "panel_dpage_light_rim.xml")
    expect("parity_allow loads glob rules", load_allow(Paths(kit_pages=fx("allow_kit_pages.json")))
           .match("fx_legacy_launcher") == "fixture: retired launcher name")

    # Check 1 (names)
    idx_pass = build_v2_index(cache, [fx("v2_pass.xml")], {})
    idx_missing = build_v2_index(cache, [fx("v2_fail_missing.xml")], {})
    expect("check 1 pass", check_names(a_names, idx_pass.names, allow, []) == [])
    f1 = check_names(a_names, idx_missing.names, allow, [])
    expect("check 1 fail: missing names reported", sorted(f.subject for f in f1) == ["fx_embedded", "fx_readout"],
           [f.subject for f in f1])
    allowed: list = []
    f1b = check_names(a_names, idx_missing.names, Allow({"fx_readout": "fixture reason"}), allowed)
    expect("check 1 allow: allowed name suppressed with its reason",
           [f.subject for f in f1b] == ["fx_embedded"] and allowed[0]["reason"] == "fixture reason", allowed)

    # Check 2 (control names)
    idx_ctrl = build_v2_index(cache, [fx("v2_fail_control.xml")], {})
    expect("check 2 pass: shortcut duplicate binding accepted",
           check_controls(a_controls, idx_pass.controls, allow, []) == [])
    f2 = check_controls(a_controls, idx_ctrl.controls, allow, [])
    expect("check 2 fail: unbound control_name reported",
           [f.subject for f in f2] == ["FxSliderSetting"], [f.subject for f in f2])

    # Check 3 (getChild, class, patterns)
    reqs = extract_getchild(read_text(fx("c3_pass.cpp")), "c3_pass.cpp", expansions)
    expect("check 3 extraction: six lookups including fx_ + side", len(reqs) == 6, len(reqs))
    expect("check 3 pass", check_getchild(reqs, idx_pass.tags) == [])
    f3c = check_getchild(extract_getchild(read_text(fx("c3_fail_class.cpp")), "c3_fail_class.cpp",
                                          expansions), idx_pass.tags)
    expect("check 3 fail: class mismatch (LLSliderCtrl on a text row)",
           len(f3c) == 1 and "LLSliderCtrl" in f3c[0].message, [f.message for f in f3c])
    exp_missing = load_expansions(fx("c3_patterns_missing.json"))
    f3m = check_getchild(extract_getchild(read_text(fx("c3_fail_missing.cpp")), "c3_fail_missing.cpp",
                                          exp_missing), idx_pass.tags)
    expect("check 3 fail: missing literal and missing expanded pattern",
           sorted(f.subject for f in f3m) == ["fx_middle", "fx_nothere"], [f.subject for f in f3m])
    f3d = check_getchild(extract_getchild(read_text(fx("c3_fail_dynamic.cpp")), "c3_fail_dynamic.cpp",
                                          expansions), idx_pass.tags)
    expect("check 3 fail: unexpanded dynamic lookups reported",
           len(f3d) == 2 and all("not expanded" in f.message for f in f3d), [f.message for f in f3d])

    # Check 4 (selector values round-trip)
    items = combo_items(leaf)
    expect("v1 combo items gathered from the leaf", sorted((i.label, i.value) for i in items)
           == [("Alpha", "1"), ("Cine 39", "39"), ("Cine 41", "41"), ("Gamma", "3")])
    expect("check 4 pass: values round-trip", check_options(items, idx_pass.options, allow, []) == [])
    idx_opt = build_v2_index(cache, [fx("v2_fail_option.xml")], {})
    f4 = check_options(items, idx_opt.options, allow, [])
    expect("check 4 fail: wrong value, missing value, unmatched labels (incl. Cine 40 absent in v1)",
           sorted(f.subject for f in f4) == ["Alpha", "Cine 40", "Delta", "Gamma"], [f.subject for f in f4])
    allowed = []
    f4b = check_options(items, idx_opt.options, Allow({"option:Delta": "fixture"}), allowed)
    expect("check 4 allow: unmatched label allowed by option: key",
           "Delta" not in [f.subject for f in f4b] and allowed and allowed[0]["check"] == "4")

    # Check 0 (page count)
    expect("check 0 fail: 34 pages", len(page_count_findings(34)) == 1)
    expect("check 0 pass: 35 pages", page_count_findings(35) == [])

    # End-to-end on fixture page_sources and fixture xui trees
    def e2e_paths(sub: str) -> Paths:
        return Paths(page_sources=e2e / "page_sources.json", xui_dir=e2e / sub,
                     kit_pages=fx("allow_kit_pages.json"), patterns=fx("c3_patterns.json"),
                     gen_report=fx("kitnames_gen_report.txt"), ref_sources=(fx("c3_pass.cpp"),))

    report_pass = evaluate(e2e_paths("pass"), ["fx_group"])
    expect("e2e pass: v2 page reached via page_file embed", report_pass["status"] == "PASS",
           report_pass["findings"])
    report_fail = evaluate(e2e_paths("fail"), ["fx_group"])
    got = sorted((f["check"], f["subject"]) for f in report_fail["findings"])
    expect("e2e fail: missing readout reported by checks 1 and 3",
           got == [("1", "fx_readout"), ("3", "fx_readout")], got)
    expect("e2e deterministic: identical JSON on rerun",
           json.dumps(report_fail, sort_keys=True) == json.dumps(evaluate(e2e_paths("fail"), ["fx_group"]),
                                                                 sort_keys=True))
    report_absent = evaluate(e2e_paths("absent"), ["fx_group"])
    expect("e2e v2 absent: 'v2 not generated' with exit 0",
           report_absent["status"] == "V2_NOT_GENERATED" and exit_code(report_absent["status"]) == 0,
           report_absent["status"])
    try:
        evaluate(e2e_paths("pass"), ["nope_group"])
        expect("unknown group is an input error", False)
    except ParityError:
        expect("unknown group is an input error", True)

    # Smoke run on the real repository (v1 side must load; v2 may be absent)
    try:
        real = evaluate(Paths(), None)
        expect("--all on the real tree: 35 pages, v1 side loads, status is a known state",
               len(real["pages"]) == EXPECTED_PAGES and real["counts"]["v1_names"] > 0
               and real["status"] in ("V2_NOT_GENERATED", "PASS", "FAIL"), real["status"])
    except ParityError as exc:
        expect("--all on the real tree loads", False, exc)

    passed = sum(results)
    print(f"selftest: {passed}/{len(results)} passed")
    return 0 if passed == len(results) else 1


def main(argv: Optional[list] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Director kit v1-to-v2 parity gate (DIRECTOR_KIT_P0_SPEC.md section 6.2).")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--groups", metavar="GROUP[,GROUP...]",
                      help="page groups to check, e.g. light_group; 'all' means --all")
    mode.add_argument("--all", action="store_true", help="check all 35 pages in page_sources.json")
    mode.add_argument("--selftest", action="store_true", help="run the fixture tests and exit")
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    args = parser.parse_args(argv)
    try:
        if args.selftest:
            return selftest()
        groups = None
        if not args.all and args.groups.strip() != "all":
            groups = [g.strip() for g in args.groups.split(",") if g.strip()]
            if not groups:
                raise ParityError("--groups needs at least one group")
        report = evaluate(Paths(), groups)
    except ParityError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        _print_text(report)
    return exit_code(report["status"])


if __name__ == "__main__":
    sys.exit(main())
