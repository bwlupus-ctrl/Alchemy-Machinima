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

NETWORK_HAZARDS = frozenset({
    "server_message", "agent_send", "object_send", "selection_send",
    "selection_broadcast", "parcel_send", "chat_im_voice", "world_action",
    "scripted_dialog",
})
LOCAL_HAZARDS = frozenset({
    "file_picker", "destructive", "logout", "delete", "rename",
    "export_import", "save_disk", "clipboard_write", "external_url_process",
    "persistent_setting", "modal",
})
LOGIN_BLOCKING_HAZARDS = frozenset({
    "file_picker", "destructive", "logout", "clipboard_write",
    "external_url_process", "modal", "opens_color_picker",
})
LOGIN_ONLY_BLOCKER_RULES = {
    "logout": r"\b(?:requestQuit|forceQuit|userQuit|handleQuit|forceDisconnect|logout|disconnectFromGrid|exit)\s*\(",
}

HAZARD_RULES = {
    "file_picker": r"\b(?:LLFilePicker|LLFilePickerReplyThread|LLFloaterFilePicker|getOpenFile|getSaveFile)\b",
    "delete": (r"\b(?:gDirUtilp\s*->\s*deleteFilesInDir|std::filesystem::(?:remove|remove_all)"
               r"|LLFile::(?:remove|unlink|rmdir)|remove_all|DeleteFile|removeItem|removeCategory|purgeObject|purgeDescendents"
               r"|deleteItem|deleteObject|deletePreset|removePreset|deleteScene|removeScene|deleteTake|removeTake"
               r"|deleteGhost|removeGhost|deleteNode|removeNode|deleteCue|removeCue|deleteSlot|clearSlot)\s*\("),
    "rename": r"\b(?:LLFile::(?:rename|copy)|std::filesystem::(?:rename|copy))\s*\(",
    "export_import": r"\b(?:export\w*|import\w*)\s*\(",
    "save_disk": (r"\b(?:llofstream|std::ofstream|ofstream)\b"
                  r"|\b(?:LLFile::)?fopen\s*\([^;\n]*?(?:LLFILE_MODE\s*\(\s*)?[\"'][^\"']*[wa+][^\"']*[\"']"
                  r"|\bLLAPRFile::(?:open|write)\s*\("
                  r"|\bLLAPRFile\s+([A-Za-z_]\w*)\s*;[^{}]{0,200}?\b\1\s*\.\s*(?:open|write)\s*\("
                  r"|\bLLSDSerialize::(?:toXML|toPrettyXML|toBinary|serialize)\s*\([^;{}]{0,200}?,\s*(?!std::(?:cout|cerr)\b)[A-Za-z_]\w*\s*\)"
                  r"|\b(?:save(?:ToFile|File|Preset|Settings|Scene|Take)\w*|writePreset|write[A-Z]\w*File)\s*\("),
    "logout": r"\b(?:requestQuit|forceDisconnect|logout|disconnectFromGrid)\s*\(",
    "server_message": (r"\bgMessageSystem\s*->\s*(?:newMessage\w*|sendReliable|sendMessage)\s*\("
                       r"|\bsendReliable(?:Message)?\s*\(|\bsendMessage\s*\("),
    "agent_send": (r"\b(?:sendAnimationRequests?|sendAgentUpdate|send_agent_update)\s*\("
                   r"|\b(?:gAgent|LLAgent\w*)\b[^;{}]{0,100}?\bsend[A-Z]\w*\s*\("
                   r"|\bLLAgent::set(?:LookAt|PointAt)\s*\("
                   r"|\b(?:gAgent|gAgentCamera)\s*\.\s*set(?:LookAt|PointAt)\s*\("
                   r"|\bgAgent\s*\.\s*set(?:Flying|AFK|DoNotDisturb|StartPosition)\s*\("
                   r"|\bLLHUDManager::(?:getInstance\s*\(\s*\)\s*->|instance\s*\(\s*\)\s*\.)?createViewerEffect\s*\("
                   r"|\bAOEngine\s*::\s*(?:(?:instance|getInstance)\s*\(\s*\)\s*(?:\.|->)\s*)?override\s*\("
                   r"|\bAOEngine\s*::\s*[A-Za-z_]\w*Request\s*\("),
    "object_send": (r"\bLLViewerObject\w*\b[^;{}]{0,120}?\bsend[A-Z]\w*\s*\("
                    r"|->\s*send[A-Z]\w*\s*\("),
    "selection_send": r"\bLLSelectMgr::(?:getInstance\s*\(\s*\)\s*->|instance\s*\(\s*\)\s*\.)\s*(?:selection\w*|send[A-Z]\w*)\s*\(",
    "selection_broadcast": (r"\b(?:selectObjectOnly|selectObjectAndFamily|selectProxy"
                            r"|selectHighlightedObjects|selectionSetObject\w*)\s*\("
                            r"|\bLLSelectMgr::(?:getInstance\s*\(\s*\)\s*->|instance\s*\(\s*\)\s*\.)\s*(?:select|deselect)\w*\s*\("
                            r"|\"animation_explorer\""
                            r"|\bALToolGhostEdit\w*\b[^;{}]{0,80}?\bsetEdit\w*\s*\("),
    "chat_im_voice": r"\b(?:sendChat|sendIM|sendInstantMessage|send_chat_from_viewer|send_improved_im|startCall|startVoice|speak|shout)\w*\s*\(",
    # Case-sensitive call forms: retain the pre-round-5 action verbs while
    # explicitly covering the concrete viewer APIs that those broad verbs miss.
    "world_action": (r"\b(?:gAgent\s*\.\s*teleport(?:Home|Cancel|ViaLandmark|ViaLocation\w*|Request)"
                     r"|LLAgent::teleport(?:Home|Cancel|ViaLandmark|ViaLocation\w*|Request)"
                     r"|LLAvatarActions::(?:teleportRequest|pay|offerTeleport)"
                     r"|LLEnvironment::(?:instance|getInstance)\s*\(\s*\)\s*(?:\.|->)\s*(?:updateParcel|updateRegion)\w*"
                     r"|LLEnvironment::(?:updateParcel|updateRegion)"
                     r"|LLSelectMgr::getInstance\s*\(\s*\)\s*->\s*(?:selection\w*|send[A-Z]\w*|select\w*|deselect\w*)"
                     r"|gAgent\s*\.\s*(?:standUp|sitDown)"
                     r"|gAgent\s*\.\s*startAutoPilot\w*"
                     r"|startAutoPilot\w*"
                     r"|handle_object_sit"
                     r"|LLAppearanceMgr::(?:instance|getInstance)\s*\(\s*\)\s*(?:\.|->)\s*(?:wear|remove|replace)\w*"
                     r"|LLAppearanceMgr::(?:instance|getInstance)\s*\(\s*\)\s*(?:\.|->)\s*addCOFItemLink"
                     r"|addCOFItemLink"
                     r"|LLAppearanceMgr::(?:wear|remove|replace)\w*"
                     r"|gInventory\s*\.\s*(?:remove|purge|changeItemParent|changeCategoryParent)\w*"
                     r"|(?:create_inventory_item|create_inventory_wearable|create_inventory_callingcard|update_inventory_item|update_inventory_category|copy_inventory_item|move_inventory_item|remove_inventory_\w*|purge_descendents_of|LLGiveInventory::\w+)"
                     r"|LLToolDragAndDrop::handleGiveDragAndDrop"
                     r"|LLViewerParcelMgr::getInstance\s*\(\s*\)\s*->\s*(?:sendParcel|buy|claim|release|deed)\w*"
                     r"|LLViewerParcelMgr::getInstance\s*\(\s*\)\s*->\s*startBuyLand"
                     r"|startBuyLand"
                     r"|LLViewerRegion::(?:set|update)\w*|send_ObjectDeRez|derez_objects(?:_separate)?"
                     r"|handle_object_take|handle_object_return)\s*\("
                     r"|\b(?:rezObject|rezAttachment|rezAtRoot|duplicateObjects|upload_new_resource|upload_new_caps_resource|inviteToGroup|ejectFromGroup)\s*\("
                     r"|\b(?:updateParcel|updateRegion)\s*\("
                     r"|\b(?:gAgent\s*\.\s*|LLAgent::)?setControlFlags\s*\([^;\n]*(?:AGENT_CONTROL_SIT|AGENT_CONTROL_STAND)"),
    "parcel_send": (r"\bLLViewerParcel\w*\b[^;{}]{0,80}?\b(?:send|update|claim|deed|buy|release)[A-Z]\w*\s*\("
                    r"|\bgAgent\s*\.\s*getRegionCapability\s*\("
                    r"|\b(?:LLHTTPClient|LLCoreHttpUtil)\w*::[^;{}]{0,160}?\b(?:post|put)\w*\s*\("),
    "scripted_dialog": r"\b(?:ScriptDialog|ScriptQuestion|LLScriptFloater|scriptQuestion)\b",
    "clipboard_write": r"\b(?:copyToClipboard|copyTextToClipboard|copyFromString)\s*\(",
    "external_url_process": (r"\b(?:loadURLExternal|spawnWebBrowser|openDir|ShellExecuteW?|"
                             r"CreateProcessW?|system)\s*\("
                             r"|\bLLProcess\s*::\s*create\s*\("),
    "modal": r"\bOSMessageBox\s*\(",
    "persistent_setting": (r"\b(?:gSavedPerAccountSettings|gCrashSettings|"
                           r"gWarningSettings)\s*\.\s*(?:set[A-Z]\w*|"
                           r"getControl\s*\([^;\n]{0,120}?setValue)"),
}


def _hazard_classes(hazards: set[str]) -> list[str]:
    classes = []
    if hazards & NETWORK_HAZARDS:
        classes.append("NETWORK")
    if hazards & LOCAL_HAZARDS:
        classes.append("LOCAL")
    return classes


def _mode_denies_hazards(hazards: set[str], inworld: bool,
                         login_no_circuit_proved: bool,
                         login_snapshot_verified: bool = True) -> bool:
    """Pure policy fixture shared by scanner tests and manifest metadata."""
    if inworld:
        return bool(hazards & (LOCAL_HAZARDS | NETWORK_HAZARDS))
    if not login_snapshot_verified or hazards & LOGIN_BLOCKING_HAZARDS:
        return True
    if hazards & NETWORK_HAZARDS:
        return not login_no_circuit_proved
    return False


def _login_block_reason(hazards: set[str]) -> str:
    reasons = []
    if "file_picker" in hazards:
        reasons.append("native file picker/dialog can block harness input")
    if hazards & {"modal", "opens_color_picker", "scripted_dialog"}:
        reasons.append("modal dialog can block harness input")
    if "logout" in hazards or "destructive" in hazards:
        reasons.append("quit/logout/exit can terminate or escape the run")
    if "external_url_process" in hazards:
        reasons.append("launches an external process or URL")
    if "clipboard_write" in hazards:
        reasons.append("writes the user's unsnapshotted clipboard")
    return "login denied: " + "; ".join(reasons)


def _login_blocking_hazards(row: dict[str, object], hazards: set[str]) -> set[str]:
    blocked = hazards & LOGIN_BLOCKING_HAZARDS
    blocked.update(row.get("direct_login_blocking_hazards", []))
    # scripted_dialog is a NETWORK rule and may arrive only from the
    # conservative callee-TU union. It blocks login only when directly
    # reachable; NETWORK ambiguity alone is inert under the no-circuit proof.
    if "scripted_dialog" in set(row.get("direct_hazards", [])):
        blocked.add("scripted_dialog")
    return blocked

