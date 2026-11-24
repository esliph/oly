#include "oly/ecs/detail/archetype.hpp"

#include <algorithm>

namespace oly::ecs::detail {
  namespace {
    std::vector<ComponentId> dataTypes(const Signature& signature) {
      std::vector<ComponentId> dataComponents;

      for (ComponentId componentId : signature) {
        if (!typeInfo(componentId).tag) {
          dataComponents.push_back(componentId);
        }
      }

      return dataComponents;
    }
  }

  Archetype::Archetype(ArchetypeId id, Signature signature)
    : id(id),
      signature(std::move(signature)),
      columnTypes(dataTypes(this->signature)),
      table(columnTypes) {}

  uint16_t columnOf(const Archetype& archetype, ComponentId componentId) noexcept {
    auto it = std::lower_bound(archetype.columnTypes.begin(), archetype.columnTypes.end(), componentId);

    if (it == archetype.columnTypes.end() || *it != componentId) {
      return NoColumn;
    }

    return static_cast<uint16_t>(it - archetype.columnTypes.begin());
  }

  Edge buildEdge(const Archetype& source, Archetype& destination, ComponentId added) {
    Edge edge;
    edge.target = &destination;
    edge.sourceToDestination.assign(source.columnTypes.size(), -1);

    size_t destinationColumn = 0;

    for (size_t sourceColumn = 0; sourceColumn < source.columnTypes.size(); ++sourceColumn) {
      while (destinationColumn < destination.columnTypes.size() && destination.columnTypes[destinationColumn] < source.columnTypes[sourceColumn]) {
        ++destinationColumn;
      }

      if (destinationColumn < destination.columnTypes.size() && destination.columnTypes[destinationColumn] == source.columnTypes[sourceColumn]) {
        edge.sourceToDestination[sourceColumn] = static_cast<int16_t>(destinationColumn);
      }
    }

    if (added != NoComponent && !typeInfo(added).tag && columnOf(source, added) == NoColumn) {
      edge.addedColumn = static_cast<int16_t>(columnOf(destination, added));
    }

    if (added == NoComponent) {
      return edge;
    }

    for (uint16_t column = 0; column < destination.columnTypes.size(); ++column) {
      const ComponentId componentId = destination.columnTypes[column];

      if (componentId != added && columnOf(source, componentId) == NoColumn) {
        edge.initializedColumns.push_back(column);
      }
    }

    return edge;
  }
}
