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
    ALKfTransport::State state() const;
    double time() const;
    double duration();
    std::uint64_t revision() const;
    const std::string& status() const;
    // Validates before stopping or swapping; resets history identity and runtime.
    bool replaceSequence(ALKf::Sequence, std::string& reason);
    const ALKf::Cache* cameraCache();
    // Call only for authoritative deletion, never visibility/cache eviction.
    void targetRemoved(const ALKf::Id&);
    void rewriteActorReferences(const ALKf::Id& oldId, const ALKf::Id& newId);
    // UI/overlay owners register cancellation here; called before capture loss.
    void setCancelEdit(std::function<void()>);
    void presented();
    // WP6 registers only while editing is visible; invoked in the guarded guides pass.
    void setGuideDraw(std::function<void()>);
    bool hasGuideDraw() const;
    void drawGuides() const;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
#endif
