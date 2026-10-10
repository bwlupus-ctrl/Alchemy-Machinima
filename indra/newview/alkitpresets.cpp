#include "llviewerprecompiledheaders.h"
#include "alkitpresets.h"
void ALKitPresetRegistry::add(std::unique_ptr<ALKitPresetProvider> p)
{
    if(p)mProviders.push_back(std::move(p));
}
ALKitPresetProvider* ALKitPresetRegistry::get(const std::string& k)
{
    for(auto& p:mProviders)if(p->key()==k)return p.get();
    return nullptr;
}
ALKitSnapshotStore::ALKitSnapshotStore(std::string k,std::vector<std::string>s):mKey(std::move(k)),mSettings(std::move(s))
{
}
