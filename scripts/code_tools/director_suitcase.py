#!/usr/bin/env python3
"""Generate the local-history Director integration suitcase (stdlib; python -I safe).

Never contacts a remote. The committed delta is always BASE..HEAD. Source-tree
fingerprints also detect dirty inputs; hook coordinates always describe HEAD.
ReShade shader content (.fx/.fxh) is opaque: inventory paths only, never read it.
"""
from __future__ import annotations

import argparse
import codecs
import collections
import contextlib
import csv
import hashlib
import io
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from xml.etree import ElementTree as ET

BASE = "7c11d3f38bd"
ROOT = Path(__file__).resolve().parents[2]
OUT = "doc/director_suitcase/"
TOOL = "scripts/code_tools/director_suitcase.py"
EVIDENCE = ["doc/director_kit/wiring/wiring_table.csv",
            "doc/director_kit/functional/functional_manifest.json",
            "doc/director_kit/page_sources.json"]

# These are discovery rules, not assertions that every matching edit is required.
RULES = {
    "director-shell": r"alkit|kit_|kit/|director(?!actor|switch|anim|y)|dpage|kit_row|kit_page",
    "common-ui": r"scrollfit|scrollfocus|sliderwheel|uifit|uirefresh|flowgrid|compassdial|undosequence|undovalue|settingsundo|setMixedValue",
    "actor-bar": r"directoractorchoice|actor.bar|avatarkey|avatarsubjectpicker|avatarstaging|directorcast",
    "actor-path": r"actormover|propmover|objectpath|pathgeometry|pathedit|pathcamera|trajectory|formation|danceball|animswitcher|animationexplorer|posepolish|posesecondary|posecontinuity",
    "clones": r"ghost|clone|avatarconstraint|avataranimationtime|presentationtime|temporalframecontext|AvatarOuterModelView",
    "crowd": r"crowd|ghostgroup|formation",
    "rig": r"cinelight|cine_light|cine_gobos|cinerig|cine_rig|rigrim|riggroup|lightrigmap|liveprobe|probedirty|probereach|probeschedule|localLightSize|locallightsize|relight|areashadow|shadowdepth|shadowslot|worldshadow|screenSpaceRayUtil|area_trace|area_near_steps|area_thin_objects",
    "gaze": r"gaze|headrotmotion|targetingmotion",
    "camera": r"cinematiccam|cinecam|cameraoperator|camera_operator|cameracurve|cameradispatch|flycam|autoframe|auto_frame|cameraqol|camerastraightlift|camerafollowjoint|camerashake|cameramaxroll|usecinematiccamera|allowcameraflip|mouselookoffset|director(?:s|_s)witch|joystick|xinput",
    "vcam": r"vcam|prism|virtual.camera|llmediaframe|llpluginfile|llpluginloop",
    "tracking": r"projectortrack|projector_track",
    "looks": r"lenslook|lens_look|actor.?fx|avatar.?light.?response|actor.?style|lightbox|colorcorrect|colorgrade|tonemap|\blut\b|colorlut|renderlut|lut3d|allpm|alpresetlook|colorchart|infrared|polarizer|gradnd|softedge|lightresponse",
    "roto-tron": r"roto|tron|actor.?mask|forced.?mask",
    "weather": r"weather|\brain\b|raindrop|rainoccl|rainenabled|rainquality|rainintensity|rainF\.glsl|rainV\.glsl|lightning|splash|wetness|puddle|snow",
    "shafts-volumetrics": r"projectorshaft|projector_shaft|projvol|projectorvolum|froxel|localfog|local_fog|cinehaze|atmosphericvol|volumetric",
    "sensor": r"sensor|exposure|slog|s_log|envintensity|env_intensity|envonly|env_only",
    "composition": r"composition|viewfinder|clientheight|client_height|worldoverlay|focuspick",
    "capture-sidecar": r"reshade|sidecar|velocity[FV]\.glsl|gVelocity|mVelocityMap|VelocityBuffer|sVelocityRender|AvatarVelocity|renderVelocity|deferred/velocity|motionblur|motion_blur|temporalcapture|temporal_capture|10.?bit|outertransform",
    "stepped-animation": r"steppedanim|stepped_anim|stepanim|reflectanim|reflectmath|avataranimspeed|avatar_anim_speed|loopseam",
    "environment": r"windlight|environmentsettings|environment_settings|adjust_water|wateradjust|bdmergeenvlibrary|environmentpreset|menu_save_settings_adjust",
}
REGEX = {k: re.compile(v, re.I) for k, v in RULES.items()}
PERF_PATH = re.compile(r"(?:^|/)(?:alframelimiter|alvramrecovery|bdmerge(?:memorybudget|meshpool|texpool|texspike)|lldrawbudget)|spgo|(?:^|/)scripts/perf/(?!director_functional_manifest|functional_report|ui_selftest)", re.I)
PERF_CODE = re.compile(r"VRAMPressure|VRAMBudget|MemoryBudget|BDMerge(?:MeshPool|TexPool|TexSpike)|SPGO|thread.pool|thread.priority|thread.affinity|decode.pool|framelimiter|texture.pool|texpool|memory.governor|emergency.purge|QualityOverrideAllowed", re.I)
OTHER = re.compile(r"(?:fsinventorycustomtab|fs(?:floater|panel)?radar|fs_radar|fsradar|inventorycustomtabs|localasset)|/branding/", re.I)
LAYERS = {
    "common-ui": 1, "director-shell": 2, "actor-bar": 3, "actor-path": 3,
    "camera": 3, "gaze": 3, "stepped-animation": 3, "composition": 3,
    "clones": 4, "crowd": 4, "roto-tron": 5, "capture-sidecar": 6,
    "rig": 7, "tracking": 7, "shafts-volumetrics": 7, "weather": 8,
    "vcam": 9, "looks": 10, "sensor": 10,
    "environment": 8,
}
TESTS = {
    "director-shell": "Open every tab, search, Quick controls, undo/redo, reset, scene save/load and hidden-tab refresh.",
    "common-ui": "Resize and scroll embedded panels; slider wheel settles once; hidden tabs do no expensive work.",
    "actor-bar": "Change actor once; all single-actor pages follow; delete/rekey/load a clone without stale selection.",
    "actor-path": "Walk/stop real actor and clone; path edits, fillets, formation, props and dance survive scene reload.",
    "camera": "Arm/release, switch/ease, object/bone lock, orbit, operator modes and recording; restore main camera.",
    "gaze": "Actor and group targeting, blink, lens tracking; disable restores ordinary head motion.",
    "stepped-animation": "Real/clone scope, independent animation speed, step cadence and exclusion of camera-mounted actors.",
    "composition": "Change guide/aspect, resize viewport and enter capture; overlays align and clear when disabled.",
    "clones": "Spawn/delete independent clones, attachments, pitch/roll and looks; real avatar never teleports/detaches.",
    "crowd": "Place a group; vary Width/Depth/Columns/Rows; move/delete/reload group and preserve stable IDs.",
    "roto-tron": "Toggle actor mask, Roto and Tron independently on real/clone actors; ordinary beauty pass stays opaque.",
    "capture-sidecar": "Check depth/normal/albedo/motion provenance, first-frame invalidity, cuts, resize and capture-off path.",
    "rig": "Create multiple lights, target groups, cues, live/wearable probes and hero mirror; save/load and disable.",
    "tracking": "Track moving actor/group with projector; remove target, cross region and release without stale pointers.",
    "shafts-volumetrics": "Projector shafts, local fog and froxel coverage; move camera/light; resize/login/off path restores GL state.",
    "weather": "Rain/splash/lightning under cover and outside, pause/time, resize, region change and weather-off beauty pass.",
    "vcam": "Full prism matrix: main/preview/capture views, bone/object POV, cuts, recursion guards, resize/release and relog.",
    "looks": "Replace complete looks, preserve targeting and shot keys; reset/undo, custom preset round trip, LUT/grade/CAS.",
    "sensor": "Exposure/ISO/quantization and color chart in main/VCam/capture; bypass/legacy paths; scene and preset round trips.",
    "environment": "Load bundled sky/day/water looks, change environment and water controls, reset and check region override behavior.",
}


