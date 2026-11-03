"""Exercise the production session actor setter and Cast synchronization without a viewer build."""
from pathlib import Path
from native_check_tools import check_directory, vcvars64
import os
import re
import subprocess
import xml.etree.ElementTree as ET

from relight_area_policy_check import method as source_method


def actor_key_baseline(source, name):
    """Reverse only the content-pinned WP01 delta before the old guard chain."""
    import hashlib, json
    raw = (Path(__file__).parent / "actor_key_wp01_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(raw).hexdigest() == "130f8a609515880460dd6ad86d6fd633a971b4dc36eb1c06155cbe0162fee58f", "WP01 allowance pin"
    allowance = json.loads(raw)
    path = "indra/newview/" + name
    if path not in allowance["files"]:
        return source
    original = subprocess.check_output(["git", "show", allowance["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowance["files"][path]:
        assert original.count(hunk["before"]) == 1, ("WP01 baseline hunk", path)
        assert source.count(hunk["after"]) == 1, ("WP01 exact hunk", path)
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == original, ("WP01 reversal must reproduce its exact base", path)
    return source


def hot_gaze_baseline(source, name):
    # Exact feature hunks with context, pinned by content. Restore only Hot Gaze
    # before applying the existing phase-2/rekey/actor-bar baseline guards.
    import hashlib
    import json
    data = (Path(__file__).parent / "hot_gaze_mover_hunks.json").read_bytes().replace(b"\r\n", b"\n")
    assert hashlib.sha256(data).hexdigest() == "5be04f5e918bdb1298df907da8cbe6478a3e345914876f59ab89d1ce0046ed25"
    allowed = json.loads(data)
    if name not in allowed["files"]:
        return source
    original = subprocess.check_output(["git", "show", allowed["base"] + ":indra/newview/" + name]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowed["files"][name]:
        assert original.count(hunk["before"]) == 1, "Hot Gaze baseline hunk"
        assert source.count(hunk["after"]) == 1, "Hot Gaze exact feature hunk"
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == original, ("Hot Gaze reversal must reproduce its exact baseline", name)
    return source


def method(path, signature):
    # The old actor-bar fixture still executes the same production transaction
    # code. New Hot Gaze behavior is exercised by its own native contract and
    # the extended clone transaction fixture, without broadening old stubs.
    if path.name in ("llactormover.cpp", "lldirectorcast.cpp") and path.parent.name == "newview":
        source = hot_gaze_baseline(actor_key_baseline(path.read_text(encoding="utf-8"), path.name), path.name)
        baseline = check_directory("actorbar_baseline") / path.name
        baseline.parent.mkdir(parents=True, exist_ok=True)
        baseline.write_text(source, encoding="utf-8")
        return source_method(baseline, signature)
    return source_method(path, signature)


def reverse_qol_allowance(path, source):
    """Exact reviewed feature delta; unrelated content remains baseline guarded."""
    import hashlib
    import json
    raw = (Path(__file__).parent / "clone_qol_renderer_hunks.json").read_text(encoding="utf-8").replace("\r\n", "\n").encode()
    assert hashlib.sha256(raw).hexdigest() == "1537cae0db0e2597e140d50937a7eb768afb729365bf2a095ae4b7fbe2aa7a0e", "QoL allowance pin"
    allowance = json.loads(raw)
    baseline = subprocess.check_output(["git", "show", allowance["base"] + ":" + path]).decode("utf-8").replace("\r\n", "\n")
    for hunk in allowance["files"][path]:
        assert baseline.count(hunk["before"]) == 1, ("QoL baseline hunk", path)
        assert source.count(hunk["after"]) == 1, ("QoL exact feature hunk", path)
        source = source.replace(hunk["after"], hunk["before"], 1)
    assert source == baseline, ("QoL reversal must reproduce its exact baseline", path)
    return source


def main():
    nv = Path("indra/newview")
    actor_key_baseline((nv / "llactormover.h").read_text(encoding="utf-8"), "llactormover.h")
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
        guarded = actor_key_baseline((nv / path).read_text(encoding="utf-8"), path)
        if path == "pipeline.cpp":
            # Remove only the two pinned blink hooks before reversing clone QoL.
            import hashlib, json
            blink_bytes = (Path(__file__).parent / "pov_blink_pipeline_hunks.json").read_bytes().replace(b"\r\n", b"\n")
            assert hashlib.sha256(blink_bytes).hexdigest() == "acdd6aced21f50d2cae538d9746cffbfa77e51dd38af76988c48201b4f9fb2ee", "blink allowance pin"
            blink = json.loads(blink_bytes)
            blink_base = subprocess.check_output(["git", "show", blink["base"] + ":indra/newview/pipeline.cpp"]).decode("utf-8").replace("\r\n", "\n")
            for hunk in blink["hunks"]:
                assert blink_base.count(hunk["before"]) == 1, "blink baseline hunk"
                assert guarded.count(hunk["after"]) == 1, "blink exact feature hunk"
                guarded = guarded.replace(hunk["after"], hunk["before"], 1)
            guarded = reverse_qol_allowance("indra/newview/" + path, guarded)
        assert guarded.encode() == subprocess.check_output(
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
    assert 'm.mId = loadedSceneMemberId(e["id"].asUUID(), scene_actor_ids)' in load
    assert 'if (alias) *alias = m;' in load
    assert 'clone->mKind != ALGhostStudio::BACKING_ENTITY_CLONE) continue' in load
    assert 'if (previous_id == e["id"].asUUID()) continue' not in load
    assert 'Actor alias collision on load' in load and 'keeping latter scene record' in load
    assert 'LLActorMover::instance().dropActor(m.mId)' in load
    assert all('loadedSceneMemberId(data["subject_' + slot + '"]' in load for slot in 'abcd')
    assert 'has_stepped_actor ? loadedSceneMemberId(' in load
    assert not (nv / "aldirectoractorstate.h").exists()
    for path in ("lldirectorcast.cpp", "lldirectorcast.h", "llactormover.cpp", "llactormover.h"):
        source = (nv / path).read_text()
        for obsolete in ('aldirectoractorstate.h', 'mAuthoredState', 'mActorAuthorOrder',
                         'authored_fields', 'captureActorEdits', 'mergeActorMembers'):
            assert obsolete not in source, (path, obsolete)
    # Exact round-2 implementations protect ordinary edits/undo and linear
    # teardown, including costs that a value-only fixture cannot detect.
    base_cast = subprocess.check_output(["git", "show", "890f04b10d6:indra/newview/lldirectorcast.cpp"]).decode()
    base_mover = subprocess.check_output(["git", "show", "890f04b10d6:indra/newview/llactormover.cpp"]).decode()
    normalize = lambda text: text.replace('\r\n', '\n')
    # Round-2 mover plus Clone Studio look registration and the two exact
    # transaction-time reference-rewrite methods below. Hashes pin their full
    # content: an unrelated mover edit still fails the original baseline guard.
    current_mover = actor_key_baseline(normalize((nv / "llactormover.cpp").read_text(encoding="utf-8")), "llactormover.cpp")
    import hashlib
    rekey_methods = {
        'bool LLActorMover::rewriteGazeTarget(': '334e1f5af6f3cf254d171550133cc40df87ac11c96500cde799a8baf4056767f',
        'bool LLActorMover::rewriteActorReferences(': 'fc6510b35824ac02735fad71f323a4254048ec9164dd97faebc1b36e333e7a95',
    }
    guarded_mover = hot_gaze_baseline(current_mover, "llactormover.cpp")
    # Reversal order: Hot Gaze, QoL, phase 2, rekey, then the original guard.
    guarded_mover = reverse_qol_allowance("indra/newview/llactormover.cpp", guarded_mover)
    import json
    phase2_bytes = normalize((Path(__file__).parent / 'clone_looks2_mover_hunks.json').read_text(encoding='utf-8')).encode()
    assert hashlib.sha256(phase2_bytes).hexdigest() == '853c3b49b143ef58c488f9ce4af4b1eb70cbed005ebd0752924e612d3ebf9bbc'
    phase2 = json.loads(phase2_bytes)
    phase2_base = normalize(subprocess.check_output([
        'git', 'show', phase2['base'] + ':indra/newview/llactormover.cpp']).decode())
    for hunk in phase2['hunks']:
        assert phase2_base.count(hunk['before']) == 1, 'phase-2 baseline hunk'
        assert guarded_mover.count(hunk['after']) == 1, 'phase-2 exact feature hunk'
        guarded_mover = guarded_mover.replace(hunk['after'], hunk['before'], 1)
    assert guarded_mover == phase2_base, 'phase-2 reversal must reproduce its exact baseline'

    for signature, digest in rekey_methods.items():
        body = normalize(method(nv / "llactormover.cpp", signature))
        assert hashlib.sha256(body.encode()).hexdigest() == digest, signature
        assert guarded_mover.count(body) == 1, signature
        guarded_mover = guarded_mover.replace(body + "\n\n", "", 1)
    # Three existing overlay source reads must reject a deleted dependency;
    # each exact replacement is count-checked before restoring the baseline.
    for old, new, count in (
        ("LLVOAvatar* av = resolve_actor(inst.mSource);",
         "LLVOAvatar* av = inst.mSourceRemoved ? nullptr : resolve_actor(inst.mSource);", 2),
        ("else if (LLVOAvatar* av = resolve_actor(inst->mSource))",
         "else if (LLVOAvatar* av = inst->mSourceRemoved ? nullptr : resolve_actor(inst->mSource))", 1),
    ):
        assert guarded_mover.count(new) == count, new
        guarded_mover = guarded_mover.replace(new, old)
    if guarded_mover != normalize(base_mover):
        import difflib
        looks_old = normalize(subprocess.check_output(["git", "show", "c5dd757d341:indra/newview/llactormover.cpp"]).decode())
        looks_new = normalize(subprocess.check_output(["git", "show", "ba1e2f1e0bb:indra/newview/llactormover.cpp"]).decode())
        # Reverse exact phase-1 hunks from fixed reviewed commits. A set of
        # changed lines could hide duplicates, reordering or removed context.
        old_lines, new_lines = looks_old.splitlines(True), looks_new.splitlines(True)
        for group in difflib.SequenceMatcher(None, old_lines, new_lines, autojunk=False).get_grouped_opcodes(3):
            before = ''.join(old_lines[group[0][1]:group[-1][2]])
            after = ''.join(new_lines[group[0][3]:group[-1][4]])
            assert looks_old.count(before) == guarded_mover.count(after) == 1, 'phase-1 exact feature hunk'
            guarded_mover = guarded_mover.replace(after, before, 1)
    assert guarded_mover == normalize(base_mover), 'original round-2 mover baseline must match exactly'
    baseline = check_directory("actorbar_baseline") / "actor_round2_cast.cpp"
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(base_cast, encoding="utf-8")
    for signature in ('void LLDirectorCast::touchScene()', 'LLSD LLDirectorCast::sceneData() const',
                      'void LLDirectorCast::setActorStyle(', 'LLSD writeActorStyle(',
                      'LLDirectorCast::ActorStyle readActorStyle('):
        assert normalize(method(nv / "lldirectorcast.cpp", signature)) == normalize(method(baseline, signature)), signature
    teardown_end = '    // Build the legacy-to-stable identity map'
    assert load[:load.index(teardown_end)] == method(baseline, 'void LLDirectorCast::applySceneData(').split(teardown_end)[0]
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
    assert 'cast.replaceActorId(old_id.notNull() ? old_id : stable_id, destination, stable_id)' in consumer
    assert 'removing_instance ? LLUUID::null : new_id.notNull() ? new_id : stable_id' in consumer
    assert 'cast.remove(' not in consumer and 'mWasDirectorSubject' not in consumer
    rekey = method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(")
    assert rekey.index('CastMember saved = *old') < rekey.index('mover.dropActor(new_id)') < rekey.index('mover.migrateActor(old_id, new_id)') < rekey.index('mCast.erase(old)') < rekey.rindex('touchScene()')
    assert 'Actor alias collision on recovery' in rekey and 'keeping migrating record' in rekey
    assert rekey.count('LL_WARNS') == 1 and '<< old_id' in rekey and '<< new_id' in rekey
    assert load.count('Actor alias collision on load') == 1
    assert '<< previous_id' in load and '<< e["id"].asUUID()' in load
    assert 'mParkedWalkKeys' not in (nv / "llactormover.cpp").read_text()
    for signature in ('void LLActorMover::clearPath(', 'void LLActorMover::applyState(',
                      'void LLActorMover::snapshotForUndo(const LLUUID& actor_id, LLView* owner)'):
        body = method(nv / "llactormover.cpp", signature)
        assert 'pathSceneData' not in body and 'sceneData' not in body and 'mCast' not in body
    # The new pass also runs in the loaded-destination collision branch.
    assert len(re.findall(r"^    +(bool references_changed = )?rewriteActorReferences\(old_id, new_id\);", rekey, re.M)) == 2
    assert "bool references_changed = rewriteActorReferences(old_id, new_id);" in rekey
    assert "if (references_changed) touchScene();" in rekey
    assert "rewriteActorReferences(stable_id, new_id)" in rekey
    from director_clone_rekey_check import check as check_clone_rekey
    check_clone_rekey()
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
#include <cmath>
using U64 = unsigned long long;
struct LLUUID {
    int value = 0;
    bool notNull() const { return value != 0; }
    bool isNull() const { return !notNull(); }
    void setNull() { value=0; }
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
int aliasWarnings=0;
#define LL_WARNS(x) (++aliasWarnings, std::cout)
#define LL_ENDL std::endl
#define LL_PROFILE_ZONE_NAMED_CATEGORY_UI(x)
struct LLViewerObject { virtual ~LLViewerObject() = default; };
struct LLVOAvatar : LLViewerObject {
    bool dead = false, control = false, ghost = false, mIsDummy = false;
    bool isDead() const { return dead; }
    bool isControlAvatar() const { return control; }
    LLUUID getID() const { return {}; } // picker fixture has no runtime identities
    bool isGhostAvatar() const { return ghost; }
    bool isUIAvatar() const { return false; }
    void* getRootJoint() const { return nullptr; }
};
struct LLGhostAvatar : LLVOAvatar {};
struct ObjectList { LLViewerObject* findObject(LLUUID) { return nullptr; } } gObjectList;
struct ALGhostStudio {
    enum { BACKING_ENTITY_CLONE=1 };
    struct Instance { LLUUID mId, mEntityId; int mKind=BACKING_ENTITY_CLONE; float mAnimSpeed=1.f; };
    std::map<LLUUID,Instance> clones;
    static ALGhostStudio& instance() { static ALGhostStudio studio; return studio; }
    const Instance* getInstance(LLUUID id) const { auto it=clones.find(id); return it==clones.end()?nullptr:&it->second; }
    const Instance* findInstanceByRuntime(LLUUID id) const { for(auto& item:clones) if(id.notNull() && item.second.mEntityId==id) return &item.second; return nullptr; }
    bool isPrivateOverlayRuntime(LLUUID) const { return false; }
    bool isDeleting(LLUUID) const { return false; } // this fixture has no retiring bodies
    void setInstanceAnimSpeed(LLUUID id, float speed) { clones.at(id).mAnimSpeed=speed; }
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
    void clearDirectorLookAtRuntime(LLUUID) {}
    void onRosterMemberRemoved(LLUUID) {}
    void migrateActor(const LLUUID&, const LLUUID&);
    double pathSceneData(LLUUID id) const { auto it=mPaths.find(id); return it==mPaths.end()?0:it->second; }
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
struct ActorEditorStub { static void rewriteActorReferences(LLUUID, LLUUID) {} };
struct ALPanelGhostStudio {static void rewriteActorReferences(LLUUID,LLUUID){}};
struct ALFloaterGazeCues { static void rewriteActorReferences(LLUUID, LLUUID) {} };
struct ALClientHeight {static ALClientHeight& instance(){static ALClientHeight s;return s;} void reset(LLUUID){}};
struct ALAvatarLightResponse {static bool instanceExists(){return false;} static ALAvatarLightResponse& instance(){static ALAvatarLightResponse s;return s;} void retireClone(LLUUID){}};
struct ALAvatarStaging { static ALAvatarStaging& instance() { static ALAvatarStaging s; return s; } void removeActorReferences(LLUUID) {} };
struct ALDanceBall { static ALDanceBall& instance() { static ALDanceBall s; return s; } void removeActorReferences(LLUUID) {} };
struct LLDirectorCast {
    struct CastMember {
        LLUUID mId; std::string mLastName, mGroup;
        int mActorStyle = 0, mGazeTarget = 0, mMark = 0;
        std::vector<int> mGazeCues, mGazeInfluenceKeys;
        // Model nested sparse Stepped defaults separately from Actor FX.
        int mode = 0, rate = 12, role = 0, part = -1;
    bool mCloneFading=false;};
    std::vector<CastMember> mCast;
    uuid_vec_t mIds;
    std::set<LLUUID> mLookAtCameraIds;
    LLUUID mSubjectA, mSubjectB, mSubjectC, mSubjectD;
    int mCastRevision = 0, mGazeCueRevision = 0, mSubjectRevision = 0, mSceneRevision = 0;
    bool isLookAtCamera(LLUUID id) const { return mLookAtCameraIds.count(id)!=0; }
    struct PendingStart { LLUUID mId; };
    std::vector<PendingStart> mPendingStarts;
    std::map<std::string, float> mGroupDelays;
    std::map<std::string, int> mGroupRates;
    void rebuildMemberIndex() {}
    void touchScene() { ++mSceneRevision; }
    void remove(const LLUUID&);
    void cancelPendingStart(LLUUID id) { mPendingStarts.erase(std::remove_if(mPendingStarts.begin(),mPendingStarts.end(),[&](const PendingStart& start){return start.mId==id;}),mPendingStarts.end()); }
    void replaceActorId(const LLUUID&, const LLUUID&, const LLUUID& = LLUUID::null);
    bool rewriteActorReferences(const LLUUID&, const LLUUID&) { return false; } // exercised by focused production harness
    uuid_vec_t membersInGroup(const std::string&) const;
    bool contains(LLUUID id) const { for(const auto& m:mCast) if(m.mId==id) return true; return false; }
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
    static bool instanceExists() { return true; }
    static bool isSelf(const LLUUID& id) { return id.isNull() || id == gAgentID; }
    static LLUUID runtimeActorId(const LLUUID& id);
    static LLUUID canonicalActorId(LLUUID id) { return id.value == 101 ? LLUUID{1} : id; }
    void add(LLUUID id) {
        if (std::find(members.begin(),members.end(),id)==members.end()) members.push_back(id);
        if(!getMember(id)) { CastMember member; member.mId=id; mCast.push_back(member); mIds.push_back(id); }
    }
    const LLUUID& getActiveActorId() const { return mActiveActorId; }
    void mActiveActorChanged() { ++events; if (changed) changed(); }
    void setActiveActorId(const LLUUID&);
    LLUUID getSteppedActorId() const { return runtimeActorId(mActiveActorId); }
    void migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids);
    void saveStepped(Scene& data) const;
    std::map<LLUUID, size_t> mMemberIndex;
    void loadWholeRecords(const std::vector<CastMember>&, const std::vector<double>&,
                          const std::vector<bool>&);
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
    static void rewriteActorReferences(LLUUID,LLUUID) {}
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
    fixture += method(nv / "lldirectorcast.cpp", "LLUUID loadedSceneMemberId(")
    # Execute the production compatibility statements against a minimal LLSD map.
    migration = re.search(r'    const bool has_stepped_actor = .*?LLUUID::null;', load, re.S)[0]
    restore = re.search(r'    if \(has_stepped_actor\) setActiveActorId\([^;]+;', load)[0]
    fixture += "void LLDirectorCast::migrateStepped(const Scene& data, const SceneActorIdRemap& scene_actor_ids) {\n" + migration + restore + "\n}\n"
    write = re.search(r'    data\["stepped_actor"\] = getSteppedActorId\(\);', save)[0]
    fixture += "void LLDirectorCast::saveStepped(Scene& data) const {\n" + write + "\n}\n"
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::remove(")
    fixture += method(nv / "lldirectorcast.cpp", "void LLDirectorCast::replaceActorId(").replace("ALPanelLensGaze::", "ActorEditorStub::")
    # Execute production collision arbitration and publication; payload copies
    # stand in for the unchanged LLSD codecs, with -1 representing absent path.
    collision = load[load.index('        CastMember* alias = getMember(m.mId);'):load.index('        m.mLastName = e["name"].asString();')]
    publication = load[load.index('        if (alias) *alias = m;'):load.index('        ++mCastRevision; // [GhostPanelPerf] loaded member')]
    fixture += r'''
struct RecordArray : std::vector<Scene> {
    auto beginArray() const { return begin(); }
    auto endArray() const { return end(); }
};
void LLDirectorCast::loadWholeRecords(const std::vector<CastMember>& payloads,
                                    const std::vector<double>& paths,
                                    const std::vector<bool>& cameras) {
    auto& mover=LLActorMover::instance();
    mCast.clear(); mIds.clear(); mMemberIndex.clear(); mLookAtCameraIds.clear();
    mover.mPaths.clear(); mover.mHistory.clear();
    RecordArray cast_arr;
    for(const auto& member:payloads) { Scene e; e["id"]=member.mId; cast_arr.push_back(e); }
    SceneActorIdRemap scene_actor_ids;
    for(auto it=cast_arr.beginArray(); it!=cast_arr.endArray(); ++it) {
        const Scene& e=*it;
        const auto index=it-cast_arr.beginArray();
        CastMember m=payloads[index];
        m.mId=loadedSceneMemberId(e["id"].asUUID(),scene_actor_ids);
        if(m.mId.isNull()) continue;
''' + collision + publication + r'''
        if(paths[index]>=0) mover.mPaths[m.mId]=paths[index];
        if(cameras[index]) mLookAtCameraIds.insert(m.mId);
    }
}
'''
    transaction = consumer[consumer.index('            const LLUUID destination'):consumer.index('\n        });')]
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
    void refreshCastCombo(bool force=false);
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
    speed = nv / "alavataranimspeed.cpp"
    fixture += r'''
struct ALAvatarAnimSpeed {
    std::map<LLUUID,F32> mSpeeds;
    int mRevision=0;
    static float clampSpeed(float speed) { return std::clamp(speed,.05f,4.f); }
    static bool isGhost(const LLUUID&);
    F32 getSpeed(const LLUUID&) const;
    void setSpeed(const LLUUID&, F32);
    void apply(LLVOAvatar&) {}
};
'''
    for signature in ('const ALGhostStudio::Instance* studioClone(', 'bool ALAvatarAnimSpeed::isGhost(',
                      'F32 ALAvatarAnimSpeed::getSpeed(', 'void ALAvatarAnimSpeed::setSpeed('):
        fixture += method(speed, signature)
    studio_speed = method(nv / "alghoststudio.cpp", "bool ALGhostStudio::setInstanceAnimSpeed(")
    assert studio_speed.index('inst->mAnimSpeed =') < studio_speed.index('if (!ghost)')
    assert 'STATE_ERROR' not in studio_speed
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
    // R3: save/load ownership normalization uses the production loader helper.
    // The payload fixture retains the complete member and path, as the scene
    // codecs do; no body resolution is required to test identity ownership.
    auto& studio=ALGhostStudio::instance();
    const LLUUID S{210}, R{211}, N{212}, M{213};
    studio.clones[S]={S,R};
    LLDirectorCast::CastMember sceneMember; sceneMember.mId=R;
    sceneMember.mActorStyle=72; sceneMember.mMark=19; sceneMember.mGazeTarget=35;
    sceneMember.mode=1; sceneMember.rate=8; sceneMember.role=1; sceneMember.part=2;
    sceneMember.mGroup="loaded"; sceneMember.mGazeCues={2,4}; sceneMember.mGazeInfluenceKeys={3,6};
    auto resetScene = [&] {
        cast.mCast={sceneMember}; cast.mIds={R}; cast.setActiveActorId(R);
        cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=R;
        mover.mPaths.clear(); mover.mHistory.clear(); mover.mPaths[R]=3.25; mover.mHistory[R]={3.25};
        studio.clones[S].mEntityId=R; cast.touchScene();
    };
    using Payload=std::pair<LLDirectorCast::CastMember,double>;
    auto saveClone = [&] { return Payload{cast.mCast.front(),mover.mPaths.at(cast.mCast.front().mId)}; };
    auto loadClone = [&](const Payload& payload) {
        cast.mCast.clear(); cast.mIds.clear(); mover.mPaths.clear(); mover.mHistory.clear();
        auto member=payload.first; member.mId=loadedSceneMemberId(member.mId,{});
        cast.mCast.push_back(member); cast.mIds.push_back(member.mId);
        mover.mPaths[member.mId]=payload.second;
        cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=member.mId;
        cast.setActiveActorId(member.mId);
    };
    auto assertClone = [&](LLUUID id) {
        assert(cast.mCast.size()==1 && cast.mCast.front().mId==id);
        const auto& member=cast.mCast.front();
        assert(member.mActorStyle==72 && member.mMark==19 && member.mGazeTarget==35);
        assert(member.mode==1 && member.rate==8 && member.role==1 && member.part==2);
        assert(member.mGroup=="loaded" && member.mGazeCues==std::vector<int>({2,4}));
        assert(member.mGazeInfluenceKeys==std::vector<int>({3,6}));
        assert(mover.mPaths.size()==1 && mover.mPaths.at(id)==3.25);
        assert(cast.mSubjectA==id && cast.mSubjectB==id && cast.mSubjectC==id && cast.mSubjectD==id);
        assert(cast.getSteppedActorId()==id);
    };
    resetScene(); auto liveSave=saveClone(); loadClone(liveSave); // save while live -> load -> respawn
    cloneTransition(S,R,N); studio.clones[S].mEntityId=N; assertClone(N);
    resetScene(); cloneTransition(S,R,{}); studio.clones[S].mEntityId={};
    auto parkedSave=saveClone(); assert(parkedSave.first.mId==S);
    loadClone(parkedSave); assertClone(S); // load while parked -> respawn
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N; assertClone(N);
    loadClone(parkedSave); assertClone(N); // parked save -> load while live -> refresh -> despawn
    cloneTransition(S,N,M); studio.clones[S].mEntityId=M; assertClone(M);
    cloneTransition(S,M,{}); studio.clones[S].mEntityId={}; assertClone(S);
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N;
    loadClone(parkedSave); cloneTransition(S,N,{}); studio.clones[S].mEntityId={}; assertClone(S);
    cloneTransition(S,{},N); studio.clones[S].mEntityId=N;
    loadClone(parkedSave); cloneTransition(S,N,{},true); // direct delete immediately after loaded stable alias
    assert(cast.mCast.empty() && mover.mPaths.empty() && cast.getActiveActorId().isNull());
    assert(cast.mSubjectA.isNull() && cast.mSubjectB.isNull() && cast.mSubjectC.isNull() && cast.mSubjectD.isNull());
    resetScene(); cloneTransition(S,R,{}); studio.clones[S].mEntityId={}; loadClone(parkedSave);
    cloneTransition(S,{}, {},true); assert(cast.mCast.empty() && mover.mPaths.empty()); // parked delete
    assert(cast.mSubjectA.isNull() && cast.mSubjectB.isNull() && cast.mSubjectC.isNull() && cast.mSubjectD.isNull());
    // Real speed writer/readback: a selected parked stable handle never enters
    // the resident map; the replacement runtime reads the same Studio value.
    ALAvatarAnimSpeed animSpeed;
    animSpeed.setSpeed(S,2.5f);
    assert(animSpeed.mSpeeds.empty() && studio.clones[S].mAnimSpeed==2.5f && animSpeed.getSpeed(S)==2.5f);
    studio.clones[S].mEntityId=N;
    assert(animSpeed.getSpeed(N)==2.5f && ALAvatarAnimSpeed::isGhost(S));
    animSpeed.setSpeed(N,.75f); assert(animSpeed.getSpeed(S)==.75f && animSpeed.mSpeeds.empty());
    // Load collisions in either order: the last complete record wins. Sparse
    // Role/part defaults in the loser cannot overwrite saved mode/rate.
    LLDirectorCast::CastMember loadStable=sceneMember, loadRuntime;
    loadStable.mId=S; loadRuntime.mId=N;
    loadRuntime.mActorStyle=6; loadRuntime.role=1; loadRuntime.part=3;
    cast.mSubjectA=cast.mSubjectB=cast.mSubjectC=cast.mSubjectD=N;
    cast.setActiveActorId(N);
    int loadWarnings=aliasWarnings;
    cast.loadWholeRecords({loadRuntime,loadStable},{9.0,3.25},{true,false});
    assertClone(N);
    assert(cast.mIds==uuid_vec_t{N} && !cast.mLookAtCameraIds.count(N));
    assert(aliasWarnings==loadWarnings+1);
    loadWarnings=aliasWarnings;
    cast.loadWholeRecords({loadStable,loadRuntime},{3.25,-1.0},{true,false});
    assert(cast.mCast.size()==1 && cast.mIds==uuid_vec_t{N});
    assert(cast.mCast.front().mActorStyle==6 && cast.mCast.front().mode==0 && cast.mCast.front().rate==12);
    assert(cast.mCast.front().role==1 && cast.mCast.front().part==3);
    assert(mover.mPaths.empty() && mover.mHistory.empty() && cast.mLookAtCameraIds.empty());
    assert(aliasWarnings==loadWarnings+1);
    cast.loadWholeRecords({loadStable},{3.25},{false});
    cast.add(N); assertClone(N); // Studio's default add cannot overwrite saved settings
    auto onlyPart=loadRuntime; onlyPart.role=0;
    cast.loadWholeRecords({onlyPart,loadStable},{9.0,3.25},{true,false}); assertClone(N);
    cast.loadWholeRecords({loadStable,loadRuntime,loadStable},{3.25,-1.0,3.25},{true,false,false});
    assertClone(N); // repeated alias still obeys last-entry rule
    auto repeatedClone=loadRuntime; repeatedClone.mActorStyle=24;
    cast.loadWholeRecords({loadStable,loadRuntime,repeatedClone},{3.25,-1.0,7.0},{true,false,false});
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==24 && mover.mPaths.at(N)==7.0);
    auto nonClone=loadStable; nonClone.mId={300};
    auto repeatedNonClone=nonClone; repeatedNonClone.mActorStyle=0;
    loadWarnings=aliasWarnings;
    cast.loadWholeRecords({nonClone,repeatedNonClone},{3.25,-1.0},{false,false});
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==72);
    assert(mover.mPaths.at({300})==3.25 && aliasWarnings==loadWarnings);
    // Recovery collision: the current migrating owner wins wholesale. Its
    // nested defaults and explicit clears travel with its path AND history.
    LLDirectorCast::CastMember stable, runtimeAlias; stable.mId=S; runtimeAlias.mId=R;
    stable.mActorStyle=80; stable.mode=1; stable.rate=8;
    runtimeAlias.mMark=17; runtimeAlias.role=1; // role-only defaults must not leak into owner
    cast.mCast={stable,runtimeAlias};
    mover.mPaths.clear(); mover.mHistory.clear();
    mover.mPaths[S]=4.0; mover.mHistory[S]={3.0,4.0};
    mover.mPaths[R]=5.0; mover.mHistory[R]={5.0};
    cast.mLookAtCameraIds={R};
    int warningsBefore=aliasWarnings;
    cast.replaceActorId(S,R);
    assert(aliasWarnings==warningsBefore+1);
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==80 && cast.mCast.front().mMark==0);
    assert(cast.mCast.front().mode==1 && cast.mCast.front().rate==8 && cast.mCast.front().role==0);
    assert(mover.mPaths.size()==1 && mover.mPaths.at(R)==4.0);
    assert(mover.mHistory.size()==1 && mover.mHistory.at(R)==std::vector<double>({3.0,4.0}));
    assert(!cast.mLookAtCameraIds.count(R));
    // Reverse direction: a deliberately default/empty source clears every
    // discarded destination value, including path/history/camera membership.
    cast.mCast.push_back(stable); cast.getMember(R)->mActorStyle=0;
    cast.getMember(R)->mode=0; cast.getMember(R)->rate=12;
    mover.mPaths.erase(R); mover.mHistory.erase(R);
    mover.mPaths[S]=6.0; mover.mHistory[S]={6.0}; cast.mLookAtCameraIds={S};
    cast.replaceActorId(R,S);
    assert(cast.mCast.size()==1 && cast.mCast.front().mActorStyle==0);
    assert(cast.mCast.front().mode==0 && cast.mCast.front().rate==12);
    assert(mover.mPaths.empty() && mover.mHistory.empty() && cast.mLookAtCameraIds.empty());
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
    std::cout << "PASS: live/parked scene round trips, live loaded stable alias refresh/despawn/delete, wholesale alias winner with path/history/defaults, parked Studio animation speed\n";
}
'''
    directory = check_directory("actorbar")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "actor.cpp").write_text(fixture, encoding="utf-8")
    vcvars = vcvars64()
    (directory / "compile.cmd").write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "actor.obj"}" '
        f'/Fe"{directory / "actor.exe"}" "{directory / "actor.cpp"}\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(directory / "compile.cmd")], check=True)
    subprocess.run([str(directory / "actor.exe")], check=True)
    print("PASS: picker/search contracts, independent renderer subjects, legacy Stepped scene round-trip, group publication, despawned-clone filter, compact rail geometry")


if __name__ == "__main__":
    main()
