// IOperation.h
#pragma once
#include "Polyhedron.h"
#include <imgui.h>
#include <numbers>

class IOperation {
public:
    virtual ~IOperation() = default;

    // Название операции
    virtual const char* name() const = 0;
    // Отрисовка UI элементов
    virtual void drawParamsUI() = 0;
    // Получение матрицы операции
    virtual Mat4 currentMatrix() const = 0;

    // Применить операцию к объекту
    virtual void apply(Polyhedron& obj) {
        Mat4 M = currentMatrix();
        for (auto& side : obj.sides) {
            for (auto& v : side.vertices)
                v = M.transformPoint(v);
            side.computeNormal();
        }
    }
};

// Поворот вокруг координатных осей: три независимых угла X, Y, Z
class RotateOperation : public IOperation {
public:
    const char* name() const override {
        return "Rotate (XYZ)";
    }

    void drawParamsUI() override {
        ImGui::SliderFloat("Angle X (deg)", &__angleX, -360.0f, 360.0f);
        ImGui::SliderFloat("Angle Y (deg)", &__angleY, -360.0f, 360.0f);
        ImGui::SliderFloat("Angle Z (deg)", &__angleZ, -360.0f, 360.0f);
    }

    Mat4 currentMatrix() const override {
        constexpr float D2R(std::numbers::pi / 180.f);
        // Порядок применения: сначала X, потом Y, потом Z
        return Mat4::rotationZ(__angleZ * D2R)
            * Mat4::rotationY(__angleY * D2R)
            * Mat4::rotationX(__angleX * D2R);
    }

private:
    float __angleX = 0.0f, __angleY = 0.0f, __angleZ = 0.0f;
};

// Смещение по X/Y/Z
class TranslateOperation : public IOperation {
public:
    const char* name() const override {
        return "Translate";
    }

    void drawParamsUI() override {
        ImGui::SliderFloat("X offset", &__offset[0], -10.f, 10.f, "%.2f");
        ImGui::SliderFloat("Y offset", &__offset[1], -10.f, 10.f, "%.2f");
        ImGui::SliderFloat("Z offset", &__offset[2], -10.f, 10.f, "%.2f");
    }

    Mat4 currentMatrix() const override {
        return Mat4::translation(__offset[0], __offset[1], __offset[2]);
    }

private:
    float __offset[3] = { 0.f, 0.f, 0.f };
};