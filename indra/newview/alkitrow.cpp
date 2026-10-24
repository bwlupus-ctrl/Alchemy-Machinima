#include "llviewerprecompiledheaders.h"
#include "alkitrow.h"
#include "aldirectorcastui.h"
#include "alfloaterlightbox.h"
#include "alkitbinding.h"
#include "alkitmenu.h"
#include "alkitstyle.h"
#include "alkitshell.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "alkitcard.h"
#include "alkithistory.h"
#include "alkitpage.h"
#include "llfontgl.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llclipboard.h"
#include "llrender2dutils.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llspinctrl.h"
#include "llsliderctrl.h"
#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "llradiogroup.h"
#include "llcontrol.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "aluirefresh.h"
#include "alcompassdial.h"
#include "alkitswitch.h"
#include "alkitqol.h"
#include "alsettingsundohistory.h"
#include "llmenugl.h"
#include "llviewermenu.h"

#include <algorithm>
#include <unordered_set>
#include <vector>
static LLDefaultChildRegistry::Register<ALKitRow> r("kit_row");

namespace
{
LLSD sRowClipboard;

std::string actionPageKey(const ALKitRow* row)
{
    if (ALKitPage* page = dynamic_cast<ALKitPage*>(ALKitHistory::inputPage()))
        return page->pageKey();
    return row->meta().page_key;
}

void dispatchAction(const std::string& action)
{
    const size_t separator = action.find(':');
    const std::string prefix = action.substr(0, separator);
    const std::string target = separator == std::string::npos ? std::string() : action.substr(separator + 1);
    if (prefix == "lightbox" && !target.empty())
    {
        ALFloaterLightBox::showTab(target);
        return;
    }
    if (prefix == "floater" && !target.empty())
    {
        LLFloaterReg::showInstance(target);
        return;
    }

    static std::unordered_set<std::string> warned_prefixes;
    if (warned_prefixes.insert(prefix).second)
    {
        LL_WARNS("DirectorKit") << "Unknown kit row action prefix '" << prefix
                                 << "' in action '" << action << "'" << LL_ENDL;
    }
}

class ALKitProxyBinding final : public ALKitBinding
{
public:
    ALKitProxyBinding(ALKitRow* source, bool writable) :
        mSource(source->getDerivedHandle<ALKitRow>()), mWritable(writable) {}
    LLSD get() const override { return mSource.get() ? mSource.get()->boundValue() : LLSD(); }
    LLSD getDefault() const override { return mSource.get() ? mSource.get()->boundDefault() : LLSD(); }
    void set(const LLSD& value) override { if (mWritable && mSource.get()) mSource.get()->setBoundValue(value); }
    void reset() override { if (mWritable && mSource.get()) mSource.get()->reset(); }
    bool canSet() const override { return mWritable; }
    bool hasDefault() const override { return mWritable && mSource.get() && mSource.get()->boundDefault().isDefined(); }
    boost::signals2::connection connectChanged(std::function<void()> callback) override
    { return mSource.get() ? mSource.get()->setValueCallback(std::move(callback)) : boost::signals2::connection(); }
    std::string describe() const override { return "Quick proxy"; }
private:
    LLHandle<ALKitRow> mSource;
    bool mWritable = false;
};

class ALKitHeaderBinding final : public ALKitBinding
{
public:
    ALKitHeaderBinding(ALKitSwitch* source, const std::string& setting) :
        mSource(source->getDerivedHandle<ALKitSwitch>()), mSetting(setting) {}
    LLSD get() const override { return mSource.get() ? mSource.get()->getValue() : LLSD(); }
    LLSD getDefault() const override
    {
        LLControlVariable* control = mSetting.empty() ? nullptr : gSavedSettings.getControl(mSetting).get();
        return control ? control->getDefault() : LLSD();
    }
    bool hasDefault() const override { return getDefault().isDefined(); }
    bool isDefault(S32 decimals) const override { return !hasDefault() || ALKitBinding::isDefault(decimals); }
    bool canSet() const override { return mSource.get() && mSource.get()->isInEnabledChain(); }
    void set(const LLSD& value) override
    {
        if (canSet())
        {
            // Exactly the source widget's normal path: one commit, no parallel setting binding.
            // LLCheckBoxCtrl binds its child button. setValue()+onCommit()
            // alone would change only its view model, leaving the setting stale.
            if (mSource.get()->isOn() != value.asBoolean())
                mSource.get()->toggle();
            mSource.get()->onCommit();
        }
    }
    boost::signals2::connection connectChanged(std::function<void()> callback) override
    {
        return mSource.get() ? mSource.get()->setFlipCallback([callback](bool) { callback(); })
                             : boost::signals2::connection();
    }
    std::string describe() const override { return "Quick header switch"; }
private:
    LLHandle<ALKitSwitch> mSource;
    std::string mSetting;
};

bool baselineEditActive(const LLView* control)
{
    if (!control)
        return false;
    if (gFocusMgr.childHasMouseCapture(control))
        return true;
    LLView* focus = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
    if (!focus || (focus != control && !focus->hasAncestor(control)))
        return false;
    return dynamic_cast<LLLineEditor*>(focus) || dynamic_cast<LLTextEditor*>(focus);
}

bool pointInLineEditor(LLView* view, S32 x, S32 y)
{
    if (!view || !view->getVisible())
        return false;
    if (dynamic_cast<LLLineEditor*>(view))
        return view->pointInView(x, y);
    for (LLView* child : *view->getChildList())
    {
        S32 child_x = 0;
        S32 child_y = 0;
        view->localPointToOtherView(x, y, &child_x, &child_y, child);
        if (pointInLineEditor(child, child_x, child_y))
            return true;
    }
    return false;
}
} // namespace

ALKitRow::~ALKitRow()
{
    if (LLContextMenu* menu = mContextMenu.get())
    {
        menu->die();
        mContextMenu.markDead();
    }
}

ALKitRow::Params::Params() :
    kind("kind"),
    widget("widget", ""),
    label("label", ""),
    label_name("label_name", ""),
    help("help", ""),
    keywords("keywords", ""),
    unit("unit", ""),
    sub("sub", ""),
    setting("setting", ""),
    bind("bind", ""),
    reset_settings("reset_settings", ""),
    default_value("default_value", ""),
    link_to("link_to", ""),
    action("action", ""),
    mirror_setting("mirror_setting", ""),
    undo_scope("undo_scope", ""),
    advanced("advanced", false),
    card_preset("card_preset", false),
    who_picker("who_picker", false),
    hidden("hidden", false),
    added("added", false),
    widget_history("widget_history", true),
    bulk_reset("bulk_reset", true),
    unconnected("unconnected", false),
    fill_width("fill_width", false),
    action_row("action_row", false),
    label_wrap("label_wrap", false),
    row_height("row_height", 28),
    decimals("decimals", 3),
    min_val("min_val", 0.f),
    max_val("max_val", 0.f),
    increment("increment", 0.f)
{
}

