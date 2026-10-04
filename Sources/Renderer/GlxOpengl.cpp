#include <Renderer/Glx.hpp>
#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/Platform/Window.hpp>
#include <Vendor/Opengl/Opengl.hpp>

#include <Renderer/GlxConfig.hpp>

#if PIPELINE_OS_WINDOWS && GLX_BACKEND_OPENGL

#include <cstdint>
#include <vector>

// GL 1.0 (exportada pelo opengl32.dll) que o Opengl.hpp ainda nao declara
extern "C" void GL_API glPixelStorei(GLenum, GLint);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constantes GL usadas aqui (locais para nao depender do que o Opengl.hpp define)

namespace Gl
{
constexpr GLenum NO_ERROR_ = 0;
constexpr GLenum INVALID_ENUM = 0x0500;
constexpr GLenum INVALID_VALUE = 0x0501;
constexpr GLenum INVALID_OPERATION = 0x0502;
constexpr GLenum OUT_OF_MEMORY = 0x0505;
constexpr GLenum INVALID_FRAMEBUFFER_OPERATION = 0x0506;
constexpr GLenum CONTEXT_LOST = 0x0507;

constexpr GLenum ARRAY_BUFFER = 0x8892;
constexpr GLenum COPY_WRITE_BUFFER = 0x8F37;
constexpr GLenum STATIC_DRAW = 0x88E4;
constexpr GLenum DYNAMIC_DRAW = 0x88E8;
constexpr GLenum STREAM_DRAW = 0x88E0;

constexpr GLenum VERTEX_SHADER = 0x8B31;
constexpr GLenum FRAGMENT_SHADER = 0x8B30;
constexpr GLenum COMPILE_STATUS = 0x8B81;
constexpr GLenum LINK_STATUS = 0x8B82;

constexpr GLenum TEXTURE0 = 0x84C0;
constexpr GLenum TEXTURE_2D = 0x0DE1;
constexpr GLenum TEXTURE_BINDING_2D = 0x8069;
constexpr GLenum TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum TEXTURE_WRAP_S = 0x2802;
constexpr GLenum TEXTURE_WRAP_T = 0x2803;
constexpr GLint NEAREST = 0x2600;
constexpr GLint LINEAR = 0x2601;
constexpr GLint CLAMP_TO_EDGE = 0x812F;
constexpr GLint REPEAT = 0x2901;
constexpr GLint R8 = 0x8229;
constexpr GLint RGB8 = 0x8051;
constexpr GLint RGBA8 = 0x8058;
constexpr GLenum RED = 0x1903;
constexpr GLenum RGB = 0x1907;
constexpr GLenum RGBA = 0x1908;
constexpr GLenum UNPACK_ALIGNMENT = 0x0CF5;

constexpr GLenum FRAMEBUFFER = 0x8D40;
constexpr GLenum RENDERBUFFER = 0x8D41;
constexpr GLenum COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum DEPTH_STENCIL_ATTACHMENT = 0x821A;
constexpr GLenum DEPTH24_STENCIL8 = 0x88F0;
constexpr GLenum FRAMEBUFFER_COMPLETE = 0x8CD5;

constexpr GLenum BYTE_ = 0x1400;
constexpr GLenum UNSIGNED_BYTE = 0x1401;
constexpr GLenum UNSIGNED_SHORT = 0x1403;
constexpr GLenum INT_ = 0x1404;
constexpr GLenum UNSIGNED_INT = 0x1405;
constexpr GLenum FLOAT_ = 0x1406;

constexpr GLenum POINTS = 0x0000;
constexpr GLenum LINES = 0x0001;
constexpr GLenum LINE_STRIP = 0x0003;
constexpr GLenum TRIANGLES = 0x0004;
constexpr GLenum TRIANGLE_STRIP = 0x0005;

constexpr GLenum CULL_FACE = 0x0B44;
constexpr GLenum DEPTH_TEST = 0x0B71;
constexpr GLenum BLEND = 0x0BE2;
constexpr GLenum SRC_ALPHA = 0x0302;
constexpr GLenum ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum FRONT = 0x0404;
constexpr GLenum BACK = 0x0405;

constexpr GLbitfield COLOR_BUFFER_BIT = 0x4000;
constexpr GLbitfield DEPTH_BUFFER_BIT = 0x0100;
constexpr GLbitfield STENCIL_BUFFER_BIT = 0x0400;
} // namespace Gl

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Funcoes GL 2.0+ (carregadas via opengl_proc; opengl32.dll so exporta GL 1.1)

