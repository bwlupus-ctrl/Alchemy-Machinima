# Alchemy-Machinima — Codebase Orientation for an AI Coding Agent

**Purpose:** get an AI agent (ChatGPT/Codex/etc.) productive in this repo fast — enough to
*research* a feature and *write code that compiles and integrates*. Read this first, then dive into
the subsystem your feature touches (§3) and the deep-research docs in `doc/` (§7).

> This repo is a fork of the **Alchemy** viewer (itself a fork of the Second Life / Firestorm-lineage
> open-source C++ viewer), specialized for **machinima** (in-world cinematography). It is a large
> LL-lineage C++ codebase; the machinima work is a thin, heavily-commented layer on top.

---

## 1. Ground rules (read before writing any code)

- **You work in an ISOLATED mirror, not the user's live tree.** The user's tree is at
  `I:\alchemy-machinima` and may carry *uncommitted* work. Never assume you can edit it in place.
  Work on a clean checkout/mirror, and **deliver a patch + a complete-source ZIP + a HANDOFF.md**
  (§6). Always state the **baseline commit SHA** you built against.
- **The final link is the user's job.** You can compile (`/t:ClCompile`) but the viewer `.exe` is
  often locked by a running instance — a link `LNK1104` is a *lock*, not a code error. Deliver
  compile-clean source; the user links it.
- **Match the house style.** This code is *densely commented explaining WHY*, uses LL idioms, and
  builds with **warnings-as-errors**. Terse or warning-y code will not build.
- **Adversarially self-review before handoff.** Aim for zero must-fix. Static-verify XML parses and
  that every `getChild("name")` resolves to a control in the XUI.

---

## 2. Repo layout

```
indra/                     LL-lineage viewer source
  llcommon/ llmath/        core types (LLSD, LLUUID, LLVector*, LLQuaternion, LLColor4, F32/S32...)
  llrender/ llwindow/      GL render + platform
  llcharacter/             animation + LLMotionController (avatar motion, time factors)
  llui/                    LLPanel/LLFloater/LLView widget framework
  llmessage/ llinventory/  networking, inventory
  newview/                 ★ THE VIEWER APP — all machinima features live here
    skins/default/xui/en/  ★ UI as XUI XML (panels/floaters/menus)
    app_settings/settings.xml  ★ the runtime settings registry (one giant LLSD map)
    tests/                 unit tests (compiled into test targets)
    CMakeLists.txt         ★ add every new .cpp/.h here or it won't build
doc/                       ★ architecture + deep-research briefs (see §7)
build-Windows-vs2026-os/   the CMake build tree (generated); exe at newview/Release/AlchemyTest.exe
```

Naming: **`LL*`** = ported/LL-lineage classes; **`AL*`** = Alchemy / machinima additions. One class
per file (`.h` + `.cpp`), lowercase filenames.

---

## 3. The machinima subsystems (where features live) + key files

