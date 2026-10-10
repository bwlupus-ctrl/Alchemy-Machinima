#include "llviewerprecompiledheaders.h"
#include "alkitshell.h"
#include "alkitbar.h"
#include "alkitbinding.h"
#include "alkitcard.h"
#include "alkitlazypage.h"
#include "alkitpage.h"
#include "alkitpopover.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "lleventtimer.h"
#include "lllineeditor.h"
#include "llrender.h"
#include "llsliderwheel.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
static LLDefaultChildRegistry::Register<ALKitShell> r("kit_shell");

namespace
{
template<typename T>
T* findFirst(LLView* root)
{
    if(!root)return nullptr;
    if(T* result=dynamic_cast<T*>(root))return result;
    for(LLView* child:*root->getChildList())if(T* result=findFirst<T>(child))return result;
    return nullptr;
}

void collectRows(LLView* root,std::vector<ALKitRow*>& rows)
{
    if(!root)return;
    if(ALKitRow* row=dynamic_cast<ALKitRow*>(root))rows.push_back(row);
    for(LLView* child:*root->getChildList())collectRows(child,rows);
}
}

ALKitShell::ALKitShell(const Params&p):LLPanel(p),mIndexFile(p.index_file)
{
    for(const RailItem& item:p.rail_item)
    {
        mRailData.push_back({item.group,item.label,item.icon,item.master_settings});
    }
    mHistory.setOwner(this);
}

ALKitShell::~ALKitShell()
{
    if(auto timer=mSearchDebounce.lock())delete timer.get();
}

bool ALKitShell::postBuild()
{
    if(!LLPanel::postBuild())return false;
    mDirectorTabs=findChild<LLTabContainer>("director_tabs");
    mRoster=findChild<LLPanel>("kit_cast_roster");
    mTransport=findChild<LLPanel>("kit_transport");
    buildChrome();
    mConnections.emplace_back(setPageBuiltCallback([this](const std::string& name,LLPanel* panel)
    {
        pageBuilt(name,panel);
    }));
    layoutChrome();
    return true;
}

