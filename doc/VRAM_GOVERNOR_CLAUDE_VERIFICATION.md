# Claude Adversarial Verification Packet: Machinima VRAM Governor

## Reviewer instructions

Perform an adversarial, read-only review of the VRAM governor in this repository
(`I:\alchemy-machinima`). Do not edit, format, configure, build, or run the
viewer. Treat `doc/VRAM_GOVERNOR.md` as design intent, not proof. Verify all
claims from source and report concrete findings before summaries.

This is the **Alchemy Machinima client**, not Firestorm. Do not expect
Firestorm-only keys such as `FSLimitTextureVRAMUsage`, Firestorm preference
panels, DXGI budgeting, or a Firestorm executable. Here,
`RenderMaxVRAMBudget == 0` is Auto and a nonzero value is Custom. The UI belongs
to `floater_preferences_graphics_advanced.xml`.

## Required outcome

The viewer must let the user choose an Auto or Custom VRAM cap, account for the
main viewer-owned GPU allocations without double-counting render targets, and
manage mip-bearing texture residency with an immediate-entry/hysteretic-recovery
state machine. It must degrade safely when vendor memory telemetry is absent.

The Machinima capture pin is conditional: while capture mode is active and the
governor is Normal it forces discard bias to 1.0 and requests full-resolution
textures. At Pressure or Critical, the user-selected cap must take ownership,
retain the applicable bias floor, and disable both capture full-resolution
request paths. `TextureLoadFullRes` must also yield to pressure. The capture pin
must remain armed and restore quality only after recovery to Normal.

## Files in scope

Primary governor:

- `indra/newview/llviewertexture.cpp`
- `indra/newview/llviewertexture.h`

Accounting and allocation lifecycle:

- `indra/llrender/llimagegl.cpp`
- `indra/llrender/llimagegl.h`
- `indra/llrender/llrendertarget.cpp`
- `indra/llrender/llrendertarget.h`
- `indra/llrender/llcubemap.cpp`
- `indra/llrender/llcubemaparray.cpp`
- `indra/llrender/llvertexbuffer.cpp`
- `indra/newview/lldrawpoolbump.cpp`
- `indra/newview/llmaniptranslate.cpp`
- `indra/newview/fsmaniptranslatejoint.cpp`
- `indra/newview/llreflectionmapmanager.cpp`
- every other result from `rg -n "setManualImage\\(|alloc_tex_image\\(" indra`

Settings, UI, and diagnostics:

- `indra/newview/app_settings/settings.xml`
- `indra/newview/llfloaterpreferencesgraphicsadvanced.cpp`
- `indra/newview/llfloaterpreferencesgraphicsadvanced.h`
- `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`
- `indra/newview/lltextureview.cpp`

Machinima interactions:

- capture-mode resolution and final bias override in
  `indra/newview/llviewertexture.cpp`
- any consumer of `sDesiredDiscardBias`
- background/minimize discard behavior in `LLViewerTexture::updateClass()`

Design record:

- `doc/VRAM_GOVERNOR.md`

## Accounting invariants to prove or disprove

For every tracked GL texture name, verify the record and all global counters are
updated and removed symmetrically:

```text
base       = level-zero allocation
estimated  = full resident estimate, including mips only when present
mipmapped  = estimated for mip-bearing records, otherwise zero

tracked    = estimated textures + vertex buffers
reducible  = mipmapped texture estimate
fixed      = max(tracked - reducible, 0)
```

Verify specifically:

1. Full mip chains terminate only when both dimensions reach 1, including
   1024×1 and other non-square textures.
2. `setManualImage` changes accounting only at mip level zero; later manual mip
   uploads cannot replace the base record with the smallest mip.
3. Generic `GL_COMPRESSED_*` and explicit DXT formats are estimated at a
   defensible block-compressed size, with and without mips.
4. Direct `glCompressedTexImage2D` paths free the previous record and allocate
   exactly one replacement record.
5. Six-face legacy cubemaps and cubemap arrays charge every face exactly once.
6. Generated bump-map mips, reflection textures, render-target mips, scale-down,
   and manual manipulator mip chains carry the correct mip classification.
7. Color/depth render-target GL errors remove the exact texture record, delete
   the GL name, reset ownership state, and do not increment or leak
   `LLRenderTarget::sBytesAllocated`.
8. `LLRenderTarget::sBytesAllocated` is diagnostic only and is not added to
   `tracked` after its textures were already counted by `LLImageGL`.