def git(root, *args, data=None):
    """Fixed local revisions only; bypass external diff/textconv and locale decoding."""
    cmd = ["git", "-c", "core.quotepath=false", *args]
    result = subprocess.run(cmd, cwd=root, input=data, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise RuntimeError(f"{cmd!r}: {result.stderr.decode('utf-8', 'replace')}")
    return result.stdout


def decode(data):
    return data.decode("utf-8", "replace")


def tags(text):
    return sorted(k for k, pattern in REGEX.items() if pattern.search(text))


def layer(features):
    return min((LAYERS[f] for f in features if f in LAYERS), default=99)


def classify(path, status, content=""):
    """Provenance first within product code; OUT decisions require narrow rules."""
    low = path.lower()
    if low.startswith((".tmp/", ".tmp.", "build-", "reshade-addon/vendor/", "vcpkg/")) or "/__pycache__/" in low or low.endswith((".tmp", ".pyc", ".log", ".exe", ".pdb")):
        return "EXCLUDED-OTHER", "local/build artifact or vendored dependency; do not port"
    if low.startswith(("doc/", "docs/")) or low.endswith((".md", ".png", ".pdf")) and not low.startswith("indra/"):
        return "GENERATED", "documentation/evidence/render; regenerate or retain as reference, not source integration"
    if "generated by" in content[:1800].lower() or Path(path).name in ("director_kit_index.xml", "gen_report.txt") or Path(path).name.startswith("panel_dpage_") or Path(path).name == "floater_director_v2.xml":
        return "GENERATED", "generator output; runtime XUI must be regenerated and shipped"
    if PERF_PATH.search(path):
        return "EXCLUDED-PERF", "pure performance/governor/pool/frame-limiter feature; adapt any creative caller"
    if low.startswith(("scripts/", ".github/")) or "/tests/" in low or low.endswith((".ps1", ".bat")) or "aluiperfselftest" in low:
        return "TOOLING", "generator/check/fixture/build tooling; carry validation separately"
    if OTHER.search(path):
        return "EXCLUDED-OTHER", "unrelated inventory/radar/local-asset feature"
    if not low.startswith(("indra/", "reshade-addon/")):
        return "EXCLUDED-OTHER", "repository metadata or local handoff/review artifact"
    if low.endswith(("threadpool.cpp", "llprofiler.h")):
        return "EXCLUDED-PERF", "thread-pool/profiling-only support; creative dependency not established"
    return ("OWNED", "absent at local base; drop-in source unit, dependencies still required") if status == "A" else ("HOOK", "present at local base; adapt individual hunks, never copy wholesale")


def changed_paths(root):
    fields = git(root, "diff", "--no-ext-diff", "--no-textconv", "--name-status", "--no-renames", "-z", BASE, "HEAD").split(b"\0")
    result = {decode(fields[i + 1]): decode(fields[i]) for i in range(0, len(fields) - 1, 2)}
    if any("\n" in p or "\r" in p for p in result):
        raise ValueError("newline-containing paths are unsupported by the blob protocol; no outputs written")
    return result


def blobs(root, revision, paths):
    """Batch reads avoid thousands of subprocesses. Deleted/missing paths are None."""
    names = sorted(p for p in paths if Path(p).suffix.lower() not in (".fx", ".fxh"))
    result = git(root, "cat-file", "--batch", data="".join(f"{revision}:{p}\n" for p in names).encode())
    pos, answer = 0, {}
    for path in names:
        end = result.index(b"\n", pos)
        header = result[pos:end]
        pos = end + 1
        if header.endswith(b" missing"):
            answer[path] = None
        else:
            size = int(header.rsplit(b" ", 1)[1])
            answer[path] = result[pos:pos + size]
            pos += size + 1
    return answer


def histories(root):
    result = collections.defaultdict(list)
    raw = git(root, "log", "--format=%x1e%h%x00%s%x00", "--name-only", "-z", f"{BASE}..HEAD")
    for record in raw.split(b"\x1e")[1:]:
        fields = record.split(b"\0")
        sha, subject = decode(fields[0]), decode(fields[1])
        for field in fields[2:]:
            path = decode(field).strip("\n")
            if path and len(result[path]) < 5:
                result[path].append({"commit": sha, "subject": subject})
    return result


def diff_args():
    return ["--no-ext-diff", "--no-textconv", "--no-color", "--no-renames", "--unified=3",
            "--src-prefix=a/", "--dst-prefix=b/", "--line-prefix=", "--inter-hunk-context=0"]


def patch_path(value, prefix):
    value = value.rstrip("\t")  # Git terminates unquoted space-containing paths with a tab.
    if value.startswith('"'):
        value = codecs.escape_decode(value[1:-1].encode("utf-8"))[0].decode("utf-8")
    if not value.startswith(prefix):
        raise ValueError("unexpected patch prefix: " + value)
    return value[len(prefix):]


def split_diff(raw):
    result = collections.defaultdict(list)
    path, current = None, None
    for line in raw.splitlines():
        if line.startswith("diff --git "):
            current, path = None, None
        elif line.startswith("+++ ") and line != "+++ /dev/null":
            path = patch_path(line[4:], "b/")
        elif line == "+++ /dev/null":
            pass  # deletion path was set by ---
        elif line.startswith("--- "):
            if line != "--- /dev/null":
                path = patch_path(line[4:], "a/")
        elif line.startswith("@@ "):
            match = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)", line)
            if not match or path is None:
                raise ValueError(f"cannot parse hunk: {line}")
            a, ac, b, bc, context = match.groups()
            current = {"base_start": int(a), "base_count": int(ac or 1),
                       "head_start": int(b), "head_count": int(bc or 1),
                       "git_context": context.strip(), "patch": []}
            result[path].append(current)
        elif current is not None and line[:1] in ("+", "-", " ", "\\"):
            current["patch"].append(line)
    return result


def function_ranges(source):
    """Conservative brace/signature scan; not a C++ parser. Unknowns stay explicit."""
    # Preserve offsets while hiding strings/comments, including braces in them.
    lex = re.compile(r'R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
    clean = lex.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), source)
    stack, result = [], []
    for match in re.finditer(r"[{}]", clean):
        offset = match.start()
        if match.group() == "{":
            start = max(clean.rfind("}", 0, offset), clean.rfind("{", 0, offset), clean.rfind(";", 0, offset)) + 1
            signature = clean[start:offset].strip()
            found = re.search(r"((?:[\w~]+::)*[\w~]+)\s*\([^;{}]*\)\s*(?:const\b|override\b|final\b|noexcept\b|->[\w:<>*&\s]+|\s)*$", signature)
            name = found.group(1) if found and found.group(1) not in ("if", "for", "while", "switch", "catch") else None
            stack.append((offset, name))
        elif stack:
            opening, name = stack.pop()
            if name:
                result.append((source.count("\n", 0, opening) + 1,
                               source.count("\n", 0, offset) + 1, name))
    return sorted(result)


