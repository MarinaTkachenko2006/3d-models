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
    // Получение матрицы операции.
    // center — текущий центр многогранника (нужен операциям "относительно центра")
    virtual Mat4 currentMatrix(const Vec3& center) const = 0;
    // Сброс параметров (операция снова даёт единичную матрицу)
    virtual void reset() = 0;

    // Применить операцию к объекту
    virtual void apply(Polyhedron& obj) {
        obj.transform(currentMatrix(obj.center()));
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

    Mat4 currentMatrix(const Vec3&) const override {
        constexpr float D2R(std::numbers::pi_v<float> / 180.f);
        // Порядок применения: сначала X, потом Y, потом Z
        return Mat4::rotationZ(__angleZ * D2R)
            * Mat4::rotationY(__angleY * D2R)
            * Mat4::rotationX(__angleX * D2R);
    }

    void reset() override { *this = RotateOperation(); }

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

    Mat4 currentMatrix(const Vec3&) const override {
        return Mat4::translation(__offset[0], __offset[1], __offset[2]);
    }

    void reset() override { *this = TranslateOperation(); }

private:
    float __offset[3] = { 0.f, 0.f, 0.f };
};

// Масштабирование по X/Y/Z относительно начала координат
class ScaleOperation : public IOperation {
public:
    const char* name() const override {
        return "Scale";
    }

    void drawParamsUI() override {
        ImGui::SliderFloat("X factor", &__factor[0], 0.1f, 5.f, "%.2f");
        ImGui::SliderFloat("Y factor", &__factor[1], 0.1f, 5.f, "%.2f");
        ImGui::SliderFloat("Z factor", &__factor[2], 0.1f, 5.f, "%.2f");
    }

    Mat4 currentMatrix(const Vec3&) const override {
        return Mat4::scaling(__factor[0], __factor[1], __factor[2]);
    }

    void reset() override { *this = ScaleOperation(); }

private:
    float __factor[3] = { 1.f, 1.f, 1.f };
};

// Отражение относительно выбранной координатной плоскости
class ReflectOperation : public IOperation {
public:
    const char* name() const override {
        return "Reflect";
    }

    void drawParamsUI() override {
        ImGui::TextUnformatted("Plane:");
        if (ImGui::RadioButton("None", __plane == Plane::None)) __plane = Plane::None;
        ImGui::SameLine();
        if (ImGui::RadioButton("XY", __plane == Plane::XY)) __plane = Plane::XY;
        ImGui::SameLine();
        if (ImGui::RadioButton("YZ", __plane == Plane::YZ)) __plane = Plane::YZ;
        ImGui::SameLine();
        if (ImGui::RadioButton("XZ", __plane == Plane::XZ)) __plane = Plane::XZ;
    }

    Mat4 currentMatrix(const Vec3&) const override {
        // Отражение = масштаб с коэффициентом -1 по оси, перпендикулярной плоскости
        switch (__plane) {
        case Plane::XY: return Mat4::scaling(1.f, 1.f, -1.f);
        case Plane::YZ: return Mat4::scaling(-1.f, 1.f, 1.f);
        case Plane::XZ: return Mat4::scaling(1.f, -1.f, 1.f);
        default:        return Mat4::identity();
        }
    }

    void reset() override { *this = ReflectOperation(); }

private:
    enum class Plane { None, XY, YZ, XZ };
    Plane __plane = Plane::None;
};

// Масштабирование многогранника относительно своего центра
class CenterScaleOperation : public IOperation {
public:
    const char* name() const override {
        return "Scale (around center)";
    }

    void drawParamsUI() override {
        ImGui::SliderFloat("Factor", &__factor, 0.1f, 5.f, "%.2f");
    }

    Mat4 currentMatrix(const Vec3& center) const override {
        // Переносим центр в начало координат, масштабируем, возвращаем обратно
        return Mat4::translation(center.x, center.y, center.z)
            * Mat4::scaling(__factor, __factor, __factor)
            * Mat4::translation(-center.x, -center.y, -center.z);
    }

    void reset() override { *this = CenterScaleOperation(); }

private:
    float __factor = 1.f;
};

// Вращение вокруг прямой, проходящей через центр многогранника параллельно выбранной оси
class CenterRotateOperation : public IOperation {
public:
    const char* name() const override {
        return "Rotate (around center)";
    }

    void drawParamsUI() override {
        ImGui::TextUnformatted("Axis:");
        if (ImGui::RadioButton("X", __axis == 0)) __axis = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Y", __axis == 1)) __axis = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Z", __axis == 2)) __axis = 2;
        ImGui::SliderFloat("Angle (deg)", &__angle, -360.0f, 360.0f);
    }

    Mat4 currentMatrix(const Vec3& center) const override {
        constexpr float D2R(std::numbers::pi_v<float> / 180.f);
        float rad(__angle * D2R);
        Mat4 rotation = (__axis == 0) ? Mat4::rotationX(rad)
            : (__axis == 1) ? Mat4::rotationY(rad)
            : Mat4::rotationZ(rad);
        return Mat4::translation(center.x, center.y, center.z)
            * rotation
            * Mat4::translation(-center.x, -center.y, -center.z);
    }

    void reset() override { *this = CenterRotateOperation(); }

private:
    int __axis = 0;   // 0 — X, 1 — Y, 2 — Z
    float __angle = 0.0f;
};

// Поворот вокруг произвольной прямой, заданной двумя точками
class LineRotateOperation : public IOperation {
public:
    const char* name() const override {
        return "Rotate (around line)";
    }

    void drawParamsUI() override {
        ImGui::DragFloat3("Point 1", __p1, 0.05f, -10.f, 10.f, "%.2f");
        ImGui::DragFloat3("Point 2", __p2, 0.05f, -10.f, 10.f, "%.2f");
        ImGui::SliderFloat("Angle (deg)", &__angle, -360.0f, 360.0f);
        if (isDegenerate())
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Points must differ");
    }

    Mat4 currentMatrix(const Vec3&) const override {
        if (isDegenerate()) return Mat4::identity();
        constexpr float D2R(std::numbers::pi_v<float> / 180.f);
        // Переносим точку 1 в начало координат, поворачиваем вокруг
        // направления (p2 - p1), возвращаем обратно
        return Mat4::translation(__p1[0], __p1[1], __p1[2])
            * Mat4::rotationAxis(__p2[0] - __p1[0], __p2[1] - __p1[1], __p2[2] - __p1[2], __angle * D2R)
            * Mat4::translation(-__p1[0], -__p1[1], -__p1[2]);
    }

    void reset() override { *this = LineRotateOperation(); }

private:
    bool isDegenerate() const {
        return length(Vec3(__p2[0] - __p1[0], __p2[1] - __p1[1], __p2[2] - __p1[2])) < 1e-6f;
    }

    float __p1[3] = { 0.f, 0.f, 0.f };
    float __p2[3] = { 1.f, 1.f, 1.f };
    float __angle = 0.0f;
};
