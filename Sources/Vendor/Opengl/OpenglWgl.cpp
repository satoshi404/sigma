#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/Platform/Window.hpp>
#include <Core/Types.hpp>
#include <Vendor/Opengl/Opengl.hpp>
#include <windef.h>

#if PIPELINE_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// WGL_ARB_pixel_format / WGL_ARB_create_context
#define WGL_DRAW_TO_WINDOW_ARB			 0x2001
#define WGL_ACCELERATION_ARB			 0x2003
#define WGL_SUPPORT_OPENGL_ARB			 0x2010
#define WGL_DOUBLE_BUFFER_ARB			 0x2011
#define WGL_PIXEL_TYPE_ARB				 0x2013
#define WGL_COLOR_BITS_ARB				 0x2014
#define WGL_DEPTH_BITS_ARB				 0x2022
#define WGL_STENCIL_BITS_ARB			 0x2023
#define WGL_FULL_ACCELERATION_ARB		 0x2027
#define WGL_TYPE_RGBA_ARB				 0x202B
#define WGL_CONTEXT_MAJOR_VERSION_ARB	 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB	 0x2092
#define WGL_CONTEXT_FLAGS_ARB			 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB	 0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB		 0x0001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

using PFN_wglChoosePixelFormatARB = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
using PFN_wglCreateContextAttribsARB = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using PFN_wglSwapIntervalEXT = BOOL(WINAPI*)(int);

namespace Window
{
extern HWND hwnd;
}

