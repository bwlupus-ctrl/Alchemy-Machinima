# GEMINI.md — working on Alchemy-Machinima

You are contributing to **Alchemy-Machinima**, a Windows C++ fork of the **Alchemy**
Second Life viewer (LL/Firestorm lineage) specialized for **machinima** (in-world
cinematography). It is a large LL-lineage C++ codebase; the machinima work is a thin,
heavily-commented `AL*` layer on top of the `LL*` viewer. Repo root: `I:\alchemy-machinima`
(branch **`develop`**).

## Read these first (do not skip)
1. **`doc/AI_AGENT_CODEBASE_ORIENTATION.md`** — the authoritative tour: full directory map,
   the machinima subsystems and their key files, coding idioms, build/verify, and how to
   research a feature. **This is your primary reference. Read it before writing any code.**
2. **`doc/MACHINIMA_USER_GUIDE.md`** — what the viewer's features do from the user's side
   (Director Console, cameras, Prism lens/camera-feed, Ghost Studio, weather, temporal, …).
3. **`doc/`** generally — per-feature deep-research briefs and handoffs (`*_DEEP_RESEARCH.md`,
   `*_HANDOFF.md`). **Grep `doc/` for your subsystem before starting** — most features have one.
4. **`doc/ARCHITECTURE.md`, `doc/BUILD.md`** — the base viewer.
5. **`CLAUDE.md`** (repo root) — the same house rules another agent follows here; the
   conventions and build facts apply to you too.

## The repo at a glance
```
indra/                         LL-lineage viewer source
  llcommon/ llmath/ llrender/  core types (LLSD, LLUUID, LLVector3/3d, LLQuaternion, F32/S32) + GL
  llcharacter/ llui/           animation/LLMotionController; LLPanel/LLFloater widget framework
  newview/                     ★ THE VIEWER APP — all machinima features live here
    skins/default/xui/en/      ★ UI as XUI XML (floaters/panels/menus)
    app_settings/settings.xml  ★ runtime settings registry (one big LLSD map)
    app_settings/shaders/      ★ GLSL (class1/class2/class3); COMPILED AT RUNTIME, not build
    CMakeLists.txt             ★ add every new .cpp/.h here or it will not build/link
doc/                           ★ architecture + deep-research + handoffs
build-Windows-vs2026-os/       generated CMake tree; exe at newview/Release/AlchemyTest.exe
```
Naming: **`LL*`** = ported/LL-lineage; **`AL*`** = Alchemy/machinima additions. One class per
`.h`+`.cpp`, lowercase filenames.

## Build, deploy, run
- Build (from repo root, PowerShell, **viewer must be closed** — it locks the exe):
  ```
  cmake --build "I:\alchemy-machinima\build-Windows-vs2026-os" --config Release --target alchemy-bin
  ```
- **Judge success by the exe timestamp + absence of `error C####`/`error LNK####`, NOT the exit
  code.** A trailing `MSB3073` (vcpkg `z-applocal` / MochaPro-manifest) step fails *after* a
  successful link — that's benign. `LNK1104: cannot open AlchemyTest.exe` = the viewer is running.
- **Warnings are errors (`/WX`).** No unused vars/params, cover all `switch` enum cases, no
  signed/unsigned or narrowing (`LLSD::Real`→`F32`) mismatches — cast explicitly.
- **Runtime asset deploy (incremental builds do NOT copy these):** after editing XUI, `settings.xml`,
  or a shader, copy the changed file into the Release tree
  (`build-Windows-vs2026-os\newview\Release\{skins\default\xui\en, app_settings, app_settings\shaders\...}`).
  **New `.glsl` files are especially not auto-copied** — a missing shader fails only in-world
  (shaders compile at runtime), and can even fail the whole deferred set if not decoupled. Restart
  the viewer after changing `settings.xml` (read at startup).

