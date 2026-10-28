#include "IGeometrySource.h"

std::optional<Polyhedron> gen_tetrahedron(){
    Polyhedron result = std::vector<Polygon>{
        std::vector<Vec3>{ Vec3(1, 1, 1), Vec3(0, 0, 1), Vec3(0, 1, 0) },
        std::vector<Vec3>{ Vec3(1, 1, 1), Vec3(0, 1, 0), Vec3(1, 0, 0) },
        std::vector<Vec3>{ Vec3(1, 1, 1), Vec3(1, 0, 0), Vec3(0, 0, 1) },
        std::vector<Vec3>{ Vec3(0, 1, 0), Vec3(1, 0, 0), Vec3(0, 0, 1) }
    };
    for (auto& side : result.sides) side.computeNormal();
    return result;
}
std::optional<Polyhedron> gen_hexahedron() { // куб с началом координат в одной из вершин
    Polyhedron result = std::vector<Polygon>{
        std::vector<Vec3>{ Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 1), Vec3(0, 0, 1) },
        std::vector<Vec3>{ Vec3(0, 0, 1), Vec3(0, 1, 1), Vec3(1, 1, 1), Vec3(1, 0, 1) },
        std::vector<Vec3>{ Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(0, 1, 1), Vec3(0, 0, 1) },
        std::vector<Vec3>{ Vec3(0, 0, 0), Vec3(0, 1, 0), Vec3(1, 1, 0), Vec3(1, 0, 0) },
        std::vector<Vec3>{ Vec3(1, 0, 1), Vec3(1, 1, 1), Vec3(1, 1, 0), Vec3(1, 0, 0) },
        std::vector<Vec3>{ Vec3(0, 1, 0), Vec3(1, 1, 0), Vec3(1, 1, 1), Vec3(0, 1, 1) }
    };
    for (auto& side : result.sides) side.computeNormal();
    return result;
}
std::optional<Polyhedron> gen_octahedron() { 
    auto hex_sides = gen_hexahedron()->sides;
    auto vertices = std::vector<Vec3>{};
    for (auto side : hex_sides) { // строим центры граней куба
        Vec3 c = Vec3(0, 0, 0);
        for (auto vertex : side.vertices)
        {
            c.x += vertex.x;
            c.y += vertex.y;
            c.z += vertex.z;
        }
        c.x /= 4;
        c.y /= 4;
        c.z /= 4;
        vertices.push_back(c);
    }
    Polyhedron result = std::vector<Polygon>{
        std::vector<Vec3>{ vertices[0], vertices[2], vertices[1] },
        std::vector<Vec3>{ vertices[0], vertices[3], vertices[2] },
        std::vector<Vec3>{ vertices[0], vertices[4], vertices[3] },
        std::vector<Vec3>{ vertices[0], vertices[1], vertices[4] },
        std::vector<Vec3>{ vertices[5], vertices[1], vertices[2] },
        std::vector<Vec3>{ vertices[5], vertices[2], vertices[3] },
        std::vector<Vec3>{ vertices[5], vertices[3], vertices[4] },
        std::vector<Vec3>{ vertices[5], vertices[4], vertices[1] }
    };
    for (auto& side : result.sides) side.computeNormal();
    return result;
}

std::vector<Vec3> circlePoints(float r, int stepDeg) {
    std::vector<Vec3> pts;
    if (stepDeg <= 0 || 360 % stepDeg != 0) return pts;   

    const int count = 360 / stepDeg;
    const float stepRad = stepDeg * 3.14159265358979323846f / 180.f;

    pts.reserve(count);
    for (int k = 0; k < count; ++k) {
        float phi = k * stepRad;
        pts.emplace_back(r * std::cos(phi), 0.f, r * std::sin(phi));
    }
    return pts;
}