ALKitRow::ALKitRow(const Params& p) :
    LLPanel(p),
    mBind(p.bind),
    mResetSettings(p.reset_settings),
    mDefaultValue(p.default_value),
    mLabelName(p.label_name),
    mRowHeight(p.row_height),
    mExplicitRowHeight(p.row_height.isProvided()),
    mActionRow(p.action_row),
    mCardPreset(p.card_preset),
    mWhoPicker(p.who_picker),
    mHidden(p.hidden),
    mAdded(p.added),
    mWidgetHistory(p.widget_history),
    mBulkReset(p.bulk_reset),
    mUnconnected(p.unconnected),
    mFillWidth(p.fill_width),
    mLabelWrap(p.label_wrap)
{
    ALKitStyle::color(ALKitStyle::WINDOW_BG);
    mMeta.kind           = p.kind;
    mMeta.widget         = p.widget;
    mMeta.label          = p.label;
    mMeta.help           = p.help;
    mMeta.keywords       = p.keywords;
    mMeta.unit           = p.unit;
    mMeta.sub            = p.sub;
    mMeta.setting        = p.setting;
    mMeta.link_to        = p.link_to;
    mMeta.action         = p.action;
    mMeta.mirror_setting = p.mirror_setting;
    mMeta.default_value  = p.default_value;
    mMeta.advanced       = p.advanced;
    mMeta.decimals       = p.decimals.isProvided() ? p.decimals : (mMeta.kind == "dial" || mMeta.kind == "arc" ? 0 : 3);
    mMeta.min_val        = p.min_val;
    mMeta.max_val        = p.max_val;
    mMeta.increment      = p.increment;
    mMeta.has_min        = p.min_val.isProvided();
    mMeta.has_max        = p.max_val.isProvided();
}

bool ALKitRow::postBuild()
{
    if (!LLPanel::postBuild())
        return false;

    mControl = findChild<LLUICtrl>(mMeta.widget);
    if (!mControl && mMeta.kind == "link")
    {
        LLButton::Params p;
        p.name      = mMeta.widget;
        p.rect      = LLRect(0, 22, 100, 0);
        p.label     = mMeta.label + " \xE2\x80\xBA";
        LLButton* b = LLUICtrlFactory::create<LLButton>(p);
        b->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                if (ALKitShell* s = ALKitShell::find(this))
                    s->navigateTo(mMeta.link_to);
            });
        addChild(b);
        mControl = b;
    }

    if (!mControl && (mMeta.kind == "vec2" || mMeta.kind == "vec3"))
    {
        mVecCount          = mMeta.kind == "vec3" ? 3 : 2;
        const char* axis[] = { "_x", "_y", "_z" };
        LLControlVariable* setting = gSavedSettings.getControl(mMeta.setting).get(); // isType() is non-const
        const bool color_components = setting && setting->isType(TYPE_COL3);
        const char* component_labels[] = { "R", "G", "B" };
        for (S32 i = 0; i < mVecCount; ++i)
        {
            LLSpinCtrl::Params p;
            p.name           = mMeta.widget + axis[i];
            p.rect           = LLRect(0, 22, 80, 0);
            if (color_components)
            {
                p.label       = component_labels[i];
                p.label_width = 12;
            }
            else
            {
                p.label_width = 0;
            }
            p.decimal_digits = mMeta.decimals;
            if (mMeta.increment > 0.f)
                p.increment = mMeta.increment;
            if (mMeta.has_min)
                p.min_value = mMeta.min_val;
            if (mMeta.has_max)
                p.max_value = mMeta.max_val;
            LLSpinCtrl* s = LLUICtrlFactory::create<LLSpinCtrl>(p);
            addChild(s);
            mVec[i] = s;
        }
        mControl = mVec[0];
    }

    if (mActionRow && mControl)
    {
        for (LLView* child : *mControl->getChildList())
            if (auto* button = dynamic_cast<LLButton*>(child))
                mActions.emplace_back(button, button->getRect().getWidth());
        std::sort(mActions.begin(), mActions.end(), [](const auto& a, const auto& b)
            { return a.first->getRect().mLeft < b.first->getRect().mLeft; });
    }

    LLTextBox::Params label;

    label.name = mLabelName.empty() ? mMeta.widget + "_label" : mLabelName;

    label.rect = LLRect(0, ALKitStyle::ROW_H, ALKitStyle::LABEL_W, 0);

    label.initial_value = mMeta.label;

    label.text_color = ALKitStyle::color(ALKitStyle::LABEL);

    label.font = ALKitStyle::sansFont("Kit12");

    mLabel = LLUICtrlFactory::create<LLTextBox>(label);
    addChild(mLabel);

    if (mControl)
    {
        auto on_right_click = [this](LLUICtrl* control, S32 x, S32 y, MASK)
        {
            // Let an editor own clicks in its text field so its standard
            // Cut/Copy/Paste menu is the only menu shown there.
            if (pointInLineEditor(control, x, y))
                return;
            S32 row_x = 0;
            S32 row_y = 0;
            control->localPointToOtherView(x, y, &row_x, &row_y, this);
            showContextMenu(row_x, row_y);
        };
        // Vector rows: every component (Y/Z too) opens the row menu, not just X.
        if (mVecCount)
        {
            for (S32 i = 0; i < mVecCount; ++i)
                mVec[i]->setRightMouseDownCallback(on_right_click);
        }
        else
        {
            mControl->setRightMouseDownCallback(on_right_click);
        }
        mLastValue = mShownValue = mControl->getValue();

        if (!mMeta.action.empty())
        {
            mActionConnection = mControl->setCommitCallback(
                [action = mMeta.action](LLUICtrl*, const LLSD&) { dispatchAction(action); });
        }

        if (mWidgetHistory && mBind == "code" && mMeta.kind != "button" &&
            mMeta.kind != "readout" && mMeta.mirror_setting.empty())
            mHistoryConnection = mControl->setCommitCallback(
                [this](LLUICtrl* c, const LLSD&)
                {
                    LLSD now = c->getValue();
                    if (ALKitShell* shell = ALKitShell::find(this))
                        shell->history().recordWidget(c, mLastValue, now, mMeta.label);
                    mLastValue = now;
                    bindingChanged();
                });

        // History must precede the binding's commit observer so it sees the
        // pre-commit baseline for code-bound/default-value rows.
        resolveBinding();

        mFocusConnection = mControl->setFocusReceivedCallback(
            [this](LLFocusableElement*)
            {
                mHovered = true;
                styleValueEditor(true);
                if (ALKitShell* shell = ALKitShell::find(this))
                    shell->help().showRow(this);
            });
        mFocusLostConnection = mControl->setFocusLostCallback(
            [this](LLFocusableElement*)
            {
                mHovered = mPointerInside;
                styleValueEditor(mHovered);
            });

        if (!mMeta.mirror_setting.empty())
            if (LLControlVariable* c = gSavedSettings.getControl(mMeta.mirror_setting).get())
            {
                LLComboBox* combo = dynamic_cast<LLComboBox*>(mControl);
                if (combo)
                {
                    combo->setCurrentByIndex(c->get().asInteger());
                    mMirrorCommitConnection = combo->setCommitCallback(
                        [this, c, combo](LLUICtrl*, const LLSD&)
                        {
                            if (mSyncingMirror)
                                return;
                            const S32 index = combo->getCurrentIndex();
                            if (index >= 0 && c->get().asInteger() != index)
                                c->set(LLSD(index));
                        });
                }
                mMirrorConnection = c->getSignal()->connect(
                    [this](LLControlVariable*, const LLSD& v, const LLSD&)
                    {
                        LLComboBox* combo = dynamic_cast<LLComboBox*>(mControl);
                        if (!combo || mSyncingMirror || combo->getCurrentIndex() == v.asInteger())
                            return;
                        ALKitHistory::ScopedSuspend suspend;
                        mSyncingMirror = true;
                        combo->setCurrentByIndex(v.asInteger());
                        combo->onCommit();
                        mSyncingMirror = false;
                    });
                LLHandle<LLPanel> handle = getHandle();
                doOnIdleOneTime(
                    [handle, c]()
                    {
                        ALKitRow*   row   = dynamic_cast<ALKitRow*>(handle.get());
                        LLComboBox* combo = row ? dynamic_cast<LLComboBox*>(row->mControl) : nullptr;
                        if (!row || !combo || row->mSyncingMirror)
                            return;
                        ALKitHistory::ScopedSuspend suspend;
                        row->mSyncingMirror = true;
                        combo->setCurrentByIndex(c->get().asInteger());
                        combo->onCommit();
                        row->mSyncingMirror = false;
                    });
            }
    }

    if (mMeta.kind == "segmented" && mControl)
    {
        std::vector<LLView*> segments(mControl->getChildList()->begin(),
                                      mControl->getChildList()->end());
        std::sort(segments.begin(), segments.end(),
            [](const LLView* left, const LLView* right)
            {
                return left->getRect().mLeft < right->getRect().mLeft;
            });
        for (LLView* segment : segments)
        {
            if (LLCheckBoxCtrl* item = dynamic_cast<LLCheckBoxCtrl*>(segment))
                mSegmentLabels.push_back(item->getLabel());
            segment->setVisible(false);
        }
        // The native radio buttons are hidden; the painted group owns the
        // single tab stop and keeps LLRadioGroup's arrow-key selection.
        mControl->setTabStop(true);
        mControl->setMouseOpaque(false);
    }

    if (mMeta.kind == "toggle" || mMeta.kind == "dial" || mMeta.kind == "arc")
    {
        LLTextBox::Params vp;
        vp.name       = mMeta.widget + (mMeta.kind == "toggle" ? "_state" : "_value");
        vp.rect       = LLRect(0, 20, ALKitStyle::VALUE_W, 0);
        vp.font       = ALKitStyle::monoFont();
        vp.text_color = ALKitStyle::color(ALKitStyle::TEXT);
        mValueText    = LLUICtrlFactory::create<LLTextBox>(vp);
        addChild(mValueText);
    }

    if (ALKitSwitch* sw = dynamic_cast<ALKitSwitch*>(mControl))
        mValueConnection = sw->setFlipCallback([this](bool) { refreshValue(); });

    else if (dynamic_cast<ALCompassDial*>(mControl))
        mValueConnection = mControl->setCommitCallback([this](LLUICtrl*, const LLSD&) { refreshValue(); });

    refreshValue();

    LLButton::Params favorite;
    favorite.name  = mMeta.widget + "_favorite";
    favorite.rect  = LLRect(0, 18, 18, 0);
    favorite.label = "\xE2\x98\x86";
    favorite.pad_left = 1;
    favorite.pad_right = 1;
    mFavorite      = LLUICtrlFactory::create<LLButton>(favorite);
    mFavorite->setToolTip(LLStringExplicit("Add to Quick"));
    mFavorite->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                if (ALKitShell* shell = ALKitShell::find(this))
                {
                    const ALKitQol::Favorite key = favoriteKey();
                    shell->toggleFavorite(key.page, key.widget);
                }
                refreshFavorite();
            });
    addChild(mFavorite);
    refreshFavorite();

    if (mBinding && mBinding->hasDefault() && !mReset)
    {
        LLButton::Params reset;
        reset.name  = mMeta.widget + "_reset";
        reset.rect  = LLRect(0, 20, 20, 0);
        reset.label = "";

        mReset = LLUICtrlFactory::create<LLButton>(reset);
        mReset->setImageOverlay("KitIcon_reset_row_16");
        mReset->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                this->reset();
            });
        addChild(mReset);
        bindingChanged();
    }

    layoutRow(getRect().getWidth());
    styleValueEditor(false);
    return true;
}

