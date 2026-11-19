/**
 * @file alvirtuallightsmodel_test.cpp
 * @brief Tests for the pure virtual-light model: prim ranges, sanitiser, aim maths,
 *        placement rules, the eight-light cap and the scene block format.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alvirtuallightsmodel.h"

#include <cmath>
#include <limits>

namespace tut
{
    using namespace ALVirtualLightsModel;

    struct virtual_lights_data {};
    typedef test_group<virtual_lights_data> virtual_lights_group;
    typedef virtual_lights_group::object virtual_lights_object;
    virtual_lights_group virtual_lights_tests("ALVirtualLightsModel");

    namespace
    {
    LLUUID mint() { return LLUUID::generateNewID(); }
    }

    // Prim ranges: radius <= 20 m, intensity <= 1, nothing negative.
    template<> template<>
    void virtual_lights_object::test<1>()
    {
        Light light;
        light.mRadius = 500.f;
        light.mIntensity = 9.f;
        light.mFalloff = -3.f;
        light.mColor[0] = 4.f;
        light.mColor[1] = -1.f;
        sanitize(light);
        ensure("vl_prim_range_radius", light.mRadius == kMaxRadius);
        ensure("vl_prim_range_intensity", light.mIntensity == kMaxIntensity);
        ensure_equals("falloff floors", light.mFalloff, kMinFalloff);
        ensure_equals("colour high clamps", light.mColor[0], 1.f);
        ensure_equals("colour low clamps", light.mColor[1], 0.f);
        light.mRadius = -1.f;
        sanitize(light);
        ensure_equals("radius floors", light.mRadius, kMinRadius);
    }

    // NaN / Inf never survive: they fall back to the defaults.
    template<> template<>
    void virtual_lights_object::test<2>()
    {
        Light light;
        const F32 nan = std::numeric_limits<F32>::quiet_NaN();
        const F32 inf = std::numeric_limits<F32>::infinity();
        light.mRadius = nan;
        light.mIntensity = inf;
        light.mPos[0] = nan;
        light.mPos[2] = -inf;
        light.mYawDeg = inf;
        light.mPitchDeg = nan;
        light.mFov = nan;
        sanitize(light);
        ensure_equals("radius default", light.mRadius, kDefaultRadius);
        ensure_equals("intensity default", light.mIntensity, kDefaultIntensity);
        ensure("x finite", std::isfinite(light.mPos[0]));
        ensure("z finite", std::isfinite(light.mPos[2]));
        ensure("yaw finite", std::isfinite(light.mYawDeg));
        ensure("pitch finite", std::isfinite(light.mPitchDeg));
        ensure_equals("fov default", light.mFov, kDefaultFov);
    }

    // The cap: eight lights, then a refusal. Order of refusals: clean frame, no region, cap, outside.
    template<> template<>
    void virtual_lights_object::test<3>()
    {
        ensure("seven is fine", placementRefusal(7, true, true, false) == Refusal::NONE);
        ensure("vl_cap_exceeded", placementRefusal(8, true, true, false) == Refusal::AT_CAP);
        ensure("outside the region", placementRefusal(0, true, false, false) == Refusal::OUTSIDE_REGION);
        ensure("no region", placementRefusal(0, false, true, false) == Refusal::NO_REGION);
        ensure("clean frame wins", placementRefusal(8, false, false, true) == Refusal::CLEAN_FRAME);
        ensure_equals("cap is eight", kMaxLights, 8);
        ensure("a refusal has text", std::string(refusalText(Refusal::AT_CAP)).size() > 0);
        ensure("no refusal has no text", std::string(refusalText(Refusal::NONE)).empty());
    }

    // Click-to-place lifts along the normal, and a missing normal lifts straight up.
    template<> template<>
    void virtual_lights_object::test<4>()
    {
        const F32 hit[3] = {10.f, 20.f, 30.f};
        const F32 up[3] = {0.f, 0.f, 1.f};
        F32 out[3];
        liftedPosition(hit, up, out);
        ensure_distance("lift along z", out[2], 30.f + kLiftM, 1.e-5f);
        ensure_equals("x untouched", out[0], 10.f);
        const F32 wall[3] = {2.f, 0.f, 0.f};   // unnormalised on purpose
        liftedPosition(hit, wall, out);
        ensure_distance("lift along the wall normal", out[0], 10.f + kLiftM, 1.e-5f);
        ensure_equals("y untouched", out[1], 20.f);
        const F32 zero[3] = {0.f, 0.f, 0.f};
        liftedPosition(hit, zero, out);
        ensure_distance("zero normal lifts up", out[2], 30.f + kLiftM, 1.e-5f);
    }

    // Aim: yaw/pitch <-> direction round trip, aiming at a point, the degenerate cases.
    template<> template<>
    void virtual_lights_object::test<5>()
    {
        F32 dir[3];
        directionFromAngles(90.f, 0.f, dir);
        ensure_distance("yaw 90 points +Y", dir[1], 1.f, 1.e-5f);
        directionFromAngles(0.f, -90.f, dir);
        ensure_distance("pitch -90 points down", dir[2], -1.f, 1.e-5f);

        F32 yaw = 0.f, pitch = 0.f;
        const F32 east_up[3] = {1.f, 0.f, 1.f};
        ensure("direction resolves", anglesFromDirection(east_up, yaw, pitch));
        ensure_distance("45 up", pitch, 45.f, 1.e-3f);
        ensure_distance("yaw east", yaw, 0.f, 1.e-3f);

        const F32 none[3] = {0.f, 0.f, 0.f};
        yaw = 7.f; pitch = 8.f;
        ensure("zero direction rejected", !anglesFromDirection(none, yaw, pitch));
        ensure_equals("yaw untouched", yaw, 7.f);
        ensure_equals("pitch untouched", pitch, 8.f);

        Light light;
        light.mPos[0] = 100.f; light.mPos[1] = 100.f; light.mPos[2] = 20.f;
        const F32 target[3] = {100.f, 110.f, 20.f};
        ensure("aim at a point", aimAt(light, target));
        ensure_distance("aimed north", light.mYawDeg, 90.f, 1.e-3f);
        ensure_distance("level", light.mPitchDeg, 0.f, 1.e-3f);
        const F32 here[3] = {100.f, 100.f, 20.f};
        ensure("aim at itself refused", !aimAt(light, here));
        ensure_distance("aim kept", light.mYawDeg, 90.f, 1.e-3f);
    }

    // toLLSD / fromLLSD round trip every field, including Track and Practicals.
    template<> template<>
    void virtual_lights_object::test<6>()
    {
        Light light;
        light.mKey = mint();
        light.mOn = false;
        light.mSpot = true;
        light.mColor[0] = 0.25f; light.mColor[1] = 0.5f; light.mColor[2] = 0.75f;
        light.mIntensity = 0.5f;
        light.mRadius = 12.f;
        light.mFalloff = 0.4f;
        light.mPos[0] = 10.f; light.mPos[1] = 20.f; light.mPos[2] = 30.f;
        light.mYawDeg = 33.f;
        light.mPitchDeg = -20.f;
        light.mTexture = mint();
        light.mFov = 1.2f; light.mFocus = 3.f; light.mAmbiance = 0.1f;
        light.mVCamCapture = mint();
        light.mAimObject = mint();
        light.mTrack.mEnabled = true;
        light.mTrack.mHasSubject = true;
        light.mTrack.mSubject = mint();
        light.mTrack.mJoint = 3;
        light.mTrack.mMode = 1;
        light.mTrack.mSmoothing = 0.5f;
        light.mPracticals = LLSD::emptyMap();
        light.mPracticals[kPracticalNoShadow] = true;
        sanitize(light);

        Light back;
        ensure("parses", fromLLSD(toLLSD(light), back));
        ensure("vl_scene_unrestored", sameLight(light, back));
        ensure("key kept", back.mKey == light.mKey);
        ensure("practicals kept", back.mPracticals[kPracticalNoShadow].asBoolean());
        ensure("VCam capture kept", back.mVCamCapture == light.mVCamCapture);
    }

    // A tolerant reader: non-maps rejected, garbage values clamped, missing fields default.
    template<> template<>
    void virtual_lights_object::test<7>()
    {
        Light out;
        ensure("a string is not a light", !fromLLSD(LLSD("nope"), out));
        LLSD wild = LLSD::emptyMap();
        wild["radius"] = 1.0e9;
        wild["intensity"] = 77.0;
        wild["pitch"] = 1.0e9;
        wild["track"] = LLSD::emptyMap();
        wild["track"]["enabled"] = true;     // no subject: must be switched off
        ensure("parses", fromLLSD(wild, out));
        ensure_equals("radius clamped", out.mRadius, kMaxRadius);
        ensure_equals("intensity clamped", out.mIntensity, kMaxIntensity);
        ensure_equals("pitch clamped", out.mPitchDeg, 90.f);
        ensure("vl_track_without_subject", !out.mTrack.mEnabled);
        ensure("missing fields keep defaults", out.mOn && !out.mSpot);
    }

    // The scene block: capped at eight, duplicate / null keys re-minted, non-maps skipped, old scene empty.
    template<> template<>
    void virtual_lights_object::test<8>()
    {
        ensure("no block means no lights", parseScene(LLSD(), mint).empty());
        ensure("a map is not a block", parseScene(LLSD::emptyMap(), mint).empty());

        std::vector<Light> twelve;
        for (S32 i = 0; i < 12; ++i)
        {
            Light light;
            light.mKey = mint();
            light.mPos[0] = static_cast<F32>(i);
            twelve.push_back(light);
        }
        ensure("vl_scene_write_cap", sceneBlock(twelve).size() == kMaxLights);
        LLSD fat = LLSD::emptyArray();
        for (const Light& light : twelve) fat.append(toLLSD(light));
        const std::vector<Light> read = parseScene(fat, mint);
        ensure("vl_scene_cap_exceeded", static_cast<S32>(read.size()) == kMaxLights);
        ensure_equals("order kept", read[3].mPos[0], 3.f);

        LLSD messy = LLSD::emptyArray();
        const LLUUID shared = mint();
        Light a, b, c;
        a.mKey = shared; b.mKey = shared;        // duplicate
        c.mKey = LLUUID::null;                   // null
        messy.append(toLLSD(a));
        messy.append(LLSD("junk"));
        messy.append(toLLSD(b));
        messy.append(toLLSD(c));
        const std::vector<Light> fixed = parseScene(messy, mint);
        ensure_equals("junk skipped", static_cast<S32>(fixed.size()), 3);
        ensure("first key kept", fixed[0].mKey == shared);
        ensure("vl_scene_duplicate_key", fixed[1].mKey != shared && fixed[1].mKey.notNull());
        ensure("null re-minted", fixed[2].mKey.notNull());
    }

    // The list helpers.
    template<> template<>
    void virtual_lights_object::test<9>()
    {
        std::vector<Light> lights(3);
        for (Light& light : lights) light.mKey = mint();
        ensure("contains", containsKey(lights, lights[1].mKey));
        ensure("does not contain", !containsKey(lights, mint()));
        ensure_equals("index", indexOfKey(lights, lights[2].mKey), 2);
        ensure_equals("missing index", indexOfKey(lights, mint()), -1);
    }

    // The full VCam binding settings survive the scene block / undo snapshot; no feed means no settings.
    template<> template<>
    void virtual_lights_object::test<10>()
    {
        Light light;
        light.mKey = mint();
        light.mVCamCapture = mint();
        light.mVCamSettings = LLSD::emptyMap();
        light.mVCamSettings["fit"] = "fill";
        light.mVCamSettings["feed_ev"] = 1.5;
        light.mVCamSettings["flip_h"] = true;
        sanitize(light);
        Light back;
        ensure("parses", fromLLSD(toLLSD(light), back));
        ensure("vl_vcam_settings_lost", back.mVCamSettings["fit"].asString() == "fill" &&
            back.mVCamSettings["flip_h"].asBoolean() && back.mVCamSettings["feed_ev"].asReal() == 1.5);
        ensure("settings take part in the undo delta", !sameLight(light, [&]() { Light x = light; x.mVCamSettings["fit"] = "fit"; return x; }()));
        light.mVCamCapture.setNull();
        sanitize(light);
        ensure("no feed means no settings", light.mVCamSettings.isUndefined());
        Light plain;
        ensure("a light without a feed writes no settings key", !toLLSD(plain).has("vcam_settings"));
    }

    // A recreated light must read its saved values back from the volume: params written before light mode is
    // on are dropped (the volume then reads defaults), which this check must catch, an Off light included.
    template<> template<>
    void virtual_lights_object::test<11>()
    {
        Light off;
        off.mOn = false;
        off.mIntensity = 0.6f;
        off.mRadius = 7.f;
        off.mFalloff = 0.3f;
        ensure("an Off light reads intensity 0 with light mode on", lightApplied(off, true, 0.f, 7.f, 0.3f));
        ensure("vl_recreated_params_lost", !lightApplied(off, true, 1.f, 10.f, 0.75f));
        Light lit = off;
        lit.mOn = true;
        ensure("a non-default light reads its values", lightApplied(lit, true, 0.6f, 7.f, 0.3f));
        ensure("default values are not the saved values", !lightApplied(lit, true, kDefaultIntensity, kDefaultRadius, kDefaultFalloff));
        ensure("light mode off is never applied", !lightApplied(lit, false, 0.6f, 7.f, 0.3f));
    }

    // The Track picker resolves the target it was opened for, in both switch directions, and a virtual object id
    // can never be a real tracker target.
    template<> template<>
    void virtual_lights_object::test<12>()
    {
        const LLUUID real = mint(), key = mint(), virtual_object = mint(), gone = mint();
        auto virtual_alive = [&](const LLUUID& k) { return k == key; };
        auto real_alive = [&](const LLUUID& id) { return id == real || id == virtual_object; };
        auto is_virtual_id = [&](const LLUUID& id) { return id == virtual_object; };

        TrackTargetRef real_target;          // opened on a real projector, the page switched to a virtual light
        real_target.mId = real;
        ensure("vl_track_target_current", trackTargetValid(real_target, virtual_alive, real_alive, is_virtual_id) &&
            !real_target.mVirtual);
        TrackTargetRef virtual_target;       // opened on a virtual light, the page switched to a real projector
        virtual_target.mVirtual = true;
        virtual_target.mKey = key;
        virtual_target.mId = virtual_object;
        ensure("virtual target stays virtual", trackTargetValid(virtual_target, virtual_alive, real_alive, is_virtual_id) &&
            virtual_target.mVirtual);
        TrackTargetRef leaked;
        leaked.mId = virtual_object;
        ensure("vl_virtual_id_reaches_tracker", !trackTargetValid(leaked, virtual_alive, real_alive, is_virtual_id));
        TrackTargetRef vanished;
        vanished.mVirtual = true;
        vanished.mKey = gone;
        ensure("a deleted virtual light does nothing", !trackTargetValid(vanished, virtual_alive, real_alive, is_virtual_id));
        TrackTargetRef nothing;
        ensure("an empty target does nothing", !trackTargetValid(nothing, virtual_alive, real_alive, is_virtual_id));
    }

    // Clone replacement reaches a light's Track subject and aim object, live and inside undo snapshots.
    template<> template<>
    void virtual_lights_object::test<13>()
    {
        const LLUUID old_id = mint(), new_id = mint();
        Light light;
        light.mKey = mint();
        light.mTrack.mHasSubject = true;
        light.mTrack.mEnabled = true;
        light.mTrack.mSubject = old_id;
        light.mAimObject = old_id;
        Light moved = light;
        ensure("changed", rewriteActorInLight(moved, old_id, new_id));
        ensure("vl_track_reference_stale", moved.mTrack.mSubject == new_id && moved.mAimObject == new_id && moved.mTrack.mEnabled);
        Light cleared = light;
        rewriteActorInLight(cleared, old_id, LLUUID::null);
        ensure("a retired clone clears Track", !cleared.mTrack.mHasSubject && !cleared.mTrack.mEnabled);
        LLSD snapshot = LLSD::emptyArray();
        snapshot.append(toLLSD(light));
        rewriteActorInSnapshot(snapshot, old_id, new_id);
        Light from_snapshot;
        ensure("snapshot parses", fromLLSD(snapshot[0], from_snapshot));
        ensure("snapshots follow the replacement", from_snapshot.mTrack.mSubject == new_id);
        // The captured Follow offset rides next to the light map and must survive a replacement (undo/redo need it).
        const F32 keep_pos[3] = {1.f, 2.f, 3.f}, keep_rot[4] = {0.f, 0.f, 0.f, 1.f};
        LLSD with_offset = LLSD::emptyArray();
        LLSD item = toLLSD(light);
        item["track_offset"] = trackOffsetToLLSD(keep_pos, keep_rot);
        with_offset.append(item);
        rewriteActorInSnapshot(with_offset, old_id, new_id);
        ensure("vl_snapshot_offset_dropped", with_offset[0].has("track_offset") &&
            with_offset[0]["track_offset"] == trackOffsetToLLSD(keep_pos, keep_rot));
        Light replaced;
        ensure("offset snapshot still follows", fromLLSD(with_offset[0], replaced) && replaced.mTrack.mSubject == new_id &&
            replaced.mAimObject == new_id);
        Light other = light;
        ensure("other actors untouched", !rewriteActorInLight(other, mint(), new_id) && other.mTrack.mSubject == old_id);
    }

    // Undo restores the Follow offset that was captured (old vs new, after the subject has moved), not a fresh one.
    template<> template<>
    void virtual_lights_object::test<14>()
    {
        const F32 old_pos[3] = {1.f, 2.f, 3.f}, old_rot[4] = {0.f, 0.f, 0.f, 1.f};
        const F32 new_pos[3] = {-4.f, 5.5f, 0.25f}, new_rot[4] = {0.f, 0.70710678f, 0.f, 0.70710678f};
        Light light;
        light.mKey = mint();
        LLSD before = LLSD::emptyArray(), after = LLSD::emptyArray();
        LLSD before_item = toLLSD(light), after_item = toLLSD(light);
        before_item["track_offset"] = trackOffsetToLLSD(old_pos, old_rot);
        after_item["track_offset"] = trackOffsetToLLSD(new_pos, new_rot);
        before.append(before_item);
        after.append(after_item);

        F32 pos[3] = {0.f, 0.f, 0.f}, rot[4] = {0.f, 0.f, 0.f, 1.f};
        ensure("old offset parses", trackOffsetFromLLSD(before[0]["track_offset"], pos, rot));
        ensure("vl_track_offset_lost", pos[0] == old_pos[0] && pos[1] == old_pos[1] && pos[2] == old_pos[2] &&
            rot[0] == old_rot[0] && rot[3] == old_rot[3]);
        F32 redo_pos[3], redo_rot[4];
        ensure("new offset parses", trackOffsetFromLLSD(after[0]["track_offset"], redo_pos, redo_rot));
        ensure("old and new offsets stay distinct", redo_pos[0] == new_pos[0] && redo_pos[1] == new_pos[1] &&
            redo_pos[2] == new_pos[2] && redo_rot[1] == new_rot[1] && redo_pos[0] != pos[0] && redo_rot[1] != rot[1]);
        // The light map itself ignores the extra key, and a scene (toLLSD of the parsed light) never carries it.
        Light parsed;
        ensure("snapshot light parses", fromLLSD(before[0], parsed) && parsed.mKey == light.mKey);
        ensure("scenes carry no offset", !toLLSD(parsed).has("track_offset"));
        // A missing, malformed or non-finite block means "recapture", and never touches the outputs.
        F32 keep_pos[3] = {9.f, 9.f, 9.f}, keep_rot[4] = {0.f, 0.f, 0.f, 1.f};
        ensure("a missing offset is refused", !trackOffsetFromLLSD(LLSD(), keep_pos, keep_rot));
        LLSD bad = trackOffsetToLLSD(old_pos, old_rot);
        bad["rot"].erase(0);
        ensure("a malformed offset is refused", !trackOffsetFromLLSD(bad, keep_pos, keep_rot));
        const F32 nan_pos[3] = {std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f};
        ensure("a non-finite offset is refused", !trackOffsetFromLLSD(trackOffsetToLLSD(nan_pos, old_rot), keep_pos, keep_rot) &&
            keep_pos[0] == 9.f);
    }
}
