// Native fixture for the coalesced body-visibility walk
// (AlchemyPerfCoalescedBodyVisibility). Not a viewer build.
//
// body_visibility_coalesce_check.py instantiates the VARIANT block below once
// per variant, each in its own namespace, with the REAL production methods
// spliced in at the PRODUCTION_* markers: the real variant as written, and the
// planted negatives with one production line changed. main() runs every
// variant and prints the bit mask of the tests it failed.
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <vector>

typedef int S32;
typedef unsigned int U32;
typedef unsigned char U8;

enum
{
    T_EQUIVALENCE = 1 << 0, // coalesced == immediate after the frame walk / at a read
    T_COALESCED   = 1 << 1, // many requests, one walk
    T_IMMEDIATE   = 1 << 2, // immediate paths walk at once and clear the mark
    T_SWITCH_OFF  = 1 << 3, // switch off = one walk per request, no marks
    T_LIVE_TOGGLE = 1 << 4, // marks pending when the switch goes off are flushed
    T_DELETE      = 1 << 5, // dead / deleted avatars leave no dirty state
    T_CLEAN       = 1 << 6, // nothing dirty = no list walk, no visibility walk
    T_ORDER       = 1 << 7, // walk before readers and before explicit hides
    T_ASSERT      = 1 << 8, // a production llassert fired
};

// VARIANT_BEGIN
namespace VARIANT_NAME
{
int gAssertFailures = 0;
#undef llassert
#define llassert(expr) do { if (!(expr)) { ++gAssertFailures; } } while (0)
// The production imbalance warning counts as a failed assert here.
struct WarningSink
{
    template <class V> WarningSink& operator<<(const V&) { return *this; }
};
#undef LL_WARNS_ONCE
#undef LL_ENDL
#define LL_WARNS_ONCE(tag) (++gAssertFailures, WarningSink())
#define LL_ENDL 0

bool gSwitch = true;
struct Settings {} gSavedSettings;
template <class T> struct LLCachedControl
{
    LLCachedControl(Settings&, const char*, T) {}
    operator T() const { return (T)gSwitch; } // live: read on every use
};

struct LLUUID
{
    int mValue = 0;
    bool operator==(const LLUUID& rhs) const { return mValue == rhs.mValue; }
    bool operator!=(const LLUUID& rhs) const { return mValue != rhs.mValue; }
};
const LLUUID IMG_USE_BAKED_HEAD{1}, IMG_USE_BAKED_UPPER{2}, IMG_USE_BAKED_LOWER{3}, IMG_USE_BAKED_EYES{4},
    IMG_USE_BAKED_SKIRT{5}, IMG_USE_BAKED_HAIR{6}, IMG_USE_BAKED_LEFTARM{7}, IMG_USE_BAKED_LEFTLEG{8},
    IMG_USE_BAKED_AUX1{9}, IMG_USE_BAKED_AUX2{10}, IMG_USE_BAKED_AUX3{11};

namespace LLAvatarAppearanceDefines
{
struct LLAvatarAppearanceDictionary
{
    static bool isBakedImageId(const LLUUID& id) { return id.mValue >= 1 && id.mValue <= 11; }
};
}

enum EBakedTextureIndex
{
    BAKED_HEAD, BAKED_UPPER, BAKED_LOWER, BAKED_EYES, BAKED_SKIRT, BAKED_HAIR, BAKED_LEFT_ARM, BAKED_LEFT_LEG,
    BAKED_AUX1, BAKED_AUX2, BAKED_AUX3, BAKED_NUM_INDICES
};
enum EMeshIndex
{
    MESH_ID_HAIR, MESH_ID_HEAD, MESH_ID_EYELASH, MESH_ID_UPPER_BODY, MESH_ID_LOWER_BODY, MESH_ID_EYEBALL_LEFT,
    MESH_ID_EYEBALL_RIGHT, MESH_ID_SKIRT, MESH_ID_COUNT
};

struct LLTextureEntry
{
    LLUUID mID;
    const LLUUID& getID() const { return mID; }
};

template <class T> struct LLPointer
{
    T* mPointer = nullptr;
    LLPointer() = default;
    LLPointer(T* p) : mPointer(p) {}
    bool isNull() const { return mPointer == nullptr; }
    T* operator->() const { return mPointer; }
    T* get() const { return mPointer; }
};

struct LLAvatarJoint
{
    bool mVisible = true;
    int mCalls = 0;
    void setVisible(bool visible, bool) { mVisible = visible; ++mCalls; }
};

class LLVOAvatar;

class LLViewerObject
{
public:
    std::vector<LLTextureEntry> mTEs;
    std::vector<LLPointer<LLViewerObject>> mChildren;
    LLVOAvatar* mAvatar = nullptr; // set while attached (children share it)

