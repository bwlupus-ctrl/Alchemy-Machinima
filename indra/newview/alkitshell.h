#ifndef AL_KIT_SHELL_H
#define AL_KIT_SHELL_H
#include "llpanel.h"
#include "alkithistory.h"
#include "alkithelp.h"
#include "alscrollfit.h"
#include "lluuid.h"
#include <boost/signals2.hpp>
#include <vector>
class ALKitPage;
class ALKitShell:public LLPanel
{
public:using uuid_vec_t=std::vector<LLUUID>;using page_built_signal_t=boost::signals2::signal<void(const std::string&,LLPanel*)>;using cast_signal_t=boost::signals2::signal<void(const uuid_vec_t&)>;struct RailItem:public LLInitParam::Block<RailItem>{Mandatory<std::string>group,label,icon;Optional<std::string>master_settings;RailItem():group("group"),label("label"),icon("icon"),master_settings("master_settings",""){};};struct ALKitTarget{std::string page_key,card_id,widget,setting;S32 option=-1;};struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Optional<std::string>index_file;Multiple<RailItem>rail_item;Params():index_file("index_file","director_kit_index.xml"),rail_item("rail_item"){};};
 ALKitHistory& history(){return mHistory;}ALKitHelp& help(){return mHelp;}static ALKitShell* find(LLView*);
 bool navigateTo(const std::string& path);bool navigateTo(const ALKitTarget&);
 boost::signals2::connection setPageBuiltCallback(const page_built_signal_t::slot_type&s){return mPageBuilt.connect(s);}boost::signals2::connection setCastSelectionCallback(const cast_signal_t::slot_type&s){return mCastSelection.connect(s);}void emitPageBuilt(const std::string&name,LLPanel*p){mPageBuilt(name,p);}void setCastSelection(const uuid_vec_t&ids){mCastSelection(ids);}
 ALScrollFit::Controller& scrollFit(){return mScrollFit;}
protected:friend class LLUICtrlFactory;explicit ALKitShell(const Params&p):LLPanel(p){}
private:ALKitHistory mHistory;ALKitHelp mHelp;ALScrollFit::Controller mScrollFit;page_built_signal_t mPageBuilt;cast_signal_t mCastSelection;
};
#endif