def hunk_detail(hunk, path, source, ranges, subjects):
    hunk = dict(hunk)
    hunk["patch"] = "\n".join(hunk["patch"]) + "\n"
    delta = "\n".join(line[1:] for line in hunk["patch"].splitlines() if line.startswith(("+", "-")))
    b, count = hunk["head_start"], hunk["head_count"]
    changed_lines, at = [], b
    for line in hunk["patch"].splitlines():
        if line.startswith(("+", "-")):
            changed_lines.append(at)
        if line.startswith(("+", " ")):
            at += 1
    funcs = sorted({name for first, last, name in ranges
                    if any(first <= line <= last for line in changed_lines or [b])})
    if not funcs:
        funcs = ["unknown/non-function (" + (hunk["git_context"] or "declaration, XML, build list or global scope") + ")"]
    features = tags(delta + "\n" + path)
    evidence = ["filename/changed-code regex"] if features else []
    candidates = tags("\n".join(s["subject"] for s in subjects))
    if not features:
        features = ["unknown"]
    comments = [re.sub(r"^\s*(?://+|/\*+|\*|<!--)\s*", "", s).strip()
                for s in delta.splitlines() if re.match(r"\s*(?://|/\*|<!--)", s)]
    identifiers = list(dict.fromkeys(re.findall(r"\b(?:AL|LL|BDMerge|Render|g[A-Z])[A-Za-z0-9_]+", delta)))
    purpose = ("Comment evidence: " + " ".join(comments)[:280]) if comments else "Edits " + (", ".join(identifiers[:10]) or "declarations/data/build wiring; purpose unknown")
    conditions = [s.strip()[:220] for s in delta.splitlines()
                  if re.search(r"\bif\s*\(|^\s*#\s*(?:if|ifdef|ifndef)|LLCachedControl|gSavedSettings\.get", s)]
    hunk.update({"id": f"{path}@{b},{count}", "head_end": b + count - 1 if count else b,
                 "range_note": "includes three context lines; count=0 is deletion anchor",
                 "functions": funcs, "function_method": "HEAD lexical brace/signature scan; unknowns retain git context",
                 "features": features, "candidate_features_from_subjects": candidates,
                 "feature_evidence": evidence, "slice": layer(features),
                 "purpose": purpose, "guarded": "unknown; conditional evidence present" if conditions else "unknown; no local conditional evidence",
                 "guard_evidence": conditions, "scope": "mixed/perf-review" if PERF_CODE.search(delta) else "creative-candidate" if features != ["unknown"] else "unknown-review",
                 "settings_references": sorted(set(re.findall(r'"([A-Za-z][A-Za-z0-9_]+)"', delta))),
                 "recent_file_subjects": subjects})
    return hunk


def llsd(node):
    if node is None:
        return None
    if node.tag == "llsd":
        return llsd(next(iter(node)))
    if node.tag == "map":
        children = list(node)
        if len(children) % 2 or any(children[i].tag != "key" for i in range(0, len(children), 2)):
            raise ValueError("malformed LLSD map")
        result = {}
        for i in range(0, len(children), 2):
            if children[i].text in result:
                raise ValueError(f"duplicate LLSD key: {children[i].text}")
            result[children[i].text] = llsd(children[i + 1])
        return result
    if node.tag == "array":
        return [llsd(n) for n in node]
    # Preserve scalar type as well as value: integer 1 and Boolean true differ.
    return {"tag": node.tag, "value": node.text or ""}


def scalar(value):
    return value.get("value", "") if isinstance(value, dict) and "tag" in value else value


def setting_document(raw):
    """Preserve duplicate declarations instead of guessing the viewer's winner."""
    declarations = collections.defaultdict(list)
    if not raw:
        return declarations
    root = ET.fromstring(raw)
    mapping = root.find("map")
    if mapping is None:
        raise ValueError("settings document has no LLSD map")
    children = list(mapping)
    if len(children) % 2:
        raise ValueError("malformed top-level settings LLSD map")
    for i in range(0, len(children), 2):
        if children[i].tag != "key":
            raise ValueError("settings key expected")
        declarations[children[i].text].append(llsd(children[i + 1]))
    return declarations


def strip_comments(source):
    return re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", "", source)


def scene_membership(texts):
    result = {}
    director = texts.get("indra/newview/llfloaterdirector.cpp", "")
    match = re.search(r"LLFloaterDirector::sceneSettingsList\(\)[\s\S]*?settings\s*=\s*\{([\s\S]*?)\};", director)
    if match:
        for key in re.findall(r'"(\w+)"', strip_comments(match.group(1))):
            result[key] = "yes: explicit sceneSettingsList -> captureSceneData settings map"
    for filename, signature in (("alrotoink.cpp", "ALRotoInk::settings()"), ("altron.cpp", "ALTron::settings()")):
        source = texts.get("indra/newview/" + filename, "")
        clean = strip_comments(source)
        match = re.search(r"kAllSettings\[\]\s*=\s*\{([\s\S]*?)\};", clean)
        body = clean[clean.find(signature):] if signature in clean else ""
        if match and signature in director and "std::begin(kAllSettings)" in body:
            owned = set(re.findall(r'"(\w+)"', match.group(1)))
            if filename == "altron.cpp":
                excluded = re.search(r"kDirectorExcluded\[\]\s*=\s*\{([\s\S]*?)\};", clean)
                if not excluded or "kDirectorExcluded" not in body:
                    continue
                for key in re.findall(r'"(\w+)"', excluded.group(1)):
                    owned.discard(key)
                    result[key] = "no: explicitly excluded by ALTron::settings()"
            for key in owned:
                result[key] = "yes: appended kAllSettings (with scene exclusions) in " + filename
    if 'scene["cinecam_mode"]' in director and '"CinematicCamMode"' in director:
        result["CinematicCamMode"] = "yes: separate cinecam_mode scene field"
    return result


def settings_rows(paths, head, base, scene_source, membership):
    result = []
    for path in sorted(paths):
        if "/app_settings/" not in path or not re.match(r"settings.*\.xml$", Path(path).name):
            continue
        old = setting_document(base.get(path))
        new = setting_document(head.get(path))
        # settings_files.xml is a configuration registry, not setting declarations.
        for key in sorted(set(old) | set(new)):
            old_declarations, new_declarations = old.get(key, []), new.get(key, [])
            before = old_declarations[-1] if old_declarations else None
            after = new_declarations[-1] if new_declarations else None
            record = after if after is not None else before
            if old_declarations == new_declarations or not isinstance(record, dict) or "Type" not in record or "Value" not in record:
                continue
            comment = str(scalar(record.get("Comment", {}))).strip()
            scene_refs = []
            for scene_path, source in scene_source.items():
                for i, line in enumerate(source.splitlines(), 1):
                    if f'"{key}"' in line:
                        scene_refs.append(f"{scene_path}:{i}")
            result.append({"name": key, "change": "removed" if after is None else "added" if before is None else "changed",
                           "type": scalar(record["Type"]), "default": scalar(record["Value"]),
                           "previous_default": scalar(before["Value"]) if before else None,
                           "file": path, "features": tags(key + " " + comment) or ["unknown"],
                           "persisted": scalar(record.get("Persist", {})) or "unspecified (viewer policy)",
                           "saved_in_scenes": membership.get(key, "unknown; structured blocks/dynamic ownership require review"),
                           "scene_evidence": scene_refs, "scope": "EXCLUDED-PERF candidate" if PERF_CODE.search(key + " " + comment) else "review feature ownership",
                           "declaration_count_head": len(new_declarations), "declaration_count_base": len(old_declarations),
                           "duplicate_note": "ambiguous duplicate declarations; default shows last text occurrence, not proven runtime policy" if len(new_declarations) > 1 else "",
                           "head_declarations": new_declarations if len(new_declarations) > 1 else [],
                           "comment": comment})
    return result