void ALKitShell::buildChrome()
{
    LLPanel::Params panel_params;
    panel_params.mouse_opaque=false;
    panel_params.rect=LLRect(0,1,1,0);
    panel_params.name="kit_rail";
    mRail=LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mRail);
    panel_params.name="kit_tabstrip";
    mTabStrip=LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mTabStrip);
    panel_params.name="kit_help";
    mHelpPanel=LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mHelpPanel);
    panel_params.name="kit_help_strip";
    mHelpStrip=LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mHelpStrip);
    mHelp.attach(mHelpPanel,mHelpStrip);

    LLButton::Params arrow_params;
    arrow_params.name="kit_tab_prev";
    arrow_params.label="<";
    arrow_params.rect=LLRect(0,28,24,0);
    mTabPrev=LLUICtrlFactory::create<LLButton>(arrow_params);
    mTabStrip->addChild(mTabPrev);
    arrow_params.name="kit_tab_next";
    arrow_params.label=">";
    mTabNext=LLUICtrlFactory::create<LLButton>(arrow_params);
    mTabStrip->addChild(mTabNext);
    LLHandle<ALKitShell> tab_handle=getDerivedHandle<ALKitShell>();
    mTabPrev->setCommitCallback([tab_handle](LLUICtrl*,const LLSD&)
    {
        if(ALKitShell* shell=tab_handle.get())
        {
            shell->mTabOffset=llmax(0,shell->mTabOffset-1);
            shell->layoutChrome();
        }
    });
    mTabNext->setCommitCallback([tab_handle](LLUICtrl*,const LLSD&)
    {
        if(ALKitShell* shell=tab_handle.get())
        {
            ++shell->mTabOffset;
            shell->layoutChrome();
        }
    });

    ALKitPopover::Params search_params;
    search_params.name="kit_search_results";
    search_params.rect=LLRect(0,390,430,0);
    search_params.side="below";
    search_params.visible=false;
    mSearchPopover=LLUICtrlFactory::create<ALKitPopover>(search_params);
    addChild(mSearchPopover);
    LLTextBox::Params text_params;
    text_params.name="kit_search_header";
    text_params.rect=LLRect(10,380,420,358);
    mSearchHeader=LLUICtrlFactory::create<LLTextBox>(text_params);
    mSearchPopover->addChild(mSearchHeader);
    for(S32 index=0;index<12;++index)
    {
        LLButton::Params result_params;
        result_params.name="kit_search_result_"+llformat("%d",index);
        result_params.rect=LLRect(10,354-index*25,420,331-index*25);
        result_params.label="";
        LLButton*button=LLUICtrlFactory::create<LLButton>(result_params);
        mSearchPopover->addChild(button);
        LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
        button->setCommitCallback([handle,index](LLUICtrl*,const LLSD&)
        {
            if(ALKitShell* shell=handle.get())
            {
                shell->mSearchSelection=index;
                shell->activateSearchResult();
            }
        });
        mSearchButtons.push_back(button);
    }
    text_params.name="kit_search_footer";
    text_params.rect=LLRect(10,48,420,20);
    text_params.use_ellipses=true;
    mSearchFooter=LLUICtrlFactory::create<LLTextBox>(text_params);
    mSearchPopover->addChild(mSearchFooter);

    if(LLButton* cast_chip=findChild<LLButton>("btn_cast_chip"))
    {
        ALKitPopover::Params params;
        params.name="kit_cast_popover";
        params.rect=LLRect(0,360,300,0);
        params.side="above";
        params.visible=false;
        mCastPopover=LLUICtrlFactory::create<ALKitPopover>(params);
        addChild(mCastPopover);
        mCastPopover->bindAnchor(cast_chip);
        LLHandle<ALKitPopover> handle=mCastPopover->getDerivedHandle<ALKitPopover>();
        cast_chip->setCommitCallback([handle](LLUICtrl*,const LLSD&)
        {
            if(ALKitPopover* popover=handle.get())popover->show();
        });
    }

    for(const RailData& item:mRailData)
    {
        LLButton::Params params;
        params.name="kit_rail_"+item.group;
        params.label=item.label;
        params.rect=LLRect(0,ALKitStyle::RAIL_ITEM_H,ALKitStyle::RAIL_W,0);
        LLButton* button=LLUICtrlFactory::create<LLButton>(params);
        button->setImageOverlay(item.icon);
        const std::string group=item.group;
        LLHandle<ALKitShell> shell_handle=getDerivedHandle<ALKitShell>();
        button->setCommitCallback([shell_handle,group](LLUICtrl*,const LLSD&)
        {
            if(ALKitShell* shell=shell_handle.get())
            {
                if(shell->mDirectorTabs)shell->mDirectorTabs->selectTabByName(group);
                shell->layoutChrome();
            }
        });
        mRail->addChild(button);
        mRailButtons.push_back(button);
    }
    if(mDirectorTabs)
    {
        for(S32 group_index=0;group_index<mDirectorTabs->getTabCount();++group_index)
        {
            LLPanel* group=mDirectorTabs->getPanelByIndex(group_index);
            if(!group)continue;
            LLTabContainer* tabs=findFirst<LLTabContainer>(group);
            if(!tabs)continue;
            for(S32 page_index=0;page_index<tabs->getTabCount();++page_index)
            {
                LLPanel* page=tabs->getPanelByIndex(page_index);
                if(!page)continue;
                LLButton::Params params;
                params.name="kit_tab_"+page->getName();
                params.label=page->getLabel();
                params.rect=LLRect(0,32,100,0);
                LLButton* button=LLUICtrlFactory::create<LLButton>(params);
                const std::string page_name=page->getName();
                LLHandle<ALKitShell> shell_handle=getDerivedHandle<ALKitShell>();
                button->setCommitCallback([shell_handle,page_name](LLUICtrl*,const LLSD&)
                {
                    if(ALKitShell* shell=shell_handle.get())
                    {
                        shell->selectPage(page_name);
                        shell->layoutChrome();
                    }
                });
                mTabStrip->addChild(button);
                mTabButtons.push_back(button);
                ALKitLazyPage* lazy=dynamic_cast<ALKitLazyPage*>(page);
                mTabs.push_back({group->getName(),page_name,page->getLabel(),lazy?lazy->masterSetting():std::string(),button});
            }
        }
    }
    for(const RailData& item:mRailData)for(const std::string& setting:LLStringUtil::getTokens(item.master_settings,","))
    {
        mConnections.emplace_back(ALKitSettingWatch::instance().watch(setting,
            [handle=getDerivedHandle<ALKitShell>()](const LLSD&,const LLSD&)
            {
                if(ALKitShell* shell=handle.get())shell->updatePips();
            }));
    }
    for(const TabData& tab:mTabs)if(!tab.master_setting.empty())
    {
        mConnections.emplace_back(ALKitSettingWatch::instance().watch(tab.master_setting,
            [handle=getDerivedHandle<ALKitShell>()](const LLSD&,const LLSD&)
            {
                if(ALKitShell* shell=handle.get())shell->updatePips();
            }));
    }
    updatePips();
}

