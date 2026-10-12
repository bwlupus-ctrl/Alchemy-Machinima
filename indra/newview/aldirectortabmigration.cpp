#include "linden_common.h"

#include "aldirectortabmigration.h"

#include "llmath.h"

namespace
{
constexpr const char* OLD_FLAT_TABS[] = {
    "move_tab", "path_tab", "ghosts_tab", "props_tab", "animate_tab", "camera_tab",
    "takes_tab", "projector_volumetrics_tab", "cine_light_rig_tab", "weather_tab",
    "atmospheric_volumes_tab", "temporal_tab", "actor_style_tab"
};
}

namespace ALDirectorTabMigration
{
std::string migrateTabName(const std::string& name)
{
    if (name == "camera_tab") return "shot_tab";
    if (name == "lens_group/vcam_tab" || name == "camera_group/vcam_tab") return "vcam_tab";
    if (name == "projector_volumetrics_tab" || name == "atmospheric_volumes_tab") return "atmosphere_tab";
    return name;
}

std::string restoreTabName(const std::string& persisted_name, S32 legacy_index)
{
    if (!persisted_name.empty()) return migrateTabName(persisted_name);
    return migrateTabName(OLD_FLAT_TABS[llclamp(legacy_index, 0, 12)]);
}
}
