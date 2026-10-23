#!/usr/bin/env python3
"""Build the Director Kit page/source evidence index.

The index is intentionally derived only from the approved mockup data and the
current XUI, C++, and settings trees.  It does not import rollout prose.
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "scripts/code_tools/director_kit/director_mockup_data.json"
XUI_DIR = ROOT / "indra/newview/skins/default/xui/en"
CPP_DIRS = (ROOT / "indra/newview", ROOT / "indra/llui")
SETTINGS_DIR = ROOT / "indra/newview/app_settings"
SETTINGS_FILES = SETTINGS_DIR / "settings_files.xml"
JSON_OUT = ROOT / "doc/director_kit/page_sources.json"
MD_OUT = ROOT / "doc/director_kit/page_sources.md"
PAGES_DIR = ROOT / "doc/director_kit/pages"

NAME_RE = re.compile(r"\bname\s*=\s*(['\"])(.*?)\1")
STRING_RE = re.compile(r'"((?:\\.|[^"\\])*)"')
METHOD_RE = re.compile(
    r"^\s*(?P<prefix>(?:(?:[A-Za-z_]\w*|[:<>,~*&]+)\s+)*)"
    r"(?P<class>[A-Za-z_]\w*)::(?P<function>~?[A-Za-z_]\w*)\s*\("
)

# A bare widget name is not ownership evidence.  These are the XUI roots that
# actually host each v1 page (or the standalone owner for launcher pages).
# C++ references are accepted only from the classes which own/build those
# roots.  Same-name hits outside this scope are retained as ambiguity evidence.
PAGE_OWNER_FILES = {
    "move_tab": {"panel_actor_mover.xml", "floater_director.xml"},
    "path_tab": {"panel_path_editor.xml"},
    "props_tab": {"panel_prop_mover.xml"},
    "ghosts_tab": {"panel_ghost_studio.xml"},
    "dance_tab": {"panel_dance_ball.xml", "floater_director.xml"},
    "animate_tab": {"floater_director.xml", "panel_anim_preview.xml"},
    "switchboard_tab": {"panel_director_anim_switcher.xml"},
    "gaze_tab": {"panel_lens_gaze.xml", "floater_director.xml"},
    "polish_tab": {"panel_pose_polish.xml", "floater_director.xml"},
    "gaze_cues_tab": {"floater_gaze_cues.xml"},
    "shot_tab": {"panel_director_switcher.xml", "panel_cinecam_params.xml", "floater_director.xml"},
    "moves_tab": {"floater_director.xml"},
    "operator_tab": {"panel_cinecam_operator.xml", "panel_cinecam_params.xml"},
    "frame_orbit_tab": {"panel_cinecam_frame.xml", "panel_flycam_orbit.xml", "panel_cinecam_params.xml"},
    "takes_tab": {"panel_flycam_recorder.xml"},
    "vcam_tab": {"floater_director.xml", "floater_prism_manager.xml"},
    "focus_tab": {"panel_dof.xml", "floater_phototools.xml"},
    "filters_tab": {"panel_lens_looks.xml"},
    "diopter_tab": {"floater_ultimate_diopter.xml"},
    "cine_light_rig_tab": {"panel_cine_light_rig.xml"},
    "light_rim_tab": {"panel_cine_light_rig.xml"},
    "exposure_tab": {"panel_exposure_hub.xml", "floater_director.xml"},
    "practicals_tab": {"floater_lightbox_settings.xml"},
    "actors_tab": {"panel_avatar_light_response.xml", "floater_lightbox_settings.xml", "floater_director.xml"},
    "light_cues_tab": {"floater_cine_light_cues.xml"},
    "image_tab": {"floater_lightbox_settings.xml"},
    "bloom_glow_tab": {"floater_lightbox_settings.xml"},
    "stylize_tab": {"floater_lightbox_settings.xml"},
    "tron_tab": {"floater_lightbox_settings.xml"},
    "actor_style_tab": {"floater_director.xml"},
    "stepped_tab": {"panel_dpage_look_stepped.xml"},
    "weather_tab": {"panel_weather_settings.xml"},
    "atmosphere_tab": {"panel_atmospheric_volumes.xml", "panel_projector_shafts.xml", "floater_lightbox_settings.xml"},
    "shadows_probes_tab": {"floater_lightbox_settings.xml"},
    "temporal_tab": {"panel_temporal_capture.xml"},
    "scene_options_tab": {"floater_director.xml"},
    "director_settings_tab": {"panel_dpage_settings_director_settings.xml"},
}

PAGE_OWNER_CLASSES = {
    "move_tab": {"ALPanelActorMover", "LLFloaterDirector"},
    "path_tab": {"ALPanelPathEditor"},
    "props_tab": {"ALPanelPropMover"},
    "ghosts_tab": {"ALPanelGhostStudio"},
    "dance_tab": {"ALPanelDanceBall", "LLFloaterDirector"},
    "animate_tab": {"ALPanelAnimPreview", "LLFloaterDirector"},
    "switchboard_tab": {"ALPanelDirectorAnimSwitcher"},
    "gaze_tab": {"ALPanelLensGaze", "LLFloaterDirector"},
    "polish_tab": {"ALPanelPosePolish", "LLFloaterDirector"},
    "gaze_cues_tab": {"ALFloaterGazeCues"},
    "shot_tab": {"ALPanelDirectorSwitcher", "ALPanelCineCamParams", "LLFloaterDirector"},
    "moves_tab": {"LLFloaterDirector"},
    "operator_tab": {"ALPanelCineCamParams"},
    "frame_orbit_tab": {"ALCineCamFramePanel", "ALPanelFlycamOrbit", "ALPanelCineCamParams"},
    "takes_tab": {"ALPanelFlycamRecorder"},
    "vcam_tab": {"LLFloaterDirector", "ALFloaterPrismManager"},
    "focus_tab": {"ALPanelDoF", "LLFloaterPhototools"},
    "filters_tab": {"ALPanelLensLooks"},
    "diopter_tab": {"ALFloaterUltimateDiopter"},
    "cine_light_rig_tab": {"ALPanelCineLightRig"},
    "light_rim_tab": {"ALPanelCineLightRig", "ALPanelRigRim", "ALRigRimController"},
    "exposure_tab": {"ALPanelExposureHub", "LLFloaterDirector"},
    "practicals_tab": {"ALFloaterLightBox"},
    "actors_tab": {"ALPanelAvatarLightResponse", "ALFloaterLightBox", "LLFloaterDirector"},
    "light_cues_tab": {"ALFloaterCineLightCues"},
    "image_tab": {"ALFloaterLightBox"},
    "bloom_glow_tab": {"ALFloaterLightBox"},
    "stylize_tab": {"ALFloaterLightBox"},
    "tron_tab": {"ALFloaterLightBox"},
    "actor_style_tab": {"LLFloaterDirector"},
    "stepped_tab": {"ALPanelSteppedAnim", "LLFloaterDirector"},
    "weather_tab": {"ALPanelWeatherSettings"},
    "atmosphere_tab": {"ALPanelAtmosphericVolumes", "ALPanelProjectorShafts", "ALFloaterLightBox"},
    "shadows_probes_tab": {"ALFloaterLightBox"},
    "temporal_tab": {"ALPanelTemporalCapture"},
    "scene_options_tab": {"LLFloaterDirector"},
    "director_settings_tab": {"LLFloaterDirector"},
}

WIDGET_NAME_OVERRIDES = {
    ("light_cues_tab", "cue_timecode_check"): "cue_timecode_mode",
}

# Approved cross-page contract aliases where the mockup row omitted `s`.
# Keep these explicit: a global same-name XUI lookup is not allowed to invent
# the relationship.
SETTING_OVERRIDES = {
    ("props_tab", "end_mode_combo"): "ActorMoverEndMode",
    ("ghosts_tab", "scale_spinner"): "TemporalWorldScale",
}

KIT_ACTION_OVERRIDES = {
    ("actors_tab", "am_test_trigger"): "owner-command:LLFloaterDirector::bindLightActorsPage",
    ("dance_tab", "dance_popout"): "floater:dance_ball",
    ("gaze_tab", "btn_actor_gaze"): "owner-command:LLFloaterDirector::onOpenActorGaze",
    ("gaze_tab", "btn_pose_polish"): "floater:pose_polish",
    ("actors_tab", "light_response_popout"): "floater:avatar_light_response:selected-actor",
    ("gaze_cues_tab", "gaze_cue_play"): "engine-work:WP8:LLActorMover::setGazeCuePreview",
    ("gaze_cues_tab", "gaze_cue_stop"): "engine-work:WP8:LLActorMover::clearGazeCuePreview",
}


def model_binding_override(page: str, widget: str) -> dict | None:
    def binding(file_name: str, line: int, class_name: str, function: str, pattern: str | None = None) -> dict:
        if class_name in ("ALPanelGhostStudio", "LLPipeline"):
            source = (ROOT / file_name).read_text(encoding="utf-8")
            match = re.search(r"(?m)^.*\b" + re.escape(class_name + "::" + function) + r"\s*\(", source)
            if match is None:
                raise RuntimeError(f"missing model binding: {class_name}::{function}")
            line = source.count("\n", 0, match.start()) + 1
        return {
            "file": file_name, "line": line, "class": class_name, "function": function,
            "wired_class": class_name, "wired_function": "postBuild",
            "pattern": pattern, "wiring_only": False, "text": "explicit scoped model binding",
        }

    if page == "path_tab" and widget == "copy_to_combo":
        return binding("indra/newview/alpanelpatheditor.cpp", 1536, "ALPanelPathEditor", "refreshCopyCombo")
    if page == "move_tab" and widget == "group_run_combo":
        return binding("indra/newview/llfloaterdirector.cpp", 2107, "LLFloaterDirector", "onClickGroup")
    if page == "props_tab" and widget in {
        "shuttle_sign_combo", "shuttle_distance_spin", "shuttle_speed_spin", "shuttle_slope_spin"
    }:
        return binding("indra/newview/alpanelpropmover.cpp", 593, "ALPanelPropMover", "onClickShuttle")
    if page == "ghosts_tab":
        if widget in {"source_combo", "clone_type_combo"}:
            return binding("indra/newview/alpanelghoststudio.cpp", 2103, "ALPanelGhostStudio", "onClickAdd")
        if widget == "formation_motion_combo":
            return binding("indra/newview/alpanelghoststudio.cpp", 4063, "ALPanelGhostStudio", "onMotionCommit")
        if widget.startswith(("formation_", "brigade_", "array_")) or widget in {"preserve_source_tilt_check", "crowd_copies_as_combo", "crowd_create_mode_combo"}:
            return binding("indra/newview/alpanelghoststudio.cpp", 3641, "ALPanelGhostStudio", "updateOwnedCrowdPlacement")
        if widget in {"strip_count_spinner", "strip_interval_spinner"}:
            return binding("indra/newview/alpanelghoststudio.cpp", 4032, "ALPanelGhostStudio", "onClickStartStrip")
    if page == "dance_tab":
        if widget == "db_dancer_list":
            return binding("indra/newview/alpaneldanceball.cpp", 933, "ALPanelDanceBall", "rebuildDancerList")
        if widget in {"db_block_step", "db_block_keep_facing"}:
            return binding("indra/newview/alpaneldanceball.cpp", 1586, "ALPanelDanceBall", "onBlockingNudgeYaw")
        if widget in {"db_block_shape", "db_block_spacing", "db_block_centre"}:
            return binding("indra/newview/alpaneldanceball.cpp", 1674, "ALPanelDanceBall", "onBlockingFormation")
        if widget in {"db_ramp_target", "db_ramp_seconds"}:
            return binding("indra/newview/alpaneldanceball.cpp", 2043, "ALPanelDanceBall", "onRampGo")
        if re.fullmatch(r"db_block_[fblrud]", widget):
            return binding("indra/newview/alpaneldanceball.cpp", 1654, "ALPanelDanceBall", "onBlockingNudge")
    if page == "animate_tab":
        if widget == "anim_list":
            return binding("indra/newview/llfloaterdirector.cpp", 2992, "LLFloaterDirector", "refreshAnimateTab")
        if widget == "paste_uuid_editor":
            return binding("indra/newview/llfloaterdirector.cpp", 2705, "LLFloaterDirector", "pasteAnimId")
        if widget == "animation_preview":
            return binding("indra/newview/alpanelanimpreview.cpp", 75, "ALPanelAnimPreview", "previewAnim")
        if widget in {"locomotion_speed", "locomotion_phase", "locomotion_allow_ao"}:
            return binding("indra/newview/llfloaterdirector.cpp", 2875, "LLFloaterDirector", "onLocomotionMetadataCommit")
    if page == "switchboard_tab" and re.fullmatch(r"anim_slot_\d{2}", widget):
        return binding("indra/newview/alpaneldirectoranimswitcher.cpp", 59,
                       "ALPanelDirectorAnimSwitcher", "onSlotButton", "anim_slot_%02d")
    if page == "gaze_tab" and widget == "gaze_influence_slider":
        return binding("indra/newview/alpanellensgaze.cpp", 1758, "ALPanelLensGaze", "onInfluenceSet")
    if page == "shot_tab" and re.fullmatch(r"switcher_slot_\d{2}", widget):
        return binding("indra/newview/alpaneldirectorswitcher.cpp", 68,
                       "ALPanelDirectorSwitcher", "onSlotButton", "switcher_slot_%02d")
    if page == "shot_tab" and widget in {
        "switcher_custom_yaw", "switcher_custom_pitch", "switcher_custom_distance",
        "switcher_custom_height", "switcher_custom_fov",
    }:
        return binding("indra/newview/alpaneldirectorswitcher.cpp", 375, "ALPanelDirectorSwitcher", "onCustomValue")
    if page == "shot_tab" and widget.startswith("reset_switcher_custom_"):
        return binding("indra/newview/alpaneldirectorswitcher.cpp", 391, "ALPanelDirectorSwitcher", "onCustomReset")
    if page == "actor_style_tab" and widget.startswith("actor_style_"):
        return binding("indra/newview/llfloaterdirector.cpp", 2404, "LLFloaterDirector", "onActorStyleControlChanged")
    if page == "atmosphere_tab" and widget.startswith((
        "localfog_center_", "localfog_size_", "localfog_rotation_"
    )) or (page == "atmosphere_tab" and widget in {
        "localfog_enabled", "localfog_label", "localfog_shape", "localfog_density",
        "localfog_feather", "localfog_height_falloff", "localfog_noise_scale",
        "localfog_noise_speed", "localfog_tint",
    }):
        return binding("indra/newview/alpanelatmosphericvolumes.cpp", 251, "ALPanelAtmosphericVolumes", "onEditorCommit")
    if page == "cine_light_rig_tab" and widget == "cine_live_probe_refresh_now":
        return binding("indra/newview/alpanelcinelightrig.cpp", 899,
                       "LLReflectionMapManager", "requestCinematicLiveProbeRefresh")
    if page == "light_rim_tab" and widget == "area_shadow_world_clear_picks":
        # World Shadow pick: the Clear button (bound in LLFloaterDirector::bindPage) calls this; the line is found by name.
        hit = binding("indra/newview/pipeline.cpp", 0, "LLPipeline", "clearWorldShadowPicks")
        hit.update(wired_class="LLFloaterDirector", wired_function="bindPage", text="explicit scoped binding: the Director commit callback clears the picks")
        return hit
    if page == "light_rim_tab" and widget == "area_shadow_world_clear_away_picks":
        # World Shadow pick fix round 1: the Clear away picks button (bound in LLFloaterDirector::bindPage) releases the picks whose object is gone.
        hit = binding("indra/newview/pipeline.cpp", 0, "LLPipeline", "clearAwayWorldShadowPicks")
        hit.update(wired_class="LLFloaterDirector", wired_function="bindPage", text="explicit scoped binding: the Director commit callback clears the away picks")
        return hit
    if page == "light_rim_tab" and widget == "cine_rimq_enable_tag":
        return {
            "file": "indra/newview/alrigrimcontroller.cpp", "line": 124,
            "class": "ALRigRimController", "function": "attach",
            "wired_class": "ALRigRimController", "wired_function": "attach",
            "pattern": None, "wiring_only": False,
            "text": "explicit scoped binding: confirmation callback enables the avatar tag",
        }
    if page != "cine_light_rig_tab":
        return None
    if widget in {"cine_group_self", "cine_group_a", "cine_group_b", "cine_group_c", "cine_group_d"}:
        return binding("indra/newview/alpanelcinelightrig.cpp", 1601, "ALPanelCineLightRig", "onGroupSlotsCommit")
    if re.fullmatch(r"cine_fixture_gel_[0-2]", widget):
        return binding("indra/newview/alpanelcinelightrig.cpp", 1325, "ALPanelCineLightRig", "onFixtureValuesCommit")
    match = re.fullmatch(r"cine_(key|fill|rim|bg)_(shaft|hero|cone_width)", widget)
    if not match:
        return None
    suffix = match.group(2)
    if suffix == "cone_width":
        return {
            "file": "indra/newview/alpanelcinelightrig.cpp", "line": 2348,
            "class": "ALPanelCineLightRig", "function": "onManualConeWidthCommit",
            "wired_class": "ALPanelCineLightRig", "wired_function": "postBuild",
            "pattern": 'prefix + "_cone_width"', "text": "role-indexed manual cone binding",
        }
    function = "setShaftEnabled" if suffix == "shaft" else "setHeroEnabled"
    line = 956 if suffix == "shaft" else 962
    behavior_class = "ALCineLightRigManager" if suffix == "shaft" else "ALCineLightRigModel"
    return {
        "file": "indra/newview/alpanelcinelightrig.cpp", "line": line,
        "class": behavior_class, "function": function,
        "wired_class": "ALPanelCineLightRig", "wired_function": "postBuild",
        "pattern": f'prefix + "_{suffix}"', "text": "role-indexed model binding",
    }

# This is deliberately checked against settings duplicated by the data.  Page
# masters are page-owned chrome, not extra rows in the rollout's 22-home table.
WRITABLE_HOMES = {
    "ActorMoverEndMode": "move_tab",
    "AlchemyRenderTonemapType": "exposure_tab",
    "AlchemyToneMapAMDExposure": "exposure_tab",
    "AlchemyToneMapSLogEV": "exposure_tab",
    "AlchemyTonemapLegacyGammaSkies": "exposure_tab",
    "BDMergeProjectorVolumetricsFeather": "atmosphere_tab",
    "BDMergeProjectorVolumetricsMaxDistance": "atmosphere_tab",
    "CineRigRimTronColorSource": "light_rim_tab",
    "CineRigRimTronMix": "light_rim_tab",
    "CineRigRimTronMode": "light_rim_tab",
    "CinematicCamArcDuration": "moves_tab",
    "CinematicCamArcGeneratorAngle": "moves_tab",
    "CinematicCamArcHeightDrift": "moves_tab",
    "CinematicCamFrameOffsetUp": "frame_orbit_tab",
    "CinematicCamJoint": "frame_orbit_tab",
    "CinematicCamVertigoDirection": "moves_tab",
    "CinematicCamVertigoDuration": "moves_tab",
    "RenderBloomStrength": "bloom_glow_tab",
    "RenderBloomThreshold": "bloom_glow_tab",
    "RenderExposure": "exposure_tab",
    "TemporalWorldScale": "temporal_tab",
}

EXPECTED_SHORTCUTS = {
    ("cine_light_rig_tab", "cine_easy_cone_feather"): "atmosphere_tab/ps_BDMergeProjectorVolumetricsFeather_slider",
    ("cine_light_rig_tab", "cine_easy_shaft_length"): "atmosphere_tab/ps_BDMergeProjectorVolumetricsMaxDistance_slider",
    ("shot_tab", "CinematicCamJoint"): "frame_orbit_tab/fo_anchor_joint",
    ("shot_tab", "CinematicCamVertigoDuration"): "moves_tab/zolly_duration",
    ("shot_tab", "CinematicCamVertigoDirection"): "moves_tab/zolly_direction",
    ("shot_tab", "CinematicCamArcDuration"): "moves_tab/hero_arc_duration",
    ("shot_tab", "CinematicCamArcGeneratorAngle"): "moves_tab/hero_arc_angle",
    ("shot_tab", "CinematicCamArcHeightDrift"): "moves_tab/hero_arc_height_drift",
    # 10/15: the five approved image_tab -> exposure_tab shortcuts (Easy card on Look > Image).
    ("image_tab", "tonemap_type_combo"): "exposure_tab/exp_tonemap_type",
    ("image_tab", "exposure_slider"): "exposure_tab/exp_exposure",
    ("image_tab", "tonemap_legacy_gamma_skies"): "exposure_tab/exp_legacy_gamma_skies",
    ("image_tab", "lpm_toneexp_slider"): "exposure_tab/exp_amd_exposure",
    ("image_tab", "slog_ev_slider"): "exposure_tab/exp_slog_ev",
    # 10/15: Light > Exposure Bloom rows are shortcuts of their home on Look > Bloom & glow.
    ("exposure_tab", "exp_bloom_strength"): "bloom_glow_tab/set_RenderBloomStrength",
    ("exposure_tab", "exp_bloom_threshold"): "bloom_glow_tab/set_RenderBloomThreshold",
}

ADAPTERS = {
    "move_tab": "ALActorMoverKitAdapter", "path_tab": "ALPathKitAdapter",
    "props_tab": "ALPropMoverKitAdapter", "ghosts_tab": "ALGhostStudioKitAdapter",
    "dance_tab": "ALDanceBallKitAdapter", "animate_tab": "ALAnimateKitAdapter",
    "switchboard_tab": "ALAnimSwitcherKitAdapter", "gaze_tab": "ALGazeKitAdapter",
    "polish_tab": "ALPosePolishKitAdapter", "gaze_cues_tab": "ALGazeCueKitAdapter",
    "shot_tab": "ALShotKitAdapter", "moves_tab": "ALCameraMoveKitAdapter",
    "operator_tab": "ALCameraOperatorKitAdapter", "frame_orbit_tab": "ALFrameOrbitKitAdapter",
    "takes_tab": "ALTakesKitAdapter", "vcam_tab": "ALVCamKitAdapter",
    "focus_tab": "ALFocusKitAdapter", "filters_tab": "ALLensFilterKitAdapter",
    "diopter_tab": "ALDiopterKitAdapter", "cine_light_rig_tab": "ALCineRigKitAdapter",
    "light_rim_tab": "ALRigRimKitAdapter", "exposure_tab": "ALExposureKitAdapter",
    "practicals_tab": "ALPracticalsKitAdapter", "actors_tab": "ALPanelAvatarLightResponse",
    "light_cues_tab": "ALLightCueKitAdapter", "image_tab": "ALImageLookKitAdapter",
    "bloom_glow_tab": "ALBloomGlowKitAdapter", "stylize_tab": "ALRotoKitAdapter",
    "tron_tab": "ALTronKitAdapter", "actor_style_tab": "ALActorStyleKitAdapter",
    "stepped_tab": "ALSteppedAnimKitAdapter",
    "weather_tab": "ALWeatherKitAdapter", "atmosphere_tab": "ALAtmosphereKitAdapter",
    "shadows_probes_tab": "ALShadowProbeKitAdapter", "temporal_tab": "ALTemporalCaptureKitAdapter",
    "scene_options_tab": "ALSceneOptionsKitAdapter",
    "director_settings_tab": "ALDirectorSettingsKitAdapter",
}

R2_FIRST = [
    "path_tab", "ghosts_tab", "dance_tab", "animate_tab", "switchboard_tab", "gaze_tab",
    "gaze_cues_tab", "shot_tab", "takes_tab", "diopter_tab", "exposure_tab", "practicals_tab",
    "actors_tab", "light_cues_tab", "image_tab", "stylize_tab", "tron_tab", "actor_style_tab",
    "atmosphere_tab", "frame_orbit_tab", "operator_tab", "move_tab",
]

SPECIAL_OWNERS = {
    "gaze_cues_tab": [
        "`floater_gaze_cues` in `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:2`; "
        "`ALFloaterGazeCues` in `indra/newview/alfloatergazecues.cpp:101-663`"
    ],
    "light_cues_tab": [
        "`floater_cine_light_cues` in `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:2`; "
        "`ALFloaterCineLightCues` in `indra/newview/alfloatercinelightcues.cpp:51-403`"
    ],
    "diopter_tab": [
        "`ultimate_diopter` in `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:8`; "
        "`ALFloaterUltimateDiopter` in `indra/newview/alfloaterultimatediopter.cpp:266-1565`"
    ],
}

SPECIAL_OWNER_RECORDS = {
    "gaze_cues_tab": [{"file": "indra/newview/skins/default/xui/en/floater_gaze_cues.xml", "tag": "floater", "name": "floater_gaze_cues", "line": 2, "class": "ALFloaterGazeCues", "class_file": "indra/newview/alfloatergazecues.cpp", "class_line": 101}],
    "light_cues_tab": [{"file": "indra/newview/skins/default/xui/en/floater_cine_light_cues.xml", "tag": "floater", "name": "floater_cine_light_cues", "line": 2, "class": "ALFloaterCineLightCues", "class_file": "indra/newview/alfloatercinelightcues.cpp", "class_line": 51}],
    "diopter_tab": [{"file": "indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml", "tag": "floater", "name": "ultimate_diopter", "line": 8, "class": "ALFloaterUltimateDiopter", "class_file": "indra/newview/alfloaterultimatediopter.cpp", "class_line": 266}],
}

PRESENTATION_NOTES = {
    "path_tab": "Use the existing waypoint `scroll_list`, buttons, combos, and spinners as kit rows. There is no path-canvas class to embed.",
    "switchboard_tab": "Keep the twelve existing `anim_slot_01`…`anim_slot_12` buttons as measured kit button rows/cells; do not create a slot-grid widget.",
    "shot_tab": "Keep the existing switcher buttons and `panel_cinecam_params.xml` controls; no slot-grid or curve editor is authorized.",
    "takes_tab": "Use the recorder's existing list, buttons, combos, sliders, and scrubber; do not create a timeline/curve editor.",
    "exposure_tab": "Use `panel_exposure_hub.xml` controls as kit rows; no curve editor exists.",
    "gaze_cues_tab": "Keep the cue list plus Add, Remove, Play, and Stop in Director. Selecting a row opens the existing Gaze Cues window at that cue; detailed keyframe editing remains there.",
    "light_cues_tab": "Keep the cue list plus Add, Remove, Play, and Stop in Director. Selecting a row opens the existing Lighting Cue Stack at that cue; detailed keyframe editing remains there.",
    "diopter_tab": "Keep only the everyday controls (on/off, preset, strength, focus split) plus Open full editor. The existing 560×720 nine-tab floater remains the detailed editor.",
    "cine_light_rig_tab": "Reuse the existing gobo control and light-map view under their existing contracts; no replacement editor is introduced here.",
    "ghosts_tab": "Reuse the existing preview/diagnostics controls with a 240 px maximum and visible-only, revision-driven refresh.",
}

ACTOR_PAGES = {"move_tab", "path_tab", "ghosts_tab", "animate_tab", "gaze_tab", "polish_tab", "gaze_cues_tab", "actors_tab", "actor_style_tab"}
LIST_PAGES = {"path_tab", "props_tab", "ghosts_tab", "dance_tab", "animate_tab", "switchboard_tab", "gaze_tab", "gaze_cues_tab", "shot_tab", "takes_tab", "light_cues_tab", "atmosphere_tab", "vcam_tab"}
PERMISSION_PAGES = {"props_tab", "ghosts_tab", "practicals_tab", "actors_tab", "atmosphere_tab", "vcam_tab"}

SPECIAL_EXTRACTIONS = {
    "shot_tab": [
        ("indra/newview/alpanelcinecamparams.cpp", "ALPanelCineCamParams", "modeTable", 127),
    ],
    "gaze_cues_tab": [
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshCueList", 235),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshCastCombo", 274),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshEditorVisibility", 303),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "loadEditor", 320),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "readEditor", 342),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onCueSelected", 395),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onAdd/onEdit/onDelete", 409),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onSetPoint/onPickObject/onClearObject", 460),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onQuickMacro", 486),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "refreshInfluenceList", 545),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "loadInfluenceEditor/readInfluenceEditor", 584),
        ("indra/newview/alfloatergazecues.cpp", "ALFloaterGazeCues", "onInfluenceAdd/onInfluenceUpdate/onInfluenceDelete", 634),
    ],
    "light_cues_tab": [
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshListNames", 126),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshCueRows", 155),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "refreshStatus", 203),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "loadEditor/readEditor", 241),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onListLoad/onListSave/onListDelete", 265),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onCueSelected/onCapture/onUpdate/onDeleteCue", 294),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onTimecodeMode", 353),
        ("indra/newview/alfloatercinelightcues.cpp", "ALFloaterCineLightCues", "onGo/onBack/onGoto/onRelease", 362),
    ],
    "diopter_tab": [
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "populateState/buildSigKey", 552),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "applyRowState", 697),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "updateBandPanels", 807),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "refreshRelevance", 844),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "armFocusPick/onFocusPickCommitted", 996),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onSetFromCameraFocusClicked", 1141),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "initRangeCombos/syncRangesToCurrentValues", 1265),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onRangeComboCommit", 1360),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onShowDepthToggled", 1485),
        ("indra/newview/alfloaterultimatediopter.cpp", "ALFloaterUltimateDiopter", "onFilterChanged", 1531),
    ],
}


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def tag_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def line_for_name(lines: list[str], name: str, start: int = 0) -> int:
    needle = re.compile(r"\bname\s*=\s*(['\"])" + re.escape(name) + r"\1")
    for number, line in enumerate(lines[start:], start + 1):
        if needle.search(line):
            return number
    return 1


def line_for_key(lines: list[str], name: str) -> int:
    needle = re.compile(r"<key>\s*" + re.escape(name) + r"\s*</key>")
    for number, line in enumerate(lines, 1):
        if needle.search(line):
            return number
    return 1


def scan_xui(widget_names: set[str]) -> tuple[dict[str, list[dict]], list[dict]]:
    by_widget: dict[str, list[dict]] = collections.defaultdict(list)
    all_controls: list[dict] = []
    for path in sorted(XUI_DIR.rglob("*.xml")):
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        try:
            root = ET.fromstring(text)
        except ET.ParseError as exc:
            print(f"warning: cannot parse {rel(path)}: {exc}", file=sys.stderr)
            continue
        root_info = {
            "file": rel(path),
            "tag": tag_name(root.tag),
            "name": root.attrib.get("name", ""),
            "line": line_for_name(lines, root.attrib.get("name", "")) if root.attrib.get("name") else 1,
        }
        search_from = 0
        for element in root.iter():
            name = element.attrib.get("name")
            if not name:
                continue
            number = line_for_name(lines, name, search_from)
            search_from = max(search_from, number - 1)
            item = {
                "file": rel(path),
                "line": number,
                "tag": tag_name(element.tag),
                "control_name": element.attrib.get("control_name"),
                "callbacks": sorted(
                    {
                        value
                        for child in element.iter()
                        for key, value in child.attrib.items()
                        if value and (key == "function" and tag_name(child.tag).endswith("commit_callback")
                                      or key.endswith(".function"))
                    }
                ),
                "root": root_info,
            }
            all_controls.append({"name": name, **item})
            if name in widget_names:
                by_widget[name].append(item)
    return dict(by_widget), all_controls


def source_files() -> list[Path]:
    files: set[Path] = set()
    for directory in CPP_DIRS:
        files.update(directory.rglob("*.cpp"))
    return sorted(files)


def printf_pattern(literal: str) -> re.Pattern[str] | None:
    """Return a full-match regex for a printf/llformat control name."""
    token = re.compile(r"%(?:0(?P<width>\d+))?[diu]")
    if not token.search(literal):
        return None
    parts: list[str] = []
    cursor = 0
    for match in token.finditer(literal):
        parts.append(re.escape(literal[cursor:match.start()]))
        width = match.group("width")
        parts.append(rf"\d{{{int(width)}}}" if width else r"\d+")
        cursor = match.end()
    parts.append(re.escape(literal[cursor:]))
    return re.compile("".join(parts) + r"\Z")


def setting_only_reference(line: str) -> bool:
    if "getChild" in line or "findChild" in line:
        return False
    return any(marker in line for marker in (
        "gSavedSettings", "gSavedPerAccountSettings", "LLCachedControl", "getControl(",
        ".setBOOL(", ".setS32(", ".setU32(", ".setF32(", ".setString(",
    ))


def behavior_from_text(text: str, owner_class: str | None, wired_function: str | None) -> tuple[str | None, str | None]:
    pointer = re.search(r"&([A-Za-z_]\w*)::([A-Za-z_]\w*)", text)
    if pointer:
        return pointer.group(1), pointer.group(2)
    if re.search(r"\[[^]]*\]\s*\([^)]*\)\s*(?:mutable\s*)?\{", text):
        ignored = {
            "getChild", "findChild", "setCommitCallback", "setClickedCallback", "setValue",
            "setEnabled", "setVisible", "bind", "llformat", "boost", "static_cast",
            "if", "for", "while", "switch", "return", "selected", "instance", "controls",
        }
        body = text[text.find("{") + 1:]
        # Prefer the terminal model call in a singleton chain, e.g.
        # Manager::instance().selected().stopFX(), over the accessor selected().
        singleton = re.search(r"([A-Za-z_]\w*)::instance\(\)", body)
        if singleton:
            statement = body[singleton.end():].split(";", 1)[0]
            methods = [name for name in re.findall(r"\.([A-Za-z_]\w*)\s*\(", statement)
                       if name not in ignored]
            if methods:
                return singleton.group(1), methods[-1]
        qualified = re.search(r"([A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\(", body)
        if qualified and qualified.group(2) not in ignored:
            return qualified.group(1), qualified.group(2)
        member_calls = [name for name in re.findall(r"\.([A-Za-z_]\w*)\s*\(", body)
                        if name not in ignored]
        if member_calls:
            return owner_class, member_calls[-1]
        for match in re.finditer(r"(?<![.:>])\b([A-Za-z_]\w*)\s*\(", body):
            function = match.group(1)
            if function not in ignored and function != wired_function:
                return owner_class, function
        return owner_class, "<inline callback>"
    singleton = re.search(r"([A-Za-z_]\w*)::instance\(\)\.([A-Za-z_]\w*)\s*\(", text)
    if singleton:
        return singleton.group(1), singleton.group(2)
    return owner_class, wired_function


def call_context(lines: list[str], start: int) -> str:
    """Collect exactly one (possibly multiline) call expression."""
    parts: list[str] = []
    balance = 0
    opened = False
    for line in lines[start:min(len(lines), start + 40)]:
        parts.append(line.strip())
        # Callback bodies in this tree do not put unmatched parentheses in
        # string literals; parenthesis balance is therefore a safer boundary
        # than a fixed look-ahead that can consume the next callback.
        balance += line.count("(") - line.count(")")
        opened = opened or "(" in line
        if opened and balance <= 0:
            break
    return " ".join(parts)


def scan_cpp(widget_names: set[str]) -> dict[str, list[dict]]:
    hits: dict[str, list[dict]] = collections.defaultdict(list)
    for path in source_files():
        source_text = path.read_text(encoding="utf-8", errors="replace")
        lines = source_text.splitlines()
        string_arrays: dict[str, list[str]] = {}
        for array_match in re.finditer(
            r"(?:const\s+)?char\s*\*\s*const\s+(\w+)\s*\[[^]]*\]\s*=\s*\{(.*?)\};",
            source_text,
            re.DOTALL,
        ):
            string_arrays[array_match.group(1)] = STRING_RE.findall(array_match.group(2))
        symbolic_prefixes: dict[str, tuple[str, list[str]]] = {}
        for prefix_match in re.finditer(
            r"(?:const\s+)?std::string\s+(\w+)\s*=\s*std::string\(\"([^\"]*)\"\)\s*\+\s*(\w+)\s*\[[^]]+\]\s*;",
            source_text,
            re.DOTALL,
        ):
            values = string_arrays.get(prefix_match.group(3), [])
            if values:
                symbolic_prefixes[prefix_match.group(1)] = (prefix_match.group(2), values)
        member_widgets: dict[str, set[str]] = collections.defaultdict(set)
        for member_match in re.finditer(
            r"\b([A-Za-z_]\w*)\s*=\s*(?:getChild|findChild)\s*<[^>]+>\s*\(\s*\"([^\"]+)\"",
            source_text,
        ):
            if member_match.group(2) in widget_names:
                member_widgets[member_match.group(1)].add(member_match.group(2))
        current_class = ""
        current_function = ""
        pending: tuple[str, str] | None = None
        depth = 0
        method_depth: int | None = None
        for number, line in enumerate(lines, 1):
            match = METHOD_RE.search(line)
            if match:
                class_name = match.group("class")
                function_name = match.group("function")
                # A return type is required except for constructors and
                # destructors.  This keeps Class::method() calls and comments
                # from being mistaken for definitions.
                if match.group("prefix").strip() or function_name.lstrip("~") == class_name:
                    pending = (class_name, function_name)
            opens = line.count("{")
            closes = line.count("}")
            if pending and opens:
                current_class, current_function = pending
                method_depth = depth + 1
                pending = None
            literals = [literal.replace(r'\"', '"').replace(r"\\", "\\") for literal in STRING_RE.findall(line)]
            matches: list[tuple[str, str | None]] = []
            callback_member = re.search(
                r"\b([A-Za-z_]\w*)\s*->\s*set(?:Commit|Clicked|MouseUp)Callback\s*\(", line
            )
            if callback_member and callback_member.group(1) in member_widgets:
                context = call_context(lines, number - 1)
                behavior_class, behavior_function = behavior_from_text(
                    context, current_class or None, current_function or None
                )
                for name in member_widgets[callback_member.group(1)]:
                    hits[name].append({
                        "file": rel(path), "line": number,
                        "class": behavior_class, "function": behavior_function,
                        "wired_class": current_class or None,
                        "wired_function": current_function or None,
                        "pattern": None, "wiring_only": False,
                        "text": line.strip(),
                    })
            for literal in literals:
                if literal in widget_names:
                    matches.append((literal, None))
                pattern = printf_pattern(literal)
                if pattern:
                    matches.extend((name, literal) for name in widget_names if pattern.fullmatch(name))
            # Constant concatenations are common beside llformat-built names.
            if len(literals) > 1 and "+" in line:
                joined = "".join(literals)
                if joined in widget_names:
                    matches.append((joined, " + ".join(literals)))
            for variable, (prefix, values) in symbolic_prefixes.items():
                suffix_match = re.search(rf"\b{re.escape(variable)}\s*\+\s*\"([^\"]*)\"", line)
                if suffix_match:
                    suffix = suffix_match.group(1)
                    matches.extend(
                        (prefix + value + suffix, f'{prefix}{{{"|".join(values)}}}{suffix}')
                        for value in values
                        if prefix + value + suffix in widget_names
                    )
            for name, pattern_text in matches:
                if setting_only_reference(line) or "/tests/" in rel(path):
                    continue
                direct_callback = "Callback" in line or "childSetAction" in line
                context = call_context(lines, number - 1) if direct_callback else line.strip()
                behavior_class, behavior_function = behavior_from_text(
                    context, current_class or None, current_function or None
                )
                hits[name].append({
                    "file": rel(path),
                    "line": number,
                    "class": behavior_class,
                    "function": behavior_function,
                    "wired_class": current_class or None,
                    "wired_function": current_function or None,
                    "pattern": pattern_text,
                    "wiring_only": bool(
                        ("getChild" in line or "findChild" in line)
                        and not direct_callback
                    ),
                    "text": line.strip(),
                })
            depth += opens - closes
            if method_depth is not None and depth < method_depth:
                current_class = ""
                current_function = ""
                method_depth = None
    return dict(hits)


def scan_callback_handlers(callback_names: set[str]) -> dict[str, list[dict]]:
    handlers: dict[str, list[dict]] = collections.defaultdict(list)
    for path in source_files():
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        current_class = ""
        current_function = ""
        pending: tuple[str, str] | None = None
        depth = 0
        method_depth: int | None = None
        for number, line in enumerate(lines, 1):
            match = METHOD_RE.search(line)
            if match and (match.group("prefix").strip() or match.group("function").lstrip("~") == match.group("class")):
                pending = (match.group("class"), match.group("function"))
            opens = line.count("{")
            closes = line.count("}")
            if pending and opens:
                current_class, current_function = pending
                method_depth = depth + 1
                pending = None
            literals = set(STRING_RE.findall(line))
            for callback in sorted(literals & callback_names):
                context = " ".join(part.strip() for part in lines[max(0, number - 1):min(len(lines), number + 8)])
                behavior_class, behavior_function = behavior_from_text(
                    context, current_class or None, current_function or None
                )
                handlers[callback].append({
                    "callback": callback,
                    "file": rel(path),
                    "line": number,
                    "class": behavior_class,
                    "function": behavior_function,
                    "wired_class": current_class or None,
                    "wired_function": current_function or None,
                    "text": line.strip(),
                })
            depth += opens - closes
            if method_depth is not None and depth < method_depth:
                current_class = ""
                current_function = ""
                method_depth = None
    return dict(handlers)


def llsd_map(element: ET.Element) -> dict[str, ET.Element]:
    children = list(element)
    result: dict[str, ET.Element] = {}
    index = 0
    while index + 1 < len(children):
        if tag_name(children[index].tag) == "key":
            result[children[index].text or ""] = children[index + 1]
            index += 2
        else:
            index += 1
    return result


def setting_bases() -> list[tuple[str, Path]]:
    root = ET.parse(SETTINGS_FILES).getroot()
    bases: list[tuple[str, Path]] = []
    seen: set[tuple[str, Path]] = set()
    for file_node in root.findall(".//file"):
        group = file_node.attrib.get("name", "")
        filename = file_node.attrib.get("file_name", "")
        if group not in {"Global", "PerAccount"} or not filename:
            continue
        path = SETTINGS_DIR / filename
        key = (group, path)
        if path.exists() and key not in seen:
            bases.append(key)
            seen.add(key)
    return bases


def scan_settings() -> dict[str, list[dict]]:
    declarations: dict[str, list[dict]] = collections.defaultdict(list)
    visited: set[tuple[str, Path]] = set()

    def load(group: str, path: Path) -> None:
        key = (group, path.resolve())
        if key in visited or not path.exists():
            return
        visited.add(key)
        root = ET.parse(path).getroot()
        map_node = next((node for node in root.iter() if tag_name(node.tag) == "map"), None)
        if map_node is None:
            return
        entries = llsd_map(map_node)
        include = entries.get("Include")
        if include is not None:
            for node in include:
                if tag_name(node.tag) == "string" and node.text:
                    load(group, path.parent / node.text)
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        for name, value in entries.items():
            if name == "Include" or tag_name(value.tag) != "map":
                continue
            fields = llsd_map(value)
            type_node = fields.get("Type")
            type_name = (type_node.text or "") if type_node is not None else ""
            declarations[name].append({
                "file": rel(path),
                "line": line_for_key(lines, name),
                "group": group,
                "type": type_name or None,
            })

    for group, path in setting_bases():
        load(group, path)
    return dict(declarations)


def independent_row_count(data: dict) -> int:
    """Count directly from the JSON containers, independently of flattening."""
    return sum(
        len(card.get("rows", []))
        + sum(len(option.get("rows", [])) for option in card.get("opts", []))
        for group in data["groups"]
        for page in group["pages"]
        for card in page.get("cards", [])
    )


def _selector_label(value: str) -> str:
    words = re.sub(r"[^a-z0-9]+", " ", value.lower()).split()
    return " ".join(word for word in words if word != "the")


def _xml_scalar(value: str):
    if re.fullmatch(r"[-+]?\d+", value):
        return int(value)
    if re.fullmatch(r"[-+]?(?:\d+\.\d*|\d*\.\d+)(?:[eE][-+]?\d+)?", value):
        return float(value)
    return value


def validate_selector_values(data: dict) -> int:
    """Round-trip every selector option through its real v1 combo value.

    Shot is XUI-authored. Rig/Rim proxy the dynamically populated v1 fixture
    role combo at alpanelcinelightrig.cpp:57-58,1064-1114; their presentation
    aliases Back/Rim and BG/Bg retain the same explicit 0..3 LLSD values.
    """
    rig_source = (ROOT / "indra/newview/alpanelcinelightrig.cpp").read_text(encoding="utf-8")
    role_match = re.search(
        r"ROLE_NAMES\s*\[[^]]+\]\s*=\s*\{\s*\"Key\"\s*,\s*\"Fill\"\s*,\s*\"Rim\"\s*,\s*\"Bg\"\s*\}",
        rig_source,
    )
    if not role_match or not re.search(
            r"mFixtureRole->add\(ROLE_NAMES\[light\],\s*LLSD\(light\)\)", rig_source):
        raise RuntimeError("cannot prove the v1 cine_fixture_role label/value population")
    dynamic_roles = {
        ("cine_light_rig_tab", "lights"): {"key": 0, "fill": 1, "back": 2, "bg": 3},
        ("light_rim_tab", "per_light_rim"): {"key": 0, "fill": 1, "back": 2, "bg": 3},
    }
    checked = 0
    for group in data["groups"]:
        for page in group["pages"]:
            for card in page.get("cards", []):
                if card.get("k") != "selector":
                    continue
                source = dynamic_roles.get((page["name"], card.get("id")))
                if source is None:
                    setting = card.get("sel_setting")
                    if not setting:
                        raise RuntimeError(f"selector {page['name']}/{card.get('id')} has no value source")
                    candidates = []
                    for file_name in PAGE_OWNER_FILES[page["name"]]:
                        path = XUI_DIR / file_name
                        if not path.exists():
                            continue
                        root = ET.parse(path).getroot()
                        for element in root.iter():
                            if tag_name(element.tag) != "combo_box":
                                continue
                            if setting not in (element.attrib.get("name"), element.attrib.get("control_name")):
                                continue
                            candidates.append({
                                _selector_label(item.attrib.get("label", "")): _xml_scalar(item.attrib["value"])
                                for item in element
                                if tag_name(item.tag) == "combo_box.item" and "value" in item.attrib
                            })
                    if len(candidates) != 1:
                        raise RuntimeError(
                            f"selector {page['name']}/{card.get('id')} expected one v1 combo for {setting}; got {len(candidates)}"
                        )
                    source = candidates[0]
                for option in card.get("opts", []):
                    if "value" not in option:
                        raise RuntimeError(f"selector option {page['name']}/{card.get('id')}/{option.get('label')} has no value")
                    label = _selector_label(option.get("label", ""))
                    tiers = [
                        [(source_label, value) for source_label, value in source.items() if source_label == label],
                        [(source_label, value) for source_label, value in source.items() if source_label.startswith(label + " ")],
                        [(source_label, value) for source_label, value in source.items() if source_label.endswith(" " + label)],
                    ]
                    matches = next((hits for hits in tiers if hits), [])
                    if len(matches) != 1 or option["value"] != matches[0][1]:
                        raise RuntimeError(
                            f"selector option {page['name']}/{card.get('id')}/{option.get('label')} "
                            f"value {option.get('value')!r} does not round-trip to its v1 combo item {matches!r}"
                        )
                    checked += 1
    return checked


def flatten_data() -> tuple[list[dict], list[dict], int]:
    data = json.loads(DATA.read_text(encoding="utf-8"))
    validate_selector_values(data)
    pages: list[dict] = []
    rows: list[dict] = []
    for group_index, group in enumerate(data["groups"]):
        for page_index, page in enumerate(group["pages"]):
            page_record = {
                "key": page["name"],
                "label": page["label"],
                "group": group["name"],
                "group_label": group["label"],
                "order": [group_index, page_index],
                "master_setting": (page.get("master") or {}).get("s"),
            }
            pages.append(page_record)
            for card_index, card in enumerate(page.get("cards", [])):
                containers = [(None, None, card.get("rows", []))]
                containers.extend(
                    (option_index, option.get("label", ""), option.get("rows", []))
                    for option_index, option in enumerate(card.get("opts", []))
                )
                for option_index, option_label, option_rows in containers:
                    for row_index, row in enumerate(option_rows):
                        rows.append({
                            "page": page_record,
                            "card_id": card.get("id", ""),
                            "card_title": card.get("t", ""),
                            "card_index": card_index,
                            "option_index": option_index,
                            "option_label": option_label,
                            "row_index": row_index,
                            "label": row.get("l", ""),
                            "kind": row.get("k", ""),
                            "widget": row.get("w", ""),
                            "declared_setting": row.get("s"),
                            "items": row.get("it", []),
                            "target": row.get("target"),
                            "target_tab": row.get("target_tab"),
                            "link_to": row.get("to"),
                            "shortcut_of": row.get("shortcut_of"),
                        })
    return pages, rows, independent_row_count(data)


def unique_dicts(items: list[dict], keys: tuple[str, ...]) -> list[dict]:
    seen: set[tuple] = set()
    result: list[dict] = []
    for item in items:
        key = tuple(item.get(field) for field in keys)
        if key not in seen:
            seen.add(key)
            result.append(item)
    return result


def build_index() -> dict:
    pages, raw_rows, expected_rows = flatten_data()
    if len(raw_rows) != expected_rows:
        raise RuntimeError(
            f"row walk is incomplete: flattened {len(raw_rows)}, JSON independently counts {expected_rows}"
        )
    actual_shortcuts = {
        (row["page"]["key"], row["widget"]): row["shortcut_of"]
        for row in raw_rows if row["shortcut_of"]
    }
    if actual_shortcuts != EXPECTED_SHORTCUTS:
        raise RuntimeError(
            f"shortcut table differs from data; actual={actual_shortcuts!r}, expected={EXPECTED_SHORTCUTS!r}"
        )
    widgets = {row["widget"] for row in raw_rows if row["widget"]}
    widgets.update(WIDGET_NAME_OVERRIDES.values())
    xui, all_controls = scan_xui(widgets)
    cpp = scan_cpp(widgets)
    callback_names = {
        callback
        for hits in xui.values()
        for hit in hits
        for callback in hit.get("callbacks", [])
    }
    callback_handlers = scan_callback_handlers(callback_names)
    settings = scan_settings()
    controls_by_setting: dict[str, list[dict]] = collections.defaultdict(list)
    for control in all_controls:
        if control.get("control_name"):
            controls_by_setting[control["control_name"]].append(control)

    page_rows: dict[str, list[dict]] = collections.defaultdict(list)
    setting_pages: dict[str, set[str]] = collections.defaultdict(set)
    for page in pages:
        if page["master_setting"]:
            setting_pages[page["master_setting"]].add(page["key"])
    resolved_rows: list[dict] = []
    for raw in raw_rows:
        page_key = raw["page"]["key"]
        widget = raw["widget"]
        match_widget = WIDGET_NAME_OVERRIDES.get((page_key, widget), widget)
        allowed_files = PAGE_OWNER_FILES[page_key]
        allowed_classes = PAGE_OWNER_CLASSES[page_key]
        all_xui_hits = xui.get(match_widget, []) if match_widget else []
        source_declared = raw["declared_setting"]
        declared = source_declared or SETTING_OVERRIDES.get((page_key, widget))
        if declared and (widget == "check_box" or widget.startswith("set_")):
            all_xui_hits = [
                {key: value for key, value in control.items() if key != "name"}
                for control in controls_by_setting.get(declared, [])
            ]
        xui_hits = [hit for hit in all_xui_hits if Path(hit["file"]).name in allowed_files]
        all_cpp_hits = cpp.get(match_widget, []) if match_widget else []
        cpp_hits = [
            hit for hit in all_cpp_hits
            if hit.get("class") in allowed_classes or hit.get("wired_class") in allowed_classes
        ]
        model_override = model_binding_override(page_key, widget)
        if model_override:
            cpp_hits.append(model_override)
        behavior_cpp_hits = [
            hit for hit in cpp_hits
            if not hit.get("wiring_only") and hit.get("function") != "postBuild"
        ]
        row_callback_names = sorted({callback for hit in xui_hits for callback in hit.get("callbacks", [])})
        all_row_callback_handlers = [
            handler
            for callback in row_callback_names
            for handler in callback_handlers.get(callback, [])
        ]
        # Callback names came from an in-scope XUI instance, so their registrar
        # is behavior evidence even when the behavior lives in a model class.
        row_callback_handlers = all_row_callback_handlers
        xml_settings = sorted({hit["control_name"] for hit in xui_hits if hit.get("control_name")})
        candidates = ([declared] if declared else []) + [item for item in xml_settings if item != declared]
        setting = declared or (xml_settings[0] if len(xml_settings) == 1 else None)
        if setting:
            setting_pages[setting].add(page_key)
        kit_action = None
        if raw["kind"] == "link" and raw["link_to"]:
            kit_action = f"link_to:{raw['link_to']}"
        elif raw["target"]:
            kit_action = f"floater:{raw['target']}"
            if raw["target_tab"]:
                kit_action += f"/{raw['target_tab']}"
        kit_action = kit_action or KIT_ACTION_OVERRIDES.get((page_key, widget))
        reasons: list[str] = []
        if setting and setting not in settings:
            reasons.append("setting declaration not found")
        if not declared and len(xml_settings) > 1:
            reasons.append("multiple in-scope XUI control_name values")
        executable = raw["kind"] in {"button", "toggle", "link"}
        if not setting and not kit_action and raw["kind"] != "readout" and not (behavior_cpp_hits or row_callback_handlers or row_callback_names):
            reasons.append("UNRESOLVED: no scoped owner callback, setting, model binding, or kit action")
        elif executable and not setting and not kit_action and not row_callback_handlers and not behavior_cpp_hits and not row_callback_names:
            reasons.append("UNRESOLVED: executable row has no scoped behavior function")
        rejected_candidates = []
        for hit in all_xui_hits:
            if hit not in xui_hits:
                rejected_candidates.append({"kind": "xui", "reason": "same widget name outside the page owner roots", **hit})
        for hit in all_cpp_hits:
            if hit not in cpp_hits:
                rejected_candidates.append({"kind": "cpp", "reason": "reference is not in a class that owns/builds this page's XUI", **hit})
        ambiguous = []
        if not declared and len(xml_settings) > 1:
            ambiguous = [
                {"kind": "xui", "reason": "in-scope candidates disagree on control_name", **hit}
                for hit in xui_hits if hit.get("control_name")
            ]
        notes = []
        if match_widget != widget:
            notes.append(f"data name `{widget}` maps to existing XUI `{match_widget}`")
        if not source_declared and declared:
            notes.append(f"page contract supplies omitted setting `{declared}`")
        if widget == "check_box" and declared:
            notes.append(f"generic data name resolved by control_name `{declared}`")
        if widget.startswith("set_") and setting:
            notes.append("kit-native setting row; synthetic data name does not require v1 XUI")
        row = {
            "card": raw["card_id"],
            "card_title": raw["card_title"],
            "option_index": raw["option_index"],
            "option_label": raw["option_label"],
            "index": raw["row_index"],
            "label": raw["label"],
            "kind": raw["kind"],
            "widget": widget or None,
            "matched_widget": match_widget or None,
            "declared_setting": declared,
            "resolved_setting": setting,
            "setting_candidates": candidates,
            "setting_declarations": settings.get(setting, []) if setting else [],
            "items": raw["items"],
            "shortcut_of": raw["shortcut_of"],
            "xui": xui_hits,
            "cpp": cpp_hits,
            "callback_handlers": row_callback_handlers,
            "callbacks": row_callback_names,
            "kit_action": kit_action,
            "resolution_notes": notes,
            "ambiguous": ambiguous,
            "rejected_candidates": rejected_candidates,
            "unresolved": reasons,
        }
        if row["shortcut_of"] is None:
            row.pop("shortcut_of")
        resolved_rows.append(row)
        page_rows[raw["page"]["key"]].append(row)

    # XUI controls outside the mockup pages are writable peers when they bind a
    # duplicated setting.  Page peers are added after every row is resolved.
    xui_setting_writers: dict[str, list[dict]] = collections.defaultdict(list)
    for control in all_controls:
        setting = control.get("control_name")
        if setting:
            xui_setting_writers[setting].append({
                "file": control["file"],
                "line": control["line"],
                "widget": control["name"],
                "root": control["root"],
            })

    output_pages: list[dict] = []
    all_unresolved = 0
    for page in pages:
        rows = page_rows[page["key"]]
        owner_counts: dict[tuple, int] = collections.Counter()
        owner_data: dict[tuple, dict] = {}
        class_counts: collections.Counter[str] = collections.Counter()
        for row in rows:
            for hit in row["xui"]:
                root = hit["root"]
                key = (root["file"], root["tag"], root["name"])
                owner_counts[key] += 1
                owner_data[key] = root
            for hit in row["cpp"]:
                if hit.get("class"):
                    class_counts[hit["class"]] += 1
            for hit in row["callback_handlers"]:
                if hit.get("class"):
                    class_counts[hit["class"]] += 1
        owners = []
        for key, count in sorted(owner_counts.items(), key=lambda pair: (-pair[1], pair[0])):
            owner = dict(owner_data[key])
            owner["matched_rows"] = count
            owner["evidence"] = "row-widget match"
            owners.append(owner)
        owner_keys = {(owner["file"], owner["name"]) for owner in owners}
        for special in SPECIAL_OWNER_RECORDS.get(page["key"], []):
            if (special["file"], special["name"]) not in owner_keys:
                owners.append({**special, "matched_rows": 0, "evidence": "full-editor owner; approved rows are summary/launcher rows"})
            class_counts[special["class"]] += 0
        setting_bound = sum(bool(row["resolved_setting"] and row["setting_declarations"]) for row in rows)
        cpp_rows = sum(bool(row["cpp"] or row["callback_handlers"]) for row in rows)
        coverage = (setting_bound / len(rows)) if rows else 0.0
        logic_class = "pure-setting" if rows and coverage >= 0.90 and cpp_rows == 0 else "code-driven"
        unresolved_count = sum(bool(row["unresolved"]) for row in rows)
        all_unresolved += unresolved_count
        output_pages.append({
            **{key: page[key] for key in ("key", "label", "group", "group_label")},
            "row_count": len(rows),
            "owner_roots": owners,
            "owner_classes": [
                {"class": name, "reference_count": count}
                for name, count in class_counts.most_common()
            ],
            "classification": {
                "logic_class": logic_class,
                "setting_bound_rows": setting_bound,
                "setting_coverage": round(coverage, 6),
                "cpp_referenced_rows": cpp_rows,
                "rule": "pure-setting iff >=90% rows have declared settings and zero rows have C++ references",
            },
            "unresolved_row_count": unresolved_count,
            "rows": rows,
        })

    conflicts = []
    for setting, page_set in sorted(setting_pages.items()):
        writers = unique_dicts(xui_setting_writers.get(setting, []), ("file", "line", "widget"))
        if len(page_set) > 1 or writers:
            conflicts.append({
                "setting": setting,
                "pages": sorted(page_set),
                "xui_writers": writers,
                "declarations": settings.get(setting, []),
                "writable_home": WRITABLE_HOMES.get(setting),
            })
    duplicate_settings = {setting for setting, page_set in setting_pages.items() if len(page_set) > 1}
    expected_homes = duplicate_settings
    if expected_homes != set(WRITABLE_HOMES):
        missing = sorted(expected_homes - set(WRITABLE_HOMES))
        stale = sorted(set(WRITABLE_HOMES) - expected_homes)
        raise RuntimeError(f"writable-home table differs from data conflicts; missing={missing}, stale={stale}")
    for setting, home in WRITABLE_HOMES.items():
        if home not in setting_pages[setting]:
            raise RuntimeError(f"writable home {home} is not a page containing {setting}")
    for page in output_pages:
        for row in page["rows"]:
            setting = row["resolved_setting"]
            if not setting:
                row["other_writers"] = []
                continue
            others = []
            for other_page in sorted(setting_pages[setting] - {page["key"]}):
                others.append({"kind": "page", "page": other_page})
            own_xui = {(hit["file"], hit["line"]) for hit in row["xui"]}
            for writer in xui_setting_writers.get(setting, []):
                if (writer["file"], writer["line"]) not in own_xui:
                    others.append({"kind": "xui", **writer})
            row["other_writers"] = unique_dicts(others, ("kind", "page", "file", "line", "widget"))

    return {
        "schema_version": 2,
        "source": rel(DATA),
        "classification_rule": "pure-setting iff >=90% rows are setting-bound and no row widget is referenced in C++; otherwise code-driven",
        "summary": {
            "pages": len(output_pages),
            "rows": len(resolved_rows),
            "owner_roots": len({owner["file"] for page in output_pages for owner in page["owner_roots"]}),
            "unresolved_rows": all_unresolved,
            "ambiguous_rows": sum(bool(row["ambiguous"]) for row in resolved_rows),
            "multi_page_settings": sum(len(item["pages"]) > 1 for item in conflicts),
            "independent_json_rows": expected_rows,
        },
        "setting_conflicts": conflicts,
        "pages": output_pages,
    }


def md_anchor(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def render_markdown(index: dict) -> str:
    lines = [
        "# Director Kit page source evidence",
        "",
        f"Generated by `scripts/code_tools/director_page_sources.py` from `{index['source']}`.",
        "Do not hand-edit. File and line references describe the scanned tree at generation time.",
        "",
        "## Summary",
        "",
        "| Pages | Rows | Distinct owner files | Unresolved rows | Ambiguous rows | Multi-page settings |",
        "|---:|---:|---:|---:|---:|---:|",
        f"| {index['summary']['pages']} | {index['summary']['rows']} | {index['summary']['owner_roots']} | {index['summary']['unresolved_rows']} | {index['summary']['ambiguous_rows']} | {index['summary']['multi_page_settings']} |",
        "",
        f"Classification: {index['classification_rule']}.",
        "",
        "## One-home conflicts",
        "",
        "| Setting | Director pages | Other XUI writers |",
        "|---|---|---|",
    ]
    for conflict in index["setting_conflicts"]:
        if len(conflict["pages"]) < 2:
            continue
        writers = ", ".join(
            f"`{item['root']['name'] or item['root']['tag']}` (`{item['file']}:{item['line']}` / `{item['widget']}`)"
            for item in conflict["xui_writers"]
        ) or "none"
        lines.append(f"| `{conflict['setting']}` | {', '.join(f'`{p}`' for p in conflict['pages'])} | {writers} |")
    lines += ["", "## Pages", ""]
    for page in index["pages"]:
        c = page["classification"]
        lines += [
            f"### {page['label']} (`{page['key']}`)",
            "",
            f"Real owner roots: " + (", ".join(
                f"`{owner['name'] or owner['tag']}` in `{owner['file']}:{owner['line']}` ({owner['matched_rows']} matched rows)"
                for owner in page["owner_roots"]
            ) or "none found") + ".",
            "",
            "C++ owner classes: " + (", ".join(
                f"`{item['class']}` ({item['reference_count']} references)" for item in page["owner_classes"]
            ) or "none found") + ".",
            "",
            f"Computed logic class: **{c['logic_class']}**; {c['setting_bound_rows']}/{page['row_count']} rows setting-bound, {c['cpp_referenced_rows']} rows referenced in C++.",
            "",
            "| Card / row | Widget | Kind | Setting declaration | XUI definitions | C++ handlers | Other writers | Unresolved |",
            "|---|---|---|---|---|---|---|---|",
        ]
        for row in page["rows"]:
            declarations = ", ".join(
                f"`{item['type']}` `{item['file']}:{item['line']}` ({item['group']})"
                for item in row["setting_declarations"]
            ) or (f"`{row['resolved_setting']}` (missing declaration)" if row["resolved_setting"] else "—")
            xui_text = ", ".join(
                f"`{hit['file']}:{hit['line']}` `{hit['root']['name'] or hit['root']['tag']}`/{hit['tag']}"
                for hit in row["xui"]
            ) or "—"
            cpp_text = ", ".join(
                f"`{hit['class']}::{hit['function']} {hit['file']}:{hit['line']}`"
                for hit in [*row["cpp"], *row["callback_handlers"]]
                if (not hit.get("wiring_only") and hit.get("class") and hit.get("function")
                    and hit.get("function") != "postBuild")
            ) or "—"
            other_text = ", ".join(
                (f"page `{item['page']}`" if item["kind"] == "page" else f"`{item['root']['name'] or item['root']['tag']}` `{item['file']}:{item['line']}`")
                for item in row["other_writers"]
            ) or "—"
            unresolved = "; ".join(row["unresolved"]) or "—"
            label = row["label"].replace("|", "\\|")
            lines.append(
                f"| `{row['card']}` / {label} | `{row['widget'] or ''}` | `{row['kind']}` | {declarations} | {xui_text} | {cpp_text} | {other_text} | {unresolved} |"
            )
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def page_audit_states(page: dict) -> list[tuple[str, str]]:
    states: list[tuple[str, str]] = [
        ("size-850x560", "Default data at the minimum supported shell size."),
        ("size-915x720", "Default data at the narrow 16:9 shell size."),
        ("size-1141x720", "Default data at the wide non-round shell size."),
    ]
    cards: dict[str, list[dict]] = collections.defaultdict(list)
    for row in page["rows"]:
        cards[row["card"]].append(row)
    for card, rows in cards.items():
        states.append((f"card-{card}-tallest-850", f"Only `{card}` is put in its tallest local state at 850 px; all other cards stay default."))
        states.append((f"card-{card}-tallest-1160", f"Only `{card}` is put in its tallest local state at 1160 px; all other cards stay default."))
        for row in rows:
            widget = row["widget"] or f"row-{row['index']}"
            if row["kind"] == "combo":
                items = list(row.get("items", []))
                # A VCam camera list (None, Cam 1..Cam N) has interchangeable
                # camera slots: audit None plus one camera, not every slot.
                if len(items) > 2 and all(re.fullmatch(r"Cam \d+", str(i)) for i in items[1:]):
                    items = items[:2]
                for index, item in enumerate(items):
                    states.append((f"{widget}-option-{index}", f"Only `{widget}` selects `{item}`; other selectors remain default."))
            elif row["kind"] == "toggle":
                states.append((f"{widget}-on", f"Only `{widget}` is on; dependent visibility is measured."))
            elif row["kind"] == "list":
                states.extend([
                    (f"{widget}-empty", f"`{widget}` is empty with no selection."),
                    (f"{widget}-one", f"`{widget}` has one selected item."),
                    (f"{widget}-overflow", f"`{widget}` has 51 items and a visible scrollbar."),
                ])
    key = page["key"]
    if key in ACTOR_PAGES:
        states.extend([
            ("actor-none", "Rail has no actor; target-dependent rows are disabled with an explicit empty state."),
            ("actor-selected", "One rail actor is selected and unpinned."),
            ("actor-pinned-rail-changes", "Page target is pinned while the rail changes to another actor."),
            ("actor-removed", "The active actor disappears while the page is visible."),
        ])
    if key in LIST_PAGES:
        states.append(("selection-deleted", "The selected model row is removed by another owner while visible."))
    if key in PERMISSION_PAGES:
        states.extend([
            ("permission-denied", "The current target exists but cannot be modified."),
            ("target-offline", "The current target/provider becomes unavailable."),
        ])
    if key in {"gaze_cues_tab", "light_cues_tab"}:
        states.extend([
            ("shared-owner-other-floater", "The standalone cue floater owns edit state; Director remains read-only."),
            ("shared-owner-director", "Director owns edit state; the standalone floater mirrors without claiming."),
            ("cue-playback", "Cue playback is active; history recording is suspended until stop."),
        ])
    if key == "vcam_tab":
        states.extend((f"captures-{count}", f"Registry contains {count} captures.") for count in (0, 1, 4, 5, 8))
        states.extend([
            ("preview-paused", "All preview tiles are paused."),
            ("preview-throttled", "Preview scheduler reports throttling."),
            ("display-raw", "A monitor shows its raw capture."),
            ("display-selected", "A monitor shows a selected display output."),
        ])
    # Preserve order while removing duplicate identifiers.
    seen: set[str] = set()
    result = []
    for state in states:
        if state[0] not in seen:
            seen.add(state[0])
            result.append(state)
    # Size budget for the per-page audit enumeration (every state is still
    # audited). 320 since VP6 (10/12): the Light Rig page reached 262.
    if len(result) > 320:
        raise RuntimeError(f"{key} has {len(result)} audit states; hard cap is 320")
    return result


def row_binding(row: dict, adapter: str) -> str:
    handlers = []
    for hit in [*row["cpp"], *row["callback_handlers"]]:
        if (hit.get("wiring_only") or not hit.get("class") or not hit.get("function")
                or hit.get("function") == "postBuild"):
            continue
        handler = f"behavior `{hit['class']}::{hit['function']}()` (`{hit['file']}:{hit['line']}`)"
        if hit.get("wired_class") and hit.get("wired_function"):
            wired = f"{hit['wired_class']}::{hit['wired_function']}()"
            if wired != f"{hit['class']}::{hit['function']}()":
                handler = f"wired in `{wired}`; {handler}"
        if handler not in handlers:
            handlers.append(handler)
    setting = row["resolved_setting"]
    if setting:
        declaration = row["setting_declarations"][0] if row["setting_declarations"] else None
        type_text = f" `{declaration['type']}`" if declaration and declaration.get("type") else ""
        binding = f"kit `{row['kind']}` ↔ `{setting}`{type_text}"
        if handlers:
            binding += f"; `{adapter}` preserves/calls " + ", ".join(handlers)
        if row.get("shortcut_of"):
            binding += f"; declared SHORTCUT of `{row['shortcut_of']}` (home remains canonical for search/undo)"
        return binding
    if row.get("kit_action"):
        return f"kit-native action `{row['kit_action']}`; no duplicate widget is required"
    if handlers:
        return f"`{adapter}` command/model binding → " + ", ".join(handlers)
    if row.get("callbacks"):
        callbacks = ", ".join(f"`{name}`" for name in row["callbacks"])
        return f"`{adapter}` dispatches registered XUI action {callbacks}"
    if row["kind"] == "readout":
        return "kit readout ← approved static copy; no runtime writer"
    reason = "; ".join(row["unresolved"]) or "no proven binding"
    return f"**UNRESOLVED:** {reason}"


def decision_contract(key: str) -> list[str]:
    if key == "diopter_tab":
        return [
            "", "## User-decision control contract", "",
            "| Director label | Real widget / setting | Exact values | Default |",
            "|---|---|---|---|",
            "| On / off | `diopter_enabled` / `CineDiopterEnabled` (`floater_ultimate_diopter.xml:66`; read by `alfloaterultimatediopter.cpp:556`) | Boolean | off (`settings.xml:608`) |",
            "| Preset | `diopter_preset` / `CineDiopterPreset` (`floater_ultimate_diopter.xml:72`) | U32 0-20; the 21 labels are the XUI items in order | 0, Custom (`settings.xml:615`) |",
            "| Strength | `diopter_blend` / `CineDiopterBlend` (`floater_ultimate_diopter.xml:105`) | 0.00-1.00, step 0.01, 2 decimals | 1.00 (`settings.xml:629`) |",
            "| Focus split | `diopter_power` / `CineDiopterPower` (`floater_ultimate_diopter.xml:287`; read by `alfloaterultimatediopter.cpp:604`) | -10.00 to +10.00 D, step 0.25, 2 decimals | +2.00 D (`settings.xml:867`) |",
            "| Open full editor | existing `ultimate_diopter` floater registration (`llviewerfloaterreg.cpp:769`) | action only | n/a |",
            "", "The Director labels Strength and Focus split are presentation labels only; they bind directly to the existing Master Blend and Power (+D) controls/settings above. Confirm that 'Focus split' reads naturally in the mandatory in-world WP12 check. The full editor is opened, never embedded or recreated.",
            "", "History test: start from any non-Custom preset, edit Focus split/`CineDiopterPower` so `alDiopterHandlePresetTransition()` (`llviewerfloaterreg.cpp:597-626`) materializes Custom, then undo and verify both the prior preset and rendered look return and the saved Custom bank is byte-for-byte unchanged; redo must reproduce the edit.",
        ]
    if key == "light_cues_tab":
        return [
            "", "## Cue-list behavior decided 2026-10-10", "",
            "- Row format is `# | Label | At | Fade | Delay | Curve | Follow`, exactly the fields built by `ALFloaterCineLightCues::refreshCueRows()` (`alfloatercinelightcues.cpp:155-190`).",
            "- Add reuses the Capture path (`onCapture()`, `alfloatercinelightcues.cpp:307-317`); Remove reuses `onDeleteCue()` (`:339-350`); Play calls selected-cue GOTO (`onGoto(false)`, `:374-378`, model `alcinelightrig.cpp:4051`); Stop calls Release (`:381-384`, model `alcinelightrig.cpp:4059`).",
            "- Selecting a Director row opens `cine_light_cues` and selects the same index, then uses `onCueSelected()` (`alfloatercinelightcues.cpp:294-304`) to load the detailed editor. Keyframe/snapshot and timing fields remain only in that window.",
            "- Refresh is event-driven: expose the existing `cueRevision()` (`alcinelightrig.h:314`) as a controller revision signal emitted by `setCueList()`, transport start/stop, list load, and selected-rig change. Refresh once on attach/reveal and on that signal; do not call the floater's `draw()` polling path (`alfloatercinelightcues.cpp:387-392`).",
            "- Add and Remove are one `ALKitHistory` command each, storing the complete before/after `CueList`; Remove is therefore undoable and needs no confirmation. Play and Stop each place one `ALKitHistory::globalBarrier(reason)` and history recording is suspended between them.",
        ]
    if key == "gaze_cues_tab":
        return [
            "", "## Cue-list behavior decided 2026-10-10", "",
            "- Row format is `Time | Target | Macro`, exactly the fields built by `ALFloaterGazeCues::refreshCueList()` (`alfloatergazecues.cpp:235-262`).",
            "- Add reuses `onAdd()` (`alfloatergazecues.cpp:409-417`); Remove reuses `onDelete()` (`:436-452`). Both mutate through `LLDirectorCast::setGazeCues()` (`lldirectorcast.cpp:1364-1397`), which sanitizes, sorts, bumps `getGazeCueRevision()`, and clears gaze runtime.",
            "- Selecting a Director row opens `gaze_cues` for that actor and selects the same cue, then follows `onCueSelected()` (`alfloatergazecues.cpp:395-406`) to load the detailed editor. Detailed targets, macros, transitions, and influence keyframes remain only in that window.",
            "- Existing transport cannot seek gaze time: `LLDirectorCast` exposes only `action()`/`cut()` (`lldirectorcast.h:415-421`, implemented at `lldirectorcast.cpp:2498-2660`), `LLPresentationTime` has no seek/setter, and `LLFlycamRecorder::seek(F32)` (`llflycamrecorder.cpp:284`) affects only recorder playback. Therefore this is explicit **WP8 engine work**, not a call to the pure `evaluateGazeCues()` helper.",
            "- Add exact public APIs `bool LLActorMover::setGazeCuePreview(const LLUUID& actor_id, S32 cue_index);` and `void LLActorMover::clearGazeCuePreview(const LLUUID& actor_id);`. Play calls the first; Stop calls the second. Invalid/unresolved actor or out-of-range index returns false without mutation. One preview per actor stores the cue index and preview-start presentation time; `applyDirectorLookAt()` consults it before normal track evaluation at `llactormover.cpp:6649-6668`, evaluates a one-cue copy on preview-local elapsed time, and never edits the cue list or global presentation clock. Clear/actor removal/logout restores the normal authored/track path and clears Director gaze runtime.",
            "- Refresh is event-driven: add a controller revision signal at `setGazeCues()`'s existing revision bump and subscribe while visible. Refresh once on attach/reveal, actor change, and that signal; do not call the floater's per-frame `draw()` refresh path (`alfloatergazecues.cpp:187-193`).",
            "- Add and Remove are one `ALKitHistory` command each, storing the actor ID and complete before/after `GazeCueList`; Remove is therefore undoable and needs no confirmation. Play and Stop each place one `ALKitHistory::globalBarrier(reason)` and history recording is suspended between them.",
        ]
    return []


def render_page_spec(page: dict) -> str:
    key = page["key"]
    adapter = ADAPTERS[key]
    c = page["classification"]
    lines = [
        f"# {page['label']} — Director Kit page contract",
        "",
        f"Page key: `{key}`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).",
        f"Computed class: **{c['logic_class']}** ({c['setting_bound_rows']}/{page['row_count']} setting-bound rows; {c['cpp_referenced_rows']} C++-referenced rows).",
        "",
        "## Real owners",
        "",
    ]
    if page["owner_roots"]:
        for owner in page["owner_roots"]:
            lines.append(f"- `{owner['name'] or owner['tag']}` / `{owner['tag']}` at `{owner['file']}:{owner['line']}` ({owner['matched_rows']} matching rows).")
    else:
        lines.append("- No approved row widget resolves to XUI.")
    for owner in SPECIAL_OWNERS.get(key, []):
        lines.append(f"- {owner}.")
    if page["owner_classes"]:
        lines.append("- Referencing classes: " + ", ".join(f"`{item['class']}` ({item['reference_count']})" for item in page["owner_classes"]) + ".")
    lines += [
        "",
        "Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.",
        "",
        "## Presentation rule",
        "",
        PRESENTATION_NOTES.get(key, "Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized."),
        "",
        "## Row bindings",
        "",
        f"`{adapter}` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.",
        "",
        "| Card / row | Existing control evidence | Required binding | Other writable homes |",
        "|---|---|---|---|",
    ]
    for row in page["rows"]:
        evidence = ", ".join(
            f"`{hit['tag']}` `{hit['file']}:{hit['line']}`" for hit in row["xui"]
        ) or "none"
        binding = row_binding(row, adapter)
        others = ", ".join(
            f"page `{item['page']}`" if item["kind"] == "page" else f"`{item['file']}:{item['line']}` `{item['widget']}`"
            for item in row["other_writers"]
        ) or "none"
        label = row["label"].replace("|", "\\|")
        card = row["card"] + (f" [{row['option_label']}]" if row.get("option_label") else "")
        notes = "; ".join(row.get("resolution_notes", []))
        if notes:
            binding += f". Contract note: {notes}"
        lines.append(f"| `{card}` / `{row['widget'] or ''}` ({label}) | {evidence} | {binding} | {others} |")

    lines += decision_contract(key)

    methods: dict[tuple, set[int]] = collections.defaultdict(set)
    for row in page["rows"]:
        for hit in [*row["cpp"], *row["callback_handlers"]]:
            if (not hit.get("wiring_only") and hit.get("class") and hit.get("function")
                    and hit.get("function") != "postBuild"):
                methods[(hit["file"], hit["class"], hit["function"])].add(hit["line"])
    for file_name, class_name, function_name, number in SPECIAL_EXTRACTIONS.get(key, []):
        methods[(file_name, class_name, function_name)].add(number)
    lines += ["", "## Adapter extraction", ""]
    if methods:
        lines += [
            "| Existing function | Extraction contract |",
            "|---|---|",
        ]
        for (file_name, class_name, function_name), numbers in sorted(methods.items()):
            number_text = ", ".join(str(number) for number in sorted(numbers))
            lines.append(f"| `{class_name}::{function_name}()` — `{file_name}:{number_text}` | Move model-facing work into `{adapter}` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |")
    else:
        lines.append(f"No approved widget has a C++ literal reference. `{adapter}` owns only explicit actions and setting signals; it must not synthesize a polling mirror.")

    lines += [
        "",
        "## Live state and refresh",
        "",
        "- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.",
        "- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.",
        "- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.",
        "- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.",
        "",
        "## Bounded non-default audit states",
        "",
    ]
    states = page_audit_states(page)
    lines.append(f"Manifest size: **{len(states)} states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.")
    lines += ["", "| State key | Exact scenario |", "|---|---|"]
    for state, description in states:
        lines.append(f"| `{state}` | {description} |")

    unresolved = [row for row in page["rows"] if row["unresolved"]]
    ambiguous = [row for row in page["rows"] if row.get("ambiguous")]
    rejected = [row for row in page["rows"] if row.get("rejected_candidates")]
    lines += [
        "",
        "## Parity check",
        "",
        "1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.",
        "2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.",
        "3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.",
        "4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.",
        "",
        "## Ambiguous same-name candidates (never auto-bound)",
        "",
    ]
    if not ambiguous:
        lines.append("None.")
    else:
        for row in ambiguous:
            candidates = ", ".join(
                f"`{hit['file']}:{hit['line']}` ({hit['reason']})" for hit in row["ambiguous"][:8]
            )
            if len(row["ambiguous"]) > 8:
                candidates += f", plus {len(row['ambiguous']) - 8} more recorded in `page_sources.json`"
            lines.append(f"- `{row['widget'] or '(unnamed)'}`: {candidates}.")
    rejected_count = sum(len(row["rejected_candidates"]) for row in rejected)
    lines += [
        "",
        "## Rejected out-of-scope same-name candidates",
        "",
        (f"{rejected_count} candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`."
         if rejected_count else "None."),
    ]
    lines += [
        "",
        "## New widget required",
        "",
        "None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.",
        "",
        "## Unresolved and pending-decision items",
        "",
    ]
    if not unresolved:
        lines.append("None in the current evidence index.")
    else:
        for row in unresolved:
            reason = "; ".join(row["unresolved"])
            lines.append(f"- `{row['widget'] or '(unnamed)'}` ({row['label']}): {reason}.")

    lines += [
        "",
        "## Risks",
        "",
        "- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.",
        "- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.",
        "- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.",
        "- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.",
        "- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.",
    ]
    return "\n".join(lines).rstrip() + "\n"


def page_specs(index: dict) -> list[tuple[Path, str]]:
    by_key = {page["key"]: page for page in index["pages"]}
    order = R2_FIRST + [page["key"] for page in index["pages"] if page["key"] not in R2_FIRST]
    return [(PAGES_DIR / f"{key}.md", render_page_spec(by_key[key])) for key in order]


def summary_table(index: dict) -> str:
    rows = ["page                 rows owners class         cpp unresolved", "-------------------- ---- ------ ------------- --- ----------"]
    for page in index["pages"]:
        rows.append(
            f"{page['key']:<20} {page['row_count']:>4} {len(page['owner_roots']):>6} "
            f"{page['classification']['logic_class']:<13} {page['classification']['cpp_referenced_rows']:>3} {page['unresolved_row_count']:>10}"
        )
    s = index["summary"]
    rows.append(
        f"TOTAL {s['pages']} pages, {s['rows']} rows, {s['owner_roots']} distinct owner files, "
        f"{s['unresolved_rows']} unresolved rows, {s['multi_page_settings']} multi-page settings"
    )
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if committed outputs differ")
    args = parser.parse_args()
    index = build_index()
    json_text = json.dumps(index, indent=2, sort_keys=False, ensure_ascii=False) + "\n"
    md_text = render_markdown(index)
    stale = []
    outputs = [(JSON_OUT, json_text), (MD_OUT, md_text), *page_specs(index)]
    if index["summary"]["rows"] != index["summary"]["independent_json_rows"]:
        raise RuntimeError("flattened row count differs from independent source-data count")
    if any("?::?()" in content for _, content in outputs):
        raise RuntimeError("generated output contains a placeholder function")
    rendered_unresolved = sum(content.count("**UNRESOLVED:**") for path, content in outputs
                              if path.parent == PAGES_DIR)
    if rendered_unresolved != index["summary"]["unresolved_rows"]:
        raise RuntimeError("rendered unresolved count differs from source index")
    for path, content in outputs:
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                stale.append(rel(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
            if path.parent == PAGES_DIR:
                print(f"wrote {rel(path)}")
    print(summary_table(index))
    if stale:
        print("stale generated files: " + ", ".join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
