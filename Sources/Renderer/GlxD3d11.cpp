#include <Core/DetectionPipeline.hpp>
#include <Renderer/GlxConfig.hpp>

#if PIPELINE_OS_WINDOWS && GLX_BACKEND_D3D11

#include <Core/Logger.hpp>
#include <Core/Platform/Window.hpp>
#include <Renderer/Glx.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>

#include <stdint.h>
#include <string.h>
#include <vector>

// Link (MSYS2/clang): -ld3d11 -ldxgi -ld3dcompiler_47
// Se o Window.hpp ja declara Window::hwnd, apague esta linha.
namespace Window
{
extern HWND hwnd;
}

#define HR_U(hr) ((unsigned) (hr))

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convencoes deste backend (shaders sao HLSL, nao GLSL):
//   - Entry points: VSMain (vertex) e PSMain (pixel), shader model 5.0
//   - Atributos de vertice: semantica ATTR<location>  (ex.: float2 pos : ATTR0)
//   - Uniforms: variaveis de cbuffer, procuradas pelo nome em shaderSetXxx
//   - Texturas: register(t<slot>) / sampler register(s<slot>), slot = textureBind(slot)

namespace
{
// IID_ID3D11ShaderReflection escrito a mao (evita depender do dxguid)
const GUID kIidShaderReflection = {0x8d536ca1, 0x0cca, 0x4956, {0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84}};

// Release manual, no lugar de ComPtr
template <typename T>
void release(T*& p)
{
	if (p)
	{
		p->Release();
		p = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pool com geracao (mesma ideia do backend OpenGL)

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
		if (++s.generation == 0)
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
	ID3D11Buffer* buffer = nullptr;
	GlxBufferType type = GlxBufferType::Vertex;
	u32 size = 0; // tamanho pedido pelo usuario (o da GPU pode ser maior em uniform buffers)
};

struct TextureRes
{
	ID3D11Texture2D* tex = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11SamplerState* sampler = nullptr;
	u32 width = 0;
	u32 height = 0;
	GlxTextureFilter minFilter = GlxTextureFilter::Linear;
	GlxTextureFilter magFilter = GlxTextureFilter::Linear;
	GlxTextureWrap wrapS = GlxTextureWrap::ClampToEdge;
	GlxTextureWrap wrapT = GlxTextureWrap::ClampToEdge;
};

struct CBufferRes
{
	u32 slot = 0;
	u32 size = 0; // multiplo de 16
	ID3D11Buffer* gpu = nullptr;
	std::vector<uint8_t> cpu; // copia local; sobe para a GPU no draw se estiver suja
	bool dirty = true;
};

struct UniformRes
{
	char name[64] = {};
	u32 cbuffer = 0; // indice em ShaderRes::cbuffers
	u32 offset = 0;
	u32 size = 0;
};

struct ShaderRes
{
	ID3D11VertexShader* vs = nullptr;
	ID3D11PixelShader* ps = nullptr;
	ID3DBlob* vsBlob = nullptr; // bytecode do VS: necessario para criar o input layout
	std::vector<CBufferRes> cbuffers;
	std::vector<UniformRes> uniforms;
};

struct FramebufferRes
{
	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11Texture2D* depthTex = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;
	u32 width = 0;
	u32 height = 0;
};

struct VertexArrayRes
{
	ID3D11Buffer* vb = nullptr; // AddRef: o VAO mantem os buffers vivos
	ID3D11Buffer* ib = nullptr;
	bool hasIndex = false;
	GlxVertexLayout layout;

	// Input layout depende do shader (assinatura do VS): criado sob demanda e cacheado
	ID3D11InputLayout* inputLayout = nullptr;
	u32 layoutShaderIndex = 0;
	u32 layoutShaderGen = 0;
};

struct State
{
	bool initialized = false;
	bool vsync = true;
	bool tearing = false;
	bool deviceLost = false;
	UINT swapFlags = 0;
	u32 width = 0;
	u32 height = 0;
	GlxError lastError = GlxError::None;

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	IDXGISwapChain1* swap = nullptr;

	// Backbuffer
	ID3D11RenderTargetView* backRtv = nullptr;
	ID3D11Texture2D* backDepthTex = nullptr;
	ID3D11DepthStencilView* backDsv = nullptr;

	// Alvo atual (nao-owning)
	ID3D11RenderTargetView* curRtv = nullptr;
	ID3D11DepthStencilView* curDsv = nullptr;
	GlxFramebuffer curFb; // invalido = backbuffer
	GlxShader curShader;

	// Estados de pipeline pre-criados
	ID3D11RasterizerState* raster[3] = {};
	ID3D11DepthStencilState* depthState[2] = {};
	ID3D11BlendState* blendState[2] = {};
	bool depthTest = false;
	bool blend = false;
	GlxCullMode cull = GlxCullMode::None;

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
// Conversoes Glx -> D3D11

D3D11_PRIMITIVE_TOPOLOGY topology(GlxDrawMode m)
{
	switch (m)
	{
	case GlxDrawMode::Points:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case GlxDrawMode::Lines:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case GlxDrawMode::LineStrip:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case GlxDrawMode::Triangles:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case GlxDrawMode::TriangleStrip:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	}
	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

D3D11_FILTER filterFor(GlxTextureFilter minF, GlxTextureFilter magF)
{
	// Bits do enum D3D11_FILTER: 0x10 = min linear, 0x04 = mag linear, 0x01 = mip linear
	UINT f = 0x01;
	if (minF == GlxTextureFilter::Linear)
		f |= 0x10;
	if (magF == GlxTextureFilter::Linear)
		f |= 0x04;
	return (D3D11_FILTER) f;
}

D3D11_TEXTURE_ADDRESS_MODE addressFor(GlxTextureWrap w)
{
	return w == GlxTextureWrap::Repeat ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
}

DXGI_FORMAT vertexFormat(GlxDataType t, u32 n, bool norm)
{
	switch (t)
	{
	case GlxDataType::Float:
		if (n == 1)
			return DXGI_FORMAT_R32_FLOAT;
		if (n == 2)
			return DXGI_FORMAT_R32G32_FLOAT;
		if (n == 3)
			return DXGI_FORMAT_R32G32B32_FLOAT;
		if (n == 4)
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case GlxDataType::Int:
		if (n == 1)
			return DXGI_FORMAT_R32_SINT;
		if (n == 2)
			return DXGI_FORMAT_R32G32_SINT;
		if (n == 3)
			return DXGI_FORMAT_R32G32B32_SINT;
		if (n == 4)
			return DXGI_FORMAT_R32G32B32A32_SINT;
		break;
	case GlxDataType::UnsignedInt:
		if (n == 1)
			return DXGI_FORMAT_R32_UINT;
		if (n == 2)
			return DXGI_FORMAT_R32G32_UINT;
		if (n == 3)
			return DXGI_FORMAT_R32G32B32_UINT;
		if (n == 4)
			return DXGI_FORMAT_R32G32B32A32_UINT;
		break;
	case GlxDataType::UnsignedShort: // 3 componentes nao existe em D3D11
		if (n == 1)
			return norm ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
		if (n == 2)
			return norm ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
		if (n == 4)
			return norm ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;
		break;
	case GlxDataType::UnsignedByte: // 3 componentes nao existe em D3D11
		if (n == 1)
			return norm ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
		if (n == 2)
			return norm ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
		if (n == 4)
			return norm ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
		break;
	}
	return DXGI_FORMAT_UNKNOWN;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Alvos de render

bool createBackbufferViews()
{
	ID3D11Texture2D* bb = nullptr;
	HRESULT hr = g.swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &bb);
	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: GetBuffer failed (hr=0x%08X)", HR_U(hr));
		return false;
	}

	hr = g.device->CreateRenderTargetView(bb, nullptr, &g.backRtv);
	release(bb);
	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: CreateRenderTargetView failed (hr=0x%08X)", HR_U(hr));
		return false;
	}

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = g.width;
	td.Height = g.height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = g.device->CreateTexture2D(&td, nullptr, &g.backDepthTex);
	if (SUCCEEDED(hr))
		hr = g.device->CreateDepthStencilView(g.backDepthTex, nullptr, &g.backDsv);
	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: backbuffer depth creation failed (hr=0x%08X)", HR_U(hr));
		return false;
	}
	return true;
}

void releaseBackbufferViews()
{
	release(g.backDsv);
	release(g.backDepthTex);
	release(g.backRtv);
}

void setFullViewport(u32 w, u32 h)
{
	D3D11_VIEWPORT vp = {};
	vp.Width = (FLOAT) w;
	vp.Height = (FLOAT) h;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	g.context->RSSetViewports(1, &vp);
}

void useBackbuffer()
{
	g.curFb = GlxFramebuffer{};
	g.curRtv = g.backRtv;
	g.curDsv = g.backDsv;
	setFullViewport(g.width, g.height);
}

// Flip model desvincula o backbuffer apos Present: rebind antes de usar
void bindTargets()
{
	ID3D11RenderTargetView* rtv = g.curRtv;
	g.context->OMSetRenderTargets(1, &rtv, g.curDsv);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Estados de pipeline

bool createStates()
{
	// Rasterizer: [None, Back, Front]. GL usa CCW como frente.
	const D3D11_CULL_MODE cullModes[3] = {D3D11_CULL_NONE, D3D11_CULL_BACK, D3D11_CULL_FRONT};
	for (int i = 0; i < 3; i++)
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = cullModes[i];
		rd.FrontCounterClockwise = TRUE;
		rd.DepthClipEnable = TRUE;
		if (FAILED(g.device->CreateRasterizerState(&rd, &g.raster[i])))
			return false;
	}

	// Depth: [off, on]
	for (int i = 0; i < 2; i++)
	{
		D3D11_DEPTH_STENCIL_DESC dd = {};
		dd.DepthEnable = i == 1 ? TRUE : FALSE;
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(g.device->CreateDepthStencilState(&dd, &g.depthState[i])))
			return false;
	}

