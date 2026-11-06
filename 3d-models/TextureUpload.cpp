#include "TextureUpload.h"
#include <SDL3/SDL.h>

namespace Render {

    ImTextureID uploadToTexture(const RenderTarget& rt,
        SDL_Renderer* renderer,
        TextureCache& cache)
    {
        if (!renderer || rt.width <= 0 || rt.height <= 0) return (ImTextureID)0;
        if (rt.color.size() != (std::size_t)rt.width * rt.height) return (ImTextureID)0;

        // Пересоздание текстуры, если размер изменился или сменился рендерер
        if (!cache.texture
            || cache.renderer != renderer
            || cache.width != rt.width
            || cache.height != rt.height)
        {
            if (cache.texture) {
                SDL_DestroyTexture(cache.texture);
                cache.texture = nullptr;
            }

            cache.texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING,
                rt.width, rt.height);

            if (!cache.texture) {
                SDL_Log("uploadToTexture: SDL_CreateTexture failed: %s", SDL_GetError());
                return (ImTextureID)0;
            }

            cache.renderer = renderer;
            cache.width = rt.width;
            cache.height = rt.height;
        }

        // Обновление содержимого.
        const int pitch = rt.width * (int)sizeof(std::uint32_t);
        if (!SDL_UpdateTexture(cache.texture, nullptr, rt.color.data(), pitch)) {
            SDL_Log("uploadToTexture: SDL_UpdateTexture failed: %s", SDL_GetError());
            return (ImTextureID)0;
        }

        return (ImTextureID)(intptr_t)cache.texture;
    }

}