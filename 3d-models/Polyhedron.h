#pragma once
#include <vector>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "Constants.h"

// Структура, описывающая вектор длины 3
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
};

// Структура, описывающая вектор длины 4 (однородные координаты)
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
    Vec4() = default;
    Vec4(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) {}
    Vec4(const Vec3& v, float w = 1.f) : x(v.x), y(v.y), z(v.z), w(w) {}
};

// Структура, описывающая класс матрицы размера 4x4
struct Mat4 {
    float mrx[4][4]; // Сама матрица

    // Конструктор (по умолчанию создаётся единичная матрица)
    Mat4() {
        for (int r(0); r < 4; ++r)
            for (int c(0); c < 4; ++c)
                mrx[r][c] = (r == c) ? 1.f : 0.f;
    }

    // Создание единичной матрицы
    static Mat4 identity() {
        return Mat4();
    }

    // Создание матрицы переноса
    static Mat4 translation(float tx, float ty, float tz) {
        Mat4 result;
        result.mrx[0][3] = tx;
        result.mrx[1][3] = ty;
        result.mrx[2][3] = tz;
        return result;
    }

    // Получение матрицы поворота вокруг оси X
    static Mat4 rotationX(float rad) {
        Mat4 result;
        float c(std::cos(rad)), s(std::sin(rad));
        result.mrx[1][1] = c;  result.mrx[1][2] = -s;
        result.mrx[2][1] = s;  result.mrx[2][2] = c;
        return result;
    }

    // Получение матрицы поворота вокруг оси Y
    static Mat4 rotationY(float rad) {
        Mat4 result;
        float c(std::cos(rad)), s(std::sin(rad));
        result.mrx[0][0] = c;   result.mrx[0][2] = s;
        result.mrx[2][0] = -s;  result.mrx[2][2] = c;
        return result;
    }

    // Получение матрицы поворота вокруг оси Z
    static Mat4 rotationZ(float rad) {
        Mat4 result;
        float c(std::cos(rad)), s(std::sin(rad));
        result.mrx[0][0] = c;  result.mrx[0][1] = -s;
        result.mrx[1][0] = s;  result.mrx[1][1] = c;
        return result;
    }

    // Создание матрицы масштабирования (относительно начала координат).
    // Отрицательный коэффициент даёт отражение по соответствующей оси.
    static Mat4 scaling(float sx, float sy, float sz) {
        Mat4 result;
        result.mrx[0][0] = sx;
        result.mrx[1][1] = sy;
        result.mrx[2][2] = sz;
        return result;
    }

    // Получение матрицы поворота вокруг произвольной оси, проходящей через начало координат.
    // (l, m, n) — направляющий вектор оси (нормализуется внутри)
    static Mat4 rotationAxis(float l, float m, float n, float rad) {
        Mat4 result;
        float len(std::sqrt(l * l + m * m + n * n));
        if (len < 1e-8f) return result;
        l /= len; m /= len; n /= len;

        float c(std::cos(rad)), s(std::sin(rad)), t(1.f - c);
        result.mrx[0][0] = l * l * t + c;      result.mrx[0][1] = l * m * t - n * s;  result.mrx[0][2] = l * n * t + m * s;
        result.mrx[1][0] = l * m * t + n * s;  result.mrx[1][1] = m * m * t + c;      result.mrx[1][2] = m * n * t - l * s;
        result.mrx[2][0] = l * n * t - m * s;  result.mrx[2][1] = m * n * t + l * s;  result.mrx[2][2] = n * n * t + c;
        return result;
    }

