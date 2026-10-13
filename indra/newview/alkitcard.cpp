#include "llviewerprecompiledheaders.h"
#include "alkitcard.h"
#include "alkitbinding.h"
#include "alkitmenu.h"
#include "alkitrow.h"
#include "alkitpage.h"
#include "alkitstyle.h"
#include "aluiperfselftest.h"
#include "alkitswitch.h"
#include "alkitshell.h"
#include "allightrigmap.h"
#include "aluirefresh.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "llfontgl.h"
#include "llmenugl.h"
#include "lltextbox.h"
#include "llfloaterreg.h"
#include "alfloaterlightbox.h"
#include "llrender2dutils.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llui.h"
#include "llviewermenu.h"
#include "llnotificationsutil.h"
#include "llprismlens.h"
#include "alkitqol.h"
#include <algorithm>
static LLDefaultChildRegistry::Register<ALKitCard>   r("kit_card");
static LLDefaultChildRegistry::Register<ALKitOption> ro("kit_option");

namespace
{
struct CardClipboard
{
    std::string type;
    std::string source;
    LLSD values;
};
CardClipboard sCardClipboard;

bool rowBranchVisible(const ALKitRow* row, const ALKitCard* card)
{
    for (const LLView* p = row->getParent(); p && p != card; p = p->getParent())
        if (!p->getVisible())
            return false;
    return true;
}
LLSD typedOptionValue(LLControlVariable* control, const LLSD& value)
{
    if (!control)
        return value;
    switch (control->type())
    {
        case TYPE_BOOLEAN:
            return LLSD(value.asBoolean());
        case TYPE_S32:
            return LLSD(value.asInteger());
        case TYPE_U32:
            return LLSD(value.asInteger());
        case TYPE_F32:
            return LLSD(value.asReal());
        case TYPE_STRING:
            return LLSD(value.asString());
        default:
            return value;
    }
}
} // namespace

ALKitOption::Params::Params() : label("label"), value("value"), enable_widget("enable_widget", ""), enable_setting("enable_setting", "")
{
}

ALKitCard::Summary::Summary() : setting("setting"), label("label"), kind("kind", "number"), unit("unit", ""), decimals("decimals", 3)
{
}

ALKitCard::Params::Params() :
    id("id"),
    title("title", ""),
    kind("kind", "rows"),
    badge("badge", ""),
    help("help", ""),
    enable_widget("enable_widget", ""),
    enable_setting("enable_setting", ""),
    enable_tooltip("enable_tooltip", ""),
    sel_label("sel_label", ""),
    sel_setting("sel_setting", ""),
    link_to("link_to", ""),
    link_label("link_label", ""),
    target("target", ""),
    target_tab("target_tab", ""),
    button_label("button_label", "Open"),
    copy_type("copy_type", ""),
    advanced("advanced", false),
    summary("summary")
{
}

ALKitCard::ALKitCard(const Params& p) :
    LLPanel(p),
    mId(p.id),
    mTitle(p.title),
    mKind(p.kind),
    mBadge(p.badge),
    mBadgeTemplate(p.badge),
    mEnableWidget(p.enable_widget),
    mEnableSetting(p.enable_setting),
    mEnableTooltip(p.enable_tooltip),
    mSelectorLabel(p.sel_label),
    mSelectorSetting(p.sel_setting),
    mLinkTo(p.link_to),
    mLinkLabel(p.link_label),
    mTarget(p.target),
    mTargetTab(p.target_tab),
    mButtonLabel(p.button_label),
    mHelp(p.help),
    mCopyType(p.copy_type),
    mAdvanced(p.advanced),
    mOuterImage(LLUI::getUIImage("Kit_RoundRect6")),
    mInnerImage(LLUI::getUIImage("Kit_RoundRect4"))
{
    ALKitStyle::color(ALKitStyle::WINDOW_BG);
    setName("kc_" + mId);
    if (mBadge.empty())
        mBadge = mAdvanced ? "Advanced" : mKind == "link" ? "Link" : mKind == "launcher" ? "Window" : "";
    for (const Summary& s : p.summary)
    {
        SummaryData data;
        data.setting  = s.setting;
        data.label    = s.label;
        data.kind     = s.kind;
        data.unit     = s.unit;
        data.decimals = s.decimals;
        mSummaries.push_back(data);
    }
}

ALKitCard::~ALKitCard()
{
    if (LLContextMenu* menu = mResetMenuHandle.get())
    {
        menu->die();
        mResetMenuHandle.markDead();
    }
    if (LLContextMenu* menu = mCardMenuHandle.get())
    {
        menu->die();
        mCardMenuHandle.markDead();
    }
}

