// Viewer-free fixtures for the REAL alavataranimspeed.h/.cpp implementation.
// The Python runner appends that implementation (only includes removed), then
// appends main(). These small interfaces replace objects that need a login.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>
using F32 = float;
using F64 = double;
using S32 = int;
using U64 = uint64_t;
using U8 = uint8_t;
struct LLUUID
{
    std::string value;
    LLUUID(const char* text = "") : value(text) {}
    bool isNull() const { return value.empty(); }
    bool notNull() const { return !isNull(); }
    bool operator<(const LLUUID& other) const { return value < other.value; }
    bool operator==(const LLUUID& other) const { return value == other.value; }
    bool operator!=(const LLUUID& other) const { return !(*this == other); }
};
struct LLSD
{
    enum Type { UNDEFINED, ARRAY, MAP, UUID, REAL, INTEGER } type = UNDEFINED;
    std::vector<LLSD> array;
    std::map<std::string, LLSD> map;
    LLUUID uuid;
    double number = 0;
    LLSD() = default;
    LLSD(LLUUID id) : type(UUID), uuid(id) {}
    LLSD(float value) : type(REAL), number(value) {}
    LLSD(double value) : type(REAL), number(value) {}
    LLSD(int value) : type(INTEGER), number(value) {}
    static LLSD emptyArray() { LLSD v; v.type = ARRAY; return v; }
    bool isArray() const { return type == ARRAY; }
    bool isMap() const { return type == MAP; }
    bool isUUID() const { return type == UUID; }
    bool isReal() const { return type == REAL; }
    bool isInteger() const { return type == INTEGER; }
    LLUUID asUUID() const { return uuid; }
    double asReal() const { return number; }
    bool has(const char* key) const { return map.count(key) != 0; }
    LLSD& operator[](const char* key) { type = MAP; return map[key]; }
    const LLSD& operator[](const char* key) const
    { static LLSD empty; auto it = map.find(key); return it == map.end() ? empty : it->second; }
    LLSD& with(const char* key, LLSD value) { (*this)[key] = value; return *this; }
    void append(LLSD value) { array.push_back(value); }
};
namespace llsd { const std::vector<LLSD>& inArray(const LLSD& v) { return v.array; } }
struct LLCharacter
{
    static inline std::vector<LLCharacter*> sInstances;
    virtual ~LLCharacter() = default;
};
struct LLControlAvatar;
struct LLVOAvatar;
struct LLViewerObject
{
    LLUUID id;
    LLControlAvatar* control = nullptr;
    bool local = false;
    bool isLocalOnly() const { return local; }
    bool isLocalMeshPreview() const { return false; }
    std::vector<LLViewerObject*> children;
    virtual ~LLViewerObject() = default;
    bool isDead() const { return false; }
    const LLUUID& getID() const { return id; }
    LLControlAvatar* getControlAvatar() { return control; }
    const auto& getChildren() const { return children; }
};
struct LLViewerJointAttachment { std::vector<LLViewerObject*> mAttachedObjects; };
struct LLVOAvatar : LLViewerObject, LLCharacter
{
    float factor = 1;
    int writes = 0;
    bool mIsDummy = false, control_avatar = false;
    std::map<int, LLViewerJointAttachment*> mAttachmentPoints;
    bool isControlAvatar() const { return control_avatar; }
    void setAnimTimeFactor(float value) { factor = value; ++writes; }
};
struct LLControlAvatar : LLVOAvatar
{
    LLVOAvatar* wearer = nullptr;
    LLViewerObject* mRootVolp = nullptr;
    LLVOAvatar* getAttachedAvatar() { return wearer; }
};
struct LLGhostAvatar : LLVOAvatar {};
struct ObjectList
{
    std::map<LLUUID, LLViewerObject*> objects;
    LLViewerObject* findObject(const LLUUID& id) const
    { auto it = objects.find(id); return it == objects.end() ? nullptr : it->second; }
} gObjectList;
LLUUID gAgentID("self");
struct Settings { bool getBOOL(const char*) { return false; } } gSavedSettings;
struct LLDirectorCast
{
    struct Member { LLUUID mId; };
    std::vector<Member> cast;
    int touches = 0;
    static bool instanceExists() { return true; }
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    const auto& getCast() { return cast; }
    void touchScene() { ++touches; }
};
struct ALGhostStudio
{
    enum { BACKING_ENTITY_CLONE, BACKING_OVERLAY };
    struct Instance { LLUUID mId, mEntityId; float mAnimSpeed = 1; int mKind = BACKING_ENTITY_CLONE; };
    std::vector<Instance> instances;
    int writes = 0;
    static ALGhostStudio& instance() { static ALGhostStudio studio; return studio; }
    const Instance* findInstanceByRuntime(const LLUUID& id)
    { for (auto& inst : instances) if (inst.mKind == BACKING_ENTITY_CLONE && inst.mEntityId == id) return &inst; return nullptr; }
    bool setInstanceAnimSpeed(const LLUUID& id, float speed)
    { for (auto& inst : instances) if (inst.mId == id) { inst.mAnimSpeed = speed; ++writes; return true; } return false; }
    const auto& getInstances() { return instances; }
};
// PRODUCTION_IMPLEMENTATION

