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

#include "rwbgfximpl.h"

namespace rw {
namespace rwbgfx {

// TODO: make some of these things platform-independent

static void*
	driverOpen(void* o, int32, int32)
{
#ifdef RW_BGFX
	engine->driver[PLATFORM_BGFX]->defaultPipeline = makeDefaultPipeline();
#endif
	engine->driver[PLATFORM_BGFX]->rasterNativeOffset = nativeRasterOffset;
	engine->driver[PLATFORM_BGFX]->rasterCreate = rasterCreate;
	engine->driver[PLATFORM_BGFX]->rasterLock = rasterLock;
	engine->driver[PLATFORM_BGFX]->rasterUnlock = rasterUnlock;
	engine->driver[PLATFORM_BGFX]->rasterNumLevels = rasterNumLevels;
	engine->driver[PLATFORM_BGFX]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_BGFX]->rasterFromImage = rasterFromImage;

	return o;
}

static void*
	driverClose(void* o, int32, int32)
{
	return o;
}

void
	registerPlatformPlugins(void)
{
	Driver::registerPlugin(PLATFORM_BGFX, 0, PLATFORM_BGFX,
		driverOpen, driverClose);
	registerNativeRaster();
}

}
}
