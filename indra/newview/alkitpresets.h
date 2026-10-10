#ifndef AL_KIT_PRESETS_H
#define AL_KIT_PRESETS_H
#include "llsingleton.h"
#include <memory>
#include <string>
#include <vector>
class ALKitPresetProvider{public:struct Entry{std::string id,label;bool builtin=false;};virtual~ALKitPresetProvider()=default;virtual const std::string& key()const=0;virtual void list(std::vector<Entry>&)const=0;virtual bool apply(const std::string&)=0;virtual bool canSaveUser()const{return false;}virtual bool saveUser(const std::string&){return false;}virtual bool canDelete(const std::string&)const{return false;}virtual bool deleteUser(const std::string&){return false;}virtual U64 revision()const=0;virtual const std::vector<std::string>& trackedSettings()const=0;};
class ALKitPresetRegistry:public LLSingleton<ALKitPresetRegistry>{LLSINGLETON_EMPTY_CTOR(ALKitPresetRegistry);public:void add(std::unique_ptr<ALKitPresetProvider>);ALKitPresetProvider* get(const std::string&);private:std::vector<std::unique_ptr<ALKitPresetProvider>>mProviders;};
class ALKitSnapshotStore{public:ALKitSnapshotStore(std::string,std::vector<std::string>);std::vector<std::string> names()const{return{};}bool save(const std::string&){return false;}bool load(const std::string&){return false;}bool remove(const std::string&){return false;}U64 revision()const{return mRevision;}private:std::string mKey;std::vector<std::string>mSettings;U64 mRevision=0;};
#endif
