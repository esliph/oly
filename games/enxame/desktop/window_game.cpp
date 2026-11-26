#include "window_game.hpp"

#include <SDL3/SDL_render.h>

#include <algorithm>
#include <cmath>

#include "oly/app/system_parameters.hpp"
#include "oly/ecs/query.hpp"
#include "oly/input/action_map.hpp"
#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"

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
    void drawTagged(oly::app::Query<const WorldTransform, oly::ecs::With<Tag>> entities, const View& view, const Camera& camera, Color color, float size) {
      const float side = std::max(1.0f, size * camera.zoom);

      SDL_SetRenderDrawColor(view.renderer, color.red, color.green, color.blue, 255);

      entities.each([&](const WorldTransform& transform) {
        fillSquare(view.renderer, screenX(camera, transform.x), screenY(camera, transform.y), side);
      });
    }

    void moveCamera(oly::app::Res<Camera> camera, oly::app::Res<const oly::input::ActionMap> actionMap, const oly::app::StepInfo& step) {
      const float deltaTime = step.fixedDeltaTime;
      const oly::input::Vector2 pan = actionMap->axis2d(actions::pan);

      camera->x += pan.x * PanSpeed / camera->zoom * deltaTime;
      camera->y += pan.y * PanSpeed / camera->zoom * deltaTime;

      if (actionMap->button(actions::zoomIn).held) {
        camera->zoom *= 1.0f + ZoomRate * deltaTime;
      }

      if (actionMap->button(actions::zoomOut).held) {
        camera->zoom /= 1.0f + ZoomRate * deltaTime;
      }

      camera->zoom = std::clamp(camera->zoom, MinimumZoom, MaximumZoom);
    }

    void drawMap(oly::app::Res<View> view, oly::app::Res<const Camera> camera, oly::app::Res<const TileMap> map) {
      const float halfWidth = LogicalWidth / 2.0f / camera->zoom;
      const float halfHeight = LogicalHeight / 2.0f / camera->zoom;
      const TilePosition first = map->tileAt(camera->x - halfWidth, camera->y - halfHeight);
      const TilePosition last = map->tileAt(camera->x + halfWidth, camera->y + halfHeight);
      const int32_t firstX = std::max(first.x, 0);
      const int32_t firstY = std::max(first.y, 0);
      const int32_t lastX = std::min(last.x, static_cast<int32_t>(map->width()) - 1);
      const int32_t lastY = std::min(last.y, static_cast<int32_t>(map->height()) - 1);
      const float side = map->tileSize() * camera->zoom;

      SDL_SetRenderDrawColor(view->renderer, 60, 60, 70, 255);

      for (int32_t y = firstY; y <= lastY; ++y) {
        for (int32_t x = firstX; x <= lastX; ++x) {
          if (map->blocked({x, y})) {
            fillSquare(view->renderer, screenX(*camera, map->centerX({x, y})), screenY(*camera, map->centerY({x, y})), side);
          }
        }
      }

      SDL_SetRenderDrawColor(view->renderer, 255, 255, 255, 255);
      fillSquare(view->renderer, screenX(*camera, map->centerX(map->base())), screenY(*camera, map->centerY(map->base())), side);
    }

    void drawEntities(oly::app::Query<const WorldTransform, oly::ecs::With<Enemy>> enemies, oly::app::Query<const WorldTransform, oly::ecs::With<Turret>> turrets, oly::app::Query<const WorldTransform, oly::ecs::With<Projectile>> projectiles, oly::app::Query<const WorldTransform, oly::ecs::With<BeltItem>> beltItems, oly::app::Query<const WorldTransform, oly::ecs::With<Assembler>> assemblers, oly::app::Query<const WorldTransform, oly::ecs::With<Vehicle>> vehicles, oly::app::Query<const WorldTransform, oly::ecs::With<VehicleTurret>> vehicleTurrets, oly::app::Res<View> view, oly::app::Res<const Camera> camera) {
      drawTagged(enemies, *view, *camera, {220, 60, 60}, EnemySize);
      drawTagged(turrets, *view, *camera, {80, 140, 255}, TurretSize);
      drawTagged(projectiles, *view, *camera, {255, 220, 80}, ProjectileSize);
      drawTagged(beltItems, *view, *camera, {170, 170, 170}, BeltItemSize);
      drawTagged(assemblers, *view, *camera, {230, 140, 40}, AssemblerSize);
      drawTagged(vehicles, *view, *camera, {80, 200, 120}, VehicleSize);
      drawTagged(vehicleTurrets, *view, *camera, {160, 240, 170}, VehicleTurretSize);
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
    setUpScenario(windowScenario, windowSize, engine);
    engine.world().insertResource(View{window.renderer()});

    engine.addSystem<&moveCamera>(oly::app::Update{}, "moveCamera");

    if (engine.world().tryResource<TileMap>() != nullptr) {
      engine.addSystem<&drawMap>(oly::app::Render{}, "drawMap");
      engine.addSystem<&drawEntities>(oly::app::Render{}, "drawEntities").after<&drawMap>();
    } else {
      engine.addSystem<&drawEntities>(oly::app::Render{}, "drawEntities");
    }
  }
}