S32 ALKitRow::heightForWidth(S32 width) const
{
    if (mFillWidth && mMeta.kind == "custom")
        if (auto* item = dynamic_cast<ALFlowGridItem*>(mControl))
            return item->heightForWidth(width);
    if (mActionRow && mControl)
    {
        S32 left = 0, lines = 1;
        const S32 available = llmax(1, width - 2 * ALKitStyle::ROW_INSET);
        for (const auto& action : mActions)
        {
            const S32 button_width = llmin(available, action.second);
            if (left && left + button_width > available) { ++lines; left = 0; }
            left += button_width + 6;
        }
        return llmax(mRowHeight, lines * 28);
    }
    if (mMeta.kind == "readout")
        if (LLTextBox* text = dynamic_cast<LLTextBox*>(mControl); text && text->getWordWrap())
        {
            // Reflow at the same value-track width used by layoutRow before measuring.
            const S32 text_width = width - 2 * ALKitStyle::ROW_INSET - (mMeta.label.empty() ? 0 :
                ALKitStyle::LABEL_W + ALKitStyle::RESET_W + 18 + 3 * ALKitStyle::COL_GAP);
            text->reshape(llmax(1, text_width), text->getRect().getHeight(), true);
            return llmax(llmax(28, mRowHeight), text->getTextPixelHeight() + 8);
        }
    if (mMeta.kind == "dial" || mMeta.kind == "arc")
        return 56;
    if (mMeta.kind == "gobo")
        return 72;
    if (mMeta.kind == "list")
        return mRowHeight > 28 ? mRowHeight : 120;
    if (mMeta.kind == "viewport")
        return (mRowHeight > 28 ? mRowHeight : 180) + 6;
    return llmax(28, mRowHeight);
}

// A label wider than its box is elided with an ellipsis; the full text moves to the tooltip.
void ALKitRow::fitLabel(S32 box_width)
{
    if (!mLabel)
        return;
    mLabel->setWordWrap(mLabelWrap);
    if (mLabelWrap)
    { setTextIfChanged(mLabel, mMeta.label); setToolTipIfChanged(mLabel, mMeta.label); return; }
    const std::string shown = ALKitStyle::fitText(ALKitStyle::sansFont("Kit12"), mMeta.label, box_width);
    setTextIfChanged(mLabel, shown);
    setToolTipIfChanged(mLabel, shown == mMeta.label ? std::string() : mMeta.label);
}

