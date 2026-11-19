# Machinima Creative Merge Fork — Plan (for Codex)

**Scope (user ruling, "only pure-perf out"):** re-port the **full creative machinima fork** onto **current** Alchemy upstream — the Director's Console suite **and** all creative/capture rendering — dropping **only** the pure-performance stack. This is ≈ the full `doc/MERGE_PLAYBOOK.md` re-port, re-pinned to current upstream, minus perf, with **two features that must be REBUILT (not re-ported)** against the updated upstream: the full VCam prism and the revamped Lightbox.

**Governing document:** `doc/MERGE_PLAYBOOK.md` supplies the methodology and the ordered 12-step slice sequence verbatim (integration branch at a pinned upstream SHA, full adjudicated ledger, dependency-ordered *buildable* vertical slices, per-slice checkpoints, the shader co-land rule, the destructive-ref-swap endgame with `--force-with-lease`). **THIS file changes only three things** and otherwise defers to the playbook: (1) re-pin to **current** upstream develop, not `af0f3bd`; (2) the ledger `Action` for the perf rows is **DROP-OBSOLETE/OUT**; (3) insert **two REBUILD slices** (VCam prism, Lightbox). Everything else — the ledger gate, the checkpoints, the shader/program closure rule, the endgame — is the playbook's.

---

## 1. Grounding — pin fresh before anything

The base is **freshly-fetched current upstream Alchemy in THIS repo** — not a new clone. Both remotes are already configured (`alchemy-upstream` = AlchemyViewer/Alchemy, `origin` = the fork). `git fetch alchemy-upstream`, then cut the pristine checkpoint + integration branch at the fetched upstream `develop` SHA, and re-port the creative features onto it. (Whether **Absolute Machinima** also gets its own new/renamed `origin` GitHub repo is a separate repo/remote decision — see §Rebranding.) Pin and record (values current at authoring; re-pin to the fresh fetch):

| Ref | SHA (re-verify) | Meaning |
|---|---|---|
| Merge base | `7c11d3f38bd` | common ancestor |
| Upstream target | `alchemy-upstream/develop` @ `60aa4149f63` | base to re-port onto (re-pin fresh) |
| Fork head | `9ebee5b54c5` (`feature/ultimate-diopter`) | source of the fork surface |
| Prior full-fork re-port (reference) | `integration/upstream-af0f3bd`, `checkpoint/00-upstream-pristine-af0f3bd`, `checkpoint/00-ledger-adjudicated` | worked example onto `af0f3bd`; **not** the base — upstream has moved past it |

Divergence at authoring: fork **+403**, upstream **+262** from the merge base (the prior effort was 327/255 onto `af0f3bd`; there is more drift now). Remotes: `alchemy-upstream` = AlchemyViewer/Alchemy, `origin` = bwlupus-ctrl/Alchemy-Machinima.

Branch naming (existing convention): pristine `checkpoint/00-upstream-pristine-<sha>`; integration `integration/machinima-upstream-<sha>`; per-slice `checkpoint/NN-<slice>`.

---

## 2. Scope: IN / OUT / REBUILD

