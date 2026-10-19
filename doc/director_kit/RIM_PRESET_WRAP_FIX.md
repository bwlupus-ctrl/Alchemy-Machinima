# Rim preset Light Wrap fix

The 20 legacy rim presets omitted all six `CineRigRimWrap*` settings.
`applyRigRimPreset` resets the RIM owned set before applying the row
(`indra/newview/alrigrimcontroller.cpp:986`); RIM ownership includes wrap
(`indra/newview/alkitpresets.cpp:73`), and reset calls `resetToDefault(true)`
(`alkitpresets.cpp:182`). `CineRigRimWrapEnabled` defaults to false
(`indra/newview/app_settings/settings.xml:3069`). Therefore even selecting
Soft Portrait Wrap or Dreamy Halo always disabled the background light-wrap
composite. Per-light Key/Fill/Rim/Bg `Wrap` values are separate angular lighting
parameters; they do not enable that composite.

Each legacy row now explicitly authors Enabled, Strength, Width, Blur,
Saturation and Clamp. The legacy loader applies those values through the same
owned-set apply helper used by full looks. Descriptions explain the soft/glowing
or crisp intent and are exposed through the existing preset-description API.
Names, indices, original lighting parameters, ownership and defaults are unchanged.
The user decision is that every preset enables Light Wrap. All **32/32** rows
now enable it with positive strength. The 10 formerly disabled legacy rows and
three formerly disabled full looks use strength 0.25 and retain their narrow
widths (1.0-1.5), saturation and clamp. Strobe Cut uses blur 0.3; the other
12 changed rows retain blur 0.5. All previously enabled rows are unchanged.
Five descriptions promising no bleed or no diffuse wrap now describe a faint,
tight wrap. Names, order, other values, ownership and defaults are unchanged.

## Does rim render with Light Wrap off?

Yes, according to the production render path. No renderer change is needed.
`ALCineRigRim::isEnabled` requires the rim master enable, positive master gain
and an enabled light-rig slot; it does not read WrapEnabled
(`indra/newview/alcinerigrim.cpp:119`). `paramsForVolume` supplies rim parameters
only for registered rig emitters (`alcinerigrim.cpp:321-324`).

The actual rim term in
`indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl:1165-1222`
is gain times light colour/attenuation times grazing factor times angular wrap
times backlight gate times albedo tint. The Light Wrap checkbox does not appear
in that function. Both deferred and forward lighting call `rigRimTermEx`;
the forward result is added to the light at `deferredUtil.glsl:1292` and
`:1434`. The deferred spot/point paths likewise add the term during lighting
(`shaders/class3/deferred/spotLightF.glsl:260,368` and
`shaders/class3/deferred/pointLightF.glsl:164,175`).
The separate HDR wrap composite is gated by WrapEnabled and positive strength
at `indra/newview/pipeline.cpp:23759` and adds background-coloured light along
the silhouette (`shaders/class1/deferred/rigRimWrapF.glsl:130`).

Exact reasons the base rim can be invisible or nearly zero, independent of wrap:

- Missing master enable, zero master gain or no enabled rig slot:
  `alcinerigrim.cpp:119-120`; non-rig emitters get zero parameters at `:321-326`.
- Face-on normals: `g = pow(1 - abs(dot(n_r, v)), k_eff)` is zero at face-on
  and can be small near face-on for sharp presets (`deferredUtil.glsl:1200`).
- Unlit-facing normals: the per-light angular factor clamps to zero when
  `dot(n_r, l) <= -rim.z` (`deferredUtil.glsl:1202-1203`).
- Light not behind the subject: `smoothstep(b0, b0 + backSoftness, -dot(v,l))`
  is zero below back bias, and small near that threshold
  (`deferredUtil.glsl:1205-1207`). Shadowing, spotlight/distance attenuation and
  light intensity also scale the supplied light colour.

These are authored directional-light behavior, not a wrap-off bug. For example,
Subtle Edge with unit white light/albedo, grazing normal, `dot(n,l)=sqrt(.75)`
and `-dot(v,l)=.5` gives approximately **0.48** per channel before attenuation,
even with Light Wrap disabled. No in-world rendering was performed for this fix.

## Per-preset wrap values

All columns are `CineRigRimWrap` suffixes. Width, Blur and Clamp retain the
renderer/settings units. Every preset enables wrap with positive strength. Wider/more blurred wraps
serve soft halos, while portrait/skin looks use restrained strengths and crisp
looks use a faint, tight wrap.

