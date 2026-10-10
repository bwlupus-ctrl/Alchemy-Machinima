#ifndef AL_KIT_LAZY_PAGE_H
#define AL_KIT_LAZY_PAGE_H
#include "llpanel.h"
class ALKitLazyPage : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> page_file;
        Optional<std::string>  master_setting;
        Params();
    };
    LLPanel*           ensureBuilt();
    void               reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void               onVisibilityChange(bool) override;
    const std::string& masterSetting() const { return mMasterSetting; }
    F64                buildMs() const { return mBuildMs; }

protected:
    friend class LLUICtrlFactory;
    explicit ALKitLazyPage(const Params& p) : LLPanel(p), mPageFile(p.page_file), mMasterSetting(p.master_setting) {}

private:
    void        fitContent();
    std::string mPageFile, mMasterSetting;
    LLPanel*    mContent        = nullptr;
    bool        mBuildAttempted = false;
    F64         mBuildMs        = -1.;
};
#endif