### IN — the full creative/capture surface
- **Director's Console suite:** `lldirectorcast`, `llfloaterdirector` + hotkeys, `aldirectorswitcher*`, `alpaneldirector{switcher,animswitcher}`, `alfloaterdirectory`, and all director XUI/menus.
- **Actor/Path engine + UI:** `llactormover`, `alpathgeometry` (shapes, corner fillets, schema 4), `altoolpathedit`, `alpanelpatheditor`, `alpanelactormover`, path-guide occlusion render.
- **Cinematic camera + switcher + FULL VCam prism** — VCam prism is a **REBUILD** slice (§3).
- **Ghost/clone:** `llghostavatar`, `alpanelghoststudio`, `gActorGhostProgram` + adapted GLSL, clone hooks around upstream impostor bake.
- **Cine Light Rig:** `alcinelightrig*`, `alpanelcinelightrig`, light hooks + GLSL.
- **Gaze:** `algaze*` + `ALGazeMotor` + avatar/actor hooks.
- **ReShade sidecar bundle (MUST) — its ENTIRE dependency chain is KEEP-ADAPT / KEEP-AS-IS; drop nothing that the sidecar needs to function.** Even where a piece reads from an upstream-owned primitive (reverse-Z depth, the normal encoding, samplers), the sidecar's own packing/provider/publish path is preserved and *adapted to consume upstream's current encoding* — consuming an upstream primitive is an ADAPT, never a reason to cede the sidecar's data path. The chain:
  - **Addon (KEEP-AS-IS, adapt to upstream G-buffer semantics):** `reshade-addon/shaders/SL_GBufferProvider.fx`, `SL_Bridge.fxh`, `SL_BridgeDebug.fx`; `reshade-addon/src/sl_reshade_bridge.cpp`.
  - **Bridge + ABI (KEEP-ADAPT):** `indra/newview/llreshadebridge.{cpp,h}`, `llreshadebridgeabi.h`.
  - **Sidecar MRT + G-buffer publish (KEEP-ADAPT into upstream `pipeline.cpp`):** the extra color attachments on `mRT->screen` — the **visible-diffuse/albedo sidecar** (`RenderVisibleDiffuseSidecar`, the `GL_SRGB8_ALPHA8` attachment + coverage/provenance gate, [S1]); the depth/normal taps the provider reads.
  - **Velocity / motion vectors (KEEP-ADAPT):** the `GL_RG16F` `mVelocityMap`, the velocity geometry pass sharing the deferred depth, previous-frame projection (`sLastVelocityProjMat`/`sHasLastVelocityProjection`), `sVelocityRender`, and the velocity/avatar-velocity **programs + GLSL** ([BDMerge A5.4-1a]) — required for any temporal ReShade effect (motion blur, RTGI, TAA).
  - **Bundle riders per playbook Step 6:** temporal capture, motion blur, 10-bit/LPM output, sidecar color-space.
  - **Settings/defines (KEEP-ADAPT):** `BDMergeVelocityBuffer`/`Debug`, `BDMergeMotionBlur`, `RenderVisibleDiffuseSidecar`, `RenderReShadeGBuffer*`, and the shader define `SL_PROVIDE_MOTION` (ship default preserved).
- **Projector volumetrics + froxel (MUST):** playbook Step 7 — targets, C++ passes, all class1/class3 GLSL, shared upstream shadow/deferred adapters, settings, UI.
- **Weather (MUST):** playbook Step 8 — model/panel/renderer, rain/surface/lightning/upsample programs + GLSL, assets, the rain-occlusion depth convention.
- **Lightbox (MUST) — REBUILD** slice (§3): color grade / LUT / CAS / tonemap grade + the recent revamp.
- **Alpha interleaving / forced mask / sidecar draw** (playbook Step 5) — prerequisite plumbing for the above.
- All shared-file hooks, shaders, settings, XUI, assets, CMake/manifest entries these require.

### OUT — pure performance only
- **VRAM budget governor** (the default-off toggle feature just reviewed) — including the capture-mode pin's *dependency on the governor's pressure state* (§4 severance).
- **SPGO / build-perf, threading, decode-pool sizing, thread priority/affinity, perf-tuning A/Bs.**
- **Renderer primitives that upstream now owns** — sampler objects, immutable `LLRenderTarget`/`LLImageGL`, `ALUniformBuffer`/engine UBOs, reverse-Z, the SH/reflection-probe **engine** (`LLReflectionMap` capture/store/sample, the hero-probe manager), modern shadows, PBR corrections, impostor bake → **UPSTREAM-WINS**; re-port only the fork primitives later creative slices require (playbook Step 1).

  **Not to be confused with feature-level probe usage, which is IN.** "Probes = upstream-wins" means only the probe *engine*. Fork features that *drive* probes stay and are **KEEP-ADAPT** (re-point their calls at upstream's probe API): the **Cine Light Rig live/wearable probe** (`ALCineLightRig::liveProbe*`, `mLiveProbeBounceScale`, per-light `mHeroEnabled` hero-projector toggles + their scene persistence, the manager's `LLReflectionMap` live probe) and the **hero-probe mirror** system. None of these are dropped.

