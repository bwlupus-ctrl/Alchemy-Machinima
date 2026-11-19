# Claude Handoff: Preset Protection, Lens Flare Scrolling, and Xbox Flycam Selection

Date: 2026-08-24
Repository: `I:\alchemy-machinima`
Build configuration used: `RelWithDebInfo`, target `alchemy-bin`

## Scope

This document covers three narrowly scoped changes requested for the Alchemy
Machinima viewer:

1. Built-in Cinematic Light Rig setup presets must not be overwritten.
2. The Lens Flare preset drop-down must be a bounded, mouse-wheel-scrollable list.
3. An Xbox controller may be polled and automatically selected as the active
   Flycam controller, without changing mappings and without entering Flycam.

The worktree contains many unrelated user changes. Preserve them. Do not revert,
format, or rewrite files outside the task-specific hunks described below.

## Final behavior requirements

### Cinematic Light Rig setup presets

- Master/built-in setup names are read-only.
- Setup category/decorative rows are also non-writable.
- Selecting or typing a built-in name disables the Save button.
- User-created setup names remain saveable and overwriteable by the user.
- The save handler and model-level save path reject built-in/decorative names as
  defense in depth, even if the UI guard is bypassed.
- The Save button tooltip explains that built-in setups are read-only and that a
  new name must be entered.

Relevant implementation:

- `indra/newview/alpanelcinelightrig.cpp`
  - Caches `cine_setup_save` as `mSetupSave`.
  - `updateDerivedStatus()` trims the current combo text and enables Save only
    when the name is non-empty, not a master setup, and not a decoration.
  - `saveSetup()` rejects invalid/built-in names and reports a `GenericAlert`.
- `indra/newview/alpanelcinelightrig.h`
  - Adds the `mSetupSave` button member.
- `indra/newview/alcinelightrig.cpp`
  - `ALCineLightRig::saveSetup()` rejects master/decorative names.
- `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml`
  - Adds the read-only explanation to the Save button tooltip.

### Lens Flare preset list

- The Lens Flare preset combo uses a bounded list rather than an unbounded panel.
- Eight rows are visible at a time.
- The open combo list consumes mouse-wheel events so the wheel scrolls the
  presets instead of the parent Cinematic Light Rig panel.

Relevant XUI:

```xml
<combo_box ... name="cine_flare_preset" ...>
    <combo_list mouse_wheel_opaque="true" page_lines="8" />
</combo_box>
```

File: `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml`

### Xbox controller polling and Flycam selection

This is the final controller behavior. It supersedes earlier ideas in the thread.

- Windows-only DirectInput polling runs every three seconds.
- Polling is controlled by persisted Boolean setting
  `JoystickAutoFlycamEnabled`, default `true`.
- The Joystick Configuration floater exposes the option as
  **Auto-detect Xbox for Flycam**.
- Cancelling the floater restores the previous polling setting, matching the
  existing Cancel behavior for other joystick controls.
- An attached DirectInput device is treated as Xbox when its product name
  contains `xbox`, case-insensitively.
- On a new connection edge, the controller GUID is selected, saved as the active
  joystick device, and joystick/Flycam support is enabled.
- Detection must **not enter Flycam**. The connection path must not call
  `toggleFlycam()` and must not change the current camera mode.
- Detection must **never apply any default mappings**. It must not change axis
  mappings, button mappings, scales, dead zones, inversion, feathering, or any
  other saved controller customization.
- Poll-triggered initialization calls `init(false, false)`. The second argument
  suppresses legacy automatic defaults, including SpaceNavigator defaults in a
  multi-controller environment.
- Existing Xbox/SpaceNavigator Defaults buttons remain manual user actions only.
- Connection is edge-triggered. Once selected, the poller does not repeatedly
  override manual device or camera choices while the controller remains attached.
- A transient `notifytip` toast announces connection/readiness and disconnection.
  These notifications fade automatically.
