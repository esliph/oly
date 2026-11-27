#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

#include "oly/ecs/component.hpp"
#include "oly/ecs/resource.hpp"

namespace oly::app {
  enum class ConflictKind : uint8_t { Component,
                                      Resource,
                                      Exclusive };

  struct Conflict {
    ConflictKind kind;
    uint32_t id; // ComponentId or ResourceId; unused for Exclusive
    bool firstWrites; // for Exclusive: the first system is the exclusive one
    bool secondWrites; // for Exclusive: the second one is
  };

  namespace detail {
    inline bool containsSorted(const std::vector<uint32_t>& ids, uint32_t id) {
      return std::binary_search(ids.begin(), ids.end(), id);
    }

    inline void insertSorted(std::vector<uint32_t>& ids, uint32_t id) {
      const auto position = std::lower_bound(ids.begin(), ids.end(), id);

      if (position == ids.end() || *position != id) {
        ids.insert(position, id);
      }
    }

    inline void eraseSorted(std::vector<uint32_t>& ids, uint32_t id) {
      const auto position = std::lower_bound(ids.begin(), ids.end(), id);

      if (position != ids.end() && *position == id) {
        ids.erase(position);
      }
    }

    inline void conflictsOfKind(ConflictKind kind, const std::vector<uint32_t>& firstReads, const std::vector<uint32_t>& firstWrites, const std::vector<uint32_t>& secondReads, const std::vector<uint32_t>& secondWrites, std::vector<Conflict>& conflicts) {
      std::vector<uint32_t> written;
      std::set_union(firstWrites.begin(), firstWrites.end(), secondWrites.begin(), secondWrites.end(), std::back_inserter(written));

      for (const uint32_t id : written) {
        const bool firstWrote = containsSorted(firstWrites, id);
        const bool secondWrote = containsSorted(secondWrites, id);
        const bool firstTouches = firstWrote || containsSorted(firstReads, id);
        const bool secondTouches = secondWrote || containsSorted(secondReads, id);

        if (firstTouches && secondTouches) {
          conflicts.push_back(Conflict{kind, id, firstWrote, secondWrote});
        }
      }
    }
  }

  // What a system declares through its parameters. Each list is sorted and without repetition; a write of a type
  // removes its read.
  struct Access {
    std::vector<ecs::ComponentId> componentReads, componentWrites;
    std::vector<ecs::ResourceId> resourceReads, resourceWrites;
    bool exclusive = false;

    void readComponent(ecs::ComponentId id) {
      if (!detail::containsSorted(componentWrites, id)) {
        detail::insertSorted(componentReads, id);
      }
    }

    void writeComponent(ecs::ComponentId id) {
      detail::eraseSorted(componentReads, id);
      detail::insertSorted(componentWrites, id);
    }

    void readResource(ecs::ResourceId id) {
      if (!detail::containsSorted(resourceWrites, id)) {
        detail::insertSorted(resourceReads, id);
      }
    }

    void writeResource(ecs::ResourceId id) {
      detail::eraseSorted(resourceReads, id);
      detail::insertSorted(resourceWrites, id);
    }
  };

  // Every type both touch where at least one writes, in id order (components, then resources); one Exclusive entry, alone,
  // when either is exclusive.
  inline std::vector<Conflict> conflictsBetween(const Access& first, const Access& second) {
    std::vector<Conflict> conflicts;

    if (first.exclusive || second.exclusive) {
      conflicts.push_back(Conflict{ConflictKind::Exclusive, 0, first.exclusive, second.exclusive});

      return conflicts;
    }

    detail::conflictsOfKind(ConflictKind::Component, first.componentReads, first.componentWrites, second.componentReads, second.componentWrites, conflicts);
    detail::conflictsOfKind(ConflictKind::Resource, first.resourceReads, first.resourceWrites, second.resourceReads, second.resourceWrites, conflicts);

    return conflicts;
  }
}
