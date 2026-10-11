#!/usr/bin/env python3
"""Summarize and compare Director functional self-test CSV files."""

from __future__ import annotations

import argparse
import csv
import json
import itertools
import re
from collections import Counter, defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET


KEY = ("page", "control", "kind", "action", "expected")
FAIL_STATUSES = {"FAIL", "KNOWN_GAP"}


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


def _function_body(text: str, qualified: str) -> str:
    """Return a conservative C++ function body for a qualified method."""
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


def _handler_hazards(row: dict[str, object], cpp_text: str,
                     body_cache: dict[str, str]) -> tuple[set[str], bool]:
    """Resolve a button's installed callback and scan it fail-closed."""
    owner = str(row.get("owner") or "")
    widget = str(row.get("widget_name") or "")
    bodies: list[str] = []
    resolved = False
    def body_for(name: str) -> str:
        if name not in body_cache:
            body_cache[name] = _function_body(cpp_text, name)
        return body_cache[name]

    owner_body = body_for(owner) if owner else ""
    if owner_body and (not widget or f'"{widget}"' in owner_body):
        at = owner_body.find(f'"{widget}"')
        direct_start = owner_body.rfind("getChild", 0, at + 1)
        window = owner_body[max(0, direct_start if direct_start >= 0 else at - 80):at + 500]
        assignment_window = owner_body[max(0, at - 160):at + 120]
        member_match = re.search(
            r"([A-Za-z_]\w*)\s*=\s*getChild(?:<[^>]+>)?\s*\(\s*\"" +
            re.escape(widget) + r"\"\s*\)", assignment_window)
        if member_match:
            callback_at = owner_body.find(member_match.group(1) + "->set", at)
            if callback_at >= 0:
                window = owner_body[callback_at:callback_at + 500]
        handlers = set(re.findall(r"&([A-Za-z_]\w*::[A-Za-z_]\w*)", window))
        owner_class = owner.split("::", 1)[0] if "::" in owner else ""
        if owner_class:
            handlers.update(owner_class + "::" + name for name in
                            re.findall(r"\b(on[A-Z][A-Za-z0-9_]*)\s*\(", window))
        if handlers:
            resolved = True
            for handler in handlers:
                body = body_for(handler)
                if body:
                    bodies.append(body)
        elif "setCommitCallback" in window or "setClickedCallback" in window:
            # Inline lambdas still have their complete call text in this window.
            resolved = True
            bodies.append(window)
    callback = str(row.get("callbacks") or "")
    if callback:
        body = body_for(callback)
        if body:
            resolved = True
            bodies.append(body)

    combined = "\n".join(bodies)
    hazards: set[str] = set()
    rules = {
        "file_picker": r"\b(?:LLFilePicker|LLFilePickerReplyThread|LLFloaterFilePicker|getOpenFile|getSaveFile)\b",
        "delete": r"\b(?:LLFile::remove|remove_all|DeleteFile|delete[A-Z]\w*|remove[A-Z]\w*)\b",
        "rename": r"\b(?:LLFile::rename|std::filesystem::rename|rename[A-Z]\w*)\b",
        "export_import": r"\b(?:export\w*|import\w*)\s*\(",
        "save_disk": r"\b(?:llofstream|ofstream|save[A-Z]\w*|writePreset|write[A-Z]\w*File)\b",
        "logout": r"\b(?:requestQuit|forceDisconnect|logout|disconnectFromGrid)\b",
    }
    for tag, pattern in rules.items():
        if re.search(pattern, combined, re.I):
            hazards.add(tag)
    return hazards, resolved


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
    body_cache: dict[str, str] = {}

    gaze_targets = {
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
        key = row.get("setting") or widget
        if widget.startswith("switcher_ease_bezier_"):
            prereqs.append({"setting": "DirectorSwitcherEaseCurve", "value": 100,
                            "source": "ALPanelDirectorSwitcher::updateCurveControlsEnabled"})
            seen.add(("DirectorSwitcherEaseCurve", 100))
        elif widget.startswith("switcher_freeze_bezier_"):
            prereqs.append({"setting": "DirectorSwitcherFreezeCurve", "value": 100,
                            "source": "ALPanelDirectorSwitcher::updateCurveControlsEnabled"})
            seen.add(("DirectorSwitcherFreezeCurve", 100))
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
            elif re.match(r"^cine_(key|fill|rim|bg)_", widget) and \
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
                row.update(action="RUN", reason="", known_gap=False, probe="gaze_reset",
                           reset_widget=target, reset_value=value,
                           reset_pre_value=(0 if value != 0 else 1),
                           reset_type="S32" if isinstance(value, int) and not isinstance(value, bool) else "F32",
                           owner="ALPanelLensGaze::postBuild")
        if widget in launchers:
            row.update(action="RUN", reason="", known_gap=False, probe="launcher",
                       launcher_floater=launchers[widget],
                       owner=row.get("owner") or "LLFloaterDirector::bindPage")
        if widget == "am_reset_CineLightRigActorMaskBias":
            value = defaults.get("CineLightRigActorMaskBias", -8.0)
            row.update(action="RUN", reason="", known_gap=False,
                       reset_widget="am_bias_slider", reset_value=value,
                       reset_pre_value=value + 1.0, reset_type="F32",
                       probe="gaze_reset", owner="LightBox.ResetControlDefault")
        if widget == "db_on":
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

        if row.get("exercise") == "click" and row.get("widget_type") == "button":
            hazards, handler_resolved = _handler_hazards(row, cpp_text, body_cache)
            if hazards:
                row["hazards"] = sorted(set(row.get("hazards", [])) | hazards)
                hazard_rows += 1
                if row.get("action") == "RUN":
                    row["action"] = "SKIP"
                    row["reason"] = "resolved handler hazard: " + ", ".join(sorted(hazards))
                    row["known_gap"] = False
            elif not handler_resolved and row.get("action") == "RUN":
                row["action"] = "SKIP"
                row["reason"] = "unresolved handler: hazard unknown"
                row["known_gap"] = False
                unresolved_hazards.append(f"{row['version']}:{row['page']}:{widget}")
            elif (handler_resolved and row.get("action") == "RUN" and
                  not row.get("setting") and not row.get("consumer") and
                  not row.get("reset_defaults") and
                  row.get("probe") not in {"launcher", "exposure_history"}):
                row["effect_kind"] = "side_effect_only"

    document.setdefault("meta", {})["r4_enable_unresolved"] = sorted(set(unresolved))
    document["meta"]["r4_enabled_control_rows"] = sum(bool(r["enabled_control"]) for r in rows)
    document["meta"]["r5_required_consumers"] = required_consumers
    document["meta"]["r5_total_consumers"] = sum(bool(r.get("consumer")) for r in rows)
    document["meta"]["r5_hazard_skip_rows"] = hazard_rows
    document["meta"]["r5_unresolved_handler_skips"] = sorted(set(unresolved_hazards))
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
    lines = [f"# Director functional self-test: {title}", "",
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
    args = parser.parse_args()
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
