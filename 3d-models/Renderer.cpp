#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include "Constants.h"

namespace Render {

    namespace {
        ScreenVertex Project(const Vec4& clip, const Vec3& world) {
            ScreenVertex v;
            v.cx = clip.x; v.cy = clip.y; v.cz = clip.z; v.cw = clip.w;
            v.wx = world.x; v.wy = world.y; v.wz = world.z;
            return v;
        }
    }

    void Render(const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix,
        Mode mode,
        ImDrawList* drawList,
        ImVec2 origin,
        ImVec2 size)
    {
        if (poly.sides.empty() || !drawList) return;

        std::vector<ScreenVertex> sv;
        sv.reserve(poly.sides.size() * 4);

        for (const auto& side : poly.sides) {
            for (const auto& v : side.vertices) {
                Vec3 wv = modelMatrix.transformPoint(v);
                Vec4 clip = proj.projectionMatrix() * Vec4(wv, 1.0f);
                sv.push_back(Project(clip, wv));
            }
        }

        auto toScreen = [&](const ScreenVertex& v) -> ImVec2 {
            float ndcX = (float)(v.cx / v.cw);
            float ndcY = (float)(v.cy / v.cw);
            return ImVec2(
                origin.x + (ndcX * 0.5f + 0.5f) * size.x,
                origin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * size.y
            );
            };

        const ImU32 lineColor = IM_COL32(64, 96, 160, 255);

        if (mode == Mode::Points) {
            for (const auto& v : sv) {
                if (v.cw <= EPSILON) continue;
                drawList->AddCircleFilled(toScreen(v), 3.0f, IM_COL32(255, 255, 255, 255));
            }
            return;
        }

        // Wireframe
        size_t base = 0;
        for (const auto& side : poly.sides) {
            const auto& verts = side.vertices;
            size_t n = verts.size();
            for (size_t i = 0; i < n; ++i) {
                size_t j = (i + 1) % n;
                const auto& a = sv[base + i];
                const auto& b = sv[base + j];
                if (a.cw <= EPSILON || b.cw <= EPSILON) continue;
                drawList->AddLine(toScreen(a), toScreen(b), lineColor, 1.5f);
            }
            base += n;
        }
    }
}