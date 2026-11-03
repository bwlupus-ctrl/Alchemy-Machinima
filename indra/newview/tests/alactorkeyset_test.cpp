/** [ActorKey] WP0: production membership rules, no viewer or GL dependencies. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alactorkeyset.h"

namespace tut
{
struct actor_key_data
{
    using Model = ALActorKeySet<int>;
    // IDs: self 1, resident 2, entity clone 3, animesh 4 (root owner 40, wearer 1).
    std::vector<Model::Avatar> live{{1,1,0,false}, {2,2,0,false}, {3,3,0,true}, {4,40,1,false}};
    std::vector<Model::Instance> clones{{30,1,3}, {31,2,0}};
};
typedef test_group<actor_key_data> actor_key_group;
typedef actor_key_group::object actor_key_object;
actor_key_group actor_key_tests("Actor chroma key keyed sets");

template<> template<> void actor_key_object::test<1>()
{
    Model m; m.all = true; m.selected = {1,2,3}; m.rebuild(live, clones);
    ensure("all residents and entity", m.avatars == std::set<int>({1,2,3}));
    ensure("overlay and entity instances", m.instances == std::set<int>({30,31}));
    ensure("animesh opt-out", !m.owners.count(40));
}
template<> template<> void actor_key_object::test<2>()
{
    for (bool all : {false, true}) for (bool deferred : {false, true})
    {
        Model m; m.all = all; m.includeClones = false; m.selected = {1,3};
        m.rebuild(live, clones);
        ensure("U-DOMAIN source is keyed", m.nativeKeyed(1,0));
        ensure("U-DOMAIN native entity excluded", !m.nativeKeyed(3,0));
        ensure("U-DOMAIN no proxy instances", m.instances.empty());
        ensure("U-DOMAIN source fallback cannot select proxy", !m.instanceKeyed(clones[0]));
        (void)deferred; // submission setting never enters the membership law
    }
}
template<> template<> void actor_key_object::test<3>()
{
    Model m; m.selected = {1}; m.rebuild(live, clones);
    ensure("Me includes source clone", m.avatars == std::set<int>({1,3}));
    ensure("Me excludes other-source overlay", m.instances == std::set<int>({30}));
    m.selected = {31}; m.rebuild(live, clones);
    ensure("cast overlay selected directly", m.instances == std::set<int>({31}));
    ensure("direct overlay does not select source", m.avatars.empty());
}
template<> template<> void actor_key_object::test<4>()
{
    Model m; m.selected = {3}; m.rebuild(live, clones);
    ensure("cast runtime clone resolves instance", m.instances == std::set<int>({30}));
    ensure("runtime clone only", m.avatars == std::set<int>({3}));
    m.selected = {30}; m.rebuild(live, clones);
    ensure("instance selection also keys native entity", m.avatars == std::set<int>({3}));
}
template<> template<> void actor_key_object::test<5>()
{
    Model m; m.selected = {4}; m.rebuild(live, clones);
    ensure("animesh root", m.nativeKeyed(40,0));
    ensure("worn animesh fallback wearer", m.nativeKeyed(999,1));
    ensure("no null owner", !m.nativeKeyed(0,0));
    ensure("root is not avatar ID", !m.nativeKeyed(4,0));
}
template<> template<> void actor_key_object::test<6>()
{
    Model m; m.selected = {1}; m.rebuild(live, clones);
    auto before = m.avatars; m.rebuild(live, clones);
    ensure("stable sets", before == m.avatars);
    m.selected.clear(); m.rebuild({}, {});
    ensure("removed/dead entries cleared", m.avatars.empty() && m.owners.empty() && m.instances.empty());
    m.selected = {0}; m.rebuild(live, clones);
    ensure("null absent entity never selects an overlay", m.instances.empty());
}
template<> template<> void actor_key_object::test<7>()
{
    Model m; auto orphan = clones; orphan[0].source = 0;
    m.all = true; m.selected = {1,2,3}; m.rebuild(live, orphan);
    ensure("All keeps source-removed entity", m.avatars.count(3) && m.instances.count(30));
    m.all = false; m.selected = {30}; m.rebuild(live, orphan);
    ensure("Cast keeps source-removed clone", m.avatars == std::set<int>({3}));
    m.selected = {1}; m.rebuild(live, orphan);
    ensure("Me no longer inherits removed source", !m.avatars.count(3));
}

}
