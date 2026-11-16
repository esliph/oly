#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "oly/schema/emitter.hpp"
#include "oly/schema/schema.hpp"

namespace oly::schema {
  // Includes the header of each plugin in `schema.plugins` and defines only the types with an empty origin.
  std::string emitCppHeader(const Schema& schema);

  std::string emitCppDeclaration(const Schema& schema);

  std::string emitCppRegistry(const Schema& schema);

  // A plugin's own header: its types with an empty origin, no declaration and no registry. Throws SchemaError when the
  // schema declares an action or a state.
  std::string emitCppPluginHeader(const Schema& schema);

  // `target` is a directory: schema.generated.hpp, schema_declaration.generated.cpp and schema_registry.generated.cpp.
  std::vector<EmittedFile> emitCpp(const Schema& schema, const std::filesystem::path& target);

  // `target` is the directory of the plugin's include path, `<root>/oly/<plugin>`: schema.generated.hpp.
  std::vector<EmittedFile> emitCppPlugin(const Schema& schema, const std::filesystem::path& target);
}
