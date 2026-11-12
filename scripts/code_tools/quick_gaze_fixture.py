"""Run the production This actor commits against actor/Cast and widget spies."""
from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags
from hot_combo_refresh_fixture import block
import copy
import json
import xml.etree.ElementTree as ET


def kit_checks():
    from director_kit_gen import DEFAULT_DATA, DEFAULT_OUT, VCAM_DATA, GenError, validate_data
    data = json.loads(DEFAULT_DATA.read_text(encoding='utf-8'))
    # The normal generation path substitutes the separately maintained VCam page before census.
    vcam = json.loads(VCAM_DATA.read_text(encoding='utf-8'))
    for group in data['groups']:
        if group['name'] == 'camera_group':
            group['pages'] = [vcam if p['name'] == 'vcam_tab' else p for p in group['pages']]
    validate_data(data)
    page = next(p for g in data['groups'] for p in g['pages'] if p['name'] == 'gaze_tab')
    assert page['cards'][0]['id'] == 'this_actor' and len(page['cards'][0]['rows']) == 8
    for delta in (-1, 1):
        broken = copy.deepcopy(data)
        p = next(p for g in broken['groups'] for p in g['pages'] if p['name'] == 'gaze_tab')
        rows = p['cards'][0]['rows']
        rows.pop() if delta < 0 else rows.append(copy.deepcopy(rows[-1]))
        try:
            validate_data(broken)
        except GenError as error:
            assert 'mockup completeness drift' in str(error), str(error)
        else:
            raise AssertionError('row census mutant survived: ' + str(delta))
    generated = ET.parse(DEFAULT_OUT / 'panel_dpage_performance_gaze.xml')
    cards = list(generated.iter('kit_card'))
    assert cards[0].get('id') == 'this_actor'
    subjects = next(c for c in cards if c.get('id') == 'gaze_subjects')
    assert all(r.get('advanced') == 'true' for r in subjects.iter('kit_row'))
    index = (DEFAULT_OUT / 'director_kit_index.xml').read_text(encoding='utf-8')
    for row in page['cards'][0]['rows']:
        assert '\t' + row['w'] + '\t' in index, row['w']
    modes = generated.find('.//combo_box[@name="hot_mode"]')
    assert [(i.get('label'), i.get('value')) for i in modes.findall('combo_box.item')] == [
        ('Auto', '2'), ('Tap', '0'), ('Hold', '1')]
    print('PASS: first This actor card, Subjects More options, eight searchable rows, Auto/Tap/Hold, census +/-1 negatives')


