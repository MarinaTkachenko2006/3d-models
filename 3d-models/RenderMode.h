#pragma once

namespace Render {
    enum class Mode {
        Solid,        // заливка с освещением
        Wireframe,    // каркас (рёбра)
        Points        // точки вершин
    };

    struct Options {
        bool cullBackFaces = true;   // отсечение нелицевых граней
        bool showNormals = false;    // рисовать нормали граней
    };
}