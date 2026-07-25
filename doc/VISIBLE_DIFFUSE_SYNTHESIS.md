# Visible-surface diffuse + coverage — synthesis of four independent research passes

**Date:** 2026-07-25
**Sources:** three internal research streams (production / coverage / consumer) run independently and
in parallel, plus an external OpenAI deep-research package (`SL_VISIBLE_DIFFUSE_RESEARCH_DELIVERABLES`).
None saw the others' work.

**Status:** design synthesis. Nothing here is implemented. Read
`doc/RESHADE_BRIDGE_CONTRACT.md` v2 first for the existing system.

---

## 0. The headline

Two independent sources — internal stream 1 and the external package — converged on the **same
architecture** without contact: capture diffuse in the **same draw calls** as the forward beauty pass
via MRT, never by replay, estimation or de-lighting. That convergence is the strongest signal in this
whole body of work.

Stream 1 then settled the question that made it actionable:

> **Can the forward pass take a second colour attachment? YES.**
> `mRT->screen` is bound across every forward draw (`pipeline.cpp:13571`, held through
> `renderGeomPostDeferred` at `:13952`, flushed `:13963`). `bindTarget()` sets `glDrawBuffers` from
> the target's own attachment count (`llrendertarget.cpp:533-545`). **One `addColorAttachment` on
> `mRT->screen` turns every forward draw into a 2-target MRT draw** — no FBO juggling, no per-pool
> retargeting.

---

## 1. What all four sources agree on

1. **Same-draw MRT capture.** Correctness by construction — identical draws, sort order, per-fragment
   discards, rigging, clip planes and alpha as the beauty image, so the diffuse composite cannot
   drift out of alignment with what the user sees.
2. **Do NOT expand the deferred G-buffer.** It is already at four attachments — and stream 1 showed
   the idea is not merely awkward but **structurally impossible**: forward geometry never draws into
   `deferredScreen`, which is flushed before `renderDeferredLighting` runs
   (`llviewerdisplay.cpp:1105-1110`). A fifth attachment would never receive a forward fragment.
3. **Keep raw `SL_ALBEDO` as-is; add a new semantic.** The existing texture is a truthful statement
   about the deferred layer and is useful for diagnostics. Visible-surface diffuse is a *different
   quantity* and deserves its own contract rather than a redefinition.
4. **Validity must be explicit and carried per-pixel.** No payload value is ever an availability
   sentinel. Known-black must be distinguishable from unknown.
5. **Default to strict COEXIST.** Overwrite the consumer's estimate only where the viewer can *prove*
   exactness; otherwise discard and leave the incumbent alone.
6. **Metals and water have zero diffuse reflectance** — that is a *statement*, not a gap.

---

## 2. Corrections to our own contract that came out of this

| # | What we had | Correction | Source |
|---|---|---|---|
| 1 | `.w > 0` proposed as a coverage gate | **Unsound.** The clear is `glClearColor(1,0,1,1)` (`llviewerdisplay.cpp:1020`) and `bindTarget` clears *all* attachments, so unwritten `.w` = 1.0 — colliding with `HAS_HDRI`. Only a windowed two-bucket test works: `abs(w-0.34)<0.1 \|\| abs(w-0.67)<0.1` | stream 2 |
| 2 | "`GL_RGB10_A2` makes `.w` unusable — 2 bits of alpha" | **Wrong.** 2-bit UNORM quantizes to `{0,⅓,⅔,1}`; flags `0.34`/`0.67` sit 0.0067/0.0033 from `⅓`/`⅔`, far inside the ±0.1 decode window. **The values were chosen to be 2-bit-safe.** Proof by dependence: non-HDR deferred lighting branches on them every frame and works | stream 2 |
| 3 | §2.4: uncovered albedo holds "the clear value or the opaque surface behind" | **Incomplete, and the omission is the dangerous one.** Sky renders *deferred* and writes its **radiance** into `frag_data[0]` (`skyF.glsl:220`) whenever `RenderEnableEmissiveBuffer` is off — the default. So sky pixels hold a plausible-looking colour that is the wrong *quantity* | streams 1 + 2 |
| 4 | `SL_COEXIST_ALBEDO_BLACK_PROXY` retained as a coverage fallback | **Nearly a no-op.** Per #3 sky albedo is non-black and sails through it, while it still misclassifies genuinely black materials. The windowed `.w` gate is strictly stronger and should replace it | stream 2 |
| 5 | ABI: `surface_coverage.R` = raw G-buffer coverage, `.G` = forward coverage | **Latent wrong-blend bug.** Raw semantics force consumers to compute `r·(1−g)`; our FX doesn't — it thresholds raw `R`, so `r=1, g=0.9` passes and gets full-strength behind-surface albedo. **Redefine R as the composite weight** | stream 2 |
| 6 | Contract §6.1 format list | Omits `0x822B → r8g8_unorm`, which the add-on already ships (`sl_reshade_bridge.cpp:134`). Doc defect | stream 2 |
| 7 | §7.3 implies OWNED is the destination for all semantics | For **albedo** that is wrong in principle: G-buffer albedo is structurally partial, so OWNED's ceiling is "correct on opaque, neutral elsewhere" — a permanent regression vs the estimate. State the ceiling | stream 3 |
| 8 | `PS_ProvideAlbedo` writes `float3` to an RGBA16F target | **No write mask** → `.a` gets undefined values, clobbering whatever the consumer stored. Add `RenderTargetWriteMask = 7` | stream 3 |

