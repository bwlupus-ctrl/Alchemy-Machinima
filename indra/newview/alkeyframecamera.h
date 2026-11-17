#ifndef AL_KEYFRAME_CAMERA_H
#define AL_KEYFRAME_CAMERA_H
#include "alkeyframecore.h"
class ALKeyframeCamera final : public ALKfClient
{
public:
    const char* kind() const override;
    std::vector<ALKf::Channel> schema() const override;
    std::unique_ptr<ALKfClientRuntime> createRuntime() const override;
    ALKf::Key capture() const override;
    void apply(const ALKf::Track&, const ALKf::Cache&, double, ALKfClientRuntime&) override;
    bool restricted(std::string&) const override;
    void rewriteRefs(ALKf::Sequence&, const std::function<void(ALKf::RefSlot&)>&) const override;
    bool ownsCamera() const override;
};
#endif