bool ALKitCard::postBuild()
{
    if (!LLPanel::postBuild())
        return false;

    std::function<void(LLView*)> walk = [&](LLView* v)
    {
        std::vector<LLView*> children(v->getChildList()->begin(), v->getChildList()->end());
        std::reverse(children.begin(), children.end());
        for (LLView* c : children)
        {
            if (ALKitOption* o = dynamic_cast<ALKitOption*>(c))
            {
                mOptions.push_back(o);
                walk(o);
            }
            else if (ALKitRow* row = dynamic_cast<ALKitRow*>(c))
                mRows.push_back(row);
            else
                walk(c);
        }
    };
    walk(this);
    if (LLControlVariable* selector = gSavedSettings.getControl(mSelectorSetting).get())
    {
        for (ALKitOption* option : mOptions)
        {
            option->setOptionValue(typedOptionValue(selector, option->optionValue()));
        }
    }

    mRigMap = findChild<ALLightRigMap>("light_rig_map");
    if (!mRigMap)
        for (LLView* c : *getChildList())
            if ((mRigMap = dynamic_cast<ALLightRigMap*>(c)))
                break;

    if (mRigMap)
    {
        static const char* labels[] = { "Top", "Side", "2.5D" };
        for (S32 i = 0; i < 3; ++i)
        {
            LLButton::Params vp;
            vp.name     = "kc_" + mId + "_view_" + llformat("%d", i);
            vp.rect     = LLRect(0, 22, 44, 0);
            vp.label    = labels[i];
            LLButton* b = LLUICtrlFactory::create<LLButton>(vp);
            b->setCommitCallback(
                [this, i](LLUICtrl*, const LLSD&)
                {
                    mRigMap->setViewMode(i);
                    requestLayout();
                });
            addChild(b);
            mViewSegments.push_back(b);
        }
    }

    std::string first_sub;
    for (ALKitRow* row : mRows)
        if (!row->meta().sub.empty())
        {
            first_sub = row->meta().sub;
            break;
        }
    std::string last_sub;
    for (ALKitRow* row : mRows)
    {
        if (row->cardPreset() && row->control())
        {
            row->control()->setToolTip("Card preset \xC2\xB7 " + row->meta().label);
            addChild(row->control());
            if (row->meta().sub.empty() || row->meta().sub == first_sub)
                mHeaderPresets.push_back(row->control());
            else
                mSubPresets.emplace_back(row, row->control());
            row->setVisible(false);
        }
        if (mKind == "combined" && !row->meta().sub.empty() && row->meta().sub != mTitle && row->meta().sub != last_sub)
        {
            last_sub = row->meta().sub;
            LLTextBox::Params hp;
            hp.name          = "kc_" + mId + "_sub_" + llformat("%d", (S32)mHeadings.size());
            hp.rect          = LLRect(0, 20, 100, 0);
            hp.initial_value = last_sub;
            hp.text_color    = ALKitStyle::color(ALKitStyle::MUTED);
            LLTextBox* t     = LLUICtrlFactory::create<LLTextBox>(hp);
            addChild(t);
            mHeadings.emplace_back(row, t);
        }
    }

    for (S32 i = 0; i < (S32)mSummaries.size(); ++i)
    {
        SummaryData&      sum = mSummaries[i];
        LLTextBox::Params tp;
        tp.name          = "kc_" + mId + "_summary_label_" + llformat("%d", i);
        tp.rect          = LLRect(0, 22, 120, 0);
        tp.initial_value = sum.label;
        tp.text_color    = ALKitStyle::color(ALKitStyle::LABEL);
        sum.labelView    = LLUICtrlFactory::create<LLTextBox>(tp);
        addChild(sum.labelView);
        tp.name          = "kc_" + mId + "_summary_value_" + llformat("%d", i);
        tp.initial_value = "";
        tp.font          = ALKitStyle::monoFont();
        tp.text_color    = ALKitStyle::color(ALKitStyle::TEXT);
        sum.valueView    = LLUICtrlFactory::create<LLTextBox>(tp);
        addChild(sum.valueView);
        refreshSummary(i);
        if (LLControlVariable* c = findControl(sum.setting))
        {
            mAmberConnections.emplace_back(
                c->getSignal()->connect([this, i](LLControlVariable*, const LLSD&, const LLSD&) { refreshSummary(i); }));
        }
    }

    if (mKind == "link" || mKind == "launcher" || mKind == "quick")
    {
        LLTextBox::Params tp;
        tp.name          = "kc_" + mId + "_body_text";
        tp.rect          = LLRect(0, 22, 200, 0);
        tp.initial_value = mKind == "link" ? "Lives on " + mLinkLabel :
                           mKind == "quick" ? "Star a row or use its right-click menu to add live controls here." : mHelp;
        tp.text_color    = ALKitStyle::color(ALKitStyle::MUTED);
        mBodyText        = LLUICtrlFactory::create<LLTextBox>(tp);
        addChild(mBodyText);
    }

    mEnable = findChild<ALKitSwitch>(mEnableWidget);
    if (!mEnableWidget.empty() && !mEnable)
    {
        ALKitSwitch::Params ep;
        ep.name = mEnableWidget;
        ep.rect = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
        mEnable = LLUICtrlFactory::create<ALKitSwitch>(ep);
        if (!mEnableSetting.empty())
            mEnable->setControlName(mEnableSetting, this);
        mEnable->setToolTip(mEnableTooltip);
        addChild(mEnable);
    }

    mOptionMore.assign(mOptions.size(), false);
    for (ALKitOption* option : mOptions)
    {
        ALKitSwitch* sw = findChild<ALKitSwitch>(option->enableWidget());
        if (!option->enableWidget().empty() && !sw)
        {
            ALKitSwitch::Params ep;
            ep.name = option->enableWidget();
            ep.rect = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
            sw      = LLUICtrlFactory::create<ALKitSwitch>(ep);
            if (!option->enableSetting().empty())
                sw->setControlName(option->enableSetting(), this);
            addChild(sw);
        }
        mOptionSwitches.push_back(sw);
    }

    LLButton::Params bp;
    bp.name  = "kc_" + mId + "_reset";
    bp.rect  = LLRect(0, 24, 24, 0);
    bp.label = "";
    mReset   = LLUICtrlFactory::create<LLButton>(bp);
    mReset->setImageOverlay("KitIcon_reset_arc_16");
    mReset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            std::string page;
            for (LLView* v = getParent(); v; v = v->getParent())
                if (ALKitPage* p = dynamic_cast<ALKitPage*>(v))
                {
                    page = p->pageKey();
                    break;
                }
            if (ALKitShell* s = ALKitShell::find(this))
                s->history().beginGesture(ALKitHistory::MOUSE, page, "Reset " + mTitle);
            for (ALKitRow* row : mRows)
                if (rowBranchVisible(row, this) && !row->cardPreset() && !row->whoPicker() && (row->getVisible() || row->meta().advanced))
                    row->reset();
        });
    addChild(mReset);

    if (!mCopyType.empty())
    {
        bp.name = "kc_" + mId + "_menu";
        bp.label = "...";
        mMenuButton = LLUICtrlFactory::create<LLButton>(bp);
        mMenuButton->setToolTip(std::string("Copy / paste this card's settings"));
        addChild(mMenuButton);
        LLContextMenu::Params menu_params;
        menu_params.name = "kc_" + mId + "_copy_menu";
        menu_params.visible = false;
        LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(menu_params);
        mCardMenuHandle = menu->getHandle();
        LLMenuItemCallGL::Params item;
        item.name = "copy_settings";
        item.label = "Copy settings";
        item.on_click.function = [this](LLUICtrl*, const LLSD&) { copySettings(); };
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
        item.name = "paste_settings";
        item.label = "Paste settings";
        item.on_click.function = [this](LLUICtrl*, const LLSD&) { pasteSettings(); };
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
        gMenuHolder->addChild(menu);
        mMenuButton->setCommitCallback(
            [this](LLUICtrl* control, const LLSD&)
            {
                if (LLContextMenu* popup = mCardMenuHandle.get())
                {
                    if (LLMenuItemGL* paste = popup->findChild<LLMenuItemGL>("paste_settings"))
                        paste->setEnabled(sCardClipboard.type == mCopyType && sCardClipboard.values.isArray());
                    popup->buildDrawLabels();
                    ALKitMenu::showContextMenu(control, popup, 0, 0);
                }
            });
    }

    if (mKind == "selector")
    {
        LLContextMenu::Params mp;
        mp.name                   = "kc_" + mId + "_reset_menu";
        mp.visible                = false;
        LLContextMenu* reset_menu = LLUICtrlFactory::create<LLContextMenu>(mp);
        mResetMenuHandle          = reset_menu->getHandle();
        LLMenuItemCallGL::Params ip;
        ip.name              = "reset_all";
        ip.label             = "Reset all " + mSelectorLabel + "s";
        ip.on_click.function = [this](LLUICtrl*, const LLSD&)
        {
            std::string page;
            for (LLView* v = getParent(); v; v = v->getParent())
                if (ALKitPage* p = dynamic_cast<ALKitPage*>(v))
                {
                    page = p->pageKey();
                    break;
                }
            if (ALKitShell* s = ALKitShell::find(this))
                s->history().beginGesture(ALKitHistory::MOUSE, page, "Reset all " + mSelectorLabel + "s");
            for (ALKitRow* row : mRows)
                if (!row->cardPreset() && !row->whoPicker() && row->resettable())
                    row->reset();
        };
        reset_menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(ip));
        gMenuHolder->addChild(reset_menu);
        mReset->setRightMouseDownCallback(
            [this](LLUICtrl* c, S32 x, S32 y, MASK)
            {
                if (LLContextMenu* menu = mResetMenuHandle.get())
                    ALKitMenu::showContextMenu(c, menu, x, y);
            });
    }

    for (ALKitRow* row : mRows)
    {
        if (row->amber())
            ++mAmberCount;
        mAmberConnections.emplace_back(row->setAmberCallback(
            [this](bool on)
            {
                mAmberCount += on ? 1 : -1;
                updateResetColor();
            }));
    }
    updateResetColor();

    bool has_advanced = false;
    for (ALKitRow* row : mRows)
        if (row->meta().advanced)
        {
            has_advanced = true;
            ++mMoreCount;
        }

    if (has_advanced)
    {
        bp.name  = "kc_" + mId + "_more";
        bp.label = "More options (" + llformat("%d", mMoreCount) + ")";
        mMore    = LLUICtrlFactory::create<LLButton>(bp);
        mMore->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                if (mOptionMore.empty())
                    mMoreOpen = !mMoreOpen;
                else
                    mOptionMore[mActiveOption] = !mOptionMore[mActiveOption];
                mMore->setLabel((moreOpen() ? "Fewer options (" : "More options (") + llformat("%d", mMoreCount) + ")");
                requestLayout();
            });
        addChild(mMore);
    }

    if (mAdvanced)
    {
        bp.name  = "kc_" + mId + "_open";
        bp.label = "Hide";
        mOpen    = LLUICtrlFactory::create<LLButton>(bp);
        mOpen->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                mBodyOpen = !mBodyOpen;
                mOpen->setLabel(mBodyOpen ? "Hide" : "Show");
                requestLayout();
            });
        addChild(mOpen);
    }

    if (mKind == "link" || mKind == "launcher")
    {
        bp.name  = "kc_" + mId + (mKind == "link" ? "_go" : "_launch");
        bp.label = mKind == "link" ? "Open \xE2\x80\xBA" : mButtonLabel;
        mAction  = LLUICtrlFactory::create<LLButton>(bp);
        mAction->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                if (mKind == "link")
                {
                    if (ALKitShell* s = ALKitShell::find(this))
                        s->navigateTo(mLinkTo);
                }
                else if (mTarget == "lightbox")
                    ALFloaterLightBox::showTab(mTargetTab);
                else
                    LLFloaterReg::showInstance(mTarget);
            });
        addChild(mAction);
    }

    if (mKind == "selector" && mOptions.size() <= 6)
    {
        for (S32 i = 0; i < (S32)mOptions.size(); ++i)
        {
            bp.name     = "kc_" + mId + "_seg_" + llformat("%d", i);
            bp.label    = mOptions[i]->optionLabel();
            LLButton* b = LLUICtrlFactory::create<LLButton>(bp);
            b->setCommitCallback(
                [this, i](LLUICtrl*, const LLSD&)
                {
                    if (!mSelectorSetting.empty())
                        alKitSetTypedControl(gSavedSettings, mSelectorSetting, mOptions[i]->optionValue());
                    else
                    {
                        mActiveOption = i;
                        syncSelector();
                        requestLayout();
                    }
                });
            addChild(b);
            mSegments.push_back(b);
        }
    }

    else if (mKind == "selector")
    {
        LLComboBox::Params cp;
        cp.name        = "kc_" + mId + "_selector";
        cp.rect        = LLRect(0, 24, 220, 0);
        mSelectorCombo = LLUICtrlFactory::create<LLComboBox>(cp);
        for (ALKitOption* o : mOptions)
            mSelectorCombo->add(o->optionLabel(), o->optionValue());
        mSelectorCombo->setCommitCallback(
            [this](LLUICtrl* c, const LLSD&)
            {
                if (!mSelectorSetting.empty())
                    alKitSetTypedControl(gSavedSettings, mSelectorSetting, c->getValue());
                else
                {
                    for (S32 i = 0; i < (S32)mOptions.size(); ++i)
                        if (mOptions[i]->optionValue() == c->getValue())
                            mActiveOption = i;
                    syncSelector();
                    requestLayout();
                }
            });
        addChild(mSelectorCombo);
    }

    if (!mSelectorSetting.empty())
        if (LLControlVariable* c = gSavedSettings.getControl(mSelectorSetting).get())
            mSelectorConnection = c->getSignal()->connect(
                [this](LLControlVariable*, const LLSD&, const LLSD&)
                {
                    syncSelector();
                    requestLayout();
                });

    syncSelector();
    mVisibility.resize(mRows.size());
    reshape(getRect().getWidth(), heightForWidth(getRect().getWidth()), true);
    return true;
}

