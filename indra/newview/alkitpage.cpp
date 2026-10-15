#include "llviewerprecompiledheaders.h"
#include "alkitpage.h"
#include "alflowgrid.h"
#include "alkitcard.h"
#include "alkitbinding.h"
#include "alcinelightrigmanager.h"
#include "aldanceball.h"
#include "aldirectorcastui.h"
#include "alkitrow.h"
#include "alkitpresets.h"
#include "alkitpopover.h"
#include "alkitstyle.h"
#include "alkitshell.h"
#include "alkitswitch.h"
#include "aluiperfselftest.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "llcontrol.h"
#include "llfontgl.h"
#include "llagent.h"
#include "lllineeditor.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "lldirectorcast.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "aluirefresh.h"
#include <algorithm>

static LLDefaultChildRegistry::Register<ALKitPage> r("kit_page");

static LLDefaultChildRegistry::Register<ALKitScroller> rs("kit_scroller");

static LLDefaultChildRegistry::Register<ALKitDoc> rd("kit_doc");

static LLDefaultChildRegistry::Register<ALKitPresetHost> rp("kit_preset_host");

namespace
{

std::vector<std::string> tokens(const std::string& value, const std::string& sep = ",")
{
    std::vector<std::string> out = LLStringUtil::getTokens(value, sep);

    for (std::string& s : out)
        LLStringUtil::trim(s);

    return out;
}

LLPanel* makePanel(const std::string& name, S32 height, S32 width = 1)
{
    LLPanel::Params p;
    p.name               = name;
    p.rect               = LLRect(0, height, llmax(1, width), 0);
    p.background_visible = false;

    return LLUICtrlFactory::create<LLPanel>(p);
}

LLButton* makeButton(const std::string& name, const std::string& label)
{
    LLButton::Params p;
    p.name  = name;
    p.rect  = LLRect(0, 24, 80, 0);
    p.label = label;

    return LLUICtrlFactory::create<LLButton>(p);
}

void stabilizeComboEditors(LLView* root)
{
    if (!root)
        return;
    if (LLComboBox* combo = dynamic_cast<LLComboBox*>(root))
        for (LLView* child : *combo->getChildList())
            if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
            {
                editor->setSelectAllonCommit(false);
                editor->setFocus(false);
                editor->deselect();
                LLHandle<LLView> editor_handle = editor->getHandle();
                editor->setFocusLostCallback(
                    [editor_handle](LLFocusableElement*)
                    {
                        doOnIdleOneTime(
                            [editor_handle]()
                            {
                                if (LLLineEditor* resting = dynamic_cast<LLLineEditor*>(editor_handle.get());
                                    resting && !resting->hasFocus())
                                    resting->deselect();
                            });
                    });
                doOnIdleOneTime(
                    [editor_handle]()
                    {
                        if (LLLineEditor* resting = dynamic_cast<LLLineEditor*>(editor_handle.get());
                            resting && !resting->hasFocus())
                            resting->deselect();
                    });
            }
    for (LLView* child : *root->getChildList())
        stabilizeComboEditors(child);
}

std::string whoChipSubject(const LLComboBox* combo)
{
    std::string subject = combo ? combo->getSelectedItemLabel() : std::string();
    if (subject.empty())
        return "You";

    const std::string separator = " \xE2\x80\x93 ";
    const size_t      split     = subject.find(separator);
    if (split != std::string::npos)
        subject = subject.substr(0, split) + " (" + subject.substr(split + separator.size()) + ")";
    return subject;
}

// Who chip text insets (LLButton pads) and the gap after its leading icon.
constexpr S32 kWhoChipLeftPad = 28, kWhoChipRightPad = 24, kWhoChipIconGap = 1, kWhoChipSlack = 8;

// Section chips are as wide as their text, as in the mockup (10 px padding each side, 6 px apart).
constexpr S32 kSectionChipPad = 10, kSectionChipGap = 6, kSectionChipSlack = 6;

static void placeSectionChips(const std::vector<LLButton*>& chips, S32 left)
{
    const LLFontGL* font = ALKitStyle::sansFont("Kit12");
    S32 x = left;
    for (LLButton* chip : chips)
    {
        if (!chip->getVisible())
            continue;
        const S32 text_w = font ? font->getWidth(chip->getText()) : 100;
        chip->reshape(text_w + 2 * kSectionChipPad + kSectionChipSlack, 24, true);
        chip->setOrigin(x, 7);
        x += chip->getRect().getWidth() + kSectionChipGap;
    }
}

// Prefix is the page's own who_label (e.g. "Stepping"); pages without one keep the
// historical "Lighting" prefix.
std::string whoChipLabel(const std::string& who_label, const LLComboBox* combo, bool follows_cast)
{
    const std::string prefix = who_label.empty() ? std::string("Lighting") : who_label;
    return prefix + ": " + whoChipSubject(combo) + (follows_cast ? " (follows cast)" : "");
}

bool whoChipHasLabel(const LLButton* chip, const std::string& label)
{
    return chip && chip->getLabelUnselected() == label && chip->getLabelSelected() == label;
}

class SectionHeading final : public LLPanel, public ALFlowGridItem
{
public:
    explicit SectionHeading(const LLPanel::Params& p, std::string label) : LLPanel(p), mLabel(std::move(label)) {}
    S32  heightForWidth(S32) const override { return 28; }
    bool spansAllColumns() const override { return true; }
    void draw() override
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
        if (const LLFontGL* f = LLFontGL::getFontSansSerifSmall())
        {
            f->renderUTF8(mLabel, 0, 0, 8, ALKitStyle::color(ALKitStyle::MUTED).get(), LLFontGL::LEFT, LLFontGL::VCENTER);
            const S32 bottom = 8 - f->getLineHeight() / 2;
            ALUIPerfSelfTest::recordKitText(this, mLabel,
                                            LLRect(0, bottom + f->getLineHeight(), f->getWidth(mLabel), bottom), this);
        }
        LLPanel::draw();
    }

private:
    std::string mLabel;
};

} // namespace

ALKitPage::Section::Section() : id("id"), label("label"), cards("cards")
{
}

ALKitPage::Mode::Mode() : name("name"), cards("cards")
{
}

ALKitPage::Params::Params() :
    page("page"),
    summary("summary", ""),
    master_widget("master_widget", ""),
    master_setting("master_setting", ""),
    master_label("master_label", ""),
    master_tooltip("master_tooltip", ""),
    modes("modes", ""),
    mode_setting("mode_setting", ""),
    mode_widget("mode_widget", ""),
    mode_on("mode_on", ""),
    mode_tooltip("mode_tooltip", ""),
    mode_adapter("mode_adapter", ""),
    who_label("who_label", ""),
    who_picker("who_picker", ""),
    who_adapter("who_adapter", ""),
    preset_mode("preset_mode", "none"),
    preset_provider("preset_provider", ""),
    undo_context("undo_context", ""),
    watch_settings("watch_settings", ""),
    chip_mode("chip_mode", "auto"),
    legacy("legacy", false),
    actor_target("actor_target", false),
    actor_target_synthetic("actor_target_synthetic", false),
    section("section"),
    mode("mode")
{
}

ALKitPage::ALKitPage(const Params& p) :
    LLPanel(p),
    mPage(p.page),
    mSummary(p.summary),
    mWatchSettings(p.watch_settings),
    mMasterWidget(p.master_widget),
    mMasterSetting(p.master_setting),
    mMasterLabel(p.master_label),
    mMasterTooltip(p.master_tooltip),
    mModes(p.modes),
    mModeSetting(p.mode_setting),
    mModeWidget(p.mode_widget),
    mModeOn(p.mode_on),
    mModeAdapter(p.mode_adapter),
    mWhoLabel(p.who_label),
    mWhoPicker(p.who_picker),
    mWhoAdapter(p.who_adapter),
    mPresetMode(p.preset_mode),
    mPresetProvider(p.preset_provider),
    mChipMode(p.chip_mode),
    mLegacy(p.legacy),
    mActorTargetPage(p.actor_target),
    mActorTargetSynthetic(p.actor_target_synthetic)
{
    for (const Section& s : p.section)
    {
        SectionData data;
        data.id    = s.id;
        data.label = s.label;
        data.cards = tokens(s.cards);
        mSections.push_back(data);
    }

    for (const Mode& m : p.mode)
    {
        ModeData data;
        data.name  = m.name;
        data.cards = tokens(m.cards);
        mModeDefs.push_back(data);
    }
}

bool ALKitScroller::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_PAGE_UP || key == KEY_PAGE_DOWN)
        return false;
    return LLScrollContainer::handleKeyHere(key, mask);
}

