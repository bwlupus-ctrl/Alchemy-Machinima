#ifndef AL_ALREFLECTANIM_H
#define AL_ALREFLECTANIM_H

#include "llsingleton.h"
#include "lluuid.h"
#include "llassettype.h"
#include "llassetstorage.h"
#include "alreflectmath.h"
#include <map>
#include <vector>

// Inventory joins only. Registry ids never leave this viewer.
class ALReflectAnim : public LLSingleton<ALReflectAnim>
{
    LLSINGLETON(ALReflectAnim);
    ~ALReflectAnim();
public:
    bool play(const LLUUID& original, ALReflect::Route route, const std::string& name, bool enabled);
    void setReflected(const LLUUID& original, ALReflect::Route route, const std::string& name, bool enabled);
    bool contains(const LLUUID& original) const
    {
        auto found = mIntents.find(original);
        return found != mIntents.end() && found->second.intent == ALReflect::Intent::PLAY;
    }
    bool handles(const LLUUID& original) const { return mIntents.count(original) != 0; }
    bool inworld(const LLUUID& original) const;
    bool isDerived(const LLUUID& id) const;
    LLUUID originalFor(const LLUUID& id) const;
    LLUUID previewID(const LLUUID& id) const;
    bool pending(const LLUUID& id) const;
    bool finished(const LLUUID& original) const;
    bool echo(const LLUUID& original, bool start);
    void stop(const LLUUID& original, bool natural = false);
private:
    enum class Build { PENDING, READY, FAILED };
    struct Asset
    {
        LLUUID derived;
        Build state = Build::PENDING;
        std::vector<U8> bytes;
        std::string name;
        std::string failure;
        bool stopped = false;
        bool notified = false;
    };
    std::map<LLUUID, Asset> mAssets;
    std::map<LLUUID, ALReflect::Playback> mIntents;
    LLUUID mAvatar;
    U64 mGeneration = 0;
    void request(const LLUUID& original, const std::string& name);
    void build(const LLUUID& original, bool fetched);
    bool mirror(const LLUUID& original, Asset& asset);
    ALReflect::Decision decision(const LLUUID& original, ALReflect::Event event) const;
    void ensure(const LLUUID& original);
    void stopLocal(const LLUUID& original);
    void clear();
    static void idle(void*);
    static void loaded(const LLUUID&, LLAssetType::EType, void*, S32, LLExtStat);
};
#endif
