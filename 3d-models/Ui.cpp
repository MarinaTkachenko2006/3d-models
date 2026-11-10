#include "Ui.h"
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

} // namespace

void drawUi(AppState& state, Registry& registry, SDL_Renderer* renderer) {
    (void)renderer;

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
            if (auto obj = src.build()) {
                state.currentObject = std::move(*obj);
                // Нормали всех граней направляем наружу относительно объекта
                state.currentObject->orientOutward();
                state.spinAngle = 0.f;
            }
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

    // ---- Отсечение нелицевых граней ----
    ImGui::Checkbox("Back-face culling", &state.renderOptions.cullBackFaces);
    ImGui::SameLine();
    ImGui::Checkbox("Show normals", &state.renderOptions.showNormals);

    // ---- Автовращение объекта ----
    const float dt = ImGui::GetIO().DeltaTime;
    ImGui::Checkbox("Auto-rotate object", &state.spinEnabled);
    ImGui::SameLine(); ImGui::RadioButton("X##spin", &state.spinAxis, 0);
    ImGui::SameLine(); ImGui::RadioButton("Y##spin", &state.spinAxis, 1);
    ImGui::SameLine(); ImGui::RadioButton("Z##spin", &state.spinAxis, 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    ImGui::SliderFloat("deg/s##spin", &state.spinSpeed, -180.f, 180.f, "%.0f");
    if (state.spinEnabled)
        state.spinAngle = std::fmod(state.spinAngle + state.spinSpeed * dt, 360.f);

    // ---- Camera ----
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        state.camera.drawParamsUI();
        ImGui::Checkbox("Auto-orbit##camera", &state.orbitEnabled);
        if (state.orbitEnabled)
            ImGui::SliderFloat("Speed (deg/s)##camera", &state.orbitSpeed, -180.f, 180.f);
    }
    // Облёт камеры вокруг объекта (меняет вектор обзора)
    if (state.orbitEnabled) {
        state.camera.yaw += state.orbitSpeed * dt;
        if (state.camera.yaw > 180.f)  state.camera.yaw -= 360.f;
        if (state.camera.yaw < -180.f) state.camera.yaw += 360.f;
    }

    // Режим отрисовки
    ImGui::Separator();
    ImGui::TextUnformatted("Render mode:");
    int rm = (int)state.renderMode;
    if (ImGui::RadioButton("Wireframe", rm == (int)Render::Mode::Wireframe))
        state.renderMode = Render::Mode::Wireframe;
    ImGui::SameLine();
    if (ImGui::RadioButton("Points", rm == (int)Render::Mode::Points))
        state.renderMode = Render::Mode::Points;
    if (ImGui::RadioButton("Solid", rm == (int)Render::Mode::Solid))
        state.renderMode = Render::Mode::Solid;

    ImGui::Separator();

    const float saveHeight = 110.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float viewH = std::max(100.0f, avail.y - saveHeight);
    ImVec2 viewSize2(avail.x, viewH);
    ImVec2 viewPos2 = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("##view_area", viewSize2);

    Mat4 opMatrix = collectOperationsMatrix(registry, *state.currentObject);

    // Автовращение: поворот вокруг оси, проходящей через центр объекта.
    // Влияет только на отображение (в Apply/Save не попадает).
    Mat4 modelMatrix = opMatrix;
    if (state.spinAngle != 0.f) {
        constexpr float D2R = 3.14159265358979323846f / 180.f;
        const float rad = state.spinAngle * D2R;
        const Vec3 c = opMatrix.transformPoint(state.currentObject->center());
        const Mat4 R = (state.spinAxis == 0) ? Mat4::rotationX(rad)
            : (state.spinAxis == 1) ? Mat4::rotationY(rad)
            : Mat4::rotationZ(rad);
        modelMatrix = Mat4::translation(c.x, c.y, c.z) * R
            * Mat4::translation(-c.x, -c.y, -c.z) * opMatrix;
    }

    // ---- Отрисовка сцены через ImDrawList ----
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(viewPos2,
        ImVec2(viewPos2.x + viewSize2.x, viewPos2.y + viewSize2.y),
        true);
    Render::Render(*state.currentObject,
        state.projection,
        state.camera,
        modelMatrix,
        state.renderMode,
        state.renderOptions,
        dl,
        viewPos2,
        viewSize2,
        renderer);
    dl->PopClipRect();

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