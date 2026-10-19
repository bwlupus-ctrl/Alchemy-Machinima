/** @file llprismgate.cpp @brief Simple Gate program rules and scene migration. */
#include "linden_common.h"
#include "llprismgate.h"
#include "llsdutil.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace LLPrismLens
{
namespace
{
const GateSourceInput* find(const GateInput& in, const LLUUID& id)
{
    for (const auto& s : in.sources) if (s.id == id) return &s;
    return nullptr;
}
bool picture(const GateSourceInput* s)
{ return s && s->hasOutput && s->fullQuality; }
bool fresh(const GateState& state, const GateSourceInput* s)
{
    if (!picture(s)) return false;
    if (s->frozen) return true;
    if (s->kind == ESourceSlotKind::MEDIA_FACE && s->paused && s->currentImage && s->copiedDecoder == s->decoder) return true;
    if (s->publication <= state.baselinePublication) return false;
    return s->kind == ESourceSlotKind::CAMERA ||
        (s->currentImage && (s->copiedDecoder != state.baselineDecoder ||
         s->identity != state.baselineIdentity));
}
void baseline(GateState& state, const GateSourceInput& s, F64 now)
{
    state.baselinePublication = s.publication;
    state.baselineDecoder = s.decoder;
    state.baselineIdentity = s.identity;
    state.awaitingCopy = false; // A new take supersedes the earlier unpublished wait.
    state.pendingStart = now;
    state.demandedSeconds = 0.;
}
void add(std::vector<LLUUID>& ids, const LLUUID& id)
{
    if (id.notNull() && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
}
bool numeric(const LLSD& v) { return v.isReal() || v.isInteger(); }
}

GateProposal gateStep(const GateState& state, const GateInput& in)
{
    GateProposal p;
    auto& s = p.next = state;
    if (in.demanded) s.unseenSince = -1.;
    else if (s.unseenSince < 0.) s.unseenSince = in.now;
    const bool seen = in.demanded || in.now - s.unseenSince < .25;
    const F64 dt = s.lastTick < 0. ? 0. : std::max(0., in.now - s.lastTick);
    s.lastTick = in.now;
    if (seen && (s.pending.notNull() || s.awaitingCopy || s.fadeEndWaiting)) s.demandedSeconds += dt;
    p.demandedSeconds = s.demandedSeconds;
    const auto fail = [&](const GateSourceInput* bad)
    {
        s.held = state.held; // The retained picture survives untick/deletion.
        s.copyKey = state.copyKey;
        const auto* held = find(in, s.held);
        s.program = held && held->eligible ? s.held : LLUUID::null;
        s.pending.setNull(); s.from.setNull(); s.awaitingCopy = s.fadeEndWaiting = false; s.alpha = 0.f;
        s.nextAuto = in.now + in.settings.mIntervalSeconds;
        s.demandedSeconds = 0.;
        s.text = bad ? bad->name + (bad->downloading ? " not ready: downloading" : " has no picture: " + bad->reason) : "Source removed";
        p.rollback = p.publish = true;
    };
    if ((s.program.notNull() && !find(in, s.program)) ||
        (s.pending.notNull() && !find(in, s.pending))) fail(nullptr);
    if (in.sources.empty())
    {
        s.program = s.pending = s.from = s.held = s.queued = LLUUID::null;
        s.awaitingCopy = s.fadeEndWaiting = false;
        s.alpha = 0.f; s.copyKey = {}; s.demandedSeconds = 0.;
        s.text = "No sources ticked";
        p.publish = p.rollback = !state.emptyPublished;
        s.emptyPublished = true;
        return p;
    }
    s.emptyPublished = false;
    if (s.program.notNull() && !find(in, s.program)->eligible) fail(find(in, s.program));
    if (s.program.notNull() && s.cursor.isNull())
    {
        s.cursor = s.program;
        s.nextAuto = in.now + in.settings.mIntervalSeconds;
    }
    const auto* taken = find(in, in.take);
    const bool restart = taken && taken->id == s.program &&
        taken->kind == ESourceSlotKind::MEDIA_URL && taken->stopped;
    if (in.take.notNull() && in.settings.mMode == EGateMode::MANUAL &&
        (in.take != s.program || restart) && taken) s.queued = in.take;
    const bool fading = s.from.notNull();
    if (fading)
    {
        const auto* incoming = find(in, s.program);
        s.alpha = !find(in, s.from) ? 1.f :
            F32(std::clamp((in.now - s.fadeStart) / s.fadeSeconds, 0., 1.));
        if (!picture(incoming) || incoming->stopped) fail(incoming);
        else if (s.alpha >= 1.f)
        {
            if (!s.fadeEndWaiting) { s.fadeEndWaiting = true; p.demandedSeconds = 0.; s.demandedSeconds = 0.; }
            if (in.demanded)
            {
                p.copy = s.program; p.needsPixels = p.publish = true;
                s.held = s.program; s.from.setNull(); s.alpha = 0.f; s.fadeEndWaiting = false;
                s.demandedSeconds = 0.; s.nextAuto = in.now + in.settings.mIntervalSeconds;
            }
        }
        else p.publish = in.demanded;
    }
    if (!fading && s.pending.isNull() && (!s.awaitingCopy || !seen))
    {
        const GateSourceInput* next = nullptr;
        if (s.queued.notNull()) { next = find(in, s.queued); s.queued.setNull(); }
        else if (s.program.isNull() || (in.settings.mMode == EGateMode::AUTO_CYCLE && in.now >= s.nextAuto))
        {
            auto cursor = std::find_if(in.sources.begin(), in.sources.end(), [&](const auto& c) { return c.id == s.cursor; });
            size_t start = cursor == in.sources.end() ? 0 : (size_t(cursor - in.sources.begin()) + 1) % in.sources.size();
            for (size_t n = 0; n < in.sources.size(); ++n)
            {
                const auto& c = in.sources[(start + n) % in.sources.size()];
                if (c.eligible) { next = &c; break; }
                s.text = c.name + ": press Play once";
            }
        }
        if (next && next->eligible && (next->id != s.program || restart))
        {
            s.pending = s.cursor = next->id;
            baseline(s, *next, in.now); p.demandedSeconds = 0.;
        }
    }
    else if (!fading && s.queued.notNull())
    {
        if (const auto* next = find(in, s.queued); next && next->eligible)
        { s.pending = s.cursor = next->id; baseline(s, *next, in.now); p.demandedSeconds = 0.; }
        s.queued.setNull();
    }
    if (s.pending.notNull())
    {
        const auto* incoming = find(in, s.pending);
        if (incoming && incoming->playRefused) fail(incoming);
        else if (incoming && incoming->eligible)
        {
            if (incoming->kind == ESourceSlotKind::MEDIA_URL && !incoming->desiredPlaying &&
                (state.pending != s.pending || in.take == s.pending)) add(p.play, incoming->id);
            if (!seen || (in.demanded && fresh(s, incoming)))
            {
                const bool fade = seen && in.demanded && in.hasGatePicture &&
                    s.held == s.program && s.pending != s.program && find(in, s.held) &&
                    in.settings.mTransition == EGateTransition::CROSSFADE;
                s.from = fade ? s.held : LLUUID::null;
                s.program = s.pending; s.pending.setNull();
                s.text = "On air: " + incoming->name; ++s.cutSerial;
                s.nextAuto = in.now + in.settings.mIntervalSeconds;
                if (fade)
                {
                    s.fadeStart = in.now; s.fadeSeconds = in.settings.mCrossfadeSeconds;
                    s.alpha = 0.f; s.demandedSeconds = 0.;
                    p.needsPixels = p.publish = true;
                }
                else if (!seen) s.awaitingCopy = true;
                else
                {
                    p.copy = s.program; p.needsPixels = p.publish = true;
                    s.held = s.program; s.demandedSeconds = 0.;
                }
            }
        }
    }
    if (s.awaitingCopy && in.demanded && fresh(s, find(in, s.program)))
    {
        p.copy = s.program; p.needsPixels = p.publish = true;
        s.held = s.program; s.awaitingCopy = false; s.demandedSeconds = 0.;
        if (const auto* program = find(in, s.program)) s.text = "On air: " + program->name;
    }
    if (seen && p.demandedSeconds >= 5. &&
        (state.pending.notNull() || state.awaitingCopy || state.fadeEndWaiting))
    {
        fail(find(in, state.pending.notNull() ? state.pending : state.program));
        p.copy.setNull(); p.needsPixels = false;
    }
    if (s.pending.isNull() && s.from.isNull() && !s.awaitingCopy)
        if (const auto* program = find(in, s.program); program && program->stopped)
            s.text = program->name + " stopped: " + program->reason;
    add(p.feed, s.program); add(p.feed, s.pending); add(p.feed, s.from);
    if (p.needsPixels) { add(p.feed, state.program); add(p.feed, state.pending); add(p.feed, state.from); }
    if (in.settings.mOffAir == EGateOffAir::PAUSE)
        for (const auto& source : in.sources)
            if (source.kind == ESourceSlotKind::MEDIA_URL && source.desiredPlaying && !source.downloading &&
                std::find(p.feed.begin(), p.feed.end(), source.id) == p.feed.end()) add(p.pause, source.id);
    return p;
}
void gateApply(GateState& state, const GateProposal& p, bool ok)
{
    if (!p.needsPixels || ok) { state = p.next; return; }
    // A rejected publication advances only the deadline clock and pending intent.
    state.lastTick = p.next.lastTick; state.unseenSince = p.next.unseenSince;
    state.demandedSeconds = p.demandedSeconds;
    state.fadeEndWaiting = state.from.notNull() && p.copy.notNull();
    if (state.fadeEndWaiting) state.alpha = 1.f;
    state.queued = p.next.queued; // A copy rejection must not consume a newer click.
    // Pruning is independent of pixel acceptance. Keeping a removed program
    // here would prune twice in the compose pass and skip the pending candidate.
    if (p.rollback)
    {
        state.program.setNull(); state.from.setNull(); state.alpha = 0.f;
        state.fadeEndWaiting = false;
    }
    if (state.from.isNull() && p.next.program != state.program)
    {
        state.pending = p.next.program; state.cursor = p.next.cursor;
        state.awaitingCopy = false;
        state.pendingStart = p.next.pendingStart;
        state.baselinePublication = p.next.baselinePublication;
        state.baselineDecoder = p.next.baselineDecoder;
        state.baselineIdentity = p.next.baselineIdentity;
    }
}
GateProposal gatePrepareStep(GateState& state, const GateInput& input)
{
    const bool empty_published = state.emptyPublished;
    const auto proposal = gateStep(state, input);
    gateApply(state, proposal, !proposal.needsPixels);
    if (proposal.publish) state.emptyPublished = empty_published;
    return proposal;
}
F32 gateGain(const GateState& s, const LLUUID& id)
{
    if (id == s.program) return s.from.notNull() ? s.alpha : 1.f;
    return id == s.from ? 1.f - s.alpha : 0.f;
}
std::map<LLUUID, F32> gateAudioGains(const std::vector<std::pair<LLUUID, F32>>& videos,
                                    const std::set<LLUUID>& previous)
{
    std::map<LLUUID, F32> gains;
    for (const auto& [id, gain] : videos)
    {
        if (id.isNull()) continue;
        const auto [it, inserted] = gains.emplace(id, gain);
        if (!inserted) it->second = llmax(it->second, gain);
    }
    for (const auto& id : previous) if (!gains.count(id)) gains.emplace(id, 1.f);
    return gains;
}
F64 gatePlayerDemand(const GateState& s, const LLUUID& id, F64 own)
{
    return id.notNull() && (id == s.program || id == s.pending || id == s.from)
        ? llmax(own, 2. * 1024. * 1024.) : own;
}
GateStatus gateStatus(const GateState& s)
{ return {s.program, s.pending, s.from, s.alpha, s.cutSerial, s.text}; }

LLSD gateSourceToLLSD(const GateSource& s)
{
    LLSD d = LLSD::emptyMap();
    d["source_kind"] = "gate";
    d["gate_sources"] = LLSD::emptyArray();
    for (const auto& id : s.mSources) d["gate_sources"].append(id);
    d["gate_mode"] = s.mMode == EGateMode::MANUAL ? "manual" : "auto";
    d["gate_interval"] = s.mIntervalSeconds;
    d["gate_transition"] = s.mTransition == EGateTransition::CUT ? "cut" : "crossfade";
    d["gate_crossfade_seconds"] = s.mCrossfadeSeconds;
    d["gate_off_air"] = s.mOffAir == EGateOffAir::KEEP_PLAYING ? "keep_playing" : "pause";
    d["gate_program_id"] = s.mProgramId;
    return d;
}
bool gateSourceFromLLSD(const LLSD& d, GateSource& result, std::string* reason)
{
    GateSource s;
    const auto fail = [&]() { if (reason) *reason = "Invalid Gate settings"; return false; };
    if (!d.isMap()) return fail();
    if (d.has("gate_mode"))
    {
        if (!d["gate_mode"].isString()) return fail();
        auto v = d["gate_mode"].asString();
        if (v == "auto") s.mMode = EGateMode::AUTO_CYCLE;
        else if (v != "manual") return fail();
    }
    if (d.has("gate_transition"))
    {
        if (!d["gate_transition"].isString()) return fail();
        auto v = d["gate_transition"].asString();
        if (v == "crossfade") s.mTransition = EGateTransition::CROSSFADE;
        else if (v != "cut") return fail();
    }
    if (d.has("gate_off_air"))
    {
        if (!d["gate_off_air"].isString()) return fail();
        auto v = d["gate_off_air"].asString();
        if (v == "pause") s.mOffAir = EGateOffAir::PAUSE;
        else if (v != "keep_playing") return fail();
    }
    if (d.has("gate_interval"))
    {
        if (!numeric(d["gate_interval"])) return fail();
        s.mIntervalSeconds = d["gate_interval"].asReal();
    }
    if (d.has("gate_crossfade_seconds"))
    {
        if (!numeric(d["gate_crossfade_seconds"])) return fail();
        s.mCrossfadeSeconds = d["gate_crossfade_seconds"].asReal();
    }
    if (!std::isfinite(s.mIntervalSeconds) || s.mIntervalSeconds < .5 || s.mIntervalSeconds > 120. ||
        !std::isfinite(s.mCrossfadeSeconds) || s.mCrossfadeSeconds < .1 || s.mCrossfadeSeconds > 3.) return fail();
    if (d.has("gate_sources"))
    {
        if (!d["gate_sources"].isArray()) return fail();
        for (const auto& id : llsd::inArray(d["gate_sources"]))
            if (id.isUUID() && id.asUUID().notNull()) s.mSources.push_back(id.asUUID());
    }
    s.mProgramId = d["gate_program_id"].asUUID();
    result = s;
    if (reason) reason->clear();
    return true;
}
void gateResolveSources(GateSource& s, const LLUUID& self, const std::vector<LLUUID>& eligible,
                        std::vector<std::string>& notes)
{
    const auto requested = s.mSources;
    s.mSources.clear();
    std::set<LLUUID> unique;
    for (const auto& id : requested)
        if (id == self || std::find(eligible.begin(), eligible.end(), id) == eligible.end() ||
            !unique.insert(id).second || unique.size() > 7) notes.push_back("Dropped invalid Gate source " + id.asString());
    for (const auto& id : eligible)
        if (id != self && unique.count(id) && s.mSources.size() < 7) s.mSources.push_back(id);
    if (std::find(s.mSources.begin(), s.mSources.end(), s.mProgramId) == s.mSources.end()) s.mProgramId.setNull();
}
bool prismFullTargetScissor(bool virtual_display, bool main_view, U32 w, U32 h, S32* scissor)
{
    if (!virtual_display && main_view) return false;
    scissor[0] = scissor[1] = 0; scissor[2] = S32(w); scissor[3] = S32(h);
    return true;
}
void gateMigrateScene(LLSD& data, std::vector<SkippedBinding>& notes)
{
    if (!data.isMap()) return;
    auto& captures = data["prism_captures"];
    std::set<LLUUID> routable;
    bool has_gate = false, legacy = false;
    for (const auto& c : llsd::inArray(captures))
    {
        has_gate |= c["source_kind"].asString() == "gate";
        const auto kind = c["source_kind"].asString();
        if (kind == "media_url" || kind == "media_face" || c["mode"].asString() == "camera_feed")
            routable.insert(c["capture_id"].asUUID());
    }
    for (const char* key : {"prism_displays", "prism_projectors"})
        if (data.has(key) && data[key].isArray())
            for (const auto& b : llsd::inArray(data[key])) legacy |= b["gate_source"].asBoolean();
    LLUUID gate_id;
    if (!has_gate && legacy && captures.size() < MAX_CAPTURES)
    {
        gate_id.generate();
        GateSource source;
        const auto& old = data["prism_gates"][0];
        const bool valid_record = old.isMap() && old["armed"].isArray() &&
            old["gate_id"].asUUID().notNull() && old["active"].isBoolean() &&
            (old["mode"].asString() == "manual" || old["mode"].asString() == "auto_cycle") &&
            numeric(old["interval_seconds"]) && std::isfinite(old["interval_seconds"].asReal()) &&
            old["interval_seconds"].asReal() >= .5 && old["interval_seconds"].asReal() <= 120. &&
            old["prewarm_frames"].isInteger() && old["prewarm_frames"].asInteger() >= 0 && old["prewarm_frames"].asInteger() <= 5 &&
            old["program_arm_index"].isInteger() &&
            (!old.has("transition") || (old["transition"].isString() &&
                (old["transition"].asString() == "cut" || old["transition"].asString() == "crossfade"))) &&
            (!old.has("crossfade_seconds") || (numeric(old["crossfade_seconds"]) &&
                std::isfinite(old["crossfade_seconds"].asReal()) && old["crossfade_seconds"].asReal() >= .1 && old["crossfade_seconds"].asReal() <= 3.));
        if (valid_record)
        {
            for (const auto& arm : llsd::inArray(old["armed"]))
                if (arm["enabled"].isBoolean() && arm["enabled"].asBoolean() &&
                    routable.count(arm["capture_id"].asUUID())) add(source.mSources, arm["capture_id"].asUUID());
            if (old["mode"].asString() == "auto_cycle") source.mMode = EGateMode::AUTO_CYCLE;
            if (numeric(old["interval_seconds"]) && std::isfinite(old["interval_seconds"].asReal()))
                source.mIntervalSeconds = std::clamp(old["interval_seconds"].asReal(), .5, 120.);
            if (old["transition"].asString() == "crossfade") source.mTransition = EGateTransition::CROSSFADE;
            if (numeric(old["crossfade_seconds"]) && std::isfinite(old["crossfade_seconds"].asReal()))
                source.mCrossfadeSeconds = std::clamp(old["crossfade_seconds"].asReal(), .1, 3.);
            if (old["program_arm_index"].isInteger())
            {
                const S32 index = old["program_arm_index"].asInteger();
                if (index >= 0 && index < old["armed"].size())
                    source.mProgramId = old["armed"][index]["capture_id"].asUUID();
            }
        }
        std::vector<std::string> ignored;
        std::vector<LLUUID> ordered;
        for (const auto& c : llsd::inArray(captures))
            if (routable.count(c["capture_id"].asUUID())) ordered.push_back(c["capture_id"].asUUID());
        gateResolveSources(source, gate_id, ordered, ignored);
        auto item = gateSourceToLLSD(source);
        item["capture_id"] = gate_id; item["label"] = "Gate";
        captures.append(item);
    }
    for (const char* key : {"prism_displays", "prism_projectors"})
    {
        if (!data.has(key) || !data[key].isArray()) continue;
        LLSD bindings = LLSD::emptyArray();
        for (auto item : llsd::inArray(data[key]))
        {
            const bool subscribed = item["gate_source"].asBoolean();
            if (subscribed && !has_gate)
            {
                LLUUID target = gate_id;
                if (target.isNull() && std::string(key) == "prism_projectors" &&
                    routable.count(item["home_capture_id"].asUUID())) target = item["home_capture_id"].asUUID();
                if (target.isNull() && routable.count(item["capture_id"].asUUID())) target = item["capture_id"].asUUID();
                if (target.isNull())
                {
                    SkippedBinding note;
                    note.mHandle.mId = item["binding_id"].asUUID();
                    note.mObjectId = item[std::string(key) == "prism_displays" ? "display_id" : "object_id"].asUUID();
                    note.mTextureEntry = item["display_te"].asInteger();
                    note.mReason = "Legacy Gate binding has no available source";
                    notes.push_back(note);
                    continue;
                }
                item["capture_id"] = target;
            }
            item.erase("gate_source"); item.erase("home_capture_id");
            bindings.append(item);
        }
        data[key] = bindings;
    }
    for (S32 i = 0; i < captures.size(); ++i) captures[i].erase("source_on_cut");
    data.erase("prism_gates");
}

}
