# G5 Stage 2b Execution Briefs

Written 2026-07-10 by the coordinator (Fable) for implementer-tier models
(Sonnet/Opus). Each brief is self-contained: goal, exact anchor points,
acceptance, revert. **Read the patch-log entries for `6fbf94c6e6` (tex pool),
`6d6fdf169d` (mesh pool), and `7c89378321` (capture pin) before starting any
item.** Campaign conventions apply (BD_MERGE_PATCHLOG.md header): one
revertable commit per item, `[BDMerge <id>]` prefix, gate defaults preserve
stock behavior, patch-log entry on landing.

**Build note (bit us once already):** `--target alchemy-bin` does NOT restage
data files. After any commit touching `app_settings/` or `skins/`:
`robocopy indra\newview\app_settings build-Windows-vs2026-os\newview\Release\app_settings /E /XO`
(same for `skins`). New source files require a CMake regen (build ZERO_CHECK)
before msbuild sees them.

---

## B-2b.1 — Tiered residency for the texture pool — **DONE (24c97a2fda, coordinator-landed 2026-07-10)**

**Goal.** Replace the texture pool's flat LRU with the spec's tiers so region
changes demote instead of dump. (Spec G5.1 item 2; the LRU already handles
pan-away churn — this adds the *region grace* and *pinned location* semantics.)

**Files.** `bdmergetexpool.cpp/h` only. Do NOT touch `lltexturefetch.cpp` —
the hook API stays identical.

