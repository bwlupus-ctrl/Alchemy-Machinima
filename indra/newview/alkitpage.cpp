#include "llviewerprecompiledheaders.h"
#include "alkitpage.h"
#include "alflowgrid.h"
#include "alkitcard.h"
#include "alkitbinding.h"
#include "alkitrow.h"
#include "alkitpresets.h"
#include "alkitstyle.h"
#include "alkitshell.h"
#include "alkitswitch.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "aluirefresh.h"
#include <algorithm>

static LLDefaultChildRegistry::Register<ALKitPage> r("kit_page");

static LLDefaultChildRegistry::Register<ALKitScroller> rs("kit_scroller");

static LLDefaultChildRegistry::Register<ALKitDoc> rd("kit_doc");

static LLDefaultChildRegistry::Register<ALKitPresetHost> rp("kit_preset_host");

namespace
{

    std::vector<std::string> tokens(const std::string& value,const std::string& sep=",")
    {

        std::vector<std::string> out=LLStringUtil::getTokens(value,sep);

        for(std::string& s:out)LLStringUtil::trim(s);

        return out;

    }

    LLPanel* makePanel(const std::string& name)
    {

        LLPanel::Params p;
        p.name=name;
        p.rect=LLRect(0,1,1,0);
        p.background_visible=false;

        return LLUICtrlFactory::create<LLPanel>(p);

    }

    LLButton* makeButton(const std::string& name,const std::string& label)
    {

        LLButton::Params p;
        p.name=name;
        p.rect=LLRect(0,24,80,0);
        p.label=label;

        return LLUICtrlFactory::create<LLButton>(p);

    }

    class SectionHeading final:public LLPanel,public ALFlowGridItem
    {

        public:explicit SectionHeading(const LLPanel::Params&p,std::string label):LLPanel(p),mLabel(std::move(label))
        {
        }
        S32 heightForWidth(S32)const override
        {
            return 28;
        }
        bool spansAllColumns()const override
        {
            return true;
        }
        void draw()override
        {
            LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
            if(const LLFontGL*f=LLFontGL::getFontSansSerifSmall())f->renderUTF8(mLabel,0,0,8,ALKitStyle::color(ALKitStyle::MUTED).get(),LLFontGL::LEFT,LLFontGL::VCENTER);
            LLPanel::draw();
        }
        private:std::string mLabel;

    }
    ;

}

ALKitPage::Section::Section():id("id"),label("label"),cards("cards")
{
}

ALKitPage::Mode::Mode():name("name"),cards("cards")
{
}

ALKitPage::Params::Params()
:   page("page"), summary("summary", ""), master_widget("master_widget", ""),
    master_setting("master_setting", ""), master_label("master_label", ""),
    master_tooltip("master_tooltip", ""), modes("modes", ""),
    mode_setting("mode_setting", ""), mode_widget("mode_widget", ""),
    mode_on("mode_on", ""), mode_tooltip("mode_tooltip", ""),
    mode_adapter("mode_adapter", ""), who_label("who_label", ""),
    who_picker("who_picker", ""), who_adapter("who_adapter", ""),
    preset_mode("preset_mode", "none"), preset_provider("preset_provider", ""),
    undo_context("undo_context", ""), watch_settings("watch_settings", ""),
    chip_mode("chip_mode", "auto"), legacy("legacy", false), section("section"), mode("mode")
{
}

ALKitPage::ALKitPage(const Params& p)
:   LLPanel(p), mPage(p.page), mSummary(p.summary), mWatchSettings(p.watch_settings),
    mMasterWidget(p.master_widget), mMasterSetting(p.master_setting),
    mMasterLabel(p.master_label), mMasterTooltip(p.master_tooltip), mModes(p.modes),
    mModeSetting(p.mode_setting), mModeWidget(p.mode_widget), mModeOn(p.mode_on),
    mModeAdapter(p.mode_adapter), mWhoLabel(p.who_label), mWhoPicker(p.who_picker),
    mWhoAdapter(p.who_adapter), mPresetMode(p.preset_mode), mPresetProvider(p.preset_provider), mChipMode(p.chip_mode),
    mLegacy(p.legacy)
{

    for(const Section&s:p.section)
    {
        SectionData data;
        data.id=s.id;
        data.label=s.label;
        data.cards=tokens(s.cards);
        mSections.push_back(data);
    }

    for(const Mode&m:p.mode)
    {
        ModeData data;
        data.name=m.name;
        data.cards=tokens(m.cards);
        mModeDefs.push_back(data);
    }

}

