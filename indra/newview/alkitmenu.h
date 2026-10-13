#ifndef AL_KIT_MENU_H
#define AL_KIT_MENU_H

#include "stdtypes.h"

class LLContextMenu;
class LLView;

namespace ALKitMenu
{
void showContextMenu(LLView* spawning_view, LLContextMenu* menu, S32 x, S32 y);
}

#endif
