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
#ifdef RW_BGFX
#include "rwbgfx.h"
#include "rwbgfxshader.h"

namespace rw {
namespace rwbgfx {

Shader *currentShader;

Shader*
Shader::create(const char *vblob, size_t vsize, const char *fblob, size_t fsize)
{
	auto vs = bgfx::createShader(bgfx::copy(vblob, vsize));
	auto fs = bgfx::createShader(bgfx::copy(fblob, fsize));

	auto program = bgfx::createProgram(vs, fs, true);

	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);	 // or global?

	// query uniform locations
	sh->program = program;

	return sh;
}

void
Shader::use(void)
{
	if(currentShader != this){
		currentShader = this;
	}
}

void
Shader::destroy(void)
{
	bgfx::destroy(program);
}

}
}

#endif
