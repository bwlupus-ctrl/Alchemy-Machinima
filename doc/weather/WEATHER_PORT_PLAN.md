# Slipstream weather port plan

## 1. User workflow first — proposed for confirmation before code

This describes the finished experience. Each release adds only the controls for features that work in that release. Everything is on **Director v2 > World > Weather**, including the advanced controls. Director search finds a control by its label, its units, or familiar words such as drizzle, puddles, footprints, roof, thunder, parcel, and reshoot. Search opens the right section and reveals the matching row.

### Starting and keeping the current rain

Open Director, select **World**, then **Weather**. The **Weather** switch remains off by default. With it on, **Weather controls: Existing rain** remains the default. Your present rain, splashes, wind response, cloud link, wet sheen, mist, lens drops, shelter, lightning, and saved looks behave as they do today. Installing the library changes none of those values or looks.

To use the new weather, choose **Weather controls: Weather library**, turn on **Local weather**, choose a rain or snow preset, and click **Apply weather preset**. The status row says, for example, **Local rain — 5 mm/h — transitioning, 12 s remaining**. The page keeps the existing rain tuning controls nearby. **Weather controls: Existing rain** returns to your previous rain settings. Applying a new weather preset changes weather and wind targets; it does not silently enable puddles, snow depth, runoff, avatar coating, audio, or higher quality settings.

The page shows **Requires Advanced Lighting Model** when that is unavailable. Weather visual controls explain why they are unavailable. The sound controls remain usable when the audio engine is available. Turning the master Weather switch off immediately clears weather effects and stops weather sound; **Stop precipitation** instead lets wet surfaces dry, snow melt, and the last drips finish. **Clear surfaces now** removes accumulated water and snow without changing your chosen storm. **Reset All** restores the built-in weather defaults, including the master switch being off.

### Rain, snow, rates, and saved looks

In **Precipitation**, choose **Rain**, **Snow**, or **Mixed rain and snow**. Rain presets are **Drizzle, Steady rain, Heavy rain, Downpour**, with starting rates of **1, 5, 20, and 80 mm/h**. Snow presets are **Flurries, Steady snow, Heavy snow, Blizzard**, at **0.3, 1.5, 5, and 12 cm/h** of fresh snowfall. Blizzard also sets wind to at least **15 m/s**; the preset description says this before Apply. Mixed weather exposes both rate boxes.

Type a rate directly into **Rain rate (mm/h)** or **Snowfall rate (cm/h)** and press Enter. The preset label becomes **Custom**. The boxes accept zero and show their limits. **Rain bands** controls how much the rate rises and falls during the storm; zero holds a steady rate. The status row distinguishes the chosen rate from the rate falling now.

Rain keeps the viewer's current streak style. **Intensity**, **Density**, **Fall speed**, **Shutter**, **Depth layers**, **Near emphasis**, and **Colour** remain artistic adjustments. In library mode, the physical rate determines the storm strength and those controls tune its appearance. Their help explains that changing brightness does not change the measured rain rate. In Existing rain mode they retain their exact present meaning. Snow has its own **Flake size**, **Fall speed**, **Draw distance**, and **Flake limit**. Falling snow catches light and disappears behind scene geometry; it does not paint snow onto the ground unless accumulation is also enabled.

Use the page's existing **Save preset** action, type a name such as **Dock rain at dusk**, and save. Loading it restores the weather choices and all weather appearance controls. Existing saved rain looks continue to load as Existing rain. A repeatable take has its own saved starting state, described below, because a look alone does not describe how far a storm has progressed.

### Wind and transitions

Open **Wind and transitions**. Choose **Simulator wind** to retain the present wind response, or **Weather wind** to type a direction and speed. Direction means where wind comes **from**: north is 0 degrees, east is 90 degrees. **Speed (m/s)** sets the mean; **Speed variation**, **Gustiness**, and **Direction swing (degrees)** create a changing wind around it. A small direction arrow and speed readout show the wind currently used. **Wind response** still scales how far rain slants. The old procedural **Gust strength** belongs to Existing rain; library mode uses the new wind rows so gusts are not added twice.

Set **Transition (seconds)** before applying a new preset or rate. Zero changes immediately; otherwise rain, snow, wind, and storm intensity change smoothly. The progress readout finishes when the change has settled. Existing rain's **Cloud link** remains available in Existing rain mode. Library weather uses the chosen source and the explicit **Weather from sky** option instead of secretly multiplying the typed rate by cloud cover. Existing ground mist and lens drops remain separate artistic choices.

### Wet surfaces and puddles

Open **Surfaces**, turn on **Persistent wet surfaces**, then optionally **Puddles**. These work with either rain control mode. Outdoor materials darken and become smoother as water builds; suitable level surfaces collect reflective patches and drop ripples. Indoor surfaces remain dry where the shelter map identifies cover. Puddles follow the viewer's reflection settings; enabling them does not automatically turn on an expensive reflection mode or create a new water plane.

Set **Soak time (seconds)** and **Dry time (seconds)** to control how quickly the scene wets and dries. **Wet amount**, **Puddle amount**, and **Ripple strength** tune the appearance. Stop precipitation and see the surface remain wet, then dry. For a wet street without waiting, set **Starting wetness** and click **Apply starting surfaces**. **Wet sheen** remains the current quick look when persistent surfaces are off; persistent surfaces replace that sheen while active, so the two do not pile brightness onto the same surface. Switching the new option off restores the current sheen behavior and values.

### Snow accumulation, coating, and tracks

In **Snow on surfaces**, turn on **Accumulate snow**. Snow gradually covers exposed upward surfaces. **Maximum depth (m)** limits the blanket, **Cover build time (seconds)** controls how quickly the visual cover appears, and **Melt time (seconds)** controls how it disappears. Fresh snowfall rate determines depth growth; cover build time is an appearance control, not another snowfall rate. Rain speeds melting and the melted snow leaves water if persistent wet surfaces are enabled.

Choose **Starting snow depth (m)** and click **Apply starting surfaces** for an established winter scene. Turn on **Snow relief** for visible depth close to the camera. Turn on **Footprints** and walk an avatar through it; **Object tracks** lets moving scene objects leave bounded contact tracks. **Track strength** adjusts their depth. New snowfall fills them back in. **Clear tracks** restores the chosen blanket without changing the storm. **Snow detail** sets the working area and detail before recording; the page shows the extent and memory limit. Ground tracks belong to this viewer and are not shared edits to the region.

Ordinary region crossings preserve nearby snow. A teleport starts a fresh ground area, with no old footprints carried across. With local weather it uses your chosen starting conditions; shared weather uses the destination's published depth when supplied. Avatar coating has its own switch below and stays off until its separate release.

### Lightning and thunder

The existing **Lightning** and **Cinematic lightning** sections retain rate, brightness, bolt shape, range, cloud sheet, glow, glint, and peak controls. **Lightning timing: Existing** remains the default. Choose **Weather storm** to let **Storm amount** and the weather seed schedule strikes with the weather. Only one timing choice operates at a time. **Strike now** still flashes immediately. In repeatable mode it can be saved as a cue and fires at the same point on a reshoot.

Turn on **Weather audio**, then **Thunder** to hear the matching strike after its distance delay. **Thunder delay scale** adjusts that delay. A strike makes one thunder event, including when several camera views are open. Thunder is available with the existing lightning timing too; changing to the library is not required just to hear a storm.

### Weather audio

Open **Audio**, turn on **Weather audio**, and set **Master weather volume**. Separate **Rain**, **Roof and drips**, **Wind**, **Snow**, and **Thunder** levels make the mix usable for live capture. Move under a roof and outdoor rain fades toward a covered rain bed; move outside and it fades back. Wind follows gusts. The listener follows the viewer's current audio listener choice, rather than every auxiliary camera.

For staging, **Cover sound: Automatic / Open / Covered** and **Roof sound: Generic / Wood / Metal / Glass / Tent** allow a deliberate choice when the scene cannot supply a reliable cover or roof answer. Automatic uses the cover information available; it does not claim to recognize a roof's material. A loading readout becomes **Audio ready** before a repeatable take can start. Existing ambient volume and mute still apply. The page explains that **Weather wind sound** replaces the viewer's procedural wind sound while this weather mix owns it, then restores the earlier wind choice when disabled.

### Parcel and shared weather

In **Weather source**, local weather takes priority while **Local weather** is on. Turn it off and enable **Use parcel weather** to follow a weather tag in the description of the parcel your avatar occupies. Status says **Parcel weather**, **Waiting for parcel**, **No published weather**, or **Tag has errors**, with a short explanation. Merely pointing a camera at another parcel does not change the source.

If no valid parcel weather is published, **Weather from sky** may produce rain from a cloudy shared sky. Both source choices are off by default. They never change a region's sky. Choosing local weather overrides them. An explicit clear-weather tag stays clear even under clouds.

To prepare shared weather, choose **Create parcel tag**. A readout shows the compact description text and its byte count. Optionally enable **Share snow depth**, set depth and growth rate, and click **Stamp start time**. **Copy tag** copies it; paste it into the parcel description in About Land if you have permission to edit that description. The Weather page does not publish or overwrite a parcel description automatically. Other compatible, opted-in viewers receive the same seed, gust schedule, strike schedule, and published snow level. They may receive the description at different times; transitions, local tracks, camera flakes, and sound recordings are not promised to match between viewers.

### Shelter and runoff

**Rain shelter** retains the current fallback cover fade and optional depth map. With library surface weather or snow active, a basic surface cover map also keeps outdoor effects out of covered areas. To improve oblique rain, doorways, and close roof edges, select **Advanced shelter**. Its **Near detail**, **Far detail**, **Coverage**, **Bias**, **Softness**, and **Update rate** are explicit controls. **Show shelter** displays the map's coverage and missing data for setup; turn it off for a clean take. Advanced shelter does not silently turn on with a storm preset. Missing coverage is reported; a camera being under a roof alone never means all rain visible through a doorway should disappear.

In **Runoff**, turn on **Roof drips and streams** after persistent wet surfaces and advanced shelter are available. Rain first wets the roof, then eaves drip; heavy rain makes more continuous streams. **Wall rivulets** adds flowing wet highlights. **Strength**, **Drain time**, **Emitter limit**, and **Range** control the result. Stop the rain and the drips taper away; melting snow can feed them too. Drips wet their landing area and feed the nearby drip sound if audio is on. The page describes these as visible water effects, not changes to the region's water or physics.

### Avatar coating

Open **Avatar coating**, turn it on, and choose **Own avatar / Director cast / Nearby avatars**. Start with **Own avatar**. **Wet coating** darkens exposed material and **Snow coating** builds on exposed shoulders, hair, clothing, and attachments. **Amount**, **Melt and dry time**, **Avatar limit**, **Detail**, and **Range** bound the result. Clothing and attachments remain authored assets; coating is a local viewing effect.

With **Worn shelter** on, a hat or umbrella can shelter the body below while its exposed top wets or catches snow. The status shows which avatars have a live coating map and when the chosen limit has been reached. Changing an outfit or leaving range rebuilds or releases the affected maps. **Clear avatar coating** removes it without resetting the world blanket. This is separately enabled from ground snow and does not appear before the avatar implementation is usable.

