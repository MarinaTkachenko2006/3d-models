#include "Polyhedron.h"
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <cstdio>

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
    std::vector<ImVec2> texcoords;   // vt 

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
            float u = 0, v = 0;
            if (ss >> u) { ss >> v; texcoords.emplace_back(u, v); }
        }
        else if (tag == "f") {
            std::vector<Vec3> fv;
            std::vector<ImVec2> fuv;
            std::vector<long>  fvn;   // -1, если у вершины нет нормали
            bool bad = false;
            bool allHaveUV = true;
            std::string tok;

            while (ss >> tok) {
                FaceVertexRef ref;
                if (!parseFaceVertex(tok, positions.size(), texcoords.size(),
                    normals.size(), ref)) {
                    bad = true;
                    break;
                }
                fv.push_back(positions[ref.v]);
                if (ref.vt >= 0 && ref.vt < (long)texcoords.size())
                    fuv.push_back(texcoords[ref.vt]);
                else
                    allHaveUV = false;
                fvn.push_back(ref.vn);
            }

            if (bad || fv.size() < 3) continue;

            Polygon p;
            p.vertices = std::move(fv);

            // UV сохраняем, только если они есть у всех вершин грани
            // (частичные UV для грани не имеют смысла).
            if (allHaveUV && fuv.size() == p.vertices.size())
                p.uvs = std::move(fuv);

            // Нормаль — как раньше
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
    // ---- Уникальные позиции ----
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

    // ---- Уникальные нормали ----
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

    // ---- Уникальные текстурные координаты ----
    std::vector<ImVec2> texcoords;
    std::unordered_map<std::uint64_t, std::size_t> tIndex;
    auto internT = [&](const ImVec2& t) -> std::size_t {
        std::uint32_t bu, bv;
        std::memcpy(&bu, &t.x, sizeof(bu));
        std::memcpy(&bv, &t.y, sizeof(bv));
        std::uint64_t k = ((std::uint64_t)bu << 32) | (std::uint64_t)bv;
        auto it = tIndex.find(k);
        if (it != tIndex.end()) return it->second;
        std::size_t id = texcoords.size();
        texcoords.push_back(t);
        tIndex.emplace(k, id);
        return id;
        };

    // ---- Грани ----
    // vt[i] == -1 означает, что у i-й вершины грани нет текстурной координаты.
    struct FaceRecord {
        std::vector<std::size_t> v;
        std::vector<long>        vt;
        std::size_t              n;
    };
    std::vector<FaceRecord> faces;
    faces.reserve(poly.sides.size());

    for (const auto& side : poly.sides) {
        if (side.vertices.size() < 3) continue;

        const bool hasUV = (side.uvs.size() == side.vertices.size());

        FaceRecord fr;
        fr.v.reserve(side.vertices.size());
        fr.vt.reserve(side.vertices.size());
        for (std::size_t i = 0; i < side.vertices.size(); ++i) {
            fr.v.push_back(internV(side.vertices[i]));
            fr.vt.push_back(hasUV ? (long)internT(side.uvs[i]) : -1L);
        }
        fr.n = internN(side.normal);
        faces.push_back(std::move(fr));
    }

    if (faces.empty()) return false;

    // ---- Запись файла ----
    std::ofstream out(path);
    if (!out) return false;

    char buf[128];
    for (const auto& p : positions) {
        std::snprintf(buf, sizeof(buf), "v %.9g %.9g %.9g\n", p.x, p.y, p.z);
        out << buf;
    }
    for (const auto& t : texcoords) {
        std::snprintf(buf, sizeof(buf), "vt %.9g %.9g\n", t.x, t.y);
        out << buf;
    }
    for (const auto& n : normals) {
        std::snprintf(buf, sizeof(buf), "vn %.9g %.9g %.9g\n", n.x, n.y, n.z);
        out << buf;
    }

    for (const auto& fr : faces) {
        out << 'f';
        for (std::size_t i = 0; i < fr.v.size(); ++i) {
            const std::size_t vid = fr.v[i] + 1;
            const std::size_t nid = fr.n + 1;
            if (fr.vt[i] >= 0) {
                const std::size_t tid = (std::size_t)fr.vt[i] + 1;
                std::snprintf(buf, sizeof(buf), " %zu/%zu/%zu", vid, tid, nid);
            }
            else {
                std::snprintf(buf, sizeof(buf), " %zu//%zu", vid, nid);
            }
            out << buf;
        }
        out << '\n';
    }

    return static_cast<bool>(out);
}


