#include "headless.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#endif

#include "allocation_counter.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "oly/ecs/world.hpp"
#include "oly/input/physical_button.hpp"
#include "scenario.hpp"
#include "schema.generated.hpp"
#include "statistics.hpp"
#include "workloads/s5_replay.hpp"
#include "world_hash.hpp"

namespace enxame {
  namespace {
    struct FileCloser {
      void operator()(std::FILE* file) const {
        std::fclose(file);
      }
    };

    using File = std::unique_ptr<std::FILE, FileCloser>;

    uint64_t nanosecondsSince(std::chrono::steady_clock::time_point start) {
      return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    }

    uint64_t medianOf(std::vector<uint64_t> values) {
      return values.empty() ? 0 : percentile(values, 50.0);
    }

    // What one run of a scenario measured: wall time per step, and per system and per flush when timing is on.
    struct WaveRecord {
      uint32_t wave;
      uint32_t archetypes;
      uint32_t liveEntities;
      uint64_t columnBytes;
      int64_t residentBytes; // -1 where unknown
    };

    struct Measurements {
      std::vector<uint64_t> stepNanoseconds;
      std::vector<uint64_t> allocationsPerStep; // operator new calls inside one frame
      std::vector<uint64_t> transitionsPerStep; // S3: status toggles of the last step of each frame
      std::vector<WaveRecord> waves; // S8: one per completed wave
      std::vector<std::vector<uint64_t>> systemNanoseconds; // [system slot]: only the steps where it ran
      std::vector<std::vector<uint64_t>> flushNanoseconds; // [phase]
    };

    void record(const oly::app::Engine& engine, Measurements& measurements) {
      const auto systems = engine.systemTimings();
      measurements.systemNanoseconds.resize(systems.size());

      for (size_t slot = 0; slot < systems.size(); ++slot) {
        if (systems[slot].ran) {
          measurements.systemNanoseconds[slot].push_back(systems[slot].nanoseconds);
        }
      }

      const auto flushes = engine.flushTimings();
      measurements.flushNanoseconds.resize(flushes.size());

      for (size_t slot = 0; slot < flushes.size(); ++slot) {
        measurements.flushNanoseconds[slot].push_back(flushes[slot].nanoseconds);
      }
    }