void ALKitCard::updateResetColor()
{
    if (mReset)
        mReset->setImageOverlay("KitIcon_reset_arc_16", LLFontGL::HCENTER,
                                (mAmberCount > 0 ? ALKitStyle::color(ALKitStyle::ACCENT) : ALKitStyle::color(ALKitStyle::FAINT)).get());
}

std::string ALKitCard::copySourceName() const
{
    if (mActiveOption >= 0 && mActiveOption < (S32)mOptions.size())
        return mOptions[mActiveOption]->optionLabel();
    return mBadge.empty() ? mTitle : mBadge;
}

void ALKitCard::copySettings()
{
    LLSD values = LLSD::emptyArray();
    for (ALKitRow* row : mRows)
    {
        if (!row || !rowBranchVisible(row, this) || !row->control() ||
            row->meta().kind == "readout" || row->meta().kind == "button")
            continue;
        LLSD entry = LLSD::emptyMap();
        entry["key"] = mCopyType == "light_role" ? row->meta().label : row->meta().widget;
        entry["value"] = row->boundValue();
        values.append(entry);
    }
    sCardClipboard = { mCopyType, copySourceName(), values };
}

void ALKitCard::pasteSettings()
{
    if (sCardClipboard.type != mCopyType || !sCardClipboard.values.isArray())
        return;
    std::string page;
    for (LLView* view = getParent(); view; view = view->getParent())
        if (ALKitPage* owner = dynamic_cast<ALKitPage*>(view))
        {
            page = owner->pageKey();
            break;
        }
    const bool vcam = mCopyType.compare(0, 5, "vcam_") == 0;
    if (!vcam)
    {
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->history().beginGesture(ALKitHistory::MOUSE, page, "Paste " + mTitle);
    }
    if (vcam)
        for (LLView* view = getParent(); view; view = view->getParent())
            if (ALKitPage* owner = dynamic_cast<ALKitPage*>(view))
            {
                if (!owner->pasteCardValues(mId, sCardClipboard.values))
                    return;
                LLNotificationsUtil::add("SystemMessageTip",
                    LLSD().with("MESSAGE", "Pasted from " + sCardClipboard.source));
                return;
            }
    bool pasted = false;
    for (ALKitRow* row : mRows)
    {
        if (!row || !rowBranchVisible(row, this) || !row->control())
            continue;
        for (LLSD::array_const_iterator it = sCardClipboard.values.beginArray(); it != sCardClipboard.values.endArray(); ++it)
            if ((*it)["key"].asString() == (mCopyType == "light_role" ? row->meta().label : row->meta().widget) &&
                ALKitQol::pasteTypesCompatible((*it)["value"], row->boundValue()))
            {
                pasted = row->setBoundValue((*it)["value"]) || pasted;
                break;
            }
    }
    if (!pasted)
        return;
    LLNotificationsUtil::add("SystemMessageTip",
        LLSD().with("MESSAGE", "Pasted from " + sCardClipboard.source));
}

