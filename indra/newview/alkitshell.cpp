#include "llviewerprecompiledheaders.h"
#include "alkitshell.h"
#include "lluictrlfactory.h"
static LLDefaultChildRegistry::Register<ALKitShell> r("kit_shell");

ALKitShell* ALKitShell::find(LLView*v)
{
    for(;v;v=v->getParent())if(auto*s=dynamic_cast<ALKitShell*>(v))return s;
    return nullptr;
}

bool ALKitShell::navigateTo(const std::string&)
{
    return false;
}
bool ALKitShell::navigateTo(const ALKitTarget&)
{
    return false;
}
