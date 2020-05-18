$input a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

#include "bgfx_shader.sh"
#include "header.vert"

void main()
{
	vec4 V = mul(u_world, vec4(a_position, 1.0));
	vec4 cV = mul(u_view, V);
	gl_Position = mul(u_proj, cV);
	v_color0 = a_color0;
	v_texcoord0.xy = a_texcoord0;
	v_texcoord0.z = DoFog(gl_Position.w);
}
