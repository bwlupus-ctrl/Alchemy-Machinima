#include "llviewerprecompiledheaders.h"
#include "alkitlazypage.h"
#include "alkitshell.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
static LLDefaultChildRegistry::Register<ALKitLazyPage> r("kit_lazy_page");

ALKitLazyPage::Params::Params():page_file("page_file"),master_setting("master_setting","")
{
}

LLPanel*ALKitLazyPage::ensureBuilt()
{
    if(mContent||mBuildAttempted)return mContent;
    mBuildAttempted=true;
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Kit page build");
    LLTimer timer;
    mContent=LLUICtrlFactory::getInstance()->createFromFile<LLPanel>(mPageFile,this,LLDefaultChildRegistry::instance());
    if(!mContent)
    {
        LL_WARNS("DirectorKit")<<"Failed to build kit page "<<mPageFile<<LL_ENDL;
        return nullptr;
    }
    mContent->reshape(getRect().getWidth(),getRect().getHeight(),true);
    mContent->setOrigin(0,0);
    mContent->setFollowsAll();
    if(mContent->getParent()!=this)addChild(mContent);
    if(ALKitShell*shell=ALKitShell::find(this))
    {
        shell->scrollFit().adoptPage(this,mContent);
        shell->emitPageBuilt(getName(),mContent);
    }
    LL_INFOS("DirectorKit")<<"Kit page "<<getName()<<" built in "<<timer.getElapsedTimeF64()*1000.<<" ms"<<LL_ENDL;
    return mContent;
}

void ALKitLazyPage::onVisibilityChange(bool visible)
{
    if(visible)ensureBuilt();
    LLPanel::onVisibilityChange(visible);
}
