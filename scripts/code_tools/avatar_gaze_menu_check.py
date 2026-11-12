#!/usr/bin/env python3
"""Guard avatar Director > Gaze wiring, with planted must-fail source mutations.

Run --check and --selftest. No viewer, login, or external dependencies required.
The existing actor/nearby native gates separately exercise the underlying model APIs.
"""
from pathlib import Path
import argparse
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
MENUS = ('menu_avatar_other.xml', 'menu_avatar_self.xml',
         'menu_attachment_other.xml', 'menu_attachment_self.xml')


def sources():
    # Preserve line endings: the shared XUI fragment is pinned byte-for-byte.
    result = {name: (NV / 'skins/default/xui/en' / name).read_bytes().decode('utf-8') for name in MENUS}
    for name in ('alghostclonemenu.cpp', 'algazeactor.h', 'alpanellensgaze.cpp', 'lltoolobjpicker.cpp', 'lltoolobjpicker.h', 'lltoolmgr.cpp', 'llviewerwindow.h', 'llviewerwindow.cpp', 'llactormover.h', 'algazemath.h'):
        result[name] = (NV / name).read_text(encoding='utf-8')
    result['director_kit_gen.py'] = (ROOT / 'scripts/code_tools/director_kit_gen.py').read_text(encoding='utf-8')
    return result


def body(text, signature):
    start = text.index(signature)
    pos = text.index('{', start)
    depth, end = 1, pos + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[pos:end]


