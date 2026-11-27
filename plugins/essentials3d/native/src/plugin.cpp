#include "oly/essentials3d/plugin.hpp"

#include <cstdio>

#include "oly/app/engine.hpp"
#include "oly/core/assert.hpp"
#include "oly/core/type_name.hpp"
#include "oly/essentials3d/schema.generated.hpp"

namespace oly::essentials3d {
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
    // The Hierarchy resource marks an earlier install and outlives its Engine. Installing again cannot work: imply refuses a
    // World where an archetype holds Position, and the old index stopped following the World with the old observers.
    OLY_ASSERT(engine.world().tryResource<detail::Hierarchy>() == nullptr, "install type=%.*s: the World already had it installed; essentials installs once per World",
               int(core::typeName<Plugin>().size()), core::typeName<Plugin>().data());

    world_ = &engine.world();
    detail::Hierarchy& hierarchy = world_->insertResource(detail::Hierarchy{});
    world_->imply<Position, WorldTransform>();
    observers_[0] = world_->onAdd<Parent>(onParentAdded, &hierarchy);
    observers_[1] = world_->onRemove<Parent>(onParentRemoved, &hierarchy);
    observers_[2] = world_->onAdd<WorldTransform>(onWorldTransformAdded, &hierarchy);
    observers_[3] = world_->onRemove<detail::HasChildren>(onHasChildrenRemoved, &hierarchy);
    engine.addPhase<Propagation>(app::FixedStep{}).after<app::Update>().before<app::UpdateAfter>();
    detail::addPropagation(engine);
  }

  bool Plugin::checkHierarchy(ecs::World& world, std::string* why) const {
    const detail::Hierarchy& hierarchy = world.resource<detail::Hierarchy>();
    size_t indexed = 0;

    for (const auto& [key, children] : hierarchy.children) {
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
        const auto recorded = hierarchy.slots.find(detail::Hierarchy::keyOf(child));

        if (link == nullptr || link->entity != parent) {
          return fail(why, "indexed child with another Parent", child);
        }
        if (recorded == hierarchy.slots.end() || recorded->second != slot) {
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
      if (hierarchy.children.find(detail::Hierarchy::keyOf(entity)) == hierarchy.children.end()) {
        markedWithoutChildren = true;
        markedEntity = entity;
      }
    });

    if (linked != indexed || hierarchy.slots.size() != indexed) {
      return fail(why, "Parent components and the index differ in size", ecs::NullEntity);
    }
    if (markedWithoutChildren) {
      return fail(why, "HasChildren without children", markedEntity);
    }

    return true;
  }
}
