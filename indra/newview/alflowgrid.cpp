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

#include "alscrollfit.h"
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
    pad("pad", 8),
    fit_cards("fit_cards", false), // [DirFix]
    pack("pack", "masonry")
{
}

ALFlowGrid::ALFlowGrid(const Params& p)
:   LLPanel(p),
    mCardWidth(llmax(1, p.card_width())),
    mMaxColumns(llmax(1, p.max_columns())),
    mColumnGap(llmax(0, p.column_gap())),
    mRowGap(llmax(0, p.row_gap())),
    mPad(llmax(0, p.pad())),
    mFitCards(p.fit_cards()), // [DirFix]
    mPackRows(p.pack() == "rows")
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
    if (mPackRows)
    {
        layoutRows(width, height, called_from_parent);
        return;
    }
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

        // [DirFix] fit_cards: size the card from its own children first (visible AND hidden
        // cards, so a hidden card keeps a content-true height). Off: declared height, as before.
        const S32 card_h = mFitCards
            ? ALScrollFit::fitPanelToChildren(card, mPad)
            : card->getRect().getHeight();
        CardLayout layout{ card, card_h };
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
    // The grid may be the scroll document itself OR nested one level down in a
    // wrapper panel (e.g. panel_cine_light_rig). Walk ancestors to the nearest
    // scroll container so a nested grid floors to the real viewport instead of
    // the wrapper's stale (possibly taller) height - otherwise llmax() below
    // could never shrink the grid after a widen reflow reduced its natural
    // height, leaving a dead scroll tail.
    S32 parent_visible_height;
    LLScrollContainer* scroll = nullptr;
    for (LLView* ancestor = getParent(); ancestor; ancestor = ancestor->getParent())
    {
        scroll = dynamic_cast<LLScrollContainer*>(ancestor);
        if (scroll)
        {
            break;
        }
    }
    if (scroll)
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

void ALFlowGrid::layoutRows(S32 width, S32 height, bool called_from_parent)
{
    struct Item
    {
        LLPanel* panel;
        ALFlowGridItem* sizing;
        S32 x = 0;
        S32 top = 0;
        S32 width = 0;
        S32 height = 0;
    };
    const S32 panel_width = llmax(0, width);
    const S32 available = llmax(1, panel_width - 2 * mPad);
    const S32 columns = llclamp((available + mColumnGap) / (mCardWidth + mColumnGap), 1, mMaxColumns);
    const S32 column_width = llmax(1, (available - (columns - 1) * mColumnGap) / columns);
    std::vector<Item> items;
    items.reserve(getChildCount());
    for (child_list_const_reverse_iter_t it = getChildList()->rbegin(); it != getChildList()->rend(); ++it)
    {
        LLPanel* panel = dynamic_cast<LLPanel*>(*it);
        if (panel && panel->getVisible()) items.push_back({panel, dynamic_cast<ALFlowGridItem*>(panel)});
    }

    S32 y = mPad;
    for (size_t begin = 0; begin < items.size();)
    {
        if (items[begin].sizing && items[begin].sizing->spansAllColumns())
        {
            Item& item = items[begin];
            item.x = mPad;
            item.top = y;
            item.width = available;
            item.height = item.sizing->heightForWidth(available);
            y += item.height + mRowGap;
            ++begin;
            continue;
        }
        const size_t end = llmin(items.size(), begin + static_cast<size_t>(columns));
        S32 row_height = 1;
        for (size_t i = begin; i < end; ++i)
        {
            if (items[i].sizing && items[i].sizing->spansAllColumns()) break;
            row_height = llmax(row_height, items[i].sizing ? items[i].sizing->heightForWidth(column_width)
                                                           : items[i].panel->getRect().getHeight());
        }
        size_t count = 0;
        while (begin + count < end && !(items[begin + count].sizing && items[begin + count].sizing->spansAllColumns())) ++count;
        for (size_t i = 0; i < count; ++i)
        {
            Item& item = items[begin + i];
            item.x = mPad + static_cast<S32>(i) * (column_width + mColumnGap);
            item.top = y;
            item.width = column_width;
            item.height = item.sizing ? item.sizing->heightForWidth(column_width) : item.panel->getRect().getHeight();
        }
        y += row_height + mRowGap;
        begin += count;
    }
    const S32 natural = items.empty() ? mPad : y - mRowGap + mPad;
    const S32 final_height = natural;
    LLPanel::reshape(panel_width, final_height, called_from_parent);
    for (Item& item : items)
    {
        item.panel->reshape(item.width, item.height, true);
        item.panel->setOrigin(item.x, final_height - item.top - item.height);
    }
}
