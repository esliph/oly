#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oly/core/assert.hpp"
#include "oly/core/small_map.hpp"
#include "oly/ecs/command_buffer.hpp"
#include "oly/ecs/component.hpp"
#include "oly/ecs/detail/archetype.hpp"
#include "oly/ecs/detail/entity_store.hpp"
#include "oly/ecs/detail/implication_table.hpp"
#include "oly/ecs/detail/observer_registry.hpp"
#include "oly/ecs/detail/query_cache.hpp"
#include "oly/ecs/detail/table.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "oly/ecs/entity.hpp"
#include "oly/ecs/observer.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/query_by_id.hpp"

namespace oly::ecs {
  namespace detail {
    template <class...>
    inline constexpr bool distinctTypes = true;
    template <class T, class... Rest>
    inline constexpr bool distinctTypes<T, Rest...> = (!(std::is_same_v<T, Rest> || ...)) && distinctTypes<Rest...>;

    template <class... Us>
    void addFilter(std::vector<ComponentId>& with, std::vector<ComponentId>&, With<Us...>) {
      (with.push_back(componentId<Us>()), ...);
    }
    template <class... Us>
    void addFilter(std::vector<ComponentId>&, std::vector<ComponentId>& without, Without<Us...>) {
      (without.push_back(componentId<Us>()), ...);
    }
    inline void sortUnique(std::vector<ComponentId>& ids) {
      std::sort(ids.begin(), ids.end());
      ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    }

    struct WorldTestAccess;
  }

  class World {
   private:
    friend struct detail::WorldTestAccess;
    friend bool checkInvariants(const World& world, std::string* why);
    friend WorldStats stats(const World& world);
    friend uint64_t signatureLookupCount(const World& world);
    friend uint32_t archetypeCount(const World& world);

    struct QueryKey {
      std::vector<uint64_t> components; // (componentId << 1) | readOnly, in the order of Ts (or read followed by write)
      detail::Signature with, without;

      auto operator<=>(const QueryKey&) const = default;
    };

    detail::EntityStore store_;
    std::vector<std::unique_ptr<detail::Archetype>> archetypes_;
    std::unordered_map<detail::Signature, detail::Archetype*, detail::SignatureHash> bySignature_;
    std::vector<core::SmallMap<detail::ArchetypeId, uint16_t>> componentIndex_; // [componentId]: archetype → column (NoColumn for tags)

    uint32_t iterating_ = 0;
    mutable uint64_t signatureLookups_ = 0;

    std::map<QueryKey, std::unique_ptr<detail::QueryCacheBase>> queryCache_;
    std::map<QueryKey, std::unique_ptr<QueryById>> queryByIdCache_; // separate: the static_cast of queryCache_ would mix up the types
    std::vector<detail::QueryCacheBase*> queries_;

    std::unordered_map<uint64_t, detail::Edge> jumpEdges_; // (source.id << 32) | destination.id

    detail::ImplicationTable implications_;
    detail::ObserverRegistry observers_;
    CommandBuffer deferred_; // what observers record into
    CommandBuffer applying_; // what applyDeferred is flushing, so observers of that flush record into deferred_
    std::vector<std::pair<Entity, ComponentId>> pendingOnAdd_; // a flush runs on add after its last transition (R-OB-7)

   public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Structural changes (spawn, destroy, add, remove, the ById variants and flush) are a contract error
    // during an iteration and, except for spawn and flush, on a dead entity (OLY_ASSERT). Each one invalidates
    // pointers and spans of any entity (R-API-1); only Entity is a stable handle.
    Entity spawn();
    // Born directly in the final archetype. Lvalues are copied.
    template <class... Ts>
    Entity spawn(Ts&&... components);
    void destroy(Entity entity);
    bool isAlive(Entity entity) const noexcept {
      return store_.isAlive(entity);
    }
    bool isIterating() const noexcept {
      return iterating_ > 0;
    }

    template <class T>
    void add(Entity entity, T&& value); // or assigns, if it already has T
    template <class T>
    void remove(Entity entity); // no effect if the entity does not have T
    template <class T>
    bool has(Entity entity) const noexcept;
    template <class T>
    T& get(Entity entity); // OLY_ASSERT if the entity is dead or does not have T
    template <class T>
    T* tryGet(Entity entity) noexcept; // nullptr if the entity is dead or does not have T