| Subsystem | What it does | Key files |
|---|---|---|
| **Ghost Studio** | Client-only avatar "clones"/ghosts: place, pose, style, and multiply into crowds. Two kinds: **Overlay** (re-draws the source's rigged batches) and **Entity** (a real `LLGhostAvatar`). | `alghoststudio.{h,cpp}` (singleton + Instance model + transforms), `llghostavatar.{h,cpp}` (entity clone), `llactormover.{h,cpp}` (overlay render + path previews), `alpanelghoststudio.{h,cpp}` + `panel_ghost_studio.xml` (UI), `alformationsolver.*` (crowd geometry), `alghostgroupmodel.*` (groups), `altoolcrowdplace.*` (placement tool), `alworldoverlayviz.*` (in-world overlay drawing), `alghostmanipproxy.*` (gizmo) |
| **Director Console** | The superset machinima control panel (cast, ghosts, cameras, paths, takes, temporal). | `llfloaterdirector.{h,cpp}`, `lldirectorcast.{h,cpp}`, `floater_director.xml` |
| **Cinematic cameras** | Camera operator, cinematic modes, flycam recorder. | `llcinematiccamera.*`, `panel_cinecam_params.xml`, flycam recorder panels |
| **Actor movers / paths** | Actors walk 3D **Catmull-Rom** paths; in-world path editor. | `llactormover.*` (path eval + traversal), `alobjectpathmover.*`, `ALToolPathEdit`/`ALPanelPathEditor` |
| **Temporal (time scale)** | Client presentation-time slow/fast motion. | `llpresentationtime.*`, `lltemporalframecontext.h`, `alfloatertemporalcapture.*`, `alpaneltemporalcapture.*` |
| **ReShade bridge** | Data provider for the ReShade post chain. | `llreshadebridge.*` |

**Render/idle hooks you will likely need:** per-frame UI-3D overlays are drawn in
`llviewerdisplay.cpp` (`render_ui_3d`); the main tick is `LLAppViewer::idle()`; avatar motion advances
in `LLVOAvatar::idleUpdate` → `LLMotionController::updateMotions`.

---

## 4. Conventions & idioms (so your code compiles and fits)

- **Singletons:** `LLSingleton<T>` (access via `T::instance()`).
- **Settings:** everything tunable is a key in `app_settings/settings.xml` (an LLSD map with
  Comment/Type/Value/Persist). Read hot-path settings with
  `static LLCachedControl<bool> foo(gSavedSettings, "MySetting", default);`. Add new settings to
  `settings.xml`.
- **UI:** a panel is an `LLPanel` subclass; controls come from a XUI `.xml` file. In `postBuild()`
  bind with `getChild<LLComboBox>("name")`; **every `getChild` must match a `name=` in the XML** or
  it asserts. Settings-backed controls use `control_name=` in the XML.
- **Strings (common compile trap):** LL setters (`setLabel`, `setText`, `setToolTip`, `setValue`)
  take `const LLStringExplicit&`. A **bare `const char*`/literal does NOT convert** (error `C2664`).
  Pass a `std::string`: `ctrl->setLabel(std::string("Foo"));` or `std::string(some_c_str)`.
- **Coordinate spaces:** `LLVector3d` = **global** coords; `LLVector3` = **agent/region** coords.
  Convert with `gAgent.getPosAgentFromGlobal(...)` / `getPosGlobalFromAgent(...)`. **Do not mix
  them** — silent placement bugs live here.
- **Quaternions:** `LLQuaternion::operator~` is the **conjugate**, which equals the inverse *only for
  a unit quaternion*, and `operator*` does **not** renormalize. **Normalize rotations you store or
  compose** or magnitudes explode (a real crash source here).
- **Types:** `F32/F64/S32/U32/BOOL`, `LLUUID`, `LLColor4`. Immediate-mode overlay drawing uses
  `gGL.begin(LLRender::LINES/TRIANGLES)` + `gGL.color4fv` + `gGL.vertex3f` under `LLGLSUIDefault` +
  `gUIProgram` with depth-write off.
- **Warnings-as-errors:** no unused variables/params, cover all `switch` enum cases (or `default`),
  no signed/unsigned mismatches. Assume `/WX`.
- **Comment density:** explain *why*, not *what*, matching the surrounding machinima code.

---

## 5. Build & verify

- Toolchain: **CMake + vcpkg**, Visual Studio 2026 generator. Viewer target: **`alchemy-bin`** →
  `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe` (channel "Alchemy Test").
- Full build: `cmake --build build-Windows-vs2026-os --config Release --target alchemy-bin`
- **Compile-only** (no link — use when the exe is locked, or to validate source):
  `MSBuild build-Windows-vs2026-os/newview/alchemy-bin.vcxproj /t:ClCompile /p:Configuration=Release`
- **Build gotchas (all real, all recurring):**
  1. The post-build `viewer_manifest.py` step usually **fails** (it runs under a wrong Python) — the
     `.exe` still **links fine**. Judge success by the exe timestamp + absence of `error C####`/
     `error LNK####`, not the overall exit code.
  2. Incremental builds do **NOT** copy skins/settings into the Release output. After changing XUI or
     `settings.xml`, copy them into `build-.../newview/Release/skins/default/xui/en/` and
     `.../Release/app_settings/` manually (and restart the viewer — `settings.xml` is read at startup).
  3. `LNK1104: cannot open AlchemyTest.exe` = the **viewer is running** (file lock). Close it to link.

---

## 6. Handoff format (deliver work like this)

Produce, against a clearly-named **baseline commit SHA**:
1. **HANDOFF.md** — baseline SHA, final commit SHA + message, the list of changed/new files, and a
   plain-English "delivered behavior" list.
2. **Complete-source ZIP** — the changed/new files only, preserving repo-relative paths (drop-in over
   a baseline checkout). This is the most robust apply path.
3. **Patch(es)** — `git diff`/`format-patch` from the baseline (and an incremental patch from any
   prior delivery commit).
4. **New files must be added to `indra/newview/CMakeLists.txt`** (and unit tests to the test target).
5. State the **compile result** (`/t:ClCompile` exit code); note the user does the final link.

---

## 7. Existing research to build on (in `doc/`)

- `ARCHITECTURE.md`, `BUILD.md`, `CHANGELOG.md` — start here for the base viewer.
- `GHOST_STUDIO_TRANSFORM_ARCHITECTURE_DEEP_RESEARCH.md` — the Ghost Studio clone/transform/manip/
  render architecture, fragility analysis, and a unified in-world "representation layer" design.
- `AVATAR_UNIFORM_SCALE_DEEP_RESEARCH.md`, `DIRECTOR_ANIMATION_CONTROL_DEEP_RESEARCH.md`,
  `FLYCAM_RECORDER_DEEP_RESEARCH_BRIEF.md`, `TEMPORAL_CAPTURE_WORLD_TIME_SCALE_BRIEF.md`,
  `CLONE_FIDELITY_AUDIT_*` — subsystem deep-dives.
- `BD_MERGE_PATCHLOG.md` + the `BDMERGE_*` briefs — the render/feature merge campaign log.
- `doc/` has many more per-feature briefs; grep it for your subsystem before starting.

---

## 8. How to research a new feature (workflow)

1. **Locate the subsystem** it belongs to (§3): is it UI, render, a machinima singleton, animation?
2. **Grep the entry points:** the panel `.xml` + panel `.cpp` (UI), the backing `AL*`/`LL*` singleton,
   the relevant `settings.xml` keys, and the render/idle hook it needs.
3. **Read one working example end-to-end** in that subsystem — the code is consistent, so mirror it.
4. **Trace the data:** where state is stored (an Instance/model struct), where it's mutated (commit
   handlers), where it's consumed (draw/idle). Respect the coordinate-space and quaternion rules (§4).
5. **Write to the conventions**, add settings + wire new files into CMake, `/t:ClCompile`, self-review.
6. **Package** per §6.

*When in doubt, prefer the smallest change that fits the existing shape over a new parallel system.*
