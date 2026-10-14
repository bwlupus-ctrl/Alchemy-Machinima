#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorcastui.h"

namespace tut
{
struct director_subject_mapping_data
{
    std::array<LLSD, 5> numeric{
        LLSD(0), LLSD(1), LLSD(2), LLSD(3), LLSD(4)
    };
    std::array<LLSD, 5> letters{
        LLSD("me"), LLSD("a"), LLSD("b"), LLSD("c"), LLSD("d")
    };
};
using director_subject_mapping_group = test_group<director_subject_mapping_data>;
using director_subject_mapping_object = director_subject_mapping_group::object;
director_subject_mapping_group director_subject_mapping_tests(
    "Director subject presentation mapping");

template<> template<> void director_subject_mapping_object::test<1>()
{
    ensure_equals("stored A displays through self while self holds A",
        ALDirectorCastUI::subjectDisplayValue(LLSD(1), numeric, 1u), LLSD(0));
    ensure_equals("stored A displays as A after self leaves A",
        ALDirectorCastUI::subjectDisplayValue(LLSD(1), numeric, 0u), LLSD(1));
}

template<> template<> void director_subject_mapping_object::test<2>()
{
    ensure_equals("string-valued stored C maps through self",
        ALDirectorCastUI::subjectDisplayValue(LLSD("c"), letters, 1u << 2),
        LLSD("me"));
    ensure_equals("unrelated stored value is preserved",
        ALDirectorCastUI::subjectDisplayValue(LLSD("camera"), letters, 0xfu),
        LLSD("camera"));
}
} // namespace tut
