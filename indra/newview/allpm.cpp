/**
 * @file allpm.cpp
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

// ============================================================================
// Provenance
// ----------------------------------------------------------------------------
// The tone-curve and color-space math below is a direct, clean C++
// transcription of the A_CPU reference implementation of AMD's FidelityFX
// LPM (Luma Preserving Mapper), taken from "ffx_lpm.h"/"ffx_a.h" as bundled
// in this fork at:
//   indra/newview/app_settings/shaders/class1/deferred/LPMUtil.glsl
// (search that file for LpmSetup / LpmColRgbToXyz / LpmMatInv3x3).
//
// AMD FidelityFX LPM is distributed under the MIT license:
//   Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All rights reserved.
// The shader portability helpers (ffx_a.h) are:
//   Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
//
// We only compute control-block entries 0..9, which are the words consumed by
// the 32-bit GPU entry point LpmFilter() (the path RunLPMFilter() uses in
// LPMUtil.glsl). The FP16 packed words (16..20, read only by LpmFilterH) and
// the unused words are zero-filled; the 32-bit filter never reads them.
// ============================================================================

#include "llviewerprecompiledheaders.h"
#include "allpm.h"

#include <cmath>
#include <cstring>

namespace
{
    // --- A_CPU scalar helpers (from ffx_a.h) --------------------------------
    inline F32 lpmRcp(F32 x) { return 1.0f / x; }
    inline F32 lpmMax(F32 a, F32 b) { return a > b ? a : b; }

    // AU1_AF1: reinterpret a float's bit pattern as a uint (the control block
    // stores raw 32-bit float words that the shader reads back with AF4_AU4).
    inline U32 lpmF2U(F32 f)
    {
        U32 u;
        std::memcpy(&u, &f, sizeof(u));
        return u;
    }

    inline F32 lpmDot3(const F32 a[3], const F32 b[3])
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    // --- 3x3 matrix / color helpers (from ffx_lpm.h) ------------------------
    void lpmMatTrn3x3(F32 ox[3], F32 oy[3], F32 oz[3],
                      const F32 ix[3], const F32 iy[3], const F32 iz[3])
    {
        ox[0] = ix[0]; ox[1] = iy[0]; ox[2] = iz[0];
        oy[0] = ix[1]; oy[1] = iy[1]; oy[2] = iz[1];
        oz[0] = ix[2]; oz[1] = iy[2]; oz[2] = iz[2];
    }

    // Low-precision inverse (matches AMD reference exactly).
    void lpmMatInv3x3(F32 ox[3], F32 oy[3], F32 oz[3],
                      const F32 ix[3], const F32 iy[3], const F32 iz[3])
    {
        F32 i = lpmRcp(ix[0] * (iy[1] * iz[2] - iz[1] * iy[2])
                     - ix[1] * (iy[0] * iz[2] - iy[2] * iz[0])
                     + ix[2] * (iy[0] * iz[1] - iy[1] * iz[0]));
        ox[0] = (iy[1] * iz[2] - iz[1] * iy[2]) * i;
        ox[1] = (ix[2] * iz[1] - ix[1] * iz[2]) * i;
        ox[2] = (ix[1] * iy[2] - ix[2] * iy[1]) * i;
        oy[0] = (iy[2] * iz[0] - iy[0] * iz[2]) * i;
        oy[1] = (ix[0] * iz[2] - ix[2] * iz[0]) * i;
        oy[2] = (iy[0] * ix[2] - ix[0] * iy[2]) * i;
        oz[0] = (iy[0] * iz[1] - iz[0] * iy[1]) * i;
        oz[1] = (iz[0] * ix[1] - ix[0] * iz[1]) * i;
        oz[2] = (ix[0] * iy[1] - iy[0] * ix[1]) * i;
    }

    void lpmMatMul3x3(F32 ox[3], F32 oy[3], F32 oz[3],
                      const F32 ax[3], const F32 ay[3], const F32 az[3],
                      const F32 bx[3], const F32 by[3], const F32 bz[3])
    {
        F32 bx2[3], by2[3], bz2[3];
        lpmMatTrn3x3(bx2, by2, bz2, bx, by, bz);
        ox[0] = lpmDot3(ax, bx2); ox[1] = lpmDot3(ax, by2); ox[2] = lpmDot3(ax, bz2);
        oy[0] = lpmDot3(ay, bx2); oy[1] = lpmDot3(ay, by2); oy[2] = lpmDot3(ay, bz2);
        oz[0] = lpmDot3(az, bx2); oz[1] = lpmDot3(az, by2); oz[2] = lpmDot3(az, bz2);
    }

    void lpmColXyToZ(F32 d[3], const F32 s[2])
    {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = 1.0f - (s[0] + s[1]);
    }

    // Returns RGB->XYZ conversion matrix rows; rgbw inputs are xy chroma coords.
    void lpmColRgbToXyz(F32 ox[3], F32 oy[3], F32 oz[3],
                        const F32 r[2], const F32 g[2], const F32 b[2], const F32 w[2])
    {
        F32 rz[3], gz[3], bz[3];
        lpmColXyToZ(rz, r); lpmColXyToZ(gz, g); lpmColXyToZ(bz, b);
        F32 r3[3], g3[3], b3[3];
        lpmMatTrn3x3(r3, g3, b3, rz, gz, bz);
        F32 w3[3];
        lpmColXyToZ(w3, w);
        {
            F32 s = lpmRcp(w[1]);
            w3[0] *= s; w3[1] *= s; w3[2] *= s;
        }
        F32 rv[3], gv[3], bv[3];
        lpmMatInv3x3(rv, gv, bv, r3, g3, b3);
        F32 s[3];
        s[0] = lpmDot3(rv, w3); s[1] = lpmDot3(gv, w3); s[2] = lpmDot3(bv, w3);
        // opAMulF3: component-wise multiply.
        ox[0] = r3[0] * s[0]; ox[1] = r3[1] * s[1]; ox[2] = r3[2] * s[2];
        oy[0] = g3[0] * s[0]; oy[1] = g3[1] * s[1]; oy[2] = g3[2] * s[2];
        oz[0] = b3[0] * s[0]; oz[1] = b3[1] * s[1]; oz[2] = b3[2] * s[2];
    }

    // Rec.709 primaries + D65 white point (xy chroma coords) from ffx_lpm.h.
    const F32 kD65[2]  = { 0.3127f, 0.3290f };
    const F32 k709R[2] = { 0.64f, 0.33f };
    const F32 k709G[2] = { 0.30f, 0.60f };
    const F32 k709B[2] = { 0.15f, 0.06f };
}

namespace ALLPM
{
    void setup709(bool shoulder,
                  F32  hdrMax,
                  F32  exposure,
                  F32  contrast,
                  F32  shoulderContrast,
                  F32  saturationR, F32 saturationG, F32 saturationB,
                  F32  crosstalkR,  F32 crosstalkG,  F32 crosstalkB,
                  U32  outCtl[CONTROL_BLOCK_WORDS])
    {
        // LPM_CONFIG_709_709: con=false, soft=false, con2=false, clip=false,
        // scaleOnly=false. Working/output/container spaces are all Rec.709/D65.
        (void)shoulder; // shoulder only gates GPU-side shaping; setup is identical.

        F32 saturation[3] = { saturationR, saturationG, saturationB };
        F32 crosstalk[3]  = { crosstalkR,  crosstalkG,  crosstalkB };

        // Contrast needs to be 1.0 based for no contrast.
        contrast += 1.0f;
        // Saturation is based on contrast.
        saturation[0] += contrast;
        saturation[1] += contrast;
        saturation[2] += contrast;

        // softGap is 0 for 709->709, but must be a little above zero.
        F32 softGap = lpmMax(0.0f, 1.0f / 1024.0f);

        F32 midIn  = hdrMax * 0.18f * std::exp2(-exposure);
        F32 midOut = 0.18f;

        // Tone-scale bias.
        F32 toneScaleBias[2];
        F32 cs = contrast * shoulderContrast;
        {
            F32 z0 = -std::pow(midIn, contrast);
            F32 z1 = std::pow(hdrMax, cs) * std::pow(midIn, contrast);
            F32 z2 = std::pow(hdrMax, contrast) * std::pow(midIn, cs) * midOut;
            F32 z3 = std::pow(hdrMax, cs) * midOut;
            F32 z4 = std::pow(midIn, cs) * midOut;
            toneScaleBias[0] = -((z0 + (midOut * (z1 - z2)) * lpmRcp(z3 - z4)) * lpmRcp(z4));

            F32 w0 = std::pow(hdrMax, cs) * std::pow(midIn, contrast);
            F32 w1 = std::pow(hdrMax, contrast) * std::pow(midIn, cs) * midOut;
            F32 w2 = std::pow(hdrMax, cs) * midOut;
            F32 w3 = std::pow(midIn, cs) * midOut;
            toneScaleBias[1] = (w0 - w1) * lpmRcp(w2 - w3);
        }

        // Working-space luma (Y row of RGB->XYZ, normalized to sum to 1).
        F32 lumaW[3];
        F32 rgbToXyzXW[3], rgbToXyzYW[3], rgbToXyzZW[3];
        lpmColRgbToXyz(rgbToXyzXW, rgbToXyzYW, rgbToXyzZW, k709R, k709G, k709B, kD65);
        {
            F32 s = lpmRcp(rgbToXyzYW[0] + rgbToXyzYW[1] + rgbToXyzYW[2]);
            lumaW[0] = rgbToXyzYW[0] * s;
            lumaW[1] = rgbToXyzYW[1] * s;
            lumaW[2] = rgbToXyzYW[2] * s;
        }

        // Crosstalk luma. With soft==false this is the working-space luma.
        F32 lumaT[3];
        F32 rgbToXyzXO[3], rgbToXyzYO[3], rgbToXyzZO[3];
        lpmColRgbToXyz(rgbToXyzXO, rgbToXyzYO, rgbToXyzZO, k709R, k709G, k709B, kD65);
        // soft == false -> copy working-space Y row.
        lumaT[0] = rgbToXyzYW[0];
        lumaT[1] = rgbToXyzYW[1];
        lumaT[2] = rgbToXyzYW[2];
        {
            F32 s = lpmRcp(lumaT[0] + lumaT[1] + lumaT[2]);
            lumaT[0] *= s; lumaT[1] *= s; lumaT[2] *= s;
        }
        F32 rcpLumaT[3] = { lpmRcp(lumaT[0]), lpmRcp(lumaT[1]), lpmRcp(lumaT[2]) };

        // softGap2 (soft==false -> zero).
        F32 softGap2[2] = { 0.0f, 0.0f };

        // First conversion (con==false -> zero) and last conversion
        // (con2==false, scaleOnly==false -> zero).
        F32 conR[3]  = { 0.0f, 0.0f, 0.0f };
        F32 conG[3]  = { 0.0f, 0.0f, 0.0f };
        F32 conB[3]  = { 0.0f, 0.0f, 0.0f };
        F32 con2R[3] = { 0.0f, 0.0f, 0.0f };
        F32 con2G[3] = { 0.0f, 0.0f, 0.0f };
        F32 con2B[3] = { 0.0f, 0.0f, 0.0f };

        // ---- Pack control block (words 0..9, consumed by 32-bit LpmFilter) --
        std::memset(outCtl, 0, sizeof(U32) * CONTROL_BLOCK_WORDS);

        // map0: saturation.rgb, contrast
        outCtl[0]  = lpmF2U(saturation[0]);
        outCtl[1]  = lpmF2U(saturation[1]);
        outCtl[2]  = lpmF2U(saturation[2]);
        outCtl[3]  = lpmF2U(contrast);
        // map1: toneScaleBias.xy, lumaT.xy
        outCtl[4]  = lpmF2U(toneScaleBias[0]);
        outCtl[5]  = lpmF2U(toneScaleBias[1]);
        outCtl[6]  = lpmF2U(lumaT[0]);
        outCtl[7]  = lpmF2U(lumaT[1]);
        // map2: lumaT.z, crosstalk.rgb
        outCtl[8]  = lpmF2U(lumaT[2]);
        outCtl[9]  = lpmF2U(crosstalk[0]);
        outCtl[10] = lpmF2U(crosstalk[1]);
        outCtl[11] = lpmF2U(crosstalk[2]);
        // map3: rcpLumaT.rgb, con2R.x
        outCtl[12] = lpmF2U(rcpLumaT[0]);
        outCtl[13] = lpmF2U(rcpLumaT[1]);
        outCtl[14] = lpmF2U(rcpLumaT[2]);
        outCtl[15] = lpmF2U(con2R[0]);
        // map4: con2R.yz, con2G.xy
        outCtl[16] = lpmF2U(con2R[1]);
        outCtl[17] = lpmF2U(con2R[2]);
        outCtl[18] = lpmF2U(con2G[0]);
        outCtl[19] = lpmF2U(con2G[1]);
        // map5: con2G.z, con2B.xyz
        outCtl[20] = lpmF2U(con2G[2]);
        outCtl[21] = lpmF2U(con2B[0]);
        outCtl[22] = lpmF2U(con2B[1]);
        outCtl[23] = lpmF2U(con2B[2]);
        // map6: shoulderContrast, lumaW.rgb
        outCtl[24] = lpmF2U(shoulderContrast);
        outCtl[25] = lpmF2U(lumaW[0]);
        outCtl[26] = lpmF2U(lumaW[1]);
        outCtl[27] = lpmF2U(lumaW[2]);
        // map7: softGap2.xy, conR.xy
        outCtl[28] = lpmF2U(softGap2[0]);
        outCtl[29] = lpmF2U(softGap2[1]);
        outCtl[30] = lpmF2U(conR[0]);
        outCtl[31] = lpmF2U(conR[1]);
        // map8: conR.z, conG.xyz
        outCtl[32] = lpmF2U(conR[2]);
        outCtl[33] = lpmF2U(conG[0]);
        outCtl[34] = lpmF2U(conG[1]);
        outCtl[35] = lpmF2U(conG[2]);
        // map9: conB.xyz, 0
        outCtl[36] = lpmF2U(conB[0]);
        outCtl[37] = lpmF2U(conB[1]);
        outCtl[38] = lpmF2U(conB[2]);
        outCtl[39] = 0u;

        // Words 40..95 (control-block entries 10..23) are only read by the
        // FP16 packed path LpmFilterH(); the 32-bit LpmFilter() used by
        // RunLPMFilter() ignores them. Left zero by the memset above.
    }
}
