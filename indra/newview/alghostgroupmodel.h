/**
 * @file alghostgroupmodel.h
 * @brief Pure authoring model for Ghost Studio crowd groups.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGHOSTGROUPMODEL_H
#define AL_ALGHOSTGROUPMODEL_H

#include "llmath.h"
#include "v3dmath.h"
#include "llquaternion.h"
#include "lluuid.h"

#include <map>
#include <string>
#include <vector>

/**
 * Crowd groups are authored hierarchically:
 *
 *     member world = group world * member local
 *
 * The viewer's quaternion/vector operators use the row-vector convention, so
 * the implementation expresses that rule as:
 *
 *     world foot     = group foot + (local foot * group scale) * group rotation
 *     world rotation = local rotation * group rotation
 *     world scale    = local scale * group scale
 *
 * This class deliberately owns no viewer objects and performs no rendering.
 * ALGhostStudio remains responsible for applying resolved transforms to overlay
 * records and entity-clone runtimes.
 */
class ALGhostGroupModel
{
public:
    enum EMode : S32
    {
        MODE_RIGID = 1,
        MODE_MOVE_FACE = 2
    };

    struct Transform
    {
        LLVector3d    mFoot;
        LLQuaternion mRotation;
        F32          mScale = 1.f;

        bool isFinite() const;
    };

    struct Member
    {
        LLUUID    mInstanceId;
        Transform mAuthoredLocal;
        Transform mLocal;
        bool      mPinned = false;
    };

    struct Group
    {
        LLUUID              mId;
        std::string         mName;
        Transform           mWorld;
        EMode               mMode = MODE_RIGID;
        std::vector<Member> mMembers;
        bool                mEditMembers = false;
        U64                 mRevision = 1;
    };

    struct WorldMember
    {
        LLUUID    mInstanceId;
        Transform mWorld;
    };

    bool createGroup(const LLUUID& group_id, const std::string& name, EMode mode,
                     const std::vector<WorldMember>& members);
    // Exact hierarchy clone used by atomic UI duplication. The ordered member
    // ids are replaced while authored/current locals, pins, edit mode, and
    // group mode are preserved. The optional world offset is applied once to
    // the cloned group pivot.
    bool cloneGroup(const LLUUID& source_group_id,
                    const LLUUID& clone_group_id,
                    const std::vector<LLUUID>& clone_member_ids,
                    const LLVector3d& world_offset);
    bool eraseGroup(const LLUUID& group_id);
    void clear();

    Group*       findGroup(const LLUUID& group_id);
    const Group* findGroup(const LLUUID& group_id) const;
    Group*       findGroupForMember(const LLUUID& instance_id);
    const Group* findGroupForMember(const LLUUID& instance_id) const;

    bool resolveMember(const LLUUID& instance_id, Transform& world) const;
    bool setGroupTransform(const LLUUID& group_id, const Transform& world);
    bool transformFromMember(const LLUUID& instance_id,
                             const Transform& requested_member_world);
    bool setMemberWorld(const LLUUID& instance_id, const Transform& world,
                        bool pin_member);
    bool setMemberAuthoredLocal(const LLUUID& instance_id,
                                const Transform& authored_local);
    // Dynamic crowd facing changes only the member-local orientation. It is
    // valid for collapsed rigid units and does not enable member translation.
    bool setMemberWorldRotation(const LLUUID& instance_id,
                                const LLQuaternion& world_rotation);
    bool resetMemberOffset(const LLUUID& instance_id);
    bool setMemberPinned(const LLUUID& instance_id, bool pinned);
    bool setEditMembers(const LLUUID& group_id, bool editing);
    bool setMode(const LLUUID& group_id, EMode mode);
    bool renameGroup(const LLUUID& group_id, const std::string& name);
    // Transaction rollback only: identity and ordered membership must exactly
    // match the existing group, and the candidate is fully validated before
    // the live record is replaced.
    bool restoreGroupSnapshot(const Group& snapshot);

    LLUUID representative(const LLUUID& group_id) const;
    std::vector<LLUUID> members(const LLUUID& group_id) const;
    // Expands either a group header or one of its members to the complete
    // authoring unit. Ungrouped ids are returned as a one-element unit.
    std::vector<LLUUID> authoringUnitMembers(const LLUUID& id) const;
    const std::map<LLUUID, Group>& groups() const { return mGroups; }

private:
    static bool normalizeTransform(Transform& transform);
    // Normalize and validate both the authored and current hierarchy before a
    // candidate replaces live state. This is the transaction boundary shared
    // by every mutation that can change resolved member transforms.
    static bool normalizeGroupCandidate(Group& group);
    static Transform worldToLocal(const Transform& group, const Transform& world);
    static Transform localToWorld(const Transform& group, const Transform& local);

    std::map<LLUUID, Group>  mGroups;
    std::map<LLUUID, LLUUID> mMemberToGroup;
};

#endif // AL_ALGHOSTGROUPMODEL_H