### Repeatable take — the same storm on a reshoot

In **Repeatable take**, select **Repeatable** instead of **Live**. Type a **Seed**, choose a **Starting time**, and set **Starting wetness**, **Starting snow depth**, and **Pre-roll seconds**. Click **Prepare take**. The viewer fixes the current weather choices, the starting surface state, and any parcel weather being followed; clears tracks and pending thunder; and prepares the storm, maps, and sound. Status moves through **Preparing** to **Ready at 00:00**. Editing a locked weather choice marks the take **Needs preparation**.

Click **Start take** to begin live recording from zero. The same action can be armed for Director's existing ACTION button, so weather and camera playback start together. **Pause take** holds the storm and its sounds. **Restart take** returns to the prepared beginning, including the wetness, snow, empty starting tracks, flake pattern, gusts, and thunder choices. Click **Add strike cue** while paused at a chosen time to put lightning there on every run. **Save take setup** stores this recipe and cues; loading it requires Prepare before Start.

Keep the same viewer build, scene, starting marks, camera/actor playback, and weather detail for a matching reshoot. The weather recipe cannot replay changes made by the simulator, another avatar, or moving scene geometry. Live parcel changes are held during a prepared take; status says **Parcel source held for take**. Return to **Live** to follow current shared weather again. Preparation may take time, but Start does not compile shaders, decode sounds, or change quality. This is the live capture-card view at the selected quality throughout; it has no separate preview or still-image mode.

## 2. Evidence, component mapping, and integration

### Scope and verified starting point

Planning date: 2026-10-25. Destination: `I:/alchemy-machinima-weather`, branch `wip/weather-port`, HEAD `330d012e1612a3f4779a01dcc0614a91939ead0f`. The only initial status entry was the untracked `.tmp/` directory. This document is a plan, not a claim of implementation or performance results. No viewer source, settings, XUI, shaders, or contribution files are changed by this task. No build or shader run is part of this task.

Evidence was read from the specified Slipstream subtree and our native code, including `.tmp/weather_port/RESEARCH_REPORT.md`. The parent repository of the Slipstream subtree reports the same HEAD; that does **not** independently pin the subtree's file contents or certify it clean. Before copying, record per-file SHA-256 and available upstream revision in the port inventory, rather than attributing the entire subtree to that parent HEAD. Never read the excluded private/reference or external shader trees. There is no dependency on them.

Verified native points:

- `indra/newview/pipeline.cpp::renderWeather` is at approximately line 20129 in this checkout. Its call near line 24599 is in the HDR finalization chain before metering/bloom/tonemapping, after the existing SSR copy. This is suitable for precipitation, impacts, mist, lens effects, and bolts, **too late** to establish a material's roughness for lighting/reflections.
- `generateWeatherRainOcclusion` near line 31063 and `pipeline.h::mWeatherRainOcclusion*` provide the existing optional cover map. It currently has main-view assumptions and validity stamps that must not be reused blindly by other views.
- `alweathermodel.{cpp,h}` and its existing tests already provide seeded lightning driven by `LLPresentationTime::currentFrame().presentation_time`. Preserve that controller and its default behavior.
- `ALPanelWeatherSettings` owns saved looks, the tracked settings list, reset, and Strike now. `LLFloaterDirector::bindWeatherPage`, `ALWeatherPresetProvider`, and `refreshWorldLensReadouts` connect it to the generated page.
- Generator input is `scripts/code_tools/director_kit/director_mockup_data.json`; generated XUI is `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml`. Source/ownership documentation includes `doc/director_kit/pages/weather_tab.md` and `director_page_sources.py`. Change the generator input and bindings together; never hand-edit a generated page as the only implementation.
- PBR material packing uses `class1/deferred/pbropaqueF.glsl` (AO, roughness, metallic in `frag_data[1].rgb`); legacy paths use their own specular/gloss packing. `class3/deferred/materialF.glsl` contains both lighting/forward and G-buffer logic. Respect those differences and existing Actor FX, normal flags, emissive carriers, and colour-space rules.
- Our `indra/llaudio/llaudioengine.h` has no Slipstream `createLocalLoop`, `createLocalShot`, loop seek/shared-buffer helpers. Slipstream audio also depends on `LLCleanAcoustics`; it is **not** a drop-in pair of files.

### Mapping table

All target paths below are relative to this worktree. **Adapt CPU** means retain Slipstream algorithms and headers with narrow host/settings/time adapters; it does not mean copying its Vulkan or acoustic dependencies. **Reimplement GLSL/GL** means new native implementations following the reference behavior, not mechanical Vulkan-to-OpenGL translation. New filenames are proposed targets.

| Slipstream component / reference | Decision | Native file targets | Integration and scope |
|---|---|---|---|
| `indra/newview/slweather.{cpp,h}`: source resolution, physical rates, wind, transitions, wetness, snow, runoff, strike slots, movement/reset serials | Adapt CPU nearly as-is; inject configuration, place events, and clocks | Copy to `indra/newview/slweather.{cpp,h}`; new `alweatherbridge.{cpp,h}`, `alweatherclock.{cpp,h}`; `llappviewer.cpp`, `CMakeLists.txt` | One main-thread state update per viewer iteration after the frozen presentation context and before rendering/audio; renderer reads immutable state. Retain source semantics, remove direct renderer assumptions. |
| `slweatherregion.{cpp,h}` and `doc/weather_region_protocol.md` | Adapt CPU nearly as-is; preserve protocol | `indra/newview/slweatherregion.{cpp,h}`, `alweatherbridge.cpp`; `doc/weather/weather_region_protocol.md` | Agent's parcel/region events; roughly 1 Hz description polling without extra requests. Bound crossing hold, teleport reset, error/status reporting; no automatic parcel writes. |
| `slfloaterweather.{cpp,h}`, `skins/default/xui/en/floater_sl_weather.xml` | Adapt rate/preset/tag validation logic; skip the floater and its registration | `alpanelweathersettings.{cpp,h}`, new `alweatheruimodel.{cpp,h}`, `llfloaterdirector.{cpp,h}`, generator JSON and generated Weather page | Existing Director kit controls and preset provider. Extract model commands; do not put polling, rebuilding, settings lookups, or file I/O into draw. |
| Slipstream weather settings | Adapt definitions/defaults selectively; skip wholesale settings merge | `indra/newview/app_settings/settings.xml`; settings bridge and preset schema | One `AlchemyWeather*` namespace; no conflicting second master. Expensive Slipstream defaults are overridden by the opt-in defaults in section 3. |
| `vkgfx/llcleanweathervk.{cpp,h}` particle simulation/draw; `clean_weather_common.glsl`, `clean_weather_sim.comp`, `clean_weather.vert/.frag` | Reimplement GLSL/GL for snowfall; skip replacing our rain | New `alweatherresources.{cpp,h}`, `alweatherparticles.{cpp,h}`; new `class1/deferred/weatherSnowV.glsl`, `weatherSnowF.glsl`, `weatherSnowSim.glsl`; shader-manager files | Bounded instanced flakes, CPU-known fixed draw cap and GL compute where supported. No Vulkan descriptor sets, indirect-count assumptions, or Vulkan allocation/pipeline machinery. Native rain remains `weatherRainF.glsl` and its upsample path. |
| `clean_weather_surface.glsl` wet/puddle and snow material response; `clean_weather_snow_relief.glsl` | Reimplement GLSL for deferred and matching forward paths | New `class1/deferred/weatherMaterialF.glsl`; `pbropaqueF.glsl`, indexed sibling, `materialF.glsl` class1/class3, `terrainF.glsl`, `pbrterrainF.glsl`, relevant diffuse/masked paths; shader feature attachment in `llviewershadermgr.cpp` and `indra/llrender/llshadermgr.cpp/.h` | Modify decoded material before G-buffer packing/lighting; apply the same function before lighting alpha/forward surfaces. No extra full-screen weather G-buffer attachment or post-lighting additive substitute. Preserve HUD, water, emissive/fullbright, and avatar exclusions until explicitly supported. |
| `llcleanweathervk.cpp` near/far/tilted roof maps and `clean_weather_surface.glsl` exposure | Reimplement GL maps, reuse existing native depth-render patterns | `alweatherresources.{cpp,h}`, new `alweathershelter.{cpp,h}`, `pipeline.{cpp,h}`, new `weatherShelterF.glsl`; depth shaders/alpha-mask permutations only as needed | WP2 basic map extends `generateWeatherRainOcclusion` ownership for new surfaces; WP7 adds near/far tilted maps. Sample world-space coverage, not camera-only exposure. Keep legacy map off path and defaults intact. |
| Persistent ground cover/depth in `llcleanweathervk.cpp`; `clean_weather_snow.comp`, `clean_weather_snowmesh.comp/.vert/.frag`, `clean_weather_snow_join.glsl` | Reimplement GLSL/GL | New `alweathersnow.{cpp,h}`, `weatherSnowGrid.glsl`, `weatherSnowMeshV.glsl`, `weatherSnowMeshF.glsl`, shared material/shelter helpers; `pipeline.cpp` | World-anchored scrolling fields, bounded geometry, terrain/object cover, melt and seam handling. Geometry is built before view/shadow draws; opaque snow participates in G-buffer/depth and shadow passes. |
| `llcleanweathervk_movers.cpp` object/instance accumulation and footprint/contact logic | Reimplement GL and native scene tracking | `alweathersnow.{cpp,h}`, new `alweathercontacts.{cpp,h}`; `llvoavatar.cpp` contact/joint adapter, `llviewerobject.cpp` lifecycle adapter | Stable object/avatar IDs rather than Vulkan instance slots. Track stamps from movement/contact events; world-origin rebase, no temporal trails from arbitrary draw order. Full body-deformation simulation is skipped. |
| `llcleanweathervk_runoff.cpp`, `clean_weather_runoff.comp`, runoff branches of `clean_weather_sim.comp` / surface shaders | Reimplement GLSL/GL | New `alweatherrunoff.{cpp,h}`, `weatherRunoffSim.glsl`, `weatherRunoffV.glsl`, `weatherRunoffF.glsl`, shared material helper | Fixed-rate flow/outlet updates after shelter publication, bounded stream/drip draws before HDR postprocessing; rivulets modify material before lighting. Delayed listener feedback without blocking readback. |
| `llcleanweathervk_shelter.cpp`, `llcleanweathervk_accum.cpp`, `clean_weather_shelter.comp/.glsl`, `clean_weather_accum.comp/.glsl` | Reimplement GLSL/GL | New `alweatheravatar.{cpp,h}`, `weatherAvatarShelterF.glsl`, `weatherAvatarAccum.glsl`; `lldrawpoolavatar.cpp`, `lldrawpoolpbropaque.cpp`, `lldrawpoolmaterials.cpp`, `lldrawpoolalpha.cpp`, shared rigged draw helpers in `lldrawpool.cpp`, `llvoavatar.{cpp,h}`, avatar/material shaders | Rest-pose coating attached to identity; skinned/worn shelter atlas and bounded updates. Audit the existing pools' rigged permutations together, rather than assuming one avatar-only draw owner. Includes alpha-mask, worn rigid, animesh behavior and rebuild rules. |
| `slweatheraudio.{cpp,h}` weights, fades, thunder delay/selection, PCM budget | Adapt CPU mixer nearly as-is; reimplement backend interface, remove acoustic coupling | Copy `indra/newview/slweatheraudio.{cpp,h}` with adapters; new pure `alweatheraudiomix.{cpp,h}`; `indra/llaudio/llaudioengine.{cpp,h}`, `llaudioengine_openal.{cpp,h}`, new `llaudioweather.{cpp,h}` / decoder helper; both CMake lists | Audio idle once per iteration after weather; existing listener/ambient/mute and lifecycle. Add narrowly scoped local buffer playback helpers, asynchronous Vorbis decode and safe shutdown. No camera-driven duplicate mixer. |
| `app_settings/sounds/weather/` and its `LICENSE.txt`, `SOURCES.md`, `manifest.json` | Copy selected CC0 files intact with provenance | Same directory in our `indra/newview/app_settings`; `viewer_manifest.py` packaging verification | Ship documented loop/gain metadata and source hashes. No asset-server uploads. Prepared hail beds are not a hail feature. |
| Source strike scheduler + sky/cloud weather hooks | Adapt CPU strike schedule; reuse our lightning presentation | `alweatherbridge.cpp`, `alweathermodel.{cpp,h}` adapter entry point, `pipeline.cpp`, `weatherLightningF.glsl` only if needed for the explicit new mode | Select one scheduler; reuse bolt/cloud-sheet controls and HDR exposure chain. Weather-state storm default zero; Existing timing unchanged. Thunder consumes the same strike event stream. |
| `clean_weather_veil.glsl/.frag`, `clean_vol_cloud_weather.comp` | Reimplement limited precipitation visibility; skip Vulkan cloud/froxel systems | Optional bounded extinction in `weatherSnowF.glsl` / new `weatherPrecipVeilF.glsl`, bridge to native mist only through explicit choice | WP3 optional precipitation visibility, open-sky masked and applied once before tone map. Existing cloud link/cloud sheet remain; physical volumetric clouds, volumetric weather injection, and dynamic EEP replacement are outside WP1–WP9. |
| `clean_rt_weather.glsl`, `clean_weather_cluster.glsl`; ray-traced acoustics/opening rays | Skip | None | No Vulkan/RT/cluster renderer port. Native material rendering supplies probe/Prism/VCam consistency. Audio gets bounded cover feedback and explicit staging controls instead of RT acoustics. |

