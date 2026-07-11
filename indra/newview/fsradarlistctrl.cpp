/**
 * @file fsradarlistctrl.cpp
 * @brief A radar-specific implementation of scrolllist
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2011 Arrehn Oberlander
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 *
 * Ported from Firestorm (I:\enve, indra/newview/fsradarlistctrl.cpp) to
 * Alchemy Machinima as part of the BD/FS -> Alchemy merge campaign, item F3.
 * See fsradarlistctrl.h for the FSScrollListCtrl consolidation note.
 */

#include "llviewerprecompiledheaders.h"

#include "fsradarlistctrl.h"
#include "llscrolllistitem.h"
#include "lltooldraganddrop.h"
#include "rlvhandler.h"

static LLDefaultChildRegistry::Register<FSRadarListCtrl> r("radar_list");

FSRadarListCtrl::FSRadarListCtrl(const Params& p)
:   LLScrollListCtrl(p),
    mContextMenu(nullptr),
    mDesiredLineHeight(p.desired_line_height),
    mContentType(p.content_type)
{
}

void FSRadarListCtrl::refreshLineHeight()
{
    if (mDesiredLineHeight > -1)
    {
        setLineHeight(mDesiredLineHeight);
    }
    else
    {
        updateLineHeight();
    }
    updateLayout();
}

bool FSRadarListCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    bool handled = LLUICtrl::handleRightMouseDown(x, y, mask);
    if ( (mContextMenu) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
    {
        if (std::vector<LLScrollListItem*> selected_items = getAllSelected(); selected_items.size() > 1)
        {
            uuid_vec_t selected_uuids;
            for (auto selected_item : selected_items)
            {
                selected_uuids.emplace_back(selected_item->getUUID());
            }
            mContextMenu->show(this, selected_uuids, x, y);
        }
        else
        {
            if (LLScrollListItem* hit_item = hitItem(x, y); hit_item)
            {
                selectByID(hit_item->getValue());
                LLUUID av = hit_item->getValue();
                uuid_vec_t selected_uuids;
                selected_uuids.emplace_back(av);
                mContextMenu->show(this, selected_uuids, x, y);
            }
        }
    }
    return handled;
}

bool FSRadarListCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (mContentType == FSRadarListCtrl::AGENTS)
    {
        gFocusMgr.setMouseCapture(this);

        S32 screen_x;
        S32 screen_y;
        localPointToScreen(x, y, &screen_x, &screen_y);
        LLToolDragAndDrop::getInstance()->setDragStart(screen_x, screen_y);
    }

    return LLScrollListCtrl::handleMouseDown(x, y, mask);
}

bool FSRadarListCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (mContentType == FSRadarListCtrl::AGENTS)
    {
        if (hasMouseCapture())
        {
            gFocusMgr.setMouseCapture(NULL);
        }
    }

    return LLScrollListCtrl::handleMouseUp(x, y, mask);
}

bool FSRadarListCtrl::handleHover(S32 x, S32 y, MASK mask)
{
    if (mContentType == FSRadarListCtrl::AGENTS)
    {
        bool handled = hasMouseCapture();
        if (handled)
        {
            S32 screen_x;
            S32 screen_y;
            localPointToScreen(x, y, &screen_x, &screen_y);

            if (LLToolDragAndDrop::getInstance()->isOverThreshold(screen_x, screen_y))
            {
                std::vector<EDragAndDropType> types;
                uuid_vec_t cargo_ids;
                typedef std::vector<LLScrollListItem*> item_vec_t;
                item_vec_t selected_items = getAllSelected();
                for (item_vec_t::const_iterator it = selected_items.begin(); it != selected_items.end(); ++it)
                {
                    cargo_ids.push_back((*it)->getUUID());
                }
                types.resize(cargo_ids.size(), DAD_PERSON);
                LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_PEOPLE;
                LLToolDragAndDrop::getInstance()->beginMultiDrag(types, cargo_ids, src);
            }
            return handled;
        }

        return LLScrollListCtrl::handleHover(x, y, mask);
    }

    return LLScrollListCtrl::handleHover(x, y, mask);
}
