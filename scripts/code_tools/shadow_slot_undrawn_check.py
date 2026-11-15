"""Projector shadow slots: an owner that leaves the view keeps its slot (RenderShadowSlotUndrawnHold).

Three parts, no viewer build or launch:
  1. wiring of pipeline.cpp / pipeline.h / settings.xml, with planted regressions;
  2. the real TUT shadow-slot tests compiled with MSVC against this worktree's header,
     then header mutants that restore the old behaviour (an undrawn owner is a vacancy);
  3. a frame-by-frame model of the native slot update (targets -> fade -> hand-over, the
     statements of LLPipeline::generateSunShadow) driven by the compiled production
     reconciliation, old against new, for a zoom and a pan.
Mutated headers and native products stay under .tmp/projflicker.
"""
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_check_tools import compile_run, viewer_native_flags

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / "indra/newview"
OUT = ROOT / ".tmp/projflicker/native"


def check_wiring(pipe, header, settings, math):
    native = pipe[pipe.index("void LLPipeline::retainSpotShadowOwners()"):pipe.index("// [BDMerge G3.3] Side-effect-free variant")]
    code = re.sub(r"//[^\n]*", "", native)
    # The switch, default ON, read once per call.
    assert 'static LLCachedControl<bool> undrawn_hold(gSavedSettings, "RenderShadowSlotUndrawnHold", true);' in native
    block = settings[settings.index("<key>RenderShadowSlotUndrawnHold</key>"):]
    block = block[:block.index("</map>")]
    assert "<string>Boolean</string>" in block and "<key>Value</key><integer>1</integer>" in block
    assert "<key>Persist</key><integer>1</integer>" in block
    # After the stale-auction early-out: a skipped or secondary lighting pass keeps the stock owners.
    assert native.index("if (!recent_auction) { mSpotChallenge = {}; return; }") < native.index("undrawn_hold(gSavedSettings")
    # The one statement of the fix, in the else-chain behind the eligible and the deleted owner.
    eligible = code.index("owners[i] = mSpotAuctionOwnerScores[i];")
    dead = code.index("else if (owner && owner->isDead())")
    hold = code.index("else if (undrawn_hold() && volume && mSpotAuctionEligible[i] != owner && owner->isState(LLDrawable::LIGHT) &&")
    assert eligible < dead < hold < code.index("const auto chosen = ALShadowSlot::retainOwners(")
    statement = code[hold:code.index("if (owners[i].tier != ALShadowSlot::UndrawnHeldTier")]
    assert "!isProjectorShadowSuppressed(volume) &&" in statement, "Cast Shadows off must keep the stock eviction"
    assert ("!(ALCineLightRigManager::anyActorOnly() && ALCineLightRigManager::instance().actorOnlyBits(volume->getID())))"
            in statement), "actor-only rig lights keep the stock immediate release"
    assert "owners[i] = ALShadowSlot::undrawnOwner(mSpotUndrawnHold[i], volume->getID(), undrawn_now, gFrameCount);" in statement
    assert code.count("undrawn_hold()") == 1, "every other statement must run identically with the switch off"
    assert code.count("ALShadowSlot::undrawnOwner(") == 1
    # The absence clock restarts whenever the slot's owner is drawn, empty or evicted.
    assert ("if (owners[i].tier != ALShadowSlot::UndrawnHeldTier && owners[i].tier != ALShadowSlot::UndrawnReleasedTier)\n"
            "            mSpotUndrawnHold[i] = {};") in native
    # The reconciliation and the mapping to drawables are today's statements.
    assert "const auto chosen = ALShadowSlot::retainOwners(owners, ranked, count, mSpotChallenge, LLTimer::getTotalSeconds(), gFrameCount);" in native
    assert "if (chosen[i] == owners[j].id) { targets[i] = mShadowSpotLight[j]; break; }" in native
    assert "mSpotLightFade" not in code[hold:code.index("const auto chosen = ALShadowSlot::retainOwners(")], "the fix never writes a fade"
    # Timers: a region crossing clears the clock with the challenge holds; hard cuts go through the same function.
    timers = pipe[pipe.index("void LLPipeline::resetShadowSlotTimers()"):pipe.index("// Both existing residency")]
    assert "mSpotUndrawnHold = {};" in timers
    assert "std::array<ALShadowSlot::UndrawnHold<LLUUID>, MAX_SPOT_SHADOWS> mSpotUndrawnHold{};" in header
    # Pure helper: held above, released below every real tier (alshaftdecision.h: 0..3).
    assert "enum : int { UndrawnHeldTier = 1 << 20, UndrawnReleasedTier = -(1 << 20) };" in math
    assert "const bool released = state.hold.reached(true, now, frame, 0.2);" in math
    assert "return {owner, 0.0, released ? UndrawnReleasedTier : UndrawnHeldTier};" in math
    # The diagnostic log: off by default, not persisted, read-only, and outside the reconciliation.
    log_block = settings[settings.index("<key>RenderShadowSlotLog</key>"):]
    log_block = log_block[:log_block.index("</map>")]
    assert "<key>Value</key><integer>0</integer>" in log_block and "<key>Persist</key><integer>0</integer>" in log_block
    assert 'static LLCachedControl<bool> slot_log(gSavedSettings, "RenderShadowSlotLog", false);' in pipe
    assert "if (slot_log()) logSpotShadowSlots(num_spots, retain_slots);\n            else mSpotSlotLogValid = false;" in pipe
    assert pipe.count("logSpotShadowSlots(") == 2, "one definition, one call"
    log = pipe[pipe.index("void LLPipeline::logSpotShadowSlots("):pipe.index("void LLPipeline::snapshotSpotShadowAuction()")]
    log_code = re.sub(r"//[^\n]*", "", log)
    for written in ("mShadowSpotLight[", "mTargetShadowSpotLight[", "mSpotLightFade[", "mSpotAuctionEligible[",
                    "mSpotChallenge", "mSpotUndrawnHold", "mSpotAuctionMain", "mSpotAuctionOwnerScores"):
        assert not re.search(re.escape(written) + r"[^;=!<>]*[^=!<>]=[^=]", log_code), ("the log writes renderer state", written)
    assert 'LL_INFOS("ShadowSlotLog")' in log and "verdict=" in log
    for field in (" light=", " old_slot=", " new_slot=", " reason=", " drawn=", " candidates="):
        assert field in log, field
    assert "logSpotShadowSlots" not in native and "mSpotSlotLog" not in native
    # The candidate count is diagnostic only: one increment, one reset, read by the log alone.
    assert pipe.count("++mSpotAuctionDrawn;") == 1 and pipe.count("mSpotAuctionDrawn = 0;") == 1
    assert len(re.findall(r"\bmSpotAuctionDrawn\b", pipe.replace(log, ""))) == 2