void ALKitCard::refreshSummary(S32 index)
{
    if (index < 0 || index >= (S32)mSummaries.size())
        return;
    SummaryData& sum = mSummaries[index];
    if (!sum.valueView)
        return;
    LLControlVariable* c = findControl(sum.setting);
    if (!c)
    {
        sum.valueView->setValue("-");
        return;
    }
    const LLSD  value = c->getValue();
    std::string text;
    if (sum.kind == "bool")
        text = value.asBoolean() ? "On" : "Off";
    else if (sum.kind == "number")
        text = llformat("%.*f", sum.decimals, value.asReal());
    else
        text = value.asString();
    if (!sum.unit.empty())
        text += " " + sum.unit;
    sum.valueView->setValue(text);
}

void ALKitCard::syncSelector()
{
    if (mOptions.empty())
        return;
    const LLSD value =
        gSavedSettings.controlExists(mSelectorSetting) ? gSavedSettings.getLLSD(mSelectorSetting) : mOptions[mActiveOption]->optionValue();
    S32 matched = -1;
    for (S32 i = 0; i < (S32)mOptions.size(); ++i)
        if (mOptions[i]->optionValue() == value)
        {
            matched = i;
            break;
        }
    // A stale/corrupt selector value must not make every option disappear.
    // Keep the last valid option (the first option during initial build).
    if (matched < 0)
        matched = llclamp(mActiveOption, 0, (S32)mOptions.size() - 1);
    mActiveOption = matched;
    for (S32 i = 0; i < (S32)mOptions.size(); ++i)
    {
        const bool active = i == matched;
        mOptions[i]->setVisible(active);
        if (i < (S32)mOptionSwitches.size() && mOptionSwitches[i])
            mOptionSwitches[i]->setVisible(active);
        if (i < (S32)mSegments.size())
        {
            mSegments[i]->setToggleState(active);
            ALKitStyle::styleButton(mSegments[i], false);
            mSegments[i]->setImageColor(active ? ALKitStyle::color(ALKitStyle::ACTIVE_FILL)
                                                : ALKitStyle::color(ALKitStyle::INPUT_BG));
            mSegments[i]->setSelectedImageColor(ALKitStyle::color(ALKitStyle::ACTIVE_FILL));
            mSegments[i]->setUnselectedLabelColor(active ? ALKitStyle::color(ALKitStyle::TEXT_STRONG)
                                                         : ALKitStyle::color(ALKitStyle::MUTED));
        }
    }
    if (mActiveOption < (S32)mOptionSwitches.size() && mOptionSwitches[mActiveOption])
        mEnable = mOptionSwitches[mActiveOption];
    if (mSelectorCombo)
        mSelectorCombo->setValue(mOptions[matched]->optionValue());
    if (mMore)
        mMore->setLabel((moreOpen() ? "Fewer options (" : "More options (") + llformat("%d", mMoreCount) + ")");
}

