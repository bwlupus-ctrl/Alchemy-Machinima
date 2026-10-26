#!/usr/bin/env python3
"""Fail-closed, source-only contract checks. --baseline HEAD demonstrates fail-before.

Only the named feature files are read; no private/reference lighting files are opened.
"""
import argparse
import re
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
KEY = "AlchemyEnvLocalLightSize"
SHADERS = "indra/newview/app_settings/shaders/"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--baseline", help="Read feature sources from this revision without moving HEAD")
    args = ap.parse_args()
    failures = []

    def read(path):
        if args.baseline:
            result = subprocess.run(["git", "show", f"{args.baseline}:{path}"], cwd=ROOT,
                                    capture_output=True, timeout=20)
            return result.stdout.decode("utf-8") if result.returncode == 0 else ""
        return (ROOT / path).read_text(encoding="utf-8")

    def check(name, condition):
        print(("PASS " if condition else "FAIL ") + name)
        if not condition:
            failures.append(name)

    settings = ET.fromstring(read("indra/newview/app_settings/settings_alchemy.xml"))
    entries = list(settings.find("map"))
    values = {entries[i].text: entries[i+1] for i in range(0, len(entries), 2)}
    setting = values.get(KEY)
    fields = list(setting) if setting is not None else []
    props = {fields[i].text: fields[i+1].text for i in range(0, len(fields), 2)}
    check("persistent F32, zero default", props.get("Type") == "F32" and props.get("Persist") == "1"
          and float(props.get("Value", -1)) == 0)
    env = read("indra/newview/alenvintensity.cpp")
    match = re.search(r"F32 localLightSize\([^)]*\)\s*\{(.*?)\n    \}", env, re.S)
    body = match[1] if match else ""
    check("runtime cache, finite clamp, neutral and unconditional rig exclusion",
          all(t in body for t in ["LLCachedControl<F32>", "isNeutral()", "isCineRigEmitter()",
                                 "std::isfinite", "0.5f"]) and "include_rig" not in body)
    control = read("indra/newview/llviewercontrol.cpp")
    manager = read("indra/newview/llviewershadermgr.cpp")
    check("no shader-reload listener", KEY not in control and
          not any(t in body for t in ["setShaders", "handleSetShaderChanged", "addPermutation"]))
    check("no shader-manager dependency or permutation", KEY not in manager and
          '"LOCAL_LIGHT_SIZE"' not in manager)
    for path in ["alenvintensitypresets.h", "alenvintensitypresets.cpp", "alsettingsundohistory.cpp",
                 "alpanelexposurehub.cpp", "alpanelexposurepage.cpp", "llfloaterenvironmentadjust.cpp"]:
        text = read("indra/newview/" + path)
        check("preset/history/reset ownership: " + path,
              ("mLocalLightSize" if path.endswith("presets.h") else KEY) in text)
    preset = read("indra/newview/alenvintensitypresets.cpp")
    check("custom preset save/load/default/apply/compare", all(s in preset for s in [
        '&Values::mLocalLightSize', 'v.mLocalLightSize', 'values.mLocalLightSize',
        'nearlyEqualF(a.mLocalLightSize, b.mLocalLightSize)', 'Values v = defaultValues();']))

    util = read(SHADERS + "class1/deferred/deferredUtil.glsl")
    for path, expected in [("class3/deferred/pointLightF.glsl", 1),
                           ("class3/deferred/multiPointLightF.glsl", 1),
                           ("class3/deferred/spotLightF.glsl", 2),
                           ("class1/deferred/deferredUtil.glsl", 2)]:
        src = read(SHADERS + path)
        branches = re.findall(r"if \((?:[^\n]+ > 0.0|size_active)\)\s*localLightPbr\([^;]+;\s*"
                              r"else\s*pbrPunctual\([^;]+;", src)
        check("original PBR off branch: " + path, len(branches) == expected)
        if path != "class1/deferred/deferredUtil.glsl":
            check("literal-zero runtime dispatch: " + path,
                  "if (local_light_size <= 0.0 || no_specular)" in src and
                  ("localLightMain(0.0); return;" in src or "localLightMain(false); return;" in src))
    for path in ["class3/deferred/pointLightF.glsl", "class3/deferred/multiPointLightF.glsl",
                 "class3/deferred/spotLightF.glsl", "class3/deferred/materialF.glsl"]:
        src = read(SHADERS + path)
        check("legacy original LUT fallback: " + path, bool(re.search(
            r"\? localLightLegacy\([^;]+:\s*texture\(lightFunc, vec2\(nh, spec.a\)\).r", src)))
    for path in ["class2/deferred/pbralphaF.glsl", "class2/deferred/sharedActorFxPbrF.glsl"]:
        src = read(SHADERS + path)
        calls = re.findall(r"light_attenuation\[i\].w,\s*(.*?)RELIGHT_DATA\(i\)", src)
        check("all forward/actor/silhouette/parity loops carry radius: " + path,
              len(calls) == 6 and all(s.strip() == "local_light_sizes[i]," for s in calls))
    render = read("indra/llrender/llrender.cpp")
    check("forward radius snapshotted and hash-invalidated", all(s in render for s in [
        "mLocalLightSize != radius", "mLocalLightSize = radius", "++gGL.mLightHash",
        "source_size[i] = light->mLocalLightSize", "LLShaderMgr::LOCAL_LIGHT_SIZES"]))
    pipeline = read("indra/newview/pipeline.cpp")
    check("all four deferred single/spot/actor uploads", pipeline.count(
        "LLShaderMgr::LOCAL_LIGHT_SIZE, ALEnvIntensity::localLightSize(volume)") == 4)
    check("deferred batches carry per-emitter radius and clear staging", all(s in pipeline for s in [
        "source_size[count] = local_light_sizes[i]", "source_sizes[i] = point.source_size",
        "local_light_sizes.clear()", "LOCAL_LIGHT_SIZES, batch_size, source_sizes"]))
    for path in ["llreflectionmapmanager.cpp", "alprobeschedule.h"]:
        check("probe capture invalidation: " + path, "mLocalLightSize" in read("indra/newview/" + path))
    check("GGX lobe normalization present", all(s in util for s in [
        "float norm = alpha / broadened", "(norm * norm) * (shapeRatio * shapeRatio)",
        "spec *= localLightGGXRatio", "mix(2.0, 3.0, cone)",
        "float sourceAlpha = localLightAlpha", "geometricOcclusion(pbrInputs)"]))
    check("Blinn exponent reduction with peak cap", all(s in util for s in [
        "reduced / exponent", "2.0 / (broadened * broadened) - 2.0", "(norm * norm)"]))
    check("existing physical sources also receive specular-only widening", all(s in util for s in [
        "spec *= localLightGGXRatio(nh,alpha,local_size,d,cone)",
        "localLightLegacy(glossLut,lookupNh,spec.a,local_size,length(lv),cone)",
        "shape,spec_keep,local_size,1.0-is_pointlight)"]))
    print(f"{'FAIL' if failures else 'PASS'}: {len(failures)} local-light-size contract failures")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
