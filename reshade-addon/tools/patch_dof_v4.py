# Re-appliable correctness patches for the stock V4 MartysMods_DEPTHOFFIELD.fx.
# Same posture as graft_launchpad_v4.py: proprietary file, references Marty's
# lines only by tiny anchors, must be re-run after every iMMERSE update.
#
# Four fixes (all present in the user's prior hand-patched working copy; the
# stock 2026 V4 drop regressed/omitted them):
#   1. Autofocus prefix-sum out-of-bounds write (b <= -> b < AF_POINTS)
#   2. Autofocus median upper-weight copy-paste bug ([j] -> [j+1]) that snapped
#      focus to discrete depth samples
#   3. Prefilter separable-blur accumulation commented out (x2) -> restore
#   4. Self-referential `float var = max(1e-6, var);` shadowing the outer var
#      (reads uninitialized) in the two bokeh gather loops (x2) -> remove
import sys

PATH = r"I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\reshade-shaders\Shaders\iMMERSE\MartysMods_DEPTHOFFIELD.fx"

s = open(PATH, "r", encoding="utf-8", newline="").read()
assert "\r\n" in s, "expected CRLF file"
s = s.replace("\r\n", "\n")
orig = s

def repl(old, new, n, tag):
    c = s_ref[0].count(old)
    assert c == n, f"[{tag}] expected {n} match(es), found {c}"
    s_ref[0] = s_ref[0].replace(old, new)

s_ref = [s]

# 1. OOB prefix-sum write
repl("for(uint b = 1; b <= AF_POINTS; b <<= 1, barrier())",
     "for(uint b = 1; b < AF_POINTS; b <<= 1, barrier())", 1, "oob")

# 2. median upper-weight index
repl("float upper_w = (j+1) == AF_POINTS ? 1.0 : focus_tgsm_w[j];",
     "float upper_w = (j+1) == AF_POINTS ? 1.0 : focus_tgsm_w[j+1];", 1, "focus-snap")

# 3. prefilter accumulation (two commented copies; main bokeh copies already live)
repl("//result += float4(tap.rgb * w, w);",
     "result += float4(tap.rgb * w, w);", 2, "prefilter")

s = s_ref[0]

# 4. drop the shadowing inner decl, whitespace-robust (keep the outer var)
lines = s.split("\n")
kept = [ln for ln in lines if ln.strip() != "float var = max(1e-6, var);"]
removed = len(lines) - len(kept)
assert removed == 2, f"[var-shadow] expected to remove 2 lines, removed {removed}"
s = "\n".join(kept)

assert s != orig
open(PATH, "w", encoding="utf-8", newline="").write(s.replace("\n", "\r\n"))
print("OK: oob, focus-snap, prefilter x2, var-shadow x2 applied")
