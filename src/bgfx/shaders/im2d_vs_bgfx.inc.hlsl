// shaderc command line:
// shadercRelease.exe --debug -f im2d.vert -o im2d_vs_bgfx.inc --bin2c --type v --platform windows --profile vs_4_0

struct Output
{
float4 gl_Position : SV_POSITION;
float4 v_color0 : COLOR;
float3 v_texcoord0 : TEXCOORD0;
};
uniform float4 u_xform;
static float2 u_alphaRef;
static float4 u_fogStart;
uniform float4 u_fogEnd;
uniform float4 u_fogRange;
uniform float4 u_fogDisable;
static float4 u_fogColor;
static float4x4 u_world;
static float4 u_ambLight;
static float4 u_lightParams[8];
static float4 u_lightPosition[8];
static float4 u_lightDirection[8];
static float4 u_lightColor[8];
static float4 u_matColor;
static float4 u_surfProps;
float3 DoDynamicLight(float3 V, float3 N)
{
float3 color = float3(0.0, 0.0, 0.0);
for(int i = 0; i < 8; i++){
if(u_lightParams[i].x == 0.0)
break;
if(u_lightParams[i].x == 1.0){
float l = max(0.0, dot(N, -u_lightDirection[i].xyz));
color += l*u_lightColor[i].rgb;
}else
;
}
return color;
}
float DoFog(float w)
{
return clamp((w - u_fogEnd.x)*u_fogRange.x, u_fogDisable.x, 1.0);
}
Output main( float4 a_color0 : COLOR0 , float3 a_position : POSITION , float2 a_texcoord0 : TEXCOORD0) { Output _varying_; _varying_.v_color0 = float4(1.0, 0.0, 0.0, 1.0);; _varying_.v_texcoord0 = float3(0.0, 0.0, 0.0);;
{
_varying_.gl_Position = float4(a_position, 1.0);
_varying_.gl_Position.xy = _varying_.gl_Position.xy * u_xform.xy + u_xform.zw;
_varying_.v_texcoord0.z = DoFog(_varying_.gl_Position.z);
_varying_.gl_Position.xyz *= _varying_.gl_Position.w;
_varying_.v_color0 = a_color0;
_varying_.v_texcoord0.xy = a_texcoord0;
} return _varying_;
}
