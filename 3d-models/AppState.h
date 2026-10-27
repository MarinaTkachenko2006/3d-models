#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include <optional>
#include <string>


struct AppState {
    // Объект, с которым работаем.
    std::optional<Polyhedron> currentObject;

    // Индексы в Registry — какой источник/операция сейчас выбраны в UI.
    // -1 = ничего не выбрано.
    int selectedSource = -1;
    int selectedOperation = -1;

    // путь для сохранения .obj
    std::string savePath = "obj_files/out.obj";

    // результат последнего сохранения 
    std::string lastSaveStatus;
    Projection projection;

    bool showVanishingPoints = false;
};