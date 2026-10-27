#pragma once
#include <vector>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cmath>

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

    // Применение матрицы к точке
    Vec3 transformPoint(const Vec3& p) const {
        Vec4 v = (*this) * Vec4(p, 1.0f);
        if (std::abs(v.w) > 1e-8f)
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

class Polygon {
public:
    std::vector<Vec3> vertices = std::vector<Vec3>{};
    Vec3 normal{ 0.f, 0.f, 1.f };   // по умолчанию — «на нас»
    Polygon() { vertices.clear(); }
    Polygon(std::vector<Vec3> vertices) { this->vertices = vertices; }

    void computeNormal() {
        if (vertices.size() < 3) normal = Vec3(0.f, 0.f, 1.f);

        float nx = 0.f, ny = 0.f, nz = 0.f;
        const std::size_t k = vertices.size();
        for (std::size_t i = 0; i < k; ++i) {
            const Vec3& a = vertices[i];
            const Vec3& b = vertices[(i + 1) % k];
            nx += (a.y - b.y) * (a.z + b.z);
            ny += (a.z - b.z) * (a.x + b.x);
            nz += (a.x - b.x) * (a.y + b.y);
        }
        normal = normalize(Vec3(nx, ny, nz));
    }
};

class Polyhedron {
public:
    std::vector<Polygon> sides = std::vector<Polygon>{};
    Polyhedron() { sides.clear(); }
    Polyhedron(std::vector<Polygon> sides) { this->sides = sides; }
};

std::optional<Polyhedron> loadObj(const std::string& path);
bool saveObj(const Polyhedron& poly, const std::string& path);