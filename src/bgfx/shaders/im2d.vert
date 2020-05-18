$input a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

uniform vec4 u_xform;

#include "header.vert"

void main()
{
	gl_Position = vec4(a_position, 1.0);
	gl_Position.xy = gl_Position.xy * u_xform.xy + u_xform.zw;
	v_texcoord0.z = DoFog(gl_Position.z);
	gl_Position.xyz *= gl_Position.w;
	v_color0 = a_color0;
	v_texcoord0.xy = a_texcoord0;
}
