#!/usr/bin/env python3
"""Gate for the "double stutter" frame hitch (RenderVBOPoolCleanBeforePresent).

Usage::

    python scripts/code_tools/frame_cadence_check.py                 (source check + selftest)
    python scripts/code_tools/frame_cadence_check.py --check         (source check only)
    python scripts/code_tools/frame_cadence_check.py --selftest      (planted negatives only)
    python scripts/code_tools/frame_cadence_check.py --trace A.csv [B.csv ...]

What went wrong (Tracy capture 2026-11-14, 30 fps cap, driver pacing, VSync on):
SwapBuffers returns at once and the wait for the display is taken by a later GL
call. Normally that is the first render-target bind of the NEXT frame, about
1 ms after that frame sampled its time, so frame time advances evenly. The VBO
pool expiry sweep ran right after the present, and about once a second its
glDeleteBuffers took the wait instead (18-26 ms), before the frame boundary.
That frame measured 56-61 ms and the next one 9-16 ms, while the presents stayed
evenly spaced: the picture jumped and then nearly repeated.

Two halves:

1. Source pins (--check). The sweep runs in swap() before the present; the
   post-present call in display() only runs when swap() did not; nothing on the
   known list of driver-synchronising calls sits between the last swap() and the
   end of display(); the setting is a persisted Boolean, default on.
2. Trace check (--trace). Input: tracy-csvexport -u output (one or more files;
   "-u -f doFrame" plus "-u -f Swap" plus "-u -f sweep" keeps them small). For
   every main-loop frame it measures the POST-PRESENT TAIL (end of the frame's
   last "Swap" zone to the end of LLAppViewer::doFrame). Verdicts:
     FAIL          a tail over TAIL_FAIL_MS, or a pre-swap sweep over SWEEP_FAIL_MS
     PASS          no such frame
     INCONCLUSIVE  too few frames or no Swap zones (exit 2): the trace cannot tell
   It also counts long-then-short frame pairs and says how many a tail explains.

Selftest: the real 11/14 excerpt (frame_cadence_fixture.csv) must FAIL, a
synthetic even trace must PASS, an even trace with one planted 22 ms tail must
FAIL, an empty trace must be INCONCLUSIVE, and every source mutation must be
caught.

Standard library only. Exit 0 pass, 1 finding, 2 missing input / inconclusive.
"""

from __future__ import annotations

import re
import statistics
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DISPLAY_CPP = ROOT / "indra" / "newview" / "llviewerdisplay.cpp"
SETTINGS_XML = ROOT / "indra" / "newview" / "app_settings" / "settings.xml"
FIXTURE = Path(__file__).with_name("frame_cadence_fixture.csv")

SETTING = "RenderVBOPoolCleanBeforePresent"
TAIL_FAIL_MS = 5.0     # healthy frames measured 0.02 ms median, 0.46 ms max; the hitch was 18-26 ms
SWEEP_FAIL_MS = 5.0    # the sweep moved before the present must stay cheap there
MIN_FRAMES = 100
# Calls known to take (or able to take) the driver's present wait. None may sit between the
# last swap() and the end of display().
POST_PRESENT_DENY = (
    "glDeleteBuffers", "glFinish", "glReadPixels", "glClientWaitSync", "glGetTexImage",
    "cleanIfDue", "flushBuffers", "resizeScreenTexture", "setSubImage",
)


# ---------------------------------------------------------------- source pins