### Render and clock ownership contract

1. **One state owner.** The bridge updates CPU weather and produces a revisioned, immutable frame. The clock supplies elapsed simulation time and a separate epoch for seeded functions. `SLWeather` must not read a second wall clock internally. Region callbacks enqueue place changes; they do not mutate renderer state. Audio consumes the same frame and uniquely identified strike queue.
2. **Materials first.** Call a gated material helper after authored material/Actor FX decoding in the chosen documented order, before packing or forward lighting. Work in linear colour; preserve channel encodings, flags, AO, emission, and non-weather alpha. Surface-off permutations retain the old instruction path. Legacy/PBR roughness conversions are explicit; a PBR roughness value must never be written into a legacy gloss channel. Water, HUDs, glowing/fullbright surfaces, and initially avatars are masked out. Standard opaque, masked, terrain, and forward/alpha support must be tested as an inventory, not assumed from one shader include.
3. **Frame preparation before any view.** Freeze CPU state, update fixed-rate fields/contact stamps once, publish map transforms/validity as one generation, then draw. No library simulation advances in `renderWeather`, probe faces, shadows, Prism, or VCam. New-state preparation is inert when all new gates are off; the legacy controller retains its existing call order in the default path. Explicit library/repeatable operation supplies a frozen per-iteration event/frame adapter, so auxiliary draws cannot advance either selected storm scheduler twice.
4. **Main/VCam/Prism.** Main renders material changes plus atmospheric precipitation and bolts. The VCam actually used for recording and Prism auxiliary views receive the same weather time, seed, and material fields with their own matrices and depth. Native Prism currently calls deferred lighting in `llprismlens.cpp`; do not assume it reaches main `renderFinalize`. Explicitly wire a native auxiliary precipitation hook there where required. Flakes must cover enabled capture views from stable world cells; allocate bounded per-view visibility lists, not another independently simulated storm. Maps are world-anchored with declared coverage: a remote camera outside it reports insufficient coverage and needs preparation for its recording area rather than sampling a stale main-camera map.
5. **Probes/reflections.** Probe captures sample the already published wet/snow material and opaque snow geometry at a frozen weather generation; they never advance, clear, allocate, or recenter it. Preserve `gCubeSnapshot`'s existing atmospheric-weather exclusion. Do not bake transient camera flakes, lens drops, or screen bolts into cubemaps. Probe refresh remains on the normal budget, not every weather tick. Distinguish actual reflection-probe cubemaps from photographic snapshots; no snapshot-specific work is planned.
6. **Shadows.** Wetness/coating without geometric depth does not change silhouettes. Snow relief geometry uses the same depth/alpha-cutout silhouette for sun/spot/area shadow views as for the beauty view; shadow draws never run precipitation or simulation. Transparent flakes/drips do not cast new per-particle shadows in this plan. Preserve all GL state on every success, failure, off, and auxiliary exit: FBO, viewport, draw buffers, blend enables/factors, colour/depth masks, depth comparison, cull, sampler bindings and matrix state.
7. **Map publication.** Keep a read/write pair or explicit barrier with current-frame/generation validity. Consumers validate place, coverage, origin, matrices, and generation together. Existing shelter fallback is for camera/lens/audio only. Invalid surface maps conservatively suppress *new* deposition in unknown cells and expose status; they do not re-label indoor surfaces open or erase stored snow. Legacy invalid-map behavior remains unchanged. Use fenced asynchronous listener samples; never wait on GPU readback in recording.
8. **Lifetime and recording.** Lazy allocation while enabled, prewarm by Prepare, fixed user-selected quality during a take, bounded work/queues/memory, explicit unsupported/allocation-failure status. Release/invalidate on master off, GL loss, shutdown, and place reset according to the CPU serials. No per-frame retry/compile loop. Restore procedural wind ownership on audio disable/error. No capture-time adaptive quality switch and no still-image feature gate.

## 3. Shippable phases, settings, Director rows, and budgets

Effort: **S** = a small isolated change, **M** = several connected subsystems, **L** = substantial renderer/state integration and validation. Estimates include usable UI, presets/reset/search, tests, and failure handling; they are not calendar promises. Each WP is a complete usable feature, builds on released prerequisites, and introduces no reference to an unfinished later WP. A user can stop upgrading after any WP. Snow, runoff, and avatar rows are not shipped as dead controls ahead of their implementations.

### Defaults and budget policy applying to every WP

Keep every existing setting and value, including master `AlchemyWeatherEnabled=false`, rain enabled, rain intensity **0.55**, density **0.65**, speed **28**, samples **12**, divisor **2**, layers **3**, shutter **0.04**, near emphasis **0.65**, gust **0.35**, simulator-wind scale **1**, cloud coupling **0.35**, splash enabled, wet sheen enabled/strength **0.35**, fallback shelter enabled/height **48 m**, optional rain map **off**, resolution **512**, extent **128 m**, bias **0.12 m**, softness **0.35 m**, lightning enabled/rate **1.5/min**/seed **1597463007**, and all current quality-lightning defaults. The invariants test the complete baseline settings table, not just this illustrative list.

All new expensive effect gates start **off**. Explicitly applying a precipitation preset may enable its chosen rain/snow layer; it does not enable secondary features such as persistent surfaces, puddles, visibility, accumulation, relief, tracks, audio, advanced shelter, runoff or avatar coating. `AlchemyWeatherControlMode=0` means Existing rain; `1` means Weather library. Imported unversioned rain looks explicitly load mode 0, return the weather clock to Live, and leave newly introduced feature gates off. New presets are versioned and include the tracked settings from the phases present. Commands, transient ready states, decoded audio handles, and take playheads are not persisted as ordinary settings. Prepare/Start and destructive reset commands have model bindings, not fake boolean settings toggled from draw.

The new compute-backed particle/field/runoff/avatar paths require validated **OpenGL 4.3 compute, SSBO, image and barrier support**, adequate sampler/image limits, and successfully linked native permutations. Unsupported features are explained and disabled on their Director rows; existing rain remains available. Do not add a second CPU simulation fallback or increase the viewer's global minimum GL version as part of this port. Raster material wetness/basic cover can remain available wherever their own validated shader/resource requirements pass.

Budgets below are **proposed acceptance ceilings, not measured costs**. Reference measurement is native **3840×2160**, initially the RTX 5090 class expected by the existing volumetric harness, with exact GPU/driver/context logged. Also validate at 1080p and the eventual supported lower-tier recording hardware; no 5090 estimate is a promise for another GPU. Numbers are incremental over the matching legacy-weather baseline at the same camera/sky and quality. Measure the whole added pipeline including maps, field updates and auxiliary work, not just the particle draw. Shared allocations/passes are counted once. Peak bytes include both ping-pong sides, SSBO/VBO/indirect storage, history, and allocator overhead; MiB means 2^20 bytes. Avoid a new full-size scratch by default: one 4K RGBA16F image alone costs about 63.3 MiB.

| WP | Feature / effort | Extra GPU p95 at 4K | Incremental VRAM ceiling | Default | Release prerequisites |
|---|---|---:|---:|---|---|
| WP1 | CPU core, rain presets, Director / M | 0.05 ms | 4 MiB | New control mode off | Existing viewer |
| WP2 | Persistent wet materials/puddles / L | 0.60 ms, including basic surface cover | 4 MiB | Off | WP1 |
| WP3 | Snowfall / M | 0.65 ms | 16 MiB | Off | WP1–WP2 shared cover/resource layer |
| WP4 | Ground/object snow, relief/tracks / L | 0.90 ms | 48 MiB | Off; relief/tracks separately off | WP1–WP3 |
| WP5 | Weather audio / M | 0 GPU ms | 0 GPU MiB; 128 MiB decoded CPU/engine cap | Off | WP1; cover adapters use WP2 if active |
| WP6 | Parcel sync and repeatable take / L | 0.05 ms administration; no duplicate simulation | 4 MiB | All external sources off; Live | WP1; handles whichever WP2–WP5 effects are present |
| WP7 | Advanced shelter / M | 0.60 ms above WP2 basic map | 16 MiB | Off | WP2; WP3–WP6 consumers continue to work without it |
| WP8 | Runoff / L | 0.45 ms | 16 MiB | Off | WP2 + WP7; melt input from WP4 if enabled |
| WP9 | Avatar wet/snow coating and worn shelter / L | 0.65 ms at default avatar cap | 32 MiB | Off | WP2 + WP4 + WP7 |

