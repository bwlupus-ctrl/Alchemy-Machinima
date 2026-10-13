#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldirectorqol.h"

namespace tut
{
struct director_qol_data {};
using director_qol_group = test_group<director_qol_data>;
using director_qol_object = director_qol_group::object;
director_qol_group director_qol_tests("ALDirectorQol");

template<> template<> void director_qol_object::test<1>()
{
    ensure("exact beats prefix", ALDirectorQol::fuzzyTokenScore("rim", "rim") >
                                  ALDirectorQol::fuzzyTokenScore("rim", "rimming"));
    ensure("prefix beats typo", ALDirectorQol::fuzzyTokenScore("expo", "exposure") >
                                ALDirectorQol::fuzzyTokenScore("expsoure", "exposure"));
    ensure("transposition typo matches", ALDirectorQol::fuzzyTokenScore("expsoure", "exposure") > 0);
    ensure("subsequence matches", ALDirectorQol::fuzzyTokenScore("shdw", "shadow") > 0);
    ensure("closer typo ranks higher",
           ALDirectorQol::fuzzyTokenScore("exposore", "exposure") >
           ALDirectorQol::fuzzyTokenScore("expasore", "exposure"));
    ensure_equals("noise rejected", ALDirectorQol::fuzzyTokenScore("zzz", "exposure"), 0);
}

template<> template<> void director_qol_object::test<2>()
{
    const LLUUID a("00000000-0000-0000-0000-000000000001");
    const LLUUID b("00000000-0000-0000-0000-000000000002");
    const LLUUID c("00000000-0000-0000-0000-000000000003");
    const LLUUID d("00000000-0000-0000-0000-000000000004");
    const LLUUID e("00000000-0000-0000-0000-000000000005");
    const std::vector<LLUUID> assigned = ALDirectorQol::subjectAssignment({a, b, c, d, e});
    ensure_equals("four subjects", assigned.size(), size_t(4));
    ensure_equals("roster order A", assigned[0], a);
    ensure_equals("roster order D", assigned[3], d);
}

template<> template<> void director_qol_object::test<3>()
{
    using Action = ALDirectorQol::RecoveryAction;
    ensure("no file is no-op", ALDirectorQol::recoveryAction(false, false) == Action::NONE);
    ensure("valid recovery is offered", ALDirectorQol::recoveryAction(true, true) == Action::OFFER_RESTORE);
    ensure("corrupt recovery is discarded", ALDirectorQol::recoveryAction(true, false) == Action::DISCARD_CORRUPT);
}

template<> template<> void director_qol_object::test<4>()
{
    using Field = ALDirectorQol::SearchField;
    ensure("setting-name fragment matches",
           ALDirectorQol::searchTokenScore("render", "render", Field::SETTING) > 0);
    ensure("setting name is a strong field",
           ALDirectorQol::searchTokenScore("bloom", "bloom", Field::SETTING) >
           ALDirectorQol::searchTokenScore("bloom", "bloom", Field::KEYWORD));
    ensure("expanded synonym remains searchable but below a direct match",
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::SYNONYM) > 0 &&
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::SYNONYM) <
           ALDirectorQol::searchTokenScore("brightness", "brightness", Field::LABEL));
    ensure("Director wins an equal-score tie",
           ALDirectorQol::searchResultBefore(200, false, 8, 200, true, 1));
    ensure("external result does not win an equal-score tie",
           !ALDirectorQol::searchResultBefore(200, true, 1, 200, false, 8));
}
} // namespace tut
