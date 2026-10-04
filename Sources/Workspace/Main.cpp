#include <Core/Containers/Array.hpp>
#include <Core/Logger.hpp>
#include <Core/String.hpp>

#include <Core/Platform/Window.hpp>
#include <Renderer/Glx.hpp>

#include <Renderer/GlxConfig.hpp>
#include <Core/DetectionPipeline.hpp>

#if PIPELINE_OS_WINDOWS && GLX_BACKEND_OPENGL
static const char* vertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
out vec2 vPos;
void main()
{
	vPos = aPos;
    vColor = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* fragmentSrc = R"(#version 330 core
in vec3 vColor;
in vec2 vPos;
out vec4 outColor;
void main()
{
	vec3 col = vec3(0);
	float dist = length(vPos) - 0.02;

	col += vec3(1, 0, 0) * step( dist, 0. );

    outColor = vec4(mix(col, vColor, 1.0), 1.0);
}
)";
#elif PIPELINE_OS_WINDOWS && GLX_BACKEND_D3D11
static const char* vertexSrc = R"(struct VSInput
{
	float2 pos : ATTR0;
	float3 color : ATTR1;
};
struct VSOutput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};
VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.pos = float4(input.pos, 0.0, 1.0);
	output.color = input.color;
	return output;
}
)";

static const char* fragmentSrc = R"(struct PSInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4(input.color, 1.0);
}
)";

#endif

static const f32 vertices[] = {
	-0.5f,
	-0.5f,
	1.0f,
	0.0f,
	0.0f,
	0.5f,
	-0.5f,
	0.0f,
	1.0f,
	0.0f,
	0.5f,
	0.5f,
	0.0f,
	0.0f,
	1.0f,
	-0.5f,
	0.5f,
	1.0f,
	1.0f,
	0.0f,
};

static const unsigned short indices[] = {0, 1, 2, 2, 3, 0};

int main()
{
#if 0
  LOG_INFO("Ola Mundo");

  {
	Array<int> list;
	list.add(3);
	list.add(5);
	list.add(3);

	for (const int item : list) {
	  LOG_WARNING("Item is: %i", item);
	}
  }

  String msg;

  if (msg.isEmpty())
	LOG_FATAL("String empty");

  if (!msg.isEmpty()) LOG_DEBUG(msg.cStr());

  LOG_FATAL("Test faltal ( Shutup )");
#endif

	Window::init();

	Window::show();
	WindowAtrib::set_title("Sigmsa Engine");

	Glx::init();

	GlxShader shader = Glx::shaderCreate(vertexSrc, fragmentSrc);

	GlxBuffer vbo = Glx::bufferCreate(GlxBufferType::Vertex, GlxBufferUsage::Static, vertices, sizeof(vertices));
	GlxBuffer ibo = Glx::bufferCreate(GlxBufferType::Index, GlxBufferUsage::Static, indices, sizeof(indices));

	GlxVertexLayout layout;
	layout.stride = 5 * sizeof(f32);
	layout.count = 2;
	layout.attribs[0] = {0, 2, GlxDataType::Float, false, 0};				// position
	layout.attribs[1] = {1, 3, GlxDataType::Float, false, 2 * sizeof(f32)}; // color

	GlxVertexArray vao = Glx::vertexArrayCreate(layout, vbo, ibo);

	while (WindowAtrib::running())
	{
		Window::poll();

		Glx::clear(0.4f, 0.1f, 0.15f, 1.0f);
		Glx::shaderUse(shader);
		Glx::drawIndexed(vao, GlxDrawMode::Triangles, 6, GlxDataType::UnsignedShort);
		Glx::present();
	}

	Window::destroy();
	Glx::vertexArrayDestroy(vao);
	Glx::bufferDestroy(ibo);
	Glx::bufferDestroy(vbo);
	Glx::shaderDestroy(shader);
	Glx::destroy();

	return 0;
}