def findings(src):
    bad = []

    def need(ok, message):
        if not ok:
            bad.append(message)

    try:
        fragments = []
        for name in MENUS:
            text = src[name]
            menu = ET.fromstring(text).find("context_menu[@name='Director']/context_menu[@name='Avatar Gaze']")
            need(menu is not None, name + ': missing Director > Gaze')
            if menu is None:
                continue
            labels = ['-' if item.tag == 'menu_item_separator' else item.get('label') for item in menu]
            need(labels == ['Gaze On', '-', 'Camera', 'Me', 'Cast member', 'Nearby people',
                            "Where it's going", 'Pick object...', '-', 'Open in Gaze page...'], name + ': order')
            for suffix, kind in (('On', 'on'), ('Camera', 'camera'), ('Me', 'me'),
                                 ('Travel', 'travel'), ('Object', 'object')):
                item = menu.find(f"menu_item_check[@name='Avatar Gaze {suffix}']")
                for event, function in (('on_click', 'Avatar.Gaze'), ('on_check', 'Avatar.CheckGaze'),
                                        ('on_enable', 'Avatar.EnableGaze')):
                    callback = None if item is None else item.find('menu_item_check.' + event)
                    need(callback is not None and callback.get('function') == ('Avatar.PickGazeObject' if suffix == 'Object' and event == 'on_click' else function) and
                         (event == 'on_enable' or (suffix == 'Object' and event == 'on_click') or callback.get('parameter') == kind),
                         name + ': ' + suffix + ' callback ' + event)
            me = menu.find("menu_item_check[@name='Avatar Gaze Me']/menu_item_check.on_visible")
            need(me is not None and me.get('function') == 'Avatar.VisibleGazeMe', name + ': self visibility callback')
            fragment = re.search(r'    <context_menu label="Gaze" name="Avatar Gaze">.*?    </context_menu>', text, re.S)
            need(fragment is not None, name + ': missing byte fragment')
            if fragment:
                fragments.append(fragment.group())
        need(len(fragments) == 4 and len(set(fragments)) == 1, 'four Gaze branches must be byte-identical')
        cpp, helper, page = (src[n] for n in ('alghostclonemenu.cpp', 'algazeactor.h', 'alpanellensgaze.cpp'))
        feature = cpp[cpp.index('LLVOAvatar* avatarGazeActor()'):cpp.index('// The avatar an overlay sits on.')]
        record = body(feature, 'void recordAvatarGazeHistory(')
        act = body(feature, 'void actAvatarGaze(') + record
        current = body(feature, 'bool avatarGazeCastCurrent(')
        nearby = body(feature, 'std::vector<ALGazeNearby::Entry> avatarGazeNearby(')
        label = body(feature, 'std::string avatarGazeLabel(')
        checked = body(feature, 'bool avatarGazeChecked(')
        need('return sMenuAvatar != gAgentID;' in feature, 'Me must be hidden for self')
        need('find_avatar_from_object(sMenuAvatar)' in feature and 'avatar->getID() == sMenuAvatar' in feature,
             'resolve picked avatar at click time without retargeting')
        need('getCast()' in feature and 'getActorChoices' not in feature, 'current Cast only, no Subject-derived choices')
        need(not re.search(r'\b\w*[Ss]ubject\w*\s*\(', feature + helper), 'no Subject API reads/writes')
        need('ALGazeActor::setTarget(actor, target, true);' in act and
             'if (enable) setEnabled(actor, true);' in helper, 'every target pick enables gaze')
        for kind, mode in (('camera', 'CAMERA'), ('travel', 'MOTION')):
            need(f'kind == "{kind}") target.mMode = LLActorMover::GazeTarget::{mode};' in act,
                 kind + ': wrong head target mode')
        need('ALGazeActor::setEnabled(actor, !before_on)' in act, 'toggle preserves target')
        need('LLDirectorCast::instance().setGazeTarget(actor, target);' in helper and
             'ALGazeActor::setTarget(actor, target);' in body(page, 'void ALPanelLensGaze::commitQuickControl('),
             'menu and card use shared local per-actor write path')
        need('contains(id)' in current and '!ALGhostStudio::instance().isDeleting(id)' in current and
             'id != actor && id != gAgentID' in current, 'reject stale/self Cast targets')
        need('avatarGazeCastCurrent(person, actor) : avatarGazeNearbyCurrent(person, actor)' in act and
             act.index('avatarGazeCastCurrent(person, actor)') < act.index('ensureAvatarGazeCast(actor)'),
             'validate stale target before any Cast/model write')
        need('if (!ALGazeNearby::nearbyAllowed()) return people;' in nearby and
             'ALGazeNearby::personDisplayName(avatar->getID())' in nearby and
             'name.empty() || Logic::containsUuidText(name)' in nearby and
             'LLDirectorCast::instance().contains(avatar->getID())' in nearby,
             'nearby must obey RLV/display-name/no-Cast rule')
        need('if (nameHiddenByRlva(id)) return "Unknown";' in label and
             'Logic::containsUuidText(name)' in label and
             not any(t in feature for t in ('getFullname(', '.asString().substr(', 'getCompleteName(')),
             'labels must withhold hidden names and UUIDs')
        need('if (kind == "on" || !on) return on;' in checked and 'getGazeTargetConfig(sMenuAvatar)' in checked,
             'checks use mover state and all targets uncheck when off')
        need('[actor, before, before_on, after, after_on, valid]' in act and 'body->getID() != actor' in act and
             '!cast.contains(actor)' in act and 'ALGazeActor::setTarget(actor, saved);' in act and
             'ALGazeActor::setEnabled(actor, redo ? after_on : before_on);' in act and
             act.count('recordCustom(') == 1 and 'getActiveActor' not in act,
             'one history step captures/restores exact state keyed to immutable actor UUID')
        need('cast.resolve(saved.mCastRef, false)' in act and 'isDeleting(saved.mCastRef)' in act and
             'gObjectList.findObject(saved.mObjectRef)' in act and
             'reason = "That gaze target is no longer available"' in act,
             'history refuses retired target identities')
        need('LLFloaterReg::findInstance("director")' in act and 'getInstance("director")' not in act,
             'apply without constructing a history host')
        need('ALGazeActor::menuObjectTarget(actor, picked)' in act and '!picked->isAvatar()' in act and
             'ALGazeActor::cardSelectedObjectTarget()' in page and
             'object->isDead() || object->isHUDAttachment()' in helper and
             'wearer->getID() == actor' in helper and 'getFirstRootObject()' in helper,
             'menu object path excludes own body/attachments')
        card = body(helper, 'inline LLUUID cardSelectedObjectTarget(')
        need(card == '{\n    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();\n    if (!object) object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();\n    if (!object) return LLUUID::null;\n    return object->getRootEdit() ? object->getRootEdit()->getID() : object->getID();\n}',
             'card preserves base selection/root-edit semantics without menu restrictions')
        valid = body(act, 'const auto valid = ')
        need('const auto& saved = redo ? after : before;' in valid and
             '}, false, valid);' in act and 'if (!valid(redo, reason)) return false;' in act and
             '!cast.contains(actor)' in valid and 'if (!object || object->isDead())' in valid and 'menuObjectTarget' not in valid and
             '!ALAvatarSubjectPicker::valid(cast.resolve(saved.mCastRef, false), true, true)' in valid and
             'isDeleting(saved.mCastRef)' in valid and 'isPrivateOverlayRuntime(saved.mCastRef)' in valid,
             'undo and redo preflight must discard unavailable actors/objects/people/clones')
        finish = body(feature, 'void finishAvatarGazePick(')
        arm = body(feature, 'void armAvatarGazePick(')
        picker = src['lltoolobjpicker.cpp']
        need('sAvatarGazePickActor = actor;' in arm and
             'setExitCallback(finishAvatarGazePick, nullptr, true)' in arm and
             'setTransientTool(picker)' in arm and 'setExitCallback(nullptr' not in arm and
             'Click an object for ' in arm and
             ' to look at (Esc cancels)' in arm, 'arm one-shot picker with captured actor and tip')
        need('picker->completedPick() ? picker->getObjectID() : LLUUID::null' in finish and
             'if (actor.isNull() || object.isNull()) return;' in finish and
             finish.index('if (actor.isNull() || object.isNull()) return;') < finish.index('actAvatarGaze("object", object, actor);') and
             'recordCustom(' not in finish and 'setTarget(' not in finish and
             'setNull()' in finish and 'setExitCallback(nullptr, nullptr)' in finish and
             'getCurrentTool() == picker' in finish and 'clearTransientTool()' in finish,
             'cancel clears pending state/restores tool without writes or history')
        need('find_avatar_from_object(captured_actor)' in act and
             'avatar->getID() != captured_actor' in act and
             '!ALAvatarSubjectPicker::valid(avatar, false, false)' in act and
             act.index('avatar->getID() != captured_actor') < act.index('ensureAvatarGazeCast(actor)'),
             're-resolve captured actor before pick writes')
        need('mCompletedPick = false;' in body(picker, 'void LLToolObjPicker::handleDeselect(') and
             'mCompletedPick = false;' in body(picker, 'void LLToolObjPicker::handleSelect(') and
             'mCancelOnEscape && key == KEY_ESCAPE' in picker and
             'mCompletedPick = hasMouseCapture() && mHitObjectID.notNull();' in picker,
             'picker distinguishes completed mouse pick from tool change/Escape')
        setter = body(picker, 'void LLToolObjPicker::setExitCallback(')
        need('if (mSessionLive)' in setter and setter.index('cancelPick();') < setter.index('mExitCallback = callback;') and
             'clearTransientTool();' in setter and
             'mSessionLive = false;' in body(picker, 'void LLToolObjPicker::onMouseCaptureLost(') and
             'mExitCallback(mExitCallbackData);' in body(picker, 'void LLToolObjPicker::onMouseCaptureLost(') and
             'mSessionLive = true;' in body(picker, 'void LLToolObjPicker::handleSelect(') and
             arm.index('setExitCallback(') < arm.index('sAvatarGazePickActor = actor;'),
             'all live picker owners exit before callback takeover/new actor assignment')
        equal = body(feature, 'bool avatarGazeTargetsEqual(')
        target = body(src['llactormover.h'], 'struct GazeTarget')
        profile = body(src['algazemath.h'], 'struct AnatomicalLimitProfile')
        fields = re.findall(r'^\s*\S+\s+(m\w+)\s*(?:=|;)', target, re.M)
        profile_fields = re.findall(r'F32\s+(m\w+)\s*=', profile)
        need(all('a.'+f+' == b.'+f in equal for f in fields if f != 'mLimitProfile') and
             all('a.mLimitProfile.'+f+' == b.mLimitProfile.'+f in equal for f in profile_fields), 'compare complete target including inactive config and limit profile')
        need('if (before_on == after_on && avatarGazeTargetsEqual(before, after)) return;' in record and
             'recordAvatarGazeHistory(actor, before, before_on, after, after_on);' in act,
             'shared record helper suppresses identical enable and full target snapshots')
        need('pick_info.mPickGeneration = pick_generation;' in src['llviewerwindow.cpp'] and
             'U64 pick_generation = 0' in src['llviewerwindow.h'] and
             'pick_info.mPickGeneration != picker->mPickGeneration' in picker and
             'LLEventTimer::run_after(1.f' in picker and
             'if (mCancelOnEscape && mSessionLive) finishSession();' in body(picker, 'void LLToolObjPicker::cancelPick(') and
             'if (mCancelOnEscape && mSessionLive) cancelPick();' in body(picker, 'void LLToolObjPicker::handleDeselect('),
             'generation transport, bounded pending pick and cleanup without capture')
        manager = src['lltoolmgr.cpp']
        need('if (hasMouseCapture()) setMouseCapture(false);' in body(picker, 'void LLToolObjPicker::cancelPick(') and
             'LLToolObjPicker::getInstance()->cancelPick();' in body(manager, 'void LLToolMgr::clearTransientTool(') and
             'cancelOnToolChange()' in body(manager, 'void LLToolMgr::setCurrentTool(') and
             'cancelOnToolChange()' in body(manager, 'void LLToolMgr::setTransientTool('),
             'opt-in cancel releases capture on Escape and tool changes')
        need('ensureAvatarGazeCast(sMenuAvatar)' in feature and 'ALDirectorSelect::selectAvatar(sMenuAvatar, false)' in feature and
             'shell->navigateTo("performance_group/gaze_tab")' in feature, 'open Gaze selects the clicked actor')
        need(cpp.count('populateAvatarGazeMenus(') == 2 and
             'populateAvatarGazeMenus(menu);' in body(cpp, 'void prepareAvatarMenu(') and
             not re.search(r'(?:idle|draw|timer|addIdleCallback)\s*\(', feature), 'populate only on menu open; no frame/idle work')
        need('import avatar_gaze_menu_check' in src['director_kit_gen.py'] and
             'clone_menu_check, avatar_gaze_menu_check):' in src['director_kit_gen.py'],
             'fixture must run through prebuild_guard director_kit_gen --check')
    except (ValueError, IndexError, ET.ParseError) as error:
        bad.append('malformed/missing required source: ' + str(error))
    return bad


