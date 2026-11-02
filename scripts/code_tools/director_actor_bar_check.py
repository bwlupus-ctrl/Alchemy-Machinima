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
    save = method(nv / "lldirectorcast.cpp", "LLSD LLDirectorCast::sceneData() const")
    load = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::applySceneData(")
    assert 'data["stepped_actor"] = getSteppedActorId()' in save
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
    assert 'cast.replaceActorId(old_id, destination)' in consumer and 'cast.replaceActorId(stable_id, destination)' in consumer
    assert 'cast.remove(' not in consumer and 'mWasDirectorSubject' not in consumer
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
    static const LLUUID null;
};
const LLUUID LLUUID::null{};
const LLUUID gAgentID{1};
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
    int mCastRevision = 0, mGazeCueRevision = 0, mSubjectRevision = 0;
    void rebuildMemberIndex() {}
    void touchScene() {}
    void remove(LLUUID id) { mCast.erase(std::remove_if(mCast.begin(),mCast.end(),[&](const CastMember& m){return m.mId==id;}),mCast.end()); }
    void replaceActorId(const LLUUID&, const LLUUID&);
    uuid_vec_t membersInGroup(const std::string&) const;
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
    fixture += r"""
struct LLVOAvatar {
    bool dead = false, control = false, ghost = false, mIsDummy = false;
    bool isDead() const { return dead; }
    bool isControlAvatar() const { return control; }
    bool isGhostAvatar() const { return ghost; }
};
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
    fixture += method(nv / "lldirectorcast.cpp", "uuid_vec_t LLDirectorCast::membersInGroup(")
    fixture += r'''
using S32 = int;
struct Value { int number = 0; std::string name; int asInteger() const { return number; } std::string asString() const { return name; } };
struct Combo { Value value; Value getValue() const { return value; } };
struct AgentAvatar { LLUUID getID() const { return gAgentID; } } agentAvatar;
AgentAvatar* gAgentAvatarp = &agentAvatar;
bool isAgentAvatarValid() { return true; }
std::vector<std::pair<LLUUID,int>> gazeWrites;
constexpr int GAZE_PRESET_COUNT = 24;
void applyGazePresetBlended(const LLUUID& actor, int preset) { gazeWrites.emplace_back(actor,preset); }
struct ALPanelLensGaze {
    Combo *mApplyTo = nullptr, *mApplyGroup = nullptr, *mPerformance = nullptr;
    bool mEditAll = false;
    LLUUID activeSlotActor() const { return LLDirectorCast::instance().getActiveActorId(); }
    uuid_vec_t editActors() const;
    void onPerformanceCommit();
};
'''
    fixture += method(nv / "alpanellensgaze.cpp", "uuid_vec_t ALPanelLensGaze::editActors(")
    fixture += method(nv / "alpanellensgaze.cpp", "void ALPanelLensGaze::onPerformanceCommit(")
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
    Combo scope, group, performance;
    scope.value.number = 1; group.value.name = "ensemble"; performance.value.number = 5;
    ALPanelLensGaze gaze; gaze.mApplyTo=&scope; gaze.mApplyGroup=&group; gaze.mPerformance=&performance;
    assert(gaze.editActors() == uuid_vec_t({{80},{2}}));
    gaze.onPerformanceCommit();
    assert(gazeWrites.size() == 2 && gazeWrites[0] == std::make_pair(LLUUID{80},5) && gazeWrites[1] == std::make_pair(LLUUID{2},5));
    group.value.name = "removed"; assert(gaze.editActors().empty());
    scope.value.number = 0; assert(gaze.editActors() == uuid_vec_t{{80}});
    std::cout << "PASS: full parked-clone authored state, identity, subjects and uniqueness; Gaze existing-group broadcast\n";
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
