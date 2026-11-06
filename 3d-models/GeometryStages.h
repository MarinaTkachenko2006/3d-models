#pragma once
#include "RenderPipeline.h"
#include "Triangulator.h"

namespace Render {

    // Stage 1: poly + modelMatrix + projection -> список треугольников в clip space.
    // Порядок: clip = P * M * v (единый для аксонометрии и перспективы).
    // Нормали и world-координаты сохраняются в ClipVertex для будущих стадий.
    inline std::vector<Tri> buildTriangles(const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix) {
        std::vector<Tri> result;
        result.reserve(poly.sides.size() * 2);

        const Mat4 P = proj.projectionMatrix();
        int faceId = 0;

        for (const auto& side : poly.sides) {
            const auto tris = triangulate(side);
            if (tris.empty()) { ++faceId; continue; }

            // Предвычисляем clip-вершины для всех вершин грани
            std::vector<ClipVertex> cv;
            cv.reserve(side.vertices.size());
            for (const auto& v : side.vertices) {
                Vec3 wv = modelMatrix.transformPoint(v);
                Vec4 clip = P * Vec4(wv, 1.0f);
                ClipVertex c;
                c.cx = clip.x; c.cy = clip.y; c.cz = clip.z; c.cw = clip.w;
                c.wx = wv.x;   c.wy = wv.y;   c.wz = wv.z;
                c.nx = side.normal.x; c.ny = side.normal.y; c.nz = side.normal.z;
                c.invW = (std::abs(clip.w) > EPSILON) ? (1.0 / clip.w) : 0.0;
                cv.push_back(c);
            }

            for (const auto& t : tris) {
                Tri tri;
                tri.v[0] = cv[t[0]];
                tri.v[1] = cv[t[1]];
                tri.v[2] = cv[t[2]];
                tri.faceId = faceId;
                result.push_back(tri);
            }
            ++faceId;
        }
        return result;
    }

    // Stage 2: clip space -> экранные координаты.
    // (деление на cw, отображение в прямоугольник).
    inline ScreenTri toScreen(const Tri& t, ImVec2 origin, ImVec2 size) {
        ScreenTri s;
        s.faceId = t.faceId;
        for (int i = 0; i < 3; ++i) {
            const ClipVertex& v = t.v[i];
            s.x[i] = (v.cx / v.cw * 0.5 + 0.5) * size.x;
            s.y[i] = (1.0 - (v.cy / v.cw * 0.5 + 0.5)) * size.y;
            s.z[i] = (v.cz / v.cw);
            s.invW[i] = v.invW;
            s.wx[i] = v.wx; s.wy[i] = v.wy; s.wz[i] = v.wz;
            s.nx[i] = v.nx; s.ny[i] = v.ny; s.nz[i] = v.nz;
        }
        return s;
    }

}