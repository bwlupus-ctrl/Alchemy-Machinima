/**
 * @file weatherSnowSim.glsl
 * @brief World-cell visibility evaluation of one immutable weather frame.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork. Slipstream Viewer contributors: precipitation design
 * reference; new OpenGL implementation, no Vulkan source/ABI copied.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
layout(local_size_x=128) in;
struct Flake { vec4 center; vec4 traits; };
layout(std430,binding=0) writeonly buffer Snow { Flake flakes[]; };
uniform int snow_count, snow_dimension, snow_slots;
uniform uint snow_seed;
uniform ivec3 snow_cell_origin;
uniform vec3 snow_cell_relative;
uniform float snow_time, snow_amount;

uint snowHash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; return x ^ (x >> 16);
}
float unitHash(uint x) { return float(snowHash(x) >> 8) * (1.0 / 16777216.0); }
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(snow_count)) return;
    uint c = i / uint(snow_slots), d = uint(snow_dimension);
    ivec3 offset = ivec3(c % d, (c / d) % d, c / (d*d));
    ivec3 cell = snow_cell_origin + offset;
    uint id = snowHash(uint(cell.x) ^ snowHash(uint(cell.y)) ^
                       snowHash(uint(cell.z) + 0x9e3779b9u) ^ snow_seed);
    id = snowHash(id ^ ((i % uint(snow_slots)) * 0x85ebca6bu));
    vec3 jitter = vec3(unitHash(id), unitHash(id+1u), unitHash(id+2u));
    vec3 p = snow_cell_relative + (vec3(offset) + jitter) * 32.0;
    // Small smooth flutter rides the shared fixed-tick mean-wind translation.
    p.xy += 0.12 * vec2(sin(snow_time*1.7+jitter.z*6.2831853),
                        cos(snow_time*1.3+jitter.x*6.2831853));
    flakes[i].center = vec4(p, step(unitHash(id+3u), snow_amount));
    flakes[i].traits = vec4(0.65+unitHash(id+4u)*0.7, jitter.z,
                            unitHash(id+3u), uintBitsToFloat(id));
}
