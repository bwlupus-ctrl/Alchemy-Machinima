#include "llviewerprecompiledheaders.h"
#include "alkitrow.h"
#include "alkitbinding.h"
#include "alkitstyle.h"
#include "alkitshell.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "alkitcard.h"
#include "alkithistory.h"
#include "alkitpage.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llrender2dutils.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llspinctrl.h"
#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "llradiogroup.h"
#include "llcontrol.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "aluirefresh.h"
#include "alcompassdial.h"
#include "alkitswitch.h"

#include <algorithm>
#include <vector>
static LLDefaultChildRegistry::Register<ALKitRow> r("kit_row");

namespace
{
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
} // namespace

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
    advanced("advanced", false),
    card_preset("card_preset", false),
    who_picker("who_picker", false),
    hidden("hidden", false),
    added("added", false),
    row_height("row_height", 28),
    decimals("decimals", 3),
    min_val("min_val", 0.f),
    max_val("max_val", 0.f)
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
    mCardPreset(p.card_preset),
    mWhoPicker(p.who_picker),
    mHidden(p.hidden),
    mAdded(p.added)
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
        for (S32 i = 0; i < mVecCount; ++i)
        {
            LLSpinCtrl::Params p;
            p.name           = mMeta.widget + axis[i];
            p.rect           = LLRect(0, 22, 80, 0);
            p.label_width    = 0;
            p.decimal_digits = mMeta.decimals;
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
        mLastValue = mShownValue = mControl->getValue();

        if (mBind == "code" && mMeta.kind != "button" && mMeta.kind != "readout" && mMeta.mirror_setting.empty())
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

    if (mMeta.kind == "segmented")
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

    if (mBinding && !mReset)
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
                if (mBinding)
                    mBinding->reset();
            });
        addChild(mReset);
        bindingChanged();
    }

    layoutRow(getRect().getWidth());
    styleValueEditor(false);
    return true;
}

S32 ALKitRow::heightForWidth(S32) const
{
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

void ALKitRow::layoutRow(S32 w)
{
    const S32 h = heightForWidth(w);
    reshape(w, h, true);
    if (mLabel)
    {
        const bool full_span_custom = mMeta.kind == "custom" && mMeta.label.empty();
        mLabel->reshape(ALKitStyle::LABEL_W, 20, true);
        mLabel->setOrigin(ALKitStyle::ROW_INSET, (h - 20) / 2);
        mLabel->setVisible(!full_span_custom);
    }

    const bool full_span_custom = mMeta.kind == "custom" && mMeta.label.empty();
    const S32 x = full_span_custom ? ALKitStyle::ROW_INSET
                                   : ALKitStyle::ROW_INSET + ALKitStyle::LABEL_W + ALKitStyle::COL_GAP;
    const S32 cw =
        full_span_custom
            ? llmax(1, w - 2 * ALKitStyle::ROW_INSET)
            : llmax(1, w - 2 * ALKitStyle::ROW_INSET - ALKitStyle::LABEL_W - ALKitStyle::VALUE_W - ALKitStyle::RESET_W - 3 * ALKitStyle::COL_GAP);
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
            width = llmin(span, (f ? f->getWidth(label) : 80) + 20);
        }
        else if (mMeta.kind == "slider" || mMeta.kind == "combo" || mMeta.kind == "text" || mMeta.kind == "readout" ||
                 mMeta.kind == "link" || mMeta.kind == "custom" || mMeta.kind == "segmented")
            width = span;
        if (mExplicitRowHeight && (mMeta.kind == "custom" || mMeta.kind == "list" || mMeta.kind == "viewport"))
            height = h;
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
        mValueText->reshape(ALKitStyle::VALUE_W, 20, true);
        mValueText->setOrigin(x + cw + ALKitStyle::COL_GAP, (h - 20) / 2);
    }
    if (mReset)
    {
        mReset->reshape(ALKitStyle::RESET_W, 20, true);
        mReset->setOrigin(w - ALKitStyle::ROW_INSET - ALKitStyle::RESET_W, (h - 20) / 2);
    }
}

void ALKitRow::layoutDialCell(S32 w)
{
    reshape(w, ALKitStyle::DIAL_CELL_H, true);
    if (mLabel)
    {
        mLabel->reshape(llmax(1, w - ALKitStyle::RESET_W - ALKitStyle::COL_GAP), 20, true);
        mLabel->setOrigin(0, ALKitStyle::DIAL_CELL_H - 20);
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
    const LLSD value = mControl->getValue();
    if (!alSameValue(value, mShownValue))
        refreshValue();
    if (!baselineEditActive(mControl))
        mLastValue = value;
}

void ALKitRow::bindingChanged()
{
    const bool now = amber();
    if (mReset)
        mReset->setImageOverlay("KitIcon_reset_row_16",
                                LLFontGL::HCENTER,
                                (now ? ALKitStyle::color(ALKitStyle::ACCENT) : ALKitStyle::color(ALKitStyle::FAINT)).get());
    if (now != mAmber)
    {
        mAmber = now;
        mAmberSignal(now);
    }
    refreshValue();
    if (mControl && !baselineEditActive(mControl))
        mLastValue = mControl->getValue();
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
        if (!mReset)
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
                    if (mBinding)
                        mBinding->reset();
                });
            addChild(mReset);
        }
        bindingChanged();
    }
}

bool ALKitRow::amber() const
{ return mBinding && !mBinding->isDefault(mMeta.decimals); }

void ALKitRow::onMouseEnter(S32 x, S32 y, MASK m)
{
    mPointerInside = true;
    mHovered       = true;
    styleValueEditor(true);
    if (ALKitShell* shell = ALKitShell::find(this))
        shell->help().showRow(this);
    LLPanel::onMouseEnter(x, y, m);
}
void ALKitRow::onMouseLeave(S32 x, S32 y, MASK m)
{
    mPointerInside = false;
    mHovered       = baselineEditActive(mControl);
    styleValueEditor(mHovered);
    LLPanel::onMouseLeave(x, y, m);
}

bool ALKitRow::handleMouseDown(S32 x, S32 y, MASK mask)
{
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
            group->setSelectedIndex(index);
            group->onCommit();
            return true;
        }
    }
    return LLPanel::handleMouseDown(x, y, mask);
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
    if (mHovered)
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
            gl_rect_2d(getLocalRect(), c, true);
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
            gl_rect_2d(segment, ALKitStyle::color(ALKitStyle::INPUT_BORDER).get(), false);
            if (font)
                font->renderUTF8(mSegmentLabels[index], 0,
                    (left + right) / 2,
                    control_rect.getCenterY(),
                    ALKitStyle::color(index == selected ? ALKitStyle::TEXT_STRONG : ALKitStyle::MUTED).get(),
                    LLFontGL::HCENTER, LLFontGL::VCENTER);
        }
    }
}
