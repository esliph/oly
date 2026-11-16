#include "oly/schema/generator.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "oly/schema/emitter.hpp"
#include "oly/schema/schema.hpp"

namespace oly::schema {
  namespace {
    constexpr std::string_view usage = "usage: oly-schema <file.oly> [--plugin <name>=<file.oly> ...] --emit <emitter>=<target> [--emit <emitter>=<target> ...]\n";

    // `--emit <name>=<target>` or `--plugin <name>=<file.oly>`.
    struct NamedValue {
      std::string name;
      std::string value;
    };

    struct Requests {
      std::vector<NamedValue> plugins;
      std::vector<NamedValue> emits;
    };

    std::optional<NamedValue> splitNamedValue(std::string_view text) {
      const std::size_t separator = text.find('=');
      if (separator == std::string_view::npos || separator == 0) {
        return std::nullopt;
      }

      return NamedValue{std::string(text.substr(0, separator)), std::string(text.substr(separator + 1))};
    }

    // The order of the --plugin arguments is the install order.
    std::optional<Requests> parseRequests(std::span<const std::string_view> arguments) {
      Requests requests;
      for (std::size_t index = 1; index < arguments.size(); index += 2) {
        if (index + 1 >= arguments.size()) {
          return std::nullopt;
        }

        std::optional<NamedValue> request = splitNamedValue(arguments[index + 1]);
        if (!request) {
          return std::nullopt;
        }

        if (arguments[index] == "--emit") {
          requests.emits.push_back(std::move(*request));
        } else if (arguments[index] == "--plugin" && !request->value.empty()) {
          requests.plugins.push_back(std::move(*request));
        } else {
          return std::nullopt;
        }
      }

      if (requests.emits.empty()) {
        return std::nullopt;
      }

      return requests;
    }

    const Emitter* findEmitter(std::string_view name) {
      for (const Emitter& emitter : emitters()) {
        if (emitter.name == name) {
          return &emitter;
        }
      }

      return nullptr;
    }

    std::string availableEmitters() {
      std::string names;
      for (const Emitter& emitter : emitters()) {
        if (!names.empty()) {
          names += ", ";
        }

        names += emitter.name;
      }

      return names;
    }

    std::optional<std::string> readText(const std::string& path) {
      std::ifstream file(path, std::ios::binary);
      if (!file) {
        return std::nullopt;
      }

      std::ostringstream text;
      text << file.rdbuf();

      return text.str();
    }

    bool writeFile(const EmittedFile& file) {
      if (!file.path.parent_path().empty()) {
        std::error_code error;
        std::filesystem::create_directories(file.path.parent_path(), error);
        if (error) {
          return false;
        }
      }

      std::ofstream output(file.path, std::ios::binary);
      output << file.contents;
      output.flush();

      return static_cast<bool>(output);
    }
  }

  int runGenerator(std::span<const std::string_view> arguments, std::ostream& errors) {
    const std::optional<Requests> requests = arguments.empty() ? std::nullopt : parseRequests(arguments);
    if (!requests) {
      errors << usage;
      return 1;
    }

    for (const NamedValue& request : requests->emits) {
      if (findEmitter(request.name) == nullptr) {
        errors << "unknown emitter \"" << request.name << "\"; available: " << availableEmitters() << "\n";
        return 1;
      }
    }

    const std::string input(arguments[0]);
    const std::optional<std::string> text = readText(input);
    if (!text) {
      errors << "cannot read " << input << "\n";
      return 1;
    }

    std::vector<PluginSource> plugins;
    for (const NamedValue& request : requests->plugins) {
      std::optional<std::string> pluginText = readText(request.value);
      if (!pluginText) {
        errors << "cannot read " << request.value << "\n";
        return 1;
      }

      plugins.push_back({request.name, request.value, std::move(*pluginText)});
    }

    std::vector<EmittedFile> files;
    try {
      const Schema schema = mergeSchemas(plugins, *text, input);
      for (const NamedValue& request : requests->emits) {
        for (EmittedFile& file : findEmitter(request.name)->emit(schema, request.value)) {
          files.push_back(std::move(file));
        }
      }
    } catch (const SchemaError& error) {
      errors << error.what() << "\n";
      return 1;
    }

    for (const EmittedFile& file : files) {
      if (!writeFile(file)) {
        errors << "cannot write " << file.path.string() << "\n";
        return 1;
      }
    }

    return 0;
  }
}
