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
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwbgfx.h"
#include "rwbgfxshader.h"
#include "rwbgfxplg.h"

#include "rwbgfximpl.h"

namespace rw {
namespace rwbgfx {

#ifdef RW_BGFX

#define U(i) currentShader->uniformLocations[i]

static Shader *envShader;
static bgfx::UniformHandle u_texMatrix;
static bgfx::UniformHandle u_fxparams;
static bgfx::UniformHandle u_colorClamp;

static void*
matfxOpen(void *o, int32, int32)
{
	u_texMatrix = bgfx::createUniform("u_texMatrix", bgfx::UniformType::Mat4);
	u_fxparams = bgfx::createUniform("u_fxparams", bgfx::UniformType::Vec4);
	u_colorClamp = bgfx::createUniform("u_colorClamp", bgfx::UniformType::Vec4);
	matFXGlobals.pipelines[PLATFORM_BGFX] = makeMatFXPipeline();

#include "shaders/matfx_env_fs_bgfx.inc"
#include "shaders/matfx_env_vs_bgfx.inc"
	envShader = Shader::create((const char*)matfx_env_vs_bgfx, sizeof(matfx_env_vs_bgfx), (const char*)matfx_env_fs_bgfx, sizeof(matfx_env_fs_bgfx));
	assert(envShader);

	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_BGFX, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}


void
matfxDefaultRender(InstanceDataHeader *header, InstanceData *inst)
{
	Material *m;
	RGBAf col;
	float surfProps[4];
	m = inst->material;

	defaultShader->use();

	convColor(&col, &m->color);
	bgfx::setUniform(u_matColor, &col);

	surfProps[0] = m->surfaceProps.ambient;
	surfProps[1] = m->surfaceProps.specular;
	surfProps[2] = m->surfaceProps.diffuse;
	surfProps[3] = 0.0f;
	bgfx::setUniform(u_surfProps, surfProps);

	setTexture(0, m->texture);

	rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

	drawInst(header, inst);
}

static Frame *lastEnvFrame;

static RawMatrix normal2texcoord = {
	{ 0.5f,  0.0f, 0.0f }, 0.0f,
	{ 0.0f, -0.5f, 0.0f }, 0.0f,
	{ 0.0f,  0.0f, 1.0f }, 0.0f,
	{ 0.5f,  0.5f, 0.0f }, 1.0f
};

void
uploadEnvMatrix(Frame *frame)
{
	Matrix invMat;
	if(frame == nil)
		frame = engine->currentCamera->getFrame();

	// cache the matrix across multiple meshes
	static RawMatrix envMtx;
	if(frame != lastEnvFrame){
		lastEnvFrame = frame;

		RawMatrix invMtx;
		Matrix::invert(&invMat, frame->getLTM());
		convMatrix(&invMtx, &invMat);
		invMtx.pos.set(0.0f, 0.0f, 0.0f);
		RawMatrix::mult(&envMtx, &invMtx, &normal2texcoord);
	}
	bgfx::setUniform(u_texMatrix, &envMtx, 4);
}

void
matfxEnvRender(InstanceDataHeader *header, InstanceData *inst, MatFX::Env *env)
{
	Material *m;
	RGBAf col;
	float surfProps[4];
	m = inst->material;

	if(env->tex == nil || env->coefficient == 0.0f){
		matfxDefaultRender(header, inst);
		return;
	}

	envShader->use();

	setTexture(0, m->texture);
	setTexture(1, env->tex);
	uploadEnvMatrix(env->frame);

	convColor(&col, &m->color);
	bgfx::setUniform(u_matColor, &col);

	surfProps[0] = m->surfaceProps.ambient;
	surfProps[1] = m->surfaceProps.specular;
	surfProps[2] = m->surfaceProps.diffuse;
	surfProps[3] = 0.0f;
	bgfx::setUniform(u_surfProps, surfProps);

	float tmp[4] = { env->coefficient, env->fbAlpha ? 0.0f : 1.0f, 0.0f, 0.0f };
	bgfx::setUniform(u_fxparams, tmp);
	static float zero[4];
	static float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	// This clamps the vertex color below. With it we can achieve both PC and PS2 style matfx
	if(MatFX::modulateEnvMap)
		bgfx::setUniform(u_colorClamp, zero);
	else
		bgfx::setUniform(u_colorClamp, one);

	rw::SetRenderState(VERTEXALPHA, 1);
	rw::SetRenderState(SRCBLEND, BLENDONE);

	drawInst(header, inst);

	rw::SetRenderState(SRCBLEND, BLENDSRCALPHA);
}

void
matfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	lastEnvFrame = nil;

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		MatFX *matfx = MatFX::get(inst->material);

		if(matfx == nil)
			matfxDefaultRender(header, inst);
		else switch(matfx->type){
		case MatFX::ENVMAP:
			matfxEnvRender(header, inst, &matfx->fx[0].env);
			break;
		default:
			matfxDefaultRender(header, inst);
			break;
		}
		inst++;
	}
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_BGFX);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = matfxRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

#else

void initMatFX(void) { }

#endif

}
}