void ALKitRow::layoutRow(S32 w)
{
    if (mFillWidth && mMeta.kind == "custom")
        if (auto* item = dynamic_cast<ALFlowGridItem*>(mControl))
        {
            const S32 height = item->heightForWidth(w);
            reshape(w, height, true);
            if (mLabel) mLabel->setVisible(false);
            if (mFavorite) mFavorite->setVisible(false);
            if (mReset) mReset->setVisible(false);
            mControl->reshape(w, height, true);
            mControl->setOrigin(0, 0);
            return;
        }
    // The favorite star sits between the value column and the reset glyph, so it takes
    // track width and never narrows the label track.
    constexpr S32 kFavoriteW = 18;
    const S32 h = heightForWidth(w);
    const S32 value_width = mFillWidth && mMeta.kind == "toggle" ? 24 : ALKitStyle::VALUE_W;
    // A full-width toggle spends its otherwise empty track on its label, as
    // authored in the cache-lifetime mockup. Other rows keep the 100 px track.
    const S32 label_width = mFillWidth && mMeta.kind == "toggle"
        ? llmax(ALKitStyle::LABEL_W, w - 2 * ALKitStyle::ROW_INSET - ALKitStyle::SWITCH_W -
            value_width - ALKitStyle::RESET_W - kFavoriteW - 4 * ALKitStyle::COL_GAP)
        : ALKitStyle::LABEL_W;
    reshape(w, h, true);
    if (mLabel)
    {
        const bool full_span_custom = (mMeta.kind == "custom" || mMeta.kind == "readout" || mMeta.kind == "button") && mMeta.label.empty();
        // The label box is the mockup's 100 px label track; the favorite star has its own slot (below).
        if (mLabelWrap)
            mLabel->reshape(label_width, llmax(20, h - 8), true);
        else if (mFillWidth && mMeta.kind == "toggle")
            mLabel->reshape(label_width, 20, true);
        else
            mLabel->reshape(ALKitStyle::LABEL_W, 20, true);
        const auto* text = dynamic_cast<LLTextBox*>(mControl);
        const bool top_aligned = mMeta.kind == "readout" && text && text->getWordWrap();
        mLabel->setOrigin(ALKitStyle::ROW_INSET, mLabelWrap ? 4 : top_aligned ? h - 24 : (h - 20) / 2);
        mLabel->setVisible(!full_span_custom);
        fitLabel(label_width);
    }

    const bool full_span_custom = (mMeta.kind == "custom" || mMeta.kind == "readout" || mMeta.kind == "button") && mMeta.label.empty();
    const S32 x = full_span_custom ? ALKitStyle::ROW_INSET
                                   : ALKitStyle::ROW_INSET + label_width + ALKitStyle::COL_GAP;
    const S32 cw =
        full_span_custom
            ? llmax(1, w - 2 * ALKitStyle::ROW_INSET)
            : llmax(1, w - 2 * ALKitStyle::ROW_INSET - label_width - value_width - ALKitStyle::RESET_W - kFavoriteW - 4 * ALKitStyle::COL_GAP);
    const S32 span = full_span_custom ? cw : cw + ALKitStyle::COL_GAP + ALKitStyle::VALUE_W;

    if (mVecCount)
    {
        const S32 each = llmax(1, (span - (mVecCount - 1) * ALKitStyle::COL_GAP) / mVecCount);
        for (S32 i = 0; i < mVecCount; ++i)
        {
            mVec[i]->reshape(each, 22, true);
            mVec[i]->setOrigin(x + i * (each + ALKitStyle::COL_GAP), (h - 22) / 2);
        }
    }

    else if (mControl)
    {
        S32 width = cw, height = 24;
        if (mMeta.kind == "toggle")
        {
            width  = ALKitStyle::SWITCH_W;
            height = ALKitStyle::SWITCH_H;
        }
        else if (mMeta.kind == "dial" || mMeta.kind == "arc")
        {
            width = height = ALKitStyle::DIAL_SIZE;
        }
        else if (mMeta.kind == "number")
            width = ALKitStyle::VALUE_W;
        else if (mMeta.kind == "color")
        {
            width  = 40;
            height = 18;
        }
        else if (mMeta.kind == "gobo")
        {
            width = height = 64;
        }
        else if (mMeta.kind == "list")
        {
            width  = span;
            height = h;
        }
        else if (mMeta.kind == "viewport")
        {
            width  = span;
            height = llmax(1, h - 6);
        }
        else if (mMeta.kind == "button")
        {
            const LLFontGL* f = ALKitStyle::sansFont("Kit12");
            const LLButton* button = dynamic_cast<LLButton*>(mControl);
            const std::string label = button ? button->getText() : mMeta.label;
            // fill_width: code rewrites the label (slot buttons), so size to the track, not the
            // placeholder text the XML started with.
            width = mFillWidth ? span : llmin(span, (f ? f->getWidth(label) : 80) + 20);
        }
        else if (mMeta.kind == "slider" || mMeta.kind == "combo" || mMeta.kind == "text" || mMeta.kind == "readout" ||
                 mMeta.kind == "link" || mMeta.kind == "custom" || mMeta.kind == "segmented")
            width = span;
        if (mMeta.kind == "readout")
            if (LLTextBox* text = dynamic_cast<LLTextBox*>(mControl); mExplicitRowHeight || (text && text->getWordWrap()))
                height = h - 8;
        if (mExplicitRowHeight && (mMeta.kind == "custom" || mMeta.kind == "list" || mMeta.kind == "viewport"))
            height = h;
        if (mActionRow) height = h - 4;
        if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(mControl); spinner && mMeta.kind == "number")
            spinner->setValueColumnLayout(width, height, true);
        else
            mControl->reshape(width, height, true);
        if (mMeta.kind == "segmented")
        {
            std::vector<LLView*> segments(mControl->getChildList()->begin(),
                                          mControl->getChildList()->end());
            std::sort(segments.begin(), segments.end(),
                [](const LLView* left, const LLView* right)
                {
                    return left->getRect().mLeft < right->getRect().mLeft;
                });
            const S32 count = static_cast<S32>(segments.size());
            const S32 each = llmax(1, width / llmax(1, count));
            S32 index = 0;
            for (LLView* segment : segments)
            {
                segment->reshape(index + 1 == count ? width - index * each : each, height, true);
                segment->setOrigin(index * each, 0);
                ++index;
            }
        }
        const S32 control_x = mMeta.kind == "number" ? x + cw + ALKitStyle::COL_GAP : x;
        mControl->setOrigin(control_x, (h - height) / 2);
        if (mActionRow)
        {
            // Geometry-less hosted panels cannot rely on XUI's initial parent
            // rect/follows translation. Own the nested button coordinates too.
            S32 left = 0, top = height;
            for (const auto& action : mActions)
            {
                const S32 button_width = llmin(width, action.second);
                if (left && left + button_width > width) { left = 0; top -= 28; }
                action.first->reshape(button_width, 24, true);
                action.first->setOrigin(left, top - 24);
                left += button_width + 6;
            }
        }

        if (mMeta.kind == "slider")
        {
            for (LLView* child : *mControl->getChildList())
            {
                if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                    editor->setRect(LLRect(width - ALKitStyle::VALUE_W, (height + 20) / 2,
                                           width, (height - 20) / 2));
                else if (LLTextBox* text = dynamic_cast<LLTextBox*>(child))
                {
                    text->setRect(LLRect(width - ALKitStyle::VALUE_W, (height + 20) / 2,
                                         width, (height - 20) / 2));
                    text->setRightAlign();
                    text->setFont(ALKitStyle::monoFont());
                }
            }
            // Let LLSliderCtrl recompute its track after the value child has
            // been widened to the shared value-column width.
            mControl->reshape(width, height, true);
        }
    }

    if (mValueText)
    {
        mValueText->reshape(value_width, 20, true);
        mValueText->setOrigin(x + cw + ALKitStyle::COL_GAP, (h - 20) / 2);
    }
    if (mReset)
    {
        mReset->reshape(ALKitStyle::RESET_W, 20, true);
        mReset->setOrigin(w - ALKitStyle::ROW_INSET - ALKitStyle::RESET_W, (h - 20) / 2);
    }
    if (mFavorite)
    {
        mFavorite->reshape(kFavoriteW, 18, true);
        mFavorite->setOrigin(full_span_custom ? ALKitStyle::ROW_INSET : x + cw + 2 * ALKitStyle::COL_GAP + value_width,
                             (h - 18) / 2);
    }
}

void ALKitRow::layoutDialCell(S32 w)
{
    reshape(w, ALKitStyle::DIAL_CELL_H, true);
    if (mLabel)
    {
        mLabel->reshape(llmax(1, w - ALKitStyle::RESET_W - ALKitStyle::COL_GAP), 20, true);
        mLabel->setOrigin(0, ALKitStyle::DIAL_CELL_H - 20);
        fitLabel(llmax(1, w - ALKitStyle::RESET_W - ALKitStyle::COL_GAP));
    }
    if (mReset)
    {
        mReset->reshape(ALKitStyle::RESET_W, 20, true);
        mReset->setOrigin(w - ALKitStyle::RESET_W, ALKitStyle::DIAL_CELL_H - 20);
    }
    if (mControl)
    {
        mControl->reshape(ALKitStyle::DIAL_SIZE, ALKitStyle::DIAL_SIZE, true);
        mControl->setOrigin(0, 6);
    }
    if (mValueText)
    {
        mValueText->reshape(llmax(1, w - ALKitStyle::DIAL_SIZE - ALKitStyle::COL_GAP), 20, true);
        mValueText->setOrigin(ALKitStyle::DIAL_SIZE + ALKitStyle::COL_GAP, 20);
    }
}

