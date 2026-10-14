/**
 * Shared Director presentation adapters for subject-slot controls.
 */
#ifndef AL_DIRECTOR_CAST_UI_H
#define AL_DIRECTOR_CAST_UI_H

#include "stdtypes.h"
#include "llsd.h"
#include <array>

class LLComboBox;

namespace ALDirectorCastUI
{
// Rebuild the identity portion of a static Self/A/B/C/D combo without changing
// its stored value. Non-subject items are retained in their original order.
void mergeSubjectCombo(LLComboBox* combo, S32 self_value, S32 subject_a_value,
                       S32 subject_count = 4);
void mergeSubjectCombo(LLComboBox* combo,
                       const std::array<LLSD, 5>& slot_values,
                       S32 subject_count = 4);
}

#endif
