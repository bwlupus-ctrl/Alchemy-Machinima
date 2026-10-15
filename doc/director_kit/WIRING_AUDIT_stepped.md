# Stepped Animation value-wiring audit

Audit basis: the 49 authoritative rows in `director_mockup_data.json`. Setting rows use typed writes and signal read-back in `alkitbinding.cpp:12-57,70-89`; in particular, segmented `LLRadioGroup` payloads are converted to integer at `alkitbinding.cpp:20-22`. Global consumers use cached controls at `alsteppedanimruntime.cpp:96-126` and one sanitize pass at `alsteppedanimruntime.cpp:139`. "Bool" means the widget and consumer are both Boolean. All rows pass.

| Control | Setting / type | Widget values or range | Consumer clamp / enum (exact source) | Handler and read-back | Result |
|---|---|---|---|---|---|
| `sa_scope` | `StepAnimScope` / S32 | 0 Selected, 1 Cast, 2 View | 0..2, `alsteppedanim.cpp:72` | typed setting binding; `alkitbinding.cpp:70-89` | PASS |
| `sa_include_ghosts` | `StepAnimIncludeGhosts` / Boolean, Persist 1, default 0 | false / true | `Config::includeGhosts`, `ActorFacts::ghostOwnAnimation`; `resolveActor` extends scope after exclusions; `ghostMayInherit` preserves independent assignment; cap still applies | typed setting binding/signal read-back; cached `readConfig`; mirror/copy source inheritance unchanged; excluded from presets | PASS |
| `sa_actor_target` | code / UUID | selected cast actor, cast members, You | canonical cast member or null/You, `lldirectorcast.cpp:1279-1292` | commit `alpanelsteppedanim.cpp:160-164`; read `:91-95,130-139` | PASS |
| `sa_override_list` | code / UUID list | current override roster | cast styles only | select `alpanelsteppedanim.cpp:166-175`; rebuilt/read `:89-128` | PASS |
| `sa_actor_override` | code / S32 | 0 Inherit, 1 Never, 2 Own | 0..2, `alpanelsteppedanim.cpp:151` | commit `:177`; read `:134` | PASS |
| `sa_actor_rate` | code / F32 | 2..30, step .5 | 2..30, `alpanelsteppedanim.cpp:152` | commit `:178`; read `:135` | PASS |
| `sa_actor_steps` | code / S32 | -1 Inherit, 0 Pose, 1 Pose+position | -1..1, `alpanelsteppedanim.cpp:153` | commit `:179`; read `:136` | PASS |
| `sa_actor_role` | code / S32 | 0 Auto, 1 Target, 2 Background | 0..2, `alpanelsteppedanim.cpp:154` | commit `:180`; read `:137` | PASS |
| `sa_override_clear` | code / action | clear selected override | restores struct defaults, `alpanelsteppedanim.cpp:182-189` | same handler; actor refresh `:189` | PASS |
| `sa_exclude_mounted` | `StepAnimExcludeCameraMounted` / Boolean | Bool | Bool; exclusion consumed `alsteppedanim.cpp:145-146` | typed setting binding/read-back | PASS |
| `sa_max_avatars` | `StepAnimMaxAvatars` / S32 | 1..100, step 1 | 1..100, `alsteppedanim.cpp:73` | typed setting binding/read-back | PASS |
| `sa_status` | code / readout | active and cap-skipped actors | runtime counters | `alpanelsteppedanim.cpp:225-233`; guarded text read-back | PASS |
| `sa_mode` | `StepAnimMode` / S32 segmented | 0 Fixed, 1 Adaptive, 2 Mixed | 0..2, `alsteppedanim.cpp:74` | integer payload + typed setting read-back; conditions `alpanelsteppedanim.cpp:206-213` | PASS |
| `sa_rate_chips` | code / S32 segmented | divisors 1, 2, 3, 4 | writes BaseFPS/divisor, `alpanelsteppedanim.cpp:192-203` | highlight tolerance .001, `:239-245` | PASS |
| `sa_rate` | `StepAnimRate` / F32 | 2..30, step .5 | 2..30, `alsteppedanim.cpp:75` | typed setting binding/read-back | PASS |
| `sa_rate_readout` | code / readout | fps | sanitized rate | `alpanelsteppedanim.cpp:236-251`; change-guarded read-back | PASS |
| `sa_fast_rate` | `StepAnimFastRate` / F32 | 0..30, step 1 | below 2 = smooth, else 2..30, `alsteppedanim.cpp:56-60,76` | typed setting binding/read-back | PASS |
| `sa_adapt_threshold` | `StepAnimAdaptThreshold` / F32 | .1..10, step .1 | .1..10, `alsteppedanim.cpp:77` | typed setting binding/read-back | PASS |
| `sa_adapt_hysteresis` | `StepAnimAdaptHysteresis` / F32 | .05...9, step .05 | .05...9, `alsteppedanim.cpp:78` | typed setting binding/read-back | PASS |
| `sa_adapt_dwell` | `StepAnimAdaptMinDwell` / F32 | .05..2, step .05 | .05..2 plus one-grid-step floor, `alsteppedanim.cpp:63-66,79` | typed setting binding/read-back | PASS |
| `sa_speed_meter` | code / readout | m/s | windowed limb speed | `alpanelsteppedanim.cpp:254-259`; runtime read-back | PASS |
| `sa_background_rate` | `StepAnimBackgroundRate` / F32 | 0..30, step 1 | below 2 = smooth, else 2..30, `alsteppedanim.cpp:56-60,80` | typed setting binding/read-back | PASS |
| `sa_what` | `StepAnimWhatSteps` / S32 segmented | 0 Pose, 1 Pose+position | 0..1, `alsteppedanim.cpp:81` | integer payload + typed setting read-back | PASS |
| `sa_face` | `StepAnimFace` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:107,132` | typed setting binding/read-back | PASS |
| `sa_gaze` | `StepAnimGaze` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:108,132` | typed setting binding/read-back | PASS |
| `sa_flexi` | `StepAnimFlexi` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:109,132` | typed setting binding/read-back | PASS |
| `sa_attach_fx` | `StepAnimAttachFX` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:110,132` | typed setting binding/read-back | PASS |
| `sa_particles` | `StepAnimParticles` / Boolean | Bool | runtime particle hold | typed setting binding/read-back; independent of presets | PASS |
| `sa_texture_anim` | `StepAnimTextureAnim` / Boolean | Bool | runtime texture-animation hold | typed setting binding/read-back; independent of presets | PASS |
| `sa_clock` | `StepAnimClock` / S32 | 0 Capture, 1 Real | 0..1, `alsteppedanim.cpp:82` | typed setting binding/read-back | PASS |
| `sa_offset` | `StepAnimOffsetMode` / S32 | 0 Random, 1 Locked, 2 Manual | 0..2, `alsteppedanim.cpp:83` | typed setting binding/read-back | PASS |
| `sa_phase` | `StepAnimPhase` / F32 | 0..1, step .05 | 0..1 then 1 -> 0, `alsteppedanim.cpp:84-85` | typed setting binding/read-back | PASS |
| `sa_base_fps` | `StepAnimBaseFPS` / S32 | payloads 24, 25, 30 | maps to 24/25/30, `alsteppedanim.cpp:86` | typed combo payload/read-back | PASS |
| `sa_boil` | `StepAnimBoil` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:115,134` | typed setting binding/read-back | PASS |
| `sa_boil_amount` | `StepAnimBoilAmount` / F32 | 0..3, step .05 | 0..3, `alsteppedanim.cpp:87` | typed setting; enabled by `StepAnimBoil` | PASS |
| `sa_boil_ticks` | `StepAnimBoilTicks` / S32 | 1..8, step 1 | 1..8, `alsteppedanim.cpp:88` | typed setting; enabled by `StepAnimBoil` | PASS |
| `sa_impact` | `StepAnimImpactHold` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:118,135` | typed setting binding/read-back | PASS |
| `sa_impact_ticks` | `StepAnimImpactTicks` / S32 | 1..12, step 1 | 1..12, `alsteppedanim.cpp:89` | typed setting; enabled by `StepAnimImpactHold` | PASS |
| `sa_impact_threshold` | `StepAnimImpactThreshold` / F32 | 5..200, step 1 | 5..200, `alsteppedanim.cpp:90` | typed setting; enabled by `StepAnimImpactHold` | PASS |
| `sa_smear` | `StepAnimSmear` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:121,136` | typed setting binding/read-back | PASS |
| `sa_smear_strength` | `StepAnimSmearStrength` / F32 | 0..1, step .05 | 0..1, `alsteppedanim.cpp:91` | typed setting; enabled by `StepAnimSmear` | PASS |
| `sa_smear_threshold` | `StepAnimSmearThreshold` / F32 | .5..10, step .1 | .5..10, `alsteppedanim.cpp:92` | typed setting; enabled by `StepAnimSmear` | PASS |
| `sa_ease` | `StepAnimEase` / Boolean | Bool | Bool, cached `alsteppedanimruntime.cpp:124,137` | typed setting binding/read-back | PASS |
| `sa_ease_amount` | `StepAnimEaseAmount` / F32 | 0..1, step .05 | 0..1, `alsteppedanim.cpp:93` | typed setting; enabled by `StepAnimEase` | PASS |
| `sa_match_roto` | `StepAnimMatchRotoInk` / Boolean | Bool | Bool; mapping consumer `alsteppedanimruntime.cpp:680-685` | typed setting binding/read-back | PASS |
| `sa_roto_link` | code / link | Look/Roto Ink target | kit navigation | action/read-back `alkitrow.cpp:200-206` | PASS |
| `sa_tron_link` | code / link | Look/Tron target | kit navigation | action/read-back `alkitrow.cpp:200-206` | PASS |
| `sa_actorfx_link` | code / link | Look/Actor FX target | kit navigation | action/read-back `alkitrow.cpp:200-206` | PASS |
| `sa_time_link` | code / link | `capture_group/temporal_tab` target | kit navigation | action/read-back `alkitrow.cpp:200-206` | PASS |

Result: **49/49 PASS; no value-wiring failures remain.** The page has 34 setting-bound rows (29 preset-owned, three independent toggles, the safety exclusion and actor cap); its master is `StepAnimEnabled` and is outside the 49 body rows.
