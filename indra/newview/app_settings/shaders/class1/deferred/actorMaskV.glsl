// [ActorMask] Native per-pass depth arithmetic, with authored coverage only.
/*[EXTRA_CODE_HERE]*/
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;
uniform mat4 projection_matrix;
uniform mat4 texture_matrix0;
uniform int mask_indexed;
uniform int mask_material_slots;
uniform int mask_vec3_position;
uniform vec4 mask_transform[16];
in vec3 position;
in vec2 texcoord0;
in vec4 diffuse_color;
in int texture_index;
out vec3 vary_position;
out vec2 vary_texcoord0;
out vec4 vertex_color;
flat out int mask_slot;
#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
#endif
#ifdef MASK_PBR
vec2 texture_transform(vec2 uv, vec4[2] transform, mat4 animation);
#endif
void main()
{
#ifdef HAS_SKIN
    mat4 mat = getObjectSkinnedTransform();
    mat = modelview_matrix * mat;
    // diffuseV keeps pos.w; bumpV/materialV/pbropaqueV reconstruct w = 1.
    if (mask_vec3_position != 0)
    {
        vec3 pos = (mat * vec4(position.xyz, 1.0)).xyz;
        vary_position = pos;
        gl_Position = projection_matrix * vec4(pos, 1.0);
    }
    else
    {
        vec4 pos = mat * vec4(position.xyz, 1.0);
        vary_position = pos.xyz;
        gl_Position = projection_matrix * pos;
    }
#else
    vary_position = (modelview_matrix * vec4(position.xyz, 1.0)).xyz;
    gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);
#endif
    mask_slot = mask_indexed != 0 ? texture_index : 0;
#ifdef MASK_PBR
    int mi = mask_material_slots != 0 ? mask_slot : 0;
    vec4 transform[2];
    transform[0] = mask_transform[2*mi];
    transform[1] = mask_transform[2*mi+1];
    vary_texcoord0 = texture_transform(texcoord0, transform,
        mask_material_slots != 0 ? mat4(1.0) : texture_matrix0);
#else
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0,0,1)).xy;
#endif
    vertex_color = diffuse_color;
}
