/**
 * Shared Director presentation adapters for subject-slot controls.
 */
#ifndef AL_DIRECTOR_CAST_UI_H
#define AL_DIRECTOR_CAST_UI_H

#include "stdtypes.h"
#include "llsd.h"
#include <array>
#include <functional>
#include <string>

class LLComboBox;

namespace ALDirectorCastUI
{
using ValueProvider = std::function<LLSD()>;

// Pure stored-value -> presentation-value mapping. A subject slot occupied by
// self is displayed through the canonical self row, without changing storage.
inline LLSD subjectDisplayValue(const LLSD& stored_value,
                                const std::array<LLSD, 5>& slot_values,
                                U32 self_subject_mask,
                                S32 subject_count = 4)
{
    for (S32 slot = 1; slot <= subject_count; ++slot)
    {
        if ((self_subject_mask & (1u << (slot - 1))) != 0 &&
            stored_value.asString() == slot_values[slot].asString())
        {
            return slot_values[0];
        }
    }
    return stored_value;
}

// Show an authoritative stored subject value without committing the mapped
// presentation value back to its control or model.
void showStoredValue(LLComboBox* combo, const LLSD& stored_value,
                     S32 self_value, S32 subject_a_value,
                     S32 subject_count = 4);
void showStoredValue(LLComboBox* combo, const LLSD& stored_value,
                     const std::array<LLSD, 5>& slot_values,
                     S32 subject_count = 4);

// Rebuild the identity portion of a static Self/A/B/C/D combo without changing
// its stored value. Bound controls supply that authoritative value automatically;
// model-backed controls must supply value_provider. Non-subject items are kept.
void mergeSubjectCombo(LLComboBox* combo, S32 self_value, S32 subject_a_value,
                       S32 subject_count = 4,
                       const ValueProvider& value_provider = ValueProvider());
void mergeSubjectCombo(LLComboBox* combo,
                       const std::array<LLSD, 5>& slot_values,
                       S32 subject_count = 4,
                       const ValueProvider& value_provider = ValueProvider());

// Merge a static self/actor picker without changing the model value. The self
// row keeps self_value even when the agent is also stored as an actor.
void mergeActorCombo(LLComboBox* combo, const LLSD& self_value,
                     const std::string& actor_value_prefix,
                     const std::string& self_label,
                     const std::string& actor_label_prefix);
}

#endif