S32 ALKitCard::heightForWidth(S32 w) const
{
    S32 h = ALKitStyle::CARD_HDR_H;
    if (!mBodyOpen)
        return h;
    h += ALKitStyle::CARD_PAD_T + ALKitStyle::CARD_PAD_B;
    if (mKind == "selector")
        h += ALKitStyle::SELECTOR_BAND_H;
    if (mKind == "link")
        h += (S32)mSummaries.size() * ALKitStyle::ROW_H + ALKitStyle::ROW_H;
    if (mKind == "launcher")
        h += ALKitStyle::ROW_H;
    if (mKind == "quick" && mRows.empty())
        h += 2 * ALKitStyle::ROW_H;
    if (mAction)
        h += ALKitStyle::MORE_H;
    if (mRigMap)
        h += mRigMap->heightForWidth(w - ALKitStyle::CARD_PAD_L - ALKitStyle::CARD_PAD_R);
    S32         advanced = 0;
    std::string sub;
    for (S32 i = 0; i < (S32)mRows.size(); ++i)
    {
        ALKitRow* row = mRows[i];
        if (row->cardPreset())
            continue;
        const bool visible = rowBranchVisible(row, this) && row->isEffectivelyVisible();
        if (!visible)
        {
            if (rowBranchVisible(row, this) && row->meta().advanced)
                ++advanced;
            continue;
        }
        if (mKind == "combined" && !row->meta().sub.empty() && row->meta().sub != sub)
        {
            h += sub.empty() ? ALKitStyle::SUB_FIRST_H : ALKitStyle::SUB_H;
            sub = row->meta().sub;
        }
        const bool aim = row->meta().kind == "dial" || row->meta().kind == "arc";
        if (aim && i + 1 < (S32)mRows.size())
        {
            ALKitRow*  next = mRows[i + 1];
            const bool pair = !next->cardPreset() && rowBranchVisible(next, this) && next->isEffectivelyVisible() &&
                              (next->meta().kind == "dial" || next->meta().kind == "arc") && next->meta().sub == row->meta().sub;
            if (pair)
            {
                h += ALKitStyle::DIAL_CELL_H;
                ++i;
                continue;
            }
        }
        h += row->heightForWidth(w - ALKitStyle::CARD_PAD_L - ALKitStyle::CARD_PAD_R);
    }
    if (mMore || advanced)
        h += ALKitStyle::MORE_H;
    return h;
}

