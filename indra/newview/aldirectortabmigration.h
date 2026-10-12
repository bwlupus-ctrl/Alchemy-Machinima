#ifndef AL_DIRECTOR_TAB_MIGRATION_H
#define AL_DIRECTOR_TAB_MIGRATION_H

#include <string>
#include "stdtypes.h"

namespace ALDirectorTabMigration
{
std::string migrateTabName(const std::string& name);
std::string restoreTabName(const std::string& persisted_name, S32 legacy_index);
}

#endif
