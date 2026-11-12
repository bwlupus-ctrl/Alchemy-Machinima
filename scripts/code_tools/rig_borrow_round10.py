"""Share reconciles last-tick recipient identities at the actual menu callback."""
from rig_shadow_presets_check import function


def extend_fixture(root, source):
    tests = r'''
    // The destination clone rekeys after its last tick and after opening Share.
    // Exercise both snapshot sources; no tick refresh may repair the stale IDs.
    for(bool storedSnapshot:{false,true})for(bool extraRecipients:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.groups.clear();c.canonical.clear();c.live.clear();
        c.members[{"destination-old"}];c.members[{"newcomer"}];
        auto& destination=m.at(Slot::A);destination.anchor={"destination-old"};destination.enabled=true;
        destination.members={{"destination-old"}};
        if(storedSnapshot)destination.stored=destination.members;
        auto action=p.share({"newcomer"},true,Slot::A,{"destination-old"});
        c.members.erase({"destination-old"});c.members[{"destination-new"}];
        m.rewriteActorReferences({"destination-old"},{"destination-new"});
        assert(destination.anchor==LLUUID{"destination-new"} && destination.enabled);
        auto& snapshot=storedSnapshot?destination.stored:destination.members;
        assert(snapshot==uuid_vec_t({{"destination-new"}}));
        assert(LLDirectorCast::canonicalActorId({"destination-old"})==LLUUID{"destination-old"});
        if(extraRecipients){
            // Duplicate aliases collapse. Cast and non-Cast away identities stay;
            // a final tombstone and a deleting identity must never be enrolled.
            snapshot.push_back({"destination-new"});snapshot.push_back({"destination-old"});
            c.members[{"away-cast"}];snapshot.push_back({"away-cast"});snapshot.push_back({"away-noncast"});
            c.members[{"retired"}];auto retiredLease=m.retainBorrowActor({"retired"});
            m.rewriteActorReferences({"retired"},LLUUID::null);c.members.erase({"retired"});snapshot.push_back({"retired"});
            snapshot.push_back({"deleting"});ALGhostStudio::instance().deleting.insert({"deleting"});
        }
        action();
        auto recipients=c.membersInGroup(destination.group);
        assert(!destination.group.empty() && recipients.size()==(extraRecipients?4:2));
        assert(std::count(recipients.begin(),recipients.end(),LLUUID{"destination-new"})==1);
        assert(std::count(recipients.begin(),recipients.end(),LLUUID{"newcomer"})==1);
        assert(!c.members.count({"destination-old"}) && !c.members.count({"retired"}) && !c.members.count({"deleting"}));
        if(extraRecipients){
            assert(std::count(recipients.begin(),recipients.end(),LLUUID{"away-cast"})==1);
            assert(std::count(recipients.begin(),recipients.end(),LLUUID{"away-noncast"})==1);
        }
        ALGhostStudio::instance().deleting.clear();
    }
    std::cout<<"PASS: round 10 actual Share callback retains rekeyed destination, drops retired IDs, deduplicates and preserves away recipients\n";
'''
    marker='    // Retire from within a click:'
    assert source.count(marker)==1
    return source.replace(marker, tests+'\n'+marker)


MUTATIONS = {
    'r10_share_redirect': ('const LLUUID actor = manager.resolveRigActorIdentity(member);',
                           'const LLUUID actor = LLDirectorCast::canonicalActorId(member);'),
}


def mutate(source, name):
    body=function(source, 'void ALPanelCineLightRig::shareRig(')
    old,new=MUTATIONS[name]
    assert body.count(old)==1
    return source.replace(body, body.replace(old,new), 1)