### REBUILD (not re-port) — dedicated design+build slices, each with its own review + acceptance
- **Full VCam prism.** The prism-lens capture is tightly coupled to render-target lifecycle, sampler binding, clip planes, reverse-Z, and the SH/SSR Prism guards — all of which upstream changed. Re-porting the fork prism onto current upstream will not work. **Rebuild** the prism capture against the current upstream render architecture, preserving the committed API surface its consumers expect (`updateCamera`/`applyFrameLens`/`resolveAnchor`/`captureCurrentSwitcherView`/`pattern*()`, `LLPrismLens::MAX_CAPTURES`, `onRenderTargetsReleased`, clip/composite accessors — see `MERGE_VCAM_ADAPTATION.md`). Land after the renderer primitives + clone/alpha are on current upstream; run the full VCam acceptance matrix. Excludes the stash-only live-feed cookie remap / `alprismcamdriver` per the playbook.
- **Lightbox.** The revamped Lightbox (color grade, LUT, CAS, tonemap grade, projvol-adjacent controls) must re-integrate against upstream's post/color pipeline. **Rebuild** its render hooks + UI wiring onto current upstream rather than hunk-adapting the fork version; own in-world grade check. Note the tonemap-grade sliders now live in the Lightbox (recent change), so the grade shader + settings ride with it.

---

## 2b. Rebranding — "Absolute Machinima"

The fork ships as **Absolute Machinima**, not Alchemy. Because upstream stays Alchemy, the rebrand is a **permanent override that re-applies on every future upstream merge** — so keep it *thin and centralized* (drive off the existing `VIEWER_CHANNEL` the build already keys on) and mark the override set clearly so it re-applies cleanly each pull.