std::optional<Polyhedron> gen_icosahedron() {
    std::vector<Vec3> vertices = circlePoints(1, 360 / 5); // верхний слой (y = 0)
    vertices.push_back(Vec3((vertices[0].x + vertices[1].x) / 2, -1, (vertices[0].z + vertices[1].z) / 2));
    vertices.push_back(Vec3((vertices[1].x + vertices[2].x) / 2, -1, (vertices[1].z + vertices[2].z) / 2));
    vertices.push_back(Vec3((vertices[2].x + vertices[3].x) / 2, -1, (vertices[2].z + vertices[3].z) / 2));
    vertices.push_back(Vec3((vertices[3].x + vertices[4].x) / 2, -1, (vertices[3].z + vertices[4].z) / 2));
    vertices.push_back(Vec3((vertices[4].x + vertices[0].x) / 2, -1, (vertices[4].z + vertices[0].z) / 2));

    float sq = std::sqrt(5);
    vertices.push_back(Vec3(0, (sq - 1) / 2, 0));
    vertices.push_back(Vec3(0, -1-(sq - 1) / 2, 0));

    Polyhedron result = std::vector<Polygon>{
        std::vector<Vec3>{ vertices[0], vertices[5], vertices[1] },
        std::vector<Vec3>{ vertices[1], vertices[5], vertices[6] },
        std::vector<Vec3>{ vertices[1], vertices[6], vertices[2] },
        std::vector<Vec3>{ vertices[2], vertices[6], vertices[7] },
        std::vector<Vec3>{ vertices[2], vertices[7], vertices[3] },
        std::vector<Vec3>{ vertices[3], vertices[7], vertices[8] },
        std::vector<Vec3>{ vertices[3], vertices[8], vertices[4] },
        std::vector<Vec3>{ vertices[4], vertices[8], vertices[9] },
        std::vector<Vec3>{ vertices[4], vertices[9], vertices[0] },
        std::vector<Vec3>{ vertices[0], vertices[9], vertices[5] },
        std::vector<Vec3>{ vertices[5], vertices[11], vertices[6] },
        std::vector<Vec3>{ vertices[6], vertices[11], vertices[7] },
        std::vector<Vec3>{ vertices[7], vertices[11], vertices[8] },
        std::vector<Vec3>{ vertices[8], vertices[11], vertices[9] },
        std::vector<Vec3>{ vertices[9], vertices[11], vertices[5] },
        std::vector<Vec3>{ vertices[10], vertices[0], vertices[1] },
        std::vector<Vec3>{ vertices[10], vertices[1], vertices[2] },
        std::vector<Vec3>{ vertices[10], vertices[2], vertices[3] },
        std::vector<Vec3>{ vertices[10], vertices[3], vertices[4] },
        std::vector<Vec3>{ vertices[10], vertices[4], vertices[0] }
    };
    for (auto& side : result.sides) side.computeNormal();
    return result;
}

std::optional<Polyhedron> gen_dodecahedron() {
    auto hex_sides = gen_icosahedron()->sides;
    auto vertices = std::vector<Vec3>{};
    for (auto side : hex_sides) { 
        Vec3 c = Vec3(0, 0, 0);
        for (auto vertex : side.vertices)
        {
            c.x += vertex.x;
            c.y += vertex.y;
            c.z += vertex.z;
        }
        c.x /= 3;
        c.y /= 3;
        c.z /= 3;
        vertices.push_back(c);
    }

    Polyhedron result = std::vector<Polygon>{
        std::vector<Vec3>{ vertices[15], vertices[16], vertices[17], vertices[18], vertices[19] },
        std::vector<Vec3>{ vertices[10], vertices[11], vertices[12], vertices[13], vertices[14] },

        std::vector<Vec3>{ vertices[16], vertices[15], vertices[0], vertices[1], vertices[2] },
        std::vector<Vec3>{ vertices[17], vertices[16], vertices[2], vertices[3], vertices[4] },
        std::vector<Vec3>{ vertices[18], vertices[17], vertices[4], vertices[5], vertices[6] },
        std::vector<Vec3>{ vertices[19], vertices[18], vertices[6], vertices[7], vertices[8] },
        std::vector<Vec3>{ vertices[15], vertices[19], vertices[8], vertices[9], vertices[0] },

        std::vector<Vec3>{ vertices[10], vertices[11], vertices[3], vertices[2], vertices[1] },
        std::vector<Vec3>{ vertices[11], vertices[12], vertices[5], vertices[4], vertices[3] },
        std::vector<Vec3>{ vertices[12], vertices[13], vertices[7], vertices[6], vertices[5] },
        std::vector<Vec3>{ vertices[13], vertices[14], vertices[9], vertices[8], vertices[7] },
        std::vector<Vec3>{ vertices[14], vertices[10], vertices[1], vertices[0], vertices[9] },
    };
    for (auto& side : result.sides) side.computeNormal();
    return result;
}

std::optional<Polyhedron> gen_regular_polyhedron(int n) {
    switch (n) {
    case 4:  return gen_tetrahedron();
    case 6:  return gen_hexahedron();
    case 8:  return gen_octahedron();
    case 12: return gen_dodecahedron();
    case 20: return gen_icosahedron();
    default: return std::nullopt;
    }
}

#include <array>

namespace {

    constexpr std::array<int, 5> kAllowedFaces = { 4, 6, 8, 12, 20 };

    const char* labelFor(int n) {
        switch (n) {
        case 4:  return "Tetrahedron (4)";
        case 6:  return "Hexahedron (6)";
        case 8:  return "Octahedron (8)";
        case 12: return "Dodecahedron (12)";
        case 20: return "Icosahedron (20)";
        default: return "?";
        }
    }

    bool isAllowed(int n) {
        for (int v : kAllowedFaces) if (v == n) return true;
        return false;
    }

} 

void GenerateRegularPolyhedron::drawParamsUI() {
    for (int n : kAllowedFaces) {
        if (ImGui::RadioButton(labelFor(n), faces_ == n))
            faces_ = n;
    }
}

std::optional<Polyhedron> GenerateRegularPolyhedron::build() {
    if (!isAllowed(faces_)) return std::nullopt;
    return gen_regular_polyhedron(faces_);
}


