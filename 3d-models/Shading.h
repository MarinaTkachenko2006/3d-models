#pragma once
#include "RenderPipeline.h"
#include "Rasterizer.h"

namespace Render {

    // один цвет на треугольник.
    // Позже заменяется на освещение
    inline std::uint32_t shadeFlat(const ScreenTri&,
        double, double, double,
        double, double, double)
    {
        return 0xFFC8C8C8;
    }

}