/**
 * @file alkitlistselection.h
 * @brief Pure rules for a multi-select kit list that mirrors one shared
 *        "primary" selection (header-only, no viewer or UI dependencies).
 *
 * The list owns the multi-selection. A model elsewhere owns one primary row
 * (the row the detail editor and in-world tool act on). These rules keep the
 * two in step without the mirror collapsing what the user selected:
 *   - a rebuild of the rows keeps every selected row that still exists;
 *   - a click makes the clicked row the primary and leaves the rest alone;
 *   - only an external change of the primary replaces the list selection.
 *
 * Id is the row key (LLUUID in the viewer). A default-constructed Id is "none".
 */

#ifndef AL_KIT_LIST_SELECTION_H
#define AL_KIT_LIST_SELECTION_H

#include <algorithm>
#include <vector>

namespace ALKitListSelection
{
    template <typename Id>
    inline bool contains(const std::vector<Id>& rows, const Id& row)
    {
        return std::find(rows.begin(), rows.end(), row) != rows.end();
    }

    // Rows to select again after the list was rebuilt: every previously selected
    // row that is still listed, in list order. Never just the first one.
    template <typename Id>
    inline std::vector<Id> restoreRows(const std::vector<Id>& selected_before,
                                       const std::vector<Id>& rows_after)
    {
        std::vector<Id> keep;
        for (const Id& row : rows_after)
        {
            if (contains(selected_before, row) && !contains(keep, row))
            {
                keep.push_back(row);
            }
        }
        return keep;
    }

    // The primary row the rest of the UI reads: the remembered primary while it is
    // still selected, otherwise the first selected row, otherwise none.
    template <typename Id>
    inline Id primaryRow(const std::vector<Id>& selected, const Id& primary)
    {
        if (!(primary == Id()) && contains(selected, primary))
        {
            return primary;
        }
        return selected.empty() ? Id() : selected.front();
    }

    // The primary after the user changed the selection. clicked is the row under
    // the pointer at mouse-up, last_selected the list's own last-selected row
    // (keyboard moves); either may be none or no longer selected (Ctrl-click off).
    template <typename Id>
    inline Id primaryAfterCommit(const std::vector<Id>& selected, const Id& clicked,
                                 const Id& last_selected, const Id& previous_primary)
    {
        for (const Id* candidate : { &clicked, &last_selected, &previous_primary })
        {
            if (!(*candidate == Id()) && contains(selected, *candidate))
            {
                return *candidate;
            }
        }
        return selected.empty() ? Id() : selected.front();
    }

    enum EMirror
    {
        MIRROR_KEEP,    // leave the list selection alone
        MIRROR_ADOPT,   // the shared primary is one of the selected rows: make it the list's primary
        MIRROR_REPLACE  // select exactly the shared primary row (or nothing)
    };

    // What the per-frame mirror of the shared primary does to the list.
    //  external_change: the shared primary was set by someone other than this list
    //                   since it last published (in-world pick, another host).
    //  gesture_active:  the pointer is down on the list; its selection is mid-change
    //                   and has not been committed yet.
    //  shared_row:      the row for the shared primary (none when nothing is shared).
    //  list_primary:    primaryRow() of the list.
    template <typename Id>
    inline EMirror mirrorAction(bool external_change, bool gesture_active, const Id& shared_row,
                                const Id& list_primary, const std::vector<Id>& selected)
    {
        if (external_change)
        {
            return MIRROR_REPLACE;
        }
        if (gesture_active || shared_row == list_primary)
        {
            return MIRROR_KEEP;
        }
        if (!(shared_row == Id()) && contains(selected, shared_row))
        {
            return MIRROR_ADOPT;
        }
        return MIRROR_REPLACE;
    }
} // namespace ALKitListSelection

#endif // AL_KIT_LIST_SELECTION_H