def csv_bytes(rows, fields):
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore", lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow({k: json.dumps(v, ensure_ascii=False, sort_keys=True) if isinstance(v, (dict, list)) else v for k, v in row.items() if k in fields})
    return stream.getvalue().encode("utf-8")


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")


def md(value):
    return str(value).replace("|", "\\|").replace("\n", " ").replace("`", "'")


def output_names():
    return ["SUITCASE.md", "inventory.csv", "inventory.json", "hooks.md", "hooks.json",
            "settings.csv", "settings.json", "features.json", "inputs.json", "compatibility.json", "unknowns.md"] + [f"features/{f}.md" for f in sorted(RULES)]


def collect(root):
    paths = changed_paths(root)
    tracked = [decode(p) for p in git(root, "ls-files", "-z").split(b"\0") if p]
    # Include this kit before its first commit; stable once generated files are committed.
    paths.setdefault(TOOL, "A")
    for name in output_names():
        paths.setdefault(OUT + name, "A")
    head = blobs(root, "HEAD", [p for p in paths if not p.startswith(OUT) and p != TOOL])
    base = blobs(root, BASE, [p for p in paths if not p.startswith(OUT) and p != TOOL])
    hist = histories(root)
    diff = decode(git(root, "diff", *diff_args(), BASE, "HEAD", "--", "indra", "reshade-addon",
                      ":(exclude,icase)*.fx", ":(exclude,icase)*.fxh"))
    patches = split_diff(diff)
    inventory, texts = [], {}
    for path, status in sorted(paths.items()):
        raw = head.get(path) or base.get(path) or b""
        source = decode(raw) if b"\0" not in raw else ""
        kind, reason = classify(path, status, source)
        features = tags(path)
        evidence = ["path/prefix"] if features else []
        if kind in ("OWNED", "HOOK") or kind == "GENERATED" and "/xui/" in path:
            texts[path] = source
            delta = "\n".join("\n".join(h["patch"]) for h in patches.get(path, []))
            discovered = tags(source if kind in ("OWNED", "GENERATED") else delta)
            if discovered:
                features = sorted(set(features + discovered))
                evidence.append("owned source / hook delta identifiers and comments")
        inventory.append({"path": path, "git_status": status, "class": kind, "reason": reason,
                          "features": features or ["unknown"], "feature_evidence": evidence,
                          "candidate_features_from_subjects": tags("\n".join(s["subject"] for s in hist.get(path, []))) if kind in ("OWNED", "HOOK") else [],
                          "layer": layer(features), "deleted": status == "D"})
    by_path = {r["path"]: r for r in inventory}
    # One-hop include graph plus qualified-call evidence. No recursive tag flooding.
    basenames = collections.defaultdict(list)
    for path in tracked:
        basenames[Path(path).name].append(path)
    direct = {p: set(r["features"]) - {"unknown"} for p, r in by_path.items()}
    compatibility, graph = [], {}
    for path, source in texts.items():
        includes, unresolved = [], []
        for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', source, re.M):
            candidates = basenames.get(Path(name).name, [])
            local = (Path(path).parent / name).as_posix()
            if local in candidates:
                candidates = [local]
            if len(candidates) == 1:
                includes.append(candidates[0])
            else:
                unresolved.append({"include": name, "candidates": candidates})
        calls = sorted(set(re.findall(r"\b((?:AL|LL|BDMerge)\w+)::(\w+)\s*\(", source)))
        graph[path] = {"includes": sorted(set(includes)), "unresolved_includes": unresolved,
                       "qualified_call_candidates": ["::".join(c) for c in calls]}
        if by_path[path]["features"] == ["unknown"]:
            found = set().union(*(direct.get(p, set()) for p in includes)) if includes else set()
            if found:
                by_path[path]["features"] = sorted(found)
                by_path[path]["feature_evidence"] = ["one-hop include neighbors; tentative, not required closure"]
                by_path[path]["layer"] = layer(found)
        for dependency in includes:
            if Path(dependency).name.startswith(("al", "fs", "bdmerge")) or paths.get(dependency) == "A":
                dep_row = by_path.get(dependency, {})
                excluded = dep_row.get("class", "").startswith("EXCLUDED")
                compatibility.append({"consumer": path, "dependency": dependency,
                                      "origin": "fork-added" if dependency in paths and paths[dependency] == "A" else "present at local Alchemy base",
                                      "action": "OUT: remove unrelated caller or adapt creative caller to a minimal target contract; do not port excluded feature" if excluded else "carry/adapt fork source" if paths.get(dependency) == "A" else "LL target: inspect Alchemy implementation; provide equivalent or port with dependencies",
                                      "excluded": excluded, "class": dep_row.get("class", "base-owned"), "reason": dep_row.get("reason", "present at base")})
    hooks = []
    for row in inventory:
        if row["class"] != "HOOK":
            continue
        path, source = row["path"], decode(head.get(row["path"]) or b"")
        ranges = function_ranges(source) if Path(path).suffix in (".cpp", ".h", ".glsl", ".mm") else []
        hunks = [hunk_detail(h, path, source, ranges, hist.get(path, [])) for h in patches.get(path, [])]
        hooks.append({"path": path, "slice": min((h["slice"] for h in hunks), default=row["layer"]),
                      "features": row["features"], "hunks": hunks,
                      "note": "binary/mode-only/deletion without textual hunks; review file-level delta" if not hunks else "HEAD coordinates; guards/features are candidate evidence"})
    hooks.sort(key=lambda r: (r["slice"], r["path"]))
    scenes = {p: s for p, s in texts.items() if Path(p).name in ("llfloaterdirector.cpp", "alcinelightrig.cpp", "alkitpresets.cpp")}
    settings = settings_rows(paths, head, base, scenes, scene_membership(texts))
    # CMake evidence is exact matching lines, not invented build target names.
    cmake = {p: decode((root / p).read_bytes()).splitlines() for p in tracked if Path(p).name == "CMakeLists.txt" and (root / p).is_file()}
    cmake_index = collections.defaultdict(list)
    for p, lines in cmake.items():
        for i, line in enumerate(lines, 1):
            for token in re.findall(r"[\w./-]+\.(?:cpp|h|glsl|xml)\b", line):
                cmake_index[Path(token).name].append({"file": p, "line": i, "text": line.strip()})
    wiring = list(csv.DictReader(io.StringIO((root / EVIDENCE[0]).read_text(encoding="utf-8"))))
    wiring_features = [(r, tags(" ".join(str(v) for v in r.values()))) for r in wiring]
    manifest = json.loads((root / EVIDENCE[1]).read_text(encoding="utf-8"))
    pages = json.loads((root / EVIDENCE[2]).read_text(encoding="utf-8"))
    csv_hash = hashlib.sha256((root / EVIDENCE[0]).read_bytes()).hexdigest()
    kit_config = json.loads((root / "scripts/code_tools/director_kit/kit_pages.json").read_text(encoding="utf-8"))
    evidence_status = {"wiring_rows": len(wiring), "functional_meta": manifest.get("meta"),
                       "page_summary": pages.get("summary"), "functional_csv_hash_matches": manifest.get("meta", {}).get("csv_sha256") == csv_hash,
                       "kit_config_sections": sorted(kit_config), "kit_config_p0_pages": sorted(kit_config.get("p0_pages", {})),
                       "limits": "Existing generated evidence is consumed, not rerun; hashes/counts are not runtime proof."}
    features = {}
    for feature in sorted(RULES):
        rows = [r for r in inventory if feature in r["features"]]
        product = [r for r in rows if r["class"] in ("OWNED", "HOOK", "GENERATED") and r["path"].startswith(("indra/", "reshade-addon/"))]
        selected = {r["path"] for r in product}
        features[feature] = {
            "layer": LAYERS[feature], "checkpoint": TESTS[feature],
            "owned": [r["path"] for r in product if r["class"] == "OWNED"],
            "hooks": [{"path": h["path"], "hunks": [n["id"] for n in h["hunks"] if feature in n["features"]]} for h in hooks if any(feature in n["features"] for n in h["hunks"])],
            "settings": [s["name"] for s in settings if feature in s["features"]],
            "shaders": sorted(p for p in selected if p.endswith((".glsl", ".fx", ".fxh"))),
            "xui": sorted(p for p in selected if "/xui/" in p),
            "generated_runtime": sorted(r["path"] for r in product if r["class"] == "GENERATED"),
            "cmake_entries": [dict(entry, source=p) for p in sorted(selected) for entry in cmake_index.get(Path(p).name, [])],
            "source_dependencies": {p: graph[p] for p in sorted(selected) if p in graph},
            "kit_wiring_pages": sorted({r.get("page", "") for r, found in wiring_features if feature in found}),
            "checks": sorted(p for p in paths if p.startswith("scripts/") and p.endswith(".py") and ("check" in p or "test" in p or "parity" in p) and feature in tags(p)),
        }
    # Fingerprint all tracked source/tool/kit evidence plus this untracked generator.
    inputs = sorted(set(p for p in tracked if p.startswith(("indra/", "scripts/", "reshade-addon/", "doc/director_kit/")) and not p.startswith(OUT)) | {TOOL, *EVIDENCE, "CLAUDE.md", "LICENSE", "doc/MERGE_PLAYBOOK.md", "doc/MERGE_FORK_DIRECTOR_CONSOLE_PLAN.md"})
    opaque = [p for p in inputs if Path(p).suffix.lower() in (".fx", ".fxh")]
    fingerprints = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() if (root / p).is_file() else "missing"
                    for p in inputs if p not in opaque}
    dirty = decode(git(root, "diff", "--no-ext-diff", "--no-textconv", "--name-only", "HEAD", "--", "indra", "scripts", "reshade-addon")).splitlines()
    dirty = [p for p in dirty if p != TOOL]
    meta = {"base": BASE, "head_source_tree": decode(git(root, "rev-parse", "HEAD:indra")).strip(),
            "coordinate_source": "committed HEAD (not dirty worktree)", "dirty_product_inputs": dirty,
            "scope": "net committed local base..HEAD, plus this generator and declared output paths; no remote/stash history",
            "opaque_shader_paths": opaque,
            "opaque_shader_policy": "path inventory only; no source, textual hunks or content fingerprints for .fx/.fxh",
            "input_sha256": hashlib.sha256(json_bytes(fingerprints)).hexdigest(),
            "counts": dict(sorted(collections.Counter(r["class"] for r in inventory).items())),
            "hook_files": len(hooks), "hunks": sum(len(h["hunks"]) for h in hooks),
            "settings": len(settings), "evidence": evidence_status}
    return inventory, hooks, settings, features, compatibility, fingerprints, meta


