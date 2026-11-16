#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "oly/core/small_map.hpp"
#include "oly/ecs/component.hpp"
#include "oly/ecs/detail/table.hpp"

namespace oly::ecs::detail {
  using ArchetypeId = uint32_t;
  using Signature = std::vector<ComponentId>;

  inline constexpr uint16_t NoColumn = UINT16_MAX;
  inline constexpr ComponentId NoComponent = UINT32_MAX;

  struct SignatureHash {
    static constexpr uint64_t kFnv1aOffsetBasis = 1469598103934665603ull;
    static constexpr uint64_t kFnv1aPrime = 1099511628211ull;

    size_t operator()(const Signature& signature) const noexcept {
      uint64_t hash = kFnv1aOffsetBasis;
      for (ComponentId id : signature) {
        for (int i = 0; i < 4; ++i) {
          hash ^= (id >> (8 * i)) & 0xffu;
          hash *= kFnv1aPrime;
        }
      }

      return static_cast<size_t>(hash);
    }
  };

  struct Archetype;

  // Edge::work bit beside ObservedOnAdd (1) and ObservedOnRemove (2): the edge has initializedColumns.
  inline constexpr uint8_t InitializesImplied = 4;

  struct Edge {
    Archetype* target = nullptr;
    std::vector<int16_t> sourceToDestination; // per source column: column in the destination, or -1
    // Add edges only: destination columns of the implied components the source lacks, value-initialized. Empty on jump
    // edges, whose implied columns the flush memo keeps with the columns its operations write.
    std::vector<uint16_t> initializedColumns;
    int16_t addedColumn = -1; // column of the added component, or -1 if the edge builds no value (removal, tag, jump)
    // ObservedOnAdd/ObservedOnRemove for the components the edge adds/removes, and InitializesImplied: the one byte the
    // add/remove fast path tests. 0 on jump edges.
    uint8_t work = 0;
  };

  struct Archetype {
    ArchetypeId id;
    uint8_t observed = 0; // ObservedOnAdd/ObservedOnRemove over the whole signature (destroy, spawn<Ts...>)
    Signature signature; // sorted, includes tags
    std::vector<ComponentId> columnTypes; // sorted, data only
    Table table;
    core::SmallMap<ComponentId, Edge> addEdges, removeEdges;

    Archetype(ArchetypeId id, Signature signature);
  };

  // NoColumn if `componentId` is absent or is a tag.
  uint16_t columnOf(const Archetype& archetype, ComponentId componentId) noexcept;

  // `added` = the newly added component; NoComponent for a removal or an arbitrary jump. For an add, initializedColumns
  // gets every destination column the source lacks except `added`'s.
  Edge buildEdge(const Archetype& source, Archetype& destination, ComponentId added);
}
