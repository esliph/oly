#include "render.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>

#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"

namespace drift {
  namespace {
    struct Color {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
    };

    constexpr Color PlayerColor{90, 200, 255};
    constexpr Color RockColor{210, 120, 70};
    constexpr Color TextColor{240, 240, 240};

    template <class Tag>
    void drawBoxes(oly::app::SystemContext& context, SDL_Renderer* renderer, Color color) {
      SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, 255);

      context.query<const Position, const Extent>(oly::ecs::With<Tag>{}).each([renderer](const Position& position, const Extent& extent) {
        const SDL_FRect box{position.x - extent.halfWidth, position.y - extent.halfHeight, extent.halfWidth * 2.0f, extent.halfHeight * 2.0f};
        SDL_RenderFillRect(renderer, &box);
      });
    }
  }

  void drawDrift(oly::app::SystemContext& context) {
    const Canvas& canvas = *static_cast<const Canvas*>(context.userData());
    const bool playing = context.state() == states::playing;
    const double score = playing ? context.time() - canvas.run->startTime : canvas.run->survivedSeconds;

    drawBoxes<Rock>(context, canvas.renderer, RockColor);
    drawBoxes<Player>(context, canvas.renderer, PlayerColor);

    SDL_SetRenderDrawColor(canvas.renderer, TextColor.red, TextColor.green, TextColor.blue, 255);
    char text[64];
    std::snprintf(text, sizeof(text), "survived %.1f s", score);
    SDL_RenderDebugText(canvas.renderer, 8.0f, 8.0f, text);
    if (!playing) {
      SDL_RenderDebugText(canvas.renderer, LogicalWidth / 2.0f - 112.0f, LogicalHeight / 2.0f, "game over - Enter to restart");
    }
  }
}
