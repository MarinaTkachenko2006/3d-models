#include "Ui.h"
#include "imgui.h"

namespace {

    constexpr float kLeftWidthRatio = 0.32f; 
    constexpr float kSourceHeightRatio = 0.55f; 

    constexpr ImGuiWindowFlags kFixedPanelFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    template <class T>
    int drawSelector(const char* label, const std::vector<std::unique_ptr<T>>& items, int current) {
        ImGui::TextUnformatted(label);
        int result = current;
        for (int i = 0; i < (int)items.size(); ++i) {
            if (ImGui::RadioButton(items[i]->name(), current == i))
                result = i;
        }
        return result;
    }

    void placeWindow(const char* name, ImVec2 pos, ImVec2 size,
        ImGuiWindowFlags extraFlags = 0) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin(name, nullptr, kFixedPanelFlags | extraFlags);
    }

} 

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
    const ImVec2 viewSize{ full.x - leftW, full.y };

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
        bool hasObject = state.currentObject.has_value();
        ImGui::BeginDisabled(!hasObject);
        if (ImGui::Button("Apply") && hasObject)
            op.apply(*state.currentObject);
        ImGui::EndDisabled();
    }
    ImGui::End();

    // ---- Просмотр объекта ----
    placeWindow("Object", viewPos, viewSize);
    if (state.currentObject) {

        // поменять на нормальную визуализацию !!!!!
        auto& o = *state.currentObject;
        ImGui::Text("Temporary representation:\n");
        ImGui::Separator();
        for (size_t i = 0; i < o.sides.size(); ++i) {
            auto& s = o.sides[i];
            ImGui::Text("side [%zu]; normal (%.3f, %.3f, %.3f)", i, s.normal.x, s.normal.y, s.normal.z);
            for (size_t i = 0; i < s.vertices.size(); ++i) 
                ImGui::Text("   vertex [%zu] (%.3f, %.3f, %.3f)", i, s.vertices[i].x, s.vertices[i].y, s.vertices[i].z);
        }


        // сохранение
        ImGui::Separator();
        ImGui::TextUnformatted("Save as .obj");

        char buf[512];
        std::snprintf(buf, sizeof(buf), "%s", state.savePath.c_str());
        if (ImGui::InputText("Path", buf, sizeof(buf)))
            state.savePath = buf;

        if (ImGui::Button("Save")) {
            if (saveObj(o, state.savePath))
                state.lastSaveStatus = "OK: " + state.savePath;
            else
                state.lastSaveStatus = "ERROR: could not write " + state.savePath;
        }
        if (!state.lastSaveStatus.empty())
            ImGui::TextUnformatted(state.lastSaveStatus.c_str());
    }
    else {
        ImGui::TextDisabled("Object not created");
    }
    ImGui::End();
}