#define GLX_GL_FUNCS(X)                                                                     \
	X(GenBuffers, void, (GLsizei, GLuint*) )                                                \
	X(BindBuffer, void, (GLenum, GLuint))                                                   \
	X(BufferData, void, (GLenum, ptrdiff_t, const void*, GLenum))                           \
	X(BufferSubData, void, (GLenum, ptrdiff_t, ptrdiff_t, const void*) )                    \
	X(DeleteBuffers, void, (GLsizei, const GLuint*) )                                       \
	X(CreateShader, GLuint, (GLenum))                                                       \
	X(ShaderSource, void, (GLuint, GLsizei, const char* const*, const GLint*) )             \
	X(CompileShader, void, (GLuint))                                                        \
	X(GetShaderiv, void, (GLuint, GLenum, GLint*) )                                         \
	X(GetShaderInfoLog, void, (GLuint, GLsizei, GLsizei*, char*) )                          \
	X(DeleteShader, void, (GLuint))                                                         \
	X(CreateProgram, GLuint, ())                                                            \
	X(AttachShader, void, (GLuint, GLuint))                                                 \
	X(LinkProgram, void, (GLuint))                                                          \
	X(GetProgramiv, void, (GLuint, GLenum, GLint*) )                                        \
	X(GetProgramInfoLog, void, (GLuint, GLsizei, GLsizei*, char*) )                         \
	X(DeleteProgram, void, (GLuint))                                                        \
	X(UseProgram, void, (GLuint))                                                           \
	X(GetUniformLocation, GLint, (GLuint, const char*) )                                    \
	X(Uniform1i, void, (GLint, GLint))                                                      \
	X(Uniform1f, void, (GLint, GLfloat))                                                    \
	X(Uniform2f, void, (GLint, GLfloat, GLfloat))                                           \
	X(Uniform3f, void, (GLint, GLfloat, GLfloat, GLfloat))                                  \
	X(Uniform4f, void, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                         \
	X(UniformMatrix4fv, void, (GLint, GLsizei, GLboolean, const GLfloat*) )                 \
	X(GenVertexArrays, void, (GLsizei, GLuint*) )                                           \
	X(BindVertexArray, void, (GLuint))                                                      \
	X(DeleteVertexArrays, void, (GLsizei, const GLuint*) )                                  \
	X(EnableVertexAttribArray, void, (GLuint))                                              \
	X(VertexAttribPointer, void, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) ) \
	X(VertexAttribIPointer, void, (GLuint, GLint, GLenum, GLsizei, const void*) )           \
	X(GenFramebuffers, void, (GLsizei, GLuint*) )                                           \
	X(BindFramebuffer, void, (GLenum, GLuint))                                              \
	X(FramebufferTexture2D, void, (GLenum, GLenum, GLenum, GLuint, GLint))                  \
	X(CheckFramebufferStatus, GLenum, (GLenum))                                             \
	X(DeleteFramebuffers, void, (GLsizei, const GLuint*) )                                  \
	X(GenRenderbuffers, void, (GLsizei, GLuint*) )                                          \
	X(BindRenderbuffer, void, (GLenum, GLuint))                                             \
	X(RenderbufferStorage, void, (GLenum, GLenum, GLsizei, GLsizei))                        \
	X(FramebufferRenderbuffer, void, (GLenum, GLenum, GLenum, GLuint))                      \
	X(DeleteRenderbuffers, void, (GLsizei, const GLuint*) )                                 \
	X(ActiveTexture, void, (GLenum))

