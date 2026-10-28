/** Stable persisted CineCam mode migration and display names. */
#include "linden_common.h"
#include "llcinematiccamera.h"

//static
S32 LLCinematicCamera::migrateLegacyMode(S32 mode)
{
    // Step Orbit was persisted as 40 before being folded back into Orbit.
    return mode == 40 ? MODE_ORBIT : mode;
}

//static
const char* LLCinematicCamera::modeName(S32 mode)
{
    static const char* const names[] = {
        "Off", "Bone Lock", "Orbit", "Fly Hover", "Sweep", "Crane",
        "Dolly Zoom", "Push-In", "Low Hero", "Overhead",
        "Over-the-Shoulder", "Crash Zoom", "Slow Zoom", "Whip Arc",
        "Arc Move", "Reveal Rise", "Pull-Back", "Two-Shot",
        "Lead Follow", "ECU Eyes", "Long Lens", "Spiral",
        "Pedestal Rise", "Barrel Roll", "Corkscrew", "Pendulum",
        "Contra-Orbit", "Fisheye Lunge", "Floor Skimmer",
        "Boost Rise", "Boom Over", "Top Spin", "Turntable Crane",
        "Floating Close-Up", "Tilt Whip", "Body Helix Reveal",
        "Descent Pedestal", "Parallax Slide", "Figure-8",
        "Detail Sweep", "Unknown", "Cable Cam Fly-by",
        "Breathing Hold", "Static Wide", "Static Medium",
        "Static Close", "Static Profile Left", "Static Profile Right",
        "Static Low Hero", "Static High Angle", "Static Full Body",
        "Pedro Cam", "Object Lock",
    };
    constexpr S32 count = (S32)(sizeof(names) / sizeof(names[0]));
    static_assert(count == MODE_OBJECT_LOCK + 1,
                  "Every persisted CineCam mode needs one stable label");
    mode = migrateLegacyMode(mode);
    return mode >= 0 && mode < count ? names[mode] : "Unknown";
}