| Preset | Enabled | Strength | Width | Blur | Saturation | Clamp |
| --- | --- | --- | --- | --- | --- | --- |
| Subtle Edge | on | 0.25 | 1 | 0.5 | 0.5 | 2 |
| Soft Portrait Wrap | on | 0.65 | 3 | 1.5 | 0.65 | 4 |
| Strong Backlight | on | 0.55 | 2 | 0.8 | 0.7 | 6 |
| Hair Shine | on | 0.25 | 1 | 0.5 | 0.7 | 4 |
| Silhouette Glow (any angle) | on | 0.8 | 3.5 | 1.8 | 0.7 | 6 |
| Neon / Stage | on | 0.25 | 1 | 0.5 | 1 | 8 |
| Dreamy Halo | on | 1.1 | 4.5 | 2.5 | 0.6 | 6 |
| Noir Kicker | on | 0.25 | 1 | 0.5 | 0.3 | 2 |
| Two-Sided Kick | on | 0.25 | 1.5 | 0.5 | 0.7 | 4 |
| Fashion Edge | on | 0.35 | 2 | 1 | 0.65 | 4 |
| Sci-Fi Hologram | on | 0.7 | 2.5 | 1.2 | 1 | 6 |
| Candle Glow | on | 0.5 | 2.5 | 1.8 | 0.8 | 4 |
| Metal Sheen | on | 0.25 | 1 | 0.5 | 0.7 | 4 |
| Skin Sheen | on | 0.25 | 2 | 1.2 | 0.55 | 3 |
| Soft Fill Wrap | on | 0.65 | 3.5 | 2 | 0.65 | 4 |
| Horror Edge | on | 0.25 | 1 | 0.5 | 0.4 | 4 |
| Moonlit Rim | on | 0.35 | 2 | 1.2 | 0.5 | 3 |
| Glass Shell | on | 0.25 | 1 | 0.5 | 0.5 | 4 |
| Tron Suit Kick | on | 0.25 | 1 | 0.5 | 1 | 6 |
| Ares Red Backlight | on | 0.25 | 1.5 | 0.5 | 1 | 6 |
| Golden Hour Flare | on | 1.4 | 4.5 | 2.2 | 0.85 | 8 |
| Glacial Frost Edge | on | 0.25 | 1 | 0.5 | 0.5 | 2 |
| Strobe Cut | on | 0.25 | 1 | 0.3 | 0.7 | 4 |
| Aquatic Caustic Rim | on | 0.9 | 3.2 | 1.8 | 0.9 | 6 |
| Ember Lick | on | 0.75 | 2.8 | 1.4 | 0.8 | 5 |
| Overcast Diffusion | on | 0.6 | 5 | 2.5 | 0.4 | 2.5 |
| Clamshell Beauty Edge | on | 0.5 | 2 | 1.2 | 0.6 | 3.5 |
| Limelight Beam | on | 0.8 | 2.2 | 1 | 0.75 | 7 |
| Crypt Pale Glow | on | 0.7 | 3 | 1.6 | 0.2 | 3 |
| Rain Slick Glaze | on | 0.25 | 1 | 0.5 | 0.6 | 4 |
| Dusk Horizon Kick | on | 1.1 | 3.5 | 1.5 | 0.8 | 6.5 |
| Spectral Silhouette | on | 1.6 | 6 | 2.6 | 0.3 | 4.5 |

## Regression and checks

`scripts/code_tools/test_rim_preset_wrap.py` compiles the production C++ tables,
both loader overloads, `applyPresetByName`, `presetNames`, ownership selection,
reset and apply definitions verbatim. It uses small LLSD/control-storage/history
scope doubles and loads the real defaults/comments from settings.xml. It does
not build or launch the viewer and does not exercise GPU output, real settings
signals or undo/history. Run it from a VS developer shell:

```text
python scripts/code_tools/test_rim_preset_wrap.py
```

The test asserts stable names/order, nonempty descriptions, one explicit occurrence of each wrap key
per row, RIM ownership of all six keys, exact resulting wrap settings and positive
strength and Enabled=true for every preset. It applies every preset after two dirty settings states
and every possible preceding preset (1,088 checked applications), checks excluded
settings are preserved, and checks unknown names have no effects. Expected values
are independent of the production tables. A temporary mutation removing only the
legacy wrap write must fail the resulting-settings assertions.

Validation, 2026-10-18:

- PASS: standalone MSVC regression, all 32 presets and 1,088 checked applications.
- PASS: mutation check; removing the legacy wrap write fails on
  `Subtle Edge CineRigRimWrapEnabled` after the production default reset.
- PASS: comparison against the pre-follow-up working copy confirms only the
  13 authorized wrap rows and five descriptions changed (32 lines); names,
  order, all other values, ownership and defaults are unchanged.
- PASS: `zs_check.ps1 -Files indra/newview/alrigrimcontroller.cpp`, 1 TU,
  1 pass, 0 failures (`.tmp/rim_followup_zs.log`). No header was touched.
- PASS: `prebuild_guard.ps1 -ChecksOnly`, all gates, exit 0; no object purge
  (`.tmp/rim_followup_guard.log`). The invocation sets exactly
  `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`,
  `GIT_CONFIG_VALUE_0=false`. It uses the existing workspace-local readable Git
  global config, workspace-local temp/cache paths and a full-path Python wrapper.
  Gate scripts, conditions and baselines are unchanged.
- PASS: `git diff --check`.

The original guard run detected stale source references after C++ lines moved.
The follow-up reran the same generator and freshness check.
Running the existing page-source generator updated only rim source references in
`page_sources.json`, `page_sources.md` and `pages/light_rim_tab.md`; the final
freshness check passed. No unrelated presets/settings were changed. Stashes were
not accessed. No viewer build, launch, commit or push was performed.
