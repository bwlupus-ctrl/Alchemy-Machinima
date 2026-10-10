#include "llviewerprecompiledheaders.h"

#include "alkitstyle.h"

#include "llfontgl.h"
#include "lluicolortable.h"

namespace ALKitStyle
{
const LLUIColor& color(EColor value)
{
    static const char* names[COLOR_COUNT] =
    {
        "KitWindowBg", "KitRailBg", "KitSubBand", "KitCardBg", "KitCardBorder",
        "KitDivider", "KitDivider2", "KitRowHover", "KitRailHover", "KitControlBg",
        "KitInputBg", "KitInputBorder", "KitTrack", "KitSwitchOff", "KitKnobOff",
        "KitKnobOn", "KitAccent", "KitAccentHi", "KitRecord", "KitRecordText",
        "KitText", "KitTextStrong", "KitLabel", "KitDim", "KitMuted", "KitFaint",
        "KitDialRing", "KitDialFace", "KitArcTrack", "KitFlash", "KitBannerBg",
        "KitMapKey", "KitMapFill", "KitMapBack", "KitMapBg", "KitOffGrey"
    };
    static LLUIColor colors[COLOR_COUNT];
    static bool initialized = false;
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

const LLFontGL* monoFont()
{
    const LLFontGL* font = LLFontGL::getFont(LLFontDescriptor("IBMPlexMono", "Small", 0));
    return font ? font : LLFontGL::getFontMonospace();
}
} // namespace ALKitStyle