def native_regressions(src, negatives=False):
    """Execute production picker methods and preflight lambda against small lifecycle doubles."""
    from native_check_tools import check_directory, compile_run
    out = check_directory('avatar_gaze')
    picker = src['lltoolobjpicker.cpp']
    methods = '\n'.join('void LLToolObjPicker::' + name + args + '\n' + body(picker, 'void LLToolObjPicker::' + name + '(')
                        for name, args in [('setExitCallback', '(void (*callback)(void *), void *callback_data, bool cancel_on_escape)'),
                                           ('handleSelect', '()'), ('handleDeselect', '()'), ('onMouseCaptureLost', '()'), ('cancelPick', '()'),
                                           ('finishSession', '()'), ('completeGazePick', '()'), ('pickCallback', '(const LLPickInfo& pick_info)')])
    methods += '\n' + '\n'.join('bool LLToolObjPicker::' + name + args + '\n' + body(picker, 'bool LLToolObjPicker::' + name + '(') for name,args in [('handleMouseDown','(S32 x, S32 y, MASK mask)'), ('handleMouseUp','(S32 x, S32 y, MASK mask)'), ('handleKey','(KEY key, MASK mask)')])
    act = body(src['alghostclonemenu.cpp'], 'void recordAvatarGazeHistory(')
    validator = body(act, 'const auto valid = ')
    prelude = r"""
#include <string>
#include <functional>
#include <cstdlib>
#include <cstdio>
#include <vector>
using U64=unsigned long long;using S32=int;using MASK=int;using KEY=int;const int KEY_ESCAPE=27;
struct Log {template<class T>Log& operator<<(T){return *this;}};
#define LL_WARNS() Log()
#define LL_ENDL 0
struct LLSD { std::string value; std::string& operator[](const char*){return value;} };
int tips=0;struct LLNotificationsUtil {static void add(const char*,LLSD){++tips;}};
struct LLEventTimer {static inline std::vector<std::function<void()>> timers;template<class F>static void run_after(float,F f){timers.push_back(f);}static void fire(){auto t=timers;timers.clear();for(auto& f:t)f();}};
void require(bool ok) { if (!ok) std::exit(2); }
struct LLUUID { int v=0; bool notNull() const { return v!=0; } bool isNull() const { return !v; } void setNull(){v=0;} bool operator==(LLUUID b) const{return v==b.v;} bool operator!=(LLUUID b) const{return v!=b.v;} static LLUUID null; };
LLUUID LLUUID::null;
struct LLView {bool handleHover(int,int,int){return false;}};
struct LLRootView:LLView {bool handleMouseDown(int,int,int){return false;}};
struct LLPickInfo { LLUUID mObjectID; U64 mPickGeneration=0; };
struct Window {LLRootView root;std::vector<LLPickInfo> pending;LLRootView* getRootView(){return &root;}void pickAsync(int,int,int,void(*)(const LLPickInfo&),bool=false,bool=false,bool=false,bool=false,U64 gen=0){pending.push_back({{3},gen});}} window,*gViewerWindow=&window;
struct LLTool { virtual void handleSelect() {} void handleDeselect(){} bool handleMouseDown(int,int,int){return false;} bool handleMouseUp(int,int,int){return false;} bool handleKey(int,int){return false;} };
struct LLToolObjPicker : LLTool {
 U64 mPickGeneration=0;bool mPickPending=false,mMouseReleased=false;
 bool mSessionLive=false,mPicked=false,mCompletedPick=false,mCancelOnEscape=false,capture=false;
 LLUUID mHitObjectID; void (*mExitCallback)(void*)=nullptr; void* mExitCallbackData=nullptr;
 bool hasMouseCapture(){return capture;} void setMouseCapture(bool on){bool old=capture;capture=on;if(old&&!on)onMouseCaptureLost();}
 static LLToolObjPicker* getInstance();
 void setExitCallback(void (*)(void*),void*,bool=false);void handleSelect();void handleDeselect();void onMouseCaptureLost();void cancelPick();void finishSession();void completeGazePick();static void pickCallback(const LLPickInfo&);bool handleMouseDown(S32,S32,MASK);bool handleMouseUp(S32,S32,MASK);bool handleKey(KEY,MASK);
};
LLToolObjPicker picker;LLToolObjPicker* LLToolObjPicker::getInstance(){return &picker;}
struct LLToolMgr { bool transient=false; static LLToolMgr* getInstance(){static LLToolMgr m;return &m;} void clearTransientTool(){transient=false;if(picker.mCancelOnEscape)picker.cancelPick();} void start(){transient=true;picker.handleSelect();} };
int exits=0; bool button=false,picking=false,takeover=true;
void particle(void*){++exits;LLToolMgr::getInstance()->clearTransientTool();button=true;require(takeover?picker.mHitObjectID.isNull():picker.mHitObjectID.v==3);}
void reporter(void*){++exits;LLToolMgr::getInstance()->clearTransientTool();picking=false;require(takeover?picker.mHitObjectID.isNull():picker.mHitObjectID.v==3);}
int delivered=0;void gaze(void*){if(picker.mCompletedPick)delivered=picker.mHitObjectID.v;++exits;picker.setExitCallback(nullptr,nullptr);LLToolMgr::getInstance()->clearTransientTool();}
struct LLVOAvatar { LLUUID id{1}; LLUUID getID(){return id;} }; LLVOAvatar avatar;
bool actorAlive=true,targetAlive=true,member=true,deleting=false,retired=false,objectAlive=true;
LLUUID gAgentID{9};
LLVOAvatar* find_avatar_from_object(LLUUID){return actorAlive?&avatar:nullptr;}
struct ALAvatarSubjectPicker { static bool valid(LLVOAvatar* p,bool,bool){return p!=nullptr;} };
struct LLDirectorCast { static LLDirectorCast& instance(){static LLDirectorCast c;return c;} bool contains(LLUUID){return member;} LLVOAvatar* resolve(LLUUID,bool){return targetAlive?&avatar:nullptr;} };
struct ALGhostStudio { static ALGhostStudio& instance(){static ALGhostStudio s;return s;} bool isDeleting(LLUUID){return deleting;} bool isPrivateOverlayRuntime(LLUUID){return retired;} };
struct Vec {int v=0;bool operator==(const Vec& b)const{return v==b.v;}static Vec zero;};Vec Vec::zero;using LLVector3=Vec;using LLVector3d=Vec;using F32=float;
namespace ALGazeMath {struct AnatomicalLimitProfile PROFILE;}
struct LLActorMover {struct GazeTarget TARGET;};
struct LLViewerObject { bool isDead(){return false;} } object;
struct Objects { LLViewerObject* findObject(LLUUID){return objectAlive?&object:nullptr;} } gObjectList;
struct ALGazeActor { static void setTarget(LLUUID,const LLActorMover::GazeTarget&){} static void setEnabled(LLUUID,bool){} static LLUUID menuObjectTarget(LLUUID,void* p){return LLUUID{p?3:0};} };
"""
    test = r"""
int main(){
 for(auto old : {particle,reporter}) {
  // Default successful exit must still deliver the original picked UUID.
  takeover=false;exits=0;button=false;picking=true;
  picker.setExitCallback(old,nullptr,false);LLToolMgr::getInstance()->start();picker.mHitObjectID={3};
  picker.setMouseCapture(false);
  require(exits==1&&!picker.capture&&!LLToolMgr::getInstance()->transient);
  require(old==particle?button:!picking);
  takeover=true;exits=0;button=false;picking=true;
  picker.setExitCallback(old,nullptr,false);LLToolMgr::getInstance()->start();picker.mHitObjectID={3};
  picker.setExitCallback(gaze,nullptr,true);
  require(exits==1&&!picker.capture&&!LLToolMgr::getInstance()->transient);
  require(old==particle?button:!picking);
  require(picker.mExitCallback==gaze);
  LLToolMgr::getInstance()->start();require(picker.capture);
  picker.cancelPick();require(exits==2);
 }
 // Each direction must reject disappearance of its destination snapshot. A false
 // preflight lets history discard the step and expose the older unrelated entry.
 for(bool redo : {false,true}) for(int missing=0;missing<6;++missing){
  actorAlive=targetAlive=member=objectAlive=true;deleting=retired=false;
  LLUUID actor{1}; LLActorMover::GazeTarget before,after;
  auto& destination=redo?after:before;
  destination.mMode=missing==1?LLActorMover::GazeTarget::OBJECT:LLActorMover::GazeTarget::CAST_MEMBER;
  destination.mObjectRef={3};destination.mCastRef={2};
  auto valid=[actor,before,after](bool redo,std::string& reason) VALIDATOR;
  std::string reason;require(valid(redo,reason));
  if(missing==0)actorAlive=false;if(missing==1)objectAlive=false;
  if(missing==2)targetAlive=false;if(missing==3)deleting=true;
  if(missing==4)retired=true;if(missing==5)member=false;
  require(!valid(redo,reason));require(!reason.empty());
 }
 std::puts("PASS: native async ordering, timeout, captureless cleanup, complete no-op history and legacy/preflight regressions");
}
""".replace('VALIDATOR', validator)
    prelude = prelude.replace('PROFILE', body(src['algazemath.h'], 'struct AnatomicalLimitProfile')).replace('TARGET', body(src['llactormover.h'], 'struct GazeTarget'))
    equal = 'bool avatarGazeTargetsEqual(const LLActorMover::GazeTarget& a, const LLActorMover::GazeTarget& b)' + body(src['alghostclonemenu.cpp'], 'bool avatarGazeTargetsEqual(')
    # Use the whole production record helper, including the real early-return and
    # recordCustom call, against a history double with unrelated redo available.
    history = equal + r"""
int redoEntries=1;
struct History {template<class Replay,class Valid>void recordCustom(void*,const char*,const char*,Replay,bool,Valid){redoEntries=0;}};
struct ALKitShell {History h;History& history(){return h;}} shell;
struct LLFloater {template<class T>T* findChild(const char*){return &shell;}} director;
struct LLFloaterReg {static LLFloater* findInstance(const char*){return &director;}};
""" + 'void recordAvatarGazeHistory(const LLUUID& actor,const LLActorMover::GazeTarget& before,bool before_on,const LLActorMover::GazeTarget& after,bool after_on)' + act
    field_changes = []
    target_fields = re.findall(r'^\s*(\S+)\s+(m\w+)\s*(?:=|;)', body(src['llactormover.h'], 'struct GazeTarget'), re.M)
    for kind, field in target_fields:
        if field == 'mLimitProfile':
            continue
        change = ('= LLActorMover::GazeTarget::MOTION' if kind == 'EMode' else
                  '= !after.' + field if kind == 'bool' else
                  '+= "changed"' if kind == 'std::string' else
                  '.v += 1' if kind in ('LLUUID', 'LLVector3', 'LLVector3d') else '+= 1')
        field_changes.append('after=before;after.'+field+change+';require(!avatarGazeTargetsEqual(before,after));redoEntries=1;recordAvatarGazeHistory({1},before,true,after,true);require(redoEntries==0);')
    for field in re.findall(r'F32\s+(m\w+)\s*=', body(src['algazemath.h'], 'struct AnatomicalLimitProfile')):
        field_changes.append('after=before;after.mLimitProfile.'+field+'+=1;require(!avatarGazeTargetsEqual(before,after));')
    test = test.replace('int main(){', r"""
int main(){
 LLActorMover::GazeTarget before,after;
 // Camera is already checked; an unrelated edit has just been undone.
 before.mMode=LLActorMover::GazeTarget::CAMERA;after=before;redoEntries=1;
 recordAvatarGazeHistory({1},before,true,after,true);require(redoEntries==1);
 recordAvatarGazeHistory({1},before,false,after,true);require(redoEntries==0);
 for(int order=0;order<4;++order){
  exits=delivered=0;window.pending.clear();LLEventTimer::timers.clear();
  picker.setExitCallback(gaze,nullptr,true);LLToolMgr::getInstance()->start();
  picker.handleMouseDown(1,2,0);require(window.pending.size()==1);auto result=window.pending.back();
  if(order==0)LLToolObjPicker::pickCallback(result);
  picker.handleMouseUp(1,2,0);
  if(order==0){require(exits==1&&delivered==3);continue;}
  require(exits==0&&picker.mSessionLive);
  if(order==1){LLToolObjPicker::pickCallback(result);require(exits==1&&delivered==3);LLEventTimer::fire();require(exits==1);}
  if(order==2){
   require(picker.handleKey(KEY_ESCAPE,7));require(exits==1&&delivered==0);
   picker.setExitCallback(gaze,nullptr,true);LLToolMgr::getInstance()->start();picker.handleMouseDown(1,2,0);
   LLToolObjPicker::pickCallback(result);require(picker.mHitObjectID.isNull());
   picker.handleMouseUp(1,2,0);require(exits==1);
   LLToolObjPicker::pickCallback(window.pending.back());require(exits==2&&delivered==3);
  }
  if(order==3){LLEventTimer::fire();require(exits==1&&delivered==0&&tips==1&&!picker.capture);LLToolObjPicker::pickCallback(result);require(picker.mHitObjectID.isNull());}
 }
 // Simulate the viewer-window testing path removing capture with no callback.
 for(bool deselect:{false,true}){
  exits=0;picker.setExitCallback(gaze,nullptr,true);LLToolMgr::getInstance()->start();picker.capture=false;
  if(deselect)picker.handleDeselect();else LLToolMgr::getInstance()->clearTransientTool();
  require(exits==1&&!picker.mSessionLive&&picker.mExitCallback==nullptr);picker.cancelPick();require(exits==1);
 }
""")
    test = test.replace(' for(int order=0;', '\n'.join(field_changes) + '\n for(int order=0;')
    real = prelude + methods + history + test
    result = compile_run(out, real, 'avatar_gaze_real')
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip())
    if negatives:
        mutants = [
            ('no_op_erases_redo', 'if (before_on == after_on && avatarGazeTargetsEqual(before, after)) return;', ''),
            ('stale_generation', 'pick_info.mPickGeneration != picker->mPickGeneration', 'false'),
            ('release_drops_pending', 'if (mPickPending)\n', 'if (false)\n'),
            ('timeout_missing', 'picker->cancelPick();', '(void)0;'),
            ('deselect_cleanup_missing', 'if (mCancelOnEscape && mSessionLive) cancelPick();', ''),
            ('missing_capture_cleanup', 'if (mCancelOnEscape && mSessionLive) finishSession();', ''),
            ('modified_escape', 'mCancelOnEscape && key == KEY_ESCAPE', 'mCancelOnEscape && key == KEY_ESCAPE && mask == 0'),
            ('particle_takeover', 'if (mSessionLive)', 'if (mCancelOnEscape && mSessionLive)'),
            ('reporter_callback', 'mExitCallback(mExitCallbackData);', '(void)mExitCallbackData;'),
            ('capture_release', 'if (hasMouseCapture()) setMouseCapture(false);', '(void)0;'),
            ('undo_always_true', 'auto valid=[actor,before,after](bool redo,std::string& reason) '+validator,
             'auto valid=[](bool,std::string&){return true;}'),
            ('redo_wrong_snapshot', 'const auto& saved = redo ? after : before;', 'const auto& saved = before;'),
        ]
        for label, old, new in mutants:
            assert old in real, label
            result = compile_run(out, real.replace(old,new), 'avatar_gaze_'+label)
            assert result.returncode == 2, ('native negative survived',label,result.stdout,result.stderr)
            print('PASS: native planted negative:', label)