bool ALKitScroller::handleKeyHere(KEY key,MASK mask)
{
    if(key==KEY_PAGE_UP||key==KEY_PAGE_DOWN)return false;
    return LLScrollContainer::handleKeyHere(key,mask);
}

bool ALKitSearchEditor::handleKeyHere(KEY key,MASK mask)
{
    if(key==KEY_PAGE_UP||key==KEY_PAGE_DOWN)return false;
    if(mask==MASK_NONE&&(key==KEY_UP||key==KEY_DOWN||key==KEY_RETURN))
    {
        if(ALKitShell* shell=ALKitShell::find(this))
        {
            if(shell->handleSearchKey(key))return true;
        }
    }
    return LLLineEditor::handleKeyHere(key,mask);
}

void ALKitDoc::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if(mDimmed)
    {
        LLViewDrawContext context(ALKitStyle::DIM_ALPHA);
        LLPanel::draw();
    }
    else LLPanel::draw();
}

std::string ALKitPage::prefix()const
{

    std::string p=mPage;
    const size_t slash=p.rfind('/');
    if(slash!=std::string::npos)p=p.substr(slash+1);
    const std::string suffix="_tab";
    if(p.size()>suffix.size()&&p.compare(p.size()-suffix.size(),suffix.size(),suffix)==0)p.erase(p.size()-suffix.size());
    return p;

}