**Design (follow, don't reinvent).** Add per-entry metadata: `mLastHitTime`
(F64, `LLTimer::getTotalSeconds()` stamped in fetch/put) and `mRegionHandle`
(U64, the agent's region handle at insert time — grab it on the MAIN thread in
`refreshSettings()` into an atomic, read it in put; do NOT call agent/region
APIs from worker threads). Eviction order (evictToBudgetLocked): evict from
the lowest-priority populated class first —
(1) entries whose region != current AND older than
`BDMergeTexPoolRegionGraceTTL` (default 900s),
(2) entries older than `BDMergeTexPoolRecencyWindow` (default 300s),
(3) plain LRU tail. Never evict an entry hit within the last 30s.
Keep it O(1)-ish: three intrusive LRU lists (one per class) or a single list
with lazy reclassification on eviction scan — do not sort.

**Settings.** `BDMergeTexPoolRegionGraceTTL` (F32 s, 900),
`BDMergeTexPoolRecencyWindow` (F32 s, 300). Add to settings.xml with
`[BDMerge G5.1-2b-tiers]` tagged comments.

**Accept.** TP to another region and back within the TTL → spike report shows
pool hits on return (no re-decode wave). Budget still respected. Gate off =
everything drops, stock behavior.

**Revert.** Single commit revert restores flat LRU.

## B-2b.2 — G5.3 scene warm-up ("prepare take")

**Goal.** A "prepare take" action that force-loads everything in draw
distance to final resolution before rolling. (Spec G5.3; capture pin G5.2 is
landed — warm-up is the *pre-fill*, pin is the *hold*.)

**Files.** New small floater or menu item (Develop ▸ or World ▸); logic in a
new `bdmergewarmup.cpp/h` (follow the pool files' license-header pattern).

**Design.** On trigger: iterate `gTextureList` (main thread), for each
`LLViewerFetchedTexture` with `getBoostLevel() < BOOST_HIGH` that has faces
(`getNumFaces()`), call `setBoostLevel(LLGLTexture::BOOST_HIGH)` transiently
and record the texture ID; restore prior boost levels when warm-up completes
or is cancelled. Completion = N consecutive frames where
`LLAppViewer::getTextureFetch()->getNumRequests()` is below a small floor.
Show progress via a simple `LLNotificationsUtil` or the floater's text line
(count of pending fetches). Do NOT touch discard-bias code — G5.2 owns that.
Pair with `BDMergeCaptureModePin`: warming turns the pin ON automatically if
`BDMergeWarmupEngagesPin` (default true).

**Accept (from spec).** After warm-up completes, panning the prepared scene
shows no progressive sharpening. Cancel restores prior boost levels.

**Gotcha.** BOOST_HIGH on thousands of textures spikes fetch/decode load —
that is the point, but throttle: boost in batches of ~500, next batch when
pending fetches drop below the floor. 24 decode threads + the RAM pool make
this fast on the target machine.

## B-2b.3 — Surface the G5 toggles in UI

**Goal.** Machinima-usable switches: capture pin toggle, warm-up button, pool
on/off + budget readouts. No new settings — bind to the existing keys.

**Files.** Extend an existing panel (prefer the Develop menu or a small new
floater XML + registration in `llviewerfloaterreg.cpp`). Skins XML +
minimal C++ only. Menu items can bind straight to settings via
`menu_item_check.control_name` — zero C++ for the toggles.

**Accept.** Toggling via UI == toggling the debug setting; pool stats visible
somewhere (simplest: a floater text line updated from the pools'
`appendReport` strings via `std::ostringstream`).

## B-2b.4 — AVX-512 decode experiment (measure first)

**Goal.** Zen 5 has full 512-bit datapath; the decoder is OpenJPEG 2.5.4.
Try `/arch:AVX512` on the hot code and measure decode MB/s via the spike
report (before/after on the same cache-warm scene).

**How.** Two independent, separately-committed experiments:
(1) viewer-side: add `/arch:AVX512` to `alchemy-bin` compile flags behind a
CMake option `AL_AVX512` (default OFF) — this affects LL's own image loops;
(2) vcpkg side: overlay-port openjpeg with `-DCMAKE_CXX_FLAGS=/arch:AVX512`
— bigger win candidate, riskier plumbing.
Compare `decoded MB/s` and decode p50 in the spike report. If <10% gain,
document and close as no-port.

**Gotcha.** This build ships to exactly one machine (9950X) — arch-specific
flags are fair game per the campaign notes, but keep them behind the CMake
option so CI/other machines build stock.

## B-2b.5 — Per-discard texture pool entries (only if evidence appears)

**Current known behavior (deliberate Stage 2 cut):** the texture pool keeps
one best-discard entry per UUID and serves finer-than-requested hits, which
uploads more VRAM than stock would at high bias. On 32GB this is noise. Only
build per-(UUID, discard) entries if VRAM pressure or upload stalls show up
in real sessions. Mirror the mesh pool's (id, LOD) Key/KeyHash pattern.

## B-2b.6 — Extend forced alpha masking (G2.3) to PBR/GLTF materials

**Goal.** `BDMergeForceAlphaMask` currently covers legacy Blinn-Phong only —
`LLFace::canRenderAsMask()` early-outs on `te->getGLTFRenderMaterial()`, and
PBR blend faces route by `LLGLTFMaterial::mAlphaMode` in `llvovolume.cpp`
(see the `gltf_mat->mAlphaMode == ALPHA_MODE_BLEND` sites, ~5965/6060 region
and the alpha-pool classification at ~6146).

**Design.** Do NOT remove the GLTF early-out in canRenderAsMask (PBR has its
own routing). Instead, at the PBR classification sites, treat
`ALPHA_MODE_BLEND` as `ALPHA_MODE_MASK` when the gate + same exclusions hold
(base-color alpha == 1 analog: `gltf_mat->mBaseColor.mV[3] == 1.f`; respect
rigged setting). Cutoff: PBR mask path uses the material's `mAlphaCutoff` —
override per-batch with `BDMergeForceAlphaMaskCutoff` the same way the
legacy sites do (grep `[BDMerge G2.3]` in llvovolume.cpp for the pattern).
Reuse the SAME three settings — no new keys.

**Accept.** A PBR blend-mode object z-fights with itself with the gate off,
renders stable/masked with it on; legacy behavior unchanged; gate off =
bit-identical stock.

**Gotcha.** Verify the emissive-mask cutoff regression the spec warns about
(LL had a bug where emissive + mask cutoff interacted) on BOTH material
systems while testing.

## B-2b.7 — Port FS Animation Explorer (F7 second half)

**Goal.** Floater listing recently played animations on nearby avatars with
source avatar, playback preview, stop/revoke. Sound explorer already exists
in Alchemy (`floater_explore_sounds.xml`) — mirror its integration style.

**Donor.** `I:\enve` (phoenix-reshade-XL, LGPL): `indra/newview/animationexplorer.cpp/h`
(601 lines, self-contained) + `skins/default/xui/en/floater_animation_explorer.xml`.
Keep the Firestorm license header (FIRESTORM-SOURCE_LICENSE_HEADER.txt pattern —
see B3a's landed files for the precedent in this tree).

**Port map.** Copy files → rename nothing (fs-prefix-free already) → register
in `llviewerfloaterreg.cpp` → menu entry near the sound explorer's → check
donor includes for FS-only widgets (e.g. `FSScrollListCtrl` → plain
`LLScrollListCtrl` if hit) → CMakeLists (both lists, then build ZERO_CHECK).

**Accept (spec F7).** Explorer lists live animation sources; playback preview
and revoke work. One commit, `[BDMerge F7]`.

## B-2b.8 — Port FS Pose Stand + Undeform (F6 portable half)

**Goal.** Neutral pose stand floater + one-click undeform (clears stuck mesh
deformers). Movelock is EXCLUDED — see the board ruling (FS implements it via
their LSL bridge; do not attempt a client-only fake).

**Donor (`I:\enve`).** `fsfloaterposestand.cpp/h` (137 lines) +
`fspose.cpp/h` (shared helper, small) + `app_settings/posestand.xml` (pose
data) + the floater XML (find via `grep -rl posestand skins/default/xui/en`)
+ undeform = `FSToolsUndeform` in `llviewermenu.cpp:10911` — one menu handler
calling `FSPose::setPose(gSavedSettings.getString("FSUndeformUUID"))`; copy
the `FSUndeformUUID` settings key + value from FS settings.xml.

**Adaptation notes (verified in the Alchemy tree 2026-07-10):**
- `LLAgent::setCustomAnim` EXISTS in Alchemy (`llagent.h:529`) — no port needed.
- RLVa present in Alchemy — the `rlvhandler.h` include and `RLV_BHVR_SIT`
  check port as-is.
- The donor pauses FS's AO via `gSavedPerAccountSettings "UseAO"` — Alchemy's
  AO uses a DIFFERENT key: find it in `ao.cpp`/`aoengine.cpp` (grep for the
  enable setting) and map the pause/resume calls onto Alchemy's AO API.
- Settings keys `FSPoseStandLock`/`FSPoseStandLastSelectedPose`: add to
  settings.xml, keep donor names (F-item precedent: B3a kept donor naming).

**Accept (spec F6, minus movelock).** Pose stand floater poses the avatar and
restores state on close (AO resumes, custom-anim flag cleared); undeform
clears a deformed avatar. One commit, `[BDMerge F6]`.

## Non-G5 queue (unchanged priorities)

Render cluster (A1.3 → A2.x → A3.x — needs the user in-world for depth
captures), B1 machinima sidebar (should also absorb B-2b.3), B6/B8 camera
modes (diff vs `llcinematiccamera` first), B7 poser, B9 gamepad flycam.
