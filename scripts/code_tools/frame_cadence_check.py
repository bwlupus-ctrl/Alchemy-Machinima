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

1. Source pins (--check). Exact, comment- and whitespace-insensitive pins:
   - clean_vbo_pool_before_present() is exactly: early return when the frame was
     already swept, both settings read, ONE block gated on both that holds the
     Tracy zone, the sweep and the flag set, in that order;
   - swap() is exactly: the helper call, then the paced present, then the flag
     for the next frame; nothing else, so nothing can follow the present there;
   - display() from its last swap() to display_stats() is exactly the known
     text, with the post-present sweep inside if (!sVBOPoolSweptBeforePresent)
     and the flag reset right after it;
   - exact call counts in the file (helper call 1, endOfFrameCleanPool 3, flag
     set 1, flag clear 2, swap() sites in display() 3), so a duplicate or a new
     site is a finding until it is audited;
   - nothing on the list of calls known to take the driver's present wait (a
     buffer delete, a render-target or framebuffer bind, a flush, a texture
     upload, a readback) between the last swap() and the end of display();
   - the setting is a persisted Boolean, default on.
   The deny-list is textual: it cannot see into functions called there. The
   trace check is the backstop for that.
2. Trace check (--trace). Input: tracy-csvexport -u output, one or more files.
   Small exports are enough: -u -f doFrame, -u -f Swap, -u -f sweep,
   -u -f delete_buffers, -u -f timeout. For every main-loop frame it measures
   the POST-PRESENT TAIL (end of the frame's last "Swap" zone to the end of
   LLAppViewer::doFrame). Verdicts:
     FAIL          a tail over TAIL_FAIL_MS; a pre-swap sweep over SWEEP_FAIL_MS;
                   or a pool expiry ("vbo cache timeout" / delete_buffers) that
                   ran after the present
     INCONCLUSIVE  (exit 2) too few frames, no Swap zones, pre-swap sweep zones
                   missing from the presented frames, or no pool expiry seen
                   INSIDE a pre-swap sweep: the trace never exercised the path
                   that stalled, so a clean tail proves nothing
     PASS          none of the above: the sweep ran before the present, really
                   expired buffers there, stayed cheap, and no frame stalled
                   after its present
   It also counts long-then-short frame pairs and says how many a tail explains.

Selftest: the real 11/14 excerpt (frame_cadence_fixture.csv) must FAIL; a
synthetic even trace with expiry inside the pre-swap sweep must PASS; planted
tail, slow sweep and post-present expiry must FAIL; no sweep zones, sweep zones
without expiry, an empty trace and a trace without Swap zones must be
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

