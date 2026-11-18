#include "oly/essentials2d/plugin.hpp"

#include <cstdio>

#include "oly/app/engine.hpp"
#include "oly/essentials2d/schema.generated.hpp"

namespace oly::essentials2d {
  namespace {
    ecs::Entity entityOfKey(uint64_t key) {
      return ecs::Entity{static_cast<uint32_t>(key >> 32), static_cast<uint32_t>(key)};
    }

    bool fail(std::string* why, const char* reason, ecs::Entity entity) {
      if (why != nullptr) {
        char text[96];
        std::snprintf(text, sizeof(text), "%s e=%u:%u", reason, entity.index, entity.generation);
        *why = text;
      }

      return false;
    }
  }

  Plugin::~Plugin() {
    if (world_ == nullptr) {
      return;
    }

    for (const ecs::ObserverId observer : observers_) {
      world_->removeObserver(observer);
    }
  }

  void Plugin::install(app::Engine& engine) {
    world_ = &engine.world();
    world_->imply<Position, WorldTransform>();
    observers_[0] = world_->onAdd<Parent>(onParentAdded, this);
    observers_[1] = world_->onRemove<Parent>(onParentRemoved, this);
    observers_[2] = world_->onAdd<WorldTransform>(onWorldTransformAdded, this);
    observers_[3] = world_->onRemove<detail::HasChildren>(onHasChildrenRemoved, this);
    engine.addSystem(app::Phase::Update, "propagateTransforms", propagateTransforms, this, app::OrderBand::Last);
  }

  bool Plugin::checkHierarchy(ecs::World& world, std::string* why) const {
    size_t indexed = 0;

    for (const auto& [key, children] : children_) {
      const ecs::Entity parent = entityOfKey(key);

      if (!world.isAlive(parent)) {
        return fail(why, "dead parent in the index", parent);
      }
      if (!world.has<detail::HasChildren>(parent)) {
        return fail(why, "parent without HasChildren", parent);
      }
      if (children.empty()) {
        return fail(why, "empty child list", parent);
      }

      for (uint32_t slot = 0; slot < children.size(); ++slot) {
        const ecs::Entity child = children[slot];
        const Parent* link = world.tryGet<Parent>(child);
        const auto recorded = slots_.find(keyOf(child));

        if (link == nullptr || link->entity != parent) {
          return fail(why, "indexed child with another Parent", child);
        }
        if (recorded == slots_.end() || recorded->second != slot) {
          return fail(why, "child with a wrong slot", child);
        }
      }

      indexed += children.size();
    }

    size_t linked = 0;
    bool markedWithoutChildren = false;
    ecs::Entity markedEntity = ecs::NullEntity;
    world.query<Parent>().each([&](Parent&) { ++linked; });
    world.query<>(ecs::With<detail::HasChildren>{}).each([&](ecs::Entity entity) {
      if (children_.find(keyOf(entity)) == children_.end()) {
        markedWithoutChildren = true;
        markedEntity = entity;
      }
    });

    if (linked != indexed || slots_.size() != indexed) {
      return fail(why, "Parent components and the index differ in size", ecs::NullEntity);
    }
    if (markedWithoutChildren) {
      return fail(why, "HasChildren without children", markedEntity);
    }

    return true;
  }
}
