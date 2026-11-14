#pragma once
#include <cstdint>

namespace Render {
    enum class Mode {
        Solid,        // заливка с освещением
        Wireframe,    // каркас (рёбра)
        Points        // точки вершин
    };

    struct Options {
        bool cullBackFaces = true;   // отсечение нелицевых граней
        bool showNormals = false;    // рисовать нормали граней

        // Текстура для Solid. Если pixels == nullptr — плоский цвет.
        const std::uint32_t* texPixels = nullptr; // RGBA (0xAARRGGBB), w*h
        int texWidth = 0;
        int texHeight = 0;
        bool texWrap = true;         // repeat vs clamp
        bool texBilinear = true;     // bilinear vs nearest
    };
}