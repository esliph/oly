#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/game.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  struct Renderer {
    SDL_Renderer* renderer;
  };

  void draw(oly::app::Query<const Position, oly::ecs::With<Player>> players, oly::app::Res<const Renderer> target) {
    SDL_Renderer* renderer = target->renderer;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    players.each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }

  class MyGame : public oly::native_host::Game {
   public:
    MyGame()
      : Game({"@GAME_NAME@", LogicalWidth, LogicalHeight}) {}

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
      engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
      engine.world().spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
      engine.world().insertResource(Renderer{window.renderer()});
      engine.addSystem<&movePlayer>(oly::app::Update{}, "movePlayer");
      engine.addSystem<&draw>(oly::app::Render{}, "draw");
    }
  };
}

int main(int argumentCount, char** arguments) {
  return oly::native_host::runGame<MyGame>(argumentCount, arguments);
}