// Ориентация граней «наружу».
// 1) Склеиваем совпадающие вершины и строим смежность граней по рёбрам.
// 2) Обходом в ширину согласуем направление обхода соседних граней: общее ребро
//    две соседние грани должны проходить в противоположных направлениях.
// 3) Для каждой связной части считаем ориентированный объём относительно центра
//    объекта (сумма смешанных произведений по треугольникам). Отрицательный объём
//    означает, что все нормали части смотрят внутрь — разворачиваем её целиком.
// Работает для любых замкнутых тел (в том числе невыпуклых), а не только для
// выпуклых, где хватило бы сравнения с направлением «от центра».
void Polyhedron::orientOutward() {
    const int F = (int)sides.size();
    if (F == 0) return;

    // --- 1. Индексы вершин ---
    std::unordered_map<Vec3Key, int, Vec3KeyHash> ids;
    std::vector<std::vector<int>> fv(F);
    for (int f = 0; f < F; ++f) {
        fv[f].reserve(sides[f].vertices.size());
        for (const auto& v : sides[f].vertices) {
            // +0.f превращает -0 в +0, чтобы совпадающие точки дали один ключ
            auto key = keyOf(Vec3(v.x + 0.f, v.y + 0.f, v.z + 0.f));
            auto it = ids.find(key);
            if (it == ids.end()) it = ids.emplace(key, (int)ids.size()).first;
            fv[f].push_back(it->second);
        }
    }

    // --- 2. Рёбра -> грани ---
    struct EdgeUse { int face; bool forward; };   // forward: ребро пройдено от меньшего индекса к большему
    auto edgeKey = [](int a, int b) -> std::uint64_t {
        if (a > b) std::swap(a, b);
        return ((std::uint64_t)(std::uint32_t)a << 32) | (std::uint32_t)b;
    };
    std::unordered_map<std::uint64_t, std::vector<EdgeUse>> edges;
    for (int f = 0; f < F; ++f) {
        const int n = (int)fv[f].size();
        for (int i = 0; i < n; ++i) {
            int a = fv[f][i], b = fv[f][(i + 1) % n];
            if (a == b) continue;
            edges[edgeKey(a, b)].push_back({ f, a < b });
        }
    }

    const Vec3 c = center();
    Vec3 lo = sides[0].vertices.empty() ? c : sides[0].vertices[0], hi = lo;
    for (const auto& side : sides)
        for (const auto& v : side.vertices) {
            lo = Vec3(std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z));
            hi = Vec3(std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z));
        }
    const double diag = length(hi - lo);
    const double volumeEps = 1e-6 * diag * diag * diag;

    // --- 3. Согласование обхода и знак объёма ---
    std::vector<char> visited(F, 0), flipped(F, 0);
    std::vector<int> component, queue;
    for (int seed = 0; seed < F; ++seed) {
        if (visited[seed]) continue;
        component.clear();
        queue.assign(1, seed);
        visited[seed] = 1;

        while (!queue.empty()) {
            const int f = queue.back();
            queue.pop_back();
            component.push_back(f);

            const int n = (int)fv[f].size();
            for (int i = 0; i < n; ++i) {
                int a = fv[f][i], b = fv[f][(i + 1) % n];
                if (a == b) continue;
                const auto& uses = edges[edgeKey(a, b)];
                if (uses.size() != 2) continue;          // край поверхности или «неправильное» ребро
                const bool fwd = ((a < b) != (bool)flipped[f]);
                for (const auto& u : uses) {
                    if (u.face == f || visited[u.face]) continue;
                    // сосед должен проходить ребро в обратную сторону
                    flipped[u.face] = (u.forward == fwd);
                    visited[u.face] = 1;
                    queue.push_back(u.face);
                }
            }
        }

        double volume = 0.0;
        for (int f : component) {
            const auto& vs = sides[f].vertices;
            double part = 0.0;
            for (std::size_t i = 1; i + 1 < vs.size(); ++i)
                part += dot(vs[0] - c, cross(vs[i] - c, vs[i + 1] - c));
            volume += flipped[f] ? -part : part;
        }
        if (volume < -volumeEps)
            for (int f : component) flipped[f] = !flipped[f];
    }

    // --- 4. Применяем и пересчитываем нормали ---
    for (int f = 0; f < F; ++f) {
        if (flipped[f]) std::reverse(sides[f].vertices.begin(), sides[f].vertices.end());
        sides[f].computeNormal();
    }
}