---

## 3. The two genuine disagreements

### 3.1 Is the convention mismatch calibration, or units?

| | External package §11 | Internal stream 3 |
|---|---|---|
| Diagnosis | **calibration** — better data, retune the consumer | **units** — display-referred vs scene-referred |
| Remedy | provider-side gain / saturation / max controls | no FX-side transform is principled |
| Exposure changes | not addressed | **the crux** — a fixed gain is calibrated at one exposure state, and SL has day cycles |

Stream 3's mechanism: ReShade runs on the **final backbuffer**, after exposure and tonemap, so the
light a screen-space GI gathers is **display-referred**. The incumbent estimate is factored *out of
that same backbuffer*, making it self-consistent **and exposure-tracking**. Our value is scene-referred
linear. Tonemapping is non-linear *per channel*, so the discrepancy is in **chroma**, not just gain —
which is why a scalar cannot fix it, and why any retune drifts as lighting changes.

They partly converge: the external package proposes a *saturation* control alongside gain, conceding
chroma is involved.

> **RESOLUTION — one experiment decides it.** Enable albedo, tune the consumer's bounce until an SL
> **noon** looks right, then jump to **midnight**. One setting holds → calibration. It drifts → units,
> and the only principled path is viewer-side: publish diffuse already passed through the viewer's
> **own** exposure and tonemap. Cheap, pre-code, and nobody can argue with the result.

### 3.2 One attachment or two?

**Stream 1:** one attachment, **seeded with the deferred opaque albedo**, then let the alpha pool's
existing `(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` blend perform the over-composite **in hardware**. Elegant
— the composite uses the identical blend the image uses, so it is pixel-aligned by construction.

**External package:** two sidecars — diffuse **and an exactness channel `K`** — with two indexed blend
states so that *unknown* draws attenuate the accumulated diffuse without contributing:
```
known:    D' = rho·a + D·(1-a);   K' = a + K·(1-a)
unknown:  D' =         D·(1-a);   K' =     K·(1-a)
```

**These are complementary, not competing.** Stream 1 gives the correct composite; `K` answers "how
much of this composite is trustworthy" — which is exactly the known-black-vs-unknown distinction our
§4 rule demands, expressed as a number. **Take both:** seed from the opaque albedo (stream 1) *and*
carry exactness (external). The seed makes `K` start at 1 wherever the G-buffer wrote, which is
precisely right.

---

## 4. The implementation hazards stream 1 found

These are the parts that would have bitten us, all with `file:line`:

1. **Undefined attachment-1 writes.** With 2 draw buffers, any shader declaring only
   `out vec4 frag_color` leaves attachment 1 **undefined** at every fragment it covers — and that is
   *everything* currently drawing into `screen` (soften, lights, haze, all forward pools). Mitigation:
   `glColorMaski` — already loaded (`llgl.cpp:496,1884`) but **currently unused in newview** — masked
   off by default, unmasked only inside contributing passes.