ALKitScroller::ALKitScroller(const Params& p) : LLScrollContainer(p)
{
    ALKitStyle::styleScroller(this);
}

ALKitSearchEditor::ALKitSearchEditor(const Params& p) : LLLineEditor(p)
{
    setTextPadding(24, 6);
    ALKitStyle::styleEditor(this);
}

void ALKitSearchEditor::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLLineEditor::draw();
    static LLPointer<LLUIImage> search = LLUI::getUIImage("KitIcon_search_16");
    if (search)
        search->draw(5, (getRect().getHeight() - 16) / 2, 16, 16, ALKitStyle::color(ALKitStyle::MUTED).get());
}

bool ALKitSearchEditor::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_PAGE_UP || key == KEY_PAGE_DOWN)
        return false;
    if (mask == MASK_NONE && (key == KEY_UP || key == KEY_DOWN || key == KEY_RETURN || key == KEY_ESCAPE))
    {
        if (ALKitShell* shell = ALKitShell::find(this))
        {
            if (shell->handleSearchKey(key))
                return true;
        }
    }
    return LLLineEditor::handleKeyHere(key, mask);
}

bool ALKitSearchEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
    const bool handled = LLLineEditor::handleMouseDown(x, y, mask);
    // Clicking back into a non-empty box re-shows its results popup.
    if (!getText().empty())
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->setSearchPopupArmed(this, true);
    return handled;
}

void ALKitDoc::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (mDimmed)
    {
        LLViewDrawContext context(ALKitStyle::DIM_ALPHA);
        LLPanel::draw();
    }
    else
        LLPanel::draw();
}

std::string ALKitPage::prefix() const
{
    std::string  p     = mPage;
    const size_t slash = p.rfind('/');
    if (slash != std::string::npos)
        p = p.substr(slash + 1);
    const std::string suffix = "_tab";
    if (p.size() > suffix.size() && p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0)
        p.erase(p.size() - suffix.size());
    return p;
}

