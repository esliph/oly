#pragma once

#include "oly/app/engine.hpp"

namespace oly::app {
  // Defined by the game's generated code (schema_declaration.generated.cpp) and called by setUpGame. Declares the
  // schema's events, actions and states in schema order, so the index in the schema is the id; the Engine must not
  // declare any of them yet (OLY_ASSERT).
  void declareSchema(Engine& engine);
}
