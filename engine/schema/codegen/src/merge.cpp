#include <map>
#include <string>
#include <utility>
#include <vector>

#include "oly/schema/hash.hpp"
#include "oly/schema/schema.hpp"
#include "validation.hpp"

namespace oly::schema {
  namespace {
    struct FirstDeclaration {
      std::string_view fileName;
      SourceLocation location;
    };

    std::string locationText(std::string_view fileName, SourceLocation location) {
      return std::string(fileName) + ":" + std::to_string(location.line) + ":" + std::to_string(location.column);
    }

    // Each file already has unique names (validateSchema); this checks them across files, in merge order.
    void checkNamesAcrossFiles(const std::vector<Schema>& pluginSchemas, const Schema& game) {
      std::map<std::string_view, FirstDeclaration> firstDeclarations;
      auto checkFile = [&](const Schema& schema) {
        for (const DeclaredName& declared : namesInSourceOrder(schema)) {
          const auto [found, inserted] = firstDeclarations.try_emplace(declared.name, FirstDeclaration{schema.fileName, declared.location});

          if (!inserted) {
            throw SchemaError(schema.fileName, declared.location, "repeated name \"" + std::string(declared.name) + "\"; first declared at " + locationText(found->second.fileName, found->second.location));
          }
        }
      };

      for (const Schema& schema : pluginSchemas) {
        checkFile(schema);
      }

      checkFile(game);
    }

    void appendTypes(std::vector<Type>& merged, std::vector<Type>& types, const std::string& origin) {
      for (Type& type : types) {
        type.origin = origin;
        merged.push_back(std::move(type));
      }
    }
  }

  Schema mergeSchemas(std::span<const PluginSource> plugins, std::string_view gameSource, std::string_view gameFileName) {
    std::vector<Schema> pluginSchemas;

    for (const PluginSource& plugin : plugins) {
      Schema schema = parseSchema(plugin.source, plugin.fileName);
      validatePluginSchema(schema);
      pluginSchemas.push_back(std::move(schema));
    }

    Schema game = parseSchema(gameSource, gameFileName);
    checkNamesAcrossFiles(pluginSchemas, game);

    Schema merged;
    merged.fileName = game.fileName;

    for (std::size_t index = 0; index < plugins.size(); ++index) {
      appendTypes(merged.types, pluginSchemas[index].types, plugins[index].name);
      appendTypes(merged.events, pluginSchemas[index].events, plugins[index].name);
      merged.plugins.push_back(plugins[index].name);
    }

    appendTypes(merged.types, game.types, "");
    appendTypes(merged.events, game.events, "");
    merged.actions = std::move(game.actions);
    merged.states = std::move(game.states);
    merged.hash = fnv1a32(canonicalForm(merged));

    return merged;
  }
}
