**Verdict: GO-after-fixes.** Reviewed HEAD `5e1134e5521` and `CLAUDE.md`; no edits, builds, or commits. References below use the brief’s file abbreviations.

**A — fixable design bugs**

1. **Reuse bypasses map-specific sampling rules (§1.4).** The proposed unconditional legacy offset/`pcfShadow` ignores world-unit bias, subject-column requirements, and differing cascade resolutions. `shadow_res` describes cascade 0; other main cascades can have independently configured sizes (`pipe:1811–1822`). Consequently, padding and filtering can be wrong even when containment succeeds. **Fix:** retain containment selection, but sample through the applicable legacy/world-unit machinery with each map’s actual dimensions and coefficients; preserve subject-column bias requirements (`shu:360–415`). The double-precision `sunVP * inverse(probeView)` composition is correct; positive homogeneous W plus divided XYZ bounds is appropriate.

2. **State restoration is unspecified (§§1.2–1.4).** Re-expressing projectors modifies shared `mSunShadowMatrix[4..13]`; restoring only the mode/reuse flag leaves probe-eye matrices behind. Reuse’s Full fallback also changes sun matrices, clip planes, and shadow view/projection state. **Fix:** for opt-in captures, save/restore all affected shared state across the entire face, including fallback. Invalidate affected fast-bind shader state or refresh view-dependent uniforms on the next bind: CPU restoration alone does not restore uploaded uniforms (`pipe:22800–22813`, `22958–23005`). Keep default Full behavior unchanged.

3. **“This frame” detection is not established (§1.4).** `age <= 1` explicitly accepts previous-frame maps. Moreover, `gFrameCount` advances inside `display()` (`disp:602`), so skipped displays need not age the stamp. **Fix:** invalidate freshness at the application render iteration’s start and publish it only after completed main-map generation; require that iteration’s token and valid allocation metadata. Previous-frame reuse also needs coordinate-origin invalidation, not merely the assertion that only moving casters lag.

4. **Off’s safety claim is false (§1.5).** With shadow detail zero, sun targets are released (`pipe:1831–1837`), yet the proposed Off branch calls `skipRenderingShadows()`, whose target binding requires an FBO. Additionally, finite matrices do not guarantee nonzero projected W: legacy PCF divides by W without checking (`shu:183–186`). Clearing depth does not prevent invalid coordinates. **Fix:** preserve the deferred/shadow-detail gate and make Off return `1.0` before directional-shadow projection/sampling, with its flag refreshed on both binding paths.

**B — inherent limits to document, not blockers**

- Retaining `mAuxillaryRT` while adding another G-buffer and four stock-sized shadow maps increases allocated resources; reducing Gaussian scratch partly offsets this. Quantify the net VRAM change (§2.3), rather than implying memory savings.
- Containment without blending produces cascade-boundary seams; camera-dependent coverage and all four user-accepted limitations remain B.
- The rebuild listener also resets hero probes and reloads shaders (`llviewercontrol.cpp:674–678`). Hero capture settings remain excluded, but changing scale can temporarily disturb mirror convergence.

**C — implementation/verification notes**

- Append reserved uniform enum/name entries consistently. Hashed-name lookup also works (`llglslshader.cpp:749–759`). Any new permutations belong after `clearPermutations`; registration itself is separate.
- Preserve the selected capture-pack pointer for filtering after `rmm:2869` restores `mRT` to main.
- Viewport, output mips, radiance/irradiance sizing, and ordinary cube Actor Mask exclusion are consistent with the design (`pipe:23811–23815`). Settings/UI/preset wiring is adequate.
- An untaken shader branch does not prove bitwise-identical output across drivers. Default equivalence and shader uniform/link budgets remain validation requirements; no compile failure is established by this read-only review.