bool ALKitPage::postBuild()
{
    if (!LLPanel::postBuild())
        return false;

    const std::string        pre     = prefix();
    std::vector<std::string> watched = tokens(mWatchSettings);
    if (!mModeSetting.empty())
        watched.push_back(mModeSetting);

    std::vector<LLView*> xml_children;
    for (LLView* c : *getChildList())
        xml_children.push_back(c);

    const S32 page_width = getRect().getWidth();
    mBar = makePanel(pre + "_bar", ALKitStyle::BAR_H, page_width);
    mBar->setBackgroundVisible(true);
    mBar->setBackgroundColor(ALKitStyle::color(ALKitStyle::WINDOW_BG));
    addChild(mBar);
    mChipsContainer = makePanel(pre + "_chips", ALKitStyle::CHIPS_H, page_width);
    mChipsContainer->setBackgroundVisible(true);
    mChipsContainer->setBackgroundColor(ALKitStyle::color(ALKitStyle::SUB_BAND));
    mChipsContainer->setVisible(false);
    addChild(mChipsContainer);
    mWhoContainer = makePanel(pre + "_who", ALKitStyle::WHO_H, page_width);
    mWhoContainer->setBackgroundVisible(true);
    mWhoContainer->setBackgroundColor(ALKitStyle::color(ALKitStyle::INPUT_BG));
    mWhoContainer->setVisible(false);
    addChild(mWhoContainer);
    S32 bar_x = ALKitStyle::PAGE_PAD_LR;

    if (mPresetMode == "provider" && !mPresetProvider.empty())
    {
        ALKitPresetStrip::Params preset_params;
        preset_params.name      = pre + "_preset_strip";
        preset_params.provider  = mPresetProvider;
        preset_params.page      = mPage;
        preset_params.rect      = LLRect(bar_x, 34, bar_x + 334, 10);
        ALKitPresetStrip* strip = LLUICtrlFactory::create<ALKitPresetStrip>(preset_params);
        mBar->addChild(strip);
        bar_x += 340;
    }

    if (mPresetMode == "widgets")
        if (ALKitPresetHost* host = findChild<ALKitPresetHost>(pre + "_preset_host"))
        {
            std::vector<LLView*> items;
            for (LLView* c : *host->getChildList())
                items.push_back(c);
            // LLView exposes children frontmost-first, opposite to their XUI
            // declaration. The preset strip follows the authored order.
            std::reverse(items.begin(), items.end());
            for (S32 i = 0; i < (S32)items.size(); ++i)
            {
                LLView* c = items[i];
                mBar->addChild(c);
                if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(c))
                    control->setFocus(false);
                c->setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);
                S32 item_width = ALKitStyle::PRESET_COMBO_W;
                if (i != 0)
                {
                    LLButton* button = dynamic_cast<LLButton*>(c);
                    const S32 label_width = button && button->getFont() ? button->getFont()->getWidth(button->getText()) : 24;
                    item_width = llmax(28, label_width + 18);
                    if (button && c->getName().find("_save") != std::string::npos)
                    {
                        button->setLabel("Save as...");
                        item_width = 76;
                    }
                    else if (button && c->getName().find("_delete") != std::string::npos)
                    {
                        button->setLabel("");
                        button->setImageOverlay("KitIcon_trash_16");
                        button->setImageOverlayColor(ALKitStyle::color(ALKitStyle::ICON_TINT_STRONG));
                        button->setToolTip(LLStringExplicit("Delete preset"));
                        item_width = 28;
                    }
                }
                c->reshape(item_width, 24, true);
                c->setOrigin(bar_x, 10);
                bar_x += c->getRect().getWidth() + ALKitStyle::COL_GAP;
            }
            host->setVisible(false);
        }

    if (mLegacy && !mSummary.empty())
    {
        LLTextBox::Params summary;
        summary.name          = pre + "_legacy_summary";
        summary.rect          = LLRect(0, 20, ALKitStyle::PRESET_COMBO_W, 0);
        summary.initial_value = mSummary;
        summary.use_ellipses  = true;
        summary.text_color    = ALKitStyle::color(ALKitStyle::MUTED);
        mLegacySummary        = LLUICtrlFactory::create<LLTextBox>(summary);
        mBar->addChild(mLegacySummary);
        mLegacySummary->setOrigin(bar_x, 12);
        bar_x += ALKitStyle::PRESET_COMBO_W + ALKitStyle::COL_GAP;
    }

    LLButton* reset = makeButton(pre + "_reset_page", "");
    mResetPage = reset;
    reset->setImageOverlay("KitIcon_reset_arc_16");
    reset->setImageOverlayColor(ALKitStyle::color(ALKitStyle::ICON_TINT_STRONG));
    reset->setCommitCallback([this](LLUICtrl*, const LLSD&) { resetPage(); });
    if (mLegacy)
    {
        // Legacy pages have no kit cards, so Reset page has nothing to reset.
        reset->setEnabled(false);
        reset->setToolTip(LLStringExplicit("Available after this page is converted"));
    }
    mBar->addChild(reset);
    reset->reshape(28, 24, true);
    reset->setOrigin(bar_x, 10);
    bar_x += 34;

    buildResetPopover();

    mUndo = makeButton(pre + "_undo", "");
    mRedo = makeButton(pre + "_redo", "");
    mUndo->setImageOverlay("KitIcon_undo_16");
    mRedo->setImageOverlay("KitIcon_redo_16");
    mUndo->setImageOverlayColor(ALKitStyle::color(ALKitStyle::ICON_TINT_STRONG));
    mRedo->setImageOverlayColor(ALKitStyle::color(ALKitStyle::ICON_TINT_STRONG));
    mUndo->setToolTip(LLStringExplicit("Undo"));
    mRedo->setToolTip(LLStringExplicit("Redo"));
    mUndo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            if (ALKitShell* s = ALKitShell::find(this))
                s->history().undo();
        });
    mRedo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            if (ALKitShell* s = ALKitShell::find(this))
                s->history().redo();
        });
    mBar->addChild(mUndo);
    mBar->addChild(mRedo);
    mUndo->reshape(28, 24, true);
    mRedo->reshape(28, 24, true);
    mUndo->setOrigin(bar_x, 10);
    bar_x += 34;
    mRedo->setOrigin(bar_x, 10);
    mBarLeftEnd = bar_x + 28;
    LLPanel* divider = makePanel(pre + "_bar_divider", 18, 1);
    divider->setBackgroundVisible(true);
    divider->setBackgroundColor(ALKitStyle::color(ALKitStyle::DIVIDER));
    divider->setOrigin(mBarLeftEnd + 6, 13);
    mBar->addChild(divider);
    mBarLeftEnd += 13;

    if (!mMasterWidget.empty())
    {
        mMasterBox = makePanel(pre + "_master_box", 24);
        // This is a layout holder, not a control.  If left mouse-opaque it
        // becomes the fallback handler whenever the switch bounding rect is
        // stale (the page master is not laid out through ALKitRow).
        mMasterBox->setMouseOpaque(false);
        mBar->addChild(mMasterBox);
        LLTextBox::Params master_label;
        master_label.name          = pre + "_master_label";
        master_label.rect          = LLRect(0, 20, ALKitStyle::MASTER_LABEL_MIN_W, 0);
        master_label.initial_value = mMasterLabel;
        mMasterLabelText           = LLUICtrlFactory::create<LLTextBox>(master_label);
        mMasterBox->addChild(mMasterLabelText);
        master_label.name          = pre + "_master_state";
        master_label.rect          = LLRect(122, 20, 150, 0);
        master_label.initial_value = "";
        mMasterState               = LLUICtrlFactory::create<LLTextBox>(master_label);
        mMasterBox->addChild(mMasterState);
        ALKitSwitch::Params sp;
        sp.name = mMasterWidget;
        sp.rect = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
        mMaster = LLUICtrlFactory::create<ALKitSwitch>(sp);
        mMaster->setControlName(mMasterSetting, this);
        mMaster->setToolTip(mMasterTooltip);
        mMasterBox->addChild(mMaster);
        mConnections.emplace_back(mMaster->setFlipCallback([this](bool) { reshape(getRect().getWidth(), getRect().getHeight(), true); }));
    }

    if (!mModes.empty())
    {
        ALKitSwitch::Params sp;
        sp.name     = mModeWidget;
        sp.rect     = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
        sp.visible  = false;
        mModeSwitch = LLUICtrlFactory::create<ALKitSwitch>(sp);
        mModeSwitch->setControlName(mModeSetting, this);
        mBar->addChild(mModeSwitch);
        mConnections.emplace_back(mModeSwitch->setFlipCallback(
            [this](bool requested)
            {
                if (mUpdatingModeSwitch)
                    return;
                if (ALKitModeAdapters::Adapter adapter = ALKitModeAdapters::get(getParent(), mModeAdapter))
                {
                    ALKitHistory::ScopedSuspend suspend;
                    const bool                  effective = adapter(requested, ALKitModeAdapters::PASSIVE);
                    if (effective != requested)
                    {
                        mUpdatingModeSwitch = true;
                        mModeSwitch->setValue(effective);
                        mUpdatingModeSwitch = false;
                    }
                }
                syncEffectiveMode();
            }));
        syncEffectiveMode();
    }

    ALKitSearchEditor::Params lp;
    lp.name  = pre + "_search";
    lp.rect  = LLRect(0, 24, ALKitStyle::SEARCH_W, 0);
    lp.label = "Find a control...";
    mSearch  = LLUICtrlFactory::create<ALKitSearchEditor>(lp);
    mSearch->setCommitCallback([this](LLUICtrl* c, const LLSD&) { setSearchText(c->getValue().asString()); });
    mBar->addChild(mSearch);
    LLCheckBoxCtrl::Params changed;
    changed.name = pre + "_changed_only";
    changed.rect = LLRect(0, 24, 100, 0);
    changed.label = "Changed only";
    mChangedOnly = LLUICtrlFactory::create<LLCheckBoxCtrl>(changed);
    ALKitShell* shell = ALKitShell::find(this);
    mChangedOnly->setValue(shell && shell->changedOnly());
    mChangedOnly->setCommitCallback(
        [this](LLUICtrl* control, const LLSD&)
        {
            if (ALKitShell* shell = ALKitShell::find(this))
                shell->setChangedOnly(control->getValue().asBoolean());
        });
    mBar->addChild(mChangedOnly);
    if (mLegacy)
    {
        // Legacy pages have no kit rows to filter by change.
        mChangedOnly->setEnabled(false);
        mChangedOnly->setToolTip(LLStringExplicit("Available after this page is converted"));
    }

    mBanner = makePanel(pre + "_banner", ALKitStyle::BANNER_H, page_width);
    mBanner->setBackgroundVisible(true);
    mBanner->setBackgroundColor(ALKitStyle::color(ALKitStyle::BANNER_BG));
    LLTextBox::Params bt;
    bt.name          = pre + "_banner_text";
    bt.rect          = LLRect(10, 24, 360, 0);
    bt.initial_value = mMasterLabel + " is off. Controls stay editable.";
    LLTextBox* btext = LLUICtrlFactory::create<LLTextBox>(bt);
    mBanner->addChild(btext);
    LLButton* turn = makeButton(pre + "_master_on", "Turn on");
    turn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            if (mMaster)
            {
                mMaster->writeBoundValue(true);
                mMaster->setValue(true);
                mMaster->onCommit();
            }
        });
    mBanner->addChild(turn);
    turn->setOrigin(370, 4);
    addChild(mBanner);

    std::string gate_text;
    if (mPage == "light_group/light_rim_tab")
        gate_text = "Light Rig is off: rim has no effect";
    else if (mPage == "world_group/weather_tab")
        gate_text = "Weather is off";
    else if (mPage == "world_group/atmosphere_tab" || mPage == "light_group/cine_light_rig_tab")
        gate_text = "Light masking is on: projector volumetrics hidden";
    if (!gate_text.empty())
    {
        mGateBanner = makePanel(pre + "_gate_banner", ALKitStyle::BANNER_H, page_width);
        mGateBanner->setBackgroundVisible(true);
        mGateBanner->setBackgroundColor(ALKitStyle::color(ALKitStyle::BANNER_BG));
        LLTextBox::Params gate;
        gate.name          = pre + "_gate_text";
        gate.rect          = LLRect(10, 24, page_width - 10, 0);
        gate.initial_value = gate_text;
        mGateText          = LLUICtrlFactory::create<LLTextBox>(gate);
        mGateBanner->addChild(mGateText);
        if (mPage == "light_group/light_rim_tab" || mPage == "world_group/weather_tab")
        {
            mGateButton = makeButton(pre + "_gate_on", "Turn on");
            if (mPage == "light_group/light_rim_tab")
                mGateButton->setCommitCallback(
                    [](LLUICtrl*, const LLSD&) { gSavedSettings.setBOOL("CineLightRigEnabled", true); });
            else
                mGateButton->setCommitCallback(
                    [](LLUICtrl*, const LLSD&) { gSavedSettings.setBOOL("AlchemyWeatherEnabled", true); });
            mGateBanner->addChild(mGateButton);
            mGateButton->setOrigin(370, 4);
        }
        addChild(mGateBanner);
    }

    if (!mWhoPicker.empty() || mActorTargetPage)
    {
        mWhoContainer->setVisible(true);
        LLTextBox::Params who_hint;
        who_hint.name          = pre + "_who_hint";
        who_hint.rect          = LLRect(0, 24, 420, 0);
        who_hint.initial_value = "Follows the Cast selection; pick one to lock.";
        who_hint.use_ellipses  = true;
        who_hint.text_color    = ALKitStyle::color(ALKitStyle::MUTED);
        mWhoHint = LLUICtrlFactory::create<LLTextBox>(who_hint);
        mWhoContainer->addChild(mWhoHint);
        who_hint.name          = pre + "_who_label";
        who_hint.rect          = LLRect(0, 20, 92, 0);
        std::string who_caps = mWhoLabel;
        LLStringUtil::toUpper(who_caps);
        who_hint.initial_value = who_caps;
        who_hint.font          = ALKitStyle::sansFont("Kit10", true);
        mWhoLabelText          = LLUICtrlFactory::create<LLTextBox>(who_hint);
        mWhoContainer->addChild(mWhoLabelText);
        mWhoCombo = (mWhoPicker.empty() || mActorTargetSynthetic) ? nullptr : findChild<LLComboBox>(mWhoPicker);
        mWhoChip  = makeButton(pre + "_who_chip", mWhoLabel);
        mWhoChip->setImageOverlay("KitIcon_who_person_16", LLFontGL::LEFT);
        mWhoChip->setImageOverlayColor(ALKitStyle::color(ALKitStyle::ICON_TINT_STRONG));
        mWhoChip->setHAlign(LLFontGL::LEFT);
        mWhoChip->setLeftHPad(kWhoChipLeftPad);
        mWhoChip->setRightHPad(kWhoChipRightPad);
        LLTextBox::Params caret_text;
        caret_text.name          = pre + "_who_caret";
        caret_text.rect          = LLRect(0, 16, 10, 0);
        caret_text.initial_value = "\xE2\x96\xBE";
        caret_text.font          = ALKitStyle::sansFont("Kit10");
        caret_text.text_color    = ALKitStyle::color(ALKitStyle::MUTED);
        mWhoCaretText            = LLUICtrlFactory::create<LLTextBox>(caret_text);
        mWhoCaretText->setMouseOpaque(false);
        mWhoChip->addChild(mWhoCaretText);
        mWhoContainer->addChild(mWhoChip);
        if (mWhoCombo)
        {
            mWhoCombo->setVisible(false);
            mWhoCombo->setCommitCallback(
                [this](LLUICtrl*, const LLSD&)
                {
                    mWhoError = false;
                    mWhoCombo->setVisible(false);
                    if (mWhoChip)
                    {
                        mWhoChip->setLabel(whoChipLabel(mWhoLabel, mWhoCombo, !mWhoPinned));
                        reshape(getRect().getWidth(), getRect().getHeight(), false);
                    }
                });
            mWhoChip->setLabel(whoChipLabel(mWhoLabel, mWhoCombo, !mWhoPinned));
        }
        else
        {
            mWhoChip->setLabel("Actor: follow selection");
            mWhoChip->setCommitCallback([this](LLUICtrl*, const LLSD&)
            {
                mWhoPinned = !mWhoPinned && mActorTarget.notNull();
                setWhoChipLabel(mWhoPinned ? "Actor: locked" : "Actor: follow selection");
                refreshActorTargetAvailability();
                saveState();
            });
        }
        if (ALKitShell* shell = ALKitShell::find(this))
        {
            mConnections.emplace_back(shell->setCastSelectionCallback(
                [this](const ALKitShell::uuid_vec_t& ids)
                {
                    if (mWhoPinned)
                        return;
                    if (mActorTargetPage && !mWhoCombo)
                    {
                        mActorTarget = ids.empty() ? LLUUID::null : ids.front();
                        mWhoError = ids.empty();
                        setWhoChipLabel(ids.empty() ? "Actor: none" : "Actor: follows cast");
                        refreshActorTargetAvailability();
                        return;
                    }
                    if (!mWhoCombo)
                        return;
                    if (!ids.empty())
                        if (ALKitWhoAdapters::Adapter adapter = ALKitWhoAdapters::get(mWhoAdapter))
                        {
                            const LLSD value = adapter(ids.front());
                            if (value.isDefined())
                            {
                                mWhoCombo->setValue(value);
                                mWhoCombo->onCommit();
                                return;
                            }
                        }
                    mWhoError = true;
                    setWhoChipLabel("That subject isn't selectable here");
                }));
            if (mActorTargetSynthetic && !shell->castSelection().empty())
            {
                mActorTarget = shell->castSelection().front();
                mWhoChip->setLabel("Actor: follows cast");
            }
        }
        if (mActorTargetPage)
        {
            mConnections.emplace_back(LLDirectorCast::instance().addChangedCallback(
                [this]() { refreshActorTargetAvailability(); }));
            mConnections.emplace_back(gAgent.addRegionChangedCallback(
                [this]() { refreshActorTargetAvailability(); }));
            refreshActorTargetAvailability();
        }
    }

    if (mLegacy)
    {
        LLPanel::Params hp;
        hp.name = pre + "_legacy_host";
        hp.rect = getLocalRect();
        hp.follows.flags(FOLLOWS_ALL);
        hp.background_visible = false;
        mLegacyHost           = LLUICtrlFactory::create<LLPanel>(hp);
        addChild(mLegacyHost);
        for (LLView* c : xml_children)
            if (c != mBar && c != mBanner && c != mGateBanner && c != mWhoChip && c != mWhoCombo &&
                dynamic_cast<ALKitPresetHost*>(c) == nullptr)
                mLegacyHost->addChild(c);
        if (mPage == "cast_group/path_tab")
            mPathPlayback = mLegacyHost->findChild<LLPanel>("nodes_playback_panel");
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->history().watchSettings(mPage, watched);
        reshape(getRect().getWidth(), getRect().getHeight(), true);
        ALKitStyle::styleTree(mBar);
        ALKitStyle::styleTree(mChipsContainer);
        ALKitStyle::styleTree(mWhoContainer);
        ALKitStyle::styleTree(mBanner);
        ALKitStyle::styleTree(mGateBanner);
        ALKitStyle::stylePill(mWhoChip);
        if (mWhoChip)
            mWhoChip->setUnselectedLabelColor(ALKitStyle::color(ALKitStyle::TEXT_STRONG));
        stabilizeComboEditors(this);
        mRestoringState = false;
        return true;
    }

    std::vector<ALKitCard*> cards;
    for (LLView* c : xml_children)
        if (ALKitCard* card = dynamic_cast<ALKitCard*>(c))
            cards.push_back(card);

    ALKitScroller::Params sp;
    sp.name = pre + "_scroll";
    sp.rect = getLocalRect();
    sp.follows.flags(FOLLOWS_ALL);
    sp.is_opaque             = false;
    sp.reserve_scroll_corner = false;
    mScroller                = LLUICtrlFactory::create<ALKitScroller>(sp);

    ALKitDoc::Params dp;
    dp.name = pre + "_doc";
    dp.rect = getLocalRect();
    dp.follows.flags(FOLLOWS_LEFT | FOLLOWS_TOP | FOLLOWS_RIGHT);
    mDoc = LLUICtrlFactory::create<ALKitDoc>(dp);

    ALFlowGrid::Params gp;
    gp.name        = pre + "_grid";
    gp.rect        = getLocalRect();
    gp.pack        = "rows";
    gp.max_columns = 2;
    gp.card_width  = ALKitStyle::CARD_MIN_W; // the narrowest card; xui_layout_check reads this same constant
    gp.column_gap  = 12;
    gp.row_gap     = 12;
    gp.pad         = 0;
    mGrid          = LLUICtrlFactory::create<ALFlowGrid>(gp);

    mScroller->addChild(mDoc);
    mDoc->addChild(mGrid);
    addChild(mScroller);

    std::vector<ALKitCard*> ordered;
    for (S32 si = 0; si < (S32)mSections.size(); ++si)
    {
        LLPanel::Params hp;
        hp.name                 = pre + "_section_" + mSections[si].id;
        hp.rect                 = LLRect(0, 28, 100, 0);
        SectionHeading* heading = new SectionHeading(hp, mSections[si].label);
        mGrid->addChild(heading);
        mSectionHeadings.push_back(heading);
        for (const std::string& id : mSections[si].cards)
            for (ALKitCard* card : cards)
                if (card->id() == id && std::find(ordered.begin(), ordered.end(), card) == ordered.end())
                {
                    mGrid->addChild(card);
                    ordered.push_back(card);
                }
    }

    for (ALKitCard* card : cards)
        if (std::find(ordered.begin(), ordered.end(), card) == ordered.end())
            mGrid->addChild(card);

    for (ALKitCard* card : cards)
    {
        mCards[card->id()] = card;
        for (ALKitRow* row : card->rows())
        {
            row->setOwnerIdentity(mPage, card->id());
            row->refreshFavorite();
            mRows[row->meta().widget] = row;
            if (!row->meta().setting.empty())
            {
                mRows[row->meta().setting] = row;
                watched.push_back(row->meta().setting);
            }
        }
    }

    if (mSections.size() > 1)
    {
        mChipsContainer->setVisible(true);
        for (S32 i = 0; i < (S32)mSections.size(); ++i)
        {
            LLButton* chip =
                makeButton(pre + "_chip_" + llformat("%d", i), mSections[i].label + " " + llformat("%d", (S32)mSections[i].cards.size()));
            chip->setCommitCallback(
                [this, i](LLUICtrl*, const LLSD&)
                {
                    mChipIndex = i;
                    applyModeAndChip();
                    saveState();
                    if (i < (S32)mSectionHeadings.size() && mSectionHeadings[i]->getVisible())
                        scrollToView(mSectionHeadings[i]);
                });
            mChipsContainer->addChild(chip);
            mChips.push_back(chip);
        }
    }

    if (ALKitShell* shell = ALKitShell::find(this))
        shell->history().watchSettings(mPage, watched);

    restoreState();
    reshape(getRect().getWidth(), getRect().getHeight(), true);
    ALKitStyle::styleTree(this);
    for (auto& row : mRows)
        row.second->refreshValueEditorStyle();
    ALKitStyle::stylePill(mWhoChip);
    if (mWhoChip)
        mWhoChip->setUnselectedLabelColor(ALKitStyle::color(ALKitStyle::TEXT_STRONG));
    stabilizeComboEditors(this);
    struct ComboSpec
    {
        const char* name;
        S32 self_value;
        S32 subject_a_value;
        S32 subject_count;
        SubjectValueSource value_source = SubjectValueSource::CONTROL;
    };
    static const ComboSpec subject_specs[] = {
        { "sd_subject_target_combo", 0, 1, 4 },
        { "ri_subject_target_combo", 0, 1, 4 },
        { "tr_subject_target_combo", 0, 1, 4 },
        { "cine_easy_probe_target", 0, 1, 4 },
        { "cine_live_probe_target", 0, 1, 4 },
        { "cine_night_mask_target", 0, 1, 4 },
        { "db_focus", 0, 2, 1, SubjectValueSource::DANCE_FOCUS },
        { "db_block_centre", 0, 2, 1,
          SubjectValueSource::DANCE_BLOCK_CENTRE },
        { "db_wave_focus", 0, 2, 1,
          SubjectValueSource::DANCE_WAVE_FOCUS }
    };
    for (const ComboSpec& spec : subject_specs)
    {
        if (LLComboBox* combo = findChild<LLComboBox>(spec.name, true))
        {
            mSubjectCombos.push_back(
                { combo, spec.self_value, spec.subject_a_value,
                  spec.subject_count, spec.value_source });
            if (ALKitRow* row = findRow(spec.name))
                row->setSubjectDisplayValues(spec.self_value,
                                             spec.subject_a_value,
                                             spec.subject_count);
        }
    }
    return true;
}