# In-world execution is allow-listed by source owner. Generated bind=setting
# rows qualify only after their C++ callbacks, listeners, and consumers scan
# resolves completely and finds no hazard.
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
# btn_enroll: its handler reads the LLSelectMgr primary object, and any selection
# (even of a viewer-only object) broadcasts PointAt + AGENT_STATE_EDITING
# (llselectmgr.cpp:679,8049; llhudeffectpointat.cpp:254; llagent.cpp:2312-2315).
# db_from_explorer: Animation Explorer sends ObjectSelect/ObjectDeselect for
# non-avatar animation sources (animationexplorer.cpp:540-556).
INWORLD_EXCLUDED_CONTROLS = {"vc_lens_locate", "vc_face_locate",
                             "btn_anim_ex_stop", "btn_anim_ex_revoke", "btn_anim_ex_blacklist",
                             "btn_enroll", "db_add_selected", "db_from_explorer",
                             # opens Animation Explorer (ObjectSelect for sources);
                             # ghost edit mode selects its proxy volume (PointAt).
                             "btn_open_animexp", "edit_ghosts_check"}
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

    if row.get("binding") == "setting" and row.get("setting"):
        if row.get("setting_reference_outside_function"):
            return False, "setting referenced from a table/initializer outside a function"
        if row.get("setting_listener_unknown") or not row.get("handler_scan_resolved"):
            return False, "setting callback/listener scan unresolved"
        if row.get("setting_sole_sink"):
            return True, "generated kit bind=setting; sole sink is gSavedSettings"
        return True, "generated kit bind=setting; C++ callbacks/listeners resolved hazard-free"

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
    if row.get("handler_scan_resolved"):
        return True, "resolved viewer-local handler (network/local hazards checked separately)"
    return False, "handler unresolved"


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
    """Return a definition, never a call nested in an if/while condition."""
    if not qualified or qualified not in text:
        return ""
    signature = re.compile(r"^[ \t]*[^\n;{}()]*?(?<![\w:])" + re.escape(qualified) + r"\s*\(", re.M)
    for match in signature.finditer(text):
        opening = text.rfind("(", match.start(), match.end())
        closing = _matching_delimiter(text, opening)
        if closing < 0:
            continue
        tail = re.match(r"\s*(?:const\s*)?(?:noexcept\s*)?\{", text[closing + 1:])
        if not tail:
            continue
        start = closing + 1 + tail.end() - 1
        end = _matching_delimiter(text, start, "{", "}")
        if end >= 0:
            return text[start:end + 1]
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


def _split_cpp_args(text: str) -> list[str]:
    """Split a C++ argument list without splitting nested calls/templates."""
    result: list[str] = []
    start = 0
    depths = {"(": 0, "[": 0, "{": 0, "<": 0}
    pairs = {")": "(", "]": "[", "}": "{", ">": "<"}
    quote = ""
    escaped = False
    for pos, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            continue
        if char in {'"', "'"}:
            quote = char
        elif char in depths:
            depths[char] += 1
        elif char in pairs and depths[pairs[char]]:
            depths[pairs[char]] -= 1
        elif char == "," and not any(depths.values()):
            result.append(text[start:pos].strip())
            start = pos + 1
    result.append(text[start:].strip())
    return result


