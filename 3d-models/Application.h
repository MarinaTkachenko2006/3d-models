#pragma once
#include "AppState.h"
#include "Registry.h"

struct SDL_Window;

class Application {
public:
    bool init();
    void run();
    void shutdown();

private:
    SDL_Window* window_ = nullptr;
    AppState state_;
    Registry registry_;
};