#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#ifdef RW_BGFX
#include "rwbgfx.h"
#include "rwbgfximpl.h"
#include "rwbgfxshader.h"

namespace rw {
namespace rwbgfx {

bgfx::DynamicIndexBufferHandle im2DIb;
bgfx::DynamicVertexBufferHandle im2DVb;
static bgfx::UniformHandle u_xform;

#define STARTINDICES 10000
#define STARTVERTICES 10000

static Shader *im2dShader;
static bgfx::VertexLayout im2dattribDesc = []()
{
	bgfx::VertexLayout vl;
	vl.begin()
		.add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();

	return vl;
}();

static uint64_t primTypeMap[] = {
	BGFX_STATE_PT_POINTS,	// invalid
	BGFX_STATE_PT_LINES,
	BGFX_STATE_PT_LINESTRIP,
	0,
	BGFX_STATE_PT_TRISTRIP,
	BGFX_STATE_PT_TRISTRIP,   // invalid, trifan
	BGFX_STATE_PT_POINTS
};

void
openIm2D(void)
{
	u_xform = bgfx::createUniform("u_xform", bgfx::UniformType::Vec4);

#include "shaders/im2d_vs_bgfx.inc"
#include "shaders/simple_fs_bgfx.inc"

	im2dShader = Shader::create((char*)im2d_vs_bgfx, sizeof(im2d_vs_bgfx), (char*)simple_fs_bgfx, sizeof(simple_fs_bgfx));
	assert(im2dShader);

	im2DVb = bgfx::createDynamicVertexBuffer(STARTVERTICES, im2dattribDesc);

	im2DIb = bgfx::createDynamicIndexBuffer(STARTINDICES);
}

void
closeIm2D(void)
{
	bgfx::destroy(im2DVb);
	bgfx::destroy(im2DIb);
	im2dShader->destroy();
	im2dShader = nil;
}

static Im2DVertex tmpprimbuf[3];

void
im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	im2DRenderPrimitive(PRIMTYPELINELIST, tmpprimbuf, 2);
}

void
im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	tmpprimbuf[2] = verts[vert3];
	im2DRenderPrimitive(PRIMTYPETRILIST, tmpprimbuf, 3);
}

uint32 sortIdx;

void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	if (primType == PRIMTYPETRIFAN)
	{
		primType = PRIMTYPETRILIST;
		
		int inNum = numVertices;
		
		static Im2DVertex svertices[128];
		numVertices = (inNum - 2) * 3;

		Im2DVertex* ivertices = (Im2DVertex*)vertices;

		for (int i = 0; i < inNum - 2; i++)
		{
			svertices[i * 3] = ivertices[0];
			svertices[i * 3 + 1] = ivertices[i + 1];
			svertices[i * 3 + 2] = ivertices[i + 2];
		}

		vertices = svertices;
	}

	float xform[4];
	Camera *cam;
	cam = (Camera*)engine->currentCamera;

	bgfx::TransientIndexBuffer tib;
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientBuffers(&tvb, im2dattribDesc, numVertices, &tib, numVertices);

	memcpy(tvb.data, vertices, numVertices * sizeof(Im2DVertex));
	//bgfx::update(im2DVb, 0, bgfx::copy(vertices, numVertices * sizeof(Im2DVertex)));

	xform[0] = 2.0f/cam->frameBuffer->width;
	xform[1] = -2.0f/cam->frameBuffer->height;
	xform[2] = -1.0f;
	xform[3] = 1.0f;

	im2dShader->use();
	bgfx::setUniform(u_xform, xform);

	flushCache(primTypeMap[primType]);

	for (int i = 0; i < numVertices; ++i)
	{
		*(uint16_t*)&tib.data[i * sizeof(uint16_t)] = i;
	}

	bgfx::setIndexBuffer(&tib);
	bgfx::setVertexBuffer(0, &tvb);
	bgfx::submit(viewId, im2dShader->program, sortIdx);
	sortIdx--;
}

void
im2DRenderIndexedPrimitive(PrimitiveType primType,
	void *vertices, int32 numVertices,
	void *indices, int32 numIndices)
{
	float xform[4];
	Camera* cam;
	cam = (Camera*)engine->currentCamera;

	bgfx::TransientIndexBuffer tib;
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientBuffers(&tvb, im2dattribDesc, numVertices, &tib, numIndices);

	memcpy(tib.data, indices, numIndices * 2);
	memcpy(tvb.data, vertices, numVertices * sizeof(Im2DVertex));

	xform[0] = 2.0f / cam->frameBuffer->width;
	xform[1] = -2.0f / cam->frameBuffer->height;
	xform[2] = -1.0f;
	xform[3] = 1.0f;

	im2dShader->use();
	bgfx::setUniform(u_xform, xform);

	flushCache(primTypeMap[primType]);

	bgfx::setVertexBuffer(0, &tvb);
	bgfx::setIndexBuffer(&tib);
	bgfx::submit(viewId, im2dShader->program);
}


// Im3D


static Shader *im3dShader;
static bgfx::VertexLayout im3dattribDesc = []()
{
	bgfx::VertexLayout vl;
	vl.begin()
		.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();

	return vl;
}();
static bgfx::DynamicIndexBufferHandle im3DIb;
static bgfx::DynamicVertexBufferHandle im3DVb;
static uint32 num3dVertices;

void
openIm3D(void)
{
#include "shaders/im3d_vs_bgfx.inc"
#include "shaders/simple_fs_bgfx.inc"

	im3dShader = Shader::create((char*)im3d_vs_bgfx, sizeof(im3d_vs_bgfx), (char*)simple_fs_bgfx, sizeof(simple_fs_bgfx));
	assert(im3dShader);

	im3DVb = bgfx::createDynamicVertexBuffer(STARTVERTICES, im3dattribDesc);

	im3DIb = bgfx::createDynamicIndexBuffer(STARTINDICES);
}

void
closeIm3D(void)
{
	bgfx::destroy(im3DVb);
	bgfx::destroy(im3DIb);
	im3dShader->destroy();
	im3dShader = nil;
}

void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	if(world == nil){
		Matrix ident;
		ident.setIdentity();
		world = &ident;
	}
	setWorldMatrix(world);
	im3dShader->use();

	if((flags & im3d::VERTEXUV) == 0)
		SetRenderStatePtr(TEXTURERASTER, nil);

	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientVertexBuffer(&tvb, numVertices, im3dattribDesc);
	memcpy(tvb.data, vertices, numVertices * sizeof(Im3DVertex));

	bgfx::setVertexBuffer(0, &tvb);

	num3dVertices = numVertices;
}

void
im3DRenderPrimitive(PrimitiveType primType)
{
	flushCache(primTypeMap[primType]);

	bgfx::TransientIndexBuffer tib;
	bgfx::allocTransientIndexBuffer(&tib, num3dVertices * 3);

	for (int i = 0; i < num3dVertices; ++i)
	{
		*(uint16_t*)&tib.data[i * sizeof(uint16_t)] = i;
	}

	bgfx::setVertexBuffer(0, im3DVb);
	bgfx::setIndexBuffer(&tib);
	bgfx::submit(viewId, im3dShader->program);
}

void
im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices)
{
	flushCache(primTypeMap[primType]);

	bgfx::TransientIndexBuffer tib;
	bgfx::allocTransientIndexBuffer(&tib, numIndices);

	memcpy(tib.data, indices, numIndices * 2);

	bgfx::setIndexBuffer(&tib);
	bgfx::submit(viewId, im3dShader->program);
}

void
im3DEnd(void)
{
}

}
}

#endif
