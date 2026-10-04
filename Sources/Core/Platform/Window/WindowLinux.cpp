#include <Core/DetectionPipeline.hpp>
#include <Core/Logger.hpp>
#include <Core/Platform/Window.hpp>

#if defined(PIPELINE_OS_LINUX) && PIPELINE_OS_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WindowAtrib

namespace WindowAtrib
{
const char* title = "Sigma";
u32 x = 100;
u32 y = 100;
u32 width = 1280;
u32 height = 720;

void update()
{
}
} // namespace WindowAtrib

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window Internal

namespace Window
{
namespace Internal
{
Display* display = nullptr;
Window xwindow = 0;
Atom wmDelete = 0;
bool running = false;
} // namespace Internal

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window API

bool init()
{
	Internal::display = XOpenDisplay(nullptr);
	if (!Internal::display)
	{
		LOG_ERROR("Failed to open X11 display");
		return false;
	}

	int screen = DefaultScreen(Internal::display);

	Internal::xwindow = XCreateSimpleWindow(
		Internal::display,
		RootWindow(Internal::display, screen),
		static_cast<int>(WindowAtrib::x),
		static_cast<int>(WindowAtrib::y),
		WindowAtrib::width,
		WindowAtrib::height,
		1,
		BlackPixel(Internal::display, screen),
		WhitePixel(Internal::display, screen));

	XStoreName(Internal::display, Internal::xwindow, WindowAtrib::title);

	Internal::wmDelete = XInternAtom(Internal::display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(Internal::display, Internal::xwindow, &Internal::wmDelete, 1);

	XSelectInput(Internal::display, Internal::xwindow,
				 ExposureMask | KeyPressMask | StructureNotifyMask);

	Internal::running = true;

	LOG_INFO("Window initialized (X11) %ux%u", WindowAtrib::width, WindowAtrib::height);
	return true;
}

void show()
{
	if (Internal::display && Internal::xwindow)
	{
		XMapWindow(Internal::display, Internal::xwindow);
		XFlush(Internal::display);
	}
}

void poll()
{
	if (!Internal::display)
		return;

	while (XPending(Internal::display))
	{
		XEvent event;
		XNextEvent(Internal::display, &event);

		switch (event.type)
		{
		case ClientMessage:
			if (static_cast<Atom>(event.xclient.data.l[0]) == Internal::wmDelete)
			{
				Internal::running = false;
			}
			break;

		case ConfigureNotify:
			WindowAtrib::width = static_cast<u32>(event.xconfigure.width);
			WindowAtrib::height = static_cast<u32>(event.xconfigure.height);
			break;
		}
	}
}

bool isRunning()
{
	return Internal::running;
}

void destroy()
{
	if (Internal::display)
	{
		if (Internal::xwindow)
		{
			XDestroyWindow(Internal::display, Internal::xwindow);
			Internal::xwindow = 0;
		}

		XCloseDisplay(Internal::display);
		Internal::display = nullptr;
	}

	Internal::running = false;
	LOG_INFO("Window destroyed");
}
} // namespace Window

#endif // PIPELINE_OS_LINUX