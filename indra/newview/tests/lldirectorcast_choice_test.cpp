#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectoractorchoice.h"

namespace tut
{
struct director_cast_choice_data
{
    LLUUID self{ "00000000-0000-0000-0000-000000000001" };
    LLUUID other{ "00000000-0000-0000-0000-000000000002" };
    LLUUID third{ "00000000-0000-0000-0000-000000000003" };
    std::array<LLUUID, 4> empty{};
};
using director_cast_choice_group = test_group<director_cast_choice_data>;
using director_cast_choice_object = director_cast_choice_group::object;
director_cast_choice_group director_cast_choice_tests("LLDirectorCast actor choices");

template<> template<> void director_cast_choice_object::test<1>()
{
    const auto choices = alBuildActorChoices(
        self, empty, { { self, "My Resident Name" }, { other, "Other" } });
    ensure_equals("self plus other", choices.size(), size_t(2));
    ensure_equals("self first and canonical", choices[0].mLabel, std::string("You"));
}

template<> template<> void director_cast_choice_object::test<2>()
{
    auto subjects = empty;
    subjects[0] = self;
    const auto choices = alBuildActorChoices(self, subjects,
        { { self, "My Resident Name" } });
    ensure_equals("self A occurs once", choices.size(), size_t(1));
    ensure_equals("self A label", choices[0].mLabel, std::string("You (Subject A)"));
    ensure("A metadata", choices[0].isSubject(1));
}

template<> template<> void director_cast_choice_object::test<3>()
{
    auto subjects = empty;
    subjects[0] = self;
    subjects[2] = self;
    const auto choices = alBuildActorChoices(self, subjects, {});
    ensure_equals("self A+C occurs once", choices.size(), size_t(1));
    ensure_equals("self A+C label", choices[0].mLabel,
                  std::string("You (Subjects A, C)"));
    ensure("both slot bits", choices[0].isSubject(1) && choices[0].isSubject(3));
}

template<> template<> void director_cast_choice_object::test<4>()
{
    const auto choices = alBuildActorChoices(self, empty, {});
    ensure_equals("empty cast still offers You", choices.size(), size_t(1));
    ensure_equals("empty cast self id", choices[0].mId, self);
}

template<> template<> void director_cast_choice_object::test<5>()
{
    auto subjects = empty;
    subjects[1] = other;
    subjects[3] = third;
    const auto choices = alBuildActorChoices(self, subjects,
        { { other, "Other" }, { third, "Third" } });
    ensure_equals("others are deduplicated", choices.size(), size_t(3));
    ensure_equals("B label", choices[1].mLabel, std::string("Other (Subject B)"));
    ensure_equals("D label", choices[2].mLabel, std::string("Third (Subject D)"));
}

template<> template<> void director_cast_choice_object::test<6>()
{
    auto subjects = empty;
    subjects[0] = self;
    const auto choices = alBuildActorChoices(self, subjects,
        { { other, "Other" } });
    ensure_equals("self absent from cast still occurs once", choices.size(), size_t(2));
    ensure_equals("self absent label", choices[0].mLabel,
                  std::string("You (Subject A)"));
}
} // namespace tut
