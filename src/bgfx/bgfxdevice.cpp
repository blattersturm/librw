#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#ifdef RW_BGFX
#define GLFW_EXPOSE_NATIVE_WIN32

#ifdef RW_GLFW
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif
#include "rwbgfx.h"
#include "rwbgfxshader.h"
#include "rwbgfximpl.h"

// for now
#include <bgfx/platform.h>

#define PLUGIN_ID 0

namespace rw {
namespace rwbgfx {
struct DisplayMode
{
#ifdef RW_GLFW
	GLFWvidmode mode;
#endif
	int32 depth;
	uint32 flags;
};

struct BgfxGlobals
{
#ifdef RW_GLFW
	GLFWwindow *window;

	GLFWmonitor *monitor;
	int numMonitors;
	int currentMonitor;

	DisplayMode *modes;
	int numModes;
	int currentMode;
	GLFWwindow **pWindow;
#endif

	int presentWidth, presentHeight;

	// for opening the window
	int winWidth, winHeight;
	const char *winTitle;
} bgfxGlobals;

int32   alphaFunc;
float32 alphaRef;

struct UniformState
{
	float32 alphaRefLow;
	float32 alphaRefHigh;
	float32 fogStart;
	float32 fogEnd;

	float32 fogRange;
	float32 fogDisable;
	int32   pad[2];

	RGBAf   fogColor;
};

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	RawMatrix    world;
	RGBAf        ambLight;
	struct {
		float type;
		float radius;
		float minusCosAngle;
		float hardSpot;
	} lightParams[MAX_LIGHTS];
	V4d lightPosition[MAX_LIGHTS];
	V4d lightDirection[MAX_LIGHTS];
	RGBAf lightColor[MAX_LIGHTS];
};

const char *shaderDecl330 = "#version 330\n";
const char *shaderDecl100es =
"#version 100\n"\
"precision highp float;\n"\
"precision highp int;\n";
const char *shaderDecl310es =
"#version 310 es\n"\
"precision highp float;\n"\
"precision highp int;\n";

const char *shaderDecl = shaderDecl330;

static bgfx::TextureHandle whitetex;
static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;

// State
bgfx::UniformHandle u_alphaRef;
bgfx::UniformHandle u_fogStart;
bgfx::UniformHandle u_fogEnd;
bgfx::UniformHandle u_fogRange;
bgfx::UniformHandle u_fogDisable;
bgfx::UniformHandle u_fogColor;

// Object
bgfx::UniformHandle u_world;
bgfx::UniformHandle u_ambLight;
bgfx::UniformHandle u_lightParams;
bgfx::UniformHandle u_lightPosition;
bgfx::UniformHandle u_lightDirection;
bgfx::UniformHandle u_lightColor;

bgfx::UniformHandle u_matColor;
bgfx::UniformHandle u_surfProps;

bgfx::UniformHandle s_tex[2];

Shader *defaultShader;

static struct BgfxStateCache
{
	Raster* rasters[2];
	uint32 textureAddressU;
	uint32 textureAddressV;
	uint32 textureFilter;
	uint32 vertexAlpha;
	uint32 textureAlpha;
	uint32 srcBlend;
	uint32 destBlend;
	uint32 zTest;
	uint32 zWrite;
	uint32 cullmode;
	uint32 gsAlpha;
	uint32 gsAlphaRef;
} stateCache;

static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:
		stateCache.rasters[0] = (Raster*)pvalue;
		stateCache.rasters[1] = NULL;
		break;
	case TEXTUREADDRESS:
		stateCache.textureAddressU = value;
		stateCache.textureAddressV = value;
		break;
	case TEXTUREADDRESSU:
		stateCache.textureAddressU = value;
		break;
	case TEXTUREADDRESSV:
		stateCache.textureAddressV = value;
		break;
	case TEXTUREFILTER:
		stateCache.textureFilter = value;
		break;
	case VERTEXALPHA:
		stateCache.vertexAlpha = value;
		break;
	case SRCBLEND:
		stateCache.srcBlend = value;
		break;
	case DESTBLEND:
		stateCache.destBlend = value;
		break;
	case ZTESTENABLE:
		stateCache.zTest = value;
		break;
	case ZWRITEENABLE:
		stateCache.zWrite = value;
		break;
	case FOGENABLE:
		uniformState.fogDisable = value ? 0.0f : 1.0f;
		break;
	case FOGCOLOR:
		RGBA c;
		c.red = value;
		c.green = value >> 8;
		c.blue = value >> 16;
		c.alpha = value >> 24;
		convColor(&uniformState.fogColor, &c);
		break;
	case CULLMODE:
		stateCache.cullmode = value;
		break;