void ALKitPage::setModeIndex(S32 index, bool explicit_user, bool search)
{
    if (index < 0 || index >= (S32)mModeDefs.size())
        return;
    if (mRestoringState)
    {
        syncEffectiveMode();
        return;
    }
    if (mModeSwitch)
    {
        bool on             = mModeDefs[index].name == mModeOn;
        mUpdatingModeSwitch = true;
        if (ALKitModeAdapters::Adapter adapter = ALKitModeAdapters::get(getParent(), mModeAdapter))
        {
            if (!explicit_user)
            {
                ALKitHistory::ScopedSuspend suspend;
                on = adapter(on, search ? ALKitModeAdapters::SEARCH : ALKitModeAdapters::PASSIVE);
            }
            else
                on = adapter(on, ALKitModeAdapters::USER);
            mModeSwitch->setValue(on);
        }
        else
        {
            mModeSwitch->writeBoundValue(on);
            mModeSwitch->setValue(on);
        }
        mUpdatingModeSwitch = false;
        syncEffectiveMode();
    }
    else
    {
        mModeIndex = index;
        applyModeAndChip();
    }
    if (explicit_user)
        saveState();
}

void ALKitPage::syncEffectiveMode()
{
    if (!mModeSwitch)
        return;
    const bool on = mModeSwitch->isOn();
    for (S32 i = 0; i < (S32)mModeDefs.size(); ++i)
        if ((mModeDefs[i].name == mModeOn) == on)
        {
            mModeIndex = i;
            break;
        }
    applyModeAndChip();
}

