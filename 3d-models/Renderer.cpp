#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace Render {

    namespace {

        // Палитра jet по t ∈ [0,1]: синий → голубой → зелёный → жёлтый → красный
        uint32_t HeatColor(double t) {
            t = std::clamp(t, 0.0, 1.0);
            float r = (float)std::clamp(2.0 - std::fabs(4.0 * t - 3.0), 0.0, 1.0);
            float g = (float)std::clamp(2.0 - std::fabs(4.0 * t - 2.0), 0.0, 1.0);
            float b = (float)std::clamp(2.0 - std::fabs(4.0 * t - 1.0), 0.0, 1.0);
            return 0xFF000000u | ((uint32_t)(255 * r) << 16) | ((uint32_t)(255 * g) << 8) | (uint32_t)(255 * b);
        }

        // Flat shading по нормали (мировые)
        uint32_t ShadeByNormal(const Vec3& n) {
            Vec3 l = normalize(Vec3(0.4f, -0.6f, 0.7f));
            float dot = std::fabs(n.x * l.x + n.y * l.y + n.z * l.z);
            float shade = std::clamp(0.25f + 0.75f * dot, 0.0f, 1.0f);
            uint8_t v = (uint8_t)(255 * shade);
            return 0xFF000000u | ((uint32_t)v << 16) | ((uint32_t)v << 8) | (uint32_t)v;
        }

        // Растеризация треугольника: clip → экран, барицентрика, z-тест по NDC z
        void RasterizeTri(PixelBuffer& buf,
            const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c,
            uint32_t color)
        {
            if (a.cw <= 1e-6 || b.cw <= 1e-6 || c.cw <= 1e-6) return;

            float ax = (float)((a.cx / a.cw * 0.5 + 0.5) * buf.Width());
            float ay = (float)((1.0 - (a.cy / a.cw * 0.5 + 0.5)) * buf.Height());
            float bx = (float)((b.cx / b.cw * 0.5 + 0.5) * buf.Width());
            float by = (float)((1.0 - (b.cy / b.cw * 0.5 + 0.5)) * buf.Height());
            float cx = (float)((c.cx / c.cw * 0.5 + 0.5) * buf.Width());
            float cy = (float)((1.0 - (c.cy / c.cw * 0.5 + 0.5)) * buf.Height());

            int x0 = std::max(0, (int)std::floor(std::min({ ax, bx, cx })));
            int x1 = std::min((int)buf.Width() - 1, (int)std::ceil(std::max({ ax, bx, cx })));
            int y0 = std::max(0, (int)std::floor(std::min({ ay, by, cy })));
            int y1 = std::min((int)buf.Height() - 1, (int)std::ceil(std::max({ ay, by, cy })));
            if (x1 < x0 || y1 < y0) return;

            float d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
            if (std::fabs(d) < 1e-6f) return;

            float invWa = 1.0f / (float)a.cw;
            float invWb = 1.0f / (float)b.cw;
            float invWc = 1.0f / (float)c.cw;

            // z/w для вершин (интерполяция zNDC)
            float za_w = (float)(a.cz / a.cw);
            float zb_w = (float)(b.cz / b.cw);
            float zc_w = (float)(c.cz / c.cw);

            float inv = 1.0f / d;

            for (int py = y0; py <= y1; ++py) {
                float fy = (float)py + 0.5f;
                for (int px = x0; px <= x1; ++px) {
                    float fx = (float)px + 0.5f;
                    float w0 = ((by - cy) * (fx - cx) + (cx - bx) * (fy - cy)) * inv;
                    float w1 = ((cy - ay) * (fx - cx) + (ax - cx) * (fy - cy)) * inv;
                    float w2 = 1.0f - w0 - w1;
                    if (w0 < -1e-5f || w1 < -1e-5f || w2 < -1e-5f) continue;

                    float invW = w0 * invWa + w1 * invWb + w2 * invWc;
                    if (invW <= 1e-9f) continue;

                    float zOverW = w0 * za_w + w1 * zb_w + w2 * zc_w;
                    float zNDC = zOverW / invW;         // корректно и для персп., и для аксонометрии
                    buf.PutPixel(px, py, zNDC, color);
                }
            }
        }

        ScreenVertex Project(const Vec4& clip, const Vec3& world) {
            ScreenVertex v;
            v.cx = clip.x; v.cy = clip.y; v.cz = clip.z; v.cw = clip.w;
            v.wx = world.x; v.wy = world.y; v.wz = world.z;
            return v;
        }

    } // namespace

    void Render(PixelBuffer& buf,
        const Polyhedron& poly,
        const Projection& proj,
        const Mat4& modelMatrix,
        Mode mode)
    {
        size_t W = buf.Width(), H = buf.Height();
        if (W == 0 || H == 0 || poly.sides.empty()) return;

        buf.Clear(0xFF1E1818);

        Mat4 VPM = proj.projectionMatrix() * modelMatrix;

        // Проекция всех вершин (clip + world-после-Model)
        std::vector<ScreenVertex> sv;
        std::vector<double> zval;  // z после Model (для heatmap)
        sv.reserve(poly.sides.size() * 4);
        zval.reserve(poly.sides.size() * 4);

        double zminP = std::numeric_limits<double>::infinity();
        double zmaxP = -std::numeric_limits<double>::infinity();

        for (const auto& side : poly.sides) {
            for (const auto& v : side.vertices) {
                Vec3 wv = modelMatrix.transformPoint(v);
                Vec4 clip = proj.projectionMatrix() * Vec4(wv, 1.0f);
                sv.push_back(Project(clip, wv));
                zval.push_back(wv.z);
                zminP = std::min(zminP, (double)wv.z);
                zmaxP = std::max(zmaxP, (double)wv.z);
            }
        }
        if (!(zmaxP > zminP)) zmaxP = zminP + 1e-6;

        if (mode == Mode::Points) {
            uint32_t col = 0xFFFFFFFF;
            for (const auto& v : sv) {
                if (v.cw <= 1e-6) continue;
                int px = (int)((v.cx / v.cw * 0.5 + 0.5) * W);
                int py = (int)((1.0 - (v.cy / v.cw * 0.5 + 0.5)) * H);
                double zNDC = v.cz / v.cw;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        buf.PutPixel(px + dx, py + dy, zNDC, col);
            }
            return;
        }

        size_t base = 0;
        for (const auto& side : poly.sides) {
            const auto& verts = side.vertices;
            size_t n = verts.size();

            if (mode == Mode::Solid) {
                uint32_t col = ShadeByNormal(side.normal);
                for (size_t i = 1; i + 1 < n; ++i)
                    RasterizeTri(buf, sv[base], sv[base + i], sv[base + i + 1], col);
            }
            else if (mode == Mode::Wireframe) {
                uint32_t lc = 0xFF4060A0;
                for (size_t i = 0; i < n; ++i) {
                    size_t j = (i + 1) % n;
                    const auto& a = sv[base + i];
                    const auto& b = sv[base + j];
                    if (a.cw <= 1e-6 || b.cw <= 1e-6) continue;
                    int x0 = (int)((a.cx / a.cw * 0.5 + 0.5) * W);
                    int y0 = (int)((1.0 - (a.cy / a.cw * 0.5 + 0.5)) * H);
                    int x1 = (int)((b.cx / b.cw * 0.5 + 0.5) * W);
                    int y1 = (int)((1.0 - (b.cy / b.cw * 0.5 + 0.5)) * H);
                    double z0 = a.cz / a.cw;
                    double z1 = b.cz / b.cw;
                    buf.DrawLineZ(x0, y0, x1, y1, z0, z1, lc);
                }
            }
            base += n;
        }
    }
}