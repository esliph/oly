#include "oly/native_host/window.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace oly::native_host {
  Window::Window(const WindowConfig& config) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      throw std::runtime_error(std::string("SDL_InitSubSystem(video): ") + SDL_GetError());
    }

    if (!SDL_CreateWindowAndRenderer(config.title.c_str(), config.width, config.height, SDL_WINDOW_RESIZABLE, &window_, &renderer_) ||
        !SDL_SetRenderLogicalPresentation(renderer_, config.width, config.height, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
      std::string message = SDL_GetError();
      SDL_DestroyRenderer(renderer_);
      SDL_DestroyWindow(window_);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
      throw std::runtime_error("SDL window: " + message);
    }

    // The dummy driver has no vsync; the loop then runs unpaced, which headless tests want anyway.
    SDL_SetRenderVSync(renderer_, 1);
  }

  Window::~Window() {
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }

  SDL_Window* Window::window() const noexcept {
    return window_;
  }

  SDL_Renderer* Window::renderer() const noexcept {
    return renderer_;
  }
}