def fixture():
    ui = (ROOT / 'indra/newview/alpanellensgaze.cpp').read_text(encoding='utf-8')
    source = r'''
#include "linden_common.h"
#include "v3math.h"
#include "v3dmath.h"
#include "llsd.h"
#include <iostream>
#include <set>
#include <functional>
#include <memory>
const LLUUID gAgentID("00000000-0000-0000-0000-000000000001");
const LLUUID actorID("00000000-0000-0000-0000-000000000002");
const LLUUID targetID("00000000-0000-0000-0000-000000000003");
struct LLUICtrl {virtual ~LLUICtrl()=default;int callbacks=0;void setCommitCallback(std::function<void(LLUICtrl*,const LLSD&)>){++callbacks;}LLSD value;std::string text;bool enabled=true,focus=false;LLSD getValue(){return value;}};
struct LLTextBox:LLUICtrl{};
struct LLCheckBoxCtrl:LLUICtrl {bool get(){return value.asBoolean();}};
struct LLComboBox:LLUICtrl {
 struct Item{std::string label;LLSD value;};std::vector<Item> items;bool open=false;int index=0;
 int getCurrentIndex(){return index;}LLSD getSelectedValue(){return value;}
 void removeall(){items.clear();index=-1;value=LLSD();}
 void add(const std::string& label,const LLSD& v,int=0,bool=true){items.push_back({label,v});}
 bool valueExists(const std::string& v){for(const auto& i:items)if(i.value.asString()==v)return true;return false;}
 bool isListVisible(){return open;}void hideList(){open=false;}void setSelectedByValue(const LLSD& v,bool){value=v;}
};
constexpr int ADD_BOTTOM=0;
bool alIsBeingEdited(LLUICtrl* w){return w->focus;}
void setTextIfChanged(LLUICtrl* w,const std::string& v){w->text=v;}
void setEnabledIfChanged(LLUICtrl* w,bool v){w->enabled=v;}
void setToolTipIfChanged(LLUICtrl*,const std::string&){}void setLabelIfChanged(LLUICtrl*,const std::string&){}
struct LLButton:LLUICtrl{};struct LLSliderCtrl:LLUICtrl{};
template<class T,class V>void setValueIfChanged(T* w,const V& v,bool=false){w->value=LLSD(v);}
struct LLVOAvatar {bool dead=false,real=true;bool isDead(){return dead;}};
struct LLActorMover {
 struct GazeTarget {enum EMode {MOTION,CAMERA,CAST_MEMBER,FIXED_POINT,OBJECT};EMode mMode=CAMERA;
 LLUUID mCastRef,mObjectRef;LLVector3d mFixedPoint;float mIntensityOverride=-1;};
 static LLActorMover& instance(){static LLActorMover m;return m;}
 LLUUID enabledActor;bool enabled=false;bool isGazeEnabled(const LLUUID&){return enabled;}float getGazeIntensity(const LLUUID&){return 1.f;}
 void setGazeEnabled(const LLUUID& id,bool on){enabledActor=id;enabled=on;}
};
struct LLDirectorCast {
 static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 LLUUID active=actorID,edited,added,cameraActor,alias,aliasDestination;LLVOAvatar body;bool member=false,camera=false;
 int writes=0,adds=0;U64 revision=1;bool resolved=true;
 struct Member{LLUUID mId;std::string mLastName;};std::vector<Member> members{{targetID,"PRIVATE LEGACY"}};
 const Member* getMember(const LLUUID& id){for(const auto& m:members)if(m.mId==id)return &m;return nullptr;}
 const auto& getCast(){return members;}U64 getCastRevision(){return revision;}
 static bool isSelf(const LLUUID& id){return id==gAgentID;}
 bool isLookAtCamera(const LLUUID&){return camera;}
 LLActorMover::GazeTarget target;
 LLUUID getActiveActorId(){return active;}
 LLVOAvatar* resolve(const LLUUID&,bool){return resolved?&body:nullptr;}
 bool contains(const LLUUID& id){if(alias.notNull()&&id==alias)return contains(aliasDestination);return id==gAgentID||id==added?member:std::any_of(members.begin(),members.end(),[&](const auto& m){return m.mId==id;});}
 void add(const LLUUID& id){added=id;member=true;++adds;}
 auto getGazeTarget(const LLUUID&){return target;}
 void setGazeTarget(const LLUUID& id,const LLActorMover::GazeTarget& t){edited=id;target=t;++writes;}
 void setLookAtCamera(const LLUUID& id,bool on){cameraActor=id;camera=on;}
};
namespace ALAvatarSubjectPicker {bool valid(LLVOAvatar* a,bool,bool){return a&&a->real&&!a->dead;}}
bool namesVisible=true,nearbyAllowed=true;
namespace RlvActions {constexpr int SNC_DEFAULT=0;bool canShowName(int,const LLUUID&){return ::namesVisible;}}
namespace ALGazeNearby {constexpr const char* HEADER_VALUE="nearby-header";constexpr const char* HEADER_LABEL="Nearby";
 bool nearbyAllowed(){return ::nearbyAllowed;}std::string nonCastTargetName(const LLUUID&){return namesVisible&&::nearbyAllowed?"Display name":"";}
 struct Entry{LLUUID mId;std::string mName;};}
bool listedAmong(const std::vector<ALGazeNearby::Entry>& entries,const LLUUID& id){
 return std::any_of(entries.begin(),entries.end(),[&](const auto& e){return e.mId==id;});}
LLUUID comboValueId(const std::string& s){return s.empty()?LLUUID::null:LLUUID(s);}
struct LLViewerObject {LLUUID id=targetID;LLViewerObject* getRootEdit(){return this;}LLUUID getID(){return id;}};
struct Selection {LLViewerObject* object=nullptr;LLViewerObject* getPrimaryObject(){return object;}LLViewerObject* getFirstRootObject(){return object;}};
struct LLSelectMgr {static LLSelectMgr* getInstance(){static LLSelectMgr m;return &m;}Selection selection;Selection* getSelection(){return &selection;}};
struct LLViewerCamera {static LLViewerCamera* getInstance(){static LLViewerCamera c;return &c;}LLVector3 getOrigin(){return LLVector3(1,2,3);}};
struct Agent {LLVector3d getPosGlobalFromAgent(const LLVector3& v){return LLVector3d(v);}}gAgent;
struct ALGhostStudio {
 enum {BACKING_ENTITY_CLONE};struct Instance{int mKind=BACKING_ENTITY_CLONE;LLUUID mId,mEntityId;std::string mName;};
 std::vector<Instance> clones;U64 revision=1;
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 U64 getInstanceRevision(){return revision;}const auto& getInstances(){return clones;}
 const Instance* getInstance(const LLUUID& id)const{for(const auto& c:clones)if(c.mId==id)return &c;return nullptr;}
 const Instance* findInstanceByRuntime(const LLUUID& id)const{for(const auto& c:clones)if(c.mEntityId==id)return &c;return nullptr;}
};
bool nearbyNamesRevoked(const std::vector<ALGazeNearby::Entry>& shown){return !shown.empty()&&(!namesVisible||!nearbyAllowed);}
struct AutoRuntime{void rewriteActorReferences(const LLUUID&,const LLUUID&){}};
AutoRuntime& hotAutoSlots(){static AutoRuntime r;return r;}
struct ALPanelLensGaze;
std::set<ALPanelLensGaze*>& actorGazePanels(){static std::set<ALPanelLensGaze*> p;return p;}
float now=0;std::vector<std::pair<float,std::function<void()>>> timers;
void doAfterInterval(std::function<void()> f,float seconds){timers.push_back({now+seconds,f});}
void advance(float dt){now+=dt;auto pending=timers;timers.clear();for(auto& t:pending){if(t.first<=now)t.second();else timers.push_back(t);}}
struct ALPanelLensGaze {
 LLCheckBoxCtrl enable,camera;LLComboBox target,cast;LLButton pick;LLSliderCtrl strength;
 LLCheckBoxCtrl *mQuickEnable=&enable,*mQuickCamera=&camera;
 LLComboBox *mQuickTarget=&target,*mQuickCast=&cast;
 LLButton* mQuickPick=&pick;LLSliderCtrl* mQuickStrength=&strength;
 LLUUID mQuickNoticeActor,mQuickNamedActor;U64 mQuickNoticeGeneration=0,mQuickNamePoll=U64(-1),mQuickNameRevision=U64(-1),mPollGeneration=0;
 bool mQuickActorIsClone=false;std::string mQuickActorName;
 LLTextBox name,notice;LLTextBox *mQuickName=&name,*mQuickNotice=&notice;
 std::vector<ALGazeNearby::Entry> mQuickPeopleShown;
 std::shared_ptr<ALPanelLensGaze*> life=std::make_shared<ALPanelLensGaze*>(this);
 struct Handle{std::weak_ptr<ALPanelLensGaze*> life;ALPanelLensGaze* get()const{auto p=life.lock();return p?*p:nullptr;}};
 Handle getHandle(){return {life};}
 ALPanelLensGaze(){actorGazePanels().insert(this);}~ALPanelLensGaze(){actorGazePanels().erase(this);life.reset();}
 LLComboBox* mCast=nullptr;uuid_vec_t mSelected;LLUUID mCastComboActor,mCastComboSelection;
 U64 mSelectionRevision=0,mCastComboRevision=0;
 void refreshCastCombo(bool){}struct Gate{std::vector<U64> last;void reset(){last.clear();}bool changed(std::initializer_list<U64> v){std::vector<U64> n(v);bool c=n!=last;last=n;return c;}} mHotChoiceGate;
 void refreshHotControls(bool){}static void rewriteActorReferences(const LLUUID&,const LLUUID&);
 struct Projection{std::vector<ALGazeNearby::Entry> mList;}mQuickProjection;
 void nearbyFor(Projection&,const LLUUID&,const LLUUID&){}
 std::vector<ALGazeNearby::Entry> mQuickNearbyShown;
 Gate mQuickChoices;
 bool liveNearbyName(const LLUUID&,std::string& n){n="Display name";return ::namesVisible;}
 std::string missing;
 template<class T>T* findChild(const std::string& widget){
  if(widget==missing)return nullptr;LLUICtrl* result=nullptr;
  if(widget=="gaze_quick_name")result=&name;else if(widget=="gaze_quick_notice")result=&notice;
  else if(widget=="gaze_quick_enable")result=&enable;else if(widget=="gaze_quick_camera")result=&camera;
  else if(widget=="gaze_quick_target")result=&target;else if(widget=="gaze_quick_cast")result=&cast;
  else if(widget=="gaze_quick_pick")result=&pick;else if(widget=="gaze_quick_strength")result=&strength;
  return dynamic_cast<T*>(result);
 }
 template<class T>T* getChild(const std::string& widget){auto* result=findChild<T>(widget);if(!result)throw std::runtime_error("missing widget");return result;}
 void buildQuickControls();void refreshQuickControls(bool force=false);void showQuickNotice(const LLUUID&);
 LLUUID quickActor()const;void commitQuickControl(LLUICtrl*);
};
'''
    source += block(ui, 'void ALPanelLensGaze::buildQuickControls()') + '\n'
    source += block(ui, 'bool hotActorAvailable(') + '\n'
    source += block(ui, 'const ALGhostStudio::Instance* quickClone(') + '\n'
    source += 'std::string actorName(const LLUUID& id){const auto* m=LLDirectorCast::instance().getMember(id);return m?m->mLastName:"NONCAST LEGACY";}\n'
    source += block(ui, 'std::string quickPersonLabel(') + '\n'
    source += block(ui, 'void ALPanelLensGaze::refreshQuickControls(') + '\n'
    source += block(ui, 'void ALPanelLensGaze::showQuickNotice(') + '\n'
    source += block(ui, 'void ALPanelLensGaze::rewriteActorReferences(') + '\n'
    source += block(ui, 'LLUUID ALPanelLensGaze::quickActor() const') + '\n'
    source += block(ui, 'void ALPanelLensGaze::commitQuickControl(') + '\n'
    source += r'''
void require(bool ok,const char* message){if(!ok){std::cerr<<message<<"\n";std::exit(2);}}
void reset(){LLDirectorCast::instance()={};LLActorMover::instance()={};LLSelectMgr::getInstance()->selection.object=nullptr;namesVisible=true;nearbyAllowed=true;ALGhostStudio::instance()={};timers.clear();now=0;}
int main(){
 for(const char* missing:{"gaze_quick_name","gaze_quick_notice","gaze_quick_enable","gaze_quick_camera","gaze_quick_target","gaze_quick_cast","gaze_quick_pick","gaze_quick_strength"}){
  ALPanelLensGaze partial;partial.missing=missing;bool threw=false;try{partial.buildQuickControls();}catch(...){threw=true;}
  require(!threw&&partial.mQuickName==nullptr,"missing quick widget must disable whole optional card without throwing");
  require(partial.enable.callbacks+partial.camera.callbacks+partial.target.callbacks+partial.cast.callbacks+partial.pick.callbacks+partial.strength.callbacks==0,"partial card wired callbacks");
  partial.refreshQuickControls();partial.commitQuickControl(&partial.enable);
 }
 {ALPanelLensGaze complete;complete.buildQuickControls();require(complete.enable.callbacks+complete.camera.callbacks+complete.target.callbacks+complete.cast.callbacks+complete.pick.callbacks+complete.strength.callbacks==6,"complete quick card not wired");}
 reset();auto& c=LLDirectorCast::instance();ALPanelLensGaze p;
 p.strength.value=.4;p.commitQuickControl(&p.strength);
 require(c.adds==1&&c.added==actorID&&p.mQuickNoticeActor==actorID,"first edit must enrol and show notice");
 require(c.edited==actorID&&fabs(c.target.mIntensityOverride-.4f)<.0001,"quick strength must address active actor");
 p.enable.value=true;p.commitQuickControl(&p.enable);
 require(c.adds==1&&LLActorMover::instance().enabledActor==actorID&&LLActorMover::instance().enabled,"Gaze On must be per actor");
 p.camera.value=true;p.commitQuickControl(&p.camera);
 require(c.cameraActor==actorID&&c.camera,"non-Subject real avatar camera toggle failed");
 p.target.value=2;p.commitQuickControl(&p.target);require(c.target.mMode==2,"head mode failed");
 p.cast.value=targetID;p.cast.index=0;p.commitQuickControl(&p.cast);require(c.target.mCastRef==targetID,"cast target failed");
 c.active.setNull();p.enable.value=false;p.commitQuickControl(&p.enable);
 require(LLActorMover::instance().enabledActor==gAgentID,"empty bar must address You");
 reset();ALPanelLensGaze clone;c.body.real=false;clone.camera.value=true;clone.commitQuickControl(&clone.camera);
 require(c.adds==0&&c.cameraActor.isNull(),"clone/animesh camera commit must be refused");
 reset();ALPanelLensGaze hidden;namesVisible=false;c.members.clear();hidden.mQuickNearbyShown={{targetID,"Display name"}};
 hidden.cast.value=targetID;hidden.cast.index=0;hidden.commitQuickControl(&hidden.cast);
 require(c.adds==0&&c.writes==0,"hidden/stale nearby target must not enrol or write");
 hidden.cast.value=ALGazeNearby::HEADER_VALUE;hidden.cast.index=0;hidden.commitQuickControl(&hidden.cast);
 require(c.adds==0&&c.writes==0,"Nearby header must not enrol or write");
 reset();ALPanelLensGaze picker;c.target.mMode=LLActorMover::GazeTarget::OBJECT;picker.commitQuickControl(&picker.pick);
 require(c.adds==0&&c.writes==0,"empty object selection must not enrol");
 LLViewerObject object;LLSelectMgr::getInstance()->selection.object=&object;picker.commitQuickControl(&picker.pick);
 require(c.target.mObjectRef==targetID&&c.edited==actorID,"object Pick failed");
 c.target.mMode=LLActorMover::GazeTarget::FIXED_POINT;picker.commitQuickControl(&picker.pick);
 require(c.target.mFixedPoint==LLVector3d(1,2,3),"fixed point Pick failed");
 reset();ALPanelLensGaze noop;c.target.mMode=LLActorMover::GazeTarget::CAMERA;noop.commitQuickControl(&noop.pick);
 require(c.adds==0&&c.writes==0,"unsupported Pick mode enrolled");LLUICtrl unknown;noop.commitQuickControl(&unknown);
 require(c.adds==0&&c.writes==0,"unknown source enrolled");
 reset();namesVisible=true;require(quickPersonLabel(targetID)=="Display name","display name preferred");
 ::nearbyAllowed=false;require(quickPersonLabel(targetID, true)=="PRIVATE LEGACY","known Cast fallback missing");
 require(quickPersonLabel(actorID, true)=="Unknown person","noncast leaked fallback");
 namesVisible=false;require(quickPersonLabel(targetID, true)=="Unknown person","hidden Cast leaked fallback");
 ::nearbyAllowed=true;namesVisible=true;
 reset();ALPanelLensGaze privacy;privacy.strength.value=.5;privacy.commitQuickControl(&privacy.strength);
 c.resolved=false;++c.revision;privacy.refreshQuickControls();
 require(privacy.name.text=="Display name"&&privacy.notice.text=="Added Display name to Cast","disappearance exposed legacy name");
 privacy.cast.focus=privacy.cast.open=true;namesVisible=false;privacy.refreshQuickControls();
 require(privacy.name.text=="Unknown person"&&privacy.notice.text=="Added Unknown person to Cast","privacy revocation exposed unresolved actor");
 require(!privacy.cast.open,"hidden cast row left open");
 for(const auto& row:privacy.cast.items)require(row.label!="Display name"&&row.label!="PRIVATE LEGACY","quick cast row exposed hidden name");
 advance(7.9f);require(privacy.mQuickNoticeActor==actorID,"notice expired early");advance(.2f);
 require(privacy.mQuickNoticeActor.isNull()&&privacy.notice.text.empty(),"notice did not expire without draw");
 reset();{ALPanelLensGaze gone;gone.showQuickNotice(actorID);}advance(9.f);
 for(bool retire:{false,true}){
  reset();ALPanelLensGaze focused;const LLUUID replacement("00000000-0000-0000-0000-000000000004");
  auto& studio=ALGhostStudio::instance();studio.clones={{ALGhostStudio::BACKING_ENTITY_CLONE,targetID,targetID,"Clone"}};
  c.target.mCastRef=targetID;focused.refreshQuickControls();focused.cast.focus=focused.cast.open=true;
  c.target.mCastRef=retire?LLUUID::null:replacement;c.members.clear();
  if(retire)studio.clones.clear();else{studio.clones[0].mEntityId=replacement;c.members.push_back({replacement,"Clone"});}
  c.alias=targetID;c.aliasDestination=retire?LLUUID::null:replacement;
  ++c.revision;++studio.revision;ALPanelLensGaze::rewriteActorReferences(targetID,c.target.mCastRef);
  require(!focused.cast.open&&!focused.cast.valueExists(targetID.asString()),"focused quick picker retained retired identity");
  require(focused.cast.value.asUUID()==c.target.mCastRef,"quick picker missed synchronous re-key");
  focused.cast.value=targetID;focused.cast.index=0;focused.commitQuickControl(&focused.cast);
  require(c.writes==0&&c.adds==0,"stale quick commit restored retired identity");
 }
 std::cout<<"PASS: quick refresh privacy/disappearance, focused re-key/deletion, stale commit, notice timer, no-op enrolment; active actor routing, first-edit enrolment/notice, real-avatar camera, target/strength, privacy, Pick\n";
}
'''
    return source


