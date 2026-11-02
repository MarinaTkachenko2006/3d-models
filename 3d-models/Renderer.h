#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include "PixelBuffer.h"
#include "RenderMode.h"
#include <vector>

namespace Render {

    struct ScreenVertex {
        double cx = 0, cy = 0, cz = 0, cw = 1; // clip space
        double wx = 0, wy = 0, wz = 0;         // world (для нормалей)
    };

    // Отрисовать Polyhedron в PixelBuffer
    void Render(PixelBuffer& buf,
        const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix,
        Mode mode);
}