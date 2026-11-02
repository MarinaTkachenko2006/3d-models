#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <limits>

namespace Render {

    class PixelBuffer {
    private:
        size_t __width = 0, __height = 0;
        std::vector<uint32_t> __color;
        std::vector<double>   __depth;

    public:
        PixelBuffer() = default;
        PixelBuffer(size_t w, size_t h) : __width(w), __height(h),
            __color(w* h, 0xFF000000u),
            __depth(w* h, std::numeric_limits<double>::infinity()) {
        }

        void Resize(size_t w, size_t h) {
            if (w == __width && h == __height) return;
            __width = w; __height = h;
            __color.assign(w * h, 0xFF000000u);
            __depth.assign(w * h, std::numeric_limits<double>::infinity());
        }

        void Clear(uint32_t bg = 0xFF1E1818) {
            std::fill(__color.begin(), __color.end(), bg);
            std::fill(__depth.begin(), __depth.end(), std::numeric_limits<double>::infinity());
        }

        void SetPixel(int x, int y, uint32_t c) {
            if (x < 0 || y < 0) return;
            size_t ux = (size_t)x, uy = (size_t)y;
            if (ux >= __width || uy >= __height) return;
            __color[uy * __width + ux] = c;
        }

        // z-тест по NDC z (ближе = меньше z; +∞ = «далеко»)
        void PutPixel(int x, int y, double zNDC, uint32_t c) {
            if (x < 0 || y < 0) return;
            size_t ux = (size_t)x, uy = (size_t)y;
            if (ux >= __width || uy >= __height) return;
            size_t k = uy * __width + ux;
            if (zNDC < __depth[k]) {
                __depth[k] = zNDC;
                __color[k] = c;
            }
        }

        void DrawLine(int x0, int y0, int x1, int y1, uint32_t c) {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            for (;;) {
                SetPixel(x0, y0, c);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }

        // Линия с z-тестом по NDC z (ближе = меньше z)
        void DrawLineZ(int x0, int y0, int x1, int y1,
            double z0, double z1, uint32_t c) {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            int total = std::max(dx, -dy); if (total == 0) total = 1;
            int step = 0;
            for (;;) {
                double z = z0 + (z1 - z0) * ((double)step / total);
                PutPixel(x0, y0, z, c);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
                ++step;
            }
        }

        size_t Width() const { return __width; }
        size_t Height() const { return __height; }
        uint32_t* Data() { return __color.data(); }
        const uint32_t* Data() const { return __color.data(); }
    };
}