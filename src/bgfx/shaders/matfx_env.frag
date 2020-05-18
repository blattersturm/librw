$input v_texcoord0, v_texcoord1, v_color0

#include "bgfx_shader.sh"

#include "header.frag"

SAMPLER2D(tex0, 0);
SAMPLER2D(tex1, 1);

uniform vec4 u_coefficient;
uniform vec4 u_colorClamp;

void main()
{
	vec4 color;
	vec4 pass1 = v_color0;
	vec4 envColor = pass1;	// TODO: colorClamp
	pass1 *= texture2D(tex0, vec2(v_texcoord0.x, 1.0-v_texcoord0.y));

	vec4 pass2 = envColor*u_coefficient.x*texture2D(tex1, vec2(v_texcoord1.x, 1.0-v_texcoord1.y));

	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_texcoord0.z);
	pass2.rgb = mix(vec3(0.0, 0.0, 0.0), pass2.rgb, v_texcoord0.z);

	color.rgb = pass1.rgb*pass1.a + pass2.rgb;
	color.a = pass1.a;

	DoAlphaTest(color.a);
	
	gl_FragColor = color;
}
