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
    removed = {"anim_speed_avatar", "gaze_edit_actor", "sa_actor_target", "actor_style_target",
               "ri_subject_target_combo", "tr_subject_target_combo", "sd_subject_target_combo"}
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
    for signature in ("LLSD LLDirectorCast::sceneData() const", "void LLDirectorCast::applySceneData("):
        body = method(nv / "lldirectorcast.cpp", signature)
        assert "mActiveActorId" not in body
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
struct LLUUID {
    int value = 0;
    bool notNull() const { return value != 0; }
    bool isNull() const { return !notNull(); }
    bool operator==(LLUUID x) const { return value == x.value; }
    bool operator!=(LLUUID x) const { return !(*this == x); }
};
const LLUUID gAgentID{1};
using uuid_vec_t = std::vector<LLUUID>;
struct LLDirectorCast {
    LLUUID mActiveActorId;
    uuid_vec_t members;
    std::function<void()> changed;
    int events = 0;
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    LLUUID canonicalActorId(LLUUID id) { return id.value == 101 ? LLUUID{1} : id; }
    void add(LLUUID id) { if (std::find(members.begin(),members.end(),id)==members.end()) members.push_back(id); }
    const LLUUID& getActiveActorId() const { return mActiveActorId; }
    void mActiveActorChanged() { ++events; if (changed) changed(); }
    void setActiveActorId(const LLUUID&);
};
struct List {
    uuid_vec_t rows, selected;
    void deselectAllItems() { selected.clear(); }
    void selectByID(LLUUID id) { if (std::find(rows.begin(),rows.end(),id)!=rows.end()) selected.push_back(id); }
};
struct Shell { uuid_vec_t selected; void setCastSelection(const uuid_vec_t& ids) { selected = ids; } };
struct LLFloaterDirector {
    List list; Shell shell;
    List* mCastList = &list; Shell* mShell = &shell;
    struct Clock { void forceDue() {} } mAnimSpeedClock;
    LLUUID mAnimSpeedSelection, pathActor;
    bool mSyncingActiveActor = false;
    bool mActorStyleHadTarget = true, mActorStyleTabDirty = false, mAnimateTabDirty = false;
    uuid_vec_t gaze;
    LLUUID firstSelectedCastId() const { return list.selected.empty() ? LLUUID{} : list.selected.front(); }
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
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::setActiveActorId(")
    fixture += method(nv / "llfloaterdirector.cpp", "void LLFloaterDirector::syncActiveActor(")
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
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.firstSelectedCastId() == LLUUID{2});
    cast.add({3}); director.refreshCastList(); director.list.selected = {{2},{3}};
    cast.setActiveActorId({2});
    assert(cast.events == 1 && director.list.selected.size() == 2); // same primary preserves batch
    cast.setActiveActorId({3});
    assert(cast.events == 2 && director.list.selected == uuid_vec_t{{3}}); // bar selects one row
    director.list.selected = {{2},{3}};
    cast.setActiveActorId(director.firstSelectedCastId());
    assert(cast.events == 3 && director.list.selected.size() == 2); // Cast publishes its first row
    cast.setActiveActorId({});
    assert(cast.events == 4 && director.list.selected.empty() && director.mAnimSpeedSelection == gAgentID);
    cast.setActiveActorId({101}); // control-avatar aliases resolve to persistent identity
    assert(cast.events == 5 && cast.getActiveActorId() == LLUUID{1});
    cast.setActiveActorId({1});
    assert(cast.events == 5 && director.pathActor == LLUUID{1}); // no feedback loop
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
    print("PASS: removed picker/search destinations, preserved targets/batch scopes, session-only selection")


if __name__ == "__main__":
    main()