namespace
{
#define X(name, ret, args)                \
	using PFN_##name = ret(GL_API*) args; \
	PFN_##name fn##name = nullptr;
GLX_GL_FUNCS(X)
#undef X

bool loadFunctions()
{
	bool ok = true;
#define X(name, ret, args)                                  \
	fn##name = (PFN_##name) opengl_proc("gl" #name);        \
	if (!fn##name)                                          \
	{                                                       \
		LOG_ERROR("Glx: missing OpenGL function gl" #name); \
		ok = false;                                         \
	}
	GLX_GL_FUNCS(X)
#undef X
	return ok;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pool com geracao

template <typename T>
class Pool
{
public:
	struct Slot
	{
		T value{};
		u32 generation = 1;
		bool used = false;
	};

	void alloc(const T& value, u32& index, u32& generation)
	{
		u32 i;
		if (!freeList.empty())
		{
			i = freeList.back();
			freeList.pop_back();
		}
		else
		{
			i = (u32) slots.size();
			slots.push_back(Slot{});
		}
		slots[i].value = value;
		slots[i].used = true;
		index = i;
		generation = slots[i].generation;
	}

	T* get(u32 index, u32 generation)
	{
		if (generation == 0 || index >= slots.size())
			return nullptr;
		Slot& s = slots[index];
		return (s.used && s.generation == generation) ? &s.value : nullptr;
	}

	bool free(u32 index, u32 generation)
	{
		if (!get(index, generation))
			return false;
		Slot& s = slots[index];
		s.used = false;
		s.value = T{};
		if (++s.generation == 0) // 0 e reservado para "invalido"
			s.generation = 1;
		freeList.push_back(index);
		return true;
	}

	template <typename F>
	void forEachUsed(F&& f)
	{
		for (Slot& s : slots)
			if (s.used)
				f(s.value);
	}

	void clear()
	{
		slots.clear();
		freeList.clear();
	}

private:
	std::vector<Slot> slots;
	std::vector<u32> freeList;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Recursos

struct BufferRes
{
	GLuint id = 0;
	GlxBufferType type = GlxBufferType::Vertex;
	u32 size = 0;
};
struct TextureRes
{
	GLuint id = 0;
	u32 width = 0;
	u32 height = 0;
};
struct ShaderRes
{
	GLuint program = 0;
};
struct FramebufferRes
{
	GLuint fbo = 0;
	GLuint depthRbo = 0;
	u32 width = 0;
	u32 height = 0;
};
struct VertexArrayRes
{
	GLuint vao = 0;
	bool hasIndex = false;
};

struct State
{
	bool initialized = false;
	bool vsync = true;
	u32 width = 0;
	u32 height = 0;
	GLuint curProgram = 0;
	GLuint curFbo = 0;
	GlxError lastError = GlxError::None;

	Pool<BufferRes> buffers;
	Pool<TextureRes> textures;
	Pool<ShaderRes> shaders;
	Pool<FramebufferRes> framebuffers;
	Pool<VertexArrayRes> vaos;
};

State g;

void setError(GlxError e)
{
	if (g.lastError == GlxError::None)
		g.lastError = e;
}

template <GlxHandleType T, typename R>
R* find(Pool<R>& pool, GlxHandle<T> h)
{
	R* r = pool.get(h.index, h.generation);
	if (!r)
		setError(GlxError::InvalidHandle);
	return r;
}

template <GlxHandleType T, typename R>
GlxHandle<T> add(Pool<R>& pool, const R& res)
{
	GlxHandle<T> h;
	pool.alloc(res, h.index, h.generation);
	return h;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conversoes Glx -> GL

GlxError fromGlError(GLenum e)
{
	switch (e)
	{
	case Gl::NO_ERROR_:
		return GlxError::None;
	case Gl::INVALID_ENUM:
		return GlxError::InvalidEnum;
	case Gl::INVALID_VALUE:
		return GlxError::InvalidValue;
	case Gl::INVALID_OPERATION:
		return GlxError::InvalidOperation;
	case Gl::OUT_OF_MEMORY:
		return GlxError::OutOfMemory;
	case Gl::INVALID_FRAMEBUFFER_OPERATION:
		return GlxError::InvalidFramebufferOperation;
	case Gl::CONTEXT_LOST:
		return GlxError::ContextLost;
	default:
		return GlxError::InvalidOperation;
	}
}

GLenum glUsage(GlxBufferUsage u)
{
	switch (u)
	{
	case GlxBufferUsage::Static:
		return Gl::STATIC_DRAW;
	case GlxBufferUsage::Dynamic:
		return Gl::DYNAMIC_DRAW;
	case GlxBufferUsage::Stream:
		return Gl::STREAM_DRAW;
	}
	return Gl::STATIC_DRAW;
}

GLenum glDataType(GlxDataType t)
{
	switch (t)
	{
	case GlxDataType::Float:
		return Gl::FLOAT_;
	case GlxDataType::Int:
		return Gl::INT_;
	case GlxDataType::UnsignedInt:
		return Gl::UNSIGNED_INT;
	case GlxDataType::UnsignedShort:
		return Gl::UNSIGNED_SHORT;
	case GlxDataType::UnsignedByte:
		return Gl::UNSIGNED_BYTE;
	}
	return Gl::FLOAT_;
}

GLenum glTopology(GlxDrawMode m)
{
	switch (m)
	{
	case GlxDrawMode::Points:
		return Gl::POINTS;
	case GlxDrawMode::Lines:
		return Gl::LINES;
	case GlxDrawMode::LineStrip:
		return Gl::LINE_STRIP;
	case GlxDrawMode::Triangles:
		return Gl::TRIANGLES;
	case GlxDrawMode::TriangleStrip:
		return Gl::TRIANGLE_STRIP;
	}
	return Gl::TRIANGLES;
}

GLint glFilter(GlxTextureFilter f)
{
	return f == GlxTextureFilter::Nearest ? Gl::NEAREST : Gl::LINEAR;
}

GLint glWrap(GlxTextureWrap w)
{
	return w == GlxTextureWrap::Repeat ? Gl::REPEAT : Gl::CLAMP_TO_EDGE;
}

// Binda a textura e restaura o binding anterior ao sair do escopo,
// para criar/alterar texturas sem bagunçar o que esta em uso no frame.
struct TexBindScope
{
	GLint prev = 0;
	explicit TexBindScope(GLuint id)
	{
		glGetIntegerv(Gl::TEXTURE_BINDING_2D, &prev);
		glBindTexture(Gl::TEXTURE_2D, id);
	}
	~TexBindScope()
	{
		glBindTexture(Gl::TEXTURE_2D, (GLuint) prev);
	}
};

GLuint compileShader(GLenum type, const char* source)
{
	GLuint id = fnCreateShader(type);
	fnShaderSource(id, 1, &source, nullptr);
	fnCompileShader(id);

	GLint ok = 0;
	fnGetShaderiv(id, Gl::COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[1024] = {};
		fnGetShaderInfoLog(id, (GLsizei) sizeof(log), nullptr, log);
		LOG_ERROR("Glx: %s shader compile error:\n%s",
				  type == Gl::VERTEX_SHADER ? "vertex" : "fragment", log);
		fnDeleteShader(id);
		return 0;
	}
	return id;
}

ShaderRes* useAndFind(GlxShader shader)
{
	ShaderRes* s = find(g.shaders, shader);
	if (!s)
		return nullptr;
	if (g.curProgram != s->program)
	{
		fnUseProgram(s->program);
		g.curProgram = s->program;
	}
	return s;
}

void deleteBuffer(BufferRes& r)
{
	fnDeleteBuffers(1, &r.id);
}
void deleteTexture(TextureRes& r)
{
	glDeleteTextures(1, &r.id);
}
void deleteVertexArray(VertexArrayRes& r)
{
	fnDeleteVertexArrays(1, &r.vao);
}

void deleteShader(ShaderRes& r)
{
	if (g.curProgram == r.program)
	{
		fnUseProgram(0);
		g.curProgram = 0;
	}
	fnDeleteProgram(r.program);
}

void deleteFramebuffer(FramebufferRes& r)
{
	if (g.curFbo == r.fbo)
	{
		fnBindFramebuffer(Gl::FRAMEBUFFER, 0);
		g.curFbo = 0;
	}
	if (r.depthRbo)
		fnDeleteRenderbuffers(1, &r.depthRbo);
	fnDeleteFramebuffers(1, &r.fbo);
}
} // namespace

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx (OpenGL)

namespace Glx
{
bool init()
{
	if (g.initialized)
		return true;

	if (!opengl_init())
		return false;

	if (!loadFunctions())
	{
		LOG_ERROR("Glx: OpenGL 3.3+ core e necessario");
		opengl_free();
		return false;
	}

	g.width = WindowAtrib::get_width();
	g.height = WindowAtrib::get_height();
	glViewport(0, 0, (GLsizei) g.width, (GLsizei) g.height);
	// opengl_set_vsync(g.vsync);

	g.initialized = true;
	LOG_INFO("Glx initialized (OpenGL)");
	return true;
}

void destroy()
{
	if (!g.initialized)
		return;

	// Libera o que o usuario esqueceu de destruir
	g.framebuffers.forEachUsed([](FramebufferRes& r)
							   { deleteFramebuffer(r); LOG_WARNING("Glx: framebuffer vazado"); });
	g.vaos.forEachUsed([](VertexArrayRes& r)
					   { deleteVertexArray(r); LOG_WARNING("Glx: vertex array vazado"); });
	g.shaders.forEachUsed([](ShaderRes& r)
						  { deleteShader(r);      LOG_WARNING("Glx: shader vazado"); });
	g.textures.forEachUsed([](TextureRes& r)
						   { deleteTexture(r);     LOG_WARNING("Glx: texture vazada"); });
	g.buffers.forEachUsed([](BufferRes& r)
						  { deleteBuffer(r);      LOG_WARNING("Glx: buffer vazado"); });

	g.framebuffers.clear();
	g.vaos.clear();
	g.shaders.clear();
	g.textures.clear();
	g.buffers.clear();

	opengl_free();

	g = State{};
	LOG_INFO("Glx destroyed (OpenGL)");
}

void present()
{
	opengl_swap();
}

void resize(u32 width, u32 height)
{
	if (width == 0 || height == 0)
		return;

	g.width = width;
	g.height = height;
	if (g.curFbo == 0)
		glViewport(0, 0, (GLsizei) width, (GLsizei) height);
}

void setVsync(bool enabled)
{
	g.vsync = enabled;
	if (g.initialized)
		opengl_set_vsync(enabled);
}

GlxError getError()
{
	GlxError e = g.lastError;
	g.lastError = GlxError::None;
	if (e == GlxError::None && g.initialized)
		e = fromGlError(glGetError());
	return e;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Frame / estado

void clear(f32 r, f32 g_, f32 b, f32 a, GlxClearMask mask)
{
	GLbitfield bits = 0;
	if (glxHas(mask, GlxClearMask::Color))
		bits |= Gl::COLOR_BUFFER_BIT;
	if (glxHas(mask, GlxClearMask::Depth))
		bits |= Gl::DEPTH_BUFFER_BIT;
	if (glxHas(mask, GlxClearMask::Stencil))
		bits |= Gl::STENCIL_BUFFER_BIT;
	if (!bits)
		return;

	glClearColor(r, g_, b, a);
	glDepthMask(GL_TRUE); // glClear respeita a depth mask; garante que limpa mesmo apos desabilitar escrita
	glClear(bits);
}

void setViewport(u32 x, u32 y, u32 width, u32 height)
{
	glViewport((GLint) x, (GLint) y, (GLsizei) width, (GLsizei) height);
}

void setDepthTest(bool enabled)
{
	if (enabled)
	{
		glEnable(Gl::DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
	else
	{
		glDisable(Gl::DEPTH_TEST);
	}
}

void setBlend(bool enabled)
{
	if (enabled)
	{
		glEnable(Gl::BLEND);
		glBlendFunc(Gl::SRC_ALPHA, Gl::ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(Gl::BLEND);
	}
}

void setCull(GlxCullMode mode)
{
	if (mode == GlxCullMode::None)
	{
		glDisable(Gl::CULL_FACE);
		return;
	}
	glEnable(Gl::CULL_FACE);
	glCullFace(mode == GlxCullMode::Back ? Gl::BACK : Gl::FRONT);
	glFrontFace(GL_CCW);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Buffers
//
// Upload sempre via GL_COPY_WRITE_BUFFER: nao mexe no GL_ELEMENT_ARRAY_BUFFER
// do VAO que estiver bindado (bug classico de bindar index buffer com VAO ativo).

GlxBuffer bufferCreate(GlxBufferType type, GlxBufferUsage usage, const void* data, u32 size)
{
	if (size == 0)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	BufferRes r;
	r.type = type;
	r.size = size;
	fnGenBuffers(1, &r.id);
	fnBindBuffer(Gl::COPY_WRITE_BUFFER, r.id);
	fnBufferData(Gl::COPY_WRITE_BUFFER, (ptrdiff_t) size, data, glUsage(usage));
	fnBindBuffer(Gl::COPY_WRITE_BUFFER, 0);

	return add<GlxHandleType::Buffer>(g.buffers, r);
}

void bufferUpdate(GlxBuffer buffer, u32 offset, const void* data, u32 size)
{
	BufferRes* r = find(g.buffers, buffer);
	if (!r)
		return;
	if (!data || offset > r->size || size > r->size - offset)
	{
		setError(GlxError::InvalidValue);
		return;
	}

	fnBindBuffer(Gl::COPY_WRITE_BUFFER, r->id);
	fnBufferSubData(Gl::COPY_WRITE_BUFFER, (ptrdiff_t) offset, (ptrdiff_t) size, data);
	fnBindBuffer(Gl::COPY_WRITE_BUFFER, 0);
}

void bufferDestroy(GlxBuffer buffer)
{
	BufferRes* r = find(g.buffers, buffer);
	if (!r)
		return;
	deleteBuffer(*r);
	g.buffers.free(buffer.index, buffer.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Texturas

GlxTexture textureCreate(u32 width, u32 height, GlxPixelFormat format, const void* data)
{
	if (width == 0 || height == 0)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	GLint internalFormat = Gl::RGBA8;
	GLenum pixelFormat = Gl::RGBA;
	switch (format)
	{
	case GlxPixelFormat::R8:
		internalFormat = Gl::R8;
		pixelFormat = Gl::RED;
		break;
	case GlxPixelFormat::RGB8:
		internalFormat = Gl::RGB8;
		pixelFormat = Gl::RGB;
		break;
	case GlxPixelFormat::RGBA8:
		internalFormat = Gl::RGBA8;
		pixelFormat = Gl::RGBA;
		break;
	}

	TextureRes r;
	r.width = width;
	r.height = height;
	glGenTextures(1, &r.id);

	{
		TexBindScope bind(r.id);

		// Padrao GL e NEAREST_MIPMAP_LINEAR: sem mipmaps a textura fica "incompleta" (preta).
		glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MIN_FILTER, Gl::LINEAR);
		glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MAG_FILTER, Gl::LINEAR);
		glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_S, Gl::CLAMP_TO_EDGE);
		glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_T, Gl::CLAMP_TO_EDGE);

		glPixelStorei(Gl::UNPACK_ALIGNMENT, 1); // linhas de R8/RGB8 nao sao multiplas de 4
		glTexImage2D(Gl::TEXTURE_2D, 0, internalFormat, (GLsizei) width, (GLsizei) height, 0,
					 pixelFormat, Gl::UNSIGNED_BYTE, data);
		glPixelStorei(Gl::UNPACK_ALIGNMENT, 4);
	}

	return add<GlxHandleType::Texture>(g.textures, r);
}

void textureSetFilter(GlxTexture texture, GlxTextureFilter minFilter, GlxTextureFilter magFilter)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;

	TexBindScope bind(t->id);
	glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MIN_FILTER, glFilter(minFilter));
	glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_MAG_FILTER, glFilter(magFilter));
}

void textureSetWrap(GlxTexture texture, GlxTextureWrap wrapS, GlxTextureWrap wrapT)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;

	TexBindScope bind(t->id);
	glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_S, glWrap(wrapS));
	glTexParameteri(Gl::TEXTURE_2D, Gl::TEXTURE_WRAP_T, glWrap(wrapT));
}

void textureBind(GlxTexture texture, u32 slot)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;

	fnActiveTexture(Gl::TEXTURE0 + slot);
	glBindTexture(Gl::TEXTURE_2D, t->id);
}

void textureDestroy(GlxTexture texture)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;
	deleteTexture(*t);
	g.textures.free(texture.index, texture.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shaders

GlxShader shaderCreate(const char* vertexSource, const char* fragmentSource)
{
	if (!vertexSource || !fragmentSource)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	GLuint vs = compileShader(Gl::VERTEX_SHADER, vertexSource);
	if (!vs)
	{
		setError(GlxError::ShaderCompile);
		return {};
	}

	GLuint fs = compileShader(Gl::FRAGMENT_SHADER, fragmentSource);
	if (!fs)
	{
		fnDeleteShader(vs);
		setError(GlxError::ShaderCompile);
		return {};
	}

	GLuint program = fnCreateProgram();
	fnAttachShader(program, vs);
	fnAttachShader(program, fs);
	fnLinkProgram(program);

	// Depois do link os shaders individuais nao sao mais necessarios
	fnDeleteShader(vs);
	fnDeleteShader(fs);

	GLint ok = 0;
	fnGetProgramiv(program, Gl::LINK_STATUS, &ok);
	if (!ok)
	{
		char log[1024] = {};
		fnGetProgramInfoLog(program, (GLsizei) sizeof(log), nullptr, log);
		LOG_ERROR("Glx: shader link error:\n%s", log);
		fnDeleteProgram(program);
		setError(GlxError::ShaderLink);
		return {};
	}

	ShaderRes r;
	r.program = program;
	return add<GlxHandleType::Shader>(g.shaders, r);
}

void shaderDestroy(GlxShader shader)
{
	ShaderRes* s = find(g.shaders, shader);
	if (!s)
		return;
	deleteShader(*s);
	g.shaders.free(shader.index, shader.generation);
}

void shaderUse(GlxShader shader)
{
	useAndFind(shader);
}

// Setters: usam o programa (binda se preciso) e ignoram uniforms inexistentes
// (o driver remove uniforms nao usados, entao location -1 e normal).

void shaderSetInt(GlxShader shader, const char* name, int value)
{
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniform1i(loc, value);
	}
}

void shaderSetFloat(GlxShader shader, const char* name, f32 value)
{
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniform1f(loc, value);
	}
}

void shaderSetVec2(GlxShader shader, const char* name, f32 x, f32 y)
{
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniform2f(loc, x, y);
	}
}

void shaderSetVec3(GlxShader shader, const char* name, f32 x, f32 y, f32 z)
{
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniform3f(loc, x, y, z);
	}
}