def function_body(src: str, signature: str) -> str | None:
    """Text of the body of the first definition matching `signature` (brace matched)."""
    m = re.search(signature + r"\s*\{", src)
    if not m:
        return None
    depth = 0
    for i in range(m.end() - 1, len(src)):
        c = src[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[m.end():i]
    return None


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def source_findings(display_src: str, settings_src: str) -> list[str]:
    out: list[str] = []

    # the setting
    try:
        root = ET.fromstring(settings_src)
        entries = list(root.find("map"))
    except Exception as e:  # noqa: BLE001
        return [f"settings.xml does not parse: {e}"]
    found = None
    for i in range(0, len(entries) - 1, 2):
        if entries[i].tag == "key" and entries[i].text == SETTING:
            found = entries[i + 1]
            break
    if found is None:
        out.append(f"settings.xml: {SETTING} is missing")
    else:
        kv = {}
        items = list(found)
        for i in range(0, len(items) - 1, 2):
            kv[items[i].text] = items[i + 1]
        if kv.get("Type") is None or kv["Type"].text != "Boolean":
            out.append(f"settings.xml: {SETTING} must be a Boolean")
        if kv.get("Persist") is None or kv["Persist"].text != "1":
            out.append(f"settings.xml: {SETTING} must persist")
        if kv.get("Value") is None or kv["Value"].text != "1":
            out.append(f"settings.xml: {SETTING} must default to 1 (the fix on)")

    # the helper
    helper = function_body(display_src, r"static\s+void\s+clean_vbo_pool_before_present\s*\(\s*\)")
    if helper is None:
        out.append("llviewerdisplay.cpp: clean_vbo_pool_before_present() is missing")
    else:
        h = strip_comments(helper)
        for needle, why in (
            ('"RenderVBOPoolCleanAtFrameEnd"', "must honour RenderVBOPoolCleanAtFrameEnd"),
            (f'"{SETTING}"', f"must read {SETTING}"),
            ("LLVertexBuffer::endOfFrameCleanPool(true)", "must run the sweep"),
            ("sVBOPoolSweptBeforePresent = true", "must mark the frame as swept"),
        ):
            if needle not in h:
                out.append(f"llviewerdisplay.cpp: clean_vbo_pool_before_present() {why}")
        if not re.search(r"if\s*\(\s*vbo_clean_at_frame_end\s*&&\s*vbo_clean_before_present\s*\)", h):
            out.append("llviewerdisplay.cpp: the pre-present sweep must be gated on both settings")

    # swap(): the sweep comes before the present
    swap = function_body(display_src, r"\nvoid\s+swap\s*\(\s*\)")
    if swap is None:
        out.append("llviewerdisplay.cpp: swap() is missing")
    else:
        s = strip_comments(swap)
        a = s.find("clean_vbo_pool_before_present();")
        b = s.find("pacedPresent")
        if b < 0:
            out.append("llviewerdisplay.cpp: swap() no longer presents through pacedPresent (re-audit this gate)")
        if a < 0:
            out.append("llviewerdisplay.cpp: swap() does not run the VBO pool sweep before the present")
        elif b >= 0 and a > b:
            out.append("llviewerdisplay.cpp: swap() runs the VBO pool sweep AFTER the present")
        if b >= 0:
            after = s[b:]
            # the text after the pacedPresent(...) call statement
            end = after.find("});")
            tail = after[end + 3:] if end >= 0 else after
            for bad in POST_PRESENT_DENY + ("endOfFrameCleanPool",):
                if bad in tail:
                    out.append(f"llviewerdisplay.cpp: swap() calls {bad} after the present")

    # display(): what follows the last swap()
    disp = function_body(display_src, r"\nvoid\s+display\s*\(\s*bool\s+rebuild[^)]*\)")
    if disp is None:
        out.append("llviewerdisplay.cpp: display() is missing")
    else:
        d = strip_comments(disp)
        last = d.rfind("swap();")
        if last < 0:
            out.append("llviewerdisplay.cpp: display() no longer calls swap() (re-audit this gate)")
        else:
            tail = d[last:]
            for bad in POST_PRESENT_DENY:
                if bad in tail:
                    out.append(f"llviewerdisplay.cpp: display() calls {bad} after the last swap()")
            calls = [m.start() for m in re.finditer(r"endOfFrameCleanPool\s*\(", tail)]
            if len(calls) != 1:
                out.append(f"llviewerdisplay.cpp: expected exactly one post-present endOfFrameCleanPool call in display(), found {len(calls)}")
            for pos in calls:
                before = tail[:pos]
                guard = before.rfind("if (!sVBOPoolSweptBeforePresent)")
                # the guard must open the block that holds the call: no closed block in between
                if guard < 0 or before[guard:].count("}") != 0 or before[guard:].count("{") != 1:
                    out.append("llviewerdisplay.cpp: the post-present endOfFrameCleanPool call is not guarded by if (!sVBOPoolSweptBeforePresent)")
            if calls and "sVBOPoolSweptBeforePresent = false;" not in tail[calls[-1]:]:
                out.append("llviewerdisplay.cpp: display() does not reset sVBOPoolSweptBeforePresent after the post-present sweep")
    return out


# ---------------------------------------------------------------- trace check

def read_zones(paths, wanted):
    """Rows of tracy-csvexport -u: name,src_file,src_line,ns_since_start,exec_time_ns,thread,value."""
    rows = []
    for p in paths:
        with open(p, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                parts = line.rstrip("\r\n").rsplit(",", 6)   # the name may hold commas
                if len(parts) < 7:
                    continue
                name = parts[0].lstrip("﻿").strip('"')
                if name not in wanted:
                    continue
                try:
                    rows.append((int(parts[3]), int(parts[4]), name, parts[5]))
                except ValueError:
                    continue
    return rows


def trace_verdict(rows, label="trace"):
    """Returns (code, lines). code 0 PASS, 1 FAIL, 2 INCONCLUSIVE."""
    import bisect
    lines = []
    frames = sorted({(s, d, th) for s, d, n, th in rows if n == "LLAppViewer::doFrame"})
    if len(frames) < MIN_FRAMES:
        return 2, [f"{label}: INCONCLUSIVE: {len(frames)} LLAppViewer::doFrame zones (need {MIN_FRAMES})"]
    main = frames[0][2]
    frames = [(s, d) for s, d, th in frames if th == main]
    starts = [f[0] for f in frames]
    swaps = sorted({(s, d) for s, d, n, th in rows if n == "Swap" and th == main})
    sweeps = sorted({(s, d) for s, d, n, th in rows if n == "vbo pool sweep (pre-swap)" and th == main})
    swap_end = {}
    for s, d in swaps:
        i = bisect.bisect_right(starts, s) - 1
        if i >= 0 and s < frames[i][0] + frames[i][1]:
            swap_end[i] = max(swap_end.get(i, 0), s + d)
    if len(swap_end) < MIN_FRAMES // 2:
        return 2, [f"{label}: INCONCLUSIVE: {len(swap_end)} frames hold a Swap zone (need {MIN_FRAMES // 2}); export with -u -f Swap"]
    durs = [d / 1e6 for _, d in frames]
    med = statistics.median(durs)
    tails = {i: (frames[i][0] + frames[i][1] - e) / 1e6 for i, e in swap_end.items()}
    bad_tail = sorted(i for i, t in tails.items() if t > TAIL_FAIL_MS)
    bad_sweep = [d / 1e6 for _, d in sweeps if d / 1e6 > SWEEP_FAIL_MS]
    pairs = [i for i in range(len(durs) - 1) if durs[i] > med + 0.4 * med and durs[i + 1] < med - 0.35 * med]
    explained = [i for i in pairs if i in set(bad_tail)]
    tv = sorted(tails.values())
    lines.append(f"{label}: {len(frames)} frames, median {med:.1f} ms; post-present tail median {statistics.median(tv):.2f} ms, "
                 f"p99 {tv[int(len(tv) * 0.99)]:.2f} ms, max {tv[-1]:.2f} ms")
    lines.append(f"{label}: long-then-short frame pairs: {len(pairs)} ({len(explained)} explained by a post-present tail)")
    if sweeps:
        sw = sorted(d / 1e6 for _, d in sweeps)
        lines.append(f"{label}: pre-swap sweep zones: {len(sw)}, max {sw[-1]:.2f} ms")
    else:
        lines.append(f"{label}: no pre-swap sweep zones (an older build, the setting off, or not exported with -f sweep)")
    code = 0
    if bad_tail:
        code = 1
        worst = max(tails[i] for i in bad_tail)
        lines.append(f"{label}: FAIL: {len(bad_tail)} frames spend more than {TAIL_FAIL_MS:.0f} ms between the present and the end "
                     f"of the frame (worst {worst:.1f} ms): the driver's present wait landed before the next frame's time sample")
    if bad_sweep:
        code = 1
        lines.append(f"{label}: FAIL: {len(bad_sweep)} pre-swap sweeps over {SWEEP_FAIL_MS:.0f} ms (worst {max(bad_sweep):.1f} ms): "
                     f"the stall moved in front of the present instead of going away")
    if code == 0:
        lines.append(f"{label}: PASS: no post-present stall" +
                     (f"; {len(pairs)} long-then-short pairs have another cause, look at them" if pairs else ""))
    return code, lines


def synthetic_rows(frames=300, planted_tail_at=None):
    """An even 30 fps trace: [1 ms][22 ms wait][9 ms render][1.3 ms Swap], tail 0.02 ms."""
    rows = []
    t = 0
    ms = 1_000_000
    for i in range(frames):
        dur = 33_333_333
        swap_start = t + dur - int(1.32 * ms)
        swap_dur = int(1.3 * ms)
        if planted_tail_at is not None and i == planted_tail_at:
            dur += 22 * ms                      # the wait lands after the present, inside this frame
        elif planted_tail_at is not None and i == planted_tail_at + 1:
            dur -= 22 * ms                      # ... so the next frame has no wait at all
            swap_start = t + dur - int(1.32 * ms)
        rows.append((t, dur, "LLAppViewer::doFrame", "1"))
        rows.append((swap_start, swap_dur, "Swap", "1"))
        rows.append((swap_start - 50_000, 20_000, "vbo pool sweep (pre-swap)", "1"))
        t += dur + 2_000
    return rows


WANTED = {"LLAppViewer::doFrame", "Swap", "vbo pool sweep (pre-swap)"}


# ---------------------------------------------------------------- selftest

def selftest() -> list[str]:
    out: list[str] = []
    display_src = DISPLAY_CPP.read_text(encoding="utf-8")
    settings_src = SETTINGS_XML.read_text(encoding="utf-8")

    def mutate(src, old, new, count=1):
        if src.count(old) < 1:
            out.append(f"selftest: mutation anchor not found: {old[:60]!r}")
            return src
        return src.replace(old, new, count)

    setting_block = settings_src[settings_src.find(f"<key>{SETTING}</key>"):]
    setting_block = setting_block[:setting_block.find("</map>")]
    negatives = [
        ("sweep removed from swap()",
         mutate(display_src, "    clean_vbo_pool_before_present();\n", "", 1)
         if "    clean_vbo_pool_before_present();\n" in display_src
         else mutate(display_src, "    clean_vbo_pool_before_present();\r\n", "", 1), settings_src),
        ("post-present sweep unguarded",
         mutate(display_src, "    if (!sVBOPoolSweptBeforePresent)", "    "), settings_src),
        ("flag never reset",
         mutate(display_src, "    sVBOPoolSweptBeforePresent = false;", "    "), settings_src),
        ("helper ignores the new setting",
         mutate(display_src, "if (vbo_clean_at_frame_end && vbo_clean_before_present)", "if (vbo_clean_at_frame_end)"), settings_src),
        ("helper never marks the frame",
         mutate(display_src, "        sVBOPoolSweptBeforePresent = true;", "        "), settings_src),
        ("a buffer flush after the last swap()",
         mutate(display_src, "    sVBOPoolSweptBeforePresent = false;", "    sVBOPoolSweptBeforePresent = false;\n    LLVertexBuffer::flushBuffers();"), settings_src),
        ("setting defaults off",
         display_src, settings_src.replace(setting_block, setting_block.replace("<integer>1</integer>\n    ", "<integer>0</integer>\n    ").replace("<key>Value</key>\n      <integer>1</integer>", "<key>Value</key>\n      <integer>0</integer>").replace("<key>Value</key>\r\n      <integer>1</integer>", "<key>Value</key>\r\n      <integer>0</integer>"))),
        ("setting missing",
         display_src, settings_src.replace(f"<key>{SETTING}</key>", "<key>RenderVBOPoolCleanBeforePresentX</key>")),
    ]
    for name, d, s in negatives:
        if not source_findings(d, s):
            out.append(f"selftest: planted negative NOT caught: {name}")

    # traces
    if not FIXTURE.exists():
        out.append(f"selftest: fixture missing: {FIXTURE}")
    else:
        code, lines = trace_verdict(read_zones([FIXTURE], WANTED), "fixture 11/14")
        if code != 1 or not any("FAIL" in l and "between the present" in l for l in lines):
            out.append("selftest: the real 11/14 excerpt must FAIL the trace check, got: " + " | ".join(lines))
    code, lines = trace_verdict(synthetic_rows(), "synthetic even")
    if code != 0:
        out.append("selftest: an even trace must PASS, got: " + " | ".join(lines))
    code, lines = trace_verdict(synthetic_rows(planted_tail_at=120), "synthetic planted")
    if code != 1:
        out.append("selftest: one planted 22 ms post-present tail must FAIL, got: " + " | ".join(lines))
    rows = synthetic_rows()
    rows = [(s, 9_000_000 if n.startswith("vbo") and s > 5_000_000_000 and s < 5_040_000_000 else d, n, th) for s, d, n, th in rows]
    code, lines = trace_verdict(rows, "synthetic slow sweep")
    if code != 1:
        out.append("selftest: a 9 ms pre-swap sweep must FAIL, got: " + " | ".join(lines))
    code, lines = trace_verdict([], "empty")
    if code != 2:
        out.append("selftest: an empty trace must be INCONCLUSIVE")
    code, lines = trace_verdict([r for r in synthetic_rows() if r[2] != "Swap"], "no swap zones")
    if code != 2:
        out.append("selftest: a trace without Swap zones must be INCONCLUSIVE")
    return out


def main(argv: list[str]) -> int:
    if "--trace" in argv:
        paths = [a for a in argv[argv.index("--trace") + 1:] if not a.startswith("--")]
        if not paths or any(not Path(p).exists() for p in paths):
            print("frame_cadence_check: --trace needs existing tracy-csvexport -u files")
            return 2
        code, lines = trace_verdict(read_zones(paths, WANTED))
        print("\n".join(lines))
        return code
    for p in (DISPLAY_CPP, SETTINGS_XML):
        if not p.exists():
            print(f"frame_cadence_check: missing {p}")
            return 2
    findings: list[str] = []
    if "--selftest" not in argv:
        findings += source_findings(DISPLAY_CPP.read_text(encoding="utf-8"), SETTINGS_XML.read_text(encoding="utf-8"))
    if "--check" not in argv:
        findings += selftest()
    for f in findings:
        print("FINDING:", f)
    print("frame_cadence_check:", "FAIL (%d findings)" % len(findings) if findings else "PASS")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