    int getNumTEs() const { return (int)mTEs.size(); }
    LLTextureEntry* getTE(int te) { return &mTEs[te]; }
    const std::vector<LLPointer<LLViewerObject>>& getChildren() const { return mChildren; }
    LLVOAvatar* getAvatar() const { return mAvatar; }

    // Mirrors LLViewerObject::setTETextureCore: write the id, then ask.
    void setTEID(int te, int id)
    {
        LLUUID old_id = mTEs[te].mID;
        mTEs[te].mID = LLUUID{id};
        updateAvatarMeshVisibility(mTEs[te].mID, old_id);
    }
    void updateAvatarMeshVisibility(const LLUUID& id, const LLUUID& old_id);
};

struct LLViewerJointAttachment
{
    std::vector<LLPointer<LLViewerObject>> mAttachedObjects;
};

struct LLCharacter;
struct CountingList
{
    std::list<struct LLCharacter*> mList;
    int mWalks = 0;
    std::list<struct LLCharacter*>::iterator begin() { ++mWalks; return mList.begin(); }
    std::list<struct LLCharacter*>::iterator end() { return mList.end(); }
};

struct LLCharacter
{
    static CountingList sInstances;
    virtual ~LLCharacter() {}
};
CountingList LLCharacter::sInstances;

bool gReadersGuarded = true; // the planted negative reads without the guard

class LLVOAvatar : public LLCharacter
{
public:
    enum AvatarOverallAppearance { AOA_NORMAL, AOA_JELLYDOLL, AOA_INVISIBLE };

    LLVOAvatar()
    {
        for (int i = 0; i < MESH_ID_COUNT; ++i) { mMeshLOD.push_back(&mJoints[i]); }
        mAttachmentPoints[1] = &mPoint;
        sInstances.mList.push_back(this);
    }
    ~LLVOAvatar() override
    {
        // PRODUCTION_DESTRUCTOR_CLEAR
        sInstances.mList.remove(this);
    }

    // Mirrors LLVOAvatar::markDead; the check pins the production line and order.
    void markDead()
    {
        // PRODUCTION_MARKDEAD_CLEAR
        mDead = true;
    }
    bool isDead() const { return mDead; }
    AvatarOverallAppearance getOverallAppearance() const { return mOverall; }

    void updateMeshVisibility();
    void hideHair();
    void hideSkirt();
// PRODUCTION_HEADER
public:
    // Mirrors the readers (renderSkinned / renderTransparent / renderRigid /
    // isActorGhostSystemReplayReady): the guard, then the joints' flags.
    U32 readVisibility()
    {
        if (gReadersGuarded) { flushMeshVisibilityIfDirty(); }
        return rawVisibility();
    }
    U32 rawVisibility() const
    {
        U32 mask = 0;
        for (int i = 0; i < MESH_ID_COUNT; ++i) { mask |= (mJoints[i].mVisible ? 1u : 0u) << i; }
        return mask;
    }
    int walks() const { return mJoints[MESH_ID_HEAD].mCalls; } // one setVisible per walk