	case ALPHATESTFUNC:
		alphaFunc = value;
		break;
	case ALPHATESTREF:
		alphaRef = value / 255.0f;
		break;
	case GSALPHATEST:
		stateCache.gsAlpha = value;
		break;
	case GSALPHATESTREF:
		stateCache.gsAlphaRef = value;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 val;
	RGBA rgba;
	switch(state){
	case TEXTURERASTER:
		return stateCache.rasters[0];
	case TEXTUREADDRESS:
		if (stateCache.textureAddressU == stateCache.textureAddressV)
			val = stateCache.textureAddressU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = stateCache.textureAddressU;
		break;
	case TEXTUREADDRESSV:
		val = stateCache.textureAddressV;
		break;
	case TEXTUREFILTER:
		val = stateCache.textureFilter;
		break;

	case VERTEXALPHA:
		val = stateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = stateCache.srcBlend;
		break;
	case DESTBLEND:
		val = stateCache.destBlend;
		break;
	case ZTESTENABLE:
		val = stateCache.zTest;
		break;
	case ZWRITEENABLE:
		val = stateCache.zWrite;
		break;
	case FOGENABLE:
		val = uniformState.fogDisable == 0.0f;
		break;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		val = RWRGBAINT(rgba.red, rgba.green, rgba.blue, rgba.alpha);
		break;
	case CULLMODE:
		val = stateCache.cullmode;
		break;

	case ALPHATESTFUNC:
		val = alphaFunc;
		break;
	case ALPHATESTREF:
		val = alphaRef * 255.0f;
		break;
	case GSALPHATEST:
		val = stateCache.gsAlpha;
		break;
	case GSALPHATESTREF:
		val = stateCache.gsAlphaRef;
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}

static void
resetRenderState(void)
{	
	alphaFunc = ALPHAGREATEREQUAL;
	alphaFunc = 0;
	alphaRef = 10.0f/255.0f;
	uniformState.fogDisable = 1.0f;
	uniformState.fogStart = 0.0f;
	uniformState.fogEnd = 0.0f;
	uniformState.fogRange = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	stateCache.gsAlpha = 0;
	stateCache.gsAlphaRef = 128;

	stateCache.vertexAlpha = 0;

	stateCache.zWrite = 1;

	stateCache.zTest = 1;

	stateCache.cullmode = CULLNONE;

	stateCache.rasters[0] = NULL;
	stateCache.rasters[1] = NULL;
}

void
setWorldMatrix(Matrix *mat)
{
	convMatrix(&uniformObject.world, mat);
}

int32
setLights(WorldLights *lightData)
{
	int i, n;
	Light *l;
	int32 bits;

	uniformObject.ambLight = lightData->ambient;

	bits = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	n = 0;
	for(i = 0; i < lightData->numDirectionals && i < 8; i++){
		l = lightData->directionals[i];
		uniformObject.lightParams[n].type = 1.0f;
		uniformObject.lightColor[n] = l->color;
		memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
		bits |= VSLIGHT_POINT;
		n++;
		if(n >= MAX_LIGHTS)
			goto out;
	}

	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			uniformObject.lightParams[n].type = 2.0f;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			bits |= VSLIGHT_POINT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			uniformObject.lightParams[n].type = 3.0f;
			uniformObject.lightParams[n].minusCosAngle = l->minusCosAngle;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				uniformObject.lightParams[n].hardSpot = 0.0f;
			else
				uniformObject.lightParams[n].hardSpot = 1.0f;
			bits |= VSLIGHT_SPOT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		}
	}

	uniformObject.lightParams[n].type = 0.0f;
