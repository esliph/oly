#pragma once

#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace oly::native_host {
  struct WindowConfig {
    std::string title;
    int width;
    int height;
  };

  class Window {
   private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

   public:
    // Throws std::runtime_error with SDL's message if SDL cannot start its video subsystem or create the window.
    explicit Window(const WindowConfig& config);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* window() const noexcept;
    SDL_Renderer* renderer() const noexcept;
  };
}
