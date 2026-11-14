#include <algorithm>
#include <cmath>
#include <string_view>
#include <tuple>

#include "drift.hpp"
#include "oly/app/handler.hpp"
#include "oly/app/state_context.hpp"
#include "oly/app/system.hpp"
#include "oly/app/task_context.hpp"
#include "oly/ecs/query.hpp"
#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"

namespace drift {
  namespace {
    using oly::app::Engine;
    using oly::app::Phase;
    using oly::input::Key;

    struct RunTarget {
      Run* run;
    };

    float between(Run& run, float low, float high) {
      const float unit = static_cast<float>(run.random() - std::minstd_rand::min()) / static_cast<float>(std::minstd_rand::max() - std::minstd_rand::min());

      return low + (high - low) * unit;
    }

    void spawnRock(oly::app::TaskContext& context) {
      Run& run = *context.payload<RunTarget>().run;
      const float halfSize = between(run, RockMinHalfSize, RockMaxHalfSize);
      const float x = between(run, halfSize, static_cast<float>(LogicalWidth) - halfSize);

      context.spawn(Rock{}, Position{x, -halfSize}, Velocity{0.0f, between(run, RockMinFallSpeed, RockMaxFallSpeed)}, Extent{halfSize, halfSize});
    }

    void startRun(oly::app::StateContext& context) {
      Run& run = *static_cast<Run*>(context.userData());
      run.startTime = context.time();
      run.survivedSeconds = 0.0;

      context.spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight - PlayerBottomMargin}, Velocity{0.0f, 0.0f}, Extent{PlayerHalfSize, PlayerHalfSize});
      context.schedule().every(RockSpawnSeconds, spawnRock, RunTarget{&run});
    }

    void steerPlayer(oly::app::SystemContext& context) {
      const oly::input::Vector2 axis = context.actions().axis2d(actions::move);

      context.query<Velocity>(oly::ecs::With<Player>{}).each([axis](Velocity& velocity) {
        velocity = Velocity{axis.x * PlayerSpeed, axis.y * PlayerSpeed};
      });
    }

    void integrate(oly::app::SystemContext& context) {
      const float deltaSeconds = context.fixedDeltaTime();

      context.query<Position, const Velocity>().each([deltaSeconds](Position& position, const Velocity& velocity) {
        position.x += velocity.x * deltaSeconds;
        position.y += velocity.y * deltaSeconds;
      });
    }

    void confinePlayer(oly::app::SystemContext& context) {
      context.query<Position, const Extent>(oly::ecs::With<Player>{}).each([](Position& position, const Extent& extent) {
        position.x = std::clamp(position.x, extent.halfWidth, static_cast<float>(LogicalWidth) - extent.halfWidth);
        position.y = std::clamp(position.y, extent.halfHeight, static_cast<float>(LogicalHeight) - extent.halfHeight);
      });
    }

    void dropFallenRocks(oly::app::SystemContext& context) {
      context.query<const Position, const Extent>(oly::ecs::With<Rock>{}).each([&context](oly::ecs::Entity rock, const Position& position, const Extent& extent) {
        if (position.y - extent.halfHeight > static_cast<float>(LogicalHeight)) {
          context.destroy(rock);
        }
      });
    }

    bool overlaps(const Position& first, const Extent& firstExtent, const Position& second, const Extent& secondExtent) {
      return std::abs(first.x - second.x) < firstExtent.halfWidth + secondExtent.halfWidth && std::abs(first.y - second.y) < firstExtent.halfHeight + secondExtent.halfHeight;
    }

    void collide(oly::app::SystemContext& context) {
      auto player = context.query<const Position, const Extent>(oly::ecs::With<Player>{}).trySingle();
      if (!player) {
        return;
      }

      const oly::ecs::Entity playerEntity = std::get<0>(*player);
      const Position playerPosition = std::get<1>(*player);
      const Extent playerExtent = std::get<2>(*player);
      bool hit = false;

      context.query<const Position, const Extent>(oly::ecs::With<Rock>{}).each([&](const Position& position, const Extent& extent) {
        hit = hit || overlaps(playerPosition, playerExtent, position, extent);
      });
      if (hit) {
        context.emit(PlayerHit{playerEntity});
      }
    }

    void endRun(oly::app::HandlerContext& context) {
      Run& run = *static_cast<Run*>(context.userData());
      run.survivedSeconds = context.time() - run.startTime;
      context.requestState(states::gameOver);
    }

    void restartOnConfirm(oly::app::SystemContext& context) {
      if (context.actions().button(actions::restart).pressed) {
        context.requestState(states::playing);
      }
    }

    void addSystemIn(Engine& engine, oly::app::StateId state, Phase phase, std::string_view name, oly::app::SystemFunction function) {
      engine.setSystemStates(engine.addSystem(phase, name, function), {state});
    }
  }

  void installGameplay(Engine& engine, Run& run) {
    engine.actions().bind(actions::move, {Key::W, Key::S, Key::A, Key::D});
    engine.actions().bind(actions::move, {Key::ArrowUp, Key::ArrowDown, Key::ArrowLeft, Key::ArrowRight});
    engine.actions().bind(actions::restart, Key::Enter);
    engine.actions().bind(actions::restart, Key::Space);

    addSystemIn(engine, states::playing, Phase::UpdateBefore, "steerPlayer", steerPlayer);
    addSystemIn(engine, states::playing, Phase::Update, "integrate", integrate);
    addSystemIn(engine, states::playing, Phase::Update, "confinePlayer", confinePlayer);
    addSystemIn(engine, states::playing, Phase::UpdateAfter, "dropFallenRocks", dropFallenRocks);
    addSystemIn(engine, states::playing, Phase::UpdateAfter, "collide", collide);
    addSystemIn(engine, states::gameOver, Phase::UpdateBefore, "restartOnConfirm", restartOnConfirm);

    engine.setHandlerStates(engine.on<PlayerHit>("endRun", endRun, &run), {states::playing});
    engine.onEnter(states::playing, "startRun", startRun, &run);
    engine.requestState(states::playing);
  }
}
