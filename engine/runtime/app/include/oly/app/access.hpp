#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "oly/ecs/component.hpp"
#include "oly/ecs/resource.hpp"

namespace oly::app {
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
}
