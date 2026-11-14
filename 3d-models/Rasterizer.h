#pragma once
#include "RenderPipeline.h"
#include "RenderTarget.h"
#include <algorithm>
#include <cmath>

namespace Render {

    // Шейдер получает контекст (userdata), барицентрику, инверсии w и UV.
    using FragmentShader = std::uint32_t(*)(const ScreenTri& tri,
        double l0, double l1, double l2,
        double invW0, double invW1, double invW2,
        double u, double v,
        void* userdata);

    inline void rasterizeTriangle(const ScreenTri& t,
        RenderTarget& rt,
        int viewportX, int viewportY,
        int viewportW, int viewportH,
        FragmentShader shader,
        void* userdata = nullptr)
    {
        double x0 = t.x[0], y0 = t.y[0];
        double x1 = t.x[1], y1 = t.y[1];
        double x2 = t.x[2], y2 = t.y[2];
        double z0 = t.z[0], z1 = t.z[1], z2 = t.z[2];

        int minX = (int)std::floor(std::min({ x0, x1, x2 }));
        int maxX = (int)std::ceil(std::max({ x0, x1, x2 }));
        int minY = (int)std::floor(std::min({ y0, y1, y2 }));
        int maxY = (int)std::ceil(std::max({ y0, y1, y2 }));
        minX = std::max(minX, viewportX);
        maxX = std::min(maxX, viewportX + viewportW - 1);
        minY = std::max(minY, viewportY);
        maxY = std::min(maxY, viewportY + viewportH - 1);
        if (minX > maxX || minY > maxY) return;

        double area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        if (std::abs(area) < 1e-12) return;
        double invArea = 1.0 / area;

        double iw0 = t.invW[0], iw1 = t.invW[1], iw2 = t.invW[2];

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                double fx = px + 0.5;
                double fy = py + 0.5;

                double w0 = ((x1 - fx) * (y2 - fy) - (x2 - fx) * (y1 - fy)) * invArea;
                double w1 = ((x2 - fx) * (y0 - fy) - (x0 - fx) * (y2 - fy)) * invArea;
                double w2 = 1.0 - w0 - w1;

                bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                    (w0 <= 0 && w1 <= 0 && w2 <= 0);
                if (!inside) continue;

                double z = w0 * z0 + w1 * z1 + w2 * z2;

                std::size_t idx = (std::size_t)py * rt.width + px;
                if (z >= rt.depth[idx]) continue;
                rt.depth[idx] = (float)z;

                double uu = 0.0, vv = 0.0;
                if (t.hasUV) {
                    double iw = w0 * iw0 + w1 * iw1 + w2 * iw2;
                    if (std::abs(iw) > 1e-12) {
                        uu = (w0 * t.u[0] * iw0
                            + w1 * t.u[1] * iw1
                            + w2 * t.u[2] * iw2) / iw;
                        vv = (w0 * t.v[0] * iw0
                            + w1 * t.v[1] * iw1
                            + w2 * t.v[2] * iw2) / iw;
                    }
                }

                rt.color[idx] = shader(t, w0, w1, w2, iw0, iw1, iw2, uu, vv, userdata);
            }
        }
    }


    inline void rasterizeLine(double x0, double y0, double z0,
        double x1, double y1, double z1,
        RenderTarget& rt,
        std::uint32_t color,
        int thickness = 2)
    {
        double dx = x1 - x0;
        double dy = y1 - y0;
        double dz = z1 - z0;

        int steps = (int)std::ceil(std::max(std::abs(dx), std::abs(dy)));
        if (steps <= 0) steps = 1;

        double sx = dx / steps;
        double sy = dy / steps;
        double sz = dz / steps;

        double len = std::sqrt(dx * dx + dy * dy);
        double nx = 0, ny = 0;
        if (len > EPSILON) { nx = -dy / len; ny = dx / len; }

        double x = x0, y = y0, z = z0;
        for (int i = 0; i <= steps; ++i) {
            for (int t = -thickness / 2; t <= thickness / 2; ++t) {
                int px = (int)std::floor(x + nx * t + 0.5);
                int py = (int)std::floor(y + ny * t + 0.5);
                if (px >= 0 && px < rt.width && py >= 0 && py < rt.height) {
                    std::size_t idx = (std::size_t)py * rt.width + px;
                    if (z <= rt.depth[idx] + 1e-4) {
                        rt.color[idx] = color;
                    }
                }
            }
            x += sx; y += sy; z += sz;
        }
    }

}