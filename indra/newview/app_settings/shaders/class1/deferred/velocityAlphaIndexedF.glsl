/**
 * Indexed multi-material alpha-mask velocity fragment shader.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

flat in int vary_material_index;
in vec2 vary_texcoord0;
in vec4 vertex_color;
in vec4 vary_cur_clip;
in vec4 vary_last_clip;
in vec3 vary_actor_fx_position;

uniform float velocity_minimum_alpha[GLTF_INDEXED_CHANNELS];
uniform int velocity_texture_alpha_only;
uniform sampler2D diffuse0;
#if GLTF_INDEXED_CHANNELS > 1
uniform sampler2D diffuse1;
#endif
#if GLTF_INDEXED_CHANNELS > 2
uniform sampler2D diffuse2;
#endif
#if GLTF_INDEXED_CHANNELS > 3
uniform sampler2D diffuse3;
#endif
#if GLTF_INDEXED_CHANNELS > 4
uniform sampler2D diffuse4;
#endif
#if GLTF_INDEXED_CHANNELS > 5
uniform sampler2D diffuse5;
#endif
#if GLTF_INDEXED_CHANNELS > 6
uniform sampler2D diffuse6;
#endif
#if GLTF_INDEXED_CHANNELS > 7
uniform sampler2D diffuse7;
#endif

bool actorFxDissolveDiscard(vec3 raw_object_position);

float sample_alpha(vec2 uv)
{
    if (vary_material_index == 0) return texture(diffuse0, uv).a;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_material_index == 1) return texture(diffuse1, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_material_index == 2) return texture(diffuse2, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_material_index == 3) return texture(diffuse3, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_material_index == 4) return texture(diffuse4, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_material_index == 5) return texture(diffuse5, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_material_index == 6) return texture(diffuse6, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_material_index == 7) return texture(diffuse7, uv).a;
#endif
    return 1.0;
}

void main()
{
    float alpha = sample_alpha(vary_texcoord0);
    if (velocity_texture_alpha_only == 0)
    {
        alpha *= vertex_color.a;
    }
    if (alpha < velocity_minimum_alpha[vary_material_index])
    {
        discard;
    }
    if (actorFxDissolveDiscard(vary_actor_fx_position))
    {
        discard;
    }

    vec2 cur_ndc = vary_cur_clip.xy / vary_cur_clip.w;
    vec2 last_ndc = vary_last_clip.xy / vary_last_clip.w;
    frag_color = vec4(cur_ndc - last_ndc, 0.0, 1.0);
}
