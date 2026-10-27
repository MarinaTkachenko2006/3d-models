#include "Ui.h"
#include "imgui.h"
#include "Projection.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {

    constexpr float kLeftWidthRatio = 0.32f;
    constexpr float kSourceHeightRatio = 0.55f;

    constexpr ImGuiWindowFlags kFixedPanelFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    template <class T>
    int drawSelector(const char* label,
        const std::vector<std::unique_ptr<T>>& items, int current) {
        ImGui::TextUnformatted(label);
        int result = current;
        for (int i = 0; i < (int)items.size(); ++i)
            if (ImGui::RadioButton(items[i]->name(), current == i)) result = i;
        return result;
    }

    void placeWindow(const char* name, ImVec2 pos, ImVec2 size,
        ImGuiWindowFlags extraFlags = 0) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin(name, nullptr, kFixedPanelFlags | extraFlags);
    }

    // Собирает суммарную матрицу всех операций в порядке их регистрации
    Mat4 collectOperationsMatrix(Registry& registry) {
        Mat4 result;
        for (auto& op : registry.operations())
            result = op->currentMatrix() * result;
        return result;
    }

    // Отсечение отрезка прямоугольником (Liang–Barsky)
    bool clipSegment(ImVec2& a, ImVec2& b,
        float xMin, float yMin, float xMax, float yMax) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float t0 = 0.0f, t1 = 1.0f;

        auto clip = [&](float p, float q) -> bool {
            if (std::abs(p) < 1e-9f) return q >= 0.0f;
            float r = q / p;
            if (p < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
            return true;
            };

        if (!clip(-dx, a.x - xMin)) return false;
        if (!clip(dx, xMax - a.x)) return false;
        if (!clip(-dy, a.y - yMin)) return false;
        if (!clip(dy, yMax - a.y)) return false;

        ImVec2 na(a.x + t0 * dx, a.y + t0 * dy);
        ImVec2 nb(a.x + t1 * dx, a.y + t1 * dy);
        a = na; b = nb;
        return true;
    }

    bool insideRect(const ImVec2& p, float xMin, float yMin, float xMax, float yMax) {
        return p.x >= xMin && p.x <= xMax && p.y >= yMin && p.y <= yMax;
    }

    // Пересечение двух бесконечных прямых a1-b1 и a2-b2
    bool lineIntersect(const ImVec2& a1, const ImVec2& b1,
        const ImVec2& a2, const ImVec2& b2,
        ImVec2& out) {
        float d1x = b1.x - a1.x, d1y = b1.y - a1.y;
        float d2x = b2.x - a2.x, d2y = b2.y - a2.y;
        float den = d1x * d2y - d1y * d2x;
        if (std::abs(den) < 1e-6f) return false;
        float t = ((a2.x - a1.x) * d2y - (a2.y - a1.y) * d2x) / den;
        out = ImVec2(a1.x + t * d1x, a1.y + t * d1y);
        return true;
    }

    // Точки схода: для каждой пары рёбер, параллельных в 3D,
    // находим их пересечение на экране и рисуем точку + пунктирные продолжения.
    // В аксонометрии такие пересечения уходят в бесконечность, поэтому
    // функция фактически имеет смысл только в перспективе.
    void drawVanishingPoints(ImDrawList* dl,
        const Polyhedron& poly,
        const std::function<bool(const Vec3&, ImVec2&)>& project,
        ImVec2 centerScreen,
        float xMin, float yMin, float xMax, float yMax) {
        struct Edge { ImVec2 a, b; Vec3 dir3D; };
        std::vector<Edge> edges;

        for (const auto& side : poly.sides) {
            std::vector<ImVec2> spts;
            std::vector<Vec3>   wpts;
            bool ok = true;
            for (const auto& v : side.vertices) {
                ImVec2 s;
                if (!project(v, s)) { ok = false; break; }
                spts.push_back(s);
                wpts.push_back(v);
            }
            if (!ok) continue;

            for (size_t i = 0; i < spts.size(); ++i) {
                size_t j = (i + 1) % spts.size();
                Vec3 d(wpts[j].x - wpts[i].x,
                    wpts[j].y - wpts[i].y,
                    wpts[j].z - wpts[i].z);
                d = normalize(d);
                // Убираем антипараллельность (ребро и его обратное направление — одно семейство)
                if (d.x < -1e-3f ||
                    (std::abs(d.x) <= 1e-3f && d.y < 0) ||
                    (std::abs(d.x) <= 1e-3f && std::abs(d.y) <= 1e-3f && d.z < 0))
                    d = Vec3(-d.x, -d.y, -d.z);
                edges.push_back({ spts[i], spts[j], d });
            }
        }

        const ImU32 kVanishing = IM_COL32(255, 80, 80, 255);
        const ImU32 kProlong = IM_COL32(255, 80, 80, 90);
        const float kMaxDist = 8000.0f;

        std::vector<ImVec2> drawnPoints;

        auto distToSeg = [](const ImVec2& p, const ImVec2& a, const ImVec2& b) {
            float dx = b.x - a.x, dy = b.y - a.y;
            float len2 = dx * dx + dy * dy;
            if (len2 < 1e-6f) return std::hypot(p.x - a.x, p.y - a.y);
            float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
            t = std::clamp(t, 0.0f, 1.0f);
            float px = a.x + t * dx, py = a.y + t * dy;
            return std::hypot(p.x - px, p.y - py);
            };

        for (size_t i = 0; i < edges.size(); ++i) {
            for (size_t j = i + 1; j < edges.size(); ++j) {
                float dot = edges[i].dir3D.x * edges[j].dir3D.x
                    + edges[i].dir3D.y * edges[j].dir3D.y
                    + edges[i].dir3D.z * edges[j].dir3D.z;
                if (dot < 0.99f) continue;   // рёбра не параллельны в 3D

                ImVec2 vp;
                if (!lineIntersect(edges[i].a, edges[i].b,
                    edges[j].a, edges[j].b, vp)) continue;

                if (distToSeg(vp, edges[i].a, edges[i].b) < 3.0f) continue;
                if (distToSeg(vp, edges[j].a, edges[j].b) < 3.0f) continue;

                float dxs = vp.x - centerScreen.x;
                float dys = vp.y - centerScreen.y;
                if (dxs * dxs + dys * dys > kMaxDist * kMaxDist) continue;

                if (!insideRect(vp, xMin, yMin, xMax, yMax)) continue;

                bool duplicate = false;
                for (const auto& p : drawnPoints) {
                    if (std::hypot(p.x - vp.x, p.y - vp.y) < 5.0f) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;
                drawnPoints.push_back(vp);

                {
                    ImVec2 a = edges[i].a, b = vp;
                    if (clipSegment(a, b, xMin, yMin, xMax, yMax))
                        dl->AddLine(a, b, kProlong, 1.0f);
                }
                {
                    ImVec2 a = edges[j].a, b = vp;
                    if (clipSegment(a, b, xMin, yMin, xMax, yMax))
                        dl->AddLine(a, b, kProlong, 1.0f);
                }

                dl->AddCircleFilled(vp, 5.0f, kVanishing);
                dl->AddCircle(vp, 7.0f, kVanishing, 0, 1.5f);
            }
        }
    }

    // Проекция + отрисовка каркаса.
    // opMatrix — суммарная матрица активных операций (накладывается ДО проекции).
    void renderWireframe(ImDrawList* dl, ImVec2 origin, ImVec2 size,
        const Polyhedron& poly, const Projection& proj,
        const Mat4& opMatrix,
        bool showVanishing) {
        if (poly.sides.empty()) return;

        Mat4 M = proj.projectionMatrix() * opMatrix;

        // ---- 1. Проецируем все вершины (с делением на w) ----
        struct ProjVert { float x, y; bool valid; };
        std::vector<ProjVert> projected;
        projected.reserve(poly.sides.size() * 4);

        for (const auto& side : poly.sides) {
            for (const auto& v : side.vertices) {
                Vec4 p = M * Vec4(v, 1.0f);
                bool valid = (p.w > 1e-4f);
                float x = valid ? p.x / p.w : 0.f;
                float y = valid ? p.y / p.w : 0.f;
                projected.push_back({ x, y, valid });
            }
        }

        // ---- 2. Фиксированный масштаб, центр мира в центре экрана ----
        const float margin = 25.0f;
        const float ww = std::max(10.0f, size.x - 2.0f * margin);
        const float hh = std::max(10.0f, size.y - 2.0f * margin);

        const float scale = std::min(ww, hh) * 0.35f;
        const float cx = 0.0f;
        const float cy = 0.0f;

        ImVec2 centerScreen{ origin.x + size.x * 0.5f, origin.y + size.y * 0.5f };

        auto toScreen = [&](float nx, float ny) -> ImVec2 {
            return ImVec2(centerScreen.x + (nx - cx) * scale,
                centerScreen.y - (ny - cy) * scale);
            };

        const float xMin = origin.x;
        const float yMin = origin.y;
        const float xMax = origin.x + size.x;
        const float yMax = origin.y + size.y;

        // ---- 3. Отрисовка каркаса с отсечением ----
        const ImU32 kEdge = IM_COL32(180, 210, 255, 255);
        const ImU32 kVert = IM_COL32(255, 160, 100, 255);

        size_t idx = 0;
        for (const auto& side : poly.sides) {
            std::vector<ImVec2> pts;
            std::vector<bool> okPt;
            for (size_t k = 0; k < side.vertices.size(); ++k, ++idx) {
                const auto& pv = projected[idx];
                if (!pv.valid) { pts.push_back(ImVec2(0, 0)); okPt.push_back(false); continue; }
                pts.push_back(toScreen(pv.x, pv.y));
                okPt.push_back(true);
            }

            for (size_t i = 0; i < pts.size(); ++i) {
                size_t j = (i + 1) % pts.size();
                if (!okPt[i] || !okPt[j]) continue;
                ImVec2 a = pts[i], b = pts[j];
                if (clipSegment(a, b, xMin, yMin, xMax, yMax))
                    dl->AddLine(a, b, kEdge, 1.5f);
            }
            for (size_t i = 0; i < pts.size(); ++i) {
                if (!okPt[i]) continue;
                if (insideRect(pts[i], xMin, yMin, xMax, yMax))
                    dl->AddCircleFilled(pts[i], 2.5f, kVert);
            }
        }

        // ---- 4. Точки схода ----
        if (showVanishing && proj.type == Projection::Type::Perspective) {
            auto projectForVanishing = [&](const Vec3& v, ImVec2& out) -> bool {
                Vec4 p = M * Vec4(v, 1.0f);
                if (p.w <= 1e-4f) return false;
                out = toScreen(p.x / p.w, p.y / p.w);
                return true;
                };
            drawVanishingPoints(dl, poly, projectForVanishing,
                centerScreen, xMin, yMin, xMax, yMax);
        }
    }

} // namespace