Sum of these targets is **3.95 ms** and **140 MiB** before shared-pass savings. Set an all-effects gate of **≤4.0 ms extra p95**, **≤5.0 ms extra p99**, and **≤160 MiB** peak new GPU allocation on the reference scene with one main recording view and the stated default caps. A second active 4K recording view gets a separately reported cost; do not hide it in the main-view result. This aggregate target may force a design revision, not a lower-quality capture-time fallback. Frame time must also fit the user's complete-scene FPS target (16.67 ms at 60 FPS, 33.33 ms at 30 FPS); a weather-only budget cannot guarantee that. No >1 ms weather initialization/upload/compile stall is permitted after Ready. Record first-use costs separately during preparation.

### WP1 — CPU weather core, presets, and Director (M)

**Deliverable:** usable physical rain rates, smooth changes, weather wind and explicit weather-timed lightning, through the current rain renderer. Snow is not selectable until WP3. The CPU already carries snow/runoff fields so later phases do not redesign its state.

**Files:** copy `slweather.{cpp,h}` with headers; add `alweatherbridge`, initial `alweatherclock`, revisioned `alweatheruimodel`; register in `indra/newview/CMakeLists.txt`; idle hookup in `llappviewer.cpp`; rain/strike uniform adapter in `pipeline.cpp`; narrowly extend `alweathermodel` to accept an externally scheduled event without altering its existing scheduler. Add settings, `ALPanelWeatherSettings` commands/preset schema, `llfloaterdirector` bindings, generator input, generated page, ownership docs, and `doc/contributions.txt` credit in the future implementation. Tests: new `tests/slweather_test.cpp` and bridge cases alongside existing `alweathermodel_test.cpp`.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherControlMode` | U32; 0; 0 Existing / 1 library | Weather controls |
| `AlchemyWeatherLocalEnabled` | Boolean; false | Local weather |
| `AlchemyWeatherRainRate` | F32; 5; 0–200 mm/h | Rain rate |
| `AlchemyWeatherPrecipVariation` | F32; 0.35; 0–1 | Rain bands |
| `AlchemyWeatherWindSource` | U32; 0; Simulator / Weather | Wind source |
| `AlchemyWeatherWindDirection`, `AlchemyWeatherWindSpeed` | F32; 225 degrees / 3 m/s; 0–360 / 0–40 | From direction; Speed |
| `AlchemyWeatherWindVariation`, `AlchemyWeatherWindGustiness`, `AlchemyWeatherWindDirVariation` | F32; 0.3 / 0.5 / 0; 0–1 / 0–1 / 0–180 degrees | Speed variation; Gustiness; Direction swing |
| `AlchemyWeatherTransitionSeconds` | F32; 20; 0–600 s | Transition |
| `AlchemyWeatherSeed` | U32; 1597463007; full U32 | Weather seed |
| `AlchemyWeatherStorm` | F32; 0; 0–1 | Storm amount |
| `AlchemyWeatherLightningTiming` | U32; 0; Existing / Weather storm | Lightning timing |
| Existing `AlchemyWeatherRainColor`, `AlchemyWeatherLightningColor` | Color3; retain (0.55, 0.68, 0.78) / (0.72, 0.82, 1.0) | Rain colour; Lightning colour, using existing themed colour control support |

New preset picker/Apply, source/current-rate/transition readouts, Stop precipitation, Clear surfaces now, and existing Save/Load/Reset/Strike are command rows. Initially source is local/clear; no pretend parcel status. Preserve all current artistic rain rows and explain mode-dependent behavior, including legacy-only procedural gust/cloud link. Library rain uses a monotonic calibrated rate-to-density/strength bridge; physical accumulation uses the physical rate, not artistic brightness. The bridge must not persistently rewrite the user's old tuning. When mode 0 with new effects off, do not execute new shader uniforms, change permutations, or alter old clock calculations.

**Cost/risk:** ≤0.05 ms GPU and ≤0.10 ms main-thread p95 added CPU, no new full-frame pass. Biggest risks are double cloud/gust/lightning modulation, default migration, per-view double updates, direct wall-clock reads, and renderer-coupled source/reset behavior. Preserve reset/place serial meanings; expose the configuration/time seams rather than rewriting proven CPU math.

### WP2 — Persistent wet surfaces and puddles (L)

**Deliverable:** weather-off identity plus persistent outdoor wet material response, puddle masks and ripples with either rain mode. A legacy-to-CPU precipitation adapter feeds accumulation when new surfaces are enabled under Existing rain; it never changes the legacy streaks or their settings. Master off clears; Stop precipitation dries naturally.

**Files:** new material helper and shared `alweatherresources`; basic surface-cover owner in `alweathershelter`; `pipeline.{cpp,h}` map preparation/lifetime, material/terrain/opaque/masked/forward shaders from section 2, shader attachment/uniform plumbing; WP1 settings/UI/preset files; `scripts/code_tools/weather_material_gl_check.py` and fixtures. No full-screen material rewrite or extra G-buffer is introduced.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherPersistentWetEnabled`, `AlchemyWeatherPuddlesEnabled` | Boolean; false / false | Persistent wet surfaces; Puddles |
| `AlchemyWeatherWetAmount`, `AlchemyWeatherPuddleAmount`, `AlchemyWeatherRippleStrength` | F32; 0.5 / 0.35 / 0.25; each 0–1 | Wet amount; Puddle amount; Ripple strength |
| `AlchemyWeatherWetSeconds`, `AlchemyWeatherDrySeconds` | F32; 120 / 300; 1–3600 / 1–7200 s | Soak time; Dry time |
| `AlchemyWeatherStartWetness` | F32; 0; 0–1 | Starting wetness |
| `AlchemyWeatherSurfaceCoverResolution`, `AlchemyWeatherSurfaceCoverExtent` | U32 512 / F32 128 m; 256–1024 power-of-two / 32–256 m | Surface cover detail; Coverage |

Apply starting surfaces and coverage/validity readouts are commands/readouts. Basic cover is required automatically *inside* the enabled new material effect, even if legacy rain occlusion is off; it does not turn on that old option or change legacy sampling. Initially vertical cover is sufficient, with a world-stable puddle mask. No unbounded cavity simulation, displaced water, guaranteed mirror reflection, or camera-ray-based indoor material mask. Suppress the old additive wet sheen and its additive wet-glint contribution only while the replacement is active; restore it unchanged otherwise. Existing splashes/mist/lens remain independent.

**Cost/risk:** ≤0.60 ms and ≤4 MiB, including a double-buffered 512² D32F basic cover map (~2 MiB) plus bounded metadata. Shader ALU/samplers rather than another full-frame image. Keep roughness above the native minimum, dielectric darkening/film response bounded, metallic response conservative, ripple normals normalized/filtered. Risks: specular energy gain, patch shimmer, alpha/PBR/legacy mismatch, surface map use after rain stops, map validity in auxiliary views, and accidental modification of emission/flags. Sky/rain alone must not wet covered interiors.

### WP3 — Snowfall particles and optional visibility (M)

**Deliverable:** snow and mixed presets/rates with wind-driven, lit, depth-aware flakes. No depth blanket is required to enjoy snowfall. Physical precipitation visibility is a separately enabled bounded veil, never a hidden fog cost in a preset.

**Files:** `alweatherparticles`, `weatherSnowV/F`, `weatherSnowSim` and optional `weatherPrecipVeilF`; particle hooks in `pipeline` and `llprismlens.cpp` auxiliary/capture entry points; shader manager/CMake; settings/Director/preset sources; `scripts/code_tools/weather_particles_gl_check.py`. Compute implementation must validate GL capability/entry points. Use a bounded instanced draw with deterministic IDs; unsupported GL gets explicit unavailable status, never a silent CPU particle storm.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherSnowEnabled` | Boolean; false | Falling snow |
| `AlchemyWeatherPrecipType` | U32; 0; Rain / Snow / Mixed | Precipitation |
| `AlchemyWeatherSnowRate` | F32; 1.5; 0–360 cm/h fresh snowfall | Snowfall rate |
| `AlchemyWeatherSnowFlakeSize`, `AlchemyWeatherSnowFallSpeed`, `AlchemyWeatherSnowRange` | F32; 0.015 m / 1 m/s / 64 m; 0.002–0.08 / 0.2–4 / 16–128 | Flake size; Fall speed; Draw distance |
| `AlchemyWeatherSnowParticleLimit` | U32; 32768; 4096–131072 | Flake limit |
| `AlchemyWeatherPrecipVisibilityEnabled`, `AlchemyWeatherPrecipVisibilityAmount` | Boolean false / F32 1; 0–1 | Precipitation visibility; Visibility amount |

Preset application does not enable ground snow/relief or the visibility veil. Shared cell hashes/time create repeatable flakes with camera culling; avoid screen-fixed sparkle and reseeding on pans. Track separate snow/rain components without adding snow to the legacy rain intensity. Apply extinction once, with open-sky cover and correct depth, without double counting existing mist.

**Cost/risk:** ≤0.65 ms combined simulation/draw/optional veil, ≤16 MiB at default cap, no default full-resolution history image. Dense high-cap stress cases get separate budgets/status, not assumed included. Risks: translucent overdraw, tiny flake aliasing, depth halos, crossing cell boundaries, wind changes, mixed-weather normalization, and auxiliary cameras seeing an empty particle volume.

### WP4 — World snow accumulation, relief, footprints, object tracks (L)

**Deliverable:** persistent cover and physically integrated depth; optional close snow mesh/relief and contact tracks. Includes modest moving-object coating carried in object space, but not avatars. Tracks can work as material/depth-field depressions without requiring an unbounded mesh.

**Files:** `alweathersnow`, `alweathercontacts`, world-origin/place integration in bridge, native object and avatar-joint/contact adapters, `weatherSnowGrid`, mesh V/F and matching shadow variants, material helper; shader manager/pipeline/CMake/UI; `scripts/code_tools/weather_snow_gl_check.py`. Native object lifetime feeds stable identities/revisions instead of modifying the Vulkan ingest layer.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherSnowAccumEnabled`, `AlchemyWeatherSnowReliefEnabled` | Boolean; false / false | Accumulate snow; Snow relief |
| `AlchemyWeatherSnowMaxDepth`, `AlchemyWeatherStartSnowDepth` | F32; 0.35 / 0 m; 0–1 / 0–max depth | Maximum depth; Starting snow depth |
| `AlchemyWeatherSnowBuildSeconds`, `AlchemyWeatherSnowMeltSeconds` | F32; 600 / 900; 1–7200 / 1–14400 s | Cover build time; Melt time |
| `AlchemyWeatherSnowGridResolution`, `AlchemyWeatherSnowExtent` | U32 512 / F32 64 m; 256–1024 power-of-two / 32–128 m | Snow detail; Working area |
| `AlchemyWeatherSnowUpdateHz` | U32; 30; 15 / 30 / 60 | Snow update rate |
| `AlchemyWeatherSnowFootprintsEnabled`, `AlchemyWeatherSnowObjectTracksEnabled` | Boolean; false / false | Footprints; Object tracks |
| `AlchemyWeatherSnowTrackStrength`, `AlchemyWeatherSnowContactLimit` | F32 0.7 / U32 256; 0–1 / 16–1024 | Track strength; Contact limit |
| `AlchemyWeatherTeleportKeepMeters`, `AlchemyWeatherCrossingFadeSeconds`, `AlchemyWeatherSnowSyncFadeSeconds` | F32; 64 m / 20 s / 8 s; 0–256 / 0–300 / 0–120 | Keep nearby teleports; Crossing fade; Shared-depth fade |
| `AlchemyWeatherTeleportSeed` | Boolean; true, effective only for enabled new accumulation | Start destination settled |