bool ALKitPage::postBuild()
{

    if(!LLPanel::postBuild())return false;
    mShell=ALKitShell::find(this);

    const std::string pre=prefix();
    std::vector<std::string>watched=tokens(mWatchSettings);

    std::vector<LLView*>xml_children;
    for(LLView*c:*getChildList())xml_children.push_back(c);

    mBar=makePanel(pre+"_bar");
    addChild(mBar);
    mChipsContainer=makePanel(pre+"_chips");
    addChild(mChipsContainer);
    mWhoContainer=makePanel(pre+"_who");
    addChild(mWhoContainer);
    S32 bar_x=ALKitStyle::PAGE_PAD_LR;

    if(mPresetMode=="provider"&&!mPresetProvider.empty())
    {
        ALKitPresetStrip::Params preset_params;
        preset_params.name=pre+"_presets";
        preset_params.provider=mPresetProvider;
        preset_params.page=mPage;
        preset_params.rect=LLRect(bar_x,34,bar_x+334,10);
        ALKitPresetStrip*strip=LLUICtrlFactory::create<ALKitPresetStrip>(preset_params);
        mBar->addChild(strip);
        bar_x+=340;
    }

    if(mPresetMode=="widgets")if(ALKitPresetHost*host=findChild<ALKitPresetHost>("kit_preset_host"))
    {
        std::vector<LLView*>items;
        for(LLView*c:*host->getChildList())items.push_back(c);
        for(S32 i=0;i<(S32)items.size();++i)
        {
            LLView*c=items[i];
            mBar->addChild(c);
            c->reshape(i==0?ALKitStyle::PRESET_COMBO_W:28,24,true);
            c->setOrigin(bar_x,10);
            bar_x+=c->getRect().getWidth()+ALKitStyle::COL_GAP;
        }
        host->setVisible(false);
    }

    if(mLegacy&&!mSummary.empty())
    {
        LLTextBox::Params summary;
        summary.name=pre+"_legacy_summary";
        summary.rect=LLRect(0,20,ALKitStyle::PRESET_COMBO_W,0);
        summary.initial_value=mSummary;
        summary.use_ellipses=true;
        summary.text_color=ALKitStyle::color(ALKitStyle::MUTED);
        mLegacySummary=LLUICtrlFactory::create<LLTextBox>(summary);
        mBar->addChild(mLegacySummary);
        mLegacySummary->setOrigin(bar_x,12);
        bar_x+=ALKitStyle::PRESET_COMBO_W+ALKitStyle::COL_GAP;
    }

    LLButton*reset=makeButton(pre+"_reset_page","");
    reset->setImageOverlay("Refresh_Off");
    reset->setCommitCallback([this](LLUICtrl*,const LLSD&)
    {
        resetPage();
    }
    );
    mBar->addChild(reset);
    reset->reshape(28,24,true);
    reset->setOrigin(bar_x,10);
    bar_x+=34;

    mUndo=makeButton(pre+"_undo","Undo");
    mRedo=makeButton(pre+"_redo","Redo");
    mUndo->setCommitCallback([this](LLUICtrl*,const LLSD&)
    {
        if(ALKitShell*s=ALKitShell::find(this))s->history().undo();
    }
    );
    mRedo->setCommitCallback([this](LLUICtrl*,const LLSD&)
    {
        if(ALKitShell*s=ALKitShell::find(this))s->history().redo();
    }
    );
    mBar->addChild(mUndo);
    mBar->addChild(mRedo);
    mUndo->reshape(52,24,true);
    mRedo->reshape(52,24,true);
    mUndo->setOrigin(bar_x,10);
    bar_x+=58;
    mRedo->setOrigin(bar_x,10);
    mBarLeftEnd=bar_x+52;

    if(!mMasterWidget.empty())
    {
        mMasterBox=makePanel(pre+"_master_box");
        mBar->addChild(mMasterBox);
        LLTextBox::Params master_label;
        master_label.name=pre+"_master_label";
        master_label.rect=LLRect(0,20,90,0);
        master_label.initial_value=mMasterLabel;
        mMasterBox->addChild(LLUICtrlFactory::create<LLTextBox>(master_label));
        master_label.name=pre+"_master_state";
        master_label.rect=LLRect(92,20,132,0);
        master_label.initial_value="";
        mMasterState=LLUICtrlFactory::create<LLTextBox>(master_label);
        mMasterBox->addChild(mMasterState);
        ALKitSwitch::Params sp;
        sp.name=mMasterWidget;
        sp.rect=LLRect(0,ALKitStyle::SWITCH_H,ALKitStyle::SWITCH_W,0);
        mMaster=LLUICtrlFactory::create<ALKitSwitch>(sp);
        mMaster->setControlName(mMasterSetting,this);
        mMaster->setToolTip(mMasterTooltip);
        mMasterBox->addChild(mMaster);
        mConnections.emplace_back(mMaster->setFlipCallback([this](bool)
        {
            reshape(getRect().getWidth(),getRect().getHeight(),true);
        }
        ));
    }

    if(!mModes.empty())
    {
        ALKitSwitch::Params sp;
        sp.name=mModeWidget;
        sp.rect=LLRect(0,ALKitStyle::SWITCH_H,ALKitStyle::SWITCH_W,0);
        sp.visible=false;
        mModeSwitch=LLUICtrlFactory::create<ALKitSwitch>(sp);
        mModeSwitch->setControlName(mModeSetting,this);
        mBar->addChild(mModeSwitch);
        mConnections.emplace_back(mModeSwitch->setFlipCallback([this](bool)
        {
            syncEffectiveMode();
        }
        ));
        syncEffectiveMode();
    }

    ALKitSearchEditor::Params lp;
    lp.name=pre+"_search";
    lp.rect=LLRect(0,24,ALKitStyle::SEARCH_W,0);
    lp.label="Find a control...";
    mSearch=LLUICtrlFactory::create<ALKitSearchEditor>(lp);
    mSearch->setCommitCallback([this](LLUICtrl*c,const LLSD&)
    {
        setSearchText(c->getValue().asString());
    }
    );
    mBar->addChild(mSearch);

    mBanner=makePanel(pre+"_banner");
    mBanner->setBackgroundVisible(true);
    mBanner->setBackgroundColor(ALKitStyle::color(ALKitStyle::BANNER_BG));
    LLTextBox::Params bt;
    bt.name=pre+"_banner_text";
    bt.rect=LLRect(10,24,360,0);
    bt.initial_value=mMasterLabel+" is off. Controls stay editable.";
    LLTextBox*btext=LLUICtrlFactory::create<LLTextBox>(bt);
    mBanner->addChild(btext);
    LLButton*turn=makeButton(pre+"_master_on","Turn on");
    turn->setCommitCallback([this](LLUICtrl*,const LLSD&)
    {
        if(mMaster)
        {
            mMaster->writeBoundValue(true);mMaster->setValue(true);mMaster->onCommit();
        }
    }
    );
    mBanner->addChild(turn);
    turn->setOrigin(370,4);
    addChild(mBanner);

    if(!mWhoPicker.empty())
    {
        LLTextBox::Params who_hint;
        who_hint.name=pre+"_who_hint";
        who_hint.rect=LLRect(370,24,620,0);
        who_hint.initial_value="Choose who this page edits";
        who_hint.text_color=ALKitStyle::color(ALKitStyle::MUTED);
        mWhoContainer->addChild(LLUICtrlFactory::create<LLTextBox>(who_hint));
        mWhoCombo=findChild<LLComboBox>(mWhoPicker);
        mWhoChip=makeButton(pre+"_who_chip",mWhoLabel);
        mWhoContainer->addChild(mWhoChip);
        if(mWhoCombo)
        {
            mWhoCombo->setVisible(false);
            mWhoChip->setCommitCallback([this](LLUICtrl*,const LLSD&)
            {
                mWhoCombo->setVisible(true);
                mWhoCombo->showList();
            }
            );
            mWhoCombo->setCommitCallback([this](LLUICtrl*,const LLSD&)
            {
                mWhoError=false;
                mWhoCombo->setVisible(false);
                if(mWhoChip)mWhoChip->setLabel(mWhoLabel+": "+mWhoCombo->getSelectedItemLabel()+" \xE2\x96\xBE");
            }
            );
            mWhoChip->setLabel(mWhoLabel+": "+mWhoCombo->getSelectedItemLabel()+" \xE2\x96\xBE");
        }
        if(ALKitShell*shell=ALKitShell::find(this))mConnections.emplace_back(shell->setCastSelectionCallback([this](const ALKitShell::uuid_vec_t&ids)
        {
            if(!mWhoCombo)return;if(!ids.empty())if(ALKitWhoAdapters::Adapter adapter=ALKitWhoAdapters::get(mWhoAdapter))
            {
                const LLSD value=adapter(ids.front());if(value.isDefined())
                {
                    mWhoCombo->setValue(value);mWhoCombo->onCommit();return;
                }
            }
            mWhoError=true;
            if(mWhoChip)mWhoChip->setLabel("That subject isn't selectable here");
        }
        ));
    }

    if(mLegacy)
    {
        LLPanel::Params hp;
        hp.name=pre+"_legacy_host";
        hp.rect=getLocalRect();
        hp.follows.flags(FOLLOWS_ALL);
        hp.background_visible=false;
        mLegacyHost=LLUICtrlFactory::create<LLPanel>(hp);
        addChild(mLegacyHost);
        for(LLView*c:xml_children)if(c!=mBar&&c!=mBanner&&c!=mWhoChip&&c!=mWhoCombo&&dynamic_cast<ALKitPresetHost*>(c)==nullptr)mLegacyHost->addChild(c);
        if(ALKitShell*shell=ALKitShell::find(this))shell->history().watchSettings(mPage,watched);
        reshape(getRect().getWidth(),getRect().getHeight(),true);
        return true;
    }

    std::vector<ALKitCard*>cards;
    for(LLView*c:xml_children)if(ALKitCard*card=dynamic_cast<ALKitCard*>(c))cards.push_back(card);

    ALKitScroller::Params sp;
    sp.name=pre+"_scroll";
    sp.rect=getLocalRect();
    sp.follows.flags(FOLLOWS_ALL);
    sp.is_opaque=false;
    sp.reserve_scroll_corner=false;
    mScroller=LLUICtrlFactory::create<ALKitScroller>(sp);

    ALKitDoc::Params dp;
    dp.name=pre+"_doc";
    dp.rect=getLocalRect();
    dp.follows.flags(FOLLOWS_LEFT|FOLLOWS_TOP|FOLLOWS_RIGHT);
    mDoc=LLUICtrlFactory::create<ALKitDoc>(dp);

    ALFlowGrid::Params gp;
    gp.name=pre+"_grid";
    gp.rect=getLocalRect();
    gp.pack="rows";
    gp.max_columns=2;
    gp.card_width=340;
    gp.column_gap=12;
    gp.row_gap=12;
    gp.pad=0;
    mGrid=LLUICtrlFactory::create<ALFlowGrid>(gp);

    mScroller->addChild(mDoc);
    mDoc->addChild(mGrid);
    addChild(mScroller);

    std::vector<ALKitCard*>ordered;
    for(S32 si=0;si<(S32)mSections.size();++si)
    {
        LLPanel::Params hp;
        hp.name=pre+"_section_"+mSections[si].id;
        hp.rect=LLRect(0,28,100,0);
        SectionHeading*heading=new SectionHeading(hp,mSections[si].label);
        mGrid->addChild(heading);
        mSectionHeadings.push_back(heading);
        for(const std::string&id:mSections[si].cards)for(ALKitCard*card:cards)if(card->id()==id&&std::find(ordered.begin(),ordered.end(),card)==ordered.end())
        {
            mGrid->addChild(card);
            ordered.push_back(card);
        }
    }

    for(ALKitCard*card:cards)if(std::find(ordered.begin(),ordered.end(),card)==ordered.end())mGrid->addChild(card);

    for(ALKitCard*card:cards)
    {
        mCards[card->id()]=card;
        for(ALKitRow*row:card->rows())
        {
            mRows[row->meta().widget]=row;
            if(!row->meta().setting.empty())
            {
                mRows[row->meta().setting]=row;
                watched.push_back(row->meta().setting);
            }
        }
    }

    if(mSections.size()>1)for(S32 i=0;i<(S32)mSections.size();++i)
    {
        LLButton*chip=makeButton(pre+"_chip_"+llformat("%d",i),mSections[i].label+" "+llformat("%d",(S32)mSections[i].cards.size()));
        chip->setCommitCallback([this,i](LLUICtrl*,const LLSD&)
        {
            mChipIndex=i;applyModeAndChip();saveState();if(i<(S32)mSectionHeadings.size()&&mSectionHeadings[i]->getVisible())scrollToView(mSectionHeadings[i]);
        }
        );
        mChipsContainer->addChild(chip);
        mChips.push_back(chip);
    }

    if(ALKitShell*shell=ALKitShell::find(this))shell->history().watchSettings(mPage,watched);

    applyModeAndChip();
    restoreState();
    reshape(getRect().getWidth(),getRect().getHeight(),true);
    return true;

}

