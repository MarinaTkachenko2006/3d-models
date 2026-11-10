#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include <imgui.h>
#include <cmath>
#include <numbers>
#include <algorithm>

// Орбитальная камера: смотрит на target с расстояния distance,
// ориентация задаётся yaw/pitch (в градусах).
struct Camera {
    Vec3 target{ 0.f, 0.f, 0.f };
    float distance = 6.0f;     // расстояние до объекта
    float yaw = 35.0f;         // вокруг Y (градусы)
    float pitch = 25.0f;       // вверх/вниз (градусы), клампим [-89,89]
    float fovDeg = 50.0f;      // угол обзора по вертикали
    float nearZ = 0.1f;
    float farZ = 100.0f;

    // Позиция камеры в мире
    Vec3 position() const {
        constexpr float D2R = std::numbers::pi_v<float> / 180.f;
        float y = yaw * D2R, p = pitch * D2R;
        float cp = std::cos(p), sp = std::sin(p);
        float cy = std::cos(y), sy = std::sin(y);
        // Сферические координаты: ось Y — вверх
        Vec3 dir{ cp * sy, sp, cp * cy };
        return Vec3(target.x + dir.x * distance,
            target.y + dir.y * distance,
            target.z + dir.z * distance);
    }

    // Вектор обзора: единичный вектор от камеры к точке target.
    Vec3 viewDirection() const {
        return normalize(target - position());
    }

    // Задать вектор обзора напрямую (пересчитывается в yaw/pitch).
    void setViewDirection(const Vec3& d) {
        if (length(d) < 1e-6f) return;
        constexpr float R2D = 180.f / std::numbers::pi_v<float>;
        const Vec3 n = normalize(d);
        pitch = std::clamp(std::asin(std::clamp(-n.y, -1.f, 1.f)) * R2D, -89.f, 89.f);
        if (std::fabs(n.x) > 1e-6f || std::fabs(n.z) > 1e-6f)
            yaw = std::atan2(-n.x, -n.z) * R2D;
    }

    // View-матрица (world -> camera space). Камера смотрит вдоль -Z.
    Mat4 viewMatrix() const {
        Vec3 eye = position();
        Vec3 f = normalize(Vec3(target.x - eye.x, target.y - eye.y, target.z - eye.z)); // forward
        Vec3 up{ 0.f, 1.f, 0.f };
        // Если смотрим почти вертикально — берём другой up
        if (std::fabs(f.y) > 0.999f) up = Vec3(0.f, 0.f, 1.f);
        Vec3 s = normalize(Vec3(f.y * up.z - f.z * up.y,
            f.z * up.x - f.x * up.z,
            f.x * up.y - f.y * up.x)); // right
        Vec3 u = Vec3(s.y * f.z - s.z * f.y,
            s.z * f.x - s.x * f.z,
            s.x * f.y - s.y * f.x);          // up

        Mat4 R;  // rotation
        R.mrx[0][0] = s.x; R.mrx[0][1] = s.y; R.mrx[0][2] = s.z;
        R.mrx[1][0] = u.x; R.mrx[1][1] = u.y; R.mrx[1][2] = u.z;
        R.mrx[2][0] = -f.x; R.mrx[2][1] = -f.y; R.mrx[2][2] = -f.z;
        // T(-eye) * R
        Mat4 T = Mat4::translation(-eye.x, -eye.y, -eye.z);
        return R * T;
    }

    // Перспективная матрица проекции с near/far и fov.
    Mat4 projectionMatrix(float aspect) const {
        constexpr float D2R = std::numbers::pi_v<float> / 180.f;
        float f = 1.f / std::tan(fovDeg * D2R * 0.5f);
        Mat4 P;
        P.mrx[0][0] = f / aspect;
        P.mrx[1][1] = f;
        P.mrx[2][2] = (farZ + nearZ) / (nearZ - farZ);
        P.mrx[2][3] = (2.f * farZ * nearZ) / (nearZ - farZ);
        P.mrx[3][2] = -1.f;
        P.mrx[3][3] = 0.f;
        return P;
    }

    // Матрица параллельной (ортографической) проекции вдоль вектора обзора.
    // Размер видимой области подобран так, чтобы масштаб объекта совпадал
    // с перспективой на расстоянии distance (Distance работает как zoom).
    Mat4 orthoMatrix(float aspect) const {
        constexpr float D2R = std::numbers::pi_v<float> / 180.f;
        const float halfH = std::max(1e-3f, distance * std::tan(fovDeg * D2R * 0.5f));
        const float halfW = halfH * aspect;
        Mat4 P;
        P.mrx[0][0] = 1.f / halfW;
        P.mrx[1][1] = 1.f / halfH;
        P.mrx[2][2] = -1.f / farZ;   // ближе к наблюдателю — меньше z (как в Z-буфере)
        return P;
    }

    void drawParamsUI() {
        // Вектор обзора: можно менять напрямую или через Yaw/Pitch
        Vec3 dir = viewDirection();
        if (ImGui::DragFloat3("View vector", &dir.x, 0.01f, -1.f, 1.f, "%.3f"))
            setViewDirection(dir);
        ImGui::DragFloat3("Target", &target.x, 0.05f, -50.f, 50.f);
        ImGui::SliderFloat("Distance", &distance, 0.5f, 50.f);
        ImGui::SliderFloat("Yaw (deg)", &yaw, -180.f, 180.f);
        ImGui::SliderFloat("Pitch (deg)", &pitch, -89.f, 89.f);
        ImGui::SliderFloat("FOV (deg)", &fovDeg, 20.f, 110.f);
        ImGui::DragFloat("Near", &nearZ, 0.01f, 0.01f, 5.f);
        ImGui::DragFloat("Far", &farZ, 0.1f, 5.f, 500.f);
        if (farZ <= nearZ) farZ = nearZ + 1.f;
        // Кнопка «сброс»
        if (ImGui::Button("Reset camera")) *this = Camera{};
    }
};