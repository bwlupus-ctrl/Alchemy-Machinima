"""Run production hot-combo refresh/commit and revision gate with native UI spies.

No viewer build. The fixture extracts the production method bodies; only widgets,
model storage, focus and time are adapters. --negative-check proves removed guards fail.
"""
import argparse
from native_check_tools import ROOT, compile_run, check_directory


def block(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def fixture():
    ui = (ROOT / 'indra/newview/alpanellensgaze.cpp').read_text(encoding='utf-8')
    helpers = (ROOT / 'indra/newview/aluirefresh.h').read_text(encoding='utf-8')
    # Keep the callback wiring covered as well as exercising the receiving method.
    assert 'commitHotControls(source);' in block(ui, 'void ALPanelLensGaze::buildHotControls()')
    code = r'''
#include <algorithm>
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <set>
#include <string>
#include <vector>
using U64=unsigned long long; using S32=int; using F32=float;
struct LLUUID {
 std::string value; LLUUID()=default; LLUUID(const std::string& s):value(s=="null"?"":s){}
 bool isNull()const{return value.empty();} bool notNull()const{return !isNull();}
 std::string asString()const{return isNull()?"null":value;}
 bool operator==(const LLUUID&)const=default; bool operator<(const LLUUID& b)const{return value<b.value;}
 static const LLUUID null;
}; const LLUUID LLUUID::null; const LLUUID gAgentID("agent");
struct LLSD {
 std::string text; double number=0;
 LLSD()=default; LLSD(const char* s):text(s){} LLSD(std::string s):text(s){}
 LLSD(LLUUID u):text(u.asString()){} LLSD(int n):text(std::to_string(n)),number(n){}
 LLSD(float n):text(std::to_string(n)),number(n){} LLSD(bool n):LLSD(int(n)){}
 std::string asString()const{return text;} LLUUID asUUID()const{return LLUUID(text);}
 int asInteger()const{return int(number);} double asReal()const{return number;} bool asBoolean()const{return number!=0;}
};
struct LLView {bool focus=false,capture=false;};
struct FocusMgr {bool childHasKeyboardFocus(const LLView* v){return v->focus;}
 bool childHasMouseCapture(const LLView* v){return v->capture;}}gFocusMgr;
struct LLUICtrl:LLView {
 LLSD value;bool enabled=true;std::string text;LLSD getValue()const{return value;}
 virtual void setValue(const LLSD& v){value=v;} virtual ~LLUICtrl()=default;
};
struct LLComboBox:LLUICtrl {
 struct Item{std::string label;LLSD value;};std::vector<Item> items;int selected=-1,removals=0;
 int getCurrentIndex()const{return selected;}
 std::string getSelectedItemLabel()const{return selected<0?"":items[selected].label;}
 void removeall(){++removals;items.clear();selected=-1;value={};}
 void add(const std::string& label,const LLSD& v){items.push_back({label,v});if(items.size()==1)setValue(v);}
 bool valueExists(const std::string& v)const{for(const auto& item:items)if(item.value.asString()==v)return true;return false;}
 LLSD getSelectedValue(){return getValue();}void setSelectedByValue(const LLSD& v,bool){setValue(v);}
 void setValue(const LLSD& v)override{for(int i=0;i<int(items.size());++i)if(items[i].value.asString()==v.asString()){selected=i;value=items[i].value;return;}selected=-1;value={};}
};
bool alValueMatches(const LLUICtrl* w,const LLSD& a,const LLSD& b){auto* c=dynamic_cast<const LLComboBox*>(w);return (!c||c->selected>=0)&&a.asString()==b.asString();}
void setEnabledIfChanged(LLUICtrl* w,bool v){w->enabled=v;}
void setTextIfChanged(LLUICtrl* w,const std::string& v){w->text=v;}
struct LLFrameTimer {static inline float now=0;float start=now;float getElapsedTimeF32(){return now-start;}void reset(){start=now;}};
bool force_always=false;bool alUiRefreshForceAlways(){return force_always;}
'''
    code += block(helpers, 'inline bool alIsBeingEdited(') + '\n'
    code += block(helpers, 'template <class Ctrl>\ninline bool setValueIfChanged(') + '\n'
    code += block(helpers, 'struct ALRevisionGate\n') + ';\n'
    code += r'''
struct LLVector3 {float mV[3]{};void set(float x,float y,float z){mV[0]=x;mV[1]=y;mV[2]=z;}};
struct LLActorMover {struct GazeTarget {
 enum EMode {TANGENT,CAMERA,CAST_MEMBER,FIXED_POINT,OBJECT};EMode mMode=CAMERA;
 LLUUID mCastRef;bool mPickedSpot=false;LLVector3 mPointNudge;
};};
namespace ALGazeHot {
'''
    code += block((ROOT / 'indra/newview/algazehotslots.h').read_text(encoding='utf-8'), 'struct Slot\n') + ';\n}\n'
    code += r'''
struct LLDirectorCast {
 U64 revision=1; std::array<ALGazeHot::Slot,4> slots;
 struct Actor {LLUUID mId;std::string mLastName;};std::vector<Actor> actors{{LLUUID("actor"),"Actor"}};
 std::vector<std::string> groups{"Group"};
 static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 U64 getCastRevision(){return revision;}const auto& getCast(){return actors;}
 const auto& getGroupNames(){return groups;}const auto& getHotGazeSlots(){return slots;}
 void setHotGazeSlot(int i,const ALGazeHot::Slot& slot){slots[i]=slot;}
};
struct ALGhostStudio {
 U64 revision=1;enum {BACKING_ENTITY_CLONE};struct Clone{int mKind=BACKING_ENTITY_CLONE;LLUUID mEntityId,mId;std::string mName;};
 std::vector<Clone> clones{{BACKING_ENTITY_CLONE,LLUUID("clone"),LLUUID("clone-id"),"Clone"}};
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 U64 getInstanceRevision(){return revision;}const auto& getInstances(){return clones;}
};
void cancelHotGazePick(void*){}
struct ALPanelLensGaze;
std::set<ALPanelLensGaze*>& actorGazePanels(){static std::set<ALPanelLensGaze*> panels;return panels;}
struct ALPanelLensGaze {
 LLComboBox* mCast=nullptr;std::vector<LLUUID> mSelected;LLUUID mCastComboActor,mCastComboSelection;
 U64 mSelectionRevision=0,mCastComboRevision=0;void refreshCastCombo(bool){}
 int mHotSlotIndex=0;ALRevisionGate mHotChoiceGate;
 LLComboBox slot,who,target,actor,spot,mode;
 LLComboBox *mHotSlot=&slot,*mHotWho=&who,*mHotTarget=&target,*mHotActor=&actor,*mHotSpot=&spot,*mHotMode=&mode;
 LLUICtrl after,snap,dwell,ret,head,eyes,torso,name,pick,x,y,z;
 LLUICtrl *mHotAfter=&after,*mHotSnap=&snap,*mHotDwell=&dwell,*mHotReturn=&ret,*mHotHead=&head,*mHotEyes=&eyes,*mHotTorso=&torso,*mHotName=&name,*mHotPick=&pick,*mHotX=&x,*mHotY=&y,*mHotZ=&z;
 ALPanelLensGaze(){actorGazePanels().insert(this);for(int i=0;i<5;++i)target.add("Mode",i);for(int i=0;i<4;++i)slot.add("Slot",i);spot.add("Origin",0);spot.add("Spot",1);mode.add("Tap",0);mode.add("Hold",1);}
 ~ALPanelLensGaze(){actorGazePanels().erase(this);}
 static void rewriteActorReferences(const LLUUID& old_id,const LLUUID& new_id);
 void refreshHotControls(bool force_choices=false);void commitHotControls(LLUICtrl* source);
};
'''
    code += block(ui, 'bool hotActorAvailable(') + '\n'
    code += block(ui, 'bool hotWhoAvailable(') + '\n'
    code += block(ui, 'void ALPanelLensGaze::rewriteActorReferences(') + '\n'
    code += block(ui, 'void ALPanelLensGaze::refreshHotControls(') + '\n'
    code += block(ui, 'void ALPanelLensGaze::commitHotControls(') + '\n'
    code += r'''
void require(bool pass,const char* message){if(!pass){std::cerr<<message<<"\n";std::exit(1);}}
void seed(){auto& s=LLDirectorCast::instance().slots[0];s={};s.mWho=LLUUID("actor");s.mTarget.mMode=LLActorMover::GazeTarget::CAST_MEMBER;s.mTarget.mCastRef=LLUUID("actor");s.mHasTarget=true;s.mTargetName="Actor";}
void selections(ALPanelLensGaze& p){require(p.who.getValue().asString()=="actor","Who selection lost");require(p.actor.getValue().asUUID()==LLUUID("actor"),"Actor selection lost");}
void timer_test(){
 seed();ALPanelLensGaze p;p.refreshHotControls();selections(p);
 for(int focus=0;focus<3;++focus){p.who.focus=focus==1;p.actor.focus=focus==2;LLFrameTimer::now+=1.1f;
 int before=p.who.removals;p.refreshHotControls();selections(p);require(p.who.removals==before&&p.actor.removals==before,"Timer-only refresh rebuilt combos");}
 // Debug forcing also must not destroy choice state.
 force_always=true;p.who.focus=p.actor.focus=false;p.refreshHotControls();selections(p);require(p.who.removals==1,"Force-only refresh rebuilt combos");force_always=false;
}
void pending_test(){
 for(int which=0;which<2;++which){seed();ALPanelLensGaze p;p.refreshHotControls();auto& cast=LLDirectorCast::instance();auto& studio=ALGhostStudio::instance();
 LLView* editing=which?static_cast<LLView*>(&p.actor):&p.who;editing->focus=!which;editing->capture=which;
 ++cast.revision;++studio.revision;cast.groups.push_back("New group");
 int before=p.who.removals;p.refreshHotControls();require(p.who.removals==before,"Rebuilt while editing");selections(p);
 editing->focus=editing->capture=false;p.refreshHotControls();require(p.who.removals==before+1,"Consumed pending revisions while editing");selections(p);
 require(p.who.items.back().label=="Group: New group","Group membership not refreshed");cast.groups.pop_back();
 }
}
void commit_test(){
 seed();ALPanelLensGaze p;p.refreshHotControls();p.actor.setValue(LLUUID::null);p.snap.setValue(.7f);p.commitHotControls(p.mHotSnap);
 const auto& s=LLDirectorCast::instance().slots[0];require(s.mHasTarget&&s.mTarget.mCastRef==LLUUID("actor")&&s.mTargetName=="Actor","Other control commit cleared actor");require(s.mSnap==.7f,"Other control did not commit");
 // Unselected/empty text is not an explicit clear, even from the actor callback.
 p.actor.selected=-1;p.actor.value={};p.commitHotControls(p.mHotActor);require(s.mHasTarget,"Unselected text cleared actor");
 // Selecting a real actor and explicitly choosing the placeholder retain their semantics.
 p.actor.setValue(LLUUID("clone"));p.commitHotControls(p.mHotActor);require(s.mTarget.mCastRef==LLUUID("clone"),"Actor selection did not commit");
 p.actor.setValue(LLUUID::null);p.commitHotControls(p.mHotActor);require(!s.mHasTarget&&s.mTarget.mCastRef.isNull(),"Explicit clear did not commit");
}
void reset_models(){
 auto& c=LLDirectorCast::instance();c.actors={{LLUUID("actor"),"Actor"}};c.groups={"Group"};++c.revision;
 auto& g=ALGhostStudio::instance();g.clones={{ALGhostStudio::BACKING_ENTITY_CLONE,LLUUID("clone"),LLUUID("clone-id"),"Clone"}};++g.revision;seed();
}
void rewrite_test(bool retire){
 for(bool clone:{false,true}){
  reset_models();auto& c=LLDirectorCast::instance();auto& g=ALGhostStudio::instance();auto& s=c.slots[0];
  const LLUUID old_id(clone?"clone":"actor"),new_id=retire?LLUUID::null:LLUUID("replacement");
  s.mWho=s.mTarget.mCastRef=old_id;s.mTargetName=clone?"Clone":"Actor";
  ALPanelLensGaze p;p.refreshHotControls();p.who.focus=true;p.actor.capture=true;
  // Model/slots migrate before the production panel identity notification.
  if(clone){if(retire)g.clones.clear();else g.clones[0].mEntityId=new_id;++g.revision;}
  else{if(retire)c.actors.clear();else c.actors[0].mId=new_id;++c.revision;}
  s.mWho=s.mTarget.mCastRef=new_id;if(retire){s.mHasTarget=false;s.mTargetName.clear();}
  ALPanelLensGaze::rewriteActorReferences(old_id,new_id);
  require(p.who.getCurrentIndex()>=0&&p.actor.getCurrentIndex()>=0,"Identity rewrite lost selection");
  require(p.who.getValue().asString()==(retire?"":new_id.asString())&&p.actor.getValue().asUUID()==new_id,"Identity rewrite left focused hot selections stale");
  require(!p.who.valueExists(old_id.asString())&&!p.actor.valueExists(old_id.asString()),"Retired identity remains selectable");
  p.snap.setValue(.6f);p.commitHotControls(p.mHotSnap);
  require(s.mWho==new_id&&s.mTarget.mCastRef==new_id,"Unrelated commit undid identity rewrite");
 }
}
void removal_test(){
 reset_models();ALPanelLensGaze p;p.refreshHotControls();auto& c=LLDirectorCast::instance();auto& s=c.slots[0];
 c.actors.clear();++c.revision;p.refreshHotControls();
 require(p.who.getCurrentIndex()==0&&p.who.getValue().asString().empty(),"Removed Who did not show placeholder");
 require(p.actor.getCurrentIndex()==0&&p.actor.getValue().asUUID().isNull(),"Removed Actor did not show placeholder");
 require(p.name.text=="No target","Removed target still displays old name");
 p.snap.setValue(.8f);p.commitHotControls(p.mHotSnap);
 require(s.mWho==LLUUID("actor")&&s.mHasTarget&&s.mTarget.mCastRef==LLUUID("actor")&&s.mTargetName=="Actor","Removal/unrelated commit deleted saved slot data");
 p.commitHotControls(p.mHotActor);require(!s.mHasTarget&&s.mTarget.mCastRef.isNull(),"Explicit placeholder no longer clears unavailable target");
 p.commitHotControls(p.mHotWho);require(s.mWho.isNull(),"Explicit Who placeholder no longer clears");
}
void deferred_commit_test(bool who){
 reset_models();ALPanelLensGaze p;p.refreshHotControls();auto& c=LLDirectorCast::instance();auto& s=c.slots[0];
 // A deferred UI can hold a different, nonempty, still-valid choice.
 p.who.focus=true;p.actor.capture=true;++c.revision;
 s.mWho=s.mTarget.mCastRef=LLUUID("clone");s.mTargetName="Clone";p.refreshHotControls();
 require(p.who.getValue().asString()=="actor"&&p.actor.getValue().asUUID()==LLUUID("actor"),"Deferred setup did not retain old selections");
 p.snap.setValue(.9f);p.commitHotControls(p.mHotSnap);
 require((who?s.mWho:s.mTarget.mCastRef)==LLUUID("clone"),"Unrelated commit authored stale nonempty selection");
 require(s.mHasTarget&&s.mTargetName=="Clone","Unrelated commit changed saved target name");
 p.who.focus=p.actor.capture=false;p.refreshHotControls();
 require(p.who.getValue().asString()=="clone"&&p.actor.getValue().asUUID()==LLUUID("clone"),"Pending rebuild did not restore current slot");
}
void validation_test(bool who){
 for(bool clone:{false,true}){
  reset_models();ALPanelLensGaze p;p.refreshHotControls();auto& c=LLDirectorCast::instance();auto& g=ALGhostStudio::instance();auto& s=c.slots[0];
  const LLUUID stale(clone?"clone":"actor");p.who.setValue(stale.asString());p.actor.setValue(stale);
  p.who.focus=p.actor.capture=true;
  if(clone){g.clones.clear();++g.revision;}else{c.actors.clear();++c.revision;}
  s.mWho=s.mTarget.mCastRef=gAgentID;s.mTargetName="You";
  p.commitHotControls(who?p.mHotWho:p.mHotActor);
  require((who?s.mWho:s.mTarget.mCastRef)==gAgentID,"Explicit stale choice escaped model validation");
 }
 if(who){
  reset_models();ALPanelLensGaze p;p.refreshHotControls();auto& c=LLDirectorCast::instance();auto& s=c.slots[0];
  p.who.setValue("group:Group");p.who.focus=true;c.groups.clear();++c.revision;p.commitHotControls(p.mHotWho);
  require(s.mGroup.empty()&&s.mWho==LLUUID("actor"),"Explicit removed group escaped model validation");
 }
}
void gate_test(int index){
 ALRevisionGate gate;std::array<U64,9> r{1,2,3,4,5,6,7,8,9};
 auto changed=[&](){return gate.changed({r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8]});};
 require(changed(),"Initial gate did not open");require(!changed(),"Unchanged gate opened");++r[index];require(changed()&&!gate.lastWasFallback(),"Gate ignored input 7..9");require(!changed(),"Gate did not accept revision");
 LLFrameTimer::now+=1.1f;require(changed()&&gate.lastWasFallback(),"Gate fallback changed behavior");
 gate.reset();require(changed()&&!gate.lastWasFallback(),"Gate reset changed behavior");
 require(gate.changed({1,2}),"Gate ignored revision count change");require(!gate.changed({1,2}),"Gate failed to accept count change");
}
int main(int argc,char** argv){
 std::string test=argc>1?argv[1]:"all";
 if(test=="all"||test=="timer")timer_test();if(test=="all"||test=="pending")pending_test();if(test=="all"||test=="commit")commit_test();
 if(test=="all"||test=="rekey")rewrite_test(false);if(test=="all"||test=="retire")rewrite_test(true);
 if(test=="all"||test=="removal")removal_test();
 if(test=="all"||test=="deferred_who")deferred_commit_test(true);if(test=="all"||test=="deferred_actor")deferred_commit_test(false);
 if(test=="all"||test=="validate_who")validation_test(true);if(test=="all"||test=="validate_actor")validation_test(false);
 for(int i=6;i<9;++i)if(test=="all"||test==std::to_string(i+1))gate_test(i);
 std::cout<<"PASS: "<<test<<" (timer/deferred refresh, explicit commits, rekey/retire/removal, model validation, revision inputs 7..9)\n";
}
'''
    return code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--negative-check', action='store_true')
    args = parser.parse_args()
    code = fixture()
    directory = check_directory('hot_combo_refresh')
    result = compile_run(directory, code, 'hot_combo_refresh')
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='')
    if args.negative_check:
        mutations = {
            'timer': ('!mHotChoiceGate.lastWasFallback()', 'true', ['timer']),
            'focus': ('!alIsBeingEdited(mHotWho) && !alIsBeingEdited(mHotActor)', 'true', ['pending']),
            'commit': ('source == mHotActor && mHotActor->getCurrentIndex() >= 0 &&', 'true &&', ['commit', 'deferred_actor']),
            'who_commit': ('source == mHotWho && mHotWho->getCurrentIndex() >= 0 &&', 'true &&', ['deferred_who']),
            'validate_who': ('&& hotWhoAvailable(who)', '&& true', ['validate_who']),
            'validate_actor': ('actor.isNull() || hotActorAvailable(actor)', 'true', ['validate_actor']),
            'rewrite': ('panel->refreshHotControls(true);', 'panel->refreshHotControls();', ['rekey', 'retire']),
            'missing_who': ('mHotWho->valueExists(who) ? who : ""', 'who', ['removal']),
            'missing_actor': ('actor_available ? slot.mTarget.mCastRef : LLUUID::null', 'slot.mTarget.mCastRef', ['removal']),
            'missing_name': ('target_available ? slot.mTargetName : "No target"', 'slot.mHasTarget ? slot.mTargetName : "No target"', ['removal']),
            'gate': ('std::equal(now.begin(), now.end(), mSeen.begin())',
                     'std::equal(now.begin(), now.begin() + std::min(size_t(6), now.size()), mSeen.begin())', ['7', '8', '9']),
        }
        import subprocess
        for name, (old, new, tests) in mutations.items():
            assert code.count(old) == 1, name
            exe = compile_run(directory, code.replace(old, new), 'negative_' + name, run=False)
            for test in tests:
                result = subprocess.run([str(exe), test], capture_output=True, text=True)
                assert result.returncode != 0, 'regression survived: ' + name + '/' + test
                print('PASS: removed protection rejected: ' + name + '/' + test)


if __name__ == '__main__':
    main()
