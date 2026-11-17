#ifndef AL_KEYFRAME_PLAYER_H
#define AL_KEYFRAME_PLAYER_H
#include "alkeyframetransport.h"
#include "alkeyframecore.h"
#include <memory>
struct LLTemporalFrameContext;
class ALKeyframePlayer
{
public:
    static ALKeyframePlayer& instance();
    ALKeyframePlayer();
    ~ALKeyframePlayer();
    bool play(bool action = false);
    bool visit(std::uint32_t key);
    bool scrub(double time);
    void stop(ALKfTransport::Exit reason);
    void lifecycleCheck();
    void advance(const LLTemporalFrameContext&);
    void updateCamera();
    void onDisconnect();
    void shutdown();
    bool ownsCamera() const;
    bool rlvHandbackActive() const;
    bool restricted(std::string& why) const;
    ALKf::Sequence& sequence();
    ALKf::History& history();
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
#endif
