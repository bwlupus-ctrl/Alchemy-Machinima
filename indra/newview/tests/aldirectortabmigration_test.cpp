#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectortabmigration.h"

namespace tut
{
struct director_tab_migration_data {};
using director_tab_migration_group = test_group<director_tab_migration_data>;
using director_tab_migration_object = director_tab_migration_group::object;
director_tab_migration_group director_tab_migration_tests("ALDirectorTabMigration");

template<> template<>
void director_tab_migration_object::test<1>()
{
    ensure_equals("old camera leaf migrates", ALDirectorTabMigration::migrateTabName("camera_tab"), "shot_tab");
    ensure_equals("old VCam group migrates", ALDirectorTabMigration::migrateTabName("lens_group/vcam_tab"), "vcam_tab");
    ensure_equals("new VCam group migrates", ALDirectorTabMigration::migrateTabName("camera_group/vcam_tab"), "vcam_tab");
    ensure_equals("canonical leaf is stable", ALDirectorTabMigration::migrateTabName("vcam_tab"), "vcam_tab");
    ensure_equals("old projector volumetrics leaf follows its rows",
                  ALDirectorTabMigration::migrateTabName("projector_volumetrics_tab"), "projector_shafts_tab");
    ensure_equals("old atmospheric volumes leaf is Atmosphere",
                  ALDirectorTabMigration::migrateTabName("atmospheric_volumes_tab"), "atmosphere_tab");
    ensure_equals("new shafts leaf is stable", ALDirectorTabMigration::migrateTabName("projector_shafts_tab"), "projector_shafts_tab");
    ensure_equals("atmosphere leaf is stable", ALDirectorTabMigration::migrateTabName("atmosphere_tab"), "atmosphere_tab");
}

template<> template<>
void director_tab_migration_object::test<2>()
{
    ensure_equals("DirectorLastTabName wins", ALDirectorTabMigration::restoreTabName("camera_group/vcam_tab", 0), "vcam_tab");
    ensure_equals("legacy index is fallback", ALDirectorTabMigration::restoreTabName("", 5), "shot_tab");
    ensure_equals("legacy fallback is clamped", ALDirectorTabMigration::restoreTabName("", 99), "actor_style_tab");
    ensure_equals("legacy index 7 (projector volumetrics) restores the shafts page",
                  ALDirectorTabMigration::restoreTabName("", 7), "projector_shafts_tab");
    ensure_equals("legacy index 10 (atmospheric volumes) restores Atmosphere",
                  ALDirectorTabMigration::restoreTabName("", 10), "atmosphere_tab");
    ensure_equals("persisted old projector leaf name restores the shafts page",
                  ALDirectorTabMigration::restoreTabName("projector_volumetrics_tab", 0), "projector_shafts_tab");
}
}
