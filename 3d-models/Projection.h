#pragma once
#include "Polyhedron.h"
#include <imgui.h>

// Класс, отвечающий за проецирование
class Projection {
public:
    enum class Type { Axonometric, Perspective }; // Тип проецирования
    Type type = Type::Axonometric;

    // Глубина перспективы — расстояние от камеры до объекта вдоль оси взгляда
    static constexpr float kPerspectiveDepth = 10.0f;

    // Получение матрицы проекции
    Mat4 projectionMatrix() const {
        Mat4 result;
        if (type == Type::Perspective)
            result.mrx[3][2] = -1.f / kPerspectiveDepth;
        return result;
    }

    void drawParamsUI() {
        int t = (type == Type::Axonometric) ? 0 : 1;
        if (ImGui::RadioButton("Axonometric", t == 0)) type = Type::Axonometric;
        ImGui::SameLine();
        if (ImGui::RadioButton("Perspective", t == 1)) type = Type::Perspective;
    }
};