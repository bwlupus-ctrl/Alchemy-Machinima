/**
 * @file alclientheightmodel_test.cpp
 * @brief [ClientHeight] Tests for the pure client-side avatar height model:
 *        the offset store, the per-avatar resolve decision / cache, the effective
 *        (worn animesh) offset, the forced-update rule and the seat-space lift.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alclientheightmodel.h"

#include <cmath>
#include <limits>

namespace tut
{
    using namespace ALClientHeightModel;

    struct client_height_data {};
    typedef test_group<client_height_data> client_height_group;
    typedef client_height_group::object client_height_object;
    client_height_group client_height_tests("ALClientHeightModel");

    namespace
    {
    // A key resolver that records how often it ran.
    struct CountingResolver
    {
        LLUUID mKey;
        int*   mCalls;
        LLUUID operator()() const { ++(*mCalls); return mKey; }
    };
    }

    // Clamp at +/-2 and reject NaN / Inf (the store is untouched).
    template<> template<>
    void client_height_object::test<1>()
    {
        Store store;
        const LLUUID a = LLUUID::generateNewID();
        ensure("set +5 changes", store.set(a, 5.f));
        ensure_equals("clamped high", store.get(a), 2.f);
        ensure("set -9 changes", store.set(a, -9.f));
        ensure_equals("clamped low", store.get(a), -2.f);

        const U64 rev = store.revision();
        ensure("NaN rejected", !store.set(a, std::numeric_limits<F32>::quiet_NaN()));
        ensure("+Inf rejected", !store.set(a, std::numeric_limits<F32>::infinity()));
        ensure("-Inf rejected", !store.set(a, -std::numeric_limits<F32>::infinity()));
        ensure_equals("value kept after rejects", store.get(a), -2.f);
        ensure_equals("rejects do not bump", store.revision(), rev);
        ensure("null key rejected", !store.set(LLUUID::null, 1.f));
        ensure_equals("null key not stored", store.size(), (size_t)1);

        F32 out = 7.f;
        ensure("sanitize NaN false", !sanitize(std::numeric_limits<F32>::quiet_NaN(), out));
        ensure_equals("sanitize leaves out untouched", out, 7.f);
    }

    // Revision bumps only on a real change; reset of an absent key does not bump;
    // resetAll empties and bumps once.
    template<> template<>
    void client_height_object::test<2>()
    {
        Store store;
        const LLUUID a = LLUUID::generateNewID();
        const LLUUID b = LLUUID::generateNewID();
        const U64 r0 = store.revision();

        ensure("first set changes", store.set(a, 0.5f));
        const U64 r1 = store.revision();
        ensure("rev bumped by real change", r1 > r0);
        ensure("same value is no change", !store.set(a, 0.5f));
        ensure_equals("same value does not bump", store.revision(), r1);

        ensure("reset of absent key is no change", !store.reset(b));
        ensure_equals("absent reset does not bump", store.revision(), r1);

        ensure("second key set", store.set(b, -0.25f));
        const U64 r2 = store.revision();
        ensure("resetAll changes", store.resetAll());
        ensure("store empty", store.empty());
        ensure_equals("resetAll bumps exactly once", store.revision(), r2 + 1);
        ensure("resetAll on empty is no change", !store.resetAll());
        ensure_equals("empty resetAll does not bump", store.revision(), r2 + 1);
    }

    // Keys are independent (clone vs source); a set to 0 erases the entry; an empty
    // store is the fast path; clearSession() (the disconnect path) empties it.
    template<> template<>
    void client_height_object::test<3>()
    {
        Store store;
        ensure("fresh store empty", store.empty());
        ensure_equals("empty store reads 0", store.get(LLUUID::generateNewID()), 0.f);

        const LLUUID source = LLUUID::generateNewID();
        const LLUUID clone  = LLUUID::generateNewID();
        store.set(source, 1.5f);
        store.set(clone, -0.5f);
        ensure_equals("source value", store.get(source), 1.5f);
        ensure_equals("clone value", store.get(clone), -0.5f);
        ensure_equals("two entries", store.size(), (size_t)2);

        ensure("set to 0 erases", store.set(source, 0.f));
        ensure_equals("erased entry reads 0", store.get(source), 0.f);
        ensure_equals("clone untouched by source erase", store.get(clone), -0.5f);
        ensure_equals("one entry left", store.size(), (size_t)1);

        // sub-half-millimetre values snap to "no offset"
        ensure("tiny value on the clone erases", store.set(clone, 0.0001f));
        ensure("store empty again", store.empty());

        store.set(source, 1.f);
        store.set(clone, 1.f);
        const U64 before = store.revision();
        ensure("clearSession empties", store.clearSession());
        ensure("empty after clearSession", store.empty());
        ensure("revision bumped by clearSession", store.revision() > before);
    }

    // Late animesh ownership: the cached key is the control avatar's own id and the
    // live hint becomes the linkset root, so it re-resolves and picks up the stored value.
    template<> template<>
    void client_height_object::test<4>()
    {
        Store store;
        const LLUUID own_id = LLUUID::generateNewID();     // control avatar's own id
        const LLUUID linkset_root = LLUUID::generateNewID();
        store.set(linkset_root, 0.75f);

        Cache cache;
        int calls = 0;
        // root volume not linked yet: getActorFxOwnerId() == own id; nothing stored for it.
        ensure("first refresh resolves (no change: 0)",
               !cache.refresh(store, 0, own_id, false, LLUUID::null, CountingResolver{own_id, &calls}));
        ensure_equals("resolved once", calls, 1);
        ensure_equals("own id key cached", cache.key(), own_id);
        ensure_equals("no value yet", cache.own(), 0.f);
        ensure("stable second refresh does not resolve",
               !cache.refresh(store, 0, own_id, false, LLUUID::null, CountingResolver{own_id, &calls}));
        ensure_equals("still one resolve", calls, 1);

        // the linkset root arrives: the live hint differs from the cached key
        ensure("hint change forces a resolve",
               needsResolve(cache.stamp(), store.revision(), 0, LLUUID::null, cache.key(), linkset_root));
        ensure("refresh reports the new value",
               cache.refresh(store, 0, linkset_root, false, LLUUID::null,
                             CountingResolver{linkset_root, &calls}));
        ensure_equals("second resolve", calls, 2);
        ensure_equals("linkset key cached", cache.key(), linkset_root);
        ensure_equals("picked up the stored value", cache.own(), 0.75f);
    }

    // Ghost runtime replacement with an existing stable-key value: a fresh cache plus
    // the stamped key resolve on the first call; a null cached key always re-resolves;
    // a ghost instance-revision bump re-resolves.
    template<> template<>
    void client_height_object::test<5>()
    {
        Store store;
        const LLUUID stable = LLUUID::generateNewID();     // Ghost Studio Instance::mId
        store.set(stable, 1.25f);

        // replacement runtime: brand new cache, the stamped key is the hint
        Cache fresh;
        int calls = 0;
        ensure("fresh cache + stamped key resolves on the first call",
               fresh.refresh(store, 7, stable, false, LLUUID::null, CountingResolver{stable, &calls}));
        ensure_equals("first-call value", fresh.own(), 1.25f);
        ensure_equals("one resolve", calls, 1);

        // unstamped clone whose instance is not registered yet: null key re-resolves EVERY call
        Cache nokey;
        int ncalls = 0;
        nokey.refresh(store, 7, LLUUID::null, false, LLUUID::null, CountingResolver{LLUUID::null, &ncalls});
        nokey.refresh(store, 7, LLUUID::null, false, LLUUID::null, CountingResolver{LLUUID::null, &ncalls});
        nokey.refresh(store, 7, LLUUID::null, false, LLUUID::null, CountingResolver{LLUUID::null, &ncalls});
        ensure_equals("null cached key is never final", ncalls, 3);

        // the instance registers: the Ghost Studio instance revision bumped
        ensure("instance revision bump re-resolves",
               needsResolve(nokey.stamp(), store.revision(), 8, LLUUID::null, nokey.key(), LLUUID::null));
        ensure("and then picks the value up",
               nokey.refresh(store, 8, LLUUID::null, false, LLUUID::null, CountingResolver{stable, &ncalls}));
        ensure_equals("value resolved after the bump", nokey.own(), 1.25f);
        ensure("quiet afterwards",
               !needsResolve(nokey.stamp(), store.revision(), 8, LLUUID::null, nokey.key(), LLUUID::null));
    }

    // Effective offset: unworn = own; worn = own + wearer; wearer 0 -> own.
    template<> template<>
    void client_height_object::test<6>()
    {
        ensure_equals("unworn ignores the wearer value", effectiveOffset(0.5f, false, 9.f), 0.5f);
        ensure_equals("worn = own + wearer", effectiveOffset(0.5f, true, 1.0f), 1.5f);
        ensure_equals("worn, wearer 0 -> own", effectiveOffset(0.5f, true, 0.f), 0.5f);
        ensure_equals("worn, own 0 -> wearer", effectiveOffset(0.f, true, -0.75f), -0.75f);
    }

    // v4: a throttled worn animesh whose own offset is unchanged and whose wearer offset
    // changed reports exactly ONE forced update, and the effective value is wearer + own
    // in that frame. A detach (wearer key becomes null) also counts as a change.
    template<> template<>
    void client_height_object::test<7>()
    {
        Store store;
        const LLUUID animesh = LLUUID::generateNewID();
        const LLUUID wearer  = LLUUID::generateNewID();
        store.set(animesh, 0.25f);

        Cache cache;
        int calls = 0;
        CountingResolver res{animesh, &calls};
        ensure("initial resolve reports the own value", cache.refresh(store, 0, animesh, true, wearer, res));
        ensure_equals("own", cache.own(), 0.25f);
        ensure_equals("effective = own while the wearer has no offset", cache.effective(), 0.25f);

        // the wearer's offset changes; the animesh's own offset does not
        store.set(wearer, 1.0f);
        ensure("wearer change forces one update", cache.refresh(store, 0, animesh, true, wearer, res));
        ensure_equals("own unchanged", cache.own(), 0.25f);
        ensure_equals("effective = wearer + own in that frame", cache.effective(), 1.25f);
        ensure("exactly once: next refresh is quiet", !cache.refresh(store, 0, animesh, true, wearer, res));
        ensure("and the one after", !cache.refresh(store, 0, animesh, true, wearer, res));
        ensure_equals("effective stays", cache.effective(), 1.25f);

        // the wearer is read from the STORE by key, never from the wearer's own cache:
        // an unrelated store change re-stamps but does not report a change
        store.set(LLUUID::generateNewID(), 0.1f);
        ensure("unrelated store change is not a forced update", !cache.refresh(store, 0, animesh, true, wearer, res));
        ensure_equals("effective unchanged", cache.effective(), 1.25f);

        // detach: wearer key becomes null
        ensure("detach counts as a change", cache.refresh(store, 0, animesh, false, LLUUID::null, res));
        ensure_equals("effective back to own", cache.effective(), 0.25f);
        ensure("detach reported once", !cache.refresh(store, 0, animesh, false, LLUUID::null, res));
    }

    // Pre-early-out refresh: a revision change on a throttled avatar reports "changed"
    // exactly once; an empty store is one branch that reports a change only on the way to 0.
    template<> template<>
    void client_height_object::test<8>()
    {
        Store store;
        const LLUUID resident = LLUUID::generateNewID();
        Cache cache;
        int calls = 0;
        CountingResolver res{resident, &calls};

        ensure("empty store: no change", !cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure_equals("empty store never resolves", calls, 0);

        store.set(resident, 0.5f);
        ensure("revision change reports changed", cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure_equals("value applied", cache.effective(), 0.5f);
        ensure("exactly once", !cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure("and again", !cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure_equals("one resolve in total", calls, 1);

        // the store empties (Reset all / disconnect): one change on the way to 0, then quiet
        store.resetAll();
        ensure("clearing reports changed", cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure_equals("back to 0", cache.effective(), 0.f);
        ensure("then quiet", !cache.refresh(store, 0, resident, false, LLUUID::null, res));

        // and a non-empty store re-resolves after having been empty
        store.set(resident, 0.5f);
        ensure("re-resolves after an empty period", cache.refresh(store, 0, resident, false, LLUUID::null, res));
        ensure_equals("value again", cache.effective(), 0.5f);
    }

    // v4.1: LLControlAvatar::computeNeedsUpdate() overwrites mNeedsImpostorUpdate with the
    // wearer's value; applyForcedUpdate() runs AFTER it, so the flag survives.
    template<> template<>
    void client_height_object::test<9>()
    {
        bool needs_update = false;
        bool impostor = true;
        // computeNeedsUpdate() ran: false, and the control-avatar override cleared the flag
        impostor = false;
        applyForcedUpdate(true, needs_update, impostor);
        ensure("changed forces an update", needs_update);
        ensure("impostor flag survives the control-avatar overwrite", impostor);

        bool nu2 = false;
        bool imp2 = false;
        applyForcedUpdate(false, nu2, imp2);
        ensure("no change leaves needs_update alone", !nu2);
        ensure("no change leaves the impostor flag alone", !imp2);

        bool nu3 = true;
        bool imp3 = false;
        applyForcedUpdate(false, nu3, imp3);
        ensure("no change never clears an update", nu3);
    }

    // A WORLD-vertical lift in a seat-local frame: rotating it by the seat's world
    // rotation gives (0, 0, z) again, for a tilted seat as well as an upright one.
    template<> template<>
    void client_height_object::test<10>()
    {
        const F32 z = 1.5f;
        const LLQuaternion upright;                                   // identity
        const LLVector3 flat = worldLiftInSeatSpace(z, upright);
        ensure("upright seat: straight up", std::fabs(flat.mV[VX]) < 1.0e-5f &&
                                            std::fabs(flat.mV[VY]) < 1.0e-5f &&
                                            std::fabs(flat.mV[VZ] - z) < 1.0e-5f);

        const LLQuaternion tilted(0.9f, LLVector3(1.f, 0.f, 0.f));    // ~51 degrees about X
        const LLVector3 local = worldLiftInSeatSpace(z, tilted);
        ensure("tilted seat: local vector is NOT vertical", std::fabs(local.mV[VY]) > 0.1f);
        ensure("length preserved", std::fabs(local.length() - z) < 1.0e-4f);
        const LLVector3 back = local * tilted;                        // seat-local -> world
        ensure("rotating by the seat gives world (0,0,z)",
               std::fabs(back.mV[VX]) < 1.0e-4f &&
               std::fabs(back.mV[VY]) < 1.0e-4f &&
               std::fabs(back.mV[VZ] - z) < 1.0e-4f);
    }

    // Writer composition: Actor Mover override + ground sit carry EXACTLY ONE client term.
    // The mover writes base + effective; the ground-sit write then adds hover + groundSitClientTerm().
    template<> template<>
    void client_height_object::test<11>()
    {
        const F32 base_z = 10.f, client = 0.75f, hover = 0.2f;

        // mover owns the root: override wrote base + client, ground sit must add hover only
        F32 z = base_z + effectiveOffset(client, false, 0.f);
        z += hover + groundSitClientTerm(true, client);
        ensure("mover + ground sit: exactly one client term", std::fabs(z - (base_z + client + hover)) < 1.0e-6f);

        // worn animesh under the mover: effective (own + wearer) once, nothing more
        const F32 wearer = 0.5f;
        z = base_z + effectiveOffset(client, true, wearer);
        z += hover + groundSitClientTerm(true, client);
        ensure("worn + mover: effective once", std::fabs(z - (base_z + client + wearer + hover)) < 1.0e-6f);

        // no mover: the stock path wrote the root WITHOUT the client term, ground sit adds it once
        z = base_z;
        z += hover + groundSitClientTerm(false, client);
        ensure("no mover: ground sit adds the client term once", std::fabs(z - (base_z + client + hover)) < 1.0e-6f);
    }
}