bool RevolutionSource::parseProfile(std::vector<std::pair<float, float>>& out,
    int& errorLine, std::string& errorMsg) const
{
    out.clear();
    errorLine = 0;
    errorMsg.clear();

    std::istringstream stream(profileBuffer_);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        ++lineNum;

        // Парсим два числа: x и y
        std::istringstream ls(line);
        float x, y;
        if (!(ls >> x >> y)) {
            errorLine = lineNum;
            errorMsg = "Expected two numbers (x y)";
            return false;
        }

        // Проверяем, что после второго числа нет мусора
        std::string extra;
        if (ls >> extra) {
            errorLine = lineNum;
            errorMsg = "Too many values on line";
            return false;
        }

        // Радиус не может быть отрицательным (или берем по модулю)
        // Лучше выдать ошибку, чтобы пользователь понимал, что не так.
        if (x < 0.f) {
            errorLine = lineNum;
            errorMsg = "Radius (x) cannot be negative";
            return false;
        }

        out.emplace_back(x, y);
    }

    if (out.size() < 2) {
        errorLine = 0;
        errorMsg = "Need at least 2 points to form a surface";
        return false;
    }

    return true;
}

void RevolutionSource::drawParamsUI() {
    // Ввод образующей
    ImGui::TextUnformatted("Profile points (x y per line, # for comment):");
    ImGui::InputTextMultiline("##profile", profileBuffer_, sizeof(profileBuffer_),
        ImVec2(-1.0f, ImGui::GetTextLineHeight() * 6.0f));

    // Валидация
    std::vector<std::pair<float, float>> parsedProfile;
    int errLine = 0;
    std::string errMsg;
    bool valid = parseProfile(parsedProfile, errLine, errMsg);

    if (!valid) {
        if (errLine > 0)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Line %d: %s", errLine, errMsg.c_str());
        else
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", errMsg.c_str());
    }
    else {
        ImGui::TextDisabled("%zu points parsed.", parsedProfile.size());
    }

    // Ось вращения
    ImGui::TextUnformatted("Axis of rotation:");
    ImGui::RadioButton("X", &axis_, 0); ImGui::SameLine();
    ImGui::RadioButton("Y", &axis_, 1); ImGui::SameLine();
    ImGui::RadioButton("Z", &axis_, 2);

    // Число разбиений
    ImGui::DragInt("Subdivisions", &subdivisions_, 1.0f, 3, 256);
}

std::optional<Polyhedron> RevolutionSource::build() {
    std::vector<std::pair<float, float>> profile;
    int errLine = 0;
    std::string errMsg;

    // Если ввод невалиден - возвращаем nullopt
    if (!parseProfile(profile, errLine, errMsg)) {
        return std::nullopt;
    }

    const int N = subdivisions_;
    const float PI = 3.14159265358979323846f;

    std::vector<Vec3> positions;
    std::vector<std::vector<int>> ring(profile.size());

    // Генерируем вершины
    for (size_t i = 0; i < profile.size(); ++i) {
        float u = profile[i].first;
        float v = profile[i].second;

        if (std::abs(u) < 1e-6f) {
            // Полюс (вершина на оси) - создаем одну точку
            int idx = (int)positions.size();

            // Координаты зависят от оси (v - вдоль оси, u - радиус = 0)
            if (axis_ == 0)      positions.emplace_back(v, 0.f, 0.f); // X
            else if (axis_ == 1) positions.emplace_back(0.f, v, 0.f); // Y
            else                 positions.emplace_back(0.f, 0.f, v); // Z
            
            // Но сюда копируем N раз - для однородной структуры колец
            ring[i].assign(N, idx);
        }
        else {
            // Кольцо из N точек
            ring[i].resize(N);
            for (int j = 0; j < N; ++j) {
                float theta = j * 2.f * PI / (float)N;
                float c = std::cos(theta);
                float s = std::sin(theta);

                int idx = (int)positions.size();
                if (axis_ == 0)      positions.emplace_back(v, u * c, u * s); // X
                else if (axis_ == 1) positions.emplace_back(u * c, v, u * s); // Y
                else                 positions.emplace_back(u * c, u * s, v); // Z

                ring[i][j] = idx;
            }
        }
    }

    // Генерируем грани
    Polyhedron result;
    // перебираем пары колец, в одну грань входит многоугольник с двумя точками на кольце i и двумя точками на кольце i + 1
    for (size_t i = 0; i + 1 < profile.size(); ++i) {
        for (int j = 0; j < N; ++j) {
            int j2 = (j + 1) % N;
            int a = ring[i][j], b = ring[i][j2];
            int c = ring[i + 1][j2], d = ring[i + 1][j];

            // Убираем дубликаты вершин (для полюсов)
            std::vector<int> face;
            for (int id : {a, b, c, d}) {
                if (face.empty() || face.back() != id) face.push_back(id);
            }
            if (face.size() > 1 && face.front() == face.back()) face.pop_back();

            if (face.size() < 3) continue;

            Polygon p;
            for (int id : face) p.vertices.push_back(positions[id]);
            p.computeNormal();
            result.sides.push_back(std::move(p));
        }
    }

    // Если фигура пустая (например, профиль не дал ни одной грани)
    if (result.sides.empty()) return std::nullopt;

    return result;
}