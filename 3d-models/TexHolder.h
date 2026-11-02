#pragma once
#include <SDL3/SDL.h>
#include "PixelBuffer.h"

struct TexHolder {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;

    void Ensure(SDL_Renderer* r, int nw, int nh) {
        if (nw <= 0 || nh <= 0) return;
        if (tex && w == nw && h == nh) return;
        Destroy();
        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, nw, nh);
        if (!tex) return;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
        w = nw; h = nh;
    }

    void Update(const Render::PixelBuffer& buf) {
        if (!tex) return;
        int pitch = (int)(buf.Width() * 4);
        SDL_UpdateTexture(tex, nullptr, buf.Data(), pitch);
    }

    void Destroy() {
        if (tex) SDL_DestroyTexture(tex);
        tex = nullptr; w = 0; h = 0;
    }

    void* ImGuiID() const { return (void*)tex; }
};