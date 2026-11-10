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

    // Грань в мировых координатах: центр и внешняя нормаль.
    struct WorldFace {
        Vec3 center;
        Vec3 normal;
    };

    // Нормаль считается по уже преобразованным вершинам (векторное произведение),
    // поэтому она корректна при любых поворотах/масштабах объекта.
    // Если преобразование содержит отражение, обход грани меняется на обратный —
    // нормаль разворачиваем, чтобы она осталась внешней.
    static WorldFace worldFace(const Polygon& side, const Mat4& model, bool mirrored) {
        Polygon w;
        w.vertices.reserve(side.vertices.size());
        for (const auto& v : side.vertices)
            w.vertices.push_back(model.transformPoint(v));
        w.computeNormal();

        WorldFace f;
        f.center = w.center();
        f.normal = mirrored ? w.normal * -1.f : w.normal;
        return f;
    }

    // Вектор обзора для грани.
    // Параллельная проекция: один и тот же вектор для всех граней (направление камеры).
    // Перспективная: вектор от положения камеры к грани.
    static Vec3 viewVector(const Projection& proj, const Camera& cam, const Vec3& point) {
        if (proj.type == Projection::Type::Perspective)
            return normalize(point - cam.position());
        return cam.viewDirection();
    }

    // Грань лицевая, если её внешняя нормаль направлена навстречу вектору обзора:
    // (N, V) < 0. При (N, V) >= 0 грань нелицевая и отбрасывается.
    static bool isFrontFace(const WorldFace& f, const Projection& proj, const Camera& cam) {
        return dot(f.normal, viewVector(proj, cam, f.center)) < 0.f;
    }

    void Render(const Polyhedron& poly,
        const Projection& proj,
        const Camera& cam,
        const Mat4& modelMatrix,
        Mode mode,
        const Options& opts,
        ImDrawList* drawList,
        ImVec2 origin,
        ImVec2 size,
        SDL_Renderer* renderer)
    {
        if (poly.sides.empty() || !drawList) return;

        // Полная матрица P * V.
        const float aspect = size.x / std::max(1.f, size.y);
        const Mat4 PV = proj.fullMatrix(cam, aspect);

        // ---- Отсечение нелицевых граней ----
        // В scene попадают только грани, прошедшие проверку; все режимы
        // отрисовки ниже работают уже с отфильтрованным набором.
        const bool mirrored = modelMatrix.det3() < 0.f;
        Polyhedron scene;
        std::vector<WorldFace> faces;
        scene.sides.reserve(poly.sides.size());
        faces.reserve(poly.sides.size());
        Vec3 lo, hi;
        bool first = true;
        for (const Polygon& side : poly.sides) {
            if (side.vertices.size() < 3) continue;
            const WorldFace f = worldFace(side, modelMatrix, mirrored);
            if (first) { lo = hi = f.center; first = false; }
            lo = Vec3(std::min(lo.x, f.center.x), std::min(lo.y, f.center.y), std::min(lo.z, f.center.z));
            hi = Vec3(std::max(hi.x, f.center.x), std::max(hi.y, f.center.y), std::max(hi.z, f.center.z));
            if (opts.cullBackFaces && !isFrontFace(f, proj, cam)) continue;
            scene.sides.push_back(side);
            faces.push_back(f);
        }

        // Нормали видимых граней (для наглядной проверки направления «наружу»)
        auto drawNormals = [&]() {
            if (!opts.showNormals) return;
            const float len = std::max(0.05f, 0.12f * length(hi - lo));
            const ImU32 color = IM_COL32(255, 220, 0, 255);
            auto project = [&](const Vec3& p, ImVec2& out) -> bool {
                Vec4 c = PV * Vec4(p, 1.0f);
                if (c.w <= EPSILON) return false;
                out = ImVec2(
                    (float)(origin.x + (c.x / c.w * 0.5 + 0.5) * size.x),
                    (float)(origin.y + (1.0 - (c.y / c.w * 0.5 + 0.5)) * size.y));
                return true;
            };
            for (const auto& f : faces) {
                ImVec2 a, b;
                if (!project(f.center, a) || !project(f.center + f.normal * len, b)) continue;
                drawList->AddLine(a, b, color, 1.5f);
                drawList->AddCircleFilled(b, 2.5f, color);
            }
        };

        // ---- для режимов Points и Wireframe ----
        if (mode == Mode::Points) {
            std::vector<Tri> tris = buildTriangles(scene, PV, modelMatrix);
            for (const auto& t : tris) {
                ScreenTri s = toScreen(t, origin, size);
                for (int i = 0; i < 3; ++i) {
                    if (s.invW[i] <= 0.0) continue;
                    drawList->AddCircleFilled(
                        ImVec2((float)(origin.x + s.x[i]), (float)(origin.y + s.y[i])),
                        3.0f, IM_COL32(200, 200, 200, 255));
                }
            }
            drawNormals();
            return;
        }

        if (mode == Mode::Wireframe) {
            const ImU32 lineColor = IM_COL32(64, 96, 160, 255);
            for (const auto& side : scene.sides) {
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
            drawNormals();
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
        for (const auto& side : scene.sides) {
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
        drawNormals();
    }

}