void ALKitPage::rebindRows()
{
    for (auto& entry : mRows)
        entry.second->resolveBinding();
}

void ALKitPage::applyModeAndChip(bool request_layout)
{
    S32 visible_count = 0;
    for (const auto& pair : mCards)
    {
        bool visible = true;
        if (!mModeDefs.empty())
        {
            bool listed = false;
            for (const ModeData& m : mModeDefs)
                listed |= std::find(m.cards.begin(), m.cards.end(), pair.first) != m.cards.end();
            visible = !listed || (mModeIndex < (S32)mModeDefs.size() &&
                                  std::find(mModeDefs[mModeIndex].cards.begin(), mModeDefs[mModeIndex].cards.end(), pair.first) !=
                                      mModeDefs[mModeIndex].cards.end());
        }
        bool search = !mSearching;
        if (!search)
            for (ALKitRow* row : pair.second->rows())
                if (row->filterVisible())
                {
                    search = true;
                    break;
                }
        if (visible && search && pair.second->codeVisible())
            ++visible_count;
    }
    const bool filtered = mChipMode == "filter" || (mChipMode == "auto" && visible_count >= 6);
    auto visibleInMode = [this](const std::string& id)
    {
        if (mModeDefs.empty())
            return true;
        bool listed = false;
        for (const ModeData& mode : mModeDefs)
            listed |= std::find(mode.cards.begin(), mode.cards.end(), id) != mode.cards.end();
        return !listed || (mModeIndex < (S32)mModeDefs.size() &&
                           std::find(mModeDefs[mModeIndex].cards.begin(), mModeDefs[mModeIndex].cards.end(), id) !=
                               mModeDefs[mModeIndex].cards.end());
    };
    auto visibleInSearch = [this](const ALKitCard* card)
    {
        if (!mSearching)
            return true;
        for (ALKitRow* row : card->rows())
            if (row->filterVisible())
                return true;
        return false;
    };

    for (auto& pair : mCards)
    {
        // While searching, every card with a hit shows regardless of mode/chip (pre-existing behaviour).
        const bool mode = mSearching || visibleInMode(pair.first);
        bool section = true;
        if (!mSearching && filtered && !mSections.empty() && mChipIndex < (S32)mSections.size())
            section = std::find(mSections[mChipIndex].cards.begin(), mSections[mChipIndex].cards.end(), pair.first) !=
                      mSections[mChipIndex].cards.end();
        const bool search = visibleInSearch(pair.second);
        pair.second->setVisible(mode && section && search && pair.second->codeVisible());
    }

    for (S32 i = 0; i < (S32)mSectionHeadings.size(); ++i)
    {
        bool any = false;
        if (i < (S32)mSections.size())
            for (const std::string& id : mSections[i].cards)
            {
                auto found = mCards.find(id);
                if (found != mCards.end() && visibleInMode(id) && visibleInSearch(found->second) &&
                    found->second->codeVisible())
                {
                    any = true;
                    break;
                }
            }
        mSectionHeadings[i]->setVisible(!filtered && any);
        if (i < (S32)mChips.size())
        {
            mChips[i]->setVisible(any);
            mChips[i]->setToggleState(filtered && i == mChipIndex);
            ALKitStyle::stylePill(mChips[i]);
            mChips[i]->setImageColor(filtered && i == mChipIndex ? ALKitStyle::color(ALKitStyle::ACTIVE_FILL)
                                                                  : ALKitStyle::color(ALKitStyle::CONTROL_BG));
            mChips[i]->setSelectedImageColor(ALKitStyle::color(ALKitStyle::ACTIVE_FILL));
            mChips[i]->setUnselectedLabelColor(filtered && i == mChipIndex ? ALKitStyle::color(ALKitStyle::ACCENT)
                                                                            : ALKitStyle::color(ALKitStyle::LABEL));
            mChips[i]->setSelectedLabelColor(ALKitStyle::color(ALKitStyle::ACCENT));
            S32 count = 0;
            if (i < (S32)mSections.size())
                for (const std::string& id : mSections[i].cards)
                {
                    auto found = mCards.find(id);
                    if (found != mCards.end() && visibleInMode(id) && visibleInSearch(found->second) &&
                        found->second->codeVisible())
                        ++count;
                }
            if (i < (S32)mSections.size())
            {
                const std::string label = mSections[i].label + " " + llformat("%d", count);
                if (mChips[i]->getLabelUnselected() != label)
                    mChips[i]->setLabel(label); // ui-draw-lint: allow(raw-write) change-gated chip count
            }
        }
    }
    placeSectionChips(mChips, ALKitStyle::PAGE_PAD_LR);
    if (request_layout)
        requestLayout();
}

void ALKitPage::updateMaster()
{
    const bool on = !mMaster || mMaster->isOn();
    if (mBanner)
        setVisibleIfChanged(mBanner, !on);
    if (mDoc)
        mDoc->setDimmed(!on);
    if (mMasterState)
        setTextIfChanged(mMasterState, on ? "On" : "Off");
}

bool ALKitPage::updateGateStatus()
{
    if (!mGateBanner)
        return false;

    bool show = false;
    if (mPage == "light_group/light_rim_tab")
        show = ALCineLightRigManager::instance().enabledMask() == 0;
    else if (mPage == "world_group/weather_tab")
    {
        static LLCachedControl<bool> weather_enabled(gSavedSettings, "AlchemyWeatherEnabled", false);
        show = !weather_enabled;
    }
    else
    {
        const U32 hidden = gPipeline.actorOnlyVolumetricLightCount();
        show = hidden > 0;
        if (show)
            setTextIfChanged(mGateText, llformat("Light masking is on: projector volumetrics hidden for %u light(s)", hidden));
    }
    const bool changed = mGateBanner->getVisible() != show;
    setVisibleIfChanged(mGateBanner, show);
    return changed;
}

