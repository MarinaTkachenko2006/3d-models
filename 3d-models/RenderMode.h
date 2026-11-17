#pragma once
#include "Polyhedron.h"
#include <cstdint>

namespace Render {
    enum class Mode {
        Solid,        // заливка с освещением
        Wireframe,    // каркас (рёбра)
        Points        // точки вершин
    };

    // Модель затенения граней
    enum class ShadingModel {
        Flat,       // плоское: цвет по нормали грани (как было)
        Gouraud     // Ламберт в вершинах + интерполяция цвета (Гуро)
    };

    // Источник света (направленный) + параметры модели Ламберта
    struct Light {
        Vec3  direction{ 0.3f, -0.8f, -0.5f }; // направление К источнику света
        Vec3  color{ 1.f, 1.f, 1.f };          // цвет источника
        float ambient = 0.15f;                  // фоновая составляющая
        float diffuse = 0.85f;                  // диффузная составляющая
    };

    struct Options {
        bool cullBackFaces = true;   // отсечение нелицевых граней
        bool showNormals = false;    // рисовать нормали граней (по центру грани)

        // Текстура для Solid. Если pixels == nullptr — плоский цвет.
        const std::uint32_t* texPixels = nullptr; // RGBA (0xAARRGGBB), w*h
        int texWidth = 0;
        int texHeight = 0;
        bool texWrap = true;         // repeat vs clamp
        bool texBilinear = true;     // bilinear vs nearest

        // ---- Освещение ----
        ShadingModel shading = ShadingModel::Gouraud;
        bool smoothNormals = true;   // сглаженные нормали вершин (false — гранёные)
        Light light;
        Vec3  objectColor{ 0.75f, 0.78f, 0.85f }; // цвет материала (линейный RGB)

        // ---- Визуализация нормалей вершин ----
        bool showVertexNormals = false; // рисовать нормали вершин (в каждой вершине грани)

        // ---- Визуализация источника света ----
        bool showLight = false;      // рисовать стрелку, откуда светит свет
    };
}