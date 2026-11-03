/** [ActorKey] Frame-local beauty decisions and exact styled component replay (D54/D55). */
#pragma once
#include "lluuid.h"
#include <functional>
#include <map>
#include <set>
#include <unordered_set>
class LLDrawInfo;
class LLGLSLShader;
class LLVertexBuffer;
class LLVOAvatar;

struct SystemComponentKey
{
    enum Part { BodyOpaque, BodyTransparent, RigidEyes };
    LLUUID id; // get_actor_fx_id(avatar), including the Director's canonical self id
    Part part;
    bool operator<(const SystemComponentKey& rhs) const
    { return id < rhs.id || (!(rhs.id < id) && part < rhs.part); }
};

class ALActorKeyCapture
{
public:
    static bool sArmed; // cached OFF branch: no allocation, lookup, capture or GL work
    static ALActorKeyCapture& instance();
    static bool beautyContext();
    static LLUUID systemActorId(const LLVOAvatar* avatar); // exactly the avatar pool beauty identity
    void beginBeauty();
    void release();
    void suppress(const LLDrawInfo* info) { mBeautySuppressed.insert(info); }
    void suppress(const SystemComponentKey& key) { mSystemSuppressed.insert(key); }
    bool native(const LLDrawInfo* info);
    bool native(const SystemComponentKey& key);
    void capture(const LLDrawInfo* info, const LLUUID& owner, bool coverage_bearing,
                 LLVertexBuffer* vb, U32 start, U32 end, U32 count, U32 offset, bool double_sided = false, bool blended = false);
    void capture(const SystemComponentKey& key, bool coverage_bearing, U32 drawn,
                 std::function<void(LLGLSLShader&)> replay);
    void contributed(const LLDrawInfo* info) { ++mDrawTrace[info].native; }
    void contributed(const SystemComponentKey& key) { ++mSystemTrace[key].native; }
    bool replay(bool include_alpha = true); // target/state are owned by K1, before the final Prism occluders
private:
    // These are decision records, never an owner-level re-evaluation of ActorStyle.
    std::unordered_set<const LLDrawInfo*> mBeautySuppressed;
    std::set<SystemComponentKey> mSystemSuppressed;
    struct Contributors { U32 native = 0, shared = 0; };
    std::map<const LLDrawInfo*, Contributors> mDrawTrace;
    std::map<SystemComponentKey, Contributors> mSystemTrace;
};
