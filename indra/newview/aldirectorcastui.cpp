#include "llviewerprecompiledheaders.h"

#include "aldirectorcastui.h"

#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llscrolllistitem.h"

namespace ALDirectorCastUI
{
void mergeSubjectCombo(LLComboBox* combo, S32 self_value, S32 subject_a_value,
                       S32 subject_count)
{
    std::array<LLSD, 5> values;
    values[0] = self_value;
    for (S32 slot = 1; slot <= 4; ++slot)
        values[slot] = subject_a_value + slot - 1;
    mergeSubjectCombo(combo, values, subject_count);
}

void mergeSubjectCombo(LLComboBox* combo,
                       const std::array<LLSD, 5>& slot_values,
                       S32 subject_count)
{
    if (!combo)
    {
        return;
    }
    struct SavedItem
    {
        std::string label;
        LLSD value;
        bool enabled;
    };
    std::vector<SavedItem> extras;
    for (LLScrollListItem* item : combo->getAllData())
    {
        if (!item || !item->getColumn(0))
        {
            continue;
        }
        bool subject_item = item->getValue().asString() == slot_values[0].asString();
        for (S32 slot = 1; slot <= subject_count && !subject_item; ++slot)
            subject_item = item->getValue().asString() == slot_values[slot].asString();
        if (!subject_item)
        {
            extras.push_back({ item->getColumn(0)->getValue().asString(),
                               item->getValue(), item->getEnabled() });
        }
    }

    const LLSD selected = combo->getValue();
    LLDirectorCast& cast = LLDirectorCast::instance();
    combo->clearRows();
    for (const LLDirectorCast::ActorChoice& choice : cast.getActorChoices())
    {
        U8 visible_subjects = 0;
        for (S32 slot = 1; slot <= subject_count; ++slot)
        {
            if (choice.isSubject(slot)) visible_subjects |= U8(1u << (slot - 1));
        }
        if (!LLDirectorCast::isSelf(choice.mId) && visible_subjects == 0)
        {
            continue;
        }
        LLSD value = LLDirectorCast::isSelf(choice.mId) ? slot_values[0]
                                                        : slot_values[1];
        for (S32 slot = 1; slot <= subject_count; ++slot)
        {
            if (choice.isSubject(slot))
            {
                value = slot_values[slot];
                break;
            }
        }
        S32 selected_slot = 0;
        for (S32 slot = 1; slot <= subject_count; ++slot)
            if (selected.asString() == slot_values[slot].asString()) selected_slot = slot;
        if (selected_slot > 0 && choice.isSubject(selected_slot))
        {
            value = selected;
        }
        else if (LLDirectorCast::isSelf(choice.mId) &&
                 selected.asString() == slot_values[0].asString())
        {
            value = slot_values[0];
        }
        combo->add(choice.mLabel, LLSD(value));
    }
    for (const SavedItem& item : extras)
    {
        combo->add(item.label, item.value, ADD_BOTTOM, item.enabled);
    }
    combo->setValue(selected);
}
}
