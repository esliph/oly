#pragma once

#include <cstdint>

#include "oly/ecs/command_buffer.hpp"
#include "oly/ecs/component.hpp"
#include "oly/ecs/entity.hpp"

namespace oly::ecs {
  class World;

  // Valid until World::removeObserver; ids are never reused.
  using ObserverId = uint32_t;

  enum class ObserverKind : uint8_t { OnAdd,
                                      OnRemove };

  // Valid only during the observer call. The World is locked for structure while observers run: they read
  // and write values, and change structure only through commands(), which the World applies right after the
  // operation that triggered them, before it returns, until nothing is pending (R-OB-6).
  class ObserverContext {
   private:
    friend class World;

    World* world_;
    Entity entity_;
    ComponentId componentId_;
    void* userData_;
    CommandBuffer* commands_;

   public:
    World& world() const noexcept {
      return *world_;
    }
    Entity entity() const noexcept {
      return entity_;
    }
    ComponentId componentId() const noexcept {
      return componentId_;
    }
    void* userData() const noexcept {
      return userData_;
    }
    CommandBuffer& commands() const noexcept {
      return *commands_;
    }

   private:
    ObserverContext(World& world, Entity entity, ComponentId componentId, void* userData, CommandBuffer& commands) noexcept
      : world_(&world),
        entity_(entity),
        componentId_(componentId),
        userData_(userData),
        commands_(&commands) {}
  };

  using ObserverFunction = void (*)(ObserverContext& context);
}