Apply starting surfaces, Clear tracks and field memory/extent readouts complete the feature. Adapt the source's 0.3 fresh-snow settlement fraction and physical depth calculation. Treat visual cover and metres separately. Schedule field updates at a fixed weather-tick divisor (30 Hz by default); interpolate presentation without tying accumulation/contact count to display FPS. Define first-arrival, crossing, teleport, origin-shift and source-change behavior exactly; clear ground patterns on new-place serial, preserve snow on ordinary snowy crossings, and do not carry region accumulation into a clear destination forever. The initial basic cover map conservatively handles indoor deposition. WP7 later improves windward edges; WP4 does not depend on future maps.

**Cost/risk:** ≤0.90 ms and ≤48 MiB. A 512² two-sided RGBA16F field is ~4 MiB; reserve outer cover, contact lists, bounded object rows and ≤32 MiB mesh buffers within the cap. Bound mesh triangles and object rows (initially 4096), and report overflow deterministically. No unconstrained voxel blanket. Risks: stacked surfaces/bridges, ledge seams, buried cover when an object moves, depth/colour/shadow mismatch, track stamps tied to FPS, camera recenter pops, animesh contacts, and melt-water accounting. Full body contact, exact vehicle tire physics, and remote track replication are skipped.

### WP5 — Shelter-aware rain/wind/snow/thunder audio (M)

**Deliverable:** weather mix usable for capture, including thunder from either native or weather-timed strikes. Port the mix math, not the RT acoustic subsystem.

**Files:** `slweatheraudio`, pure `alweatheraudiomix`, native `llaudioweather`/decode helper and minimal audio-engine/OpenAL backend extensions from section 2; `llappviewer.cpp` idle/shutdown ordering; CMake and `indra/newview/viewer_manifest.py`; copied sound assets/provenance; all Director files; new `tests/alweatheraudiomix_test.cpp` and audio lifecycle/backend tests. Inventory actual source engine helper implementations before porting the required subset; do not copy the entire engine.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherAudioEnabled`, `AlchemyWeatherThunderEnabled` | Boolean; false / false | Weather audio; Thunder |
| `AlchemyWeatherAudioVolume` | F32; 0.5; 0–1 | Master weather volume |
| `AlchemyWeatherAudioRain`, `AlchemyWeatherAudioRoof`, `AlchemyWeatherAudioWind`, `AlchemyWeatherAudioSnow`, `AlchemyWeatherAudioThunder` | F32; each 1; 0–1 | Rain; Roof and drips; Wind; Snow; Thunder levels |
| `AlchemyWeatherAudioCoverMode`, `AlchemyWeatherAudioRoofMaterial` | U32; 0 Automatic / 0 Generic; enums described in section 1 | Cover sound; Roof sound |
| `AlchemyWeatherThunderDelayScale` | F32; 0.35; 0–1 | Thunder delay scale |
| `AlchemyWeatherAudioLoopVariation` | Boolean; true, only inside enabled audio | Vary loops |
| `AlchemyWeatherAudioPcmBudgetMiB` | U32; 128; 32–256 | Decoded audio limit |

Readouts: loaded/needed beds, engine availability, cover confidence, memory use, Audio ready. Port equal-power weighting and approximately 0.6 s in / 1.8 s out fades, loaded-neighbor fallback, delayed thunder, and bounded decode jobs. No hail UI/state despite included reference recordings. Without material measurements use Generic or the explicit manual roof choice, not fabricated wood/metal detection. Bounded existing scene ray or map feedback is sufficient for initial cover; WP7 improves it. Keep PCM in one engine copy where possible, free silent beds, fence stale decode completions on reset/engine restart. Schedule sound by actual audio listener and event ID.

**Cost/risk:** zero new GPU cost; ≤0.15 ms p95 main-thread mix/commit, decoding off thread, ≤128 MiB decoded data including pending decodes and OpenAL buffers, ≤32 MiB encoded assets cache. Avoid double-counting the source's engine-only budget while a worker holds PCM. Initial voice budget: at most 16 loop playheads plus 8 concurrent one-shots, voice stealing deterministic. Risks: source helper API gap, double simulator/weather wind, clicks/phase changes, decode hitches, ambient mute/device restart, duplicate thunder, compressed-distance delay, and missing files. Silent/unavailable plus one diagnostic on asset failure; no surprise fallback sound.

### WP6 — Parcel/synced weather and repeatable take clock (L)

**Deliverable:** compatible opt-in parcel/EEP sources and a separate local take recipe/transport that can repeat existing rain and whichever new effects have shipped.

**Files:** copy/adapt `slweatherregion` and protocol doc; extend `alweatherclock`, bridge/source model; new `alweathertake.{cpp,h}` and versioned recipe serializer; existing Director ACTION/CUT command path in `llfloaterdirector` plus Weather page bindings; `slweatheraudio` deterministic selection/playhead hooks; uniform clocks in native rain/surface/lightning/new shaders. Tests: `tests/slweatherregion_test.cpp`, `alweatherclock_test.cpp`, `alweathertake_test.cpp`, new `scripts/code_tools/weather_take_gl_check.py`. Identify and reuse the existing transport dispatcher instead of adding another competing ACTION callback.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherAllowParcel`, `AlchemyWeatherFromEnvironment` | Boolean; false / false | Use parcel weather; Weather from sky |
| `AlchemyWeatherClockMode` | U32; 0 Live / 1 Repeatable | Clock mode |
| `AlchemyWeatherTakeSeed` | U32; 1597463007 | Seed |
| `AlchemyWeatherTakeEpoch` | String; `2026-01-01T00:00:00Z`; validated UTC | Starting time |
| `AlchemyWeatherTakePrerollSeconds` | F32; 10; 0–600 s | Pre-roll |
| `AlchemyWeatherTakeArmAction` | Boolean; false | Start with ACTION |
| `AlchemyWeatherParcelShareDepth` | Boolean; false | Share snow depth |
| `AlchemyWeatherParcelDepth`, `AlchemyWeatherParcelSnowRate`, `AlchemyWeatherParcelSince` | F32 0 m / F32 0 m/h / String empty; 0–10 / -100–100 / accepted protocol Unix seconds | Published depth; Growth rate; Start time |

Starting wetness/depth reuse WP2/WP4 rows and are saved in the take recipe. Prepare/Start/Pause/Restart/Save/Load, strike-cue editing, source/errors, frozen-source status, Create tag/Copy/Stamp, UTF-8 byte count and round-trip status are command/readout rows. First release prepares text only; About Land remains the explicit publication route.

**Source contract:** master off wins; local target wins; a parsed parcel tag wins over opted-in EEP fallback; explicit `off/clear` suppresses EEP; otherwise no source. Use only the current agent parcel, with bounded crossing hold and stale-property rejection. Preserve first-tag/case/clamp/unknown-key/per-token-error behavior, rate keys taking priority over amount keys, default seed from region+parcel identity, physical units, `since` handling, rate sign, and depth caps. Packet/description availability is lazy; shared timing is not lockstep rendering. Shared gust/strike epoch uses source-compatible corrected UTC (`LLDate` plus server UTC offset) sampled once into the frame; keep network/watchdog time independent from presentation time.

**Take contract:** a weather-only clock composes with the existing frozen presentation context; it must never retime network/UI/watchdogs or write `LLPresentationTime` global state. Live local weather follows presentation deltas; live shared weather follows the captured shared epoch. Repeatable time is `recipe_epoch + integer_tick/120`, with a fractional render sample and an explicit weather event tick. CPU integration, contacts, grids and cues run in a defined fixed order; event/hash keys are stable IDs, not frame count, allocation order, `getTotalTime`, or audio load completion. Existing rain's time uniforms receive this clock **only** in explicitly selected Repeatable mode; Live/Existing defaults preserve the original path.

The recipe includes schema/version, source target/tag and effective settings, epoch/seed, initial wetness/cover/depth/runoff, initialized empty tracks/contact queues, per-avatar initial coating rules, scene/origin anchor, scheduled cues, view/detail/caps, and sound variant/loop phase seeds. Prepare restores that canonical starting state and simulates pre-roll before tick zero. A saved recipe is not a dump of arbitrary old GPU tracks. Preparing with persistent effects disabled uses zero state and does not allocate their resources. Manual effects during a running take mark it changed unless recorded as ordered cues. Pause freezes weather playheads and pending thunder; resume does not repeat a strike. Restart discards stale GPU readbacks/decode results by generation and restores deterministic playheads. Available audio backends must support the needed seek/pause semantics before marking Audio ready.

Take replay is guaranteed for the same build, GPU/driver, settings and scene/contact inputs: exact CPU event/state traces, GPU fields within stated numerical tolerance and matching visible motion at matched weather times. It is not a cross-GPU bitwise-image promise or simulator replay. Large jumps/backward time do not silently clamp dt and change the storm: seek/reset/replay happens while Preparing; if live playback loses its prepared resources or cannot account for elapsed ticks, mark Needs preparation and report the interruption. A normal hitch may drop a displayed frame but cannot reseed or skip a cue's state. Fixed-step GPU work must remain bounded within the chosen take/catch-up contract; benchmark this before claiming Ready at any target FPS. A parcel source change marks live state but cannot mutate a held recipe.

**Cost/risk:** ≤0.05 ms GPU admin, ≤4 MiB metadata/work queues; ≤0.15 ms p95 ordinary CPU clock/source administration. Pre-roll/replay cost is outside recording and reported. No hidden duplicate atlas checkpoint; canonical initialization fits existing field allocations. Risks: source's direct time reads and dt clamp, source audio's non-deterministic variant/history logic, epoch precision, description byte limits, identical-seed parcels, asynchronous resets, transport ordering, and overstating what other viewers synchronize.

### WP7 — Near/far tilted shelter maps (M)

**Deliverable:** better windward cover and doorway behavior for rain/snow/surfaces/audio without replacing the current shelter choice by default.