def wiring():
    pipe = (NV / "pipeline.cpp").read_text(encoding="utf-8")
    header = (NV / "pipeline.h").read_text(encoding="utf-8")
    settings = (NV / "app_settings/settings.xml").read_text(encoding="utf-8")
    math = (NV / "alshadowslotmath.h").read_text(encoding="utf-8")
    check_wiring(pipe, header, settings, math)
    hold_line = "            owners[i] = ALShadowSlot::undrawnOwner(mSpotUndrawnHold[i], volume->getID(), undrawn_now, gFrameCount);\n"
    hold_if = ("        else if (undrawn_hold() && volume && mSpotAuctionEligible[i] != owner && owner->isState(LLDrawable::LIGHT) &&\n"
               "                 !isProjectorShadowSuppressed(volume) &&\n"
               "                 !(ALCineLightRigManager::anyActorOnly() && ALCineLightRigManager::instance().actorOnlyBits(volume->getID())))\n")
    setting = settings[settings.index("<key>RenderShadowSlotUndrawnHold</key>"):]
    setting = setting[:setting.index("</map>")]
    mutants = {
        # The old behaviour: an undrawn owner is a vacancy.
        "old_behaviour_branch_removed": (pipe.replace(hold_if + hold_line, ""), header, settings, math),
        "old_behaviour_statement_empty": (pipe.replace(hold_line, "            ;\n"), header, settings, math),
        "switch_ignored": (pipe.replace("else if (undrawn_hold() && volume &&", "else if (volume &&"), header, settings, math),
        "switch_inverted": (pipe.replace("else if (undrawn_hold() && volume &&", "else if (!undrawn_hold() && volume &&"), header, settings, math),
        "default_off_in_code": (pipe.replace('"RenderShadowSlotUndrawnHold", true);', '"RenderShadowSlotUndrawnHold", false);'), header, settings, math),
        "default_off_in_settings": (pipe, header, settings.replace(setting, setting.replace("<key>Value</key><integer>1</integer>", "<key>Value</key><integer>0</integer>")), math),
        "suppressed_light_keeps_shadow": (pipe.replace("                 !isProjectorShadowSuppressed(volume) &&\n                 !(ALCineLightRigManager::anyActorOnly()", "                 !(ALCineLightRigManager::anyActorOnly()"), header, settings, math),
        "actor_light_held": (pipe.replace(" &&\n                 !(ALCineLightRigManager::anyActorOnly() && ALCineLightRigManager::instance().actorOnlyBits(volume->getID())))\n            owners[i] = ALShadowSlot::undrawnOwner(", ")\n            owners[i] = ALShadowSlot::undrawnOwner("), header, settings, math),
        "switched_off_light_held": (pipe.replace("mSpotAuctionEligible[i] != owner && owner->isState(LLDrawable::LIGHT) &&", "mSpotAuctionEligible[i] != owner &&"), header, settings, math),
        "drawn_owner_treated_as_absent": (pipe.replace("volume && mSpotAuctionEligible[i] != owner && owner->isState", "volume && owner->isState"), header, settings, math),
        "ahead_of_stale_auction_exit": (pipe.replace("    if (!recent_auction) { mSpotChallenge = {}; return; } // Keep the stock targets/owners after skipped or secondary lighting.\n", "", 1)
                                        .replace("    const F64 undrawn_now = LLTimer::getTotalSeconds();\n", "    const F64 undrawn_now = LLTimer::getTotalSeconds();\n    if (!recent_auction) { mSpotChallenge = {}; return; } // Keep the stock targets/owners after skipped or secondary lighting.\n", 1), header, settings, math),
        "clock_never_restarts": (pipe.replace("            mSpotUndrawnHold[i] = {}; // [SlotUndrawn] drawn, empty or evicted: the absence clock restarts\n", "            ;\n"), header, settings, math),
        "crossing_keeps_clock": (pipe.replace("    mSpotUndrawnHold = {}; // [SlotUndrawn]\n", ""), header, settings, math),
        "frame_clock": (pipe.replace("mSpotUndrawnHold[i], volume->getID(), undrawn_now, gFrameCount);", "mSpotUndrawnHold[i], volume->getID(), gFrameCount, gFrameCount);"), header, settings, math),
        "tiers_inside_real_range": (pipe, header, settings, math.replace("UndrawnHeldTier = 1 << 20, UndrawnReleasedTier = -(1 << 20)", "UndrawnHeldTier = 2, UndrawnReleasedTier = 0")),
        "log_on_by_default": (pipe.replace('"RenderShadowSlotLog", false);', '"RenderShadowSlotLog", true);'), header, settings, math),
        "log_keeps_stale_history": (pipe.replace("            else mSpotSlotLogValid = false;\n", ""), header, settings, math),
        "log_evicts_owner": (pipe.replace("        if (!event) continue;\n", "        if (!event) continue;\n        mShadowSpotLight[i] = NULL;\n", 1), header, settings, math),
        "log_resets_fade": (pipe.replace("        if (!event) continue;\n", "        if (!event) continue;\n        mSpotLightFade[i] = 0.f;\n", 1), header, settings, math),
        "candidate_count_feeds_auction": (pipe.replace("if (!potential) break;", "if (!potential || mSpotAuctionDrawn > 64u) break;", 1), header, settings, math),
    }
    for name, args in mutants.items():
        assert args != (pipe, header, settings, math), (name, "mutation drift: nothing changed")
        try:
            check_wiring(*args)
        except (AssertionError, ValueError):
            continue
        raise AssertionError("wiring mutant survived: " + name)
    print("undrawn-owner wiring: PASS, %d mutants rejected" % len(mutants), flush=True)


