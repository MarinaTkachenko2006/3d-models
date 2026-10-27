#include "Application.h"
#include "Ui.h"
#include <SDL3/SDL.h>
#include "imgui.h"
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "IOperation.h"

struct SDL_Renderer* renderer_ = nullptr;

bool Application::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow("3d Model", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!window_) return false;
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer3_Init(renderer_);

    //новые способы задания объектов добавляем сюда
    registry_.addSource(std::make_unique<ObjFileSource>());
    registry_.addSource(std::make_unique<GenerateRegularPolyhedron>());

    //новые способы преобразования объектов добавляем сюда
    registry_.addOperation(std::make_unique<TranslateOperation>());
    registry_.addOperation(std::make_unique<RotateOperation>());

    return true;
}

void Application::run() {
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();

        drawUi(state_, registry_);

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }
}

void Application::shutdown() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    SDL_Quit();
}