- On disconnect, if the Xbox device is still active, input state is cleared and
  the driver is marked uninitialized. If the user manually entered Flycam before
  unplugging, disconnect safely exits Flycam.
- Disabling polling stops enumeration immediately and clears the presence latch.
  Re-enabling allows the currently attached Xbox controller to be selected on the
  next poll.

## Xbox implementation map

### `indra/newview/llviewerjoystick.cpp`

- Includes `llframetimer.h` and `llnotificationsutil.h`.
- Defines `XBOX_DEVICE_POLL_SECONDS = 3.f`.
- Adds Windows helpers:
  - `isXboxProductName()`
  - `sameWindowsDeviceId()`
  - `XboxDeviceProbe`
  - `di8_xbox_probe_callback()`
- Adds `LLViewerJoystick::pollForXboxController()`.
- Calls the poller at the start of `scanJoystick()`, before the legacy early exit
  for disabled/uninitialized joystick input.
- Extends `init()` with `apply_defaults`, defaulting to `true` for existing callers.
- The polling path uses `init(false, false)` and then explicitly selects the Xbox
  GUID when necessary.
- On successful selection it sets only:
  - `JoystickFlycamEnabled = true`
  - `JoystickEnabled = true`
  - `JoystickInitialized = "XboxController"`
  - the selected `JoystickDeviceUUID`
- It then refreshes cached joystick settings, sets the presence latch, logs the
  handoff, and emits the ready toast. It does not call `toggleFlycam()`.

### `indra/newview/llviewerjoystick.h`

- Changes `init(bool autoenable)` to
  `init(bool autoenable, bool apply_defaults = true)`.
- Declares `pollForXboxController()`.
- Adds `mXboxWasPresent` for edge detection.

### `indra/newview/app_settings/settings.xml`

Adds persisted default-on Boolean `JoystickAutoFlycamEnabled`.

### `indra/newview/skins/default/xui/en/floater_joystick.xml`

Adds the **Auto-detect Xbox for Flycam** checkbox beside the device selector. Its
tooltip states the three-second interval and explicitly promises not to change
saved mappings.

### `indra/newview/llfloaterjoystick.cpp` and `.h`

Snapshot and restore `JoystickAutoFlycamEnabled` for Cancel handling.

### `indra/newview/skins/default/xui/en/notifications.xml`

Adds transient notification templates:

- `XboxControllerConnected`
- `XboxControllerDisconnected`

The connected message says the controller is ready as the active Flycam
controller; it must not claim that Flycam was activated.

## Validation already completed

- `git diff --check` passed. The only console noise was a sandbox permission
  warning for the user's global Git ignore file.
- XML parsing passed for:
  - `indra/newview/app_settings/settings.xml`
  - `indra/newview/skins/default/xui/en/floater_joystick.xml`
  - `indra/newview/skins/default/xui/en/notifications.xml`
- Full build command passed:

```powershell
cmake --build build-Windows-vs2026-os --config RelWithDebInfo --target alchemy-bin -- /m:4
```

- Built executable:
  `I:\alchemy-machinima\build-Windows-vs2026-os\newview\RelWithDebInfo\AlchemyTest.exe`

## Remaining smoke test

A physical Xbox controller was not available during implementation. Recommended
manual test:

1. Start the rebuilt viewer with no Xbox controller attached.
2. Open Joystick Configuration and confirm auto-detection is enabled.
3. Plug in the controller and wait up to three seconds.
4. Confirm the ready toast appears and the Xbox device becomes the active joystick.
5. Confirm the camera does not enter Flycam.
6. Confirm all saved axes/buttons/scales/dead zones are unchanged.
7. Enter Flycam manually and verify the controller works.
8. Exit Flycam manually and confirm polling does not force re-entry.
9. Unplug the controller and confirm the disconnect toast and safe input release.
10. Disable auto-detection, repeat plug/unplug, and confirm no polling handoff occurs.