namespace
{
HMODULE glModule = nullptr;
HDC hdc = nullptr;
HGLRC hglrc = nullptr;

PFN_wglChoosePixelFormatARB wglChoosePixelFormatARB_ = nullptr;
PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB_ = nullptr;
PFN_wglSwapIntervalEXT wglSwapIntervalEXT_ = nullptr;

// Janela temporaria so para carregar as extensoes WGL
bool loadWglExtensions()
{
	const char* className = "PipelineGlDummy";

	WNDCLASSA wc = {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(nullptr);
	wc.lpszClassName = className;
	RegisterClassA(&wc); // se ja registrada, falha sem problema

	HWND dummy = CreateWindowExA(0, className, "", WS_OVERLAPPEDWINDOW,
								 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
	if (!dummy)
		return false;

	HDC dc = GetDC(dummy);
	HGLRC rc = nullptr;
	bool ok = false;

	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(dc, &pfd);
	if (pf && SetPixelFormat(dc, pf, &pfd))
	{
		rc = wglCreateContext(dc);
		if (rc && wglMakeCurrent(dc, rc))
		{
			wglChoosePixelFormatARB_ = (PFN_wglChoosePixelFormatARB) wglGetProcAddress("wglChoosePixelFormatARB");
			wglCreateContextAttribsARB_ = (PFN_wglCreateContextAttribsARB) wglGetProcAddress("wglCreateContextAttribsARB");
			wglSwapIntervalEXT_ = (PFN_wglSwapIntervalEXT) wglGetProcAddress("wglSwapIntervalEXT");
			ok = wglChoosePixelFormatARB_ && wglCreateContextAttribsARB_;
		}
	}

	wglMakeCurrent(nullptr, nullptr);
	if (rc)
		wglDeleteContext(rc);
	ReleaseDC(dummy, dc);
	DestroyWindow(dummy);
	UnregisterClassA(className, wc.hInstance);
	return ok;
}

void cleanup()
{
	if (hglrc)
	{
		if (wglGetCurrentContext() == hglrc)
			wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(hglrc);
		hglrc = nullptr;
	}
	if (hdc)
	{
		ReleaseDC(Window::hwnd, hdc);
		hdc = nullptr;
	}
	Window::hwnd = nullptr;
}
} // namespace

bool opengl_init()
{
	if (hglrc)
		return true;

	if (!Window::hwnd)
	{
		LOG_ERROR("opengl_init: window not created (HWND null)");
		return false;
	}

	hdc = GetDC(Window::hwnd);
	if (!hdc)
	{
		LOG_ERROR("opengl_init: GetDC failed");
		cleanup();
		return false;
	}

	const bool hasArb = loadWglExtensions();

	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	// Pixel format (ARB se disponivel, senao o legado)
	int pixelFormat = 0;
	if (hasArb)
	{
		const int attribs[] = {
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB, 32,
			WGL_DEPTH_BITS_ARB, 24,
			WGL_STENCIL_BITS_ARB, 8,
			0};
		UINT count = 0;
		if (!wglChoosePixelFormatARB_(hdc, attribs, nullptr, 1, &pixelFormat, &count) || count == 0)
			pixelFormat = 0;
	}
	if (!pixelFormat)
		pixelFormat = ChoosePixelFormat(hdc, &pfd);

	if (!pixelFormat)
	{
		LOG_ERROR("opengl_init: no suitable pixel format");
		cleanup();
		return false;
	}

	// SetPixelFormat so pode ser chamado UMA vez por janela
	if (!DescribePixelFormat(hdc, pixelFormat, sizeof(pfd), &pfd) || !SetPixelFormat(hdc, pixelFormat, &pfd))
	{
		LOG_ERROR("opengl_init: SetPixelFormat failed");
		cleanup();
		return false;
	}

	// Contexto: tenta core 4.6 -> 3.3, senao cai no legado
	if (hasArb)
	{
		static const int versions[][2] = {{4, 6}, {4, 5}, {4, 3}, {4, 1}, {3, 3}};
		for (const auto& v : versions)
		{
			int flags = 0;
#if defined(PIPELINE_DEBUG)
			flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
#endif
			const int attribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, v[0],
				WGL_CONTEXT_MINOR_VERSION_ARB, v[1],
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				WGL_CONTEXT_FLAGS_ARB, flags,
				0};
			hglrc = wglCreateContextAttribsARB_(hdc, nullptr, attribs);
			if (hglrc)
				break;
		}
	}
	if (!hglrc)
	{
		LOG_WARNING("opengl_init: core context unavailable, using legacy context");
		hglrc = wglCreateContext(hdc);
	}
	if (!hglrc)
	{
		LOG_ERROR("opengl_init: context creation failed");
		cleanup();
		return false;
	}

	if (!wglMakeCurrent(hdc, hglrc))
	{
		LOG_ERROR("opengl_init: wglMakeCurrent failed");
		cleanup();
		return false;
	}

	LOG_INFO("OpenGL context created (WGL)");
	LOG_INFO("  Vendor  : %s", (const char*) glGetString(GL_VENDOR));
	LOG_INFO("  Renderer: %s", (const char*) glGetString(GL_RENDERER));
	LOG_INFO("  Version : %s", (const char*) glGetString(GL_VERSION));
	return true;
}

bool opengl_free()
{
	cleanup();

	if (glModule)
	{
		FreeLibrary(glModule); // balanceia o LoadLibrary do opengl_proc
		glModule = nullptr;
	}
	wglSwapIntervalEXT_ = nullptr;
	return true;
}

bool opengl_swap()
{
	if (!hdc)
	{
		LOG_ERROR("opengl_swap: HDC is null");
		return false;
	}
	return SwapBuffers(hdc) != 0;
}

void opengl_set_vsync(bool enabled)
{
	if (wglSwapIntervalEXT_)
		wglSwapIntervalEXT_(enabled ? 1 : 0);
}

bool opengl_load()
{
	// TODO: carregar os ponteiros GL 1.2+ via opengl_proc (glGenBuffers, glCreateShader, ...)
	return true;
}

void opengl_update()
{
}

void* opengl_proc(const char* name)
{
	void* proc = (void*) wglGetProcAddress(name);
	if (proc && proc != (void*) 0x1 && proc != (void*) 0x2 && proc != (void*) 0x3 && proc != (void*) -1)
		return proc;

	if (!glModule)
	{
		glModule = LoadLibraryA("opengl32.dll");
		if (!glModule)
		{
			LOG_FATAL("Failed to load opengl32.dll");
			return nullptr;
		}
	}
	return (void*) GetProcAddress(glModule, name);
}

#endif // PIPELINE_OS_WINDOWS