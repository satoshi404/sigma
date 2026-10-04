#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/Platform/Window.hpp>

#if PIPELINE_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace WindowAtrib
{
extern bool is_running;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window Internal

namespace Window
{
HWND hwnd = nullptr;
HDC hdc = nullptr;
} // namespace Window

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
	case WM_DESTROY:
		WindowAtrib::is_running = false;
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		WindowAtrib::set_size(HIWORD(lParam), HIWORD(lParam));
		return 0;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
} // namespace Internal

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window API

bool Window::init()
{
	HINSTANCE hInstance = GetModuleHandleA(nullptr);

	WNDCLASSEXA wc = {};
	wc.cbSize = sizeof(WNDCLASSEXA);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = "SigmaWindowClass";

	if (!RegisterClassExA(&wc))
	{
		LOG_ERROR("Failed to register window class");
		return false;
	}

	DWORD style = WS_OVERLAPPEDWINDOW;

	RECT rect = {
		0, 0,
		static_cast<LONG>(WindowAtrib::get_width()),
		static_cast<LONG>(WindowAtrib::get_height())};
	AdjustWindowRect(&rect, style, FALSE);

	hwnd = CreateWindowExA(
		0,
		"SigmaWindowClass",
		"NoTitle",
		style,
		static_cast<int>(WindowAtrib::get_posx()),
		static_cast<int>(WindowAtrib::get_posy()),
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (!hwnd)
	{
		LOG_ERROR("Failed to create window");
		return false;
	}

	hdc = GetDC(hwnd);

	LOG_INFO("Window initialized (Win32) %ux%u", WindowAtrib::get_width(), WindowAtrib::get_height());
	return true;
}

void Window::show()
{
	if (hwnd)
	{
		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
	}
}

void Window::poll()
{
	MSG msg;
	while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

void Window::destroy()
{
	if (hdc && hwnd)
	{
		ReleaseDC(hwnd, hdc);
		hdc = nullptr;
	}

	if (hwnd)
	{
		DestroyWindow(hwnd);
		hwnd = nullptr;
	}

	UnregisterClassA("SigmaWindowClass", GetModuleHandleA(nullptr));

	LOG_INFO("Window destroyed");

} // namespace Window

void WindowAtrib::update()
{
	// Title
	{
		wchar_t buffer[256];
		MultiByteToWideChar(CP_UTF8, 0, WindowAtrib::get_title(), -1, buffer, sizeof(buffer) / sizeof(wchar_t));
		SetWindowTextW(Window::hwnd, buffer);
	}

	// Position and Size
	SetWindowPos(Window::hwnd, nullptr, WindowAtrib::get_posx(), WindowAtrib::get_posy(), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

#endif // PIPELINE_OS_WINDOWS