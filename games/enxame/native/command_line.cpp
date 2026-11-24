#include "command_line.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace enxame {
  namespace {
    constexpr const char* Usage =
      "usage: enxame [--scenario <name>] [--size <n>] [--frames <n>]\n"
      "       enxame --headless --scenario <name> --size <n> --commit <sha> --out <file.json> [--timing on|off] [--steps <n>]\n"
      "       enxame --list\n";

    std::nullopt_t refuse(const char* message, std::string_view detail = {}) {
      std::fprintf(stderr, "enxame: %s%.*s\n%s", message, static_cast<int>(detail.size()), detail.data(), Usage);

      return std::nullopt;
    }

    std::optional<uint32_t> parseNumber(std::string_view text) {
      if (text.empty() || text.size() > 9) {
        return std::nullopt;
      }

      uint32_t value = 0;

      for (const char character : text) {
        if (character < '0' || character > '9') {
          return std::nullopt;
        }

        value = value * 10 + static_cast<uint32_t>(character - '0');
      }

      return value;
    }

    bool validCommit(std::string_view commit) {
      if (commit.empty()) {
        return false;
      }

      for (const char character : commit) {
        const bool allowed = (character >= '0' && character <= '9') || (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || character == '.' || character == '_' || character == '-';

        if (!allowed) {
          return false;
        }
      }

      return true;
    }
  }

  std::optional<LaunchOptions> parseCommandLine(int argumentCount, char** arguments) {
    LaunchOptions options;
    options.passThrough.push_back(arguments[0]);

    for (int index = 1; index < argumentCount; ++index) {
      const std::string_view argument = arguments[index];
      const bool hasValue = index + 1 < argumentCount;

      if (argument == "--headless") {
        options.headless = true;
      } else if (argument == "--list") {
        options.list = true;
      } else if (argument == "--scenario" && hasValue) {
        const std::string_view name = arguments[++index];
        options.scenario = parseScenario(name);

        if (!options.scenario) {
          return refuse("unknown scenario ", name);
        }
      } else if (argument == "--size" && hasValue) {
        options.size = parseNumber(arguments[++index]);

        if (!options.size) {
          return refuse("--size needs a number");
        }
      } else if (argument == "--steps" && hasValue) {
        options.steps = parseNumber(arguments[++index]);

        if (!options.steps) {
          return refuse("--steps needs a number");
        }
      } else if (argument == "--timing" && hasValue) {
        const std::string_view value = arguments[++index];

        if (value != "on" && value != "off") {
          return refuse("--timing is on or off");
        }

        options.timing = value == "on";
      } else if (argument == "--commit" && hasValue) {
        options.commit = arguments[++index];

        if (!validCommit(options.commit)) {
          return refuse("--commit must match [0-9A-Za-z._-]+");
        }
      } else if (argument == "--out" && hasValue) {
        options.out = arguments[++index];
      } else {
        options.passThrough.push_back(arguments[index]);
      }
    }

    if (options.headless) {
      if (options.passThrough.size() > 1) {
        return refuse("unknown option ", options.passThrough[1]);
      }

      if (!options.scenario) {
        return refuse("--headless needs --scenario");
      }

      if (!options.size) {
        return refuse("--headless needs --size");
      }

      if (options.commit.empty()) {
        return refuse("--headless needs --commit");
      }

      if (options.out.empty()) {
        return refuse("--headless needs --out");
      }
    }

    return options;
  }
}