// Sizes the who chip to its current label.  Every label change goes through
// setWhoChipLabel, so a chip never keeps the width of the label it replaced.
void ALKitPage::layoutWhoChip(S32 page_width)
{
    if (!mWhoChip || !mWhoLabelText || !mWhoHint || !mWhoCaretText)
        return;
    const S32 label_x = ALKitStyle::PAGE_PAD_LR;
    const S32 chip_x  = label_x + 94;
    // LLButton puts a LEFT overlay icon at the left pad and starts the label after it
    // (imgoverlay_label_space, default 1 px); the label keeps the right pad.  The text
    // budget must start there, or the end of the label is clipped by the icon's width.
    const LLPointer<LLUIImage> icon = mWhoChip->getImageOverlay();
    const S32 text_left = kWhoChipLeftPad + (icon.notNull() ? icon->getWidth() + kWhoChipIconGap : 0);
    // Measured in the font stylePill gives the chip; the chip may be sized before it is styled.
    const LLFontGL* chip_font = ALKitStyle::sansFont("Kit12");
    const S32 label_w   = chip_font ? chip_font->getWidth(mWhoChip->getText()) : 120;
    // The slack keeps the last glyph inside the chip's text area.
    const S32 content_w = text_left + label_w + kWhoChipRightPad + kWhoChipSlack;
    const S32 chip_w    = llmin(llmax(120, content_w), llmax(120, page_width - chip_x - 8));
    mWhoLabelText->setOrigin(label_x, 7);
    mWhoChip->reshape(chip_w, 24, true);
    mWhoChip->setOrigin(chip_x, 5);
    const S32 caret_x = llmin(text_left + label_w + 6, chip_w - 18);
    mWhoCaretText->setOrigin(caret_x, 4);
    mWhoHint->reshape(llmax(1, page_width - chip_x - chip_w - 24), 20, true);
    mWhoHint->setOrigin(chip_x + chip_w + 12, 7);
}

void ALKitPage::setWhoChipLabel(const std::string& label)
{
    if (!mWhoChip || whoChipHasLabel(mWhoChip, label))
        return;
    mWhoChip->setLabel(label);
    layoutWhoChip(getRect().getWidth());
}

void ALKitPage::reshape(S32 w, S32 h, bool p)
{
    LLPanel::reshape(w, h, p);
    // The v1 Path accordion fits its panel four pixels shorter at runtime.
    // Correct only the embedded v2 instance; the shared v1 XUI remains untouched.
    if (mPathPlayback)
        mPathPlayback->reshape(mPathPlayback->getRect().getWidth(), llmax(304, mPathPlayback->getRect().getHeight()), true);
    S32 top = h;
    if (!mChips.empty())
    {
        top -= ALKitStyle::CHIPS_H;
        mChipsContainer->reshape(w, ALKitStyle::CHIPS_H, true);
        mChipsContainer->setOrigin(0, top);
        placeSectionChips(mChips, ALKitStyle::PAGE_PAD_LR);
    }

    if (mWhoChip)
    {
        top -= ALKitStyle::WHO_H;
        mWhoContainer->reshape(w, ALKitStyle::WHO_H, true);
        mWhoContainer->setOrigin(0, top);
        layoutWhoChip(w);
    }
    top -= ALKitStyle::BAR_H;
    if (mBar)
    {
        mBar->reshape(w, ALKitStyle::BAR_H, true);
        mBar->setOrigin(0, top);
        // Master box: [label][gap][switch][gap][state].  The label box is sized to its
        // text (MASTER_LABEL_MIN_W..MAX_W) and the switch and state follow it.
        constexpr S32 kMasterGap = 8, kMasterStateW = 28;
        const S32  master_label_w  = ALKitStyle::masterLabelWidth(mMasterLabel);
        const S32  master_switch_x = master_label_w + kMasterGap;
        const S32  master_state_x  = master_switch_x + ALKitStyle::SWITCH_W + kMasterGap;
        const S32  master_box_w    = master_state_x + kMasterStateW;
        const S32  master_x       = mBarLeftEnd + ALKitStyle::COL_GAP;
        const S32  right          = w - ALKitStyle::PAGE_PAD_LR;
        const bool master_fits    = mMasterBox && master_x + master_box_w <= w - ALKitStyle::PAGE_PAD_LR;
        const S32  occupied_right = master_fits ? master_x + master_box_w : mBarLeftEnd;
        const S32  changed_w      = 100;
        const S32  search_w       = llmin(ALKitStyle::SEARCH_W, llmax(0, right - occupied_right - changed_w - 2 * ALKitStyle::COL_GAP));
        const S32  search_x       = right - search_w;
        const bool search_fits    = mSearch && search_w >= 120;
        if (mMasterBox)
        {
            mMasterBox->reshape(master_box_w, 24, true);
            mMasterBox->setOrigin(master_x, 10);
            setVisibleIfChanged(mMasterBox, master_fits);
        }
        if (mMasterLabelText)
        {
            const std::string shown = ALKitStyle::fitText(ALKitStyle::sansFont("Kit12"), mMasterLabel, master_label_w);
            mMasterLabelText->reshape(master_label_w, 20, true);
            mMasterLabelText->setOrigin(0, 0);
            setTextIfChanged(mMasterLabelText, shown);
            setToolTipIfChanged(mMasterLabelText, shown == mMasterLabel ? std::string() : mMasterLabel);
        }
        if (mMasterState)
        {
            mMasterState->reshape(kMasterStateW, 20, true);
            mMasterState->setOrigin(master_state_x, 0);
        }
        if (mMaster)
        {
            // LLCheckBoxCtrl uses a child-derived bounding rect.  Recompute it
            // after ALKitSwitch has replaced the stock button geometry so the
            // complete painted switch remains hittable.
            mMaster->reshape(ALKitStyle::SWITCH_W, ALKitStyle::SWITCH_H, true);
            mMaster->setOrigin(master_switch_x, 3);
        }
        if (mSearch)
        {
            mSearch->reshape(search_w, 24, true);
            mSearch->setOrigin(search_x, 10);
            setVisibleIfChanged(mSearch, search_fits);
        }
        if (mChangedOnly)
        {
            mChangedOnly->reshape(changed_w, 24, true);
            mChangedOnly->setOrigin(search_x - changed_w - ALKitStyle::COL_GAP, 10);
            setVisibleIfChanged(mChangedOnly, search_fits);
        }
    }
    updateMaster();
    updateGateStatus();
    if (mBanner && mBanner->getVisible())
    {
        top -= ALKitStyle::BANNER_H;
        mBanner->reshape(w, ALKitStyle::BANNER_H, true);
        mBanner->setOrigin(0, top);
    }
    if (mGateBanner && mGateBanner->getVisible())
    {
        top -= ALKitStyle::BANNER_H;
        mGateBanner->reshape(w, ALKitStyle::BANNER_H, true);
        mGateBanner->setOrigin(0, top);
        if (mGateText)
        {
            const S32 button_space = mGateButton ? 100 : 0;
            mGateText->reshape(llmax(1, w - 20 - button_space), 24, true);
            mGateText->setOrigin(10, 4);
        }
        if (mGateButton)
            mGateButton->setOrigin(w - 90, 4);
    }
    if (mScroller)
    {
        mScroller->reshape(w, llmax(1, top), true);
        mScroller->setOrigin(0, 0);
        relayoutNow();
    }
    if (mLegacyHost)
    {
        mLegacyHost->reshape(w, llmax(1, top), true);
        mLegacyHost->setOrigin(0, 0);
    }
}

void ALKitPage::relayoutNow()
{
    if (!mGrid || !mScroller)
        return;
    // Code visibility is persistent state. Combine it with all page filters
    // before every layout, including layouts requested directly by a card.
    applyModeAndChip(false);
    static LLCachedControl<S32> scrollbar_size(gSavedSettings, "UIScrollbarSize");
    const S32                   sb        = llmax(0, (S32)scrollbar_size);
    const S32                   doc_width = llmax(1, mScroller->getRect().getWidth() - 2 * mScroller->getBorderWidth() - sb);
    const S32                   width     = llmax(1, doc_width - 2 * ALKitStyle::PAGE_PAD_LR);
    mGrid->reshape(width, mGrid->getRect().getHeight(), true);
    const S32 natural  = mGrid->getRect().getHeight() + ALKitStyle::PAGE_PAD_T + ALKitStyle::PAGE_PAD_B;
    const S32 viewport = llmax(1, mScroller->getRect().getHeight() - 2 * mScroller->getBorderWidth());
    const S32 doc_h    = llmax(natural, viewport);
    // The document is deliberately no wider than the vertical-scroll viewport.
    // Kit pages never scroll horizontally.
    mDoc->reshape(doc_width, doc_h, true);
    mGrid->setOrigin(ALKitStyle::PAGE_PAD_LR, doc_h - ALKitStyle::PAGE_PAD_T - mGrid->getRect().getHeight());
}

