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

    // ---- Утилиты упаковки/распаковки цвета (0xAARRGGBB) ----
    inline std::uint32_t packColor(float r, float g, float b, float a = 1.f) {
        auto c = [](float x) {
            x = std::clamp(x, 0.f, 1.f);
            return (std::uint32_t)std::lround(x * 255.f);
            };
        return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
    }

    inline void unpackColor(std::uint32_t px, float& r, float& g, float& b, float& a) {
        a = ((px >> 24) & 0xFF) / 255.f;
        r = ((px >> 16) & 0xFF) / 255.f;
        g = ((px >> 8) & 0xFF) / 255.f;
        b = (px & 0xFF) / 255.f;
    }

    inline std::uint32_t lerpColor(std::uint32_t c0, std::uint32_t c1, double t) {
        float r0, g0, b0, a0, r1, g1, b1, a1;
        unpackColor(c0, r0, g0, b0, a0);
        unpackColor(c1, r1, g1, b1, a1);
        return packColor(
            (float)(r0 + (r1 - r0) * t),
            (float)(g0 + (g1 - g0) * t),
            (float)(b0 + (b1 - b0) * t),
            (float)(a0 + (a1 - a0) * t));
    }

    // ---- Модель Ламберта: диффузное отражение ----
    // N — нормаль вершины (мир, единичная), L — направление к источнику (единичное).
    // I = ambient * base + diffuse * max(0, N·L) * base * lightColor
    inline std::uint32_t lambertVertex(
        const Vec3& N, const Vec3& L,
        const Vec3& baseColor, const Vec3& lightColor,
        float ambient, float diffuse)
    {
        float ndl = dot(N, L);
        if (ndl < EPSILON) ndl = 0.f; // теневая сторона
        float k = ambient + diffuse * ndl;
        return packColor(
            baseColor.x * lightColor.x * k,
            baseColor.y * lightColor.y * k,
            baseColor.z * lightColor.z * k);
    }

    // ---- Фрагментный шейдер Гуро ----
    // Барицентрика в экранном пространстве. Для Гуро цвет линеен в экранном
    // пространстве при аффинной проекции; для перспективы используем
    // перспективно-корректные веса (по 1/w), что и есть обобщение
    // билинейной интерполяции на треугольник.
    inline std::uint32_t shadeGouraud(
        const ScreenTri& tri,
        double l0, double l1, double l2,
        double iw0, double iw1, double iw2,
        double /*u*/, double /*v*/,
        void* /*userdata*/)
    {
        double iw = l0 * iw0 + l1 * iw1 + l2 * iw2;
        double w0, w1, w2;
        if (std::abs(iw) < EPSILON) {
            w0 = l0; w1 = l1; w2 = l2;
        }
        else {
            w0 = (l0 * iw0) / iw;
            w1 = (l1 * iw1) / iw;
            w2 = (l2 * iw2) / iw;
        }

        // Билинейная интерполяция цвета между вершинами треугольника
        // (эквивалентно линейной комбинации с барицентрическими весами).
        float r = 0, g = 0, b = 0, a = 0;
        float rr, gg, bb, aa;
        auto add = [&](std::uint32_t c, double w) {
            unpackColor(c, rr, gg, bb, aa);
            r += (float)(rr * w); g += (float)(gg * w);
            b += (float)(bb * w); a += (float)(aa * w);
            };
        add(tri.vcolor[0], w0);
        add(tri.vcolor[1], w1);
        add(tri.vcolor[2], w2);
        return packColor(r, g, b, a);
    }

    // ---- Плоский шейдер (как было) ----
    inline std::uint32_t shadeFlat(
        const ScreenTri& /*tri*/,
        double /*l0*/, double /*l1*/, double /*l2*/,
        double /*iw0*/, double /*iw1*/, double /*iw2*/,
        double /*u*/, double /*v*/,
        void* /*userdata*/)
    {
        return 0xFFC8C8C8u; // светло-серый
    }

    // ---- Текстурный шейдер (как было) ----
    inline std::uint32_t shadeTextured(
        const ScreenTri& tri,
        double /*l0*/, double /*l1*/, double /*l2*/,
        double /*iw0*/, double /*iw1*/, double /*iw2*/,
        double u, double v,
        void* userdata)
    {
        const TextureSampler* s = static_cast<const TextureSampler*>(userdata);
        if (!s) return 0xFFFFFFFFu;
        return sampleTexture(*s, u, v);
    }

}