"""Identity transactions reconcile every recipient cache before redirect pruning."""
from rig_shadow_presets_check import function


def extend_fixture(root, source):
    tests = r'''
    // Rekey BEFORE opening Share: no callback leases old and no rig tick repairs
    // either snapshot. The transaction may safely prune the now-unused redirect.
    for(bool storedSnapshot:{false,true}){
        m=ALCineLightRigManager();c.members.clear();c.groups.clear();c.canonical.clear();c.live.clear();
        c.members[{"destination-old"}];c.members[{"newcomer"}];
        auto& destination=m.at(Slot::A);destination.anchor={"destination-old"};destination.enabled=true;
        destination.members={{"destination-old"}};
        if(storedSnapshot)destination.stored=destination.members;
        c.members.erase({"destination-old"});c.members[{"destination-new"}];
        m.rewriteActorReferences({"destination-old"},{"destination-new"});
        assert(destination.members==uuid_vec_t({{"destination-new"}}));
        assert(destination.stored==(storedSnapshot?uuid_vec_t({{"destination-new"}}):uuid_vec_t()));
        assert(!m.mBorrowRekeys.count({"destination-old"}));
        auto action=p.share({"newcomer"},true,Slot::A,{"destination-new"});
        action();
        const auto recipients=c.membersInGroup(destination.group);
        assert(recipients.size()==2);
        assert(std::count(recipients.begin(),recipients.end(),LLUUID{"destination-new"})==1);
        assert(std::count(recipients.begin(),recipients.end(),LLUUID{"newcomer"})==1);
        assert(!c.members.count({"destination-old"}));
    }
    // Every rig, including SELF and rigs whose anchor is unrelated, owns both
    // caches. Rewrite/drop only the retiring identity, preserving other members.
    m=ALCineLightRigManager();c.members.clear();c.groups.clear();
    c.members[{"old"}];c.members[{"keeper"}];
    for(auto& rig:m.mInstances){rig.anchor={"keeper"};rig.members={{"old"},{"keeper"}};rig.stored=rig.members;}
    c.members.erase({"old"});c.members[{"new"}];
    m.rewriteActorReferences({"old"},{"new"});
    for(auto& rig:m.mInstances){
        assert(rig.anchor==LLUUID{"keeper"});
        assert(rig.members==uuid_vec_t({{"new"},{"keeper"}}) && rig.stored==rig.members);
    }
    c.members.erase({"new"});m.rewriteActorReferences({"new"},LLUUID::null);
    for(auto& rig:m.mInstances){
        assert(rig.anchor==LLUUID{"keeper"});
        assert(rig.members==uuid_vec_t({{"keeper"}}) && rig.stored==rig.members);
    }
    // Belt and braces: an independently stale snapshot must retain a redirect
    // (or tombstone) with no Cast, anchor, enrolled-anchor or menu-lease reference.
    for(int slot=0;slot<5;++slot)for(bool storedSnapshot:{false,true})for(bool retired:{false,true}){
        m=ALCineLightRigManager();c.members.clear();
        auto& rig=m.mInstances[slot];
        auto& snapshot=storedSnapshot?rig.stored:rig.members;snapshot={{"source"}};
        m.mBorrowRekeys[{"source"}]=retired?LLUUID::null:LLUUID{"destination"};
        m.mBorrowRekeys[{"unused"}]={"destination"};
        m.pruneRemovedAnchors();
        assert(m.mBorrowRekeys.count({"source"})==1 && !m.mBorrowRekeys.count({"unused"}));
        assert(m.resolveRigActorIdentity({"source"})==(retired?LLUUID::null:LLUUID{"destination"}));
        snapshot.clear();m.pruneRemovedAnchors();assert(m.mBorrowRekeys.empty());
    }
    std::cout<<"PASS: round 11 pre-menu Share, all-rig snapshot rekey/retirement and snapshot-aware redirect pruning\n";
'''
    marker = '    // Retire from within a click:'
    assert source.count(marker) == 1
    return source.replace(marker, tests + '\n' + marker)


MUTATIONS = {
    'r11_snapshot_rewrite': (
        '            rewrite_members(rig.members);\n            rewrite_members(rig.stored);',
        '            (void)rig;'),
    'r11_prune_snapshot_references': (
        '            if (std::find(members.begin(), members.end(), id) != members.end() ||\n'
        '                std::find(stored.begin(), stored.end(), id) != stored.end()) return true;',
        '            (void)members; (void)stored;'),
}


def mutate(source, name):
    signature = ('void ALCineLightRigManager::rewriteActorReferences('
                 if name == 'r11_snapshot_rewrite'
                 else 'void ALCineLightRigManager::pruneRemovedAnchors(')
    body = function(source, signature)
    old, new = MUTATIONS[name]
    assert body.count(old) == 1
    return source.replace(body, body.replace(old, new), 1)
