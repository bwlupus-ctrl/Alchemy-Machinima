/**
 * @file llscrolllistgutter.h
 * @brief Pure column-area rules for a scroll list that reserves a left gutter for
 *        its selection bar (header-only, no viewer or UI dependencies).
 *
 * A list that opts into the selected-row bar draws its cells that many pixels
 * in from the row's left edge. Hit testing, hover, tooltips, cell rectangles and
 * the width columns may use must move with the drawn cells, or a click near a
 * column boundary lands on a different cell from the one displayed. A list
 * without the bar has a gutter of 0 and every function returns its input.
 */

#ifndef LL_LLSCROLLLISTGUTTER_H
#define LL_LLSCROLLLISTGUTTER_H

#include <algorithm>

namespace LLScrollListGutter
{
    // x is relative to the item list's left edge. A point inside the gutter belongs
    // to the first column (it must not fall off the left end of the column walk).
    inline int columnLocalX(int x, int gutter)
    {
        return std::max(0, x - gutter);
    }

    // Offset of a column's left edge from the item list's left edge, given the sum of
    // the widths + padding of the columns before it.
    inline int columnOffset(int columns_before, int gutter)
    {
        return columns_before + gutter;
    }

    // Width the columns may share: the list's width without the gutter.
    inline int availableWidth(int list_width, int gutter)
    {
        return std::max(0, list_width - gutter);
    }
} // namespace LLScrollListGutter

#endif // LL_LLSCROLLLISTGUTTER_H
