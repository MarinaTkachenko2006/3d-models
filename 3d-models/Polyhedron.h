#pragma once
#include <vector>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib> 

struct Vec3 {
	float x = 0, y = 0, z = 0;
	Vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
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