out:
	return bits;
}

void
setProjectionMatrix(float32 *mat)
{
	memcpy(&uniformScene.proj, mat, 64);
}

void
setViewMatrix(float32 *mat)
{
	memcpy(&uniformScene.view, mat, 64);
}

Shader *lastShaderUploaded;

static uint64 blendMap[] = {
	BGFX_STATE_BLEND_ZERO,	// actually invalid
	BGFX_STATE_BLEND_ZERO,
	BGFX_STATE_BLEND_ONE,
	BGFX_STATE_BLEND_SRC_COLOR,
	BGFX_STATE_BLEND_INV_SRC_COLOR,
	BGFX_STATE_BLEND_SRC_ALPHA,
	BGFX_STATE_BLEND_INV_SRC_ALPHA,
	BGFX_STATE_BLEND_DST_COLOR,
	BGFX_STATE_BLEND_INV_DST_COLOR,
	BGFX_STATE_BLEND_DST_ALPHA,
	BGFX_STATE_BLEND_INV_DST_ALPHA,
	BGFX_STATE_BLEND_SRC_ALPHA_SAT,
};

void
flushCache(uint64_t baseState)
{
#define U(i) i

	for (int i = 0; i < 2; i++)
	{
		if (stateCache.rasters[i])
		{
			BgfxRaster* natras = PLUGINOFFSET(BgfxRaster, stateCache.rasters[i], nativeRasterOffset);
			bgfx::setTexture(i, s_tex[i], natras->texid);

			if (i == 0)
			{
				stateCache.textureAlpha = natras->hasAlpha;
			}
		}
		else
		{
			bgfx::setTexture(i, s_tex[i], whitetex);

			if (i == 0)
			{
				stateCache.textureAlpha = 0;
			}
		}
	}

	bgfx::setUniform(U(u_world), (float*)&uniformObject.world, 4);
	bgfx::setUniform(U(u_ambLight), (float*)&uniformObject.ambLight, 1);
	bgfx::setUniform(U(u_lightParams), (float*)&uniformObject.lightParams, MAX_LIGHTS);
	bgfx::setUniform(U(u_lightPosition), (float*)&uniformObject.lightPosition, MAX_LIGHTS);
	bgfx::setUniform(U(u_lightDirection), (float*)&uniformObject.lightDirection, MAX_LIGHTS);
	bgfx::setUniform(U(u_lightColor), (float*)&uniformObject.lightPosition, MAX_LIGHTS);

	bgfx::setViewTransform(viewId, uniformScene.view, uniformScene.proj);

	float alphaRefPtr[4] = { -1000.0f, 1000.0f, 0.0f, 0.0f };

	if (stateCache.vertexAlpha || stateCache.textureAlpha)
	{
		switch (alphaFunc) {
		case ALPHAALWAYS:
		default:
			alphaRefPtr[0] = -1000.0f;
			alphaRefPtr[1] = 1000.0f;
			break;
		case ALPHAGREATEREQUAL:
			alphaRefPtr[0] = alphaRef;
			alphaRefPtr[1] = 1000.0f;
			break;
		case ALPHALESS:
			alphaRefPtr[0] = -1000.0f;
			alphaRefPtr[1] = alphaRef;
			break;
		}
	}

	bgfx::setUniform(U(u_alphaRef), alphaRefPtr);

	float tmp[4] = { 0.f };
	tmp[0] = uniformState.fogStart;
	bgfx::setUniform(U(u_fogStart), tmp);

	tmp[0] = uniformState.fogEnd;
	bgfx::setUniform(U(u_fogEnd), tmp);

	tmp[0] = uniformState.fogRange;
	bgfx::setUniform(U(u_fogRange), tmp);

	tmp[0] = uniformState.fogDisable;
	bgfx::setUniform(U(u_fogDisable), tmp);

	bgfx::setUniform(U(u_fogColor), &uniformState.fogColor);

	uint64 cullMode = 0;// (stateCache.cullmode == CULLNONE) ? 0 :
		//((stateCache.cullmode == CULLBACK) ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW);

	uint64 blendFunc = 0;

	if (stateCache.vertexAlpha || stateCache.textureAlpha)
	{
		blendFunc = BGFX_STATE_BLEND_FUNC(blendMap[stateCache.srcBlend], blendMap[stateCache.destBlend]);
	}

	bgfx::setState(
		baseState | 
		cullMode | 
		BGFX_STATE_WRITE_RGB |
		((stateCache.zWrite) ? BGFX_STATE_WRITE_Z : 0) |
		((stateCache.zTest) ? BGFX_STATE_DEPTH_TEST_LEQUAL : ((stateCache.zWrite) ? BGFX_STATE_DEPTH_TEST_ALWAYS : 0)) |
		blendFunc
	);
}

