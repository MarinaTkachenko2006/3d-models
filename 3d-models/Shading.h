#pragma once
#include "RenderPipeline.h"
#include "Rasterizer.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Render {

    struct TextureSampler {
        const std::uint32_t* pixels = nullptr;
        int width = 0;
        int height = 0;
        bool wrap = true;
        bool bilinear = true;
    };

    inline std::uint32_t sampleTexture(const TextureSampler& t, double u, double v) {
        if (!t.pixels || t.width <= 0 || t.height <= 0) return 0xFFFFFFFFu;

        if (t.wrap) {
            u = u - std::floor(u);
            v = v - std::floor(v);
        }
        else {
            u = std::clamp(u, 0.0, 1.0);
            v = std::clamp(v, 0.0, 1.0);
        }

        double x = u * t.width - 0.5;
        double y = v * t.height - 0.5;

        if (!t.bilinear) {
            int xi = (int)std::floor(x + 0.5);
            int yi = (int)std::floor(y + 0.5);
            xi = std::clamp(xi, 0, t.width - 1);
            yi = std::clamp(yi, 0, t.height - 1);
            return t.pixels[(std::size_t)yi * t.width + xi];
        }

        int x0 = (int)std::floor(x);
        int y0 = (int)std::floor(y);
        double fx = x - x0;
        double fy = y - y0;
        int x1 = x0 + 1, y1 = y0 + 1;

        auto cx = [&](int xx) { return std::clamp(xx, 0, t.width - 1); };
        auto cy = [&](int yy) { return std::clamp(yy, 0, t.height - 1); };

        std::uint32_t c00 = t.pixels[(std::size_t)cy(y0) * t.width + cx(x0)];
        std::uint32_t c10 = t.pixels[(std::size_t)cy(y0) * t.width + cx(x1)];
        std::uint32_t c01 = t.pixels[(std::size_t)cy(y1) * t.width + cx(x0)];
        std::uint32_t c11 = t.pixels[(std::size_t)cy(y1) * t.width + cx(x1)];

        auto lerpCh = [](std::uint32_t a, std::uint32_t b, double t) {
            double x = (double)(a & 0xFF);
            double y = (double)(b & 0xFF);
            return (std::uint32_t)std::lround(x + (y - x) * t);
            };
        auto lerpPix = [&](std::uint32_t a, std::uint32_t b, double t) {
            std::uint32_t r = lerpCh(a, b, t);
            std::uint32_t g = lerpCh(a >> 8, b >> 8, t);
            std::uint32_t bl = lerpCh(a >> 16, b >> 16, t);
            std::uint32_t al = lerpCh(a >> 24, b >> 24, t);
            return r | (g << 8) | (bl << 16) | (al << 24);
            };

        std::uint32_t c0 = lerpPix(c00, c10, fx);
        std::uint32_t c1 = lerpPix(c01, c11, fx);
        return lerpPix(c0, c1, fy);
    }

    inline std::uint32_t shadeFlat(const ScreenTri&,
        double, double, double,
        double, double, double,
        double, double,
        void*)
    {
        return 0xFFC8C8C8;
    }

    // Цвет по текстуре. Если текстуры нет — серый.
    inline std::uint32_t shadeTextured(const ScreenTri&,
        double, double, double,
        double, double, double,
        double u, double v,
        void* userdata)
    {
        const TextureSampler* tex = static_cast<const TextureSampler*>(userdata);
        if (!tex || !tex->pixels) return 0xFFC8C8C8;
        return sampleTexture(*tex, u, v);
    }

}