**Files:** expand `alweathershelter`, `alweatherresources`, `pipeline` depth/cull/map helpers and publication; GLSL exposure helper; native material/particle/audio consumers; shader manager/UI; `scripts/code_tools/weather_shelter_gl_check.py`. New map owner must not commandeer sun/spot/probe shadow targets.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherAdvancedShelterEnabled` | Boolean; false | Advanced shelter |
| `AlchemyWeatherShelterNearResolution`, `AlchemyWeatherShelterFarResolution` | U32; 1024 / 512; power-of-two 256–2048 / 256–1024 | Near detail; Far detail |
| `AlchemyWeatherShelterNearExtent`, `AlchemyWeatherShelterFarExtent` | F32; 32 / 128 m; 16–128 / 64–512 | Near coverage; Far coverage |
| `AlchemyWeatherShelterTiltEnabled` | Boolean; true under advanced gate | Follow weather wind |
| `AlchemyWeatherShelterUpdateHz` | U32; 30; 5–60 | Update rate |
| `AlchemyWeatherShelterBias`, `AlchemyWeatherShelterSoftness` | F32; 0.12 / 0.35 m; 0–1 / 0.01–2 | Bias; Softness |
| `AlchemyWeatherShelterDebug` | Boolean; false | Show shelter |

Maps use slow mean wind and a bounded fall-angle shear, not each gust; snap world-space cells and blend near/far borders. Snow cover/depth needs vertical height and slanted precipitation reach: retain an untilted height source and do not reinterpret tilted depth as ground height. Alpha-masked roofs are rasterized with native cutoff rules; blended glass is not assumed opaque. Document the initial glass/transparent-cover limitation and allow existing camera fallback for sound/lens. Dynamic static-object revisions trigger bounded dirty updates; avatar/worn cover belongs to WP9. Update/coverage status is searchable. At default resolutions, two near D32F surfaces take ~8 MiB, far ~2 MiB, with remaining budget for vertical data/metadata. Reuse rather than add WP2's basic map when valid.

**Cost/risk:** ≤0.60 ms extra p95 over basic cover, ≤16 MiB added. Measure full scene cull/draw/dirty costs and angular refresh spikes, including a moving roof. Risks: snapping/swimming, tilted/vertical coordinate mismatch, stale movers, bias leaks, far-camera coverage, invalid publication, render-state contamination, and GPU readback stalls. Map failures retain explicit fallback/unknown status, not a false valid stamp.

### WP8 — Roof runoff, streams, wall rivulets (L)

**Deliverable:** bounded runoff with drainage and melt input, outdoor/covered landing wetness, optional flow highlights, and drip sound feedback.

**Files:** `alweatherrunoff`, flow/particle GLSL targets from section 2, material/cover helpers, pipeline/resource lifecycle, state/audio bridge and all UI/preset files; `scripts/code_tools/weather_runoff_gl_check.py`. Prerequisites are visible in the page: persistent wet surfaces and advanced shelter; enabling runoff explicitly offers/enables those required choices before starting, not on preset load.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherRunoffEnabled`, `AlchemyWeatherRivuletsEnabled` | Boolean; false / false | Roof drips and streams; Wall rivulets |
| `AlchemyWeatherRunoffStrength`, `AlchemyWeatherRunoffDrainSeconds` | F32; 0.5 / 90 s; 0–1 / 1–600 | Strength; Drain time |
| `AlchemyWeatherRunoffRange`, `AlchemyWeatherRunoffEmitterLimit` | F32 32 m / U32 512; 8–64 / 64–2048 | Range; Emitter limit |
| `AlchemyWeatherRunoffUpdateHz` | U32; 15; 5–30 | Flow update rate |

Show live emitters/overflow/near-listener level. Integrate source runoff, never above available wetness; permit source-defined melt contribution, saturate and drain predictably. Fixed-grid flow/outlet extraction with deterministic thinning, bounded update iterations and persistent emitter IDs avoids a fresh random roof each frame. Shader rivulets affect roughness/normals, not painted dark streaks or emission. Landing wetness uses explicit outlet/landing height, including shelter beneath an eave. No full fluid simulation, collision-water physics, or actual moving water volumes.

**Cost/risk:** ≤0.45 ms and ≤16 MiB with no synchronous outlet-list readback. Risks: flow amplification, cyclic/slope topology, sheet aliasing, emitter popping on cull/recenter, rain-off drain discontinuity, indoor landing leaks, and sound/visual delays. Audio feedback is bounded, delayed, generation-tagged, and dispensable on a missed readback.

### WP9 — Avatar coating, worn shelter, animated persistence (L)

**Deliverable:** local wet/snow coating that follows animation and clothing, with explicitly bounded avatar scope and worn shelter. Ordinary particle rain and ground snow remain usable when this is off.

**Files:** `alweatheravatar`, native avatar/rigged/worn draw adapters, rest-pose accumulation/shelter GLSL, avatar/opaque/legacy/forward/masked shader variants and manager uniforms, resource/bridge lifecycle, Director cast membership adapter, all UI/preset files; `scripts/code_tools/weather_avatar_gl_check.py` and identity/slot lifecycle unit cases. Include animesh through its actual skin/draw owner, not an assumption about avatar-only rendering.

| Proposed setting | Type; default; range | Director row |
|---|---|---|
| `AlchemyWeatherAvatarCoatingEnabled` | Boolean; false | Avatar coating |
| `AlchemyWeatherAvatarScope` | U32; 0; Own / Cast / Nearby | Apply to |
| `AlchemyWeatherAvatarWetEnabled`, `AlchemyWeatherAvatarSnowEnabled`, `AlchemyWeatherAvatarWornShelter` | Boolean; true / true / true, only within master coating gate | Wet coating; Snow coating; Worn shelter |
| `AlchemyWeatherAvatarAmount` | F32; 0.5; 0–1 | Amount |
| `AlchemyWeatherAvatarDrySeconds`, `AlchemyWeatherAvatarMeltSeconds` | F32; 300 / 900; 1–7200 / 1–14400 s | Dry time; Melt time |
| `AlchemyWeatherAvatarLimit`, `AlchemyWeatherAvatarMapResolution`, `AlchemyWeatherAvatarRange` | U32 4 / U32 128 / F32 24 m; 1–16 / power-of-two 64–256 / 4–64 | Avatar limit; Detail; Range |
| `AlchemyWeatherAvatarGraceSeconds` | F32; 30; 0–120 s | Keep out-of-range coating |

Clear avatar coating and atlas/slot status complete the page. Default scope Own means at most the local avatar allocates initially; the budget includes up to four at default detail. Slots follow UUID/rig identity with sticky admission and deterministic eviction; rest-pose coordinates keep wet/snow patches attached across animation. Use geometric exposure normals and correct world/rest transforms, not only tangent-space bump normals. Worn shelter is updated from actual skinned and rigid attachment draws, with alpha masks; it does not make the world roof atlas include the entire avatar. Preserve modest per-avatar amounts through temporary map loss, safely invalidate pose/mesh coordinates on outfit/skeleton change, clear removed identities, and never transfer a previous slot's coat to the next occupant. Snow coating can follow global snowfall even when ground relief is off; no accidental dependency on drawing the snow mesh.

**Cost/risk:** ≤0.65 ms and ≤32 MiB at four 128² avatars, plus a separately measured 16-avatar/256² stress case which may exceed the default ceiling and is explicitly an advanced choice. Risks: animation swimming, first-person attachment visibility, avatar-light/Actor FX interference, slot churn, teleport/region drying, transformed normal errors, skin-path permutations, and per-avatar shadow mismatch if relief is added. This phase is a material coat without a new displaced avatar silhouette; do not claim geometric snow thickness or body-contact deformation.

## 4. Test plan and release gates

These are **future implementation requirements**. Nothing in this section was run during the planning task. Read-only source inspection is not a shader, performance, UI, or in-world pass.

### Harness construction and evidence

- Follow `scripts/code_tools/relight_3d_gl.py`: hidden WGL context, production shader module assembly, `SourceTree` pinned-ref versus WORKTREE reads, native texture formats and actual G-buffer packing. Use baseline **330d012e1612a3f4779a01dcc0614a91939ead0f** throughout the initial port; do not substitute one relight script's older lighting baseline.
- Follow `relight_world_gl_check.py` for **actual GL rasterized** roof/alpha-mask/geometry fixtures, published maps and production host math; an analytic CPU cover map cannot prove GPU shelter. Follow `relight_volumetric_gl_check.py` for in-memory mutations, warmed `GL_TIME_ELAPSED`, repeated motion sequences, and trace hosts built from actual production helpers. Never create a test-only weather algorithm that merely resembles the shipping one. Missing required GL version, entry points, compiler, fixtures, or target renderer is a failed/unavailable gate, not a passing skip.
- Proposed runners: `weather_material_gl_check.py`, `weather_particles_gl_check.py`, `weather_snow_gl_check.py`, `weather_take_gl_check.py`, `weather_shelter_gl_check.py`, `weather_runoff_gl_check.py`, `weather_avatar_gl_check.py`, with a later `weather_port_check.py` orchestrator. Add scripts only with their implementing WP. Keep generated reports/images under `.tmp/weather_port/<WP>/<run>/`. Set `PYTHONUTF8=1` for every Python invocation.
- Every report records tested ref, baseline ref, content hashes, shader defines/modules and link logs, GPU/vendor/driver/GL/extensions, resolution, scene, settings, clock/tick/seed, errors, timing distributions and bytes. Store raw HDR/depth/material arrays plus reviewable images and motion strips; generated PNG comparisons are diagnostic test artifacts, not a snapshot feature. Preserve a machine-readable list of gates and mutation verdicts.
- WGL cannot instantiate the whole viewer/LLPipeline or audio engine. Supplement shader evidence with compiled production CPU helpers, source/call-site wiring checks, native unit tests, and an in-world capture-card run. A string check alone never proves runtime integration; a standalone shader pass alone never proves all native variants are wired.

### Common gates for every phase

**A. Off and compatibility goldens.** On the same GL context/driver, render baseline and changed code with identical input uploads and frozen time. With master off, compare HDR colour, all material/depth attachments and final LDR **byte-for-byte**, as well as draw/state/allocated-resource traces. With master on, Existing rain selected and every new feature off, require the same byte identity for the complete old weather chain: default rain, splashes, wet sheen, manual and automatic lightning, old presets, optional shelter, mist/lens and quality-lightning variants. The source files/old variants should remain unchanged when no feature needs them. Compare pinned baseline code to new **production permutation paths**, not two invocations of the new disabled shader. A default transition from a new feature on to off also restores the old rendering exactly, without stale map/field/additive brightness. Old visible-look identity is independent of releasing no-longer-needed allocations. No tolerance waives these same-context compatibility gates.

**B. Physical/material bounds.** Test finite HDR everywhere, wetness/cover/runoff in [0,1], depth in [0,max], unit normals within 1e-3, valid alpha/AO/flags/emissive bytes unchanged by a pure coating, authored colour-space correctness, native minimum roughness, and material values in their defined ranges. For unlit black inputs, wetness/snow alone cannot emit light. Evaluate dielectric/metallic hemispherical BRDF samples: integrated reflected energy ≤1.05 of the defined incident-energy reference, with the extra 5% only numerical integration tolerance; do not assert that each brighter specular pixel is an energy violation. New coating albedo ≤1 and no negative radiance. Legacy bright artistic lightning has its own configured HDR ceiling and must not be mistaken for reflected material energy. Zero rain/snow amount gives no new deposits; snow alone does not wet; melt feeds water only once. Test tilted walls, roof undersides, metallic objects, rough pavement, emissive/fullbright, alpha-mask fences and glass separately.