void ALKitShell::updatePips()
{
    for(S32 index=0;index<(S32)mRailButtons.size();++index)
    {
        bool off=false;
        for(const std::string& setting:LLStringUtil::getTokens(mRailData[index].master_settings,","))
        {
            if(gSavedSettings.controlExists(setting)&&!gSavedSettings.getBOOL(setting))off=true;
        }
        mRailButtons[index]->setLabel(mRailLabels?mRailData[index].label+(off?"  \xE2\x97\x8B":""):std::string());
    }
    for(TabData& tab:mTabs)
    {
        const bool off=!tab.master_setting.empty()&&gSavedSettings.controlExists(tab.master_setting)&&!gSavedSettings.getBOOL(tab.master_setting);
        tab.button->setLabel(tab.label+(off?"  \xE2\x97\x8B":""));
    }
}

void ALKitShell::reshape(S32 width,S32 height,bool called_from_parent)
{
    LLPanel::reshape(width,height,called_from_parent);
    const bool labels=mRailLabels?width>=ALKitStyle::BP_RAIL:width>=ALKitStyle::BP_RAIL+ALKitStyle::BP_HYST;
    const bool help=mWideHelp?width>=ALKitStyle::BP_FULL:width>=ALKitStyle::BP_FULL+ALKitStyle::BP_HYST;
    mRailLabels=labels;
    mWideHelp=help;
    layoutChrome();
}

void ALKitShell::layoutChrome()
{
    if(!mDirectorTabs||!mRail||!mTabStrip||!mTransport)return;
    const S32 width=getRect().getWidth();
    const S32 height=getRect().getHeight();
    const S32 rail_width=mRailLabels?ALKitStyle::RAIL_W:ALKitStyle::RAIL_ICON_W;
    const S32 help_width=mWideHelp?ALKitStyle::HELP_W:0;
    const S32 transport_height=ALKitStyle::TRANSPORT_H;
    const S32 help_strip_height=mWideHelp?0:ALKitStyle::HELP_STRIP_H;
    mTransport->reshape(width,transport_height,true);
    mTransport->setOrigin(0,0);
    mRail->reshape(rail_width,height-transport_height,true);
    mRail->setOrigin(0,transport_height);
    mTabStrip->reshape(width-rail_width-help_width,ALKitStyle::TABSTRIP_H,true);
    mTabStrip->setOrigin(rail_width,height-ALKitStyle::TABSTRIP_H);
    mDirectorTabs->reshape(width-rail_width-help_width,height-transport_height-ALKitStyle::TABSTRIP_H-help_strip_height,true);
    mDirectorTabs->setOrigin(rail_width,transport_height+help_strip_height);
    mHelpPanel->setVisible(mWideHelp);
    mHelpPanel->reshape(help_width,height-transport_height,true);
    mHelpPanel->setOrigin(width-help_width,transport_height);
    mHelpStrip->setVisible(!mWideHelp);
    mHelpStrip->reshape(width-rail_width,help_strip_height,true);
    mHelpStrip->setOrigin(rail_width,transport_height);
    mHelp.layout(help_width,height-transport_height,width-rail_width);
    S32 y=height-transport_height-ALKitStyle::RAIL_ITEM_H;
    for(S32 i=0;i<(S32)mRailButtons.size();++i)
    {
        LLButton* button=mRailButtons[i];
        button->reshape(rail_width,ALKitStyle::RAIL_ITEM_H,true);
        button->setOrigin(0,y);
        y-=ALKitStyle::RAIL_ITEM_H;
    }
    if(mRoster)
    {
        const bool roster_in_rail=mRailLabels&&y>=120;
        mRoster->setVisible(roster_in_rail);
        if(roster_in_rail)
        {
            if(mRoster->getParent()!=mRail)mRail->addChild(mRoster);
            mRoster->reshape(rail_width,llmax(120,y),true);
            mRoster->setOrigin(0,0);
        }
        else if(mCastPopover)
        {
            if(mRoster->getParent()!=mCastPopover)mCastPopover->addChild(mRoster);
            mRoster->setVisible(true);
            mRoster->reshape(280,340,true);
            mRoster->setOrigin(10,10);
        }
    }
    LLPanel* current_group=mDirectorTabs->getCurrentPanel();
    const std::string group_name=current_group?current_group->getName():std::string();
    LLTabContainer* current_tabs=findFirst<LLTabContainer>(current_group);
    LLPanel* current_leaf=current_tabs?current_tabs->getCurrentPanel():nullptr;
    const std::string current_page=current_leaf?current_leaf->getName():std::string();
    for(S32 index=0;index<(S32)mRailButtons.size();++index)
    {
        mRailButtons[index]->setToggleState(index<(S32)mRailData.size()&&mRailData[index].group==group_name);
    }
    refreshModeProxy();
    const ALKitPage* mode_page=currentKitPage();
    const S32 mode_width=(mode_page?mode_page->modeCount():0)*86;
    S32 visible_tabs=0;
    for(const TabData& tab:mTabs)if(tab.group==group_name)++visible_tabs;
    const S32 available=llmax(100,mTabStrip->getRect().getWidth()-mode_width-16);
    const bool overflow=visible_tabs*104>available;
    const S32 slots=llmax(1,(available-(overflow?56:0))/104);
    mTabOffset=llclamp(mTabOffset,0,llmax(0,visible_tabs-slots));
    mTabPrev->setVisible(overflow);
    mTabNext->setVisible(overflow);
    if(overflow)
    {
        mTabPrev->setOrigin(4,8);
        mTabNext->setOrigin(available-28,8);
    }
    S32 tab_x=overflow?32:8;
    S32 group_tab=0;
    for(TabData& tab:mTabs)
    {
        const bool in_group=tab.group==group_name;
        const bool visible=in_group&&group_tab>=mTabOffset&&group_tab<mTabOffset+slots;
        tab.button->setVisible(visible);
        tab.button->setToggleState(tab.page==current_page);
        if(visible)
        {
            tab.button->reshape(100,32,true);
            tab.button->setOrigin(tab_x,4);
            tab_x+=104;
        }
        if(in_group)++group_tab;
    }
    updatePips();
}

