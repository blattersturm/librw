$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

#include "header.frag"

SAMPLER2D(tex0, 0);

void main()
{
	vec4 col = v_color0*texture2D(tex0, vec2(v_texcoord0.x, 1.0-v_texcoord0.y));
	
	gl_FragColor = vec4(mix(u_fogColor.rgb, col.rgb, v_texcoord0.z), col.a);
	DoAlphaTest(col.a);
}

