// State
uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogRange;
uniform float u_fogDisable;
//uniform vec4  u_fogColor;

// Scene
uniform mat4 u_proj;
uniform mat4 u_view;

#define MAX_LIGHTS 8
struct Light {
	float enabled;
	float radius;
	float minusCosAngle;
	float hardSpot;
	vec4  position;
	vec4  direction;
	vec4  color;
};

// Object
uniform mat4  u_world;
uniform vec4  u_ambLight;
uniform Light u_directLights[MAX_LIGHTS];
uniform Light u_pointLights[MAX_LIGHTS];
uniform Light u_spotLights[MAX_LIGHTS];

uniform vec4 u_matColor;
uniform vec4 u_surfProps;	// amb, spec, diff, extra

#define surfAmbient (u_surfProps.x)
#define surfSpecular (u_surfProps.y)
#define surfDiffuse (u_surfProps.z)

vec3 DoDirLight(Light L, vec3 N)
{
	float l = max(0.0, dot(N, -L.direction.xyz));
	return l*L.color.rgb;
}

vec3 DoPointLight(Light L, vec3 V, vec3 N)
{
	// As on PS2
	vec3 dir = V - L.position.xyz;
	float dist = length(dir);
	float atten = max(0.0, (1.0 - dist/L.radius));
	float l = max(0.0, dot(N, -normalize(dir)));
	return l*L.color.rgb*atten;
}

vec3 DoSpotLight(Light L, vec3 V, vec3 N)
{
	// As on PS2
	vec3 dir = V - L.position.xyz;
	float dist = length(dir);
	float atten = max(0.0, (1.0 - dist/L.radius));
	dir /= dist;
	float l = max(0.0, dot(N, -dir));
	float pcos = dot(dir, L.direction.xyz);	// cos to point
	float ccos = -L.minusCosAngle;
	float falloff = (pcos-ccos)/(1.0-ccos);
	if(falloff < 0.0)	// outside of cone
		l = 0.0;
	l *= max(falloff, L.hardSpot);
	return l*L.color.xyz*atten;
}

float DoFog(float w)
{
	return clamp((w - u_fogEnd)*u_fogRange, u_fogDisable, 1.0);
}

#define DIRECTIONALS
//#define POINTLIGHTS
//#define SPOTLIGHTS