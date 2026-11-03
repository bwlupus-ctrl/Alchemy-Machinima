// [ActorKey] Pure ABI 1.3 guard decisions; no viewer, SDK or GL dependency.
#pragma once
#include "../../indra/newview/llreshadebridgeabi.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace SLKey {
constexpr const char* guarantee = "Key colour is exact except where ReShade's own UI draws, and an add-on effect switched on mid-key may tint ONE frame before keying stops.";
enum class State { Off, NoReShade, Arming, Keying, Refused };
enum class Reason { Off, NoReShade, Checking, Settling, NoReader, OldReader, StaleSource,
                    Activity, Foreign, Host, CheckFailed, Heartbeat };
inline bool readerCapable(uint32_t abi, const SLReShadeReaderInfo& reader) {
    return SLReShade_ABI_Major(abi)==1 && SLReShade_ABI_Minor(abi)>=3 &&
        SLReShade_ABI_Major(reader.abi_version)==1 && SLReShade_ABI_Minor(reader.abi_version)>=3 &&
        (reader.caps&3)==3 && reader.struct_size>=sizeof(reader);
}
inline bool sourceMatches(const SLReShadeReaderInfo& reader, const char* expected) {
    if (!expected || strlen(expected)!=64) return false;
    constexpr const char* hex="0123456789abcdef";
    for (unsigned i=0;i<32;++i)
        if(expected[2*i]!=hex[reader.source_sha256[i]>>4] || expected[2*i+1]!=hex[reader.source_sha256[i]&15]) return false;
    return true;
}
inline uint64_t nextModuleState(uint64_t old, uint32_t tick) {
    return (uint64_t(uint32_t(old>>32)+1)<<32)|tick;
}
struct Guard {
    State state = State::Off;
    Reason reason = Reason::Off;
    uint64_t epoch = 0, lastPublished = 0, stateAtPublish = 0;
    uint32_t armPresents = 0, heartbeatMiss = 0;
    bool havePublished = false, arm = false;
    uint64_t consumedReload = UINT64_MAX;
    void reset() { *this = Guard{}; }
    void enterArm(uint64_t frame) {
        state = State::Arming; reason = Reason::Checking; epoch = frame; armPresents = 0;
        havePublished = false;
    }
    bool refuse(Reason why) { state = State::Refused; reason = why; arm = false; return false; }
    // gate: -1 scan failed, 0 no ReShade, 1 ReShade, 2 unsettled.
    bool evaluate(uint64_t frame, int gate, bool reader, bool capable, bool hash,
                  uint64_t modState, const SLReShadeKeyGuard& report) {
        arm = false;
        if (gate < 0) return refuse(Reason::CheckFailed);
        if (gate == 2) { if (state == State::Refused) return false; if (state != State::Arming) enterArm(frame); reason = Reason::Settling; return false; }
        if (gate == 0) { state = State::NoReShade; reason = Reason::NoReShade; return true; }
        if (!reader) return refuse(Reason::NoReader);
        if (!capable) return refuse(Reason::OldReader);
        if (!hash) return refuse(Reason::StaleSource);
        if (state == State::Refused) {
            if (!(report.gui_flags & SLRESHADE_GUI_RELOADED) || report.viewer_frame < epoch || report.viewer_frame == consumedReload)
                return false;
            consumedReload = report.viewer_frame;
            enterArm(frame);
        }
        if (state == State::Off || state == State::NoReShade) enterArm(frame);
        const bool fresh = havePublished && report.viewer_frame == lastPublished && report.viewer_frame >= epoch;
        if (fresh && report.verdict) {
            if (report.verdict == SLRESHADE_GUARD_CONTAMINATED) return refuse(Reason::Activity);
            if (report.verdict == SLRESHADE_GUARD_FOREIGN_ADDON)
                return refuse((report.gui_flags & SLRESHADE_GUI_UNVERIFIED_HOST) ? Reason::Host :
                    (report.gui_flags & SLRESHADE_GUI_CHECK_FAILED) ? Reason::CheckFailed : Reason::Foreign);
            if (report.verdict == SLRESHADE_GUARD_SKIPPED) return refuse(Reason::Heartbeat);
            if (report.verdict == SLRESHADE_GUARD_CLEAN && modState == stateAtPublish) {
                state = State::Keying; reason = Reason::Checking; return true;
            }
            if (report.verdict == SLRESHADE_GUARD_UNSETTLED || modState != stateAtPublish) {
                if (state != State::Arming) enterArm(frame);
                armPresents = 0; reason = Reason::Settling; arm = true; return false;
            }
            return refuse(Reason::Heartbeat); // unknown verdict never grants permission
        }
        if (state == State::Keying) { ++heartbeatMiss; enterArm(frame); }
        if (armPresents >= 60) return refuse(Reason::Heartbeat);
        arm = true;
        return false;
    }
    void publish(uint64_t frame, uint64_t modState, bool keyed) {
        if (!keyed && !arm) return;
        lastPublished = frame; stateAtPublish = modState; havePublished = true;
        if (!keyed) ++armPresents;
    }
};
inline uint32_t satAdd(uint32_t a, uint32_t b) { return b > UINT32_MAX-a ? UINT32_MAX : a+b; }
// Shared settle/cache policy; scan() returns a complete, comparable result.
// The cached state is always the word captured BEFORE either scan.
template<class Result> struct SettledScan {
    bool valid = false;
    uint64_t settledState = 0;
    uint32_t scannedAt = 0, scans = 0;
    Result cached{};
    template<class Read, class Scan>
    int run(bool notify, uint32_t now, Read read, Scan scan, Result& out) {
        const uint64_t before = read();
        if (notify && valid && before == settledState && uint32_t(now-scannedAt) < 500) {
            cached.summary(out); return out.verdict;
        }
        valid = false;
        if (notify && uint32_t(now-uint32_t(before)) < 200) return 2;
        ++scans; Result a = scan(); if (a.verdict < 0) { out = a; return -1; }
        ++scans; Result b = scan(); if (b.verdict < 0) { out = b; return -1; }
        if (!(a == b) || before != read()) return 2;
        b.summary(out); cached = std::move(b); settledState = before; scannedAt = now; valid = notify;
        return b.verdict;
    }
};
} // namespace SLKey
