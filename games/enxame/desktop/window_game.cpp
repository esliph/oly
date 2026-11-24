#include "window_game.hpp"

#include <SDL3/SDL_render.h>

#include <algorithm>
#include <cmath>

#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace enxame {
  namespace {
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 720;
    constexpr float PanSpeed = 400.0f;
    constexpr float ZoomRate = 1.5f;
    constexpr float MinimumZoom = 0.05f;
    constexpr float MaximumZoom = 8.0f;
    constexpr float EnemySize = 4.0f;
    constexpr float TurretSize = 8.0f;
    constexpr float ProjectileSize = 2.0f;
    constexpr float BeltItemSize = 3.0f;
    constexpr float AssemblerSize = 8.0f;
    constexpr float VehicleSize = 6.0f;
    constexpr float VehicleTurretSize = 3.0f;

    ScenarioId windowScenario = ScenarioId::Playable;
    uint32_t windowSize = 0;

    float screenX(const Camera& camera, float worldX) {
      return (worldX - camera.x) * camera.zoom + LogicalWidth / 2.0f;
    }

    float screenY(const Camera& camera, float worldY) {
      return (worldY - camera.y) * camera.zoom + LogicalHeight / 2.0f;
    }

    void fillSquare(SDL_Renderer* renderer, float centerX, float centerY, float side) {
      const SDL_FRect rectangle{centerX - side / 2.0f, centerY - side / 2.0f, side, side};

      SDL_RenderFillRect(renderer, &rectangle);
    }

    struct Color {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
    };

    // Every entity with Tag, from its WorldTransform (it already holds the parent chain), at least one pixel wide.
    template <class Tag>
    void drawEntities(oly::app::SystemContext& context, const WorldView& view, Color color, float size) {
      const Camera& camera = view.simulation->camera;
      const float side = std::max(1.0f, size * camera.zoom);

      SDL_SetRenderDrawColor(view.renderer, color.red, color.green, color.blue, 255);

      context.query<const WorldTransform>(oly::ecs::With<Tag>{}).each([&](const WorldTransform& transform) {
        fillSquare(view.renderer, screenX(camera, transform.x), screenY(camera, transform.y), side);
      });
    }

    void drawMap(const WorldView& view) {
      if (!view.simulation->map) {
        return;
      }

      const Camera& camera = view.simulation->camera;
      const TileMap& map = *view.simulation->map;
      const float halfWidth = LogicalWidth / 2.0f / camera.zoom;
      const float halfHeight = LogicalHeight / 2.0f / camera.zoom;
      const TilePosition first = map.tileAt(camera.x - halfWidth, camera.y - halfHeight);
      const TilePosition last = map.tileAt(camera.x + halfWidth, camera.y + halfHeight);
      const int32_t firstX = std::max(first.x, 0);
      const int32_t firstY = std::max(first.y, 0);
      const int32_t lastX = std::min(last.x, static_cast<int32_t>(map.width()) - 1);
      const int32_t lastY = std::min(last.y, static_cast<int32_t>(map.height()) - 1);
      const float side = map.tileSize() * camera.zoom;

      SDL_SetRenderDrawColor(view.renderer, 60, 60, 70, 255);

      for (int32_t y = firstY; y <= lastY; ++y) {
        for (int32_t x = firstX; x <= lastX; ++x) {
          if (map.blocked({x, y})) {
            fillSquare(view.renderer, screenX(camera, map.centerX({x, y})), screenY(camera, map.centerY({x, y})), side);
          }
        }
      }

      SDL_SetRenderDrawColor(view.renderer, 255, 255, 255, 255);
      fillSquare(view.renderer, screenX(camera, map.centerX(map.base())), screenY(camera, map.centerY(map.base())), side);
    }
  }

  void setWindowScenario(ScenarioId scenario, uint32_t size) {
    windowScenario = scenario;
    windowSize = size;
  }

  EnxameGame::EnxameGame()
    : Game({"enxame", WindowWidth, WindowHeight}) {}

  void EnxameGame::onStart(oly::app::Engine& engine, oly::native_host::Window& window) {
    using oly::input::Key;

    oly::input::ActionMap& actionMap = engine.actions();
    actionMap.bind(actions::pan, {Key::W, Key::S, Key::A, Key::D});
    actionMap.bind(actions::zoomIn, Key::E);
    actionMap.bind(actions::zoomOut, Key::Q);

    SDL_SetRenderLogicalPresentation(window.renderer(), LogicalWidth, LogicalHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    simulation_ = setUpScenario(windowScenario, windowSize, engine);
    view_ = {window.renderer(), simulation_.get()};

    engine.addSystem(oly::app::Phase::Update, "moveCamera", moveCamera, simulation_.get());
    engine.addSystem(oly::app::Phase::Render, "drawWorld", drawWorld, &view_);
  }
}

OLY_SYSTEM(moveCamera) {
  enxame::Camera& camera = static_cast<enxame::Simulation*>(context.userData())->camera;
  const float deltaTime = context.fixedDeltaTime();
  const oly::input::Vector2 pan = context.actions().axis2d(actions::pan);

  camera.x += pan.x * enxame::PanSpeed / camera.zoom * deltaTime;
  camera.y += pan.y * enxame::PanSpeed / camera.zoom * deltaTime;

  if (context.actions().button(actions::zoomIn).held) {
    camera.zoom *= 1.0f + enxame::ZoomRate * deltaTime;
  }

  if (context.actions().button(actions::zoomOut).held) {
    camera.zoom /= 1.0f + enxame::ZoomRate * deltaTime;
  }

  camera.zoom = std::clamp(camera.zoom, enxame::MinimumZoom, enxame::MaximumZoom);
}

OLY_SYSTEM(drawWorld) {
  const enxame::WorldView& view = *static_cast<enxame::WorldView*>(context.userData());

  enxame::drawMap(view);
  enxame::drawEntities<Enemy>(context, view, {220, 60, 60}, enxame::EnemySize);
  enxame::drawEntities<Turret>(context, view, {80, 140, 255}, enxame::TurretSize);
  enxame::drawEntities<Projectile>(context, view, {255, 220, 80}, enxame::ProjectileSize);
  enxame::drawEntities<BeltItem>(context, view, {170, 170, 170}, enxame::BeltItemSize);
  enxame::drawEntities<Assembler>(context, view, {230, 140, 40}, enxame::AssemblerSize);
  enxame::drawEntities<Vehicle>(context, view, {80, 200, 120}, enxame::VehicleSize);
  enxame::drawEntities<VehicleTurret>(context, view, {160, 240, 170}, enxame::VehicleTurretSize);
}