	// Blend: [off, on] (src_alpha, 1 - src_alpha)
	for (int i = 0; i < 2; i++)
	{
		D3D11_BLEND_DESC bd = {};
		D3D11_RENDER_TARGET_BLEND_DESC& rt = bd.RenderTarget[0];
		rt.BlendEnable = i == 1 ? TRUE : FALSE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(g.device->CreateBlendState(&bd, &g.blendState[i])))
			return false;
	}
	return true;
}

void applyStates()
{
	g.context->RSSetState(g.raster[(u32) g.cull]);
	g.context->OMSetDepthStencilState(g.depthState[g.depthTest ? 1 : 0], 0);
	g.context->OMSetBlendState(g.blendState[g.blend ? 1 : 0], nullptr, 0xFFFFFFFF);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device

bool createDevice(D3D_DRIVER_TYPE type, UINT flags, D3D_FEATURE_LEVEL& level)
{
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0};

	HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, flags, levels, ARRAYSIZE(levels),
								   D3D11_SDK_VERSION, &g.device, &level, &g.context);

	// Sistemas sem FL 11_1 retornam E_INVALIDARG
	if (hr == E_INVALIDARG)
	{
		hr = D3D11CreateDevice(nullptr, type, nullptr, flags, levels + 1, ARRAYSIZE(levels) - 1,
							   D3D11_SDK_VERSION, &g.device, &level, &g.context);
	}
	return SUCCEEDED(hr);
}