# ---- exact pins (comments stripped, whitespace collapsed) ----
HELPER_PIN = (
    'if (sVBOPoolSweptBeforePresent) { return; } '
    'static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, "RenderVBOPoolCleanAtFrameEnd", true); '
    'static LLCachedControl<bool> vbo_clean_before_present(gSavedSettings, "RenderVBOPoolCleanBeforePresent", true); '
    'if (vbo_clean_at_frame_end && vbo_clean_before_present) { '
    'LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("vbo pool sweep (pre-swap)"); '
    'LLVertexBuffer::endOfFrameCleanPool(true); '
    'sVBOPoolSweptBeforePresent = true; }'
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
    'if (!sVBOPoolSweptBeforePresent) { '
    'static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, "RenderVBOPoolCleanAtFrameEnd", true); '
    'LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end); } '
    'sVBOPoolSweptBeforePresent = false;'
)
TAIL_PIN = (
    'swap(); } } if (!display_cleanup_done) { LLSpatialGroup::sNoDelete = false; gPipeline.clearReferences(); } } '
    'LLAppViewer::instance()->pingMainloopTimeout("Display:FrameStats"); stop_glerror(); '
    + TAIL_SWEEP_PIN + ' display_stats();'
)
# token -> exact count in the comment-stripped file
FILE_COUNTS = (
    ("clean_vbo_pool_before_present(", 2, "one definition and one call (in swap())"),
    ("endOfFrameCleanPool(", 3, "the minimised path, the pre-present helper and the guarded post-present call"),
    ("sVBOPoolSweptBeforePresent = true", 1, "set only by the pre-present helper"),
    ("sVBOPoolSweptBeforePresent = false", 2, "the declaration and the reset at the end of display()"),
    ("sVBOPoolSweptBeforePresent", 5, "declaration, helper test, helper set, display() test, display() reset"),
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
    return " ".join(strip_comments(text).split())


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
    elif norm(helper) != HELPER_PIN:
        out.append(F + "HELPER PIN: clean_vbo_pool_before_present() is not exactly [early return when already swept; "
                       "both settings; one block gated on both holding the zone, the sweep and the flag set]")

    swap = function_body(display_src, r"\nvoid\s+swap\s*\(\s*\)")
    if swap is None:
        out.append(F + "swap() is missing")
    elif norm(swap) != SWAP_PIN:
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
            if not tail.startswith(TAIL_PIN):
                out.append(F + "TAIL PIN: display() from its last swap() to display_stats() is not exactly the audited text "
                               "(the post-present sweep must sit inside if (!sVBOPoolSweptBeforePresent), once, with the "
                               "flag reset right after it, and nothing new may run there)")
            for bad in POST_PRESENT_DENY:
                if bad in tail:
                    out.append(F + f"DENY: display() calls {bad} after the last swap() (it can take the driver's present wait)")

    whole = strip_comments(display_src)
    for token, want, why in FILE_COUNTS:
        got = whole.count(token)
        if got != want:
            out.append(F + f"COUNT: {token!r} appears {got} times, expected {want} ({why})")
    return out


# ---------------------------------------------------------------- trace check

FRAME = "LLAppViewer::doFrame"
SWAP = "Swap"
SWEEP = "vbo pool sweep (pre-swap)"
EXPIRY = ("vbo cache timeout", "delete_buffers")
WANTED = {FRAME, SWAP, SWEEP, *EXPIRY}


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

    swaps = sorted({(s, d) for s, d, n, th in rows if n == SWAP and th == main})
    sweeps = sorted({(s, d) for s, d, n, th in rows if n == SWEEP and th == main})
    expiry = sorted({(s, d) for s, d, n, th in rows if n in EXPIRY and th == main})
    swap_end = {}
    for s, d in swaps:
        i = frame_of(s)
        if i >= 0:
            swap_end[i] = max(swap_end.get(i, 0), s + d)
    if len(swap_end) < MIN_FRAMES // 2:
        return 2, [f"{label}: INCONCLUSIVE: {len(swap_end)} frames hold a Swap zone (need {MIN_FRAMES // 2}); export with -u -f Swap"]

    durs = [d / 1e6 for _, d in frames]
    med = statistics.median(durs)
    tails = {i: (frames[i][0] + frames[i][1] - e) / 1e6 for i, e in swap_end.items()}
    bad_tail = sorted(i for i, t in tails.items() if t > TAIL_FAIL_MS)
    bad_sweep = [d / 1e6 for _, d in sweeps if d / 1e6 > SWEEP_FAIL_MS]
    pairs = [i for i in range(len(durs) - 1) if durs[i] > 1.4 * med and durs[i + 1] < 0.65 * med]
    explained = [i for i in pairs if i in set(bad_tail)]

    # where did the pool expiry run: inside a pre-swap sweep, or after the present?
    sweep_starts = [s for s, _ in sweeps]
    expiry_in_sweep = 0
    expiry_after_present = 0
    for s, d in expiry:
        k = bisect.bisect_right(sweep_starts, s) - 1
        if k >= 0 and s < sweeps[k][0] + sweeps[k][1]:
            expiry_in_sweep += 1
            continue
        i = frame_of(s)
        if i in swap_end and s >= swap_end[i]:
            expiry_after_present += 1
    swept_frames = {frame_of(s) for s, _ in sweeps} & set(swap_end)

    tv = sorted(tails.values())
    lines.append(f"{label}: {len(frames)} frames, median {med:.1f} ms; post-present tail median {statistics.median(tv):.2f} ms, "
                 f"p99 {tv[int(len(tv) * 0.99)]:.2f} ms, max {tv[-1]:.2f} ms")
    lines.append(f"{label}: long-then-short frame pairs: {len(pairs)} ({len(explained)} explained by a post-present tail)")
    lines.append(f"{label}: pre-swap sweep zones in {len(swept_frames)} of {len(swap_end)} presented frames"
                 + (f", max {max(d for _, d in sweeps) / 1e6:.2f} ms" if sweeps else ""))
    lines.append(f"{label}: pool expiry zones: {expiry_in_sweep} inside a pre-swap sweep, {expiry_after_present} after the present")

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
        inconclusive.append("no pool expiry (vbo cache timeout / delete_buffers) ran inside a pre-swap sweep: the capture never "
                            "exercised the path that stalled (capture longer with scene churn, and export -u -f delete_buffers "
                            "and -u -f timeout)")
    if inconclusive:
        for why in inconclusive:
            lines.append(f"{label}: INCONCLUSIVE: {why}")
        return 2, lines
    lines.append(f"{label}: PASS: the sweep ran before the present, expired buffers there and no frame stalled after its present"
                 + (f"; {len(pairs)} long-then-short pairs have another cause, look at them" if pairs else ""))
    return 0, lines


def synthetic_rows(frames=300, planted_tail_at=None, sweeps=True, expiry=True, expiry_after_present_at=None):
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
        rows.append((t, dur, FRAME, "1"))
        rows.append((swap_start, swap_dur, SWAP, "1"))
        if sweeps:
            rows.append((swap_start - 50_000, 20_000, SWEEP, "1"))
            if expiry and i % 40 == 7:
                rows.append((swap_start - 45_000, 10_000, "vbo cache timeout", "1"))
                rows.append((swap_start - 44_000, 8_000, "delete_buffers", "1"))
        if expiry_after_present_at is not None and i == expiry_after_present_at:
            rows.append((swap_start + swap_dur + 2_000, 5_000, "delete_buffers", "1"))
        t += dur + 2_000
    return rows


# ---------------------------------------------------------------- selftest

def selftest() -> list[str]:
    out: list[str] = []
    display_src = DISPLAY_CPP.read_text(encoding="utf-8")
    settings_src = SETTINGS_XML.read_text(encoding="utf-8")

    HELPER_CALL = "    clean_vbo_pool_before_present();\n"
    HELPER_BLOCK = ("    if (vbo_clean_at_frame_end && vbo_clean_before_present)\n    {\n"
                    "        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY(\"vbo pool sweep (pre-swap)\");\n"
                    "        LLVertexBuffer::endOfFrameCleanPool(true);\n"
                    "        sVBOPoolSweptBeforePresent = true;\n    }\n")
    EARLY_RETURN = "    if (sVBOPoolSweptBeforePresent)\n    {\n        return;\n    }\n"
    TAIL_BLOCK = ("    if (!sVBOPoolSweptBeforePresent)\n    {\n"
                  "        static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, \"RenderVBOPoolCleanAtFrameEnd\", true);\n"
                  "        LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end);\n    }\n"
                  "    sVBOPoolSweptBeforePresent = false;\n")
    PRESENT_END = "        });\n    }\n    gDisplaySwapBuffers = true;\n"
    AFTER_TAIL = "    gShiftFrame = false;\n\n    if (gShaderProfileFrame)\n"
    LAST_SWAP = ("                // tail for the live A/B off path and deferred snapshots.\n"
                 "                swap();\n")
    for name, anchor in (("HELPER_CALL", HELPER_CALL), ("HELPER_BLOCK", HELPER_BLOCK), ("EARLY_RETURN", EARLY_RETURN),
                         ("TAIL_BLOCK", TAIL_BLOCK), ("PRESENT_END", PRESENT_END), ("AFTER_TAIL", AFTER_TAIL),
                         ("LAST_SWAP", LAST_SWAP)):
        if display_src.count(anchor) != 1:
            out.append(f"selftest: mutation anchor {name} found {display_src.count(anchor)} times in llviewerdisplay.cpp (need 1)")
    if out:
        return out

    def sub(old, new):
        return display_src.replace(old, new, 1)

    setting_at = settings_src.find(f"<key>{SETTING}</key>")
    setting_end = settings_src.find("</map>", setting_at)
    block = settings_src[setting_at:setting_end]
    value_off = re.sub(r"(<key>Value</key>\s*<integer>)1(</integer>)", r"\g<1>0\g<2>", block)
    not_persisted = re.sub(r"(<key>Persist</key>\s*<integer>)1(</integer>)", r"\g<1>0\g<2>", block)
    if value_off == block or not_persisted == block:
        return ["selftest: could not build the settings mutations"]

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
                           "    sVBOPoolSweptBeforePresent = true;\n"), settings_src, "HELPER PIN"),
        ("helper ignores the new setting",
         sub("if (vbo_clean_at_frame_end && vbo_clean_before_present)", "if (vbo_clean_at_frame_end)"), settings_src, "HELPER PIN"),
        ("helper ignores RenderVBOPoolCleanAtFrameEnd",
         sub("if (vbo_clean_at_frame_end && vbo_clean_before_present)", "if (vbo_clean_before_present)"), settings_src, "HELPER PIN"),
        ("helper never marks the frame",
         sub("        sVBOPoolSweptBeforePresent = true;\n", ""), settings_src, "HELPER PIN"),
        ("helper marks the frame outside the gated block",
         sub("        sVBOPoolSweptBeforePresent = true;\n    }\n", "    }\n    sVBOPoolSweptBeforePresent = true;\n"),
         settings_src, "HELPER PIN"),
        ("helper sweeps twice",
         sub("        LLVertexBuffer::endOfFrameCleanPool(true);\n",
             "        LLVertexBuffer::endOfFrameCleanPool(true);\n        LLVertexBuffer::endOfFrameCleanPool(true);\n"),
         settings_src, "HELPER PIN"),
        ("early return removed (not once per frame)", sub(EARLY_RETURN, ""), settings_src, "HELPER PIN"),
        ("helper called from a second place",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    clean_vbo_pool_before_present();\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "COUNT: 'clean_vbo_pool_before_present('"),
        ("post-present sweep unguarded",
         sub("    if (!sVBOPoolSweptBeforePresent)\n", ""), settings_src, "TAIL PIN"),
        ("empty guard, then an unconditional post-present sweep",
         sub(TAIL_BLOCK, "    if (!sVBOPoolSweptBeforePresent)\n    {\n    }\n"
                         "    static LLCachedControl<bool> vbo_clean_at_frame_end(gSavedSettings, \"RenderVBOPoolCleanAtFrameEnd\", true);\n"
                         "    LLVertexBuffer::endOfFrameCleanPool(vbo_clean_at_frame_end);\n"
                         "    sVBOPoolSweptBeforePresent = false;\n"), settings_src, "TAIL PIN"),
        ("flag never reset", sub("    sVBOPoolSweptBeforePresent = false;\n", ""), settings_src, "TAIL PIN"),
        ("flag reset before the post-present test",
         sub(TAIL_BLOCK, "    sVBOPoolSweptBeforePresent = false;\n" + TAIL_BLOCK.replace("    sVBOPoolSweptBeforePresent = false;\n", "")),
         settings_src, "TAIL PIN"),
        ("a second post-present sweep later in display()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    LLVertexBuffer::endOfFrameCleanPool(true);\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "COUNT: 'endOfFrameCleanPool('"),
        ("a render-target bind after the last swap()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    gPipeline.mRT->deferredScreen.bindTarget();\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "DENY: display() calls bindTarget"),
        ("a framebuffer bind after the last swap()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    glBindFramebuffer(GL_FRAMEBUFFER, 0);\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "DENY: display() calls BindFramebuffer"),
        ("gGL.flush() after the last swap()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    gGL.flush();\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "DENY: display() calls gGL.flush"),
        ("a buffer flush after the last swap()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    LLVertexBuffer::flushBuffers();\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "DENY: display() calls flushBuffers"),
        ("a buffer delete after the last swap()",
         sub(AFTER_TAIL, "    gShiftFrame = false;\n    glDeleteBuffers(1, &gFrameHitchSelftestName);\n\n    if (gShaderProfileFrame)\n"),
         settings_src, "DENY: display() calls glDeleteBuffers"),
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
    expect("synthetic planted tail", synthetic_rows(planted_tail_at=120), 1, "between the present and the end")
    slow = [(s, 9_000_000 if n == SWEEP and 5_000_000_000 < s < 5_040_000_000 else d, n, th) for s, d, n, th in synthetic_rows()]
    expect("synthetic slow sweep", slow, 1, "pre-swap sweeps over")
    expect("synthetic expiry after the present", synthetic_rows(expiry_after_present_at=150), 1, "expiry ran after the present")
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
