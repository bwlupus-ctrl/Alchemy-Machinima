#ifndef AL_CLEAN_TELEMETRY_H
#define AL_CLEAN_TELEMETRY_H
#include "alcleanframe.h"
#include "alkeyframeparticles.h"
#include <array>
#include <algorithm>
#include <cstdint>
// Fixed storage. Calls are per enclosing guard or draw batch, never per face.
class ALCleanTelemetry
{
public:
    enum Site { UI, UI3D, HUD, HUDText, Highlights, Debug, Attachments, Composition,
                Viewfinder, Cursor, Ring, EffectSources, EffectGeometry, Count };
    struct Counter { std::uint64_t evaluated=0, blocked=0, forbidden=0; };
    inline static std::array<Counter, Count> sites{};
    inline static std::array<double, 4096> intervals{};
    inline static std::uint64_t frames=0, acquires=0, releases=0, conflicts=0;
    inline static double maximum=0;
    static void begin() { sites={}; intervals={}; frames=0; acquires=1; releases=conflicts=0; maximum=0; }
    static void guard(Site s, bool work) { if(ALCleanFrame::active()) { ++sites[s].evaluated; sites[s].blocked += work; } }
    static void submit(Site s) { if(ALCleanFrame::active()) ++sites[s].forbidden; }
    static void frame(double dt) { intervals[frames++ % intervals.size()]=dt; maximum=std::max(maximum,dt); }
    static const char* verdict()
    {
        for(const auto& c:sites) if(c.forbidden) return "FAIL";
        if(!ALCleanParticleBatches::complete()) return "INCONCLUSIVE";
        // UI3D encloses guides/axes; HUD encloses HUDText; UI encloses UI2D/nameplates.
        for(int i=0;i<Count;++i)
            if(!sites[i].evaluated && !(i==HUDText && sites[HUD].evaluated)) return "INCONCLUSIVE";
        return frames>=30 && acquires==1 && releases==1 ? "PASS" : "INCONCLUSIVE";
    }
};
#endif
