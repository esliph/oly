#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/game.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  void draw(oly::app::SystemContext& context) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(context.userData());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    context.query<const Position>(oly::ecs::With<Player>{}).each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }

  class MyGame : public oly::native_host::Game {
   public:
    MyGame()
      : Game({"enxame", LogicalWidth, LogicalHeight}) {}

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
      engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
      engine.world().spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
      engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);
      engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());
    }
  };
}

int main(int argumentCount, char** arguments) {
  return oly::native_host::runGame<MyGame>(argumentCount, arguments);
}
