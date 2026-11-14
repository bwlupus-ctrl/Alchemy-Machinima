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

1. Source pins (--check). Exact pins; comments, whitespace and spacing around
   punctuation do not matter:
   - clean_vbo_pool_before_present() is exactly: read the frame number, return
     when that frame was already swept, both settings read, ONE block gated on
     both that holds the Tracy zone, the sweep and the frame stamp, in that
     order;
   - "swept" is a frame stamp (sVBOPoolSweepFrame == LLFrameTimer::getFrameCount()),
     never a flag that something has to clear: swap() is also called outside
     display() (the forward snapshot path), and a flag set there outlived its
     frame and cancelled the next frame's sweep;
   - swap() is exactly: the helper call, then the paced present, then the flag
     for the next frame; nothing else, so nothing can follow the present there;
   - display() from its last swap() to display_stats() is exactly the known
     text, with the post-present sweep inside
     if (sVBOPoolSweepFrame != LLFrameTimer::getFrameCount());
   - exact counts in the file (helper call 1, endOfFrameCleanPool 3, stamp
     assignments 2 = the initial value and the helper's stamp, swap() sites in
     display() 3), so a duplicate, a reset or a new site is a finding until it
     is audited;
   - nothing on the list of calls known to take the driver's present wait (a
     buffer delete, a render-target or framebuffer bind, a flush, a texture
     upload, a readback) between the last swap() and the end of display();
   - the setting is a persisted Boolean, default on.
   The deny-list is textual: it cannot see into functions called there. The
   trace check is the backstop for that.
2. Trace check (--trace). Input: tracy-csvexport -u output, one or more files.
   Small exports are enough: -u -f doFrame, -u -f Swap, -u -f sweep,
   -u -f cleanIfDue, -u -f delete_buffers, -u -f timeout. For every main-loop
   frame it measures the POST-PRESENT TAIL (end of the frame's last "Swap" zone
   to the end of LLAppViewer::doFrame).
   POOL EXPIRY means the pool sweep only: a "vbo cache timeout" zone, or a
   delete_buffers zone nested inside LLDefaultVBOPool::cleanIfDue, a
   "vbo cache timeout" zone or the sweep zone. delete_buffers has other callers
   (buffer destruction); those are not pool expiry and never fail the check by
   themselves (the tail check still covers them if they stall).
   A sweep zone is PRE-SWAP only when its whole interval ends before the start
   of the first Swap zone of its frame.
   Verdicts:
     FAIL          a tail over TAIL_FAIL_MS; a pre-swap sweep over SWEEP_FAIL_MS;
                   a sweep zone that is not before its frame's Swap; or a pool
                   expiry that ran after the present
     INCONCLUSIVE  (exit 2) too few frames, no Swap zones, pre-swap sweep zones
                   missing from the presented frames, or no pool expiry seen
                   INSIDE a pre-swap sweep: the trace never exercised the path
                   that stalled, so a clean tail proves nothing
     PASS          none of the above
   It also counts long-then-short frame pairs and says how many a tail explains.

Selftest: the real 11/14 excerpt (frame_cadence_fixture.csv) must FAIL; a
synthetic even trace with expiry inside the pre-swap sweep must PASS, also with
an unrelated delete_buffers after the present; planted tail, slow sweep,
misplaced sweep and post-present pool expiry must FAIL; no sweep zones, sweep
zones without expiry, an empty trace and a trace without Swap zones must be
INCONCLUSIVE; every source mutation must be caught by the finding named next to
it (one mutation each).