**C. Temporal stability.** Include stationary holds, 1 mm camera dollies and 0.02-degree yaw microsteps, regular pan/orbit/dolly, abrupt view switches, wind turns across 359→0 degrees, near/far map seams, scrolling-grid boundaries, region rebases, doorways, changing sky, cold start, zero-duration and 20/600-second transitions, stopping precipitation, melting, and hitches. Test 24/30/60/120 FPS and matched absolute weather times. In material-only stable-region reprojection, away from true disocclusions, require p99 absolute linear-luminance residual ≤0.01 normalized to unit incident light and no >0.05 outlier; a uniform-scene camera recenter must not step mean cover/wetness by >0.01. Map boundary seams must not change level-surface wetness by >0.02. Particle tests compare matched world trajectories/distributions and edge coverage, not identical successive moving-flake pixels. No NaN, sudden whole-field reseed, disappearing roof, or unreviewed startup frame is acceptable. Tune any fixture-specific tolerance before implementation against baseline/reference and record it; do not loosen after seeing a failure without explaining the metric's defect.

**D. Determinism.** Feed identical fixed-tick inputs with multiple display-frame partitions and compare CPU state/event hashes exactly at common ticks (serialized fields, not padded structs). Duplicate/reorder read-only main/shadow/probe/Prism/VCam draws and demand unchanged simulation/event count. On the same GPU/driver, repeat prepared takes with ≤1e-4 absolute GPU field difference and no changed emitter/contact identity. For matched clock/time and camera, compare visible output with explicit material/particle tolerances. Test zero seed, high U32 seeds, long epochs, pause/resume/restart, pre-roll, reset during async work, engine restart, repeated manual cues and source changes. Audio scheduling/variant/loop-start traces must match sample indices; hardware-output waveforms may differ with device latency. Never call the source protocol fully deterministic across users when it explicitly does not sync transitions/thunder choices.

**E. Cost and memory A/B.** At 3840×2160 run baseline → off/new-build → feature-on → off/new-build, warm shaders/resources for at least 120 frames, then collect at least 600 samples and repeat 3 times in alternating A/B order. Time each production pass and total new weather GPU work; report median/p95/p99/min/max, CPU preparation/culling/audio/decode commit, initial warmup, allocation bytes and pending work. GPU queries alone exclude CPU/readback, so report viewer frame-time distributions as well. Disable vsync for diagnostic GPU measurements, then validate the real capture-card pacing with the intended vsync/FPS settings. Cover open outdoor, dense city/trees/alpha roofs, indoor doorway, snowy ground with contacts, and a default-cap avatar scene. Report main-only and main+Prism/VCam separately. No same-frame GL query/readback waits. Saturate contact/emitter/voice/resource caps, exercise GL loss/failed allocation/feature toggles, and require bounded work plus truthful status. Combined all-effects defaults must pass section 3's aggregate target; passing each microbenchmark is insufficient.

**F. Must-fail controls.** Every new runner must actually execute its deliberate mutations and exit nonzero for each. Apply mutations only in memory or a disposable test tree under `.tmp/weather_port/`, never leave viewer files modified. Include one scene/input where each mutant is observably wrong. Test the test runner: omitted draws, constant outputs, unused uniforms, disconnected production call sites, forged validity, missing barriers and duplicate state updates cannot all evade a passing gate. A mutation compile error alone is not proof that an image/state invariant detects the intended defect; prefer runnable wrong behavior and identify the failing named assertion.

### Phase-specific tests and deliberate failures

| WP | Production tests and assertions | Runnable mutations which must fail |
|---|---|---|
| WP1 | Existing `alweathermodel_test.cpp` unchanged cases plus pure `SLWeather` config/time bridge: amount↔rate endpoints/monotonicity, finite clamps/invalid config, local/none/master source rules, exponential 95%-settled transition semantics, vector wind wrap, deterministic bands/strike slots, reset/place serials, no source snow/runoff side effects when their consumers are off. GL execute old rain chain with new bridge off and library uniform uploads from production helper; light/medium/heavy rate response monotonic under fixed art settings, old presets byte-identical. Test two-scheduler exclusion and one event per strike. | Enable library adapter in default mode; double cloud/gust modulation; read wall time per view; leave both lightning schedulers active; remove master clear; drop the actual uniform upload/call site. |
| WP2 | Rasterize roof, sloped roof, terrain, vertical wall, masked canopy and doorway; inspect actual exposure/material output and PBR/legacy lighting. Test rain-start soak, rain-stop dry, puddle coverage stability, ripple range filtering, post-toggle exact restoration, emissive/HUD/water exclusion, forward alpha consistency, probe material response and stationary exposure under camera motion. Baseline native sheen remains exact when new gates off. | Replace roughness with 0; add water to emission; apply wetness in sRGB; use camera shelter for all surfaces; force map valid with wrong matrix; omit legacy gloss conversion; leave additive wet sheen active; disconnect helper from indexed/terrain variant. |
| WP3 | Real depth occluders/soft edges with fixed flake IDs: silhouettes occlude, rates/caps stay bounded, wind follows weather clock, no snow-to-rain leakage. Compile all instanced/compute/mixed/Prism permutations, test odd and 4K framebuffer sizes, camera cell crossing, unsupported-capability status and zero alloc while off. Veil extinction is monotonic, ≥0, physically scaled and applied once only to the relevant open depth. | Disable depth test; seed from view/frame count; omit a compute→draw barrier; exceed cap by one workgroup; clip flakes to main camera for a remote VCam; apply veil twice; let Snow preset enable relief. |
| WP4 | CPU snow depth: 5 cm/h for 600 s with 0.3 settlement gives 0.005833… m before cap/melt; test cover cap, mixed rain melt, no water during snowfall, and correct unit conversions. GL persistent grids rebase and scroll without resetting in-window tracks; geometry/depth/shadow joins agree; footprint locations match production contact matrices, object rows survive slot reorder but not deletion; old-place ground patterns never survive a teleport. Covered/stacked fixtures avoid upside-down deposits. All extra maps release on master off. | Treat cm/h as m/s; key grid to camera pixels; clear it on each pan; stamp a contact once per rendered view; reuse a removed object's row; omit snow geometry from shadows; convert melt to water twice; erase a track when only map origin shifts. |
| WP5 | Pure mixer tests with fake engine/clock: equal-power group weights, bounded gain/voices, specified fades, loaded-neighbor fallback, mute/ambient multiplication, thunder distance delay/serial dedupe, deterministic stealing, wind ownership restoration, no-hail state. Decode budget counts in-flight and engine copies; stale jobs after reset/shutdown/device loss are discarded safely. Backend smoke playback/loop wrap/seek/listener follows native engine; verify packaged assets/provenance and listen to captured fades/thunder. GL side verifies cover readback ring does not block; audio needs no new shader to pass. | Schedule a thunder twice from two cameras; mix procedural wind simultaneously; use wall time for take variants; omit ambient mute; free buffer before async completion; accept stale generation; do synchronous readback; double-allocate PCM beyond cap. |
| WP6 | CPU parser/formatter/editDescription properties: first tag/case, absent/missing `]`, bogus keys, malformed/NaN/Inf/overflow numbers, locale independence, decimal/hex seeds, all clamps, `off` precedence, rate-over-amount, UTF-8 length≤255, unrelated description bytes preserved, normalized round-trip. Test positive/negative/held depth and missing `since`, rate-only tags, stale agent parcel, crossing timeout, late edits and EEP fallback. Clock/take tests exact fixed-tick traces across frame partitions, source hold, backward seek preparation, cues/transport order, pause/restart/audio sample offset, reloaded recipe validation. Real GL repeats old rain + new effects at matched times, two-pass auxiliary ordering, and pre-roll initialization; live/default identity retained. | Use `getTotalTime`/frame number as seed; silently clamp integration dt and skip remaining ticks; let parcel change edit prepared state; accept previous region's parcel; reset loop selection from load order; serialize transient playhead as a look; accept NaN or overflow timestamp; count characters instead of UTF-8 bytes. |
| WP7 | GPU-rendered near/far vertical and tilted maps, native masked roofs and moving blockers. Known directional rain reaches windward walls and is blocked leeward; near/far seams and mean-wind changes stay continuous; snow samples correct vertical height. Verify map generation/matrix/coverage stamps, origin rebase and allocation failure; main/probe/Prism/VCam state-save/restore sentinel draws after every early exit. Listener ring uses only signaled results with acceptable age. | Reverse shear sign; read tilted depth as vertical ground; use last view's matrix; stamp valid after failed allocation; remove one publish barrier; publish map and matrix from different generations; leak GL_BLEND or draw-buffer state; wait on current-frame listener fence. |
| WP8 | Small known roof/valley fixtures with deterministic flow/outlets and landing heights; bounded runoff≤wetness, no initial dry-roof streams, rain-stop drainage, snow-melt feeding, no extra energy in rivulets, indoor drip-line wetness only at valid landing. Test zero slope, near-zero flow, new roof, changing camera/flow grid, overflow thinning and 15 Hz updates across display FPS. Compare cached asynchronous listener feedback with allowed latency. | Remove flow clamps; create outlets on dry roof; reseed emitters every frame; cap by nondeterministic append order; keep runoff forever; allow every sheltered floor to wet; use rivulet value as emission; force GPU readback wait. |
| WP9 | Synthetic skinned fixture using production skin/rest transforms and rigid worn canopy, plus in-world local/cast/nearby/avatar/animesh/outfit cases. Coat rides shoulders through animation/rotation and respects hat/umbrella masks; shader inventory covers opaque, PBR, legacy, forward, masked and first-person visible attachments. Identity/slot tests handle eviction, grace expiry, deletion, outfit replacement, teleport, off toggles, clone/instance separation and take reset. Default-cap cost and atlas bytes obey budget; no new avatar silhouette means original shadow silhouette remains exact. | Index coating by transient draw slot; use world-space noise for moving skin; fail to clear a reused layer; ignore worn attachment mask; convert normal with position matrix; mutate atlas from a probe/shadow draw; leave old mesh rest coordinates after outfit change. |

### Director, buildability, and live acceptance for each WP

Every WP includes named, measured kit rows, tooltips, units/limits, prerequisites/status, keyword search, preset tracking/version migration, Reset All and off behavior. Functional commands use the shared weather model so older UI entry points, if retained, cannot diverge. Event/revision-driven UI uses cached widgets/settings and the existing diff-guarded UI writers; no disk/decode/poll/map work, lookups, or rebuilding in `draw()`.

Future implementation checks (listed here, **not executed by this planning task**):

```powershell
$env:PYTHONUTF8 = '1'
python scripts/code_tools/director_kit_gen.py --check
python scripts/code_tools/director_page_sources.py --check
python scripts/code_tools/director_theme_check.py --check
python scripts/code_tools/director_functional_manifest.py --check
python scripts/code_tools/xui_layout_check.py indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml
python scripts/code_tools/kit_lint.py --all
python scripts/code_tools/ui_draw_lint.py --changed 330d012e1612a3f4779a01dcc0614a91939ead0f
```

