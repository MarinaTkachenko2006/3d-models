#pragma once
#include "AppState.h"
#include "Registry.h"

struct SDL_Renderer;

void drawUi(AppState& state, Registry& registry, SDL_Renderer* renderer);