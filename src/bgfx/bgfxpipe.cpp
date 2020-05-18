#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwbgfx.h"
#include "rwbgfxshader.h"

namespace rw {
namespace rwbgfx {

#ifdef RW_BGFX

void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_BGFX)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	bgfx::destroy(header->ibo);
	bgfx::destroy(header->vbo);
	rwFree(header->indexBuffer);
	rwFree(header->vertexBuffer);
	rwFree(header);
}

void*
destroyNativeData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

static InstanceDataHeader*
instanceMesh(rw::ObjPipeline *rwpipe, Geometry *geo)
{
	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY);
	header->ibo = { bgfx::kInvalidHandle };
	header->vbo = { bgfx::kInvalidHandle };

	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_BGFX;

	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = (meshh->flags & MeshHeader::TRISTRIP) ? BGFX_STATE_PT_TRISTRIP : uint64(0);
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);

	header->indexBuffer = rwNewT(uint16, header->totalNumIndex, MEMDUR_EVENT | ID_GEOMETRY);
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 offset = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, &inst->numVertices);
		assert(inst->minVert != 0xFFFFFFFF);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->program = 0;
		inst->offset = offset;
		memcpy((uint8*)header->indexBuffer + inst->offset,
		       mesh->indices, inst->numIndex*2);
		offset += inst->numIndex*2;
		mesh++;
		inst++;
	}

	header->vertexBuffer = nil;
	
	if (bgfx::isValid(header->ibo))
	{
		bgfx::destroy(header->ibo);
	}

	header->ibo = bgfx::createIndexBuffer(bgfx::copy(header->indexBuffer, header->totalNumIndex * 2));

	return header;
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	// don't try to (re)instance native data
	if(geo->flags & Geometry::NATIVE)
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(geo->instData){
		// Already have instanced data, so check if we have to reinstance
		assert(header->platform == PLATFORM_BGFX);
		if(header->serialNumber != geo->meshHeader->serialNum){
			// Mesh changed, so reinstance everything
			freeInstanceData(geo);
		}
	}

	// no instance or complete reinstance
	if(geo->instData == nil){
		geo->instData = instanceMesh(rwpipe, geo);
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 0);
	}else if(geo->lockedSinceInst)
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 1);

	geo->lockedSinceInst = 0;
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	assert(0 && "can't uninstance");
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	pipe->instance(atomic);
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_BGFX);
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform)
{
	this->impl.instance = rwbgfx::instance;
	this->impl.uninstance = rwbgfx::uninstance;
	this->impl.render = rwbgfx::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	bgfx::VertexLayout vl;
	vl.begin();

	vl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

	// Normals
	// TODO: compress
	if (hasNormals) {
		vl.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
	}

	// Prelighting
	if (isPrelit) {
		vl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
	} else {
		vl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
	}

	// Texture coordinates
	for (int32 n = 0; n < geo->numTexCoordSets; n++) {
		vl.add((bgfx::Attrib::Enum)((int)bgfx::Attrib::TexCoord0 + n), 2, bgfx::AttribType::Float);
	}

	vl.end();

	// TODO: does this make sense?
	if(!reinstance){
		//
		// Allocate vertex buffer
		//
		header->vertexBuffer = rwNewT(uint8, header->totalNumVertex*vl.getStride(), MEMDUR_EVENT | ID_GEOMETRY);
	}

	//
	// Fill vertex buffer
	//

	uint8 *verts = header->vertexBuffer;

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES){
		instV3d(VERT_FLOAT3, verts + vl.getOffset(bgfx::Attrib::Position),
			geo->morphTargets[0].vertices,
			header->totalNumVertex, vl.getStride());
	}

	// Normals
	if(hasNormals && (!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS)){
		instV3d(VERT_FLOAT3, verts + vl.getOffset(bgfx::Attrib::Normal),
			geo->morphTargets[0].normals,
			header->totalNumVertex, vl.getStride());
	}

	// Prelighting
	if(isPrelit && (!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT)){
		int n = header->numMeshes;
		InstanceData *inst = header->inst;
		while(n--){
			assert(inst->minVert != 0xFFFFFFFF);
			inst->vertexAlpha = instColor(VERT_RGBA,
				verts + vl.getOffset(bgfx::Attrib::Color0) + vl.getStride()*inst->minVert,
				geo->colors + inst->minVert,
				inst->numVertices, vl.getStride());
			inst++;
		}
	}
	else if (!isPrelit)
	{
		for (int i = 0; i < header->totalNumVertex; i++)
		{
			*(uint32_t*)(verts + vl.getOffset(bgfx::Attrib::Color0) + (vl.getStride() * i)) = 0xFFFFFFFF;
		}
	}

	// Texture coordinates
	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		if(!reinstance || geo->lockedSinceInst&(Geometry::LOCKTEXCOORDS<<n)){
			instTexCoords(VERT_FLOAT2, verts + vl.getOffset((bgfx::Attrib::Enum)((int)bgfx::Attrib::TexCoord0 + n)),
				geo->texCoords[n],
				header->totalNumVertex, vl.getStride());
		}
	}

	if (bgfx::isValid(header->vbo))
	{
		bgfx::destroy(header->vbo);
	}

	header->vbo = bgfx::createVertexBuffer(bgfx::makeRef(header->vertexBuffer, header->totalNumVertex * vl.getStride()), vl);
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_BGFX);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

#else
void *destroyNativeData(void *object, int32, int32) { return object; }
#endif

}
}
