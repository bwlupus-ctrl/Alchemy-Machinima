#ifndef AL_KIT_HISTORY_H
#define AL_KIT_HISTORY_H
#include "llhandle.h"
#include "llsd.h"
#include <functional>
#include <string>
#include <vector>
class LLUICtrl;
class ALKitHistory
{
public:
 enum EOrigin{MOUSE,WHEEL,KEY};
 struct Entry{bool widget=false;std::string key;LLHandle<LLUICtrl> ctrl;LLSD oldv,newv;};
 struct CustomEntry{std::string key;std::function<bool(bool,std::string&)> replay;};
 struct Step{std::string label,context,page;U64 scene_generation=0;std::vector<Entry> entries;std::vector<CustomEntry> custom;};
 static constexpr size_t MAX_STEPS=200;
 void beginGesture(EOrigin,const std::string&,const std::string& label=std::string()); static void noteKey(); void watchSettings(const std::string&,const std::vector<std::string>&);
 void recordWidget(LLUICtrl*,const LLSD&,const LLSD&,const std::string&); void recordCustom(const std::string&,const std::string&,std::function<bool(bool,std::string&)>);
 void barrier(const std::string&); static void globalBarrier(const std::string&); bool canUndo()const{return !mUndo.empty();} bool canRedo()const{return !mRedo.empty();}
 std::string undoLabel()const{return canUndo()?mUndo.back().label:std::string();} std::string redoLabel()const{return canRedo()?mRedo.back().label:std::string();}
 void undo();void redo();U64 revision()const{return mRevision;} void registerContext(const std::string&,std::function<std::string()>);
 struct ScopedSuspend{ScopedSuspend();~ScopedSuspend();}; static bool suspended();
private: std::vector<Step> mUndo,mRedo;U64 mRevision=0,mGeneration=0;std::string mPage,mGestureLabel;EOrigin mOrigin=MOUSE;
};
#endif