bool createDeviceWithFallback(D3D_FEATURE_LEVEL& level)
{
	UINT flags = 0;
#if defined(PIPELINE_DEBUG)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	bool ok = createDevice(D3D_DRIVER_TYPE_HARDWARE, flags, level);

	if (!ok && (flags & D3D11_CREATE_DEVICE_DEBUG)) // debug layer nao instalada
	{
		LOG_WARNING("GlxD3D11: debug layer indisponivel, tentando sem ela");
		flags &= ~D3D11_CREATE_DEVICE_DEBUG;
		ok = createDevice(D3D_DRIVER_TYPE_HARDWARE, flags, level);
	}

	if (!ok)
	{
		LOG_WARNING("GlxD3D11: hardware falhou, usando WARP");
		ok = createDevice(D3D_DRIVER_TYPE_WARP, flags, level);
	}

	if (!ok)
		LOG_ERROR("GlxD3D11: failed to create D3D11 device");
	return ok;
}

// Libera TUDO que o device/swapchain criou, mesmo se o init falhou no meio
void releaseDeviceObjects()
{
	if (g.context)
	{
		g.context->OMSetRenderTargets(0, nullptr, nullptr);
		g.context->ClearState();
		g.context->Flush();
	}

	for (int i = 0; i < 3; i++) release(g.raster[i]);
	for (int i = 0; i < 2; i++) release(g.depthState[i]);
	for (int i = 0; i < 2; i++) release(g.blendState[i]);

	releaseBackbufferViews();
	release(g.swap);
	release(g.context);
	release(g.device);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruicao de recursos

void destroyBuffer(BufferRes& r)
{
	release(r.buffer);
}

void destroyTexture(TextureRes& r)
{
	release(r.sampler);
	release(r.srv);
	release(r.tex);
}

void destroyShader(ShaderRes& r)
{
	for (CBufferRes& cb : r.cbuffers)
		release(cb.gpu);
	release(r.vsBlob);
	release(r.vs);
	release(r.ps);
}

void destroyFramebuffer(FramebufferRes& r)
{
	release(r.dsv);
	release(r.depthTex);
	release(r.rtv);
}

void destroyVertexArray(VertexArrayRes& r)
{
	release(r.inputLayout);
	release(r.vb);
	release(r.ib);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shaders (HLSL)

ID3DBlob* compileHlsl(const char* source, const char* entry, const char* target)
{
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(PIPELINE_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob* code = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT hr = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, entry, target, flags, 0, &code, &errors);

	if (FAILED(hr))
	{
		LOG_ERROR("Glx: HLSL compile error (%s):\n%s", entry,
				  errors ? (const char*) errors->GetBufferPointer() : "(sem mensagem)");
		release(errors);
		release(code);
		return nullptr;
	}

	release(errors); // pode conter warnings
	return code;
}

// Le os cbuffers/variaveis do bytecode para permitir shaderSetXxx por nome
void reflectShader(ID3DBlob* blob, ShaderRes& shader)
{
	ID3D11ShaderReflection* refl = nullptr;
	if (FAILED(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), kIidShaderReflection, (void**) &refl)))
		return;

	D3D11_SHADER_DESC sd = {};
	refl->GetDesc(&sd);

	for (UINT i = 0; i < sd.ConstantBuffers; i++)
	{
		ID3D11ShaderReflectionConstantBuffer* cb = refl->GetConstantBufferByIndex(i);

		D3D11_SHADER_BUFFER_DESC bd = {};
		if (FAILED(cb->GetDesc(&bd)) || bd.Type != D3D_CT_CBUFFER)
			continue;

		D3D11_SHADER_INPUT_BIND_DESC ib = {};
		if (FAILED(refl->GetResourceBindingDescByName(bd.Name, &ib)))
			continue;

		// Mesmo slot em VS e PS = mesmo cbuffer
		u32 index = (u32) shader.cbuffers.size();
		for (u32 k = 0; k < (u32) shader.cbuffers.size(); k++)
		{
			if (shader.cbuffers[k].slot == ib.BindPoint)
			{
				index = k;
				break;
			}
		}

		const u32 size = (bd.Size + 15) & ~15u;
		if (index == (u32) shader.cbuffers.size())
		{
			CBufferRes c;
			c.slot = ib.BindPoint;
			c.size = size;
			c.cpu.assign(size, 0);
			shader.cbuffers.push_back(c);
		}
		else if (size > shader.cbuffers[index].size)
		{
			shader.cbuffers[index].size = size;
			shader.cbuffers[index].cpu.resize(size, 0);
		}

		for (UINT j = 0; j < bd.Variables; j++)
		{
			D3D11_SHADER_VARIABLE_DESC vd = {};
			if (FAILED(cb->GetVariableByIndex(j)->GetDesc(&vd)))
				continue;

			bool exists = false;
			for (const UniformRes& u : shader.uniforms)
			{
				if (u.cbuffer == index && strcmp(u.name, vd.Name) == 0)
				{
					exists = true;
					break;
				}
			}
			if (exists)
				continue;

			UniformRes u;
			strncpy(u.name, vd.Name, sizeof(u.name) - 1);
			u.cbuffer = index;
			u.offset = vd.StartOffset;
			u.size = vd.Size;
			shader.uniforms.push_back(u);
		}
	}

	release(refl);
}

