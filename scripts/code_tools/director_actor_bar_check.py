"""Exercise the production session actor setter and Cast synchronization without a viewer build."""
from pathlib import Path
from native_check_tools import check_directory, vcvars64
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

from relight_area_policy_check import method as source_method


# Forward integration order; each baseline is immutable main plus earlier layers.
ACTOR_KEY_LAYERS = (('actor_key_wp01_hunks.json', 'dc44aa60b218927b77f4f521240063afe57136c341bc1898177c75ee839f0580'), ('actor_key_wp2_hunks.json', 'f3a43e5c9d72ce23e55e7692d985609b36e11c8e1d177828dd707b5b9754baf8'), ('actor_key_lens_hunks.json', '844681a5ef69be59d5b25963eb31c390313762ad9877fafd4cd5845d887745b8'), ('actor_key_wp1b_hunks.json', '61e677b1d11538f03a399943091deeab599c599cd6d0d8904064c52264d80edf'), ('actor_key_shadow_hunks.json', 'b1575e291227801c5ed9b2ab5645ed52964b3f1651e7dc9d53a34fb2780d6b67'), ('actor_key_wp5_ui_hunks.json', 'c12e8b597cf14cd0a6fb06484ca436acbb8595bbe833e6a078311a1fd8231130'), ('actor_key_fix_all_hunks.json', 'aaca4e8c58bf144096032c4765adfb5287840dc2b15f1ce837ba76edb146017a'), ('actor_key_noguard_hunks.json', '580c6cd973d4e90cb196cb1f66070a4320b28054ee271eb0aab06a2f4d85d926'))


CLEAR_ANIM_DIGEST = "4bed3598af86a7f82bd07b9a00f6cf83f056e157914cd4d12e9603b3690d6b3c"
GAZE_NEARBY_DIGEST = "6c575e2bd500ef4f4ff972d267a0e6363eec5e035f7455b5681ffb5c1a051856"


HOT_GAZE_QUERY_DIGEST = "408b82b9c15d87a42abd5ad31ceeb23e3d6f1c8f61c0ef2276bbf7e01ad2d1b0"


