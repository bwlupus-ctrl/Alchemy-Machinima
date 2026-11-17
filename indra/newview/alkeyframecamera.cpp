#include "llviewerprecompiledheaders.h"
#include "alkeyframecamera.h"
#include "alkeyframeplayer.h"
#include "alkeyframeviewer.h"
#include "llagent.h"
#include "llcinematiccamera.h"
#include "llviewerobjectlist.h"
#include "llviewercamera.h"
#include "llvoavatar.h"
#include "llghostavatar.h"
#include "alghoststudio.h"
#include "lljoint.h"
#include "m3math.h"
#include "rlvactions.h"
#include "rlvhandler.h"
#include <cstring>

namespace
{
bool sRestrictionDirty = true, sRestricted = false, sEnabled = false;
class Resolver final : public ALKf::Resolver
{
    bool resolve(const ALKf::RefSlot& ref, ALKf::Vec3& out) const override
    {
        LL_PROFILE_ZONE_NAMED("keyframe resolve");
        if (ref.kind == ALKf::RefKind::Point) { out = ref.fallback; return true; }
        LLUUID uuid; std::memcpy(uuid.mData, ref.id.data(), 16);
        LLViewerObject* object = gObjectList.findObject(uuid);
        if (!object && ref.kind == ALKf::RefKind::Clone)
            object = ALGhostStudio::instance().resolveEntityClone(uuid);
        if (!object || object->isDead()) return false;
        LLVector3d p = object->getPositionGlobal();
        if (LLVOAvatar* avatar = object->asAvatar())
        {
            if (LLJoint* joint = avatar->getJoint(ref.joint))
            {
                LLVector3 point = joint->getWorldPosition();
                LLVector3 base = avatar->getPositionAgent();
                if(LLJoint* root=avatar->getRootJoint())
                { base=root->getWorldPosition(); base.mV[VZ]-=avatar->getPelvisToFoot(); }
                point = base + (point - base) * avatar->getUniformScale();
                p = gAgent.getPosGlobalFromAgent(point);
            }
        }
        out = {p.mdV[0], p.mdV[1], p.mdV[2]};
        return ALKf::finite(out);
    }
};
}
ALKf::Id ALKeyframeViewer::id(const LLUUID& uuid)
{
    ALKf::Id value; std::memcpy(value.data(), uuid.mData, 16); return value;
}
void ALKeyframeViewer::dirtyRestriction() { sRestrictionDirty = true; }
bool ALKeyframeViewer::cameraRestricted()
{
    const bool enabled = RlvActions::isRlvEnabled();
    if (sRestrictionDirty || enabled != sEnabled)
    {
        sEnabled = enabled; sRestrictionDirty = false;
        sRestricted = enabled && (RlvActions::isCameraPresetLocked() ||
            gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK) ||
            RlvActions::isCameraDistanceClamped() || RlvActions::isCameraFOVClamped());
    }
    return sRestricted;
}
ALKf::Pose ALKeyframeViewer::capturePose()
{
    LLVector3 position; LLQuaternion q; F32 fov;
    LLCinematicCamera::preOperatorPresentedPose(position,q,fov);
    const LLVector3d p = gAgent.getPosGlobalFromAgent(position);
    ALKf::Pose pose{{p.mdV[0], p.mdV[1], p.mdV[2]}, {q.mQ[3], q.mQ[0], q.mQ[1], q.mQ[2]}, fov, 0};
    const auto forward = pose.rotation.rotate({1,0,0});
    const auto level = ALKf::levelAim(forward, 0);
    const auto up = level.rotate({0,0,1}), actual = pose.rotation.rotate({0,0,1});
    pose.roll = std::atan2(ALKf::dot(forward, ALKf::cross(up, actual)), ALKf::dot(up, actual));
    return pose;
}
void ALKeyframeViewer::present(const ALKf::Pose& pose)
{
    auto* cam = LLViewerCamera::getInstance();
    const LLVector3 p = gAgent.getPosAgentFromGlobal(LLVector3d(pose.position.x, pose.position.y, pose.position.z));
    const LLQuaternion q((F32)pose.rotation.x, (F32)pose.rotation.y, (F32)pose.rotation.z, (F32)pose.rotation.w);
    const LLMatrix3 axes(q);
    const F32 fov = llclamp((F32)pose.fov, cam->getMinView(), cam->getMaxView());
    cam->setViewNoBroadcast(fov);
    cam->setOrigin(p);
    cam->mXAxis = LLVector3(axes.mMatrix[0]);
    cam->mYAxis = LLVector3(axes.mMatrix[1]);
    cam->mZAxis = LLVector3(axes.mMatrix[2]);
    LLCinematicCamera::rememberPreOperatorPresentation(p, q, fov);
}
const char* ALKeyframeCamera::kind() const { return "camera"; }
std::vector<ALKf::Channel> ALKeyframeCamera::schema() const { return ALKf::cameraTrack().schema; }
std::unique_ptr<ALKfClientRuntime> ALKeyframeCamera::createRuntime() const { return std::make_unique<ALKfClientRuntime>(); }
ALKf::Key ALKeyframeCamera::capture() const
{
    const auto p = ALKeyframeViewer::capturePose();
    ALKf::Key key; key.values = {p.position, p.rotation, p.fov, p.roll}; return key;
}
void ALKeyframeCamera::apply(const ALKf::Track& track, const ALKf::Cache& cache, double time, ALKfClientRuntime& runtime)
{
    static const Resolver resolver;
    const auto state = ALKeyframePlayer::instance().state();
    const auto mode = state == ALKfTransport::State::PREVIEW ? ALKf::EvalMode::Preview : ALKf::EvalMode::Play;
    LL_PROFILE_ZONE_NAMED("keyframe reacquire");
    ALKeyframeViewer::present(ALKf::evalCamera(track, cache, time, resolver, mode, runtime));
}
bool ALKeyframeCamera::restricted(std::string& why) const
{
    const bool blocked = ALKeyframeViewer::cameraRestricted();
    why = blocked ? "Camera restricted by RLVa" : ""; return blocked;
}
void ALKeyframeCamera::rewriteRefs(ALKf::Sequence& s, const std::function<void(ALKf::RefSlot&)>& rewrite) const { ALKf::rewriteRefs(s, rewrite); }
bool ALKeyframeCamera::ownsCamera() const { return ALKeyframePlayer::instance().ownsCamera(); }
