/**
 * @file alflowgrid.h
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

#ifndef AL_FLOWGRID_H
#define AL_FLOWGRID_H

#include "llpanel.h"

class ALFlowGridItem
{
public:
    virtual ~ALFlowGridItem() = default;
    virtual S32 heightForWidth(S32 width) const = 0;
    virtual bool spansAllColumns() const { return false; }
};

class ALFlowGrid final : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Optional<S32> card_width;
        Optional<S32> max_columns;
        Optional<S32> column_gap;
        Optional<S32> row_gap;
        Optional<S32> pad;
        // [DirFix] H2: size every card from its own children (ALScrollFit::fitPanelToChildren,
        // pad = this grid's pad) instead of the card's declared height. Default off: grids
        // that do not opt in lay out exactly as before.
        Optional<bool> fit_cards;
        Optional<std::string> pack;

        Params();
    };

    explicit ALFlowGrid(const Params& p);

    bool postBuild() override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

private:
    void layoutRows(S32 width, S32 height, bool called_from_parent);
    S32 mCardWidth;
    S32 mMaxColumns;
    S32 mColumnGap;
    S32 mRowGap;
    S32 mPad;
    bool mFitCards; // [DirFix]
    bool mPackRows;
};

#endif // AL_FLOWGRID_H