    // Mirrors LLVOAvatar::attachObject / detachObject: change, then walk at once.
    void attach(LLViewerObject* object)
    {
        mPoint.mAttachedObjects.push_back(object);
        setObjectAvatar(object, this);
        updateMeshVisibility();
    }
    void detach(LLViewerObject* object)
    {
        auto& list = mPoint.mAttachedObjects;
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            if (it->get() == object) { list.erase(it); break; }
        }
        setObjectAvatar(object, nullptr);
        updateMeshVisibility();
    }
    // Mirrors LLVOAvatar::updateOverallAppearance.
    void setOverall(AvatarOverallAppearance overall)
    {
        if (overall != mOverall) { mOverall = overall; updateMeshVisibility(); }
    }
    static void setObjectAvatar(LLViewerObject* object, LLVOAvatar* avatar)
    {
        object->mAvatar = avatar;
        for (auto& child : object->mChildren) { child->mAvatar = avatar; }
    }

    LLAvatarJoint mJoints[MESH_ID_COUNT];
    std::vector<LLAvatarJoint*> mMeshLOD;
    LLViewerJointAttachment mPoint;
    std::map<S32, LLViewerJointAttachment*> mAttachmentPoints;
    AvatarOverallAppearance mOverall = AOA_NORMAL;
    bool mDead = false;
};

// ---- production methods, spliced verbatim (or with one planted change) ----
// PRODUCTION_METHODS
// ---------------------------------------------------------------------------

struct Rng
{
    U32 mState;
    U32 next() { mState = mState * 1664525u + 1013904223u; return mState >> 8; }
    int pick(int n) { return (int)(next() % (U32)n); }
};

