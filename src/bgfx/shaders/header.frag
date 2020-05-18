uniform vec4 u_alphaRef;

uniform vec4 u_fogStart;
uniform vec4 u_fogEnd;
uniform vec4 u_fogRange;
uniform vec4 u_fogDisable;
uniform vec4  u_fogColor;

void DoAlphaTest(float a)
{
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
}