def main():
    kit_checks()
    flags, libraries = viewer_native_flags()
    directory = check_directory('quick_gaze')
    source = fixture()
    result = compile_run(directory, source, 'quick_gaze', flags=flags, libraries=libraries)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='')
    for name, old, new in (
        ('cast_fallback_removed', 'name = actorName(id);', 'name.clear();'),
        ('cast_fallback_privacy_removed', 'RlvActions::canShowName(RlvActions::SNC_DEFAULT, id))', 'true)'),
        ('cast_fallback_noncast', 'LLDirectorCast::instance().getMember(id) &&', ''),
        ('missing_quick_widget_throws', 'mQuickNotice = findChild<LLTextBox>', 'mQuickNotice = getChild<LLTextBox>'),
        ('missing_quick_widget_guard', '!mQuickName || !mQuickNotice ||', '!mQuickName ||'),
        ('wrong_actor', 'void ALPanelLensGaze::commitQuickControl(LLUICtrl* source)\n{\n    if (!mQuickName) return;\n    auto& cast = LLDirectorCast::instance();\n    const LLUUID actor = quickActor();', 'void ALPanelLensGaze::commitQuickControl(LLUICtrl* source)\n{\n    if (!mQuickName) return;\n    auto& cast = LLDirectorCast::instance();\n    const LLUUID actor = gAgentID;'),
        ('no_enrolment', 'cast.add(actor);', '(void)actor;'),
        ('unsupported_camera', 'if (source == mQuickCamera && !ALAvatarSubjectPicker::valid(body, false, false)) return;', ''),
        ('hidden_target', '!liveNearbyName(picked_actor, name)', 'false'),
        ('unresolved_privacy', '!mQuickActorIsClone && !LLDirectorCast::isSelf(actor)', 'real_avatar && !LLDirectorCast::isSelf(actor)'),
        ('disappearance_legacy', 'mQuickActorName = quickPersonLabel(actor, true);', 'mQuickActorName = body ? quickPersonLabel(actor) : std::string("PRIVATE LEGACY");'),
        ('legacy_cast_row', 'const std::string label = quickPersonLabel(member.mId);', 'const std::string label = member.mLastName;'),
        ('focused_rekey', 'panel->refreshQuickControls(true);', 'panel->refreshQuickControls();'),
        ('stale_commit', 'picked_actor.notNull() && !hotActorAvailable(picked_actor)', 'false'),
        ('alias_commit', '!hotActorAvailable(picked_actor)', '!cast.contains(picked_actor)'),
        ('pick_enrols', 'if (source == mQuickPick && target.mMode != LLActorMover::GazeTarget::OBJECT &&', 'if (false && source == mQuickPick && target.mMode != LLActorMover::GazeTarget::OBJECT &&'),
        ('notice_sticks', 'panel->mQuickNoticeActor.setNull();', '(void)panel;'),
    ):
        assert source.count(old) == 1, name
        result = compile_run(directory, source.replace(old, new), name, flags=flags, libraries=libraries)
        assert result.returncode == 2, (name, result.stdout, result.stderr)
        print('PASS: planted negative rejected: ' + name + ' (' + result.stderr.strip() + ')')


if __name__ == '__main__':
    main()