ALKitCard* ALKitRow::card() const
{
    if (mCard)
        return mCard;
    for (LLView* p = getParent(); p; p = p->getParent())
        if (ALKitCard* c = dynamic_cast<ALKitCard*>(p))
        {
            const_cast<ALKitRow*>(this)->mCard = c;
            return c;
        }
    return nullptr;
}

bool ALKitRow::isEffectivelyVisible() const
{
    ALKitCard* owner           = card();
    const bool composite       = mMeta.kind == "link" || mMeta.kind == "vec2" || mMeta.kind == "vec3";
    const bool control_visible = !mControl || mControl->getVisible() || composite;
    return !mHidden && !mWhoPicker && mFilterVisible && !mCodeHidden && control_visible && (!mMeta.advanced || !owner || owner->moreOpen());
}

bool ALKitRow::setControlVisible(bool visible)
{
    if (mCodeHidden == !visible)
        return false;
    mCodeHidden = !visible;
    if (ALKitCard* owner = card())
        owner->requestLayout();
    return true;
}

bool ALKitRow::setControlVisible(LLView* c, bool v)
{
    if (!c)
        return false;
    for (LLView* p = c->getParent(); p; p = p->getParent())
        if (ALKitRow* row = dynamic_cast<ALKitRow*>(p))
        {
            if (row->mControl == c)
            {
                const bool changed = row->setControlVisible(v);
                c->setVisible(v);
                return changed;
            }
        }
    if (c->getVisible() == v)
        return false;
    c->setVisible(v);
    return true;
}

void ALKitRow::refreshValue()
{
    if (!mControl)
        return;
    const LLSD value = mControl->getValue();
    if (mValueText)
    {
        if (mMeta.kind == "toggle")
            setTextIfChanged(mValueText, value.asBoolean() ? "On" : "Off");
        else
            setTextIfChanged(mValueText, llformat("%.*f\xC2\xB0", mMeta.decimals, value.asReal()));
    }
    mShownValue = value;
}

void ALKitRow::pollLiveValue()
{
    if (!mControl)
        return;
    if (mHeaderProxy)
    {
        const bool enabled = enabledForEdit();
        if (mControl->getEnabled() != enabled)
            mControl->setEnabled(enabled);
        if (mBinding && !alSameValue(mControl->getValue(), mBinding->get()))
            bindingChanged();
    }
    const LLSD value = mControl->getValue();
    if (!alSameValue(value, mShownValue))
    {
        LLTextBox* text = mMeta.kind == "readout" ? dynamic_cast<LLTextBox*>(mControl) : nullptr;
        const bool wrapped = text && text->getWordWrap();
        // Live bindings have already changed the text; retain the last laid-out height.
        const S32 previous_height = getRect().getHeight();
        const S32 width = getRect().getWidth();
        refreshValue();
        if (wrapped)
        {
            if (heightForWidth(width) != previous_height)
            {
                // Resize and move sibling rows together in the card's idle
                // layout. Growing this row alone temporarily paints over them.
                if (ALKitCard* owner = card()) owner->requestLayout();
            }
            else layoutRow(width);
        }
    }
    if (!baselineEditActive(mControl))
        mLastValue = value;
}

void ALKitRow::bindingChanged()
{
    // Vec rows: ALKitVecBinding::syncControls already pushed each component into
    // its own spinner; mControl is only the X spinner and must never get the array
    // (an array's asReal() is 0, which would then be committed as X).
    // A code-bound row may use reset_settings only to own a multi-setting reset.
    // Its list binding returns an LLSD map, which is not the widget's value and
    // must not be pushed into the control (the page binder owns that sync).
    if (mBinding && mControl && !mVecCount && (mBind != "code" || mResetSettings.empty()) &&
        !baselineEditActive(mControl))
    {
        const LLSD value = mBinding->get();
        if (!alSameValue(value, mControl->getValue()))
        {
            if (mSubjectDisplay)
                ALDirectorCastUI::showStoredValue(
                    dynamic_cast<LLComboBox*>(mControl), value,
                    mSubjectSelfValue, mSubjectAValue, mSubjectCount);
            else
                mControl->setValue(value);
        }
    }
    const bool now = amber();
    if (mReset)
    {
        mReset->setImageOverlay("KitIcon_reset_row_16", LLFontGL::HCENTER);
        // Live colour-table reference, so the icon follows colour theme changes.
        mReset->setImageOverlayColor(now ? ALKitStyle::color(ALKitStyle::ACCENT) : ALKitStyle::color(ALKitStyle::FAINT));
    }
    if (now != mAmber)
    {
        mAmber = now;
        mAmberSignal(now);
    }
    refreshValue();
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->refreshChangedMarkers(this);
    if (mControl && !baselineEditActive(mControl))
        mLastValue = mControl->getValue();
    mValueSignal();
}

void ALKitRow::setSubjectDisplayValues(S32 self_value, S32 subject_a_value,
                                       S32 subject_count)
{
    mSubjectDisplay = true;
    mSubjectSelfValue = self_value;
    mSubjectAValue = subject_a_value;
    mSubjectCount = subject_count;
}

void ALKitRow::resolveBinding()
{
    if (mBinding || !mControl)
        return;
    if (mVecCount)
        mBinding = std::make_unique<ALKitVecBinding>(mMeta.setting, mVec[0], mVec[1], mVec[2]);
    else if (mBind == "setting" && !mMeta.setting.empty())
        mBinding = std::make_unique<ALKitSettingBinding>(mMeta.setting);
    else if (!mResetSettings.empty())
        mBinding = std::make_unique<ALKitSettingsListBinding>(LLStringUtil::getTokens(mResetSettings, ","));
    else
    {
        LLView* root = getRootView();
        for (LLView* v = getParent(); v; v = v->getParent())
            if (dynamic_cast<ALKitPage*>(v))
            {
                root = v;
                break;
            }
        mBinding = ALKitBindingRegistry::instance().make(root, mMeta.widget);
    }
    if (!mBinding && !mDefaultValue.empty())
        mBinding = std::make_unique<ALKitWidgetBinding>(mControl, LLSD(mDefaultValue));
    if (mBinding)
    {
        mBindingConnection = mBinding->connectChanged([this]() { bindingChanged(); });
        if (mBinding->hasDefault() && !mReset)
        {
            LLButton::Params reset;
            reset.name  = mMeta.widget + "_reset";
            reset.rect  = LLRect(0, 20, 20, 0);
            reset.label = "";
            mReset      = LLUICtrlFactory::create<LLButton>(reset);
            mReset->setImageOverlay("KitIcon_reset_row_16");
            mReset->setCommitCallback(
                [this](LLUICtrl*, const LLSD&)
                {
                    this->reset();
                });
            addChild(mReset);
        }
        bindingChanged();
        // The card's reset-icon visibility (can_reset in ALKitCard::layoutBody) depends on which rows are
        // resettable. A binding that arrives after the first layout (registry rebind via
        // ALKitPage::rebindRows) must relayout the card; requestLayout() is coalesced to one idle pass.
        if (ALKitCard* owner = card())
            owner->requestLayout();
    }
}

void ALKitRow::reset()
{
    ALKitHistory::ScopedSource source(this);
    // Every entry point (glyph, Home, context menu and bulk reset) respects live availability.
    if (!enabledForEdit() || mUnconnected || !mBinding)
        return;
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->history().ensureGesture(actionPageKey(this), "Reset " + mMeta.label);
    mBinding->reset();
    commitCardPreset();
    bindingChanged();
}

