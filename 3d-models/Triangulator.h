#pragma once
#include "Polyhedron.h"
#include <array>
#include <vector>

namespace Render {

    // Триангуляция полигона веером относительно 0-й вершины.
    // Для полигонов <3 вершин возвращает пустой список.
    // Возвращает индексы вершин внутри polygon.vertices.
    // Только для выпуклых полигонов
    inline std::vector<std::array<int, 3>> triangulate(const Polygon& poly) {
        std::vector<std::array<int, 3>> result;
        const std::size_t n = poly.vertices.size();
        if (n < 3) return result;
        result.reserve(n - 2);
        for (std::size_t i = 1; i + 1 < n; ++i)
            result.push_back({ 0, (int)i, (int)(i + 1) });
        return result;
    }

}