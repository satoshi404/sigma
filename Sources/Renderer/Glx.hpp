#pragma once

#include <Core/Types.hpp>
#include <Renderer/GlxEnum.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx Handles
//
// Handle = indice no pool + geracao. Geracao 0 significa "invalido".
// Usar um handle ja destruido e detectado (a geracao do slot mudou) em vez de
// acessar o recurso errado.

template <GlxHandleType T>
struct GlxHandle
{
	u32 index = 0;
	u32 generation = 0;

	bool valid() const
	{
		return generation != 0;
	}
};

using GlxBuffer = GlxHandle<GlxHandleType::Buffer>;
using GlxTexture = GlxHandle<GlxHandleType::Texture>;
using GlxShader = GlxHandle<GlxHandleType::Shader>;
using GlxFramebuffer = GlxHandle<GlxHandleType::Framebuffer>;
using GlxVertexArray = GlxHandle<GlxHandleType::VertexArray>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex Layout

struct GlxVertexAttrib
{
	u32 location = 0;	// layout(location = N) no shader
	u32 components = 0; // 1..4
	GlxDataType type = GlxDataType::Float;
	bool normalized = false; // inteiros -> [0,1] / [-1,1]
	u32 offset = 0;			 // bytes desde o inicio do vertice
};

struct GlxVertexLayout
{
	static constexpr u32 MaxAttribs = 16;

	GlxVertexAttrib attribs[MaxAttribs];
	u32 count = 0;
	u32 stride = 0; // bytes por vertice
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx

namespace Glx
{
// Ciclo de vida. Precisa que Window::init() ja tenha sido chamado.
extern bool init();
extern void destroy();
extern void present();
extern void resize(u32 width, u32 height);
extern void setVsync(bool enabled);

// Retorna e limpa o ultimo erro
extern GlxError getError();

// Frame / estado
extern void clear(f32 r, f32 g, f32 b, f32 a = 1.0f, GlxClearMask mask = GlxClearMask::Color | GlxClearMask::Depth);
extern void setViewport(u32 x, u32 y, u32 width, u32 height);
extern void setDepthTest(bool enabled);
extern void setBlend(bool enabled); // alpha blending padrao (src_alpha, 1 - src_alpha)
extern void setCull(GlxCullMode mode);

// Buffers (tamanhos em bytes)
extern GlxBuffer bufferCreate(GlxBufferType type, GlxBufferUsage usage, const void* data, u32 size);
extern void bufferUpdate(GlxBuffer buffer, u32 offset, const void* data, u32 size);
extern void bufferDestroy(GlxBuffer buffer);

// Texturas 2D (filtro Linear + ClampToEdge por padrao)
extern GlxTexture textureCreate(u32 width, u32 height, GlxPixelFormat format, const void* data);
extern void textureSetFilter(GlxTexture texture, GlxTextureFilter minFilter, GlxTextureFilter magFilter);
extern void textureSetWrap(GlxTexture texture, GlxTextureWrap wrapS, GlxTextureWrap wrapT);
extern void textureBind(GlxTexture texture, u32 slot);
extern void textureDestroy(GlxTexture texture);

// Shaders (programa = vertex + fragment, codigo-fonte GLSL)
extern GlxShader shaderCreate(const char* vertexSource, const char* fragmentSource);
extern void shaderDestroy(GlxShader shader);
extern void shaderSetInt(GlxShader shader, const char* name, int value);
extern void shaderSetFloat(GlxShader shader, const char* name, f32 value);
extern void shaderSetVec2(GlxShader shader, const char* name, f32 x, f32 y);
extern void shaderSetVec3(GlxShader shader, const char* name, f32 x, f32 y, f32 z);
extern void shaderSetVec4(GlxShader shader, const char* name, f32 x, f32 y, f32 z, f32 w);
extern void shaderSetMat4(GlxShader shader, const char* name, const f32* columnMajor16);
extern void shaderUse(GlxShader shader);

// Vertex array: liga layout + vertex buffer (+ index buffer opcional)
extern GlxVertexArray vertexArrayCreate(const GlxVertexLayout& layout, GlxBuffer vertexBuffer, GlxBuffer indexBuffer = {});
extern void vertexArrayDestroy(GlxVertexArray vertexArray);

// Framebuffers (render para textura). Handle invalido em framebufferBind = backbuffer.
extern GlxFramebuffer framebufferCreate(GlxTexture color, bool withDepthStencil);
extern void framebufferBind(GlxFramebuffer framebuffer);
extern void framebufferDestroy(GlxFramebuffer framebuffer);

// Draw
extern void draw(GlxVertexArray vertexArray, GlxDrawMode mode, u32 first, u32 count);
extern void drawIndexed(GlxVertexArray vertexArray, GlxDrawMode mode, u32 count, GlxDataType indexType, u32 offsetBytes = 0);
} // namespace Glx