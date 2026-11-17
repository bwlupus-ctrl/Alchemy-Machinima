#ifndef AL_KEYFRAME_VIEWER_H
#define AL_KEYFRAME_VIEWER_H
#include "alkeyframecore.h"
class LLUUID;
class LLSpatialGroup;
namespace ALKeyframeViewer
{
ALKf::Id id(const LLUUID&);
ALKf::Pose capturePose();
void present(const ALKf::Pose&);
bool cameraRestricted();
// Cached restriction is dirtied by the RLVa toggle signal, not polled per frame.
void dirtyRestriction();
void cleanParticleTransition();
void particleGroupBuilt(LLSpatialGroup*, bool forbiddenGeometry);
bool particleGroupForbidden(const LLSpatialGroup*);
}
#endif
