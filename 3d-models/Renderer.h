#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include "RenderMode.h"
#include "imgui.h"
#include <vector>

struct SDL_Renderer;

namespace Render {

    // Отрисовать Polyhedron через ImDrawList (Wireframe / Points).
    void Render(const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix,
        Mode mode,
        ImDrawList* drawList,
        ImVec2 origin,
        ImVec2 size,
        SDL_Renderer* renderer); 
}