void ALKitShell::refreshModeProxy()
{
    ALKitPage* page=currentKitPage();
    const S32 count=page?page->modeCount():0;
    while((S32)mModeButtons.size()<count)
    {
        const S32 index=(S32)mModeButtons.size();
        LLButton::Params params;
        params.name="kit_mode_seg_"+llformat("%d",index);
        params.rect=LLRect(0,28,82,0);
        LLButton* button=LLUICtrlFactory::create<LLButton>(params);
        mTabStrip->addChild(button);
        LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
        button->setCommitCallback([handle,index](LLUICtrl*,const LLSD&)
        {
            if(ALKitShell* shell=handle.get())if(ALKitPage* current=shell->currentKitPage())
            {
                current->setModeIndex(index,true);
                shell->refreshModeProxy();
            }
        });
        mModeButtons.push_back(button);
    }
    S32 x=mTabStrip->getRect().getWidth()-count*86-4;
    for(S32 index=0;index<(S32)mModeButtons.size();++index)
    {
        const bool visible=index<count;
        mModeButtons[index]->setVisible(visible);
        if(visible)
        {
            mModeButtons[index]->setLabel(page->modeName(index));
            mModeButtons[index]->setToggleState(index==page->modeIndex());
            mModeButtons[index]->setOrigin(x+index*86,8);
        }
    }
}

void ALKitShell::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALKitShell draw");
    LLPanel::draw();
    LLView* view=mHighlight.get();
    if(view&&mHighlightTimer.getElapsedTimeF32()<ALKitStyle::FLASH_SEC)
    {
        LLRect rect;
        if(view->localRectToOtherView(view->getLocalRect(),&rect,this))
        {
            gGL.color4fv(ALKitStyle::color(ALKitStyle::ACCENT).get().mV);
            gl_rect_2d(rect,false);
        }
    }
}

ALKitShell* ALKitShell::find(LLView*v)
{
    for(;v;v=v->getParent())
    {
        if(auto*s=dynamic_cast<ALKitShell*>(v))
        {
            return s;
        }
    }
    return nullptr;
}

