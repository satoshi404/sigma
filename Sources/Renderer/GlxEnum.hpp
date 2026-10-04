#pragma once

#include <Core/Types.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx Enums
//
// Os valores sao SEQUENCIAIS e independentes de API grafica. Cada backend
// (OpenGL, D3D11, ...) converte para os seus proprios valores internamente.

enum class GlxHandleType : u32
{
	Invalid = 0,
	Buffer,
	Texture,
	Shader,
	Framebuffer,
	VertexArray,
	Count
};

enum class GlxError : u32
{
	None = 0,
	InvalidEnum,
	InvalidValue,
	InvalidOperation,
	InvalidHandle,
	InvalidFramebufferOperation,
	OutOfMemory,
	ShaderCompile,
	ShaderLink,
	Unsupported,
	ContextLost
};

enum class GlxClearMask : u32
{
	None = 0,
	Color = 1 << 0,
	Depth = 1 << 1,
	Stencil = 1 << 2
};

constexpr GlxClearMask operator|(GlxClearMask a, GlxClearMask b)
{
	return (GlxClearMask) ((u32) a | (u32) b);
}

constexpr bool glxHas(GlxClearMask value, GlxClearMask flag)
{
	return ((u32) value & (u32) flag) != 0;
}

enum class GlxShaderType : u32
{
	Vertex,
	Fragment
};

enum class GlxPixelFormat : u32
{
	R8,
	RGB8,
	RGBA8
};

enum class GlxTextureFilter : u32
{
	Nearest,
	Linear
};

enum class GlxTextureWrap : u32
{
	ClampToEdge,
	Repeat
};

enum class GlxFramebufferStatus : u32
{
	Complete,
	IncompleteAttachment,
	IncompleteMissingAttachment,
	Unsupported,
	Unknown
};

enum class GlxBufferUsage : u32
{
	Static,
	Dynamic,
	Stream
};

enum class GlxBufferType : u32
{
	Vertex,
	Index,
	Uniform
};

// LineLoop e TriangleFan ficaram de fora de proposito: D3D11 nao suporta.
enum class GlxDrawMode : u32
{
	Points,
	Lines,
	LineStrip,
	Triangles,
	TriangleStrip
};

enum class GlxDataType : u32
{
	Float,
	Int,
	UnsignedInt,
	UnsignedShort,
	UnsignedByte
};

enum class GlxCullMode : u32
{
	None,
	Back,
	Front
};