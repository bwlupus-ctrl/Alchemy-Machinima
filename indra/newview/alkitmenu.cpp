#include "llviewerprecompiledheaders.h"

#include "alkitmenu.h"

#include "llmenugl.h"
#include "llview.h"

void ALKitMenu::showContextMenu(LLView* spawning_view, LLContextMenu* menu, S32 x, S32 y)
{
    if (!spawning_view || !menu)
        return;

    S32 screen_x = 0;
    S32 screen_y = 0;
    spawning_view->localPointToScreen(x, y, &screen_x, &screen_y);
    menu->show(screen_x, screen_y, spawning_view);
}
