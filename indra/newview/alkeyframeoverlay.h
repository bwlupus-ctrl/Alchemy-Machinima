#ifndef AL_KEYFRAME_OVERLAY_H
#define AL_KEYFRAME_OVERLAY_H
#include "alkeyframecore.h"
class ALKeyframeOverlay
{
public:
    static void draw(); // WP6: inside the effective-UI guides pass only
    static void refresh(const ALKf::Track&, const ALKf::Cache&, unsigned selected, bool dots, bool icons);
    static void clear();
    struct Pick { unsigned key; int handle; ALKf::Vec3 position; };
    static const std::vector<Pick>& picks();
};
#endif
