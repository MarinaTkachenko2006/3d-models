#pragma once
#include "RenderPipeline.h"
#include "Triangulator.h"
#include <vector>
#include <cmath>

namespace Render {

    // Перегрузка с явной матрицей PV = P * V (учитывает камеру).
    inline std::vector<Tri> buildTriangles(const Polyhedron& poly,
        const Mat4& PV,
        const Mat4& modelMatrix) {
        std::vector<Tri> result;
        result.reserve(poly.sides.size() * 2);

        const Mat4& P = PV;
        int faceId = 0;

        for (const auto& side : poly.sides) {
            const auto tris = triangulate(side);
            if (tris.empty()) { ++faceId; continue; }

            const bool hasUV = (side.uvs.size() == side.vertices.size());

            std::vector<ClipVertex> cv;
            cv.reserve(side.vertices.size());
            for (std::size_t i = 0; i < side.vertices.size(); ++i) {
                const Vec3& v = side.vertices[i];
                Vec3 wv = modelMatrix.transformPoint(v);
                Vec4 clip = P * Vec4(wv, 1.0f);
                ClipVertex c;
                c.cx = clip.x; c.cy = clip.y; c.cz = clip.z; c.cw = clip.w;
                c.wx = wv.x;   c.wy = wv.y;   c.wz = wv.z;
                c.nx = side.normal.x; c.ny = side.normal.y; c.nz = side.normal.z;
                c.invW = (std::abs(clip.w) > EPSILON) ? (1.0 / clip.w) : 0.0;
                if (hasUV) {
                    c.u = side.uvs[i].x;
                    c.v = side.uvs[i].y;
                    c.hasUV = true;
                }
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

    // Старая сигнатура: без камеры, P берётся из Projection.
    inline std::vector<Tri> buildTriangles(const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix) {
        return buildTriangles(poly, proj.projectionMatrix(), modelMatrix);
    }

    // Stage 2: clip space -> экранные координаты.
    inline ScreenTri toScreen(const Tri& t, ImVec2 origin, ImVec2 size) {
        ScreenTri s;
        s.faceId = t.faceId;
        s.hasUV = t.v[0].hasUV && t.v[1].hasUV && t.v[2].hasUV;
        for (int i = 0; i < 3; ++i) {
            const ClipVertex& v = t.v[i];
            s.x[i] = (v.cx / v.cw * 0.5 + 0.5) * size.x;
            s.y[i] = (1.0 - (v.cy / v.cw * 0.5 + 0.5)) * size.y;
            s.z[i] = (v.cz / v.cw);
            s.invW[i] = v.invW;
            s.wx[i] = v.wx; s.wy[i] = v.wy; s.wz[i] = v.wz;
            s.nx[i] = v.nx; s.ny[i] = v.ny; s.nz[i] = v.nz;
            s.u[i] = v.u;
            s.v[i] = v.v;
        }
        return s;
    }

}