#include "linden_common.h"
#include "../test/lltut.h"
#include "../alkitlistselection.h"

#include "lluuid.h"

#include <vector>

namespace tut
{
struct alkitlistselection_data
{
    const LLUUID a = LLUUID("11111111-1111-1111-1111-111111111111");
    const LLUUID b = LLUUID("22222222-2222-2222-2222-222222222222");
    const LLUUID c = LLUUID("33333333-3333-3333-3333-333333333333");
    const LLUUID d = LLUUID("44444444-4444-4444-4444-444444444444");
    const LLUUID none;
};
typedef test_group<alkitlistselection_data> alkitlistselection_group;
typedef alkitlistselection_group::object alkitlistselection_object;
alkitlistselection_group gAlKitListSelection("alkitlistselection");

using namespace ALKitListSelection;
typedef std::vector<LLUUID> Rows;

namespace
{
    // A multi-select list as the panel drives it: rows, the selected rows (list order),
    // the remembered primary and the shared primary it mirrors.
    struct FakeList
    {
        Rows rows, selected;
        LLUUID primary, shared;

        void select(const Rows& wanted)
        {
            selected = restoreRows(wanted, rows);
        }

        // The production rebuild: every selected row that still exists is selected again.
        void rebuild(const Rows& rows_after)
        {
            const Rows before = selected;
            rows = rows_after;
            selected = restoreRows(before, rows_after);
        }

        // The rebuild this fix replaces: only the first selected row was restored.
        void rebuildFirstOnly(const Rows& rows_after)
        {
            const Rows before = selected;
            rows = rows_after;
            selected.clear();
            if (!before.empty() && contains(rows_after, before.front()))
            {
                selected.push_back(before.front());
            }
        }

        // One frame of the mirror.
        void mirror(bool external_change, bool gesture_active)
        {
            switch (mirrorAction(external_change, gesture_active, shared,
                                 primaryRow(selected, primary), selected))
            {
            case MIRROR_REPLACE:
                selected.clear();
                if (contains(rows, shared)) selected.push_back(shared);
                primary = shared;
                break;
            case MIRROR_ADOPT:
                primary = shared;
                break;
            case MIRROR_KEEP:
                break;
            }
        }

        // The mirror this fix replaces: any difference from the first selected row collapsed.
        void mirrorFirstRowOnly()
        {
            const LLUUID first = selected.empty() ? LLUUID::null : selected.front();
            if (shared != first)
            {
                selected.clear();
                if (contains(rows, shared)) selected.push_back(shared);
            }
        }

        // Mouse-up commit of a click that left `now` selected with `clicked` under the pointer.
        void commit(const Rows& now, const LLUUID& clicked)
        {
            select(now);
            primary = primaryAfterCommit(selected, clicked, LLUUID::null, primary);
            shared = primary;
        }
    };