void ALKitPage::requestLayout()
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
                ALKitPage* page      = dynamic_cast<ALKitPage*>(p);
                page->mLayoutPending = false;
                page->relayoutNow();
            }
        });
}

void ALKitPage::scrollToView(LLView* view)
{
    if (!view || !mScroller || !mDoc)
        return;
    LLHandle<LLPanel> page_handle = getHandle();
    LLHandle<LLView>  view_handle = view->getHandle();
    doOnIdleOneTime(
        [page_handle, view_handle]()
        {
            ALKitPage* page   = dynamic_cast<ALKitPage*>(page_handle.get());
            LLView*    target = view_handle.get();
            if (!page || !target)
                return;
            page->relayoutNow();
            LLRect rect;
            if (target->localRectToOtherView(target->getLocalRect(), &rect, page->mDoc))
            {
                const LLRect visible = page->mScroller->getVisibleContentRect();
                const S32 center = visible.getHeight() / 2;
                const S32 half = llmax(1, rect.getHeight() / 2);
                page->mScroller->scrollToShowRect(
                    rect, LLRect(0, center + half, visible.getWidth(), center - half));
            }
        });
}

ALKitRow* ALKitPage::findRow(const std::string& k) const
{
    auto i = mRows.find(k);
    return i == mRows.end() ? nullptr : i->second;
}
ALKitRow* ALKitPage::findRowBySetting(const std::string& s) const
{ return findRow(s); }
ALKitCard* ALKitPage::findCard(const std::string& k) const
{
    auto i = mCards.find(k);
    return i == mCards.end() ? nullptr : i->second;
}

bool ALKitPage::changed() const
{
    for (const auto& entry : mCards)
        if (entry.second && entry.second->changed())
            return true;
    return false;
}

void ALKitPage::registerDynamicRow(ALKitRow* row)
{
    if (row)
        mRows[row->meta().widget] = row;
}

void ALKitPage::unregisterDynamicRow(ALKitRow* row)
{
    if (!row)
        return;
    const auto found = mRows.find(row->meta().widget);
    if (found != mRows.end() && found->second == row)
        mRows.erase(found);
}

void ALKitPage::applyLocalFilter(const std::string& query, bool changed_only)
{
    std::string lowered = query;
    LLStringUtil::toLower(lowered);
    const std::vector<std::string> tokens = LLStringUtil::getTokens(lowered, " ");
    mSearching = !tokens.empty() || changed_only;
    for (auto& entry : mRows)
    {
        ALKitRow* row = entry.second;
        if (!row || entry.first != row->meta().widget)
            continue;
        std::string haystack = row->meta().label + " " + row->meta().help + " " + row->meta().keywords;
        LLStringUtil::toLower(haystack);
        bool visible = !changed_only || row->amber();
        for (const std::string& token : tokens)
            visible = visible && haystack.find(token) != std::string::npos;
        row->setFilterVisible(visible);
    }
    applyModeAndChip();
}

void ALKitPage::refreshChangedFilter(ALKitRow* row, const std::string& query)
{
    if (!row)
        return;
    std::string lowered = query;
    LLStringUtil::toLower(lowered);
    std::string haystack = row->meta().label + " " + row->meta().help + " " + row->meta().keywords;
    LLStringUtil::toLower(haystack);
    bool visible = row->amber();
    for (const std::string& token : LLStringUtil::getTokens(lowered, " "))
        visible = visible && haystack.find(token) != std::string::npos;
    row->setFilterVisible(visible);
    applyModeAndChip();
}

void ALKitPage::applySearch(const ALKitSearch::Hits& hits)
{
    mSearching = true;
    for (auto& p : mRows)
        p.second->setFilterVisible(false);
    for (const auto& h : hits)
        if (h.row)
            h.row->setFilterVisible(true);
    applyModeAndChip();
}

void ALKitPage::clearSearch()
{
    mSearching = false;
    for (auto& p : mRows)
        p.second->setFilterVisible(true);
    applyModeAndChip();
}

bool ALKitPage::revealRow(ALKitRow* r, bool flash_it)
{
    if (!r)
        return false;
    r->setFilterVisible(true);
    if (flash_it)
        r->flash();
    for (auto& c : mCards)
        if (std::find(c.second->rows().begin(), c.second->rows().end(), r) != c.second->rows().end())
        {
            // A card can belong to both Easy and Advanced while individual
            // rows in it are Advanced-only or disabled by Easy mode.  Card
            // membership alone therefore cannot reveal such a row (for
            // example Rig's Fill EV in the shared Lights card).
            LLUICtrl* control = r->findChild<LLUICtrl>(r->meta().widget, true);
            // An unconnected row is disabled for good, not by Easy mode: it never needs Advanced.
            const bool needs_advanced = !r->unconnected() && (r->meta().advanced || (control && !control->getEnabled()));
            if (needs_advanced)
                for (S32 i = 0; i < (S32)mModeDefs.size(); ++i)
                    if (mModeDefs[i].name == "Advanced" &&
                        std::find(mModeDefs[i].cards.begin(), mModeDefs[i].cards.end(), c.first) !=
                            mModeDefs[i].cards.end())
                    {
                        setModeIndex(i, !mSearching, mSearching);
                        break;
                    }
            c.second->revealRow(r);
            const bool revealed = revealCard(c.second, flash_it);
            if (revealed)
                scrollToView(r);
            return revealed;
        }
    return false;
}

bool ALKitPage::revealCard(ALKitCard* c, bool)
{
    if (!c)
        return false;
    const auto listed_in_mode = [c](const ModeData& mode)
    {
        return std::find(mode.cards.begin(), mode.cards.end(), c->id()) != mode.cards.end();
    };
    const bool visible_in_current = mModeIndex >= 0 && mModeIndex < (S32)mModeDefs.size() && listed_in_mode(mModeDefs[mModeIndex]);
    if (!visible_in_current)
        for (S32 i = 0; i < (S32)mModeDefs.size(); ++i)
            if (listed_in_mode(mModeDefs[i]))
            {
                // Search deliberately displays cross-mode hits without
                // rewriting mode-owned settings.  Other callers asked for an
                // actual reveal, so use the normal user transition and do not
                // return true while leaving the requested card hidden.
                setModeIndex(i, !mSearching, mSearching);
                break;
            }
    for (S32 i = 0; i < (S32)mSections.size(); ++i)
        if (std::find(mSections[i].cards.begin(), mSections[i].cards.end(), c->id()) != mSections[i].cards.end())
        {
            mChipIndex = i;
            break;
        }
    applyModeAndChip();
    scrollToView(c);
    return true;
}

void ALKitPage::setSearchText(const std::string& q)
{
    if (mSearch && mSearch->getValue().asString() != q)
        mSearch->setValue(q);
    if (q.empty())
    {
        if (ALKitShell* shell = ALKitShell::find(this); shell && shell->changedOnly())
            applyLocalFilter("", true);
        else
            clearSearch();
    }
}

void ALKitPage::setChangedOnlyValue(bool enabled)
{
    setValueIfChanged(mChangedOnly, LLSD(enabled), true);
    // A legacy page cannot filter, but a filter that is already on must stay switch-off-able.
    if (mLegacy && mChangedOnly)
        mChangedOnly->setEnabled(enabled);
}

void ALKitPage::buildResetPopover()
{
    if (mResetPopover || !mResetPage)
        return;
    const std::string pre = prefix();
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        ALKitPopover::Params popover_params;
        popover_params.name    = pre + "_reset_popover";
        popover_params.rect    = LLRect(0, 96, 320, 0);
        popover_params.visible = false;
        mResetPopover          = LLUICtrlFactory::create<ALKitPopover>(popover_params);
        shell->addChild(mResetPopover);
        mResetPopover->bindAnchor(mResetPage);

        LLTextBox::Params message_params;
        message_params.name          = pre + "_reset_message";
        message_params.rect          = LLRect(12, 84, 308, 48);
        message_params.initial_value = "Reset every control on this page to its default?";
        message_params.wrap          = true;
        mResetPopover->addChild(LLUICtrlFactory::create<LLTextBox>(message_params));

        LLButton* cancel  = makeButton(pre + "_reset_cancel", "Cancel");
        LLButton* confirm = makeButton(pre + "_reset_confirm", "Reset");
        cancel->setOrigin(140, 12);
        confirm->setOrigin(228, 12);
        mResetPopover->addChild(cancel);
        mResetPopover->addChild(confirm);
        LLHandle<ALKitPopover> popover_handle = mResetPopover->getDerivedHandle<ALKitPopover>();
        LLHandle<ALKitPage>    page_handle    = getDerivedHandle<ALKitPage>();
        cancel->setCommitCallback(
            [popover_handle](LLUICtrl*, const LLSD&)
            {
                if (ALKitPopover* popover = popover_handle.get())
                    popover->hide();
            });
        confirm->setCommitCallback(
            [page_handle, popover_handle](LLUICtrl*, const LLSD&)
            {
                if (ALKitPopover* popover = popover_handle.get())
                    popover->hide();
                if (ALKitPage* page = page_handle.get())
                    page->performPageReset();
            });
        ALKitStyle::styleTree(mResetPopover);
    }

}

