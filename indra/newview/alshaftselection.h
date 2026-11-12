/** One raw-selection projector pick for the menu and shaft status page. */
#ifndef AL_SHAFT_SELECTION_H
#define AL_SHAFT_SELECTION_H
#include "llselectmgr.h"
#include "llprismlens.h"
#include "llvovolume.h"
inline LLVOVolume* alSelectedProjector(bool& hasSelection)
{
    hasSelection = false;
    LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
    if (selection.isNull()) return nullptr;
    // Raw nodes: client-only flags also work before ObjectProperties arrives,
    // including other people's no-mod fixtures. Selected prim, then children.
    for (auto it = selection->begin(); it != selection->end(); ++it)
    {
        LLSelectNode* node = *it;
        LLViewerObject* object = node ? node->getObject() : nullptr;
        if (!object) continue;
        hasSelection = true;
        LLVector3 params;
        auto* volume = dynamic_cast<LLVOVolume*>(object);
        if (volume && LLPrismLens::projectorRenderParams(volume, params)) return volume;
        for (const auto& child : object->getChildren())
        {
            volume = dynamic_cast<LLVOVolume*>(child.get());
            if (volume && LLPrismLens::projectorRenderParams(volume, params)) return volume;
        }
    }
    return nullptr;
}
#endif
