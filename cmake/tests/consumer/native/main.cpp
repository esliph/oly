#include <cstdio>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/world.hpp"
#include "schema.generated.hpp"

int main() {
  // Unused on purpose: the game's own warnings stay warnings, because a subproject does not add -Werror to it. The
  // consumer test fails if the compiler stops reporting this one.
  int unusedOnPurpose = 0;

  oly::ecs::World world;
  oly::app::Engine engine(world);
  oly::app::setUpGame(engine);
  world.spawn(Player{}, Position{1.0f, 2.0f});

  std::puts("consumer ok");
  return 0;
}