2. **The `setColorMask` caching trap.** `LLRender::setColorMask` (`llrender.cpp:1319-1338`) issues a
   *global* `glColorMask`, which per GL semantics stomps all indexed masks — and pools call it
   mid-pass (`lldrawpoolwater.cpp:110`, `lldrawpoolalpha.cpp:260`, glow at `lldrawpoolsimple.cpp:53`).
   Needs a gated re-assert hook.
3. **`renderGeomPostDeferred` has other callers.** Impostors (`pipeline.cpp:16476/16483/16495`, into
   `avatar->mImpostor`) and HUDs (`llviewerdisplay.cpp:1448`). Gate on *target identity*, not on
   "we are in a post-deferred pass".
4. **Additive faces are emitters, not occluders.** Custom per-face blends exist
   (`lldrawpoolalpha.cpp:807`); when dst factor isn't `ONE_MINUS_SRC_ALPHA`, pin
   `glBlendFunci(1, GL_ZERO, GL_ONE)` so an additive fire effect doesn't recolour the floor's bounce.
5. **sRGB blending.** Write **linear** diffuse into an `SRGB8_ALPHA8` attachment with
   `GL_FRAMEBUFFER_SRGB` so the hardware does decode-blend-encode. Precedent in tree:
   `lldrawpoolpbropaque.cpp:56`.

---

## 5. Category verdicts (merged)

| Category | Verdict |
|---|---|
| Legacy alpha, PBR alpha, materials alpha, avatar alpha | Contribute. Base colour available at output — `alphaF.glsl:220`, `pbralphaF.glsl:137` |
| Fullbright (opaque, mask, alpha, shiny) | Contribute — real occluding surfaces. Exclude glow/emissive; do not smuggle emission into diffuse |
| Water | **Convention value**, not radiance and not invalid: dark fog-tinted (`fog_color_linear × k`, k≈0.02-0.05). It genuinely occludes, so falling back to behind-albedo is also wrong |
| Sky | **Exclude.** Not a forward problem — sky is deferred and already contaminates attachment 0 with radiance. Its light reaches consumers via `color_hdr`; fabricating sky albedo lets GI harvest bounce from a wall at the far plane |
| PBR metals | **Zero diffuse, exactness 1.** At `metallic=1` base colour tints *specular*; there is no diffuse lobe. Publishing gold base colour as diffuse creates coloured bounce from a surface that has none |
| Glow / emissive passes / haze / water-exclusion | Exclude (masked) |

---

## 6. Recommended order

**Stage 0 — free, pre-code, and it may end the project.** Run the noon/midnight exposure sweep (§3.1).
If the mismatch is units rather than calibration, everything downstream must publish *display-referred*
diffuse, which changes the design before a line is written.

**Stage 1 — free, one line.** `RenderTargetWriteMask = 7` on `ProvideAlbedo` (§2 #8). Hygiene
regardless of the outcome.

**Stage 2 — cheap, high value.** Replace the black proxy with the windowed `.w` gate (§2 #1, #4).
Kills sky contamination today, without the sidecar existing.

**Stage 3 — the ABI fix, before anything consumes it.** Redefine `surface_coverage.R` as the composite
weight (§2 #5). Stream 2 supplied paste-ready contract text. Cheap now, breaking later.

**Stage 4 — the sidecar.** One attachment on `mRT->screen`, seeded from deferred albedo, plus the
exactness channel; gated `default-off`; the five hazards in §4 handled explicitly.

**Stage 5 — publish coverage**, retire the interim `.w` gate.

Stages 0-3 are worth doing whatever happens to stages 4-5.

---

## 7. What none of the four could settle

1. Whether the consumer linearises the backbuffer internally (changes the mismatch's shape, not its
   existence).
2. Whether anything reads `AlbedoTex.a`.
3. Whether any iMMERSE consumer accepts a reactivity/reset input — determines whether `motion_meta.G`
   has a consumer at all.
4. Blit-to-attachment-1 seeding is GL-spec-inferred, not codebase-proven.
5. Indexed-mask containment across a whole frame — needs a `glGetBooleani_v` assert sweep.
6. `PASS_POST_BUMP` semantics — toggle the pass and diff.
7. Actual GPU cost — Tracy zone delta, one variable per launch.

Estimated cost from stream 1, **estimate not measurement**: ~1% of a 16.7 ms frame even in a
pathological 4K case (33 MB attachment, ~185 µs worst-case write traffic).
