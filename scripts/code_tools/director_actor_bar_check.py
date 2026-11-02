"""Exercise the production session actor setter and Cast synchronization without a viewer build."""
from pathlib import Path
import os
import re
import subprocess
import xml.etree.ElementTree as ET

from relight_area_policy_check import method


def main():
    nv = Path("indra/newview")
    xui = nv / "skins/default/xui/en"
    removed = {"anim_speed_avatar", "gaze_edit_actor", "sa_actor_target", "actor_style_target"}
    index = (xui / "director_kit_index.xml").read_text(encoding="utf-8")
    assert all("\t" + name + "\t" not in index for name in removed)
    shell = ET.parse(xui / "floater_director_v2.xml").getroot()
    bar = shell.find(".//*[@name='kit_actor_bar']")
    assert bar is not None
    assert {"kit_actor_" + name for name in ("name", "status", "choose", "selected", "you", "clear")} <= {
        node.get("name") for node in bar}
    for path in xui.glob("panel_dpage_*.xml"):
        tree = ET.parse(path).getroot()
        assert not removed & {node.get("widget") for node in tree.iter("kit_row")}
    preserved = {"panel_dpage_performance_gaze.xml": "gaze_cast_combo",
                 "panel_dpage_cast_path.xml": "copy_to_combo",
                 "panel_dpage_cast_move.xml": "scope_radio",
                 "panel_dpage_look_stylize.xml": "ri_subject_targetset_combo",
                 "panel_dpage_look_tron.xml": "tr_subject_target_set_combo"}
    for path, widget in preserved.items():
        assert ET.parse(xui / path).find(".//*[@widget='" + widget + "']") is not None
    independent = {"panel_dpage_look_stylize.xml": "ri_subject_target_combo",
                   "panel_dpage_look_tron.xml": "tr_subject_target_combo",
                   "panel_dpage_world_shadows_probes.xml": "sd_subject_target_combo"}
    for path, widget in independent.items():
        assert ET.parse(xui / path).find(".//*[@widget='" + widget + "']") is not None
        assert "\t" + widget + "\t" in index
    import subprocess
    for path in ("pipeline.cpp", "pipeline.h", "alfloaterlightbox.cpp"):
        assert (nv / path).read_bytes().replace(b"\r\n", b"\n") == subprocess.check_output(
            ["git", "show", "c5dd757d341:indra/newview/" + path]).replace(b"\r\n", b"\n"), path
    # D1 includes the subject display adapter, not just rows and renderer reads.
    kit = (nv / "alkitpage.cpp").read_text()
    base_kit = subprocess.check_output(["git", "show", "c5dd757d341:indra/newview/alkitpage.cpp"]).decode("utf-8")
    def subject_adapter(source):
        return source[source.index('    struct ComboSpec'):source.index('\n    return true;', source.index('    struct ComboSpec'))].replace('\r\n', '\n')
    assert subject_adapter(kit) == subject_adapter(base_kit)
    save = method(nv / "lldirectorcast.cpp", "LLSD LLDirectorCast::sceneData() const")
    load = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::applySceneData(")
    assert 'data["stepped_actor"] = getSteppedActorId()' in save
    assert 'LLActorMover::instance().pathSceneData(m.mId)' in save
    assert 'data.has("stepped_actor")' in load and 'data["stepped_actor"].asUUID(), scene_actor_ids' in load
    assert 'if (has_stepped_actor) setActiveActorId(isSelf(stepped_actor) ? gAgentID : stepped_actor)' in load
    assert 'if (!has_stepped_actor && !isSelf(getActiveActorId()) && !contains(getActiveActorId()))' in load
    runtime = (nv / "alsteppedanimruntime.cpp").read_text()
    assert 'const LLUUID selected = cast.getSteppedActorId()' in runtime
    assert 'f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected' in runtime
    assert 'return runtimeActorId(mActiveActorId)' in (nv / "lldirectorcast.h").read_text()
    group = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::onGroupsListSelect(")
    assert group.index('selectMultiple(cast.membersInGroup(name))') < group.index('cast.setActiveActorId(firstSelectedCastId())')
    assert 'mShell->setCastSelection(selectedCastIds())' in group
    chooser = (nv / "alavatarsubjectpicker.cpp").read_text()
    assert 'if (animesh)\n        for (const auto& clone' in chooser
    assert 'clone.mKind == ALGhostStudio::BACKING_ENTITY_CLONE && clone.mEntityId.notNull()' in chooser
    assert 'if (ghosts)\n        for (const auto& clone' not in chooser
    sync = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor(")
    assert 'setCastSelection' not in sync # independent Light Rig adapters never receive a bar mirror
    setter = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId(")
    assert 'add(actor)' not in setter
    speed_context = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::showAnimSpeed(")
    assert 'setActiveActorId' not in speed_context and 'mAnimSpeedContextLabel' in speed_context
    gaze = ET.parse(xui / "panel_dpage_performance_gaze.xml")
    assert gaze.find(".//*[@widget='gaze_apply_to']") is not None
    assert gaze.find(".//*[@widget='gaze_apply_group']") is not None
    preset = method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onPerformanceCommit(")
    assert 'for (const LLUUID& actor : editActors())' in preset and 'applyGazePresetBlended(actor' in preset
    for signature in ("LLSD LLDirectorCast::sceneData() const", "void LLDirectorCast::applySceneData("):
        body = method(nv / "lldirectorcast.cpp", signature)
        assert "mActiveActorId" not in body
    studio = (nv / "alghoststudio.cpp").read_text()
    consumer = studio[studio.index('mRuntimeConsumers.emplace_back('):studio.index('mRuntimeConsumers.emplace_back(', studio.index('mRuntimeConsumers.emplace_back(') + 1)]
    assert 'cast.replaceActorId(old_id.notNull() ? old_id : stable_id, destination)' in consumer
    assert 'removing_instance ? LLUUID::null : new_id.notNull() ? new_id : stable_id' in consumer
    assert 'cast.remove(' not in consumer and 'mWasDirectorSubject' not in consumer
    rekey = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(")
    assert rekey.index('mover.migrateActor(old_id, new_id)') < rekey.index('CastMember saved = *old') < rekey.index('touchScene()')
    assert 'mParkedWalkKeys' not in (nv / "llactormover.cpp").read_text()
    gaze_draw = method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::draw(")
    assert 'cast.getCastRevision()' in gaze_draw and 'refreshEditActorCombo()' in gaze_draw
    assert 'refreshApplyGroupCombo()' in method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::refreshEditActorCombo(")
    stamp = method(nv / "llfloaterdirector.cpp", "LLFloaterDirector::SceneChangeStamp LLFloaterDirector::currentSceneStamp(")
    assert 'stamp.mCast = LLDirectorCast::instance().getSceneRevision()' in stamp
    autosave = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::idleSceneRecovery(")
    assert autosave.index('updateSceneDirty()') < autosave.index('if (!service.mSceneDirty')
    styling = method(nv / "llfloaterdirector.cpp", "bool LLFloaterDirector::hasActorStyleTarget(")
    assert "getMember(actor)" in styling and "isGhostAvatar" not in styling
    selection = method(nv / "alavatarsubjectpicker.cpp", "LLVOAvatar* fromSelection(")
    assert "!object->isAttachment()" in selection and "fromObject(object)" in selection
    roster = method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::refreshCastList(")
    assert "mCastList->selectMultiple(previous_selection)" in roster
    layout = method(nv / "alkitshell.cpp", "void ALKitShell::layoutChrome(")
    widths = [int(re.search(name + r"->reshape\((\d+), 26", layout)[1])
              for name in ("mActorClear", "mActorYou", "mActorSelected", "mActorChoose")]
    assert 850 - 8 - sum(widths) - 4 * 4 - 56 >= 120
    assert "getRect().getHeight() - ACTOR_H" in layout
    assert 'constexpr S32 ACTOR_H = 28' in layout and bar.get('height') == '28'
    count = len(list(shell.iter('kit_shell.rail_item')))
    assert count > 0
    for height in (590, 600, 620, 646, 720):
        compact = 40 - (28 + count) // (count + 1)
        assert compact >= 36
        assert height - 28 - 40 - (count + 1) * compact >= height - 40 - (count + 1) * 40
    for path in ("panel_dpage_cast_move.xml", "panel_dpage_cast_path.xml", "panel_dpage_performance_animate.xml",
                 "panel_dpage_performance_gaze.xml", "panel_dpage_look_stepped.xml", "panel_dpage_look_actor_style.xml"):
        page = next(ET.parse(xui / path).getroot().iter("kit_page"))
        assert page.get("actor_target") is None and page.get("who_picker") is None
    fixture = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <map>
struct LLUUID {
    int value = 0;
    bool notNull() const { return value != 0; }
    bool isNull() const { return !notNull(); }
    bool operator<(LLUUID x) const { return value < x.value; }
    bool operator==(LLUUID x) const { return value == x.value; }
    bool operator!=(LLUUID x) const { return !(*this == x); }
    std::string asString() const { return std::to_string(value); }
    static const LLUUID null;
};
const LLUUID LLUUID::null{};
const LLUUID gAgentID{1};
std::ostream& operator<<(std::ostream& out, LLUUID id) { return out << id.value; }
#define LL_INFOS(x) std::cout
#define LL_ENDL std::endl
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(x)
struct LLVOAvatar {
    bool dead = false, control = false, ghost = false, mIsDummy = false;
    bool isDead() const { return dead; }
    bool isControlAvatar() const { return control; }
    bool isGhostAvatar() const { return ghost; }
    void* getRootJoint() const { return nullptr; }
};
LLVOAvatar* resolve_actor(LLUUID) { return nullptr; }
namespace ALGazeMotor { struct GazeMotorState {}; }
struct LLActorMover {
    struct Handle { void clear() {} };
    struct Move {
        int mDwellNode = -1, mEndMode = 0;
        bool mSuspended = true, mIsPath = true, mArrived = false;
        float mSpeed = 1, mT = 4, mDistance = 9;
        LLUUID mDwellAnim, mAnim;
        Handle mLocomotion, mSecondaryLocomotion, mDwellMotion;
    };
    struct Gaze { ALGazeMotor::GazeMotorState mGazeMotor; int authored = 0; };
    struct Follow { LLUUID mLeader; };
    std::map<LLUUID, Move> mMoves;
    std::map<LLUUID, double> mPaths;
    std::map<LLUUID, std::vector<double>> mHistory;
    std::map<LLUUID, Gaze> mGazes;
    std::map<LLUUID, Follow> mFollows;
    std::map<LLUUID, int> mGhostImpostors;
    int mGazeRevision = 0;
    LLUUID mEditActor;
    static LLActorMover& instance() { static LLActorMover mover; return mover; }
    void stopLocomotion(LLVOAvatar*, Move&) {}
    void startLocomotion(LLVOAvatar*, Move&) {}
    void startLocalMotion(LLVOAvatar*, Handle&, LLUUID, bool, float, bool) {}
    void invalidateGazeCache(Gaze&, bool) {}
    void migrateActor(const LLUUID&, const LLUUID&);
    void dropActor(LLUUID id) { mPaths.erase(id); mHistory.erase(id); mMoves.erase(id); mGazes.erase(id); mFollows.erase(id); }
};
using uuid_vec_t = std::vector<LLUUID>;
using SceneActorIdRemap = std::map<LLUUID, LLUUID>;
struct SceneValue {
    LLUUID value;
    SceneValue& operator=(LLUUID id) { value=id; return *this; }
    LLUUID asUUID() const { return value; }
};
struct Scene : std::map<std::string, SceneValue> {
    bool has(const std::string& key) const { return count(key)!=0; }
    const SceneValue& operator[](const std::string& key) const { return at(key); }
    SceneValue& operator[](const std::string& key) { return std::map<std::string, SceneValue>::operator[](key); }
};
struct LLDirectorCast {
    struct CastMember {
        LLUUID mId; std::string mLastName, mGroup;
        int mActorStyle = 0, mGazeTarget = 0, mMark = 0;
        std::vector<int> mGazeCues, mGazeInfluenceKeys;
    };
    std::vector<CastMember> mCast;
    uuid_vec_t mIds;
    std::set<LLUUID> mLookAtCameraIds;
    LLUUID mSubjectA, mSubjectB, mSubjectC, mSubjectD;
    int mCastRevision = 0, mGazeCueRevision = 0, mSubjectRevision = 0, mSceneRevision = 0;
    struct PendingStart { LLUUID mId; };
    std::vector<PendingStart> mPendingStarts;
    std::map<std::string, float> mGroupDelays;
    std::map<std::string, int> mGroupRates;
    void rebuildMemberIndex() {}
    void touchScene() { ++mSceneRevision; }
    void remove(LLUUID id) { mCast.erase(std::remove_if(mCast.begin(),mCast.end(),[&](const CastMember& m){return m.mId==id;}),mCast.end()); }
    void replaceActorId(const LLUUID&, const LLUUID&);
    uuid_vec_t membersInGroup(const std::string&) const;
    CastMember* getMember(LLUUID id) { for(auto& m:mCast) if(m.mId==id) return &m; return nullptr; }
    void pruneGroupData(const std::string&) {}
    void setGroup(const LLUUID&, const std::string&);
    bool renameGroup(const std::string&, const std::string&);
    bool dissolveGroup(const std::string&);
    std::vector<std::string> getGroupNames() const;
    const uuid_vec_t& getIds() const { return mIds; }
    int getCastRevision() const { return mCastRevision; }
    struct GazeTarget { LLUUID mCastRef; };
    std::map<LLUUID, GazeTarget> targets, eyeTargets;
    GazeTarget getGazeTarget(LLUUID id) const { auto it=targets.find(id); return it==targets.end()?GazeTarget{}:it->second; }
    bool hasEyeGazeTarget(LLUUID id) const { return eyeTargets.count(id)!=0; }
    GazeTarget getEyeGazeTarget(LLUUID id) const { return eyeTargets.at(id); }
    LLUUID getSubjectA() const { return mSubjectA; }
    LLUUID getSubjectB() const { return mSubjectB; }
    LLUUID getSubjectC() const { return mSubjectC; }
    LLUUID getSubjectD() const { return mSubjectD; }
    LLUUID mActiveActorId;
    uuid_vec_t members;
    std::function<void()> changed;
    int events = 0;
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    static bool isSelf(const LLUUID& id) { return id.isNull() || id == gAgentID; }
    static LLUUID runtimeActorId(const LLUUID& id);
    static LLUUID canonicalActorId(LLUUID id) { return id.value == 101 ? LLUUID{1} : id; }
    void add(LLUUID id) { if (std::find(members.begin(),members.end(),id)==members.end()) members.push_back(id); }
    const LLUUID& getActiveActorId() const { return mActiveActorId; }
    void mActiveActorChanged() { ++events; if (changed) changed(); }
    void setActiveActorId(const LLUUID&);
    LLUUID getSteppedActorId() const { return runtimeActorId(mActiveActorId); }
    void migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids);
    void saveStepped(Scene& data) const;
};
struct List {
    uuid_vec_t rows, selected;
    void deselectAllItems() { selected.clear(); }
    void selectMultiple(uuid_vec_t ids) { selected.clear(); for (auto id : ids) selectByID(id); }
    void selectByID(LLUUID id) { if (std::find(rows.begin(),rows.end(),id)!=rows.end()) selected.push_back(id); }
};
struct Shell { uuid_vec_t selected; void setCastSelection(const uuid_vec_t& ids) { selected = ids; } };
struct LLFloaterDirector {
    List list; Shell shell;
    List* mCastList = &list; Shell* mShell = &shell;
    struct Clock { void forceDue() {} } mAnimSpeedClock;
    LLUUID mAnimSpeedSelection, pathActor, mPrimaryCastActor;
    std::string mAnimSpeedContextLabel;
    bool mSyncingActiveActor = false;
    bool mActorStyleHadTarget = true, mActorStyleTabDirty = false, mAnimateTabDirty = false;
    uuid_vec_t gaze;
    LLUUID firstSelectedCastId() const { if (mPrimaryCastActor.notNull() && std::find(list.selected.begin(),list.selected.end(),mPrimaryCastActor)!=list.selected.end()) return mPrimaryCastActor; return list.selected.empty() ? LLUUID{} : list.selected.front(); }
    uuid_vec_t selectedCastIds() const { return list.selected; }
    void refreshCastList() { list.rows = LLDirectorCast::instance().members; }
    void retargetPathFromCastSelection() { pathActor = LLDirectorCast::instance().getActiveActorId(); }
    void refreshGazeSelection(const uuid_vec_t& ids) { gaze = ids; }
    void syncActiveActor();
};
'''
    fixture += method(nv / "llactormover.cpp", "void LLActorMover::migrateActor(")
    fixture += r"""
namespace ALSteppedAnim { void renameGroupRate(std::map<std::string,int>& rates, const std::string& old, const std::string& name) {
    if(rates.count(old)) { if(!rates.count(name)) rates[name]=rates[old]; rates.erase(old); }
} }
namespace ALAvatarSubjectPicker {
"""
    fixture += method(nv / "alavatarsubjectpicker.cpp", "bool valid(") + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID LLDirectorCast::runtimeActorId(")
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId(")
    fixture += method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor(")
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID canonicalSceneActorId(")
    # Execute the production compatibility statements against a minimal LLSD map.
    migration = re.search(r'    const bool has_stepped_actor = .*?LLUUID::null;', load, re.S)[0]
    restore = re.search(r'    if \(has_stepped_actor\) setActiveActorId\([^;]+;', load)[0]
    fixture += "void LLDirectorCast::migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids) {\n" + migration + restore + "\n}\n"
    write = re.search(r'    data\["stepped_actor"\] = getSteppedActorId\(\);', save)[0]
    fixture += "void LLDirectorCast::saveStepped(Scene& data) const {\n" + write + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(")
    transaction = consumer[consumer.index('            const LLUUID destination'):consumer.index('            if (new_id.notNull()')]
    fixture += "void cloneTransition(LLUUID stable_id, LLUUID old_id, LLUUID new_id, bool removing_instance=false) {\n    auto& cast=LLDirectorCast::instance();\n" + transaction + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "uuid_vec_t LLDirectorCast::membersInGroup(")
    for signature in ("void LLDirectorCast::setGroup(", "bool LLDirectorCast::renameGroup(",
                      "bool LLDirectorCast::dissolveGroup(", "std::vector<std::string> LLDirectorCast::getGroupNames("):
        fixture += method(nv / "lldirectorcast.cpp", signature)
    fixture += r'''
using S32 = int;
using U64 = unsigned long long;
struct Value {
    int number = 0; std::string name;
    Value() = default; Value(int n):number(n) {} Value(std::string s):name(s) {}
    int asInteger() const { return number; } std::string asString() const { return name; }
};
using LLSD = Value;
struct Combo {
    Value value; std::vector<std::string> rows; bool visible = false;
    Value getValue() const { return value; }
    void removeall() { rows.clear(); clear(); }
    void clear() { value={}; }
    void add(const std::string&, Value v) { rows.push_back(v.name); }
    bool setSelectedByValue(Value v, bool) { if(std::find(rows.begin(),rows.end(),v.name)==rows.end()) return false; value=v; return true; }
    void selectFirstItem() { if(!rows.empty()) value=rows.front(); }
    int getItemCount() const { return int(rows.size()); }
    int getCurrentIndex() const { auto it=std::find(rows.begin(),rows.end(),value.name); return it==rows.end()?-1:int(it-rows.begin()); }
    Combo* getParent() { return this; }
};
void setVisibleIfChanged(Combo* c, bool visible) { c->visible=visible; }
void setValueIfChanged(Combo* c, Value value, bool) { c->setSelectedByValue(value,true); }
bool isEditing(Combo*) { return false; }
std::string actorName(LLUUID id) { return id.asString(); }
struct AgentAvatar { LLUUID getID() const { return gAgentID; } } agentAvatar;
AgentAvatar* gAgentAvatarp = &agentAvatar;
bool isAgentAvatarValid() { return true; }
std::vector<std::pair<LLUUID,int>> gazeWrites;
constexpr int GAZE_PRESET_COUNT = 24;
void applyGazePresetBlended(const LLUUID& actor, int preset) { gazeWrites.emplace_back(actor,preset); }
struct ALPanelLensGaze {
    Combo *mApplyTo = nullptr, *mApplyGroup = nullptr, *mPerformance = nullptr;
    Combo *mCast = nullptr;
    std::vector<std::string> mLastApplyGroups;
    bool mEditingEyeTarget = false;
    U64 mCastComboRevision = 0;
    LLUUID mCastComboActor, mCastComboSelection;
    bool mEditAll = false;
    LLUUID activeSlotActor() const { return LLDirectorCast::instance().getActiveActorId(); }
    uuid_vec_t editActors() const;
    LLUUID editDisplayActor() const;
    void refreshCastCombo();
    void refreshApplyGroupCombo();
    void onPerformanceCommit();
};
'''
    fixture += method(nv / "alpanellensgaze.cpp", "uuid_vec_t ALPanelLensGaze::editActors(")
    for signature in ("LLUUID ALPanelLensGaze::editDisplayActor(", "void ALPanelLensGaze::refreshCastCombo(",
                      "void ALPanelLensGaze::refreshApplyGroupCombo("):
        fixture += method(nv / "alpanellensgaze.cpp", signature)
    fixture += method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onPerformanceCommit(")
    fixture += r'''
struct LLFloater {};
struct NullHandle { LLFloater* get() const { return nullptr; } } sRecoveryDirectorHandle;
template<class T> void setTextIfChanged(T*, const char*) {}
template<class T> struct LLCachedControl { T value; LLCachedControl(int, const char*, T v):value(v) {} operator T() const { return value; } };
int gSavedSettings=0;
using F32=float;
struct RecoveryProbe : LLFloater {
    struct Timer { float elapsed=0; void reset() { elapsed=0; } float getElapsedTimeF32() const { return elapsed; } };
    struct RecoveryServiceState { bool mHaveCleanSceneStamp=true, mSceneDirty=false; int mCleanSceneStamp=0; Timer mAutosaveTimer; };
    void* mSceneDirtyText=nullptr;
    static RecoveryServiceState& recoveryServiceState() { static RecoveryServiceState service; return service; }
    static int currentSceneStamp() { return LLDirectorCast::instance().mSceneRevision; }
    static void markSceneClean() { auto& s=recoveryServiceState(); s.mCleanSceneStamp=currentSceneStamp(); s.mHaveCleanSceneStamp=true; s.mSceneDirty=false; }
    static void updateSceneDirty();
    static void idleSceneRecovery(bool force_write);
    static int writes;
    static Scene saved;
};
int RecoveryProbe::writes=0;
Scene RecoveryProbe::saved;
'''
    fixture += method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::updateSceneDirty(").replace("LLFloaterDirector", "RecoveryProbe")
    # Execute the unmodified autosave entry/eligibility gate. Only its file I/O
    # is replaced with a payload capture; currentSceneStamp's Cast link is
    # independently asserted above.
    entry = autosave[:autosave.index('    // An oversized scene')].replace('LLFloaterDirector', 'RecoveryProbe')
    fixture += entry + '    ++writes; LLDirectorCast::instance().saveStepped(saved);\n}\n'
    fixture += r'''
int main() {
    LLVOAvatar resident, animesh, clone;
    animesh.control = animesh.mIsDummy = true;
    clone.ghost = clone.mIsDummy = true;
    assert(ALAvatarSubjectPicker::valid(&resident, false, false));
    assert(!ALAvatarSubjectPicker::valid(&animesh, true, false)); // camera filters unchanged
    assert(ALAvatarSubjectPicker::valid(&animesh, true, true)); // shell admits real control avatars
    assert(ALAvatarSubjectPicker::valid(&clone, true, false));
    assert(!ALAvatarSubjectPicker::valid(&clone, false, false));
    auto& cast = LLDirectorCast::instance(); LLFloaterDirector director;
    cast.changed = [&] { director.syncActiveActor(); };
    cast.add({2});
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.firstSelectedCastId() == LLUUID{2});
    cast.add({3}); director.refreshCastList(); director.list.selected = {{2},{3}};
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.list.selected.size() == 2); // same primary preserves batch
    cast.setActiveActorId({3});
    assert(cast.events == 2 && director.list.selected == uuid_vec_t({{2},{3}}) && director.firstSelectedCastId() == LLUUID{3}); // changing primary preserves every batch member
    director.list.selected = {{2},{3}};
    director.mPrimaryCastActor = {}; // explicit roster commit chooses its first row
    cast.setActiveActorId(director.firstSelectedCastId());
    assert(cast.events == 3 && director.list.selected.size() == 2); // Cast publishes its first row
    cast.setActiveActorId({});
    assert(cast.events == 4 && director.list.selected == uuid_vec_t({{2},{3}}) && director.mAnimSpeedSelection == gAgentID); // Clear also preserves a live batch
    const auto members_before = cast.members;
    cast.setActiveActorId({101}); // control-avatar aliases resolve to persistent identity
    assert(cast.events == 5 && cast.getActiveActorId() == LLUUID{1});
    cast.setActiveActorId({1});
    assert(cast.events == 5 && director.pathActor == LLUUID{1}); // no feedback loop
    assert(cast.members == members_before); // You cannot re-enrol a removed self
    // N3: effective render selections dirty clean scenes and reach both timed
    // and forced autosave; explicit You and Clear do not create false changes.
    RecoveryProbe::markSceneClean();
    const int self_revision=cast.mSceneRevision;
    cast.setActiveActorId({});
    assert(cast.mSceneRevision==self_revision);
    RecoveryProbe::idleSceneRecovery(true);
    assert(RecoveryProbe::writes==0);
    cast.setActiveActorId({2});
    assert(cast.mSceneRevision>self_revision);
    RecoveryProbe::updateSceneDirty();
    assert(RecoveryProbe::recoveryServiceState().mSceneDirty);
    RecoveryProbe::idleSceneRecovery(false);
    assert(RecoveryProbe::writes==0);
    RecoveryProbe::recoveryServiceState().mAutosaveTimer.elapsed=301;
    RecoveryProbe::idleSceneRecovery(false);
    assert(RecoveryProbe::writes==1 && RecoveryProbe::saved["stepped_actor"].asUUID()==LLUUID{2});
    RecoveryProbe::markSceneClean(); cast.setActiveActorId({3});
    RecoveryProbe::idleSceneRecovery(true);
    assert(RecoveryProbe::writes==2 && RecoveryProbe::saved["stepped_actor"].asUUID()==LLUUID{3});
    cast.setActiveActorId(gAgentID);
    assert(LLDirectorCast::runtimeActorId(gAgentID).isNull());
    // Execute the exact production Selected predicate; Mixed consumes this same flag.
    auto selected_predicate = [](LLUUID selected, LLUUID actor_id, bool is_self) {
        struct { bool isSelf; bool isSelected; } f{is_self, false};
        f.isSelected = selected.isNull() ? f.isSelf : actor_id == selected;
        return f.isSelected;
    };
    const auto self = LLDirectorCast::runtimeActorId(gAgentID);
    assert(selected_predicate(self, LLUUID{}, true));
    assert(!selected_predicate(self, LLUUID{2}, false));
    assert(selected_predicate(LLUUID{2}, LLUUID{2}, false));
    assert(!selected_predicate(LLUUID{2}, LLUUID{}, true));
    cast.changed = {}; // isolate clone lifecycle from the UI mock
    Scene legacy, saved;
    legacy["stepped_actor"] = LLUUID{100};
    cast.migrateStepped(legacy, {{{100},{80}}});
    assert(cast.getActiveActorId() == LLUUID{80});
    cast.saveStepped(saved);
    assert(saved.has("stepped_actor") && saved["stepped_actor"].asUUID() == LLUUID{80});
    legacy["stepped_actor"] = LLUUID{};
    cast.migrateStepped(legacy, {});
    assert(cast.getActiveActorId() == gAgentID && cast.getSteppedActorId().isNull());
    cast.saveStepped(saved);
    assert(saved.has("stepped_actor") && saved["stepped_actor"].asUUID().isNull());
    cast.setActiveActorId({4}); cast.migrateStepped(Scene{}, {});
    assert(cast.getActiveActorId() == LLUUID{4}); // legacy field absent retains the session choice
    // N1: the actual Studio destination expression and central rekey execute
    // with the production mover migration, including an unselected despawn.
    auto& mover=LLActorMover::instance();
    LLDirectorCast::CastMember pathActor; pathActor.mId={150}; pathActor.mMark=12;
    pathActor.mActorStyle=48; pathActor.mGazeCues={9,8}; pathActor.mGazeInfluenceKeys={6,5};
    cast.mCast={pathActor};
    cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD={150};
    cast.setActiveActorId({2});
    mover.mPaths[{150}]=1.0; mover.mHistory[{150}]={1.0}; mover.mMoves[{150}]={};
    mover.mGazes[{150}].authored=17; mover.mFollows[{151}].mLeader={150}; mover.mEditActor={150};
    cloneTransition({160},{150},{});
    assert(cast.mCast.front().mId==LLUUID{160} && mover.mPaths.at({160})==1.0);
    assert(!mover.mPaths.count({150}) && mover.mHistory.at({160})==std::vector<double>{1.0});
    assert(mover.mEditActor==LLUUID{160} && mover.mMoves.at({160}).mT==4);
    mover.mPaths[{160}]=2.5; mover.mHistory[{160}].push_back(2.5); // edit while parked
    assert(mover.mPaths.at(cast.mCast.front().mId)==2.5); // same key sceneData serializes
    cast.setActiveActorId({160}); cast.mCast.front().mActorStyle=49;
    cloneTransition({160},{},{170});
    assert(cast.mCast.size()==1 && cast.mCast.front().mId==LLUUID{170});
    assert(cast.mCast.front().mMark==12 && cast.mCast.front().mActorStyle==49);
    assert(cast.mCast.front().mGazeCues==pathActor.mGazeCues && cast.mCast.front().mGazeInfluenceKeys==pathActor.mGazeInfluenceKeys);
    assert(mover.mPaths.size()==1 && mover.mPaths.at({170})==2.5 && !mover.mHistory.count({160}));
    assert(mover.mHistory.at({170})==std::vector<double>({1.0,2.5}));
    assert(mover.mMoves.at({170}).mT==4 && mover.mGazes.at({170}).authored==17);
    assert(mover.mEditActor==LLUUID{170} && mover.mFollows.at({151}).mLeader==LLUUID{170});
    assert(cast.getSteppedActorId()==LLUUID{170});
    assert(cast.mSubjectA==LLUUID{170} && cast.mSubjectB==LLUUID{170} && cast.mSubjectC==LLUUID{170} && cast.mSubjectD==LLUUID{170});
    cast.setActiveActorId({180}); // selection outside Cast must also clear on final removal
    cloneTransition({180},{},{},true);
    assert(cast.getActiveActorId().isNull());
    LLDirectorCast::CastMember authored;
    authored.mId = {50}; authored.mGroup = "ensemble"; authored.mLastName = "Parked clone";
    authored.mActorStyle = 77; authored.mGazeTarget = 31; authored.mMark = 9;
    authored.mGazeCues = {1,4,8}; authored.mGazeInfluenceKeys = {3,7};
    LLDirectorCast::CastMember duplicate; duplicate.mId = {60};
    cast.mCast = {authored, duplicate}; cast.mSubjectA = {50}; cast.mSubjectD = {50};
    cast.setActiveActorId({50});
    cast.replaceActorId({50}, {60}); // disappearance parks the entire member
    cast.mCast.front().mActorStyle = 99; // newly authored while parked
    cast.setActiveActorId({2}); // respawn must also work after selecting somebody else
    cast.replaceActorId({60}, {70});
    assert(cast.mCast.size() == 1 && cast.mCast.front().mId == LLUUID{70});
    const auto& restored = cast.mCast.front();
    assert(restored.mActorStyle == 99 && restored.mGazeTarget == 31 && restored.mMark == 9);
    assert(restored.mGazeCues == authored.mGazeCues && restored.mGazeInfluenceKeys == authored.mGazeInfluenceKeys);
    assert(restored.mGroup == "ensemble" && cast.mSubjectA == LLUUID{70} && cast.mSubjectD == LLUUID{70});
    cast.setActiveActorId({70});
    cast.mSubjectD = {}; // an explicit unassignment while parked must survive recovery
    cast.replaceActorId({70}, {80});
    assert(cast.getActiveActorId() == LLUUID{80} && cast.mCast.size() == 1 && cast.mCast.front().mActorStyle == 99);
    assert(cast.mSubjectA == LLUUID{80} && cast.mSubjectD.isNull());
    LLDirectorCast::CastMember second; second.mId = {2}; second.mGroup = "ensemble";
    LLDirectorCast::CastMember other; other.mId = {3}; other.mGroup = "other";
    cast.mCast.push_back(second); cast.mCast.push_back(other);
    Combo scope, group, performance, castPicker;
    scope.value.number = 1; group.value.name = "ensemble"; performance.value.number = 5;
    ALPanelLensGaze gaze; gaze.mApplyTo=&scope; gaze.mApplyGroup=&group; gaze.mPerformance=&performance;
    gaze.mCast=&castPicker;
    assert(gaze.editActors() == uuid_vec_t({{80},{2}}));
    gaze.onPerformanceCommit();
    assert(gazeWrites.size() == 2 && gazeWrites[0] == std::make_pair(LLUUID{80},5) && gazeWrites[1] == std::make_pair(LLUUID{2},5));
    // N2: Group readback and filtering follow A, with bar X outside the group.
    cast.setActiveActorId({3}); cast.mIds={{80},{2},{3}};
    cast.targets[{3}].mCastRef={80}; cast.targets[{80}].mCastRef={2};
    gaze.refreshCastCombo();
    assert(castPicker.value.name=="2");
    assert(castPicker.rows==std::vector<std::string>({"2","3"})); // X remains a valid target
    cast.eyeTargets[{80}].mCastRef={3}; gaze.mEditingEyeTarget=true;
    gaze.refreshCastCombo(); assert(castPicker.value.name=="3");
    cast.setActiveActorId({2}); gaze.refreshCastCombo();
    assert(castPicker.value.name=="3" && castPicker.rows==std::vector<std::string>({"2","3"}));
    // N4: execute group mutations and the real combo rebuild; names need not
    // change for membership/representative invalidation to be required.
    gaze.refreshApplyGroupCombo(); assert(group.value.name=="ensemble");
    int revision=cast.mCastRevision;
    cast.setGroup({80},"new"); assert(cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(std::find(group.rows.begin(),group.rows.end(),"new")!=group.rows.end());
    revision=cast.mCastRevision;
    cast.setGroup({80},"ensemble"); assert(cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(group.value.name=="ensemble");
    revision=cast.mCastRevision;
    assert(cast.renameGroup("other","renamed") && cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo(); assert(std::find(group.rows.begin(),group.rows.end(),"renamed")!=group.rows.end());
    revision=cast.mCastRevision;
    assert(cast.dissolveGroup("ensemble") && cast.mCastRevision>revision);
    gaze.refreshApplyGroupCombo();
    assert(group.value.name.empty() && gaze.editActors().empty()); // no substitution of "renamed"
    assert(group.rows==std::vector<std::string>{"renamed"});
    group.value.name = "removed"; assert(gaze.editActors().empty());
    cast.setActiveActorId({80});
    scope.value.number = 0; assert(gaze.editActors() == uuid_vec_t{{80}});
    std::cout << "PASS: full parked-clone authored state, identity, subjects and uniqueness; Gaze existing-group broadcast\n";
    std::cout << "PASS: parked path/history/move edits survive recovery; Group Head/Eyes picker; dirty/timed/forced autosave; group mutation refresh and removal\n";
    std::cout << "PASS: session actor, canonical identity, two-way Cast sync, batch preservation, Clear speed default, no feedback loop\n";
}
'''
    directory = Path(".tmp/actorbar_20261101/contract").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "actor.cpp").write_text(fixture, encoding="utf-8")
    vcvars = next((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob(
        "*/Community/VC/Auxiliary/Build/vcvars64.bat"))
    (directory / "compile.cmd").write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "actor.obj"}" '
        f'/Fe"{directory / "actor.exe"}" "{directory / "actor.cpp"}\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(directory / "compile.cmd")], check=True)
    subprocess.run([str(directory / "actor.exe")], check=True)
    print("PASS: picker/search contracts, independent renderer subjects, legacy Stepped scene round-trip, group publication, despawned-clone filter, compact rail geometry")


if __name__ == "__main__":
    main()