bool ALKitShell::navigateTo(const std::string& path)
{
    ALKitTarget target;
    const size_t slash=path.find('/');
    if(slash==std::string::npos)target.page_key=path;
    else
    {
        target.page_key=path.substr(0,slash);
        target.widget=path.substr(slash+1);
    }
    return navigateTo(target);
}
bool ALKitShell::navigateTo(const ALKitTarget& target)
{
    ALKitHistory::ScopedSuspend suspend;
    clearSearch();
    LLPanel* panel=ensurePageBuilt(target.page_key);
    if(!panel)return false;
    if(ALKitPage* page=findFirst<ALKitPage>(panel))
    {
        ALKitRow* row=!target.widget.empty()?page->findRow(target.widget):page->findRowBySetting(target.setting);
        if(row)return page->revealRow(row,true);
        if(ALKitCard* card=page->findCard(target.card_id))return page->revealCard(card,true);
    }
    if(!target.widget.empty())if(LLView* view=panel->findChild<LLView>(target.widget))
    {
        mHighlight=view->getHandle();
        mHighlightTimer.reset();
        return true;
    }
    return false;
}

bool ALKitShell::selectPage(const std::string& page_name)
{
    if(ALKitPage* current=currentKitPage())current->saveState();
    ALKitLazyPage* page=findChild<ALKitLazyPage>(page_name);
    if(!page)return false;
    LLTabContainer* inner=dynamic_cast<LLTabContainer*>(page->getParent());
    LLPanel* group=inner?dynamic_cast<LLPanel*>(inner->getParent()):nullptr;
    if(mDirectorTabs&&group)mDirectorTabs->selectTabByName(group->getName());
    if(inner)inner->selectTabByName(page_name);
    return page->ensureBuilt()!=nullptr;
}

LLPanel* ALKitShell::ensurePageBuilt(const std::string& page_name)
{
    std::string leaf=page_name;
    const size_t slash=leaf.rfind('/');
    if(slash!=std::string::npos)leaf=leaf.substr(slash+1);
    if(!selectPage(leaf))return nullptr;
    ALKitLazyPage* page=findChild<ALKitLazyPage>(leaf);
    return page?page->ensureBuilt():nullptr;
}

ALKitPage* ALKitShell::currentKitPage()const
{
    if(!mDirectorTabs)return nullptr;
    LLPanel* group=mDirectorTabs->getCurrentPanel();
    LLTabContainer* tabs=group?group->findChild<LLTabContainer>(group->getName().substr(0,group->getName().find("_group"))+"_tabs",false):nullptr;
    LLPanel* leaf=tabs?tabs->getCurrentPanel():nullptr;
    return findFirst<ALKitPage>(leaf);
}

std::string ALKitShell::currentPageKey()const
{
    ALKitPage* page=currentKitPage();
    return page?page->pageKey():std::string();
}

void ALKitShell::pageBuilt(const std::string&,LLPanel* panel)
{
    ALKitPage* page=dynamic_cast<ALKitPage*>(panel);
    if(!page)page=findFirst<ALKitPage>(panel);
    if(!page)return;
    layoutChrome();
    std::string prefix=page->pageKey();
    const size_t slash=prefix.rfind('/');
    if(slash!=std::string::npos)prefix=prefix.substr(slash+1);
    if(prefix.size()>4&&prefix.compare(prefix.size()-4,4,"_tab")==0)prefix.erase(prefix.size()-4);
    if(LLLineEditor* editor=page->findChild<LLLineEditor>(prefix+"_search"))
    {
        LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
        editor->setKeystrokeCallback([handle](LLLineEditor* line,void*)
        {
            if(ALKitShell* shell=handle.get())shell->queueSearch(line->getText());
        },nullptr);
        if(mSearchPopover)mSearchPopover->bindAnchor(editor);
    }
    if(LLButton* who=page->findChild<LLButton>(prefix+"_who_chip"))
    {
        std::vector<ALKitRow*> rows;
        collectRows(page,rows);
        for(ALKitRow* row:rows)if(row->whoPicker()&&row->control())
        {
            ALKitPopover::Params params;
            params.name=prefix+"_who_popover";
            params.rect=LLRect(0,40,360,0);
            params.visible=false;
            ALKitPopover* popover=LLUICtrlFactory::create<ALKitPopover>(params);
            addChild(popover);
            popover->bindAnchor(who);
            LLUICtrl* picker=row->control();
            popover->addChild(picker);
            picker->reshape(340,24,true);
            picker->setOrigin(10,8);
            picker->setVisible(true);
            LLHandle<ALKitPopover> handle=popover->getDerivedHandle<ALKitPopover>();
            LLHandle<LLUICtrl> picker_handle=picker->getHandle();
            who->setCommitCallback([handle,picker_handle](LLUICtrl*,const LLSD&)
            {
                if(ALKitPopover* target=handle.get())
                {
                    if(LLUICtrl* control=picker_handle.get())
                    {
                        control->reshape(340,24,true);
                        control->setOrigin(10,8);
                        control->setVisible(true);
                    }
                    target->show();
                }
            });
            break;
        }
    }
    startSearchLoad();
}

