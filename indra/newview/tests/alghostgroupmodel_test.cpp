/**
 * @file alghostgroupmodel_test.cpp
 * @brief Tests for the pure Ghost Studio crowd-group authoring model.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alghostgroupmodel.h"

#include <algorithm>
#include <limits>

namespace tut
{
struct alghostgroupmodel_data
{
    static LLUUID id(const char* value)
    {
        return LLUUID(value);
    }

    static ALGhostGroupModel::Transform transform(
        F64 x, F64 y, F64 z, F32 yaw = 0.f, F32 scale = 1.f)
    {
        ALGhostGroupModel::Transform value;
        value.mFoot.set(x, y, z);
        value.mRotation = LLQuaternion(yaw, LLVector3::z_axis);
        value.mScale = scale;
        return value;
    }
};

typedef test_group<alghostgroupmodel_data> alghostgroupmodel_group;
typedef alghostgroupmodel_group::object alghostgroupmodel_object;
alghostgroupmodel_group gALGhostGroupModel("alghostgroupmodel");

template<> template<>
void alghostgroupmodel_object::test<1>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("group created", model.createGroup(group, "Extras",
        ALGhostGroupModel::MODE_RIGID,
        {{a, transform(-1.0, 0.0, 2.0)}, {b, transform(1.0, 0.0, 2.0)}}));

    const ALGhostGroupModel::Group* authored = model.findGroup(group);
    ensure("group exists", authored != nullptr);
    ensure_approximately_equals("stable centroid x",
        authored->mWorld.mFoot.mdV[VX], 0.0, 1.0e-8);
    ensure_approximately_equals("stable centroid z",
        authored->mWorld.mFoot.mdV[VZ], 2.0, 1.0e-8);

    ALGhostGroupModel::Transform resolved;
    ensure("member resolves", model.resolveMember(a, resolved));
    ensure_approximately_equals("member x preserved",
        resolved.mFoot.mdV[VX], -1.0, 1.0e-5);
}

template<> template<>
void alghostgroupmodel_object::test<2>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    model.createGroup(group, "Extras", ALGhostGroupModel::MODE_RIGID,
        {{a, transform(-1.0, 0.0, 0.0)}, {b, transform(1.0, 0.0, 0.0)}});

    ALGhostGroupModel::Transform desired =
        transform(-2.0, 4.0, 0.0, F_PI_BY_TWO, 2.f);
    ensure("group transform from member",
        model.transformFromMember(a, desired));

    ALGhostGroupModel::Transform got_a;
    ALGhostGroupModel::Transform got_b;
    ensure("a resolves", model.resolveMember(a, got_a));
    ensure("b resolves", model.resolveMember(b, got_b));
    ensure_approximately_equals("selected member lands exactly x",
        got_a.mFoot.mdV[VX], desired.mFoot.mdV[VX], 1.0e-5);
    ensure_approximately_equals("selected member lands exactly y",
        got_a.mFoot.mdV[VY], desired.mFoot.mdV[VY], 1.0e-5);
    ensure_approximately_equals("rigid scale reaches requested",
        got_a.mScale, desired.mScale, 1.0e-5);
    ensure_approximately_equals("member separation scales",
        (got_b.mFoot - got_a.mFoot).length(), 4.0, 1.0e-4);
}

template<> template<>
void alghostgroupmodel_object::test<3>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    model.createGroup(group, "Extras", ALGhostGroupModel::MODE_MOVE_FACE,
        {{a, transform(-1.0, 0.0, 0.0)}, {b, transform(1.0, 0.0, 0.0)}});

    ensure("move+face edit accepted",
        model.transformFromMember(a, transform(-1.0, 0.0, 0.0, 0.f, 2.f)));
    ALGhostGroupModel::Transform got_a;
    ALGhostGroupModel::Transform got_b;
    model.resolveMember(a, got_a);
    model.resolveMember(b, got_b);
    ensure_approximately_equals("selected scale changes", got_a.mScale, 2.f, 1.0e-5);
    ensure_approximately_equals("other scale stays", got_b.mScale, 1.f, 1.0e-5);
    ensure_approximately_equals("formation extent stays",
        (got_b.mFoot - got_a.mFoot).length(), 2.0, 1.0e-4);
}

template<> template<>
void alghostgroupmodel_object::test<4>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    ensure("reject nonfinite member", !model.createGroup(
        group, "Bad", ALGhostGroupModel::MODE_RIGID,
        {{a, transform(std::numeric_limits<F64>::quiet_NaN(), 0.0, 0.0)}}));
    ensure("bad group not retained", model.findGroup(group) == nullptr);
}

template<> template<>
void alghostgroupmodel_object::test<5>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("group created", model.createGroup(
        group, "Editable", ALGhostGroupModel::MODE_RIGID,
        {{a, transform(-1.0, 0.0, 0.0)},
         {b, transform(1.0, 0.0, 0.0)}}));
    ensure("member editing enabled", model.setEditMembers(group, true));

    const ALGhostGroupModel::Transform custom =
        transform(-2.5, 3.0, 0.4, 0.35f, 1.4f);
    ensure("custom member-local offset accepted",
        model.setMemberWorld(a, custom, true));
    ALGhostGroupModel::Transform resolved;
    ensure("custom member resolves", model.resolveMember(a, resolved));
    ensure_approximately_equals("custom x exact",
        resolved.mFoot.mdV[VX], custom.mFoot.mdV[VX], 1.0e-5);
    ensure_approximately_equals("custom y exact",
        resolved.mFoot.mdV[VY], custom.mFoot.mdV[VY], 1.0e-5);
    ensure_approximately_equals("custom scale exact",
        resolved.mScale, custom.mScale, 1.0e-5);

    ensure("custom offset resets", model.resetMemberOffset(a));
    ensure("authored member resolves after reset",
        model.resolveMember(a, resolved));
    ensure_approximately_equals("authored x restored",
        resolved.mFoot.mdV[VX], -1.0, 1.0e-5);
    ensure_approximately_equals("authored y restored",
        resolved.mFoot.mdV[VY], 0.0, 1.0e-5);
    const ALGhostGroupModel::Group* saved = model.findGroup(group);
    ensure("reset clears the pin", saved && !saved->mMembers.front().mPinned);
}

template<> template<>
void alghostgroupmodel_object::test<6>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("large-offset group created", model.createGroup(
        group, "Large offsets", ALGhostGroupModel::MODE_RIGID,
        {{a, transform(0.125, 0.0, 0.0)},
         {b, transform(2000000000.625, 0.0, 0.0)}}));

    const ALGhostGroupModel::Group* initial = model.findGroup(group);
    ensure("large-offset group exists", initial != nullptr);
    const LLVector3d local = initial->mMembers.front().mLocal.mFoot;
    ensure_approximately_equals("large local fraction survives grouping",
        local.mdV[VX], -1000000000.25, 1.0e-9);
    ALGhostGroupModel::Transform initial_world;
    ensure("initial large-offset member resolves",
        model.resolveMember(a, initial_world));
    ensure_approximately_equals("initial authored foot is preserved",
        initial_world.mFoot.mdV[VX], 0.125, 1.0e-9);

    ALGhostGroupModel::Transform moved =
        transform(500000000000.0, -250000000000.0, 8.0, 0.37f, 1.f);
    ensure("large-offset group transform accepted",
        model.setGroupTransform(group, moved));
    const ALGhostGroupModel::Transform applied =
        model.findGroup(group)->mWorld;
    const LLVector3d expected =
        applied.mFoot + local * applied.mRotation;

    ALGhostGroupModel::Transform resolved;
    ensure("large-offset member resolves", model.resolveMember(a, resolved));
    ensure("F64 quaternion rotation preserves the authored offset",
        (resolved.mFoot - expected).length() <= 0.001);
}

template<> template<>
void alghostgroupmodel_object::test<7>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("transaction group created", model.createGroup(
        group, "Transactions", ALGhostGroupModel::MODE_MOVE_FACE,
        {{a, transform(-1.0, 0.0, 0.0)},
         {b, transform(1.0, 0.0, 0.0)}}));
    ensure("member edit enabled", model.setEditMembers(group, true));

    ALGhostGroupModel::Transform tiny =
        transform(0.0, 0.0, 0.0, 0.f,
                  std::numeric_limits<F32>::min());
    ensure("tiny finite group scale accepted",
        model.setGroupTransform(group, tiny));
    const ALGhostGroupModel::Group before_member =
        *model.findGroup(group);

    ALGhostGroupModel::Transform impossible_member =
        transform(1.0e300, 0.0, 0.0, 0.f, 1.f);
    ensure("overflowing world-to-local edit rejected",
        !model.setMemberWorld(a, impossible_member, true));
    const ALGhostGroupModel::Group* after_member =
        model.findGroup(group);
    ensure("failed member edit does not bump revision",
        after_member->mRevision == before_member.mRevision);
    ensure("failed member edit preserves local foot",
        after_member->mMembers[0].mLocal.mFoot ==
        before_member.mMembers[0].mLocal.mFoot);
    ensure("failed member edit preserves pin",
        after_member->mMembers[0].mPinned ==
        before_member.mMembers[0].mPinned);

    const ALGhostGroupModel::Group before_transform = *after_member;
    ALGhostGroupModel::Transform impossible_scale =
        transform(0.0, 0.0, 0.0, 0.f,
                  std::numeric_limits<F32>::max());
    ensure("overflowing move+face local scale rejected",
        !model.transformFromMember(a, impossible_scale));
    const ALGhostGroupModel::Group* after_transform =
        model.findGroup(group);
    ensure("failed group edit does not bump revision",
        after_transform->mRevision == before_transform.mRevision);
    ensure("failed group edit preserves member scale",
        after_transform->mMembers[0].mLocal.mScale ==
        before_transform.mMembers[0].mLocal.mScale);
    ensure("failed group edit preserves group transform",
        after_transform->mWorld.mFoot == before_transform.mWorld.mFoot &&
        after_transform->mWorld.mScale == before_transform.mWorld.mScale);
}

template<> template<>
void alghostgroupmodel_object::test<8>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("snapshot group created", model.createGroup(
        group, "Rollback", ALGhostGroupModel::MODE_RIGID,
        {{a, transform(-1.0, 0.0, 2.0)},
         {b, transform(1.0, 0.0, 2.0)}}));
    const ALGhostGroupModel::Group snapshot = *model.findGroup(group);

    ensure("group changed before rollback",
        model.setGroupTransform(group,
            transform(10.0, 20.0, 30.0, 0.4f, 2.f)));
    ensure("valid snapshot restored",
        model.restoreGroupSnapshot(snapshot));
    const ALGhostGroupModel::Group* restored = model.findGroup(group);
    ensure("snapshot revision restored exactly",
        restored->mRevision == snapshot.mRevision);
    ensure("snapshot transform restored exactly",
        restored->mWorld.mFoot == snapshot.mWorld.mFoot &&
        restored->mWorld.mScale == snapshot.mWorld.mScale);

    ALGhostGroupModel::Group invalid = snapshot;
    invalid.mMembers[0].mLocal.mFoot.mdV[VX] =
        std::numeric_limits<F64>::infinity();
    ensure("nonfinite snapshot rejected",
        !model.restoreGroupSnapshot(invalid));
    ensure("invalid snapshot leaves revision unchanged",
        model.findGroup(group)->mRevision == snapshot.mRevision);

    invalid = snapshot;
    std::swap(invalid.mMembers[0], invalid.mMembers[1]);
    ensure("reordered membership rejected",
        !model.restoreGroupSnapshot(invalid));
    ensure("reordered snapshot leaves membership unchanged",
        model.findGroup(group)->mMembers[0].mInstanceId == a &&
        model.findGroup(group)->mMembers[1].mInstanceId == b);

    invalid = snapshot;
    invalid.mMode = static_cast<ALGhostGroupModel::EMode>(99);
    ensure("invalid snapshot mode rejected",
        !model.restoreGroupSnapshot(invalid));
    ensure("invalid mode leaves group unchanged",
        model.findGroup(group)->mMode == snapshot.mMode);
}

template<> template<>
void alghostgroupmodel_object::test<9>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("extreme finite centroid remains representable",
        model.createGroup(group, "Extreme", ALGhostGroupModel::MODE_RIGID,
            {{a, transform(std::numeric_limits<F64>::max(), 0.0, 0.0)},
             {b, transform(std::numeric_limits<F64>::max(), 0.0, 0.0)}}));
    ALGhostGroupModel::Transform resolved;
    ensure("extreme member still resolves", model.resolveMember(a, resolved));
    ensure("extreme resolved foot remains finite", resolved.mFoot.isFinite());
}

template<> template<>
void alghostgroupmodel_object::test<10>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("wide group created",
        model.createGroup(group, "Wide", ALGhostGroupModel::MODE_RIGID,
            {{a, transform(-1.0e308, 0.0, 0.0)},
             {b, transform(1.0e308, 0.0, 0.0)}}));
    const ALGhostGroupModel::Group before = *model.findGroup(group);

    ALGhostGroupModel::Transform overflow = before.mWorld;
    overflow.mScale = 2.f;
    ensure("group transform preflights every member",
        !model.setGroupTransform(group, overflow));
    ensure("failed hierarchy transform preserves revision",
        model.findGroup(group)->mRevision == before.mRevision);

    ALGhostGroupModel::Transform bad_rotation = before.mWorld;
    for (S32 i = 0; i < LENGTHOFQUAT; ++i)
    {
        bad_rotation.mRotation.mQ[i] =
            std::numeric_limits<F32>::max();
    }
    ensure("overflowing quaternion magnitude rejected",
        !model.setGroupTransform(group, bad_rotation));
    ensure("bad quaternion cannot mutate the group",
        model.findGroup(group)->mRevision == before.mRevision);
}

template<> template<>
void alghostgroupmodel_object::test<11>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID clone = id("00000000-0000-0000-0000-000000000200");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    const LLUUID copy_a = id("00000000-0000-0000-0000-000000000011");
    const LLUUID copy_b = id("00000000-0000-0000-0000-000000000012");
    ensure("source group created",
        model.createGroup(group, "Exact", ALGhostGroupModel::MODE_MOVE_FACE,
            {{a, transform(-1.0, 0.0, 0.0)},
             {b, transform(1.0, 0.0, 0.0)}}));
    ensure("member edit enabled", model.setEditMembers(group, true));
    ensure("member custom offset accepted",
        model.setMemberWorld(a, transform(-2.0, 0.5, 0.0), true));
    const ALGhostGroupModel::Group source = *model.findGroup(group);

    ensure("exact hierarchy clone succeeds",
        model.cloneGroup(group, clone, {copy_a, copy_b},
                         LLVector3d(3.0, 4.0, 0.0)));
    const ALGhostGroupModel::Group* copied = model.findGroup(clone);
    ensure("clone preserves mode and edit state",
        copied && copied->mMode == source.mMode &&
        copied->mEditMembers == source.mEditMembers);
    ensure("clone preserves authored and pinned local state",
        copied->mMembers[0].mAuthoredLocal.mFoot ==
            source.mMembers[0].mAuthoredLocal.mFoot &&
        copied->mMembers[0].mLocal.mFoot ==
            source.mMembers[0].mLocal.mFoot &&
        copied->mMembers[0].mPinned);
    ensure("clone maps replacement member ids",
        model.findGroupForMember(copy_a) == copied &&
        model.findGroupForMember(copy_b) == copied);
}

template<> template<>
void alghostgroupmodel_object::test<12>()
{
    ALGhostGroupModel model;
    const LLUUID group = id("00000000-0000-0000-0000-000000000100");
    const LLUUID a = id("00000000-0000-0000-0000-000000000001");
    const LLUUID b = id("00000000-0000-0000-0000-000000000002");
    ensure("editable group created",
        model.createGroup(group, "Authored safety",
            ALGhostGroupModel::MODE_RIGID,
            {{a, transform(-1.0, 0.0, 0.0)},
             {b, transform(1.0, 0.0, 0.0)}}));
    ensure("group scale set",
        model.setGroupTransform(
            group, transform(0.0, 0.0, 0.0, 0.f, 2.f)));
    ensure("editing enabled", model.setEditMembers(group, true));
    ensure("member pinned at valid custom world transform",
        model.setMemberWorld(a, transform(-3.0, 1.0, 0.0), true));
    const ALGhostGroupModel::Group before = *model.findGroup(group);

    ALGhostGroupModel::Transform impossible_authored =
        transform(std::numeric_limits<F64>::max(), 0.0, 0.0);
    ensure("authored local that overflows current group is rejected",
        !model.setMemberAuthoredLocal(a, impossible_authored));
    const ALGhostGroupModel::Group* after = model.findGroup(group);
    ensure("failed authored edit is atomic",
        after->mRevision == before.mRevision &&
        after->mMembers[0].mAuthoredLocal.mFoot ==
            before.mMembers[0].mAuthoredLocal.mFoot &&
        after->mMembers[0].mLocal.mFoot ==
            before.mMembers[0].mLocal.mFoot);
}
}
