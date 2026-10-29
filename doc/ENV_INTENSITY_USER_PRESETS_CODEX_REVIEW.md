**NO P0. No P1 found. One P2 must-fix.**

- **P2 — Valid Unicode preset names are silently truncated.** [floater_env_intensity_presets.xml:58](/I:/alchemy-machinima/indra/newview/skins/default/xui/en/floater_env_intensity_presets.xml:58) sets `max_length_bytes="128"`, while the manager accepts 64 Unicode characters. A 43-character CJK name requires 129 bytes. Typing/pasting truncates it; selecting a valid longer name loaded from disk also truncates it through [alfloaterenvintensitypresets.cpp:134](/I:/alchemy-machinima/indra/newview/alfloaterenvintensitypresets.cpp:134). Save then targets the shortened name instead of overwriting the selected preset. Allow at least 256 bytes and retain the manager’s 64-character validation.

The approach otherwise holds up under static review:

- Combo rebuilding does **not** apply presets: `selectFirstItem()` delegates to `setCurrentByIndex()`, which selects without committing. Floater `selectByValue()` does commit, but its callback only updates text/buttons.
- The refactor preserves all 16 writes, their order, comparison epsilon, and built-in precedence. The three excluded settings remain untouched.
- Windows replacement works: `LLFile::rename()` uses `std::filesystem::rename()`; the installed MSVC implementation calls `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`. Stream flushing does **not** establish power-loss durability.
- Ranges match the sliders. Whole-preset rejection is consistent with the stated validation policy. Reserved names currently match all built-ins; duplication is a maintenance risk, not an existing collision.
- Scoped connections, confirmation captures, CMake entries, registration, callback signatures, and strip geometry show no concrete defect. Localized XUI layers inherit the new English controls.

Read-only review only; no edits, builds, commits, or runtime tests.

Codex session ID: 01a0f02f-b081-7153-8336-9221925b5fcb
Resume in Codex: codex resume 01a0f02f-b081-7153-8336-9221925b5fcb
