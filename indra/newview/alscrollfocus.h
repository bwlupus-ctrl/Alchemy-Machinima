/**
 * @file alscrollfocus.h
 * @brief Focus-reveal helpers for fixed-height documents in LLScrollContainer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_SCROLLFOCUS_H
#define AL_SCROLLFOCUS_H

#include "llfocusmgr.h"
#include "llviewerwindow.h"
#include "llscrollcontainer.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ALScrollFocus
{
namespace detail
{
// [ScrollFit] Per-control idempotency. LLFocusableElement::setFocusReceivedCallback
// APPENDS a slot, so installing twice (e.g. installAncestor from an embedded panel and a
// later tab-level install by ALScrollFit) would reveal the same control twice. The
// registry remembers, per (control, scroller), a weak token whose only strong owners are
// the copies of the installed lambda. When the control dies its signal frees the slot, the
// lambda and the token, the weak_ptr expires, and a new control that reuses the address is
// correctly treated as fresh -- no dangling-pointer false positives.
struct Entry
{
    const LLScrollContainer* scroller;
    std::weak_ptr<bool> token;
};
using Registry = std::unordered_map<const LLView*, std::vector<Entry>>;

inline Registry& registry()
{
    static Registry r;
    return r;
}

// Returns the token the new slot must own, or nullptr when (control, scroller) already
// has a live scroll-focus slot.
inline std::shared_ptr<bool> claim(const LLView* control, const LLScrollContainer* scroller,
                                   bool reveal_pointer_focus)
{
    Registry& reg = registry();
    static size_t prune_at = 256;
    if (reg.size() > prune_at) // drop entries of destroyed controls now and then
    {
        for (auto it = reg.begin(); it != reg.end();)
        {
            std::vector<Entry>& v = it->second;
            v.erase(std::remove_if(v.begin(), v.end(),
                        [](const Entry& e) { return e.token.expired(); }), v.end());
            it = v.empty() ? reg.erase(it) : std::next(it);
        }
        prune_at = std::max<size_t>(256, reg.size() * 2);
    }
    std::vector<Entry>& v = reg[control];
    for (auto it = v.begin(); it != v.end();)
    {
        if (it->token.expired())
        {
            it = v.erase(it);
            continue;
        }
        if (it->scroller == scroller)
        {
            if (std::shared_ptr<bool> existing_token = it->token.lock())
            {
                *existing_token = *existing_token && reveal_pointer_focus;
            }
            return nullptr;
        }
        ++it;
    }
    std::shared_ptr<bool> slot_token = std::make_shared<bool>(reveal_pointer_focus);
    v.push_back({scroller, slot_token});
    return slot_token;
}
} // namespace detail

// LLScrollContainer does not reveal a descendant merely because keyboard focus
// moved to it. Register every UI control in a document so Tab navigation keeps
// the focused control inside the visible viewport. [ScrollFit] Idempotent per
// (control, scroller): a control that already has a scroll-focus slot for this
// scroller is skipped, so repeated or overlapping installs never stack callbacks.
inline void install(LLScrollContainer* scroller, LLView* document, LLView* parent,
                    bool reveal_pointer_focus = true)
{
    if (!scroller || !document || !parent)
    {
        return;
    }

    for (LLView* child : *parent->getChildList())
    {
        if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(child))
        {
            if (std::shared_ptr<bool> token = detail::claim(control, scroller, reveal_pointer_focus))
            {
                control->setFocusReceivedCallback(
                    [scroller, document, child, token](LLFocusableElement*)
                    {
                        // Kit controls take focus after their button captures the mouse.
                        // Revealing then moves the document between press and release.
                        // Keyboard focus still needs the normal reveal for Tab navigation.
                        // Combos/sliders/lists take focus before (or outside) the capture, so
                        // also treat any focus arriving while the left button is down as pointer focus.
                        // Pointer focus must never synchronously move a pressed control.
                        // This applies to legacy scroll containers too: several combo,
                        // checkbox and button implementations take focus after mouse-down,
                        // and moving the document before mouse-up cancels the real click.
                        // Tab focus arrives with neither condition and still reveals.
                        if (gFocusMgr.childHasMouseCapture(child) ||
                            (gViewerWindow && gViewerWindow->getLeftMouseDown()))
                        {
                            return;
                        }
                        const LLRect local_rect(0, child->getRect().getHeight(),
                                                child->getRect().getWidth(), 0);
                        LLRect document_rect;
                        if (child->localRectToOtherView(local_rect, &document_rect, document))
                        {
                            scroller->scrollToShowRect(document_rect);
                        }
                    });
            }
        }
        install(scroller, document, child, reveal_pointer_focus);
    }
}

inline void install(LLView* owner, const char* scroller_name, const char* document_name)
{
    if (!owner)
    {
        return;
    }
    LLScrollContainer* scroller = owner->getChild<LLScrollContainer>(scroller_name);
    LLView* document = owner->getChildView(document_name);
    install(scroller, document, document);
}

// Shared panels can discover the scroll document supplied by either host.
inline void installAncestor(LLView* root)
{
    if (!root)
    {
        return;
    }

    LLView* document = root;
    for (LLView* ancestor = root->getParent(); ancestor; ancestor = ancestor->getParent())
    {
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor))
        {
            install(scroller, document, root);
            return;
        }
        document = ancestor;
    }
}
} // namespace ALScrollFocus

#endif // AL_SCROLLFOCUS_H
