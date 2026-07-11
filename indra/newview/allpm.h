/**
 * @file allpm.h
 * @brief CPU-side setup for AMD FidelityFX LPM (Luma Preserving Mapper) tonemapper.
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Alchemy Machinima Viewer Source Code
 * Copyright (C) 2024, Rye Mutt<rye@alchemyviewer.org>
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
 * $/LicenseInfo$
 */

// The control-block math in allpm.cpp is a clean C++ transcription of the
// A_CPU reference code in AMD's FidelityFX "ffx_lpm.h" / "ffx_a.h"
// (bundled here as class1/deferred/LPMUtil.glsl). See that file for the
// original AMD MIT license text.

#ifndef AL_LPM_H
#define AL_LPM_H

#include "stdtypes.h"

namespace ALLPM
{
    // Number of U32 words in the LPM control block: 24 uvec4 == 96 uints.
    // Uploaded to the shader's `uniform uvec4 tonemap_amd[24]`.
    constexpr U32 CONTROL_BLOCK_WORDS = 24 * 4;

    // Computes the LPM control block for the Rec.709 working -> Rec.709 output
    // configuration (LPM_CONFIG_709_709 / LPM_COLORS_709_709), matching the
    // RunLPMFilter() call site in LPMUtil.glsl.
    //
    // shoulder          : enable optional shoulderContrast shaping (fast path = false).
    // hdrMax            : maximum input value.
    // exposure          : stops between hdrMax and 18% mid-level input.
    // contrast          : extra contrast, 0 (none) .. 1 (maximum).
    // shoulderContrast  : shoulder shaping, 1.0 = no change (fast path).
    // saturation[R/G/B] : per-channel saturation, <0 decrease, 0 none, >0 increase.
    // crosstalk[R/G/B]  : over-exposure hue shaping (one channel must be 1.0).
    // outCtl            : receives CONTROL_BLOCK_WORDS uints.
    void setup709(bool shoulder,
                  F32  hdrMax,
                  F32  exposure,
                  F32  contrast,
                  F32  shoulderContrast,
                  F32  saturationR, F32 saturationG, F32 saturationB,
                  F32  crosstalkR,  F32 crosstalkG,  F32 crosstalkB,
                  U32  outCtl[CONTROL_BLOCK_WORDS]);
}

#endif // AL_LPM_H