bool ALKitRow::amber() const
{ return mBinding && !mBinding->isDefault(mMeta.decimals); }

LLSD ALKitRow::boundValue() const
{ return mBinding ? mBinding->get() : (mControl ? mControl->getValue() : LLSD()); }

LLSD ALKitRow::boundDefault() const
{ return mBinding ? mBinding->getDefault() : LLSD(); }

bool ALKitRow::setBoundValue(const LLSD& value)
{
    ALKitHistory::ScopedSource source(this);
    if (mUnconnected || !enabledForEdit())
        return false;
    if ((mMeta.kind == "text" || dynamic_cast<LLLineEditor*>(mControl)) && value.isString())
    {
        if (!mControl || (mBinding && !mBinding->canSet())) return false;
        if (ALKitRow* original = sourceRow())
        {
            const bool applied = original->setBoundValue(value);
            bindingChanged();
            return applied;
        }
        // Text adapters can expose a non-string bound value. Let the editor's
        // ordinary commit validate/convert text before any source/setting write.
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->history().ensureGesture(actionPageKey(this), mMeta.label);
        mControl->setValue(value);
        mControl->onCommit();
        bindingChanged();
        return true;
    }
    const LLSD current = boundValue();
    LLSD       coerced = ALKitQol::coercePasteValue(value, current);
    if (!coerced.isDefined() || (mBinding && !mBinding->canSet()))
        return false;
    // Paste (and Quick proxy commits) must stay inside the row's range. The scalar range
    // applies per component to vectors and colours (Vector2/3, Color3/4 are numeric arrays).
    bool  has_min = mMeta.has_min;
    bool  has_max = mMeta.has_max;
    F64   low     = mMeta.min_val;
    F64   high    = mMeta.max_val;
    if (!has_min && !has_max)
        if (LLSliderCtrl* ranged = dynamic_cast<LLSliderCtrl*>(mControl)) // sliders always declare a range
        {
            has_min = has_max = true;
            low               = ranged->getMinValue();
            high              = ranged->getMaxValue();
        }
    // A colour with no declared minimum never goes negative (HDR above 1 stays allowed).
    LLControlVariable* colour_setting = (mVecCount == 3 && !mMeta.setting.empty())
        ? gSavedSettings.getControl(mMeta.setting).get() : nullptr;
    const bool colour_row = mMeta.kind == "color" || (colour_setting && colour_setting->isType(TYPE_COL3));
    if (colour_row && !has_min)
    {
        has_min = true;
        low     = 0.0;
    }
    auto clamp_number = [&](const LLSD& number_value, bool as_integer) -> LLSD
    {
        F64 number = number_value.asReal();
        if (has_min)
            number = llmax(number, low);
        if (has_max)
            number = llmin(number, high);
        return as_integer ? LLSD(static_cast<LLSD::Integer>(llround(number))) : LLSD(number);
    };
    if (coerced.isArray())
    {
        // A vector or colour must match the destination's component count, and every
        // numeric component is clamped on its own.
        if (current.isArray() && coerced.size() != current.size())
            return false;
        LLSD clamped = LLSD::emptyArray();
        for (S32 index = 0; index < coerced.size(); ++index)
        {
            const LLSD& component = coerced[index];
            if (component.isInteger() || component.isReal())
            {
                const bool as_integer = current.isArray() && current[index].isInteger();
                clamped.append((has_min || has_max) ? clamp_number(component, as_integer)
                                                    : (as_integer ? LLSD(static_cast<LLSD::Integer>(llround(component.asReal())))
                                                                  : LLSD(component.asReal())));
            }
            else
                clamped.append(component);
        }
        coerced = clamped;
    }
    else if ((coerced.isInteger() || coerced.isReal()) && (has_min || has_max))
        coerced = clamp_number(coerced, coerced.isInteger());
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->history().ensureGesture(actionPageKey(this), mMeta.label);
    if (mBinding)
    {
        mBinding->set(coerced);
        commitCardPreset();
    }
    else if (mControl)
    {
        mControl->setValue(coerced);
        mControl->onCommit();
    }
    else
        return false;
    bindingChanged();
    return true;
}

void ALKitRow::commitCardPreset()
{
    // A setting binding does not dispatch the native action. Quick proxies
    // and paste must run the same preset loader as selecting the source combo.
    // Widget/proxy bindings already dispatch; replay restores captured values.
    if (mCardPreset && mControl && dynamic_cast<ALKitSettingBinding*>(mBinding.get()) && !ALKitHistory::replaying())
    {
        mControl->setValue(mBinding->get());
        mControl->onCommit();
    }
}

bool ALKitRow::canPasteValue(const LLSD& value) const
{
    return enabledForEdit() && !mUnconnected && (!mBinding || mBinding->canSet()) &&
           ALKitQol::pasteTypesCompatible(value, boundValue());
}

void ALKitRow::replayBoundValue(const LLSD& value)
{
    if (mBinding && mBinding->canSet())
        mBinding->replay(value);
    else if (mControl)
    {
        mControl->setValue(value);
        mControl->onCommit();
    }
    bindingChanged();
}

bool ALKitRow::enabledForEdit() const
{
    if (mHeaderProxy)
    {
        if (ALKitSwitch* control = mHeaderSource.get())
        {
            for (LLView* view = control->getParent(); view; view = view->getParent())
                if (ALKitPage* page = dynamic_cast<ALKitPage*>(view))
                {
                    page->refreshAvailability();
                    break;
                }
            return control->isInEnabledChain();
        }
        return false;
    }
    if (ALKitRow* source = sourceRow())
        return source->enabledForEdit();
    for (LLView* view = getParent(); view; view = view->getParent())
        if (ALKitPage* page = dynamic_cast<ALKitPage*>(view))
        {
            page->refreshAvailability();
            break;
        }
    return mControl && mControl->isInEnabledChain();
}

ALKitQol::Favorite ALKitRow::favoriteKey() const
{
    if (mUnconnected)
        return ALKitQol::Favorite{}; // invalid: no star, no menu entry, no Quick proxy
    const ALKitQol::Favorite row{ mMeta.page_key, mMeta.widget };
    ALKitRow* source = mSourceRow.get();
    if (mHeaderProxy)
        return mHeaderSource.get() ? ALKitQol::Favorite{ mMeta.page_key, mHeaderSource.get()->getName() }
                                   : ALKitQol::Favorite{};
    const ALKitQol::Favorite source_key = source
        ? ALKitQol::Favorite{ source->mMeta.page_key, source->mMeta.widget }
        : ALKitQol::Favorite{};
    return ALKitQol::sourceFavorite(row, source_key);
}

ALKitRow* ALKitRow::createHeaderProxy(ALKitPage* page, ALKitCard* card, const std::string& widget,
                                    const std::string& name)
{
    if (!page) return nullptr;
    ALKitSwitch* source = card ? card->headerSwitch(widget) : page->masterSwitch();
    if (!source || (card ? !card->headerQuickSafe(widget)
                        : (!page->isMasterWidget(widget) || !page->masterQuickSafe() || page->masterSetting().empty()))) return nullptr;
    Params params;
    params.name = name;
    params.rect = LLRect(0, ALKitStyle::ROW_H, 400, 0);
    params.kind = "toggle";
    params.widget = name + "_control";
    params.label = card ? card->headerLabel(widget) : page->masterLabel();
    params.help = source->getToolTip();
    params.link_to = page->pageKey() + "/" + widget;
    params.widget_history = false; // One setting observer owns undo; ScopedSource attributes it to Quick.
    params.bulk_reset = false;
    ALKitRow* proxy = new ALKitRow(params);
    proxy->mHeaderProxy = true;
    proxy->mHeaderSource = source->getDerivedHandle<ALKitSwitch>();
    proxy->setOwnerIdentity(page->pageKey(), card ? card->id() : std::string());
    ALKitSwitch::Params control_params;
    control_params.name = params.widget;
    control_params.rect = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
    ALKitSwitch* control = LLUICtrlFactory::create<ALKitSwitch>(control_params);
    proxy->addChild(control);
    proxy->mBinding = std::make_unique<ALKitHeaderBinding>(source, card ? card->headerSetting(widget) : page->masterSetting());
    proxy->postBuild();
    proxy->mBindingConnection = proxy->mBinding->connectChanged([proxy]() { proxy->bindingChanged(); });
    control->setCommitCallback([proxy](LLUICtrl* edited, const LLSD&) { proxy->setBoundValue(edited->getValue()); });
    ALExposureSettingsHistory::installEditFinalizers(proxy);
    proxy->bindingChanged();
    control->setEnabled(proxy->enabledForEdit());
    return proxy;
}

