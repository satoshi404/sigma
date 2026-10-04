#include <Core/Platform/Window.hpp>

namespace WindowAtrib
{
const char* title = "NoTitle";
u32 width = 800;
u32 height = 600;
u32 x = 0;
u32 y = 0;
bool is_running = true;
} // namespace WindowAtrib

bool WindowAtrib::running()
{
	return WindowAtrib::is_running;
}

u32 WindowAtrib::get_width()
{
	return WindowAtrib::width;
}

u32 WindowAtrib::get_height()
{
	return WindowAtrib::height;
}

u32 WindowAtrib::get_posx()
{
	return WindowAtrib::x;
}

u32 WindowAtrib::get_posy()
{
	return WindowAtrib::y;
}

const char* WindowAtrib::get_title()
{
	return WindowAtrib::title;
}

void WindowAtrib::set_title(const char* new_title)
{
	WindowAtrib::title = new_title;
	WindowAtrib::update();
}

void WindowAtrib::set_size(const u32 new_width, const u32 new_height)
{
	WindowAtrib::width = new_width;
	WindowAtrib::height = new_height;
	WindowAtrib::update();
}

void WindowAtrib::set_pos(const u32 new_x, const u32 new_y)
{
	WindowAtrib::x = new_x;
	WindowAtrib::y = new_y;
	WindowAtrib::update();
}
