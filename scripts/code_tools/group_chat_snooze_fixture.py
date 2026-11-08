"""Exercise production group Snooze methods with a fake clock and UI/transport spies.

Runs extracted production methods with native spies, never a viewer build.
Does not validate XUI layout, real floater ownership, transport or live grid behavior.
"""
from pathlib import Path
import re
import argparse
from native_check_tools import ROOT, compile_run, check_directory


def method(source, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\(', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    # These methods contain no brace-bearing string literals or block comments.
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--negative-check", action="store_true",
                        help="also prove each regression test rejects a removed protection")
    args = parser.parse_args()
    model = (ROOT / 'indra/newview/llimview.cpp').read_text(encoding='utf-8')
    ui = (ROOT / 'indra/newview/llfloaterimsession.cpp').read_text(encoding='utf-8')
    code = r'''
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <string>
#include <vector>
using S32=int; using U32=unsigned; using F64=double;
struct LLUUID {
 int value=0; LLUUID(int v=0):value(v){} bool isNull()const{return !value;}bool notNull()const{return value!=0;}
 bool operator==(const LLUUID&)const=default;
 bool operator<(const LLUUID& o)const{return value<o.value;}
 std::string asString()const{return std::to_string(value);}
 static const LLUUID null;
}; const LLUUID LLUUID::null;
const LLUUID gAgentID(99); const std::string SYSTEM_FROM="system", INTERACTIVE_SYSTEM_FROM="interactive";
struct LLSD {
 int option=-1; std::variant<int,std::string,LLUUID,bool> value{0}; std::map<std::string,LLSD> fields;
 LLSD& operator[](const std::string& key){return fields[key];}
 const LLSD& operator[](const std::string& key)const{static LLSD empty;auto it=fields.find(key);return it==fields.end()?empty:it->second;}
 LLSD& operator=(int v){value=v;return *this;} LLSD& operator=(bool v){value=v;return *this;}
 LLSD& operator=(const std::string& v){value=v;return *this;} LLSD& operator=(const LLUUID& v){value=v;return *this;}
 int asInteger()const{return std::holds_alternative<int>(value)?std::get<int>(value):0;}
 bool asBoolean()const{return std::holds_alternative<bool>(value)?std::get<bool>(value):false;}
 std::string asString()const{return std::holds_alternative<std::string>(value)?std::get<std::string>(value):"";}
 LLUUID asUUID()const{return std::holds_alternative<LLUUID>(value)?std::get<LLUUID>(value):LLUUID();}
 bool has(const std::string& key)const{return fields.count(key)!=0;}
};
std::ostream& operator<<(std::ostream& s,const LLSD&){return s;}
bool gDisconnected=false;
struct LLFrameTimer {static inline double now=100; static double getTotalSeconds(){return now;}};
int llclamp(int v,int a,int b){return std::clamp(v,a,b);}
struct LLLogChat {static std::string timestamp2LogString(U32,bool){return "time";}};
struct LLRecentPeople {static LLRecentPeople& instance(){static LLRecentPeople p;return p;} void add(LLUUID){}};
struct LLVoiceChannel {bool active=false;bool isActive(){return active;}
 bool callStarted(){return active;} static LLVoiceChannel* getChannelByID(LLUUID){return nullptr;}};
int history=0,transcripts=0,signals=0,restores=0,leaves=0,hangups=0,base_closes=0;
std::vector<std::string> trace;
bool ui_exists=true; LLUUID selected(1);
std::function<void(LLUUID)> rebuild;
struct LLChat {LLUUID mFromID,mSessionID,mNotifId;std::string mFromName,mTimeStr,mText;int mChatStyle=0,mSourceType=0;};
const int CHAT_STYLE_HISTORY=1, CHAT_SOURCE_REGION=1;
struct LLChatHistory {enum {PARSE_NONE=0,PARSE_ALL=255};U32 mask=PARSE_ALL;
 U32 getParseHighlightTypeMask(){return mask;}void setParseHighlightTypeMask(U32 v){mask=v;}};
int displayed=0,alerts=0;bool throw_append=false;
struct LLAvatarName {std::string getUserName()const{return "caller";}std::string getDisplayName()const{return "Caller";}};
struct LLAvatarNameCache {
 static inline std::function<void(const LLUUID&,const LLAvatarName&)> pending;
 static bool get(LLUUID,LLAvatarName*){return true;}
 static void get(LLUUID,std::function<void(const LLUUID&,const LLAvatarName&)> cb){pending=cb;}
};
using namespace std::placeholders; namespace boost {using std::bind;}
enum EInstantMessage {IM_SESSION_P2P_INVITE,IM_SESSION_GROUP_START,IM_NOTHING_SPECIAL};
#define LL_WARNS(x) std::ostringstream()
#define LL_INFOS(x) std::ostringstream()
#define LL_ENDL ""
struct Agent {bool isInGroup(LLUUID id,bool){return id==LLUUID(1);}bool isDoNotDisturb(){return false;}} gAgent;
struct LLMute {enum {flagVoiceChat=1,flagAll=255};};
struct LLMuteList {static bool isLinden(const std::string&){return false;}
 static LLMuteList* getInstance(){static LLMuteList l;return &l;}bool isMuted(LLUUID,int){return false;}};
struct LLAvatarTracker {static LLAvatarTracker& instance(){static LLAvatarTracker t;return t;}
 void* getBuddyInfo(LLUUID){return reinterpret_cast<void*>(1);}bool isBuddy(LLUUID){return true;}};
struct LLSLURL {LLSLURL(const char*,LLUUID,const char*){}std::string getSLURLString(){return "caller";}};
int dialogs=0,declines=0;
struct LLFloaterReg {static void showInstance(const char*,LLSD,bool){++dialogs;}};
struct LLIncomingCallDialog {static void processCallResponse(S32 response,const LLSD&){assert(response==1);++declines;}};
void* gMessageSystem=nullptr;
void send_do_not_disturb_message(void*,LLUUID,LLUUID){}
namespace LLNotificationsUI {
 const LLUUID NOTIFICATION_CHANNEL_UUID;
 struct LLScreenChannel {void hideToast(LLUUID){}};
 struct LLChannelManager {static LLChannelManager* getInstance(){static LLChannelManager c;return &c;}
 LLScreenChannel* findChannelByID(LLUUID){static LLScreenChannel c;return &c;}};
}
struct LLIMModel {
 struct LLIMSession {
  int type=0,mNumUnread=4,mParticipantUnreadMessageCount=4,mSessionType=0;
  std::string mName="Group"; LLUUID mOtherParticipantID{2}; LLVoiceChannel* mVoiceChannel=nullptr;
  std::list<LLSD> mMsgs;bool mSessionInitialized=true;EInstantMessage mType=IM_SESSION_GROUP_START;
  bool isGroupSessionType(){return type==0;} bool isP2PSessionType(){return type==1;}
  bool isAdHocSessionType(){return type==2;}
 };
 U32 mGroupChatSnoozeGeneration=0;
 std::map<LLUUID, LLIMSession*> mId2SessionMap; std::map<LLUUID,F64> mGroupChatSnoozes;
 static LLIMModel& instance(){static LLIMModel m;return m;}
 static LLIMModel* getInstance(){return &instance();}
 LLIMSession* findIMSession(LLUUID id){auto it=mId2SessionMap.find(id);return it==mId2SessionMap.end()?nullptr:it->second;}
 bool snoozeGroupChat(const LLUUID&,S32); bool isGroupChatSnoozed(const LLUUID&);
 void endGroupChatSnooze(const LLUUID&); bool clearSession(const LLUUID&);
 void clearGroupChatSnoozes();U32 getGroupChatSnoozeGeneration()const{return mGroupChatSnoozeGeneration;}
 void newSession(LLUUID,const std::string&,EInstantMessage,LLUUID,LLSD,bool){}
 void getMessages(LLUUID id,std::list<LLSD>& out,int start,bool){for(auto msg:findIMSession(id)->mMsgs)if(msg["index"].asInteger()>=start)out.push_back(msg);}
 void sendNoUnreadMessages(LLUUID id){auto* s=findIMSession(id);s->mNumUnread=s->mParticipantUnreadMessageCount=0;}
 void addToHistory(LLUUID id,const std::string& from,LLUUID from_id,const std::string& text,bool region,U32){
  ++history;trace.push_back("history");auto* s=findIMSession(id);LLSD msg;
  msg["index"]=s->mMsgs.empty()?0:s->mMsgs.front()["index"].asInteger()+1;
  msg["from"]=from;msg["from_id"]=from_id;msg["message"]=text;msg["is_region_msg"]=region;
  s->mMsgs.push_front(msg);
 }
 std::string getHistoryFileName(LLUUID){return "transcript";}
 void logToFile(const std::string&,const std::string&,LLUUID,const std::string&){++transcripts;trace.push_back("transcript");}
 void mNewMsgSignal(LLSD){assert(ui_exists);++signals;trace.push_back("signal");}
 LLIMSession* addMessageSilently(const LLUUID&,const std::string&,const LLUUID&,const std::string&,bool=true,bool=false,U32=0,bool=true);
 void processAddingMessage(const LLUUID&,const std::string&,const LLUUID&,const std::string&,bool,bool,U32,bool=true);
};
struct LLFloater {std::shared_ptr<int> lifetime=std::make_shared<int>();virtual ~LLFloater()=default;bool dead=false;bool isDead(){return dead;}};
template<class T> struct LLHandle {T* p;std::weak_ptr<int> lifetime; T* get()const{return lifetime.expired()?nullptr:p;}};
struct LLFloaterIMSessionTab:LLFloater {static void restoreFloater(){} void onClose(bool){++base_closes;}};
struct LLFloaterIMContainer {
 static LLFloaterIMContainer* getInstance(){static LLFloaterIMContainer c;return &c;}
 void sessionAdded(LLUUID id,const std::string&,LLUUID,bool){++restores;ui_exists=true;trace.push_back("restore");if(rebuild)rebuild(id);}
 void removeConversationListItem(LLUUID){ui_exists=false;}
 LLUUID getSelectedSession(){return selected;}
 void selectConversationPair(LLUUID id,bool,bool){selected=id;}
};
struct Settings {bool ask=true;int minutes=30;bool getBOOL(const std::string& key){return key=="GroupChatSnoozeAsk"?ask:false;}int getS32(const char*){return minutes;}} gSavedSettings,gSavedPerAccountSettings;
struct LLIMMgr {
 enum EInvitationType {INVITATION_TYPE_VOICE,INVITATION_TYPE_IMMEDIATE};
 LLSD mPendingInvitations;
 void inviteToSession(const LLUUID&,const std::string&,const LLUUID&,const std::string&,EInstantMessage,EInvitationType,const LLSD&);
 static void onInviteNameLookup(LLSD,const LLUUID&,const LLAvatarName&);
 bool hasSession(LLUUID id){return LLIMModel::instance().findIMSession(id)!=nullptr;}
 void clearPendingInvitation(LLUUID id){mPendingInvitations.fields.erase(id.asString());}
 void addSystemMessage(LLUUID,const char*,LLSD){}

 void endCall(LLUUID id){++hangups;LLIMModel::instance().findIMSession(id)->mVoiceChannel->active=false;trace.push_back("hangup");}
 void leaveSession(LLUUID id){++leaves;LLIMModel::instance().clearSession(id);}
} manager; auto* gIMMgr=&manager;
namespace LLNotificationsUtil {
 using Callback=std::function<void(const LLSD&,const LLSD&)>;
 std::vector<std::pair<std::string,Callback>> pending;
 int getSelectedOption(const LLSD&,const LLSD& response){return response.option;}
 void add(const std::string& name,LLSD,LLSD,Callback cb){pending.emplace_back(name,cb);}
 void add(const std::string&,LLSD){}
 void* find(LLUUID){return nullptr;}
 void answer(int option){assert(!pending.empty());auto cb=pending.back().second;pending.pop_back();LLSD r;r.option=option;cb(LLSD(),r);}
}
struct LLFloaterIMSession:LLFloaterIMSessionTab {
 S32 mSnoozeReplayThrough=-1,mLastMessageIndex=-1;LLChatHistory chat_history;LLChatHistory* mChatHistory=&chat_history;
 bool mIsP2PChat=false,mSessionInitialized=false;EInstantMessage mDialog=IM_SESSION_GROUP_START;
 LLUUID mSessionID{1};LLIMModel::LLIMSession* mSession=nullptr;bool mClosingForSnooze=false;
 LLHandle<LLFloater> getHandle(){return {this,lifetime};}
 static void* findInstance(LLUUID){return ui_exists?reinterpret_cast<void*>(1):nullptr;}
 void setKey(LLUUID){}bool hasFocus(){return false;}bool getVisible(){return false;}
 void initIMSession(const LLUUID&);void updateMessages();
 void appendMessage(const LLChat&){if(throw_append){throw_append=false;throw std::runtime_error("append failed");}
  ++displayed;if(chat_history.mask!=LLChatHistory::PARSE_NONE)++alerts;}
 void onSnoozeClicked();void requestGroupChatSnooze(S32,bool=false);void onClose(bool);
 void setTyping(bool){} void closeFloater(){onClose(false);dead=true;ui_exists=false;}
};
'''
    for name in ('snoozeGroupChat', 'isGroupChatSnoozed', 'endGroupChatSnooze',
                 'clearGroupChatSnoozes', 'clearSession', 'addMessageSilently', 'processAddingMessage'):
        code += '\n' + method(model, 'LLIMModel::' + name)
    for name in ('initIMSession', 'updateMessages', 'onSnoozeClicked', 'requestGroupChatSnooze', 'onClose'):
        code += '\n' + method(ui, 'LLFloaterIMSession::' + name)
    for name in ('inviteToSession', 'onInviteNameLookup'):
        code += '\n' + method(model, 'LLIMMgr::' + name)
    code += r'''
int main(){
 auto& m=LLIMModel::instance();
 auto fresh=[&](int id,int type=0){auto* s=new LLIMModel::LLIMSession;s->type=type;m.mId2SessionMap[id]=s;return s;};
 auto* group=fresh(1);fresh(2,1);fresh(3,2);fresh(4);
 assert(!m.snoozeGroupChat(20,30)); assert(!m.snoozeGroupChat(2,30));assert(!m.snoozeGroupChat(3,30));
 assert(m.snoozeGroupChat(1,15));assert(m.isGroupChatSnoozed(1));assert(!m.isGroupChatSnoozed(4));
 assert(group->mNumUnread==0 && group->mParticipantUnreadMessageCount==0);
 ui_exists=false;
 for(int i=0;i<3;++i)m.processAddingMessage(1,"resident",LLUUID(7),"message",true,false,0);
 assert(history==3 && transcripts==3 && signals==0 && restores==0);
 assert(group->mNumUnread==0 && group->mParticipantUnreadMessageCount==0);
 m.addMessageSilently(1,"resident",LLUUID(7),"silent message");
 assert(history==4 && transcripts==4 && group->mNumUnread==0);
 LLFrameTimer::now=999.999;assert(m.isGroupChatSnoozed(1));
 LLFrameTimer::now=1000;assert(!m.isGroupChatSnoozed(1));
 m.processAddingMessage(1,"resident",LLUUID(7),"next message",true,false,0);
 assert(history==5 && transcripts==5 && signals==1 && restores==1 && group->mNumUnread==1);
 assert(trace[trace.size()-4]=="restore" && trace.back()=="signal");
 // Translation received during Snooze must stay silent even if it finishes after expiry.
 ui_exists=false;m.processAddingMessage(1,"resident",LLUUID(7),"late translation",true,false,0,false);
 assert(history==6 && transcripts==6 && signals==1 && restores==1 && group->mNumUnread==1);
 // An older translation finishing while Snooze is active is silent too.
 assert(m.snoozeGroupChat(1,1));m.processAddingMessage(1,"resident",LLUUID(7),"older translation",true,false,0,true);
 assert(history==7 && transcripts==7 && signals==1 && group->mNumUnread==0);
 m.endGroupChatSnooze(1);assert(!m.isGroupChatSnoozed(1));
 m.snoozeGroupChat(1,-3);assert(m.mGroupChatSnoozes[1]==1060);
 m.snoozeGroupChat(4,500);assert(m.mGroupChatSnoozes[4]==15400);
 m.clearGroupChatSnoozes();assert(!m.isGroupChatSnoozed(1)&&!m.isGroupChatSnoozed(4));
 // Ask off uses the configured duration and closes UI without sending a leave.
 LLFloaterIMSession f;f.mSession=group;ui_exists=true;selected=1;gSavedSettings.ask=false;gSavedSettings.minutes=43;
 f.onSnoozeClicked();assert(f.dead && m.isGroupChatSnoozed(1) && leaves==0 && selected.isNull());
 assert(m.mGroupChatSnoozes[1]==3580);
 // Ask on: Cancel, dismissed prompt and expired window do nothing; each duration works.
 gSavedSettings.ask=true;
 for(int option:{-1,3,0,1,2}){
  m.endGroupChatSnooze(1);LLFloaterIMSession window;window.mSession=group;ui_exists=true;
  window.onSnoozeClicked();assert(LLNotificationsUtil::pending.back().first=="GroupChatSnoozeDuration");
  assert(!m.isGroupChatSnoozed(1));LLNotificationsUtil::answer(option);
  if(option<0||option==3){assert(!window.dead && !m.isGroupChatSnoozed(1));}
  else {assert(window.dead && leaves==0);int durations[]={15,30,60};assert(m.mGroupChatSnoozes[1]==1000+durations[option]*60);}
 }
 m.endGroupChatSnooze(1);LLFloaterIMSession stale;stale.mSession=group;stale.onSnoozeClicked();stale.dead=true;
 LLNotificationsUtil::answer(0);assert(!m.isGroupChatSnoozed(1));
 // Voice requires an additional confirmation; Cancel preserves both voice and text.
 LLVoiceChannel voice;voice.active=true;group->mVoiceChannel=&voice;gSavedSettings.ask=false;
 LLFloaterIMSession voice_window;voice_window.mSession=group;voice_window.onSnoozeClicked();
 assert(LLNotificationsUtil::pending.back().first=="ConfirmGroupChatSnoozeCall");
 assert(voice.active && hangups==0 && !m.isGroupChatSnoozed(1));LLNotificationsUtil::answer(1);
 assert(voice.active && !voice_window.dead && !m.isGroupChatSnoozed(1));
 voice_window.onSnoozeClicked();LLNotificationsUtil::answer(0);
 assert(!voice.active && hangups==1 && voice_window.dead && leaves==0 && m.isGroupChatSnoozed(1));
 // Rebuild executes production cutoff capture and replay parsing, with a chat-history spy.
 // A post-expiry message must be outside the cutoff; older pre-snooze messages are quiet too.
 LLFloaterIMSession replay;displayed=alerts=0;m.snoozeGroupChat(1,1);LLFrameTimer::now+=60;ui_exists=false;
 rebuild=[&](LLUUID id){replay.initIMSession(id);};
 int retained=static_cast<int>(group->mMsgs.size());
 m.processAddingMessage(1,"resident",LLUUID(7),"new alert",true,false,0);
 rebuild={};replay.updateMessages();
 assert(displayed==retained+1 && alerts==1 && replay.chat_history.mask==LLChatHistory::PARSE_ALL);
 m.processAddingMessage(1,"resident",LLUUID(7),"next alert",true,false,0);replay.updateMessages();
 assert(alerts==2 && displayed==retained+2);
 // Manual reopening also replays the entire retained backlog silently.
 LLFloaterIMSession manual;manual.initIMSession(1);displayed=alerts=0;manual.updateMessages();
 assert(displayed==retained+2 && alerts==0 && manual.chat_history.mask==LLChatHistory::PARSE_ALL);
 // Arrival during Snooze, translation completion after manual reopen, then display.
 m.snoozeGroupChat(1,30);const bool translation_notify=!m.isGroupChatSnoozed(1);
 m.endGroupChatSnooze(1);LLFloaterIMSession translated;translated.initIMSession(1);
 translated.updateMessages();displayed=alerts=0;
 m.processAddingMessage(1,"resident",LLUUID(7),"translated snoozed mention",true,false,0,translation_notify);
 assert(group->mMsgs.front()["index"].asInteger()>translated.mSnoozeReplayThrough);
 translated.updateMessages();assert(displayed==1 && alerts==0);
 m.processAddingMessage(1,"resident",LLUUID(7),"normal mention",true,false,0);
 translated.updateMessages();assert(displayed==2 && alerts==1);
 // A suppressed append throws; unwinding must restore parsing for the next normal message.
 m.snoozeGroupChat(1,30);
 m.processAddingMessage(1,"resident",LLUUID(7),"throwing snoozed mention",true,false,0);
 m.endGroupChatSnooze(1);throw_append=true;alerts=0;
 bool caught=false;try{translated.updateMessages();}catch(const std::runtime_error&){caught=true;}
 assert(caught);
 m.processAddingMessage(1,"resident",LLUUID(7),"normal after exception",true,false,0);
 translated.updateMessages();assert(alerts==1 && translated.chat_history.mask==LLChatHistory::PARSE_ALL);
 // Normal one-to-one and conference parsing is unchanged.
 for(int id:{2,3}){m.addMessageSilently(id,"resident",LLUUID(7),"normal");
  LLFloaterIMSession normal;normal.initIMSession(id);alerts=0;normal.updateMessages();assert(alerts==1);}
 // Immediate and deferred invitations are suppressed without deleting the joined session.
 m.snoozeGroupChat(1,30);int old_leaves=leaves;
 manager.inviteToSession(1,"group",7,"Caller",IM_SESSION_GROUP_START,LLIMMgr::INVITATION_TYPE_VOICE,LLSD());
 assert(declines==1 && dialogs==0 && !LLAvatarNameCache::pending && m.findIMSession(1)==group && leaves==old_leaves);
 manager.inviteToSession(1,"group",7,"",IM_SESSION_GROUP_START,LLIMMgr::INVITATION_TYPE_IMMEDIATE,LLSD());
 assert(declines==2 && dialogs==0 && !LLAvatarNameCache::pending);
 m.endGroupChatSnooze(1);
 manager.inviteToSession(1,"group",7,"",IM_SESSION_GROUP_START,LLIMMgr::INVITATION_TYPE_VOICE,LLSD());
 assert(LLAvatarNameCache::pending && declines==2 && dialogs==0);
 m.snoozeGroupChat(1,30);auto lookup=LLAvatarNameCache::pending;LLAvatarNameCache::pending={};lookup(7,LLAvatarName());
 assert(declines==3 && dialogs==0 && m.findIMSession(1)==group && leaves==old_leaves);
 // Other conversations still surface; no snoozed-group notice handler is changed here.
 manager.inviteToSession(2,"p2p",7,"Caller",IM_SESSION_P2P_INVITE,LLIMMgr::INVITATION_TYPE_VOICE,LLSD());
 manager.inviteToSession(3,"conference",7,"Caller",IM_SESSION_GROUP_START,LLIMMgr::INVITATION_TYPE_IMMEDIATE,LLSD());
 assert(dialogs==2 && declines==3);
 m.endGroupChatSnooze(1);manager.clearPendingInvitation(1);
 manager.inviteToSession(1,"group",7,"Caller",IM_SESSION_GROUP_START,LLIMMgr::INVITATION_TYPE_VOICE,LLSD());
 assert(dialogs==3 && declines==3);
 // Clear on logout/disconnect invalidates both pending prompt stages, even after reconnect.
 group->mVoiceChannel=nullptr;
 for(bool reconnect:{false,true}){
  for(bool voice_prompt:{false,true}){
   LLFloaterIMSession pending;pending.mSession=group;ui_exists=true;
   gSavedSettings.ask=!voice_prompt;voice.active=voice_prompt;group->mVoiceChannel=voice_prompt?&voice:nullptr;
   pending.onSnoozeClicked();gDisconnected=true;m.clearGroupChatSnoozes();
   if(reconnect)gDisconnected=false;
   int old_hangups=hangups;LLNotificationsUtil::answer(0);
   assert(!m.isGroupChatSnoozed(1) && !pending.dead && hangups==old_hangups);
   gDisconnected=false;
  }
 }
 // Destroying the fixture floater expires its weak lifetime spy.
 group->mVoiceChannel=nullptr;gSavedSettings.ask=true;
 {LLFloaterIMSession destroyed;destroyed.mSession=group;destroyed.onSnoozeClicked();}
 LLNotificationsUtil::answer(0);assert(!m.isGroupChatSnoozed(1));
 // Ordinary close still sends a leave and clears any deadline with the session.
 LLFloaterIMSession ordinary;ordinary.mSession=group;ordinary.onClose(false);
 assert(leaves==1 && !m.findIMSession(1) && !m.isGroupChatSnoozed(1));
 for(auto [id,s]:m.mId2SessionMap)delete s;
 std::cout<<"PASS: group-only eligibility, per-group deadlines, exact expiry, clamping, history/transcripts, unread and signal suppression, reconstruction request before insertion/signal, delayed translation, manual cancellation, session clear, logout clear, duration choices, prompt cancellation, stale prompt, voice confirmation/hangup, retained join, normal leave, immediate/deferred invite suppression, quiet retained replay with new-message alerts and unchanged P2P/conference parsing, disconnect callback invalidation\n";
}
'''
    directory = check_directory('group_chat_snooze')
    result = compile_run(directory, code, name='group_chat_snooze', flags=['/W4'])
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='')
    if args.negative_check:
        mutations = {
            'invite': ('if (LLIMModel::instance().isGroupChatSnoozed(session_id))', 'if (false)'),
            'deferred_invite': ('if (LLIMModel::instance().isGroupChatSnoozed(payload["session_id"].asUUID()))', 'if (false)'),
            'replay': ('msg["index"].asInteger() <= mSnoozeReplayThrough', 'false'),
            'delayed_translation_display': ('session->mMsgs.front()["suppress_alerts"] = suppress_alerts;', ''),
            'message_alert_suppression': (' || msg["suppress_alerts"].asBoolean()', ''),
            'append_exception_restore': ('~ParseMaskRestore() { history->setParseHighlightTypeMask(mask); }', '~ParseMaskRestore() {}'),
            'rebuild': ('if (notify && session && session->isGroupSessionType()', 'if (false && notify && session && session->isGroupSessionType()'),
            'disconnect_prompt': ('if (generation != LLIMModel::instance().getGroupChatSnoozeGeneration()) return;', ''),
        }
        for name, (old, new) in mutations.items():
            assert old in code, name
            broken = code.replace(old, new)
            result = compile_run(directory, broken, name='negative_' + name)
            assert result.returncode != 0, 'regression survived: ' + name
            print('PASS: planted removal rejected: ' + name)


if __name__ == '__main__':
    main()