void ALKitPage::setModeIndex(S32 index,bool explicit_user)
{
    if(index<0||index>=(S32)mModeDefs.size())return;
    if(mModeSwitch)
    {
        bool on=mModeDefs[index].name==mModeOn;
        if(ALKitModeAdapters::Adapter adapter=ALKitModeAdapters::get(getParent(),mModeAdapter))
        {
            if(!explicit_user)
            {
                ALKitHistory::ScopedSuspend suspend;
                on=adapter(on,ALKitModeAdapters::PASSIVE);
            }
            else on=adapter(on,ALKitModeAdapters::USER);
        }
        mModeSwitch->writeBoundValue(on);
        mModeSwitch->setValue(on);
        syncEffectiveMode();
    }
    else
    {
        mModeIndex=index;
        applyModeAndChip();
    }
    if(explicit_user)saveState();
}

void ALKitPage::syncEffectiveMode()
{
    if(!mModeSwitch)return;
    const bool on=mModeSwitch->isOn();
    for(S32 i=0;i<(S32)mModeDefs.size();++i)if((mModeDefs[i].name==mModeOn)==on)
    {
        mModeIndex=i;
        break;
    }
    applyModeAndChip();
}

void ALKitPage::rebindRows()
{
    for(auto&entry:mRows)entry.second->resolveBinding();
}

