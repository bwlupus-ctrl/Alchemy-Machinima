/**
 * @file alghostplacementadapter.h
 * @brief Thin viewer adapter supplying real probes to
 *        ALGhostPlacementResolver -- see alghostplacementresolver.h.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * This is deliberately the ONE place every Ghost Studio placement tool
 * (crowd hover/click/Enter in altoolcrowdplace.cpp, single-ghost click in
 * altoolghostplace.cpp) builds real probes from, so the call sites cannot
 * quietly drift apart on what counts as a miss again -- that disagreement
 * was the original defect.
 *
 * Deliberately NOT part of the pure test target: it touches gViewerWindow /
 * LLWorld / gAgent, so it lives in its own translation unit outside
 * alghostplacementresolver.cpp (which stays link-clean for the isolated unit
 * test project), mirroring how altoolcrowdplace.cpp itself glues
 * pickImmediate into ALGhostInteractionState's pure reducer.
 */
#ifndef AL_ALGHOSTPLACEMENTADAPTER_H
#define AL_ALGHOSTPLACEMENTADAPTER_H

#include "alghostplacementresolver.h"

namespace ALGhostPlacementAdapter
{
    // A resolver Request for the camera ray through screen pixel (x, y).
    // work_plane_height_z should be the caller's best "last-valid / source-
    // foot" height (see alghostplacementresolver.h); max_distance_meters
    // bounds the free-space plane/camera-depth rungs.
    ALGhostPlacementResolver::Request screenRequest(
        S32 x, S32 y, F64 work_plane_height_z,
        F64 max_distance_meters = 128.0);

    // Real probes for the request above: pickImmediate(x, y, ...) for the
    // surface rung, LLWorld terrain/region lookups for the terrain rung and
    // the legality region-bounds check, and the region's water height.
    //
    // Legality here is deliberately just "does a loaded region exist at this
    // point" (clamped into the agent's current region when it does not):
    // Ghost Studio instances are client-only visuals (overlay ghosts /
    // client-only entity clones, see alghoststudio.h) that are never
    // server-rezzed, so a land-use "no build" flag has no bearing on them --
    // only region membership does.
    ALGhostPlacementResolver::Probes realProbes(S32 x, S32 y);

} // namespace ALGhostPlacementAdapter

#endif // AL_ALGHOSTPLACEMENTADAPTER_H