## Conventions that make it compile & fit
- **Strings (the #1 compile trap):** LL setters (`setText/setLabel/setToolTip`) take
  `const LLStringExplicit&`; a bare `const char*` literal does **not** convert (`C2664`). Use
  `LLStringExplicit("…")` or a `std::string`. (`llformat(...)`/`std::string` args are fine.)
- **UI:** every `getChild<T>("name")` in a panel's `postBuild()`/handlers MUST match a `name=` in
  its XUI `.xml`, or it returns a dummy / asserts. Settings-backed controls use `control_name=`.
- **Settings:** add tunables to `app_settings/settings.xml` (LLSD Comment/Type/Value/Persist); read
  hot paths via `static LLCachedControl<T> foo(gSavedSettings,"Key",default);`. Default new
  features **off**.
- **Coordinate spaces:** `LLVector3d` = global, `LLVector3` = agent/region — convert with
  `gAgent.getPosAgentFromGlobal/getPosGlobalFromAgent`; never mix (silent placement bugs).
- **Quaternions:** `operator~` is conjugate (= inverse only when unit); `operator*` doesn't
  renormalize. Normalize stored/composed rotations or magnitudes explode (a real crash here).
- **Add new `.cpp/.h` to `indra/newview/CMakeLists.txt`.** Match the surrounding comment density
  (explain *why*). Singletons: `LLSingleton<T>` via `T::instance()`.

## Review before you build (this project's discipline)
Every change is **adversarially reviewed** before it's built and shipped — aim for **zero
must-fix**. Before handing off / building: `git diff --check`; confirm every changed XUI parses and
every `getChild` resolves; and for any change to the **main render / deferred-lighting path**, prove
that with the feature **off** the frame is **byte-identical** to before, and that any auxiliary
render pass **fully restores** global GL/camera/matrix state on every exit path. Do not weaken these
to make a demo look complete.

## Git & delivery
- **Remotes:** `origin` = `https://github.com/bwlupus-ctrl/Alchemy-Machinima.git` (**our fork — the
  only push target**); `alchemy-upstream` = `AlchemyViewer/Alchemy.git` (**read-only; never push**).
  Work on **`develop`**.
- **Stage explicitly** the files your change touches (+ new files + their CMake entry). **Never
  `git add -A`** — the tree carries unrelated user files, crash dumps, `.tmp.driveupload/`, and
  scratch docs that must not be committed.
- **NEVER commit or push without the user's explicit go-ahead.** Pushing publishes to GitHub.
- Commit messages: imperative subject + a concise body of what/why; end with a trailing
  `Co-Authored-By:` line for the agent.
- **Two ways to deliver, depending on your access:**
  - **Direct file access (Gemini CLI in this repo):** edit files in place, add them to CMake, then
    ask the user (or Claude) to build — state the baseline commit SHA and the exact files changed.
  - **No repo access (patch handoff):** deliver against a named **baseline commit SHA** — a
    `HANDOFF.md` (baseline SHA, files changed, delivered behavior), a complete-source ZIP of only the
    changed/new files at repo-relative paths, and a `git diff` patch. (See orientation §6.)
- The final **build/link is done in this environment**, not by you — deliver compile-clean source.

## Current state (update this as it changes)
- `develop` HEAD ≈ `e77366a9057`, pushed to `origin`. Latest feature: **Prism** — surface-locked
  magnifier lenses + a render-to-texture **camera feed** (`llprismlens.*`, `llfloaterprismmanager.*`,
  `pipeline.cpp`, class1 `prismLens*.glsl`, class3 `reflectionProbeF.glsl`); design in
  `doc/PRISM_VIRTUAL_CAMERA_SURFACE_DEEP_RESEARCH.md` + `doc/PRISM_CAMERA_FEED_CLAUDE_HANDOFF.md`.
- Some working-tree files are intentionally uncommitted (workflow notes, crash dumps, dropped design
  docs). Run `git status` and confirm scope with the user before committing anything.

## When in doubt
Prefer the **smallest change that fits the existing shape** over a new parallel system. Match the
subsystem you're touching (read one working example end-to-end first). If a build fact here conflicts
with what you observe, trust the compiler/tree and tell the user.
