/**
 * @file class1/deferred/froxelUtil.glsl
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
 */

// [BDMerge Froxel F0] Shared helper for the hybrid froxel-volumetrics grid. Pure
// function library, no main() - attached as a second fragment object to the froxel
// programs the same way deferredUtil is attached (see llviewershadermgr.cpp). All
// symbols are froxel-prefixed so they cannot collide with any other unit.
//
// ---- Conventions (binding for every froxel pass) -------------------------------
//  Grid dims                : GRID = (GX, GY, GZ) integer froxel counts.
//  Froxel integer coord     : (fx, fy, fz), fx in [0,GX), fy in [0,GY), fz in [0,GZ).
//  View space               : camera at the origin looking down -Z; "view depth" z
//                             is the POSITIVE distance in front of the eye (metres).
//  Exponential Z            : slice boundary i (i in [0,GZ]) sits at view depth
//                               z(i) = near * (far/near)^(i/GZ).
//                             A froxel's CENTRE uses i = fz + 0.5.
//  Atlas                    : the GZ Z-slices are packed into one 2D texture as a
//                             tilesX x tilesY grid of GX x GY tiles. Slice fz lives
//                             in tile (tx, ty) with tx = fz % tilesX, ty = fz / tilesX
//                             (row-major). Its atlas pixel origin is (tx*GX, ty*GY).
//  Half-texel inset         : bilinear taps clamp the in-tile position to
//                             [0.5, GX-0.5] x [0.5, GY-0.5] so a tap can never bleed
//                             into a neighbouring slice's tile at the tile border.
//
//  atlasP packs the atlas layout as (tilesX, tilesY, atlasW, atlasH).

// ---- Exponential slice depth <-> continuous slice coordinate -------------------

// Continuous slice coordinate (float, can be fractional e.g. fz+0.5 for a centre)
// -> view depth in metres. nf = (near, far).
float froxelSliceToViewZ(float slice, vec2 nf, float gz)
{
    return nf.x * pow(nf.y / nf.x, slice / gz);
}

// Inverse: view depth in metres -> continuous slice coordinate. This is the exact
// analytic inverse of froxelSliceToViewZ. Values < 0 or > gz mean the depth lies
// nearer than the near plane / beyond froxel_far respectively.
float froxelViewZToSlice(float z, vec2 nf, float gz)
{
    return gz * log(max(z, 1.0e-4) / nf.x) / log(nf.y / nf.x);
}

// ---- Froxel centre -> view-space position --------------------------------------

// Reconstruct the view-space position of a froxel sample. fxy is the continuous
// froxel xy coordinate (e.g. fx+0.5, fy+0.5 for a centre); viewz is the froxel's
// view depth (metres, positive); thf = (tan(fovx/2), tan(fovy/2)). The grid tiles
// the camera frustum, so froxel xy maps linearly onto NDC xy.
vec3 froxelViewPos(vec2 fxy, float viewz, vec3 grid, vec2 thf)
{
    vec2 ndc = (fxy / grid.xy) * 2.0 - 1.0; // [-1,1] across the frustum
    return vec3(ndc * thf * viewz, -viewz); // -Z is in front of the eye
}

// ---- Atlas addressing ----------------------------------------------------------

// UV of a bilinear tap inside slice `sliceIdx` at in-tile position `local`
// (local.xy in [0,GX]x[0,GY]). Applies the half-texel inset so the tap stays
// inside the tile.
vec2 froxelSliceUV(float sliceIdx, vec2 local, vec3 grid, vec4 atlasP)
{
    float si = clamp(sliceIdx, 0.0, grid.z - 1.0);
    float tx = mod(si, atlasP.x);
    float ty = floor(si / atlasP.x);
    vec2  loc = clamp(local, vec2(0.5), grid.xy - vec2(0.5));
    vec2  pixel = vec2(tx, ty) * grid.xy + loc;
    return pixel / atlasP.zw;
}

// Bilinear tap of one slice (the atlas texture's own filtering does the in-tile
// bilinear; the inset above keeps it inside the tile).
vec4 froxelBilinear(sampler2D atlas, float sliceIdx, vec2 local, vec3 grid, vec4 atlasP)
{
    return texture(atlas, froxelSliceUV(sliceIdx, local, grid, atlasP));
}

// Manual trilinear: two bilinear slice taps + a lerp in Z. `fc` is the continuous
// froxel coordinate in SAMPLE-CENTRE space, i.e. fc.z is already offset so that an
// integer value lands on a stored slice centre (caller subtracts 0.5 from the raw
// slice coordinate). Written and compiled now; the integrate/apply batches use it.
vec4 froxelTrilinear(sampler2D atlas, vec3 fc, vec3 grid, vec4 atlasP)
{
    float k0    = floor(fc.z);
    float fracz = clamp(fc.z - k0, 0.0, 1.0);
    vec4  a = froxelBilinear(atlas, k0,       fc.xy, grid, atlasP);
    vec4  b = froxelBilinear(atlas, k0 + 1.0, fc.xy, grid, atlasP);
    return mix(a, b, fracz);
}

// ---- Animated value-noise fbm (world-anchored, tasteful 3-octave) --------------
// Same form as the per-cone projvolFbm: value noise with quintic interpolation for
// soft cloud-like lobes, 3 octaves. Kept here so every froxel pass shares one
// medium definition.
float froxelHash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float froxelValueNoise(vec3 x)
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0); // quintic fade -> C2 continuity

    float n000 = froxelHash(p + vec3(0.0, 0.0, 0.0));
    float n100 = froxelHash(p + vec3(1.0, 0.0, 0.0));
    float n010 = froxelHash(p + vec3(0.0, 1.0, 0.0));
    float n110 = froxelHash(p + vec3(1.0, 1.0, 0.0));
    float n001 = froxelHash(p + vec3(0.0, 0.0, 1.0));
    float n101 = froxelHash(p + vec3(1.0, 0.0, 1.0));
    float n011 = froxelHash(p + vec3(0.0, 1.0, 1.0));
    float n111 = froxelHash(p + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z); // [0,1]
}

float froxelFbm(vec3 p)
{
    float sum = 0.0;
    float amp = 0.5;
    for (int o = 0; o < 3; ++o)
    {
        sum += amp * froxelValueNoise(p);
        p   *= 2.02;
        amp *= 0.5;
    }
    return sum; // ~[0,1], mean ~0.5
}

// ---- Debug colour ramp (black -> green -> yellow -> red over t in [0,1]) --------
vec3 froxelRamp(float t)
{
    t = clamp(t, 0.0, 1.0);
    if (t < 1.0 / 3.0)
    {
        return mix(vec3(0.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), t * 3.0);
    }
    else if (t < 2.0 / 3.0)
    {
        return mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 1.0 / 3.0) * 3.0);
    }
    return mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 2.0 / 3.0) * 3.0);
}
