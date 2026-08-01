/**
 * @file alflowgrid.cpp
 * @brief Responsive masonry-style panel layout for XUI cards.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alflowgrid.h"

#include "llscrollcontainer.h"

#include <algorithm>
#include <vector>

static LLDefaultChildRegistry::Register<ALFlowGrid> r("flow_grid");
// LLScrollContainer has a restricted child registry. Register there as well so
// <flow_grid> can be its direct scroll document rather than requiring a shim.
static ScrollContainerRegistry::Register<ALFlowGrid> scroll_r("flow_grid");

ALFlowGrid::Params::Params()
:   card_width("card_width", 340),
    max_columns("max_columns", 4),
    column_gap("column_gap", 12),
    row_gap("row_gap", 12),
    pad("pad", 8)
{
}

ALFlowGrid::ALFlowGrid(const Params& p)
:   LLPanel(p),
    mCardWidth(llmax(1, p.card_width())),
    mMaxColumns(llmax(1, p.max_columns())),
    mColumnGap(llmax(0, p.column_gap())),
    mRowGap(llmax(0, p.row_gap())),
    mPad(llmax(0, p.pad()))
{
}

bool ALFlowGrid::postBuild()
{
    if (!LLPanel::postBuild())
    {
        return false;
    }

    // Children are not present during construction, so perform the initial
    // layout after XUI has built the complete card list.
    reshape(getRect().getWidth(), getRect().getHeight(), true);
    return true;
}

void ALFlowGrid::reshape(S32 width, S32 height, bool called_from_parent)
{
    struct CardLayout
    {
        LLPanel* panel;
        S32 height;
    };

    const S32 panel_width = llmax(0, width);
    const S32 available_width = llmax(1, panel_width - 2 * mPad);
    const S32 natural_stride = mCardWidth + mColumnGap;
    const S32 columns = llclamp(
        (available_width + mColumnGap) / natural_stride,
        1,
        mMaxColumns);
    const S32 column_width = llmax(
        1,
        (available_width - (columns - 1) * mColumnGap) / columns);

    std::vector<CardLayout> cards;
    std::vector<CardLayout> hidden_cards;
    cards.reserve(getChildCount());
    hidden_cards.reserve(getChildCount());
    // LLView stores the front-most child first. XUI declaration order is the
    // reverse traversal order because addChild() pushes each new child front.
    for (child_list_const_reverse_iter_t it = getChildList()->rbegin();
         it != getChildList()->rend();
         ++it)
    {
        LLPanel* card = dynamic_cast<LLPanel*>(*it);
        if (!card)
        {
            continue;
        }

        CardLayout layout{ card, card->getRect().getHeight() };
        if (card->getVisible())
        {
            cards.push_back(layout);
        }
        else
        {
            hidden_cards.push_back(layout);
        }
    }

    std::vector<S32> column_bottoms(columns, mPad);
    for (const CardLayout& card : cards)
    {
        S32 column = 0;
        for (S32 candidate = 1; candidate < columns; ++candidate)
        {
            if (column_bottoms[candidate] < column_bottoms[column])
            {
                column = candidate;
            }
        }
        column_bottoms[column] += card.height + mRowGap;
    }

    S32 grid_height = mPad;
    if (!cards.empty())
    {
        grid_height = *std::max_element(column_bottoms.begin(), column_bottoms.end())
                    - mRowGap + mPad;
    }

    // Floor the document to the viewport only when we are actually the scroll
    // document, so a short grid still fills the visible area. For any other
    // parent (or none) honor the requested height instead of ballooning to the
    // parent's full height. Subtracting the border keeps a borderless scroll
    // exact and avoids a permanent 2px "crawl" scrollbar when a host adds one.
    S32 parent_visible_height;
    if (LLScrollContainer* scroll = dynamic_cast<LLScrollContainer*>(getParent()))
    {
        parent_visible_height = scroll->getRect().getHeight() - 2 * scroll->getBorderWidth();
    }
    else
    {
        parent_visible_height = llmax(0, height);
    }
    const S32 final_height = llmax(grid_height, parent_visible_height);

    // Resize this document first. LLPanel propagates follows flags to card
    // children; the explicit placement below then establishes their final
    // top-anchored masonry positions.
    LLPanel::reshape(panel_width, final_height, called_from_parent);

    // The grid owns card sizing. Preserve even hidden cards' declared heights
    // if a caller supplied follows flags that the base reshape would stretch.
    for (const CardLayout& card : hidden_cards)
    {
        card.panel->reshape(card.panel->getRect().getWidth(), card.height, true);
    }

    std::fill(column_bottoms.begin(), column_bottoms.end(), mPad);
    for (const CardLayout& card : cards)
    {
        S32 column = 0;
        for (S32 candidate = 1; candidate < columns; ++candidate)
        {
            // Strict comparison deliberately keeps the leftmost column on ties.
            if (column_bottoms[candidate] < column_bottoms[column])
            {
                column = candidate;
            }
        }

        const S32 card_height = card.height;
        const S32 x = mPad + column * (column_width + mColumnGap);
        const S32 top_offset = column_bottoms[column];
        const S32 bottom = final_height - top_offset - card_height;

        card.panel->reshape(column_width, card_height, true);
        card.panel->setOrigin(x, bottom);
        column_bottoms[column] += card_height + mRowGap;
    }
}