    // Произведение матриц
    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int i(0); i < 4; ++i) {
            for (int j(0); j < 4; ++j) {
                result.mrx[i][j] = 0;
                for (int k(0); k < 4; ++k)
                    result.mrx[i][j] += mrx[i][k] * other.mrx[k][j];
            }
        }
        return result;
    }

    // Применение матрицы к вершине в однородных координатах
    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            mrx[0][0] * v.x + mrx[0][1] * v.y + mrx[0][2] * v.z + mrx[0][3] * v.w,
            mrx[1][0] * v.x + mrx[1][1] * v.y + mrx[1][2] * v.z + mrx[1][3] * v.w,
            mrx[2][0] * v.x + mrx[2][1] * v.y + mrx[2][2] * v.z + mrx[2][3] * v.w,
            mrx[3][0] * v.x + mrx[3][1] * v.y + mrx[3][2] * v.z + mrx[3][3] * v.w);
    }

    // Определитель линейной части (3x3). Отрицательный — преобразование
    // содержит отражение и меняет ориентацию обхода граней.
    float det3() const {
        return mrx[0][0] * (mrx[1][1] * mrx[2][2] - mrx[1][2] * mrx[2][1])
             - mrx[0][1] * (mrx[1][0] * mrx[2][2] - mrx[1][2] * mrx[2][0])
             + mrx[0][2] * (mrx[1][0] * mrx[2][1] - mrx[1][1] * mrx[2][0]);
    }

    // Применение матрицы к точке
    Vec3 transformPoint(const Vec3& p) const {
        Vec4 v = (*this) * Vec4(p, 1.0f);
        if (std::abs(v.w) > EPSILON)
            return Vec3(v.x / v.w, v.y / v.w, v.z / v.w);
        return Vec3(v.x, v.y, v.z);
    }
};

inline float length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len < 1e-8f) return Vec3(0.f, 0.f, 1.f);
    return Vec3(v.x / len, v.y / len, v.z / len);
}

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, float k) { return Vec3(a.x * k, a.y * k, a.z * k); }

// Скалярное произведение
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Векторное произведение
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

class Polygon {
public:
    std::vector<Vec3> vertices = std::vector<Vec3>{};
    Vec3 normal{ 0.f, 0.f, 1.f };   // по умолчанию — «на нас»
    Polygon() { vertices.clear(); }
    Polygon(std::vector<Vec3> vertices) { this->vertices = vertices; }

    // Нормаль грани — векторное произведение векторов, лежащих в плоскости грани.
    // Берём векторы из 0-й вершины в соседние (v[i]-v[0]) x (v[i+1]-v[0]) и
    // суммируем по всем треугольникам веера: для плоской грани все слагаемые
    // сонаправлены, а сумма устойчива к вырожденным (коллинеарным) тройкам вершин.
    // Направление задаётся порядком обхода: если смотреть на грань снаружи и
    // вершины идут против часовой стрелки, нормаль направлена наружу.
    void computeNormal() {
        normal = Vec3(0.f, 0.f, 1.f);
        const std::size_t k = vertices.size();
        if (k < 3) return;

        Vec3 sum;
        for (std::size_t i = 1; i + 1 < k; ++i)
            sum = sum + cross(vertices[i] - vertices[0], vertices[i + 1] - vertices[0]);
        normal = normalize(sum);
    }

    // Центр грани — среднее арифметическое её вершин
    Vec3 center() const {
        Vec3 c;
        if (vertices.empty()) return c;
        for (const auto& v : vertices) c = c + v;
        return c * (1.f / (float)vertices.size());
    }
};

class Polyhedron {
public:
    std::vector<Polygon> sides = std::vector<Polygon>{};
    Polyhedron() { sides.clear(); }
    Polyhedron(std::vector<Polygon> sides) { this->sides = sides; }

    // Центр многогранника — среднее арифметическое вершин граней
    Vec3 center() const {
        Vec3 c;
        std::size_t count = 0;
        for (const auto& side : sides) {
            for (const auto& v : side.vertices) {
                c.x += v.x; c.y += v.y; c.z += v.z;
                ++count;
            }
        }
        if (count == 0) return c;
        return Vec3(c.x / count, c.y / count, c.z / count);
    }

    // Применение матрицы ко всем вершинам (с пересчётом нормалей).
    // Отражение (det < 0) меняет направление обхода на противоположное,
    // поэтому порядок вершин разворачиваем — нормали остаются внешними.
    void transform(const Mat4& M) {
        const bool mirrored = M.det3() < 0.f;
        for (auto& side : sides) {
            for (auto& v : side.vertices)
                v = M.transformPoint(v);
            if (mirrored) std::reverse(side.vertices.begin(), side.vertices.end());
            side.computeNormal();
        }
    }

    // Согласовать обход всех граней и пересчитать нормали так, чтобы они
    // были направлены наружу относительно объекта.
    void orientOutward();
};

std::optional<Polyhedron> loadObj(const std::string& path);
bool saveObj(const Polyhedron& poly, const std::string& path);