void ALKitPage::applyModeAndChip()
{

    S32 visible_count=0;
    for(const auto&pair:mCards)
    {
        bool visible=true;
        if(!mModeDefs.empty())
        {
            bool listed=false;
            for(const ModeData&m:mModeDefs)listed|=std::find(m.cards.begin(),m.cards.end(),pair.first)!=m.cards.end();
            visible=!listed||(mModeIndex<(S32)mModeDefs.size()&&std::find(mModeDefs[mModeIndex].cards.begin(),mModeDefs[mModeIndex].cards.end(),pair.first)!=mModeDefs[mModeIndex].cards.end());
        }
        if(visible)++visible_count;
    }
    const bool filtered=mChipMode=="filter"||(mChipMode=="auto"&&visible_count>=6);

    for(auto&pair:mCards)
    {
        bool mode=true;
        if(!mModeDefs.empty())
        {
            bool listed=false;
            for(const ModeData&m:mModeDefs)listed|=std::find(m.cards.begin(),m.cards.end(),pair.first)!=m.cards.end();
            mode=!listed||(mModeIndex<(S32)mModeDefs.size()&&std::find(mModeDefs[mModeIndex].cards.begin(),mModeDefs[mModeIndex].cards.end(),pair.first)!=mModeDefs[mModeIndex].cards.end());
        }
        bool section=true;
        if(filtered&&!mSections.empty()&&mChipIndex<(S32)mSections.size())section=std::find(mSections[mChipIndex].cards.begin(),mSections[mChipIndex].cards.end(),pair.first)!=mSections[mChipIndex].cards.end();
        pair.second->setVisible(mode&&section);
    }

    for(S32 i=0;i<(S32)mSectionHeadings.size();++i)
    {
        bool any=false;
        if(i<(S32)mSections.size())for(const std::string&id:mSections[i].cards)
        {
            auto found=mCards.find(id);
            if(found!=mCards.end()&&found->second->getVisible())
            {
                any=true;
                break;
            }
        }
        mSectionHeadings[i]->setVisible(!filtered&&any);
        if(i<(S32)mChips.size())mChips[i]->setVisible(any);
    }
    requestLayout();

}