def feature_markdown(feature, info):
    lines = [f"# {feature}: dependency evidence", "", f"Landing layer {info['layer']}. {info['checkpoint']}", "",
             "Candidate associations require review. Includes/calls are discovery edges, not a complete call graph or proof of build closure.", ""]
    for key in ("owned", "hooks", "settings", "shaders", "xui", "generated_runtime", "cmake_entries", "kit_wiring_pages", "checks"):
        lines += ["## " + key.replace("_", " "), ""]
        for item in info[key]:
            if isinstance(item, dict):
                lines.append("- " + md(json.dumps(item, ensure_ascii=False)))
            else:
                lines.append(f"- `{item}`")
        if not info[key]:
            lines.append("None identified by local evidence; not a claim that none is needed.")
        lines.append("")
    lines += ["Full include/call edges: ../features.json. Shared hooks: ../hooks.md. Setting values and save coverage: ../settings.csv.", ""]
    return "\n".join(lines).encode()


def guide(meta, features):
    rows = "\n".join(f"| [{f}](features/{f}.md) | {info['layer']} | {len(info['owned'])} | {len(info['hooks'])} | {len(info['settings'])} |" for f, info in sorted(features.items(), key=lambda x: (x[1]['layer'], x[0])))
    return f'''# Director Suitcase

This is a generated integration kit for the Director and its creative features,
with source provenance, hook patches and dependency evidence. It is a porting
ledger and guide, not an apply-ready patch or a claim of target buildability.

**All hooks are mapped against local Alchemy base `7c11d3f38bd` (July 2026).
Current upstream may have drifted.** “Upstream Alchemy” here means that local
base; “LL viewer” means the LL-origin `ll*` code present at that base. No current
remote state, stash or external source was consulted. Coordinates describe
committed HEAD. `inputs.json` identifies its `indra` tree and fingerprints the
working source; dirty source is disclosed separately and needs a commit before
it can enter this committed-history hook map.
ReShade `.fx`/`.fxh` files have path inventory only: their contents, textual hunks
and content fingerprints are excluded (see `opaque_shader_paths` in `inputs.json`).

## Regenerate and check

Run from the repository root with Python 3 (stdlib only):

```powershell
python -I scripts/code_tools/director_suitcase.py
python -I scripts/code_tools/director_suitcase.py --check
python -I scripts/code_tools/director_suitcase.py --selftest
```

Default mode writes only `doc/director_suitcase/`. Check computes the same bytes,
returns 1 for stale/missing/unexpected outputs, and never repairs them. Self-test
uses temporary planted fixtures under this worktree's `.tmp/` and creates no commits.
This gate is not added to prebuild_guard.
After source/kit evidence changes, regenerate upstream kit evidence with its
existing generators as appropriate, then regenerate this suitcase. `--check`
does not run builds, GL harnesses or other generators. Missing/malformed required
inputs fail generation. No timestamp or changing HEAD commit label is embedded;
committing only these outputs should not itself make them stale.

## Read the ledger before copying

`inventory.csv` / `inventory.json` contain {sum(meta['counts'].values())} classified paths.
OWNED means absent at the local base: copy source units with their dependencies.
It does not mean standalone or certified creative-only. HOOK means a pre-existing
LL/Alchemy file: adapt individual hunks from `hooks.json`, never copy that file
wholesale. GENERATED covers documents/evidence/renders and generated runtime XUI:
**regenerate and ship required runtime XUI**. TOOLING carries generators, checks
and fixtures separately. EXCLUDED-PERF is pure governor/pool/threading/tuning work;
EXCLUDED-OTHER records artifacts, vendors and unrelated features with reasons.
Mixed shared files remain HOOK; each hunk marked `mixed/perf-review` requires
creative/perf severance. Unrecognised paths/functions/guards remain explicitly
unknown; `unknowns.md` is the explicit review queue. Feature associations use path, changed code/comments, recent file-level
commit subjects and one-hop includes/qualified calls. Commit-only associations are
kept as separate `candidate_features_from_subjects`, with ownership still unknown.
They are candidate ownership,
not a compiler-derived dependency graph. Generic names can overmatch; inspect the
patch and evidence before landing. Layer 99 means unresolved.

## Features and dependency layers

Start common helpers and kit shell, then page/controller registration, then complete
feature vertical slices. A page often depends on a feature controller; shell-first
does not authorize linking all pages against missing engines. Add each page with
its engine, settings, assets, hooks and shader closure. Each linked feature sheet
lists OWNED sources, exact HOOK hunk IDs, setting keys, shaders, XUI, generated
runtime files, CMake entry lines, kit pages and candidate checks. `features.json`
adds include/call edges; `compatibility.json` lists Alchemy/fork helper dependencies.

| Feature sheet | Layer | OWNED | HOOK files | Setting changes |
|---|---:|---:|---:|---:|
{rows}

These layers are a review order. A shared file/hunk can serve several layers and
must be split by contract. There is no claim that mechanically applying a layer
compiles; establish source/include/link, settings/UI and shader closure on the
target before its checkpoint.

## Porting order and checkpoint tests

Reuse the methodology and Steps 0–12 in `../MERGE_PLAYBOOK.md` and the creative IN /
pure-perf OUT ruling in `../MERGE_FORK_DIRECTOR_CONSOLE_PLAN.md`, subject to their
new local-suitcase override. On a separately prepared target, take a checkpoint
before each slice; preserve failed experiments before rollback. No branch mutation,
build, commit or push is part of generating this kit.

1. **Baseline and ledger (Step 0).** Resolve unknown ownership and all perf entanglements.
   Checkpoint: XML parses; ledger accounts for every net changed path, including deletions.
2. **Required primitives, common UI and shell (Steps 1–2).** Land helpers, custom widgets,
   history/undo, scroll fit, registrations, kit generator inputs and CMake closure.
   Checkpoint: target compiles/starts with only closed pages registered; resize/scroll/undo work.
3. **Application pages (Step 2).** Land cast/actor bar, actor/path/dance, camera/operator,
   gaze, stepped animation and composition with their controllers and reachable UI.
   Checkpoint: open pages, select/rekey actor, walk/stop, camera release, gaze off and scene round trip.
4. **Clones and crowd (Step 4).** Land spawn/material/attachment isolation and ghost shader.
   Checkpoint: independent clones/crowd survive edits/save/load; user's avatar stays untouched.
5. **Masks and alpha (Step 5).** Land actor masks, Roto/Tron and shared draw-pool adapters.
   Checkpoint: independent enables; feature-off opaque beauty pass remains correct.
6. **Capture/sidecar (Step 6).** Land bridge/ABI/provider, velocity/temporal/motion blur,
   10-bit/color-space dependencies together. Checkpoint: provider provenance and motion validity,
   camera cuts, resize and feature-off; remove governor coupling without losing capture quality.
7. **Rig/tracking/shafts (Steps 7, 9).** Land complete rig UI/model/cues/probes,
   projector tracking, targets, froxel/projvol passes and GLSL. Checkpoint: multiple lights,
   moving subjects, live probes, shadows, resize/login/off path and scene reload.
8. **Weather (Step 8).** Land model/renderer/UI/assets and occlusion convention together.
   Checkpoint: rain under cover/outdoors, splash/lightning, region transition and disable.
9. **Full VCam prism (deferred Step 3).** Adapt/rebuild against target renderer after
   clones/alpha/capture contracts settle. Checkpoint: full matrix in its feature sheet.
10. **Lightbox/looks/sensor and closure (Steps 9–11).** Adapt/rebuild post-processing;
    include exposure, LUT/grade, actor FX, lens presets and save compatibility.
    Checkpoint: chart/exposure comparisons, custom presets, grouped undo and complete looks;
    then all static gates and all feature-sheet in-world tests. Build/in-world acceptance
    on the target requires its own authorized environment; neither was performed here.

## Targets: local Alchemy and LL-derived viewers

For Alchemy at the pinned base, base-owned helpers exist but renderer and app hooks
still need the full fork delta. Prior plans' UPSTREAM-WINS decisions assumed a newer
renderer: **do not discard reverse-Z, UBO, sampler/target or probe changes solely on
that label** at this older local base. Audit which primitive each creative pass needs;
unknown shared renderer hunks stay review candidates rather than perf exclusions.

For an LL-derived client, compare its local implementation to the LL-origin code
at this base. `compatibility.json` distinguishes fork-added dependencies from
`al*`/`fs*`/`bdmerge*` helpers already present at the Alchemy base. A base-owned
Alchemy helper is not assumed present in LL. Port its implementation with its
dependencies, or supply a target-native adapter preserving the observed contract.
Use exact include and qualified-call evidence in `features.json`; it cannot prove
absence in an unseen client. Examples in this tree include the fork kit widgets,
`alscrollfit`, `aluirefresh`, `alpresetlook` and the Director actor-choice helpers;
base-owned dependencies include `indra/newview/alfloaterlightbox.h`,
`indra/newview/alviewermenu.h`, `indra/newview/fsposeranimator.h` and
`indra/llui/fsvirtualtrackpad.h`. For an LL target, port or replace their observed
floater/menu, animation-pose and widget contracts at the recorded consumers;
inspect the dependency table before choosing adapters. Their presence at the Alchemy
base does not establish their presence in another LL-derived client.
Reconcile floater/panel factories, menus/key bindings, build/package manifests,
settings groups, PBR/deferred targets, shadow/depth encodings, shader uniforms and
avatar attachment/lifecycle semantics. Never invoke simulator detach/teleport paths
for local clones. Filename prefixes are not provenance: new `ll*` files can be OWNED.

## Settings and save-data compatibility

`settings.csv` / `.json` enumerate every added/changed/removed declaration in changed
`app_settings/settings*.xml`: type, declared default, Persist metadata, previous default,
feature and candidate scene references. Preserve keys (Alchemy prefixes are identifiers),
types and default semantics. Persist does not prove scene ownership. Scene coverage marks
explicit scene-list members and appended Roto/Tron literal vectors as saved. Other keys
remain unknown: dynamic lists, actor state and structured blocks need semantic review;
a literal reference alone is evidence only. Duplicate declarations remain visible in
settings JSON; the displayed default is the last text occurrence, not a proven runtime policy.

The local scene controller is `indra/newview/llfloaterdirector.cpp`: `SCENE_VERSION`
is 6, `director_scenes` lives under `LL_PATH_USER_SETTINGS`, and scenes use LLSD XML.
Its loader handles legacy versions explicitly (including the unshipped v2 case);
preserve that policy and stable actor IDs. `llactormover.cpp` writes path schema 4
and accepts schemas 1–4; `alpathgeometry.h` supplies path geometry. `alkitpresets.cpp`
uses `director_presets/<family>.xml`; `alcinelightrig.cpp` has version-1 rig/cue blocks
and user directories `cine_light_rig` / `cine_light_rig_cues`. Keep bundled preset assets, escaping rules, recovery
and corrupt-backup policy with the serializer/loader. These are observations of this
local snapshot; when versions change, review this guide's narrative as well as regenerate.

Use each preset family's owned-setting definition. Replace complete looks by defaults
then authored values; preserve targeting, Shot key metadata, calibration, master enables
and compile-time shader settings. Validate before mutation; grouped undo captures the
old values. Old custom presets default missing owned values. User data paths depend on
the target's `gDirUtilp`/channel configuration: copy backups explicitly when migrating
between clients; do not assume two channels share a user-settings directory.

## Shader co-landing and hard spots

Co-land program declaration/registration/loading, all class variants and included GLSL,
reserved uniforms/defines, render-target attachments, feature flags, settings and reachable
UI. A default-off setting is insufficient if startup still calls `createShader()`.
If closure is incomplete, omit registration and source entry until the complete slice
lands, or guard startup creation as well as use. Preserve the target's depth convention,
normal encoding, sampler bindings, color space and target lifecycle explicitly.

Prism/VCam and Lightbox retain the prior plan's REBUILD/adapt warning; unseen targets
need design review, not blind hunk replay. Stage prism after clone/alpha/target support;
stage Lightbox after post/color contracts settle. Rig probes, froxel/projvol, weather,
sidecar and actor masks are renderer-heavy. Check beauty and feature-off paths, GL blend,
color masks, draw buffers and resource release on every experiment. Quality/cost controls
belong to creative features; a name containing “performance” does not make pose polish OUT.
Committed VCam media frame provenance, file admission/close fences and loop-seam helpers
in `indra/llplugin/` are IN dependencies of prism/media playback. The old playbook's
stash-only exclusions do not exclude code that is now committed at this fork HEAD.

Perf severance is mandatory: `llviewertexture.cpp` capture mode calls
`BDMergeMemoryBudget::isCritical()` and capture quality also uses `sQualityOverrideAllowed`.
The prior plan retained the RAM-pressure dependency; this suitcase excludes the pure
budget/pool implementation as a portable feature and marks its callers for adaptation.
Provide the target's equivalent capture-quality/pressure policy or isolate the minimal
required safety contract; never drop the include and leave a missing symbol, and never
silently reintroduce the VRAM governor. This is an explicit IN/OUT difference from the plan.

## Verification and evidence limits

Existing evidence: {meta['evidence']['wiring_rows']} wiring rows; page-source summary
`{json.dumps(meta['evidence']['page_summary'], sort_keys=True)}`.
Functional manifest's recorded wiring CSV SHA matches current bytes:
**{meta['evidence']['functional_csv_hash_matches']}**. Existing manifests can still be stale
against code. `inputs.json` preserves their metadata and hashes; unresolved/ambiguous
rows remain unresolved. They do not establish in-world behavior.

Use the existing generator/check entry points (read `--help` for harness prerequisites):

```powershell
python -I scripts/code_tools/director_kit_gen.py --check
python -I scripts/code_tools/director_page_sources.py --check
python -I scripts/code_tools/director_functional_manifest.py --check
python -I scripts/code_tools/director_parity.py --all
```

`director_wiring_table.py` is a writing generator with its own historical ORIG baseline,
not a `--check` gate; inspect that baseline before rerunning it. The suitcase reads its
existing CSV as evidence. Kit lint, curation/theme/header, actor-bar and light-actors checks complement wiring
and functional metadata. UI layout/draw lint cover only their stated invariants.
Per-feature sheets enumerate actual local scripts, including clone looks, alpha blend,
rig shadow, relight volumetric, sensor and weather GL harnesses. These may require a
GL context, dependencies or compiled fixture hosts; inspect each script before running.
A static/source gate proves its invariant only. GL fixtures prove their test scenes,
not viewer integration. Use every feature sheet's in-world checkpoint and distinguish
PASS / FAIL / INCONCLUSIVE. This kit does not claim a port has been built or run.

The generated hook map covers {meta['hook_files']} shared files / {meta['hunks']} canonical
three-context-line hunks. Function extraction is a conservative lexical scan; XML,
declarations, constructors or complex macros may remain unknown. Local conditional
evidence does not prove a dominating setting guard or an inert off path. Unknown and
mixed/perf-review entries are the manual adjudication queue in `hooks.json` and inventory.
The initial 15+15 deterministic random code spot-checks and exact validation results
are recorded separately in `.tmp/suitcase_20261102/REPORT.md`; that report is not regenerated.

## Licensing and attribution

The repository `LICENSE` is LGPL 2.1. Preserve Linden Lab, Alchemy and other author
notices in each source/shader/asset and carry applicable license texts. Imported FS/BD,
ReShade and vendor files may have their own terms; inspect each notice and redistribute
only what those terms allow. Do not treat the suitcase's class as license clearance or
replace code identifiers during branding. No external license guidance was consulted.
'''.encode("utf-8")


