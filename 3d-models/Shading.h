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
        if (ndl < 0.f) ndl = 0.f;                  // теневая сторона
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
        if (std::abs(iw) < 1e-12) {
            w0 = l0; w1 = l1; w2 = l2;
        }
        else {
            w0 = (l0 * iw0) / iw;
            w1 = (l1 * iw1) / iw;
            w2 = (l2 * iw2) / iw;
        }

        // Билинейная интерполяция цвета между вершинами треугольника
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

    // ---- Текстура × Ламберт (Фонг-стиль) ----
    // Цвет пикселя = цвет текстуры × коэффициент Ламберта,
    // где нормаль интерполируется по треугольнику (перспективно-корректно)
    // и нормализуется. Это точнее, чем Гуро, для текстур: градиент света
    // и блик зависят от нормали во фрагменте, а не в вершине.
    struct TexturedLitData {
        TextureSampler sampler;
        Vec3 lightDir{ 0.f, 1.f, 0.f };   // L (единичный, к источнику)
        Vec3 lightColor{ 1.f, 1.f, 1.f };
        Vec3 objectColor{ 1.f, 1.f, 1.f }; // цвет-модулятор (обычно белый, т.к. цвет из текстуры)
        float ambient = 0.15f;
        float diffuse = 0.85f;
    };

    inline std::uint32_t shadeTexturedLit(
        const ScreenTri& tri,
        double l0, double l1, double l2,
        double iw0, double iw1, double iw2,
        double u, double v,
        void* userdata)
    {
        auto* d = static_cast<TexturedLitData*>(userdata);
        if (!d) return 0xFFFFFFFFu;

        // 1) цвет текстуры
        std::uint32_t tex = sampleTexture(d->sampler, u, v);

        // 2) интерполируем нормаль (перспективно-корректно) и нормализуем
        double iw = l0 * iw0 + l1 * iw1 + l2 * iw2;
        double w0, w1, w2;
        if (std::abs(iw) < 1e-12) {
            w0 = l0; w1 = l1; w2 = l2;
        }
        else {
            w0 = (l0 * iw0) / iw;
            w1 = (l1 * iw1) / iw;
            w2 = (l2 * iw2) / iw;
        }

        Vec3 N(
            (float)(w0 * tri.nx[0] + w1 * tri.nx[1] + w2 * tri.nx[2]),
            (float)(w0 * tri.ny[0] + w1 * tri.ny[1] + w2 * tri.ny[2]),
            (float)(w0 * tri.nz[0] + w1 * tri.nz[1] + w2 * tri.nz[2]));
        if (length(N) > 1e-8f) N = normalize(N);
        else N = Vec3(0.f, 1.f, 0.f);

        // 3) Ламберт
        float ndl = dot(N, d->lightDir);
        if (ndl < 0.f) ndl = 0.f;
        float k = d->ambient + d->diffuse * ndl;

        // 4) цвет = текстура × lightColor × objectColor × k
        float r, g, b, a;
        unpackColor(tex, r, g, b, a);
        return packColor(
            r * d->lightColor.x * d->objectColor.x * k,
            g * d->lightColor.y * d->objectColor.y * k,
            b * d->lightColor.z * d->objectColor.z * k,
            a);
    }

    // =====================================================================
    // ---- Шейдинг Фонга для модели туншейдинга (задание 1.2) ----
    // =====================================================================
    // Шейдинг Фонга: в отличие от Гуро, интерполируется не цвет, а НОРМАЛЬ.
    // В каждом пикселе нормаль собирается из нормалей трёх вершин
    // (барицентрическая = билинейная интерполяция на треугольнике,
    // перспективно-корректная по 1/w), затем нормализуется, и уже по ней
    // считается освещённость.
    //
    // Модель туншейдинга (cel shading): вместо плавного cos(N, L) освещённость
    // квантуется на несколько ступеней — получаются резкие «мультяшные» полосы.
    // Дополнительно — бинарный блик (есть/нет) по модели Блинна-Фонга.
    struct PhongToonData {
        TextureSampler sampler;
        bool  useTexture = false;              // брать базовый цвет из текстуры
        Vec3  lightDir{ 0.f, 1.f, 0.f };       // L (единичный, к источнику)
        Vec3  lightColor{ 1.f, 1.f, 1.f };
        Vec3  objectColor{ 1.f, 1.f, 1.f };
        float ambient = 0.15f;
        float diffuse = 0.85f;
        int   levels = 4;                      // число ступеней освещённости
        bool  specular = true;
        float shininess = 32.f;
        // Вектор к наблюдателю: для перспективы — от точки к камере,
        // для параллельной проекции — один для всех точек.
        bool  perspective = true;
        Vec3  cameraPos{ 0.f, 0.f, 0.f };
        Vec3  toViewer{ 0.f, 0.f, 1.f };       // для параллельной проекции
    };

    // Квантование освещённости x ∈ [0,1] на levels ступеней:
    // levels = 4 → значения 0, 1/3, 2/3, 1.
    inline float toonQuantize(float x, int levels) {
        if (levels < 2) return x > 0.f ? 1.f : 0.f;
        x = std::clamp(x, 0.f, 1.f);
        int band = (int)std::floor(x * levels);
        if (band > levels - 1) band = levels - 1;
        return (float)band / (float)(levels - 1);
    }

    inline std::uint32_t shadePhongToon(
        const ScreenTri& tri,
        double l0, double l1, double l2,
        double iw0, double iw1, double iw2,
        double u, double v,
        void* userdata)
    {
        auto* d = static_cast<PhongToonData*>(userdata);
        if (!d) return 0xFFFFFFFFu;

        // 1) Перспективно-корректные веса вершин (по 1/w)
        double iw = l0 * iw0 + l1 * iw1 + l2 * iw2;
        double w0, w1, w2;
        if (std::abs(iw) < 1e-12) {
            w0 = l0; w1 = l1; w2 = l2;
        }
        else {
            w0 = (l0 * iw0) / iw;
            w1 = (l1 * iw1) / iw;
            w2 = (l2 * iw2) / iw;
        }

        // 2) Интерполяция нормали между вершинами + нормализация
        Vec3 N(
            (float)(w0 * tri.nx[0] + w1 * tri.nx[1] + w2 * tri.nx[2]),
            (float)(w0 * tri.ny[0] + w1 * tri.ny[1] + w2 * tri.ny[2]),
            (float)(w0 * tri.nz[0] + w1 * tri.nz[1] + w2 * tri.nz[2]));
        if (length(N) > 1e-8f) N = normalize(N);
        else N = Vec3(0.f, 1.f, 0.f);

        // 3) Туншейдинг: диффузная часть, квантованная на ступени
        float ndl = dot(N, d->lightDir);
        if (ndl < 0.f) ndl = 0.f;
        float k = d->ambient + d->diffuse * toonQuantize(ndl, d->levels);

        // 4) Резкий блик (Блинн-Фонг + порог): либо полный, либо никакого
        float spec = 0.f;
        if (d->specular && ndl > 0.f) {
            Vec3 V = d->toViewer;
            if (d->perspective) {
                // мировая позиция пикселя тоже интерполируется перспективно-корректно
                Vec3 P(
                    (float)(w0 * tri.wx[0] + w1 * tri.wx[1] + w2 * tri.wx[2]),
                    (float)(w0 * tri.wy[0] + w1 * tri.wy[1] + w2 * tri.wy[2]),
                    (float)(w0 * tri.wz[0] + w1 * tri.wz[1] + w2 * tri.wz[2]));
                Vec3 toCam = d->cameraPos - P;
                if (length(toCam) > 1e-8f) V = normalize(toCam);
            }
            Vec3 H = d->lightDir + V;
            if (length(H) > 1e-8f) {
                H = normalize(H);
                float ndh = std::max(0.f, dot(N, H));
                if (std::pow(ndh, d->shininess) > 0.5f) spec = 0.6f;
            }
        }

        // 5) Базовый цвет: текстура (если есть UV) × цвет объекта, иначе цвет объекта
        float br = d->objectColor.x, bg = d->objectColor.y, bb = d->objectColor.z, ba = 1.f;
        if (d->useTexture && tri.hasUV) {
            float tr, tg, tb;
            unpackColor(sampleTexture(d->sampler, u, v), tr, tg, tb, ba);
            br *= tr; bg *= tg; bb *= tb;
        }

        return packColor(
            br * d->lightColor.x * k + spec * d->lightColor.x,
            bg * d->lightColor.y * k + spec * d->lightColor.y,
            bb * d->lightColor.z * k + spec * d->lightColor.z,
            ba);
    }

}