    // The property a refresh must keep: nothing the user selected is dropped while it exists.
    bool survivesRefresh(const Rows& before, const Rows& rows_after, const Rows& after)
    {
        for (const LLUUID& row : before)
        {
            if (contains(rows_after, row) && !contains(after, row))
            {
                return false;
            }
        }
        return true;
    }
}

// A rebuild keeps the whole selection; rows that vanished drop out, order follows the list.
template<> template<> void alkitlistselection_object::test<1>()
{
    ensure("all kept", restoreRows(Rows{ c, a }, Rows{ a, b, c, d }) == (Rows{ a, c }));
    ensure("vanished row dropped", restoreRows(Rows{ a, b, c }, Rows{ a, c, d }) == (Rows{ a, c }));
    ensure("nothing selected", restoreRows(Rows{}, Rows{ a, b }).empty());
    ensure("no duplicates", restoreRows(Rows{ a, a }, Rows{ a, a, b }) == (Rows{ a }));

    FakeList list{ { a, b, c, d }, {}, none, none };
    list.commit({ a, b, d }, d);
    const Rows before = list.selected;
    list.rebuild({ a, b, c, d });
    ensure("three rows survive a rebuild", list.selected == before);
    ensure("refresh property holds", survivesRefresh(before, list.rows, list.selected));
}

// Planted negative: the old first-row-only rebuild collapses a multi-selection and is rejected.
template<> template<> void alkitlistselection_object::test<2>()
{
    FakeList list{ { a, b, c, d }, {}, none, none };
    list.commit({ a, b, d }, d);
    const Rows before = list.selected;
    list.rebuildFirstOnly({ a, b, c, d });
    ensure_equals("old rebuild keeps one row", list.selected.size(), size_t(1));
    ensure("collapsed multi-select is rejected", !survivesRefresh(before, list.rows, list.selected));
}

// The clicked row becomes the primary; Ctrl-click off falls back sensibly.
template<> template<> void alkitlistselection_object::test<3>()
{
    ensure_equals("clicked row wins", primaryAfterCommit(Rows{ a, b, c }, c, a, a), c);
    ensure_equals("Shift-click upward: clicked top row", primaryAfterCommit(Rows{ a, b, c }, a, c, c), a);
    ensure_equals("Ctrl-click off: keep previous primary", primaryAfterCommit(Rows{ a, c }, b, none, a), a);
    ensure_equals("Ctrl-click off the primary: first remaining", primaryAfterCommit(Rows{ b, c }, a, none, a), b);
    ensure_equals("keyboard: last selected", primaryAfterCommit(Rows{ a, b }, none, b, a), b);
    ensure_equals("nothing left", primaryAfterCommit(Rows{}, a, a, a), none);

    ensure_equals("primary while selected", primaryRow(Rows{ a, b, c }, c), c);
    ensure_equals("stale primary: first row", primaryRow(Rows{ a, b }, d), a);
    ensure_equals("no primary: first row", primaryRow(Rows{ b, c }, none), b);
    ensure_equals("empty", primaryRow(Rows{}, a), none);
}

// The mirror: external changes replace, everything else leaves the selection alone.
template<> template<> void alkitlistselection_object::test<4>()
{
    const Rows sel{ a, b, c };
    ensure_equals("in step", mirrorAction(false, false, c, c, sel), MIRROR_KEEP);
    ensure_equals("external pick replaces", mirrorAction(true, false, d, c, sel), MIRROR_REPLACE);
    ensure_equals("external pick of a selected row still replaces", mirrorAction(true, false, a, c, sel), MIRROR_REPLACE);
    ensure_equals("external clear replaces", mirrorAction(true, false, none, c, sel), MIRROR_REPLACE);
    ensure_equals("click in progress is not touched", mirrorAction(false, true, c, a, sel), MIRROR_KEEP);
    ensure_equals("shared row is one of the selected rows", mirrorAction(false, false, b, a, sel), MIRROR_ADOPT);
    ensure_equals("shared row not selected", mirrorAction(false, false, d, a, sel), MIRROR_REPLACE);
    ensure_equals("nothing shared, rows selected", mirrorAction(false, false, none, a, sel), MIRROR_REPLACE);
    ensure_equals("nothing shared, nothing selected", mirrorAction(false, false, none, none, Rows{}), MIRROR_KEEP);
}

// The reported sequence: select C, Ctrl-click A above it, frames draw before and after
// mouse-up, the model changes, the list rebuilds. Everything stays selected.
template<> template<> void alkitlistselection_object::test<5>()
{
    FakeList list{ { a, b, c, d }, {}, none, none };
    list.commit({ c }, c);
    list.mirror(false, false);
    ensure("single click", list.selected == (Rows{ c }));

    // Ctrl-click A: the row is selected at mouse-down, the shared primary is still C.
    list.select({ a, c });
    list.mirror(false, true);
    ensure("mirror waits for the click to commit", list.selected == (Rows{ a, c }));
    list.commit({ a, c }, a);
    ensure_equals("primary follows the clicked row", list.shared, a);
    list.mirror(false, false);
    ensure("two rows after commit", list.selected == (Rows{ a, c }));

    // Shift-click D extends; then a model change rebuilds the rows.
    list.commit({ a, b, c, d }, d);
    list.mirror(false, false);
    list.rebuild({ a, b, c, d });
    list.mirror(false, false);
    ensure("four rows after extend, rebuild and mirror", list.selected == (Rows{ a, b, c, d }));
    ensure_equals("primary is the last clicked row", primaryRow(list.selected, list.primary), d);

    // An in-world pick of B is external: it replaces the list selection.
    list.shared = b;
    list.mirror(true, false);
    ensure("external pick replaces", list.selected == (Rows{ b }));
}

// Planted negative: the old mirror collapsed a Ctrl-click above the primary before mouse-up.
template<> template<> void alkitlistselection_object::test<6>()
{
    FakeList list{ { a, b, c, d }, {}, none, none };
    list.commit({ c }, c);
    const Rows wanted{ a, c };
    list.select(wanted);            // mouse-down of Ctrl-click on A
    list.mirrorFirstRowOnly();      // a frame draws: first row A != shared C
    ensure_equals("old mirror keeps one row", list.selected.size(), size_t(1));
    ensure("collapsed multi-select is rejected", !survivesRefresh(wanted, list.rows, list.selected));
}
} // namespace tut