**Surface (ground truth from the repo):**
- **Master knob — `VIEWER_CHANNEL`** (`indra/newview/CMakeLists.txt`) already drives the output binary name (`VIEWER_CHANNEL_ONEWORD` → the exe, currently `AlchemyTest.exe` → `AbsoluteMachinima.exe`), macOS bundle names/strings, the `--channel=` fed to `viewer_manifest`/version, symbol-tarball names, and product name. Set `VIEWER_CHANNEL = "Absolute Machinima"` (+ the ONEWORD variant) where the channel default is defined (Codex locates it — build arg / CMake cache / channel file). **This one change carries most of the rebrand.**
- **Grid channel + per-user data dir:** the channel string is reported to the grid AND names the per-user settings/cache directory, so "Absolute Machinima" gets its own data dir (won't clobber an Alchemy install). Verify the settings-dir derivation follows the channel.
- **Strings / About:** `strings.xml`, `floater_about.xml` — app name, About/credits, splash/login text, help/support/feedback URLs. **Preserve the Alchemy + Linden Lab attribution and the LGPL/GPL notices** (rebrand is permitted; stripping attribution is not).
- **Art:** `indra/newview/res/`, `res-sdl/`, `installers/` — `icon1.ico`, `ll_icon.ico/.BMP`, `install_icon`/`uninstall_icon.BMP`, `loginbackground.bmp`, `bitmap2.bmp`, splash. **User supplies the Absolute Machinima art.**
- **Installer/manifest:** `installers/` (Windows NSIS branding) + `viewer_manifest` channel plumbing.
- **Version:** `indra/cmake/BuildVersion.cmake` if the version scheme changes.

**Decisions/actions the user owns (needed before the branding slice runs):**
- **TPV compliance + channel registration:** register the "Absolute Machinima" channel in the Second Life Third-Party Viewer directory for proper grid access + viewer stats; keep TPV-required notices; do not present as an official/LL viewer. (Registration/account action, not code.)
- **New origin repo — YES (decided).** Absolute Machinima gets its **own** GitHub `origin` repo (proposed `Absolute-Machinima`) as its published home; the current `bwlupus-ctrl/Alchemy-Machinima` origin is retained for the existing fork. Create the new repo at publish time (endgame), add it as a remote, and push the accepted integration branch there — not before there is anything to publish. (User confirms repo name + visibility when ready.)
- Supply the exact channel name + ONEWORD + the art set.

**Compile-safety guardrail — the prior branding attempt did NOT build. Do not repeat it.** The rebrand is **display strings + channel config + art ONLY**. Concretely:
- **Never blind-replace "Alchemy" across the tree.** `AL*`/`al*` class and file prefixes, `Alchemy*`/`al_*` **settings-key names**, namespaces, macros, and function/identifier names are **code**, not branding — replacing them breaks the build. Rebrand only *user-visible display text* (About/splash/window title) + the *channel/version config* + *art*, and leave every identifier alone.
- **The space is a trap.** "Absolute Machinima" (with the space) is a *display* string only. Every place that derives a **token** — the output binary/exe name, any compile define, a path segment, a manifest key — must use the space-free ONEWORD form (`AbsoluteMachinima`). Set `VIEWER_CHANNEL_ONEWORD` explicitly and confirm the exe/manifest/symbol paths use ONEWORD, not the spaced channel.
- Build the branding slice on its own checkpoint and capture the first error; if it differs from the two causes above, record it in this doc.

**Slice placement:** land branding **late** — after features + static closure, with/just before build acceptance — as `checkpoint/NN-branding-absolute-machinima`, isolated so it is a clean, re-appliable override each merge.

---

## 3. Slice map — playbook Steps 0–12, annotated for this scope

Use the playbook's steps as written; the annotations are the only deltas.

| Playbook step | This fork |
|---|---|
| 0 — Baseline + full ledger | Partition every fork row IN/OUT/REBUILD. **OUT = perf only** (VRAM governor, SPGO/threading). Mark VCam-prism + Lightbox **REBUILD**. Gate: zero unassigned rows. |
| 1 — Renderer core + fork primitives | UPSTREAM-WINS on primitives; re-port only fork primitives the creative slices need (indexed draw guard, sidecar attachment constants, 10-bit formats, outer-transform holders). |
| 2 — Render-independent app foundation | Director engine (Cast, actor mover, cinematic-camera control, switcher, gaze), renderer-independent classes. |
| 3 — VCam | **REBUILD** (§3). May sequence later than the playbook's Step 3 — after primitives + clone/alpha give a stable render base to rebuild against. |
| 4 — Clone/ghost | As playbook: `gActorGhostProgram` + GLSL co-land, clone hooks around upstream impostor bake, Ghost Studio files. |
| 5 — Alpha / forced mask / sidecar draw | As playbook (prerequisite for sidecar + interleave). |
| 6 — Velocity / temporal / motion blur / 10-bit / **ReShade sidecar (MUST)** | Full bundle IN. Co-land every program with its GLSL; document sidecar MRT layout + color space. |
| 7 — **Projvol + froxel (MUST)** | Full slice IN — targets, passes, all class1/class3 GLSL, shared shadow/deferred adapters, settings, UI. |
| 8 — **Weather (MUST)** | Full slice IN — model/panel/renderer, rain/surface/lightning/upsample GLSL, assets; state the rain-occlusion depth convention. |
| 9 — App/UI/asset closure | Director panels/floaters/menus, Cine Light Rig, path/actor UI, **Lightbox REBUILD**, remaining hooks; every unassigned creative ledger row. |
| 10 — Static adversarial closure | Zero unassigned rows; zero OUT rows landed; all programs paired with GLSL; XML/settings parse, no dup/dead keys. |
| 10b — **Rebranding (Absolute Machinima)** | Dedicated slice (§2b): set `VIEWER_CHANNEL`, strings/About, art, installer, per-user data dir; preserve Alchemy/LL attribution; isolate as a re-appliable override. `checkpoint/NN-branding-absolute-machinima`. |
| 11 — Authorized build + in-world acceptance | Build only at closure. Acceptance per creative feature: director hub, actor/path (fillets, schema 4), ghost, cine light, gaze, **VCam prism**, temporal/sidecar/ReShade, projvol/froxel, weather, **Lightbox** grade, water/mirrors/cube. |
| 12 — Endgame | **Parallel line by default** (§5). Ref-swap only with separate explicit approval. |

---

## 4. Severing the perf dependency (the sharp entanglement)

Several creative features were authored atop the fork's optimized pipeline. Cut those cleanly — a creative feature must not drag an OUT perf feature in:
- **Capture-mode pin ↔ VRAM governor.** The current capture-quality pin gates on `isVRAMPressureActive()` / `isCaptureQualityPinActive()` — governor state that is OUT of this fork. Rebuild the capture pin to its **pre-governor** form: `isCaptureModeActive()` (which reads `sCaptureModeActive && !BDMergeMemoryBudget::isCritical()` — the RAM decode-pool signal, **not** the VRAM governor, and stays) pins full quality unconditionally; drop the `sVRAMPressureState`/`isVRAMPressureActive` helpers and the full-res-yields-to-pressure branches. This restores the machinima capture behavior without the governor.
- **Tonemap grade / Lightbox ↔ VRAM/optimization.** The grade is creative (rides with the Lightbox rebuild); keep it, but re-express any hook that referenced the perf pipeline against upstream.
- Ledger discipline: only perf rows carry `Action = OUT`. A "while we're here" perf row fails the Step 0 gate.

---

## 5. Endgame

**Parallel line by default:** `integration/machinima-upstream-<sha>` becomes the creative machinima fork on modern upstream; the perf work (VRAM governor, SPGO) stays on its own branches. **Do not ref-swap `develop` or force-push `origin`** as part of this plan — that is the playbook's separate, explicitly-approved step, and it must not collide with the still-parked 8-commit push on `feature/ultimate-diopter`. If the user later wants this fork to become mainline, follow playbook §"Critical endgame" (preserve old ref, move branch, `--force-with-lease`, never bare force) under its own approval.

---

## 6. Risks
- **The two rebuilds (VCam prism, Lightbox)** are the schedule risk — design work against a changed render base, not mechanical hunk-adaptation. Give each its own design pass + review + acceptance.
- **Upstream renderer drift (262 commits)** under the entire creative rendering stack (ghost, cine light, sidecar/temporal, projvol/froxel, weather) — the bulk of the effort; budget for shader/pipeline re-expression and co-land programs+GLSL.
- **Perf/creative severance (§4)** done wrong either breaks a creative feature or smuggles the governor in. The capture-pin rebuild is the load-bearing case.
- **Schema/settings/key collisions** with upstream; preserve Path schema 4 + creative keys; drop perf keys.

---

## 7. Codex — first concrete steps

**0. PREREQUISITE — commit the working tree first.** The ledger re-ports **committed** fork state; anything uncommitted is invisible to it (playbook excludes stash/WIP). At authoring there were ~30 uncommitted tracked files, many **IN-scope creative** — Cine Light Rig (`alcinelightrig*`, `alpanelcinelightrig`), Ghost Studio (`alpanelghoststudio`), **ReShade sidecar** (`llreshadebridge.cpp`, `SL_Bridge.fxh`, `SL_GBufferProvider.fx`), director hooks (`llfloaterdirector`, `llviewermenu`, `menu_viewer.xml`, `llviewerdisplay`), path UI (`alpanelpatheditor*`, `panel_path_editor.xml`), plus `pipeline.*`/`llshadermgr.*`/`materialF.glsl`/`llsettingsvo.*`. **Commit all IN-scope creative work by feature (explicit paths, never `git add -A` — `.tmp.driveupload`/logs must stay out)** so `<forkHead>` contains it. OUT/perf work (VRAM governor + toggle, graphics-advanced) may stay uncommitted or go on its own branch. Re-pin `<forkHead>` after committing.

1. `git fetch alchemy-upstream`; re-pin the SHAs in §1; record them.
2. Create `checkpoint/00-upstream-pristine-<sha>` and `integration/machinima-upstream-<sha>` at the pinned upstream SHA.
3. Generate the full `7c11d3f..<forkHead>` ledger (playbook Step 0) and partition every row **IN / OUT(perf) / REBUILD**. **Stop and confirm** the perf-OUT list (VRAM governor, SPGO/threading, upstream-owned primitives) and the two REBUILD approaches (VCam prism, Lightbox) before landing anything.
4. Proceed slice by slice per §3 / the playbook, checkpointing each, co-landing shaders, keeping the OUT set out, and treating VCam-prism + Lightbox as design+build+review slices.

Read alongside: `MERGE_PLAYBOOK.md` (governing), `MERGE_VCAM_ADAPTATION.md` + `MERGE_SHADER_DRIFT.md` + `MERGE_CONTRACT_DIFF.md` (how shared-file/shader/VCam hooks were adapted onto the prior upstream — re-verify against current), `BDMERGE_DIRECTOR_CONSOLE_BRIEF.md` (Cast/hub architecture), and the `BDMERGE_*`/`DIRECTOR_*` per-feature briefs.