9. An unexpected release-build record replacement cannot drift counters upward.
10. List any live texture path that bypasses these wrappers and quantify the
    likely undercount where possible.

## Budget and state-machine invariants

Verify:

- Auto cap is detected VRAM divided by `max(RenderTextureVRAMDivisor, 1)`.
- Custom mode is exactly `RenderMaxVRAMBudget > 0`.
- Configured cap clamps to 768 MB through detected VRAM, including cold-start or
  zero detection and a stale persisted value above the current adapter.
- Effective cap is `max(configured, fixed + 256 MB)`.
- Pressure ratio is `reducible / max(effective - fixed, 1 MB)`.
- Entry boundaries are inclusive at 85% and 95%.
- Driver reserve is 10% of detected VRAM, clamped to 256–1024 MB.
- A new NVIDIA eviction, low system memory, 95% cap use, or driver headroom below
  half reserve immediately produces Critical.
- Recovery is one rung per independent five-second interval: Critical→Pressure
  below 90% and Pressure→Normal below 80%.
- Recovery target changes reset the timer; boundary hovering cannot reuse stale
  elapsed time.
- Only observed pressure ramps bias upward. A latched recovery hold enforces its
  floor but does not continue ratcheting bias to 4.0 after the signal clears.
- Normal recovery can decrement bias below 80% with sufficient system/driver
  headroom.
- Background restoration cannot undo an active pressure floor.
- Capture pins bias to 1.0 only at Normal. Pressure/Critical floors must win.
- Both full-resolution request sites must yield whenever VRAM pressure is
  active, including requests caused by `TextureLoadFullRes`.
- Capture quality must resume only after hysteretic recovery reaches Normal.
- System-memory factor uses floating-point arithmetic and remains bounded.

## Driver/platform checks

Verify from the local OpenGL declarations and call site:

1. `updateClass()` executes on the render thread with an active GL context.
2. Vendor queries run at most once per second and are skipped while
   `gGLManager.mInited` is false.
3. Query destinations initialize to `-1`; only `GL_NO_ERROR && value >= 0`
   becomes valid data.
4. Unsupported/failed telemetry cannot become a false zero-free-memory Critical
   signal.
5. NVIDIA current-free and eviction-count units/semantics are handled correctly.
6. ATI texture free-memory semantics are suitable as a coarse pressure signal.
7. Eviction reset, wrap, and a long-running counter do not create arithmetic or
   false-positive problems.
8. Driver values are adapter-wide. Explain the implications for two viewer
   instances and other GPU-heavy applications.
9. Public telemetry is accessed only where its render-thread update model is
   safe.

## UI/settings checks

Verify:

- The slider exists in the Machinima advanced graphics floater, not a different
  viewer's panel.
- The slider's floating-point UI value is explicitly normalized into the
  `RenderMaxVRAMBudget` U32 setting, and the governor reads the live U32 rather
  than retaining a stale Auto value in a typed cache.
- Its range is 0 through detected VRAM, with 0 preserved as Auto.
- The displayed Auto/Custom mode matches the actual budget branch.
- Live status displays tracked/effective cap, pressure, reducible textures, and
  driver Unavailable rather than a negative value.
- An effective-cap fixed-floor adjustment is visible.
- Control changes take effect live through the saved setting.
- Preferences Apply/Cancel behavior does not unexpectedly commit or clamp a
  value merely by opening the floater.
- The XUI is well formed, the text fits/wraps, and the added vertical space does
  not overlap Machinima's tone-mapping controls or bottom buttons.
- `llformat` specifiers and temporary string lifetimes are valid.
- The texture console has nine distinct rows with no overlaps and labels render
  target memory as a subset/diagnostic rather than adding it to Total.

## Required scenario matrix

Walk each scenario frame by frame and mark Pass, Fail, or Runtime required:

1. Cold start before reliable VRAM detection.
2. Auto mode with divisor 0, 1, and an extreme value.
3. Custom 768 MB, exactly detected VRAM, and stale above detected VRAM.
4. Reducible use at 84%, 85%, 94%, 95%, 90%, 89%, 80%, and 79%.
5. Driver telemetry unavailable for the whole session.
6. NVIDIA free memory crossing reserve and half-reserve.
7. NVIDIA eviction count increasing once, unchanged, reset, and wrapped.
8. ATI query valid, zero, failed, and implausibly large.
9. Low system RAM with low GPU allocation use.
10. Pressure → minimize → Critical → restore.
11. A one-frame Critical spike that instantly resolves: prove bias does not ramp
    to 4.0 throughout both recovery holds.
