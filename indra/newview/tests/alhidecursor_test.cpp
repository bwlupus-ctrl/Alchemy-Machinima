#ifdef AL_HIDE_CURSOR_STANDALONE
#include <cstdlib>
#else
#include "linden_common.h"
#include "../test/lltut.h"
#endif
#include "../alhidecursor.h"

namespace
{
bool decisionsCorrect()
{
    // Exhaust the policy, including both UI mechanisms' shared hidden input.
    for (unsigned bits = 0; bits < 16; ++bits)
    {
        if (ALHideCursor::wantHidden((bits & 1) != 0, (bits & 2) != 0,
                (bits & 4) != 0, (bits & 8) != 0) != (bits == 15))
        {
            return false;
        }
    }
    // Releasing any condition immediately releases our override; re-entry arms it.
    return ALHideCursor::wantHidden(true, true, true, true)
        && !ALHideCursor::wantHidden(false, true, true, true)
        && !ALHideCursor::wantHidden(true, false, true, true)
        && !ALHideCursor::wantHidden(true, true, false, true)
        && !ALHideCursor::wantHidden(true, true, true, false)
        && ALHideCursor::wantHidden(true, true, true, true);
}
}

#ifdef AL_HIDE_CURSOR_STANDALONE
int main()
{
    return decisionsCorrect() ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
namespace tut
{
struct hide_cursor_data {};
using hide_cursor_group = test_group<hide_cursor_data>;
using hide_cursor_object = hide_cursor_group::object;
hide_cursor_group hide_cursor_tests("alhidecursor");
template<> template<> void hide_cursor_object::test<1>()
{
    ensure("all policy combinations and release transitions", decisionsCorrect());
}
}
#endif
