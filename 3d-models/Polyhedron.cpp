#include "Polyhedron.h"
#include <unordered_map>

namespace {

    struct FaceVertexRef {
        long v = -1;   // индекс позиции (обязательный)
        long vt = -1;   // индекс текстурных координат (может отсутствовать)
        long vn = -1;   // индекс нормали (может отсутствовать)
    };

    // Разбирает токен грани вида "7", "7/3", "7//2", "7/3/2"
    // vCount/vtCount/vnCount — число вершин/текстур/нормалей (для отрицательных индексов)
    // Возвращает false, если токен невалиден (не число, нулевой индекс, лишние '/'),
    // либо если индекс v вышел за пределы ...Count 
    bool parseFaceVertex(const std::string& tok,
        std::size_t vCount,
        std::size_t vtCount,
        std::size_t vnCount,
        FaceVertexRef& out)
    {
        out = {};

        std::string parts[3];
        int count = 0;
        std::size_t start = 0;
        while (true) {
            std::size_t end = tok.find('/', start);
            std::string piece = (end == std::string::npos)
                ? tok.substr(start)
                : tok.substr(start, end - start);
            if (count >= 3) return false;            
            parts[count++] = std::move(piece);
            if (end == std::string::npos) break;
            start = end + 1;
        }

        auto parseOne = [](const std::string& s, std::size_t poolCount) -> long { // разбор фрагмента
            if (s.empty()) return -1;                 // компонента отсутствует
            char* e = nullptr;
            long idx = std::strtol(s.c_str(), &e, 10);
            if (e == s.c_str() || *e != '\0') return -2;   // не число — ошибка
            if (idx > 0) return idx - 1;
            if (idx < 0) return (long)poolCount + idx;
            return -2;                                // 0 недопустим
            };

        long v = parseOne(parts[0], vCount);
        if (v < 0 || v >= (long)vCount) return false;    

        long vt = -1;
        long vn = -1;
        if (count >= 2) {
            vt = parseOne(parts[1], vtCount);
            if (vt == -2) return false;                   
            if (vt >= (long)vtCount) return false;
        }
        if (count >= 3) {
            vn = parseOne(parts[2], vnCount);
            if (vn == -2) return false;
            if (vn >= (long)vnCount) return false;
        }

        out.v = v;
        out.vt = vt;  
        out.vn = vn;
        return true;
    }

} 

std::optional<Polyhedron> loadObj(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;

    std::vector<Vec3> positions;   // v
    std::vector<Vec3> normals;     // vn
                                   // vt — пока не используем

    std::size_t vtCount = 0;

    Polyhedron poly;
    bool hasFace = false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        if (line[first] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x, y, z;
            if (ss >> x >> y >> z) positions.emplace_back(x, y, z);
        }
        else if (tag == "vn") {
            float x, y, z;
            if (ss >> x >> y >> z) normals.emplace_back(x, y, z);
        }
        else if (tag == "vt") {
            // vt не храним в модели, но учитываем для валидации индексов в f.
            float u = 0, v = 0, w = 0;
            if (ss >> u) { ++vtCount; ss >> v; ss >> w; }
        }
        else if (tag == "f") {
            std::vector<Vec3> fv;
            std::vector<long>  fvn;   // -1, если у вершины нет нормали
            bool bad = false;
            std::string tok;

            while (ss >> tok) {
                FaceVertexRef ref;
                if (!parseFaceVertex(tok, positions.size(), vtCount, normals.size(), ref)) {
                    bad = true;
                    break;
                }
                fv.push_back(positions[ref.v]);
                fvn.push_back(ref.vn);
            }

            if (bad || fv.size() < 3) continue;

            Polygon p;
            p.vertices = std::move(fv);

            // Нормаль берём из файла, только если она однозначна для всей грани:
            // у всех вершин есть vn-индекс и он совпадает.
            bool fromFile = !fvn.empty();
            long firstN = -1;
            for (long ni : fvn) {
                if (ni < 0) { fromFile = false; break; }
                if (firstN < 0) firstN = ni;
                else if (ni != firstN) { fromFile = false; break; }
            }

            if (fromFile) p.normal = normalize(normals[firstN]);
            else          p.computeNormal();

            poly.sides.push_back(std::move(p));
            hasFace = true;
        }
        // vp / l / o / g / s / mtllib / usemtl — пропускаем.
    }

    if (!hasFace) return std::nullopt;
    return poly;
}


namespace {
    struct Vec3Key {
        std::uint32_t x, y, z;
        bool operator==(const Vec3Key& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct Vec3KeyHash {
        std::size_t operator()(const Vec3Key& k) const noexcept {
            std::size_t h = 1469598103934665603ull;
            auto mix = [&](std::uint32_t v) {
                h ^= v; h *= 1099511628211ull;
                };
            mix(k.x); mix(k.y); mix(k.z);
            return h;
        }
    };

    Vec3Key keyOf(const Vec3& v) {
        std::uint32_t bx, by, bz;
        std::memcpy(&bx, &v.x, sizeof(bx));
        std::memcpy(&by, &v.y, sizeof(by));
        std::memcpy(&bz, &v.z, sizeof(bz));
        return { bx, by, bz };
    }

}


bool saveObj(const Polyhedron& poly, const std::string& path) {
    // Уникальные позиции
    std::vector<Vec3> positions;
    std::unordered_map<Vec3Key, std::size_t, Vec3KeyHash> vIndex;
    auto internV = [&](const Vec3& v) -> std::size_t {
        auto k = keyOf(v);
        auto it = vIndex.find(k);
        if (it != vIndex.end()) return it->second;
        std::size_t id = positions.size();
        positions.push_back(v);
        vIndex.emplace(k, id);
        return id;
        };

    // Уникальные нормали
    std::vector<Vec3> normals;
    std::unordered_map<Vec3Key, std::size_t, Vec3KeyHash> nIndex;
    auto internN = [&](const Vec3& n) -> std::size_t {
        auto k = keyOf(n);
        auto it = nIndex.find(k);
        if (it != nIndex.end()) return it->second;
        std::size_t id = normals.size();
        normals.push_back(n);
        nIndex.emplace(k, id);
        return id;
        };

    // Записываем грани
    struct FaceRecord {
        std::vector<std::size_t> v;   
        std::size_t n;     
    };
    std::vector<FaceRecord> faces;
    faces.reserve(poly.sides.size());

    for (const auto& side : poly.sides) {
        if (side.vertices.size() < 3) continue;
        FaceRecord fr;
        fr.v.reserve(side.vertices.size());
        for (const auto& vert : side.vertices)
            fr.v.push_back(internV(vert));
        fr.n = internN(side.normal);
        faces.push_back(std::move(fr));
    }

    if (faces.empty()) return false;

    // Пишем файл
    std::ofstream out(path);
    if (!out) return false;

    char buf[96];
    for (const auto& p : positions) {
        std::snprintf(buf, sizeof(buf), "v %.9g %.9g %.9g\n", p.x, p.y, p.z);
        out << buf;
    }
    for (const auto& n : normals) {
        std::snprintf(buf, sizeof(buf), "vn %.9g %.9g %.9g\n", n.x, n.y, n.z);
        out << buf;
    }
    for (const auto& fr : faces) {
        out << "f";
        for (std::size_t vid : fr.v)
            out << ' ' << (vid + 1) << "//" << (fr.n + 1);
        out << '\n';
    }

    return static_cast<bool>(out);
}