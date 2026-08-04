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

#include "llscrollcontainer.h"

namespace ALScrollFocus
{
// LLScrollContainer does not reveal a descendant merely because keyboard focus
// moved to it. Register every UI control in a document so Tab navigation keeps
// the focused control inside the visible viewport.
inline void install(LLScrollContainer* scroller, LLView* document, LLView* parent)
{
    if (!scroller || !document || !parent)
    {
        return;
    }

    for (LLView* child : *parent->getChildList())
    {
        if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(child))
        {
            control->setFocusReceivedCallback(
                [scroller, document, child](LLFocusableElement*)
                {
                    const LLRect local_rect(0, child->getRect().getHeight(),
                                            child->getRect().getWidth(), 0);
                    LLRect document_rect;
                    if (child->localRectToOtherView(local_rect, &document_rect, document))
                    {
                        scroller->scrollToShowRect(document_rect);
                    }
                });
        }
        install(scroller, document, child);
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