void ALKitCard::layoutBody(S32 w)
{
    for (ALKitOption* option : mOptions)
    {
        option->reshape(w, getRect().getHeight(), true);
        option->setOrigin(0, 0);
    }

    const S32 body_w   = w - ALKitStyle::CARD_PAD_L - ALKitStyle::CARD_PAD_R;
    S32       header_x = w - ALKitStyle::CARD_PAD_R - 24;

    if (mReset)
    {
        mReset->reshape(24, 24, true);
        mReset->setOrigin(header_x, getRect().getHeight() - 30);
        header_x -= 24 + ALKitStyle::COL_GAP;
    }
    if (mMenuButton)
    {
        mMenuButton->reshape(28, 22, true);
        mMenuButton->setOrigin(header_x - 28, getRect().getHeight() - 29);
        header_x -= 28 + ALKitStyle::COL_GAP;
    }
    for (ALKitSwitch* sw : mOptionSwitches)
        if (sw)
            sw->setVisible(sw == mEnable);
    if (mEnable)
    {
        mEnable->setOrigin(header_x - ALKitStyle::SWITCH_W, getRect().getHeight() - 27);
        header_x -= ALKitStyle::SWITCH_W + ALKitStyle::COL_GAP;
    }
    if (mOpen)
    {
        mOpen->reshape(48, 22, true);
        mOpen->setOrigin(header_x - 48, getRect().getHeight() - 29);
        header_x -= 48 + ALKitStyle::COL_GAP;
    }
    for (auto it = mHeaderPresets.rbegin(); it != mHeaderPresets.rend(); ++it)
    {
        (*it)->reshape(ALKitStyle::CARD_PRESET_W, 22, true);
        (*it)->setOrigin(header_x - ALKitStyle::CARD_PRESET_W, getRect().getHeight() - 29);
        header_x -= ALKitStyle::CARD_PRESET_W + ALKitStyle::COL_GAP;
        (*it)->setVisible(true);
    }

    for (ALKitRow* row : mRows)
        if (!row->cardPreset())
            row->setVisible(false);
    for (auto& heading : mHeadings)
        heading.second->setVisible(false);
    for (auto& p : mSubPresets)
        p.second->setVisible(false);
    for (SummaryData& sum : mSummaries)
    {
        sum.labelView->setVisible(mBodyOpen);
        sum.valueView->setVisible(mBodyOpen);
    }
    if (mBodyText)
        mBodyText->setVisible(mBodyOpen && (mKind != "quick" || mRows.empty()));
    if (mAction)
        mAction->setVisible(mBodyOpen);
    if (mRigMap)
        mRigMap->setVisible(mBodyOpen);
    for (LLButton* b : mViewSegments)
        b->setVisible(mBodyOpen);
    for (LLButton* b : mSegments)
        b->setVisible(mBodyOpen);
    if (mSelectorCombo)
        mSelectorCombo->setVisible(mBodyOpen);
    if (mMore)
        mMore->setVisible(mBodyOpen);

    if (!mBodyOpen)
        return;
    S32 top = getRect().getHeight() - ALKitStyle::CARD_HDR_H - ALKitStyle::CARD_PAD_T;

    if (!mSegments.empty())
    {
        const S32 sw = llmax(1, (w - 2 * ALKitStyle::CARD_PAD_L) / (S32)mSegments.size());
        for (S32 i = 0; i < (S32)mSegments.size(); ++i)
        {
            mSegments[i]->reshape(sw, 24, true);
            mSegments[i]->setOrigin(ALKitStyle::CARD_PAD_L + i * sw, top - 28);
        }
        top -= ALKitStyle::SELECTOR_BAND_H;
    }
    else if (mSelectorCombo)
    {
        mSelectorCombo->reshape(llmin(220, w - 2 * ALKitStyle::CARD_PAD_L), 24, true);
        mSelectorCombo->setOrigin(ALKitStyle::CARD_PAD_L, top - 28);
        top -= ALKitStyle::SELECTOR_BAND_H;
    }
    else if (mKind == "selector")
        top -= ALKitStyle::SELECTOR_BAND_H;

    if (mRigMap)
    {
        const S32 mh = mRigMap->heightForWidth(body_w);
        mRigMap->reshape(body_w, mh, true);
        mRigMap->setOrigin(ALKitStyle::CARD_PAD_L, top - mh);
        const S32 map_bottom = top - mh;
        top -= mh;
        S32 view_x = ALKitStyle::CARD_PAD_L + 36;
        for (S32 i = 0; i < (S32)mViewSegments.size(); ++i)
        {
            LLButton* segment = mViewSegments[i];
            segment->reshape(42, 22, true);
            segment->setOrigin(view_x, map_bottom + ALKitStyle::MAP_HINT_H + 3);
            segment->setToggleState(mRigMap->viewMode() == i);
            ALKitStyle::styleButton(segment, false);
            segment->setImageColor(mRigMap->viewMode() == i ? ALKitStyle::color(ALKitStyle::ACTIVE_FILL)
                                                             : ALKitStyle::color(ALKitStyle::INPUT_BG));
            segment->setSelectedImageColor(ALKitStyle::color(ALKitStyle::ACTIVE_FILL));
            view_x += 42;
        }
    }

    for (SummaryData& sum : mSummaries)
    {
        sum.labelView->reshape(ALKitStyle::LABEL_W, 22, true);
        sum.valueView->reshape(llmax(1, body_w - ALKitStyle::LABEL_W - ALKitStyle::COL_GAP), 22, true);
        sum.labelView->setOrigin(ALKitStyle::CARD_PAD_L, top - 24);
        sum.valueView->setOrigin(ALKitStyle::CARD_PAD_L + ALKitStyle::LABEL_W + ALKitStyle::COL_GAP, top - 24);
        top -= ALKitStyle::ROW_H;
    }

    if (mBodyText && mBodyText->getVisible())
    {
        mBodyText->reshape(body_w, 22, true);
        mBodyText->setOrigin(ALKitStyle::CARD_PAD_L, top - 24);
        top -= ALKitStyle::ROW_H;
    }

    std::string sub;
    for (S32 i = 0; i < (S32)mRows.size(); ++i)
    {
        ALKitRow* row = mRows[i];
        if (row->cardPreset())
            continue;
        const bool visible = rowBranchVisible(row, this) && row->isEffectivelyVisible();
        if (!visible)
            continue;
        if (mKind == "combined" && !row->meta().sub.empty() && row->meta().sub != mTitle && row->meta().sub != sub)
        {
            for (auto& heading : mHeadings)
                if (heading.first == row)
                {
                    const S32 sh = sub.empty() ? ALKitStyle::SUB_FIRST_H : ALKitStyle::SUB_H;
                    heading.second->reshape(body_w, 20, true);
                    heading.second->setOrigin(ALKitStyle::CARD_PAD_L, top - sh + 3);
                    heading.second->setVisible(true);
                    for (auto& p : mSubPresets)
                        if (p.first->meta().sub == row->meta().sub)
                        {
                            p.second->reshape(ALKitStyle::CARD_PRESET_W, 22, true);
                            p.second->setOrigin(ALKitStyle::CARD_PAD_L + body_w - ALKitStyle::CARD_PRESET_W, top - sh + 1);
                            p.second->setVisible(true);
                        }
                    top -= sh;
                    break;
                }
            sub = row->meta().sub;
        }
        row->setVisible(true);
        const bool aim = row->meta().kind == "dial" || row->meta().kind == "arc";
        if (aim && i + 1 < (S32)mRows.size())
        {
            ALKitRow*  next = mRows[i + 1];
            const bool pair = !next->cardPreset() && rowBranchVisible(next, this) && next->isEffectivelyVisible() &&
                              (next->meta().kind == "dial" || next->meta().kind == "arc") && next->meta().sub == row->meta().sub;
            if (pair)
            {
                const S32 cell = (body_w - ALKitStyle::GRID_GAP) / 2;
                row->layoutDialCell(cell);
                next->layoutDialCell(body_w - cell - ALKitStyle::GRID_GAP);
                row->setOrigin(ALKitStyle::CARD_PAD_L, top - ALKitStyle::DIAL_CELL_H);
                next->setOrigin(ALKitStyle::CARD_PAD_L + cell + ALKitStyle::GRID_GAP, top - ALKitStyle::DIAL_CELL_H);
                next->setVisible(true);
                top -= ALKitStyle::DIAL_CELL_H;
                ++i;
                continue;
            }
        }
        const S32 rh = row->heightForWidth(body_w);
        row->layoutRow(body_w);
        row->setOrigin(ALKitStyle::CARD_PAD_L, top - rh);
        top -= rh;
    }
    if (mAction)
    {
        mAction->reshape(llmin(180, body_w), 22, true);
        mAction->setOrigin(ALKitStyle::CARD_PAD_L, ALKitStyle::CARD_PAD_B + (mMore ? ALKitStyle::MORE_H : 0));
    }
    if (mMore)
    {
        mMore->reshape(140, 22, true);
        mMore->setOrigin(ALKitStyle::CARD_PAD_L, ALKitStyle::CARD_PAD_B);
    }
}

