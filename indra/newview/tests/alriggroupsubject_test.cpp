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
    const LLUUID friend_id("00000000-0000-0000-0000-000000000002");
    const LLUUID other("00000000-0000-0000-0000-000000000003");
    std::array<LLUUID, 4> subjects{};
    ensure_equals("empty chooses A", alRigGroupSubject(subjects, self, friend_id), 0);
    subjects[0] = other;
    ensure_equals("occupied A chooses B", alRigGroupSubject(subjects, self, friend_id), 1);
    subjects[2] = friend_id;
    ensure_equals("existing C beats empty B", alRigGroupSubject(subjects, self, friend_id), 2);
    subjects = { other, self, other, other };
    ensure_equals("full disables new avatar", alRigGroupSubject(subjects, self, friend_id), -1);
    ensure_equals("full retains existing slot", alRigGroupSubject(subjects, self, other), 0);
    ensure_equals("self excluded", alRigGroupSubject(subjects, self, self), -1);
    ensure_equals("null excluded", alRigGroupSubject(subjects, self, LLUUID::null), -1);
    for (S32 slot = 0; slot < 4; ++slot)
    {
        subjects.fill(other);
        subjects[slot].setNull();
        ensure_equals("each free slot is usable", alRigGroupSubject(subjects, self, friend_id), slot);
        ensure("choice never fills or overwrites subjects", subjects[slot].isNull());
    }
}
}