void ALKitShell::runSearch(const std::string& query)
{
    mSearch.setQuery(query);
    ALKitPage* page=currentKitPage();
    if(!page)return;
    if(query.empty())
    {
        page->clearSearch();
        if(mSearchPopover)mSearchPopover->hide();
        mSearchHits.clear();
        mHasTopSearchHit=false;
        return;
    }
    mSearch.setCurrentPage(page->pageKey());
    ALKitSearch::Hits hits=mSearch.query(query);
    ALKitSearch::Hits local;
    for(ALKitSearch::Hit hit:hits)if(hit.page_key==page->pageKey())
    {
        hit.row=!hit.widget.empty()?page->findRow(hit.widget):page->findRowBySetting(hit.setting);
        if(hit.row)local.push_back(std::move(hit));
    }
    page->applySearch(local);
    updateSearchResults(hits,page);
}

void ALKitShell::queueSearch(const std::string& query)
{
    mPendingQuery=query;
    mSearch.setQuery(query);
    if(auto timer=mSearchDebounce.lock())delete timer.get();
    LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
    LLEventTimer* timer=LLEventTimer::run_after(.12f,[handle]()
    {
        if(ALKitShell* shell=handle.get())
        {
            shell->mSearchDebounce.reset();
            shell->runSearch(shell->mPendingQuery);
        }
    });
    mSearchDebounce=timer->getWeak();
}

void ALKitShell::updateSearchResults(const ALKitSearch::Hits& hits,ALKitPage* page)
{
    mSearchHits.clear();
    mHasTopSearchHit=!hits.empty();
    if(mHasTopSearchHit)mTopSearchHit=hits.front();
    S32 local_count=0;
    S32 elsewhere=0;
    const bool global=gSavedSettings.getBOOL("KitSearchGlobal");
    for(const ALKitSearch::Hit& hit:hits)
    {
        if(hit.page_key==page->pageKey())++local_count;
        else if(global)
        {
            ++elsewhere;
            if(mSearchHits.size()<12)mSearchHits.push_back(hit);
        }
    }
    mSearchSelection=0;
    if(mSearchHeader)mSearchHeader->setText(llformat("%d on this page \xC2\xB7 %d elsewhere",local_count,elsewhere));
    for(S32 index=0;index<(S32)mSearchButtons.size();++index)
    {
        const bool visible=index<(S32)mSearchHits.size();
        mSearchButtons[index]->setVisible(visible);
        if(visible)
        {
            std::string label=mSearchHits[index].path;
            if(mSearchHits[index].advanced)label+="  More";
            mSearchButtons[index]->setLabel(label);
        }
    }
    if(mSearchFooter)mSearchFooter->setText(llformat("Showing the best %d of %d. Add a word to narrow it down.",(S32)mSearchHits.size(),elsewhere));
    if(mSearchPopover)
    {
        if(global&&!mSearchHits.empty())mSearchPopover->show();
        else mSearchPopover->hide();
    }
}

bool ALKitShell::handleSearchKey(KEY key)
{
    if(key==KEY_RETURN&&mHasTopSearchHit)
    {
        ALKitTarget target;
        target.page_key=mTopSearchHit.page_key;
        target.card_id=mTopSearchHit.card_id;
        target.widget=mTopSearchHit.widget;
        target.setting=mTopSearchHit.setting;
        target.option=mTopSearchHit.option;
        navigateTo(target);
        return true;
    }
    if(mSearchHits.empty())return false;
    if(key==KEY_UP)mSearchSelection=(mSearchSelection+(S32)mSearchHits.size()-1)%(S32)mSearchHits.size();
    else if(key==KEY_DOWN)mSearchSelection=(mSearchSelection+1)%(S32)mSearchHits.size();
    else return false;
    for(S32 index=0;index<(S32)mSearchButtons.size();++index)mSearchButtons[index]->setToggleState(index==mSearchSelection);
    return true;
}

