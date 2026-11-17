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

        const float aspect = size.x / std::max(1.f, size.y);
        const Mat4 PV = proj.fullMatrix(cam, aspect);

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

        // Центр объекта (для визуализации света)
        Vec3 objectCenter = (lo + hi) * 0.5f;
        const float diagLen = std::max(1e-3f, length(hi - lo));

        // Проекция точки мира в экранные координаты
        auto project = [&](const Vec3& p, ImVec2& out) -> bool {
            Vec4 c = PV * Vec4(p, 1.0f);
            if (c.w <= EPSILON) return false;
            out = ImVec2(
                (float)(origin.x + (c.x / c.w * 0.5 + 0.5) * size.x),
                (float)(origin.y + (1.0 - (c.y / c.w * 0.5 + 0.5)) * size.y));
            return true;
            };

        // Нормали видимых граней
        auto drawNormals = [&]() {
            if (!opts.showNormals) return;
            const float len = std::max(0.05f, 0.12f * diagLen);
            const ImU32 color = IM_COL32(255, 220, 0, 255);
            for (const auto& f : faces) {
                ImVec2 a, b;
                if (!project(f.center, a) || !project(f.center + f.normal * len, b)) continue;
                drawList->AddLine(a, b, color, 1.5f);
                drawList->AddCircleFilled(b, 2.5f, color);
            }
            };

        // Нормали вершин
        auto drawVertexNormals = [&](const Polyhedron& shaded) {
            if (!opts.showVertexNormals) return;
            const float len = std::max(0.03f, 0.08f * diagLen);
            const ImU32 color = IM_COL32(80, 200, 255, 255);
            for (const auto& side : shaded.sides) {
                const std::size_t n = side.vertices.size();
                if (n < 3) continue;
                const bool hasVN = (side.vertexNormals.size() == n);
                for (std::size_t i = 0; i < n; ++i) {
                    Vec3 wp = modelMatrix.transformPoint(side.vertices[i]);
                    Vec3 N = hasVN ? side.vertexNormals[i] : side.normal;

                    N = Vec3(
                        modelMatrix.mrx[0][0] * N.x + modelMatrix.mrx[0][1] * N.y + modelMatrix.mrx[0][2] * N.z,
                        modelMatrix.mrx[1][0] * N.x + modelMatrix.mrx[1][1] * N.y + modelMatrix.mrx[1][2] * N.z,
                        modelMatrix.mrx[2][0] * N.x + modelMatrix.mrx[2][1] * N.y + modelMatrix.mrx[2][2] * N.z);
                    if (mirrored) N = N * -1.f;
                    if (length(N) > EPSILON) N = normalize(N);
                    else N = Vec3(0.f, 1.f, 0.f);

                    ImVec2 a, b;
                    if (!project(wp, a) || !project(wp + N * len, b)) continue;
                    drawList->AddLine(a, b, color, 1.2f);
                    drawList->AddCircleFilled(b, 2.0f, color);
                    drawList->AddCircleFilled(a, 1.5f, color);
                }
            }
            };

        // Визуализация источника света.
        // Свет направленный (бесконечно далеко), поэтому рисуем:
        //   - точку «где-то» по направлению К свету (sourcePos)
        //   - стрелку от sourcePos к центру объекта
        //   - подпись "Light" у sourcePos
        // Плюс маленькие параллельные стрелки у объекта, чтобы было видно,
        // что свет идёт с этого направления для всех точек.
        auto drawLight = [&]() {
            if (!opts.showLight) return;
            const Vec3 L = normalize(opts.light.direction);   // к источнику
            const float dist = std::max(1.f, 1.2f * diagLen); // «расстояние» до солнца
            Vec3 sourcePos = objectCenter + L * dist;
            Vec3 arrowTo = objectCenter + L * (0.4f * diagLen);  // конец стрелки — ближе к объекту
            const ImU32 color = IM_COL32(255, 200, 0, 255);

            // Солнце
            ImVec2 sp;
            if (project(sourcePos, sp)) {
                drawList->AddCircleFilled(sp, 6.0f, color);
                drawList->AddCircle(sp, 9.0f, color, 16, 1.5f);
                drawList->AddText(ImVec2(sp.x + 12.f, sp.y - 8.f), color, "Light");
            }

            // Стрелка от источника к объекту
            ImVec2 a, b;
            if (project(sourcePos, a) && project(arrowTo, b)) {
                drawList->AddLine(a, b, color, 2.0f);
                // Наконечник
                Vec3 dir = normalize(arrowTo - objectCenter);   // к объекту
                Vec3 right = normalize(cross(dir, Vec3(0, 1, 0)));
                if (length(right) < EPSILON) right = Vec3(1, 0, 0);
                Vec3 up = normalize(cross(right, dir));
                const float headSize = 0.06f * diagLen;
                Vec3 p1 = arrowTo + (right + up) * headSize * -1.f;   // назад
                Vec3 p2 = arrowTo + (up - right) * headSize;
                ImVec2 h1, h2;
                if (project(p1, h1) && project(p2, h2)) {
                    drawList->AddTriangleFilled(b, h1, h2, color);
                }
            }

            // Несколько параллельных стрелок «свет идёт с этой стороны»
            for (int i = -1; i <= 1; ++i) {
                Vec3 offset = (i == 0) ? Vec3(0, 0, 0)
                    : (i < 0 ? Vec3(0.3f, 0, 0) * diagLen : Vec3(0, 0.3f, 0) * diagLen);
                Vec3 from = objectCenter + L * (0.9f * diagLen) + offset;
                Vec3 to = objectCenter + L * (0.6f * diagLen) + offset;
                ImVec2 a2, b2;
                if (project(from, a2) && project(to, b2))
                    drawList->AddLine(a2, b2, color, 1.0f);
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
            Polyhedron vnScene = scene;
            vnScene.computeVertexNormals(opts.smoothNormals);
            drawVertexNormals(vnScene);
            drawLight();
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
            Polyhedron vnScene = scene;
            vnScene.computeVertexNormals(opts.smoothNormals);
            drawVertexNormals(vnScene);
            drawLight();
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

        // Сэмплер
        TextureSampler sampler;
        bool useTexture = false;
        if (opts.texPixels && opts.texWidth > 0 && opts.texHeight > 0) {
            sampler.pixels = opts.texPixels;
            sampler.width = opts.texWidth;
            sampler.height = opts.texHeight;
            sampler.wrap = opts.texWrap;
            sampler.bilinear = opts.texBilinear;
            useTexture = true;
        }

        // Подготовка сцены с нормалями вершин (для Гуро / текстура+освещение)
        Polyhedron shadedScene = scene;
        if (mode == Mode::Solid && opts.shading == ShadingModel::Gouraud)
            shadedScene.computeVertexNormals(opts.smoothNormals);
        // Фонг (туншейдинг) тоже нуждается в нормалях вершин — их и интерполируем
        if (mode == Mode::Solid && opts.shading == ShadingModel::PhongToon)
            shadedScene.computeVertexNormals(opts.smoothNormals);

        // Направление к источнику света (единичное)
        const Vec3 L = normalize(opts.light.direction);

        // Данные для шейдера «текстура × Ламберт» (один раз на кадр)
        TexturedLitData litData;
        litData.sampler = sampler;
        litData.lightDir = L;
        litData.lightColor = opts.light.color;
        litData.objectColor = opts.objectColor;
        litData.ambient = opts.light.ambient;
        litData.diffuse = opts.light.diffuse;

        // Данные для шейдера Фонга с туншейдингом (один раз на кадр)
        PhongToonData toonData;
        toonData.sampler = sampler;
        toonData.useTexture = useTexture;
        toonData.lightDir = L;
        toonData.lightColor = opts.light.color;
        toonData.objectColor = opts.objectColor;
        toonData.ambient = opts.light.ambient;
        toonData.diffuse = opts.light.diffuse;
        toonData.levels = opts.toonLevels;
        toonData.specular = opts.toonSpecular;
        toonData.shininess = opts.toonShininess;
        toonData.perspective = (proj.type == Projection::Type::Perspective);
        toonData.cameraPos = cam.position();
        toonData.toViewer = cam.viewDirection() * -1.f;

        for (const auto& side : shadedScene.sides) {
            Polyhedron singleFace;
            singleFace.sides.push_back(side);
            std::vector<Tri> faceTris = buildTriangles(singleFace, PV, modelMatrix);

            for (const auto& t : faceTris) {
                ScreenTri s = toScreen(t, origin, size);

                if (opts.shading == ShadingModel::Gouraud) {
                    for (int i = 0; i < 3; ++i) {
                        Vec3 N = normalize(Vec3((float)t.v[i].nx, (float)t.v[i].ny, (float)t.v[i].nz));
                        s.vcolor[i] = lambertVertex(N, L, opts.objectColor,
                            opts.light.color,
                            opts.light.ambient,
                            opts.light.diffuse);
                    }
                }

                FragmentShader localShader;
                void* localData = nullptr;
                if (s.hasUV && useTexture && opts.shading == ShadingModel::Gouraud) {
                    localShader = &shadeTexturedLit;
                    localData = &litData;
                }
                else if (s.hasUV && useTexture) {
                    localShader = &shadeTextured;
                    localData = &sampler;
                }
                else if (opts.shading == ShadingModel::Gouraud) {
                    localShader = &shadeGouraud;
                    localData = nullptr;
                }
                else {
                    localShader = &shadeFlat;
                    localData = nullptr;
                }
                // Фонг + туншейдинг: нормали вершин уже лежат в s.nx/ny/nz,
                // шейдер интерполирует их в каждом пикселе
                if (opts.shading == ShadingModel::PhongToon) {
                    localShader = &shadePhongToon;
                    localData = &toonData;
                }
                rasterizeTriangle(s, g_target, 0, 0, W, H, localShader, localData);
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
        if (opts.shading == ShadingModel::Gouraud)
            drawVertexNormals(shadedScene);
        else {
            Polyhedron vnScene = scene;
            vnScene.computeVertexNormals(opts.smoothNormals);
            drawVertexNormals(vnScene);
        }
        drawLight();
    }

}