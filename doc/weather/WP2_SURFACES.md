# Persistent wet surfaces implementation notes

WP2 is opt-in through Director > World > Weather > Surfaces. The ten settings,
Apply starting surfaces, Clear surfaces now and coverage readout are live in the
generated page, preset/reset allowlist and native controller. Preset version 3
adds surfaces; loading an older look resets missing surface settings to defaults.

The order is authored material decoding, Actor FX/Avatar Light Response material
adjustments, linear weather response, native packing or reflection/lighting.
Weather never writes AO, metallic, emission carriers, flags or authored coverage.
Water, authored glowing draws, fullbright/HUD and avatar permutations are excluded; rigid worn attachments
are excluded by cached draw metadata, including shared Actor FX replay.

The surface-off permutation omits the helper and sampler. Enabling persistent
surfaces reloads shaders and reserves one sampler, including at the indexed PBR
batch limits. A changed indexed batch width retires cached draw maps and queues
their geometry for rebuilding before reuse. No reflection quality setting is changed. Wet sheen and its old
wet-glint contribution are suppressed only while the replacement has valid cover.
Other old surface effects keep their own switches.

Both rain modes feed the same production 120 Hz accumulation path. Existing rain
uses its effective brightness/cloud-coupled rain strength as the adapter input;
library mode uses physical rainfall independently of appearance. Master off
clears immediately. Surface commands preserve the precipitation target/epoch.

Basic cover is a dedicated pair of D32F targets. Allocation, clear and complete
terrain/caster traversal publish texture, transform, origin, place and validity
together. COMPLETE_EMPTY is valid open sky; the legacy rain predicate remains
unchanged. Failed/incomplete/outside coverage stays dry. There is no camera-ray
indoor material mask. Publication occurs once before the main/auxiliary/probe
draws, never from a probe face. Allocations failing at one detail setting are
held failed until that setting changes or the effect is toggled off.
Opaque and alpha-masked roofs use native shadow coverage. Blended glass is not
assumed to stop precipitation; the initial vertical map cannot infer that semantic.

Default cover storage is 2 MiB at 512 pixels per side; 256 uses 0.5 MiB, and the
advanced 1024 option uses 8 MiB and exceeds the default 4 MiB acceptance budget.
No full-frame material image or extra G-buffer attachment is allocated. On-demand
and cinematic On-change probe signatures quantize material/configuration state
and coalesce at 2 Hz, retaining the existing scheduler work/face caps. Pure rain
and ripple animation do not force probe refreshes.

The real-WGL gate covers native opaque/masked/indexed/terrain/forward/alpha
fragments, identity against 330d012e161, native BRDF integration, sheltered/open/
failed coverage, sloped roofs, vertical walls, alpha-cut canopies and doorways.
Reprojected camera motion includes 1 mm dollies and 0.02-degree yaw steps;
Director rows drive the compiled native reader, fixed-step model and GL upload.
Legacy forward material alpha preserves its original glare-dependent coverage by
evaluating the authored coverage before the wet beauty result. Its additional
lighting cost needs inclusion in the final native mixed-scene measurement.

Standalone cost is paired at 3840x2160 with a native shadow-fragment D32F cover
fixture. PENDING-QUIET means scene-density traversal, final native integration,
state restoration in the complete viewer and user visual acceptance are still
provisional. No viewer build or automated in-world test is part of this phase.
