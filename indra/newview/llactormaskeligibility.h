// [ActorMask] Native suppression and asynchronous per-view measurement data.
#ifndef LL_LLACTORMASKELIGIBILITY_H
#define LL_LLACTORMASKELIGIBILITY_H
#include "lluuid.h"
#include "llrender.h" // [ActorMask] Complete texture-unit snapshots without changing LLRender.
#include <unordered_set>
#include <set>
#include <vector>
#include <array>
class LLDrawInfo;
struct LLActorMaskViewStamp
{
    U32 frame = 0;
    const void* pack = nullptr;
    const void* scratch = nullptr;
    bool cube = false, mirror = false, prism = false;
    bool operator==(const LLActorMaskViewStamp& rhs) const;
};
class LLActorMaskEligibility
{
public:
    static LLActorMaskEligibility& instance();
    static bool sArmed;
    class Scope
    {
    public:
        explicit Scope(const LLActorMaskViewStamp& stamp);
        ~Scope();
    };
    void suppress(const LLDrawInfo* draw) { mDraws.insert(draw); }
    void suppress(const LLUUID& body) { mBodies.insert(body); }
    bool matches(const LLActorMaskViewStamp& stamp) const { return mStamp == stamp; }
    bool suppressed(const LLDrawInfo* draw) const { return mDraws.count(draw) != 0; }
    bool suppressed(const LLUUID& body) const { return mBodies.count(body) != 0; }
    // [ActorMask] Native coverage changes invalidate all recipient bits on that draw.
    void coverageChanged(const LLUUID& owner);
    void coverageChanged(const LLDrawInfo& draw);
    U8 coverageBits() const { return mCoverageBits; }
private:
    LLActorMaskViewStamp mStamp;
    std::unordered_set<const LLDrawInfo*> mDraws;
    std::set<LLUUID> mBodies;
    U8 mCoverageBits = 0;
};
enum class LLActorMaskQueryKind : U8 { TIGHT, LOOSE, RASTER };
struct LLActorMaskQueryTag
{
    U32 frame = 0;
    U64 view = 0;
    LLUUID recipient;
    U8 recipientBit = 0;
    LLActorMaskQueryKind kind = LLActorMaskQueryKind::TIGHT;
    U32 drawsIssued = 0;
    U32 batchesSkippedLikeNative = 0; // [ActorMask] Per-query replay skips for WP7.
    bool inFrustum = false;
    bool impostor = false, styled = false; // captured, never inferred from current view
};
struct LLActorMaskQueryResult
{
    LLActorMaskQueryTag tag;
    U32 samples = 0;
};
// Never reuse a pending query; slow GPU results may skip measurements.
class LLActorMaskQueries
{
public:
    bool begin(const LLActorMaskQueryTag& tag);
    void end(U32 draws, U32 skipped = 0);
    void poll();
    void release();
    std::vector<LLActorMaskQueryResult> takeResults();
private:
    struct Entry { U32 id = 0; bool pending = false; LLActorMaskQueryTag tag; };
    std::array<std::array<Entry, 24>, 3> mRing{};
    Entry* mActive = nullptr;
    std::vector<LLActorMaskQueryResult> mResults;
};
// [ActorMask] Copy every wrapper field (including mip/any colour-space bookkeeping).
// This LLTexUnit version has no colour-space member; the enum alone carries no state.
// Replay binds 2D textures; slow avatar binds can also unbind the previous target.
// Preserve both actual bindings independently of logical zero/white/TT_NONE.
class LLActorMaskTextureState : public LLTexUnit
{
public:
    explicit LLActorMaskTextureState(LLTexUnit& unit);
    void restore(LLTexUnit& unit) const;
    std::array<U32, 5> signature() const
    { return { mCurrTexture, static_cast<U32>(mCurrTexType), mHasMipMaps ? 1U : 0U,
               static_cast<U32>(mBinding2D), static_cast<U32>(mBindingOriginal) }; }
private:
    GLint mBinding2D = 0;
    GLint mBindingOriginal = 0;
};
// WP1 companion guard: the existing ghost diagnostics are owned by WP7.
// WP2 can use this guard around its actor light block.
class LLScopedActorMaskInvariant
{
public:
    explicit LLScopedActorMaskInvariant(const char* scope);
    ~LLScopedActorMaskInvariant();
private:
    struct State
    {
        std::array<S32, 42> ints{};
        std::array<F32, 86> floats{}; // clear, polygon offset, modelview + 4 texture matrices
        std::array<const void*, 3> pointers{};
        std::vector<std::array<U32, 5>> textures; // [ActorMask] Wrapper type, id, mips and actual bindings.
        std::vector<S32> drawBuffers; // [ActorMask] All supported draw-buffer slots.
        bool operator==(const State& r) const
        { return ints == r.ints && floats == r.floats && pointers == r.pointers &&
                 textures == r.textures && drawBuffers == r.drawBuffers; }
    };
    static State capture();
    State mBefore;
    const char* mScope;
    bool mEnabled;
};
#endif
