#include "oly/ecs/world.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace oly::ecs {
  namespace {
    bool checkEdgeMapping(const detail::Archetype& source, const detail::Archetype& destination, const detail::Edge& edge) {
      if (edge.sourceToDestination.size() != source.columnTypes.size()) {
        return false;
      }

      for (size_t i = 0; i < source.columnTypes.size(); ++i) {
        const uint16_t column = detail::columnOf(destination, source.columnTypes[i]);
        const int16_t expected = column == detail::NoColumn ? int16_t(-1) : static_cast<int16_t>(column);
        if (edge.sourceToDestination[i] != expected) {
          return false;
        }
      }

      return true;
    }

    bool fail(std::string* why, const char* invariantId, const std::string& what) {
      if (why) {
        *why = std::string(invariantId) + " " + what;
      }

      return false;
    }

    std::string entityLabel(uint32_t index, uint32_t generation) {
      return "e=" + std::to_string(index) + ":" + std::to_string(generation);
    }

    std::string archetypeLabel(const detail::Archetype& archetype) {
      return "arch=" + std::to_string(archetype.id);
    }

    bool checkTableShape(const detail::Archetype& archetype, std::string* why) {
      const detail::Table& table = archetype.table;
      if (table.entities().size() != table.count() || table.capacity() < table.count()) {
        return fail(why, "I-1", archetypeLabel(archetype));
      }

      return true;
    }

    bool checkColumnTypes(const detail::Archetype& archetype, std::string* why) {
      if (!std::is_sorted(archetype.signature.begin(), archetype.signature.end()) || std::adjacent_find(archetype.signature.begin(), archetype.signature.end()) != archetype.signature.end()) {
        return fail(why, "I-5", archetypeLabel(archetype) + " signature");
      }

      std::vector<ComponentId> dataComponents;
      for (ComponentId componentId : archetype.signature) {
        if (!typeInfo(componentId).tag) {
          dataComponents.push_back(componentId);
        }
      }

      if (dataComponents != archetype.columnTypes) {
        return fail(why, "I-5", archetypeLabel(archetype) + " columnTypes");
      }

      const detail::Table& table = archetype.table;
      if (table.columnCount() != archetype.columnTypes.size() || !std::equal(table.columnTypes().begin(), table.columnTypes().end(), archetype.columnTypes.begin(), archetype.columnTypes.end())) {
        return fail(why, "I-5", archetypeLabel(archetype) + " table");
      }

      return true;
    }

    detail::Signature edgeSignature(detail::Signature signature, ComponentId componentId, bool adding) {
      auto position = std::lower_bound(signature.begin(), signature.end(), componentId);
      if (adding) {
        signature.insert(position, componentId);
      } else if (position != signature.end() && *position == componentId) {
        signature.erase(position);
      }

      return signature;
    }

    std::vector<uint16_t> expectedInitializedColumns(const detail::Archetype& source, const detail::Archetype& destination, ComponentId added) {
      std::vector<uint16_t> columns;
      for (uint16_t column = 0; column < destination.columnTypes.size(); ++column) {
        const ComponentId componentId = destination.columnTypes[column];
        if (componentId != added && !std::binary_search(source.signature.begin(), source.signature.end(), componentId)) {
          columns.push_back(column);
        }
      }

      return columns;
    }

    bool checkCachedEdges(const detail::Archetype& archetype, const detail::ImplicationTable& implications, std::string* why) {
      for (int adding = 1; adding >= 0; --adding) {
        for (const auto& [componentId, edge] : adding ? archetype.addEdges : archetype.removeEdges) {
          const std::string where = std::string(adding ? "add" : "remove") + " cid=" + std::to_string(componentId) + " " + archetypeLabel(archetype);
          if (!edge.target) {
            return fail(why, "I-9", where + " no destination");
          }

          const detail::Archetype& target = *edge.target;
          if (target.signature != implications.resolve(archetype.signature, edgeSignature(archetype.signature, componentId, adding))) {
            return fail(why, "I-9", where + " signature");
          }
          if (!checkEdgeMapping(archetype, target, edge)) {
            return fail(why, "I-9", where + " sourceToDestination");
          }
          if (edge.initializedColumns != expectedInitializedColumns(archetype, target, adding ? componentId : detail::NoComponent)) {
            return fail(why, "I-9", where + " initializedColumns");
          }

          const int16_t expectedAddedColumn = adding && !typeInfo(componentId).tag ? static_cast<int16_t>(detail::columnOf(target, componentId)) : int16_t(-1);
          if (edge.addedColumn != expectedAddedColumn) {
            return fail(why, "I-9", where + " addedColumn");
          }
        }
      }

      return true;
    }

    // Memo of the last flush result: entities in a batch tend to share the same (source, operations),
    // so the final signature, the edge and the columns are computed only once.
    struct FlushMemo {
      bool valid = false;
      const detail::Archetype* source = nullptr;
      std::vector<std::pair<ComponentId, bool>> key;

      detail::Archetype* destination = nullptr;
      const detail::Edge* edge = nullptr; // nullptr if source == destination or the entity is reserved
      std::vector<detail::Column*> columns; // per operation: column in the destination (nullptr: no value to construct)
      std::vector<uint8_t> existed; // per operation: the component already existed in the source
      // Implied components that neither the source nor an operation provides (R-IM-2).
      std::vector<std::pair<detail::Column*, detail::ValueInitializer>> initialized;

      // Net change seen by observers (R-OB-7): what the transition removes or adds, plus the data components it
      // assigns again (on remove with the old value, then on add with the new one). In ComponentId order.
      uint8_t observed = 0;
      std::vector<ComponentId> onRemove, onAdd;

      bool matches(const detail::Archetype* candidateSource, std::span<const detail::NetComponentOperation> operations) const {
        if (!valid || source != candidateSource || key.size() != operations.size()) {
          return false;
        }

        for (size_t i = 0; i < operations.size(); ++i) {
          if (key[i].first != operations[i].componentId || key[i].second != operations[i].present) {
            return false;
          }
        }

        return true;
      }

      void assign(const detail::Archetype* newSource, std::span<const detail::NetComponentOperation> operations, detail::Archetype* newDestination, const detail::Edge* newEdge, const detail::ImplicationTable& implications) {
        source = newSource;
        destination = newDestination;
        edge = newEdge;

        key.clear();
        columns.clear();
        existed.clear();
        for (const detail::NetComponentOperation& operation : operations) {
          key.emplace_back(operation.componentId, operation.present);
          columns.push_back(operation.present && !typeInfo(operation.componentId).tag ? &destination->table.column(detail::columnOf(*destination, operation.componentId)) : nullptr);
          existed.push_back(source && std::binary_search(source->signature.begin(), source->signature.end(), operation.componentId));
        }

        initialized.clear();
        if (!implications.empty()) {
          for (uint16_t column = 0; column < destination->columnTypes.size(); ++column) {
            const ComponentId componentId = destination->columnTypes[column];
            const bool inSource = source && detail::columnOf(*source, componentId) != detail::NoColumn;
            const bool written = std::find(columns.begin(), columns.end(), &destination->table.column(column)) != columns.end();
            if (!inSource && !written) {
              initialized.emplace_back(&destination->table.column(column), implications.initializerOf(componentId));
            }
          }
        }

        valid = true;
      }

      void assignObservers(const detail::ObserverRegistry& observers, std::span<const detail::NetComponentOperation> operations) {
        onRemove.clear();
        onAdd.clear();
        observed = 0;
        if (!((source ? source->observed : 0) | destination->observed)) {
          return;
        }

        static const detail::Signature noComponents;
        const detail::Signature& sourceSignature = source ? source->signature : noComponents;
        const detail::Signature& destinationSignature = destination->signature;

        auto assignedAgain = [&](ComponentId componentId) {
          auto operation = std::lower_bound(operations.begin(), operations.end(), componentId,
                                            [](const detail::NetComponentOperation& candidate, ComponentId target) { return candidate.componentId < target; });
          return operation != operations.end() && operation->componentId == componentId && operation->present && !typeInfo(componentId).tag;
        };

        for (ComponentId componentId : sourceSignature) {
          const bool leaves = !std::binary_search(destinationSignature.begin(), destinationSignature.end(), componentId);
          if ((observers.mask(componentId) & detail::ObservedOnRemove) && (leaves || assignedAgain(componentId))) {
            onRemove.push_back(componentId);
          }
        }
        for (ComponentId componentId : destinationSignature) {
          const bool arrives = !std::binary_search(sourceSignature.begin(), sourceSignature.end(), componentId);
          if ((observers.mask(componentId) & detail::ObservedOnAdd) && (arrives || assignedAgain(componentId))) {
            onAdd.push_back(componentId);
          }
        }

        observed = uint8_t((onRemove.empty() ? 0 : detail::ObservedOnRemove) | (onAdd.empty() ? 0 : detail::ObservedOnAdd));
      }
    };

    detail::Signature applyOperations(detail::Signature signature, std::span<const detail::NetComponentOperation> operations) {
      for (const detail::NetComponentOperation& operation : operations) {
        auto position = std::lower_bound(signature.begin(), signature.end(), operation.componentId);
        const bool has = position != signature.end() && *position == operation.componentId;
        if (operation.present && !has) {
          signature.insert(position, operation.componentId);
        } else if (!operation.present && has) {
          signature.erase(position);
        }
      }

      return signature;
    }

    // `slot` must not be constructed; `value` is left moved-from, and destroying it is up to the caller.
    void constructValue(const TypeInfo& info, void* slot, void* value) noexcept {
      if (!info.trivial) {
        info.moveConstruct(slot, value);
        return;
      }

      // Small, common sizes inline: avoids the variable-size memcpy call.
      switch (info.size) {
      case 4: std::memcpy(slot, value, 4); break;
      case 8: std::memcpy(slot, value, 8); break;
      case 12: std::memcpy(slot, value, 12); break;
      case 16: std::memcpy(slot, value, 16); break;
      default: std::memcpy(slot, value, info.size);
      }
    }

    void writeValues(const FlushMemo& memo, std::span<const detail::NetComponentOperation> operations, uint32_t row) {
      for (size_t i = 0; i < operations.size(); ++i) {
        if (!memo.columns[i]) {
          continue;
        }

        detail::Column& column = *memo.columns[i];
        void* slot = column.at(row);
        if (memo.existed[i]) {
          detail::destroyValue(*column.info, slot);
        }

        constructValue(*column.info, slot, operations[i].payload);
      }
    }

    // The first direct implier of `implied` present in `signature`; NoComponent if none.
    ComponentId implierIn(const detail::ImplicationTable& implications, const detail::Signature& signature, ComponentId implied) {
      for (ComponentId implier : implications.impliersOf(implied)) {
        if (std::binary_search(signature.begin(), signature.end(), implier)) {
          return implier;
        }
      }

      return detail::NoComponent;
    }

    void validateQueryByIdComponents(const std::vector<ComponentId>& read, const std::vector<ComponentId>& write) {
      std::vector<ComponentId> seen;
      for (const std::vector<ComponentId>* group : {&read, &write}) {
        for (ComponentId componentId : *group) {
          OLY_ASSERT(componentId < registeredTypeCount(), "queryById: unknown component id=%u", componentId);
          [[maybe_unused]] const TypeInfo& info = typeInfo(componentId);
          OLY_ASSERT(!info.tag, "queryById: tag in read/write is not allowed type=%.*s", int(info.name.size()), info.name.data());
          OLY_ASSERT(std::find(seen.begin(), seen.end(), componentId) == seen.end(), "queryById: repeated component type=%.*s",
                     int(info.name.size()), info.name.data());
          seen.push_back(componentId);
        }
      }
    }
  }

  World::World()
    : deferred_(store_),
      applying_(store_) {
    findOrCreate({}); // the first archetype gets id 0: it is what root() returns (R-AR-3)
  }

  World::~World() = default;

  detail::Archetype* World::findOrCreate(detail::Signature signature) {
    ++signatureLookups_;
    if (auto it = bySignature_.find(signature); it != bySignature_.end()) {
      return it->second;
    }

    const auto id = static_cast<detail::ArchetypeId>(archetypes_.size());
    archetypes_.push_back(std::make_unique<detail::Archetype>(id, signature));
    detail::Archetype* archetype = archetypes_.back().get();
    archetype->observed = observers_.signatureMask(archetype->signature);
    bySignature_.emplace(std::move(signature), archetype);

    for (ComponentId componentId : archetype->signature) {
      if (componentId >= componentIndex_.size()) {
        componentIndex_.resize(size_t(componentId) + 1);
      }
      componentIndex_[componentId].insert(id, detail::columnOf(*archetype, componentId));
    }

    for (detail::QueryCacheBase* query : queries_) {
      query->onArchetypeCreated(*archetype);
    }

    return archetype;
  }

  Entity World::place(detail::Archetype& archetype) {
    Entity entity = store_.allocate();
    detail::EntityRecord& record = store_.record(entity.index);

    record.row = archetype.table.pushUninitialized(entity);
    record.archetype = &archetype;

    return entity;
  }

  Entity World::spawn() {
    OLY_ASSERT(iterating_ == 0, "spawn during iteration=%u", iterating_);
    return place(root());
  }

  void World::destroy(Entity entity) {
    OLY_ASSERT(isAlive(entity) && iterating_ == 0, "destroy e=%u:%u during iteration=%u", entity.index, entity.generation, iterating_);

    if (destroyNotifying(entity)) {
      applyDeferred();
    }
  }

  bool World::destroyNotifying(Entity entity) {
    detail::Archetype& archetype = *store_.record(entity.index).archetype;
    const bool observed = archetype.observed & detail::ObservedOnRemove;
    if (observed) {
      notifyAll(ObserverKind::OnRemove, entity, archetype.signature);
    }

    const uint32_t row = store_.record(entity.index).row;
    archetype.table.destroyRow(row);
    if (auto moved = archetype.table.fillHole(row)) {
      store_.record(moved->index).row = row;
    }

    store_.release(entity.index);
    return observed;
  }

  uint32_t World::moveEntity(Entity entity, detail::EntityRecord& record, detail::Archetype& destination, const detail::Edge& edge) {
    detail::Archetype& source = *record.archetype;
    const uint32_t sourceRow = record.row;
    const uint32_t destinationRow = destination.table.pushUninitialized(entity);

    for (uint16_t columnIndex = 0; columnIndex < source.table.columnCount(); ++columnIndex) {
      detail::Column& sourceColumn = source.table.column(columnIndex);
      const int16_t destinationColumn = edge.sourceToDestination[columnIndex];
      if (destinationColumn >= 0) {
        detail::relocateValue(*sourceColumn.info, destination.table.column(static_cast<uint16_t>(destinationColumn)).at(destinationRow), sourceColumn.at(sourceRow));
      } else {
        detail::destroyValue(*sourceColumn.info, sourceColumn.at(sourceRow));
      }
    }

    if (auto moved = source.table.fillHole(sourceRow)) {
      store_.record(moved->index).row = sourceRow;
    }

    record.archetype = &destination;
    record.row = destinationRow;

    return destinationRow;
  }

  void World::addById(Entity entity, ComponentId componentId, void* value) {
    const TypeInfo& info = typeInfo(componentId);
    OLY_ASSERT(isAlive(entity) && iterating_ == 0, "add e=%u:%u type=%.*s", entity.index, entity.generation, int(info.name.size()),
               info.name.data());

    detail::EntityRecord& record = store_.record(entity.index);
    detail::Archetype& source = *record.archetype;

    if (std::binary_search(source.signature.begin(), source.signature.end(), componentId)) {
      if (info.tag) {
        return;
      }

      const uint8_t observed = source.observed ? observers_.mask(componentId) : 0;
      if (observed & detail::ObservedOnRemove) {
        detail::IterationGuard guard(&iterating_);
        runObservers(ObserverKind::OnRemove, entity, componentId);
      }

      void* slot = source.table.column(detail::columnOf(source, componentId)).at(store_.record(entity.index).row);
      detail::destroyValue(info, slot);
      constructValue(info, slot, value);

      if (observed & detail::ObservedOnAdd) {
        detail::IterationGuard guard(&iterating_);
        runObservers(ObserverKind::OnAdd, entity, componentId);
      }
      if (observed) {
        applyDeferred();
      }
      return;
    }

    const detail::Edge& edge = edgeFor(source, componentId, true, entity);
    detail::Archetype& destination = *edge.target;
    const uint32_t destinationRow = moveEntity(entity, record, destination, edge);

    if (edge.addedColumn >= 0) {
      void* slot = destination.table.column(static_cast<uint16_t>(edge.addedColumn)).at(destinationRow);
      constructValue(info, slot, value);
    }
    for (uint16_t column : edge.initializedColumns) {
      implications_.initializerOf(destination.columnTypes[column])(destination.table.column(column).at(destinationRow));
    }

    if (edge.observed & detail::ObservedOnAdd) {
      notifyDifference(ObserverKind::OnAdd, entity, destination.signature, source.signature, componentId);
      applyDeferred();
    }
  }

  void* World::getById(Entity entity, ComponentId componentId) {
    void* value = tryGetById(entity, componentId);
    OLY_ASSERT(value != nullptr, "getById e=%u:%u type=%.*s", entity.index, entity.generation, int(typeInfo(componentId).name.size()),
               typeInfo(componentId).name.data());

    return value;
  }

  void World::removeById(Entity entity, ComponentId componentId) {
    OLY_ASSERT(isAlive(entity) && iterating_ == 0, "remove e=%u:%u type=%.*s", entity.index, entity.generation,
               int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data());

    detail::EntityRecord& record = store_.record(entity.index);
    detail::Archetype& source = *record.archetype;
    if (!std::binary_search(source.signature.begin(), source.signature.end(), componentId)) {
      return;
    }

    const detail::Edge& edge = edgeFor(source, componentId, false, entity);
    if (!(edge.observed & detail::ObservedOnRemove)) {
      moveEntity(entity, record, *edge.target, edge);
      return;
    }

    notifyDifference(ObserverKind::OnRemove, entity, source.signature, edge.target->signature, componentId);
    // Read again: an observer's commands().spawn() reserves an index and may reallocate the records.
    moveEntity(entity, store_.record(entity.index), *edge.target, edge);
    applyDeferred();
  }

  const detail::Edge& World::edgeFor(detail::Archetype& source, ComponentId componentId, bool adding, Entity entity) {
    core::SmallMap<ComponentId, detail::Edge>& edges = adding ? source.addEdges : source.removeEdges;
    if (const detail::Edge* cached = edges.find(componentId)) {
      return *cached;
    }

    detail::Signature signature = source.signature;
    auto position = std::lower_bound(signature.begin(), signature.end(), componentId);
    if (adding) {
      signature.insert(position, componentId);
    } else {
      signature.erase(position);
    }

    detail::Archetype* destination = findOrCreate(resolveSignature(source.signature, std::move(signature), entity));
    detail::Edge edge = detail::buildEdge(source, *destination, adding ? componentId : detail::NoComponent);
    edge.observed = observers_.transitionMask(source.signature, destination->signature);
    return edges.insert(componentId, std::move(edge));
  }

  detail::Signature World::resolveSignature(const detail::Signature& source, detail::Signature changed, Entity entity) const {
    if (implications_.empty()) {
      return changed;
    }

    detail::Signature resolved = implications_.resolve(source, changed);
    for (ComponentId componentId : source) {
      if (std::binary_search(changed.begin(), changed.end(), componentId)) {
        continue;
      }

      const ComponentId implier = implierIn(implications_, resolved, componentId);
      OLY_ASSERT(implier == detail::NoComponent, "remove e=%u:%u type=%.*s implied by %.*s", entity.index, entity.generation,
                 int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data(), int(typeInfo(implier).name.size()), typeInfo(implier).name.data());
      if (implier != detail::NoComponent) {
        return changed; // asserts off: the removal proceeds and I-13 reports the archetype
      }
    }

    return resolved;
  }

  void World::initializeImplied(detail::Archetype& archetype, uint32_t row, const detail::Signature& constructed) {
    for (uint16_t column = 0; column < archetype.columnTypes.size(); ++column) {
      const ComponentId componentId = archetype.columnTypes[column];
      if (!std::binary_search(constructed.begin(), constructed.end(), componentId)) {
        implications_.initializerOf(componentId)(archetype.table.column(column).at(row));
      }
    }
  }

  ObserverId World::observeById(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData) {
    OLY_ASSERT(componentId < registeredTypeCount(), "observeById: unknown component id=%u", componentId);
    OLY_ASSERT(iterating_ == 0, "observeById type=%.*s during iteration=%u", int(typeInfo(componentId).name.size()), typeInfo(componentId).name.data(),
               iterating_);

    const ObserverId observer = observers_.add(kind, componentId, function, userData);
    refreshObservedFlags();
    return observer;
  }

  void World::removeObserver(ObserverId observer) {
    OLY_ASSERT(iterating_ == 0, "removeObserver id=%u during iteration=%u", observer, iterating_);

    observers_.remove(observer);
    refreshObservedFlags();
  }

  void World::refreshObservedFlags() {
    for (const auto& owned : archetypes_) {
      detail::Archetype& archetype = *owned;
      archetype.observed = observers_.signatureMask(archetype.signature);
      for (auto* edges : {&archetype.addEdges, &archetype.removeEdges}) {
        for (auto& [componentId, edge] : *edges) {
          edge.observed = observers_.transitionMask(archetype.signature, edge.target->signature);
        }
      }
    }
  }

  void World::runObservers(ObserverKind kind, Entity entity, ComponentId componentId) {
    for (const detail::ObserverEntry& entry : observers_.observers(kind, componentId)) {
      ObserverContext context(*this, entity, componentId, entry.userData, deferred_);
      entry.function(context);
    }
  }

  void World::notifyAll(ObserverKind kind, Entity entity, const detail::Signature& signature) {
    detail::IterationGuard guard(&iterating_);
    for (ComponentId componentId : signature) {
      if (observers_.mask(componentId) & detail::observedBit(kind)) {
        runObservers(kind, entity, componentId);
      }
    }
  }

  void World::notifyDifference(ObserverKind kind, Entity entity, const detail::Signature& having, const detail::Signature& lacking, ComponentId first) {
    detail::IterationGuard guard(&iterating_);
    const uint8_t bit = detail::observedBit(kind);
    if (observers_.mask(first) & bit) {
      runObservers(kind, entity, first);
    }

    for (ComponentId componentId : having) {
      if (componentId != first && (observers_.mask(componentId) & bit) && !std::binary_search(lacking.begin(), lacking.end(), componentId)) {
        runObservers(kind, entity, componentId);
      }
    }
  }

  void World::applyDeferred() {
    while (!deferred_.empty()) {
      std::swap(deferred_, applying_);
      flushWithoutDeferred(applying_);
    }
  }

  bool World::checkInvariants(std::string* why) const {
    if (iterating_ != 0) {
      return fail(why, "I-11", "iterating=" + std::to_string(iterating_));
    }

    return checkArchetypes(why) && checkRecords(why) && checkFreeList(why) && checkComponentIndex(why) && checkQueries(why)
      && checkEdges(why) && checkObservedFlags(why) && checkImplications(why);
  }

  bool World::checkImplications(std::string* why) const {
    for (const auto& owned : archetypes_) {
      const detail::Signature& signature = owned->signature;
      for (ComponentId componentId : signature) {
        for (ComponentId implied : implications_.implicationsOf(componentId)) {
          if (!std::binary_search(signature.begin(), signature.end(), implied)) {
            return fail(why, "I-13", archetypeLabel(*owned) + " cid=" + std::to_string(componentId) + " lacks cid=" + std::to_string(implied));
          }
        }
      }
    }

    return true;
  }

  bool World::checkObservedFlags(std::string* why) const {
    for (const auto& owned : archetypes_) {
      const detail::Archetype& archetype = *owned;
      if (archetype.observed != observers_.signatureMask(archetype.signature)) {
        return fail(why, "I-12", archetypeLabel(archetype));
      }

      for (int adding = 1; adding >= 0; --adding) {
        for (const auto& [componentId, edge] : adding ? archetype.addEdges : archetype.removeEdges) {
          if (edge.observed != observers_.transitionMask(archetype.signature, edge.target->signature)) {
            return fail(why, "I-12", std::string(adding ? "add" : "remove") + " cid=" + std::to_string(componentId) + " " + archetypeLabel(archetype));
          }
        }
      }
    }

    return true;
  }

  bool World::checkArchetypes(std::string* why) const {
    for (size_t i = 0; i < archetypes_.size(); ++i) {
      const detail::Archetype& archetype = *archetypes_[i];

      auto it = bySignature_.find(archetype.signature);
      if (archetype.id != i || it == bySignature_.end() || it->second != &archetype) {
        return fail(why, "I-6", archetypeLabel(archetype));
      }

      if (!checkTableShape(archetype, why) || !checkColumnTypes(archetype, why) || !checkRows(archetype, why)) {
        return false;
      }
    }

    if (bySignature_.size() != archetypes_.size()) {
      return fail(why, "I-6", "different sizes");
    }

    return true;
  }

  bool World::checkRows(const detail::Archetype& archetype, std::string* why) const {
    const detail::Table& table = archetype.table;
    for (uint32_t row = 0; row < table.count(); ++row) {
      Entity entity = table.entities()[row];
      if (entity.index >= store_.slotCount()) {
        return fail(why, "I-4", archetypeLabel(archetype) + " row=" + std::to_string(row));
      }

      const detail::EntityRecord& record = store_.record(entity.index);
      if (record.archetype != &archetype || record.row != row || record.generation != entity.generation) {
        return fail(why, "I-4", entityLabel(entity.index, entity.generation) + " " + archetypeLabel(archetype) + " row=" + std::to_string(row));
      }
    }

    return true;
  }

  bool World::checkRecords(std::string* why) const {
    for (uint32_t i = 0; i < store_.slotCount(); ++i) {
      const detail::EntityRecord& record = store_.record(i);
      if (!record.archetype) {
        continue;
      }

      const detail::Table& table = record.archetype->table;
      if (record.row >= table.count() || table.entities()[record.row] != Entity{i, record.generation}) {
        return fail(why, "I-3", entityLabel(i, record.generation));
      }
    }

    return true;
  }

  bool World::checkFreeList(std::string* why) const {
    std::vector<uint32_t> freeList(store_.freeList().begin(), store_.freeList().end());
    for (uint32_t slot : freeList) {
      if (slot >= store_.slotCount() || store_.record(slot).archetype != nullptr) {
        return fail(why, "I-10", "slot=" + std::to_string(slot));
      }
    }

    std::sort(freeList.begin(), freeList.end());
    if (std::adjacent_find(freeList.begin(), freeList.end()) != freeList.end()) {
      return fail(why, "I-10", "repeated");
    }

    return true;
  }

  bool World::checkComponentIndex(std::string* why) const {
    for (const auto& owned : archetypes_) {
      for (ComponentId componentId : owned->signature) {
        const uint16_t* column = componentId < componentIndex_.size() ? componentIndex_[componentId].find(owned->id) : nullptr;
        if (!column || *column != detail::columnOf(*owned, componentId)) {
          return fail(why, "I-7", "cid=" + std::to_string(componentId) + " " + archetypeLabel(*owned));
        }
      }
    }

    for (size_t componentId = 0; componentId < componentIndex_.size(); ++componentId) {
      for (const auto& [archetypeId, column] : componentIndex_[componentId]) {
        if (archetypeId >= archetypes_.size()) {
          return fail(why, "I-7", "cid=" + std::to_string(componentId));
        }

        const detail::Archetype& archetype = *archetypes_[archetypeId];
        if (!std::binary_search(archetype.signature.begin(), archetype.signature.end(), ComponentId(componentId)) || column != detail::columnOf(archetype, ComponentId(componentId))) {
          return fail(why, "I-7", "cid=" + std::to_string(componentId) + " " + archetypeLabel(archetype));
        }
      }
    }

    return true;
  }

  bool World::checkQueries(std::string* why) const {
    for (size_t queryIndex = 0; queryIndex < queries_.size(); ++queryIndex) {
      for (const auto& owned : archetypes_) {
        if (queries_[queryIndex]->isMatched(owned.get()) != queries_[queryIndex]->matches(*owned)) {
          return fail(why, "I-8", "query=" + std::to_string(queryIndex) + " " + archetypeLabel(*owned));
        }
      }
    }

    return true;
  }

  bool World::checkEdges(std::string* why) const {
    for (const auto& owned : archetypes_) {
      if (!checkCachedEdges(*owned, implications_, why)) {
        return false;
      }
    }

    for (const auto& [key, edge] : jumpEdges_) {
      const uint32_t sourceId = static_cast<uint32_t>(key >> 32), destinationId = static_cast<uint32_t>(key);
      const std::string where = "jump " + std::to_string(sourceId) + "->" + std::to_string(destinationId);
      if (sourceId >= archetypes_.size() || destinationId >= archetypes_.size()) {
        return fail(why, "I-9", where + " id");
      }
      if (edge.target != archetypes_[destinationId].get()) {
        return fail(why, "I-9", where + " destination");
      }
      if (!checkEdgeMapping(*archetypes_[sourceId], *archetypes_[destinationId], edge)) {
        return fail(why, "I-9", where + " sourceToDestination");
      }
      if (edge.addedColumn != -1) {
        return fail(why, "I-9", where + " addedColumn");
      }
      if (edge.initializedColumns != expectedInitializedColumns(*archetypes_[sourceId], *archetypes_[destinationId], detail::NoComponent)) {
        return fail(why, "I-9", where + " initializedColumns");
      }
    }

    return true;
  }

  QueryById& World::queryById(const QueryByIdSpec& spec) {
    validateQueryByIdComponents(spec.read, spec.write);

    std::vector<ComponentId> components = spec.read;
    components.insert(components.end(), spec.write.begin(), spec.write.end());

    detail::Signature with = components, without = spec.without;
    with.insert(with.end(), spec.with.begin(), spec.with.end());
    detail::sortUnique(with);
    detail::sortUnique(without);

    QueryKey key{{}, with, without};
    for (ComponentId componentId : spec.read) {
      key.components.push_back(uint64_t(componentId) << 1 | 1);
    }
    for (ComponentId componentId : spec.write) {
      key.components.push_back(uint64_t(componentId) << 1);
    }
    if (auto it = queryByIdCache_.find(key); it != queryByIdCache_.end()) {
      return *it->second;
    }

    auto created = std::make_unique<QueryById>(&iterating_, std::move(components), with, std::move(without));
    created->initialize(archetypes_, smallestIndex(with));
    QueryById& reference = *created;
    queries_.push_back(&reference);
    queryByIdCache_.emplace(std::move(key), std::move(created));
    return reference;
  }

  detail::QueryCacheBase* World::registerQuery(QueryKey key, std::unique_ptr<detail::QueryCacheBase> query) {
    detail::QueryCacheBase* registered = query.get();
    queries_.push_back(registered);
    queryCache_.emplace(std::move(key), std::move(query));
    return registered;
  }

  // Index of the `with` component present in the fewest archetypes (R-QY-3); nullptr if `with` is empty.
  const core::SmallMap<detail::ArchetypeId, uint16_t>* World::smallestIndex(const detail::Signature& with) const {
    static const core::SmallMap<detail::ArchetypeId, uint16_t> empty;

    const core::SmallMap<detail::ArchetypeId, uint16_t>* best = nullptr;
    for (ComponentId componentId : with) {
      const auto* index = componentId < componentIndex_.size() ? &componentIndex_[componentId] : &empty;
      if (!best || index->size() < best->size()) {
        best = index;
      }
    }

    return best;
  }

  void World::materialize(Entity reserved) {
    detail::EntityRecord& record = store_.record(reserved.index);
    detail::Archetype& rootArchetype = root();

    record.row = rootArchetype.table.pushUninitialized(reserved);
    record.archetype = &rootArchetype;
  }

  void World::destroyOrRelease(Entity entity, bool alive) {
    if (alive) {
      destroyNotifying(entity);
    } else {
      store_.release(entity.index);
    }
  }

  uint32_t World::transitionTo(Entity entity, detail::EntityRecord& record, detail::Archetype& destination, const detail::Edge* edge) {
    if (!record.archetype) {
      record.row = destination.table.pushUninitialized(entity);
      record.archetype = &destination;
    } else if (edge) {
      moveEntity(entity, record, destination, *edge);
    }

    return record.row;
  }

  void World::flushSequential(CommandBuffer& commandBuffer) {
    OLY_ASSERT(iterating_ == 0, "flush during iteration=%u", iterating_);
    OLY_ASSERT(commandBuffer.store() == &store_, "flush of a buffer created by another World");

    commandBuffer.forEach([&](const detail::CommandView& command) {
      switch (command.operation) {
      case detail::CommandOperation::Spawn:
        if (store_.isReserved(command.entity)) {
          materialize(command.entity);
        }
        break;
      case detail::CommandOperation::Destroy:
        if (store_.isAlive(command.entity)) {
          destroy(command.entity);
        } else if (store_.isReserved(command.entity)) {
          store_.release(command.entity.index);
        }
        break;
      case detail::CommandOperation::Add:
        if (store_.isAlive(command.entity)) {
          addById(command.entity, command.componentId, command.payload);
        }
        break;
      case detail::CommandOperation::Remove:
        if (store_.isAlive(command.entity)) {
          removeById(command.entity, command.componentId);
        }
        break;
      }
    });

    commandBuffer.clear();
  }

  const detail::Edge& World::jumpEdge(detail::Archetype& source, detail::Archetype& destination) {
    const uint64_t key = (uint64_t(source.id) << 32) | destination.id;
    if (auto it = jumpEdges_.find(key); it != jumpEdges_.end()) {
      return it->second;
    }

    return jumpEdges_.emplace(key, detail::buildEdge(source, destination, detail::NoComponent)).first->second;
  }

  void World::flush(CommandBuffer& commandBuffer) {
    OLY_ASSERT(iterating_ == 0, "flush during iteration=%u", iterating_);
    OLY_ASSERT(commandBuffer.store() == &store_, "flush of a buffer created by another World");

    flushWithoutDeferred(commandBuffer);
    applyDeferred();
  }

  void World::flushWithoutDeferred(CommandBuffer& commandBuffer) {
    const detail::CommandPlan plan = commandBuffer.plan();
    detail::NetEffect net;
    FlushMemo memo;

    for (const detail::EntityPlan& entityPlan : plan.entities) {
      const Entity entity = entityPlan.entity;
      commandBuffer.netEffect(plan, entityPlan, net);

      const bool alive = store_.isAlive(entity);
      const bool reserved = net.spawned && store_.isReserved(entity);
      if (!alive && !reserved) {
        continue; // stale handle: ignored instead of cancelling the commands on destroy (spec §10, question 3)
      }

      if (net.destroyed) {
        destroyOrRelease(entity, alive);
        continue;
      }

      const std::span<const detail::NetComponentOperation> operations = net.operations;
      detail::Archetype* source = store_.record(entity.index).archetype; // nullptr if reserved
      if (!memo.matches(source, operations)) {
        static const detail::Signature noComponents;
        const detail::Signature& sourceSignature = source ? source->signature : noComponents;
        detail::Archetype* destination = findOrCreate(resolveSignature(sourceSignature, applyOperations(sourceSignature, operations), entity));
        memo.assign(source, operations, destination, (source && destination != source) ? &jumpEdge(*source, *destination) : nullptr, implications_);
        memo.assignObservers(observers_, operations);
      }

      if (memo.observed & detail::ObservedOnRemove) {
        detail::IterationGuard guard(&iterating_);
        for (ComponentId componentId : memo.onRemove) {
          runObservers(ObserverKind::OnRemove, entity, componentId);
        }
      }

      // Read after the observers: their commands().spawn() reserves an index and may reallocate the records.
      const uint32_t row = transitionTo(entity, store_.record(entity.index), *memo.destination, memo.edge);
      writeValues(memo, operations, row);
      for (const auto& [column, initializer] : memo.initialized) {
        initializer(column->at(row));
      }

      if (memo.observed & detail::ObservedOnAdd) {
        for (ComponentId componentId : memo.onAdd) {
          pendingOnAdd_.emplace_back(entity, componentId);
        }
      }
    }

    commandBuffer.clear();
    runPendingOnAdd();
  }

  void World::runPendingOnAdd() {
    if (pendingOnAdd_.empty()) {
      return;
    }

    {
      detail::IterationGuard guard(&iterating_);
      for (const auto& [entity, componentId] : pendingOnAdd_) {
        runObservers(ObserverKind::OnAdd, entity, componentId);
      }
    }
    pendingOnAdd_.clear();
  }
}