ALKitRow* ALKitRow::createProxy(ALKitRow* source, const std::string& name)
{
    if (!source || !source->mControl || source->mUnconnected)
        return nullptr;
    const LLSD value = source->boundValue();
    Params params;
    params.name = name;
    params.rect = LLRect(0, ALKitStyle::ROW_H, 400, 0);
    const bool open_on_page = value.isMap() || value.isArray() ||
        source->mMeta.widget == "vc_gate_sources" || source->mMeta.widget == "vc_gate_status";
    params.kind = open_on_page ? "link" :
        (value.isBoolean() ? "toggle" : (value.isInteger() || value.isReal() ? "number" : "text"));
    params.widget = name + "_control";
    params.label = open_on_page ? source->mMeta.label + " — Open on its page" : source->mMeta.label;
    params.help = source->mMeta.help;
    params.link_to = source->mMeta.page_key + "/" + source->mMeta.widget;
    params.decimals = source->mMeta.decimals;
    params.min_val = source->mMeta.has_min ? source->mMeta.min_val : -1000000.f;
    params.max_val = source->mMeta.has_max ? source->mMeta.max_val : 1000000.f;
    // A non-resettable source stays non-resettable: a Quick reset must never reach its setting.
    params.bulk_reset = source->mBulkReset;
    params.widget_history = false; // The source binding records the action under the input page.
    ALKitRow* proxy = new ALKitRow(params);
    proxy->mSourceRow = source->getDerivedHandle<ALKitRow>();
    if (open_on_page)
    {
        proxy->mBinding = std::make_unique<ALKitProxyBinding>(source, false);
        proxy->mMeta.page_key = source->mMeta.page_key;
        proxy->mMeta.card_id = source->mMeta.card_id;
        proxy->postBuild();
        proxy->mBindingConnection = proxy->mBinding->connectChanged([proxy]() { proxy->bindingChanged(); });
        proxy->bindingChanged();
        return proxy;
    }
    LLUICtrl* control = nullptr;
    if (value.isBoolean())
    {
        ALKitSwitch::Params control_params;
        control_params.name = params.widget;
        control_params.rect = LLRect(0, ALKitStyle::SWITCH_H, ALKitStyle::SWITCH_W, 0);
        control = LLUICtrlFactory::create<ALKitSwitch>(control_params);
    }
    else if (value.isInteger() || value.isReal())
    {
        LLSpinCtrl::Params control_params;
        control_params.name = params.widget;
        control_params.rect = LLRect(0, 24, 120, 0);
        control_params.min_value = source->mMeta.has_min ? source->mMeta.min_val : -1000000.f;
        control_params.max_value = source->mMeta.has_max ? source->mMeta.max_val : 1000000.f;
        control_params.decimal_digits = source->mMeta.decimals;
        control = LLUICtrlFactory::create<LLSpinCtrl>(control_params);
    }
    else if (value.isString())
    {
        LLLineEditor::Params control_params;
        control_params.name = params.widget;
        control_params.rect = LLRect(0, 24, 180, 0);
        control = LLUICtrlFactory::create<LLLineEditor>(control_params);
    }
    else
    {
        delete proxy;
        return nullptr;
    }
    proxy->addChild(control);
    proxy->mBinding = std::make_unique<ALKitProxyBinding>(source, true);
    proxy->mMeta.page_key = source->mMeta.page_key;
    proxy->mMeta.card_id = source->mMeta.card_id;
    proxy->postBuild();
    proxy->mBindingConnection = proxy->mBinding->connectChanged([proxy]() { proxy->bindingChanged(); });
    control->setCommitCallback([proxy](LLUICtrl* edited, const LLSD&) { proxy->setBoundValue(edited->getValue()); });
    ALExposureSettingsHistory::installEditFinalizers(proxy);
    proxy->bindingChanged();
    return proxy;
}

void ALKitRow::refreshFavorite()
{
    const ALKitShell* shell = ALKitShell::find(this);
    const ALKitQol::Favorite key = favoriteKey();
    const bool favorite = shell && shell->isFavorite(key.page, key.widget);
    if (mFavorite)
    {
        mFavorite->setLabel(favorite ? "\xE2\x98\x85" : "\xE2\x98\x86");
        mFavorite->setToolTip(LLStringExplicit(favorite ? "Remove from Quick" : "Add to Quick"));
        // Rows outside a page (popover rows) have no page key and cannot be Quick favorites.
        const bool inline_bar = mFillWidth && mMeta.kind == "custom" && dynamic_cast<ALFlowGridItem*>(mControl);
        mFavorite->setVisible(!inline_bar && ALKitQol::isValidFavorite(key) && (mHovered || favorite));
    }
}

void ALKitRow::onMouseEnter(S32 x, S32 y, MASK m)
{
    mPointerInside = true;
    mHovered       = true;
    refreshFavorite();
    styleValueEditor(true);
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->help().showRow(this);
    LLPanel::onMouseEnter(x, y, m);
}
void ALKitRow::onMouseLeave(S32 x, S32 y, MASK m)
{
    mPointerInside = false;
    mHovered       = baselineEditActive(mControl);
    refreshFavorite();
    styleValueEditor(mHovered);
    LLPanel::onMouseLeave(x, y, m);
}

bool ALKitRow::handleKeyHere(KEY key, MASK mask)
{
    if (mMeta.kind == "segmented" && mControl && mControl->getEnabled() &&
        mControl->hasFocus() && mask == MASK_NONE)
    {
        if (key == KEY_HOME && resettable())
        {
            reset();
            return true;
        }
        if (key == ' ' || key == KEY_RETURN)
        {
            mControl->onCommit();
            return true;
        }
    }
    return LLPanel::handleKeyHere(key, mask);
}

bool ALKitRow::handleMouseDown(S32 x, S32 y, MASK mask)
{
    ALKitHistory::ScopedSource source(this);
    if (sourceRow())
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->history().beginGesture(ALKitHistory::MOUSE, actionPageKey(this), mMeta.label);
    if (mMeta.kind == "segmented" && mControl && mControl->getEnabled() &&
        mControl->getRect().pointInRect(x, y))
    {
        LLRadioGroup* group = dynamic_cast<LLRadioGroup*>(mControl);
        const S32 count = static_cast<S32>(mSegmentLabels.size());
        if (group && count)
        {
            const S32 index = llclamp((x - mControl->getRect().mLeft) * count /
                                          llmax(1, mControl->getRect().getWidth()),
                                      0, count - 1);
            group->setFocus(true);
            group->setSelectedIndex(index);
            group->onCommit();
            return true;
        }
    }
    return LLPanel::handleMouseDown(x, y, mask);
}

