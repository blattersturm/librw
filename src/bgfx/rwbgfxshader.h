#ifdef RW_BGFX

#include <bgfx/bgfx.h>

namespace rw {
namespace rwbgfx {

struct Shader
{
	bgfx::ProgramHandle program;
	// same number of elements as UniformRegistry::numUniforms
	bgfx::UniformHandle *uniformLocations;

	static Shader *create(const char *vblob, size_t vsize, const char *fblob, size_t fsize);
	void use(void);
	void destroy(void);
};

extern Shader *currentShader;

}
}

#endif