    void addById(Entity entity, ComponentId componentId, void* value); // moves from `value` (the caller still owns the moved-from object); ignored for tags
    void removeById(Entity entity, ComponentId componentId);

    // The buffer can only be applied to this World (OLY_ASSERT on flush) and must not outlive it.
    CommandBuffer commands() {
      return CommandBuffer(store_);
    }
    // Two passes (R-CB-3): groups by entity and makes a single transition to the final archetype.
    // Commands for stale handles are ignored; the buffer ends up empty.
    void flush(CommandBuffer& commandBuffer);

    // Lifecycle observers (R-OB-1..8). On add runs after the component is in place; on remove runs before it
    // leaves, and destroy runs it for every observed component. Assigning an existing data component through add
    // runs on remove (old value) then on add (new value); direct writes through get or a query are not observed.
    // Observers of one component and kind run in registration order. Registering or removing an observer during
    // an iteration or inside an observer is a contract error (OLY_ASSERT), and so is removing an unknown id.
    template <class T>
    ObserverId onAdd(ObserverFunction function, void* userData = nullptr);
    template <class T>
    ObserverId onRemove(ObserverFunction function, void* userData = nullptr);
    ObserverId observeById(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData = nullptr);
    void removeObserver(ObserverId observer);

    // Adding T also adds Implied, value-initialized, in the same transition (R-IM-2); removing T removes it again
    // once no component left implies it (R-IM-3). Transitive; a cycle closes to one set. Registered before any
    // archetype contains T (R-IM-1). Removing Implied while T stays is a contract error (R-IM-4).
    template <class T, class Implied>
    void imply();

    // Cached by (Ts..., filters): same instance for the same combination, valid while the World exists.
    template <class... Ts, class... Filters>
    Query<Ts...>& query(Filters... filters);

    // Query by ComponentId, cached by spec (read/write keep the given order), valid while the World exists.
    QueryById& queryById(const QueryByIdSpec& spec);

    // Asserts if the entity is dead or the component is absent (a tag is also an error).
    void* getById(Entity entity, ComponentId componentId);

    void* tryGetById(Entity entity, ComponentId componentId) noexcept {
      if (!store_.isAlive(entity)) {
        return nullptr;
      }
      if (componentId >= componentIndex_.size()) {
        return nullptr;
      }

      const detail::EntityRecord& record = store_.record(entity.index);
      const uint16_t* column = componentIndex_[componentId].find(record.archetype->id);
      if (!column || *column == detail::NoColumn) {
        return nullptr;
      }

      return record.archetype->table.column(*column).at(record.row);
    }

    bool hasById(Entity entity, ComponentId componentId) const noexcept {
      if (!store_.isAlive(entity) || componentId >= componentIndex_.size()) {
        return false;
      }

      return componentIndex_[componentId].find(store_.record(entity.index).archetype->id) != nullptr;
    }

   private:
    // Oracle without coalescing: applies each command in recording order and clears the buffer.
    void flushSequential(CommandBuffer& commandBuffer);
    void flushWithoutDeferred(CommandBuffer& commandBuffer);
    // Flushes what observers recorded until nothing is pending. Never called while it runs: the paths it reaches
    // (flushWithoutDeferred and the helpers below) leave the commands their observers record in deferred_.
    // Out of line: inlined, the CommandBuffer temporary of its swap gave every addById a 200-byte frame.
    [[gnu::noinline]] void applyDeferred();

    // The caller holds no IterationGuard; these take one for the whole batch.
    void notifyAll(ObserverKind kind, Entity entity, const detail::Signature& signature);
    // Runs `kind` for each component of `having` absent from `lacking`: `first` before the others, which follow in
    // ComponentId order.
    void notifyDifference(ObserverKind kind, Entity entity, const detail::Signature& having, const detail::Signature& lacking, ComponentId first);
    void runObservers(ObserverKind kind, Entity entity, ComponentId componentId);
    void runPendingOnAdd();
    // Returns whether an observer ran (the caller then owes applyDeferred).
    bool destroyNotifying(Entity entity);
    // add of a component the entity has, and the cached-edge add and remove, when observers or implied columns are
    // involved. Kept out of line so the plain path keeps the base cost: inlined, they made addById spill its
    // arguments around moveEntity and lose its tail call (perf review of 2026-11-16).
    [[gnu::noinline]] void replaceNotifying(Entity entity, ComponentId componentId, void* value);
    [[gnu::noinline]] void addWithEdgeWork(Entity entity, detail::EntityRecord& record, const detail::Archetype& source, const detail::Edge& edge, ComponentId componentId, void* value);
    [[gnu::noinline]] void removeNotifying(Entity entity, const detail::Archetype& source, const detail::Edge& edge, ComponentId componentId);
    void refreshObservedFlags();

