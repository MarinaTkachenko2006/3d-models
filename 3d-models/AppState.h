#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include "Camera.h"
#include "RenderMode.h"
#include <optional>
#include <string>

struct AppState {
    std::optional<Polyhedron> currentObject;

    int selectedSource = -1;
    int selectedOperation = -1;

    std::string savePath = "obj_files/out.obj";
    std::string lastSaveStatus;
    Projection projection;
    bool showVanishingPoints = false;

    Render::Mode renderMode = Render::Mode::Solid;

    Camera camera;
    bool orbitEnabled = false; // Для доп.опции
    float orbitSpeed = 30.f;
};