void drawUi(AppState& state, Registry& registry) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 origin = vp->WorkPos;
    const ImVec2 full = vp->WorkSize;

    const float leftW = full.x * kLeftWidthRatio;
    const float sourceH = full.y * kSourceHeightRatio;

    const ImVec2 sourcePos = origin;
    const ImVec2 sourceSize{ leftW, sourceH };

    const ImVec2 opPos = { origin.x, origin.y + sourceH };
    const ImVec2 opSize = { leftW, full.y - sourceH };

    const ImVec2 viewPos = { origin.x + leftW, origin.y };
    const ImVec2 viewSize = { full.x - leftW, full.y };

    // ---- Источник фигуры ----
    placeWindow("Object source", sourcePos, sourceSize);
    state.selectedSource = drawSelector("Way of creation:",
        registry.sources(), state.selectedSource);

    if (state.selectedSource >= 0) {
        auto& src = *registry.sources()[state.selectedSource];
        src.drawParamsUI();
        if (ImGui::Button("Create")) {
            if (auto obj = src.build())
                state.currentObject = std::move(*obj);
        }
    }
    ImGui::End();

    // ---- Операция ----
    placeWindow("Transformation", opPos, opSize);
    state.selectedOperation = drawSelector("Operation:",
        registry.operations(), state.selectedOperation);

    if (state.selectedOperation >= 0) {
        auto& op = *registry.operations()[state.selectedOperation];
        op.drawParamsUI();
    }
    ImGui::End();

    // ---- Просмотр объекта ----
    placeWindow("Object", viewPos, viewSize);

    if (!state.currentObject) {
        ImGui::TextDisabled("Object not created");
        ImGui::End();
        return;
    }

    state.projection.drawParamsUI();
    ImGui::Checkbox("Show vanishing points", &state.showVanishingPoints);
    ImGui::Separator();

    const float saveHeight = 110.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float  viewH = std::max(100.0f, avail.y - saveHeight);

    ImVec2 viewSize2(avail.x, viewH);
    ImVec2 viewPos2 = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##view_area", viewSize2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(viewPos2,
        ImVec2(viewPos2.x + viewSize2.x, viewPos2.y + viewSize2.y),
        IM_COL32(22, 24, 30, 255), 4.0f);
    dl->AddRect(viewPos2,
        ImVec2(viewPos2.x + viewSize2.x, viewPos2.y + viewSize2.y),
        IM_COL32(70, 75, 90, 255), 4.0f);

    Mat4 opMatrix = collectOperationsMatrix(registry);
    renderWireframe(dl, viewPos2, viewSize2,
        *state.currentObject, state.projection,
        opMatrix,
        state.showVanishingPoints);

    // ---- сохранение ----
    ImGui::Separator();
    ImGui::TextUnformatted("Save as .obj");

    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", state.savePath.c_str());
    if (ImGui::InputText("Path", buf, sizeof(buf)))
        state.savePath = buf;

    if (ImGui::Button("Save")) {
        Polyhedron snapshot = *state.currentObject;
        for (auto& side : snapshot.sides) {
            for (auto& v : side.vertices)
                v = opMatrix.transformPoint(v);
            side.computeNormal();
        }
        state.lastSaveStatus = saveObj(snapshot, state.savePath)
            ? "OK: " + state.savePath
            : "ERROR: could not write " + state.savePath;
    }
    if (!state.lastSaveStatus.empty())
        ImGui::TextUnformatted(state.lastSaveStatus.c_str());

    ImGui::End();
}