void shaderSetVec4(GlxShader shader, const char* name, f32 x, f32 y, f32 z, f32 w)
{
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniform4f(loc, x, y, z, w);
	}
}

void shaderSetMat4(GlxShader shader, const char* name, const f32* columnMajor16)
{
	if (!columnMajor16)
	{
		setError(GlxError::InvalidValue);
		return;
	}
	if (ShaderRes* s = useAndFind(shader))
	{
		GLint loc = fnGetUniformLocation(s->program, name);
		if (loc >= 0)
			fnUniformMatrix4fv(loc, 1, GL_FALSE, columnMajor16);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex Array

GlxVertexArray vertexArrayCreate(const GlxVertexLayout& layout, GlxBuffer vertexBuffer, GlxBuffer indexBuffer)
{
	BufferRes* vbo = find(g.buffers, vertexBuffer);
	if (!vbo)
		return {};

	if (layout.count == 0 || layout.count > GlxVertexLayout::MaxAttribs || layout.stride == 0)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	BufferRes* ibo = nullptr;
	if (indexBuffer.valid())
	{
		ibo = find(g.buffers, indexBuffer);
		if (!ibo)
			return {};
	}

	VertexArrayRes r;
	r.hasIndex = (ibo != nullptr);

	fnGenVertexArrays(1, &r.vao);
	fnBindVertexArray(r.vao);
	fnBindBuffer(Gl::ARRAY_BUFFER, vbo->id);

	for (u32 i = 0; i < layout.count; i++)
	{
		const GlxVertexAttrib& a = layout.attribs[i];
		const GLenum type = glDataType(a.type);
		const void* off = (const void*) (uintptr_t) a.offset;

		fnEnableVertexAttribArray(a.location);

		const bool isIntType = (a.type == GlxDataType::Int || a.type == GlxDataType::UnsignedInt);
		if (isIntType && !a.normalized)
			fnVertexAttribIPointer(a.location, (GLint) a.components, type, (GLsizei) layout.stride, off);
		else
			fnVertexAttribPointer(a.location, (GLint) a.components, type,
								  a.normalized ? GL_TRUE : GL_FALSE, (GLsizei) layout.stride, off);
	}

	if (ibo)
		fnBindBuffer(0x8893 /* GL_ELEMENT_ARRAY_BUFFER */, ibo->id); // fica guardado no VAO

	fnBindVertexArray(0);
	fnBindBuffer(Gl::ARRAY_BUFFER, 0);

	return add<GlxHandleType::VertexArray>(g.vaos, r);
}

void vertexArrayDestroy(GlxVertexArray vertexArray)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v)
		return;
	deleteVertexArray(*v);
	g.vaos.free(vertexArray.index, vertexArray.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Framebuffers

GlxFramebuffer framebufferCreate(GlxTexture color, bool withDepthStencil)
{
	TextureRes* t = find(g.textures, color);
	if (!t)
		return {};

	FramebufferRes r;
	r.width = t->width;
	r.height = t->height;

	fnGenFramebuffers(1, &r.fbo);
	fnBindFramebuffer(Gl::FRAMEBUFFER, r.fbo);
	fnFramebufferTexture2D(Gl::FRAMEBUFFER, Gl::COLOR_ATTACHMENT0, Gl::TEXTURE_2D, t->id, 0);

	if (withDepthStencil)
	{
		fnGenRenderbuffers(1, &r.depthRbo);
		fnBindRenderbuffer(Gl::RENDERBUFFER, r.depthRbo);
		fnRenderbufferStorage(Gl::RENDERBUFFER, Gl::DEPTH24_STENCIL8, (GLsizei) r.width, (GLsizei) r.height);
		fnFramebufferRenderbuffer(Gl::FRAMEBUFFER, Gl::DEPTH_STENCIL_ATTACHMENT, Gl::RENDERBUFFER, r.depthRbo);
		fnBindRenderbuffer(Gl::RENDERBUFFER, 0);
	}

	const GLenum status = fnCheckFramebufferStatus(Gl::FRAMEBUFFER);
	fnBindFramebuffer(Gl::FRAMEBUFFER, g.curFbo); // restaura o que estava ativo

	if (status != Gl::FRAMEBUFFER_COMPLETE)
	{
		LOG_ERROR("Glx: framebuffer incompleto (status=0x%04X)", (unsigned) status);
		if (r.depthRbo)
			fnDeleteRenderbuffers(1, &r.depthRbo);
		fnDeleteFramebuffers(1, &r.fbo);
		setError(GlxError::InvalidFramebufferOperation);
		return {};
	}

	return add<GlxHandleType::Framebuffer>(g.framebuffers, r);
}

void framebufferBind(GlxFramebuffer framebuffer)
{
	if (!framebuffer.valid())
	{
		fnBindFramebuffer(Gl::FRAMEBUFFER, 0);
		g.curFbo = 0;
		glViewport(0, 0, (GLsizei) g.width, (GLsizei) g.height);
		return;
	}

	FramebufferRes* f = find(g.framebuffers, framebuffer);
	if (!f)
		return;

	fnBindFramebuffer(Gl::FRAMEBUFFER, f->fbo);
	g.curFbo = f->fbo;
	glViewport(0, 0, (GLsizei) f->width, (GLsizei) f->height);
}

void framebufferDestroy(GlxFramebuffer framebuffer)
{
	FramebufferRes* f = find(g.framebuffers, framebuffer);
	if (!f)
		return;
	deleteFramebuffer(*f);
	g.framebuffers.free(framebuffer.index, framebuffer.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw

void draw(GlxVertexArray vertexArray, GlxDrawMode mode, u32 first, u32 count)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v)
		return;

	fnBindVertexArray(v->vao);
	glDrawArrays(glTopology(mode), (GLint) first, (GLsizei) count);
}

void drawIndexed(GlxVertexArray vertexArray, GlxDrawMode mode, u32 count, GlxDataType indexType, u32 offsetBytes)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v)
		return;

	if (!v->hasIndex ||
		(indexType != GlxDataType::UnsignedByte &&
		 indexType != GlxDataType::UnsignedShort &&
		 indexType != GlxDataType::UnsignedInt))
	{
		setError(GlxError::InvalidOperation);
		return;
	}

	fnBindVertexArray(v->vao);
	glDrawElements(glTopology(mode), (GLsizei) count, glDataType(indexType), (const void*) (uintptr_t) offsetBytes);
}
} // namespace Glx

#endif // GLX_GL_HPP