def _cpp_without_comments(text: str) -> str:
    """Blank C++ comments while retaining offsets and string literals."""
    chars = list(text)
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    raw_end = ""
    pos = 0
    while pos < len(text):
        char = text[pos]
        following = text[pos + 1] if pos + 1 < len(text) else ""
        if raw_end:
            if text.startswith(raw_end, pos):
                pos += len(raw_end)
                raw_end = ""
            else:
                pos += 1
            continue
        if line_comment:
            if char == "\n":
                line_comment = False
            else:
                chars[pos] = " "
            pos += 1
            continue
        if block_comment:
            if char == "*" and following == "/":
                chars[pos] = chars[pos + 1] = " "
                block_comment = False
                pos += 2
            elif char != "\n":
                chars[pos] = " "
                pos += 1
            else:
                pos += 1
            continue
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = ""
            pos += 1
            continue
        if char == "/" and following == "/":
            chars[pos] = chars[pos + 1] = " "
            line_comment = True
            pos += 2
            continue
        if char == "/" and following == "*":
            chars[pos] = chars[pos + 1] = " "
            block_comment = True
            pos += 2
            continue
        raw = re.match(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(', text[pos:pos + 24])
        if raw and (pos == 0 or not (text[pos - 1].isalnum() or text[pos - 1] == "_")):
            raw_end = ")" + raw.group(1) + '"'
            pos += raw.end()
            continue
        if char in {'"', "'"}:
            # C++14 digit separators are not character-literal delimiters.
            if char == "'" and pos > 0 and pos + 1 < len(text) and \
                    text[pos - 1].isalnum() and text[pos + 1].isalnum():
                pos += 1
                continue
            quote = char
        pos += 1
    return "".join(chars)


def _callback_call(text: str, match: re.Match[str]) -> str:
    """Extract a complete set*Callback/childSet*/registrar.add call."""
    opening = text.rfind("(", match.start(), match.end())
    closing = _matching_delimiter(text, opening) if opening >= 0 else -1
    return text[match.start():closing + 1] if closing >= 0 else ""


def _source_body(sources: dict[Path, str], qualified: str,
                 preferred: Path | None = None,
                 cache: dict[str, object] | None = None) -> tuple[str, Path | None]:
    key = "@body:" + qualified + "@" + str(preferred or "")
    if cache is not None and key in cache:
        return cache[key]
    paths = ([preferred] if preferred in sources else []) + [p for p in sources if p != preferred]
    for path in paths:
        body = _function_body(sources[path], qualified)
        if body:
            result = (body, path)
            if cache is not None:
                cache[key] = result
            return result
    if cache is not None:
        cache[key] = ("", None)
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
    if restrict is None and cache is not None:
        index_key = "@all_member_body_index"
        if index_key not in cache:
            index: dict[str, list[tuple[str, Path]]] = defaultdict(list)
            definition = re.compile(
                r"^[ \t]*(?:[\w:<>,*&~]+[ \t]+)*((?:[A-Za-z_]\w*::)*([A-Za-z_]\w*))\s*"
                r"\([^;{}]*\)\s*(?:const\s*)?\{", re.M)
            control_words = {"if", "for", "while", "switch", "catch"}
            for path, source_text in sources.items():
                for match in definition.finditer(source_text):
                    name = match.group(2)
                    if name in control_words:
                        continue
                    opening = source_text.find("{", match.start(), match.end())
                    closing = _matching_delimiter(source_text, opening, "{", "}")
                    if closing >= 0:
                        # Validate this candidate in place; rescanning the entire
                        # source for every candidate makes the index quadratic.
                        prefix = source_text[source_text.rfind("\n", 0, match.start()) + 1:match.start()]
                        if not re.fullmatch(r"[\w:<>,*&~ \t]*", prefix) or re.search(r"\b(?:return|if|else|while|for|switch)\b", prefix):
                            continue
                        args_start = source_text.find("(", match.start())
                        args_end = _matching_delimiter(source_text, args_start)
                        if args_end < 0 or not re.fullmatch(r"\s*(?:const\s*)?", source_text[args_end + 1:opening]):
                            continue
                        index[name].append((source_text[opening:closing + 1], path))
            cache[index_key] = index
        found = cache[index_key].get(leaf, [])  # type: ignore[union-attr]
        cache[cache_key] = found
        return found
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


def _receiver_types(text, source, sources, cache):
    """Resolve object identifiers from declarations, rather than method-name collisions."""
    key = "@receivers:" + str(source) + ":" + str(hash(text))
    if key in cache:
        return cache[key]
    declaration = re.compile(
        r"\b((?:(?:LL|AL)[A-Za-z_]\w*(?:::\w+)*|std::(?:shared_ptr|unique_ptr))"
        r"(?:\s*<[^;{}\n]+>)?)(?:\s+[*&]*\s*|[*&]+\s*)([A-Za-z_]\w*)\s*(?=[\[;=({,)])")
    def parse(content, members_only=False):
        found = defaultdict(set)
        for match in declaration.finditer(content):
            kind, name = match.groups()
            if members_only and not re.match(r"[mg][A-Z_]", name):
                continue
            pointer = re.match(r"(?:LLPointer|std::shared_ptr|std::unique_ptr)\s*<\s*((?:LL|AL)\w+(?:::\w+)*)", kind)
            kind = pointer.group(1) if pointer else kind.split("<", 1)[0].strip()
            found[name].add(kind)
        return found
    global_key = "@global_receiver_types"
    if global_key not in cache:
        globals_index = defaultdict(set)
        for parent in {p.parent for p in sources if p.parent.is_dir()}:
            for header in parent.glob("*.h"):
                content = header.read_text(encoding="utf-8", errors="replace")
                for line in re.findall(r"\bextern\s+[^;]+;", content):
                    for name, kinds in parse(line).items():
                        globals_index[name].update(kinds)
        cache[global_key] = globals_index
    result = {name: set(kinds) for name, kinds in cache[global_key].items()}
    if source:
        header_key = "@member_receiver_types:" + str(source)
        if header_key not in cache:
            header = source.with_suffix(".h")
            content = sources.get(source, "")
            if header.is_file():
                content += "\n" + header.read_text(encoding="utf-8", errors="replace")
            cache[header_key] = parse(content, members_only=True)
        result.update({name: set(kinds) for name, kinds in cache[header_key].items()})
        offset = sources.get(source, "").find(text)
        if offset >= 0:
            signature = sources[source][max(0, offset - 600):offset]
            # Parameters belong to this definition; previous bodies do not.
            signature = signature[signature.rfind("}") + 1:]
            result.update(parse(signature))
    result.update(parse(text))
    cache[key] = result
    return result


def _expand_callback_calls(seed: list[tuple[str, Path | None]], owner_class: str,
                           sources: dict[Path, str], body_cache: dict[str, object],
                           depth_limit: int = 3,
                           reached_paths: set[Path] | None = None,
                           unresolved_member_calls: set[str] | None = None,
                           dispatch_widget: str = "",
                           resolve_members: bool = True) -> list[str]:
    """Scan callback text and its same/known-owner-file callees to bounded depth."""
    expansion_key = "@expand:" + owner_class + ":" + dispatch_widget + ":" + str(depth_limit) + ":" + str(resolve_members) + ":" + repr(
        sorted((str(path or ""), hash(text)) for text, path in seed))
    cached_expansion = body_cache.get(expansion_key)
    if cached_expansion is not None:
        cached_result, cached_paths, cached_unresolved = cached_expansion
        if reached_paths is not None:
            reached_paths.update(cached_paths)
        if unresolved_member_calls is not None:
            unresolved_member_calls.update(cached_unresolved)
        return list(cached_result)
    result: list[str] = []
    queue = [(text, path, 0) for text, path in seed]
    seen: set[tuple[str, str]] = set()
    ignored = {
        "if", "for", "while", "switch", "return", "sizeof", "decltype", "catch",
        "getChild", "findChild", "setCommitCallback", "setClickedCallback",
        "childSetAction", "childSetCommitCallback", "bind", "add",
        "get", "getValue", "getName", "getID", "getUUID", "getHandle",
        "asString", "asBoolean", "asInteger", "asReal", "isMap", "isArray",
        "empty", "size", "begin", "end", "find", "count", "insert",
        "push_back", "emplace_back", "reset", "clear", "lock", "unlock",
        "erase", "reserve", "at", "length", "compare", "notNull", "isNull",
        "getJoint", "isDead", "getWidth", "getHeight", "getParent", "isFinite",
        "getPosGlobalFromAgent", "getPosAgentFromGlobal", "getWaterHeight",
        "getRegion", "getWorldMatrix", "getPositionAgent", "getRenderPosition",
        "getRotation", "getVisible", "getEnabled", "getControl", "getSignal",
        "getCommitSignal", "getString", "getFullname", "getObject", "getAvatar",
        "asAvatar", "getControlAvatar", "isGhostAvatar", "isAttachment",
        "setValue", "setVisible", "setEnabled", "setText", "setLabel", "reshape",
        "getBOOL", "getS32", "getU32", "getF32", "getLLSD", "getVector3",
        "getVector3d", "getColor3", "getColor4", "setBOOL", "setS32", "setU32",
        "setF32", "setString", "setLLSD", "llclamp", "llmin", "llmax", "ll_round",
        "llabs", "fabs", "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast",
        "groupForMember", "findGroup", "getMember", "findEntry", "getItemCount",
        "getControlVariable", "getPath", "getEffects", "getState", "selected", "isActive",
        "getInstance", "instance",
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
        if reached_paths is not None and source is not None:
            reached_paths.add(source)
        restrict = owner_files | frozenset(p for p in (source,) + tuple(
            q for _, q in seed) if p is not None)
        if depth >= depth_limit:
            continue
        calls = set(re.findall(r"(?<![.&>])\b((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(", text))
        calls.update(re.findall(r"&([A-Za-z_]\w*(?:::[A-Za-z_]\w*)+)", text))
        member_calls = set(re.findall(r"(?:\.|->)\s*([A-Za-z_]\w*)\s*\(", text))
        for called in calls:
            leaf = called.rsplit("::", 1)[-1]
            if leaf in ignored:
                continue
            candidates = [called] if "::" in called else ([owner_class + "::" + called] if owner_class else [])
            for candidate in candidates:
                body, body_source = _source_body(sources, candidate, source, body_cache)
                if (body and owner_class == "ALPanelDirectorVCam" and
                        candidate.rsplit("::", 1)[-1] in {
                            "onControl", "commitCamera", "commitDisplay", "commitProjector"}):
                    body, branch_found = _vcam_dispatch_branch(body, dispatch_widget)
                    if not branch_found and unresolved_member_calls is not None:
                        unresolved_member_calls.add(
                            candidate.rsplit("::", 1)[-1] + ":" + dispatch_widget)
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
        # Prefer an explicit singleton receiver. Otherwise inspect candidate
        # definitions rather than treating an unrelated same-name method as
        # proof of a call. Audited camera/light models use explicit receivers.
        receiver_types = _receiver_types(text, source, sources, body_cache) if resolve_members else {}
        for leaf in (member_calls - ignored if resolve_members else ()):
            # A singleton-qualified receiver gives the actual service type.
            # Resolve it before considering unrelated methods with the same name.
            typed = re.findall(
                r"\b((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)::(?:instance|getInstance)\s*\(\s*\)"
                r"\s*(?:\.|->)\s*" + re.escape(leaf) + r"\s*\(", text)
            identifiers = re.findall(r"\b([A-Za-z_]\w*)(?:\s*\[[^\]]+\])?\s*(?:\.|->)\s*" + re.escape(leaf) + r"\s*\(", text)
            for identifier in identifiers:
                typed.extend(receiver_types.get(identifier, ()))
            candidates = []
            for receiver in dict.fromkeys(typed):
                typed_body, typed_source = _source_body(sources, receiver + "::" + leaf, source, body_cache)
                if typed_body and typed_source:
                    candidates.append((typed_body, typed_source))
            if not typed:
                candidates = _source_bodies_by_leaf(sources, leaf, source, body_cache, restrict)
                if not candidates:
                    candidates = _source_bodies_by_leaf(sources, leaf, source, body_cache, None)
                if len(candidates) > 1:
                    if unresolved_member_calls is not None:
                        unresolved_member_calls.add(leaf)
                    continue
            # A known receiver implemented outside newview must not borrow an
            # unrelated newview class's method merely because the leaf matches.
            for body, body_source in candidates:
                if owner_class == "ALPanelDirectorVCam" and leaf in {
                        "onControl", "commitCamera", "commitDisplay", "commitProjector"}:
                    body, branch_found = _vcam_dispatch_branch(body, dispatch_widget)
                    if not branch_found and unresolved_member_calls is not None:
                        unresolved_member_calls.add(leaf + ":" + dispatch_widget)
                key = ("member:" + leaf + ":" + str(hash(body)), str(body_source))
                if key not in seen:
                    seen.add(key)
                    queue.append((body, body_source, depth + 1))
    body_cache[expansion_key] = (
        tuple(result), frozenset(reached_paths or ()),
        frozenset(unresolved_member_calls or ()))
    return result


def _setting_table_listeners(sources, cache):
    """Resolve literal range-for listener tables to their actual callback scope."""
    key = "@setting_table_listeners"
    if key in cache:
        return cache[key]
    index = defaultdict(list)
    scopes = []
    loop_re = re.compile(
        r"\bfor\s*\(\s*(?:const\s+)?(?:char\s*\*|std::string\s*&?|auto\s*&?)\s*"
        r"(?:const\s+)?([A-Za-z_]\w*)\s*:\s*(\{[^}]*\}|[A-Za-z_]\w*)\s*\)")
    for path, text in sources.items():
        for loop in loop_re.finditer(text):
            variable, collection = loop.groups()
            table = collection
            if not table.startswith("{"):
                declarations = list(re.finditer(r"\b" + re.escape(collection) +
                    r"\s*(?:\[\s*\])?\s*=\s*(\{[^}]*\})", text[:loop.start()]))
                if not declarations:
                    continue
                table = declarations[-1].group(1)
            start = loop.end()
            while start < len(text) and text[start].isspace():
                start += 1
            if start >= len(text):
                continue
            if text[start] == "{":
                end = _matching_delimiter(text, start, "{", "}") + 1
            else:
                end = start
                while end < len(text) and text[end] != ";":
                    if text[end] in "([{":
                        end = _matching_delimiter(text, end, text[end], {"(": ")", "[": "]", "{": "}"}[text[end]])
                        if end < 0:
                            break
                    end += 1
                end += 1
            scope = text[start:end] if end > start else ""
            control = r"getControl\s*\(\s*" + re.escape(variable) + r"\s*\)"
            if not re.search(control, scope) or not re.search(r"get(?:Commit)?Signal\s*\(\s*\)\s*->\s*connect", scope):
                continue
            scopes.append((path, loop.start(), end, variable))
            for setting in re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"', table):
                index[setting].append((re.sub(control, 'getControl("' + setting + '")', scope), path))
    cache["@setting_table_scopes"] = scopes
    cache[key] = index
    return index


def _setting_reader_scope(body: str, literal: str) -> str:
    """Keep a block-local cached read with the code that can use its value."""
    declarations = list(re.finditer(r"LLCachedControl\s*<[^>]+>\s+([A-Za-z_]\w*)\s*\([^;{}]*" +
                                   re.escape(literal), body))
    if not declarations or re.search(r"get(?:BOOL|S32|U32|F32|String|LLSD|Vector3|Color3|Color4)\s*\(\s*" + re.escape(literal), body):
        return body
    scopes = []
    for declaration in declarations:
        enclosing = []
        for brace in re.finditer(r"\{", body[:declaration.start()]):
            end = _matching_delimiter(body, brace.start(), "{", "}")
            if end > declaration.end():
                enclosing.append((brace.start(), end + 1))
        if len(enclosing) < 2:
            return body
        start, end = enclosing[-1]
        variable = declaration.group(1)
        if re.search(r"\b" + re.escape(variable) + r"\b", body[:start] + body[end:]):
            return body
        # A value copied out of the block needs broader data-flow analysis.
        if re.search(r"[.>\w]+\s*=(?!=)[^;{}]*\b" + re.escape(variable) + r"\b", body[start:end]):
            return body
        scopes.append(body[start:end])
    return "\n".join(dict.fromkeys(scopes))


def _setting_reachability(setting: str, sources: dict[Path, str],
                          body_cache: dict[str, object]) -> tuple[list[tuple[str, Path]], bool, int, int]:
    """Return every literal C++ setting consumer and exact signal listener.

    LLCachedControl reads are consumers, not listeners. They are still scanned
    for reachable hazards, but do not make listener resolution fail.
    """
    cache_key = "@setting:" + setting
    cached = body_cache.get(cache_key)
    if cached is not None:
        return cached  # type: ignore[return-value]
    literal = '"' + setting + '"'
    index_key = "@literal_function_index"
    if index_key not in body_cache:
        definition = re.compile(
            r"^[^\n;{}]*\b(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*\s*"
            r"\([^;{}]*\)\s*(?:const\s*)?\{", re.M)
        index: dict[str, list[tuple[str, Path]]] = defaultdict(list)
        outside: dict[str, set[Path]] = defaultdict(set)
        definitions: list[tuple[str, list[str], str, Path]] = []
        for path, text in sources.items():
            spans: list[tuple[int, int]] = []
            for match in definition.finditer(text):
                start = text.find("{", match.start())
                end = _matching_delimiter(text, start, "{", "}")
                if end < 0:
                    continue
                spans.append((match.start(), end + 1))
                body = text[start:end + 1]
                signature = text[match.start():start]
                name_match = re.search(r"((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\(", signature)
                open_paren = signature.find("(", name_match.start() if name_match else 0)
                close_paren = _matching_delimiter(signature, open_paren) if open_paren >= 0 else -1
                if name_match and close_paren >= 0:
                    definitions.append((name_match.group(1), _split_cpp_args(
                        signature[open_paren + 1:close_paren]), body, path))
                for value in set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"', body)):
                    index[value].append((body, path))
            uncommented = _cpp_without_comments(text)
            for literal_match in re.finditer(r'"([A-Za-z_][A-Za-z0-9_]*)"', uncommented):
                if not any(start <= literal_match.start() < end for start, end in spans):
                    outside[literal_match.group(1)].add(path)
        body_cache[index_key] = index
        body_cache["@literal_outside_function"] = outside
        body_cache["@function_definitions"] = definitions
    literal_index: dict[str, list[tuple[str, Path]]] = body_cache[index_key]  # type: ignore[assignment]
    definitions = body_cache["@function_definitions"]
    bodies: list[tuple[str, Path]] = []
    listener_bodies: list[tuple[str, Path]] = []
    listeners = 0
    consumers = 0
    dynamic_listener_seen = False
    # Static tables/member initializers are not executable setting callbacks.
    # Reachability is defined by function bodies that read the setting; only an
    # unresolved signal connection in such a body fails this scan closed.
    resolved = True
    literal_callers = list(literal_index.get(setting, [])) + _setting_table_listeners(sources, body_cache).get(setting, [])
    for body, path in literal_callers:
        # Binding a widget or writing a default is not a setting consumer.
        # Read sites and signal callbacks are executable consumers; a postBuild
        # string must not donate every neighbouring handler.
        literal_read = re.search(
            r"get(?:BOOL|S32|U32|F32|String|LLSD|Vector3|Vector3d|Color3|Color4|Rect)\s*\(\s*" +
            re.escape(literal) + r"|LLCachedControl\s*<[^>]+>[^;{}]*" + re.escape(literal), body)
        setup_only = re.search(r"set(?:Commit|Clicked)Callback\s*\(|childSet(?:Action|CommitCallback)\s*\(", body)
        if literal_read and not setup_only and "selftest" not in path.name.lower():
            consumers += 1
            bodies.append((_setting_reader_scope(body, literal), path))
        listener_matches: list[re.Match[str]] = []
        direct = re.compile(
            r"getControl\s*\(\s*" + re.escape(literal) +
            r"\s*\)\s*->\s*get(?:Commit)?Signal\s*\(\s*\)\s*->\s*connect\s*\(")
        listener_matches.extend(direct.finditer(body))
        aliases = set(re.findall(
            r"\b([A-Za-z_]\w*)\s*=\s*[^;{}]*getControl\s*\(\s*" +
            re.escape(literal) + r"\s*\)", body))
        for alias in aliases:
            listener_matches.extend(re.finditer(
                r"\b" + re.escape(alias) +
                r"\s*->\s*get(?:Commit)?Signal\s*\(\s*\)\s*->\s*connect\s*\(", body))
        exact = len(listener_matches)
        listeners += exact
        for listener_match in listener_matches:
            call = _callback_call(body, listener_match)
            if not call:
                resolved = False
                continue
            bodies.append((call, path))
            listener_bodies.append((call, path))
            callback_resolved = bool(re.search(r"connect\s*\(\s*\[", call))
            for candidate in reversed(re.findall(
                    r"&?((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)", call)):
                if candidate in {"boost", "bind", "std", "connect", "getSignal",
                                 "getCommitSignal", "getControl", "this", "true", "false"}:
                    continue
                callback_body, callback_path = _source_body(sources, candidate, path, body_cache)
                if callback_body:
                    bodies.append((callback_body, callback_path or path))
                    listener_bodies.append((callback_body, callback_path or path))
                    callback_resolved = True
                    break
                local = re.search(r"\b" + re.escape(candidate) + r"\s*=\s*\[", body)
                if local:
                    brace = body.find("{", local.end())
                    end = _matching_delimiter(body, brace, "{", "}")
                    if end >= 0:
                        listener_bodies.append((body[local.start():end + 1], path))
                        bodies.append((body[local.start():end + 1], path))
                        callback_resolved = True
                        break
            if not callback_resolved:
                resolved = False
        # A function containing this setting and a signal connection that
        # cannot be tied to the literal/alias is a dynamic listener table.
        # It may include this setting, so fail closed.
        if re.search(r"get(?:Commit)?Signal\s*\(\s*\)\s*->\s*connect\s*\(", body) and not exact and re.search(
                r"getControl\s*\(\s*([A-Za-z_]\w*)", body):
            source_start = sources[path].find(body)
            dynamic_calls = list(re.finditer(r"getControl\s*\(\s*([A-Za-z_]\w*)", body))
            if any(source_start < 0 or not any(
                    scope_path == path and variable == match.group(1) and
                    start <= source_start + match.start() < end
                    for scope_path, start, end, variable in body_cache.get("@setting_table_scopes", []))
                   for match in dynamic_calls):
                dynamic_listener_seen = True
    # Listener helpers accept a setting name, connect its signal internally,
    # and receive the callback at the call site.  Attribute the literal call to
    # the helper and scan the bound callback; unresolved callbacks deny.
    helper_key = "@setting_listener_helpers"
    if helper_key not in body_cache:
        helper_index: dict[str, list[tuple[int, list[int], str, Path]]] = defaultdict(list)
        for qualified, params, helper_body, helper_path in definitions:
            if not re.search(r"get(?:Commit)?Signal\s*\(\s*\)\s*->\s*connect\s*\(", helper_body):
                continue
            param_names = [next(iter(re.findall(r"([A-Za-z_]\w*)\s*(?:[=&*]\s*)?$", p)), "")
                           for p in params]
            for setting_index, param in enumerate(param_names):
                if param and re.search(r"(?:getControl|setting_get_control)\s*\([^;{}]*\b" +
                                       re.escape(param) + r"\b", helper_body):
                    callback_indices = [i for i, p in enumerate(params)
                                        if i != setting_index and re.search(r"callback|slot|function", p, re.I)]
                    helper_index[qualified.rsplit("::", 1)[-1]].append(
                        (setting_index, callback_indices, helper_body, helper_path))
        body_cache[helper_key] = helper_index
    helper_defs = body_cache[helper_key]
    for caller_body, caller_path in literal_callers:
        for helper, variants in helper_defs.items():
            for call_match in re.finditer(r"\b" + re.escape(helper) + r"\s*\(", caller_body):
                opening = caller_body.find("(", call_match.start())
                closing = _matching_delimiter(caller_body, opening)
                if closing < 0:
                    resolved = False
                    continue
                args = _split_cpp_args(caller_body[opening + 1:closing])
                for setting_index, callback_indices, helper_body, helper_path in variants:
                    if setting_index >= len(args) or args[setting_index].strip() != literal:
                        continue
                    listeners += 1
                    bodies.append((helper_body, helper_path))
                    listener_bodies.append((helper_body, helper_path))
                    for callback_index in callback_indices:
                        if callback_index >= len(args):
                            resolved = False
                            continue
                        callback_arg = args[callback_index]
                        candidates = re.findall(
                            r"&?((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)", callback_arg)
                        ignored = {"boost", "bind", "std", "function", "this", "true", "false"}
                        callback_bodies = []
                        for candidate in reversed(candidates):
                            if candidate in ignored:
                                continue
                            callback_body, callback_path = _source_body(
                                sources, candidate, caller_path, body_cache)
                            if callback_body:
                                callback_bodies.append((callback_body, callback_path or caller_path))
                                break
                        if callback_bodies:
                            bodies.extend(callback_bodies)
                            listener_bodies.extend(callback_bodies)
                        elif callback_arg.lstrip().startswith("["):
                            bodies.append((callback_arg, caller_path))
                            listener_bodies.append((callback_arg, caller_path))
                        else:
                            resolved = False
    if dynamic_listener_seen and listeners == 0:
        resolved = False
    result = (bodies, resolved, listeners, consumers)
    body_cache["@setting_listener_bodies:" + setting] = listener_bodies
    body_cache[cache_key] = result
    return result


def _cpp_if_branches(text: str) -> list[tuple[str, str]]:
    """Return condition/body pairs without admitting neighbouring else arms."""
    branches: list[tuple[int, str, str]] = []
    for match in re.finditer(r"\b(?:if|else\s+if)\s*\(", text):
        opening = text.find("(", match.start(), match.end())
        closing = _matching_delimiter(text, opening)
        if closing < 0:
            continue
        start = closing + 1
        while start < len(text) and text[start].isspace():
            start += 1
        if start >= len(text):
            continue
        if text[start] == "{":
            end = _matching_delimiter(text, start, "{", "}")
            if end < 0:
                continue
            body = text[start:end + 1]
        else:
            end = text.find(";", start)
            if end < 0:
                continue
            body = text[start:end + 1]
        depth = text.count("{", 0, match.start()) - text.count("}", 0, match.start())
        branches.append((depth, text[opening + 1:closing], body))
    if not branches:
        return []
    top_depth = min(depth for depth, _condition, _body in branches)
    return [(condition, body) for depth, condition, body in branches
            if depth == top_depth]


def _vcam_dispatch_branch(body: str, widget: str,
                          action: str = "") -> tuple[str, bool]:
    """Select only one VCam name/action arm from an onControl-style body."""
    name_literal = re.escape(widget)
    named: list[tuple[str, str]] = []
    for condition, branch in _cpp_if_branches(body):
        if re.search(r'(?:\bname\s*==\s*"' + name_literal +
                     r'"|"' + name_literal + r'"\s*==\s*\bname)', condition):
            named.append((condition, branch))
    if action:
        action_literal = re.escape(action)
        exact = [(condition, branch) for condition, branch in named
                 if re.search(r'==\s*"' + action_literal + r'"', condition)]
        if exact:
            return exact[0][1], len(exact) == 1
    if len(named) == 1:
        selected = named[0][1]
        if widget in {"vc_monitor_header", "vc_monitor_strip"}:
            if not action:
                return "", False
            nested = [(condition, branch) for condition, branch in _cpp_if_branches(selected)
                      if re.search(r'==\s*"' + re.escape(action) + r'"', condition)]
            return (nested[0][1], True) if len(nested) == 1 else ("", False)
        return selected, True
    if len(named) > 1:
        return "", False

    # The tail of onControl routes families to a second exact name dispatch.
    route = "commitCamera"
    if widget.startswith("vc_proj_"):
        route = "commitProjector"
    elif widget.startswith(("vc_tv_", "vc_face_", "vc_anchor_", "vc_bar_", "vc_screen_")):
        route = "commitDisplay"
    route_match = re.search(r"\b" + route + r"\s*\(\s*name\s*,\s*value\s*\)\s*;", body)
    return (route_match.group(0), True) if route_match else ("", False)


def _handler_hazards(row: dict[str, object], cpp_sources: dict[Path, str],
                     body_cache: dict[str, object]) -> tuple[set[str], bool]:
    """Resolve this control's exact callback and scan it fail-closed."""
    if row.get("exercise") == "readout":
        return set(), True
    if row.get("probe") == "launcher" and row.get("launcher_floater") in {
            "lightbox", "exposure_hub", "ultimate_diopter", "env_adjust_snapshot"}:
        row["inworld_callback_evidence"] = "ALKitRow::postBuild / registered settings editor launcher"
        return set(), True
    if row.get("link_to") and row.get("owner") == "ALKitRow::postBuild":
        row["inworld_callback_evidence"] = "alkitrow.cpp:postBuild link_to callback"
        return set(), True
    owner = str(row.get("owner") or "")
    widget = str(row.get("widget_name") or "")
    bodies: list[tuple[str, Path | None]] = []
    resolved = False
    callback = str(row.get("callbacks") or "")
    owner_body, owner_source = _source_body(cpp_sources, owner, cache=body_cache) if owner else ("", None)
    if owner and owner_body:
        body_cache.setdefault(owner, owner_body)
    owner_class = owner.split("::", 1)[0] if "::" in owner else ""

    if owner == "ALPanelDirectorVCam::onControl" and owner_body:
        branch, resolved = _vcam_dispatch_branch(
            owner_body, widget, str(row.get("dispatch_action") or ""))
        if branch:
            bodies.append((branch, owner_source))
            if owner_source:
                offset = cpp_sources[owner_source].find(branch)
                if offset >= 0:
                    row["inworld_callback_evidence"] = (
                        f"{owner_source.name}:{cpp_sources[owner_source].count(chr(10), 0, offset) + 1}")

    registration_text = cpp_sources.get(owner_source, "") if owner_source else owner_body
    if not bodies and registration_text and (not widget or f'"{widget}"' in registration_text):
        at = registration_text.find(f'"{widget}"')
        direct_start = registration_text.rfind("getChild", 0, at + 1)
        legacy_window = registration_text[
            max(0, direct_start if direct_start >= 0 else at - 160):at + 2000]
        registrations: list[str] = []

        # Direct literal registration: getChild("this-control")->set*Callback(...).
        direct_re = re.compile(
            r"(?:getChild|findChild)(?:<[^>]+>)?\s*\(\s*\"" + re.escape(widget) +
            r"\"\s*(?:,\s*(?:true|false))?\s*\)\s*->\s*set(?:Commit|Clicked)Callback\s*\(")
        registrations.extend(filter(None, (_callback_call(registration_text, match)
                                             for match in direct_re.finditer(registration_text))))

        # Named/member control registration. Array members deliberately match by
        # base name (mCenter[VX] -> mCenter[axis]); the callback is shared by the
        # whole array, so this remains exact for the named control.
        assignment_re = re.compile(
            r"([A-Za-z_]\w*(?:\s*\[[^\]]+\])?)\s*=\s*(?:[^;{}]*?)"
            r"(?:getChild|findChild)(?:<[^>]+>)?\s*\(\s*\"" + re.escape(widget) + r"\"\s*(?:,\s*(?:true|false))?\s*\)")
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
            # Actor FX and similar binders attach one callback to an explicit
            # initializer-list of member controls. Resolve membership by name.
            for shared in re.finditer(
                    r"for\s*\(\s*LLUICtrl\s*\*\s*(\w+)\s*:\s*\{([^}]+)\}\s*\)", registration_text):
                if not re.search(r"\b" + re.escape(base) + r"\b", shared.group(2)):
                    continue
                brace = registration_text.find("{", shared.end())
                end = _matching_delimiter(registration_text, brace, "{", "}")
                scope = registration_text[brace:end + 1] if end >= 0 else ""
                shared_re = re.compile(r"\b" + re.escape(shared.group(1)) +
                                       r"\s*->\s*set(?:Commit|Clicked)Callback\s*\(")
                registrations.extend(filter(None, (_callback_call(scope, match)
                                                     for match in shared_re.finditer(scope))))

        # Legacy helper APIs bind the literal control name in the same call.
        helper_re = re.compile(
            r"\b(?:childSetAction|childSetCommitCallback)\s*\(\s*\"" +
            re.escape(widget) + r"\"\s*,")
        registrations.extend(filter(None, (_callback_call(registration_text, match)
                                             for match in helper_re.finditer(registration_text))))

        # Local binder lambdas receive the exact widget name as an argument.
        # Scan that helper's callback body, never a proximity window of neighbours.
        if not registrations and owner_body:
            helper_call_re = re.compile(r"\b([A-Za-z_]\w*)\s*\(\s*\"" + re.escape(widget) + r"\"\s*[,)]")
            for helper_call in helper_call_re.finditer(owner_body):
                helper = helper_call.group(1)
                declarations = list(re.finditer(r"\b" + re.escape(helper) + r"\s*=\s*\[",
                                                owner_body[:helper_call.start()]))
                if not declarations:
                    continue
                brace = owner_body.find("{", declarations[-1].end())
                end = _matching_delimiter(owner_body, brace, "{", "}")
                helper_body = owner_body[brace:end + 1] if end >= 0 else ""
                if re.search(r"set(?:Commit|Clicked)Callback\s*\(", helper_body):
                    registrations.append(helper_body)

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
            # The structural control_name binding is the commit path when no
            # callback registration targets this widget. Unrelated callbacks in
            # its page builder do not invalidate that binding.
            resolved = row.get("setting_source") == "xui control_name" or (
                "setCommitCallback" not in owner_body and "setClickedCallback" not in owner_body)
        else:
            # Exact resolution failed: retain the old conservative union of
            # every handler in the whole window. Never select only the first.
            handlers = set(re.findall(r"&([A-Za-z_]\w*(?:::[A-Za-z_]\w*)+)", legacy_window))
            if owner_class:
                handlers.update(owner_class + "::" + name for name in
                                re.findall(r"\b(on[A-Z][A-Za-z0-9_]*)\s*\(", legacy_window))
            bodies.append((legacy_window, owner_source))
            for handler in handlers:
                body, source = _source_body(cpp_sources, handler, owner_source, body_cache)
                if body:
                    bodies.append((body, source))
    if callback:
        body, source = _source_body(cpp_sources, callback, owner_source, body_cache)
        if body:
            if not resolved:
                bodies.clear()  # An exact callback supersedes the neighbour fallback.
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
                        if not resolved:
                            bodies.clear()
                        resolved = True
                        bodies.append((call, source_path))
    if (not resolved and row.get("setting") and not callback and not owner):
        # A plain control_name binding has no owner handler: LLUICtrl commits
        # directly to the viewer-local setting. This is a resolved local sink,
        # not an unknown callback.
        resolved = True

    setting = str(row.get("setting") or "")
    setting_listener_resolved = True
    setting_listeners = 0
    setting_consumers = 0
    setting_bodies: list[tuple[str, Path]] = []
    if setting:
        setting_bodies, setting_listener_resolved, setting_listeners, setting_consumers = \
            _setting_reachability(setting, cpp_sources, body_cache)
        if owner_class == "ALPanelDirectorVCam":
            scoped_setting_bodies: list[tuple[str, Path]] = []
            for setting_body, setting_path in setting_bodies:
                if re.search(r"\b(?:const\s+)?std::string\s+name\b", setting_body):
                    setting_branch, branch_found = _vcam_dispatch_branch(
                        setting_body, widget, str(row.get("dispatch_action") or ""))
                    if branch_found:
                        scoped_setting_bodies.append((setting_branch, setting_path))
                    else:
                        setting_listener_resolved = False
            setting_bodies = scoped_setting_bodies
        row["inworld_setting_listener_count"] = setting_listeners
        row["inworld_setting_consumer_count"] = setting_consumers
        row["setting_consumer_evidence"] = [
            {"file": path.name, "line": cpp_sources[path].count("\n", 0, max(0, cpp_sources[path].find(body))) + 1}
            for body, path in setting_bodies]
        row["inworld_setting_listener_resolved"] = setting_listener_resolved
        if not setting_listener_resolved:
            row["setting_listener_unknown"] = True
        resolved = resolved and setting_listener_resolved
        row["setting_sole_sink"] = bool(
            resolved and setting_listeners == 0 and setting_consumers == 0 and
            not callback and "setCommitCallback" not in owner_body and
            "setClickedCallback" not in owner_body)

    # LOCAL and NETWORK hazards belong to code reachable from this row: its
    # exact callback/binder or setting consumers, plus three call levels.
    called_paths: set[Path] = set()
    unresolved_member_calls: set[str] = set()
    callback_reachable = _expand_callback_calls(
        bodies + body_cache.get("@setting_listener_bodies:" + setting, []),
        owner_class, cpp_sources, body_cache,
        depth_limit=3, reached_paths=called_paths,
        unresolved_member_calls=unresolved_member_calls,
        dispatch_widget=widget if owner_class == "ALPanelDirectorVCam" else "")
    # A renderer read is not a commit callback. Follow explicit/static and
    # same-file helper calls without guessing unrelated receiver types by name.
    callback_reachable += _expand_callback_calls(
        setting_bodies, "", cpp_sources, body_cache, depth_limit=3,
        reached_paths=called_paths, resolve_members=False)
    if unresolved_member_calls:
        row["unresolved_member_calls"] = sorted(unresolved_member_calls)
        # The LLControl binding is a known sink. Ambiguous renderer accessors
        # do not make that pure setting commit an unknown handler.
        if owner or callback or not setting or not setting_listener_resolved or setting_listeners:
            resolved = False
    # Setting consumers are already complete function bodies. Listener callback
    # bodies are added by _setting_reachability; do not fan their incidental
    # calls out through unrelated same-name helpers.
    reachable_seed = bodies + setting_bodies
    reachable = "\n".join(callback_reachable)

    # Resolved rows use their callback/consumer paths. For unresolved rows,
    # retain the owning translation units' NETWORK union as a conservative
    # diagnostic; the unresolved-handler policy will keep them excluded.
    reachable_paths = {path for _body, path in reachable_seed if path is not None} | called_paths
    hazards: set[str] = set()
    direct_hazards: set[str] = set()
    for tag, pattern in HAZARD_RULES.items():
        if re.search(pattern, reachable):
            hazards.add(tag)
            direct_hazards.add(tag)
    row["direct_hazards"] = sorted(direct_hazards)
    row["direct_login_blocking_hazards"] = sorted(
        tag for tag, pattern in LOGIN_ONLY_BLOCKER_RULES.items()
        if re.search(pattern, reachable))
    if not resolved:
        for path in reachable_paths:
            cache_key = "@network_tu_hazards:" + str(path)
            if cache_key not in body_cache:
                source = cpp_sources[path]
                body_cache[cache_key] = frozenset(
                    tag for tag in NETWORK_HAZARDS
                    if tag in HAZARD_RULES and re.search(HAZARD_RULES[tag], source))
            hazards.update(body_cache[cache_key])
    # A virtual avatar call can dispatch gAgent.sendAnimationRequest when the
    # self avatar's AO overrides the motion.  Qualified LLCharacter calls are
    # deliberately not matched.
    if re.search(r"->\s*(?:startMotion|stopMotion)\s*\(", reachable):
        hazards.add("agent_send")
        row["motion_send_hazard"] = True
    if setting and resolved:
        hazards.discard("persistent_setting")  # Restored by the existing LLControl snapshot.
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
            "setting referenced from a table/initializer outside a function"
            if row.get("setting_reference_outside_function") else
            "resolved handler hazard: " + ", ".join(sorted(hazards)))


def _apply_unresolved_handler_policy(row: dict[str, object], hazards: set[str]) -> None:
    """Round-8 classifier: login may run; in-world always denies unresolved."""
    row["handler_scan_resolved"] = False
    row["inworld_allowed"] = False
    row["inworld_allow_evidence"] = "unresolved handler reachability"
    row["inworld_skip_reason"] = "local reachability unresolved"
    login_blockers = _login_blocking_hazards(row, hazards)
    row["login_blocking_hazards"] = sorted(login_blockers)
    if login_blockers:
        row["action"] = "SKIP"
        row["reason"] = _login_block_reason(login_blockers)
    elif row.get("action") == "RUN":
        row["login_policy_run"] = True
        row["reason"] = ""
    row["known_gap"] = False


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
        if row.get("handler_scan_resolved") and not (
                set(row.get("hazards", [])) & (NETWORK_HAZARDS | LOCAL_HAZARDS)):
            row["inworld_new_row_evidence"] = row.get("inworld_callback_evidence") or row.get("evidence") or row.get("setting")
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


def prepare_manifest(path: Path, repo: Path, output: Path, progress=None) -> tuple[int, int, int, int]:
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
    cpp_sources = {p: _cpp_without_comments(p.read_text(encoding="utf-8", errors="replace"))
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

    for row_index, row in enumerate(rows):
        if progress and row_index % 250 == 0:
            progress(row_index, len(rows))
        if row.get("declared_non_interactive"):
            row.update(action="SKIP", inworld_allowed=False, enabled_control=[])
            continue
        kit_row = row.get("resolution") == "generated_kit_xml"
        kit_owner = row.get("owner")
        kit_callbacks = row.get("callbacks")
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
        row["reveal"] = list(row.get("reveal", [])) + reveal

        widget = row.get("widget_name", "")
        path_owner_controls = {
            "btn_wp_add", "btn_wp_insert", "btn_edit_undo", "btn_edit_redo",
            "btn_edit_reverse", "btn_edit_mirror", "btn_edit_loopclose",
            "btn_play_path", "btn_stop_path", "btn_walk_here", "btn_copy_to",
            "node_height_spinner", "node_dwell_spinner", "node_speed_spinner",
            "node_corner_radius_spinner", "btn_node_setcam",
            "node_cam_transition_combo", "btn_node_preview",
            "path_groundfollow_check", "path_pitch_check", "btn_primitive_center",
            "btn_fit_half_arc", "btn_fit_sine", "btn_stop_following",
        }
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
            reveal_settings.append({"setting": "CineDiopterEnabled", "value": True,
                                    "source": "ALDiopterRelevance master gate"})
            reveal_driver = ("CineDiopterToolMode", 0)
        elif widget == "diopter_power":
            reveal_settings.append({"setting": "CineDiopterEnabled", "value": True,
                                    "source": "ALDiopterRelevance master gate"})
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
        elif widget in {"brigade_columns_spinner", "brigade_rows_spinner",
                        "brigade_file_spacing_spinner", "brigade_rank_spacing_spinner"}:
            row["reveal"].append({"selector": "array_formation_combo", "value": 31})
        code_settings = {
            "vc_main_fps": "PrismProtectedMainFPS",
            "vc_capture_budget": "PrismCaptureRefreshCeilingHz",
            "dof_res_combo": "CameraDoFResScale",
        }
        if widget in code_settings:
            setting = code_settings[widget]
            expected_value = row.get("test_value_resolved")
            if expected_value is None:
                test_value = row.get("test_value")
                expected_value = (test_value.get("value")
                                  if isinstance(test_value, dict) else test_value)
            row["setting"] = setting
            row["setting_type"] = "F32"
            row["expect"] = {
                "setting": setting, "type": "F32",
                "value": expected_value,
                "callback": ("ALPanelDirectorVCam::onControl"
                             if widget.startswith("vc_") else
                             "LLFloaterDirector::bindFocusPage"),
                "commit": True,
            }
            row["owner"] = row["expect"]["callback"]
        if widget == "vc_source_type":
            # The local test-pattern source exercises a real alternate without object selection.
            row["test_value"] = {"item_index": 3}
        if widget == "vc_camera_name":
            row.update(owner="ALPanelDirectorVCam::onControl",
                       probe="vcam_camera_name", known_gap=False)
            row.pop("effect_kind", None)
        if widget.startswith("switcher_custom_") and widget != "switcher_custom_enabled":
            row.update(probe="switcher_bank", known_gap=False)
            row.pop("effect_kind", None)
        if widget == "cine_easy_flare_preset":
            row["test_value"] = {"item_index": 2, "resolve": "runtime"}
            row["setting"] = "RenderCineLensFlarePreset"
            row["setting_type"] = "S32"
            row["expect"] = {"setting": "RenderCineLensFlarePreset", "type": "S32",
                             "value": {"resolve": "runtime"}, "callback": "ALPanelCineLightRig::postBuild",
                             "commit": True}
            row["oracle_pre_value"] = 0
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
        if row.get("page") == "exposure_tab" and widget in {"exp_filters_btn", "exp_sky_lighting_btn"}:
            row["probe"] = "launcher"
            row["launcher_floater"] = "env_adjust_snapshot" if widget == "exp_sky_lighting_btn" else "lightbox"
            row["known_gap"] = False
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
        if row.get("page") == "dance_tab" and (
                row.get("action") == "DEFER_INWORLD" or widget == "db_t_speed"):
            row.update(probe="dance_owner", known_gap=False)
            row.pop("no_state_oracle", None)
        if row.get("page") == "props_tab" and widget in {
                "btn_enroll", "prop_list", "node_list", "btn_node_add",
                "btn_drive", "btn_stop", "btn_stop_all"}:
            row.update(probe="prop_owner", known_gap=False)
            row.pop("no_state_oracle", None)
        if row.get("page") == "path_tab" and widget in path_owner_controls:
            row.update(probe="path_owner", known_gap=False)
            row.pop("no_state_oracle", None)
        if widget == "waypoint_list":
            row.update(probe="path_selection", known_gap=False)
            row.pop("no_state_oracle", None)
        if widget in {"show_all_check", "edit_ghosts_check"}:
            row.update(probe="ghost_owner", known_gap=False)
            row.pop("no_state_oracle", None)
        if widget == "gaze_slot_you":
            row.update(probe="gaze_slot", known_gap=False)
            row.pop("no_state_oracle", None)
        if widget in {"gaze_cast_combo", "reset_gaze_target"}:
            row.update(probe="gaze_cast_target", known_gap=False,
                       gaze_cast_mode=1 if widget == "reset_gaze_target" else 2)
            row.pop("no_state_oracle", None)
        if widget == "localfog_volume_list":
            row.update(probe="list_owner", known_gap=False)
            row.pop("no_state_oracle", None)
        if row.get("page") == "takes_tab" and widget == "btn_stop":
            row.update(probe="flycam_stop", known_gap=False)
            row.pop("no_state_oracle", None)
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

        login_destructive_candidate = (
            row.get("action") == "SKIP" and
            str(row.get("reason") or "").startswith("destructive ("))
        login_destructive_reason = str(row.get("reason") or "")
        if row.get("exercise") not in {"", "none", None} and (
                row.get("action") not in {"SKIP", "NO_EXERCISE", "EXPECT_MISSING"} or
                login_destructive_candidate):
            if kit_row:
                # Functional probes may add oracles/settings, but generated kit
                # ownership is immutable: never re-import legacy CSV evidence.
                row["owner"] = kit_owner
                row["callbacks"] = kit_callbacks
            legacy_button_scan = (row.get("exercise") == "click" and
                                  row.get("widget_type") == "button")
            hazards, handler_resolved = _handler_hazards(row, cpp_sources, body_cache)
            row["handler_scan_resolved"] = handler_resolved
            inworld_allowed, inworld_evidence = _inworld_local_effect(row)
            row["inworld_allowed"] = inworld_allowed
            row["inworld_allow_evidence"] = inworld_evidence
            if row.get("setting") and not handler_resolved:
                row["inworld_allowed"] = False
                row["inworld_allow_evidence"] = (
                    "setting referenced from a table/initializer outside a function"
                    if row.get("setting_reference_outside_function") else
                    "setting callback/listener scan unresolved")
            if hazards:
                row["hazards"] = sorted(set(row.get("hazards", [])) | hazards)
                login_blockers = _login_blocking_hazards(row, set(row["hazards"]))
                row["login_blocking_hazards"] = sorted(login_blockers)
                row["hazard_classes"] = _hazard_classes(set(row["hazards"]))
                hazard_rows += 1
                # Hazardous callbacks are never authorized in-world, including
                # sliders/spinners and already-deferred rows. Login-screen
                # coverage may remain executable for non-button controls.
                _apply_inworld_hazard_policy(row, hazards)
                if not handler_resolved:
                    _apply_unresolved_handler_policy(row, set(row["hazards"]))
                    unresolved_hazards.append(f"{row['version']}:{row['page']}:{widget}")
                elif ((row.get("action") == "RUN" and legacy_button_scan) or
                        row.get("motion_send_hazard")):
                    hazard_reason = (
                        "in-world: may send animation request via AO override "
                        "(llvoavatar.cpp:7398-7413,7442-7457; "
                        "aldirectoranimswitcher.cpp:414,418,565; "
                        "llfloaterdirector.cpp:3726,3741)"
                        if row.get("motion_send_hazard") else
                        "resolved handler hazard: " + ", ".join(sorted(hazards)))
                    row["inworld_skip_reason"] = hazard_reason
                    if login_blockers:
                        row["action"] = "SKIP"
                        row["reason"] = _login_block_reason(login_blockers)
                    else:
                        row["login_policy_run"] = True
                        row["reason"] = ""
                    row["known_gap"] = False
            elif not handler_resolved:
                _apply_unresolved_handler_policy(row, set(row.get("hazards", [])))
                unresolved_hazards.append(f"{row['version']}:{row['page']}:{widget}")
            if login_destructive_candidate:
                policy_hazards = set(row.get("hazards", []))
                login_blockers = _login_blocking_hazards(row, policy_hazards)
                row["login_blocking_hazards"] = sorted(login_blockers)
                row["inworld_allowed"] = False
                row["inworld_allow_evidence"] = login_destructive_reason
                row["inworld_skip_reason"] = login_destructive_reason
                if login_blockers:
                    row["action"] = "SKIP"
                    row["reason"] = _login_block_reason(login_blockers)
                else:
                    row["action"] = "RUN"
                    row["reason"] = ""
                    row["login_policy_run"] = True
            elif (handler_resolved and row.get("action") in {"RUN", "DEFER_INWORLD"} and
                  row.get("exercise") != "readout" and
                  not row.get("setting") and not row.get("consumer") and
                  not row.get("reset_defaults") and
                  row.get("probe") not in {
                      "selector_segment", "temporal_preset", "gaze_reset",
                      "cine_fx_stop", "cine_live_probe_refresh", "vc_snap_view",
                      "vc_face_list", "vc_camera_selector", "actor_mover_active",
                      "actor_style_reset", "exposure_history", "launcher",
                      "dance_owner", "prop_owner", "path_owner", "path_selection", "ghost_owner",
                      "gaze_slot", "gaze_cast_target", "list_owner",
                      "flycam_stop",
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


def classification_counts(document, inworld, version="v2"):
    """Static execution decisions; fixture availability is reported at runtime."""
    counts = Counter()
    excluded_settings = []
    local_defer_run = 0
    for row in document.get("entries", []):
        if row.get("version") != version:
            continue
        action = row.get("action")
        reason = ""
        hazards = set(row.get("hazards", []))
        if inworld and row.get("inworld_skip_reason"):
            reason = row["inworld_skip_reason"]
        elif inworld and row.get("inworld_known_gap_reason"):
            reason = row["inworld_known_gap_reason"]
        elif action in {"NO_EXERCISE", "EXPECT_MISSING"} or (
                action == "SKIP" and (inworld or not row.get("login_policy_run"))):
            reason = row.get("reason") or action
        elif inworld and (not row.get("inworld_allowed") or
                          row.get("widget_name") in INWORLD_EXCLUDED_CONTROLS):
            reason = row.get("inworld_allow_evidence") or "in-world: not on the viewer-local allow-list"
        elif inworld and hazards & (NETWORK_HAZARDS | LOCAL_HAZARDS | {"handler_unknown"}):
            reason = "in-world: handler hazard: " + ", ".join(sorted(
                hazards & (NETWORK_HAZARDS | LOCAL_HAZARDS | {"handler_unknown"})))
        elif not inworld and _login_blocking_hazards(row, hazards):
            reason = row.get("reason") or _login_block_reason(_login_blocking_hazards(row, hazards))
        elif not inworld and (action == "DEFER_INWORLD" or row.get("page") == "path_tab"):
            reason = "deferred: world fixture required"
        counts["SKIP: " + reason if reason else "RUN"] += 1
        if inworld and not reason and action == "DEFER_INWORLD":
            local_defer_run += 1
        if inworld and reason and row.get("setting") and hazards & NETWORK_HAZARDS:
            excluded_settings.append({"setting": row["setting"], "widget": row["widget_name"],
                                      "reason": reason,
                                      "evidence": row.get("setting_consumer_evidence") or row.get("inworld_callback_evidence") or row.get("evidence")})
    return {"mode": "in-world" if inworld else "login", "version": version,
            "RUN": counts.pop("RUN", 0), "SKIP": sum(counts.values()),
            "viewer_local_DEFER_INWORLD_RUN": local_defer_run,
            "skip_by_reason": dict(sorted(counts.items())),
            "excluded_server_settings": excluded_settings}


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
    bad_warn = [row for row in rows
                if row["PASS/FAIL/SKIP"] == "WARN" and not row["reason"].strip()]
    if bad_warn:
        raise SystemExit(f"{path}: {len(bad_warn)} WARN row(s) lack an explicit reason")
    unknown = sorted({row["PASS/FAIL/SKIP"] for row in rows}
                     - {"PASS", "FAIL", "SKIP", "KNOWN_GAP", "WARN"})
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


RULE_TITLES = {
    "R1_REACHABILITY": "Every page and link is reachable through the real navigation code",
    "R2_POPOVERS": "Every shell popover opens correctly and closes both ways",
    "R3_LABEL_FIT": "No visible label is clipped (elided labels are WARN)",
    "R4_RAW_NAMES": "No internal id is shown to the user",
    "R5_THEMED_PAGES": "Every Director page is a themed kit page (known-legacy pages are WARN)",
    "R6_CONNECTED": "Every interactive kit control has an observable effect",
}


def rule_rows_of(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    return [row for row in rows if row.get("kind") == "rule"]


def _row_items(row: dict[str, str]) -> int:
    try:
        return max(1, int(json.loads(row["observed"]).get("items", 1)))
    except (TypeError, ValueError, json.JSONDecodeError):
        return 1


def rules_section(rows: list[dict[str, str]], require: bool) -> tuple[list[str], bool]:
    """Per-rule PASS/FAIL/WARN counts and per-item lines for the Director rule engine."""
    rule_rows = rule_rows_of(rows)
    lines = ["## Director rule checks", ""]
    missing: list[str] = []
    if require and any(row["ui_version"] == "v2" for row in rows):
        seen = {row["action"] for row in rule_rows}
        missing = [rule for rule in RULE_TITLES if rule not in seen]
    failed = bool(missing)
    if not rule_rows:
        lines.append("- No rule rows in this run." +
                     (" REQUIRED rules missing: " + ", ".join(missing) if missing else ""))
        lines.append("")
        return lines, failed
    by_rule: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rule_rows:
        by_rule[row["action"]].append(row)
    lines += ["| Rule | PASS | FAIL | WARN | SKIP | Items checked |", "|---|---:|---:|---:|---:|---:|"]
    for rule in sorted(by_rule):
        group = by_rule[rule]
        status = Counter(row["PASS/FAIL/SKIP"] for row in group)
        items = sum(_row_items(row) for row in group if row["PASS/FAIL/SKIP"] == "PASS") + \
            status["FAIL"] + status["WARN"] + status["SKIP"]
        lines.append(f"| {rule} | {status['PASS']} | {status['FAIL']} | {status['WARN']} | "
                     f"{status['SKIP']} | {items} |")
    for rule in sorted(set(RULE_TITLES) - set(by_rule)):
        lines.append(f"| {rule} | 0 | 0 | 0 | 0 | 0 (did not run) |")
    lines.append("")
    if missing:
        lines += ["REQUIRED rules missing from this run: " + ", ".join(missing), ""]
    for rule in sorted(by_rule):
        group = by_rule[rule]
        lines += [f"### {rule} - {RULE_TITLES.get(rule, '')}".rstrip(" -"), ""]
        for status in ("FAIL", "WARN", "SKIP"):
            for row in group:
                if row["PASS/FAIL/SKIP"] != status:
                    continue
                reason = row["reason"].replace("\n", " ").replace("|", "/")
                lines.append(f"- {status} `{row['page']}` / `{row['control']}`: {reason}")
        passes = [row for row in group if row["PASS/FAIL/SKIP"] == "PASS"]
        if passes:
            lines.append(f"- PASS: {len(passes)} check(s) over {sum(_row_items(r) for r in passes)} item(s)")
        lines.append("")
    legacy = [row for row in by_rule.get("R5_THEMED_PAGES", [])
              if row["control"] == "legacy (not themed)"]
    if legacy:
        lines += ["### Legacy (not themed) pages", "", "| Page | Plain widgets | Detail |", "|---|---:|---|"]
        for row in sorted(legacy, key=lambda r: r["page"]):
            lines.append(f"| {row['page']} | {_row_items(row)} | {row['reason'].replace('|', '/')} |")
        lines.append("")
    effects: Counter[str] = Counter()
    for row in by_rule.get("R6_CONNECTED", []):
        by_effect = json.loads(row["observed"]).get("by_effect", {})
        if isinstance(by_effect, dict):
            for name, count in by_effect.items():
                effects[name] += int(count)
    if effects:
        lines += ["### Connectedness: effect proven per interactive kit control", "",
                  "| Effect | Controls |", "|---|---:|"]
        for name, count in sorted(effects.items()):
            lines.append(f"| {name} | {count} |")
        lines.append("")
    failed = failed or any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in rule_rows)
    return lines, failed


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
              f"{total['KNOWN_GAP']} KNOWN_GAP (failing), {total['SKIP']} SKIP, "
              f"{total['WARN']} WARN.", ""]
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
    positive = {
        "file_picker": "LLFilePicker::instance().getOpenFile();",
        "delete": "LLFile::remove(path);",
        "rename": "std::filesystem::rename(old_path, new_path);",
        "export_import": "exportScene(scene);",
        "save_disk": "savePreset(name);",
        "logout": "requestQuit();",
        "server_message": "gMessageSystem->sendReliable(host);",
        "agent_send": "gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);",
        "object_send": "LLViewerObject::sendShapeUpdate();",
        "selection_send": "LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);",
        "selection_broadcast": "LLSelectMgr::getInstance()->selectObjectOnly(object);",
        "parcel_send": "LLViewerParcelMgr::sendParcelPropertiesUpdate();",
        "chat_im_voice": "sendChat(text);",
        "world_action": "LLSelectMgr::getInstance()->sendTake();",
        "scripted_dialog": "LLScriptFloater::show(script);",
        "clipboard_write": "copyToClipboard(value);",
        "external_url_process": "loadURLExternal(url);",
        "persistent_setting": "gSavedPerAccountSettings.setBOOL(\"Flag\", true);",
    }
    for tag, fixture in positive.items():
        assert re.search(HAZARD_RULES[tag], _cpp_without_comments(fixture)), \
            "positive fixture missed hazard rule: " + tag
    positive_calls = {
        "world_action": [
            "upload_new_resource(asset, type, name, description);",
            "gAgent.teleportHome();",  # alchatcommand.cpp:193
            "gAgent.teleportCancel();",  # alfloaterprogressview.cpp:100
            "gAgent.teleportViaLandmark(asset_id);",
            "gAgent.teleportViaLocation(pos_global);",  # alchatcommand.cpp:125
            "gAgent.teleportRequest(region_handle, position, false);",
            "LLAvatarActions::teleportRequest(id);",  # alfloatersceneexplorer.cpp:3141
            "LLAvatarActions::offerTeleport(getAvatarId());",  # alpanelprofilelegacy.cpp:645
            "LLAvatarActions::pay(getAvatarId());",  # alpanelprofilelegacy.cpp:651
            "LLEnvironment::instance().updateParcel(parcel_id, settings);",
            "LLEnvironment::getInstance()->updateRegion(settings);",
            "LLSelectMgr::getInstance()->selectionUpdateTemporary(1);",  # alviewermenu.cpp:283
            "LLSelectMgr::getInstance()->sendDelink();",  # alviewermenu.cpp:285
            "LLSelectMgr::getInstance()->selectObjectOnly(object);",
            "LLSelectMgr::getInstance()->deselectAll();",
            "gAgent.standUp();",  # fsfloaterposestand.cpp:104
            "gAgent.sitDown();",  # fsfloaterposestand.cpp:152
            "setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);",  # llagent.cpp:1423
            "LLAppearanceMgr::instance().wearItemOnAvatar(inv_item, true);",
            "LLAppearanceMgr::instance().removeItemsFromAvatar(ids);",
            "LLAppearanceMgr::instance().replaceCurrentOutfit(outfit_id);",
            "gInventory.removeItem(item_id);",
            "gInventory.purgeObject(object_id);",
            "gInventory.changeItemParent(item, parent, true);",
            "create_inventory_item(gAgent.getID(), session, parent, id, type, inv, name, desc, sale, flags, cb);",
            "update_inventory_item(item, callback);",
            "copy_inventory_item(owner, item, parent, name, callback);",
            "move_inventory_item(item, parent, name, callback);",
            "remove_inventory_item(item_id);",
            "purge_descendents_of(category_id);",
            "LLGiveInventory::doGiveInventoryItem(avatar_id, item, session_id);",
            "gAgent.startAutoPilotGlobal(pos_global);",  # llfloaterworldmap.cpp:1630
            "handle_object_sit(obj->getID());",  # alfloatersceneexplorer.cpp:2967
            "LLAppearanceMgr::instance().addCOFItemLink(item);",  # llpaneloutfitedit.cpp:1229
            "create_inventory_wearable(gAgent.getID(), session, parent, transaction, type, subtype, name, desc, cb);",  # llagentwearables.cpp:852
            "create_inventory_callingcard(avatar_id, folder_id, cb);",  # llfriendcard.cpp:618
            "update_inventory_category(category_id, updates, NULL);",  # llfloaterchangeitemthumbnail.cpp:1088
            "LLViewerParcelMgr::getInstance()->startBuyLand();",  # llpanelland.cpp:247
        ],
        "agent_send": [
            "gAgent.setLookAt(LOOKAT_TARGET_SELECT, object, position);",
            "gAgentCamera.setPointAt(POINTAT_TARGET_SELECT, object, position);",  # llselectmgr.cpp:8043
            "gAgentCamera.setLookAt(LOOKAT_TARGET_SELECT, object, position);",  # llselectmgr.cpp:8044
            "gAgent.setFlying(true);",  # llviewerinput.cpp:112
            "gAgent.setAFK();",  # llappviewer.cpp:561
            "gAgent.setDoNotDisturb(false);",  # llavataractions.cpp:1545
            "gAgent.setStartPosition(START_LOCATION_ID_HOME);",  # alchatcommand.cpp:198
            "LLAgent::setPointAt(POINTAT_TARGET_SELECT, object, position);",
            "LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINTAT);",
            "sendAgentUpdate(true, true);",
        ],
        "object_send": ["objectp->sendShapeUpdate();"],
        "server_message": [
            "gMessageSystem->newMessageFast(_PREHASH_ObjectGrab);",
            "gMessageSystem->sendReliable(region->getHost());",
            "gMessageSystem->sendMessage(host);",
        ],
        "parcel_send": [
            "LLHTTPClient::post(gAgent.getRegionCapability(\"Example\"), body, responder);",
            "LLCoreHttpUtil::put(gAgent.getRegionCapability(\"Example\"), body, handler);",
        ],
        "delete": [
            "gDirUtilp->deleteFilesInDir(directory, mask);",
            "std::filesystem::remove(path);", "std::filesystem::remove_all(path);",
            "LLFile::unlink(path);", "LLFile::rmdir(path);",
        ],
        "rename": [
            "std::filesystem::rename(old_path, new_path);",
            "std::filesystem::copy(old_path, new_path);",
            "LLFile::rename(old_path, new_path);", "LLFile::copy(old_path, new_path);",
        ],
        "save_disk": [
            "FILE* fp = fopen(path.c_str(), \"wx\");",
            "LLFILE* fp = LLFile::fopen(path, LLFILE_MODE(\"wb\"));",
            "llofstream output(path.c_str());", "std::ofstream output(path);",
            "LLAPRFile file; file.open(path, LL_APR_WB);",
            "LLAPRFile file; file.write(buffer, size);",
            "LLSDSerialize::toXML(data, output);",
        ],
        "logout": ["requestQuit();"],
        "clipboard_write": [
            "getWindow()->copyTextToClipboard(text);",  # alfloaterparticleeditor.cpp:534
        ],
        "external_url_process": [
            "gDirUtilp->openDir(path);",  # alfloaterexploresounds.cpp:591
            "ShellExecuteW(window, operation, file, params, directory, show);",
            "ShellExecute(window, operation, file, params, directory, show);",
            "CreateProcessW(app, command, attrs, thread, false, flags, env, cwd, startup, process);",
            "LLProcess::create(params);",  # llexternaleditor.cpp:120
        ],
        "modal": ["OSMessageBox(message, title, OSMB_OK);"],  # llappviewer.cpp:2435
    }
    for tag, fixtures in positive_calls.items():
        for fixture in fixtures:
            assert re.search(HAZARD_RULES[tag], _cpp_without_comments(fixture)), \
                f"positive call fixture missed {tag}: {fixture}"
    assert not re.search(HAZARD_RULES["save_disk"], "ALDirectorSwitcher::saveBank(bank);"), \
        "saving an LLControl bank was confused with host-file I/O"
    assert not re.search(HAZARD_RULES["rename"], "model.renameCamera(label);"), \
        "renaming a local camera label was confused with renaming a host file"
    assert not re.search(HAZARD_RULES["world_action"],
                         "uploadDebugLightingTerm(shader); uploadShadowUniforms(shader);"), \
        "GPU uniform uploads were treated as uploads to the simulator"
    for fixture in ("LLAppViewer::instance()->forceQuit();", "exit(0);"):
        assert re.search(LOGIN_ONLY_BLOCKER_RULES["logout"], fixture), \
            f"login quit/exit blocker fixture missed: {fixture}"
    raw_and_digit = _cpp_without_comments(
        'auto raw = R"tag(// text /* still raw */)tag";\n'
        "auto n = 1'000; gAgent.teleportHome();\n"
        "#if 0\ngAgent.teleportCancel();\n#endif\n")
    assert "gAgent.teleportHome" in raw_and_digit and "gAgent.teleportCancel" in raw_and_digit, \
        "raw string or digit separator hid executable/#if-0 code"
    negative = _cpp_without_comments(r'''
// requestQuit(); LLFile::remove(path); gAgent.sendAnimationRequest(id, true);
/* savePreset(name); logout(); */
saved_projection = true;
log << "saved into cache";
return (x >= 0);
bank (account);
''')
    assert not any(re.search(pattern, negative) for pattern in HAZARD_RULES.values()), \
        "comments or loose English identifiers produced a hazard"

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
    getChild<LLButton>("select_danger")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onPickProp(); });
    getChild<LLButton>("explorer_danger")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("animation_explorer"); });
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
void Fixture::onPickProp() { LLSelectMgr::getInstance()->selectObjectOnly(prop); }
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
    assert _mode_denies_hazards(hazards, True, False), \
        "in-world mode must fail closed for every NETWORK hazard"
    assert not _mode_denies_hazards(hazards, False, True), \
        "proved no-circuit login mode must run NETWORK-only rows"
    assert _mode_denies_hazards(hazards, False, False), \
        "login mode must abort when the no-circuit proof is absent"
    assert _mode_denies_hazards(hazards, False, True, False), \
        "login mode must abort when its file snapshot is unverified"

    login_network_fixture = dict(danger, action="RUN", login_policy_run=True)
    assert login_network_fixture["action"] == "RUN" and \
        not _mode_denies_hazards(hazards, False, True) and \
        _mode_denies_hazards(hazards, True, True), \
        "NETWORK row must run only at proved-no-circuit login and remain denied in-world"
    login_unresolved_fixture = {
        "action": "RUN", "reason": "", "known_gap": True,
        "direct_hazards": [],
    }
    _apply_unresolved_handler_policy(login_unresolved_fixture, set())
    assert login_unresolved_fixture["action"] == "RUN" and \
        login_unresolved_fixture["inworld_allowed"] is False, \
        "unresolved row must run under login policy and remain denied in-world"
    assert not _mode_denies_hazards({"save_disk", "delete"}, False, True), \
        "snapshot-covered file writes/deletes must run at login"
    for blocked in LOGIN_BLOCKING_HAZARDS:
        assert _mode_denies_hazards({blocked}, False, True), \
            f"login blocker escaped policy: {blocked}"

    safe, hazards, resolved = scan("safe_neighbor")
    assert resolved and not (hazards & LOCAL_HAZARDS), \
        "a neighbouring control's LOCAL handler was attributed"
    assert not (hazards & NETWORK_HAZARDS), "unrelated sender contaminated an exact local callback"
    assert safe["inworld_allowed"] is True

    local_safe, hazards, resolved = scan("local_safe")
    assert resolved and not (hazards & LOCAL_HAZARDS), \
        "a reused local control donated its neighbour's LOCAL handler"
    assert not (hazards & NETWORK_HAZARDS) and local_safe["inworld_allowed"] is True

    transitive, hazards, resolved = scan("transitive_danger")
    assert resolved and "agent_send" in hazards and transitive["inworld_allowed"] is False

    localfog, hazards, resolved = scan("localfog_center_x")
    assert resolved and "agent_send" in hazards and localfog["inworld_allowed"] is False

    select, hazards, resolved = scan("select_danger")
    assert resolved and "selection_broadcast" in hazards and select["inworld_allowed"] is False

    explorer, hazards, resolved = scan("explorer_danger")
    assert resolved and "selection_broadcast" in hazards and explorer["inworld_allowed"] is False

    vcam_source = r'''
void ALPanelDirectorVCam::onControl(LLUICtrl* source)
{
    const std::string name = source->getName();
    const LLSD value = source->getValue();
    if (name == "vc_correct") onCorrect();
    else if (name == "vc_sibling") LLFile::remove(path);
    else if (name == "vc_monitor_header")
    {
        const std::string action = value["action"].asString();
        if (action == "safe") onCorrect();
        else if (action == "danger") OSMessageBox(message, title, OSMB_OK);
    }
}
void ALPanelDirectorVCam::onCorrect() { savePreset(name); }
'''
    vcam_sources = {Path("alpaneldirectorvcam.cpp"): vcam_source}

    def scan_vcam(widget: str, action: str = "") -> tuple[set[str], bool]:
        row: dict[str, object] = {
            "owner": "ALPanelDirectorVCam::onControl", "widget_name": widget,
            "page": "vcam_tab", "action": "RUN", "exercise": "click",
        }
        if action:
            row["dispatch_action"] = action
        return _handler_hazards(row, vcam_sources, {})

    hazards, resolved = scan_vcam("vc_correct")
    assert resolved and "save_disk" in hazards, \
        "VCam row did not select its own branch/callee"
    assert "delete" not in hazards, \
        "VCam sibling branch hazard leaked into the selected row"
    hazards, resolved = scan_vcam("vc_sibling")
    assert resolved and "delete" in hazards, \
        "VCam hazard in the selected branch was missed"
    hazards, resolved = scan_vcam("vc_monitor_header", "safe")
    assert resolved and "save_disk" in hazards and "modal" not in hazards, \
        "VCam action sub-dispatch selected the wrong branch"

    assert not re.search(HAZARD_RULES["world_action"],
                         "LLEnvironment::instance().updateEnvironment(transition);"), \
        "local sky/water selection was classified as a parcel/region update"
    assert re.search(HAZARD_RULES["world_action"],
                     "LLEnvironment::instance().updateParcel(parcel_id, settings);"), \
        "parcel environment capability update escaped the sender rules"

    call_only = "void Unrelated::update() { if (LLTimer::getElapsedSeconds() > 1) { gAgent.sendReliableMessage(); } }"
    assert not _function_body(call_only, "LLTimer::getElapsedSeconds"), \
        "a function call in a conditional was mistaken for a definition"
    assert not _source_body({Path("unrelated.cpp"): call_only}, "LLTimer::getElapsedSeconds")[0]

    receiver_sources = {Path("receiver_fixture.cpp"): r'''
extern LLPipeline gPipeline;
LLRenderTarget mTarget;
LLRelay mRelay;
void LocalFixture::commit() { gPipeline.releaseGLBuffers(); }
void LLPipeline::releaseGLBuffers() { mTarget.cleanup(); }
void Unrelated::cleanup() { gAgent.sendReliableMessage(); }
void RemoteFixture::commit() { mRelay.apply(); }
void LLRelay::apply() { gAgent.sendReliableMessage(); }
'''}
    receiver_row = {"owner": "LocalFixture::commit", "callbacks": "LocalFixture::commit", "widget_name": "local", "action": "RUN"}
    hazards, resolved = _handler_hazards(dict(receiver_row), receiver_sources, {})
    assert resolved and not (hazards & NETWORK_HAZARDS), "an unrelated cleanup() was attributed to a render target"
    receiver_row.update(owner="RemoteFixture::commit", callbacks="RemoteFixture::commit")
    hazards, resolved = _handler_hazards(receiver_row, receiver_sources, {})
    assert resolved and "agent_send" in hazards, "the declared receiver's indirect sender was missed"

    camera_sources = {Path("camera_fixture.cpp"): r'''
LLSD mEditor;
void ALPanelDirectorSwitcher::onSlotButton() { ALDirectorSwitcher::instance().punch(); }
void ALDirectorSwitcher::punch() { mEditor.set(LLSD()); }
void Unrelated::set() { gAgent.sendAnimationRequest(id, ANIM_REQUEST_START); }
'''}
    camera_row = {"owner": "ALPanelDirectorSwitcher::postBuild", "callbacks": "ALPanelDirectorSwitcher::onSlotButton",
                  "widget_name": "slot", "page": "shot_tab", "action": "RUN", "exercise": "click"}
    hazards, resolved = _handler_hazards(dict(camera_row), camera_sources, {})
    assert resolved and not (hazards & NETWORK_HAZARDS), "unrelated set() contaminated the local camera model"
    network_camera_sources = {path: text.replace("mEditor.set(LLSD());", "gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);")
                              for path, text in camera_sources.items()}
    hazards, resolved = _handler_hazards(dict(camera_row), network_camera_sources, {})
    assert resolved and "agent_send" in hazards, "camera model sender escaped its exact call path"

    setting_source = r'''
bool SettingFixture::postBuild()
{
    getChild<LLUICtrl>("setting_danger");
    getChild<LLUICtrl>("setting_helper");
    getChild<LLUICtrl>("setting_table");
    return true;
}
void SettingFixture::connectSetting()
{
    gSavedSettings.getControl("DangerSetting")->getSignal()->connect(
        boost::bind(&SettingFixture::onSetting, this));
}
void SettingFixture::onSetting()
{
    AOEngine::getInstance()->override(id, true);
}
void setting_setup_signal_listener(LLControlGroup& group, const std::string& setting,
                                   std::function<void()> callback)
{
    setting_get_control(group, setting)->getSignal()->connect([callback]() { callback(); });
}
void SettingFixture::connectHelper()
{
    setting_setup_signal_listener(gSavedSettings, "HelperSetting", handleHelperSetting);
}
void handleHelperSetting()
{
    gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
}
void SettingFixture::connectUnknown()
{
    const char* settings[] = { "UnknownSetting" };
    for (const char* name : settings)
        gSavedSettings.getControl(name)->getSignal()->connect(onUnknown);
}
void SettingFixture::readSafeSetting()
{
    LLCachedControl<bool> safe(gSavedSettings, "SafeSetting", false);
    consumeLocally(safe);
}
void SettingFixture::unrelatedNetworkSender()
{
    gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
}
struct SettingTable
{
    LLCachedControl<bool> value{gSavedSettings, "TableSetting", false};
};
'''
    setting_source += r'''
void SettingFixture::connectLiteralTables()
{
    const char* remote[] = { "TableNetwork" };
    for (const char* name : remote)
        gSavedSettings.getControl(name)->getSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&) { gAgent.sendAnimationRequest(id, ANIM_REQUEST_START); });
    for (const char* name : { "TableLocalA", "TableLocalB" })
    {
        if (LLControlVariable* color = gSavedSettings.getControl(name))
            color->getSignal()->connect([](LLControlVariable*, const LLSD&, const LLSD&) { gSavedSettings.setBOOL("Local", true); });
    }
}
'''
    setting_source += r'''
void SettingFixture::tick()
{
    {
        static LLCachedControl<unsigned> limit(gSavedSettings, "LocalWait", 60);
        waitLocally(limit);
    }
    gAgent.sendReliableMessage();
}
void SettingFixture::sendWhenEnabled()
{
    static LLCachedControl<bool> flag(gSavedSettings, "ConditionalSender", false);
    if (flag) gAgent.sendReliableMessage();
}
'''
    setting_sources = {Path("setting_fixture.cpp"): setting_source}
    setting_row: dict[str, object] = {
        "owner": "SettingFixture::postBuild", "widget_name": "setting_danger",
        "page": "fixture", "action": "RUN", "exercise": "toggle",
        "widget_type": "toggle", "binding": "setting", "setting": "DangerSetting",
    }
    hazards, resolved = _handler_hazards(setting_row, setting_sources, {})
    assert resolved and setting_row["inworld_setting_listener_count"] == 1
    assert "agent_send" in hazards, "AO override reachable from a setting listener was missed"

    unknown_row = dict(setting_row, widget_name="setting_unknown", setting="UnknownSetting")
    _hazards, resolved = _handler_hazards(unknown_row, setting_sources, {})
    assert not resolved and unknown_row.get("setting_listener_unknown"), \
        "dynamic setting listener must fail closed"

    helper_row = dict(setting_row, widget_name="setting_helper", setting="HelperSetting")
    hazards, resolved = _handler_hazards(helper_row, setting_sources, {})
    assert resolved and helper_row["inworld_setting_listener_count"] == 1
    assert "agent_send" in hazards, "helper-mediated setting listener callback was missed"

    safe_network_row = dict(setting_row, owner="", widget_name="setting_safe", setting="SafeSetting")
    hazards, resolved = _handler_hazards(safe_network_row, setting_sources, {})
    assert resolved and not (hazards & NETWORK_HAZARDS), (
        "unrelated sender contaminated a local setting consumer: "
        f"resolved={resolved}, hazards={sorted(hazards)}, row={safe_network_row}")
    assert not _mode_denies_hazards(hazards, True, False)

    local_wait = dict(setting_row, owner="", setting="LocalWait")
    hazards, resolved = _handler_hazards(local_wait, setting_sources, {})
    assert resolved and not (hazards & NETWORK_HAZARDS), "unrelated frame-loop sender contaminated a scoped local wait"
    conditional_sender = dict(setting_row, owner="", setting="ConditionalSender")
    hazards, resolved = _handler_hazards(conditional_sender, setting_sources, {})
    assert resolved and "agent_send" in hazards, "setting-conditional sender escaped the consumer scan"

    network_table = dict(setting_row, owner="", setting="TableNetwork")
    hazards, resolved = _handler_hazards(network_table, setting_sources, {})
    assert resolved and "agent_send" in hazards, "literal-table sender escaped the consumer scan"
    local_table = dict(setting_row, owner="", setting="TableLocalA")
    hazards, resolved = _handler_hazards(local_table, setting_sources, {})
    assert resolved and local_table["inworld_setting_listener_count"] == 1 and not (hazards & NETWORK_HAZARDS), \
        "local literal-table listener was confused with its neighbouring sender"
    table_row = dict(setting_row, widget_name="setting_table", setting="TableSetting")
    _hazards, resolved = _handler_hazards(table_row, setting_sources, {})
    assert resolved and table_row.get("setting_sole_sink"), \
        "non-executable static setting declaration blocked the direct setting sink"


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
    parser.add_argument("--require-rules", action="store_true",
                        help="fail when a v2 run lacks rows for any Director rule (R1-R6)")
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
    right = [row for row in (load(args.right) if args.right else []) if row.get("kind") != "rule"]
    left_all = left
    left = [row for row in left if row.get("kind") != "rule"]
    lines = render(left, csv_path(args.left).parent.name)
    rule_lines, rules_failed = rules_section(
        left_all, args.require_rules and not pages and not row_re)
    lines += [""] + rule_lines
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
    failed = failed or budget_failed or rules_failed
    if args.right:
        failed = failed or any(row["PASS/FAIL/SKIP"] in FAIL_STATUSES for row in right)
    if args.dry_reveal:
        print("DRY REVEAL: NOT A FUNCTIONAL RESULT")
        return 3
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
