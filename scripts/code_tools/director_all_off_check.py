#!/usr/bin/env python3
"""Fail closed on Director ALL OFF scope, shared traversal and keyboard guards.

Usage: director_all_off_check.py --check | --selftest
Only reads named native/XUI sources; no shader or private sources.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
HEADER = "indra/newview/aldirectoralloff.h"
SERVICE = "indra/newview/aldirectoralloff.cpp"
FLOATER = "indra/newview/llfloaterdirector.cpp"
SETTINGS = "indra/newview/app_settings/settings.xml"
ALCHEMY_SETTINGS = "indra/newview/app_settings/settings_alchemy.xml"
ENV_FLOATER = "indra/newview/llfloaterenvironmentadjust.cpp"
GEAR = "indra/newview/skins/default/xui/en/panel_dpage_settings_director_settings.xml"
FOCUS = "indra/newview/skins/default/xui/en/panel_dpage_lens_focus.xml"
INPUTS = (HEADER, SERVICE, FLOATER, SETTINGS, ALCHEMY_SETTINGS, ENV_FLOATER, GEAR, FOCUS)
IDLE_GUARD = "if (mIsV2 && mAllOffMode && !cast.isRunning() && !cast.isCountingDown()) return;"


def body(source: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^;]*?\)\s*(?:const\s*)?\{", source)
    if not match:
        return ""
    start, level = match.end(), 1
    for index in range(start, len(source)):
        if source[index] == "{":
            level += 1
        elif source[index] == "}":
            level -= 1
            if not level:
                return source[start:index]
    return ""


def check_sources(files: dict[str, str]) -> list[str]:
    errors = []
    header, service, floater = (files[p] for p in (HEADER, SERVICE, FLOATER))
    table = re.search(r"\bFEATURES\[\]\s*=\s*\{(.*?)\n\};", header, re.S)
    if not table:
        return ["missing single FEATURES table"]
    rows = re.findall(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*Operation::(Default|Off|Rig|Switcher)\s*\}', table[1])
    if len(rows) != 43 or len(set(name for name, _ in rows)) != len(rows):
        errors.append("feature contract must contain 43 unique settings")
    known = set()
    control_types = {}
    for path in (SETTINGS, ALCHEMY_SETTINGS):
        nodes = list(ET.fromstring(files[path]).find("map"))
        declarations = {node.text for node in nodes[::2]}
        for duplicate in sorted(known & declarations):
            errors.append(f"setting declared in both files: {duplicate}")
        known.update(declarations)
        for key, declaration in zip(nodes[::2], nodes[1::2]):
            fields = list(declaration)
            properties = {k.text: v for k, v in zip(fields[::2], fields[1::2])}
            control_types[key.text] = properties.get("Type", ET.Element("missing")).text
            if key.text == "AlchemyEnvMoonLinked" and properties.get("Value", ET.Element("missing")).text != "1":
                errors.append("Personal Lighting reset must re-link the moon")
    advanced = re.search(r"\bENV_INTENSITY_ADV_KEYS\[\]\s*=\s*\{(.*?)\};", files[ENV_FLOATER], re.S)
    if not advanced:
        errors.append("missing Personal Lighting advanced source array")
    else:
        required = set(re.findall(r'"(AlchemyEnv\w+)"', advanced[1]))
        required.update(("AlchemyEnvSunEV", "AlchemyEnvSkyGIEV", "AlchemyEnvMoonLinked", "AlchemyEnvEnvironmentEV"))
        defaults = {name for name, operation in rows if operation == "Default"}
        for name in sorted(required - defaults):
            errors.append(f"Personal Lighting default missing: {name}")
    gear_settings = set()
    for page in (GEAR, FOCUS):
        for node in ET.fromstring(files[page]).iter():
            for attr in ("setting", "control_name", "reset_settings", "watch_settings", "enable_setting", "master_setting"):
                gear_settings.update(filter(None, re.split(r"[, ]+", node.get(attr, ""))))
    forbidden = re.compile(r"ToneMap|Tonemap|SLog|Slog|S_Log|DepthOfField|DoF|DOF|FrameGuide|CameraGuide|Composition|ALPolish|RenderProbeOnDemand", re.I)
    for name, operation in rows:
        if name not in known:
            errors.append(f"unknown setting: {name}")
        if forbidden.search(name) or name in gear_settings:
            errors.append(f"forbidden setting: {name}")
        if name == "AlchemyEnvEnvironmentOnly":
            errors.append(f"forbidden setting: {name}")
        if operation == "Default" and not name.startswith("AlchemyEnv"):
            errors.append(f"default reset outside Personal Lighting: {name}")
        allowed_types = ("F32", "Boolean", "Color4") if operation == "Default" else ("Boolean",)
        if name in known and control_types.get(name) not in allowed_types:
            errors.append(f"unsupported control type for {name}")
    for name in ("captureSettings", "applySettings", "validSettings"):
        code = body(header, name)
        if len(re.findall(r"for\s*\(const Feature& feature : FEATURES\)", code)) != 1:
            errors.append(f"{name} must traverse the single FEATURES table")
    validation = body(header, "validSettings")
    for contract in ("switch (readType(feature))", "case TYPE_F32:", "case TYPE_BOOLEAN:", "case TYPE_COL4:",
                     "!value.isReal()", "!value.isBoolean()", "!value.isArray() || value.size() != 4",
                     "component < 4", "!value[component].isReal()", "std::isfinite(value.asReal())",
                     "std::isfinite(value[component].asReal())"):
        if contract not in validation:
            errors.append(f"backup type validation missing: {contract}")
    if not re.search(r'validSettings\(root\["settings"\],\s*\[\]\(const Feature& feature\)\s*\{\s*'
                     r'LLControlVariable\* control = gSavedSettings.getControl\(feature.setting\);\s*'
                     r'return control \? control->type\(\) : TYPE_COUNT;', body(service, "readBackup")):
        errors.append("backup validation must use the real control type")
    if "captureSettings(" not in body(service, "reset") or "applySettings(" not in body(service, "apply"):
        errors.append("backup and off paths must use shared table helpers")
    if not re.search(r"apply\(root,\s*false\)", body(service, "reset")) or not re.search(r"apply\(root,\s*true\)", body(service, "restore")):
        errors.append("reset and restore must use the same apply path")
    if re.search(r'gSavedSettings\.(?:set\w+|getControl)\(\s*"', service):
        errors.append("service has a setting path outside the table")
    mode = body(floater, "LLFloaterDirector::updateAllOffMode")
    if not re.search(r"setDefaultBtn\(mAllOffMode\s*\?\s*nullptr\s*:\s*mActionBtn\)", mode):
        errors.append("Enter guard missing: ALL OFF must not be default")
    if "setCommitOnReturn(!mAllOffMode)" not in mode:
        errors.append("focused Return guard missing")
    action = body(floater, "LLFloaterDirector::onClickAction")
    if (IDLE_GUARD not in action or "reset(" in action or "requestAllOff(" in action
            or action.find(IDLE_GUARD) > action.find("cast.cut()")
            or action.find(IDLE_GUARD) > action.find("cast.action()")):
        errors.append("keyboard commit can reach ALL OFF or bypass v2 guard")
    if "mAllOffMouseCommit" in floater:
        errors.append("obsolete mouse commit tagging")
    chrome = body(floater, "LLFloaterDirector::bindChrome")
    if not re.search(r"setMouseDownCallback\([\s\S]*?requestAllOff\(\)", chrome):
        errors.append("ALL OFF must originate from the mouse callback")
    if "mActionBtn->setMouseUpCallback" in chrome:
        errors.append("ALL OFF must not use mouse-up tagging")
    if "getSignal()->connect(" not in chrome or "updateAllOffMode();" not in chrome:
        errors.append("live mode signal binding missing")
    transport = body(floater, "LLFloaterDirector::refreshTransport")
    if "(all_off ? 2 : 0)" not in transport or "all_off || mActionArmed" not in transport:
        errors.append("transport cache/enable state does not include ALL OFF")
    if ("styleTransportAction(mActionBtn, mCutBtn, mArmBtn, transport_live);" not in transport
            or not re.search(r"if \(all_off && !transport_live\)\s*ALKitStyle::styleTransportAction\(mActionBtn, nullptr, nullptr, true\);", transport)):
        errors.append("ALL OFF must style only ACTION red while idle")
    reset = body(service, "reset")
    take = body(service, "cameraTakeActive")
    if "STATE_RECORDING" not in take or "recorder.isPlaybackActive()" not in take or "cameraTakeActive()" not in body(service, "idle"):
        errors.append("reset/restore must reject recording and active/paused playback")
    if reset.find("atomicWrite(") < 0 or reset.find("atomicWrite(") > reset.find("apply(root, false)"):
        errors.append("reset changes state before atomic backup")
    for name in ("requestAllOff", "performAllOff"):
        if not body(floater, "LLFloaterDirector::" + name):
            errors.append(f"missing confirmation route {name}")
    for token in ('root["rig_lit_groups"] = LLSD::emptyArray();', '.getLitGroup()',
                  'rigs.setLitGroup(static_cast<ALCineLightRigManager::Slot>(i), root["rig_lit_groups"][i].asString());',
                  'groups.size() != ALCineLightRigManager::SLOT_COUNT', '!group.isString()'):
        if token not in service:
            errors.append("missing ALL OFF lit-group contract: " + token)
    return errors


def check(root: Path = ROOT) -> list[str]:
    return check_sources({p: (root / p).read_text(encoding="utf-8")
                          for p in INPUTS})


def selftest(quiet: bool = False) -> int:
    clean = {p: (ROOT / p).read_text(encoding="utf-8") for p in INPUTS}
    errors = check_sources(clean)
    if errors:
        print("selftest clean fixture failed: " + "; ".join(errors))
        return 1
    fixtures = [
        ("lit-group capture", SERVICE, 'root["rig_lit_groups"] = LLSD::emptyArray();', '/* planted missing capture */'),
        ("lit-group restore", SERVICE, 'rigs.setLitGroup(static_cast<ALCineLightRigManager::Slot>(i), root["rig_lit_groups"][i].asString());', '/* planted missing restore */'),
        ("lit-group validation", SERVICE, '!group.isString()', 'false'),
        ("unknown setting", HEADER, '"AlchemyEnvSunEV"', '"PlantedMissingSetting"'),
        ("tonemapper", HEADER, '"AlchemyEnvSunEV"', '"AlchemyToneMapType"'),
        ("S-Log", HEADER, '"AlchemyEnvSunEV"', '"AlchemyToneMapSLogEV"'),
        ("DoF", HEADER, '"AlchemyEnvSunEV"', '"RenderDepthOfField"'),
        ("guides", HEADER, '"AlchemyEnvSunEV"', '"CinematicCompositionGuide"'),
        ("Pose Polish", HEADER, '"AlchemyEnvSunEV"', '"ALPolishEnabled"'),
        ("gear preference", HEADER, '"AlchemyEnvSunEV"', '"DirectorTheme"'),
        ("capture list drift", HEADER, "for (const Feature& feature : FEATURES)", "for (const Feature& feature : CAPTURE_ONLY)"),
        ("off list drift", HEADER, "void applySettings(const LLSD& saved", "void plantedOffSettings(const LLSD& saved"),
        ("restore bypass", SERVICE, "apply(root, true)", "plantedRestore(root)"),
        ("Enter default guard", FLOATER, "setDefaultBtn(mAllOffMode ? nullptr : mActionBtn)", "setDefaultBtn(mActionBtn)"),
        ("focused Enter guard", FLOATER, "setCommitOnReturn(!mAllOffMode)", "setCommitOnReturn(true)"),
        ("idle ALL OFF commit guard", FLOATER, IDLE_GUARD, ""),
        ("weakened ALL OFF commit guard", FLOATER, IDLE_GUARD, IDLE_GUARD.replace("mIsV2 && ", "")),
        ("Personal Lighting advanced key", HEADER, '"AlchemyEnvSunKelvin"', '"AlchemyEnvIntensityAdvanced"'),
        ("future Personal Lighting advanced key", ENV_FLOATER, '"AlchemyEnvSunKelvin",', '"AlchemyEnvSunKelvin", "AlchemyEnvFutureAdvanced",'),
        ("ARM idle colour", FLOATER, "mArmBtn, transport_live);", "mArmBtn, transport_live || all_off);"),
        ("Color4 validation", HEADER, "value.size() != 4", "value.size() != 3"),
        ("Boolean validation", HEADER, "!value.isBoolean()", "false"),
        ("real control type validation", SERVICE, "control->type() : TYPE_COUNT", "TYPE_F32 : TYPE_COUNT"),
        ("atomic ordering", SERVICE, "apply(root, false)", "plantedApply(root)"),
        ("standalone take guard", SERVICE, " || recorder.isPlaybackActive()", ""),
    ]
    for label, path, before, after in fixtures:
        planted = dict(clean)
        if before not in planted[path]:
            raise AssertionError(f"fixture did not plant: {label}")
        planted[path] = planted[path].replace(before, after, 1)
        findings = check_sources(planted)
        if not findings:
            print(f"FAIL: accepted planted {label}")
            return 1
        expected = {
            "unknown setting": "unknown setting: PlantedMissingSetting",
            "Personal Lighting advanced key": "Personal Lighting default missing: AlchemyEnvSunKelvin",
            "future Personal Lighting advanced key": "Personal Lighting default missing: AlchemyEnvFutureAdvanced",
        }.get(label)
        if expected and expected not in findings:
            print(f"FAIL: planted {label} missed required finding: {expected}")
            return 1
        if not quiet:
            print(f"PASS: rejected planted {label}")
    # Existing valid declarations in Off rows isolate forbidden-scope checking:
    # rejection cannot hide behind a missing key or a Default-operation check.
    for key in ("CinematicFrameGuideEnabled", "CameraFNumber", "CameraMaxCoF", "DirectorTheme", "ALPolishEnabled"):
        planted = dict(clean)
        planted[HEADER] = planted[HEADER].replace('"RenderGradNDEnabled"', '"' + key + '"', 1)
        findings = check_sources(planted)
        if not any(f == "forbidden setting: " + key for f in findings):
            print("FAIL: forbidden-scope fixture escaped: " + key)
            return 1
        if not quiet:
            print("PASS: rejected existing forbidden Off-row setting " + key)
    planted = dict(clean)
    planted[SETTINGS] = planted[SETTINGS].replace("<map>", "<map><key>AlchemyEnvSunEV</key><map/>", 1)
    if "setting declared in both files: AlchemyEnvSunEV" not in check_sources(planted):
        print("FAIL: accepted cross-file duplicate declaration")
        return 1
    if not quiet:
        print("PASS: rejected cross-file duplicate declaration")
        print(f"director_all_off_check selftest: {len(fixtures) + 6} bad fixtures rejected; clean fixture passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    choice = parser.add_mutually_exclusive_group(required=True)
    choice.add_argument("--check", action="store_true")
    choice.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    errors = check()
    for error in errors:
        print("FAIL: " + error)
    if not errors:
        print("director_all_off_check: PASS (43 settings, complete Personal Lighting, shared backup/off/restore table, Enter and idle commit guards)")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())
