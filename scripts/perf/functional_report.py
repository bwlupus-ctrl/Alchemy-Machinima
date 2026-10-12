#!/usr/bin/env python3
"""Summarize and compare Director functional self-test CSV files."""

from __future__ import annotations

import argparse
import csv
import json
import itertools
import functools
import re
from collections import Counter, defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET


KEY = ("inworld", "page", "control", "kind", "action", "expected")
FAIL_STATUSES = {"FAIL", "KNOWN_GAP"}

# In-world execution is allow-listed by source owner, never inferred from the
# absence of a hazard. These owners are limited to viewer-local Director
# models/controllers documented in functional/INWORLD_MODE.md.
INWORLD_LOCAL_OWNERS = {
    "ALPanelCineLightRig", "ALRigRimController", "ALPanelRigRim",
    "ALFloaterLightBox", "ALPanelLensLooks", "ALPanelExposureHub",
    "ALPanelWeatherSettings", "ALPanelAtmosphericVolumes",
    "ALPanelLensGaze", "ALFloaterGazeCues", "ALPanelGhostStudio",
    "ALPanelDanceBall", "ALPanelDirectorAnimSwitcher",  # NOT ALPanelAnimPreview: stop/revoke send to the simulator, blacklist writes the account asset_blocklist
    "ALPanelPropMover", "ALPanelActorMover", "ALPanelPathEditor",
    "ALObjectPathMover", "ALPanelDirectorVCam", "ALVCamPageModel", "ALKitPage",
    "ALPanelCineCamParams", "ALPanelDirectorSwitcher",
    "ALPanelFlycamRecorder", "LLFlycamRecorder", "ALPanelTemporalCapture",
}
INWORLD_EXCLUDED_CONTROLS = {"vc_lens_locate", "vc_face_locate",
                             "btn_anim_ex_stop", "btn_anim_ex_revoke", "btn_anim_ex_blacklist"}
INWORLD_VETTED_OWNER_PAGES = {
    "LLPipeline::renderWeather": {"weather_tab"},
    "LLPipeline::renderFinalize": {"exposure_tab", "shadows_probes_tab"},
    "LLFloaterDirector::postBuild": {
        "actors_tab", "actor_style_tab", "animate_tab", "dance_tab",
        "frame_orbit_tab", "gaze_tab", "move_tab", "moves_tab", "shot_tab",
    },
    "LLFloaterDirector::sceneSettingsList": {"switchboard_tab"},
}

# Narrow false-positive exemptions for callbacks that persist viewer-local
# configuration.  The key includes the source owner and page: a same-named
# control elsewhere gets no exemption.  Only the four filesystem tags may be
# removed; simulator/agent/world rules always remain active.
INWORLD_LOCAL_HAZARD_EXEMPTIONS: dict[tuple[str, str, str], tuple[set[str], str]] = {}


def _add_local_exemptions(owner: str, page: str, widgets: set[str], evidence: str) -> None:
    for widget in widgets:
        INWORLD_LOCAL_HAZARD_EXEMPTIONS[(owner, page, widget)] = (
            {"save_disk", "rename", "file_picker", "export_import"}, evidence)


_add_local_exemptions("LLFloaterDirector::postBuild", "move_tab",
                      {"group_run_combo", "group_delay_spinner"},
                      "llfloaterdirector.cpp:318-320,2670-2731")
_add_local_exemptions("ALPanelDanceBall::postBuild", "dance_tab",
                      {"db_radius", "db_mode", "db_adv_interval", "db_t_speed",
                       "db_t_desync", "db_t_crossfade"},
                      "alpaneldanceball.cpp:179-226")
_add_local_exemptions("ALPanelDirectorAnimSwitcher::postBuild", "switchboard_tab",
                      {"anim_slot_kind", "anim_slot_target", "anim_pose_name",
                       "anim_pose_load_method", "anim_slot_priority", "anim_slot_speed",
                       "anim_slot_loop", "anim_slot_snap", "anim_slot_enabled"},
                      "alpaneldirectoranimswitcher.cpp:204-247,277-365")
_add_local_exemptions("ALPanelDirectorSwitcher::postBuild", "shot_tab",
                      {"switcher_slot_mode", "switcher_slot_enabled",
                       "switcher_primary_subject", "switcher_secondary_subject",
                       "switcher_custom_enabled"},
                      "alpaneldirectorswitcher.cpp:314-425")
_add_local_exemptions("ALPanelFlycamRecorder::postBuild", "takes_tab",
                      {"FlycamWeightedWeight"}, "alpanelflycamrecorder.cpp:271-281")
_add_local_exemptions("ALPanelAtmosphericVolumes::postBuild", "atmosphere_tab",
                      {f"localfog_{kind}_{axis}" for kind in ("center", "size", "rotation")
                       for axis in ("x", "y", "z")},
                      "alpanelatmosphericvolumes.cpp:179-216")


def _inworld_local_effect(row: dict[str, object]) -> tuple[bool, str]:
    """Return an explicit viewer-local authorization, not a hazard guess."""
    widget = str(row.get("widget_name") or "")
    if widget in INWORLD_EXCLUDED_CONTROLS:
        return False, "object selection is sent to the simulator"

    owner = str(row.get("owner") or "")
    callback = str(row.get("callbacks") or "")
    if row.get("setting") and not owner and not callback:
        return True, "pure setting write"

    page = str(row.get("page") or "")
    if page in INWORLD_VETTED_OWNER_PAGES.get(owner, set()):
        return True, "vetted viewer-local owner/page: " + owner

    witnesses = {value.split("::", 1)[0] for value in (owner, callback) if value}
    local = sorted(witnesses & INWORLD_LOCAL_OWNERS)
    if local:
        return True, "viewer-local owner: " + ", ".join(local)
    return False, "no vetted viewer-local owner"


def _llsd_pairs(element: ET.Element | None):
    children = list(element) if element is not None else []
    return [(children[i].text or "", children[i + 1])
            for i in range(len(children) - 1) if children[i].tag == "key"]


def _llsd_value(element: ET.Element):
    text = (element.text or "").strip()
    if element.tag == "boolean":
        return text.lower() in {"1", "true"}
    if element.tag == "integer":
        return int(text)
    if element.tag == "real":
        return float(text)
    if element.tag == "array":
        return [_llsd_value(child) for child in element]
    if element.tag == "map":
        return {key: _llsd_value(value) for key, value in _llsd_pairs(element)}
    return text


def load_setting_defaults(repo: Path) -> dict[str, object]:
    result: dict[str, object] = {}
    base = repo / "indra/newview/app_settings"
    for name in ("settings.xml", "settings_alchemy.xml",
                 "settings_per_account.xml", "settings_per_account_alchemy.xml"):
        path = base / name
        if not path.exists():
            continue
        root = ET.parse(path).getroot()
        for setting, body in _llsd_pairs(root.find("map")):
            fields = dict(_llsd_pairs(body if body.tag == "map" else None))
            if setting not in result and "Value" in fields:
                result[setting] = _llsd_value(fields["Value"])
    return result


