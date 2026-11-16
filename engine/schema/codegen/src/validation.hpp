#pragma once

#include <string_view>
#include <vector>

#include "oly/schema/schema.hpp"

namespace oly::schema {
  struct DeclaredName {
    std::string_view name;
    SourceLocation location;
  };

  void validateSchema(const Schema& schema);

  // A plugin file declares only components, tags and events: actions and states get constexpr ids from 0 in the
  // generated header, which a plugin generates once while its ids would depend on each game's install order.
  void validatePluginSchema(const Schema& schema);

  // Every name the schema declares (types, events, actions, states), in source order. Views into `schema`.
  std::vector<DeclaredName> namesInSourceOrder(const Schema& schema);
}
