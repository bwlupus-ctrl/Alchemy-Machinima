#ifndef AL_KIT_STYLE_H
#define AL_KIT_STYLE_H

#include "lluicolor.h"
#include <string>

class LLFontGL;
class LLButton;
class LLComboBox;
class LLLineEditor;
class LLScrollContainer;
class LLScrollListCtrl;
class LLView;
namespace ALKitStyle
{
constexpr S32 ROW_H = 28, LABEL_W = 100, VALUE_W = 52, RESET_W = 20, COL_GAP = 6, ROW_INSET = 4;
constexpr S32 DIAL_SIZE = 60, DIAL_ROW_H = 68, DIAL_CELL_H = 94, NUMBER_FIELD_W = 96;
constexpr S32 SWITCH_W = 34, SWITCH_H = 18, SWITCH_KNOB = 14, SWITCH_INSET = 2;
constexpr S32 RESET_GLYPH = 11, CARD_RESET_W = 24, CARD_RESET_GLYPH = 13;
constexpr S32 CARD_HDR_H = 36, CARD_PAD_L = 12, CARD_PAD_R = 8, CARD_PAD_T = 6, CARD_PAD_B = 8;
constexpr S32 CARD_RADIUS = 6, CARD_MIN_W = 340, GRID_GAP = 12;
constexpr S32 PAGE_PAD_T = 14, PAGE_PAD_LR = 16, PAGE_PAD_B = 16;
constexpr S32 SUB_FIRST_H = 26, SUB_H = 34, MORE_H = 26, SELECTOR_BAND_H = 34, TRACK_MIN = 120;
constexpr S32 RAIL_W = 176, RAIL_ICON_W = 56, RAIL_ITEM_H = 40, ROSTER_HDR_H = 24, ROSTER_TOOL = 22;
constexpr S32 HELP_W = 224, HELP_STRIP_H = 30, TABSTRIP_H = 40, CHIPS_H = 38, WHO_H = 34;
constexpr S32 BAR_H = 44, BANNER_H = 32, TRANSPORT_H = 40, SEARCH_W = 170, PRESET_COMBO_W = 160;
constexpr S32 CARD_PRESET_W = 160, MAP_LEGEND_H = 40, MAP_CONTROL_H = 28, MAP_HINT_H = 18;
constexpr S32 MAP_FOOTER_H = MAP_LEGEND_H + MAP_CONTROL_H + MAP_HINT_H;
constexpr S32 BP_FULL = 1160, BP_RAIL = 980, BP_HYST = 8;
constexpr S32 FLOATER_DEFAULT_W = 1160, FLOATER_DEFAULT_H = 720, FLOATER_MIN_W = 850, FLOATER_MIN_H = 560;
constexpr F32 FLASH_SEC = 1.2f, DIM_ALPHA = .45f;
enum EColor
{
    WINDOW_BG,
    RAIL_BG,
    SUB_BAND,
    CARD_BG,
    CARD_BORDER,
    DIVIDER,
    DIVIDER2,
    ROW_HOVER,
    RAIL_HOVER,
    CONTROL_BG,
    ACTIVE_FILL,
    HOVER_BG,
    INPUT_BG,
    INPUT_BORDER,
    TRACK,
    SWITCH_OFF,
    KNOB_OFF,
    KNOB_ON,
    ACCENT,
    ACCENT_HI,
    RECORD,
    RECORD_TEXT,
    TEXT,
    TEXT_STRONG,
    LABEL,
    DIM,
    MUTED,
    FAINT,
    DIAL_RING,
    DIAL_FACE,
    ARC_TRACK,
    FLASH,
    BANNER_BG,
    MAP_KEY,
    MAP_FILL,
    MAP_BACK,
    MAP_BG,
    OFF_GREY,
    LIGHTMAP_CENTRE,
    LIGHTMAP_EDGE,
    LIGHTMAP_BORDER,
    ICON_TINT,
    ICON_TINT_STRONG,
    RECORD_TEXT_ON,
    COLOR_COUNT
};
const LLUIColor& color(EColor color);
const LLFontGL*  sansFont(const std::string& size = "Kit12", bool bold = false);
const LLFontGL*  monoFont(const std::string& size = "Kit12", bool bold = false);
void             styleButton(LLButton* button, bool transparent = false);
void             stylePill(LLButton* button);
void             styleTransportAction(LLButton* action, LLButton* cut, LLButton* arm, bool running);
void             styleCombo(LLComboBox* combo);
void             styleEditor(LLLineEditor* editor, bool mono = false);
void             styleScroller(LLScrollContainer* scroller);
void             styleList(LLScrollListCtrl* list);
void             styleTree(LLView* root);
// Re-reads the colours of every styled scroll list. Call after the colour theme changes.
void             restyleLists();
} // namespace ALKitStyle
#endif
