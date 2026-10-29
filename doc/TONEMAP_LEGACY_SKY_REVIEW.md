# [TonemapLegacySky] adversarial review (Opus, 2026-09-29)

Scope: uncommitted diff in pipeline.cpp, pipeline.h, settings_alchemy.xml, floater_lightbox_settings.xml.

## Verdict: NO-GO until P1-1 is fixed. Then GO.

## 1. OFF parity: proven
- With the setting OFF, `legacy_gamma` implies `no_post`, so `colorCorrectWillApplyExposure(true, &lg) && lg` is always false. That makes `tonemap_legacy_gamma` false, so `use_history` is unchanged and the exposure bounds are HEAD's `probe_ambiance > 0.f`.
- The tonemap-mix ternary (pipeline.cpp:12452) takes HEAD's branch, because `legacy_gamma && !no_post` is false.
- Night Mask (12876), on-lens (13138) and bloom (13574) now use `!no_post`, which is the same expression HEAD duplicated. The only difference is a null-`psky` guard where HEAD would have crashed.
- New state written when OFF: `mLastExposureLegacyTonemap = false` every frame. It is always false and has no observable effect.
- Shader selection is untouched. Snapshot and build-floater no-post are preserved.

## P1-1: S-Log3 (type 8) is corrupted by the legacy-gamma variant
In colorCorrectF.glsl (lines 157-175), type 8 `applyToneMap` returns `srgb_to_linear(code)`. `linear_to_srgb` then restores the log code. After that, `#ifdef LEGACY_GAMMA` applies `legacyGamma(x) = 1 - (1-x)^gamma`, where `gamma` is the raw sky gamma (llsettingsvo.cpp:1030). That step is not bypassed for type 8, unlike every COLOR_GRADE stage.
- For any sky gamma other than 1, the output is not an S-Log3/S-Log2/LogC3/Cineon curve. Example at gamma 1.5: 18% grey at code 0.41 comes out as 0.55. The Ninja's IRE and stop readings would be wrong.
- At gamma 1.0 the step is an identity.

Fix (CPU only; shaders are off-limits). In `colorCorrect`, before selecting the shader:
```cpp
static LLCachedControl<S32> cc_tm_type(gSavedSettings, "AlchemyRenderTonemapType", 0);
// [TonemapLegacySky] log capture must emit the raw OETF; the legacy curve is a display look
const bool legacy_curve = legacy_gamma && !(apply_tonemap && !no_post && cc_tm_type() == 8);
```
Use `legacy_curve` in place of `legacy_gamma` in the selection at 11880-11896. With the setting OFF, `!no_post` is false whenever `legacy_gamma` is true, so the result is byte-identical to HEAD. Keep the predicate's `out_legacy_gamma` as it is, because exposure uses it.

Types 0-7: the legacy variant is correct, not a double gamma. It uses the same display-space curve, in the same order, as the stock no-post legacy path (clamp, then `linear_to_srgb`, then `legacyGamma`). The only change is that exposure, tonemap, bloom and grade replace the hard clamp. Keep the Tonemap+LegacyGamma variants for types 0-7.

Side note, not a bug: legacy skies are lit with SKY_HDR_SCALE = 1 (llsettingsvo.cpp:1013), so S-Log on them reads darker than on PBR skies. Compensate with EV.

## P2-1: the exposure history reset causes a pop and adds state (pipeline.cpp:11628-11636, pipeline.h:1463)
exposureF.glsl only does `mix(prev, target)`. While OFF, the history holds 1.0, which is continuous with the no-exposure image. Resetting the history snaps straight to the target (up to 2x at night). The reset also fires on every sky crossfade or region change into or out of an ambiance-0 sky, and on every `gSnapshotNoPost` or Build no-post frame. Stock never resets on sky change.

Fix: delete the reset and `mLastExposureLegacyTonemap`. Keep the bounds change.