void ALKitCard::reshape(S32 w, S32 h, bool parent)
{
    LLPanel::reshape(w, h, parent);
    layoutBody(w);
}

void ALKitCard::setQuickRows(std::vector<ALKitRow*> rows)
{
    ALKitPage* owner = nullptr;
    for (LLView* view = getParent(); view && !owner; view = view->getParent())
        owner = dynamic_cast<ALKitPage*>(view);
    mQuickAmberConnections.clear();
    for (ALKitRow* row : mQuickRows)
    {
        if (row->amber())
            --mAmberCount;
        mRows.erase(std::remove(mRows.begin(), mRows.end(), row), mRows.end());
        if (owner)
            owner->unregisterDynamicRow(row);
        removeChild(row);
        delete row;
    }
    mQuickRows.clear();
    for (ALKitRow* row : rows)
    {
        if (!row)
            continue;
        addChild(row);
        mRows.push_back(row);
        mQuickRows.push_back(row);
        row->refreshFavorite();
        if (owner)
            owner->registerDynamicRow(row);
        if (row->amber())
            ++mAmberCount;
        mQuickAmberConnections.emplace_back(row->setAmberCallback(
            [this](bool on)
            {
                mAmberCount += on ? 1 : -1;
                updateResetColor();
            }));
    }
    if (mBodyText)
        mBodyText->setVisible(rows.empty());
    mVisibility.assign(mRows.size(), true);
    requestLayout();
}

void ALKitCard::requestLayout()
{
    if (mLayoutPending)
        return;
    mLayoutPending           = true;
    LLHandle<LLPanel> handle = getHandle();
    doOnIdleOneTime(
        [handle]()
        {
            if (LLPanel* p = handle.get())
            {
                ALKitCard* c      = dynamic_cast<ALKitCard*>(p);
                c->mLayoutPending = false;
                for (LLView* v = c->getParent(); v; v = v->getParent())
                    if (ALKitPage* page = dynamic_cast<ALKitPage*>(v))
                    {
                        page->requestLayout();
                        return;
                    }
                c->reshape(c->getRect().getWidth(), c->heightForWidth(c->getRect().getWidth()), true);
            }
        });
}

bool ALKitCard::setBadge(const std::string& badge)
{
    if (mBadge == badge)
        return false;
    mBadge = badge;
    requestLayout();
    return true;
}

bool ALKitCard::setBadgePlaceholder(const std::string& placeholder,
                                    const std::string& value)
{
    mBadgeSubstitutions[placeholder] = value;
    std::string badge = mBadgeTemplate;
    for (const auto& substitution : mBadgeSubstitutions)
    {
        std::string::size_type offset = 0;
        while ((offset = badge.find(substitution.first, offset)) !=
               std::string::npos)
        {
            badge.replace(offset, substitution.first.size(),
                          substitution.second);
            offset += substitution.second.size();
        }
    }
    return setBadge(badge);
}

bool ALKitCard::setCodeVisible(bool visible)
{
    if (mCodeHidden == !visible)
        return false;
    mCodeHidden = !visible;
    requestLayout();
    return true;
}

bool ALKitCard::revealRow(ALKitRow* row)
{
    if (!row || std::find(mRows.begin(), mRows.end(), row) == mRows.end())
        return false;
    mBodyOpen = true;
    for (S32 i = 0; i < (S32)mOptions.size(); ++i)
        for (LLView* p = row->getParent(); p && p != this; p = p->getParent())
            if (p == mOptions[i])
            {
                mActiveOption = i;
                if (!mSelectorSetting.empty())
                    alKitSetTypedControl(gSavedSettings, mSelectorSetting, mOptions[i]->optionValue());
                syncSelector();
                break;
            }
    if (row->meta().advanced)
    {
        if (mOptionMore.empty())
            mMoreOpen = true;
        else
            mOptionMore[mActiveOption] = true;
        if (mMore)
        mMore->setLabel("Fewer options (" + llformat("%d", mMoreCount) + ")");
    }
    requestLayout();
    return true;
}