RUNNER = r'''
#include <tut/tut_reporter.hpp>
namespace tut { test_runner_singleton runner; }
int main() {
    tut::reporter reporter;
    tut::runner.get().set_callback(&reporter);
    tut::runner.get().run_tests();
    return reporter.all_ok() ? 0 : 1;
}
'''

# The native slot update of LLPipeline::generateSunShadow, statement for statement (world lights, no actor rig),
# around the PRODUCTION reconciliation. Lights are ints; 0 is "no light". One call is one frame at 30 fps.
MODEL = r'''
#include "alshadowslotmath.h"
#include <array>
#include <cstdio>
#include <vector>
namespace {
const unsigned N = 16;
struct Candidate { int id; double priority; };
struct World
{
    unsigned count = 4;
    bool hold = true;
    std::array<int, N> owner{};
    std::array<double, N> fade{};
    std::array<ALShadowSlot::Challenge<int>, N> challenge{};
    std::array<ALShadowSlot::UndrawnHold<int>, N> undrawn{};
    unsigned frame = 1;
    // What the lighting pass of the frame just rendered showed for one drawn light: 1 = shadowed, 0 = unshadowed.
    double shown(int id) const
    {
        for (unsigned i = 0; i < count; ++i) if (owner[i] == id) return fade[i];
        return 0.0;
    }
    // drawn: the projectors the main lighting pass drew last frame, any order. camera_speed: metres per second.
    void step(const std::vector<Candidate>& drawn, double camera_speed)
    {
        const double dt = 1.0 / 30.0, now = frame * dt;
        // setupSpotLight: the stock auction over the drawn lights, and the eligible owners' scores.
        std::array<ALShadowSlot::Light<int>, N> ranked{}, owners{};
        for (const auto& c : drawn)
        {
            ALShadowSlot::Light<int> potential{c.id, c.priority, 1};
            for (unsigned i = 0; i < count && potential.id; ++i)
                if (!ranked[i].id || potential.priority > ranked[i].priority) std::swap(ranked[i], potential);
            for (unsigned i = 0; i < count; ++i) if (owner[i] == c.id) owners[i] = {c.id, c.priority, 1};
        }
        // retainSpotShadowOwners.
        for (unsigned i = 0; i < count; ++i)
        {
            if (hold && owner[i] && !owners[i].id)
                owners[i] = ALShadowSlot::undrawnOwner(undrawn[i], owner[i], now, frame);
            if (owners[i].tier != ALShadowSlot::UndrawnHeldTier && owners[i].tier != ALShadowSlot::UndrawnReleasedTier)
                undrawn[i] = {};
        }
        const auto target = ALShadowSlot::retainOwners(owners, ranked, count, challenge, now, frame);
        // generateSunShadow: fade toward the target state, hand an empty or faded-out slot to a pending target.
        const double fade_amt = ALShadowSlot::fadeStep(dt * std::max(camera_speed, 1.0), dt, true);
        for (unsigned i = 0; i < count; ++i)
        {
            bool is_target = false;
            for (unsigned j = 0; j < count; ++j) is_target = is_target || (owner[i] && owner[i] == target[j]);
            if (is_target) { fade[i] = std::min(fade[i] + fade_amt, 1.0); continue; }
            fade[i] = std::max(fade[i] - fade_amt, 0.0);
            if (fade[i] != 0.0 && owner[i]) continue;
            for (unsigned j = 0; j < count; ++j)
            {
                if (!target[j]) continue;
                bool held = false;
                for (unsigned k = 0; k < count; ++k) held = held || (k != i && owner[k] == target[j]);
                if (!held) { owner[i] = target[j]; break; }
            }
        }
        ++frame;
    }
};
int failures = 0;
void expect(bool ok, const char* what) { if (!ok) { ++failures; std::printf("FAIL %s\n", what); } }
// Frames in which a DRAWN light is lit with a shadow fade below 1, after a settle period: what the user sees as a pop.
unsigned unsteady(World& w, const std::vector<std::vector<Candidate>>& shots, unsigned frames_per_shot, double speed, int watch)
{
    unsigned bad = 0;
    for (unsigned cycle = 0; cycle < 6; ++cycle)
        for (const auto& shot : shots)
            for (unsigned f = 0; f < frames_per_shot; ++f)
            {
                bool drawn = false;
                for (const auto& c : shot) drawn = drawn || c.id == watch;
                w.step(shot, speed);
                // The lighting pass that follows draws `shot` with the fades just computed.
                if (cycle >= 1 && drawn && w.shown(watch) < 1.0) ++bad;
            }
    return bad;
}
}
int main()
{
    // 1. Zoom with spare slots: 3 projectors, 4 slots. Zooming in drops the lamp (7) out of the frustum for one
    //    second, zooming out brings it back. The camera does not move (speed 0), so the stock fade lasts one second.
    const std::vector<Candidate> wide{{7, 300.0}, {8, 900.0}, {9, 500.0}}, tight{{8, 3600.0}, {9, 2000.0}};
    for (bool hold : {false, true})
    {
        World w; w.hold = hold;
        const unsigned bad = unsteady(w, {wide, tight}, 30, 0.0, 7);
        std::printf("zoom, spare slots, hold=%d: %u frames with the lamp's shadow below full\n", int(hold), bad);
        if (hold) expect(bad == 0, "hold ON: a returning light must be shadowed on its first frame");
        else expect(bad >= 100, "hold OFF must reproduce the fade-in on every return (the reported flicker)");
    }
    // 2. Pan with spare slots, camera at 3 m/s: the same, the stock fade is three times faster.
    for (bool hold : {false, true})
    {
        World w; w.hold = hold;
        const unsigned bad = unsteady(w, {wide, tight}, 30, 3.0, 7);
        std::printf("pan, spare slots, hold=%d: %u frames with the lamp's shadow below full\n", int(hold), bad);
        if (hold) expect(bad == 0, "hold ON: pan");
        else expect(bad >= 30, "hold OFF must reproduce the fade-in on every return under a pan");
    }
    // 3. Frame-edge flap with MORE projectors than slots (6 lights, 4 slots): the lamp's box leaves the frustum
    //    for two frames in every ten. Old: the slot is offered to a waiting light at once and the lamp's shadow dips.
    const std::vector<Candidate> all{{7, 800.0}, {8, 900.0}, {9, 700.0}, {10, 600.0}, {11, 100.0}, {12, 90.0}};
    std::vector<Candidate> without = all; without.erase(without.begin());
    for (bool hold : {false, true})
    {
        World w; w.hold = hold;
        unsigned bad = 0, owner_changes = 0;
        for (unsigned f = 0; f < 600; ++f)
        {
            const bool in_view = f % 10 < 8;
            const auto before = w.owner;
            w.step(in_view ? all : without, 2.0);
            if (f >= 60 && before != w.owner) ++owner_changes;
            if (f >= 60 && in_view && w.shown(7) < 1.0) ++bad;
        }
        std::printf("edge flap, 6 lights / 4 slots, hold=%d: %u dipped frames, %u owner changes\n", int(hold), bad, owner_changes);
        expect(owner_changes == 0, "a two-frame absence never hands the slot over");
        if (hold) expect(bad == 0, "hold ON: a two-frame absence leaves the shadow at full strength");
        else expect(bad >= 40, "hold OFF must reproduce the shadow dip at the frame edge");
    }
    // 4. Scarce slots, long absence: after 0.2 s the slot must go to the waiting light (no slot is wasted off screen).
    {
        World w;
        for (unsigned f = 0; f < 60; ++f) w.step(all, 0.0);
        expect(w.shown(7) == 1.0 && w.shown(11) == 0.0, "top four own the slots");
        unsigned frames = 0;
        while (w.shown(11) < 1.0 && frames < 300) { w.step(without, 0.0); ++frames; }
        std::printf("scarce, lamp leaves for good: the waiting light is fully shadowed after %u frames\n", frames);
        expect(frames >= 6 + 30 && frames <= 6 + 30 + 30 + 6, "0.2 s hold, then the stock fade-out and fade-in");
        bool unique = true;
        for (unsigned i = 0; i < 4; ++i) for (unsigned j = i + 1; j < 4; ++j) unique = unique && (!w.owner[i] || w.owner[i] != w.owner[j]);
        expect(unique, "no light owns two slots");
    }
    // 5. An empty slot is used before an absent owner is displaced.
    {
        World w;
        const std::vector<Candidate> two{{7, 800.0}, {8, 900.0}}, other{{8, 900.0}, {9, 700.0}, {10, 600.0}};
        for (unsigned f = 0; f < 60; ++f) w.step(two, 0.0);
        for (unsigned f = 0; f < 120; ++f) w.step(other, 0.0);
        expect(w.shown(9) == 1.0 && w.shown(10) == 1.0, "newcomers take the two empty slots");
        expect(w.shown(7) == 1.0, "the absent lamp keeps its slot and its fade: nobody needed it");
    }
    std::printf(failures ? "MODEL FAIL\n" : "MODEL PASS\n");
    return failures ? 1 : 0;
}
'''


