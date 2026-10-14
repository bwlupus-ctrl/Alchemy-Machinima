#include "llviewerprecompiledheaders.h"

#include "aldirectorcastui.h"

#include "llagent.h"
#include "llcombobox.h"
#include "lldirectorcast.h"
#include "llscrolllistitem.h"

#include <utility>

namespace ALDirectorCastUI
{
namespace
{
struct SavedItem
{
    std::string label;
    LLSD value;
    bool enabled;
};

bool sameValue(const LLSD& left, const LLSD& right)
{
    return left.asString() == right.asString();
}
}

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
    std::vector<SavedItem> extras;
    for (LLScrollListItem* item : combo->getAllData())
    {
        if (!item || !item->getColumn(0))
        {
            continue;
        }
        bool subject_item = sameValue(item->getValue(), slot_values[0]);
        for (S32 slot = 1; slot <= subject_count && !subject_item; ++slot)
            subject_item = sameValue(item->getValue(), slot_values[slot]);
        if (!subject_item)
        {
            extras.push_back({ item->getColumn(0)->getValue().asString(),
                               item->getValue(), item->getEnabled() });
        }
    }

    const LLSD selected = combo->getValue();
    LLDirectorCast& cast = LLDirectorCast::instance();
    const std::vector<LLDirectorCast::ActorChoice> choices = cast.getActorChoices();
    combo->clearRows();

    const LLDirectorCast::ActorChoice* self_choice = nullptr;
    for (const LLDirectorCast::ActorChoice& choice : choices)
    {
        if (LLDirectorCast::isSelf(choice.mId))
        {
            self_choice = &choice;
            break;
        }
    }
    combo->add(self_choice ? self_choice->mLabel : std::string("You"), slot_values[0]);

    S32 selected_self_slot = 0;
    for (S32 slot = 1; slot <= subject_count; ++slot)
    {
        const LLDirectorCast::ActorChoice* subject_choice = nullptr;
        for (const LLDirectorCast::ActorChoice& choice : choices)
        {
            if (choice.isSubject(slot))
            {
                subject_choice = &choice;
                break;
            }
        }
        if (subject_choice && LLDirectorCast::isSelf(subject_choice->mId))
        {
            if (sameValue(selected, slot_values[slot]))
                selected_self_slot = slot;
            continue;
        }
        const char letter = static_cast<char>('A' + slot - 1);
        std::string label = llformat("Subject %c \xE2\x80\x93 ", letter);
        if (!subject_choice)
        {
            label += "not set";
        }
        else
        {
            const LLDirectorCast::CastMember* member = cast.getMember(subject_choice->mId);
            label += member && !member->mLastName.empty()
                ? member->mLastName : subject_choice->mId.asString();
        }
        combo->add(label, slot_values[slot]);
    }
    for (const SavedItem& item : extras)
    {
        combo->add(item.label, item.value, ADD_BOTTOM, item.enabled);
    }
    combo->setValue(selected_self_slot > 0 ? slot_values[0] : selected);
}

void mergeActorCombo(LLComboBox* combo, const LLSD& self_value,
                     const std::string& actor_value_prefix,
                     const std::string& self_label,
                     const std::string& actor_label_prefix)
{
    if (!combo)
        return;

    const LLSD selected = combo->getValue();
    std::vector<SavedItem> before;
    std::vector<SavedItem> after;
    bool saw_identity = false;
    for (LLScrollListItem* item : combo->getAllData())
    {
        if (!item || !item->getColumn(0))
            continue;
        const std::string value = item->getValue().asString();
        const bool identity = sameValue(item->getValue(), self_value) ||
            value.rfind(actor_value_prefix, 0) == 0;
        if (identity)
        {
            saw_identity = true;
            continue;
        }
        SavedItem saved{ item->getColumn(0)->getValue().asString(),
                         item->getValue(), item->getEnabled() };
        (saw_identity ? after : before).push_back(std::move(saved));
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool self_is_actor = cast.getMember(gAgentID) != nullptr;
    const std::string self_actor_value = actor_value_prefix + gAgentID.asString();
    combo->clearRows();
    for (const SavedItem& item : before)
        combo->add(item.label, item.value, ADD_BOTTOM, item.enabled);
    combo->add(self_is_actor ? self_label + " (Actor: you)" : self_label,
               self_value);
    for (const LLDirectorCast::ActorChoice& choice : cast.getActorChoices())
    {
        if (!LLDirectorCast::isSelf(choice.mId))
            combo->add(actor_label_prefix + choice.mLabel,
                       actor_value_prefix + choice.mId.asString());
    }
    for (const SavedItem& item : after)
        combo->add(item.label, item.value, ADD_BOTTOM, item.enabled);

    combo->setValue(self_is_actor && selected.asString() == self_actor_value
        ? self_value : selected);
}
}
