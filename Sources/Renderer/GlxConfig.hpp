#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Glx Backend
//
// Escolha UM backend. O builder compila todos os .cpp de Sources/, entao cada
// backend se desliga sozinho quando nao e o escolhido (evita simbolos duplicados).

#ifndef GLX_BACKEND_OPENGL
#define GLX_BACKEND_OPENGL 1
#endif

#ifndef GLX_BACKEND_D3D11
#define GLX_BACKEND_D3D11 0
#endif