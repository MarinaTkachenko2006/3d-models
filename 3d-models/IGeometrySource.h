#pragma once
#include "Polyhedron.h"
#include <optional>
#include <string>
#include <imgui.h>
class IGeometrySource {
public:
    virtual ~IGeometrySource() = default;

    // Имя для кнопки/селектора в UI.
    virtual const char* name() const = 0;

    // Панель параметров (если нужны)
    virtual void drawParamsUI() {}

    // Создать фигуру
    virtual std::optional<Polyhedron> build() = 0;
};


// -----Загрузка из .obj файла-----
// Возвращает std::nullopt, если файл не открылся или в нём нет ни одной валидной грани.
class ObjFileSource : public IGeometrySource {
public:
    const char* name() const override { return "OBJ file"; }

    void drawParamsUI() override {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "%s", path_.c_str());
        if (ImGui::InputText("Path", buf, sizeof(buf)))
            path_ = buf;
    }

    std::optional<Polyhedron> build() override {
        return loadObj(path_);
    }

private:
    std::string path_ = "obj_files/hex.obj";
};


// -----Генерация правильного многогранника-----
// n — число граней: 4 (тетраэдр), 6 (гексаэдр), 8 (октаэдр), 12 (додекаэдр), 20 (икосаэдр).
// Возвращает std::nullopt, если n не из поддерживаемого набора.
std::optional<Polyhedron> gen_regular_polyhedron(int n);

class GenerateRegularPolyhedron : public IGeometrySource {
public:
    const char* name() const override { return "Regular polyhedron"; }

    void drawParamsUI() override;

    std::optional<Polyhedron> build() override;

private:
    int faces_ = 4;   // по умолчанию
};

// -----Фигура вращения-----
class RevolutionSource : public IGeometrySource {
public:
    const char* name() const override { return "Surface of revolution"; }

    void drawParamsUI() override;
    std::optional<Polyhedron> build() override;

private:
    // Буфер для многострочного ввода образующей. Формат: "x y" на каждой строке.
    // Инициализируем примером по умолчанию.
    char profileBuffer_[1024] = "0.5 0.0\n1.0 1.0\n0.5 2.0\n";

    int axis_ = 1;      // 0 = X, 1 = Y, 2 = Z. По умолчанию вращаем вокруг Y
    int subdivisions_ = 32; // Число разбиений (сегментов)

    bool parseProfile(std::vector<std::pair<float, float>>& out,
        int& errorLine, std::string& errorMsg) const;

};