12. Allocation use hovering in each hysteresis dead band.
13. Two viewers individually below cap but collectively exhausting the adapter.
14. Fixed allocations already above a small Custom cap.
15. GL initialization, context loss, and shutdown windows.
16. Generic compressed and explicit DXT uploads, both with/without mip chains.
17. Render-target color and depth OOM/error after tracker insertion.
18. Capture begins during Critical: bias/full-resolution overrides remain
    suppressed, the UI reports cap enforcement, and quality resumes only after
    hysteretic recovery reaches Normal.
19. Cubemap faces repeatedly initialized and later deleted.
20. A 4096×1 mip-bearing texture and a no-mip noise/depth texture.

## Static commands allowed

```powershell
git status --short
git diff --check
git diff -- indra/llrender/llimagegl.cpp indra/llrender/llimagegl.h
git diff -- indra/llrender/llrendertarget.cpp indra/llrender/llcubemap.cpp indra/llrender/llcubemaparray.cpp
git diff -- indra/newview/llviewertexture.cpp indra/newview/llviewertexture.h
git diff -- indra/newview/llfloaterpreferencesgraphicsadvanced.cpp indra/newview/llfloaterpreferencesgraphicsadvanced.h
git diff -- indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml
git diff -- indra/newview/lltextureview.cpp indra/newview/app_settings/settings.xml
rg -n -S "VRAM|RenderMaxVRAMBudget|sDesiredDiscardBias|setManualImage\\(|alloc_tex_image\\(" indra
[xml](Get-Content indra/newview/app_settings/settings.xml -Raw) | Out-Null
[xml](Get-Content indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml -Raw) | Out-Null
```

## Validation boundary

Codex ran these integration gates on 2026-08-30:

- `git diff --check`: clean (only unrelated existing line-ending warnings).
- Both edited XML files parsed successfully with PowerShell's XML parser.
- All `setManualImage`, `alloc_tex_image`, and `update_tex_image` call sites were
  enumerated and audited.
- `cmake --build build-Windows-vs2026-os --config Release --target viewer -- /m`
  completed with exit code 0 using Visual Studio 18 2026/MSBuild 18.7.8.
- Output after cap propagation, layout remediation, and pressure-aware capture
  ownership:
  `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`, 82,355,200 bytes,
  timestamp 2026-08-30 12:53:46 local, SHA-256
  `9049E04DC333D170A8CEF98016CE0D19C997AF603A0448177BF7427273C90866`.
- Staged `settings.xml` and
  `skins/default/xui/en/floater_preferences_graphics_advanced.xml` SHA-256
  hashes exactly matched their source files and contained `VRAMCap`,
  `VRAMStatus`, and `RenderMaxVRAMBudget`.
- The staged floater is 825×632 XUI units and places a three-row live status
  block in a dedicated full-width bordered panel. The user-reported overlap
  between the old inline status text, VSync, and Avatar heading is removed.
- Static inspection confirms that capture bias and both full-resolution texture
  request sites call the shared pressure-aware policy. The standalone
  `TextureLoadFullRes` override also yields at Pressure/Critical.

The final corrective build linked and staged successfully with no viewer
process holding the output executable.

A successful build is compile/link evidence only. Hardware behavior under real
residency pressure, real NVIDIA eviction, AMD telemetry, multiple clients,
forced render-target OOM, UI visual layout, and capture-mode overload remains
runtime verification unless separately evidenced.

## Required response format

1. **Verdict:** Ready, Ready with risks, or Not ready.
2. **Findings:** P0–P3, each with file/line, triggering sequence, impact, and a
   concrete correction.
3. **Scenario matrix:** all 20 scenarios.
4. **Accounting assessment:** counted, double-counted, undercounted, estimated,
   and classification errors.
5. **State-machine proof:** entry, recovery, timers, bias ramp/decay, background,
   and pressure-aware capture ownership transfer.
6. **Platform assessment:** NVIDIA, AMD/ATI, unsupported fallback, multi-client.
7. **UI/settings assessment:** visibility, range, live behavior, cancellation,
   layout, localization fallback.
8. **Runtime gaps:** smallest useful hardware test plan.
9. **Final recommendation:** exact merge blockers and follow-ups.

Do not provide a generic summary. Demonstrate every claimed defect with a
specific state transition or allocation/deletion sequence.
