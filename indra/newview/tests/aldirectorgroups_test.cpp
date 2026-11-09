#include "linden_common.h"
#include "../test/lltut.h"
#include "../aldirectorgroups.h"

namespace tut
{
struct director_groups_data {};
using director_groups_group = test_group<director_groups_data>;
using director_groups_object = director_groups_group::object;
director_groups_group director_groups_tests("Director group values");

template<> template<> void director_groups_object::test<1>()
{
    using namespace ALDirectorGroups;
    ensure_equals("trim", normalizeGroupName("  Guards  "), std::string("Guards"));
    ensure("empty rejected", normalizeGroupName(" \t ").empty());
    ensure("reserved label rejected without case sensitivity", normalizeGroupName(" (NoNe) ").empty());
    ensure_equals("32 characters", normalizeGroupName(std::string(40, 'x')), std::string(32, 'x'));
    std::string unicode;
    for (int i = 0; i < 40; ++i) unicode += "\xc3\xa9";
    ensure_equals("UTF-8 is truncated at a character boundary", normalizeGroupName(unicode).size(), size_t(64));
}

template<> template<> void director_groups_object::test<2>()
{
    using namespace ALDirectorGroups;
    ensure_equals("case-insensitive auto name", uniqueGroupName({ "gRoUp 1", "GROUP 2" }), std::string("Group 3"));
    ensure("explicit names cannot duplicate case-insensitively", uniqueGroupName({ "GUARDS" }, "guards") != "guards");
    const std::string legacy = "  " + std::string(40, 'x') + "  ";
    const std::vector<std::string> names{ "Guards", "guards", legacy };
    ensure_equals("exact existing case wins", resolveGroupName(names, "guards"), std::string("guards"));
    ensure_equals("exact legacy shape wins", resolveGroupName(names, legacy), legacy);
    ensure_equals("new spelling coalesces", resolveGroupName(names, " GUARDS "), std::string("Guards"));
    ensure_equals("new name normalizes", resolveGroupName(names, "  Extras  "), std::string("Extras"));
    ensure_equals("available explicit name", uniqueGroupName({}, "  Guards  "), std::string("Guards"));
}

template<> template<> void director_groups_object::test<3>()
{
    using namespace ALDirectorGroups;
    const LLUUID old_id("00000000-0000-0000-0000-000000000001");
    const LLUUID new_id("00000000-0000-0000-0000-000000000002");
    GroupSnapshot original;
    original.tags[old_id] = "GuArDs";
    original.delays["GuArDs"] = 2.5f;
    original.rates["GuArDs"] = 2;
    original.hotGroups[0] = { "GuArDs", old_id };
    auto replay = original;
    replay.rewriteActor(old_id, new_id);
    replay.rewriteActor(new_id, old_id);
    ensure("snapshot identity round trip preserves exact names and values", replay == original);
    replay.rewriteActor(old_id, LLUUID::null);
    ensure("retired actor is dropped", replay.tags.empty());
}
} // namespace tut