void setTexture(int32 n, Texture* tex)
{
	if (tex)
	{
		BgfxRaster* natras = PLUGINOFFSET(BgfxRaster, tex->raster, nativeRasterOffset);

		if (!natras->setName && bgfx::isValid(natras->texid))
		{
			bgfx::setName(natras->texid, tex->name);
			natras->setName = true;
		}
	}

	stateCache.rasters[n] = tex ? tex->raster : NULL;
}

bool32 getAlphaBlend()
{
	return stateCache.vertexAlpha;
}

int viewId;

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	auto flags = BGFX_CLEAR_NONE;

	if (mode & Camera::CLEARIMAGE)
	{
		flags |= BGFX_CLEAR_COLOR;
	}

	if (mode & Camera::CLEARZ)
	{
		flags |= BGFX_CLEAR_DEPTH;
	}

	// TODO: check color
	bgfx::setViewClear(viewId, flags, *(uint32_t*)&col);
	bgfx::touch(viewId);
}

static void
showRaster(Raster *raster, uint32 flags)
{
	bgfx::frame();
	viewId = 0;
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	BgfxRaster *natdst = PLUGINOFFSET(BgfxRaster, dst, nativeRasterOffset);
	BgfxRaster *natsrc = PLUGINOFFSET(BgfxRaster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			//bgfx::blit(viewId, natdst->texid, x, y, natsrc->texid);
			return 1;
		}
		break;
	}
	return 0;
}

static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0] = -inv.right.x;
	view[1] = inv.right.y;
	view[2] = inv.right.z;
	view[3] = 0.0f;
	view[4] = -inv.up.x;
	view[5] = inv.up.y;
	view[6] = inv.up.z;
	view[7] = 0.0f;
	view[8] = -inv.at.x;
	view[9] = inv.at.y;
	view[10] = inv.at.z;
	view[11] = 0.0f;
	view[12] = -inv.pos.x;
	view[13] = inv.pos.y;
	view[14] = inv.pos.z;
	view[15] = 1.0f;
	memcpy(&cam->devView, view, sizeof(RawMatrix));
	setViewMatrix(view);

	// Projection Matrix
	float32 invwx = 1.0f / cam->viewWindow.x;
	float32 invwy = 1.0f / cam->viewWindow.y;
	float32 invz = 1.0f / (cam->farPlane - cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x * invwx;
	proj[9] = cam->viewOffset.y * invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if (cam->projection == Camera::PERSPECTIVE) {
		proj[10] = cam->farPlane * invz;
		proj[11] = 1.0f;

		proj[15] = 0.0f;
	}
	else {
		proj[10] = invz;
		proj[11] = 0.0f;

		proj[15] = 1.0f;
	}
	proj[14] = -cam->nearPlane * proj[10];

	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));
	setProjectionMatrix(proj);

	bgfx::setViewRect(viewId, cam->frameBuffer->offsetX, cam->frameBuffer->offsetY, cam->frameBuffer->width, cam->frameBuffer->height);
	bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential); // we don't want weird sorting

	sortIdx = 0xFFFFFFFF;

	uniformState.fogStart = cam->fogPlane;
	uniformState.fogEnd = cam->farPlane;
	uniformState.fogRange = 1.0f / (uniformState.fogStart - uniformState.fogEnd);
}

static void
endUpdate(Camera *cam)
{
	viewId++;
}