If other generated registries/contracts are touched, run their generators' `--check` as well. The current `director_parity.py` accepts only `--groups light_group` for its P0 gate; it explicitly rejects `world_group`. Do not prescribe that unsupported command. Use ownership/functional-manifest checks plus Weather binding/search tests for this page; a future World-specific parity extension would need its own implementation and fixtures. Require **xui_layout_check 0 findings**, **kit_lint 0 errors**, **ui_draw_lint 0 new findings**; do not waive new findings or rewrite a baseline to silence them. Run layout checks on every changed XUI file, at supported panel widths/themes/scales; verify scrollability, labels/units, disabled explanations, hotkey/focus behavior, every row through search, and no raw Debug Settings dependency. Generator/check commands must not change source during verification.

Build/test each WP through the repository's normal Windows viewer/test configuration **only in the subsequent authorized implementation**, with a reviewer checking shared GL state and default/off paths before a viewer build. The current brief forbids builds, source edits, pushes, HEAD movement and other-worktree mutations; this plan does not authorize any of them. Each future phase must stand alone with no unlanded source, shader, CMake or asset dependency. Finish the implemented phase's live acceptance before starting a more costly renderer phase.

Live acceptance uses a saved scene/camera route, fixed sky/exposure choices, 4K capture-card output, target FPS, audio listener and sound mix, then two prepared reshoots. Walk outdoor→doorway→interior, pan across roof edges/reflective pavement/snow, move marked actors, switch the recording camera/Prism feed, stop precipitation, and toggle master off. Inspect continuous recordings for stutter, sparkle, pop, flashes, sound clicks, and whether effects match status. Also run a default rain recording against the pre-port viewer. Report what is measured, what the harness cannot prove, and what still needs human visual/audio acceptance.

## 5. Licence, attribution, and copy/reimplementation inventory

### Licence and provenance handling

The inspected CPU files identify **Slipstream Viewer Source Code** and **LGPL version 2.1 only** (`viewerlgpl` markers; some CPP/H headers refer to their partner header for full text). Preserve the complete existing copyright/licence headers, original authorship, and existing notices when copying; add a short Alchemy adaptation note and change description without replacing them. New derived shader/math implementations retain appropriate source credit and licence notices as applicable; “reimplement” is an engineering decision, not permission to strip attribution. This inventory follows the supplied local notices; it is not a claim that uninspected external assets have the same terms.

In the implementing WP1 add a credit to our existing `doc/contributions.txt`, for example: **Slipstream Viewer contributors — weather state/source protocol, precipitation/wind model, weather audio, and reference designs for wet surfaces, snow, shelter and runoff; adapted for Alchemy Machinima's OpenGL renderer.** Include exact upstream project/revision information available at import and a `doc/weather/UPSTREAM_FILES.md` table of source paths, hashes, destination paths, licence and modifications. Record any file with a different/absent notice individually before importing it; do not silently assume all shaders inherit a CPU header.

For sounds, the inspected local `LICENSE.txt` says the pack derives from **CC0 1.0** Freesound recordings. `SOURCES.md` records author, Freesound page/ID, check date and source hash; `manifest.json` records generated files, processing, 48 kHz format, loop points, gains and loudness/peak targets. Preserve all three byte-for-byte beside copied assets. Copy final OGGs, not private originals/download caches; no re-fetch is required for this plan. Verify final-file hashes and manifest references at import and in packaging, retain courtesy credits, and separately describe any later edits/re-encoding. The original uploaders' CC0 assertions/check dates are provenance, not an independent legal guarantee. No license-checking web request or asset publication is part of this planning task.

### Files to copy with headers/notices, then narrowly adapt

| Source under `I:/alchemy-machinima/slipstream/` | Destination / phase | What changes |
|---|---|---|
| `indra/newview/slweather.cpp`, `slweather.h` | Same relative paths; WP1 | Settings configuration adapter, injected frame/epoch, no Vulkan call ownership, native movement/reset events. Preserve rate/integration/wind/strike formulas; isolate tests. |
| `indra/newview/slweatherregion.cpp`, `slweatherregion.h` | Same relative paths; WP6 | Host setting names and status/events; preserve parser/formatter/protocol; no automatic publishing. |
| `indra/newview/slweatheraudio.cpp`, `slweatheraudio.h` | Same relative paths; WP5 | Extract pure mixer; native local-audio/decoder/listener adapter; remove RT-acoustic dependency; WP6 seeded variant/playhead/cue clock corrections. Retain headers on extracted portions too. |
| `doc/weather_region_protocol.md` | `doc/weather/weather_region_protocol.md`; WP6 | Keep upstream description/credit; append an Alchemy compatibility section and distinguish synchronized values from local presentation. |
| Rate ladders and tag-validation portions of `slfloaterweather.cpp/.h` | Selected functions into `alweatheruimodel` / `ALPanelWeatherSettings`; WP1/WP6 | Preserve attribution on copied fragments. Do not copy floater class, LLFloater registration or runtime draw/status polling. |
| Weather-only entries from `indra/newview/app_settings/settings.xml` | Reviewed definitions in our settings file; each owning WP | Prefix/mapping/defaults/ranges in section 3; never copy the entire settings file or introduce source's second master and costly defaults. |
| `indra/newview/app_settings/sounds/weather/LICENSE.txt`, `SOURCES.md`, `manifest.json` | Same relative paths; WP5 | Exact provenance copies; record selected/excluded final files in port inventory. |
| Selected final sound files listed below | Same relative paths; WP5 | Copy unchanged, ensure manifest packaging and hash verification. |

**Initial shipped sound selection:** `rain_light_loop.ogg`, `rain_medium_loop.ogg`, `rain_heavy_loop.ogg`; `roof_wood_loop.ogg`, `roof_wood_heavy_loop.ogg`, `roof_metal_loop.ogg`, `roof_metal_heavy_loop.ogg`, `roof_glass_loop.ogg`, `roof_tent_loop.ogg`; `drip_loop.ogg` and `drip_01.ogg` through `drip_08.ogg`; `wind_light_loop.ogg`, `wind_gust_loop.ogg`, `wind_howl_loop.ogg`, `wind_indoor_loop.ogg`, `snow_wind_muffled_loop.ogg`; `snow_ambience_loop.ogg`, `snow_interior_loop.ogg`; `thunder_close_01.ogg` through `thunder_close_05.ogg`, `thunder_mid_01.ogg` through `thunder_mid_06.ogg`, `thunder_distant_01.ogg` through `thunder_distant_08.ogg`.

Keep the full upstream manifest/provenance unchanged even though this first selection omits some listed files; add a separate Alchemy shipped-file list and make packaging/runtime validate **that selection**, not demand every upstream entry. Exclude initially: `hail_*`, `snow_step_*`, `rain_foliage*`, `rain_water_loop.ogg`, `wind_whistle*`, `wind_howl_window_loop.ogg`, and sound-pack fetching/build scripts. Those need state/material/contact/opening support beyond this plan's mixer; do not present silent controls for them. If a retained mixer path references an excluded file, remove/disable that path or expand the reviewed shipped list before release.

### Reference-only files to reimplement, not copy as renderer code

- `indra/newview/vkgfx/llcleanweathervk.cpp`, `.h`, `_movers.cpp`, `_accum.cpp`, `_runoff.cpp`, `_shelter.cpp`: behavior, lifetime, data-flow and budget reference only. Reimplement native GL state/resources, scene contacts, fields, maps, atlas and draw ownership in the targets of section 2.
- `indra/vkclean/shaders/clean_weather_common.glsl`, `clean_weather_sim.comp`, `clean_weather.vert`, `clean_weather.frag`: reimplement new snow/drip rendering. Preserve our current rain shaders and defaults.
- `clean_weather_surface.glsl`, `clean_weather_snow_relief.glsl`, `clean_weather_snow_join.glsl`, `clean_weather_snow.comp`, `clean_weather_snowmesh.comp/.vert/.frag`: reimplement material response, world fields, seams and optional geometry for the native G-buffer/forward/shadow contracts.
- `clean_weather_shelter.comp/.glsl`, `clean_weather_accum.comp/.glsl`, `clean_weather_runoff.comp`: reimplement avatar shelter/coating and flow in GL; no Vulkan set/binding/push-constant ABI is copied.
- `clean_weather_veil.glsl/.frag`: limited optional native visibility implementation. `clean_vol_cloud_weather.comp`, `clean_weather_cluster.glsl`, `clean_rt_weather.glsl`: no direct port; volumetric-cloud/cluster/RT integration is skipped.
- `skins/default/xui/en/floater_sl_weather.xml`: UX reference only; use Director kit theme, measured rows and search metadata. Do not add a separate Slipstream Weather window or toolbar dependency.

### Open decisions for the user after workflow review

1. Confirm the proposed order: controllable rain first, then wet materials/puddles before snowfall; or prioritize snowfall ahead of wet materials while retaining shared resource/shelter groundwork.
2. Confirm the primary live target: 4K60 or 4K30, the recording GPU, and whether a simultaneous full-resolution Prism/VCam feed is normally required. These determine the final measured budgets; the proposed numbers are not benchmarks.
3. Confirm repeatable takes starting from explicit wetness/snow depth with empty tracks and a fixed pre-roll. Capturing an arbitrary existing footprint field would require an additional versioned GPU checkpoint design and memory budget.
4. Confirm Own avatar as the initial coating scope and whether Director cast should be the next priority. Keep nearby crowds an explicit capped option.
5. Confirm copied parcel tags through About Land as the initial publishing workflow. The plan includes compatible reading/sharing without an automatic parcel-description write button.

Section 1 must be shown and confirmed before implementation, as required by `PLAN_BRIEF.txt`. This plan leaves all code and feature choices unimplemented pending that review.

## Lead decisions (10/25, user delegated: "make the plan solid, then let Codex build it, normal review rounds, and when it's ready build it")

The user delegated workflow confirmation to the lead. Section 1 is CONFIRMED as written, with these answers:

1. Order: as proposed. WP1 controllable rain core, WP2 persistent wet surfaces/puddles, then WP3 snowfall, and so on.
2. Live target: native 3840x2160 on the RTX 5090 32 GB, aiming for 60 FPS where the scene allows. A simultaneous Prism/VCam feed is sometimes used: report the second view's cost separately, as the plan says. Keep the proposed budgets as acceptance ceilings.
3. Repeatable takes: explicit starting wetness/snow depth, empty tracks, fixed pre-roll. Sufficient for v1. No GPU footprint checkpoint.
4. Avatar coating scope: own avatar first (default); Director cast through the standard Director who-picker is the next step inside WP9 if budget allows; nearby crowds stay an explicit capped option.
5. Parcel weather: read-only parsing of the parcel description plus a "Copy tag" text the user pastes into About Land themselves. The viewer never writes parcel data.
6. Execution: phases are implemented in order, each in its own commit series on wip/weather-port, each with its real-GL shader tests and CPU tests, then a Codex adversarial review round; P0 or core-breaking P1 findings are fixed before the next phase starts. Phases are merged to main only when reviewed clean. One viewer build at the end of the batch.
7. Snapshots are not a priority (external capture only): no snapshot-specific work anywhere.
