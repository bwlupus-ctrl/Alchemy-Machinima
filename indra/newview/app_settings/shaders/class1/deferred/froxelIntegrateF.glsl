/**
 * @file class1/deferred/froxelIntegrateF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 *
 * [BDMerge Froxel F1] P4 integrate pass. One full-atlas draw: every output atlas
 * fragment is one froxel (fx,fy,k). Front-to-back accumulate the media column
 * j = 0..k with Hillaire's energy-conserving slice integral and write
 *   rgb = integrated in-scatter radiance L from the camera to froxel k's slice-CENTRE
 *   a   = transmittance T over the same span.
 * Reads ONLY the media atlas (rgb = sigma_s, a = sigma_t) - a DIFFERENT texture than
 * the write target - so there is no GL feedback-loop hazard; no 64-draw slice-scan.
 * Source radiance this batch is uniform ambient only: S_j = froxel_ambient * sigma_s.
 * F2 will add per-light in-scatter to S_j.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D froxelMedia;   // RGBA16F media atlas (rgb = sigma_s, a = sigma_t)

uniform vec3  froxel_grid;       // (GridX, GridY, GridZ)
uniform vec4  froxel_atlas;      // (tilesX, tilesY, atlasW, atlasH)
uniform vec2  froxel_near_far;   // (camera near, BDMergeFroxelFar) metres
uniform float froxel_ambient;    // uniform ambient in-scatter radiance (BDMergeFroxelAmbient)

// froxelUtil.glsl
float froxelSliceToViewZ(float slice, vec2 nf, float gz);

// Constant loop bound (GridZ is clamped to [16,128] C++-side); the real work stops at
// j > k via the break, so this only sets the compiler's upper limit.
const int FROXEL_MAX_SLICES = 128;

void main()
{
    int gx     = int(froxel_grid.x);
    int gy     = int(froxel_grid.y);
    int gz     = int(froxel_grid.z);
    int tilesX = int(froxel_atlas.x);

    // Decode the atlas pixel -> tile (tx,ty) -> froxel (fx,fy,k). gl_FragCoord is
    // pixel-centred; integer truncation recovers the froxel index (same convention
    // as the media pass).
    ivec2 apix = ivec2(gl_FragCoord.xy);
    int   tx   = apix.x / gx;
    int   ty   = apix.y / gy;
    int   fx   = apix.x - tx * gx;
    int   fy   = apix.y - ty * gy;
    int   k    = ty * tilesX + tx; // this fragment's slice index

    // Dead tiles past the last real slice: no medium, full transmittance.
    if (k >= gz)
    {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Front-to-back accumulation from the camera (near plane = slice boundary 0) to
    // froxel k's slice-CENTRE (boundary offset k+0.5). Storing to the centre keeps
    // the integrate output aligned with the apply pass's -0.5 slice-centre sampling
    // convention (an integer slice coordinate lands on a stored slice centre).
    vec3  L = vec3(0.0);
    float T = 1.0;

    for (int j = 0; j < FROXEL_MAX_SLICES; ++j)
    {
        if (j > k)
        {
            break;
        }

        // Media at (fx,fy,j): the column is atlas-aligned, so a point texelFetch of
        // the j-th tile's (fx,fy) texel is exact - no filtering needed.
        int jtx = j - (j / tilesX) * tilesX; // j % tilesX
        int jty = j / tilesX;
        vec4 m  = texelFetch(froxelMedia, ivec2(jtx * gx + fx, jty * gy + fy), 0);
        vec3  sigma_s = m.rgb;
        float sigma_t = m.a;

        // Metric slice thickness d_j (exponential mapping). The last slice (j==k)
        // contributes only its first half so the running integral lands on the
        // centre, not the far boundary.
        float zCur  = froxelSliceToViewZ(float(j), froxel_near_far, froxel_grid.z);
        float zNext = (j < k) ? froxelSliceToViewZ(float(j + 1),   froxel_near_far, froxel_grid.z)
                              : froxelSliceToViewZ(float(j) + 0.5,  froxel_near_far, froxel_grid.z);
        float d_j   = max(zNext - zCur, 0.0);

        // Source radiance this slice: uniform ambient in-scatter (F2 adds lights).
        vec3 S_j = froxel_ambient * sigma_s;

        // Hillaire's energy-conserving slice integral (NOT point-sampled S*d, which
        // bands). As sigma_t -> 0 the max() floors the divisor at 1e-5 and contrib ->
        // S_j * d_j (the correct thin-medium limit) since (1 - e^-x)/x -> 1.
        float e     = exp(-sigma_t * d_j);
        vec3  contrib = (S_j - S_j * e) / max(sigma_t, 1.0e-5);
        L += T * contrib;
        T *= e;
    }

    frag_color = vec4(L, T);
}