void ALKitShell::activateSearchResult()
{
    if(mSearchSelection<0||mSearchSelection>=(S32)mSearchHits.size())return;
    const ALKitSearch::Hit& hit=mSearchHits[mSearchSelection];
    ALKitTarget target;
    target.page_key=hit.page_key;
    target.card_id=hit.card_id;
    target.widget=hit.widget;
    target.setting=hit.setting;
    target.option=hit.option;
    navigateTo(target);
}

bool ALKitShell::cycleChip(S32 delta)
{
    ALKitPage* page=currentKitPage();
    return page&&page->cycleChip(delta);
}

bool ALKitShell::cyclePage(S32 delta)
{
    if(!mDirectorTabs)return false;
    LLPanel* group=mDirectorTabs->getCurrentPanel();
    LLTabContainer* tabs=findFirst<LLTabContainer>(group);
    if(!tabs||tabs->getTabCount()<1)return false;
    const S32 current=tabs->getCurrentPanelIndex();
    LLPanel* next=tabs->getPanelByIndex((current+delta+tabs->getTabCount())%tabs->getTabCount());
    return next&&selectPage(next->getName());
}

bool ALKitShell::cycleGroup(S32 delta)
{
    if(!mDirectorTabs||mDirectorTabs->getTabCount()<1)return false;
    const S32 current=mDirectorTabs->getCurrentPanelIndex();
    LLPanel* group=mDirectorTabs->getPanelByIndex((current+delta+mDirectorTabs->getTabCount())%mDirectorTabs->getTabCount());
    if(!group)return false;
    mDirectorTabs->selectTabPanel(group);
    LLTabContainer* tabs=findFirst<LLTabContainer>(group);
    LLPanel* page=tabs?tabs->getCurrentPanel():nullptr;
    return page&&selectPage(page->getName());
}

void ALKitShell::startSearchLoad()
{
    if(mSearchStarted)return;
    mSearchStarted=true;
    LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
    doOnIdleOneTime([handle]()
    {
        if(ALKitShell* shell=handle.get())if(shell->mSearch.load(shell->mIndexFile))shell->scheduleSearchBatch();
    });
}

void ALKitShell::scheduleSearchBatch()
{
    LLHandle<ALKitShell> handle=getDerivedHandle<ALKitShell>();
    doOnIdleOneTime([handle]()
    {
        if(ALKitShell* shell=handle.get())if(shell->mSearch.tokenizeBatch())shell->scheduleSearchBatch();
    });
}

void ALKitShell::clearSearch()
{
    if(auto timer=mSearchDebounce.lock())delete timer.get();
    mSearchDebounce.reset();
    mSearch.setQuery("");
    if(ALKitPage* page=currentKitPage())
    {
        page->setSearchText("");
        page->clearSearch();
    }
    mSearchHits.clear();
    mHasTopSearchHit=false;
    if(mSearchPopover)mSearchPopover->hide();
}

bool ALKitShell::closePopover()
{
    if(!mOpenPopover||!mOpenPopover->getVisible())return false;
    mOpenPopover->hide();
    mOpenPopover=nullptr;
    return true;
}

void ALKitShell::setOpenPopover(ALKitPopover* popover)
{
    mOpenPopover=popover;
}

bool ALKitShell::handleMouseDown(S32 x,S32 y,MASK mask)
{
    if(mOpenPopover&&mOpenPopover->getVisible())
    {
        S32 local_x=0;
        S32 local_y=0;
        localPointToOtherView(x,y,&local_x,&local_y,mOpenPopover);
        if(!mOpenPopover->pointInView(local_x,local_y))closePopover();
    }
    return LLPanel::handleMouseDown(x,y,mask);
}

bool ALKitShell::handleScrollWheel(S32 x,S32 y,LLScrollDelta delta)
{
    if(mTabStrip&&mTabNext&&mTabNext->getVisible())
    {
        S32 local_x=0;
        S32 local_y=0;
        localPointToOtherView(x,y,&local_x,&local_y,mTabStrip);
        if(mTabStrip->pointInView(local_x,local_y)&&delta.mClicks!=0)
        {
            mTabOffset=llmax(0,mTabOffset+(delta.mClicks>0?1:-1));
            layoutChrome();
            LLSliderWheel::noteScroll(this);
            return true;
        }
    }
    return LLPanel::handleScrollWheel(x,y,delta);
}
