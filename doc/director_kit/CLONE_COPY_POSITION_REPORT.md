# Clone Copy position

Implemented on `feature/director-kit-p0`, starting at `cecf63e32e2`.

## Exact click paths

1. Right-click another avatar in world > **Copy position** (top-level context-menu item).
2. Right-click yourself in world > **Copy position** (top-level context-menu item; explicitly targets your avatar).
3. Director Console > Cast list > right-click an avatar row > **Copy position**. Registered in both the legacy and v2 console hosts.
4. Machinima > **Clone Studio...** > select a clone > **Place** tab > **Placement** card > **Paste position**.
5. Legacy Director Console > **Clones** tab > select a clone > **Place** tab > **Placement** card > **Paste position**. This and the standalone window share `panel_ghost_studio.xml` and `ALPanelGhostStudio`.
6. Director v2 > **Cast > Clones** > select a clone > **Position** card > **Paste position**.

Before the first successful copy, Paste position is disabled with tooltip `Right-click an avatar > Copy position first`. Copy works without adding the avatar to the cast and snapshots its placement at click time. An unresolved avatar leaves the existing clipboard untouched.

## Behavior

The shared `ALGhostStudio` clipboard contains only a global position, full facing quaternion and validity flag. It has no setting or serialization. Position uses the existing clone-placement helper: base root/pelvis position minus pelvis-to-foot height, converted to global coordinates. Facing comes from the root joint's world rotation. No rounding, ground snapping or UI spinner round-trip is involved.

Paste preserves the selected unit's scale and calls the existing panel `applyUnitTransform` wrapper, which dispatches to `transformUnit` / `transformGroup`. This preserves normal group/member editing and transform commit behavior, including `applyEntityTransform` and its Reflect/True Mirror stamp-owned facing rules. It acts on the selected unit, like To actor/To me. Clone persistence is unchanged; clones and this clipboard remain session-only. No pipeline or shader changes and no new math helper/test.

Kit source gains exactly one row, in Position; row pin **2140 -> 2141 (+1)** has an explanatory comment. JSON is UTF-8 with LF and was structurally compared against HEAD: removing the new row reproduces the original data exactly. All three generators were regenerated, including their required generated evidence and fingerprint updates.

## Gates

- `zs_check.ps1`, one touched `.cpp` per invocation: **PASS 4/4**, zero failures: `alghoststudio.cpp`, `alpanelghoststudio.cpp`, `alviewermenu.cpp`, `llfloaterdirector.cpp`. Logs: `.tmp/clone_copy_<translation-unit>_zs.log`.
- `director_kit_gen.py --check`: **PASS**.
- `director_page_sources.py --check`: **PASS**; log `.tmp/clone_copy_sources_check.log`.
- `director_functional_manifest.py --check`: **PASS**; log `.tmp/clone_copy_manifest_check.log`.
- `prebuild_guard.ps1 -ChecksOnly`: **PASS**, exit 0; all 11 gates OK; exited before object purge. Log `.tmp/clone_copy_prebuild_guard.log`.
- `git diff --check`: **PASS**, including the final report.

The guard uses `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`. The existing workspace Git config supplies safe-directory/excludes paths; the compiler's supported cache argument and TEMP/TMP point inside the workspace. The installed guard is unchanged. An initial guard run overlapped the final v2 registration/source regeneration and reported stale page-source evidence; the final run follows completed regeneration.

No build, viewer launch, commit, push or stash was performed. In-world clicks and visual behavior have not been runtime-tested. Existing unrelated workspace files and the dirty `vcpkg` submodule were left alone.

P0 fix: legacy and v2 Cast-row Copy position use the right-clicked row UUID, falling back to first selected only when no row was hit; targeted zs_check and git diff --check PASS; prebuild_guard -ChecksOnly (requested safecrlf environment) exits 2 on stale generated kit headers (kit gen/lint); log: `.tmp/copypos_click_guard.log`.