def native():
    OUT.mkdir(parents=True, exist_ok=True)
    flags, libraries = viewer_native_flags()
    flags = [f'/I"{OUT}"', f'/I"{ROOT / "indra/externals/tut"}"'] + flags
    source = (NV / "tests/alshadowslotmath_test.cpp").read_text(encoding="utf-8")
    source = source.replace('"../test/lltut.h"', '"' + (ROOT / "indra/test/lltut.h").as_posix() + '"')
    source = re.sub(r'"\.\./(al[^"/]+\.h)"', r'"\1"', source) + RUNNER
    assert "shadow_slot_object::test<28>()" in source and "shadow_slot_object::test<29>()" in source
    shadow = OUT / "alshadowslotmath.h"
    if shadow.exists():
        shadow.unlink()
    result = compile_run(OUT, source, "slot_tut", flags, libraries)
    print(result.stdout.strip(), flush=True)
    assert result.returncode == 0, result.stdout + result.stderr
    model = compile_run(OUT, MODEL, "slot_model", flags, libraries)
    print(model.stdout.strip(), flush=True)
    assert model.returncode == 0 and "MODEL PASS" in model.stdout, model.stdout + model.stderr

    original = (NV / "alshadowslotmath.h").read_text(encoding="utf-8")
    ret = "    return {owner, 0.0, released ? UndrawnReleasedTier : UndrawnHeldTier};"
    mutations = {
        # (before, after, MUST fail the TUT tests, MUST fail the frame model)
        "old_behaviour_vacancy": (ret, "    return {};", True, True),
        "never_held": (ret, "    return {owner, 0.0, UndrawnReleasedTier};", True, True),
        "never_released": (ret, "    return {owner, 0.0, UndrawnHeldTier};", True, True),
        "no_hold_time": ("state.hold.reached(true, now, frame, 0.2);", "state.hold.reached(true, now, frame, 0.0);", True, True),
        "ten_second_hold": ("state.hold.reached(true, now, frame, 0.2);", "state.hold.reached(true, now, frame, 10.0);", True, True),
        "shared_clock": ("    if (state.owner != owner) { state = {}; state.owner = owner; }\n    const bool released", "    const bool released", True, False),
        "held_below_real_tiers": ("UndrawnHeldTier = 1 << 20,", "UndrawnHeldTier = 1,", True, False),
        "released_above_world": ("UndrawnReleasedTier = -(1 << 20)", "UndrawnReleasedTier = 2", True, False),
    }
    try:
        for name, (before, after, tut_fails, model_fails) in mutations.items():
            assert original.count(before) == 1, (name, "mutation drift")
            shadow.write_text(original.replace(before, after), encoding="utf-8")
            tut = compile_run(OUT, source, "tut_" + name, flags, libraries)
            sim = compile_run(OUT, MODEL, "model_" + name, flags, libraries)
            assert tut.returncode != 0 or not tut_fails, ("planted regression survived the TUT tests", name, tut.stdout[-400:])
            assert sim.returncode != 0 or not model_fails, ("planted regression survived the frame model", name, sim.stdout[-400:])
            assert tut.returncode != 0 or sim.returncode != 0, "planted regression survived: " + name
            print("PASS rejected planted regression:", name,
                  "(TUT %s, model %s)" % ("fails" if tut.returncode else "passes", "fails" if sim.returncode else "passes"), flush=True)
    finally:
        if shadow.exists():
            shadow.unlink()
    print("undrawn-owner native: PASS, real TUT tests + frame model; %d header mutants rejected" % len(mutations), flush=True)


if __name__ == "__main__":
    wiring()
    if "--wiring-only" not in sys.argv:
        native()
    print("PASS: shadow_slot_undrawn_check", flush=True)
