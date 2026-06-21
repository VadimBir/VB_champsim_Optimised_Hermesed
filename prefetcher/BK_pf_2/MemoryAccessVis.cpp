#include <SDL2/SDL.h>
#include <iostream>

class MemoryAccessVis {
public:
    MemoryAccessVis() : window(nullptr), renderer(nullptr) {}

    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }

        window = SDL_CreateWindow("Memory Access Visualizer", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
            return false;
        }

        std::cout << "SDL2 initialized successfully!" << std::endl;
        return true;
    }

    void render() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE); // Black background
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE); // Red point
        SDL_RenderDrawPoint(renderer, 400, 300); // Draw point in the center

        SDL_RenderPresent(renderer); // Present the render
    }

    void cleanup() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        std::cout << "SDL2 cleanup complete!" << std::endl;
    }

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
};
