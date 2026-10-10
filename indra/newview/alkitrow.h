#ifndef AL_KIT_ROW_H
#define AL_KIT_ROW_H
#include "llpanel.h"
#include "llframetimer.h"
#include "alkitbinding.h"
#include <boost/signals2.hpp>
#include <array>
#include <memory>
class LLUICtrl;class LLTextBox;class LLButton;class ALKitCard;
struct ALKitRowMeta{std::string kind,widget,label,help,keywords,setting,unit,sub,link_to,action,mirror_setting;F32 min_val=0,max_val=0;S32 decimals=3;bool advanced=false,has_min=false,has_max=false;S32 option=-1;std::string card_id,page_key;};
class ALKitRow:public LLPanel
{
public:
 struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Mandatory<std::string>kind;Optional<std::string>widget,label,label_name,help,keywords,unit,sub,setting,bind,reset_settings,default_value,link_to,action,mirror_setting;Optional<bool>advanced,card_preset,who_picker,hidden,added;Optional<S32>row_height,decimals;Optional<F32>min_val,max_val;Params();};
 bool postBuild()override;void draw()override;void onMouseEnter(S32,S32,MASK)override;void onMouseLeave(S32,S32,MASK)override;
 S32 heightForWidth(S32)const;void layoutRow(S32);void layoutDialCell(S32);bool isEffectivelyVisible()const;static bool setControlVisible(LLView*,bool);void resolveBinding();void refreshValue();void pollLiveValue();
 using amber_signal_t=boost::signals2::signal<void(bool)>;const ALKitRowMeta& meta()const{return mMeta;}LLUICtrl* control()const{return mControl;}void setFilterVisible(bool v){mFilterVisible=v;}void flash(){mFlashTimer.start();mFlashing=true;}bool amber()const;boost::signals2::connection setAmberCallback(const amber_signal_t::slot_type&s){return mAmberSignal.connect(s);}bool resettable()const{return !!mBinding;}void reset(){if(mBinding)mBinding->reset();}bool cardPreset()const{return mCardPreset;}bool whoPicker()const{return mWhoPicker;}
protected:friend class LLUICtrlFactory;explicit ALKitRow(const Params&);
private:void bindingChanged();ALKitCard* card()const;ALKitRowMeta mMeta;std::string mBind,mResetSettings,mDefaultValue,mLabelName;S32 mRowHeight=28;bool mCardPreset=false,mWhoPicker=false,mHidden=false,mAdded=false,mFilterVisible=true,mCodeHidden=false,mHovered=false,mFlashing=false,mAmber=false;LLUICtrl*mControl=nullptr;std::array<LLUICtrl*,3>mVec{{nullptr,nullptr,nullptr}};S32 mVecCount=0;LLTextBox*mLabel=nullptr;LLTextBox*mValueText=nullptr;LLButton*mReset=nullptr;ALKitCard*mCard=nullptr;std::unique_ptr<ALKitBinding>mBinding;boost::signals2::scoped_connection mBindingConnection,mFocusConnection,mMirrorConnection,mHistoryConnection,mValueConnection;amber_signal_t mAmberSignal;LLFrameTimer mFlashTimer;LLSD mLastValue,mShownValue;
};
#endif