bool ALKitRow::handleMouseUp(S32 x, S32 y, MASK mask)
{
    ALKitHistory::ScopedSource source(this);
    if (mMeta.kind == "segmented" && mControl && mControl->getEnabled() &&
        mControl->getRect().pointInRect(x, y))
        return true;
    return LLPanel::handleMouseUp(x, y, mask);
}

bool ALKitRow::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    ALKitHistory::ScopedSource source(this);
    if (sourceRow())
        if (ALKitShell* shell = ALKitShell::find(this))
            shell->history().beginGesture(ALKitHistory::WHEEL, actionPageKey(this), mMeta.label);
    return LLPanel::handleScrollWheel(x, y, delta);
}

bool ALKitRow::handleRightMouseDown(S32 x, S32 y, MASK)
{
    showContextMenu(x, y);
    return true;
}

void ALKitRow::showContextMenu(S32 x, S32 y)
{
    if (!mContextMenu.get())
    {
        LLContextMenu::Params params;
        params.name = mMeta.widget + "_row_menu";
        params.visible = false;
        LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(params);
        mContextMenu = menu->getHandle();
        auto add_item = [menu](const std::string& name, const std::string& label,
                               const std::function<void()>& action)
        {
            LLMenuItemCallGL::Params item;
            item.name = name;
            item.label = label;
            item.on_click.function = [action](LLUICtrl*, const LLSD&) { action(); };
            menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
        };
        add_item("reset", "Reset to default", [this]() { reset(); });
        add_item("copy", "Copy value", [this]()
        {
            sRowClipboard = boundValue();
            if (mMeta.kind == "text" || dynamic_cast<LLLineEditor*>(mControl))
            {
                const auto text = utf8str_to_wstring(sRowClipboard.asString());
                LLClipboard::instance().copyToClipboard(text, 0, static_cast<S32>(text.size()));
            }
        });
        add_item("paste", "Paste value", [this]()
        {
            ALKitQol::pasteRowValue(mMeta.kind == "text" || dynamic_cast<LLLineEditor*>(mControl), sRowClipboard, []()
            {
                LLWString text;
                return LLClipboard::instance().pasteFromClipboard(text)
                    ? LLSD(wstring_to_utf8str(text)) : LLSD();
            }, [this](const LLSD& value) { return setBoundValue(value); });
        });
        add_item("favorite", "Add to Quick", [this]()
        {
            if (ALKitShell* shell = ALKitShell::find(this))
            {
                const ALKitQol::Favorite key = favoriteKey();
                shell->toggleFavorite(key.page, key.widget);
            }
            refreshFavorite();
        });
        add_item("search", "Show in search", [this]()
        {
            if (ALKitShell* shell = ALKitShell::find(this))
                shell->showRowInSearch(this);
        });
        gMenuHolder->addChild(menu);
    }
    LLContextMenu* menu = mContextMenu.get();
    if (!menu)
        return;
    if (LLMenuItemGL* item = menu->findChild<LLMenuItemGL>("reset"))
        item->setVisible(resettable());
    if (LLMenuItemGL* item = menu->findChild<LLMenuItemGL>("paste"))
        item->setEnabled((mMeta.kind == "text" || dynamic_cast<LLLineEditor*>(mControl))
            ? LLClipboard::instance().isTextAvailable() && enabledForEdit() && !mUnconnected &&
                (!mBinding || mBinding->canSet())
            : canPasteValue(sRowClipboard));
    if (LLMenuItemGL* item = menu->findChild<LLMenuItemGL>("favorite"))
    {
        const ALKitQol::Favorite key = favoriteKey();
        item->setVisible(ALKitQol::isValidFavorite(key));
        item->setLabel(LLStringExplicit(ALKitShell::find(this) &&
            ALKitShell::find(this)->isFavorite(key.page, key.widget) ? "Remove from Quick" : "Add to Quick"));
    }
    menu->buildDrawLabels();
    ALKitMenu::showContextMenu(this, menu, x, y);
}

void ALKitRow::styleValueEditor(bool active)
{
    if (!mControl || (mMeta.kind != "slider" && mMeta.kind != "number"))
        return;
    if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(mControl); spinner && mMeta.kind == "number")
        spinner->setValueColumnLayout(mControl->getRect().getWidth(), mControl->getRect().getHeight(), true);
    for (LLView* child : *mControl->getChildList())
    {
        if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
        {
            editor->setHAlign(LLFontGL::RIGHT);
            editor->setUseBgColor(true);
            const bool field_visible = active || mMeta.kind == "number";
            editor->setBgColor(field_visible ? ALKitStyle::color(ALKitStyle::INPUT_BG) : LLUIColor(LLColor4::transparent));
            editor->setBgBorderColor(field_visible ? ALKitStyle::color(ALKitStyle::INPUT_BORDER)
                                                   : LLUIColor(LLColor4::transparent));
            editor->setFont(ALKitStyle::monoFont());
        }
        else if (LLTextBox* text = dynamic_cast<LLTextBox*>(child))
        {
            text->setRightAlign();
            text->setFont(ALKitStyle::monoFont());
        }
    }
}

void ALKitRow::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    if (mMeta.kind == "dial" || mMeta.kind == "arc")
    {
        gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::CARD_BG).get(), true);
        gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::CARD_BORDER).get(), false);
    }
    if (mHovered && !(mFillWidth && mMeta.kind == "custom" && dynamic_cast<ALFlowGridItem*>(mControl)))
        gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::ROW_HOVER).get(), true);
    if (mAdded)
    {
        gGL.color4fv(ALKitStyle::color(ALKitStyle::ACCENT).get().mV);
        gl_circle_2d(2.5f, getRect().getHeight() * .5f, 2.5f, 10, true);
    }
    if (mFlashing)
    {
        const F32 t = mFlashTimer.getElapsedTimeF32();
        if (t >= ALKitStyle::FLASH_SEC)
            mFlashing = false;
        else
        {
            LLColor4 c = ALKitStyle::color(ALKitStyle::FLASH).get();
            c.mV[VALPHA] *= 1.f - t / ALKitStyle::FLASH_SEC;
            gl_rect_2d(getLocalRect(), c, false);
        }
    }
    LLPanel::draw();
    if (mMeta.kind == "segmented" && mControl && !mSegmentLabels.empty())
    {
        const LLRect control_rect = mControl->getRect();
        const S32 count = static_cast<S32>(mSegmentLabels.size());
        const S32 selected = dynamic_cast<LLRadioGroup*>(mControl)
            ? static_cast<LLRadioGroup*>(mControl)->getSelectedIndex() : -1;
        const LLFontGL* font = ALKitStyle::sansFont("Kit12");
        for (S32 index = 0; index < count; ++index)
        {
            const S32 left = control_rect.mLeft + index * control_rect.getWidth() / count;
            const S32 right = control_rect.mLeft + (index + 1) * control_rect.getWidth() / count;
            const LLRect segment(left, control_rect.mTop, right - 1, control_rect.mBottom);
            gl_rect_2d(segment, ALKitStyle::color(index == selected ? ALKitStyle::ACTIVE_FILL
                                                                    : ALKitStyle::INPUT_BG).get(), true);
            gl_rect_2d(segment, ALKitStyle::color(mControl->hasFocus() && index == selected
                ? ALKitStyle::ACCENT : ALKitStyle::INPUT_BORDER).get(), false);
            if (font)
                font->renderUTF8(mSegmentLabels[index], 0,
                    (left + right) / 2,
                    control_rect.getCenterY(),
                    ALKitStyle::color(index == selected ? ALKitStyle::TEXT_STRONG : ALKitStyle::MUTED).get(),
                    LLFontGL::HCENTER, LLFontGL::VCENTER);
        }
    }
}