void ALKitPage::updateMaster()
{
    const bool on=!mMaster||mMaster->isOn();
    if(mBanner)setVisibleIfChanged(mBanner,!on);
    if(mDoc)mDoc->setDimmed(!on);
    if(mMasterState)setTextIfChanged(mMasterState,on?"On":"Off");
}

void ALKitPage::reshape(S32 w,S32 h,bool p)
{

    LLPanel::reshape(w,h,p);
    S32 top=h;
    if(!mChips.empty())
    {
        top-=ALKitStyle::CHIPS_H;
        mChipsContainer->reshape(w,ALKitStyle::CHIPS_H,true);
        mChipsContainer->setOrigin(0,top);
        S32 x=ALKitStyle::PAGE_PAD_LR;
        for(LLButton*b:mChips)
        {
            b->reshape(100,24,true);
            b->setOrigin(x,7);
            x+=106;
        }
    }

    if(mWhoChip)
    {
        top-=ALKitStyle::WHO_H;
        mWhoContainer->reshape(w,ALKitStyle::WHO_H,true);
        mWhoContainer->setOrigin(0,top);
        mWhoChip->reshape(llmin(360,w-2*ALKitStyle::PAGE_PAD_LR),24,true);
        mWhoChip->setOrigin(ALKitStyle::PAGE_PAD_LR,5);
    }
    top-=ALKitStyle::BAR_H;
    if(mBar)
    {
        mBar->reshape(w,ALKitStyle::BAR_H,true);
        mBar->setOrigin(0,top);
        const S32 master_x=mBarLeftEnd+ALKitStyle::COL_GAP;
        const S32 right=w-ALKitStyle::PAGE_PAD_LR;
        const bool master_fits=mMasterBox&&master_x+170<=w-ALKitStyle::PAGE_PAD_LR;
        const S32 occupied_right=master_fits?master_x+170:mBarLeftEnd;
        const S32 search_w=llmin(ALKitStyle::SEARCH_W,llmax(0,right-occupied_right-ALKitStyle::COL_GAP));
        const S32 search_x=right-search_w;
        const bool search_fits=mSearch&&search_w>=120;
        if(mMasterBox){mMasterBox->reshape(170,24,true);mMasterBox->setOrigin(master_x,10);setVisibleIfChanged(mMasterBox,master_fits);}
        if(mMaster)mMaster->setOrigin(136,3);
        if(mSearch){mSearch->reshape(search_w,24,true);mSearch->setOrigin(search_x,10);setVisibleIfChanged(mSearch,search_fits);}
    }
    updateMaster();
    if(mBanner&&mBanner->getVisible())
    {
        top-=ALKitStyle::BANNER_H;
        mBanner->reshape(w,ALKitStyle::BANNER_H,true);
        mBanner->setOrigin(0,top);
    }
    if(mScroller)
    {
        mScroller->reshape(w,llmax(1,top),true);
        mScroller->setOrigin(0,0);
        relayoutNow();
    }
    if(mLegacyHost)
    {
        mLegacyHost->reshape(w,llmax(1,top),true);
        mLegacyHost->setOrigin(0,0);
    }

}

