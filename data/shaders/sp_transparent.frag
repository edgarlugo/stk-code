uniform int fog_enabled;
uniform float custom_alpha;

in vec2 uv;
in vec4 color;
in vec4 world_position;
out vec4 o_diffuse_color;

#stk_include "utils/sp_texture_sampling.frag"

void main()
{
    vec4 diffusecolor = sampleTextureLayer0(uv);
    vec4 finalcolor = vec4(0.);
    if (fog_enabled == 0)
    {
        finalcolor = diffusecolor;
        finalcolor.xyz *= color.xyz;
        finalcolor.a *= color.a;
    }
    else
    {
        diffusecolor.xyz *= color.xyz;
        diffusecolor.a *= color.a;
        // Distance to the camera
        float dist = distance(world_position.xyz, u_inverse_view_matrix[3].xyz);
        float fog = smoothstep(u_fog_data.x, u_fog_data.y, dist);
        fog = min(fog, u_fog_data.z);
        finalcolor = u_fog_color * fog + diffusecolor * (1. - fog);
    }
    o_diffuse_color = vec4(finalcolor.rgb * (finalcolor.a * (1.0 - custom_alpha)),
        finalcolor.a * (1.0 - custom_alpha));
}
