#include "Renderer.h"
#include "RenderPipeline.h"
#include "GeometryStages.h"
#include "RenderTarget.h"
#include "Rasterizer.h"
#include "Shading.h"
#include "TextureUpload.h"
#include <cmath>
#include <algorithm>
#include "Constants.h"

namespace Render {

    // Кэш рендер-таргета между кадрами
    static RenderTarget g_target;
    static TextureCache g_texCache;

    void Render(const Polyhedron& poly,
        const Projection& proj,
        const Camera& cam,
        const Mat4& modelMatrix,
        Mode mode,
        ImDrawList* drawList,
        ImVec2 origin,
        ImVec2 size,
        SDL_Renderer* renderer)
    {
        if (poly.sides.empty() || !drawList) return;

        // Полная матрица P * V и отдельно P (для перевода clip->экран в Solid/Wireframe).
        const float aspect = size.x / std::max(1.f, size.y);
        const Mat4 PV = proj.fullMatrix(cam, aspect);
        const Mat4 P = (proj.type == Projection::Type::Perspective)
            ? cam.projectionMatrix(aspect)
            : proj.projectionMatrix();

        // ---- для режимов Points и Wireframe ----
        if (mode == Mode::Points) {
            std::vector<Tri> tris = buildTriangles(poly, PV, modelMatrix);
            for (const auto& t : tris) {
                ScreenTri s = toScreen(t, origin, size);
                for (int i = 0; i < 3; ++i) {
                    if (s.invW[i] <= 0.0) continue;
                    drawList->AddCircleFilled(
                        ImVec2((float)(origin.x + s.x[i]), (float)(origin.y + s.y[i])),
                        3.0f, IM_COL32(200, 200, 200, 255));
                }
            }
            return;
        }

        if (mode == Mode::Wireframe) {
            const ImU32 lineColor = IM_COL32(64, 96, 160, 255);
            for (const auto& side : poly.sides) {
                const std::size_t n = side.vertices.size();
                for (std::size_t i = 0; i < n; ++i) {
                    std::size_t j = (i + 1) % n;
                    Vec3 wva = modelMatrix.transformPoint(side.vertices[i]);
                    Vec3 wvb = modelMatrix.transformPoint(side.vertices[j]);
                    Vec4 ca = PV * Vec4(wva, 1.0f);
                    Vec4 cb = PV * Vec4(wvb, 1.0f);
                    if (ca.w <= EPSILON || cb.w <= EPSILON) continue;
                    auto toScreenPoint = [&](const Vec4& c) -> ImVec2 {
                        double ndcX = c.x / c.w;
                        double ndcY = c.y / c.w;
                        return ImVec2(
                            (float)(origin.x + (ndcX * 0.5 + 0.5) * size.x),
                            (float)(origin.y + (1.0 - (ndcY * 0.5 + 0.5)) * size.y));
                        };
                    drawList->AddLine(toScreenPoint(ca), toScreenPoint(cb), lineColor, 1.5f);
                }
            }
            return;
        }

        // ---- Solid ----
        int W = (int)size.x;
        int H = (int)size.y;
        if (W <= 0 || H <= 0) return;

        if (g_target.width != W || g_target.height != H)
            g_target.resize(W, H);
        g_target.clear(0xFF000000);

        const ImU32 edgeColor = 0xFF0000FF;

        // Проходим по каждой грани: триангуляция → заливка → рёбра
        for (const auto& side : poly.sides) {
            Polyhedron singleFace;
            singleFace.sides.push_back(side);
            std::vector<Tri> faceTris = buildTriangles(singleFace, PV, modelMatrix);

            for (const auto& t : faceTris) {
                ScreenTri s = toScreen(t, origin, size);
                rasterizeTriangle(s, g_target, 0, 0, W, H, &shadeFlat);
            }

            const std::size_t n = side.vertices.size();
            for (std::size_t i = 0; i < n; ++i) {
                std::size_t j = (i + 1) % n;
                Vec3 wva = modelMatrix.transformPoint(side.vertices[i]);
                Vec3 wvb = modelMatrix.transformPoint(side.vertices[j]);
                Vec4 ca = PV * Vec4(wva, 1.0f);
                Vec4 cb = PV * Vec4(wvb, 1.0f);
                if (ca.w <= EPSILON || cb.w <= EPSILON) continue;

                double xa = (ca.x / ca.w * 0.5 + 0.5) * W;
                double ya = (1.0 - (ca.y / ca.w * 0.5 + 0.5)) * H;
                double za = ca.z / ca.w;

                double xb = (cb.x / cb.w * 0.5 + 0.5) * W;
                double yb = (1.0 - (cb.y / cb.w * 0.5 + 0.5)) * H;
                double zb = cb.z / cb.w;

                rasterizeLine(xa, ya, za, xb, yb, zb, g_target, edgeColor);
            }
        }

        ImTextureID tex = uploadToTexture(g_target, renderer, g_texCache);
        if (tex)
            drawList->AddImage(tex, origin, ImVec2(origin.x + W, origin.y + H));
    }

}