#include "linden_common.h"
#include "../test/lltut.h"
#include "../alriggroupsubject.h"

namespace tut
{
struct rig_group_subject_data {};
typedef test_group<rig_group_subject_data> rig_group_subject_group;
typedef rig_group_subject_group::object rig_group_subject_object;
rig_group_subject_group rig_group_subject_tests("alriggroupsubject");

template<> template<> void rig_group_subject_object::test<1>()
{
    const LLUUID self("00000000-0000-0000-0000-000000000001");
    const LLUUID actor("00000000-0000-0000-0000-000000000002");
    const LLUUID other("00000000-0000-0000-0000-000000000003");
    std::array<LLUUID, 4> subjects{}, anchors{};
    std::array<bool, 4> enabled{};
    std::array<U64, 4> used{4, 2, 1, 3};
    auto choose = [&]() { return alRigBorrowChoice(subjects, anchors, enabled, used, self, actor); };
    ensure_equals("first free off empty rig", choose().mSlot, 0);
    enabled[0] = true;
    ensure_equals("on empty rig is busy", choose().mSlot, 1);
    subjects[1] = other;
    ensure_equals("off Subject rig is busy", choose().mSlot, 2);
    anchors[2] = other;
    ensure_equals("off borrowed rig is busy", choose().mSlot, 3);
    anchors[1] = actor;
    ensure_equals("own anchor beats free rig", choose().mSlot, 1);
    subjects[0] = actor;
    ensure_equals("own anchor beats Subject", choose().mSlot, 1);
    anchors[1] = other;
    ensure_equals("existing unanchored Subject reused", choose().mSlot, 0);
    subjects.fill(other); anchors.fill(other); enabled.fill(true);
    ensure_equals("full requires menu", choose().mSlot, -1);
    ensure_equals("LRU replacement", choose().mReplacement, 2);
    used.fill(0);
    ensure_equals("LRU ties choose A", choose().mReplacement, 0);
    ensure_equals("self excluded", alRigBorrowChoice(subjects, anchors, enabled, used, self, self).mSlot, -1);
    ensure_equals("null excluded", alRigBorrowChoice(subjects, anchors, enabled, used, self, LLUUID::null).mSlot, -1);
    for (S32 slot = 0; slot < 4; ++slot)
    {
        subjects.fill(other); anchors.fill(other); enabled.fill(true);
        subjects[slot].setNull(); anchors[slot].setNull(); enabled[slot] = false;
        ensure_equals("each free rig usable", choose().mSlot, slot);
        ensure("choice never writes Subject", subjects[slot].isNull());
        ensure("selection itself never anchors", anchors[slot].isNull());
    }
}

template<> template<> void rig_group_subject_object::test<2>()
{
    const LLUUID self("00000000-0000-0000-0000-000000000001");
    const LLUUID actor("00000000-0000-0000-0000-000000000002");
    const LLUUID other("00000000-0000-0000-0000-000000000003");
    std::array<LLUUID, 4> subjects{}, anchors{};
    std::array<bool, 4> enabled{};
    std::array<U64, 4> used{4, 2, 1, 3};
    auto choose = [&]() { return alRigBorrowChoice(subjects, anchors, enabled, used, self, actor); };
    // Every rig is busy with another actor, but all are switched off.
    subjects.fill(other); anchors.fill(other); enabled.fill(false);
    ensure_equals("off anchored rig reused, LRU among off rigs", choose().mSlot, 2);
    enabled[2] = true;
    ensure_equals("ON rig never chosen while an off one exists", choose().mSlot, 1);
    enabled[1] = true;
    ensure_equals("next LRU off rig", choose().mSlot, 3);
    anchors[3].setNull();
    ensure_equals("off Subject-only rig reused", choose().mSlot, 3);
    enabled[3] = true;
    ensure_equals("one off rig left", choose().mSlot, 0);
    enabled[0] = true;
    ensure_equals("all four ON asks", choose().mSlot, -1);
    ensure_equals("all ON offers LRU replacement", choose().mReplacement, 2);
    // Own anchor and own Subject still win before any reuse.
    enabled.fill(false);
    anchors[3] = actor;
    ensure_equals("own anchor wins over LRU off rig", choose().mSlot, 3);
    anchors[3] = other; anchors[0].setNull(); subjects[0] = actor;
    ensure_equals("own Subject wins over LRU off rig", choose().mSlot, 0);
    enabled.fill(true);
    ensure_equals("own Subject wins even when all ON", choose().mSlot, 0);
    std::array<LLUUID, 4> before = subjects;
    subjects.fill(other); anchors.fill(other); enabled.fill(false);
    before = subjects;
    choose();
    ensure("reuse never writes Subjects", before == subjects);
}
}