    detail::Archetype* findOrCreate(detail::Signature signature);
    // Out of line on purpose: inlined into spawn<Ts...>, the signature copy trips a false -Warray-bounds in GCC release
    // builds.
    detail::Archetype* findOrCreateClosure(const detail::Signature& signature);
    uint32_t moveEntity(Entity entity, detail::EntityRecord& record, detail::Archetype& destination, const detail::Edge& edge);

    detail::QueryCacheBase* registerQuery(QueryKey key, std::unique_ptr<detail::QueryCacheBase> query);
    const core::SmallMap<detail::ArchetypeId, uint16_t>* smallestIndex(const detail::Signature& with) const;

    // When removing, `componentId` must be in source.signature (the caller checks). `entity` only names the
    // entity in the R-IM-4 assert.
    const detail::Edge& edgeFor(detail::Archetype& source, ComponentId componentId, bool adding, Entity entity);
    // The destination of `source` after `changed`, closed under implication (`added` as in ImplicationTable::resolve);
    // asserts R-IM-4 for each component of `source` that `changed` lacks while one of its impliers stays (in `source` and in the result).
    detail::Signature resolveSignature(const detail::Signature& source, detail::Signature changed, Entity entity, std::span<const ComponentId> added = {}) const;
    void initializeImplied(detail::Archetype& archetype, uint32_t row, const detail::Signature& constructed);
    const detail::Edge& jumpEdge(detail::Archetype& source, detail::Archetype& destination);

    void materialize(Entity reserved);
    void destroyOrRelease(Entity entity, bool alive); // a reserved (not alive) entity only returns the index
    // The row is born dead if the entity was reserved. `edge` nullptr: reserved or already in `destination`.
    uint32_t transitionTo(Entity entity, detail::EntityRecord& record, detail::Archetype& destination, const detail::Edge* edge);

    Entity place(detail::Archetype& archetype); // the row is born dead: the caller constructs the columns
    detail::Archetype& root() noexcept {
      return *archetypes_[0];
    }

    bool checkInvariants(std::string* why) const;
    bool checkArchetypes(std::string* why) const;
    bool checkRows(const detail::Archetype& archetype, std::string* why) const;
    bool checkRecords(std::string* why) const;
    bool checkFreeList(std::string* why) const;
    bool checkComponentIndex(std::string* why) const;
    bool checkQueries(std::string* why) const;
    bool checkEdges(std::string* why) const;
    bool checkObservedFlags(std::string* why) const;
    bool checkImplications(std::string* why) const;
  };

  template <class... Ts>
  Entity World::spawn(Ts&&... components) {
    static_assert(detail::distinctTypes<std::remove_cvref_t<Ts>...>, "spawn<Ts...>: repeated types");
    OLY_ASSERT(iterating_ == 0, "spawn during iteration=%u", iterating_);

    detail::Signature signature{componentId<Ts>()...};
    std::sort(signature.begin(), signature.end());
    const bool hasImplications = !implications_.empty();
    detail::Archetype* archetype = hasImplications ? findOrCreateClosure(signature) : findOrCreate(std::move(signature));
    Entity entity = place(*archetype);

    const uint32_t row = store_.record(entity.index).row;
    (
      [&] {
        using U = std::remove_cvref_t<Ts>;
        if constexpr (!std::is_empty_v<U>) {
          void* slot = archetype->table.column(detail::columnOf(*archetype, componentId<U>())).at(row);
          ::new (slot) U(std::forward<Ts>(components));
        }
      }(),
      ...);
    if (hasImplications) {
      initializeImplied(*archetype, row, signature);
    }

    if (archetype->observed & detail::ObservedOnAdd) {
      notifyAll(ObserverKind::OnAdd, entity, archetype->signature);
      applyDeferred();
    }

    return entity;
  }