static int
initOpenGL(void)
{
	u_alphaRef = bgfx::createUniform("u_alphaRef", bgfx::UniformType::Vec4);
	u_fogStart = bgfx::createUniform("u_fogStart", bgfx::UniformType::Vec4);
	u_fogEnd = bgfx::createUniform("u_fogEnd", bgfx::UniformType::Vec4);
	u_fogRange = bgfx::createUniform("u_fogRange", bgfx::UniformType::Vec4);
	u_fogDisable = bgfx::createUniform("u_fogDisable", bgfx::UniformType::Vec4);
	u_fogColor = bgfx::createUniform("u_fogColor", bgfx::UniformType::Vec4);
	u_world = bgfx::createUniform("u_world", bgfx::UniformType::Mat4);
	u_ambLight = bgfx::createUniform("u_ambLight", bgfx::UniformType::Vec4);
	u_lightParams = bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4, MAX_LIGHTS);
	u_lightPosition = bgfx::createUniform("u_lightPosition", bgfx::UniformType::Vec4, MAX_LIGHTS);
	u_lightDirection = bgfx::createUniform("u_lightDirection", bgfx::UniformType::Vec4, MAX_LIGHTS);
	u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4, MAX_LIGHTS);
	lastShaderUploaded = nil;

	u_matColor = bgfx::createUniform("u_matColor", bgfx::UniformType::Vec4);
	u_surfProps = bgfx::createUniform("u_surfProps", bgfx::UniformType::Vec4);

	s_tex[0] = bgfx::createUniform("tex0", bgfx::UniformType::Sampler);
	s_tex[1] = bgfx::createUniform("tex1", bgfx::UniformType::Sampler);

	byte whitepixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	whitetex = bgfx::createTexture2D(1, 1, false, 0, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(whitepixel, sizeof(whitepixel)));

	resetRenderState();

#include "shaders/default_vs_bgfx.inc"
#include "shaders/simple_fs_bgfx.inc"
	defaultShader = Shader::create((char*)default_vs_bgfx, sizeof(default_vs_bgfx), (char*)simple_fs_bgfx, sizeof(simple_fs_bgfx));
	assert(defaultShader);

	openIm2D();
	openIm3D();

	return 1;
}

static int
termOpenGL(void)
{
	closeIm3D();
	closeIm2D();

	stateCache.rasters[0] = NULL;
	stateCache.rasters[1] = NULL;

	return 1;
}

static int
finalizeOpenGL(void)
{
	return 1;
}

#ifdef RW_GLFW
static void
addVideoMode(const GLFWvidmode *mode)
{
	int i;

	for(i = 1; i < bgfxGlobals.numModes; i++){
		if(bgfxGlobals.modes[i].mode.width == mode->width &&
			bgfxGlobals.modes[i].mode.height == mode->height &&
			bgfxGlobals.modes[i].mode.redBits == mode->redBits &&
			bgfxGlobals.modes[i].mode.greenBits == mode->greenBits &&
			bgfxGlobals.modes[i].mode.blueBits == mode->blueBits){
			// had this mode already, remember highest refresh rate
			if(mode->refreshRate > bgfxGlobals.modes[i].mode.refreshRate)
				bgfxGlobals.modes[i].mode.refreshRate = mode->refreshRate;
			return;
		}
	}

	// none found, add
	bgfxGlobals.modes[bgfxGlobals.numModes].mode = *mode;
	bgfxGlobals.modes[bgfxGlobals.numModes].flags = VIDEOMODEEXCLUSIVE;
	bgfxGlobals.numModes++;
}

static void
makeVideoModeList(void)
{
	int i, num;
	const GLFWvidmode *modes;

	modes = glfwGetVideoModes(bgfxGlobals.monitor, &num);
	rwFree(bgfxGlobals.modes);
	bgfxGlobals.modes = rwNewT(DisplayMode, num, ID_DRIVER | MEMDUR_EVENT);

	bgfxGlobals.modes[0].mode = *glfwGetVideoMode(bgfxGlobals.monitor);
	bgfxGlobals.modes[0].flags = 0;
	bgfxGlobals.numModes = 1;

	for(i = 0; i < num; i++)
		addVideoMode(&modes[i]);

	for(i = 0; i < bgfxGlobals.numModes; i++){
		num = bgfxGlobals.modes[i].mode.redBits +
			bgfxGlobals.modes[i].mode.greenBits +
			bgfxGlobals.modes[i].mode.blueBits;
		// set depth to power of two
		for(bgfxGlobals.modes[i].depth = 1; bgfxGlobals.modes[i].depth < num; bgfxGlobals.modes[i].depth <<= 1);
	}
}

