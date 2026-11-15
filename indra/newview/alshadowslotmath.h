/** Allocation-free shadow-slot timing; independent of renderer and viewer clocks. */
#ifndef AL_SHADOW_SLOT_MATH_H
#define AL_SHADOW_SLOT_MATH_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
namespace ALShadowSlot
{
// /fp:fast may fold floating-point classification; inspect the IEEE exponent instead.
inline bool finite(double value)
{
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}
// Forget only after both a multi-frame absence and a long wall gap. A single
// stalled frame preserves history; timeStep bounds its ramp/hold progress.
inline bool historyGap(double now, double then, std::uint32_t frame, std::uint32_t previous)
{
    const double elapsed = now - then;
    return !finite(now) || elapsed < 0.0 ||
        (frame - previous > 6u && elapsed > 0.4);
}
inline double timeStep(double elapsed) { return std::clamp(elapsed, 0.0, 1.0 / 15.0); }
struct ContinuousHold
{
    double time = 0.0, elapsed = 0.0;
    std::uint32_t lastFrame = 0;
    bool tracking = false;
    bool reached(bool condition, double now, std::uint32_t frame, double seconds)
    {
        if (!condition || !finite(now)) { *this = {}; return false; }
        // Timer-only restart policy; a long stall never clears visible channel history.
        if (!tracking || (frame != lastFrame && frame != lastFrame + 1u) ||
            now - time > 0.4 || historyGap(now, time, frame, lastFrame))
        { elapsed = 0.0; tracking = true; }
        else if (frame != lastFrame) elapsed += timeStep(now - time);
        time = now; lastFrame = frame;
        return elapsed >= seconds;
    }
};

template<class ID> struct Challenge
{
    ID owner{}, challenger{};
    ContinuousHold hold;
};
// Return true to retain an eligible owner. Different auction tiers retain their existing
// precedence; within one tier, the SAME challenger must exceed 1.5x for 0.2 real seconds.
// Disabling is an exact bypass of retention, including stale timers on re-enable.
template<class ID> inline bool retain(Challenge<ID>& state, bool enabled, bool valid,
    const ID& owner, const ID& challenger, double priority, double challengerPriority,
    int tier, int challengerTier, double now, std::uint32_t frame)
{
    if (!enabled || !valid) { state = {}; return false; }
    if (challenger == ID{} || challenger == owner) { state = {}; return true; }
    if (tier != challengerTier) { state = {}; return tier > challengerTier; }
    if (state.owner != owner || state.challenger != challenger)
    { state = {}; state.owner = owner; state.challenger = challenger; }
    const bool exceeds = finite(priority) && finite(challengerPriority) && challengerPriority > priority * 1.5;
    return !state.hold.reached(exceeds, now, frame, 0.2);
}
template<class ID> struct Light
{
    ID id{}; // Null means ineligible/absent; eligibility is supplied by the existing auction.
    double priority = 0.0;
    int tier = 1;
};
// Keep the stock winners that already own slots. Fill vacancies BEFORE challenging the
// weakest remaining owner, so retention never wastes a spare slot or evicts a stronger
// owner while a weaker one survives. All loops are bounded by the native slot limit.
template<class ID, std::size_t N> inline std::array<ID, N> retainOwners(
    const std::array<Light<ID>, N>& owners, const std::array<Light<ID>, N>& ranked,
    unsigned count, std::array<Challenge<ID>, N>& states, double now, std::uint32_t frame)
{
    count = std::min(count, static_cast<unsigned>(N));
    std::array<ID, N> chosen{};
    std::array<unsigned, N> order{};
    for (unsigned i = 0; i < count; ++i)
    {
        order[i] = i;
        if (owners[i].id == ID{}) { states[i] = {}; continue; }
        for (unsigned j = 0; j < count; ++j)
            if (ranked[j].id == owners[i].id) { chosen[i] = owners[i].id; states[i] = {}; break; }
    }
    const auto reserved = [&](const ID& id)
    {
        for (unsigned i = 0; i < count; ++i)
            if (chosen[i] == id || owners[i].id == id) return true;
        return false;
    };
    const auto next = [&]() -> Light<ID>
    {
        for (unsigned j = 0; j < count; ++j)
            if (ranked[j].id != ID{} && !reserved(ranked[j].id)) return ranked[j];
        return {};
    };
    for (unsigned i = 0; i < count; ++i)
        if (owners[i].id == ID{}) chosen[i] = next().id;
    std::sort(order.begin(), order.begin() + count, [&](unsigned a, unsigned b)
    {
        if (owners[a].tier != owners[b].tier) return owners[a].tier < owners[b].tier;
        if (owners[a].priority != owners[b].priority) return owners[a].priority < owners[b].priority;
        return a < b;
    });
    for (unsigned at = 0; at < count; ++at)
    {
        const unsigned i = order[at];
        if (owners[i].id == ID{} || chosen[i] != ID{}) continue;
        const auto challenger = next();
        chosen[i] = retain(states[i], true, true, owners[i].id, challenger.id,
            owners[i].priority, challenger.priority, owners[i].tier, challenger.tier, now, frame)
            ? owners[i].id : challenger.id;
    }
    for (unsigned i = count; i < N; ++i) states[i] = {};
    return chosen;
}

// [SlotUndrawn] An owner the main lighting pass did not draw this frame (its light volume left
// the view) is not a vacancy on the spot. retainOwners sees it as an owner of one of two tiers:
//   held     (absent < 0.2 s, continuous): above every real tier, so no challenger takes the slot;
//   released (absent >= 0.2 s): below every real tier, so it yields to the first challenger left
//            AFTER the empty slots are filled, and before any drawn owner is challenged.
// With no challenger either tier keeps the slot (retain() returns true for a null challenger),
// so the owner stays the target and its fade stays where it is: a light that returns to the
// view is shadowed on its first frame instead of fading in again.
enum : int { UndrawnHeldTier = 1 << 20, UndrawnReleasedTier = -(1 << 20) };
template<class ID> struct UndrawnHold
{
    ID owner{};
    ContinuousHold hold;
};
template<class ID> inline Light<ID> undrawnOwner(UndrawnHold<ID>& state, const ID& owner,
    double now, std::uint32_t frame)
{
    if (state.owner != owner) { state = {}; state.owner = owner; }
    const bool released = state.hold.reached(true, now, frame, 0.2);
    return {owner, 0.0, released ? UndrawnReleasedTier : UndrawnHeldTier};
}

// First native frame after a hard cut follows the stock auction/fade even when
// its prior auction stamp was invalidated. Stable-spots and OFF remain exact bypasses.
inline bool retainNativeFrame(bool enabled, bool stable, bool& seed)
{
    if (!enabled || stable) return false;
    if (seed) { seed = false; return false; }
    return true;
}
// An exit fits only within stock-retained planes; zero uses the current draw's planes.
inline bool spareChannel(unsigned lights, unsigned kept = 0)
{
    if (!kept) kept = (lights + 3u) / 4u;
    return lights > 0 && lights < 8 && lights + 1u <= kept * 4u;
}
struct RampStatus
{
    bool eligible = false, cubePending = false, cubeReady = false, cubeRendered = true;
    double previousStrength = 1.0;
};
template<class ID> struct ChannelRamps
{
    struct Entry
    {
        ID id{};
        double fade = 0.0;
        bool wanted = true, cube = false, pending = false;
        double pinTime = 0.0;
        std::uint32_t pinFrame = 0;
        double exitStrength = 1.0; // Keep a partial-strength exit's full 0.25 s duration.
    };
    struct Output
    {
        std::array<ID, 8> ids{};
        std::array<double, 8> fade{{1,1,1,1,1,1,1,1}};
        std::array<bool, 8> exiting{};
        unsigned size = 0;
    };
    std::array<Entry, 8> entries{};
    double time = 0.0;
    std::uint32_t frame = 0;
    bool initialized = false;
    bool contains(const ID& id, std::uint32_t currentFrame, double now) const
    {
        if (!initialized || historyGap(now, time, currentFrame, frame)) return false;
        for (const auto& e : entries) if (e.id == id && id != ID{}) return true;
        return false;
    }
    void retainRendered(const std::array<ID,8>& ids, unsigned count)
    {
        count = std::min(count,8u);
        for (auto& e : entries)
        {
            bool drawn = false;
            for (unsigned i = 0; i < count; ++i) if (e.id == ids[i]) { drawn = true; break; }
            if (!drawn) e = {}; // Do not advance a ramp that was never drawn.
        }
    }
    template<class Status> Output update(const std::array<ID, 8>& desired, unsigned count,
        unsigned limit, bool extraAllowed, bool enabled,
        double now, std::uint32_t currentFrame, Status status, unsigned kept = 0)
    {
        Output out;
        count = std::min(count, 8u);
        limit = std::min(limit, count);
        if (!enabled)
        {
            *this = {};
            out.size = std::min(count, limit);
            for (unsigned i = 0; i < out.size; ++i) out.ids[i] = desired[i];
            return out; // Identity fades; no arithmetic on the disabled path.
        }
        if (!finite(now)) { *this = {}; return out; }
        if (initialized && historyGap(now, time, currentFrame, frame)) *this = {};
        const double step = initialized && currentFrame != frame ? timeStep(now - time) * 4.0 : 0.0;
        const bool spare = extraAllowed && spareChannel(limit,kept);
        const unsigned capacity = limit + (spare ? 1u : 0u);
        const auto wanted = [&](const ID& id)
        { for (unsigned i = 0; i < count; ++i) if (desired[i] == id) return true; return false; };
        unsigned held = 0;
        for (auto& e : entries)
        {
            if (e.id == ID{}) continue;
            const auto live = status(e.id);
            if (!live.eligible) { e = {}; continue; } // Never retain deleted/disabled/culled lights.
            // A cube pin never changes channel strength: while pending, use the old
            // screen trace. Entry/exit progress is independent of cube readiness.
            e.fade = std::clamp(e.fade + (e.wanted ? step : -step) * (e.wanted ? 1.0 : e.exitStrength), 0.0, 1.0);
            const bool resident = live.cubePending || live.cubeReady;
            if (resident && !e.cube)
            { e.pending = live.cubePending; e.pinTime = now; e.pinFrame = currentFrame; }
            if (e.pending && (live.cubeReady || !resident || !live.cubeRendered ||
                currentFrame - e.pinFrame >= 6u || now - e.pinTime >= 0.1)) e.pending = false;
            const bool desiredNow = wanted(e.id);
            if (e.wanted && !desiredNow)
            { e.fade = std::clamp(live.previousStrength, 0.0, 1.0); e.exitStrength = e.fade; }
            e.wanted = desiredNow;
            e.cube = resident;
            // Discard floating-point residue at the exact quarter-second endpoint.
            if (!e.wanted && e.fade <= e.exitStrength * 1.e-12) { e = {}; continue; }
            ++held;
        }
        // Budget/cap reductions cannot keep excess residents, even during a ramp.
        bool capacity_cut = false;
        for (auto it = entries.rbegin(); it != entries.rend() && held > capacity; ++it)
            if (it->id != ID{} && !it->wanted) { *it = {}; --held; capacity_cut = true; }
        for (auto it = entries.rbegin(); it != entries.rend() && held > capacity; ++it)
            if (it->id != ID{}) { *it = {}; --held; }
        for (unsigned i = 0; i < count; ++i)
        {
            bool present = false;
            for (const auto& e : entries) if (e.id == desired[i]) { present = true; break; }
            if (present || !status(desired[i]).eligible) continue;
            bool cut = false;
            if (held >= capacity)
            {
                // A ranked-in light must not wait behind exits. Finish the faintest
                // exit first, including at the hard eight-channel maximum.
                Entry* faintest = nullptr;
                for (auto& e : entries)
                    if (e.id != ID{} && !e.wanted && (!faintest || e.fade < faintest->fade)) faintest = &e;
                if (faintest) { *faintest = {}; --held; cut = capacity_cut = true; }
            }
            if (held >= capacity) continue;
            for (auto& e : entries)
                if (e.id == ID{})
                {
                    const auto live = status(desired[i]);
                    bool exit = false;
                    for (const auto& old : entries) exit = exit || (old.id != ID{} && !old.wanted);
                    const bool ramp = spare && exit && !cut;
                    e = {desired[i], ramp ? 0.0 : 1.0, true, live.cubePending || live.cubeReady,
                        live.cubePending && live.cubeRendered, now, currentFrame}; ++held; break;
                }
        }
        bool exit = false;
        for (const auto& e : entries) exit = exit || (e.id != ID{} && !e.wanted);
        // Only capacity/budget cuts settle paired entries. Eligibility loss leaves
        // each incoming light to finish its own ramp, even after its exit disappears.
        if (!exit && capacity_cut) for (auto& e : entries) if (e.wanted) e.fade = 1.0;
        for (const auto& e : entries)
            if (e.id != ID{})
            { out.ids[out.size] = e.id; out.fade[out.size] = e.fade; out.exiting[out.size] = !e.wanted; ++out.size; }
        time = now; frame = currentFrame; initialized = true;
        return out;
    }
};

inline double fadeStep(double oldStep, double elapsed, bool enabled)
{
    // At most four fade units per real second: a full fade takes >= 0.25 s.
    return enabled ? std::min(oldStep, timeStep(elapsed) * 4.0) : oldStep;
}
}
#endif