def reverse_hot_gaze_query_mover(source, name):
    """Peel the newest sanctioned query layer first; idempotent, exact full-file base."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    filename = "hot_gaze_query_mover_hunks.json"
    raw = (Path(__file__).parent / filename).read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == HOT_GAZE_QUERY_DIGEST, "hot gaze query allowance pin"
    allowed = json.loads(raw)
    assert allowed["base"] == "f2b73c3d416", "hot gaze query base"
    if name not in allowed["files"] or not any(h["after"] in source for h in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, filename, HOT_GAZE_QUERY_DIGEST,
                                     "reviewed owned hot gaze query")


def reverse_gaze_nearby_mover(source, name):
    """Peel the reviewed Gaze Nearby hunks (the newest layer, on top of Clear animations).

    Skipped when the hunks are already gone; the older layers below still demand the exact
    older source, so nothing unreviewed is accepted. The layer is the gaze status line only.
    """
    source = reverse_hot_gaze_query_mover(source, name)
    if name != "llactormover.cpp":
        return source
    import json
    source = source.replace("\r\n", "\n")
    allowed = json.loads((Path(__file__).parent / "gaze_nearby_mover_hunks.json").read_bytes().replace(b"\r\n", b"\n"))
    if not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "gaze_nearby_mover_hunks.json", GAZE_NEARBY_DIGEST,
                                     "reviewed Gaze Nearby")


def reverse_clear_anim_mover(source, name):
    """Peel the reviewed Clear animations hunks (the newest layer) from the mover files.

    Skipped when the hunks are already gone (a caller that peeled them first); the baseline
    chain below still demands the exact older source, so nothing unreviewed is accepted.
    """
    if name not in ("llactormover.cpp", "llactormover.h"):
        return source
    source = source.replace("\r\n", "\n")
    source = reverse_hot_gaze_query_mover(source, name)
    source = reverse_gaze_nearby_mover(source, name)
    import json
    allowed = json.loads((Path(__file__).parent / "clear_anim_studio_hunks.json").read_bytes().replace(b"\r\n", b"\n"))
    if not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "clear_anim_studio_hunks.json", CLEAR_ANIM_DIGEST,
                                     "reviewed Clear animations")


def actor_key_baseline(source, name):
    """Peel only exact key deltas; full-file equality rejects unrelated edits."""
    from clone_rest_frame_pins import reverse_rest_frame_mover
    source = reverse_clear_anim_mover(source, name)
    source = reverse_director_groups_cast(source, name)
    source = reverse_rig_no_group_cast(source, name)
    source = reverse_rest_frame_mover(source, name)
    source = reverse_feature_allowance(source, name, "clone_fx_mover_hunks.json",
        "9ccf4c83774ff89260a93adb6b76706773b88da5bc5389b2fa85d61bc48fa846", "six session-only clone FX")
    source = reverse_feature_allowance(source, name, "legacy_animation_clock_hunks.json",
        "a47ad5eb15ded19a917512b5f569847f22cfd9e7294e45d324c342685b4ae775", "legacy animation bounded clock")
    source = reverse_feature_allowance(source, name, "looks16_mover_hunks.json",
        "6a5a743be0cb78ff444140532b082538a94b71d38d5d6f5e846b99ef4747b029", "cinematic looks 85-100")
    import hashlib, json
    path = "indra/newview/" + name
    layers = []
    for filename, digest in ACTOR_KEY_LAYERS:
        raw = (Path(__file__).parent / filename).read_bytes().replace(b"\r\n", b"\n")
        assert hashlib.sha256(raw).hexdigest() == digest, (filename, "allowance pin")
        data = json.loads(raw)
        assert data["integration_base"] == "ab2329ba39f", (filename, "integration base")
        assert data["integration_predecessors"] == [item[0] for item in layers], (filename, "layer order")
        files = data.get("files", {"indra/newview/pipeline.cpp": data.get("hunks", [])})
        layers.append((filename, files))
    if not any(path in files for _, files in layers):
        return source
    baseline = subprocess.check_output(["git", "show", "ab2329ba39f:" + path]).decode("utf-8").replace("\r\n", "\n")
    checkpoints = []
    for filename, files in layers:
        before = baseline
        for hunk in files.get(path, []):
            assert baseline.count(hunk["before"]) == 1, (filename, "baseline hunk", name)
            baseline = baseline.replace(hunk["before"], hunk["after"], 1)
        checkpoints.append((filename, files.get(path, []), before, baseline))
    for filename, hunks, before, after in reversed(checkpoints):
        assert source == after, (filename, "full feature source", name)
        for hunk in hunks:
            assert source.count(hunk["after"]) == 1, (filename, "exact feature hunk", name)
            source = source.replace(hunk["after"], hunk["before"], 1)
        assert source == before, (filename, "reversal must reproduce exact baseline", name)
    return source


def reverse_post_chroma_main(source, name):
    """Reverse the pinned main edits made after the looks branch left it; the whole branch file must return."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "post_chroma_main_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == "54c1dfa5a34174b277dfec9072386a81d0f9eaf0c099fe34c9232f68a4f447e2", "post-chroma allowance pin"
    data = json.loads(raw)
    path = "indra/newview/" + name
    if path not in data["files"]:
        return source
    for hunk in data["files"][path]:
        assert source.count(hunk["after"]) == 1, ("post-chroma hunk", name)
        source = source.replace(hunk["after"], hunk["before"], 1)
    base = subprocess.check_output(["git", "show", data["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    assert source == base, ("post-chroma reversal must reproduce the base", name)
    return source


def reverse_feature_allowance(source, name, filename, digest, label):
    """Reverse one pinned layer, then require its entire historical baseline."""
    import hashlib
    import json
    raw = (Path(__file__).parent / filename).read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == digest, (label, "allowance pin")
    allowed = json.loads(raw)
    if name not in allowed["files"]:
        return source
    original = subprocess.check_output([
        "git", "show", allowed["base"] + ":indra/newview/" + name
    ]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowed["files"][name]:
        assert original.count(hunk["before"]) == 1, (label, "baseline hunk", name)
        assert source.count(hunk["after"]) == 1, (label, "exact feature hunk", name)
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == original, (label, "reversal must reproduce its exact baseline", name)
    return source


SPOT_SHADOWS_16_DIGEST = "1fec21dea70c05465499dbff45160c9b7f185f4ce671912b055bf67f54536827"


def reverse_spot_shadows_16(source, name):
    """Peel the exact 16-slot ceiling layer before all older pipeline pins."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "spot_shadows_16_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SPOT_SHADOWS_16_DIGEST, "16-slot allowance pin"
    allowed = json.loads(raw)
    assert allowed["base"] == "fb616f2519c", "16-slot base"
    if name not in allowed["files"] or not any(h["after"] in source for h in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "spot_shadows_16_hunks.json",
                                     SPOT_SHADOWS_16_DIGEST, "16-slot projector ceiling")


SHAFT_SLOT_MERGE_DIGEST = "899d39e572e8a7718d6cb35f8bb40c7d6f2d6a472b8a999774210d92102f6ba6"


def reverse_shaft_slot_merge(source, name):
    """Peel the newest pipeline layer; preserve all older full-file pins."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    source = reverse_spot_shadows_16(source, name)
    raw = (Path(__file__).parent / "shaft_slot_merge_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SHAFT_SLOT_MERGE_DIGEST, ("reviewed shaft slot merge", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "8c350a71b4e", "shaft slot merge base"
    if name not in allowed["files"] or not any(h["after"] in source for h in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "shaft_slot_merge_hunks.json",
                                     SHAFT_SLOT_MERGE_DIGEST, "reviewed shaft slot merge")


SOFTSHADOW_METRIC_DIGEST = "e6df75b85308e69fcfae1d6862e6bc22b70b52d4f972f9a36ecc030d67b330da"


def reverse_softshadow_metric(source, name):
    """Peel the newest pipeline layer (soft shadow fix 4: the per-slot metric penumbra upload after the per-slot
    softness upload) before the projector fade layer (reverse_softshadow_fade calls this first). Idempotent: skipped
    when its hunk is already gone; the pinned digest is checked every time. Exact: removing the one reviewed hunk must
    reproduce the complete 28c8c83af9d file (main with the fade layer, which is peeled next)."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_metric_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_METRIC_DIGEST, ("reviewed soft shadow metric", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "28c8c83af9d", "soft shadow metric base"
    assert set(allowed["files"]) == {"pipeline.cpp"} and len(allowed["files"]["pipeline.cpp"]) == 1, "soft shadow metric scope"
    if name not in allowed["files"] or not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "softshadow_metric_hunks.json",
                                     SOFTSHADOW_METRIC_DIGEST, "reviewed soft shadow metric")


SOFTSHADOW_METRIC_SHADER_DIGEST = "ec16b3ef545120791d26aa8c6f73aca9e4fa2b47e26356a8ac0d0b64a511b033"


def reverse_softshadow_metric_shader(source):
    """Peel the reviewed soft shadow fix 4 edits of class1/deferred/shadowUtil.glsl (metric projector penumbra) before
    the projector fade layer (relight_spot_bias_gl_check.reverse_fade_layer calls this first). Idempotent: skipped when
    none of its hunks is present. Exact: every pinned hunk must be present once and reversing them must reproduce the
    28c8c83af9d file byte for byte (main with the fade layer, which is peeled next)."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_metric_shader_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_METRIC_SHADER_DIGEST, ("reviewed soft shadow metric shader", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "28c8c83af9d", "soft shadow metric shader base"
    assert set(allowed["files"]) == {"shadowUtil.glsl"} and len(allowed["files"]["shadowUtil.glsl"]) == 2, "soft shadow metric shader scope"
    if not any(hunk["after"] in source for hunk in allowed["files"]["shadowUtil.glsl"]):
        return source
    path = "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl"
    original = subprocess.check_output(["git", "show", allowed["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowed["files"]["shadowUtil.glsl"]:
        assert original.count(hunk["before"]) == 1, "soft shadow metric shader baseline hunk"
        assert source.count(hunk["after"]) == 1, "soft shadow metric shader hunk"
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == original, "soft shadow metric shader reversal must reproduce its exact baseline"
    return source


SOFTSHADOW_QUALITY_DIGEST = "27dc363be5445b13fcda058ff9b818a8752b6023a80a34e95547efbf4d237105"


def reverse_softshadow_quality(source, name):
    """Peel the newest pipeline layer (soft shadow quality uniforms: fill step, noise model, sun light-map dead
    samples) before the projector bias layer. Idempotent: skipped when its hunk is already gone; the pinned digest
    is checked every time. Exact: removing the one reviewed hunk must reproduce the complete c63f7ca1b2f file."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_quality_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_QUALITY_DIGEST, ("reviewed soft shadow quality", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "c63f7ca1b2f", "soft shadow quality base"
    assert set(allowed["files"]) == {"pipeline.cpp"} and len(allowed["files"]["pipeline.cpp"]) == 1, "soft shadow quality scope"
    if name not in allowed["files"] or not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "softshadow_quality_hunks.json",
                                     SOFTSHADOW_QUALITY_DIGEST, "reviewed soft shadow quality")


SOFTSHADOW_QUALITY_SHADER_DIGEST = "e79590a0bcc709255adf5e2d6cd7985e91fe0630658e0ce3263b3c81f3f42715"


def reverse_softshadow_quality_shader(source):
    """Peel the reviewed soft shadow quality edits of class1/deferred/shadowUtil.glsl (fill step retired, noise
    hygiene). Exact: every pinned hunk must be present once and reversing them must reproduce the c63f7ca1b2f file
    byte for byte, so any other edit still fails the checks that compare against an older reference."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_quality_shader_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_QUALITY_SHADER_DIGEST, ("reviewed soft shadow quality shader", "allowance pin")
    allowed = json.loads(raw)
    path = "indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl"
    original = subprocess.check_output(["git", "show", allowed["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowed["files"]["shadowUtil.glsl"]:
        assert source.count(hunk["after"]) == 1, "soft shadow quality shader hunk"
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == original, "soft shadow quality shader reversal must reproduce its exact baseline"
    return source


SOFTSHADOW_FADE_DIGEST = "e2d360ac0c0885ce032026305d7f3378e8d90159568301ffcc1067ef26e70ad2"


def reverse_softshadow_fade(source, name):
    """Peel the newest pipeline layer (projector fade-model uniform + hand-over clip).

    Exact and never optional for pipeline.cpp: both reviewed hunks must be present
    once, and removing them must reproduce the complete 1d1162aaf77 file (main with
    the soft shadow quality layer, which is peeled next), so a missing, duplicated
    or edited hunk and any other edit still fail.
    """
    import hashlib
    import json
    # Newest first: the metric penumbra upload (built on this layer) is peeled exactly to this layer's own file.
    source = reverse_softshadow_metric(source, name)
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_fade_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_FADE_DIGEST, ("reviewed soft shadow fade", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "1d1162aaf77", "soft shadow fade base"
    assert set(allowed["files"]) == {"pipeline.cpp"} and len(allowed["files"]["pipeline.cpp"]) == 2, "soft shadow fade scope"
    return reverse_feature_allowance(source, name, "softshadow_fade_hunks.json",
                                     SOFTSHADOW_FADE_DIGEST, "reviewed soft shadow fade")


SOFTSHADOW_BIAS_DIGEST = "d3eb81e27c60fed3adb00978c31ea7482c5df255462a727dc31942c543b4493b"


def reverse_softshadow_bias(source, name):
    """Peel the projector bias-model uniform layer before the shaft slot merge.

    Exact: the one reviewed hunk must be present once, and removing it must
    reproduce the complete fb616f2519c file, so any other edit still fails.
    The newer fade-model layer (reverse_softshadow_fade) is peeled first, here,
    so every caller of this function gets the full chain newest-first.
    """
    import hashlib
    import json
    # Newest first: the projector fade model (built on the soft shadow quality layer), then the quality layer.
    source = reverse_softshadow_fade(source, name)
    source = reverse_softshadow_quality(source, name)
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "softshadow_bias_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == SOFTSHADOW_BIAS_DIGEST, ("reviewed soft shadow bias", "allowance pin")
    allowed = json.loads(raw)
    assert allowed["base"] == "fb616f2519c", "soft shadow bias base"
    assert set(allowed["files"]) == {"pipeline.cpp"} and len(allowed["files"]["pipeline.cpp"]) == 1, "soft shadow bias scope"
    return reverse_feature_allowance(source, name, "softshadow_bias_hunks.json",
                                     SOFTSHADOW_BIAS_DIGEST, "reviewed soft shadow bias")


ENTITY_SPAWN_ON_SOURCE_DIGEST = "63ce56881cbc42b08045b590eb39c0bcd7018d19795efc5c79a18bca39217068"


def reverse_entity_spawn_on_source(source, name):
    """Peel the entity spawn-on-source layer (the newest layer on alghoststudio.cpp). Idempotent: skipped when
    its hunk is already gone; the pinned digest is checked every time."""
    import hashlib
    import json
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "entity_spawn_on_source_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == ENTITY_SPAWN_ON_SOURCE_DIGEST, ("reviewed entity spawn on source", "allowance pin")
    allowed = json.loads(raw)
    if name not in allowed["files"] or not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "entity_spawn_on_source_hunks.json",
                                     ENTITY_SPAWN_ON_SOURCE_DIGEST, "reviewed entity spawn on source")


CLONE_SELECT_PICK_DIGEST = "6cc20778f4d8f6908c39f607ed4e5b6acf6b3e8b854b3d05fb0d337a737f4f86"


def reverse_clone_select_pick(source, name):
    """Peel the clone world-pick layer (the body-box pick), after the newer spawn-on-source layer. Idempotent:
    skipped when its hunks are already gone; the pinned digest is checked every time."""
    import hashlib
    import json
    source = reverse_entity_spawn_on_source(source, name)
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "clone_select_pick_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == CLONE_SELECT_PICK_DIGEST, ("reviewed clone world pick", "allowance pin")
    allowed = json.loads(raw)
    if name not in allowed["files"] or not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "clone_select_pick_hunks.json", CLONE_SELECT_PICK_DIGEST, "reviewed clone world pick")


def reverse_clone_menu_fade(source, name):
    """Peel the clone menu's reviewed fade layer (the newest layer on alghoststudio.cpp, lldirectorcast.cpp and
    lldirectorcast.h). Idempotent: skipped when its hunks are already gone; the pinned digest is checked every time."""
    import hashlib
    import json
    source = reverse_clone_select_pick(source, name)
    source = source.replace("\r\n", "\n")
    raw = (Path(__file__).parent / "clone_menu_fade_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == CLONE_MENU_FADE_DIGEST, ("reviewed clone menu fade", "allowance pin")
    allowed = json.loads(raw)
    if name not in allowed["files"] or not any(hunk["after"] in source for hunk in allowed["files"][name]):
        return source
    return reverse_feature_allowance(source, name, "clone_menu_fade_hunks.json", CLONE_MENU_FADE_DIGEST, "reviewed clone menu fade")


def reverse_director_groups_cast(source, name):
    source = reverse_clone_menu_fade(source, name)
    source = reverse_feature_allowance(source, name, "director_groups_fable_hunks.json",
        "6c3d5788ca2d7b1557cd9f48822a70ee3f442366e0b3ff1c05e966ebabd9d4d1", "reviewed Director groups fixes")
    source = reverse_feature_allowance(source, name, "rig_lit_group_cast_hunks.json",
        "35c6ab1cb0144851a6fe8201e9665431ee234db68cd89c4a7e3ccc950a9e0a17", "reviewed rig lit-group no-name resolution")
    # Groups integrate on top of rig-no-group; peel them before that older layer.
    return reverse_feature_allowance(source, name, "director_groups_cast_hunks.json",
        "320a044ff24581975b613b3d6defa3ffffe1cfe285dba574f61ad7a66c29f144", "reviewed Director groups cast")


def reverse_rig_no_group_cast(source, name):
    return reverse_feature_allowance(source, name, "rig_no_group_cast_hunks.json",
        "5d967cf95fada38b1bbe10eb5aac5bdae6c996a1643298760138c31301652e14", "reviewed rig-no-group cast")


CLONE_MENU_FADE_DIGEST = "a234366fff8b306290e1c9bbd2ef082a37c256cf24a6bd72c85ace5217a47003"
TAG_HIGHLIGHT_DIGEST = "ab65349f18b818569a996d2c1acc5d2c9eef1461db95f348f111d9658dbe0461"
TAG_HIGHLIGHT_PIPELINE_DIGEST = "b7907a9921708e57910c4b9b778caf08e683674a7a32a7d62d465560054f1ba5"


def reverse_clone_anim_drop(source, name):
    source = reverse_hot_gaze_query_mover(source, name)
    # The clone menu's hidden-only fade sits on top of main 831f6cd3ebb; peel it first (newest layer).
    source = reverse_clone_menu_fade(source, name)
    # The Director tag highlight sits on top of main fc5f3e5a78f; peel it next.
    source = reverse_feature_allowance(source, name, "tag_highlight_studio_hunks.json",
        TAG_HIGHLIGHT_DIGEST, "reviewed Director tag highlight")
    # Clear animations integrates on top of 23c45e6ee7f; peel it next.
    source = reverse_feature_allowance(source, name, "clear_anim_studio_hunks.json",
        CLEAR_ANIM_DIGEST, "reviewed Clear animations")
    # Clone animation drop integrates on top of main 845dfff22e9; peel it before
    # the post-chroma reversal that pins that main state.
    return reverse_feature_allowance(source, name, "clone_anim_drop_hunks.json",
        "621dfd76f4562913468a6b127b7f254773b5812b4ff04adb8f5739054f62acf0", "reviewed clone animation drop")


def hot_gaze_baseline(source, name):
    # Peel independent layers newest first. In particular, retiring renderer
    # looks must preserve interplay until its own exact allowance reverses it.
    for filename, digest, label in (
        ("matrix_mover_hunks.json",
         "f1084f70936ab1631b2a81b0ffb5bdb74e59320c2b509600c084a3babe32cb20", "Matrix"),
        ("ashfix_mover_hunks.json",
         "f345ee66bf29b1e58aebb895667f0b35e88b7569461f9ffe3ded53767071b135", "retired-look"),
        ("hot_clone_interplay_hunks.json",
         "4169dfa99685ba2a6d85308c1c4e1bfb8e490fb777f45fd9b3f6e76bfc140337", "Hot Gaze / clone interplay"),
        ("hot_gaze_mover_hunks.json",
         "c352097782c87b09eafae9ed80b081bb71314862dfabfd5ecb8292d14f37c411", "Hot Gaze"),
    ):
        source = reverse_feature_allowance(source, name, filename, digest, label)
    return source


def method(path, signature):
    # The old actor-bar fixture still executes the same production transaction
    # code. New Hot Gaze behavior is exercised by its own native contract and
    # the extended clone transaction fixture, without broadening old stubs.
    if path.name in ("llactormover.cpp", "lldirectorcast.cpp") and path.parent.name == "newview":
        # The clone menu's fade layer (incl. the coverage of a clone that is not a Cast member) is the newest layer.
        text = reverse_clone_menu_fade(path.read_text(encoding="utf-8"), path.name)
        source = hot_gaze_baseline(actor_key_baseline(text, path.name), path.name)
        baseline = check_directory("actorbar_baseline") / path.name
        baseline.parent.mkdir(parents=True, exist_ok=True)
        baseline.write_text(source, encoding="utf-8")
        return source_method(baseline, signature)
    return source_method(path, signature)


def reverse_qol_allowance(path, source):
    """Exact reviewed feature delta; unrelated content remains baseline guarded."""
    import hashlib
    import json
    raw = (Path(__file__).parent / "clone_qol_renderer_hunks.json").read_text(encoding="utf-8").replace("\r\n", "\n").encode()
    assert hashlib.sha256(raw).hexdigest() == "1537cae0db0e2597e140d50937a7eb768afb729365bf2a095ae4b7fbe2aa7a0e", "QoL allowance pin"
    allowance = json.loads(raw)
    baseline = subprocess.check_output(["git", "show", allowance["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowance["files"][path]:
        assert baseline.count(hunk["before"]) == 1, ("QoL baseline hunk", path)
        assert source.count(hunk["after"]) == 1, ("QoL exact feature hunk", path)
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == baseline, ("QoL reversal must reproduce its exact baseline", path)
    return source


def main():
    nv = Path("indra/newview")
    actor_key_baseline((nv / "llactormover.h").read_text(encoding="utf-8"), "llactormover.h")
    xui = nv / "skins/default/xui/en"
    removed = {"anim_speed_avatar", "gaze_edit_actor", "sa_actor_target", "actor_style_target"}
    index = (xui / "director_kit_index.xml").read_text(encoding="utf-8")
    assert all("\t" + name + "\t" not in index for name in removed)
    shell = ET.parse(xui / "floater_director_v2.xml").getroot()
    bar = shell.find(".//*[@name='kit_actor_bar']")
    assert bar is not None
    assert {"kit_actor_" + name for name in ("name", "status", "choose", "selected", "you", "clear")} <= {
        node.get("name") for node in bar}
    for path in xui.glob("panel_dpage_*.xml"):
        tree = ET.parse(path).getroot()
        assert not removed & {node.get("widget") for node in tree.iter("kit_row")}
    preserved = {"panel_dpage_performance_gaze.xml": "gaze_cast_combo",
                 "panel_dpage_cast_path.xml": "copy_to_combo",
                 "panel_dpage_cast_move.xml": "scope_radio",
                 "panel_dpage_look_stylize.xml": "ri_subject_targetset_combo",
                 "panel_dpage_look_tron.xml": "tr_subject_target_set_combo"}
    for path, widget in preserved.items():
        assert ET.parse(xui / path).find(".//*[@widget='" + widget + "']") is not None
    independent = {"panel_dpage_look_stylize.xml": "ri_subject_target_combo",
                   "panel_dpage_look_tron.xml": "tr_subject_target_combo",
                   "panel_dpage_world_shadows_probes.xml": "sd_subject_target_combo"}
    for path, widget in independent.items():
        assert ET.parse(xui / path).find(".//*[@widget='" + widget + "']") is not None
        assert "\t" + widget + "\t" in index
    import subprocess
    for path in ("pipeline.cpp", "pipeline.h", "alfloaterlightbox.cpp"):
        # The Director tag highlight's exact renderer-selection hooks sit on top of main fc5f3e5a78f.
        # Newest first: the projector fade-model layer (peeled inside reverse_softshadow_bias), the bias-model uniform
        # (both pipeline.cpp only), then the shaft slot merge.
        tagged = reverse_feature_allowance(reverse_shaft_slot_merge(reverse_softshadow_bias((nv / path).read_text(encoding="utf-8"), path), path), path,
            "tag_highlight_pipeline_hunks.json", TAG_HIGHLIGHT_PIPELINE_DIGEST, "reviewed Director tag highlight renderer hooks")
        guarded = reverse_post_chroma_main(tagged, path)
        guarded = actor_key_baseline(guarded, path)
        if path == "pipeline.cpp":
            # Remove only the two pinned blink hooks before reversing clone QoL.
            import hashlib, json
            blink_bytes = (Path(__file__).parent / "pov_blink_pipeline_hunks.json").read_bytes().replace(b"\r\n", b"\n")
            assert hashlib.sha256(blink_bytes).hexdigest() == "acdd6aced21f50d2cae538d9746cffbfa77e51dd38af76988c48201b4f9fb2ee", "blink allowance pin"
            blink = json.loads(blink_bytes)
            blink_base = subprocess.check_output(["git", "show", blink["base"] + ":indra/newview/pipeline.cpp"]).decode("utf-8").replace("\r\n", "\n")
            for hunk in blink["hunks"]:
                assert blink_base.count(hunk["before"]) == 1, "blink baseline hunk"
                assert guarded.count(hunk["after"]) == 1, "blink exact feature hunk"
                guarded = guarded.replace(hunk["after"], hunk["before"], 1)
            guarded = reverse_qol_allowance("indra/newview/" + path, guarded)
        assert guarded.encode() == subprocess.check_output(
            ["git", "show", "c5dd757d341:indra/newview/" + path]).replace(b"\r\n", b"\n"), path
    # D1 includes the subject display adapter, not just rows and renderer reads.
    kit = (nv / "alkitpage.cpp").read_text()
    base_kit = subprocess.check_output(["git", "show", "c5dd757d341:indra/newview/alkitpage.cpp"]).decode("utf-8")
    def subject_adapter(source):
        return source[source.index('    struct ComboSpec'):source.index('\n    return true;', source.index('    struct ComboSpec'))].replace('\r\n', '\n')
    assert subject_adapter(kit) == subject_adapter(base_kit)
    save = method(nv / "lldirectorcast.cpp", "LLSD LLDirectorCast::sceneData() const")
    load = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::applySceneData(")
    assert 'data["stepped_actor"] = getSteppedActorId()' in save
    assert 'LLActorMover::instance().pathSceneData(m.mId)' in save
    assert 'data.has("stepped_actor")' in load and 'data["stepped_actor"].asUUID(), scene_actor_ids' in load
    assert 'if (has_stepped_actor) setActiveActorId(isSelf(stepped_actor) ? gAgentID : stepped_actor)' in load
    assert 'if (!has_stepped_actor && !isSelf(getActiveActorId()) && !contains(getActiveActorId()))' in load
    assert 'm.mId = loadedSceneMemberId(e["id"].asUUID(), scene_actor_ids)' in load
    assert 'if (alias) *alias = m;' in load
    assert 'clone->mKind != ALGhostStudio::BACKING_ENTITY_CLONE) continue' in load
    assert 'if (previous_id == e["id"].asUUID()) continue' not in load
    assert 'Actor alias collision on load' in load and 'keeping latter scene record' in load
    assert 'LLActorMover::instance().dropActor(m.mId)' in load
    assert all('loadedSceneMemberId(data["subject_' + slot + '"]' in load for slot in 'abcd')
    assert 'has_stepped_actor ? loadedSceneMemberId(' in load
    assert not (nv / "aldirectoractorstate.h").exists()
    for path in ("lldirectorcast.cpp", "lldirectorcast.h", "llactormover.cpp", "llactormover.h"):
        source = (nv / path).read_text()
        for obsolete in ('aldirectoractorstate.h', 'mAuthoredState', 'mActorAuthorOrder',
                         'authored_fields', 'captureActorEdits', 'mergeActorMembers'):
            assert obsolete not in source, (path, obsolete)
    # Exact round-2 implementations protect ordinary edits/undo and linear
    # teardown, including costs that a value-only fixture cannot detect.
    base_cast = subprocess.check_output(["git", "show", "890f04b10d6:indra/newview/lldirectorcast.cpp"]).decode()
    base_mover = subprocess.check_output(["git", "show", "890f04b10d6:indra/newview/llactormover.cpp"]).decode()
    normalize = lambda text: text.replace('\r\n', '\n')
    # Round-2 mover plus Clone Studio look registration and the two exact
    # transaction-time reference-rewrite methods below. Hashes pin their full
    # content: an unrelated mover edit still fails the original baseline guard.
    current_mover = actor_key_baseline(normalize((nv / "llactormover.cpp").read_text(encoding="utf-8")), "llactormover.cpp")
    import hashlib
    rekey_methods = {
        'bool LLActorMover::rewriteGazeTarget(': '334e1f5af6f3cf254d171550133cc40df87ac11c96500cde799a8baf4056767f',
        'bool LLActorMover::rewriteActorReferences(': 'fc6510b35824ac02735fad71f323a4254048ec9164dd97faebc1b36e333e7a95',
    }
    # Guard every additional file covered by the new layers, including Studio
    # where retirement and interplay both changed the Hot Gaze implementation.
    for name in ("alghoststudio.cpp", "llactormover.h", "lldirectorcast.h"):
        hot_gaze_baseline(actor_key_baseline(reverse_post_chroma_main(
            reverse_clone_anim_drop((nv / name).read_text(encoding="utf-8"), name), name), name), name)
    guarded_mover = hot_gaze_baseline(current_mover, "llactormover.cpp")
    # Reversal order: key WP5/WP4/WP1b/LENS/WP2/WP01, Matrix, retired looks, interplay, Hot Gaze, QoL, phase 2,
    # rekey, phase 1, then the unchanged original round-2 guard.
    guarded_mover = reverse_qol_allowance("indra/newview/llactormover.cpp", guarded_mover)
    import json
    phase2_bytes = normalize((Path(__file__).parent / 'clone_looks2_mover_hunks.json').read_text(encoding='utf-8')).encode()
    assert hashlib.sha256(phase2_bytes).hexdigest() == '853c3b49b143ef58c488f9ce4af4b1eb70cbed005ebd0752924e612d3ebf9bbc'
    phase2 = json.loads(phase2_bytes)
    phase2_base = normalize(subprocess.check_output([
        'git', 'show', phase2['base'] + ':indra/newview/llactormover.cpp']).decode())
    for hunk in phase2['hunks']:
        assert phase2_base.count(hunk['before']) == 1, 'phase-2 baseline hunk'
        assert guarded_mover.count(hunk['after']) == 1, 'phase-2 exact feature hunk'
        guarded_mover = guarded_mover.replace(hunk['after'], hunk['before'], 1)
    assert guarded_mover == phase2_base, 'phase-2 reversal must reproduce its exact baseline'

    for signature, digest in rekey_methods.items():
        body = normalize(method(nv / "llactormover.cpp", signature))
        assert hashlib.sha256(body.encode()).hexdigest() == digest, signature
        assert guarded_mover.count(body) == 1, signature
        guarded_mover = guarded_mover.replace(body + "\n\n", "", 1)
    # Three existing overlay source reads must reject a deleted dependency;
    # each exact replacement is count-checked before restoring the baseline.
    for old, new, count in (
        ("LLVOAvatar* av = resolve_actor(inst.mSource);",
         "LLVOAvatar* av = inst.mSourceRemoved ? nullptr : resolve_actor(inst.mSource);", 2),
        ("else if (LLVOAvatar* av = resolve_actor(inst->mSource))",
         "else if (LLVOAvatar* av = inst->mSourceRemoved ? nullptr : resolve_actor(inst->mSource))", 1),
    ):
        assert guarded_mover.count(new) == count, new
        guarded_mover = guarded_mover.replace(new, old)
    if guarded_mover != normalize(base_mover):
        import difflib
        looks_old = normalize(subprocess.check_output(["git", "show", "c5dd757d341:indra/newview/llactormover.cpp"]).decode())
        looks_new = normalize(subprocess.check_output(["git", "show", "ba1e2f1e0bb:indra/newview/llactormover.cpp"]).decode())
        # Reverse exact phase-1 hunks from fixed reviewed commits. A set of
        # changed lines could hide duplicates, reordering or removed context.
        old_lines, new_lines = looks_old.splitlines(True), looks_new.splitlines(True)
        for group in difflib.SequenceMatcher(None, old_lines, new_lines, autojunk=False).get_grouped_opcodes(3):
            before = ''.join(old_lines[group[0][1]:group[-1][2]])
            after = ''.join(new_lines[group[0][3]:group[-1][4]])
            assert looks_old.count(before) == guarded_mover.count(after) == 1, 'phase-1 exact feature hunk'
            guarded_mover = guarded_mover.replace(after, before, 1)
    assert guarded_mover == normalize(base_mover), 'original round-2 mover baseline must match exactly'
    baseline = check_directory("actorbar_baseline") / "actor_round2_cast.cpp"
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(base_cast, encoding="utf-8")
    for signature in ('void LLDirectorCast::touchScene()', 'LLSD LLDirectorCast::sceneData() const',
                      'void LLDirectorCast::setActorStyle(', 'LLSD writeActorStyle(',
                      'LLDirectorCast::ActorStyle readActorStyle('):
        assert normalize(method(nv / "lldirectorcast.cpp", signature)) == normalize(method(baseline, signature)), signature
    teardown_end = '    // Build the legacy-to-stable identity map'
    assert load[:load.index(teardown_end)] == method(baseline, 'void LLDirectorCast::applySceneData(').split(teardown_end)[0]
    runtime = (nv / "alsteppedanimruntime.cpp").read_text()
    assert 'const LLUUID selected = cast.getSteppedActorId()' in runtime
    assert 'f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected' in runtime
    assert 'return runtimeActorId(mActiveActorId)' in (nv / "lldirectorcast.h").read_text()
    group = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::onGroupsListSelect(")
    assert "selectGroupMembers(name);" not in group
    assert "setActiveActorId" not in group and "touchScene" not in group
    group = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::selectGroupMembers(")
    assert group.index('selectMultiple(cast.membersInGroup(name))') < group.index('cast.setActiveActorId(firstSelectedCastId())')
    assert 'mShell->setCastSelection(selectedCastIds())' in group
    chooser = (nv / "alavatarsubjectpicker.cpp").read_text()
    assert 'if (animesh)\n        for (const auto& clone' in chooser
    assert 'clone.mKind == ALGhostStudio::BACKING_ENTITY_CLONE && clone.mEntityId.notNull()' in chooser
    assert 'if (ghosts)\n        for (const auto& clone' not in chooser
    sync = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor(")
    assert 'setCastSelection' not in sync # independent Light Rig adapters never receive a bar mirror
    setter = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId(")
    assert 'add(actor)' not in setter
    speed_context = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::showAnimSpeed(")
    assert 'setActiveActorId' not in speed_context and 'mAnimSpeedContextLabel' in speed_context
    gaze = ET.parse(xui / "panel_dpage_performance_gaze.xml")
    assert gaze.find(".//*[@widget='gaze_apply_to']") is not None
    assert gaze.find(".//*[@widget='gaze_apply_group']") is not None
    preset = method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onPerformanceCommit(")
    assert 'for (const LLUUID& actor : editActors())' in preset and 'applyGazePresetBlended(actor' in preset
    for signature in ("LLSD LLDirectorCast::sceneData() const", "void LLDirectorCast::applySceneData("):
        body = method(nv / "lldirectorcast.cpp", signature)
        assert "mActiveActorId" not in body
    studio = (nv / "alghoststudio.cpp").read_text()
    consumer = studio[studio.index('mRuntimeConsumers.emplace_back('):studio.index('mRuntimeConsumers.emplace_back(', studio.index('mRuntimeConsumers.emplace_back(') + 1)]
    assert 'cast.replaceActorId(old_id.notNull() ? old_id : stable_id, destination, stable_id)' in consumer
    assert 'removing_instance ? LLUUID::null : new_id.notNull() ? new_id : stable_id' in consumer
    assert 'cast.remove(' not in consumer and 'mWasDirectorSubject' not in consumer
    rekey = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(")
    assert rekey.index('CastMember saved = *old') < rekey.index('mover.dropActor(new_id)') < rekey.index('mover.migrateActor(old_id, new_id)') < rekey.index('mCast.erase(old)') < rekey.rindex('touchScene()')
    assert 'Actor alias collision on recovery' in rekey and 'keeping migrating record' in rekey
    assert rekey.count('LL_WARNS') == 1 and '<< old_id' in rekey and '<< new_id' in rekey
    assert load.count('Actor alias collision on load') == 1
    assert '<< previous_id' in load and '<< e["id"].asUUID()' in load
    assert 'mParkedWalkKeys' not in (nv / "llactormover.cpp").read_text()
    for signature in ('void LLActorMover::clearPath(', 'void LLActorMover::applyState(',
                      'void LLActorMover::snapshotForUndo(const LLUUID& actor_id, LLView* owner)'):
        body = method(nv / "llactormover.cpp", signature)
        assert 'pathSceneData' not in body and 'sceneData' not in body and 'mCast' not in body
    # The new pass also runs in the loaded-destination collision branch.
    assert len(re.findall(r"^    +(bool references_changed = )?rewriteActorReferences\(old_id, new_id\);", rekey, re.M)) == 2
    assert "bool references_changed = rewriteActorReferences(old_id, new_id);" in rekey
    assert "if (references_changed) touchScene();" in rekey
    assert "rewriteActorReferences(stable_id, new_id)" in rekey
    from director_clone_rekey_check import check as check_clone_rekey
    check_clone_rekey()
    gaze_draw = method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::draw(")
    assert 'cast.getCastRevision()' in gaze_draw and 'refreshEditActorCombo()' in gaze_draw
    assert 'refreshApplyGroupCombo()' in method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::refreshEditActorCombo(")
    stamp = method(nv / "llfloaterdirector.cpp", "LLFloaterDirector::SceneChangeStamp LLFloaterDirector::currentSceneStamp(")
    assert 'stamp.mCast = LLDirectorCast::instance().getSceneRevision()' in stamp
    autosave = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::idleSceneRecovery(")
    assert autosave.index('updateSceneDirty()') < autosave.index('if (!service.mSceneDirty')
    styling = method(nv / "llfloaterdirector.cpp", "bool LLFloaterDirector::hasActorStyleTarget(")
    assert "getMember(actor)" in styling and "isGhostAvatar" not in styling
    selection = method(nv / "alavatarsubjectpicker.cpp", "LLVOAvatar* fromSelection(")
    assert "!object->isAttachment()" in selection and "fromObject(object)" in selection
    roster = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::refreshCastList(")
    assert "mCastList->selectMultiple(previous_selection)" in roster
    layout = method(nv / "alkitshell.cpp", "void ALKitShell::layoutChrome(")
    widths = [int(re.search(name + r"->reshape\((\d+), 26", layout)[1])
              for name in ("mActorClear", "mActorYou", "mActorSelected", "mActorChoose")]
    assert 850 - 8 - sum(widths) - 4 * 4 - 56 >= 120
    assert "getRect().getHeight() - ACTOR_H" in layout
    assert 'constexpr S32 ACTOR_H = 28' in layout and bar.get('height') == '28'
    count = len(list(shell.iter('kit_shell.rail_item')))
    assert count > 0
    for height in (590, 600, 620, 646, 720):
        compact = 40 - (28 + count) // (count + 1)
        assert compact >= 36
        assert height - 28 - 40 - (count + 1) * compact >= height - 40 - (count + 1) * 40
    for path in ("panel_dpage_cast_move.xml", "panel_dpage_cast_path.xml", "panel_dpage_performance_animate.xml",
                 "panel_dpage_performance_gaze.xml", "panel_dpage_look_stepped.xml", "panel_dpage_look_actor_style.xml"):
        page = next(ET.parse(xui / path).getroot().iter("kit_page"))
        assert page.get("actor_target") is None and page.get("who_picker") is None
    fixture = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <cmath>
using U64 = unsigned long long;
struct LLUUID {
    int value = 0;
    bool notNull() const { return value != 0; }
    bool isNull() const { return !notNull(); }
    void setNull() { value=0; }
    bool operator<(LLUUID x) const { return value < x.value; }
    bool operator==(LLUUID x) const { return value == x.value; }
    bool operator!=(LLUUID x) const { return !(*this == x); }
    std::string asString() const { return std::to_string(value); }
    static const LLUUID null;
};
const LLUUID LLUUID::null{};
const LLUUID gAgentID{1};
std::ostream& operator<<(std::ostream& out, LLUUID id) { return out << id.value; }
#define LL_INFOS(x) std::cout
int aliasWarnings=0;
#define LL_WARNS(x) (++aliasWarnings, std::cout)
#define LL_ENDL std::endl
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(x)
struct LLViewerObject { virtual ~LLViewerObject() = default; };
struct LLVOAvatar : LLViewerObject {
    bool dead = false, control = false, ghost = false, mIsDummy = false;
    bool isDead() const { return dead; }
    bool isControlAvatar() const { return control; }
    LLUUID getID() const { return {}; } // picker fixture has no runtime identities
    bool isGhostAvatar() const { return ghost; }
    bool isUIAvatar() const { return false; }
    void* getRootJoint() const { return nullptr; }
};
struct LLGhostAvatar : LLVOAvatar {};
struct ObjectList { LLViewerObject* findObject(LLUUID) { return nullptr; } } gObjectList;
struct ALGhostStudio {
    enum { BACKING_ENTITY_CLONE=1 };
    struct Instance { LLUUID mId, mEntityId; int mKind=BACKING_ENTITY_CLONE; float mAnimSpeed=1.f; };
    std::map<LLUUID,Instance> clones;
    static ALGhostStudio& instance() { static ALGhostStudio studio; return studio; }
    const Instance* getInstance(LLUUID id) const { auto it=clones.find(id); return it==clones.end()?nullptr:&it->second; }
    const Instance* findInstanceByRuntime(LLUUID id) const { for(auto& item:clones) if(id.notNull() && item.second.mEntityId==id) return &item.second; return nullptr; }
    bool isPrivateOverlayRuntime(LLUUID) const { return false; }
    bool isDeleting(LLUUID) const { return false; } // this fixture has no retiring bodies
    void setInstanceAnimSpeed(LLUUID id, float speed) { clones.at(id).mAnimSpeed=speed; }
};
LLVOAvatar* resolve_actor(LLUUID) { return nullptr; }
namespace ALGazeMotor { struct GazeMotorState {}; }
struct LLActorMover {
    struct Handle { void clear() {} };
    struct Move {
        int mDwellNode = -1, mEndMode = 0;
        bool mSuspended = true, mIsPath = true, mArrived = false;
        float mSpeed = 1, mT = 4, mDistance = 9;
        LLUUID mDwellAnim, mAnim;
        Handle mLocomotion, mSecondaryLocomotion, mDwellMotion;
    };
    struct Gaze { ALGazeMotor::GazeMotorState mGazeMotor; int authored = 0; };
    struct Follow { LLUUID mLeader; };
    std::map<LLUUID, Move> mMoves;
    std::map<LLUUID, double> mPaths;
    std::map<LLUUID, std::vector<double>> mHistory;
    std::map<LLUUID, Gaze> mGazes;
    std::map<LLUUID, Follow> mFollows;
    std::map<LLUUID, int> mGhostImpostors;
    int mGazeRevision = 0;
    LLUUID mEditActor;
    static LLActorMover& instance() { static LLActorMover mover; return mover; }
    void stopLocomotion(LLVOAvatar*, Move&) {}
    void startLocomotion(LLVOAvatar*, Move&) {}
    void startLocalMotion(LLVOAvatar*, Handle&, LLUUID, bool, float, bool) {}
    void invalidateGazeCache(Gaze&, bool) {}
    void clearDirectorLookAtRuntime(LLUUID) {}
    void onRosterMemberRemoved(LLUUID) {}
    void migrateActor(const LLUUID&, const LLUUID&);
    double pathSceneData(LLUUID id) const { auto it=mPaths.find(id); return it==mPaths.end()?0:it->second; }
    void dropActor(LLUUID id) { mPaths.erase(id); mHistory.erase(id); mMoves.erase(id); mGazes.erase(id); mFollows.erase(id); }
};
using uuid_vec_t = std::vector<LLUUID>;
using SceneActorIdRemap = std::map<LLUUID, LLUUID>;
struct SceneValue {
    LLUUID value;
    SceneValue& operator=(LLUUID id) { value=id; return *this; }
    LLUUID asUUID() const { return value; }
};
struct Scene : std::map<std::string, SceneValue> {
    bool has(const std::string& key) const { return count(key)!=0; }
    const SceneValue& operator[](const std::string& key) const { return at(key); }
    SceneValue& operator[](const std::string& key) { return std::map<std::string, SceneValue>::operator[](key); }
};
struct ActorEditorStub { static void rewriteActorReferences(LLUUID, LLUUID) {} };
struct ALPanelGhostStudio {static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALFloaterGazeCues { static void rewriteActorReferences(LLUUID, LLUUID) {} };
struct ALClientHeight {static ALClientHeight& instance(){static ALClientHeight s;return s;} void reset(LLUUID){}};
struct ALAvatarLightResponse {static bool instanceExists(){return false;} static ALAvatarLightResponse& instance(){static ALAvatarLightResponse s;return s;} void retireClone(LLUUID){}};
struct ALAvatarStaging { static ALAvatarStaging& instance() { static ALAvatarStaging s; return s; } void removeActorReferences(LLUUID) {} };
struct ALDanceBall { static ALDanceBall& instance() { static ALDanceBall s; return s; } void removeActorReferences(LLUUID) {} };
struct LLDirectorCast {
    struct CastMember {
        LLUUID mId; std::string mLastName, mGroup;
        int mActorStyle = 0, mGazeTarget = 0, mMark = 0;
        std::vector<int> mGazeCues, mGazeInfluenceKeys;
        // Model nested sparse Stepped defaults separately from Actor FX.
        int mode = 0, rate = 12, role = 0, part = -1;
    bool mCloneFading=false;};
    std::vector<CastMember> mCast;
    uuid_vec_t mIds;
    std::set<LLUUID> mLookAtCameraIds;
    LLUUID mSubjectA, mSubjectB, mSubjectC, mSubjectD;
    int mCastRevision = 0, mGazeCueRevision = 0, mSubjectRevision = 0, mSceneRevision = 0;
    bool isLookAtCamera(LLUUID id) const { return mLookAtCameraIds.count(id)!=0; }
    struct PendingStart { LLUUID mId; };
    std::vector<PendingStart> mPendingStarts;
    std::map<std::string, float> mGroupDelays;
    std::map<std::string, int> mGroupRates;
    void rebuildMemberIndex() {}
    void touchScene() { ++mSceneRevision; }
    void remove(const LLUUID&);
    void cancelPendingStart(LLUUID id) { mPendingStarts.erase(std::remove_if(mPendingStarts.begin(),mPendingStarts.end(),[&](const PendingStart& start){return start.mId==id;}),mPendingStarts.end()); }
    void replaceActorId(const LLUUID&, const LLUUID&, const LLUUID& = LLUUID::null);
    bool rewriteActorReferences(const LLUUID&, const LLUUID&) { return false; } // exercised by focused production harness
    uuid_vec_t membersInGroup(const std::string&) const;
    bool contains(LLUUID id) const { for(const auto& m:mCast) if(m.mId==id) return true; return false; }
    CastMember* getMember(LLUUID id) { for(auto& m:mCast) if(m.mId==id) return &m; return nullptr; }
    void pruneGroupData(const std::string&) {}
    void setGroup(const LLUUID&, const std::string&);
    bool renameGroup(const std::string&, const std::string&);
    bool dissolveGroup(const std::string&);
    std::vector<std::string> getGroupNames() const;
    const uuid_vec_t& getIds() const { return mIds; }
    int getCastRevision() const { return mCastRevision; }
    struct GazeTarget { LLUUID mCastRef; };
    std::map<LLUUID, GazeTarget> targets, eyeTargets;
    GazeTarget getGazeTarget(LLUUID id) const { auto it=targets.find(id); return it==targets.end()?GazeTarget{}:it->second; }
    bool hasEyeGazeTarget(LLUUID id) const { return eyeTargets.count(id)!=0; }
    GazeTarget getEyeGazeTarget(LLUUID id) const { return eyeTargets.at(id); }
    LLUUID getSubjectA() const { return mSubjectA; }
    LLUUID getSubjectB() const { return mSubjectB; }
    LLUUID getSubjectC() const { return mSubjectC; }
    LLUUID getSubjectD() const { return mSubjectD; }
    LLUUID mActiveActorId;
    uuid_vec_t members;
    std::function<void()> changed;
    int events = 0;
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    static bool instanceExists() { return true; }
    static bool isSelf(const LLUUID& id) { return id.isNull() || id == gAgentID; }
    static LLUUID runtimeActorId(const LLUUID& id);
    static LLUUID canonicalActorId(LLUUID id) { return id.value == 101 ? LLUUID{1} : id; }
    void add(LLUUID id) {
        if (std::find(members.begin(),members.end(),id)==members.end()) members.push_back(id);
        if(!getMember(id)) { CastMember member; member.mId=id; mCast.push_back(member); mIds.push_back(id); }
    }
    const LLUUID& getActiveActorId() const { return mActiveActorId; }
    void mActiveActorChanged() { ++events; if (changed) changed(); }
    void setActiveActorId(const LLUUID&);
    LLUUID getSteppedActorId() const { return runtimeActorId(mActiveActorId); }
    void migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids);
    void saveStepped(Scene& data) const;
    std::map<LLUUID, size_t> mMemberIndex;
    void loadWholeRecords(const std::vector<CastMember>&, const std::vector<double>&,
                          const std::vector<bool>&);
};
struct List {
    uuid_vec_t rows, selected;
    void deselectAllItems() { selected.clear(); }
    void selectMultiple(uuid_vec_t ids) { selected.clear(); for (auto id : ids) selectByID(id); }
    void selectByID(LLUUID id) { if (std::find(rows.begin(),rows.end(),id)!=rows.end()) selected.push_back(id); }
};
struct Shell { uuid_vec_t selected; void setCastSelection(const uuid_vec_t& ids) { selected = ids; } };
struct LLFloaterDirector {
    List list; Shell shell;
    List* mCastList = &list; Shell* mShell = &shell;
    struct Clock { void forceDue() {} } mAnimSpeedClock;
    LLUUID mAnimSpeedSelection, pathActor, mPrimaryCastActor;
    std::string mAnimSpeedContextLabel;
    bool mSyncingActiveActor = false;
    bool mActorStyleHadTarget = true, mActorStyleTabDirty = false, mAnimateTabDirty = false;
    uuid_vec_t gaze;
    LLUUID firstSelectedCastId() const { if (mPrimaryCastActor.notNull() && std::find(list.selected.begin(),list.selected.end(),mPrimaryCastActor)!=list.selected.end()) return mPrimaryCastActor; return list.selected.empty() ? LLUUID{} : list.selected.front(); }
    uuid_vec_t selectedCastIds() const { return list.selected; }
    void refreshCastList() { list.rows = LLDirectorCast::instance().members; }
    void retargetPathFromCastSelection() { pathActor = LLDirectorCast::instance().getActiveActorId(); }
    void refreshGazeSelection(const uuid_vec_t& ids) { gaze = ids; }
    void syncActiveActor();
    static void rewriteActorReferences(LLUUID,LLUUID) {}
};
'''
    fixture += method(nv / "llactormover.cpp", "void LLActorMover::migrateActor(")
    fixture += r"""
namespace ALSteppedAnim { void renameGroupRate(std::map<std::string,int>& rates, const std::string& old, const std::string& name) {
    if(rates.count(old)) { if(!rates.count(name)) rates[name]=rates[old]; rates.erase(old); }
} }
namespace ALAvatarSubjectPicker {
"""
    fixture += method(nv / "alavatarsubjectpicker.cpp", "bool valid(") + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID LLDirectorCast::runtimeActorId(")
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId(")
    fixture += method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor(")
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID canonicalSceneActorId(")
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID loadedSceneMemberId(")
    # Execute the production compatibility statements against a minimal LLSD map.
    migration = re.search(r'    const bool has_stepped_actor = .*?LLUUID::null;', load, re.S)[0]
    restore = re.search(r'    if \(has_stepped_actor\) setActiveActorId\([^;]+;', load)[0]
    fixture += "void LLDirectorCast::migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids) {\n" + migration + restore + "\n}\n"
    write = re.search(r'    data\["stepped_actor"\] = getSteppedActorId\(\);', save)[0]
    fixture += "void LLDirectorCast::saveStepped(Scene& data) const {\n" + write + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::remove(")
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(").replace("ALPanelLensGaze::", "ActorEditorStub::")
    # Execute production collision arbitration and publication; payload copies
    # stand in for the unchanged LLSD codecs, with -1 representing absent path.
    collision = load[load.index('        CastMember* alias = getMember(m.mId);'):load.index('        m.mLastName = e["name"].asString();')]
    publication = load[load.index('        if (alias) *alias = m;'):load.index('        ++mCastRevision; // [GhostPanelPerf] loaded member')]
    fixture += r'''
struct RecordArray : std::vector<Scene> {
    auto beginArray() const { return begin(); }
    auto endArray() const { return end(); }
};
void LLDirectorCast::loadWholeRecords(const std::vector<CastMember>& payloads,
                                    const std::vector<double>& paths,
                                    const std::vector<bool>& cameras) {
    auto& mover=LLActorMover::instance();
    mCast.clear(); mIds.clear(); mMemberIndex.clear(); mLookAtCameraIds.clear();
    mover.mPaths.clear(); mover.mHistory.clear();
    RecordArray cast_arr;
    for(const auto& member:payloads) { Scene e; e["id"]=member.mId; cast_arr.push_back(e); }
    SceneActorIdRemap scene_actor_ids;
    for(auto it=cast_arr.beginArray(); it!=cast_arr.endArray(); ++it) {
        const Scene& e=*it;
        const auto index=it-cast_arr.beginArray();
        CastMember m=payloads[index];
        m.mId=loadedSceneMemberId(e["id"].asUUID(),scene_actor_ids);
        if(m.mId.isNull()) continue;
''' + collision + publication + r'''
        if(paths[index]>=0) mover.mPaths[m.mId]=paths[index];
        if(cameras[index]) mLookAtCameraIds.insert(m.mId);
    }
}
'''
    transaction = consumer[consumer.index('            const LLUUID destination'):consumer.index('\n        });')]
    fixture += "void cloneTransition(LLUUID stable_id, LLUUID old_id, LLUUID new_id, bool removing_instance=false) {\n    auto& cast=LLDirectorCast::instance();\n" + transaction + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "uuid_vec_t LLDirectorCast::membersInGroup(")
    for signature in ("void LLDirectorCast::setGroup(", "bool LLDirectorCast::renameGroup(",
                      "bool LLDirectorCast::dissolveGroup(", "std::vector<std::string> LLDirectorCast::getGroupNames("):
        fixture += method(nv / "lldirectorcast.cpp", signature)
    fixture += r'''
using S32 = int;
using U64 = unsigned long long;
struct Value {
    int number = 0; std::string name;
    Value() = default; Value(int n):number(n) {} Value(std::string s):name(s) {}
    int asInteger() const { return number; } std::string asString() const { return name; }
};
using LLSD = Value;
struct Combo {
    Value value; std::vector<std::string> rows; std::vector<bool> enabled; bool visible = false;
    bool focus = false, list_open = false;
    bool isListVisible() const { return list_open; } void hideList() { list_open = false; }
    Value getValue() const { return value; } Value getSelectedValue() const { return value; }
    void removeall() { rows.clear(); enabled.clear(); clear(); }
    void clear() { value={}; }
    void add(const std::string&, Value v) { rows.push_back(v.name); enabled.push_back(true); }
    void add(const std::string&, Value v, int, bool row_enabled) { rows.push_back(v.name); enabled.push_back(row_enabled); }
    bool valueExists(const std::string& v) const { return std::find(rows.begin(),rows.end(),v)!=rows.end(); }
    bool setCurrentByIndex(int i) { if(i<0||i>=int(rows.size())||!enabled[i]) return false; value=Value(rows[i]); return true; }
    bool setSelectedByValue(Value v, bool) { if(std::find(rows.begin(),rows.end(),v.name)==rows.end()) return false; value=v; return true; }
    void selectFirstItem() { if(!rows.empty()) value=rows.front(); }
    int getItemCount() const { return int(rows.size()); }
    int getCurrentIndex() const { auto it=std::find(rows.begin(),rows.end(),value.name); return it==rows.end()?-1:int(it-rows.begin()); }
    Combo* getParent() { return this; }
};
void setVisibleIfChanged(Combo* c, bool visible) { c->visible=visible; }
void setValueIfChanged(Combo* c, Value value, bool) { c->setSelectedByValue(value,true); }
bool isEditing(Combo* c) { return c->focus; }
std::string actorName(LLUUID id) { return id.asString(); }
constexpr int ADD_BOTTOM = 0; // Nearby section (selection rules: director_gaze_nearby_check.py)
namespace ALGazeNearby { struct Entry { LLUUID mId; std::string mName; };
    constexpr const char* HEADER_LABEL = "Nearby"; constexpr const char* HEADER_VALUE = "nearby-header";
    inline std::string awayLabel(const std::string& n) { return n + " (away)"; }
    inline bool sameMembers(const std::vector<Entry>& a, const std::vector<Entry>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (a[i].mId != b[i].mId || a[i].mName != b[i].mName) return false;
        return true; } }
std::vector<ALGazeNearby::Entry> nearby_double; // what nearbyFor() projects for the displayed actor
bool rlv_hides_names = false;                   // RLV adapter for liveNearbyName
LLUUID comboValueId(const std::string& v) { return (v.empty() || v == ALGazeNearby::HEADER_VALUE) ? LLUUID::null : LLUUID{std::stoi(v)}; }
std::string knownPersonName(const LLUUID& id) { return id.asString() == "777" ? std::string("Departed") : std::string(); }
bool listedAmong(const std::vector<ALGazeNearby::Entry>& people, const LLUUID& id) { for (const auto& e : people) if (e.mId == id) return true; return false; }
struct AgentAvatar { LLUUID getID() const { return gAgentID; } } agentAvatar;
AgentAvatar* gAgentAvatarp = &agentAvatar;
bool isAgentAvatarValid() { return true; }
std::vector<std::pair<LLUUID,int>> gazeWrites;
constexpr int GAZE_PRESET_COUNT = 24;
void applyGazePresetBlended(const LLUUID& actor, int preset) { gazeWrites.emplace_back(actor,preset); }
struct ALPanelLensGaze {
    Combo *mApplyTo = nullptr, *mApplyGroup = nullptr, *mPerformance = nullptr;
    Combo *mCast = nullptr;
    std::vector<std::string> mLastApplyGroups;
    bool mEditingEyeTarget = false;
    U64 mCastComboRevision = 0;
    LLUUID mCastComboActor, mCastComboSelection;
    std::vector<ALGazeNearby::Entry> mCastNearbyShown; std::string mCastAwayShown;
    LLUUID mCastAwayId; bool mCastAwayNamed = false, mCastRestore = false;
    U64 mPollGeneration = 0, mAwayPoll = U64(-1); LLUUID mAwayFor; std::string mAwayLabelCached; bool mAwayNamedCached = false;
    struct NearbyProjection { std::vector<ALGazeNearby::Entry> mList; } mCastProjection;
    void nearbyFor(NearbyProjection& proj, const LLUUID&, const LLUUID&) const { proj.mList = nearby_double; }
    bool liveNearbyName(const LLUUID& id, std::string& name) const {
        if (rlv_hides_names) return false;
        for (const auto& e : nearby_double) if (e.mId == id) { name = e.mName; return true; }
        return false; }
    void onCastCommit();
    bool mEditAll = false;
    LLUUID activeSlotActor() const { return LLDirectorCast::instance().getActiveActorId(); }
    uuid_vec_t editActors() const;
    LLUUID editDisplayActor() const;
    void refreshCastCombo(bool force=false);
    void refreshApplyGroupCombo();
    void onPerformanceCommit();
};
'''
    fixture += method(nv / "alpanellensgaze.cpp", "uuid_vec_t ALPanelLensGaze::editActors(")
    for signature in ("LLUUID ALPanelLensGaze::editDisplayActor(", "void ALPanelLensGaze::refreshCastCombo(",
                      "void ALPanelLensGaze::refreshApplyGroupCombo("):
        fixture += method(nv / "alpanellensgaze.cpp", signature)
    fixture += method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onPerformanceCommit(")
    # The production Gaze cast commit. Only the edit-target walk is an adapter (it applies the
    # mutation to the stub Cast's head/eye targets); everything before it is production code.
    fixture += r'''
template <typename Fn>
void editDetailTargetsFor(const uuid_vec_t& actors, bool editing_eye, Fn&& mutate)
{
    auto& cast = LLDirectorCast::instance();
    for (const LLUUID& actor : actors)
        mutate(editing_eye && cast.hasEyeGazeTarget(actor) ? cast.eyeTargets[actor] : cast.targets[actor]);
}
'''
    fixture += method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onCastCommit(").replace(
        "LLActorMover::GazeTarget", "LLDirectorCast::GazeTarget")
    fixture += r'''
struct LLFloater {};
struct NullHandle { LLFloater* get() const { return nullptr; } } sRecoveryDirectorHandle;
template<class T> void setTextIfChanged(T*, const char*) {}
template<class T> struct LLCachedControl { T value; LLCachedControl(int, const char*, T v):value(v) {} operator T() const { return value; } };
int gSavedSettings=0;
using F32=float;
struct RecoveryProbe : LLFloater {
    struct Timer { float elapsed=0; void reset() { elapsed=0; } float getElapsedTimeF32() const { return elapsed; } };
    struct RecoveryServiceState { bool mHaveCleanSceneStamp=true, mSceneDirty=false; int mCleanSceneStamp=0; Timer mAutosaveTimer; };
    void* mSceneDirtyText=nullptr;
    static RecoveryServiceState& recoveryServiceState() { static RecoveryServiceState service; return service; }
    static int currentSceneStamp() { return LLDirectorCast::instance().mSceneRevision; }
    static void markSceneClean() { auto& s=recoveryServiceState(); s.mCleanSceneStamp=currentSceneStamp(); s.mHaveCleanSceneStamp=true; s.mSceneDirty=false; }
    static void updateSceneDirty();
    static void idleSceneRecovery(bool force_write);
    static int writes;
    static Scene saved;
};
int RecoveryProbe::writes=0;
Scene RecoveryProbe::saved;
'''
    fixture += method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::updateSceneDirty(").replace("LLFloaterDirector", "RecoveryProbe")
    # Execute the unmodified autosave entry/eligibility gate. Only its file I/O
    # is replaced with a payload capture; currentSceneStamp's Cast link is
    # independently asserted above.
    entry = autosave[:autosave.index('    // An oversized scene')].replace('LLFloaterDirector', 'RecoveryProbe')
    fixture += entry + '    ++writes; LLDirectorCast::instance().saveStepped(saved);\n}\n'
    speed = nv / "alavataranimspeed.cpp"
    fixture += r'''
struct ALAvatarAnimSpeed {
    std::map<LLUUID,F32> mSpeeds;
    int mRevision=0;
    static float clampSpeed(float speed) { return std::clamp(speed,.05f,4.f); }
    static bool isGhost(const LLUUID&);
    F32 getSpeed(const LLUUID&) const;
    void setSpeed(const LLUUID&, F32);
    void apply(LLVOAvatar&) {}
};
'''
    for signature in ('const ALGhostStudio::Instance* studioClone(', 'bool ALAvatarAnimSpeed::isGhost(',
                      'F32 ALAvatarAnimSpeed::getSpeed(', 'void ALAvatarAnimSpeed::setSpeed('):
        fixture += method(speed, signature)
    studio_speed = method(nv / "alghoststudio.cpp", "bool ALGhostStudio::setInstanceAnimSpeed(")
    assert studio_speed.index('inst->mAnimSpeed =') < studio_speed.index('if (!ghost)')
    assert 'STATE_ERROR' not in studio_speed
    fixture += r'''
int main() {
    LLVOAvatar resident, animesh, clone;
    animesh.control = animesh.mIsDummy = true;
    clone.ghost = clone.mIsDummy = true;
    assert(ALAvatarSubjectPicker::valid(&resident, false, false));
    assert(!ALAvatarSubjectPicker::valid(&animesh, true, false)); // camera filters unchanged
    assert(ALAvatarSubjectPicker::valid(&animesh, true, true)); // shell admits real control avatars
    assert(ALAvatarSubjectPicker::valid(&clone, true, false));
    assert(!ALAvatarSubjectPicker::valid(&clone, false, false));
    auto& cast = LLDirectorCast::instance(); LLFloaterDirector director;
    cast.changed = [&] { director.syncActiveActor(); };
    cast.add({2});
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.firstSelectedCastId() == LLUUID{2});
    cast.add({3}); director.refreshCastList(); director.list.selected = {{2},{3}};
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.list.selected.size() == 2); // same primary preserves batch
    cast.setActiveActorId({3});
    assert(cast.events == 2 && director.list.selected == uuid_vec_t({{2},{3}}) && director.firstSelectedCastId() == LLUUID{3}); // changing primary preserves every batch member
    director.list.selected = {{2},{3}};
    director.mPrimaryCastActor = {}; // explicit roster commit chooses its first row
    cast.setActiveActorId(director.firstSelectedCastId());
    assert(cast.events == 3 && director.list.selected.size() == 2); // Cast publishes its first row
    cast.setActiveActorId({});
    assert(cast.events == 4 && director.list.selected == uuid_vec_t({{2},{3}}) && director.mAnimSpeedSelection == gAgentID); // Clear also preserves a live batch
    const auto members_before = cast.members;
    cast.setActiveActorId({101}); // control-avatar aliases resolve to persistent identity
    assert(cast.events == 5 && cast.getActiveActorId() == LLUUID{1});
    cast.setActiveActorId({1});
    assert(cast.events == 5 && director.pathActor == LLUUID{1}); // no feedback loop
    assert(cast.members == members_before); // You cannot re-enrol a removed self
    // N3: effective render selections dirty clean scenes and reach both timed
    // and forced autosave; explicit You and Clear do not create false changes.
    RecoveryProbe::markSceneClean();
    const int self_revision=cast.mSceneRevision;
    cast.setActiveActorId({});
    assert(cast.mSceneRevision==self_revision);
    RecoveryProbe::idleSceneRecovery(true);
    assert(RecoveryProbe::writes==0);
    cast.setActiveActorId({2});
    assert(cast.mSceneRevision>self_revision);
    RecoveryProbe::updateSceneDirty();
    assert(RecoveryProbe::recoveryServiceState().mSceneDirty);
    RecoveryProbe::idleSceneRecovery(false);
    assert(RecoveryProbe::writes==0);
    RecoveryProbe::recoveryServiceState().mAutosaveTimer.elapsed=301;
    RecoveryProbe::idleSceneRecovery(false);
    assert(RecoveryProbe::writes==1 && RecoveryProbe::saved["stepped_actor"].asUUID()==LLUUID{2});
    RecoveryProbe::markSceneClean(); cast.setActiveActorId({3});
    RecoveryProbe::idleSceneRecovery(true);
    assert(RecoveryProbe::writes==2 && RecoveryProbe::saved["stepped_actor"].asUUID()==LLUUID{3});
    cast.setActiveActorId(gAgentID);
    assert(LLDirectorCast::runtimeActorId(gAgentID).isNull());
    // Execute the exact production Selected predicate; Mixed consumes this same flag.
    auto selected_predicate = [](LLUUID selected, LLUUID actor_id, bool is_self) {
        struct { bool isSelf; bool isSelected; } f{is_self, false};
        f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected;
        return f.isSelected;
    };
    const auto self = LLDirectorCast::runtimeActorId(gAgentID);
    assert(selected_predicate(self, LLUUID{}, true));
    assert(!selected_predicate(self, LLUUID{2}, false));
    assert(selected_predicate(LLUUID{2}, LLUUID{2}, false));
    assert(!selected_predicate(LLUUID{2}, LLUUID{}, true));
    cast.changed = {}; // isolate clone lifecycle from the UI mock
    Scene legacy, saved;
    legacy["stepped_actor"] = LLUUID{100};
    cast.migrateStepped(legacy, {{{100},{80}}});
    assert(cast.getActiveActorId() == LLUUID{80});
    cast.saveStepped(saved);
    assert(saved.has("stepped_actor") && saved["stepped_actor"].asUUID() == LLUUID{80});
    legacy["stepped_actor"] = LLUUID{};
    cast.migrateStepped(legacy, {});
    assert(cast.getActiveActorId() == gAgentID && cast.getSteppedActorId().isNull());
    cast.saveStepped(saved);
    assert(saved.has("stepped_actor") && saved["stepped_actor"].asUUID().isNull());
    cast.setActiveActorId({4}); cast.migrateStepped(Scene{}, {});
    assert(cast.getActiveActorId() == LLUUID{4}); // legacy field absent retains the session choice
    // N1: the actual Studio destination expression and central rekey execute
    // with the production mover migration, including an unselected despawn.
    auto& mover=LLActorMover::instance();
    LLDirectorCast::CastMember pathActor; pathActor.mId={150}; pathActor.mMark=12;
    pathActor.mActorStyle=48; pathActor.mGazeCues={9,8}; pathActor.mGazeInfluenceKeys={6,5};
    cast.mCast={pathActor};
    cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD={150};
    cast.setActiveActorId({2});
    mover.mPaths[{150}]=1.0; mover.mHistory[{150}]={1.0}; mover.mMoves[{150}]={};
    mover.mGazes[{150}].authored=17; mover.mFollows[{151}].mLeader={150}; mover.mEditActor={150};
    cloneTransition({160},{150},{});
    assert(cast.mCast.front().mId==LLUUID{160} && mover.mPaths.at({160})==1.0);
    assert(!mover.mPaths.count({150}) && mover.mHistory.at({160})==std::vector<double>{1.0});
    assert(mover.mEditActor==LLUUID{160} && mover.mMoves.at({160}).mT==4);
    mover.mPaths[{160}]=2.5; mover.mHistory[{160}].push_back(2.5); // edit while parked
    assert(mover.mPaths.at(cast.mCast.front().mId)==2.5); // same key sceneData serializes
    cast.setActiveActorId({160}); cast.mCast.front().mActorStyle=49;
    cloneTransition({160},{},{170});
    assert(cast.mCast.size()==1 && cast.mCast.front().mId==LLUUID{170});
    assert(cast.mCast.front().mMark==12 && cast.mCast.front().mActorStyle==49);
    assert(cast.mCast.front().mGazeCues==pathActor.mGazeCues && cast.mCast.front().mGazeInfluenceKeys==pathActor.mGazeInfluenceKeys);
    assert(mover.mPaths.size()==1 && mover.mPaths.at({170})==2.5 && !mover.mHistory.count({160}));
    assert(mover.mHistory.at({170})==std::vector<double>({1.0,2.5}));
    assert(mover.mMoves.at({170}).mT==4 && mover.mGazes.at({170}).authored==17);
    assert(mover.mEditActor==LLUUID{170} && mover.mFollows.at({151}).mLeader==LLUUID{170});
    assert(cast.getSteppedActorId()==LLUUID{170});
    assert(cast.mSubjectA==LLUUID{170} && cast.mSubjectB==LLUUID{170} && cast.mSubjectC==LLUUID{170} && cast.mSubjectD==LLUUID{170});
    cast.setActiveActorId({180}); // selection outside Cast must also clear on final removal
    cloneTransition({180},{},{},true);
    assert(cast.getActiveActorId().isNull());
    // R3: save/load ownership normalization uses the production loader helper.
    // The payload fixture retains the complete member and path, as the scene
    // codecs do; no body resolution is required to test identity ownership.
    auto& studio=ALGhostStudio::instance();
    const LLUUID S{210}, R{211}, N{212}, M{213};
    studio.clones[S]={S,R};
    LLDirectorCast::CastMember sceneMember; sceneMember.mId=R;
    sceneMember.mActorStyle=72; sceneMember.mMark=19; sceneMember.mGazeTarget=35;
    sceneMember.mode=1; sceneMember.rate=8; sceneMember.role=1; sceneMember.part=2;
    sceneMember.mGroup="loaded"; sceneMember.mGazeCues={2,4}; sceneMember.mGazeInfluenceKeys={3,6};
    auto resetScene = [&] {
        cast.mCast={sceneMember}; cast.mIds={R}; cast.setActiveActorId(R);
        cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=R;
        mover.mPaths.clear(); mover.mHistory.clear(); mover.mPaths[R]=3.25; mover.mHistory[R]={3.25};
        studio.clones[S].mEntityId=R; cast.touchScene();
    };
    using Payload=std::pair<LLDirectorCast::CastMember,double>;
    auto saveClone = [&] { return Payload{cast.mCast.front(),mover.mPaths.at(cast.mCast.front().mId)}; };
    auto loadClone = [&](const Payload& payload) {
        cast.mCast.clear(); cast.mIds.clear(); mover.mPaths.clear(); mover.mHistory.clear();
        auto member=payload.first; member.mId=loadedSceneMemberId(member.mId,{});
        cast.mCast.push_back(member); cast.mIds.push_back(member.mId);
        mover.mPaths[member.mId]=payload.second;
        cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=member.mId;
        cast.setActiveActorId(member.mId);
    };
    auto assertClone = [&](LLUUID id) {
        assert(cast.mCast.size()==1 && cast.mCast.front().mId==id);
        const auto& member=cast.mCast.front();
        assert(member.mActorStyle==72 && member.mMark==19 && member.mGazeTarget==35);
        assert(member.mode==1 && member.rate==8 && member.role==1 && member.part==2);
        assert(member.mGroup=="loaded" && member.mGazeCues==std::vector<int>({2,4}));
        assert(member.mGazeInfluenceKeys==std::vector<int>({3,6}));
        assert(mover.mPaths.size()==1 && mover.mPaths.at(id)==3.25);
        assert(cast.mSubjectA==id && cast.mSubjectB==id && cast.mSubjectC==id && cast.mSubjectD==id);
        assert(cast.getSteppedActorId()==id);
    };
    resetScene(); auto liveSave=saveClone(); loadClone(liveSave); // save while live -> load -> respawn
    cloneTransition(S,R,N); studio.clones[S].mEntityId=N; assertClone(N);
    resetScene(); cloneTransition(S,R,{}); studio.clones[S].mEntityId={};
    auto parkedSave=saveClone(); assert(parkedSave.first.mId==S);
    loadClone(parkedSave); assertClone(S); // load while parked -> respawn
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N; assertClone(N);
    loadClone(parkedSave); assertClone(N); // parked save -> load while live -> refresh -> despawn
    cloneTransition(S,N,M); studio.clones[S].mEntityId=M; assertClone(M);
    cloneTransition(S,M,{}); studio.clones[S].mEntityId={}; assertClone(S);
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N;
    loadClone(parkedSave); cloneTransition(S,N,{}); studio.clones[S].mEntityId={}; assertClone(S);
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N;
    loadClone(parkedSave); cloneTransition(S,N,{},true); // direct delete immediately after loaded stable alias
    assert(cast.mCast.empty() && mover.mPaths.empty() && cast.getActiveActorId().isNull());
    assert(cast.mSubjectA.isNull() && cast.mSubjectB.isNull() && cast.mSubjectC.isNull() && cast.mSubjectD.isNull());
    resetScene(); cloneTransition(S,R,{}); studio.clones[S].mEntityId={}; loadClone(parkedSave);
    cloneTransition(S,{}, {},true); assert(cast.mCast.empty() && mover.mPaths.empty()); // parked delete
    assert(cast.mSubjectA.isNull() && cast.mSubjectB.isNull() && cast.mSubjectC.isNull() && cast.mSubjectD.isNull());
    // Real speed writer/readback: a selected parked stable handle never enters
    // the resident map; the replacement runtime reads the same Studio value.
    ALAvatarAnimSpeed animSpeed;
    animSpeed.setSpeed(S,2.5f);
    assert(animSpeed.mSpeeds.empty() && studio.clones[S].mAnimSpeed==2.5f && animSpeed.getSpeed(S)==2.5f);
    studio.clones[S].mEntityId=N;
    assert(animSpeed.getSpeed(N)==2.5f && ALAvatarAnimSpeed::isGhost(S));
    animSpeed.setSpeed(N,.75f); assert(animSpeed.getSpeed(S)==.75f && animSpeed.mSpeeds.empty());
    // Load collisions in either order: the last complete record wins. Sparse
    // Role/part defaults in the loser cannot overwrite saved mode/rate.
    LLDirectorCast::CastMember loadStable=sceneMember, loadRuntime;
    loadStable.mId=S; loadRuntime.mId=N;
    loadRuntime.mActorStyle=6; loadRuntime.role=1; loadRuntime.part=3;
    cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=N;
    cast.setActiveActorId(N);
    int loadWarnings=aliasWarnings;
    cast.loadWholeRecords({loadRuntime,loadStable},{9.0,3.25},{true,false});
    assertClone(N);
    assert(cast.mIds==uuid_vec_t{N} && !cast.mLookAtCameraIds.count(N));
    assert(aliasWarnings==loadWarnings+1);
    loadWarnings=aliasWarnings;
    cast.loadWholeRecords({loadStable,loadRuntime},{3.25,-1.0},{true,false});
    assert(cast.mCast.size()==1 && cast.mIds==uuid_vec_t{N});
    assert(cast.mCast.front().mActorStyle==6 && cast.mCast.front().mode==0 && cast.mCast.front().rate==12);
    assert(cast.mCast.front().role==1 && cast.mCast.front().part==3);
    assert(mover.mPaths.empty() && mover.mHistory.empty() && cast.mLookAtCameraIds.empty());
    assert(aliasWarnings==loadWarnings+1);
    cast.loadWholeRecords({loadStable},{3.25},{false});
    cast.add(N); assertClone(N); // Studio's default add cannot overwrite saved settings
    auto onlyPart=loadRuntime; onlyPart.role=0;
    cast.loadWholeRecords({onlyPart,loadStable},{9.0,3.25},{true,false}); assertClone(N);
    cast.loadWholeRecords({loadStable,loadRuntime,loadStable},{3.25,-1.0,3.25},{true,false,false});
    assertClone(N); // repeated alias still obeys last-entry rule
    auto repeatedClone=loadRuntime; repeatedClone.mActorStyle=24;
    cast.loadWholeRecords({loadStable,loadRuntime,repeatedClone},{3.25,-1.0,7.0},{true,false,false});
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==24 && mover.mPaths.at(N)==7.0);
    auto nonClone=loadStable; nonClone.mId={300};
    auto repeatedNonClone=nonClone; repeatedNonClone.mActorStyle=0;
    loadWarnings=aliasWarnings;
    cast.loadWholeRecords({nonClone,repeatedNonClone},{3.25,-1.0},{false,false});
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==72);
    assert(mover.mPaths.at({300})==3.25 && aliasWarnings==loadWarnings);
    // Recovery collision: the current migrating owner wins wholesale. Its
    // nested defaults and explicit clears travel with its path AND history.
    LLDirectorCast::CastMember stable, runtimeAlias; stable.mId=S; runtimeAlias.mId=R;
    stable.mActorStyle=80; stable.mode=1; stable.rate=8;
    runtimeAlias.mMark=17; runtimeAlias.role=1; // role-only defaults must not leak into owner
    cast.mCast={stable,runtimeAlias};
    mover.mPaths.clear(); mover.mHistory.clear();
    mover.mPaths[S]=4.0; mover.mHistory[S]={3.0,4.0};
    mover.mPaths[R]=5.0; mover.mHistory[R]={5.0};
    cast.mLookAtCameraIds={R};
    int warningsBefore=aliasWarnings;
    cast.replaceActorId(S,R);
    assert(aliasWarnings==warningsBefore+1);
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==80 && cast.mCast.front().mMark==0);
    assert(cast.mCast.front().mode==1 && cast.mCast.front().rate==8 && cast.mCast.front().role==0);
    assert(mover.mPaths.size()==1 && mover.mPaths.at(R)==4.0);
    assert(mover.mHistory.size()==1 && mover.mHistory.at(R)==std::vector<double>({3.0,4.0}));
    assert(!cast.mLookAtCameraIds.count(R));
    // Reverse direction: a deliberately default/empty source clears every
    // discarded destination value, including path/history/camera membership.
    cast.mCast.push_back(stable); cast.getMember(R)->mActorStyle=0;
    cast.getMember(R)->mode=0; cast.getMember(R)->rate=12;
    mover.mPaths.erase(R); mover.mHistory.erase(R);
    mover.mPaths[S]=6.0; mover.mHistory[S]={6.0}; cast.mLookAtCameraIds={S};
    cast.replaceActorId(R,S);
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==0);
    assert(cast.mCast.front().mode==0 && cast.mCast.front().rate==12);
    assert(mover.mPaths.empty() && mover.mHistory.empty() && cast.mLookAtCameraIds.empty());
    LLDirectorCast::CastMember authored;
    authored.mId = {50}; authored.mGroup = "ensemble"; authored.mLastName = "Parked clone";
    authored.mActorStyle = 77; authored.mGazeTarget = 31; authored.mMark = 9;
    authored.mGazeCues = {1,4,8}; authored.mGazeInfluenceKeys = {3,7};
    LLDirectorCast::CastMember duplicate; duplicate.mId = {60};
    cast.mCast = {authored, duplicate}; cast.mSubjectA = {50}; cast.mSubjectD = {50};
    cast.setActiveActorId({50});
    cast.replaceActorId({50}, {60}); // disappearance parks the entire member
    cast.mCast.front().mActorStyle = 99; // newly authored while parked
    cast.setActiveActorId({2}); // respawn must also work after selecting somebody else
    cast.replaceActorId({60}, {70});
    assert(cast.mCast.size() == 1 && cast.mCast.front().mId == LLUUID{70});
    const auto& restored = cast.mCast.front();
    assert(restored.mActorStyle == 99 && restored.mGazeTarget == 31 && restored.mMark == 9);
    assert(restored.mGazeCues == authored.mGazeCues && restored.mGazeInfluenceKeys == authored.mGazeInfluenceKeys);
    assert(restored.mGroup == "ensemble" && cast.mSubjectA == LLUUID{70} && cast.mSubjectD == LLUUID{70});
    cast.setActiveActorId({70});
    cast.mSubjectD = {}; // an explicit unassignment while parked must survive recovery
    cast.replaceActorId({70}, {80});
    assert(cast.getActiveActorId() == LLUUID{80} && cast.mCast.size() == 1 && cast.mCast.front().mActorStyle == 99);
    assert(cast.mSubjectA == LLUUID{80} && cast.mSubjectD.isNull());
    LLDirectorCast::CastMember second; second.mId = {2}; second.mGroup = "ensemble";
    LLDirectorCast::CastMember other; other.mId = {3}; other.mGroup = "other";
    cast.mCast.push_back(second); cast.mCast.push_back(other);
    Combo scope, group, performance, castPicker;
    scope.value.number = 1; group.value.name = "ensemble"; performance.value.number = 5;
    ALPanelLensGaze gaze; gaze.mApplyTo=&scope; gaze.mApplyGroup=&group; gaze.mPerformance=&performance;
    gaze.mCast=&castPicker;
    assert(gaze.editActors() == uuid_vec_t({{80},{2}}));
    gaze.onPerformanceCommit();
    assert(gazeWrites.size() == 2 && gazeWrites[0] == std::make_pair(LLUUID{80},5) && gazeWrites[1] == std::make_pair(LLUUID{2},5));
    // N2: Group readback and filtering follow A, with bar X outside the group.
    cast.setActiveActorId({3}); cast.mIds={{80},{2},{3}};
    cast.targets[{3}].mCastRef={80}; cast.targets[{80}].mCastRef={2};
    gaze.refreshCastCombo();
    assert(castPicker.value.name=="2");
    assert(castPicker.rows==std::vector<std::string>({"2","3"})); // X remains a valid target
    cast.eyeTargets[{80}].mCastRef={3}; gaze.mEditingEyeTarget=true;
    gaze.refreshCastCombo(); assert(castPicker.value.name=="3");
    cast.setActiveActorId({2}); gaze.refreshCastCombo();
    assert(castPicker.value.name=="3" && castPicker.rows==std::vector<std::string>({"2","3"}));
    // Gaze Nearby picker (production refreshCastCombo): disabled header, people after the cast,
    // default row chosen by value (never the header), saved target kept (away only if absent).
    nearby_double = {{LLUUID{90},"Nearby Person"}};
    gaze.refreshCastCombo();
    {
        auto header = std::find(castPicker.rows.begin(), castPicker.rows.end(), std::string(ALGazeNearby::HEADER_VALUE));
        assert(header != castPicker.rows.end() && !castPicker.enabled[header - castPicker.rows.begin()]);
        assert(*(header + 1) == "90" && castPicker.value.name == "3");
    }
    const auto saved_ids = cast.mIds; const auto saved_eye = cast.eyeTargets[{80}].mCastRef;
    cast.mIds = {}; cast.eyeTargets[{80}].mCastRef = {};
    gaze.refreshCastCombo();
    assert(castPicker.value.name == "90"); // empty cast: the default is the first PERSON, not the header
    cast.eyeTargets[{80}].mCastRef = {777};
    gaze.refreshCastCombo();
    assert(castPicker.value.name == "777" && castPicker.rows.back() == "777"); // absent target stays visible as an away row
    nearby_double = {{LLUUID{777},"Departed"}, {LLUUID{90},"Nearby Person"}};
    gaze.refreshCastCombo();
    assert(std::count(castPicker.rows.begin(), castPicker.rows.end(), std::string("777")) == 1 && castPicker.value.name == "777"); // present: listed once, not away
    cast.mIds = saved_ids; cast.eyeTargets[{80}].mCastRef = saved_eye; nearby_double.clear();
    gaze.refreshCastCombo();
    assert(castPicker.rows == std::vector<std::string>({"2","3"}) && castPicker.value.name == "3");
    // Gaze Nearby COMMITS (production onCastCommit; the combo has focus, so only a refusal's own
    // forced rebuild can restore it): header, departed pick, RLV-hidden pick, live pick, saved away ref.
    {
        const auto saved_targets = cast.targets;
        const LLUUID head_before = cast.eyeTargets[{80}].mCastRef; // {3}
        nearby_double = {{LLUUID{90},"Nearby Person"}};
        castPicker.focus = false; gaze.refreshCastCombo();
        assert(castPicker.valueExists("90") && castPicker.valueExists(ALGazeNearby::HEADER_VALUE));
        castPicker.focus = true;
        castPicker.value = Value(std::string(ALGazeNearby::HEADER_VALUE)); gaze.onCastCommit();
        assert(cast.eyeTargets[{80}].mCastRef == head_before && castPicker.value.name == "3"); // header refused + restored
        nearby_double.clear(); // 90 left while the (focused) list still offers the row
        castPicker.value = Value(std::string("90")); gaze.onCastCommit();
        assert(cast.eyeTargets[{80}].mCastRef == head_before && castPicker.value.name == "3"); // departed pick refused + restored
        nearby_double = {{LLUUID{90},"Nearby Person"}};
        castPicker.focus = false; gaze.refreshCastCombo(); castPicker.focus = true;
        rlv_hides_names = true;
        castPicker.value = Value(std::string("90")); gaze.onCastCommit();
        rlv_hides_names = false;
        assert(cast.eyeTargets[{80}].mCastRef == head_before && castPicker.value.name == "3"); // RLV-hidden pick refused + restored
        castPicker.value = Value(std::string("90")); gaze.onCastCommit();
        assert(cast.eyeTargets[{80}].mCastRef == LLUUID{90}); // a live, nameable pick is stored as-is
        cast.eyeTargets[{80}].mCastRef = LLUUID{777}; nearby_double.clear();
        castPicker.focus = false; gaze.refreshCastCombo(); castPicker.focus = true;
        assert(castPicker.rows.back() == "777");
        castPicker.value = Value(std::string("777")); gaze.onCastCommit();
        assert(cast.eyeTargets[{80}].mCastRef == LLUUID{777}); // the saved away reference is untouched
        cast.eyeTargets[{80}].mCastRef = head_before; cast.targets = saved_targets; castPicker.focus = false;
        gaze.refreshCastCombo();
        assert(castPicker.rows == std::vector<std::string>({"2","3"}) && castPicker.value.name == "3");
    }
    // N4: execute group mutations and the real combo rebuild; names need not
    // change for membership/representative invalidation to be required.
    gaze.refreshApplyGroupCombo(); assert(group.value.name=="ensemble");
    int revision=cast.mCastRevision;
    cast.setGroup({80},"new"); assert(cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(std::find(group.rows.begin(),group.rows.end(),"new")!=group.rows.end());
    revision=cast.mCastRevision;
    cast.setGroup({80},"ensemble"); assert(cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(group.value.name=="ensemble");
    revision=cast.mCastRevision;
    assert(cast.renameGroup("other","renamed") && cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(std::find(group.rows.begin(),group.rows.end(),"renamed")!=group.rows.end());
    revision=cast.mCastRevision;
    assert(cast.dissolveGroup("ensemble") && cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo();
    assert(group.value.name.empty() && gaze.editActors().empty()); // no substitution of "renamed"
    assert(group.rows==std::vector<std::string>{"renamed"});
    group.value.name = "removed"; assert(gaze.editActors().empty());
    cast.setActiveActorId({80});
    scope.value.number = 0; assert(gaze.editActors() == uuid_vec_t{{80}});
    std::cout << "PASS: full parked-clone authored state, identity, subjects and uniqueness; Gaze existing-group broadcast\n";
    std::cout << "PASS: parked path/history/move edits survive recovery; Group Head/Eyes picker; dirty/timed/forced autosave; group mutation refresh and removal\n";
    std::cout << "PASS: session actor, canonical identity, two-way Cast sync, batch preservation, Clear speed default, no feedback loop\n";
    std::cout << "PASS: live/parked scene round trips, live loaded stable alias refresh/despawn/delete, wholesale alias winner with path/history/defaults, parked Studio animation speed\n";
}
'''
    directory = check_directory("actorbar")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "actor.cpp").write_text(fixture, encoding="utf-8")
    vcvars = vcvars64()
    (directory / "compile.cmd").write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "actor.obj"}" '
        f'/Fe"{directory / "actor.exe"}" "{directory / "actor.cpp"}\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(directory / "compile.cmd")], check=True)
    subprocess.run([str(directory / "actor.exe")], check=True)
    print("PASS: picker/search contracts, independent renderer subjects, legacy Stepped scene round-trip, group publication, despawned-clone filter, compact rail geometry")
    if "--negative-check" in sys.argv:
        negative_gaze_nearby_controls(directory, fixture)


def negative_gaze_nearby_controls(directory, fixture):
    """Planted regressions in the production Gaze Nearby code the native fixture executes: each
    mutated fixture must fail its asserts (compile errors are a broken control, not a pass)."""
    header_block = ("        mCastRestore = true;\n        refreshCastCombo(true);\n        return;\n    }\n"
                    "    const LLUUID cast_target")
    departed_block = ("        mCastRestore = true;\n        refreshCastCombo(true);\n        return;\n    }\n"
                      "    editDetailTargetsFor")
    controls = {
        "gaze_header_guard_removed": ("    if (value == ALGazeNearby::HEADER_VALUE)\n", "    if (false)\n"),
        "gaze_header_restore_missing": (header_block, header_block.replace("mCastRestore = true;", "(void)0;")),
        "gaze_departed_pick_accepted": ("!liveNearbyName(cast_target, live_name))", "false)"),
        "gaze_refusal_restore_missing": (departed_block, departed_block.replace("mCastRestore = true;", "(void)0;")),
        "gaze_restore_flag_ignored": ("current != mCastComboSelection || mCastRestore ||", "current != mCastComboSelection ||"),
    }
    for name, (old, new) in controls.items():
        assert fixture.count(old) == 1, (name, fixture.count(old))
        path = directory / ("negative_" + name)
        path.with_suffix(".cpp").write_text(fixture.replace(old, new), encoding="utf-8")
        command = directory / ("negative_" + name + ".cmd")
        command.write_text(f'@echo off\ncall "{vcvars64()}" >nul\n'
            f'cl /nologo /std:c++17 /EHsc /Fo"{path}.obj" /Fe"{path}.exe" "{path}.cpp"\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", str(command)], check=True, capture_output=True)
        result = subprocess.run([str(path.with_suffix(".exe"))], capture_output=True)
        assert result.returncode != 0, "regression survived: " + name
        print("PASS: planted Gaze Nearby regression rejected: " + name)


if __name__ == "__main__":
    main()
