#include "llviewerprecompiledheaders.h"

#include "alkitstyle.h"

#include "llfontgl.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
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
        "KitRailHover", "KitControlBg", "KitInputBg",  "KitInputBorder", "KitTrack",      "KitSwitchOff",  "KitKnobOff",  "KitKnobOn",
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
    button->setImageHoverUnselected(image);
    button->setImageHoverSelected(image);
    button->setImagePressed(image);
    button->setScaleImage(true);
    button->setImageColor(transparent ? LLUIColor(LLColor4::transparent) : color(CONTROL_BG));
    button->setUnselectedLabelColor(color(LABEL));
    button->setSelectedLabelColor(color(TEXT_STRONG));
    button->setDisabledLabelColor(color(FAINT));
    button->setFont(sansFont("Kit12"));
    button->setHoverGlowStrength(.08f);
    button->setDropShadowedText(false);
}

void styleTree(LLView* root)
{
    if (!root)
        return;
    std::function<void(LLView*, bool)> visit = [&](LLView* view, bool composite_child)
    {
        const bool composite = composite_child || dynamic_cast<LLComboBox*>(view) || dynamic_cast<LLSpinCtrl*>(view) ||
                               dynamic_cast<LLSliderCtrl*>(view) || dynamic_cast<LLCheckBoxCtrl*>(view);
        const std::string& name = view->getName();
        if (LLButton* button = dynamic_cast<LLButton*>(view))
        {
            const bool clear = name.rfind("kit_rail_", 0) == 0 || name.rfind("kit_tab_", 0) == 0 ||
                               name.rfind("kit_mode_seg_", 0) == 0;
            styleButton(button, clear);
            if (name == "btn_action")
            {
                button->setImageColor(color(ACCENT));
                button->setUnselectedLabelColor(color(KNOB_ON));
                button->setSelectedLabelColor(color(KNOB_ON));
                button->setFont(sansFont("Kit12", true));
            }
            else if (name == "btn_cut")
            {
                button->setImageColor(color(RECORD));
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
        {
            editor->setFont(name.find("spinner") != std::string::npos ? monoFont() : sansFont("Kit12"));
            editor->setFgColor(color(TEXT));
            editor->setReadOnlyFgColor(color(DIM));
            editor->setTentativeFgColor(color(MUTED));
            editor->setCursorColor(color(ACCENT));
        }
        else if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(view))
        {
            list->setBgWriteableColor(color(INPUT_BG).get());
            list->setReadOnlyBgColor(color(INPUT_BG).get());
            list->setBgSelectedColor(color(CONTROL_BG).get());
            list->setBgStripeColor(color(WINDOW_BG).get());
            list->setFgSelectedColor(color(TEXT_STRONG).get());
            list->setFgUnselectedColor(color(TEXT).get());
            list->setFgDisableColor(color(FAINT).get());
            list->setHoveredColor(color(RAIL_HOVER).get());
            list->setHighlightedColor(color(ACCENT).get());
        }
        for (LLView* child : *view->getChildList())
            visit(child, composite && view != root);
    };
    visit(root, false);
}
} // namespace ALKitStyle