bool createConstantBuffers(ShaderRes& shader)
{
	for (CBufferRes& cb : shader.cbuffers)
	{
		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = cb.size;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(g.device->CreateBuffer(&bd, nullptr, &cb.gpu)))
			return false;
	}
	return true;
}

void setUniform(GlxShader shader, const char* name, const void* data, u32 bytes)
{
	ShaderRes* s = find(g.shaders, shader);
	if (!s || !name || !data)
		return;

	// Uniform inexistente (ou removido pelo compilador) e normal: ignora, como no GL
	for (UniformRes& u : s->uniforms)
	{
		if (strcmp(u.name, name) != 0)
			continue;

		CBufferRes& cb = s->cbuffers[u.cbuffer];
		const u32 n = bytes < u.size ? bytes : u.size;
		memcpy(cb.cpu.data() + u.offset, data, n);
		cb.dirty = true;
		return;
	}
}

// Sobe cbuffers sujos e liga tudo ao pipeline
void flushShader(ShaderRes& s)
{
	for (CBufferRes& cb : s.cbuffers)
	{
		if (cb.dirty)
		{
			g.context->UpdateSubresource(cb.gpu, 0, nullptr, cb.cpu.data(), 0, 0);
			cb.dirty = false;
		}
		g.context->VSSetConstantBuffers(cb.slot, 1, &cb.gpu);
		g.context->PSSetConstantBuffers(cb.slot, 1, &cb.gpu);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw

bool buildInputLayout(VertexArrayRes& v, ShaderRes& s)
{
	D3D11_INPUT_ELEMENT_DESC elems[GlxVertexLayout::MaxAttribs] = {};
	for (u32 i = 0; i < v.layout.count; i++)
	{
		const GlxVertexAttrib& a = v.layout.attribs[i];
		elems[i].SemanticName = "ATTR";
		elems[i].SemanticIndex = a.location;
		elems[i].Format = vertexFormat(a.type, a.components, a.normalized);
		elems[i].InputSlot = 0;
		elems[i].AlignedByteOffset = a.offset;
		elems[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	}

	release(v.inputLayout);
	HRESULT hr = g.device->CreateInputLayout(elems, v.layout.count, s.vsBlob->GetBufferPointer(),
											 s.vsBlob->GetBufferSize(), &v.inputLayout);
	if (FAILED(hr))
	{
		LOG_ERROR("Glx: CreateInputLayout failed (hr=0x%08X). O VS usa semanticas ATTR<location>?", HR_U(hr));
		setError(GlxError::InvalidOperation);
		return false;
	}
	return true;
}

bool prepareDraw(VertexArrayRes& v, GlxDrawMode mode)
{
	ShaderRes* s = g.curShader.valid() ? g.shaders.get(g.curShader.index, g.curShader.generation) : nullptr;
	if (!s)
	{
		setError(GlxError::InvalidOperation); // nenhum shader ativo (shaderUse)
		return false;
	}

	if (!v.inputLayout || v.layoutShaderIndex != g.curShader.index || v.layoutShaderGen != g.curShader.generation)
	{
		if (!buildInputLayout(v, *s))
			return false;
		v.layoutShaderIndex = g.curShader.index;
		v.layoutShaderGen = g.curShader.generation;
	}

	bindTargets();
	applyStates();

	const UINT stride = v.layout.stride;
	const UINT offset = 0;
	g.context->IASetInputLayout(v.inputLayout);
	g.context->IASetVertexBuffers(0, 1, &v.vb, &stride, &offset);
	g.context->IASetPrimitiveTopology(topology(mode));

	g.context->VSSetShader(s->vs, nullptr, 0);
	g.context->PSSetShader(s->ps, nullptr, 0);
	flushShader(*s);
	return true;
}
} // namespace

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx (D3D11)

namespace Glx
{
bool init()
{
	if (g.initialized)
		return true;

	HWND hwnd = Window::hwnd;
	if (!hwnd)
	{
		LOG_ERROR("GlxD3D11: window not created (HWND null)");
		return false;
	}

	g.width = WindowAtrib::get_width();
	g.height = WindowAtrib::get_height();

	D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
	if (!createDeviceWithFallback(level))
		return false;

	// DXGI factory (a mesma que criou o device)
	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* adapter = nullptr;
	IDXGIFactory2* factory = nullptr;

	HRESULT hr = g.device->QueryInterface(__uuidof(IDXGIDevice), (void**) &dxgiDevice);
	if (SUCCEEDED(hr))
		hr = dxgiDevice->GetAdapter(&adapter);
	if (SUCCEEDED(hr))
		hr = adapter->GetParent(__uuidof(IDXGIFactory2), (void**) &factory);
	release(adapter);
	release(dxgiDevice);

	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: failed to get DXGI factory (hr=0x%08X)", HR_U(hr));
		releaseDeviceObjects();
		return false;
	}

	// Tearing (vsync off sem travar o frame)
	IDXGIFactory5* factory5 = nullptr;
	if (SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5), (void**) &factory5)))
	{
		BOOL allow = FALSE;
		if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
			g.tearing = allow == TRUE;
		release(factory5);
	}
	g.swapFlags = g.tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = g.width;
	desc.Height = g.height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 2;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	desc.Flags = g.swapFlags;

	hr = factory->CreateSwapChainForHwnd(g.device, hwnd, &desc, nullptr, nullptr, &g.swap);
	if (SUCCEEDED(hr))
		factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	release(factory);

	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: failed to create swap chain (hr=0x%08X)", HR_U(hr));
		releaseDeviceObjects();
		return false;
	}

	if (!createBackbufferViews() || !createStates())
	{
		releaseDeviceObjects();
		return false;
	}

	useBackbuffer();

	g.initialized = true;
	LOG_INFO("Glx initialized (D3D11) Feature Level %d.%d%s",
			 (level >> 12) & 0xF, (level >> 8) & 0xF, g.tearing ? " [tearing]" : "");
	return true;
}