@functools.lru_cache(maxsize=None)
def _function_body(text: str, qualified: str) -> str:
    """Return a conservative C++ function body for a qualified method."""
    if not qualified or qualified not in text:
        return ""
    match = re.search(r"\b" + re.escape(qualified) + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{", text)
    if not match:
        return ""
    start = text.find("{", match.start())
    depth = 0
    for pos in range(start, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    return ""


def _matching_delimiter(text: str, start: int, opener: str = "(", closer: str = ")") -> int:
    """Return the matching delimiter, ignoring C++ strings and comments."""
    depth = 0
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    pos = start
    while pos < len(text):
        char = text[pos]
        following = text[pos + 1] if pos + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                pos += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
        elif char == "/" and following == "/":
            line_comment = True
            pos += 1
        elif char == "/" and following == "*":
            block_comment = True
            pos += 1
        elif char in {'"', "'"}:
            quote = char
        elif char == opener:
            depth += 1
        elif char == closer:
            depth -= 1
            if depth == 0:
                return pos
        pos += 1
    return -1


def _callback_call(text: str, match: re.Match[str]) -> str:
    """Extract a complete set*Callback/childSet*/registrar.add call."""
    opening = text.rfind("(", match.start(), match.end())
    closing = _matching_delimiter(text, opening) if opening >= 0 else -1
    return text[match.start():closing + 1] if closing >= 0 else ""


def _source_body(sources: dict[Path, str], qualified: str,
                 preferred: Path | None = None) -> tuple[str, Path | None]:
    paths = ([preferred] if preferred in sources else []) + [p for p in sources if p != preferred]
    for path in paths:
        body = _function_body(sources[path], qualified)
        if body:
            return body, path
    return "", None


def _source_bodies_by_leaf(sources: dict[Path, str], leaf: str,
                           preferred: Path | None = None,
                           cache: dict[str, object] | None = None,
                           restrict: frozenset | None = None) -> list[tuple[str, Path]]:
    """Conservatively resolve member calls whose receiver type is not in text.

    Unknown-receiver calls resolve only within the calling file and the known
    owner files (restrict); searching every newview source by leaf name fans
    out through unrelated get()/size()/find() definitions without bound.
    """
    cache_key = "@leaf:" + leaf + "@" + ",".join(sorted(str(p) for p in (restrict or ())))
    if cache is not None and cache_key in cache:
        return cache[cache_key]  # type: ignore[return-value]
    paths = ([preferred] if preferred in sources else []) + [p for p in sources if p != preferred]
    if restrict is not None:
        paths = [p for p in paths if p in restrict]
    found: list[tuple[str, Path]] = []
    definition = re.compile(r"\b((?:[A-Za-z_]\w*::)*" + re.escape(leaf) +
                            r")\s*\([^;{}]*\)\s*(?:const\s*)?\{")
    for path in paths:
        if leaf not in sources[path]:
            continue
        for qualified in definition.findall(sources[path]):
            body = _function_body(sources[path], qualified)
            if body:
                found.append((body, path))
    if cache is not None:
        cache[cache_key] = found
    return found


def _expand_callback_calls(seed: list[tuple[str, Path | None]], owner_class: str,
                           sources: dict[Path, str], body_cache: dict[str, object],
                           depth_limit: int = 3) -> list[str]:
    """Scan callback text and its same/known-owner-file callees to bounded depth."""
    result: list[str] = []
    queue = [(text, path, 0) for text, path in seed]
    seen: set[tuple[str, str]] = set()
    ignored = {
        "if", "for", "while", "switch", "return", "sizeof", "decltype", "catch",
        "getChild", "findChild", "setCommitCallback", "setClickedCallback",
        "childSetAction", "childSetCommitCallback", "bind", "add",
    }
    owner_key = "@owner_files:" + owner_class
    if owner_key not in body_cache:
        marker = owner_class + "::" if owner_class else ""
        body_cache[owner_key] = frozenset(
            p for p, t in sources.items() if marker and marker in t)
    owner_files: frozenset = body_cache[owner_key]  # type: ignore[assignment]
    while queue:
        text, source, depth = queue.pop(0)
        result.append(text)
        restrict = owner_files | frozenset(p for p in (source,) + tuple(
            q for _, q in seed) if p is not None)
        if depth >= depth_limit:
            continue
        calls = set(re.findall(r"(?<![.&>])\b((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(", text))
        calls.update(re.findall(r"&([A-Za-z_]\w*(?:::[A-Za-z_]\w*)+)", text))
        member_calls = set(re.findall(r"(?:->|\.)\s*([A-Za-z_]\w*)\s*\(", text))
        for called in calls:
            leaf = called.rsplit("::", 1)[-1]
            if leaf in ignored:
                continue
            candidates = [called] if "::" in called else ([owner_class + "::" + called] if owner_class else [])
            for candidate in candidates:
                body, body_source = _source_body(sources, candidate, source)
                key = (candidate, str(body_source or ""))
                if body and key not in seen:
                    seen.add(key)
                    queue.append((body, body_source, depth + 1))
                    break
            else:
                for body, body_source in _source_bodies_by_leaf(
                        sources, leaf, source, body_cache, restrict):
                    key = (leaf + ":" + str(hash(body)), str(body_source))
                    if key not in seen:
                        seen.add(key)
                        queue.append((body, body_source, depth + 1))
        for leaf in member_calls - ignored:
            preferred_name = owner_class + "::" + leaf if owner_class else ""
            body, body_source = _source_body(sources, preferred_name, source) if preferred_name else ("", None)
            candidates = [(body, body_source)] if body and body_source else \
                _source_bodies_by_leaf(sources, leaf, source, body_cache, restrict)
            for candidate_body, candidate_source in candidates:
                key = (leaf + ":" + str(hash(candidate_body)), str(candidate_source))
                if key not in seen:
                    seen.add(key)
                    queue.append((candidate_body, candidate_source, depth + 1))
    return result


def _handler_hazards(row: dict[str, object], cpp_sources: dict[Path, str],
                     body_cache: dict[str, object]) -> tuple[set[str], bool]:
    """Resolve this control's exact callback and scan it fail-closed."""
    owner = str(row.get("owner") or "")
    widget = str(row.get("widget_name") or "")
    bodies: list[tuple[str, Path | None]] = []
    resolved = False
    callback = str(row.get("callbacks") or "")
    owner_body, owner_source = _source_body(cpp_sources, owner) if owner else ("", None)
    if owner and owner_body:
        body_cache.setdefault(owner, owner_body)
    owner_class = owner.split("::", 1)[0] if "::" in owner else ""

    registration_text = cpp_sources.get(owner_source, "") if owner_source else owner_body
    if registration_text and (not widget or f'"{widget}"' in registration_text):
        at = registration_text.find(f'"{widget}"')
        direct_start = registration_text.rfind("getChild", 0, at + 1)
        legacy_window = registration_text[
            max(0, direct_start if direct_start >= 0 else at - 160):at + 2000]
        registrations: list[str] = []

        # Direct literal registration: getChild("this-control")->set*Callback(...).
        direct_re = re.compile(
            r"(?:getChild|findChild)(?:<[^>]+>)?\s*\(\s*\"" + re.escape(widget) +
            r"\"\s*\)\s*->\s*set(?:Commit|Clicked)Callback\s*\(")
        registrations.extend(filter(None, (_callback_call(registration_text, match)
                                             for match in direct_re.finditer(registration_text))))

        # Named/member control registration. Array members deliberately match by
        # base name (mCenter[VX] -> mCenter[axis]); the callback is shared by the
        # whole array, so this remains exact for the named control.
        assignment_re = re.compile(
            r"([A-Za-z_]\w*(?:\s*\[[^\]]+\])?)\s*=\s*(?:[^;{}]*?)"
            r"(?:getChild|findChild)(?:<[^>]+>)?\s*\(\s*\"" + re.escape(widget) + r"\"\s*\)")
        for assignment in assignment_re.finditer(registration_text):
            member = assignment.group(1).strip()
            base = re.match(r"[A-Za-z_]\w*", member).group(0)
            member_scope = registration_text
            if not re.match(r"m[A-Z_]", base):
                # Locals such as `control` are routinely reused for adjacent
                # widgets. Restrict them to their enclosing if-block or until
                # the next assignment to that local.
                brace = registration_text.find("{", assignment.end(), assignment.end() + 240)
                semicolon = registration_text.find(";", assignment.end(), assignment.end() + 240)
                if brace >= 0 and (semicolon < 0 or brace < semicolon):
                    end = _matching_delimiter(registration_text, brace, "{", "}")
                    member_scope = registration_text[assignment.end():end + 1] if end >= 0 else ""
                else:
                    next_assignment = re.search(
                        r"\b" + re.escape(base) + r"\s*=\s*[^;{}]*"
                        r"(?:getChild|findChild)", registration_text[assignment.end():])
                    end = (assignment.end() + next_assignment.start()
                           if next_assignment else min(len(registration_text), assignment.end() + 2000))
                    member_scope = registration_text[assignment.end():end]
            member_re = re.compile(
                r"\b" + re.escape(base) + r"(?:\s*\[[^\]]+\])?\s*->\s*"
                r"set(?:Commit|Clicked)Callback\s*\(")
            registrations.extend(filter(None, (_callback_call(member_scope, match)
                                                 for match in member_re.finditer(member_scope))))

        # Legacy helper APIs bind the literal control name in the same call.
        helper_re = re.compile(
            r"\b(?:childSetAction|childSetCommitCallback)\s*\(\s*\"" +
            re.escape(widget) + r"\"\s*,")
        registrations.extend(filter(None, (_callback_call(registration_text, match)
                                             for match in helper_re.finditer(registration_text))))

        if registrations:
            resolved = True
            for registration in dict.fromkeys(registrations):
                bodies.append((registration, owner_source))
                if owner_source:
                    offset = cpp_sources[owner_source].find(registration)
                    if offset >= 0:
                        row["inworld_callback_evidence"] = (
                            f"{owner_source.name}:{cpp_sources[owner_source].count(chr(10), 0, offset) + 1}")
                # A callback may be a previously declared local lambda.
                argument = registration[registration.find("(") + 1:]
                registration_offset = registration_text.find(registration)
                for local in re.findall(r"\b([a-z_]\w*)\b", argument):
                    declarations = list(re.finditer(
                        r"\b" + re.escape(local) + r"\s*=\s*\[",
                        registration_text[:registration_offset] if registration_offset >= 0
                        else registration_text))
                    declaration = declarations[-1] if declarations else None
                    if declaration:
                        brace = registration_text.find("{", declaration.start())
                        end = _matching_delimiter(registration_text, brace, "{", "}")
                        if end >= 0:
                            bodies.append((registration_text[declaration.start():end + 1], owner_source))
        elif row.get("setting") and not callback:
            # A control_name XUI binding is itself the exact registration: the
            # widget commits directly to this viewer setting and has no C++
            # callback to borrow from a neighbouring control.
            resolved = True
        else:
            # Exact resolution failed: retain the old conservative union of
            # every handler in the whole window. Never select only the first.
            handlers = set(re.findall(r"&([A-Za-z_]\w*(?:::[A-Za-z_]\w*)+)", legacy_window))
            if owner_class:
                handlers.update(owner_class + "::" + name for name in
                                re.findall(r"\b(on[A-Z][A-Za-z0-9_]*)\s*\(", legacy_window))
            bodies.append((legacy_window, owner_source))
            for handler in handlers:
                body, source = _source_body(cpp_sources, handler, owner_source)
                if body:
                    bodies.append((body, source))
    if callback:
        body, source = _source_body(cpp_sources, callback, owner_source)
        if body:
            resolved = True
            bodies.append((body, source))
        else:
            # XUI commit_callback.function names resolve through the callback
            # registry's exact string key.
            registry_re = re.compile(r"\badd\s*\(\s*\"" + re.escape(callback) + r"\"\s*,")
            for source_path, source_text in cpp_sources.items():
                for match in registry_re.finditer(source_text):
                    call = _callback_call(source_text, match)
                    if call:
                        resolved = True
                        bodies.append((call, source_path))
    if (not resolved and row.get("setting") and not callback and not owner):
        # A plain control_name binding has no owner handler: LLUICtrl commits
        # directly to the viewer-local setting. This is a resolved local sink,
        # not an unknown callback.
        resolved = True

    combined = "\n".join(_expand_callback_calls(
        bodies, owner_class, cpp_sources, body_cache))
    hazards: set[str] = set()
    rules = {
        "file_picker": r"\b(?:LLFilePicker|LLFilePickerReplyThread|LLFloaterFilePicker|getOpenFile|getSaveFile)\b",
        # Explicit user-data deletes only: the old delete[A-Z]\w* / remove[A-Z]\w* (matched
        # case-insensitively) flagged removeChild/removeObserver etc. and failed good controls.
        "delete": r"\b(?:LLFile::remove|remove_all|DeleteFile|removeItem|removeCategory|purgeObject|purgeDescendents"
                  r"|deleteItem|deleteObject|deletePreset|removePreset|deleteScene|removeScene|deleteTake|removeTake"
                  r"|deleteGhost|removeGhost|deleteNode|removeNode|deleteCue|removeCue|deleteSlot|clearSlot)\b",
        "rename": r"\b(?:LLFile::rename|std::filesystem::rename|rename[A-Z]\w*)\b",
        "export_import": r"\b(?:export\w*|import\w*)\s*\(",
        "save_disk": r"\b(?:llofstream|ofstream|save[A-Z]\w*|writePreset|write[A-Z]\w*File)\b",
        "logout": r"\b(?:requestQuit|forceDisconnect|logout|disconnectFromGrid)\b",
        "server_message": r"\b(?:gMessageSystem|sendReliable(?:Message)?|sendMessage)\b",
        "agent_send": r"\b(?:sendAnimationRequest|(?:gAgent|LLAgent\w*)\b[^;{}]{0,80}?\bsend[A-Z]\w*)\b",
        "object_send": r"\bLLViewerObject\w*\b[^;{}]{0,80}?\bsend[A-Z]\w*\b",
        "selection_send": r"\bLLSelectMgr\w*\b[^;{}]{0,80}?\bsend[A-Z]\w*\b",
        "parcel_send": r"\bLLViewerParcel\w*\b[^;{}]{0,80}?\b(?:send|update|claim|deed|buy|release)[A-Z]\w*\b",
        "chat_im_voice": r"\b(?:sendChat|sendIM|sendInstantMessage|send_chat_from_viewer|send_improved_im|startCall|startVoice|speak|shout)\w*\b",
        "world_action": r"\b(?:rez|duplicate|delete|take|return|give|pay|buy|teleport|sit|stand|attach|detach|wear|takeOff|upload|invite|eject|ban|friend|estate|updateRegion|setRegion|updateParcel|setParcel|claimParcel|releaseParcel)\w*\s*\(",
        "scripted_dialog": r"\b(?:ScriptDialog|ScriptQuestion|LLScriptFloater|scriptQuestion)\b",
    }
    for tag, pattern in rules.items():
        if re.search(pattern, combined, re.I):
            hazards.add(tag)
    # A virtual avatar call can dispatch gAgent.sendAnimationRequest when the
    # self avatar's AO overrides the motion.  Qualified LLCharacter calls are
    # deliberately not matched.
    if re.search(r"->\s*(?:startMotion|stopMotion)\s*\(", combined):
        hazards.add("agent_send")
        row["motion_send_hazard"] = True
    exemption = INWORLD_LOCAL_HAZARD_EXEMPTIONS.get((owner, str(row.get("page") or ""), widget))
    if exemption:
        allowed_tags, evidence = exemption
        hazards.difference_update(allowed_tags)
        row["inworld_hazard_exemption_evidence"] = evidence
    return hazards, resolved


def _apply_inworld_hazard_policy(row: dict[str, object], hazards: set[str]) -> None:
    if hazards:
        row["inworld_allowed"] = False
        row["inworld_allow_evidence"] = (
            "resolved handler hazard: " + ", ".join(sorted(hazards)))


def _owner_enable_index(cpp_sources: dict[Path, str]) -> dict[str, tuple[str, str]]:
    """Resolve getChild member -> setEnabled expression without proximity windows."""
    result: dict[str, tuple[str, str]] = {}
    assignment = re.compile(
        r"(?:this->)?([A-Za-z_]\w*)\s*=\s*(?:getChild|findChild)(?:<[^>]+>)?\s*\(\s*\"([^\"]+)\"")
    for path, text in cpp_sources.items():
        enables: dict[str, str] = {}
        for member, expression in re.findall(
                r"setEnabledIfChanged\s*\(\s*([A-Za-z_]\w*)\s*,\s*([^;\n]+)", text):
            enables.setdefault(member, expression.rstrip(") "))
        for member, expression in re.findall(
                r"([A-Za-z_]\w*)\s*->\s*setEnabled\s*\(\s*([^;\n]+)", text):
            enables.setdefault(member, expression.rstrip(") "))
        for match in assignment.finditer(text):
            if match.group(1) in enables:
                result.setdefault(match.group(2), (enables[match.group(1)], path.name))
    return result


def _merge_required_consumers(document: dict[str, object], repo: Path) -> int:
    oracle_path = repo / "doc/director_kit/functional/consumers.json"
    oracle = json.loads(oracle_path.read_text(encoding="utf-8"))
    if len(oracle.get("required", [])) != 47:
        raise SystemExit("consumer oracle must contain exactly 47 required rows")
    rows = document["entries"]
    by_key = {(r.get("version"), r.get("page"), r.get("widget_name")): r for r in rows}
    missing: list[str] = []
    for required in oracle.get("required", []):
        key = (required["version"], required["page"], required["widget_name"])
        row = by_key.get(key)
        if row is None:
            missing.append(":".join(key))
            continue
        row["consumer"] = required["consumer"]
    merged_missing = [":".join((r["version"], r["page"], r["widget_name"]))
                      for r in oracle.get("required", [])
                      if by_key.get((r["version"], r["page"], r["widget_name"]), {}).get("consumer") != r["consumer"]]
    if missing or merged_missing:
        raise SystemExit("required consumer merge failed: " + ", ".join(sorted(set(missing + merged_missing))))
    return len(oracle.get("required", []))


def _enforce_inworld_run1_baseline(document: dict[str, object], repo: Path) -> list[str]:
    """Deny newly executable rows unless the locality table authorizes them."""
    baseline_path = repo / "doc/director_kit/run_functional_20261012_093309/manifest_merged.json"
    evidence_path = repo / "doc/director_kit/functional/INWORLD_NEW_ROWS.md"
    if not baseline_path.exists():
        document.setdefault("meta", {})["inworld_run1_diff"] = "baseline unavailable"
        denied = []
        for row in document.get("entries", []):
            if row.get("inworld_allowed") is True and row.get("action") in {"RUN", "DEFER_INWORLD"}:
                row["inworld_allowed"] = False
                row["inworld_allow_evidence"] = "run-1 baseline unavailable; fail-closed denial"
                denied.append(":".join((str(row.get("version") or ""),
                                        str(row.get("page") or ""),
                                        str(row.get("widget_name") or ""))))
        document["meta"]["inworld_run1_new_rows_denied"] = denied
        return denied

    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    old = {(row.get("version"), row.get("page"), row.get("widget_name")): row
           for row in baseline.get("entries", [])}
    approved: dict[tuple[str, str, str], str] = {}
    if evidence_path.exists():
        for line in evidence_path.read_text(encoding="utf-8").splitlines():
            cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
            if len(cells) == 4 and cells[0] in {"v1", "v2"} and \
                    re.fullmatch(r"[^:|]+:\d+(?:-\d+)?", cells[3]):
                approved[(cells[0], cells[1], cells[2])] = cells[3]

    executable = {"RUN", "DEFER_INWORLD"}
    denied: list[str] = []
    for row in document.get("entries", []):
        key = (str(row.get("version") or ""), str(row.get("page") or ""),
               str(row.get("widget_name") or ""))
        before = old.get(key)
        now_exec = row.get("inworld_allowed") is True and row.get("action") in executable
        was_exec = bool(before and before.get("inworld_allowed") is True and
                        before.get("action") in executable)
        if not now_exec or was_exec:
            continue
        evidence = approved.get(key)
        if evidence:
            row["inworld_new_row_evidence"] = evidence
            row["inworld_allow_evidence"] = "run-1 locality: " + evidence
            continue
        row["inworld_allowed"] = False
        row["inworld_allow_evidence"] = "new since run 1; no documented file:line locality evidence"
        denied.append(":".join(key))
    document.setdefault("meta", {})["inworld_run1_new_rows_denied"] = denied
    return denied


def prepare_manifest(path: Path, repo: Path, output: Path) -> tuple[int, int, int, int]:
    """Enrich the generated plan from XUI and owner refresh patterns.

    This deliberately runs after director_functional_manifest.py: that script
    remains the coverage oracle, while this pass records every enable-chain
    prerequisite and the bounded R4 action probes.
    """
    document = json.loads(path.read_text(encoding="utf-8"))
    rows = document["entries"]
    required_consumers = _merge_required_consumers(document, repo)
    defaults = load_setting_defaults(repo)
    xui_cache: dict[Path, tuple[ET.Element, dict[ET.Element, ET.Element]]] = {}
    cpp_sources = {p: p.read_text(encoding="utf-8", errors="replace")
                   for p in (repo / "indra/newview").glob("*.cpp")}
    cpp_text = "\n".join(cpp_sources.values())
    owner_enable = _owner_enable_index(cpp_sources)
    cinecam_text = cpp_sources.get(repo / "indra/newview/alpanelcinecamparams.cpp", "")
    cinecam_modes = {panel: int(mode) for mode, panel in re.findall(
        r'\{\s*(\d+)\s*,\s*"(panel_mode_[^"]+)"', cinecam_text)}

    owner_rules = (
        (re.compile(r"^(Tron|tr_)", re.I), "TronEnabled"),
        (re.compile(r"^(CineOutline|ri_)", re.I), "CineOutlineEnabled"),
        (re.compile(r"^(CineHaze|ch_)", re.I), "CineHazeEnable"),
        (re.compile(r"^(AlchemyWeather|weather_)", re.I), "AlchemyWeatherEnabled"),
        (re.compile(r"^(CineRigRim|cine_rig_rim_)", re.I), "CineLightRigEnabled"),
        (re.compile(r"^(CineLightRig|cine_)", re.I), "CineLightRigEnabled"),
    )
    unresolved: list[str] = []
    unresolved_hazards: list[str] = []
    hazard_rows = 0
    body_cache: dict[str, object] = {}

    gaze_targets = {
        "target": ("gaze_target_combo", 0),
        "eye_target": ("gaze_eye_target_combo", -1),
        "intensity": ("gaze_intensity_slider", 1.0),
        "blend": ("gaze_blend_slider", 0.7), "torso": ("gaze_torso_slider", 0.25),
        "smoothing": ("gaze_smoothing_slider", 0.5),
        "eyeline_yaw": ("gaze_eyeline_yaw_slider", 0.0),
        "eyeline_pitch": ("gaze_eyeline_pitch_slider", 0.0),
        "camera_roll": ("gaze_camera_roll_slider", defaults.get("DirectorGazeCameraRoll", 0.0)),
        "ease_acquire": ("gaze_ease_acquire_spinner", defaults.get("DirectorGazeEaseAcquireSec", 0.2)),
        "ease_release": ("gaze_ease_release_spinner", defaults.get("DirectorGazeEaseReleaseSec", 0.35)),
        "deadzone": ("gaze_deadzone_spinner", defaults.get("BDMergeGazeDeadZone", 2.0)),
        "priority": ("gaze_priority_combo", defaults.get("DirectorGazePriority", 2)),
        "ik_lean": ("gaze_ik_lean_combo", -1),
        "anim_priority": ("gaze_anim_priority_combo", defaults.get("DirectorGazeAnimationPriority", 3)),
        "composition": ("gaze_composition_combo", defaults.get("DirectorGazeComposition", 0)),
        "composition_mix": ("gaze_composition_mix_slider", defaults.get("DirectorGazeCompositionMix", 0.5)),
        "chest_share": ("gaze_chest_share_slider", defaults.get("DirectorGazeChestShare", 0.5)),
        "range_preset": ("gaze_range_preset_combo", -1),
        "limit_mode": ("gaze_limit_mode_combo", defaults.get("DirectorGazeLimitMode", 0)),
        "exaggerate": ("gaze_exaggerate_slider", defaults.get("DirectorGazeExaggerate", 1.0)),
        "lean_curve": ("gaze_lean_curve_combo", defaults.get("DirectorGazeLeanCurve", 0)),
        "lean_threshold": ("gaze_lean_threshold_spinner", defaults.get("DirectorGazeLeanThresholdDeg", 15.0)),
        "lean_softness": ("gaze_lean_softness_spinner", defaults.get("DirectorGazeLeanSoftnessDeg", 10.0)),
        "lean_max": ("gaze_lean_max_spinner", defaults.get("DirectorGazeLeanMaxDeg", 12.0)),
        "limit_profile": ("gaze_limit_eye_yaw_spinner", 35.0),
        "persona_dom": ("gaze_persona_dom_slider", 0.0),
        "persona_aff": ("gaze_persona_aff_slider", 0.0),
        "persona_anx": ("gaze_persona_anx_slider", 0.0),
        "microlife": ("gaze_microlife_slider", defaults.get("DirectorGazeMicroLife", 0.5)),
        "variation": ("gaze_variation_slider", defaults.get("DirectorGazeVariation", 0.2)),
        "break_frequency": ("gaze_break_freq_slider", defaults.get("DirectorGazeBreakFrequency", 0.1)),
    }
    gaze_actor_override_resets = {
        "camera_roll", "ease_acquire", "ease_release", "deadzone", "priority",
        "anim_priority", "composition", "composition_mix", "chest_share",
        "limit_mode", "limit_profile", "exaggerate", "lean_curve", "lean_threshold",
        "lean_softness", "lean_max", "microlife", "variation",
        "break_frequency",
    }
    gaze_reset_settings = {
        "camera_roll": "DirectorGazeCameraRoll",
        "ease_acquire": "DirectorGazeEaseAcquireSec",
        "ease_release": "DirectorGazeEaseReleaseSec",
        "deadzone": "BDMergeGazeDeadZone",
        "priority": "DirectorGazePriority",
        "anim_priority": "DirectorGazeAnimationPriority",
        "composition": "DirectorGazeComposition",
        "composition_mix": "DirectorGazeCompositionMix",
        "chest_share": "DirectorGazeChestShare",
        "limit_mode": "DirectorGazeLimitMode",
        "exaggerate": "DirectorGazeExaggerate",
        "lean_curve": "DirectorGazeLeanCurve",
        "lean_threshold": "DirectorGazeLeanThresholdDeg",
        "lean_softness": "DirectorGazeLeanSoftnessDeg",
        "lean_max": "DirectorGazeLeanMaxDeg",
        "microlife": "DirectorGazeMicroLife",
        "variation": "DirectorGazeVariation",
        "break_frequency": "DirectorGazeBreakFrequency",
    }
    # These controls write per-actor/crowd/dance model state. At login their
    # widgets can commit, but there is deliberately no entity model to provide
    # the independent oracle required for RUN.
    entity_state_controls = {
        "show_all_check", "clone_type_combo", "source_combo", "array_count_spinner",
        "array_spacing_spinner", "formation_edge_gap_spinner",
        "formation_jitter_spinner", "formation_seed_spinner",
        "formation_terrain_check", "facing_yaw_offset_spinner",
        "formation_guide_shape_combo", "crowd_create_mode_combo",
        "strip_count_spinner", "strip_interval_spinner",
        "formation_motion_speed", "formation_motion_amplitude",
        "pose_rate_spinner", "db_header_desync", "db_focus", "db_autojoin",
        "db_include_seated", "db_include_me", "db_adv_jitter",
        "db_t_speed_jitter", "db_t_priority", "db_t_dance_drives_face",
        "db_block_yaw", "db_block_step", "db_block_face",
        "db_block_keep_facing", "db_block_mode", "db_block_shape",
        "db_block_spacing", "db_block_centre", "db_wave_focus",
        "db_wave_speed", "db_wave_dir", "db_wave_apply", "db_ramp_target",
        "db_ramp_seconds", "db_on",
        "look_at_camera_torso", "look_at_camera_torso_amount",
    }
    inworld_unprovisioned = {
        "btn_drive_all": "in-world design: Drive all requires an enrolled prop path; the harness creates none",
        "anim_list": "in-world design: animation selection requires a populated inventory animation list",
        "cue_list": "in-world design: cue selection requires a saved cue; the harness creates none",
        "btn_cue_delete": "in-world design: Delete is disabled until a saved cue is selected",
        "actor_style_target": "in-world design: actor-style target requires a populated cast subject beyond the isolated self fixture",
        "vc_monitor_strip": "in-world design: the monitor strip commits only named tile menu/drag hit zones, not its decorative centre",
        "vc_monitor_tile_manifest": "in-world design: manifest-only monitor tile documentation has no standalone LLUICtrl",
        "vc_source_type": "in-world design: Virtual at my view is already Virtual; the source selector has no safe alternate fixture",
    }
    for name in {
            "facing_yaw_offset_spinner", "strip_count_spinner", "strip_interval_spinner",
            "formation_motion_speed", "formation_motion_amplitude", "pose_rate_spinner",
            "btn_group_unit", "btn_ungroup_unit", "btn_rig_diag_refresh"}:
        inworld_unprovisioned[name] = \
            "in-world design: ghost/crowd edits require a selected generated unit; the harness creates none"
    for name in {
            "db_prev", "db_next", "db_blocking_list", "db_block_toggle_rows",
            "db_block_yaw_minus", "db_block_yaw", "db_block_yaw_plus", "db_block_face",
            "db_block_pin", "db_block_release", "db_block_mode", "db_block_set_centre",
            "db_block_apply_selected", "db_block_apply_all", "db_block_reset_selected"}:
        inworld_unprovisioned[name] = \
            "in-world design: dance transport/blocking requires a populated playlist or selected blocking row"
    launchers = {
        "btn_actor_gaze": "actor_gaze", "btn_pose_polish": "pose_polish",
        "btn_gaze_cues": "gaze_cues", "polish_popout": "pose_polish",
        "dance_popout": "dance_ball",
        "light_response_popout": "avatar_light_response",
        "btn_open_animexp": "animation_explorer",
    }
    reset_all = {
        "weather_reset_all": ("AlchemyWeather", "AlchemyWeatherEnabled", "WeatherConfirmResetAll"),
        "ri_reset_all": ("CineOutline", "CineOutlineEnabled", ""),
        "tr_reset_all": ("Tron", "TronEnabled", ""),
        "ch_reset_all": ("CineHaze", "CineHazeEnable", ""),
    }

    for row in rows:
        prereqs: list[dict[str, object]] = []
        seen: set[tuple[str, object]] = set()
        xui_name = row.get("xui_file", "")
        xui_path = repo / xui_name if xui_name else None
        element = None
        ancestor_names: list[str] = []
        if xui_path and xui_path.exists():
            if xui_path not in xui_cache:
                root = ET.parse(xui_path).getroot()
                xui_cache[xui_path] = (root, {child: parent for parent in root.iter() for child in parent})
            root, parents = xui_cache[xui_path]
            element = next((e for e in root.iter() if e.get("name") == row.get("widget_name")), None)
            current = element
            while current is not None:
                if current.get("name"):
                    ancestor_names.append(current.get("name", ""))
                for attr, value in (("enabled_control", True), ("disabled_control", False),
                                    ("enable_setting", True), ("master_setting", True)):
                    setting = current.get(attr)
                    if setting and setting != row.get("setting") and (setting, value) not in seen:
                        prereqs.append({"setting": setting, "value": value,
                                        "source": f"xui:{xui_path.name}:{attr}"})
                        seen.add((setting, value))
                current = parents.get(current)

        reveal: list[dict[str, object]] = []
        mode_panel = next((name for name in ancestor_names if name in cinecam_modes), "")
        if mode_panel:
            reveal.append({"selector": "CinematicCamMode",
                           "value": cinecam_modes[mode_panel]})
        weather_groups = {
            "weather_rain_group": "rain", "weather_surface_group": "surface",
            "weather_occlusion_group": "occlusion",
            "weather_lightning_group": "lightning",
        }
        weather_group = next((name for name in ancestor_names if name in weather_groups), "")
        if weather_group:
            reveal.append({"selector": "weather_control_group",
                           "value": weather_groups[weather_group]})
        row["reveal"] = reveal

        widget = row.get("widget_name", "")
        if row.get("page") == "path_tab":
            row["prereq"] = "path_actor_edit_2_waypoints"
        elif widget == "animation_library_combo":
            row["prereq"] = "ghost_selected_entity"
        elif widget == "array_parameter_spinner":
            row["prereq"] = "ghost_selected_formation"
        reveal_settings: list[dict[str, object]] = []
        reveal_driver: tuple[str, object] | None = None
        if widget.startswith("tm_"):
            reveal_driver = ("AlchemyRenderTonemapType", 0)
        elif widget.startswith("lpm_"):
            reveal_driver = ("AlchemyRenderTonemapType", 7)
        elif widget == "ri_match_reach_slider":
            reveal_driver = ("CineOutlineInkColorMode", 1)
        elif widget == "ri_metric_thr_slider":
            reveal_driver = ("CineOutlineDepthMode", 1)
        elif widget in {"ri_color2_swatch", "ri_color2_speed_slider", "ri_color2_length_slider"}:
            reveal_driver = ("CineOutlineColor2Mode", 1)
        elif widget in {"ri_subject_invert_check", "ri_subject_targetset_combo",
                        "ri_subject_max_spinner", "ri_subject_shape_combo",
                        "ri_subject_target_color_check"}:
            reveal_driver = ("CineOutlineSubjectMode", 3)
        elif widget == "ri_subject_source_combo":
            reveal_driver = ("CineOutlineSubjectMode", 1)
        elif widget in {"ri_subject_near_slider", "ri_subject_far_slider"}:
            reveal_driver = ("CineOutlineSubjectMode", 4)
        elif widget.startswith("ri_subject_screen_"):
            reveal_driver = ("CineOutlineSubjectMode", 7)
        elif widget in {"ri_pattern_size_slider", "ri_pattern_ratio_slider",
                        "ri_pattern_drift_slider"}:
            reveal_driver = ("CineOutlinePatternType", 1)
        elif widget in {"ri_hatch_angle_slider", "ri_hatch_reach_slider", "ri_hatch_cross_check"}:
            reveal_driver = ("CineOutlinePatternType", 4)
        elif widget == "ri_motion_shape_slider":
            reveal_driver = ("CineOutlineMotionStyle", 6)
        elif widget == "ri_motion_angle_slider":
            reveal_driver = ("CineOutlineMotionStyle", 3)
        elif widget == "ri_motion2_shape_slider":
            reveal_driver = ("CineOutlineMotion2Style", 6)
        elif widget == "ri_motion2_angle_slider":
            reveal_driver = ("CineOutlineMotion2Style", 3)
        elif widget == "ri_motion_phase_slider":
            reveal_driver = ("CineOutlineMotionTempo", 1.0)
        elif widget.startswith("tr_trace_"):
            reveal_settings.append({"setting": "TronTraceEnabled", "value": True,
                                    "source": "ALFloaterLightBox::updateTronVisibility"})
            if widget in {"tr_trace_pulse_speed_slider", "tr_trace_pulse_length_slider",
                          "tr_trace_seed_slider"}:
                reveal_driver = ("TronTracePulseAmount", 1.0)
        elif widget.startswith("tr_rim_scan_"):
            reveal_settings.append({"setting": "TronRimEnabled", "value": True,
                                    "source": "ALFloaterLightBox::updateTronVisibility"})
            reveal_driver = ("TronRimScanAmount", 1.0)
        elif widget == "tr_trail_width_slider":
            reveal_settings.append({"setting": "TronTrailEnabled", "value": True,
                                    "source": "ALFloaterLightBox::updateTronVisibility"})
            reveal_driver = ("TronTrailStyle", 1)
        elif widget == "tr_subject_target_combo":
            reveal_settings.append({"setting": "TronRimEnabled", "value": True,
                                    "source": "ALFloaterLightBox::updateTronVisibility subject group"})
            reveal_driver = ("TronSubjectTargetSet", 0)
        elif widget in {"FlycamWeightedPreset", "FlycamWeightedWeight"}:
            reveal_driver = ("FlycamFlightModel", 2)
        elif widget == "diopter_blend":
            reveal_driver = ("CineDiopterToolMode", 0)
        elif widget == "diopter_power":
            reveal_settings.append({"setting": "CineDiopterToolMode", "value": 0,
                                    "source": "ALDiopterRelevance tool gate"})
            reveal_driver = ("CineDiopterLensFocusMode", 0)
        if reveal_driver:
            reveal_settings.append({"setting": reveal_driver[0], "value": reveal_driver[1],
                                    "source": "functional visibility prerequisite"})
        row["reveal_settings"] = reveal_settings
        if widget in {"anim_pose_name", "anim_pose_load_method"}:
            row["reveal"].append({"selector": "anim_slot_kind", "value": 1})
        elif widget == "gaze_cast_combo":
            row["reveal"].append({"selector": "gaze_target_combo", "value": 2})
        elif widget == "gaze_composition_mix_slider":
            row["reveal"].append({"selector": "gaze_composition_combo", "value": 2})
        elif widget == "array_parameter_spinner":
            row["reveal"].append({"selector": "array_formation_combo", "value": 2})
        elif widget == "animation_library_combo":
            row["reveal"].append({"selector": "drive_mode_combo", "value": 1})
        if widget in {"anim_switcher_armed", "anim_switcher_auto",
                      "btn_anim_play", "btn_paste_play"}:
            row.update(
                action="SKIP", known_gap=False,
                reason=("in-world: may send animation request via AO override "
                        "(llvoavatar.cpp:7398-7413,7442-7457; "
                        "aldirectoranimswitcher.cpp:414,418,565; "
                        "llfloaterdirector.cpp:3726,3741)"))
            row["hazards"] = sorted(set(row.get("hazards", [])) | {"agent_send"})
        if widget in inworld_unprovisioned:
            row["inworld_known_gap_reason"] = inworld_unprovisioned[widget]
        if widget == "groups_list":
            row["inworld_known_gap_reason"] = \
                "in-world fixture gap: list has no safe viewer-local item to select"
        if row.get("page") == "props_tab" and widget == "btn_stop":
            row["inworld_known_gap_reason"] = \
                "in-world design: prop Stop requires an enrolled prop path; the harness creates none"
        if widget == "cine_fx_stop":
            row.update(probe="cine_fx_stop", action="DEFER_INWORLD", known_gap=False)
        elif widget == "cine_live_probe_refresh_now":
            row.update(probe="cine_live_probe_refresh", action="DEFER_INWORLD", known_gap=False)
            prereqs.append({"setting": "CineLightRigLiveProbeRefresh", "value": 4,
                            "source": "LLReflectionMapManager::requestCinematicLiveProbeRefresh"})
        elif widget == "vc_snap_view":
            row.update(probe="vc_snap_view", action="DEFER_INWORLD", known_gap=False)
        elif widget == "vc_face_list":
            row.update(probe="vc_face_list", action="DEFER_INWORLD", known_gap=False)
        elif widget == "vc_camera_selector":
            row.update(probe="vc_camera_selector", action="DEFER_INWORLD", known_gap=False,
                       exercise="select_item",
                       test_value={"item_index": 1, "resolve": "runtime"})
        elif row.get("page") == "move_tab" and widget in {"btn_walk", "btn_stop"}:
            row.update(probe="actor_mover_active", action="DEFER_INWORLD", known_gap=False,
                       actor_mover_expect_active=widget == "btn_walk")
        elif widget == "gaze_breakoff_check":
            # This callback writes a real global setting; use that independent
            # setting oracle instead of treating the absent per-actor field as
            # an in-world fixture gap.
            row.update(action="RUN", known_gap=False,
                       setting="BDMergeGazeBehindPolicy", setting_type="S32")
            row["expect"] = {"setting": "BDMergeGazeBehindPolicy", "type": "S32",
                             "value": 1}
        elif widget == "actor_style_reset":
            row.update(probe="actor_style_reset", action="DEFER_INWORLD", known_gap=False)
        elif widget == "ir_enable_tag":
            row.update(action="DEFER_INWORLD", known_gap=False,
                       confirm_notification="EnableGBufferAvatarTagConfirm",
                       oracle_pre_value=False, setting="RenderGBufferAvatarTag")
            row["expect"] = {"setting": "RenderGBufferAvatarTag", "type": "Boolean",
                             "value": True}
        if widget in entity_state_controls and row.get("action") == "RUN":
            row.update(action="DEFER_INWORLD", known_gap=False,
                       reason=f"{widget}: requires an active actor/crowd/dance model oracle",
                       enable_when="login has an active entity model",
                       hazards=sorted(set(row.get("hazards", [])) | {"requires_world"}))
        key = row.get("setting") or widget
        if widget.startswith("switcher_ease_bezier_"):
            prereqs.append({"setting": "DirectorSwitcherEaseCurve", "value": 100,
                            "source": "ALPanelDirectorSwitcher::updateCurveControlsEnabled"})
            seen.add(("DirectorSwitcherEaseCurve", 100))
        elif widget.startswith("switcher_freeze_bezier_"):
            prereqs.append({"setting": "DirectorSwitcherFreezeCurve", "value": 100,
                            "source": "ALPanelDirectorSwitcher::updateCurveControlsEnabled"})
            seen.add(("DirectorSwitcherFreezeCurve", 100))
        localfog_fields = {
            "localfog_center_x": "center_x", "localfog_center_y": "center_y",
            "localfog_center_z": "center_z", "localfog_size_x": "size_x",
            "localfog_size_y": "size_y", "localfog_size_z": "size_z",
            "localfog_rotation_x": "rotation_x", "localfog_rotation_y": "rotation_y",
            "localfog_rotation_z": "rotation_z",
        }
        if widget in localfog_fields:
            row.update(consumer="localfog_selected_" + localfog_fields[widget],
                       known_gap=False, snapshot_all=True)
        anim_fields = {
            "anim_slot_kind": "kind", "anim_slot_target": "target",
            "anim_pose_name": "pose", "anim_pose_load_method": "pose_load",
            "anim_slot_priority": "priority", "anim_slot_speed": "speed",
            "anim_slot_loop": "loop", "anim_slot_snap": "snap",
            "anim_slot_enabled": "enabled",
        }
        if widget in anim_fields:
            row.update(consumer="anim_switcher_slot_" + anim_fields[widget],
                       known_gap=False, snapshot_all=True)
        if widget.startswith("auto_frame_settle_bezier_"):
            for gate, value in (("CinematicAutoFrameEnabled", True),
                                ("CinematicAutoFrameSettleCurve", 100)):
                if (gate, value) not in seen:
                    prereqs.append({"setting": gate, "value": value,
                                    "source": "LLCinematicCamera::updateAutoFrameCurveControls"})
                    seen.add((gate, value))
        if widget == "exp_amd_exposure" and ("AlchemyRenderTonemapType", 7) not in seen:
            prereqs.append({"setting": "AlchemyRenderTonemapType", "value": 7,
                            "source": "ALPanelExposureHub::refreshToneMapAvailability"})
            seen.add(("AlchemyRenderTonemapType", 7))
        if (widget.startswith("switcher_custom_") and widget != "switcher_custom_enabled") or \
                widget.startswith("reset_switcher_custom_"):
            row["enable_widget"] = "switcher_custom_enabled"
        for pattern, master in owner_rules:
            if pattern.search(key) and master != row.get("setting") and (master, True) not in seen:
                prereqs.append({"setting": master, "value": True, "source": "owner refresh family"})
                seen.add((master, True))
        light_gate = re.match(r"^CineLightRig(Key|Fill|Rim|Bg|Catchlight|Bounce)(?!On$|Enabled$)",
                              row.get("setting") or "")
        if light_gate:
            suffix = light_gate.group(1)
            master = (f"CineLightRig{suffix}On" if suffix in {"Key", "Fill", "Rim", "Bg"}
                      else f"CineLightRig{suffix}" if suffix == "Catchlight"
                      else "CineLightRigBounceEnabled")
            if master != row.get("setting") and (master, True) not in seen:
                prereqs.append({"setting": master, "value": True,
                                "source": "ALPanelCineLightRig per-light refresh gate"})
                seen.add((master, True))
        enable_expr, enable_source = owner_enable.get(widget, ("", ""))
        if enable_expr:
            for negated, gate in re.findall(r'(!?)\s*(?:gSavedSettings\.)?getBOOL\(\"([A-Za-z0-9_]+)\"\)', enable_expr):
                value = not bool(negated)
                if gate != row.get("setting") and (gate, value) not in seen:
                    prereqs.append({"setting": gate, "value": value,
                                    "source": f"{enable_source}: owner setEnabled expression"})
                    seen.add((gate, value))
            source_text = cpp_sources.get(repo / "indra/newview" / enable_source, "")
            for variable in re.findall(r'\b([A-Za-z_]\w*)\b', enable_expr):
                cached = re.search(r'LLCachedControl\s*<\s*bool\s*>\s+' + re.escape(variable) +
                                   r'\s*\([^;]*\"([A-Za-z0-9_]+)\"', source_text)
                if cached:
                    value = not bool(re.search(r'!\s*' + re.escape(variable) + r'\b', enable_expr))
                    gate = cached.group(1)
                    if gate != row.get("setting") and (gate, value) not in seen:
                        prereqs.append({"setting": gate, "value": value,
                                        "source": f"{enable_source}: LLCachedControl in owner gate"})
                        seen.add((gate, value))
            world = re.search(r'\b(isAgentAvatarValid|gAgentAvatarp|getSelection|mActor|path|entity|have|inst|selected|crowd_active)\b', enable_expr)
            if world and not prereqs:
                row["enable_when"] = f"{enable_source}: {enable_expr.strip()}"
                row["action"] = "DEFER_INWORLD"
                row["reason"] = f"{widget}: requires world prerequisite ({world.group(1)})"
                row["hazards"] = sorted(set(row.get("hazards", [])) | {"requires_world"})
        if widget == "weather_trigger" and ("AlchemyWeatherLightningEnabled", True) not in seen:
            prereqs.append({"setting": "AlchemyWeatherLightningEnabled", "value": True,
                            "source": "ALPanelWeatherSettings::updateDeferredAvailability"})
            seen.add(("AlchemyWeatherLightningEnabled", True))

        # Code-bound light-rig rows have live prerequisites/oracles that XUI
        # control_name scanning cannot infer.
        group_slots = {
            "cine_group_self": 0, "cine_group_a": 1, "cine_group_b": 2,
            "cine_group_c": 3, "cine_group_d": 4,
        }
        if row.get("page") == "cine_light_rig_tab" and widget in group_slots:
            row["consumer"] = "rig_group_slots"
            row["rig_group_index"] = group_slots[widget]
        if row.get("page") == "cine_light_rig_tab" and widget in {
                "cine_fixture_kelvin", "cine_fixture_source_size",
                "cine_fixture_gel_0", "cine_fixture_gel_1", "cine_fixture_gel_2"}:
            row["enable_widget"] = "cine_fixture_mode"

        # Easy controls are callback-owned and deliberately inert outside Easy
        # mode; advanced per-light controls are disabled while Easy is active.
        if row.get("page") == "cine_light_rig_tab":
            if widget.startswith("cine_easy_") and widget != "cine_easy_mode" and \
                    ("CineLightRigEasyMode", True) not in seen:
                prereqs.append({"setting": "CineLightRigEasyMode", "value": True,
                                "source": "ALPanelCineLightRig::syncDerivedStatus easy controls"})
                seen.add(("CineLightRigEasyMode", True))
            elif (re.match(r"^cine_(key|fill|rim|bg)_", widget) or
                  widget in {"cine_ratio", "cine_ratio_lock"}) and \
                    ("CineLightRigEasyMode", False) not in seen:
                prereqs.append({"setting": "CineLightRigEasyMode", "value": False,
                                "source": "ALPanelCineLightRig::syncDerivedStatus advanced controls"})
                seen.add(("CineLightRigEasyMode", False))
            if widget in {"cine_key_actor_only", "cine_fill_actor_only",
                          "cine_rim_actor_only", "cine_bg_actor_only",
                          "cine_catchlight_actor_only"} and \
                    ("CineLightRigActorOnlyAll", False) not in seen:
                prereqs.append({"setting": "CineLightRigActorOnlyAll", "value": False,
                                "source": "ALPanelCineLightRig::syncActorOnlyControls"})
                seen.add(("CineLightRigActorOnlyAll", False))
            if widget == "cine_fill_ev" and ("CineLightRigRatioLock", False) not in seen:
                prereqs.append({"setting": "CineLightRigRatioLock", "value": False,
                                "source": "ALPanelCineLightRig::syncDerivedStatus fill EV gate"})
                seen.add(("CineLightRigRatioLock", False))
        if row.get("version") == "v2" and row.get("page") == "light_rim_tab" and \
                widget.startswith("cine_rig_rim_tron_") and \
                ("CineRigRimEnabled", True) not in seen:
            prereqs.append({"setting": "CineRigRimEnabled", "value": True,
                            "source": "ALRigRimController::tickRefresh visibility gate"})
            seen.add(("CineRigRimEnabled", True))

        # The who-picker's backing combo is intentionally hidden; the visible
        # interaction is the cast rail/chip, covered by selector probes.
        if row.get("page") == "cine_light_rig_tab" and widget == "cine_anchor":
            row["action"] = "NO_EXERCISE"
            row["reason"] = "hidden who-picker backing combo; visible cast rail is tested separately"
        if row.get("page") == "cine_light_rig_tab" and widget == "cine_gobo_library":
            row["action"] = "NO_EXERCISE"
            row["reason"] = "login run has no inventory-backed gobo item; populated library requires in-world inventory"
        if row.get("page") == "cine_light_rig_tab" and widget == "cine_night_mask_tint_color":
            row["action"] = "NO_EXERCISE"
            row["reason"] = "modal color-picker selection has no public non-modal harness API; open/close safety is covered"
        if row.get("consumer") == "rim_render_active" and ("CineLightRigEnabled", True) not in seen:
            prereqs.append({"setting": "CineLightRigEnabled", "value": True,
                            "source": "ALCineRigRim::isEnabled"})
        row["enabled_control"] = prereqs

        confirmed_wired = {
            "db_mode", "db_t_speed", "db_t_desync", "gaze_enable_check",
            "gaze_deadzone_spinner", "gaze_composition_combo", "gaze_range_preset_combo",
            "gaze_blinks_check", "gaze_break_freq_slider", "switcher_custom_enabled",
            "shake_preset_combo", "ir_preset_action", "cine_easy_rim", "cine_easy_bg",
            "cine_easy_cone_width", "cine_easy_flare_preset", "cine_easy_night_mask_preset",
            "cine_seed_randomize", "cine_aim_reset", "cine_fixture_mode",
            "cine_fixture_preset", "cine_fixture_kelvin", "cine_open_cues",
            "cine_flare_preset", "cine_night_mask_preset",
        }
        temporal_presets = {
            "btn_freeze": (0.0, 1.0), "btn_quarter": (0.25, 1.0),
            "btn_half": (0.5, 1.0), "btn_realtime": (1.0, 0.5),
            "btn_double": (2.0, 1.0),
        }
        if row.get("page") == "temporal_tab" and widget in temporal_presets:
            expected, pre_value = temporal_presets[widget]
            row["setting"] = "TemporalWorldScale"
            row["expect"] = {"setting": "TemporalWorldScale", "type": "F32",
                             "value": expected, "commit": True,
                             "callback": row.get("owner", "")}
            row["oracle_pre_value"] = pre_value
            row.pop("effect_kind", None)
        if row.get("page") == "exposure_tab" and widget in {"exp_undo", "exp_redo"}:
            row["probe"] = "exposure_history"
            row.pop("effect_kind", None)
        if row.get("page") == "exposure_tab" and widget == "exp_filters_btn":
            row["probe"] = "launcher"
            row["launcher_floater"] = "lightbox"
            row.pop("effect_kind", None)
        if widget == "btn_reset_mode":
            default = defaults.get("CinematicCamOrbitRadius")
            if isinstance(default, (int, float)):
                row["reset_defaults"] = {"CinematicCamOrbitRadius": default}
                row["reset_prestate"] = {
                    "CinematicCamMode": 2,
                    "CinematicCamOrbitRadius": default + 1.0,
                }
                row["snapshot_all"] = True
                row.pop("effect_kind", None)
        if widget == "switcher_reset_slots" and "DirectorSwitcherBank" in defaults:
            stock = defaults["DirectorSwitcherBank"]
            altered = json.loads(json.dumps(stock))
            if isinstance(altered, list) and altered and isinstance(altered[0], dict):
                altered[0]["label"] = "Harness non-default"
                row["reset_defaults"] = {"DirectorSwitcherBank": stock}
                row["reset_prestate"] = {"DirectorSwitcherBank": altered}
                row["snapshot_all"] = True
                row.pop("effect_kind", None)

        if widget.startswith("reset_gaze_"):
            suffix = widget.removeprefix("reset_gaze_")
            if suffix in gaze_targets:
                target, value = gaze_targets[suffix]
                row.update(action="DEFER_INWORLD" if suffix in gaze_actor_override_resets else "RUN",
                           reason=(f"{widget}: reset mutates the edited actor override; login has no actor"
                                   if suffix in gaze_actor_override_resets else ""),
                           known_gap=False, probe="gaze_reset", reset_widget=target,
                           reset_value=value, reset_pre_value=(0 if value != 0 else 1),
                           reset_type="S32" if isinstance(value, int) and not isinstance(value, bool) else "F32",
                           owner="ALPanelLensGaze::postBuild")
                if suffix in gaze_actor_override_resets:
                    row["enable_when"] = "ALPanelLensGaze::editActors() is non-empty"
                    row["hazards"] = sorted(set(row.get("hazards", [])) | {"requires_world"})
                    row["gaze_oracle_field"] = suffix
                    row["gaze_oracle_value"] = -2 if suffix == "anim_priority" else -1
                elif suffix in gaze_reset_settings:
                    row["reset_setting"] = gaze_reset_settings[suffix]
        if widget == "reset_look_at_camera_mode":
            row.update(action="DEFER_INWORLD", known_gap=False,
                       reason=f"{widget}: reset mutates the edited actor override; login has no actor",
                       enable_when="ALPanelLensGaze::editActors() is non-empty",
                       hazards=sorted(set(row.get("hazards", [])) | {"requires_world"}),
                       probe="gaze_reset", reset_widget="look_at_camera_mode",
                       reset_value=0, reset_pre_value=1, reset_type="S32",
                       gaze_oracle_field="camera_mode", gaze_oracle_value=-1,
                       owner="ALPanelLensGaze::postBuild")
        if widget in launchers:
            row.update(action="RUN", reason="", known_gap=False, probe="launcher",
                       launcher_floater=launchers[widget],
                       owner=row.get("owner") or "LLFloaterDirector::bindPage")
        if widget == "vc_open_manager":
            row.update(probe="launcher", launcher_floater="prism_manager",
                       owner="ALPanelDirectorVCam::onControl")
        if widget == "am_reset_CineLightRigActorMaskBias":
            value = defaults.get("CineLightRigActorMaskBias", -8.0)
            row.update(action="RUN", reason="", known_gap=False,
                       reset_widget="am_bias_slider", reset_value=value,
                       reset_pre_value=value + 1.0, reset_type="F32",
                       probe="gaze_reset", owner="LightBox.ResetControlDefault")
        if widget == "db_on" and row.get("action") != "DEFER_INWORLD":
            row.update(action="RUN", reason="", known_gap=False, exercise="toggle",
                       owner="ALPanelDanceBall::postBuild")
        if widget in reset_all:
            prefix, master, notification = reset_all[widget]
            group = {name: value for name, value in defaults.items() if name.startswith(prefix)}
            row.update(action="RUN", reason="", known_gap=False, snapshot_all=True,
                       reset_defaults=group, reset_prestate={master: not bool(defaults.get(master, False))},
                       owner=row.get("owner") or "setting-only reset-all")
            if notification:
                row["confirm_notification"] = notification

        multiwrite = (row.get("probe") == "preset_strip" or row.get("provider") or
                      "preset" in widget.lower() or "apply_" in widget.lower() or
                      widget in reset_all or row.get("probe") == "launcher")
        if multiwrite:
            row["snapshot_all"] = True
            row["snapshot_rig"] = ("rig" in widget.lower() or row.get("page") in
                                   {"cine_light_rig_tab", "light_rim_tab", "tron_tab"})

        match = re.fullmatch(r"cine_(key|fill|rim|bg)_(shaft|hero|cone_width)", widget)
        if match:
            index = {"key": 0, "fill": 1, "rim": 2, "bg": 3}[match.group(1)]
            field = match.group(2)
            row.update(known_gap=False, owner="ALPanelCineLightRig::bindLightRigPage",
                       consumer="selected_rig_light_" + field, rig_light_index=index,
                       snapshot_rig=True)
            row["expect"] = {"ui": "selected rig slot state changes in widget units"}

        if row.get("action") == "DEFER_INWORLD" and row.get("reason") == \
                "page acts on in-world actor/object/selection":
            missing = "a logged-in avatar"
            if row.get("page") in {"props_tab", "ghosts_tab", "actors_tab"}:
                missing = "a logged-in object or actor selection"
            elif row.get("page") in {"dance_tab", "animate_tab", "moves_tab"}:
                missing = "a logged-in avatar and populated animation state"
            row["reason"] = f"{widget}: requires {missing}"

        # Record owner-controlled gates found close to a widget reference.
        if not prereqs and widget and not row.get("enable_widget"):
            pos = cpp_text.find(f'"{widget}"')
            if pos >= 0:
                window = cpp_text[max(0, pos - 800):pos + 1600]
                if "setEnabled" in window:
                    gates = re.findall(r'getBOOL\("([A-Za-z0-9_]+)"\)', window)
                    for gate in gates:
                        if gate != row.get("setting") and (gate, True) not in seen:
                            prereqs.append({"setting": gate, "value": True,
                                            "source": "C++ owner setEnabled/getBOOL"})
                            seen.add((gate, True))
                    if not gates:
                        expression = next(iter(re.findall(
                            r'(?:setEnabledIfChanged\s*\([^,]+,|->setEnabled\s*\()\s*([^;\n]+)',
                            window)), "")
                        world = re.search(r'\b(isAgentAvatarValid|gAgentAvatarp|getSelection|mActor|path|entity|have|inst|selected|crowd_active)\b', expression)
                        if world:
                            row["enable_when"] = f"owner refresh: {expression.strip()}"
                            row["action"] = "DEFER_INWORLD"
                            row["reason"] = f"{widget}: requires world prerequisite ({world.group(1)})"
                            row["hazards"] = sorted(set(row.get("hazards", [])) | {"requires_world"})
                        else:
                            unresolved.append(f"{row['version']}:{row['page']}:{widget}: owner setEnabled has no literal setting")

        if row.get("exercise") not in {"", "none", None} and row.get("action") not in {
                "SKIP", "NO_EXERCISE", "EXPECT_MISSING"}:
            inworld_allowed, inworld_evidence = _inworld_local_effect(row)
            row["inworld_allowed"] = inworld_allowed
            row["inworld_allow_evidence"] = inworld_evidence
            legacy_button_scan = (row.get("exercise") == "click" and
                                  row.get("widget_type") == "button")
            hazards, handler_resolved = _handler_hazards(row, cpp_sources, body_cache)
            if hazards:
                row["hazards"] = sorted(set(row.get("hazards", [])) | hazards)
                hazard_rows += 1
                # Hazardous callbacks are never authorized in-world, including
                # sliders/spinners and already-deferred rows. Login-screen
                # coverage may remain executable for non-button controls.
                _apply_inworld_hazard_policy(row, hazards)
                if ((row.get("action") == "RUN" and legacy_button_scan) or
                        row.get("motion_send_hazard")):
                    row["action"] = "SKIP"
                    row["reason"] = (
                        "in-world: may send animation request via AO override "
                        "(llvoavatar.cpp:7398-7413,7442-7457; "
                        "aldirectoranimswitcher.cpp:414,418,565; "
                        "llfloaterdirector.cpp:3726,3741)"
                        if row.get("motion_send_hazard") else
                        "resolved handler hazard: " + ", ".join(sorted(hazards)))
                    row["known_gap"] = False
            elif not handler_resolved and row.get("action") == "RUN" and legacy_button_scan:
                row["action"] = "SKIP"
                row["reason"] = "unresolved handler: hazard unknown"
                row["known_gap"] = False
                unresolved_hazards.append(f"{row['version']}:{row['page']}:{widget}")
            elif not handler_resolved:
                row["hazards"] = sorted(set(row.get("hazards", [])) | {"handler_unknown"})
                unresolved_hazards.append(f"{row['version']}:{row['page']}:{widget}")
            elif (handler_resolved and row.get("action") in {"RUN", "DEFER_INWORLD"} and
                  not row.get("setting") and not row.get("consumer") and
                  not row.get("reset_defaults") and
                  row.get("probe") not in {
                      "selector_segment", "temporal_preset", "gaze_reset",
                      "cine_fx_stop", "cine_live_probe_refresh", "vc_snap_view",
                      "vc_face_list", "vc_camera_selector", "actor_mover_active",
                      "actor_style_reset", "exposure_history", "launcher",
                  }):
                row["reason"] = f"{widget}: no independent state oracle"
                if row.get("action") == "RUN":
                    row["effect_kind"] = "side_effect_only"
                    row["known_gap"] = True
                else:
                    # Deferred rows must still exercise real in-world refusals,
                    # but a successful commit/visible change can never PASS.
                    row["no_state_oracle"] = True
                    row["known_gap"] = False
        # These exact rows completed their real in-world action and oracle in
        # run_functional_20261012_122652; an older UNWIRED classification must
        # not overwrite that evidence during enrichment.
        if widget in confirmed_wired:
            row["known_gap"] = False

    document.setdefault("meta", {})["r4_enable_unresolved"] = sorted(set(unresolved))
    document["meta"]["r4_enabled_control_rows"] = sum(bool(r["enabled_control"]) for r in rows)
    document["meta"]["r5_required_consumers"] = required_consumers
    document["meta"]["r5_total_consumers"] = sum(bool(r.get("consumer")) for r in rows)
    document["meta"]["r5_hazard_skip_rows"] = hazard_rows
    document["meta"]["r5_unresolved_handler_skips"] = sorted(set(unresolved_hazards))
    _enforce_inworld_run1_baseline(document, repo)
    if document["meta"]["r5_total_consumers"] < required_consumers:
        raise SystemExit(f"consumer preflight failed: expected at least {required_consumers}, got {document['meta']['r5_total_consumers']}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=1, ensure_ascii=True) + "\n", encoding="utf-8")
    return (document["meta"]["r4_enabled_control_rows"], len(set(unresolved)),
            document["meta"]["r5_total_consumers"], len(set(unresolved_hazards)))


def csv_path(value: str) -> Path:
    path = Path(value)
    return path / "functional.csv" if path.is_dir() else path


def load(value: str) -> list[dict[str, str]]:
    path = csv_path(value)
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    required = {*KEY, "ui_version", "inworld", "PASS/FAIL/SKIP", "reason"}
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
    unknown_modes = sorted({row["inworld"].lower() for row in rows} - {"true", "false"})
    if unknown_modes:
        raise SystemExit(f"{path}: unknown inworld values: {', '.join(unknown_modes)}")
    malformed = []
    for row in rows:
        try:
            observed = json.loads(row.get("observed", ""))
            if not isinstance(observed, dict):
                malformed.append(f"{row['ui_version']}:{row['page']}:{row['control']}")
        except (TypeError, json.JSONDecodeError):
            malformed.append(f"{row['ui_version']}:{row['page']}:{row['control']}")
    if malformed:
        raise SystemExit(f"{path}: {len(malformed)} row(s) lack structured observed JSON")
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
    modes = sorted({row.get("inworld", "false").lower() for row in rows})
    mode = "in-world" if modes == ["true"] else "login-screen" if modes == ["false"] else "mixed"
    lines = [f"# Director functional self-test: {title}", "", f"Mode: {mode}.", "",
             "| UI | Page | PASS | EXPECT_MISSING | FAIL | GAP | SKIP |", "|---|---|---:|---:|---:|---:|---:|"]
    for (version, page), count in sorted(counts(rows).items()):
        page_rows = [row for row in rows if row["ui_version"] == version and row["page"] == page]
        missing = sum(row["PASS/FAIL/SKIP"] == "PASS" and "MISSING" in row.get("expected", "")
                      for row in page_rows)
        lines.append(f"| {version} | {page} | {count['PASS'] - missing} | {missing} | {count['FAIL']} | {count['KNOWN_GAP']} | {count['SKIP']} |")
    total = Counter(row["PASS/FAIL/SKIP"] for row in rows)
    missing_total = sum(row["PASS/FAIL/SKIP"] == "PASS" and "MISSING" in row.get("expected", "")
                        for row in rows)
    lines += ["", f"Total: {total['PASS'] - missing_total} interactive/state PASS, "
              f"{missing_total} EXPECT_MISSING, {total['FAIL']} FAIL, "
              f"{total['KNOWN_GAP']} KNOWN_GAP (failing), {total['SKIP']} SKIP.", ""]
    reasons = Counter((row["ui_version"], row["PASS/FAIL/SKIP"], row["reason"] or "(none)")
                      for row in rows if row["PASS/FAIL/SKIP"] != "PASS")
    if reasons:
        lines += ["## Reasons", "", "| UI | Status | Reason | Count |",
                  "|---|---|---|---:|"]
        for (version, status, reason), count in sorted(reasons.items()):
            lines.append(f"| {version} | {status} | {reason.replace('|', '/')} | {count} |")
        lines.append("")
    breaks = Counter()
    for row in rows:
        observed = json.loads(row["observed"])
        for field in ("visible_break", "enabled_break"):
            value = observed.get(field)
            if isinstance(value, dict) and value.get("path"):
                breaks[(row["ui_version"], row["page"], field, value["path"])] += 1
    if breaks:
        lines += ["## First chain break points", "",
                  "| UI | Page | Chain | Path | Count |", "|---|---|---|---|---:|"]
        for (version, page, field, path), count in sorted(breaks.items()):
            lines.append(f"| {version} | {page} | {field} | {path.replace('|', '/')} | {count} |")
        lines.append("")
    skip_reasons = Counter((row["ui_version"], row["reason"].strip()) for row in rows
                           if row["PASS/FAIL/SKIP"] == "SKIP")
    if skip_reasons:
        lines += ["## Explicit skips", "", "| UI | Reason | Count |",
                  "|---|---|---:|"]
        for (version, reason), count in sorted(skip_reasons.items()):
            lines.append(f"| {version} | {reason} | {count} |")
        lines.append("")
    versions = sorted({row["ui_version"] for row in rows})
    if {"v1", "v2"}.issubset(versions):
        lines += compare([r for r in rows if r["ui_version"] == "v1"],
                         [r for r in rows if r["ui_version"] == "v2"], "v1", "v2")
    else:
        lines += ["## v1 vs v2", "", "- Only " + ", ".join(versions) +
                  " is present in this run; pass a second run to compare."]
    return lines


def budget_status(rows: list[dict[str, str]], budget_path: Path | None,
                  dry_reveal: bool = False) -> tuple[list[str], bool, dict]:
    lines = ["## Harness v2 budget", "",
             "| UI | Toggle | Slider | Spinner | Combo | Button | Interactive PASS / RUN | Floor |",
             "|---|---:|---:|---:|---:|---:|---:|---|"]
    failed = False
    kind_alias = {"toggle": "toggle", "slider": "slider", "spinner": "spinner",
                  "combo": "combo", "button": "button"}
    for version in sorted({row["ui_version"] for row in rows}):
        version_rows = [row for row in rows if row["ui_version"] == version]
        run = [row for row in version_rows if
               json.loads(row["observed"]).get("manifest_action") == "RUN" and
               "MISSING" not in row.get("expected", "") and row["kind"] not in {"state", "focus", "gate", "sentinel", "unit", "file", "floater"}]
        passes = []
        for row in run:
            observed = json.loads(row["observed"])
            if (row["PASS/FAIL/SKIP"] == "PASS" and
                    observed.get("click", {}).get("commits", 0) >= 1 and
                    not observed.get("effect", {}).get("programmatic", False) and
                    observed.get("effect", {}).get("category") != "B"):
                passes.append(row)
        counts_by_kind = Counter(kind_alias.get(row["kind"], row["kind"]) for row in passes)
        present_kinds = {kind_alias[row["kind"]] for row in run if row["kind"] in kind_alias}
        ratio = len(passes) / len(run) if run else 0.0
        floor = all(counts_by_kind[kind] >= 1 for kind in present_kinds) and ratio >= .60
        failed |= not floor and not dry_reveal
        lines.append(f"| {version} | {counts_by_kind['toggle']} | {counts_by_kind['slider']} | "
                     f"{counts_by_kind['spinner']} | {counts_by_kind['combo']} | {counts_by_kind['button']} | "
                     f"{len(passes)} / {len(run)} ({ratio:.1%}) | "
                     f"{'NOT RUN (DryReveal)' if dry_reveal else ('PASS' if floor else 'FAIL')} |")

    current: dict[str, dict[str, int]] = defaultdict(dict)
    budgeted_skips = [r for r in rows if r["PASS/FAIL/SKIP"] == "SKIP" and
                      json.loads(r["observed"]).get("manifest_action") in
                      {"SKIP", "NO_EXERCISE", "DEFER_INWORLD"}]
    for (version, page), group in itertools.groupby(
            sorted((r["ui_version"], r["page"]) for r in budgeted_skips)):
        current[version][page] = sum(1 for _ in group)
    if budget_path:
        frozen = json.loads(budget_path.read_text(encoding="utf-8"))
        increases = []
        for version, pages in current.items():
            for page, count in pages.items():
                allowed = int(frozen.get(version, {}).get(page, 0))
                if count > allowed:
                    increases.append(f"{version}/{page}: {count} > {allowed}")
        failed |= bool(increases)
        lines += ["", "Defer budget: " + ("FAIL — " + "; ".join(increases) if increases else "PASS (no page increased).")]
    lines.append("")
    return lines, failed, {version: dict(pages) for version, pages in current.items()}


def annotate_failure_shots(rows: list[dict[str, str]], run_dir: Path) -> tuple[int, str]:
    shots = run_dir / "functional_shots"
    if not shots.is_dir():
        return 0, ""
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return 0, "Pillow unavailable; screenshots were copied without rectangle overlays."
    annotated = 0
    for row in rows:
        if row["PASS/FAIL/SKIP"] != "FAIL":
            continue
        safe = re.sub(r"[^A-Za-z0-9_-]", "_", row["control"])[:80] or "row"
        candidates = sorted((shots / row["ui_version"] / row["page"]).glob(safe + "__*.png"))
        if not candidates:
            continue
        observed = json.loads(row["observed"])
        rects = observed.get("rect", {})
        floater = rects.get("floater")
        if not (isinstance(floater, list) and len(floater) == 4):
            continue
        for path in candidates:
            with Image.open(path) as source:
                image = source.convert("RGB")
            draw = ImageDraw.Draw(image)
            for name, colour in (("ctrl", "#ff4040"), ("target", "#40e0ff")):
                rect = rects.get(name)
                if not (isinstance(rect, list) and len(rect) == 4):
                    continue
                left = rect[0] - floater[0]
                right = rect[2] - floater[0]
                top = image.height - (rect[3] - floater[1])
                bottom = image.height - (rect[1] - floater[1])
                draw.rectangle((left, top, right, bottom), outline=colour, width=2)
            image.save(path)
            annotated += 1
    return annotated, ""


def self_test_handler_scanner() -> None:
    """Regression fixtures for exact/local/transitive hazard resolution."""
    source = r'''
bool Fixture::postBuild()
{
    getChild<LLButton>("lambda_danger")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) {
            gMessageSystem->sendReliable(host);
            gAgent.sendReliableMessage();
            sendAnimationRequest(id, true);
        });
    getChild<LLButton>("safe_neighbor")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSafe(); });
    getChild<LLButton>("danger_neighbor")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickShadowFixIt(); });
    if (LLButton* control = findChild<LLButton>("local_safe"))
    {
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSafe(); });
    }
    if (LLButton* control = findChild<LLButton>("local_danger"))
    {
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickShadowFixIt(); });
    }
    getChild<LLButton>("transitive_danger")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { adjustAim(); });
    mCenter[VX] = getChild<LLSpinCtrl>("localfog_center_x");
    const auto editor_commit = [this](LLUICtrl*, const LLSD&) { onEditorCommit(); };
    for (S32 axis = 0; axis < 3; ++axis)
        mCenter[axis]->setCommitCallback(editor_commit);
    return true;
}
void Fixture::onSafe() {}
void Fixture::onClickShadowFixIt() { gMessageSystem->sendReliable(host); }
void Fixture::adjustAim() { gAgent.sendReliableMessage(); }
void Fixture::onEditorCommit() { sendAnimationRequest(id, true); }
'''
    sources = {Path("fixture.cpp"): source}

    def scan(widget: str) -> tuple[dict[str, object], set[str], bool]:
        row: dict[str, object] = {
            "owner": "Fixture::postBuild", "widget_name": widget,
            "page": "fixture", "action": "RUN", "exercise": "click",
            "widget_type": "button", "inworld_allowed": True,
        }
        hazards, resolved = _handler_hazards(row, sources, {})
        _apply_inworld_hazard_policy(row, hazards)
        return row, hazards, resolved

    danger, hazards, resolved = scan("lambda_danger")
    assert resolved and {"server_message", "agent_send"}.issubset(hazards)
    assert danger["inworld_allowed"] is False, "hazardous lambda must be denied in-world"

    safe, hazards, resolved = scan("safe_neighbor")
    assert resolved and not hazards, "a neighbouring control's handler was attributed"
    assert safe["inworld_allowed"] is True

    local_safe, hazards, resolved = scan("local_safe")
    assert resolved and not hazards, "a reused local control donated its neighbour's handler"
    assert local_safe["inworld_allowed"] is True

    transitive, hazards, resolved = scan("transitive_danger")
    assert resolved and "agent_send" in hazards and transitive["inworld_allowed"] is False

    localfog, hazards, resolved = scan("localfog_center_x")
    assert resolved and "agent_send" in hazards and localfog["inworld_allowed"] is False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", nargs="?", help="functional.csv or its run directory")
    parser.add_argument("right", nargs="?", help="optional comparison CSV/run directory")
    parser.add_argument("-o", "--output", help="write Markdown here (default: stdout)")
    parser.add_argument("--prepare-manifest", type=Path,
                        help="enrich a generated functional manifest for the R4 runner")
    parser.add_argument("--prepared-output", type=Path,
                        help="write the merged manifest here; never modifies the input")
    parser.add_argument("--repo-root", type=Path,
                        help="repository root for --prepare-manifest")
    parser.add_argument("--pages", help="comma-separated page filter")
    parser.add_argument("--rows", help="control-name regular expression")
    parser.add_argument("--version", choices=("v1", "v2", "both"))
    parser.add_argument("--defer-budget", type=Path)
    parser.add_argument("--update-budget", action="store_true",
                        help="write a proposed budget beside the report (never rewrites tracked inputs)")
    parser.add_argument("--dry-reveal", action="store_true",
                        help="report reveal-only results without enforcing the click PASS floor")
    parser.add_argument("--self-test", action="store_true",
                        help="run callback-scanner regression fixtures")
    args = parser.parse_args()
    if args.self_test:
        self_test_handler_scanner()
        print("functional_report self-tests: PASS")
        return 0
    if args.prepare_manifest:
        if not args.repo_root:
            parser.error("--prepare-manifest requires --repo-root")
        if not args.prepared_output:
            parser.error("--prepare-manifest requires --prepared-output")
        enabled, unresolved, consumers, unknown = prepare_manifest(
            args.prepare_manifest, args.repo_root, args.prepared_output)
        print(f"R5 manifest: {enabled} prerequisite row(s), {unresolved} unresolved owner gate(s), "
              f"{consumers} consumers, {unknown} unresolved-handler SKIP row(s)")
        return 0
    if not args.left:
        parser.error("left is required unless --prepare-manifest is used")
    left = load(args.left)
    pages = {p.strip() for p in (args.pages or "").split(",") if p.strip()}
    row_re = re.compile(args.rows) if args.rows else None
    if pages:
        left = [row for row in left if row["page"] in pages]
    if row_re:
        left = [row for row in left if row_re.search(row["control"])]
    if args.version and args.version != "both":
        left = [row for row in left if row["ui_version"] == args.version]
    right = load(args.right) if args.right else []
    lines = render(left, csv_path(args.left).parent.name)
    annotated, shot_note = annotate_failure_shots(left, csv_path(args.left).parent)
    if annotated or shot_note:
        lines += ["", "## Failure screenshots", "",
                  f"Annotated {annotated} screenshot(s)." + (f" {shot_note}" if shot_note else "")]
    budget_lines, budget_failed, proposed_budget = budget_status(
        left, args.defer_budget, args.dry_reveal)
    lines += [""] + budget_lines
    if args.right:
        lines += [""] + compare(left, right,
                                 csv_path(args.left).parent.name,
                                 csv_path(args.right).parent.name)
    text = "\n".join(lines).rstrip() + "\n"
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8", newline="\n")
        if args.update_budget:
            candidate = Path(args.output).with_name("defer_budget.proposed.json")
            candidate.write_text(json.dumps(proposed_budget, indent=2, sort_keys=True) + "\n",
                                 encoding="utf-8", newline="\n")
    else:
        print(text, end="")
    failed = any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in left)
    failed = failed or budget_failed
    if args.right:
        failed = failed or any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in right)
    if args.dry_reveal:
        print("DRY REVEAL: NOT A FUNCTIONAL RESULT")
        return 3
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
