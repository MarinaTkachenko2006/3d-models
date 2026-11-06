#pragma once
#include <cstdint>
#include <vector>

namespace Render {

    // цвет + Z-буфер (float).
    // Хранится в памяти, затем загружается в текстуру ImGui/SDL.
    struct RenderTarget {
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> color;  
        std::vector<float>         depth;  

        void resize(int w, int h) {
            width = w; height = h;
            color.assign((std::size_t)w * h, 0);
            depth.assign((std::size_t)w * h, 1.0f);   
        }

        void clear(std::uint32_t bg = 0xFF000000, float farZ = 1e9f) { 
            std::fill(color.begin(), color.end(), bg);
            std::fill(depth.begin(), depth.end(), farZ);
        }
    };

}