$input a_position, a_normal, a_color0, a_texcoord0
$output v_texcoord0, v_color0

#include "bgfx_shader.sh"
#include "header.vert"

void main()
{
	vec4 V = mul(u_world, vec4(a_position, 1.0));
	vec4 position = mul(u_proj, mul(u_view, V));
	mat3 otherWorld = (mat3)u_world;
	vec3 N = mul(otherWorld, a_normal).xyz;

	v_texcoord0.xy = a_texcoord0;

	v_color0 = a_color0;
	v_color0.rgb += u_ambLight.rgb*surfAmbient;
	v_color0.rgb += DoDynamicLight(V.xyz, N)*surfDiffuse;
	v_color0 = clamp(v_color0, 0.0f, 1.0);
	v_color0 *= u_matColor;

	v_texcoord0.z = DoFog(position.w);
	
	gl_Position = position;
}