void ALKitPage::resetPage()
{
    buildResetPopover();
    if (mResetPopover)
        mResetPopover->toggle();
}

void ALKitPage::performPageReset()
{
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->history().beginGesture(ALKitHistory::MOUSE, mPage, "Reset page");
    for (auto& card : mCards)
        for (ALKitRow* row : card.second->rows())
            if (row->bulkResettable() && !row->cardPreset() && !row->whoPicker())
                row->reset();
}

bool ALKitPage::cycleChip(S32 delta)
{
    if (mChips.empty())
        return false;
    S32 next = mChipIndex;
    for (S32 count = 0; count < (S32)mChips.size(); ++count)
    {
        next = (next + delta + (S32)mChips.size()) % (S32)mChips.size();
        if (mChips[next]->getVisible())
        {
            mChipIndex = next;
            applyModeAndChip();
            saveState();
            if (next < (S32)mSectionHeadings.size() && mSectionHeadings[next]->getVisible())
                scrollToView(mSectionHeadings[next]);
            return true;
        }
    }
    return false;
}

bool ALKitPage::setChipIndex(S32 index)
{
    if (index < 0 || index >= (S32)mChips.size())
        return false;
    mChipIndex = index;
    applyModeAndChip();
    return true;
}

bool ALKitPage::revealChip(S32 index)
{
    if (index < 0 || index >= (S32)mSections.size())
        return false;
    for (const std::string& id : mSections[index].cards)
    {
        auto found = mCards.find(id);
        if (found != mCards.end())
        {
            revealCard(found->second, false);
            return setChipIndex(index);
        }
    }
    return false;
}

void ALKitPage::saveState()
{
    if (mRestoringState)
        return;
    LLSD state   = gSavedSettings.getLLSD("DirectorV2PageState");
    LLSD page    = LLSD::emptyMap();
    page["chip"] = mChipIndex;
    if (mWhoPinned && mWhoCombo)
    {
        page["who_pin"] = mWhoCombo->getValue().asString();
    }
    else if (mWhoPinned && mActorTargetPage)
    {
        page["who_pin"] = mActorTarget.asString();
    }
    for (const auto& card : mCards)
    {
        page["cards"][card.first]["body"] = card.second->bodyOpen();
        page["cards"][card.first]["more"] = card.second->moreState();
    }
    state[mPage] = page;
    gSavedSettings.setLLSD("DirectorV2PageState", state);
}

void ALKitPage::setWhoPin(const LLSD& value)
{
    mWhoPinned = value.isDefined() && !value.asString().empty();
    if (mActorTargetPage && mWhoPinned)
        mActorTarget = value.asUUID();
    if (mWhoChip && mWhoCombo)
    {
        mWhoChip->setLabel(whoChipLabel(mWhoLabel, mWhoCombo, !mWhoPinned));
        reshape(getRect().getWidth(), getRect().getHeight(), false);
    }
    refreshActorTargetAvailability();
    saveState();
}

LLUUID ALKitPage::actorTarget() const
{
    return mWhoCombo ? mWhoCombo->getValue().asUUID() : mActorTarget;
}

void ALKitPage::refreshActorTargetAvailability()
{
    if (!mActorTargetPage || !mWhoChip)
        return;
    const LLUUID target = actorTarget();
    const bool explicit_you = mWhoCombo && mWhoCombo->getCurrentIndex() >= 0 && target.isNull();
    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool present = explicit_you || (target.notNull() &&
        cast.getMember(target) && cast.resolve(target));
    mActorTargetAway = (target.notNull() || explicit_you) && !present;
    if (mActorTargetAway)
        setWhoChipLabel("Actor: Away");
    else if (!mWhoCombo)
    {
        const std::string label = mWhoPinned ? "Actor: locked" :
            (target.notNull() ? "Actor: follows cast" : "Actor: none");
        setWhoChipLabel(label);
    }
    if (mLegacyHost)
        mLegacyHost->setEnabled(!mActorTargetAway && (target.notNull() || explicit_you));
}

void ALKitPage::restoreState()
{
    mRestoringState  = true;
    const LLSD state = gSavedSettings.getLLSD("DirectorV2PageState");
    mChipIndex       = 0;
    mWhoPinned       = false;
    for (auto& card : mCards)
        card.second->restoreOpenState(true, false);
    if (!state.has(mPage))
    {
        applyModeAndChip();
        mRestoringState = false;
        return;
    }
    const LLSD& page = state[mPage];
    if (page.has("chip"))
        mChipIndex = llclamp(page["chip"].asInteger(), 0, llmax(0, (S32)mSections.size() - 1));
    mWhoPinned = page.has("who_pin") && !page["who_pin"].asString().empty();
    if (mActorTargetPage && mWhoPinned)
        mActorTarget = page["who_pin"].asUUID();
    if (page.has("cards"))
        for (auto& card : mCards)
            if (page["cards"].has(card.first))
            {
                const LLSD& saved = page["cards"][card.first];
                card.second->restoreOpenState(saved["body"].asBoolean(), saved["more"]);
            }
    applyModeAndChip();
    mRestoringState = false;
}

void ALKitPage::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    const U64 subject_revision = LLDirectorCast::instance().getActorChoiceRevision();
    if (subject_revision != mSubjectComboRevision)
    {
        mSubjectComboRevision = subject_revision;
        refreshSubjectCombos(); // ui-draw-lint: allow-function(raw-write) revision-gated
    }
    if (mReadoutClock.due())
    {
        for (const auto& card : mCards)
            if (card.second->getVisible())
                for (ALKitRow* row : card.second->rows())
                    if (row->isEffectivelyVisible())
                        row->pollLiveValue();
        if (mWhoCombo && mWhoChip && !mWhoError)
        {
            const std::string label = whoChipLabel(mWhoLabel, mWhoCombo, !mWhoPinned);
            if (!whoChipHasLabel(mWhoChip, label))
            {
                setWhoChipLabel(label);
                reshape(getRect().getWidth(), getRect().getHeight(), false);
            }
        }
        refreshActorTargetAvailability();
    }
    if (ALKitShell* shell = ALKitShell::find(this))
    {
        const U64 revision = shell->history().revision();
        if (revision != mHistoryRevision)
        {
            mHistoryRevision = revision;
            if (mUndo)
                mUndo->setEnabled(shell->history().canUndo());
            if (mRedo)
                mRedo->setEnabled(shell->history().canRedo());
        }
    }
    updateMaster();
    if (updateGateStatus())
        reshape(getRect().getWidth(), getRect().getHeight(), true);
    LLPanel::draw();
}

void ALKitPage::refreshSubjectCombos()
{
    for (const SubjectCombo& spec : mSubjectCombos)
    {
        ALDirectorCastUI::ValueProvider provider;
        switch (spec.mValueSource)
        {
        case SubjectValueSource::DANCE_FOCUS:
            provider = []() -> LLSD
            {
                return LLSD(static_cast<S32>(
                    ALDanceBall::instance().autoJoinFocus()));
            };
            break;
        case SubjectValueSource::DANCE_BLOCK_CENTRE:
            provider = []() -> LLSD
            {
                return LLSD(static_cast<S32>(
                    ALDanceBall::instance().blockingCentreFocus()));
            };
            break;
        case SubjectValueSource::DANCE_WAVE_FOCUS:
            provider = []() -> LLSD
            {
                return LLSD(static_cast<S32>(
                    ALDanceBall::instance().wave().mFocus));
            };
            break;
        case SubjectValueSource::CONTROL:
            break;
        }
        ALDirectorCastUI::mergeSubjectCombo(
            spec.mCombo, spec.mSelfValue, spec.mSubjectAValue,
            spec.mSubjectCount, provider);
    }
}
