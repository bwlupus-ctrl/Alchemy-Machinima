#include "llviewerprecompiledheaders.h"
#include "alkitlazypage.h"
#include "alkitpage.h"
#include "alkitshell.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
static LLDefaultChildRegistry::Register<ALKitLazyPage> r("kit_lazy_page");

namespace
{
ALKitPage* findKitPage(LLView* root)
{
    if (!root)
        return nullptr;
    if (ALKitPage* page = dynamic_cast<ALKitPage*>(root))
        return page;
    for (LLView* child : *root->getChildList())
        if (ALKitPage* page = findKitPage(child))
            return page;
    return nullptr;
}
} // namespace

ALKitLazyPage::Params::Params() : page_file("page_file"), master_setting("master_setting", "")
{
}

LLPanel* ALKitLazyPage::ensureBuilt()
{
    if (mContent || mBuildAttempted)
        return mContent;
    mBuildAttempted = true;
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Kit page build");
    LLTimer timer;
    mContent = LLUICtrlFactory::getInstance()->createFromFile<LLPanel>(mPageFile, this, LLDefaultChildRegistry::instance());
    if (!mContent)
    {
        LL_WARNS("DirectorKit") << "Failed to build kit page " << mPageFile << LL_ENDL;
        return nullptr;
    }
    if (mContent->getParent() != this)
        addChild(mContent);
    fitContent();
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        shell->scrollFit().adoptPage(this, mContent);
        shell->emitPageBuilt(getName(), mContent);
    }
    mBuildMs = timer.getElapsedTimeF64() * 1000.;
    LL_INFOS("DirectorKit") << "Kit page " << getName() << " built in " << mBuildMs << " ms" << LL_ENDL;
    return mContent;
}

void ALKitLazyPage::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    fitContent();
}

void ALKitLazyPage::fitContent()
{
    if (!mContent)
        return;
    mContent->reshape(getRect().getWidth(), getRect().getHeight(), true);
    mContent->setOrigin(0, 0);
    mContent->setFollowsAll();
    if (ALKitPage* page = findKitPage(mContent))
    {
        page->reshape(mContent->getRect().getWidth(), mContent->getRect().getHeight(), true);
        page->setOrigin(0, 0);
        page->setFollowsAll();
    }
}

void ALKitLazyPage::onVisibilityChange(bool visible)
{
    if (visible)
        ensureBuilt();
    LLPanel::onVisibilityChange(visible);
}
