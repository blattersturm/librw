#ifdef RW_BGFX
#include <bgfx/bgfx.h>

#ifdef RW_GLFW
#include <GLFW/glfw3.h>
#endif
#endif

namespace rw {

#ifdef RW_BGFX
struct EngineOpenParams
{
#ifdef RW_GLFW
	GLFWwindow **window;
#else
	void* nwh;
	void* context;
	void* backBuffer;
	void* backBufferDS;
#endif
	int width, height;
	const char *windowtitle;

	EngineOpenParams()
	{
#ifndef RW_GLFW
		nwh = NULL;
		context = NULL;
		backBuffer = NULL;
		backBufferDS = NULL;
#endif
	}
};
#endif

namespace rwbgfx {

void registerPlatformPlugins(void);

extern Device renderdevice;

// arguments to glVertexAttribPointer basically
struct AttribDesc
{
	uint32 index;
	int32  type;
	bool32 normalized;
	int32  size;
	uint32 stride;
	uint32 offset;
};

enum AttribIndices
{
	ATTRIB_POS = 0,
	ATTRIB_NORMAL,
	ATTRIB_COLOR,
	ATTRIB_TEXCOORDS0,
	ATTRIB_TEXCOORDS1,
	ATTRIB_TEXCOORDS2,
	ATTRIB_TEXCOORDS3,
	ATTRIB_TEXCOORDS4,
	ATTRIB_TEXCOORDS5,
	ATTRIB_TEXCOORDS6,
	ATTRIB_TEXCOORDS7,
	ATTRIB_WEIGHTS,
	ATTRIB_INDICES
};

#ifdef RW_BGFX
// default uniform indices
extern bgfx::UniformHandle u_matColor;
extern bgfx::UniformHandle u_surfProps;
#endif

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;	// not used for rendering
	int32     numVertices;	//
	Material *material;
	bool32    vertexAlpha;
	uint32    program;
	uint32    offset;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32      serialNumber;	// not really needed right now
	uint32      numMeshes;
	uint16     *indexBuffer;
	uint64      primType;
	uint8      *vertexBuffer;
	uint32      totalNumIndex;
	uint32      totalNumVertex;

#ifdef RW_BGFX
	bgfx::IndexBufferHandle      ibo;
	bgfx::VertexBufferHandle     vbo;
#endif

	InstanceData *inst;
};

#ifdef RW_BGFX

struct Shader;

extern Shader *defaultShader;

struct Im3DVertex
{
	V3d     position;
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }

	float getX(void) { return this->position.x; }
	float getY(void) { return this->position.y; }
	float getZ(void) { return this->position.z; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u, float recipz) { this->u = u; }
	void setV(float32 v, float recipz) { this->v = v; }

	float getScreenX(void) { return this->x; }
	float getScreenY(void) { return this->y; }
	float getScreenZ(void) { return this->z; }
	float getCameraZ(void) { return this->w; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

void setAttribPointers(AttribDesc *attribDescs, int32 numAttribs);
void disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs);

// Render state

// Vertex shader bits
enum
{
	// These should be low so they could be used as indices
	VSLIGHT_DIRECT	= 1,
	VSLIGHT_POINT	= 2,
	VSLIGHT_SPOT	= 4,
	VSLIGHT_MASK	= 7,	// all the above
	// less critical
	VSLIGHT_AMBIENT = 8,
};

extern const char *shaderDecl;	// #version stuff
extern const char *header_vert_src;
extern const char *header_frag_src;

// per Scene
void setProjectionMatrix(float32*);
void setViewMatrix(float32*);

// per Object
void setWorldMatrix(Matrix*);
int32 setLights(WorldLights *lightData);

// per Mesh
void setTexture(int32 n, Texture *tex);

void setAlphaBlend(bool32 enable);
bool32 getAlphaBlend(void);

uint32 bindTexture(uint32 texid);

void flushCache(uint64);

#endif

class ObjPipeline : public rw::ObjPipeline
{
public:
	void (*instanceCB)(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
	void (*uninstanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*renderCB)(Atomic *atomic, InstanceDataHeader *header);

	ObjPipeline(uint32 platform);
};

void defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
void defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultRenderCB(Atomic *atomic, InstanceDataHeader *header);
int32 lightingCB(Atomic *atomic);

void drawInst_simple(InstanceDataHeader *header, InstanceData *inst);
// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void drawInst_GSemu(InstanceDataHeader *header, InstanceData *inst);
// This one switches between the above two depending on render state;
void drawInst(InstanceDataHeader *header, InstanceData *inst);


void *destroyNativeData(void *object, int32, int32);

ObjPipeline *makeDefaultPipeline(void);

// Native Texture and Raster

extern int32 nativeRasterOffset;

struct BgfxRaster
{
#ifdef RW_BGFX
	// arguments to glTexImage2D
	bgfx::TextureFormat::Enum internalFormat;
	// texture object
	bgfx::TextureHandle texid;
#endif
	int32 type;
	int32 format;
	int32 bpp;	// bytes per pixel

	bool32 setName;
	bool32 hasAlpha;
	// cached filtermode and addressing
	uint8 filterMode;
	uint8 addressU;
	uint8 addressV;
};

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

void registerNativeRaster(void);

}
}
