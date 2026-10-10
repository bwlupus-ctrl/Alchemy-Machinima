#ifndef AL_KIT_LAZY_PAGE_H
#define AL_KIT_LAZY_PAGE_H
#include "llpanel.h"
class ALKitLazyPage:public LLPanel
{
public:struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Mandatory<std::string>page_file;Optional<std::string>master_setting;Params();};LLPanel*ensureBuilt();void onVisibilityChange(bool)override;
protected:friend class LLUICtrlFactory;explicit ALKitLazyPage(const Params&p):LLPanel(p),mPageFile(p.page_file),mMasterSetting(p.master_setting){}
private:std::string mPageFile,mMasterSetting;LLPanel*mContent=nullptr;bool mBuildAttempted=false;
};
#endif