    void writeJson(std::FILE* file, const LaunchOptions& options, const ScenarioInfo& info, RunLength run, const oly::app::Engine& engine, Measurements& measurements, const std::string& measuresJson) {
      std::fprintf(file, "{\n  \"commit\": \"%s\", \"scenario\": \"%.*s\", \"size\": %u, \"seed\": %llu,\n", options.commit.c_str(), static_cast<int>(info.name.size()), info.name.data(), *options.size, static_cast<unsigned long long>(info.seed));
      std::fprintf(file, "  \"warmup\": %u, \"steps\": %u, \"stepsOverridden\": %s, \"timing\": %s,\n", run.warmup, run.steps, options.steps ? "true" : "false", options.timing ? "true" : "false");
      std::fprintf(file, "  \"step\": { \"p50\": %llu, \"p99\": %llu, \"max\": %llu },\n", static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 50.0)), static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 99.0)), static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 100.0)));
      std::fprintf(file, "  \"systems\": [");

      const auto systems = engine.systemTimings();

      for (size_t slot = 0; slot < systems.size(); ++slot) {
        const std::vector<uint64_t>& values = measurements.systemNanoseconds[slot];
        std::fprintf(file, "%s\n    { \"name\": \"%.*s\", \"phase\": \"%.*s\", \"ran\": %zu, \"median\": %llu }", slot == 0 ? "" : ",", static_cast<int>(systems[slot].name.size()), systems[slot].name.data(), static_cast<int>(systems[slot].phaseName.size()), systems[slot].phaseName.data(), values.size(), static_cast<unsigned long long>(medianOf(values)));
      }

      std::fprintf(file, "%s],\n  \"flushes\": [", systems.empty() ? "" : "\n  ");

      const auto flushes = engine.flushTimings();

      for (size_t slot = 0; slot < flushes.size(); ++slot) {
        std::fprintf(file, "%s\n    { \"phase\": \"%.*s\", \"median\": %llu }", slot == 0 ? "" : ",", static_cast<int>(flushes[slot].phaseName.size()), flushes[slot].phaseName.data(), static_cast<unsigned long long>(medianOf(measurements.flushNanoseconds[slot])));
      }

      std::fprintf(file, "%s],\n  \"measures\": %s\n}\n", flushes.empty() ? "" : "\n  ", measuresJson.c_str());
    }

    template <class Tag>
    size_t countOf(oly::ecs::World& world) {
      size_t count = 0;

      world.query<const Position>(oly::ecs::With<Tag>{}).each([&](const Position&) {
        ++count;
      });

      return count;
    }

    // Resident memory of the process, in bytes; -1 where /proc is not there.
    int64_t residentBytes() {
#ifdef __linux__
      std::FILE* file = std::fopen("/proc/self/statm", "r");

      if (file == nullptr) {
        return -1;
      }

      long totalPages = 0;
      long residentPages = 0;
      const int fields = std::fscanf(file, "%ld %ld", &totalPages, &residentPages);
      std::fclose(file);

      return fields == 2 ? static_cast<int64_t>(residentPages) * sysconf(_SC_PAGESIZE) : -1;
#else
      return -1;
#endif
    }

    WaveRecord waveRecordOf(oly::ecs::World& world, uint32_t wave) {
      const oly::ecs::WorldStats stats = oly::ecs::stats(world);
      uint64_t columnBytes = 0;

      for (const oly::ecs::ArchetypeStats& archetype : stats.archetypes) {
        columnBytes += archetype.columnBytes;
      }

      return {wave, stats.archetypeCount, stats.liveEntities, columnBytes, residentBytes()};
    }

    std::string wavesJson(const std::vector<WaveRecord>& waves) {
      std::string json = "[";

      for (size_t index = 0; index < waves.size(); ++index) {
        const WaveRecord& wave = waves[index];
        char text[320];
        std::snprintf(text, sizeof(text), "%s\n    { \"wave\": %u, \"archetypes\": %u, \"liveEntities\": %u, \"columnBytes\": %llu, \"residentBytes\": ", index == 0 ? "" : ",", wave.wave, wave.archetypes, wave.liveEntities, static_cast<unsigned long long>(wave.columnBytes));
        json += text;
        json += wave.residentBytes < 0 ? "null" : std::to_string(wave.residentBytes);
        json += " }";
      }

      return json + "\n  ]";
    }

    std::string measuresOf(ScenarioId scenario, oly::ecs::World& world, const oly::app::Engine& engine, const Measurements& measurements) {
      char hash[32];
      std::snprintf(hash, sizeof(hash), "0x%016llx", static_cast<unsigned long long>(worldHash(world)));
      char text[320];

      if (scenario == ScenarioId::S5) {
        std::snprintf(text, sizeof(text), "{ \"worldHash\": \"%s\", \"enemiesLeft\": %zu, \"turrets\": %zu }", hash, countOf<Enemy>(world), countOf<Turret>(world));

        return text;
      }

      if (scenario == ScenarioId::S3) {
        std::vector<uint64_t> transitions = measurements.transitionsPerStep;
        std::string flushMedian = "null";
        const auto flushes = engine.flushTimings();

        for (size_t slot = 0; slot < flushes.size() && slot < measurements.flushNanoseconds.size(); ++slot) {
          if (flushes[slot].phase == engine.phaseId<oly::app::UpdateAfter>()) {
            flushMedian = std::to_string(medianOf(measurements.flushNanoseconds[slot]));
          }
        }

        std::snprintf(text, sizeof(text), "{ \"worldHash\": \"%s\", \"transitionsPerStep\": { \"p50\": %llu, \"max\": %llu }, \"updateAfterFlushMedian\": %s }", hash, static_cast<unsigned long long>(percentile(transitions, 50.0)), static_cast<unsigned long long>(percentile(transitions, 100.0)), flushMedian.c_str());

        return text;
      }

      if (scenario == ScenarioId::S6) {
        size_t visited = 0;

        world.query<const Position>().each([&](const Position&) {
          ++visited;
        });

        std::string perEntity = "null";
        const auto systems = engine.systemTimings();

        for (size_t slot = 0; slot < systems.size() && slot < measurements.systemNanoseconds.size(); ++slot) {
          if (systems[slot].name == "propagateTransforms") {
            char number[32];
            std::snprintf(number, sizeof(number), "%.3f", static_cast<double>(medianOf(measurements.systemNanoseconds[slot])) / static_cast<double>(visited));
            perEntity = number;
          }
        }

        std::snprintf(text, sizeof(text), "{ \"worldHash\": \"%s\", \"visitedEntities\": %zu, \"propagateTransformsNanosecondsPerEntity\": %s }", hash, visited, perEntity.c_str());

        return text;
      }

      if (scenario == ScenarioId::S8) {
        return std::string("{ \"worldHash\": \"") + hash + "\", \"waves\": " + wavesJson(measurements.waves) + " }";
      }

      if (scenario == ScenarioId::S2) {
        std::vector<uint64_t> perStep = measurements.allocationsPerStep;
        uint64_t total = 0;

        for (const uint64_t allocated : perStep) {
          total += allocated;
        }

        std::snprintf(text, sizeof(text), "{ \"worldHash\": \"%s\", \"allocationsPerStep\": { \"p50\": %llu, \"max\": %llu, \"total\": %llu } }", hash, static_cast<unsigned long long>(percentile(perStep, 50.0)), static_cast<unsigned long long>(percentile(perStep, 100.0)), static_cast<unsigned long long>(total));

        return text;
      }

      return "{ \"worldHash\": \"" + std::string(hash) + "\" }";
    }
  }

  int runHeadless(const LaunchOptions& options) {
    if (!checkScenario(*options.scenario, options.size)) {
      return 64;
    }

    File file(std::fopen(options.out.c_str(), "w"));

    if (!file) {
      std::fprintf(stderr, "enxame: cannot write %s\n", options.out.c_str());

      return 1;
    }

    const ScenarioInfo& info = scenarioInfo(*options.scenario);
    const RunLength run = options.steps ? RunLength{0, *options.steps} : info.run;
    oly::ecs::World world;
    oly::app::Engine engine(world);
    oly::app::setUpGame(engine);
    setUpScenario(*options.scenario, *options.size, engine);
    engine.setTimingEnabled(options.timing);
    Measurements measurements;
    engine.frame(0.0);

    uint32_t nextClick = 0;

    for (uint32_t frame = 1; frame <= run.warmup + run.steps; ++frame) {
      const bool clicking = *options.scenario == ScenarioId::S5 && nextClick < workloads::S5ClickCount && workloads::s5ClickAt(nextClick).step + 1 == frame;

      if (clicking) {
        const workloads::Click click = workloads::s5ClickAt(nextClick++);
        engine.actions().pointerMove(click.screenX, click.screenY);
        engine.actions().buttonDown(oly::input::MouseButton::Left);
      }

      const uint64_t allocationsBefore = allocationCount();
      const auto start = std::chrono::steady_clock::now();
      engine.frame(static_cast<double>(frame) / 60.0);
      const uint64_t elapsed = nanosecondsSince(start);
      const uint64_t allocated = allocationCount() - allocationsBefore;

      if (clicking) {
        engine.actions().buttonUp(oly::input::MouseButton::Left);
      }

      if (frame > run.warmup) {
        measurements.stepNanoseconds.push_back(elapsed);
        measurements.allocationsPerStep.push_back(allocated);

        if (*options.scenario == ScenarioId::S3) {
          measurements.transitionsPerStep.push_back(world.resource<StatusToggles>().lastStep);
        }

        if (options.timing) {
          record(engine, measurements);
        }
      }

      if (*options.scenario == ScenarioId::S8 && world.resource<WaveClock>().wavesCompleted > measurements.waves.size()) {
        measurements.waves.push_back(waveRecordOf(world, world.resource<WaveClock>().wavesCompleted));
      }
    }

    writeJson(file.get(), options, info, run, engine, measurements, measuresOf(*options.scenario, world, engine, measurements));
    const bool writeFailed = std::ferror(file.get()) != 0;
    const bool closeFailed = std::fclose(file.release()) != 0;

    if (writeFailed || closeFailed) {
      std::fprintf(stderr, "enxame: cannot write %s\n", options.out.c_str());

      return 1;
    }

    std::printf("wrote %s\n", options.out.c_str());

    return 0;
  }
}
