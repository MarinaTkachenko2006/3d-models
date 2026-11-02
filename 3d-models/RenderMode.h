#pragma once

namespace Render {
    enum class Mode {
        Solid,        // заливка с освещением
        Wireframe,    // каркас (рёбра)
        Points        // точки вершин
    };
}