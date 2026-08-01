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

        Params();
    };

    explicit ALFlowGrid(const Params& p);

    bool postBuild() override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

private:
    S32 mCardWidth;
    S32 mMaxColumns;
    S32 mColumnGap;
    S32 mRowGap;
    S32 mPad;
};

#endif // AL_FLOWGRID_H