def generate(root):
    inventory, hooks, settings, features, compatibility, fingerprints, meta = collect(root)
    result = {
        "SUITCASE.md": guide(meta, features),
        "inventory.json": json_bytes({"metadata": meta, "paths": inventory}),
        "inventory.csv": csv_bytes(inventory, ["path", "git_status", "class", "features", "layer", "reason", "feature_evidence", "candidate_features_from_subjects", "deleted"]),
        "hooks.json": json_bytes({"base": BASE, "coordinate_source": "HEAD", "files": hooks}),
        "settings.json": json_bytes(settings),
        "settings.csv": csv_bytes(settings, ["name", "change", "type", "default", "previous_default", "file", "features", "persisted", "saved_in_scenes", "scene_evidence", "declaration_count_head", "declaration_count_base", "duplicate_note", "scope", "comment"]),
        "features.json": json_bytes(features), "compatibility.json": json_bytes(compatibility),
        "inputs.json": json_bytes({"metadata": meta, "sha256": fingerprints}),
    }
    lines = ["# Shared hook map", "", f"Local base `{BASE}` only; current upstream may have drifted. HEAD line ranges include three context lines.", "",
             "Grouped by shared file, ordered by earliest candidate landing slice. Full patches and evidence: [hooks.json](hooks.json).",
             "Guards are unknown unless manually proven. Conditional evidence is not a proof that the whole hunk is guarded.", ""]
    for file in hooks:
        lines += [f"## Slice {file['slice']}: `{file['path']}`", "", file["note"], "",
                  "| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |",
                  "|---|---|---|---|---|---|"]
        for h in file["hunks"]:
            lines.append(f"| {h['head_start']}–{h['head_end']} (n={h['head_count']}) | {md(', '.join(h['functions']))} | {md(', '.join(h['features']))} | {h['scope']} | {h['guarded']} | {md(h['purpose'])} |")
        lines.append("")
    result["hooks.md"] = ("\n".join(lines) + "\n").encode()
    for f, info in features.items():
        result[f"features/{f}.md"] = feature_markdown(f, info)
    unknown_paths = [r for r in inventory if r["features"] == ["unknown"] and r["class"] in ("OWNED", "HOOK")]
    unknown_hunks = [h for file in hooks for h in file["hunks"] if h["features"] == ["unknown"]]
    unknown_functions = [h for file in hooks for h in file["hunks"] if all(f.startswith("unknown/") for f in h["functions"])]
    queue = ["# Manual adjudication queue", "", "No unknown is silently accepted as safe to port.", "",
             f"All {meta['hunks']} hunk guard conclusions remain unknown: local conditional evidence is not domination proof. See hooks.md / hooks.json.", "",
             f"## Unknown product-path features ({len(unknown_paths)})", ""]
    queue += [f"- `{r['path']}` ({r['class']})" for r in unknown_paths]
    queue += ["", f"## Unknown hunk features ({len(unknown_hunks)})", ""]
    queue += [f"- `{h['id']}`" for h in unknown_hunks]
    queue += ["", f"## Unknown/non-function contexts ({len(unknown_functions)})", ""]
    queue += [f"- `{h['id']}`: {md(', '.join(h['functions']))}" for h in unknown_functions]
    queue += ["", "Other classified paths with unknown tags remain in inventory.json, including excluded artifacts and generated docs.", ""]
    result["unknowns.md"] = "\n".join(queue).encode()
    return result, meta