const int AVATARS = 3;
const int OBJECTS = 3; // per avatar
const int ROOT_TES = 4;
const int CHILD_TES = 2;
const int TE_IDS[] = { 0, 100, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

struct World
{
    LLVOAvatar mAvatars[AVATARS];
    LLViewerObject mRoots[AVATARS][OBJECTS];
    LLViewerObject mKids[AVATARS][OBJECTS];
    bool mAttached[AVATARS][OBJECTS] = {};
    World()
    {
        for (int a = 0; a < AVATARS; ++a)
        {
            for (int o = 0; o < OBJECTS; ++o)
            {
                mRoots[a][o].mTEs.resize(ROOT_TES);
                mKids[a][o].mTEs.resize(CHILD_TES);
                mRoots[a][o].mChildren.push_back(&mKids[a][o]);
            }
        }
    }
};

// One scripted session. The trace is every visibility a reader or a frame end
// could see; it must not depend on the switch.
std::vector<U32> runSession(U32 seed, bool coalesce, int* total_walks)
{
    gSwitch = coalesce;
    std::vector<U32> trace;
    {
        World world;
        Rng rng{seed};
        for (int step = 0; step < 4000; ++step)
        {
            const int a = rng.pick(AVATARS);
            const int o = rng.pick(OBJECTS);
            LLVOAvatar& avatar = world.mAvatars[a];
            const int op = rng.pick(100);
            if (op < 60)
            {   // attachment texture entry change (the coalesced path), often in bursts
                const int burst = 1 + rng.pick(6);
                for (int i = 0; i < burst; ++i)
                {
                    const bool kid = rng.pick(3) == 0;
                    LLViewerObject& object = kid ? world.mKids[a][o] : world.mRoots[a][o];
                    object.setTEID(rng.pick(kid ? CHILD_TES : ROOT_TES), TE_IDS[rng.pick(13)]);
                }
            }
            else if (op < 70)
            {   // attach / detach (immediate)
                if (world.mAttached[a][o]) { avatar.detach(&world.mRoots[a][o]); }
                else { avatar.attach(&world.mRoots[a][o]); }
                world.mAttached[a][o] = !world.mAttached[a][o];
            }
            else if (op < 75)
            {   // appearance change (immediate)
                avatar.setOverall(rng.pick(2) ? LLVOAvatar::AOA_NORMAL : LLVOAvatar::AOA_JELLYDOLL);
            }
            else if (op < 79)
            {   // explicit hides (other writers of the same flags)
                if (rng.pick(2)) { avatar.hideHair(); } else { avatar.hideSkirt(); }
            }
            else if (op < 89)
            {   // a reader inside the frame (shadow pass, probe, preview, clone replay)
                trace.push_back(avatar.readVisibility());
            }
            else
            {   // frame end: the frame walk, then everything draws
                LLVOAvatar::flushPendingMeshVisibility();
                for (int i = 0; i < AVATARS; ++i) { trace.push_back(world.mAvatars[i].rawVisibility()); }
            }
        }
        LLVOAvatar::flushPendingMeshVisibility();
        for (int i = 0; i < AVATARS; ++i)
        {
            trace.push_back(world.mAvatars[i].rawVisibility());
            *total_walks += world.mAvatars[i].walks();
        }
    }
    return trace;
}

int run()
{
    int failed = 0;
#define CHECK(bit, expr) do { if (!(expr)) { failed |= (bit); } } while (0)

    // T_EQUIVALENCE
    {
        int immediate_walks = 0, coalesced_walks = 0;
        for (U32 seed = 1; seed <= 8; ++seed)
        {
            const std::vector<U32> immediate = runSession(seed, false, &immediate_walks);
            CHECK(T_EQUIVALENCE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
            const std::vector<U32> coalesced = runSession(seed, true, &coalesced_walks);
            CHECK(T_EQUIVALENCE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
            CHECK(T_EQUIVALENCE, immediate == coalesced);
        }
        CHECK(T_COALESCED, coalesced_walks < immediate_walks);
    }

    // T_COALESCED: a body's faces all change in one callback -> one walk.
    {
        gSwitch = true;
        LLVOAvatar avatar;
        LLViewerObject body;
        body.mTEs.resize(200);
        avatar.attach(&body);
        const int before = avatar.walks();
        for (int te = 0; te < 200; ++te) { body.setTEID(te, te % 2 ? 1 : 2); }
        CHECK(T_COALESCED, avatar.walks() == before);
        CHECK(T_COALESCED, avatar.isMeshVisibilityDirty());
        CHECK(T_COALESCED, LLVOAvatar::getMeshVisibilityDirtyCount() == 1);
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_COALESCED, avatar.walks() == before + 1);
        CHECK(T_COALESCED, !avatar.isMeshVisibilityDirty());
        CHECK(T_COALESCED, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        CHECK(T_COALESCED, !avatar.mJoints[MESH_ID_HEAD].mVisible && !avatar.mJoints[MESH_ID_UPPER_BODY].mVisible);
        CHECK(T_COALESCED, avatar.mJoints[MESH_ID_LOWER_BODY].mVisible);
        // unchanged ids and non-bake ids never ask
        body.setTEID(0, 2);
        body.setTEID(1, 1);
        CHECK(T_COALESCED, !avatar.isMeshVisibilityDirty());
        LLViewerObject plain;
        plain.mTEs.resize(1);
        avatar.attach(&plain);
        plain.setTEID(0, 100);
        CHECK(T_COALESCED, !avatar.isMeshVisibilityDirty());
    }

    // T_IMMEDIATE: attach / detach / appearance walk at once, mark or no mark.
    {
        gSwitch = true;
        LLVOAvatar avatar;
        LLViewerObject body, hair;
        body.mTEs.resize(2);
        hair.mTEs.resize(1);
        hair.mTEs[0].mID = IMG_USE_BAKED_HAIR;
        int before = avatar.walks();
        avatar.attach(&body);
        CHECK(T_IMMEDIATE, avatar.walks() == before + 1);
        body.setTEID(0, 1); // pending
        CHECK(T_IMMEDIATE, avatar.isMeshVisibilityDirty() && avatar.mJoints[MESH_ID_HEAD].mVisible);
        before = avatar.walks();
        avatar.attach(&hair); // immediate, and it satisfies the pending request
        CHECK(T_IMMEDIATE, avatar.walks() == before + 1);
        CHECK(T_IMMEDIATE, !avatar.mJoints[MESH_ID_HEAD].mVisible && !avatar.mJoints[MESH_ID_HAIR].mVisible);
        CHECK(T_IMMEDIATE, !avatar.isMeshVisibilityDirty());
        CHECK(T_IMMEDIATE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_IMMEDIATE, avatar.walks() == before + 1); // nothing left to do
        avatar.detach(&hair);
        CHECK(T_IMMEDIATE, avatar.walks() == before + 2 && avatar.mJoints[MESH_ID_HAIR].mVisible);
        avatar.setOverall(LLVOAvatar::AOA_JELLYDOLL);
        CHECK(T_IMMEDIATE, avatar.walks() == before + 3 && avatar.mJoints[MESH_ID_HEAD].mVisible);
    }

    // T_SWITCH_OFF: the previous behaviour.
    {
        gSwitch = false;
        LLVOAvatar avatar;
        LLViewerObject body;
        body.mTEs.resize(50);
        avatar.attach(&body);
        const int before = avatar.walks();
        const int list_walks = LLCharacter::sInstances.mWalks;
        for (int te = 0; te < 50; ++te)
        {
            body.setTEID(te, 1);
            CHECK(T_SWITCH_OFF, avatar.walks() == before + te + 1);
            CHECK(T_SWITCH_OFF, !avatar.isMeshVisibilityDirty());
            CHECK(T_SWITCH_OFF, !avatar.mJoints[MESH_ID_HEAD].mVisible);
        }
        body.setTEID(0, 1);   // unchanged
        body.setTEID(1, 100); // bake -> plain still asks
        CHECK(T_SWITCH_OFF, avatar.walks() == before + 51);
        CHECK(T_SWITCH_OFF, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_SWITCH_OFF, avatar.walks() == before + 51);
        CHECK(T_SWITCH_OFF, LLCharacter::sInstances.mWalks == list_walks);
    }

    // T_LIVE_TOGGLE
    {
        gSwitch = true;
        LLVOAvatar first, second;
        LLViewerObject one, two;
        one.mTEs.resize(1);
        two.mTEs.resize(1);
        first.attach(&one);
        second.attach(&two);
        one.setTEID(0, 3);
        two.setTEID(0, 4);
        CHECK(T_LIVE_TOGGLE, LLVOAvatar::getMeshVisibilityDirtyCount() == 2);
        gSwitch = false; // turned off with work pending
        const int before = first.walks();
        one.setTEID(0, 2); // a request after the switch: walks now, clears its mark
        CHECK(T_LIVE_TOGGLE, first.walks() == before + 1 && !first.isMeshVisibilityDirty());
        CHECK(T_LIVE_TOGGLE, !first.mJoints[MESH_ID_UPPER_BODY].mVisible);
        CHECK(T_LIVE_TOGGLE, LLVOAvatar::getMeshVisibilityDirtyCount() == 1);
        LLVOAvatar::flushPendingMeshVisibility(); // the frame flush runs whatever the switch says
        CHECK(T_LIVE_TOGGLE, !second.isMeshVisibilityDirty() && !second.mJoints[MESH_ID_EYEBALL_LEFT].mVisible);
        CHECK(T_LIVE_TOGGLE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        gSwitch = true; // and back on
        two.setTEID(0, 0);
        CHECK(T_LIVE_TOGGLE, second.isMeshVisibilityDirty() && !second.mJoints[MESH_ID_EYEBALL_LEFT].mVisible);
        CHECK(T_LIVE_TOGGLE, (second.readVisibility() >> MESH_ID_EYEBALL_LEFT) & 1u);
    }

    // T_DELETE
    {
        gSwitch = true;
        LLViewerObject one, two, three;
        one.mTEs.resize(1);
        two.mTEs.resize(1);
        three.mTEs.resize(1);
        LLVOAvatar keeper;
        keeper.attach(&three);
        {
            LLVOAvatar dying;
            dying.attach(&one);
            one.setTEID(0, 1);
            CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 1);
            dying.markDead();
            CHECK(T_DELETE, !dying.isMeshVisibilityDirty());
            CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
            const int list_walks = LLCharacter::sInstances.mWalks;
            const int before = dying.walks();
            LLVOAvatar::flushPendingMeshVisibility();
            CHECK(T_DELETE, LLCharacter::sInstances.mWalks == list_walks && dying.walks() == before);
            one.setTEID(0, 2); // a late callback on a dead avatar: old behaviour, no mark
            CHECK(T_DELETE, dying.walks() == before + 1 && !dying.isMeshVisibilityDirty());
            CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        }
        {
            LLVOAvatar deleted; // destroyed while marked, without markDead
            deleted.attach(&two);
            two.setTEID(0, 1);
            three.setTEID(0, 6);
            CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 2);
            two.mAvatar = nullptr;
        }
        CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 1);
        LLVOAvatar::flushPendingMeshVisibility(); // must not touch the deleted avatar
        CHECK(T_DELETE, LLVOAvatar::getMeshVisibilityDirtyCount() == 0);
        CHECK(T_DELETE, !keeper.mJoints[MESH_ID_HAIR].mVisible);
    }

    // T_CLEAN
    {
        gSwitch = true;
        LLVOAvatar avatar;
        LLViewerObject body;
        body.mTEs.resize(1);
        avatar.attach(&body);
        const int before = avatar.walks();
        const int list_walks = LLCharacter::sInstances.mWalks;
        for (int frame = 0; frame < 1000; ++frame)
        {
            LLVOAvatar::flushPendingMeshVisibility();
            avatar.readVisibility();
        }
        CHECK(T_CLEAN, avatar.walks() == before);
        CHECK(T_CLEAN, LLCharacter::sInstances.mWalks == list_walks);
        body.setTEID(0, 1);
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_CLEAN, LLCharacter::sInstances.mWalks == list_walks + 1 && avatar.walks() == before + 1);
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_CLEAN, LLCharacter::sInstances.mWalks == list_walks + 1 && avatar.walks() == before + 1);
    }

    // T_ORDER
    {
        gSwitch = true;
        LLVOAvatar avatar;
        LLViewerObject body;
        body.mTEs.resize(2);
        avatar.attach(&body);
        // a reader between the request and the frame walk sees the new state
        body.setTEID(0, 1);
        CHECK(T_ORDER, avatar.mJoints[MESH_ID_HEAD].mVisible); // not walked yet
        CHECK(T_ORDER, ((avatar.readVisibility() >> MESH_ID_HEAD) & 1u) == 0);
        // an explicit hide after a request is not undone by the late walk
        body.setTEID(1, 2);
        avatar.hideSkirt();
        CHECK(T_ORDER, !avatar.isMeshVisibilityDirty()); // the hide ran the pending walk first
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_ORDER, !avatar.mJoints[MESH_ID_SKIRT].mVisible && !avatar.mJoints[MESH_ID_UPPER_BODY].mVisible);
        body.setTEID(1, 3);
        avatar.hideHair();
        CHECK(T_ORDER, !avatar.isMeshVisibilityDirty());
        LLVOAvatar::flushPendingMeshVisibility();
        CHECK(T_ORDER, !avatar.mJoints[MESH_ID_HAIR].mVisible && !avatar.mJoints[MESH_ID_LOWER_BODY].mVisible);
        CHECK(T_ORDER, avatar.mJoints[MESH_ID_UPPER_BODY].mVisible);
    }

    if (gAssertFailures) { failed |= T_ASSERT; }
    if (LLVOAvatar::getMeshVisibilityDirtyCount() != 0) { failed |= T_DELETE; }
#undef CHECK
    return failed;
}
} // namespace VARIANT_NAME
// VARIANT_END

int main()
{
    // VARIANT_CALLS
    return 0;
}