Standard library only. Exit 0 pass, 1 finding, 2 missing input / inconclusive.
"""

from __future__ import annotations

import bisect
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
SWEEP_COVERAGE = 0.9   # share of presented frames that must hold a pre-swap sweep zone
MIN_EXPIRY_IN_SWEEP = 1

# Calls known to take (or able to take) the driver's present wait: the 11/14 capture showed it on
# glDeleteBuffers (the hitch), on a render-target / framebuffer bind (every normal frame) and on a
# texture upload. None may sit between the last swap() and the end of display().
POST_PRESENT_DENY = (
    "glDeleteBuffers", "glFinish", "glReadPixels", "glClientWaitSync", "glGetTexImage",
    "cleanIfDue", "flushBuffers", "resizeScreenTexture", "setSubImage", "glTexSubImage",
    "bindTarget", "bindFramebuffer", "BindFramebuffer", "glBlitFramebuffer", "copyContents",
    "gGL.flush", "glFlush", ".flush(", "glClear(",
)

# ---- exact pins (compared after norm(): comments stripped, whitespace collapsed, no space around punctuation) ----
HELPER_PIN = (
    'const U32 frame = LLFrameTimer::getFrameCount(); '
    'if (sVBOPoolSweepFrame == frame) { return; } '
    'static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, "RenderVBOPoolCleanAtFrameEnd", true); '
    'static LLCachedControl<bool> vbo_clean_before_present(gSavedSettings, "RenderVBOPoolCleanBeforePresent", true); '
    'if (vbo_clean_at_frame_end && vbo_clean_before_present) { '
    'LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("vbo pool sweep (pre-swap)"); '
    'LLVertexBuffer::endOfFrameCleanPool(true); '
    'sVBOPoolSweepFrame = frame; }'
)
SWAP_PIN = (
    'clean_vbo_pool_before_present(); '
    'if (gDisplaySwapBuffers) { ALFrameLimiter::pacedPresent([] { '
    'LLPerfStats::RecordSceneTime T ( ALFrameLimiter::driverPacing() ? LLPerfStats::StatType_t::RENDER_SLEEP '
    ': LLPerfStats::StatType_t::RENDER_SWAP ); '
    'LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Swap"); LL_PROFILE_GPU_ZONE("swap"); '
    'gViewerWindow->getWindow()->swapBuffers(); }); } '
    'gDisplaySwapBuffers = true;'
)
TAIL_SWEEP_PIN = (
    'if (sVBOPoolSweepFrame != LLFrameTimer::getFrameCount()) { '
    'static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, "RenderVBOPoolCleanAtFrameEnd", true); '
    'LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end); }'
)
TAIL_PIN = (
    'swap(); } } if (!display_cleanup_done) { LLSpatialGroup::sNoDelete = false; gPipeline.clearReferences(); } } '
    'LLAppViewer::instance()->pingMainloopTimeout("Display:FrameStats"); stop_glerror(); '
    + TAIL_SWEEP_PIN + ' display_stats();'
)
DECL_PIN = 'static U32 sVBOPoolSweepFrame = U32_MAX;'
# regex (on the normalised file) -> exact count
FILE_COUNTS = (
    (r"clean_vbo_pool_before_present\(", 2, "helper", "one definition and one call (in swap())"),
    (r"endOfFrameCleanPool\(", 3, "endOfFrameCleanPool", "the minimised path, the pre-present helper and the guarded post-present call"),
    (r"sVBOPoolSweepFrame=(?!=)", 2, "stamp assignment", "the initial value and the helper's stamp; nothing may reset it"),
    (r"sVBOPoolSweepFrame", 4, "stamp use", "declaration, helper test, helper stamp, display() test"),
    (r"sVBOPoolSweptBeforePresent", 0, "old flag", "the per-frame bool is gone; a flag can outlive its frame"),
)
DISPLAY_SWAP_SITES = 3


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


def norm(text: str) -> str:
    """Comments out, whitespace collapsed, no whitespace next to punctuation ('swap ();' == 'swap();')."""
    t = " ".join(strip_comments(text).split())
    return re.sub(r"\s*([^\w\s])\s*", r"\1", t)


def setting_findings(settings_src: str) -> list[str]:
    out: list[str] = []
    try:
        entries = list(ET.fromstring(settings_src).find("map"))
    except Exception as e:  # noqa: BLE001
        return [f"settings.xml does not parse: {e}"]
    found = [entries[i + 1] for i in range(0, len(entries) - 1, 2)
             if entries[i].tag == "key" and entries[i].text == SETTING]
    if len(found) != 1:
        return [f"settings.xml: {SETTING} must be defined exactly once (found {len(found)})"]
    items = list(found[0])
    kv = {items[i].text: items[i + 1] for i in range(0, len(items) - 1, 2)}
    if kv.get("Type") is None or kv["Type"].text != "Boolean":
        out.append(f"settings.xml: {SETTING} must be a Boolean")
    if kv.get("Persist") is None or kv["Persist"].text != "1":
        out.append(f"settings.xml: {SETTING} must persist")
    if kv.get("Value") is None or kv["Value"].text != "1":
        out.append(f"settings.xml: {SETTING} must default to 1 (the fix on)")
    return out


def source_findings(display_src: str, settings_src: str) -> list[str]:
    out = setting_findings(settings_src)
    F = "llviewerdisplay.cpp: "

    helper = function_body(display_src, r"static\s+void\s+clean_vbo_pool_before_present\s*\(\s*\)")
    if helper is None:
        out.append(F + "clean_vbo_pool_before_present() is missing")
    elif norm(helper) != norm(HELPER_PIN):
        out.append(F + "HELPER PIN: clean_vbo_pool_before_present() is not exactly [frame number; return when that frame "
                       "was swept; both settings; one block gated on both holding the zone, the sweep and the frame stamp]")

    swap = function_body(display_src, r"\nvoid\s+swap\s*\(\s*\)")
    if swap is None:
        out.append(F + "swap() is missing")
    elif norm(swap) != norm(SWAP_PIN):
        out.append(F + "SWAP PIN: swap() is not exactly [pre-present sweep; paced present; gDisplaySwapBuffers = true] "
                       "(the sweep must come first and once, and nothing may follow the present)")

    disp = function_body(display_src, r"\nvoid\s+display\s*\(\s*bool\s+rebuild[^)]*\)")
    if disp is None:
        out.append(F + "display() is missing")
    else:
        d = norm(disp)
        sites = len(re.findall(r"(?<![\w.>:])swap\(\);", d))
        if sites != DISPLAY_SWAP_SITES:
            out.append(F + f"SWAP SITES: display() calls swap() {sites} times, expected {DISPLAY_SWAP_SITES} (audit the new site)")
        last = d.rfind("swap();")
        if last < 0:
            out.append(F + "display() no longer calls swap() (re-audit this gate)")
        else:
            tail = d[last:]
            if not tail.startswith(norm(TAIL_PIN)):
                out.append(F + "TAIL PIN: display() from its last swap() to display_stats() is not exactly the audited text "
                               "(the post-present sweep must sit inside if (sVBOPoolSweepFrame != LLFrameTimer::getFrameCount()), "
                               "once, and nothing new may run there)")
            for bad in POST_PRESENT_DENY:
                if bad in tail:
                    out.append(F + f"DENY: display() calls {bad} after the last swap() (it can take the driver's present wait)")

    whole = norm(display_src)
    if whole.count(norm(DECL_PIN)) != 1:
        out.append(F + "DECL PIN: the sweep stamp must be declared exactly once as 'static U32 sVBOPoolSweepFrame = U32_MAX;'")
    for pattern, want, name, why in FILE_COUNTS:
        got = len(re.findall(pattern, whole))
        if got != want:
            out.append(F + f"COUNT [{name}]: found {got}, expected {want} ({why})")
    return out


# ---------------------------------------------------------------- trace check

FRAME = "LLAppViewer::doFrame"
SWAP = "Swap"
SWEEP = "vbo pool sweep (pre-swap)"
CLEAN = "LLDefaultVBOPool::cleanIfDue"
TIMEOUT = "vbo cache timeout"
DELETE = "delete_buffers"
WANTED = {FRAME, SWAP, SWEEP, CLEAN, TIMEOUT, DELETE}
POOL_ZONES = (SWEEP, CLEAN, TIMEOUT, DELETE)


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


class Intervals:
    """Sorted zones; inside(t) = index of the zone holding time t, or -1 (zones of one name do not overlap)."""

    def __init__(self, zones):
        self.zones = sorted(set(zones))
        self.starts = [s for s, _ in self.zones]

    def inside(self, t):
        k = bisect.bisect_right(self.starts, t) - 1
        return k if k >= 0 and t < self.zones[k][0] + self.zones[k][1] else -1


def trace_verdict(rows, label="trace"):
    """Returns (code, lines). code 0 PASS, 1 FAIL, 2 INCONCLUSIVE."""
    lines = []
    frames = sorted({(s, d, th) for s, d, n, th in rows if n == FRAME})
    if len(frames) < MIN_FRAMES:
        return 2, [f"{label}: INCONCLUSIVE: {len(frames)} {FRAME} zones (need {MIN_FRAMES})"]
    main = frames[0][2]
    frames = [(s, d) for s, d, th in frames if th == main]
    starts = [f[0] for f in frames]

    def frame_of(t):
        i = bisect.bisect_right(starts, t) - 1
        return i if i >= 0 and t < frames[i][0] + frames[i][1] else -1

    def zones(name):
        return sorted({(s, d) for s, d, n, th in rows if n == name and th == main})

    swap_first = {}   # frame -> start of its first Swap zone
    swap_end = {}     # frame -> end of its last Swap zone
    for s, d in zones(SWAP):
        i = frame_of(s)
        if i >= 0:
            swap_first[i] = min(swap_first.get(i, s), s)
            swap_end[i] = max(swap_end.get(i, 0), s + d)
    if len(swap_end) < MIN_FRAMES // 2:
        return 2, [f"{label}: INCONCLUSIVE: {len(swap_end)} frames hold a Swap zone (need {MIN_FRAMES // 2}); export with -u -f Swap"]

    durs = [d / 1e6 for _, d in frames]
    med = statistics.median(durs)
    tails = {i: (frames[i][0] + frames[i][1] - e) / 1e6 for i, e in swap_end.items()}
    bad_tail = sorted(i for i, t in tails.items() if t > TAIL_FAIL_MS)
    pairs = [i for i in range(len(durs) - 1) if durs[i] > 1.4 * med and durs[i + 1] < 0.65 * med]
    explained = [i for i in pairs if i in set(bad_tail)]

    # sweep zones: pre-swap only when the whole zone ends before the first Swap of its frame
    pre_sweeps = []
    misplaced = 0
    for s, d in zones(SWEEP):
        i = frame_of(s)
        if i < 0 or i not in swap_first:
            continue                      # a frame that did not present: nothing to be before or after
        if s + d <= swap_first[i]:
            pre_sweeps.append((s, d, i))
        else:
            misplaced += 1
    pre = Intervals([(s, d) for s, d, _ in pre_sweeps])
    swept_frames = {i for _, _, i in pre_sweeps}
    bad_sweep = [d / 1e6 for s, d, _ in pre_sweeps if d / 1e6 > SWEEP_FAIL_MS]

    # pool expiry = the pool sweep only: a timeout zone, or a delete_buffers nested in the sweep's own zones
    any_sweep = Intervals(zones(SWEEP))
    clean = Intervals(zones(CLEAN))
    timeouts = zones(TIMEOUT)
    timeout_iv = Intervals(timeouts)
    pool_expiry = [s for s, _ in timeouts]
    other_deletes_after_present = 0
    for s, d in zones(DELETE):
        if clean.inside(s) >= 0 or timeout_iv.inside(s) >= 0 or any_sweep.inside(s) >= 0:
            pool_expiry.append(s)
        else:
            i = frame_of(s)
            if i in swap_first and s >= swap_first[i]:
                other_deletes_after_present += 1
    expiry_in_sweep = expiry_after_present = expiry_elsewhere = 0
    for s in pool_expiry:
        if pre.inside(s) >= 0:
            expiry_in_sweep += 1
            continue
        i = frame_of(s)
        if i in swap_first and s >= swap_first[i]:
            expiry_after_present += 1
        else:
            expiry_elsewhere += 1

    tv = sorted(tails.values())
    lines.append(f"{label}: {len(frames)} frames, median {med:.1f} ms; post-present tail median {statistics.median(tv):.2f} ms, "
                 f"p99 {tv[int(len(tv) * 0.99)]:.2f} ms, max {tv[-1]:.2f} ms")
    lines.append(f"{label}: long-then-short frame pairs: {len(pairs)} ({len(explained)} explained by a post-present tail)")
    lines.append(f"{label}: pre-swap sweep zones in {len(swept_frames)} of {len(swap_end)} presented frames"
                 + (f", max {max(d for _, d, _ in pre_sweeps) / 1e6:.2f} ms" if pre_sweeps else "")
                 + f"; sweep zones not before their frame's Swap: {misplaced}")
    lines.append(f"{label}: pool expiry zones: {expiry_in_sweep} inside a pre-swap sweep, {expiry_after_present} after the present, "
                 f"{expiry_elsewhere} elsewhere; other delete_buffers after the present (not pool expiry, not a finding): "
                 f"{other_deletes_after_present}")

    fail = False
    if bad_tail:
        fail = True
        worst = max(tails[i] for i in bad_tail)
        lines.append(f"{label}: FAIL: {len(bad_tail)} frames spend more than {TAIL_FAIL_MS:.0f} ms between the present and the end "
                     f"of the frame (worst {worst:.1f} ms): the driver's present wait landed before the next frame's time sample")
    if bad_sweep:
        fail = True
        lines.append(f"{label}: FAIL: {len(bad_sweep)} pre-swap sweeps over {SWEEP_FAIL_MS:.0f} ms (worst {max(bad_sweep):.1f} ms): "
                     f"the stall moved in front of the present instead of going away")
    if misplaced:
        fail = True
        lines.append(f"{label}: FAIL: {misplaced} sweep zones do not end before the start of their frame's Swap: "
                     f"the sweep is not before the present")
    if expiry_after_present:
        fail = True
        lines.append(f"{label}: FAIL: the VBO pool expiry ran after the present {expiry_after_present} times "
                     f"(RenderVBOPoolCleanBeforePresent off, or the sweep is back behind the swap)")
    if fail:
        return 1, lines

    inconclusive = []
    if len(swept_frames) < SWEEP_COVERAGE * len(swap_end):
        inconclusive.append(f"pre-swap sweep zones in only {len(swept_frames)} of {len(swap_end)} presented frames "
                            f"(an older build, the setting off, or not exported with -u -f sweep)")
    if expiry_in_sweep < MIN_EXPIRY_IN_SWEEP:
        inconclusive.append("no pool expiry (vbo cache timeout, or delete_buffers inside the sweep) ran inside a pre-swap sweep: "
                            "the capture never exercised the path that stalled (capture longer with scene churn, and export "
                            "-u -f timeout, -u -f cleanIfDue and -u -f delete_buffers)")
    if inconclusive:
        for why in inconclusive:
            lines.append(f"{label}: INCONCLUSIVE: {why}")
        return 2, lines
    lines.append(f"{label}: PASS: the sweep ran before the present, expired buffers there and no frame stalled after its present"
                 + (f"; {len(pairs)} long-then-short pairs have another cause, look at them" if pairs else ""))
    return 0, lines


def synthetic_rows(frames=300, planted_tail_at=None, sweeps=True, expiry=True, pool_expiry_after_present_at=None,
                   misplaced_sweep_at=None, unrelated_delete_after_present_at=None):
    """An even 30 fps trace: [1 ms][22 ms wait][9 ms render][sweep][1.3 ms Swap], tail 0.02 ms."""
    rows = []
    t = 0
    ms = 1_000_000
    for i in range(frames):
        dur = 33_333_333
        if planted_tail_at is not None and i == planted_tail_at:
            swap_start = t + dur - int(1.32 * ms)
            dur += 22 * ms                      # the wait lands after the present, inside this frame
        elif planted_tail_at is not None and i == planted_tail_at + 1:
            dur -= 22 * ms                      # ... so the next frame has no wait at all
            swap_start = t + dur - int(1.32 * ms)
        else:
            swap_start = t + dur - int(1.32 * ms)
        swap_dur = int(1.3 * ms)
        after = swap_start + swap_dur           # 20 us of frame are left after the present
        rows.append((t, dur, FRAME, "1"))
        rows.append((swap_start, swap_dur, SWAP, "1"))

        def sweep_at(at, length, with_expiry):
            rows.append((at, length, SWEEP, "1"))
            if with_expiry:
                rows.append((at + 1_000, length - 2_000, CLEAN, "1"))
                rows.append((at + 2_000, length - 4_000, TIMEOUT, "1"))
                rows.append((at + 3_000, length - 6_000, DELETE, "1"))

        if sweeps:
            if misplaced_sweep_at is not None and i == misplaced_sweep_at:
                sweep_at(after + 1_000, 12_000, True)          # short, but behind the present
            else:
                sweep_at(swap_start - 50_000, 20_000, expiry and i % 40 == 7)
        if pool_expiry_after_present_at is not None and i == pool_expiry_after_present_at:
            rows.append((after + 1_000, 12_000, CLEAN, "1"))
            rows.append((after + 2_000, 8_000, TIMEOUT, "1"))
            rows.append((after + 3_000, 5_000, DELETE, "1"))
        if unrelated_delete_after_present_at is not None and i == unrelated_delete_after_present_at:
            rows.append((after + 2_000, 5_000, DELETE, "1"))   # a buffer destructor, not the pool sweep
        t += dur + 2_000
    return rows


# ---------------------------------------------------------------- selftest

def selftest() -> list[str]:
    out: list[str] = []
    display_src = DISPLAY_CPP.read_text(encoding="utf-8")
    settings_src = SETTINGS_XML.read_text(encoding="utf-8")

    HELPER_CALL = "    clean_vbo_pool_before_present();\n"
    STAMP = "        sVBOPoolSweepFrame = frame;\n"
    HELPER_BLOCK = ("    if (vbo_clean_at_frame_end && vbo_clean_before_present)\n    {\n"
                    "        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY(\"vbo pool sweep (pre-swap)\");\n"
                    "        LLVertexBuffer::endOfFrameCleanPool(true);\n" + STAMP + "    }\n")
    FRAME_READ = "    const U32 frame = LLFrameTimer::getFrameCount();\n"
    EARLY_RETURN = "    if (sVBOPoolSweepFrame == frame)\n    {\n        return;\n    }\n"
    TAIL_IF = "    if (sVBOPoolSweepFrame != LLFrameTimer::getFrameCount())\n"
    TAIL_BLOCK = (TAIL_IF + "    {\n"
                  "        static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, \"RenderVBOPoolCleanAtFrameEnd\", true);\n"
                  "        LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end);\n    }\n")
    DECL = "static U32 sVBOPoolSweepFrame = U32_MAX;\n"
    PRESENT_END = "        });\n    }\n    gDisplaySwapBuffers = true;\n"
    AFTER_TAIL = "    gShiftFrame = false;\n\n    if (gShaderProfileFrame)\n"
    LAST_SWAP = ("                // tail for the live A/B off path and deferred snapshots.\n"
                 "                swap();\n")
    for name, anchor in (("HELPER_CALL", HELPER_CALL), ("HELPER_BLOCK", HELPER_BLOCK), ("EARLY_RETURN", EARLY_RETURN),
                         ("FRAME_READ", FRAME_READ), ("TAIL_BLOCK", TAIL_BLOCK), ("DECL", DECL),
                         ("PRESENT_END", PRESENT_END), ("AFTER_TAIL", AFTER_TAIL), ("LAST_SWAP", LAST_SWAP), ("STAMP", STAMP)):
        if display_src.count(anchor) != 1:
            out.append(f"selftest: mutation anchor {name} found {display_src.count(anchor)} times in llviewerdisplay.cpp (need 1)")
    if out:
        return out

    def sub(old, new):
        return display_src.replace(old, new, 1)

    def after_tail(stmt):
        return sub(AFTER_TAIL, "    gShiftFrame = false;\n    " + stmt + "\n\n    if (gShaderProfileFrame)\n")

    setting_at = settings_src.find(f"<key>{SETTING}</key>")
    setting_end = settings_src.find("</map>", setting_at)
    block = settings_src[setting_at:setting_end]
    value_off = re.sub(r"(<key>Value</key>\s*<integer>)1(</integer>)", r"\g<1>0\g<2>", block)
    not_persisted = re.sub(r"(<key>Persist</key>\s*<integer>)1(</integer>)", r"\g<1>0\g<2>", block)
    if value_off == block or not_persisted == block:
        return ["selftest: could not build the settings mutations"]

    # the round-1 design (a bool set by the helper and cleared at the end of display()): the forward snapshot
    # path's own swap() set it after display()'s reset, so the next frame skipped both sweeps
    bool_flag = (display_src
                 .replace(DECL, "static bool sVBOPoolSweptBeforePresent = false;\n", 1)
                 .replace(FRAME_READ, "", 1)
                 .replace(EARLY_RETURN, "    if (sVBOPoolSweptBeforePresent)\n    {\n        return;\n    }\n", 1)
                 .replace(STAMP, "        sVBOPoolSweptBeforePresent = true;\n", 1)
                 .replace(TAIL_BLOCK, TAIL_BLOCK.replace(TAIL_IF, "    if (!sVBOPoolSweptBeforePresent)\n")
                          + "    sVBOPoolSweptBeforePresent = false;\n", 1))

    # (name, display source, settings source, the finding that must catch it)
    negatives = [
        ("sweep removed from swap()", sub(HELPER_CALL, ""), settings_src, "SWAP PIN"),
        ("helper called twice in swap()", sub(HELPER_CALL, HELPER_CALL + HELPER_CALL), settings_src, "SWAP PIN"),
        ("helper called after the present",
         sub(HELPER_CALL, "").replace(PRESENT_END, PRESENT_END + HELPER_CALL, 1), settings_src, "SWAP PIN"),
        ("a second sweep after the present in swap()",
         sub(PRESENT_END, PRESENT_END + "    LLVertexBuffer::endOfFrameCleanPool(true);\n"), settings_src, "SWAP PIN"),
        ("empty gated block, then an unconditional sweep (helper)",
         sub(HELPER_BLOCK, "    if (vbo_clean_at_frame_end && vbo_clean_before_present)\n    {\n    }\n"
                           "    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY(\"vbo pool sweep (pre-swap)\");\n"
                           "    LLVertexBuffer::endOfFrameCleanPool(true);\n"
                           "    sVBOPoolSweepFrame = frame;\n"), settings_src, "HELPER PIN"),
        ("helper ignores the new setting",
         sub("if (vbo_clean_at_frame_end && vbo_clean_before_present)", "if (vbo_clean_at_frame_end)"), settings_src, "HELPER PIN"),
        ("helper ignores RenderVBOPoolCleanAtFrameEnd",
         sub("if (vbo_clean_at_frame_end && vbo_clean_before_present)", "if (vbo_clean_before_present)"), settings_src, "HELPER PIN"),
        ("helper never stamps the frame", sub(STAMP, ""), settings_src, "HELPER PIN"),
        ("helper stamps the frame outside the gated block",
         sub(STAMP + "    }\n", "    }\n    sVBOPoolSweepFrame = frame;\n"), settings_src, "HELPER PIN"),
        ("helper stamps a value that is not this frame",
         sub(STAMP, "        sVBOPoolSweepFrame = frame + 1;\n"), settings_src, "HELPER PIN"),
        ("helper sweeps twice",
         sub("        LLVertexBuffer::endOfFrameCleanPool(true);\n",
             "        LLVertexBuffer::endOfFrameCleanPool(true);\n        LLVertexBuffer::endOfFrameCleanPool(true);\n"),
         settings_src, "HELPER PIN"),
        ("early return removed (not once per frame)", sub(EARLY_RETURN, ""), settings_src, "HELPER PIN"),
        ("helper called from a second place", after_tail("clean_vbo_pool_before_present();"), settings_src, "COUNT [helper]"),
        ("the round-1 bool flag (stale after a snapshot's own swap())", bool_flag, settings_src, "COUNT [old flag]"),
        ("stamp reset at the end of display() (a flag again)",
         after_tail("sVBOPoolSweepFrame = U32_MAX;"), settings_src, "COUNT [stamp assignment]"),
        ("stamp is not a U32_MAX-initialised U32",
         sub(DECL, "static U32 sVBOPoolSweepFrame = 0;\n"), settings_src, "DECL PIN"),
        ("post-present test not tied to the frame number",
         sub(TAIL_IF, "    if (sVBOPoolSweepFrame == U32_MAX)\n"), settings_src, "TAIL PIN"),
        ("post-present sweep unguarded", sub(TAIL_IF, ""), settings_src, "TAIL PIN"),
        ("empty guard, then an unconditional post-present sweep",
         sub(TAIL_BLOCK, TAIL_IF + "    {\n    }\n"
                         "    static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, \"RenderVBOPoolCleanAtFrameEnd\", true);\n"
                         "    LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end);\n"), settings_src, "TAIL PIN"),
        ("a second post-present sweep later in display()",
         after_tail("LLVertexBuffer::endOfFrameCleanPool(true);"), settings_src, "COUNT [endOfFrameCleanPool]"),
        ("a render-target bind after the last swap()",
         after_tail("gPipeline.mRT->deferredScreen.bindTarget();"), settings_src, "DENY: display() calls bindTarget"),
        ("a framebuffer bind after the last swap()",
         after_tail("glBindFramebuffer(GL_FRAMEBUFFER, 0);"), settings_src, "DENY: display() calls BindFramebuffer"),
        ("gGL.flush() after the last swap()", after_tail("gGL.flush();"), settings_src, "DENY: display() calls gGL.flush"),
        ("a buffer flush after the last swap()",
         after_tail("LLVertexBuffer::flushBuffers();"), settings_src, "DENY: display() calls flushBuffers"),
        ("a buffer delete after the last swap()",
         after_tail("glDeleteBuffers(1, &gFrameHitchSelftestName);"), settings_src, "DENY: display() calls glDeleteBuffers"),
        ("a bind right after the last swap()",
         sub(LAST_SWAP, LAST_SWAP + "                gPipeline.mRT->deferredScreen.bindTarget();\n"), settings_src, "TAIL PIN"),
        ("a new swap() site in display()",
         sub(LAST_SWAP, LAST_SWAP + "                swap();\n"), settings_src, "SWAP SITES"),
        ("setting defaults off", display_src, settings_src.replace(block, value_off, 1), "must default to 1"),
        ("setting not persisted", display_src, settings_src.replace(block, not_persisted, 1), "must persist"),
        ("setting missing", display_src,
         settings_src.replace(f"<key>{SETTING}</key>", f"<key>{SETTING}X</key>", 1), "exactly once"),
    ]
    for name, d, s, want in negatives:
        if d == display_src and s == settings_src:
            out.append(f"selftest: mutation did not apply: {name}")
            continue
        got = source_findings(d, s)
        if not any(want in g for g in got):
            out.append(f"selftest: planted negative NOT caught by [{want}]: {name} (findings: {got})")

    # harmless spacing must not trip the pins
    spaced = (display_src.replace(HELPER_CALL, "    clean_vbo_pool_before_present ( ) ;\n", 1)
              .replace(LAST_SWAP, LAST_SWAP.replace("swap();", "swap ();"), 1)
              .replace(STAMP, "        sVBOPoolSweepFrame=frame ;\n", 1))
    if spaced == display_src:
        out.append("selftest: the spacing variant did not apply")
    else:
        got = source_findings(spaced, settings_src)
        if got:
            out.append(f"selftest: harmless spacing around punctuation must not be a finding: {got}")

    # traces
    def expect(label, rows, code_want, needle=None):
        code, lines = trace_verdict(rows, label)
        if code != code_want or (needle and not any(needle in l for l in lines)):
            out.append(f"selftest: trace '{label}' expected exit {code_want}" + (f" with [{needle}]" if needle else "")
                       + f", got {code}: " + " | ".join(lines))

    if not FIXTURE.exists():
        out.append(f"selftest: fixture missing: {FIXTURE}")
    else:
        expect("fixture 11/14", read_zones([FIXTURE], WANTED), 1, "between the present and the end")
    expect("synthetic even", synthetic_rows(), 0, "PASS")
    expect("synthetic unrelated delete_buffers after the present",
           synthetic_rows(unrelated_delete_after_present_at=150), 0, "PASS")
    expect("synthetic planted tail", synthetic_rows(planted_tail_at=120), 1, "between the present and the end")
    # one sweep takes 9 ms and still ends where it did, before the present
    slow = [(s + d - 9_000_000, 9_000_000, n, th) if n == SWEEP and 5_000_000_000 < s < 5_040_000_000 else (s, d, n, th)
            for s, d, n, th in synthetic_rows()]
    expect("synthetic slow sweep", slow, 1, "pre-swap sweeps over")
    expect("synthetic misplaced sweep (short, behind the present)",
           synthetic_rows(misplaced_sweep_at=150), 1, "do not end before the start of their frame's Swap")
    all_behind = [(s + 1_351_000, d // 2, n, th) if n in POOL_ZONES else (s, d, n, th) for s, d, n, th in synthetic_rows()]
    expect("synthetic every sweep behind the present", all_behind, 1, "do not end before the start of their frame's Swap")
    expect("synthetic pool expiry after the present", synthetic_rows(pool_expiry_after_present_at=150), 1,
           "expiry ran after the present")
    expect("synthetic no sweep zones", synthetic_rows(sweeps=False), 2, "pre-swap sweep zones in only")
    expect("synthetic sweep zones, no expiry", synthetic_rows(expiry=False), 2, "no pool expiry")
    half = [r for r in synthetic_rows() if not (r[2] == SWEEP and (r[0] // 33_335_333) % 2)]
    expect("synthetic sweep zones in half the frames", half, 2, "pre-swap sweep zones in only")
    expect("empty", [], 2, "INCONCLUSIVE")
    expect("no swap zones", [r for r in synthetic_rows() if r[2] != SWAP], 2, "INCONCLUSIVE")
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