static int
openGLFW(EngineOpenParams *openparams)
{
	bgfxGlobals.winWidth = openparams->width;
	bgfxGlobals.winHeight = openparams->height;
	bgfxGlobals.winTitle = openparams->windowtitle;
	bgfxGlobals.pWindow = openparams->window;

	/* Init GLFW */
	if(!glfwInit()){
		RWERROR((ERR_GENERAL, "glfwInit() failed"));
		return 0;
	}

	bgfxGlobals.monitor = glfwGetMonitors(&bgfxGlobals.numMonitors)[0];

	makeVideoModeList();

	return 1;
}

static int
closeGLFW(void)
{
	glfwTerminate();
	return 1;
}

static void
glfwerr(int error, const char *desc)
{
	fprintf(stderr, "GLFW Error: %s\n", desc);
}

static int
startGLFW(void)
{
	GLenum status;
	GLFWwindow *win;
	DisplayMode *mode;

	mode = &bgfxGlobals.modes[bgfxGlobals.currentMode];

	glfwSetErrorCallback(glfwerr);

	if(mode->flags & VIDEOMODEEXCLUSIVE)
		win = glfwCreateWindow(mode->mode.width, mode->mode.height, bgfxGlobals.winTitle, nil, nil);
	else
		win = glfwCreateWindow(bgfxGlobals.winWidth, bgfxGlobals.winHeight, bgfxGlobals.winTitle, nil, nil);
	if(win == nil){
		RWERROR((ERR_GENERAL, "glfwCreateWindow() failed"));
		return 0;
	}

	// platform might require this
	//bgfx::renderFrame();

	// Initialize bgfx using the native window handle and window resolution.
	bgfx::Init init;
#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
	init.platformData.ndt = glfwGetX11Display();
	init.platformData.nwh = (void*)(uintptr_t)glfwGetX11Window(win);
#elif BX_PLATFORM_OSX
	init.platformData.nwh = glfwGetCocoaWindow(win);
#elif BX_PLATFORM_WINDOWS
	init.platformData.nwh = glfwGetWin32Window(win);
#endif

	int width, height;
	glfwGetWindowSize(win, &width, &height);
	init.resolution.width = (uint32_t)width;
	init.resolution.height = (uint32_t)height;
	init.resolution.reset = 0;// BGFX_RESET_VSYNC;
	if (!bgfx::init(init))
		return 1;

	bgfx::setDebug(BGFX_DEBUG_STATS);

	bgfxGlobals.window = win;
	*bgfxGlobals.pWindow = win;
	bgfxGlobals.presentWidth = 0;
	bgfxGlobals.presentHeight = 0;
	return 1;
}

static int
stopGLFW(void)
{
	glfwDestroyWindow(bgfxGlobals.window);
	return 1;
}

