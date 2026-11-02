#pragma once
#include "IGeometrySource.h"
#include "Expression.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>

// Источник: сегмент поверхности z = f(x,y) на [x0,x1]×[y0,y1] с N разбиениями
class SurfacePlotSource : public IGeometrySource {
public:
    const char* name() const override { return "Surface f(x,y)"; }

    void drawParamsUI() override {
        // Функция f(x,y)
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", funcText_.c_str());
        if (ImGui::InputText("f(x,y)##surface", buf, sizeof(buf)))
            funcText_ = buf;

        // Диапазоны X, Y (идентичные — одна галочка)
        ImGui::Checkbox("Same range for X and Y##surface", &sameRange_);
        if (sameRange_) {
            float a = xMin_, b = xMax_;
            if (ImGui::DragFloat("min##surface", &a, 0.1f, -100.f, 100.f)) {
                xMin_ = a; yMin_ = a;
            }
            if (ImGui::DragFloat("max##surface", &b, 0.1f, -100.f, 100.f)) {
                xMax_ = b; yMax_ = b;
            }
            if (xMax_ <= xMin_) xMax_ = xMin_ + 1.f;
            if (yMax_ <= yMin_) yMax_ = yMin_ + 1.f;
        }
        else {
            ImGui::DragFloat("x0##surface", &xMin_, 0.1f, -100.f, 100.f);
            ImGui::DragFloat("x1##surface", &xMax_, 0.1f, -100.f, 100.f);
            ImGui::DragFloat("y0##surface", &yMin_, 0.1f, -100.f, 100.f);
            ImGui::DragFloat("y1##surface", &yMax_, 0.1f, -100.f, 100.f);
            if (xMax_ <= xMin_) xMax_ = xMin_ + 1.f;
            if (yMax_ <= yMin_) yMax_ = yMin_ + 1.f;
        }

        // Число разбиений (одинаково для X и Y)
        ImGui::DragInt("N (subdivisions)##surface", &N_, 1.f, 2, 200);
    }

    std::optional<Polyhedron> build() override {
        // 1. Парсим f(x,y)
        Core::RealExpression expr;
        std::string err;
        if (!expr.Parse(funcText_, { "x", "y" }, &err))
            return std::nullopt;
        if (!expr.Valid()) return std::nullopt;

        const int N = std::max(2, N_);
        const int VN = N + 1;

        // 2. Вершины сетки: z = f(x,y)
        std::vector<Vec3> grid(VN * VN);
        for (int j = 0; j < VN; ++j) {
            for (int i = 0; i < VN; ++i) {
                double x = xMin_ + (xMax_ - xMin_) * i / (double)N;
                double y = yMin_ + (yMax_ - yMin_) * j / (double)N;
                Core::VarsT<double> v;
                v.x = x;
                v.y = y;
                double z = expr.Eval(v);
                if (!std::isfinite(z)) z = 0.0;
                grid[j * VN + i] = Vec3((float)x, (float)y, (float)z);
            }
        }

        // 3. Грани: 4-угольник на ячейку (v00, v10, v11, v01)
        Polyhedron result;
        result.sides.reserve(N * N);
        for (int j = 0; j < N; ++j) {
            for (int i = 0; i < N; ++i) {
                int i00 = j * VN + i;
                int i10 = i00 + 1;
                int i01 = i00 + VN;
                int i11 = i01 + 1;

                Polygon p;
                p.vertices = { grid[i00], grid[i10], grid[i11], grid[i01] };
                p.computeNormal();
                result.sides.push_back(std::move(p));
            }
        }

        if (result.sides.empty()) return std::nullopt;
        return result;
    }

private:
    std::string funcText_ = "sin(x) * cos(y)";
    float xMin_ = -5.f, xMax_ = 5.f;
    float yMin_ = -5.f, yMax_ = 5.f;
    int N_ = 32;
    bool sameRange_ = true;
};