#include "linden_common.h"
#include "../test/lltut.h"
#include "../alghostanimnames.h"
#include "llanimationstates.h"

namespace tut
{
struct ghost_anim_names_data
{
    ALGhostAnimNames names;
    ALGhostAnimNames::Library library;
    static const char* builtin(const LLUUID& id) { return gAnimLibrary.animStateToString(id); }
};
typedef test_group<ghost_anim_names_data> ghost_anim_names_group;
typedef ghost_anim_names_group::object ghost_anim_names_object;
ghost_anim_names_group ghost_anim_names_tests("alghostanimnames");

template<> template<> void ghost_anim_names_object::test<1>()
{
    ensure_equals("friendly built-in stand", names.resolve(ANIM_AGENT_STAND, library, 0, builtin), "stand");
    ensure_equals("friendly built-in dance", names.resolve(ANIM_AGENT_DANCE1, library, 0, builtin), "dance1");
    library.emplace_back("My stand", ANIM_AGENT_STAND);
    library.emplace_back("Second inventory match", ANIM_AGENT_STAND);
    ensure_equals("inventory wins; first match stable", names.resolve(ANIM_AGENT_STAND, library, 1, builtin), "My stand");
    library[0].first = "Renamed stand";
    ensure_equals("revision retains cached library", names.resolve(ANIM_AGENT_STAND, library, 1, builtin), "My stand");
    ensure_equals("library revision invalidates cache", names.resolve(ANIM_AGENT_STAND, library, 2, builtin), "Renamed stand");
    library.clear();
    ensure_equals("removed inventory falls back", names.resolve(ANIM_AGENT_STAND, library, 3, builtin), "stand");
}

template<> template<> void ghost_anim_names_object::test<2>()
{
    const LLUUID unknown("01234567-89ab-cdef-0123-456789abcdef");
    ensure_equals("unknown fallback", names.resolve(unknown, library, 0, builtin), "Unknown animation");
    ensure_equals("late session name", names.resolve(unknown, library, 0, builtin, "Learned wave"), "Learned wave");
    ensure_equals("session cache", names.resolve(unknown, library, 0, builtin), "Learned wave");
    ensure_equals("built-in beats session", names.resolve(ANIM_AGENT_STAND, library, 0, builtin, "Learned stand"), "stand");
    library.emplace_back("Owned wave", unknown);
    ensure_equals("inventory beats learned", names.resolve(unknown, library, 1, builtin, "Learned wave"), "Owned wave");
}
template<> template<> void ghost_anim_names_object::test<3>()
{
    library.emplace_back("", ANIM_AGENT_STAND);
    library.emplace_back(" \t\r\n", ANIM_AGENT_DANCE1);
    ensure_equals("empty inventory falls through", names.resolve(ANIM_AGENT_STAND, library, 1, builtin), "stand");
    ensure_equals("whitespace inventory falls through", names.resolve(ANIM_AGENT_DANCE1, library, 1, builtin), "dance1");
    library.emplace_back("Owned stand", ANIM_AGENT_STAND);
    ensure_equals("next usable inventory match", names.resolve(ANIM_AGENT_STAND, library, 2, builtin), "Owned stand");
    const LLUUID unknown("01234567-89ab-cdef-0123-456789abcdef");
    ensure_equals("empty unknown", names.resolve(unknown, library, 2, builtin, ""), "Unknown animation");
    ensure_equals("metadata arrives", names.resolve(unknown, library, 2, builtin, "Wave", 1), "Wave");
    ensure_equals("metadata revision refreshes name", names.resolve(unknown, library, 2, builtin, "New wave", 2), "New wave");
}

template<> template<> void ghost_anim_names_object::test<4>()
{
    library.emplace_back("Original inventory", ANIM_AGENT_STAND);
    ensure_equals("prime inventory", names.resolve(ANIM_AGENT_STAND, library, 1, builtin, "", 1), "Original inventory");
    library[0].first = "Not published yet";
    ensure_equals("metadata never rebuilds inventory index", names.resolve(ANIM_AGENT_STAND, library, 1, builtin, "", 2), "Original inventory");
    ensure_equals("library revision publishes inventory", names.resolve(ANIM_AGENT_STAND, library, 2, builtin, "", 2), "Not published yet");
}

template<> template<> void ghost_anim_names_object::test<5>()
{
    static S32 lookups = 0;
    lookups = 0;
    auto counting_builtin = +[](const LLUUID& id) -> const char*
    {
        ++lookups;
        return gAnimLibrary.animStateToString(id);
    };
    ensure_equals("prime built-in", names.resolve(ANIM_AGENT_STAND, library, 1, counting_builtin, "", 1), "stand");
    ensure_equals("metadata retains built-in cache", names.resolve(ANIM_AGENT_STAND, library, 1, counting_builtin, "", 2), "stand");
    ensure_equals("no repeat built-in lookup", lookups, S32(1));
}

}