## P2-2: tooltip is inaccurate (floater_lightbox_settings.xml:380)
- The option also turns on bloom, Graduated ND/Polarizer, Night Mask, Roto Ink/Tron exposure and the ReShade raw capture on these skies.
- Auto-exposure starts adapting.
- "May look more contrasty" is unsupported. A tonemapper compresses, and a gamma above 1 lifts.

Reword to cover these.

## P2-3: Environment Adjust note is stale when ON (llfloaterenvironmentadjust.cpp:870, alenvintensity.cpp:281)
The Sun EV cap and its "no tonemapper" note still apply while ON. The behaviour is acceptable and conservative, but the note is now misleading. Follow-up only.

## Verified clean
- **Water:** no water shader calls `applyExposure`, `applyToneMap` or tonemap uniforms. lldrawpoolwater's uploads are dead, so water is tonemapped with the scene.
- **Bloom:** extraction and composition share the predicate. The legacy tonemap variants carry BLOOM_COMPOSITE and HALATION, and their permutations match the non-legacy ones.
- **Night Mask, on-lens, Roto Ink (16583) and Tron (17596):** all read one predicate.
- **GLTF preview (use_history=false):** consistent.
- **Live toggle:** it is a static `LLCachedControl` read each call. No latch, and no recompile because the variants already exist. However, the Tonemap+LegacyGamma variants were unreachable at HEAD, so they have never run in-world. They compile at startup, but check them in-world.
- **Compile:** no new warnings. `&&` sequencing makes the out-param read safe. The removed locals are not referenced later. The const method only reads. LF line endings, and `git diff --check` is clean.
- **UI:** the checkbox sits at y84-99, clear of the reset button (y59-79). Simulated panel bottom goes from 767 to 787, which fits the tab client of about 805. `control_name` matches the setting. alfloaterlightbox.cpp never hides or disables the checkbox. The tonemap reset group does not reset it, which is correct because it is a mode toggle.

## Round 2 (Opus): verdict GO, with no must-fix items

- **P1-1 fixed (pipeline.cpp:11874-11875).** `legacy_curve = legacy_gamma && !(apply_tonemap && !no_post && type == 8)`.
  - With the setting OFF, `legacy_gamma` implies `no_post`, so the inner term is false and `legacy_curve == legacy_gamma`. That keeps all three selection sites byte-identical to HEAD.
  - With `apply_tonemap == false`, it reduces to `legacy_gamma` (identical).
  - With the setting ON and a snapshot or Build no-post frame, `no_post` holds, so the legacy gamma-only variant is used, as at HEAD.
  - With the setting ON and type 8, it selects `gCGTonemapProgram` or `gCGTonemapColorgradeProgram`. Neither defines LEGACY_GAMMA, and the COLOR_GRADE stages already bypass type 8 (colorCorrectF.glsl:150/165/182), so no legacy curve reaches log output.
  - Type 8 is the only log type: curves 0-3 are sub-modes of it. Unknown types fall through the switch as display types.
  - `LLCachedControl<S32>` with default 0 matches the S32 setting. The name `tonemap_type` does not clash inside `colorCorrect`.
- **Auto-exposure claim confirmed.** tonemapUtilF.glsl:426 forces `exp_scale = 1.0` for type 8, and TONEMAP_TYPE is uploaded to the clean variant too. The ON exposure bounds are therefore harmless for type 8.
- **P2-1 fixed.** The reset and the member are gone, with no remaining references. The bounds change remains; with the setting OFF it reduces to HEAD because `tonemap_legacy_gamma` is always false.
- **P2-2 fixed.** The tooltip is now accurate.
- **P2-3 listener.** It is a `scoped_connection` in `mEnvIntensityPresetConns`, which is never cleared, so it disconnects when the floater is destroyed. `updateGammaLabel` returns early when `mLiveSky` is null, which is safe after `onClose`. The Sun EV cap is unchanged.
- **P2 (optional):** the ON tooltip string is hard-coded English in llfloaterenvironmentadjust.cpp:887. Move it into the floater XML as a `<string>` for localization.
- **Compile:** no new locals are unused. `git diff --check` is clean.