void ALKitPage::relayoutNow()
{
    if(!mGrid||!mScroller)return;
    static LLCachedControl<S32>scrollbar_size(gSavedSettings,"UIScrollbarSize");
    const S32 sb=llmax(0,(S32)scrollbar_size);
    const S32 width=llmax(1,getRect().getWidth()-2*ALKitStyle::PAGE_PAD_LR-sb);
    mGrid->reshape(width,mGrid->getRect().getHeight(),true);
    const S32 natural=mGrid->getRect().getHeight()+ALKitStyle::PAGE_PAD_T+ALKitStyle::PAGE_PAD_B;
    const S32 viewport=llmax(1,mScroller->getRect().getHeight()-2*mScroller->getBorderWidth());
    const S32 doc_h=llmax(natural,viewport);
    mDoc->reshape(getRect().getWidth(),doc_h,true);
    mGrid->setOrigin(ALKitStyle::PAGE_PAD_LR,doc_h-ALKitStyle::PAGE_PAD_T-mGrid->getRect().getHeight());
}

void ALKitPage::requestLayout()
{
    if(mLayoutPending)return;
    mLayoutPending=true;
    LLHandle<LLPanel>handle=getHandle();
    doOnIdleOneTime([handle]()
    {
        if(LLPanel*p=handle.get())
        {
            ALKitPage*page=dynamic_cast<ALKitPage*>(p);page->mLayoutPending=false;page->relayoutNow();
        }
    }
    );
}

void ALKitPage::scrollToView(LLView* view)
{
    if(!view||!mScroller||!mDoc)return;
    LLHandle<LLPanel>page_handle=getHandle();
    LLHandle<LLView>view_handle=view->getHandle();
    doOnIdleOneTime([page_handle,view_handle]()
    {
        ALKitPage*page=dynamic_cast<ALKitPage*>(page_handle.get());
        LLView*target=view_handle.get();
        if(!page||!target)return;
        page->relayoutNow();
        LLRect rect;
        if(target->localRectToOtherView(target->getLocalRect(),&rect,page->mDoc))
        {
            page->mScroller->scrollToShowRect(rect);
        }
    });
}

ALKitRow*ALKitPage::findRow(const std::string&k)const
{
    auto i=mRows.find(k);
    return i==mRows.end()?nullptr:i->second;
}
ALKitRow*ALKitPage::findRowBySetting(const std::string&s)const
{
    return findRow(s);
}
ALKitCard*ALKitPage::findCard(const std::string&k)const
{
    auto i=mCards.find(k);
    return i==mCards.end()?nullptr:i->second;
}

void ALKitPage::applySearch(const ALKitSearch::Hits&hits)
{
    mSearching=true;
    for(auto&p:mRows)p.second->setFilterVisible(false);
    for(const auto&h:hits)if(h.row)h.row->setFilterVisible(true);
    for(auto&c:mCards)
    {
        bool any=false;
        for(ALKitRow*r:c.second->rows())for(const auto&h:hits)if(h.row==r)
        {
            any=true;
            break;
        }
        c.second->setVisible(any);
    }
    for(LLPanel*h:mSectionHeadings)h->setVisible(false);
    requestLayout();
}

void ALKitPage::clearSearch()
{
    mSearching=false;
    for(auto&p:mRows)p.second->setFilterVisible(true);
    applyModeAndChip();
}

bool ALKitPage::revealRow(ALKitRow*r,bool flash_it)
{
    if(!r)return false;
    r->setFilterVisible(true);
    if(flash_it)r->flash();
    for(auto&c:mCards)if(std::find(c.second->rows().begin(),c.second->rows().end(),r)!=c.second->rows().end())
    {
        c.second->revealRow(r);
        return revealCard(c.second,flash_it);
    }
    return false;
}