def run_check():
    bad = findings(sources())
    for message in bad:
        print('FAIL:', message)
    if not bad:
        print('PASS: avatar gaze menu source contract')
        native_regressions(sources())
    return int(bool(bad))


def selftest():
    src = sources()
    mutations = [
        ('no-op truncates redo', 'alghostclonemenu.cpp', 'if (before_on == after_on && avatarGazeTargetsEqual(before, after)) return;', ''),
        ('incomplete target equality', 'alghostclonemenu.cpp', 'a.mPickedSpot == b.mPickedSpot &&', ''),
        ('generation transport lost', 'llviewerwindow.cpp', 'pick_info.mPickGeneration = pick_generation;', ''),
        ('stale pick accepted', 'lltoolobjpicker.cpp', 'pick_info.mPickGeneration != picker->mPickGeneration', 'false'),
        ('captureless cancel lost', 'lltoolobjpicker.cpp', 'if (mCancelOnEscape && mSessionLive) finishSession();', ''),
        ('particle session takeover bypassed', 'lltoolobjpicker.cpp', 'if (mSessionLive)', 'if (mCancelOnEscape && mSessionLive)'),
        ('reporter exit callback lost', 'lltoolobjpicker.cpp', 'mExitCallback(mExitCallbackData);', ''),
        ('takeover capture retained', 'lltoolobjpicker.cpp', '        cancelPick();', ''),
        ('takeover previous tool retained', 'lltoolobjpicker.cpp', '        LLToolMgr::getInstance()->clearTransientTool();', ''),
        ('undo validator removed', 'alghostclonemenu.cpp', '}, false, valid);', '});'),
        ('redo validator always true', 'alghostclonemenu.cpp', '}, false, valid);', '}, false, [](bool, std::string&) { return true; });'),
        ('undo validates wrong state', 'alghostclonemenu.cpp', 'const auto& saved = redo ? after : before;', 'const auto& saved = after;'),
        ('redo validates wrong state', 'alghostclonemenu.cpp', 'const auto& saved = redo ? after : before;', 'const auto& saved = before;'),
        ('card rejects own attachments', 'algazeactor.h', '    if (!object) return LLUUID::null;', '    if (!object || object->isHUDAttachment()) return LLUUID::null;'),
        ('card loses root edit', 'algazeactor.h', 'return object->getRootEdit() ? object->getRootEdit()->getID() : object->getID();', 'return object->getID();'),
        ('cancel retains capture', 'lltoolobjpicker.cpp', 'if (hasMouseCapture()) setMouseCapture(false);', ''),
        ('cancel tool hook removed', 'lltoolmgr.cpp', 'LLToolObjPicker::getInstance()->cancelPick();', ''),
        ('pick without Gaze On', 'alghostclonemenu.cpp', 'else ALGazeActor::setTarget(actor, target, true);', 'else ALGazeActor::setTarget(actor, target);'),
        ('pick accepts own attachment', 'algazeactor.h', 'wearer->getID() == actor', 'false'),
        ('cancel writes history', 'alghostclonemenu.cpp', 'if (actor.isNull() || object.isNull()) return;', 'if (actor.isNull() || object.isNull()) { recordCustom(); return; }'),
        ('stale picked actor written', 'alghostclonemenu.cpp', 'avatar->getID() != captured_actor', 'false'),
        ('pick callback', MENUS[0], 'function="Avatar.PickGazeObject"', 'function="Avatar.Gaze"'),
        ('pick accepts avatar', 'alghostclonemenu.cpp', '!picked->isAvatar()', 'true'),
        ('tool change applies pick', 'lltoolobjpicker.cpp', 'void LLToolObjPicker::handleDeselect()\n{\n    mCompletedPick = false;', 'void LLToolObjPicker::handleDeselect()\n{'),
        ('branch identity', MENUS[0], 'label="Camera" name="Avatar Gaze Camera"', 'label="Lens" name="Avatar Gaze Camera"'),
        ('branch line endings', MENUS[0], '<context_menu label="Gaze" name="Avatar Gaze">\n', '<context_menu label="Gaze" name="Avatar Gaze">\r\n'),
        ('self Me visibility', MENUS[1], '<menu_item_check.on_visible function="Avatar.VisibleGazeMe" />', ''),
        ('self callback', 'alghostclonemenu.cpp', 'return sMenuAvatar != gAgentID;', 'return true;'),
        ('target enables', 'alghostclonemenu.cpp', 'ALGazeActor::setTarget(actor, target, true);', 'ALGazeActor::setTarget(actor, target);'),
        ('enable helper', 'algazeactor.h', 'if (enable) setEnabled(actor, true);', ''),
        ('stale cast', 'alghostclonemenu.cpp', 'LLDirectorCast::instance().contains(id) &&', 'true &&'),
        ('stale nearby', 'alghostclonemenu.cpp', 'avatarGazeNearbyCurrent(person, actor)))', 'true))'),
        ('shownearby', 'alghostclonemenu.cpp', 'if (!ALGazeNearby::nearbyAllowed()) return people;', ''),
        ('hidden names', 'alghostclonemenu.cpp', 'if (nameHiddenByRlva(id)) return "Unknown";', ''),
        ('nearby label', 'alghostclonemenu.cpp', 'ALGazeNearby::personDisplayName(avatar->getID());', 'avatar->getFullname();'),
        ('Subject write', 'algazeactor.h', 'if (enable) setEnabled(actor, true);', 'LLDirectorCast::instance().setSubjectA(actor);'),
        ('history identity', 'alghostclonemenu.cpp', 'body->getID() != actor', 'false'),
        ('history enable restore', 'alghostclonemenu.cpp', 'ALGazeActor::setEnabled(actor, redo ? after_on : before_on);', ''),
        ('history target staleness', 'alghostclonemenu.cpp', 'cast.resolve(saved.mCastRef, false)', 'body'),
        ('history deleting clone', 'alghostclonemenu.cpp', 'isDeleting(saved.mCastRef)', 'isDeleting(actor)'),
        ('off checks', 'alghostclonemenu.cpp', 'if (kind == "on" || !on) return on;', 'if (kind == "on") return on;'),
        ('population on idle', 'alghostclonemenu.cpp', 'void populateAvatarGazeMenus(LLContextMenu* menu)', 'void draw() { populateAvatarGazeMenus(nullptr); }\nvoid populateAvatarGazeMenus(LLContextMenu* menu)'),
        ('object self attachment', 'algazeactor.h', 'wearer->getID() == actor', 'false'),
        ('camera head mode', 'alghostclonemenu.cpp', 'kind == "camera") target.mMode = LLActorMover::GazeTarget::CAMERA;', 'kind == "camera") target.mMode = LLActorMover::GazeTarget::MOTION;'),
        ('wrong menu handler', MENUS[0], 'function="Avatar.Gaze" parameter="camera"', 'function="Avatar.SelectInDirector" parameter="camera"'),
        ('prebuild wiring', 'director_kit_gen.py', 'clone_menu_check, avatar_gaze_menu_check):', 'clone_menu_check):'),
    ]
    failures = []
    for label, name, old, new in mutations:
        mutated = dict(src)
        mutated[name] = src[name].replace(old, new)
        if mutated[name] == src[name] or not findings(mutated):
            failures.append(label)
        else:
            print('PASS: rejected planted negative:', label)
    for label in failures:
        print('SELFTEST FAIL:', label)
    if not failures:
        native_regressions(src, negatives=True)
    return int(bool(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    return selftest() if args.selftest else run_check()


if __name__ == '__main__':
    sys.exit(main())
