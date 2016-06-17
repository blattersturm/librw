namespace rw {

#define PLUGINOFFSET(type, base, offset) \
	((type*)((char*)(base) + (offset)))

typedef void *(*Constructor)(void *object, int32 offset, int32 size);
typedef void *(*Destructor)(void *object, int32 offset, int32 size);
typedef void *(*CopyConstructor)(void *dst, void *src, int32 offset, int32 size);
typedef Stream *(*StreamRead)(Stream *stream, int32 length, void *object, int32 offset, int32 size);
typedef Stream *(*StreamWrite)(Stream *stream, int32 length, void *object, int32 offset, int32 size);
typedef int32 (*StreamGetSize)(void *object, int32 offset, int32 size);
typedef void (*RightsCallback)(void *object, int32 offset, int32 size, uint32 data);

struct Plugin
{
	int32 offset;
	int32 size;
	uint32 id;
	Constructor constructor;
	Destructor destructor;
	CopyConstructor copy;
	StreamRead read;
	StreamWrite write;
	StreamGetSize getSize;
	RightsCallback rightsCallback;
	Plugin *next;
};

template <typename T>
struct PluginBase
{
	static int32 s_defaultSize;
	static int32 s_size;
	static Plugin *s_plugins;

	void constructPlugins(void);
	void destructPlugins(void);
	void copyPlugins(T *t);
	bool streamReadPlugins(Stream *stream);
	void streamWritePlugins(Stream *stream);
	int streamGetPluginSize(void);
	void assertRights(uint32 pluginID, uint32 data);

	static int registerPlugin(int32 size, uint32 id,
	                          Constructor, Destructor, CopyConstructor);
	static int registerPluginStream(uint32 id,
	                                StreamRead, StreamWrite, StreamGetSize);
	static int setStreamRightsCallback(uint32 id, RightsCallback cb);
	static int getPluginOffset(uint32 id);
	static void *operator new(size_t size);
	static void operator delete(void *p);
};
template <typename T>
int PluginBase<T>::s_defaultSize = sizeof(T);
template <typename T>
int PluginBase<T>::s_size = sizeof(T);
template <typename T>
Plugin *PluginBase<T>::s_plugins = 0;

template <typename T> void
PluginBase<T>::constructPlugins(void)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->constructor)
			p->constructor((void*)this, p->offset, p->size);
}

template <typename T> void
PluginBase<T>::destructPlugins(void)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->destructor)
			p->destructor((void*)this, p->offset, p->size);
}

template <typename T> void
PluginBase<T>::copyPlugins(T *t)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->copy)
			p->copy((void*)this, (void*)t, p->offset, p->size);
}

template <typename T> bool
PluginBase<T>::streamReadPlugins(Stream *stream)
{
	int32 length;
	ChunkHeaderInfo header;
	if(!findChunk(stream, ID_EXTENSION, (uint32*)&length, NULL))
		return false;
	while(length > 0){
		if(!readChunkHeaderInfo(stream, &header))
			return false;
		length -= 12;
		for(Plugin *p = this->s_plugins; p; p = p->next)
			if(p->id == header.type && p->read){
				p->read(stream, header.length,
				        (void*)this, p->offset, p->size);
				goto cont;
			}
		//printf("skipping plugin %X\n", header.type);
		stream->seek(header.length);
cont:
		length -= header.length;
	}
	return true;
}

template <typename T> void
PluginBase<T>::streamWritePlugins(Stream *stream)
{
	int size = this->streamGetPluginSize();
	writeChunkHeader(stream, ID_EXTENSION, size);
	for(Plugin *p = this->s_plugins; p; p = p->next){
		if(p->getSize == NULL ||
		   (size = p->getSize(this, p->offset, p->size)) <= 0)
			continue;
		writeChunkHeader(stream, p->id, size);
		p->write(stream, size, this, p->offset, p->size);
	}
}

template <typename T> int32
PluginBase<T>::streamGetPluginSize(void)
{
	int32 size = 0;
	int32 plgsize;
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->getSize &&
		   (plgsize = p->getSize(this, p->offset, p->size)) > 0)
			size += 12 + plgsize;
	return size;
}

template <typename T> void
PluginBase<T>::assertRights(uint32 pluginID, uint32 data)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->id == pluginID){
			if(p->rightsCallback)
				p->rightsCallback(this,
				                  p->offset, p->size, data);
			return;
		}
}

template <typename T> int32
PluginBase<T>::registerPlugin(int32 size, uint32 id,
	Constructor ctor, Destructor dtor, CopyConstructor cctor)
{
	Plugin *p = new Plugin;
	p->offset = s_size;
	s_size += size;

	p->size = size;
	p->id = id;
	p->constructor = ctor;
	p->copy = cctor;
	p->destructor = dtor;
	p->read = NULL;
	p->write = NULL;
	p->getSize = NULL;
	p->rightsCallback = NULL;

	Plugin **next;
	for(next = &s_plugins; *next; next = &(*next)->next)
		;
	*next = p;
	p->next = NULL;
	return p->offset;
}

template <typename T> int32
PluginBase<T>::registerPluginStream(uint32 id,
	StreamRead read, StreamWrite write, StreamGetSize getSize)
{
	for(Plugin *p = PluginBase<T>::s_plugins; p; p = p->next)
		if(p->id == id){
			p->read = read;
			p->write = write;
			p->getSize = getSize;
			return p->offset;
		}
	return -1;
}

template <typename T> int32
PluginBase<T>::setStreamRightsCallback(uint32 id, RightsCallback cb)
{
	for(Plugin *p = PluginBase<T>::s_plugins; p; p = p->next)
		if(p->id == id){
			p->rightsCallback = cb;
			return p->offset;
		}
	return -1;
}

template <typename T> int32
PluginBase<T>::getPluginOffset(uint32 id)
{
	for(Plugin *p = PluginBase<T>::s_plugins; p; p = p->next)
		if(p->id == id)
			return p->offset;
	return -1;
}

template <typename T> void*
PluginBase<T>::operator new(size_t)
{
	abort();
	return NULL;
}

template <typename T> void
PluginBase<T>::operator delete(void *p)
{
	abort();
}
}