bool ALKitPage::revealCard(ALKitCard*c,bool)
{
    if(!c)return false;
    for(S32 i=0;i<(S32)mModeDefs.size();++i)if(std::find(mModeDefs[i].cards.begin(),mModeDefs[i].cards.end(),c->id())!=mModeDefs[i].cards.end())
    {
        setModeIndex(i,false);
        break;
    }
    for(S32 i=0;i<(S32)mSections.size();++i)if(std::find(mSections[i].cards.begin(),mSections[i].cards.end(),c->id())!=mSections[i].cards.end())
    {
        mChipIndex=i;
        break;
    }
    c->setVisible(true);
    requestLayout();
    scrollToView(c);
    return true;
}

void ALKitPage::setSearchText(const std::string&q)
{
    if(mSearch&&mSearch->getValue().asString()!=q)mSearch->setValue(q);
    if(q.empty())clearSearch();
}

void ALKitPage::resetPage()
{
    if(mShell)mShell->history().beginGesture(ALKitHistory::MOUSE,mPage,"Reset page");
    for(auto&card:mCards)for(ALKitRow*row:card.second->rows())if(row->resettable()&&!row->cardPreset()&&!row->whoPicker())row->reset();
}

bool ALKitPage::cycleChip(S32 delta)
{
    if(mChips.empty())return false;
    S32 next=mChipIndex;
    for(S32 count=0;count<(S32)mChips.size();++count)
    {
        next=(next+delta+(S32)mChips.size())%(S32)mChips.size();
        if(mChips[next]->getVisible())
        {
            mChipIndex=next;
            applyModeAndChip();
            saveState();
            if(next<(S32)mSectionHeadings.size()&&mSectionHeadings[next]->getVisible())scrollToView(mSectionHeadings[next]);
            return true;
        }
    }
    return false;
}

void ALKitPage::saveState()
{
    LLSD state=gSavedSettings.getLLSD("DirectorV2PageState");
    LLSD page=LLSD::emptyMap();
    page["chip"]=mChipIndex;
    page["mode"]=mModeIndex;
    for(const auto& card:mCards)
    {
        page["cards"][card.first]["body"]=card.second->bodyOpen();
        page["cards"][card.first]["more"]=card.second->moreOpen();
    }
    state[mPage]=page;
    gSavedSettings.setLLSD("DirectorV2PageState",state);
}

void ALKitPage::restoreState()
{
    const LLSD state=gSavedSettings.getLLSD("DirectorV2PageState");
    if(!state.has(mPage))return;
    const LLSD& page=state[mPage];
    if(page.has("chip"))mChipIndex=llclamp(page["chip"].asInteger(),0,llmax(0,(S32)mSections.size()-1));
    if(page.has("mode")&&!mModeDefs.empty())setModeIndex(llclamp(page["mode"].asInteger(),0,(S32)mModeDefs.size()-1),false);
    if(page.has("cards"))for(auto& card:mCards)if(page["cards"].has(card.first))
    {
        const LLSD& saved=page["cards"][card.first];
        card.second->restoreOpenState(saved["body"].asBoolean(),saved["more"].asBoolean());
    }
    applyModeAndChip();
}

void ALKitPage::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if(mReadoutClock.due())
    {
        for(const auto& card:mCards)if(card.second->getVisible())
            for(ALKitRow*row:card.second->rows())if(row->isEffectivelyVisible())row->pollLiveValue();
        if(mWhoCombo&&mWhoChip&&!mWhoError)
        {
            const std::string label=mWhoLabel+": "+mWhoCombo->getSelectedItemLabel()+" \xE2\x96\xBE";
            if(mWhoChip->getLabelSelected()!=label)mWhoChip->setLabel(label);
        }
    }
    if(mShell)
    {
        const U64 revision=mShell->history().revision();
        if(revision!=mHistoryRevision)
        {
            mHistoryRevision=revision;
            if(mUndo)mUndo->setEnabled(mShell->history().canUndo());
            if(mRedo)mRedo->setEnabled(mShell->history().canRedo());
        }
    }
    updateMaster();
    LLPanel::draw();
}
