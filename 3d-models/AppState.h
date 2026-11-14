#pragma once
#include "Polyhedron.h"
#include "Projection.h"
#include "Camera.h"
#include "RenderMode.h"
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

struct AppState {
    std::optional<Polyhedron> currentObject;

    int selectedSource = -1;
    int selectedOperation = -1;

    std::string savePath = "obj_files/out.obj";
    std::string lastSaveStatus;
    Projection projection;
    bool showVanishingPoints = false;

    Render::Mode renderMode = Render::Mode::Solid;
    Render::Options renderOptions;

    // ---- Текстура (превью + данные для рендера) ----
    std::string texturePath;
    unsigned int texturePreview = 0;     // ImTextureID для превью в панели
    std::vector<std::uint32_t> texturePixels; // RGBA, для шейдера
    int textureWidth = 0;
    int textureHeight = 0;

    // Автовращение
    bool spinEnabled = true;
    int spinAxis = 1;
    float spinSpeed = 45.f;
    float spinAngle = 0.f;

    Camera camera;
    bool orbitEnabled = false;
    float orbitSpeed = 30.f;
};