void destroy()
{
	if (!g.initialized)
		return;

	// Libera o que o usuario esqueceu de destruir
	g.framebuffers.forEachUsed([](FramebufferRes& r)
							   { destroyFramebuffer(r); LOG_WARNING("Glx: framebuffer vazado"); });
	g.vaos.forEachUsed([](VertexArrayRes& r)
					   { destroyVertexArray(r); LOG_WARNING("Glx: vertex array vazado"); });
	g.shaders.forEachUsed([](ShaderRes& r)
						  { destroyShader(r); LOG_WARNING("Glx: shader vazado"); });
	g.textures.forEachUsed([](TextureRes& r)
						   { destroyTexture(r); LOG_WARNING("Glx: texture vazada"); });
	g.buffers.forEachUsed([](BufferRes& r)
						  { destroyBuffer(r); LOG_WARNING("Glx: buffer vazado"); });

	releaseDeviceObjects();

	g = State{};
	LOG_INFO("Glx destroyed (D3D11)");
}

void present()
{
	if (!g.swap || g.deviceLost)
		return;

	const UINT sync = g.vsync ? 1 : 0;
	const UINT flags = (!g.vsync && g.tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

	HRESULT hr = g.swap->Present(sync, flags);

	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		LOG_ERROR("GlxD3D11: device lost (hr=0x%08X, reason=0x%08X)", HR_U(hr), HR_U(g.device->GetDeviceRemovedReason()));
		g.deviceLost = true;
	}
	else if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: Present failed (hr=0x%08X)", HR_U(hr));
	}
	// DXGI_STATUS_OCCLUDED e sucesso (janela coberta/minimizada)
}

void resize(u32 width, u32 height)
{
	if (!g.swap || width == 0 || height == 0)
		return;

	g.context->OMSetRenderTargets(0, nullptr, nullptr);
	releaseBackbufferViews();
	g.curRtv = nullptr;
	g.curDsv = nullptr;
	g.context->Flush();

	g.width = width;
	g.height = height;

	// BufferCount 0 / FORMAT_UNKNOWN = manter; flags precisam ser iguais as da criacao
	HRESULT hr = g.swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, g.swapFlags);
	if (FAILED(hr))
	{
		LOG_ERROR("GlxD3D11: ResizeBuffers failed (hr=0x%08X)", HR_U(hr));
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
			g.deviceLost = true;
		return;
	}

	if (!createBackbufferViews())
		return;

	if (!g.curFb.valid())
		useBackbuffer(); // renova ponteiros e viewport
	else
		bindTargets();
}

