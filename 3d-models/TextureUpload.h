#pragma once
#include "RenderTarget.h"
#include <SDL3/SDL.h>
#include <imgui.h>

namespace Render {

    // Кэш одной текстуры — привязан к SDL_Renderer.
    // При изменении размера RenderTarget текстура пересоздаётся;
    // при том же размере — обновляется через SDL_UpdateTexture
    struct TextureCache {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        SDL_Renderer* renderer = nullptr;

        ~TextureCache() {
            if (texture) SDL_DestroyTexture(texture);
        }

        TextureCache() = default;
        TextureCache(const TextureCache&) = delete;
        TextureCache& operator=(const TextureCache&) = delete;
    };

    // Загружает rt в текстуру (создаёт/обновляет в cache) и возвращает ImTextureID.
    ImTextureID uploadToTexture(const RenderTarget& rt,
        SDL_Renderer* renderer,
        TextureCache& cache);

}