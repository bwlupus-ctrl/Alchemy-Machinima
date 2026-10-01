# Actor Mask UI note (WP6)

Surface for the Cine Light Rig "Actor only" feature (brief: CINE_RIG_ACTOR_MASK_BRIEF_V3.md, section 6.3).
All controls are `control_name`-bound to settings that already exist (WP5); no new settings.

## Cine Light Rig panel (`panel_cine_light_rig.xml`, also embedded in the Director)

| Control | Setting | Where |
|---|---|---|
| `cine_{key,fill,rim,bg}_actor_only` + `_reset` | `CineLightRig{Key,Fill,Rim,Bg}ActorOnly` | new row at top=237 of each light card (the Shaft/Hero row is full); card height 239 -> 263 |
| `cine_bounce_lights_set` + `_reset` | `CineLightRigBounceLightsSet` | rig card, top=248; card height 252 -> 298 |
| `cine_catchlight_actor_only` + `_reset` | `CineLightRigCatchlightActorOnly` | rig card, top=272 |

Object-target rigs: `ALPanelCineLightRig::syncActorOnlyControls()` (called from `updateDerivedStatus()`)
disables all six checkboxes and their reset buttons and swaps in an explanatory tooltip. The saved values are
not touched, so clearing the object target brings the user's choices back. The XML tooltip is captured at
`postBuild` as the enabled-state text. Reset all and presets already cover the six rig keys through
`ALPanelCineLightRig::settings()` (WP5); nothing is added to llpresetsmanager.cpp.

## Lightbox "Actor Mask" tab (`floater_lightbox_settings.xml`, tab `actor_mask_settings`)

| Control | Setting |
|---|---|
| `am_disable_check` | `CineLightRigActorMaskDisable` (kill switch) |
| `am_debug_combo` Off / Overlay / Mask only | `CineLightRigActorMaskDebug` (0 / 1 / 2, not persisted) |
| `am_bias_slider` (-64..0, step 0.5) + `am_reset_CineLightRigActorMaskBias` | `CineLightRigActorMaskBias` (default -8) |
| `am_test_combo` Idle / Run test / Cancel | `CineLightRigActorMaskTest` (0 / 1 / 2; the renderer resets it to 0 itself) |
| `am_limits_text` | the known limits of brief section 8, in plain language |

The isolation-test trigger is a combo bound to the self-resetting U32 rather than a button, because a
button would need a new commit callback outside the files WP6 may edit.