LLSD ALKitCard::moreState() const
{
    if (mOptionMore.empty())
        return LLSD(mMoreOpen);
    LLSD state = LLSD::emptyArray();
    for (S32 i = 0; i < (S32)mOptions.size(); ++i)
    {
        LLSD option     = LLSD::emptyMap();
        option["value"] = mOptions[i]->optionValue();
        option["open"]  = mOptionMore[i];
        state.append(option);
    }
    return state;
}

void ALKitCard::restoreOpenState(bool body, const LLSD& more)
{
    mBodyOpen = body;
    if (mOptionMore.empty())
        mMoreOpen = more.asBoolean();
    else if (more.isArray())
        for (LLSD::array_const_iterator it = more.beginArray(); it != more.endArray(); ++it)
            for (S32 i = 0; i < (S32)mOptions.size(); ++i)
            {
                const LLSD& saved = *it;
                if (saved["value"] == mOptions[i]->optionValue())
                    mOptionMore[i] = saved["open"].asBoolean();
            }
    else
        std::fill(mOptionMore.begin(), mOptionMore.end(), more.asBoolean());
    if (mMore)
        mMore->setLabel((moreOpen() ? "Fewer options (" : "More options (") + llformat("%d", mMoreCount) + ")");
    requestLayout();
}

void ALKitCard::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (mOuterImage && mInnerImage)
    {
        mOuterImage->draw(0, 0, getRect().getWidth(), getRect().getHeight(), ALKitStyle::color(ALKitStyle::CARD_BORDER).get());
        mInnerImage->draw(1, 1, llmax(1, getRect().getWidth() - 2), llmax(1, getRect().getHeight() - 2),
                          ALKitStyle::color(ALKitStyle::CARD_BG).get());
    }
    else
    {
        gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::CARD_BG).get(), true);
        gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::CARD_BORDER).get(), false);
    }
    if (mBodyOpen)
    {
        gl_line_2d(0, getRect().getHeight() - ALKitStyle::CARD_HDR_H, getRect().getWidth(), getRect().getHeight() - ALKitStyle::CARD_HDR_H,
                   ALKitStyle::color(ALKitStyle::DIVIDER).get());
    }
    if (mAmberCount > 0)
    {
        gGL.color4fv(ALKitStyle::color(ALKitStyle::ACCENT).get().mV);
        gl_circle_2d((F32)ALKitStyle::CARD_PAD_L * .5f,
                     (F32)getRect().getHeight() - 8.f, 3.f, 12, true);
    }
    if (const LLFontGL* f = ALKitStyle::sansFont("Kit13", true))
    {
        const S32 title_width = llmax(20, getRect().getWidth() / 2 - ALKitStyle::CARD_PAD_L);
        f->renderUTF8(mTitle, 0, (F32)ALKitStyle::CARD_PAD_L, (F32)(getRect().getHeight() - 23),
                      ALKitStyle::color(ALKitStyle::TEXT_STRONG).get(), LLFontGL::LEFT, LLFontGL::VCENTER, LLFontGL::NORMAL,
                      LLFontGL::NO_SHADOW, S32_MAX, title_width, nullptr, true);
        const S32 title_draw_width = llmin(title_width, f->getWidth(mTitle));
        const S32 title_bottom     = getRect().getHeight() - 23 - f->getLineHeight() / 2;
        ALUIPerfSelfTest::recordKitText(this, mTitle,
                                        LLRect(ALKitStyle::CARD_PAD_L, title_bottom + f->getLineHeight(),
                                               ALKitStyle::CARD_PAD_L + title_draw_width, title_bottom),
                                        this);
        if (!mBadge.empty())
        {
            const S32 badge_x = ALKitStyle::CARD_PAD_L + llmin(title_width - 8, f->getWidth(mTitle)) + 8;
            f->renderUTF8(mBadge, 0, badge_x, getRect().getHeight() - 23,
                          ALKitStyle::color(mBadge == "NEW" ? ALKitStyle::ACCENT : ALKitStyle::MUTED).get(), LLFontGL::LEFT,
                          LLFontGL::VCENTER);
            ALUIPerfSelfTest::recordKitText(this, mBadge,
                                            LLRect(badge_x, title_bottom + f->getLineHeight(),
                                                   badge_x + f->getWidth(mBadge), title_bottom),
                                            this);
        }
    }
    if (isWidgetOnScreen(this))
    {
        bool changed = false;
        for (S32 i = 0; i < (S32)mRows.size(); ++i)
        {
            const bool v = mRows[i]->isEffectivelyVisible();
            if (v != mVisibility[i])
            {
                mVisibility[i] = v;
                changed        = true;
            }
        }
        if (changed)
            requestLayout();
    }
    for (child_list_const_reverse_iter_t it = getChildList()->rbegin(); it != getChildList()->rend(); ++it)
    {
        LLView*    child  = *it;
        const bool header = child == mReset || child == mMenuButton || child == mEnable || child == mOpen ||
                            std::find(mHeaderPresets.begin(), mHeaderPresets.end(), child) != mHeaderPresets.end() ||
                            std::find(mOptionSwitches.begin(), mOptionSwitches.end(), child) != mOptionSwitches.end() ||
                            std::find(mViewSegments.begin(), mViewSegments.end(), child) != mViewSegments.end();
        if (!header && mEnable && !mEnable->isOn())
        {
            LLViewDrawContext context(ALKitStyle::DIM_ALPHA);
            drawChild(child);
        }
        else
            drawChild(child);
    }
}
