#include "Ui.h"
#include "TexHolder.h"
#include "Renderer.h"
#include "imgui.h"
#include "Projection.h"
#include <SDL3/SDL.h>
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

    Mat4 collectOperationsMatrix(Registry& registry, const Polyhedron& poly) {
        const Vec3 center = poly.center();
        Mat4 result;
        for (auto& op : registry.operations())
            result = op->currentMatrix(result.transformPoint(center)) * result;
        return result;
    }

    // ---- Vanishing points / wireframe overlay (оставлено для аксонометрии/перспективы) ----
    // ... (ваш прежний код drawVanishingPoints / clipSegment / lineIntersect — без изменений)

    // Единый TexHolder (статический, чтобы не пересоздавать текстуру)
    static TexHolder g_tex;

} // namespace

void drawUi(AppState& state, Registry& registry, SDL_Renderer* renderer) {
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

    // ---- Object source ----
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

    // ---- Transformation ----
    placeWindow("Transformation", opPos, opSize);
    state.selectedOperation = drawSelector("Operation:",
        registry.operations(), state.selectedOperation);

    if (state.selectedOperation >= 0) {
        auto& op = *registry.operations()[state.selectedOperation];
        op.drawParamsUI();
        if (ImGui::Button("Reset")) op.reset();
    }

    ImGui::Separator();
    if (state.currentObject && ImGui::Button("Apply to object")) {
        state.currentObject->transform(collectOperationsMatrix(registry, *state.currentObject));
        for (auto& op : registry.operations()) op->reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset all"))
        for (auto& op : registry.operations()) op->reset();
    ImGui::End();

    // ---- Object (view) ----
    placeWindow("Object", viewPos, viewSize);

    if (!state.currentObject) {
        ImGui::TextDisabled("Object not created");
        ImGui::End();
        return;
    }

    state.projection.drawParamsUI();
    ImGui::Checkbox("Show vanishing points", &state.showVanishingPoints);

    // Режим отрисовки
    ImGui::Separator();
    ImGui::TextUnformatted("Render mode:");
    int rm = (int)state.renderMode;
    if (ImGui::RadioButton("Solid", rm == 0)) state.renderMode = Render::Mode::Solid;
    ImGui::SameLine();
    if (ImGui::RadioButton("Wireframe", rm == 1)) state.renderMode = Render::Mode::Wireframe;
    ImGui::SameLine();
    if (ImGui::RadioButton("Points", rm == 3)) state.renderMode = Render::Mode::Points;

    ImGui::Separator();

    const float saveHeight = 110.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float viewH = std::max(100.0f, avail.y - saveHeight);
    ImVec2 viewSize2(avail.x, viewH);
    ImVec2 viewPos2 = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##view_area", viewSize2);

    // Растеризация в PixelBuffer
    size_t pw = (size_t)std::max(1.0f, viewSize2.x);
    size_t ph = (size_t)std::max(1.0f, viewSize2.y);
    if (state.buffer.Width() != pw || state.buffer.Height() != ph)
        state.buffer.Resize(pw, ph);

    Mat4 opMatrix = collectOperationsMatrix(registry, *state.currentObject);
    Render::Render(state.buffer, *state.currentObject, state.projection, opMatrix, state.renderMode);

    // SDL-текстура + ImGui::Image
    g_tex.Ensure(renderer, (int)pw, (int)ph);
    g_tex.Update(state.buffer);
    ImGui::SetCursorScreenPos(viewPos2);
    ImGui::Image(g_tex.ImGuiID(), viewSize2);

    // ---- Save ----
    ImGui::Separator();
    ImGui::TextUnformatted("Save as .obj");

    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", state.savePath.c_str());
    if (ImGui::InputText("Path", buf, sizeof(buf)))
        state.savePath = buf;

    if (ImGui::Button("Save")) {
        Polyhedron snapshot = *state.currentObject;
        snapshot.transform(opMatrix);
        state.lastSaveStatus = saveObj(snapshot, state.savePath)
            ? "OK: " + state.savePath
            : "ERROR: could not write " + state.savePath;
    }
    if (!state.lastSaveStatus.empty())
        ImGui::TextUnformatted(state.lastSaveStatus.c_str());

    ImGui::End();
}