static int
deviceSystem(DeviceReq req, void *arg, int32 n)
{
	GLFWmonitor **monitors;
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openGLFW((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeGLFW();

	case DEVICEINIT:
		return startGLFW() && initOpenGL();
	case DEVICETERM:
		return termOpenGL() && stopGLFW();

	case DEVICEFINALIZE:
		return finalizeOpenGL();


	case DEVICEGETNUMSUBSYSTEMS:
		return bgfxGlobals.numMonitors;

	case DEVICEGETCURRENTSUBSYSTEM:
		return bgfxGlobals.currentMonitor;

	case DEVICESETSUBSYSTEM:
		monitors = glfwGetMonitors(&bgfxGlobals.numMonitors);
		if(n >= bgfxGlobals.numMonitors)
			return 0;
		bgfxGlobals.currentMonitor = n;
		bgfxGlobals.monitor = monitors[bgfxGlobals.currentMonitor];
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		monitors = glfwGetMonitors(&bgfxGlobals.numMonitors);
		if(n >= bgfxGlobals.numMonitors)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, glfwGetMonitorName(monitors[n]), sizeof(SubSystemInfo::name));
		return 1;


	case DEVICEGETNUMVIDEOMODES:
		return bgfxGlobals.numModes;

	case DEVICEGETCURRENTVIDEOMODE:
		return bgfxGlobals.currentMode;

	case DEVICESETVIDEOMODE:
		if(n >= bgfxGlobals.numModes)
			return 0;
		bgfxGlobals.currentMode = n;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = bgfxGlobals.modes[n].mode.width;
		rwmode->height = bgfxGlobals.modes[n].mode.height;
		rwmode->depth = bgfxGlobals.modes[n].depth;
		rwmode->flags = bgfxGlobals.modes[n].flags;
		return 1;

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}
#else
static int
openBGFX(EngineOpenParams* openparams)
{
	bgfxGlobals.winWidth = openparams->width;
	bgfxGlobals.winHeight = openparams->height;
	bgfxGlobals.winTitle = openparams->windowtitle;

	// Initialize bgfx using the native window handle and window resolution.
	bgfx::Init init;
	init.platformData.nwh = openparams->nwh;
	init.platformData.context = openparams->context;
	init.platformData.backBuffer = openparams->backBuffer;
	init.platformData.backBufferDS = openparams->backBufferDS;

	int width, height;
	init.resolution.width = openparams->width;
	init.resolution.height = openparams->height;
	init.type = bgfx::RendererType::Direct3D11;
	init.resolution.reset = 0;// BGFX_RESET_VSYNC;
	if (!bgfx::init(init))
		return 1;

	return 1;
}

static int
closeBGFX(void)
{
	bgfx::shutdown();

	return 1;
}

static int
startBGFX(void)
{
	bgfxGlobals.presentWidth = 0;
	bgfxGlobals.presentHeight = 0;
	return 1;
}

static int
stopBGFX(void)
{
	return 1;
}

static int w, h;

static int
deviceSystem(DeviceReq req, void* arg, int32 n)
{
	VideoMode* rwmode;

	switch (req) {
	case DEVICEOPEN:
		return openBGFX((rw::EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeBGFX();

	case DEVICEINIT:
		return startBGFX() && initOpenGL();
	case DEVICETERM:
		return termOpenGL() && stopBGFX();

	case DEVICEFINALIZE:
		return finalizeOpenGL();


	case DEVICEGETNUMSUBSYSTEMS:
		return 1;

	case DEVICEGETCURRENTSUBSYSTEM:
		return 0;

	case DEVICESETSUBSYSTEM:
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		strncpy(((SubSystemInfo*)arg)->name, "nice", sizeof(SubSystemInfo::name));
		return 1;


	case DEVICEGETNUMVIDEOMODES:
		return 1;

	case DEVICEGETCURRENTVIDEOMODE:
		return 0;

	case DEVICESETVIDEOMODE:
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = w;
		rwmode->height = h;
		rwmode->depth = 32;
		rwmode->flags = rw::VIDEOMODEEXCLUSIVE;
		return 1;

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}
#endif

Device renderdevice = {
	0.0f, 1.0f, // TODO: read `homogeneousDepth` field in bgfx caps!
	rwbgfx::beginUpdate,
	rwbgfx::endUpdate,
	rwbgfx::clearCamera,
	rwbgfx::showRaster,
	rwbgfx::rasterRenderFast,
	rwbgfx::setRenderState,
	rwbgfx::getRenderState,
	rwbgfx::im2DRenderLine,
	rwbgfx::im2DRenderTriangle,
	rwbgfx::im2DRenderPrimitive,
	rwbgfx::im2DRenderIndexedPrimitive,
	rwbgfx::im3DTransform,
	rwbgfx::im3DRenderPrimitive,
	rwbgfx::im3DRenderIndexedPrimitive,
	rwbgfx::im3DEnd,
	rwbgfx::deviceSystem
};

}
}

void hackSetVideoMode(int w, int h)
{
	rw::rwbgfx::w = w;
	rw::rwbgfx::h = h;
}

#endif