int main()
{
    auto& owner = ALAvatarAnimSpeed::instance();
    auto& studio = ALGhostStudio::instance();
    LLVOAvatar self, cast, resident;
    self.id = "self"; cast.id = "cast"; resident.id = "resident";
    gObjectList.objects = {{self.id, &self}, {cast.id, &cast}, {resident.id, &resident}};
    LLCharacter::sInstances = {&self, &cast, &resident};
    LLDirectorCast::instance().cast = {{{"cast"}}, {{"absent"}}, {{"self"}}};
    owner.setSpeed(self.id, 99); assert(self.factor == 4);
    owner.setSpeed(self.id, -99); assert(self.factor == .05f);
    owner.setSlowMotion(true); assert(std::abs(self.factor - .01f) < 1e-6f);
    owner.setSpeed(self.id, 2); assert(self.factor == .4f);
    owner.setSpeed(self.id, 1); assert(self.factor == .2f);
    assert(owner.sceneData().array.empty()); // 1 erases, slow motion is separate
    owner.setSlowMotion(false); assert(self.factor == 1);
    // Dance Ball samples from its independent transport, not controller time.
    owner.setSpeed(self.id, .5f);
    ALDanceBallModel::Clock dance_clock;
    dance_clock.setSpeed(2);
    dance_clock.advance(1);
    assert(dance_clock.time() == 2 && self.factor == .5f);
    dance_clock.setSpeed(.25);
    dance_clock.advance(4);
    assert(dance_clock.time() == 3 && self.factor == .5f);
    owner.setSpeed(self.id, 1);
    owner.setSpeed(cast.id, .5f);
    owner.setSpeed(cast.id, std::numeric_limits<float>::quiet_NaN());
    assert(cast.factor == .5f);
    // Avatar recreation and both attachment/control creation orderings.
    LLVOAvatar recreated; recreated.id = "cast";
    gObjectList.objects[recreated.id] = &recreated;
    owner.apply(recreated); assert(recreated.factor == .5f);
    LLControlAvatar control; control.wearer = &recreated;
    owner.applyControl(control); assert(control.factor == .5f);
    LLViewerObject attachment; attachment.control = &control;
    LLViewerJointAttachment point; point.mAttachedObjects = {&attachment};
    recreated.mAttachmentPoints[1] = &point;
    control.factor = 1; owner.apply(recreated); assert(control.factor == .5f);
    control.wearer = nullptr; owner.applyControl(control); assert(control.factor == 1);
    // Vary all deduplicates self/cast/nearby and includes absent cast members.
    assert(owner.targets().size() == 4);
    for (float spread : {0.f, 15.f, 50.f})
    {
        owner.varyAll(spread);
        for (auto id : owner.targets()) assert(std::abs(owner.getSpeed(id) - 1) <= spread/100 + 1e-6f);
    }
    // Stored data round-trip, absent identities, old scenes, malformed atomicity.
    owner.setSpeed("absent", .75f);
    LLSD scene; scene["avatar_anim_speeds"] = owner.sceneData();
    owner.resetAll(); assert(owner.sceneData().array.empty());
    owner.applySceneData(scene); assert(owner.getSpeed("absent") == .75f);
    owner.applySceneData(LLSD()); assert(owner.getSpeed("absent") == .75f);
    LLSD malformed = scene; malformed["avatar_anim_speeds"].append(LLSD(2));
    owner.applySceneData(malformed); assert(owner.getSpeed("absent") == .75f);
    // Ghost UI routing calls Studio and never writes the ghost/controller.
    LLGhostAvatar ghost; ghost.id = "ghost";
    gObjectList.objects[ghost.id] = &ghost;
    studio.instances.push_back({"instance", "ghost", 1});
    owner.setSpeed(ghost.id, .25f);
    assert(studio.instances[0].mAnimSpeed == .25f && studio.writes == 1);
    assert(owner.getSpeed(ghost.id) == .25f && ghost.writes == 0);
    owner.apply(ghost); assert(ghost.writes == 0);
    control.wearer = &ghost; int writes = control.writes;
    owner.applyControl(control); assert(control.writes == writes);
    LLViewerObject local_root; local_root.local = true;
    control.mRootVolp = &local_root; control.wearer = nullptr;
    owner.applyControl(control); assert(control.writes == writes);
    for (auto entry : owner.sceneData().array) assert(entry["avatar"].asUUID() != ghost.id);
    LLSD clone_scene; clone_scene["avatar_anim_speeds"] = LLSD::emptyArray();
    clone_scene["avatar_anim_speeds"].append(LLSD().with("avatar", ghost.id).with("speed", 3.f));
    owner.applySceneData(clone_scene); assert(owner.getSpeed(ghost.id) == .25f);
    owner.resetAll(); assert(owner.getSpeed(ghost.id) == 1);
    assert(owner.sceneData().array.empty());
    std::cout << "PASS: real arbiter math, vary, reset, rez, late animesh, ghost routing, scene round-trip/legacy/validation, independent Dance Ball clock\n";
}
