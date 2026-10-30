#ifndef AL_HIDE_CURSOR_H
#define AL_HIDE_CURSOR_H

namespace ALHideCursor
{
// The backend supplies live client/focus scope; arming the policy uses true/true.
constexpr bool wantHidden(bool setting, bool ui_hidden, bool mouse_in_client, bool window_focused)
{
    return setting && ui_hidden && mouse_in_client && window_focused;
}
}

#endif
