/**
 * @file fsradarlistctrl.h
 * @brief A radar-specific scrolllist implementation, so we can subclass custom methods.
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
 * Ported from Firestorm (I:\enve, indra/newview/fsradarlistctrl.h) to Alchemy
 * Machinima as part of the BD/FS -> Alchemy merge campaign, item F3.
 * Alchemy has no FSScrollListCtrl base class, so this now derives directly
 * from the stock LLScrollListCtrl and folds in the small subset of
 * FSScrollListCtrl behavior the radar list needs (right-click multi-select
 * context menu dispatch, agent drag-and-drop start, desired line height).
 */

#ifndef FS_RADARLISTCTRL_H
#define FS_RADARLISTCTRL_H

#include "lllistcontextmenu.h"
#include "llscrolllistctrl.h"

class FSRadarListCtrl
: public LLScrollListCtrl, public LLInstanceTracker<FSRadarListCtrl>
{
public:
    using LLScrollListCtrl::setContextMenu;

    typedef enum e_content_type
    {
        AGENTS,
        MISC
    } EContentType;

    struct ContentTypeNames : public LLInitParam::TypeValuesHelper<FSRadarListCtrl::EContentType, ContentTypeNames>
    {
        static void declareValues()
        {
            declare("Agents", FSRadarListCtrl::AGENTS);
            declare("Misc", FSRadarListCtrl::MISC);
        }
    };

    struct Params : public LLInitParam::Block<Params, LLScrollListCtrl::Params>
    {
        Optional<S32>                            desired_line_height;
        Optional<EContentType, ContentTypeNames> content_type;

        Params()
        :   desired_line_height("desired_line_height", -1),
            content_type("content_type", MISC)
        {}
    };

    virtual ~FSRadarListCtrl() = default;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;

    void setContextMenu(LLListContextMenu* menu) { mContextMenu = menu; }
    void refreshLineHeight();

protected:
    friend class LLUICtrlFactory;
    FSRadarListCtrl(const Params&);

    LLListContextMenu*  mContextMenu;
    S32                  mDesiredLineHeight;
    EContentType         mContentType;
};

#endif // FS_RADARLISTCTRL_H
