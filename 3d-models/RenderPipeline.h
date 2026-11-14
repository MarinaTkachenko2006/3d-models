#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include "RenderMode.h"
#include <vector>

namespace Render {

    // Вершина после model + projection (clip space), с мировыми координатами
    // и нормалью — для будущих стадий (отсечение по нормалям, Z-буфер,
    // освещение, перспективно-корректная интерполяция).
    struct ClipVertex {
        double cx = 0, cy = 0, cz = 0, cw = 1;  // clip space
        double wx = 0, wy = 0, wz = 0;          // world space
        double nx = 0, ny = 0, nz = 1;          // world normal
        double invW = 1.0;                      // 1/cw (для перспективно-корректной интерполяции)
        double u = 0.0, v = 0.0;                // текстурные координаты
        bool   hasUV = false;                   // есть ли UV у этой вершины
    };

    // Треугольник в clip space с идентификатором исходной грани
    struct Tri {
        ClipVertex v[3];
        int faceId = -1;
    };

    // Треугольник в экранном пространстве
    struct ScreenTri {
        double x[3] = { 0,0,0 };
        double y[3] = { 0,0,0 };
        double z[3] = { 0,0,0 };      // NDC z
        double invW[3] = { 1,1,1 };
        double wx[3] = { 0,0,0 };
        double wy[3] = { 0,0,0 };
        double wz[3] = { 0,0,0 };
        double nx[3] = { 0,0,1 };
        double ny[3] = { 0,0,1 };
        double nz[3] = { 0,0,1 };
        double u[3] = { 0,0,0 };      // UV вершин
        double v[3] = { 0,0,0 };
        bool   hasUV = false;         // true, если все три вершины с UV
        int faceId = -1;
    };

}