  template <class T>
  void World::add(Entity entity, T&& value) {
    using U = std::remove_cvref_t<T>;
    const ComponentId componentId = oly::ecs::componentId<U>();
    OLY_ASSERT(isAlive(entity) && iterating_ == 0, "add e=%u:%u type=%.*s", entity.index, entity.generation,
               int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data());

    if constexpr (std::is_empty_v<U>) {
      addById(entity, componentId, nullptr);
    } else {
      // Temporary built BEFORE any structural change: `value` may point
      // into the World itself (e.g. get<B>(another)).
      U temporary(std::forward<T>(value));
      addById(entity, componentId, &temporary);
    }
  }

  template <class T>
  void World::remove(Entity entity) {
    const ComponentId componentId = oly::ecs::componentId<T>();
    OLY_ASSERT(isAlive(entity), "remove e=%u:%u type=%.*s", entity.index, entity.generation, int(typeInfo(componentId).name.size()),
               typeInfo(componentId).name.data());

    removeById(entity, componentId);
  }

  template <class T>
  ObserverId World::onAdd(ObserverFunction function, void* userData) {
    const ComponentId componentId = oly::ecs::componentId<T>();
    OLY_ASSERT(iterating_ == 0, "onAdd type=%.*s during iteration=%u", int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data(),
               iterating_);

    return observeById(ObserverKind::OnAdd, componentId, function, userData);
  }

  template <class T>
  ObserverId World::onRemove(ObserverFunction function, void* userData) {
    const ComponentId componentId = oly::ecs::componentId<T>();
    OLY_ASSERT(iterating_ == 0, "onRemove type=%.*s during iteration=%u", int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data(),
               iterating_);

    return observeById(ObserverKind::OnRemove, componentId, function, userData);
  }

  template <class T, class Implied>
  void World::imply() {
    static_assert(!std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<Implied>>, "imply<T, T>: a component cannot imply itself");
    static_assert(std::is_nothrow_default_constructible_v<std::remove_cvref_t<Implied>>, "imply<T, Implied>: Implied must be nothrow default constructible");
    using U = std::remove_cvref_t<Implied>;
    const ComponentId implying = componentId<T>(), implied = componentId<U>();
    OLY_ASSERT(iterating_ == 0, "imply type=%.*s during iteration=%u", int(typeInfo(implying).name.size()), typeInfo(implying).name.data(), iterating_);
    OLY_ASSERT(implying >= componentIndex_.size() || componentIndex_[implying].size() == 0, "imply type=%.*s after an archetype contains it",
               int(typeInfo(implying).name.size()), typeInfo(implying).name.data());

    implications_.add(implying, implied, [](void* slot) noexcept { ::new (slot) U(); });
  }

  template <class T>
  bool World::has(Entity entity) const noexcept {
    return hasById(entity, componentId<T>());
  }

  template <class T>
  T& World::get(Entity entity) {
    using U = std::remove_cvref_t<T>;
    static_assert(!std::is_empty_v<U>, "get<Tag> is not allowed");
    const ComponentId componentId = oly::ecs::componentId<U>();
    OLY_ASSERT(isAlive(entity) && hasById(entity, componentId), "get e=%u:%u type=%.*s", entity.index, entity.generation,
               int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data());

    return *static_cast<U*>(tryGetById(entity, componentId));
  }

  template <class T>
  T* World::tryGet(Entity entity) noexcept {
    using U = std::remove_cvref_t<T>;
    static_assert(!std::is_empty_v<U>, "tryGet<Tag> is not allowed");
    return static_cast<U*>(tryGetById(entity, componentId<U>()));
  }

  template <class... Ts, class... Filters>
  Query<Ts...>& World::query(Filters... filters) {
    using Q = Query<Ts...>;

    detail::Signature with{componentId<std::remove_const_t<Ts>>()...}, without;
    (detail::addFilter(with, without, filters), ...);
    detail::sortUnique(with);
    detail::sortUnique(without);

    QueryKey key{std::vector<uint64_t>{(uint64_t(componentId<std::remove_const_t<Ts>>()) << 1 | uint64_t(std::is_const_v<Ts>))...},
                 with, without};
    if (auto it = queryCache_.find(key); it != queryCache_.end()) {
      return static_cast<Q&>(*it->second);
    }

    auto created = std::make_unique<Q>(&iterating_, with, without);
    created->initialize(archetypes_, smallestIndex(with));
    Q& reference = *created;
    registerQuery(std::move(key), std::move(created));
    return reference;
  }
}
