#include "llviewerprecompiledheaders.h"

#include "alkitstyle.h"

#include "llfontgl.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llradiogroup.h"
#include "llscrollbar.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluicolortable.h"
#include <functional>

namespace ALKitStyle
{
const LLUIColor& color(EColor value)
{
    static const char* names[COLOR_COUNT] = {
        "KitWindowBg",  "KitRailBg",    "KitSubBand",  "KitCardBg",      "KitCardBorder", "KitDivider",    "KitDivider2", "KitRowHover",
        "KitRailHover", "KitControlBg", "KitActiveFill", "KitHoverBg", "KitInputBg", "KitInputBorder", "KitTrack",      "KitSwitchOff",  "KitKnobOff",  "KitKnobOn",
        "KitAccent",    "KitAccentHi",  "KitRecord",   "KitRecordText",  "KitText",       "KitTextStrong", "KitLabel",    "KitDim",
        "KitMuted",     "KitFaint",     "KitDialRing", "KitDialFace",    "KitArcTrack",   "KitFlash",      "KitBannerBg", "KitMapKey",
        "KitMapFill",   "KitMapBack",   "KitMapBg",    "KitOffGrey",    "KitLightMapBgCentre", "KitLightMapBgEdge",
        "KitLightMapBorder"
    };
    static LLUIColor colors[COLOR_COUNT];
    static bool      initialized = false;
    if (!initialized)
    {
        for (S32 i = 0; i < COLOR_COUNT; ++i)
        {
            colors[i] = LLUIColorTable::instance().getColor(names[i]);
        }
        initialized = true;
    }
    return colors[llclamp((S32)value, 0, (S32)COLOR_COUNT - 1)];
}

const LLFontGL* sansFont(const std::string& size, bool bold)
{
    const LLFontGL* font = LLFontGL::getFont(LLFontDescriptor("IBMPlexSans", size, bold ? LLFontGL::BOLD : 0));
    return font ? font : LLFontGL::getFontSansSerifSmall();
}

const LLFontGL* monoFont()
{
    const LLFontGL* font = LLFontGL::getFont(LLFontDescriptor("IBMPlexMono", "Kit12", 0));
    return font ? font : LLFontGL::getFontMonospace();
}

void styleButton(LLButton* button, bool transparent)
{
    if (!button)
        return;
    static LLPointer<LLUIImage> rounded = LLUI::getUIImage("Kit_RoundRect4");
    static LLPointer<LLUIImage> clear   = LLUI::getUIImage("transparent.j2c");
    LLPointer<LLUIImage> image = transparent ? clear : rounded;
    button->setImageUnselected(image);
    button->setImageSelected(image);
    button->setImageHoverUnselected(rounded);
    button->setImageHoverSelected(rounded);
    button->setImagePressed(rounded);
    button->setImageDisabled(image);
    button->setImageDisabledSelected(image);
    button->setScaleImage(true);
    button->setImageColor(transparent ? LLUIColor(LLColor4::transparent) : color(CONTROL_BG));
    button->setSelectedImageColor(transparent ? LLUIColor(LLColor4::transparent) : color(ACTIVE_FILL));
    button->setHoverImageColor(color(HOVER_BG));
    button->setPressedImageColor(color(ACTIVE_FILL));
    button->setDisabledImageColor(transparent ? LLUIColor(LLColor4::transparent) : color(INPUT_BG));
    button->setUnselectedLabelColor(color(LABEL));
    button->setSelectedLabelColor(color(TEXT_STRONG));
    button->setDisabledLabelColor(color(FAINT));
    button->setFont(sansFont("Kit12"));
    button->setHoverGlowStrength(.08f);
    button->setDropShadowedText(false);
}

void stylePill(LLButton* button)
{
    if (!button)
        return;
    styleButton(button, false);
    static LLPointer<LLUIImage> pill = LLUI::getUIImage("Kit_Pill9");
    button->setImageUnselected(pill);
    button->setImageSelected(pill);
    button->setImageHoverUnselected(pill);
    button->setImageHoverSelected(pill);
    button->setImagePressed(pill);
    button->setImageDisabled(pill);
    button->setImageDisabledSelected(pill);
}

void styleEditor(LLLineEditor* editor, bool mono)
{
    if (!editor)
        return;
    editor->setUseBgColor(true);
    editor->setBgColor(color(INPUT_BG));
    editor->setBgBorderColor(color(INPUT_BORDER));
    editor->setFont(mono ? monoFont() : sansFont("Kit12"));
    editor->setFgColor(color(TEXT));
    editor->setReadOnlyFgColor(color(DIM));
    editor->setTentativeFgColor(color(MUTED));
    editor->setCursorColor(color(ACCENT));
}

void styleCombo(LLComboBox* combo)
{
    if (!combo)
        return;
    for (LLView* child : *combo->getChildList())
        if (LLButton* button = dynamic_cast<LLButton*>(child))
        {
            styleButton(button, false);
            button->setImageOverlay("KitIcon_chevron_down_10_16", LLFontGL::RIGHT, color(DIM).get());
            button->setRightHPad(20);
            break;
        }
}

void styleScroller(LLScrollContainer* scroller)
{
    if (!scroller)
        return;
    static LLPointer<LLUIImage> track = LLUI::getUIImage("Kit_RoundRect4");
    static LLPointer<LLUIImage> thumb = LLUI::getUIImage("Kit_Pill9");
    for (LLView* child : *scroller->getChildList())
        if (LLScrollbar* scrollbar = dynamic_cast<LLScrollbar*>(child))
            scrollbar->setStyle(track, track, thumb, thumb, color(INPUT_BG), color(INPUT_BORDER), color(INPUT_BG));
}

void styleList(LLScrollListCtrl* list)
{
    if (!list)
        return;
    list->setBgWriteableColor(color(INPUT_BG).get());
    list->setReadOnlyBgColor(color(INPUT_BG).get());
    list->setBgSelectedColor(color(CONTROL_BG).get());
    list->setBgStripeColor(color(WINDOW_BG).get());
    list->setFgSelectedColor(color(TEXT_STRONG).get());
    list->setFgUnselectedColor(color(TEXT).get());
    list->setFgDisableColor(color(FAINT).get());
    list->setHoveredColor(color(RAIL_HOVER).get());
    list->setHighlightedColor(color(ACCENT).get());
    static LLPointer<LLUIImage> track = LLUI::getUIImage("Kit_RoundRect4");
    static LLPointer<LLUIImage> thumb = LLUI::getUIImage("Kit_Pill9");
    for (LLView* child : *list->getChildList())
        if (LLScrollbar* scrollbar = dynamic_cast<LLScrollbar*>(child))
            scrollbar->setStyle(track, track, thumb, thumb, color(INPUT_BG), color(INPUT_BORDER), color(INPUT_BG));
}

void styleTree(LLView* root)
{
    if (!root)
        return;
    std::function<void(LLView*)> visit = [&](LLView* view)
    {
        const std::string& name = view->getName();
        if (LLComboBox* combo = dynamic_cast<LLComboBox*>(view))
        {
            styleCombo(combo);
            return;
        }
        if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(view))
        {
            for (LLView* child : *spinner->getChildList())
                if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                    styleEditor(editor, true);
            return;
        }
        if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(view))
        {
            for (LLView* child : *slider->getChildList())
                if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                    styleEditor(editor, true);
            return;
        }
        if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(view))
        {
            styleList(list);
            return;
        }
        if (dynamic_cast<LLCheckBoxCtrl*>(view) || dynamic_cast<LLRadioGroup*>(view) ||
            dynamic_cast<LLScrollbar*>(view) || dynamic_cast<LLTabContainer*>(view))
            return;
        if (LLButton* button = dynamic_cast<LLButton*>(view))
        {
            const bool clear = name.rfind("kit_rail_", 0) == 0 || name.rfind("kit_tab_", 0) == 0 ||
                               name.rfind("kit_mode_seg_", 0) == 0 || name.find("_reset") != std::string::npos;
            styleButton(button, clear);
            if (name == "btn_action")
            {
                button->setImageColor(color(ACCENT));
                button->setSelectedImageColor(color(ACCENT_HI));
                button->setHoverImageColor(color(ACCENT_HI));
                button->setPressedImageColor(color(ACTIVE_FILL));
                button->setUnselectedLabelColor(color(KNOB_ON));
                button->setSelectedLabelColor(color(KNOB_ON));
                button->setFont(sansFont("Kit12", true));
            }
            else if (name == "btn_cut")
            {
                button->setImageColor(color(RECORD));
                button->setSelectedImageColor(color(RECORD));
                button->setUnselectedLabelColor(LLUIColor(LLColor4::white));
                button->setSelectedLabelColor(LLUIColor(LLColor4::white));
                button->setFont(sansFont("Kit12", true));
            }
            else if (name.find("_more") != std::string::npos)
            {
                styleButton(button, true);
                button->setUnselectedLabelColor(color(ACCENT));
                button->setSelectedLabelColor(color(ACCENT_HI));
                button->setHAlign(LLFontGL::LEFT);
            }
        }
        else if (LLTextBox* text = dynamic_cast<LLTextBox*>(view))
        {
            const bool mono = name.find("timecode") != std::string::npos || name.find("rec_indicator") != std::string::npos ||
                              name.find("_value") != std::string::npos;
            text->setFont(mono ? monoFont() : sansFont("Kit12"));
        }
        else if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(view))
            styleEditor(editor, name.find("_value") != std::string::npos || name.find("timecode") != std::string::npos);
        for (LLView* child : *view->getChildList())
            visit(child);
    };
    visit(root);
}
} // namespace ALKitStyle