void setVsync(bool enabled)
{
	g.vsync = enabled;
}

GlxError getError()
{
	GlxError e = g.lastError;
	g.lastError = GlxError::None;
	if (e == GlxError::None && g.deviceLost)
		e = GlxError::ContextLost;
	return e;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Frame / estado

void clear(f32 r, f32 gr, f32 b, f32 a, GlxClearMask mask)
{
	if (!g.initialized)
		return;

	bindTargets();

	if (glxHas(mask, GlxClearMask::Color) && g.curRtv)
	{
		const f32 color[4] = {r, gr, b, a};
		g.context->ClearRenderTargetView(g.curRtv, color);
	}

	UINT flags = 0;
	if (glxHas(mask, GlxClearMask::Depth))
		flags |= D3D11_CLEAR_DEPTH;
	if (glxHas(mask, GlxClearMask::Stencil))
		flags |= D3D11_CLEAR_STENCIL;
	if (flags && g.curDsv)
		g.context->ClearDepthStencilView(g.curDsv, flags, 1.0f, 0);
}

// Atencao: em D3D11 a origem do viewport e o canto SUPERIOR esquerdo (no GL e o inferior).
void setViewport(u32 x, u32 y, u32 width, u32 height)
{
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = (FLOAT) x;
	vp.TopLeftY = (FLOAT) y;
	vp.Width = (FLOAT) width;
	vp.Height = (FLOAT) height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	g.context->RSSetViewports(1, &vp);
}

void setDepthTest(bool enabled)
{
	g.depthTest = enabled;
}

void setBlend(bool enabled)
{
	g.blend = enabled;
}

void setCull(GlxCullMode mode)
{
	g.cull = mode;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Buffers
//
// Usage (Static/Dynamic/Stream) e ignorado por enquanto: todo buffer e DEFAULT
// e atualizado com UpdateSubresource. Uniform buffers so aceitam update completo.

GlxBuffer bufferCreate(GlxBufferType type, GlxBufferUsage usage, const void* data, u32 size)
{
	(void) usage;

	if (size == 0)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	UINT bind = D3D11_BIND_VERTEX_BUFFER;
	if (type == GlxBufferType::Index)
		bind = D3D11_BIND_INDEX_BUFFER;
	else if (type == GlxBufferType::Uniform)
		bind = D3D11_BIND_CONSTANT_BUFFER;

	// Constant buffers precisam de tamanho multiplo de 16
	const u32 gpuSize = (type == GlxBufferType::Uniform) ? ((size + 15) & ~15u) : size;

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = gpuSize;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = bind;

	BufferRes r;
	r.type = type;
	r.size = size;

	HRESULT hr;
	if (data)
	{
		std::vector<uint8_t> padded;
		const void* src = data;
		if (gpuSize != size)
		{
			padded.assign(gpuSize, 0);
			memcpy(padded.data(), data, size);
			src = padded.data();
		}

		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = src;
		hr = g.device->CreateBuffer(&bd, &sd, &r.buffer);
	}
	else
	{
		hr = g.device->CreateBuffer(&bd, nullptr, &r.buffer);
	}

	if (FAILED(hr))
	{
		LOG_ERROR("Glx: CreateBuffer failed (hr=0x%08X)", HR_U(hr));
		setError(GlxError::OutOfMemory);
		return {};
	}

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

	if (r->type == GlxBufferType::Uniform)
	{
		if (offset != 0 || size != r->size)
		{
			setError(GlxError::InvalidOperation); // D3D11 nao permite update parcial de constant buffer
			return;
		}

		std::vector<uint8_t> padded((r->size + 15) & ~15u, 0);
		memcpy(padded.data(), data, size);
		g.context->UpdateSubresource(r->buffer, 0, nullptr, padded.data(), 0, 0);
		return;
	}

	D3D11_BOX box = {};
	box.left = offset;
	box.right = offset + size;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	g.context->UpdateSubresource(r->buffer, 0, &box, data, 0, 0);
}

void bufferDestroy(GlxBuffer buffer)
{
	BufferRes* r = find(g.buffers, buffer);
	if (!r)
		return;
	destroyBuffer(*r);
	g.buffers.free(buffer.index, buffer.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Texturas

static bool rebuildSampler(TextureRes& t)
{
	release(t.sampler);

	D3D11_SAMPLER_DESC sd = {};
	sd.Filter = filterFor(t.minFilter, t.magFilter);
	sd.AddressU = addressFor(t.wrapS);
	sd.AddressV = addressFor(t.wrapT);
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MaxAnisotropy = 1;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sd.MinLOD = 0;
	sd.MaxLOD = D3D11_FLOAT32_MAX;

	return SUCCEEDED(g.device->CreateSamplerState(&sd, &t.sampler));
}

GlxTexture textureCreate(u32 width, u32 height, GlxPixelFormat format, const void* data)
{
	if (width == 0 || height == 0)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	DXGI_FORMAT dxgi = DXGI_FORMAT_R8G8B8A8_UNORM;
	u32 bytesPerPixel = 4;
	if (format == GlxPixelFormat::R8)
	{
		dxgi = DXGI_FORMAT_R8_UNORM;
		bytesPerPixel = 1;
	}

	// D3D11 nao tem RGB8: expande para RGBA8 (alpha = 255)
	std::vector<uint8_t> expanded;
	const void* src = data;
	if (format == GlxPixelFormat::RGB8 && data)
	{
		const uint8_t* rgb = (const uint8_t*) data;
		expanded.resize((size_t) width * height * 4);
		for (size_t i = 0, n = (size_t) width * height; i < n; i++)
		{
			expanded[i * 4 + 0] = rgb[i * 3 + 0];
			expanded[i * 4 + 1] = rgb[i * 3 + 1];
			expanded[i * 4 + 2] = rgb[i * 3 + 2];
			expanded[i * 4 + 3] = 255;
		}
		src = expanded.data();
	}

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = dxgi;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // RT para poder virar framebuffer

	TextureRes r;
	r.width = width;
	r.height = height;

	HRESULT hr;
	if (src)
	{
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = src;
		sd.SysMemPitch = width * bytesPerPixel;
		hr = g.device->CreateTexture2D(&td, &sd, &r.tex);
	}
	else
	{
		hr = g.device->CreateTexture2D(&td, nullptr, &r.tex);
	}

	if (SUCCEEDED(hr))
		hr = g.device->CreateShaderResourceView(r.tex, nullptr, &r.srv);

	if (FAILED(hr) || !rebuildSampler(r))
	{
		LOG_ERROR("Glx: texture creation failed (hr=0x%08X)", HR_U(hr));
		destroyTexture(r);
		setError(GlxError::OutOfMemory);
		return {};
	}

	return add<GlxHandleType::Texture>(g.textures, r);
}

void textureSetFilter(GlxTexture texture, GlxTextureFilter minFilter, GlxTextureFilter magFilter)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;
	t->minFilter = minFilter;
	t->magFilter = magFilter;
	rebuildSampler(*t);
}

void textureSetWrap(GlxTexture texture, GlxTextureWrap wrapS, GlxTextureWrap wrapT)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;
	t->wrapS = wrapS;
	t->wrapT = wrapT;
	rebuildSampler(*t);
}

void textureBind(GlxTexture texture, u32 slot)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;

	g.context->PSSetShaderResources(slot, 1, &t->srv);
	g.context->PSSetSamplers(slot, 1, &t->sampler);
}

void textureDestroy(GlxTexture texture)
{
	TextureRes* t = find(g.textures, texture);
	if (!t)
		return;
	destroyTexture(*t);
	g.textures.free(texture.index, texture.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shaders (fontes em HLSL: VSMain / PSMain)

GlxShader shaderCreate(const char* vertexSource, const char* fragmentSource)
{
	if (!vertexSource || !fragmentSource)
	{
		setError(GlxError::InvalidValue);
		return {};
	}

	ShaderRes r;

	r.vsBlob = compileHlsl(vertexSource, "VSMain", "vs_5_0");
	if (!r.vsBlob)
	{
		setError(GlxError::ShaderCompile);
		return {};
	}

	ID3DBlob* psBlob = compileHlsl(fragmentSource, "PSMain", "ps_5_0");
	if (!psBlob)
	{
		destroyShader(r);
		setError(GlxError::ShaderCompile);
		return {};
	}

	HRESULT hr = g.device->CreateVertexShader(r.vsBlob->GetBufferPointer(), r.vsBlob->GetBufferSize(), nullptr, &r.vs);
	if (SUCCEEDED(hr))
		hr = g.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &r.ps);

	if (SUCCEEDED(hr))
	{
		reflectShader(r.vsBlob, r);
		reflectShader(psBlob, r);
		if (!createConstantBuffers(r))
			hr = E_FAIL;
	}
	release(psBlob);

	if (FAILED(hr))
	{
		LOG_ERROR("Glx: shader creation failed (hr=0x%08X)", HR_U(hr));
		destroyShader(r);
		setError(GlxError::ShaderLink);
		return {};
	}

	return add<GlxHandleType::Shader>(g.shaders, r);
}

void shaderDestroy(GlxShader shader)
{
	ShaderRes* s = find(g.shaders, shader);
	if (!s)
		return;

	if (g.curShader.index == shader.index && g.curShader.generation == shader.generation)
		g.curShader = GlxShader{};

	destroyShader(*s);
	g.shaders.free(shader.index, shader.generation);
}

void shaderUse(GlxShader shader)
{
	if (find(g.shaders, shader))
		g.curShader = shader; // os estagios sao ligados no draw
}

void shaderSetInt(GlxShader shader, const char* name, int value)
{
	setUniform(shader, name, &value, sizeof(value));
}

void shaderSetFloat(GlxShader shader, const char* name, f32 value)
{
	setUniform(shader, name, &value, sizeof(value));
}

void shaderSetVec2(GlxShader shader, const char* name, f32 x, f32 y)
{
	const f32 v[2] = {x, y};
	setUniform(shader, name, v, sizeof(v));
}

void shaderSetVec3(GlxShader shader, const char* name, f32 x, f32 y, f32 z)
{
	const f32 v[3] = {x, y, z};
	setUniform(shader, name, v, sizeof(v));
}

void shaderSetVec4(GlxShader shader, const char* name, f32 x, f32 y, f32 z, f32 w)
{
	const f32 v[4] = {x, y, z, w};
	setUniform(shader, name, v, sizeof(v));
}

// column-major, igual ao GL (HLSL usa column_major por padrao em float4x4)
void shaderSetMat4(GlxShader shader, const char* name, const f32* columnMajor16)
{
	if (!columnMajor16)
	{
		setError(GlxError::InvalidValue);
		return;
	}
	setUniform(shader, name, columnMajor16, 16 * sizeof(f32));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

	for (u32 i = 0; i < layout.count; i++)
	{
		const GlxVertexAttrib& a = layout.attribs[i];
		if (vertexFormat(a.type, a.components, a.normalized) == DXGI_FORMAT_UNKNOWN)
		{
			setError(GlxError::Unsupported); // ex.: 3 componentes de byte/short
			return {};
		}
	}

	BufferRes* ibo = nullptr;
	if (indexBuffer.valid())
	{
		ibo = find(g.buffers, indexBuffer);
		if (!ibo)
			return {};
	}

	VertexArrayRes r;
	r.layout = layout;
	r.vb = vbo->buffer;
	r.vb->AddRef();
	if (ibo)
	{
		r.ib = ibo->buffer;
		r.ib->AddRef();
		r.hasIndex = true;
	}

	return add<GlxHandleType::VertexArray>(g.vaos, r);
}

void vertexArrayDestroy(GlxVertexArray vertexArray)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v)
		return;
	destroyVertexArray(*v);
	g.vaos.free(vertexArray.index, vertexArray.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Framebuffers

GlxFramebuffer framebufferCreate(GlxTexture color, bool withDepthStencil)
{
	TextureRes* t = find(g.textures, color);
	if (!t)
		return {};

	FramebufferRes r;
	r.width = t->width;
	r.height = t->height;

	HRESULT hr = g.device->CreateRenderTargetView(t->tex, nullptr, &r.rtv);

	if (SUCCEEDED(hr) && withDepthStencil)
	{
		D3D11_TEXTURE2D_DESC td = {};
		td.Width = r.width;
		td.Height = r.height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = g.device->CreateTexture2D(&td, nullptr, &r.depthTex);
		if (SUCCEEDED(hr))
			hr = g.device->CreateDepthStencilView(r.depthTex, nullptr, &r.dsv);
	}

	if (FAILED(hr))
	{
		LOG_ERROR("Glx: framebuffer creation failed (hr=0x%08X)", HR_U(hr));
		destroyFramebuffer(r);
		setError(GlxError::InvalidFramebufferOperation);
		return {};
	}

	return add<GlxHandleType::Framebuffer>(g.framebuffers, r);
}

void framebufferBind(GlxFramebuffer framebuffer)
{
	if (!framebuffer.valid())
	{
		useBackbuffer();
		bindTargets();
		return;
	}

	FramebufferRes* f = find(g.framebuffers, framebuffer);
	if (!f)
		return;

	g.curFb = framebuffer;
	g.curRtv = f->rtv;
	g.curDsv = f->dsv;
	bindTargets();
	setFullViewport(f->width, f->height);
}

void framebufferDestroy(GlxFramebuffer framebuffer)
{
	FramebufferRes* f = find(g.framebuffers, framebuffer);
	if (!f)
		return;

	if (g.curFb.index == framebuffer.index && g.curFb.generation == framebuffer.generation)
	{
		useBackbuffer();
		bindTargets();
	}

	destroyFramebuffer(*f);
	g.framebuffers.free(framebuffer.index, framebuffer.generation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw

void draw(GlxVertexArray vertexArray, GlxDrawMode mode, u32 first, u32 count)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v || !prepareDraw(*v, mode))
		return;

	g.context->Draw(count, first);
}

void drawIndexed(GlxVertexArray vertexArray, GlxDrawMode mode, u32 count, GlxDataType indexType, u32 offsetBytes)
{
	VertexArrayRes* v = find(g.vaos, vertexArray);
	if (!v)
		return;

	DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
	u32 indexSize = 0;
	if (indexType == GlxDataType::UnsignedShort)
	{
		fmt = DXGI_FORMAT_R16_UINT;
		indexSize = 2;
	}
	else if (indexType == GlxDataType::UnsignedInt)
	{
		fmt = DXGI_FORMAT_R32_UINT;
		indexSize = 4;
	}

	if (!v->hasIndex || fmt == DXGI_FORMAT_UNKNOWN) // UnsignedByte nao existe em D3D11
	{
		setError(GlxError::InvalidOperation);
		return;
	}

	if (!prepareDraw(*v, mode))
		return;

	g.context->IASetIndexBuffer(v->ib, fmt, 0);
	g.context->DrawIndexed(count, offsetBytes / indexSize, 0);
}
} // namespace Glx

#endif // PIPELINE_OS_WINDOWS && GLX_BACKEND_D3D11