#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostanimdrop.h"

namespace tut
{
struct ghost_anim_drop_data
{
    const LLUUID asset = LLUUID("0b2a1c3d-4e5f-6071-8293-a4b5c6d7e8f9");
    const LLUUID clone_a = LLUUID("11111111-1111-1111-1111-111111111111");
    const LLUUID clone_b = LLUUID("22222222-2222-2222-2222-222222222222");
};
typedef test_group<ghost_anim_drop_data> ghost_anim_drop_group;
typedef ghost_anim_drop_group::object ghost_anim_drop_object;
ghost_anim_drop_group ghost_anim_drop_tests("alghostanimdrop");

using namespace ALGhostAnimDrop;

// 3D drop-target classification
template<> template<> void ghost_anim_drop_object::test<1>()
{
    ensure_equals("not over a clone: today's behaviour", classify3D(false, true, true, true, true), VERDICT_PASS_THROUGH);
    ensure_equals("non-clone texture drag untouched", classify3D(false, false, true, true, true), VERDICT_PASS_THROUGH);
    ensure_equals("owned inventory animation plays", classify3D(true, true, true, true, true), VERDICT_PLAY_ON_CLONE);
    ensure_equals("texture over a clone refused", classify3D(true, false, true, true, true), VERDICT_REFUSE);
    ensure_equals("task-inventory source refused", classify3D(true, true, false, true, true), VERDICT_REFUSE);
    ensure_equals("not owned refused", classify3D(true, true, true, false, true), VERDICT_REFUSE);
    ensure_equals("null asset refused", classify3D(true, true, true, true, false), VERDICT_REFUSE);
}

// Card / menu apply plan
template<> template<> void ghost_anim_drop_object::test<2>()
{
    ApplyPlan none = planApply({});
    ensure("nothing selected toasts", none.mNothingSelected);
    ensure("nothing selected plays nothing", none.mPlay.empty());
    ensure("nothing selected switches nothing", !none.mSwitchToDirected);

    ApplyPlan mixed = planApply({ { clone_a, true }, { clone_b, false } });
    ensure("mixed: no toast", !mixed.mNothingSelected);
    ensure("mixed: no switch", !mixed.mSwitchToDirected);
    ensure_equals("mixed: only the Directed clone", mixed.mPlay.size(), (size_t)1);
    ensure("mixed: Directed clone is the target", mixed.mPlay[0] == clone_a);

    ApplyPlan no_directed = planApply({ { clone_a, false }, { clone_b, false } });
    ensure("none Directed: switch", no_directed.mSwitchToDirected);
    ensure_equals("none Directed: every selected clone", no_directed.mPlay.size(), (size_t)2);
}

// Never-expose rule: the editor text
template<> template<> void ghost_anim_drop_object::test<3>()
{
    ensure_equals("null asset shows nothing", directedAnimEditorText(LLUUID::null, true, "x"), "");
    ensure_equals("full-perm shows the UUID", directedAnimEditorText(asset, true, "Wave"), asset.asString());
    const std::string hidden = directedAnimEditorText(asset, false, "Wave");
    ensure_equals("non-full-perm shows the name", hidden, "Wave (inventory animation, UUID hidden)");
    // PLANTED NEGATIVE: any UUID leak into the editor text fails here.
    ensure("non-full-perm editor text never carries the UUID", !mentionsUuid(hidden, asset));
    ensure("non-full-perm, no name, still hidden", !mentionsUuid(directedAnimEditorText(asset, false, ""), asset));
    ensure_equals("non-full-perm, no name, generic text", directedAnimEditorText(asset, false, " \t"), "(inventory animation, UUID hidden)");
}

// Never-expose rule: every UI string of the feature
template<> template<> void ghost_anim_drop_object::test<4>()
{
    const std::string name = "No-mod dance";
    const std::string clone = "Clone 2";
    const std::string strings[] = {
        hoverText(name, clone),
        playingToast(name, clone),
        playingToast(name, (size_t)3),
        selectCloneFirstToast(),
        menuDisabledReason(),
        cloneLabel(""),
        cloneLabel(clone),
    };
    for (const std::string& text : strings)
    {
        // PLANTED NEGATIVE: a UUID in any toast/tooltip/menu text fails the gate.
        ensure("UI text never carries an asset UUID: " + text, !mentionsUuid(text, asset));
    }
    ensure_equals("hover text", hoverText(name, clone), "Play No-mod dance on Clone 2");
    ensure_equals("toast text", playingToast(name, clone), "Playing No-mod dance on Clone 2");
    ensure_equals("toast count text", playingToast(name, (size_t)3), "Playing No-mod dance on 3 clones");
    ensure_equals("toast single count", playingToast(name, (size_t)1), "Playing No-mod dance on 1 clone");
    ensure_equals("unnamed clone label", cloneLabel("  "), "the clone");
}

// The oracle itself must catch disguised leaks
template<> template<> void ghost_anim_drop_object::test<5>()
{
    ensure("plain", mentionsUuid("x " + asset.asString() + " y", asset));
    std::string upper = asset.asString();
    for (char& c : upper) c = (char)toupper((unsigned char)c);
    ensure("upper case", mentionsUuid(upper, asset));
    std::string nodash;
    for (char c : asset.asString()) if (c != '-') nodash += c;
    ensure("dashes stripped", mentionsUuid(nodash, asset));
    ensure("prefix only is not the UUID", !mentionsUuid(asset.asString().substr(0, 8), asset));
    ensure("null id never matches", !mentionsUuid("00000000-0000-0000-0000-000000000000", LLUUID::null));
}

// Worn-attachment entry acceptance: masked ids are counted, never guessed
template<> template<> void ghost_anim_drop_object::test<6>()
{
    WornNameTally tally;
    ensure("non-animation ignored", !acceptWornEntry(false, true, "Script", tally));
    ensure("masked asset id not recorded", !acceptWornEntry(true, false, "Hidden dance", tally));
    ensure("blank name not recorded", !acceptWornEntry(true, true, " ", tally));
    ensure("named animation recorded", acceptWornEntry(true, true, "Dance 1", tally));
    ensure_equals("named count", tally.mNamed, (size_t)1);
    ensure_equals("masked count", tally.mMasked, (size_t)1);
}

// Restriction ledger: monotonic, revisioned; the editor sink re-masks late
template<> template<> void ghost_anim_drop_object::test<7>()
{
    RestrictionLedger ledger;
    ensure("nothing hidden yet", !ledger.hidden(asset));
    ensure("exposable sighting records nothing", !ledger.note(asset, true));
    ensure_equals("revision untouched", ledger.revision(), (U64)0);
    // The editor shows the UUID while the asset is believed full perm...
    ensure_equals("pre-restriction editor", directedAnimEditorText(asset, !ledger.hidden(asset), "Late"), asset.asString());
    // ...then the item is resolved (drop / menu / link) as non-full-perm.
    ensure("restriction recorded", ledger.note(asset, false));
    ensure_equals("revision bumped", ledger.revision(), (U64)1);
    ensure("hidden now", ledger.hidden(asset));
    // PLANTED NEGATIVE: the sink must redact after a late restriction.
    const std::string late = directedAnimEditorText(asset, !ledger.hidden(asset), "Late");
    ensure("late restriction redacts the editor", !mentionsUuid(late, asset));
    ensure("a later full-perm sighting cannot un-hide", !ledger.note(asset, true));
    ensure("still hidden (monotonic)", ledger.hidden(asset));
    ensure("duplicate hidden sighting is idempotent", !ledger.note(asset, false));
    ensure_equals("revision stable on no-op", ledger.revision(), (U64)1);
    ensure("null id never hidden", !ledger.note(LLUUID::null, false) && !ledger.hidden(LLUUID::null));
}

// Kit row clipboard: the real sink ("Copy value" of the Inventory / Source /
// Anim UUID rows) must refuse a protected id in every LLSD shape
template<> template<> void ghost_anim_drop_object::test<8>()
{
    RestrictionLedger ledger;
    ensure("uuid value allowed before restriction", clipboardValueAllowed(LLSD(asset), ledger));
    ledger.note(asset, false);
    // PLANTED NEGATIVES: any of these reaching the clipboard fails the gate.
    ensure("combo value (LLSD uuid) refused", !clipboardValueAllowed(LLSD(asset), ledger));
    ensure("editor text (uuid string) refused", !clipboardValueAllowed(LLSD(asset.asString()), ledger));
    std::string upper = asset.asString();
    for (char& c : upper) c = (char)toupper((unsigned char)c);
    ensure("upper-case string refused", !clipboardValueAllowed(LLSD(upper), ledger));
    ensure("string mentioning the id refused", !clipboardValueAllowed(LLSD("Wave " + asset.asString()), ledger));
    ensure("masked editor text allowed", clipboardValueAllowed(LLSD(directedAnimEditorText(asset, false, "Wave")), ledger));
    ensure("other uuid allowed", clipboardValueAllowed(LLSD(clone_a), ledger));
    ensure("integer value allowed", clipboardValueAllowed(LLSD(3), ledger));
    ensure("undefined allowed", clipboardValueAllowed(LLSD(), ledger));
}

// 3D drop: screen proximity never steals a drop from a nearer real pick
template<> template<> void ghost_anim_drop_object::test<9>()
{
    ensure("no real pick: clone wins", cloneInFront(false, false, 10.f, 0.f));
    ensure("clone nearer than the pick: clone wins", cloneInFront(true, false, 4.f, 9.f));
    ensure("real pick nearer: today's behaviour", !cloneInFront(true, false, 9.f, 4.f));
    ensure("equal distance: today's behaviour", !cloneInFront(true, false, 5.f, 5.f));
    // PLANTED NEGATIVE: a HUD hit near a clone probe keeps the drop whatever
    // its (HUD-space) distance says.
    ensure("HUD hit always wins", !cloneInFront(true, true, 0.1f, 50.f));
    ensure("HUD flag alone wins", !cloneInFront(false, true, 0.f, 0.f));
}
}