def sync_outputs(directory, outputs, check=False):
    existing = {p.relative_to(directory).as_posix() for p in directory.rglob("*") if p.is_file()} if directory.exists() else set()
    stale = sorted(name for name, data in outputs.items() if not (directory / name).is_file() or (directory / name).read_bytes() != data)
    extra = sorted(existing - outputs.keys())
    if check:
        return stale + ["unexpected: " + name for name in extra]
    if extra:
        raise ValueError("unexpected files in generated output directory (not deleted): " + ", ".join(extra))
    for name, data in outputs.items():
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.read_bytes() != data:
            path.write_bytes(data)
    return stale


def selftest():
    temporary_root = ROOT / ".tmp"
    temporary_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="director-suitcase-selftest-", dir=temporary_root) as folder:
        root = Path(folder)
        assert root.resolve().is_relative_to(temporary_root.resolve())
        file = root / "head/llfixture.cpp"
        file.parent.mkdir(parents=True)
        original = root / "base/llfixture.cpp"
        original.parent.mkdir()
        original.write_text('void fixture()\n{\n    original();\n}\n', encoding="utf-8")
        file.write_text('void fixture()\n{\n    if (enabled) ghost();\n    original();\n}\n', encoding="utf-8")
        for directory in ("base", "head"):
            (root / directory / "space name.cpp").write_text('void odd() {' + (' ghost();' if directory == "head" else '') + ' }\n', encoding="utf-8")
        result = subprocess.run(["git", "-c", "diff.noprefix=true", "-c", "diff.mnemonicPrefix=true", "-c", "diff.interHunkContext=50", "diff", "--no-index", *diff_args(), "base", "head"], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert result.returncode == 1, decode(result.stderr)
        raw = decode(result.stdout)
        parsed = split_diff(raw)
        assert "head/space name.cpp" in parsed
        assert patch_path('"b/quoted\\\"name.cpp"', "b/") == 'quoted"name.cpp'
        h = parsed["head/llfixture.cpp"][0]
        detail = hunk_detail(h, "indra/newview/llfixture.cpp", file.read_text(), function_ranges(file.read_text()), [])
        assert detail["functions"] == ["fixture"] and detail["head_start"] == 1
        assert "clones" in detail["features"] and detail["guard_evidence"]
        assert not tags("constraint resolution absolute")
        scene_fixture = {
            "indra/newview/llfloaterdirector.cpp": 'LLFloaterDirector::sceneSettingsList() { static auto settings = {"Weather"}; ALRotoInk::settings(); ALTron::settings(); }',
            "indra/newview/alrotoink.cpp": 'const char* kAllSettings[] = {"Ink"}; ALRotoInk::settings() { std::begin(kAllSettings); }',
            "indra/newview/altron.cpp": 'const char* kAllSettings[] = {"Tron", "UIOnly"}; const char* kDirectorExcluded[] = {"UIOnly"}; ALTron::settings() { std::begin(kAllSettings); kDirectorExcluded; }',
        }
        membership = scene_membership(scene_fixture)
        assert membership["Ink"].startswith("yes:") and membership["Tron"].startswith("yes:")
        assert membership["UIOnly"].startswith("no:")
        planted = [("indra/newview/alghostfixture.cpp", "A", "OWNED"),
                   ("indra/newview/llfixture.cpp", "M", "HOOK"),
                   ("indra/newview/skins/default/xui/en/panel_dpage_fixture.xml", "A", "GENERATED"),
                   ("indra/newview/bdmergememorybudget.cpp", "A", "EXCLUDED-PERF")]
        for path, status, expected in planted:
            assert classify(path, status)[0] == expected
        for name in ("llmediaframestate.h", "llpluginfileadmission.h", "llpluginfileclose.h", "llpluginloopseam.h"):
            path = "indra/llplugin/" + name
            assert classify(path, "A")[0] == "OWNED" and "vcam" in tags(path)
        output = root / "out"
        wanted = {"fixture.txt": b"fresh\n"}
        assert sync_outputs(output, wanted, check=True) == ["fixture.txt"]
        sync_outputs(output, wanted)
        assert sync_outputs(output, wanted, check=True) == []
        (output / "fixture.txt").write_bytes(b"stale\n")
        assert sync_outputs(output, wanted, check=True) == ["fixture.txt"]
        assert (output / "fixture.txt").read_bytes() == b"stale\n"
        saved_root, saved_generate = globals()["ROOT"], globals()["generate"]
        globals()["ROOT"] = root
        globals()["generate"] = lambda unused: (wanted, {"counts": {}, "hook_files": 0, "hunks": 0, "settings": 0})
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                assert main(["--check"]) == 1
                sync_outputs(root / OUT, wanted)
                assert main(["--check"]) == 0
                (root / OUT / "fixture.txt").write_bytes(b"planted stale\n")
                assert main(["--check"]) == 1
                assert (root / OUT / "fixture.txt").read_bytes() == b"planted stale\n"
        finally:
            globals()["ROOT"], globals()["generate"] = saved_root, saved_generate
        print("PASS selftest: OWNED, HOOK/function/range, GENERATED runtime XUI, EXCLUDED-PERF; canonical diff/config/quoted paths; scene vectors/exclusions; missing/fresh/stale --check CLI is read-only")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="return 1 if outputs are stale; do not write")
    mode.add_argument("--selftest", action="store_true", help="run planted miniature-repository fixtures")
    args = parser.parse_args(argv)
    try:
        if args.selftest:
            selftest()
            return 0
        outputs, meta = generate(ROOT)
        stale = sync_outputs(ROOT / OUT, outputs, args.check)
        print(json.dumps({"mode": "check" if args.check else "write", "counts": meta["counts"],
                          "hook_files": meta["hook_files"], "hunks": meta["hunks"], "settings": meta["settings"],
                          "outputs": len(outputs), "stale": stale}, sort_keys=True))
        return 1 if args.check and stale else 0
    except (OSError, ValueError, RuntimeError, ET.ParseError, KeyError) as error:
        print(f"director_suitcase: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
