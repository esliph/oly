#pragma once

#include <random>

#include "oly/app/engine.hpp"

// Initial values, not measured: tuned by playing.
namespace drift {
  inline constexpr int LogicalWidth = 640;
  inline constexpr int LogicalHeight = 480;
  inline constexpr float PlayerSpeed = 320.0f;
  inline constexpr float PlayerHalfSize = 16.0f;
  inline constexpr float PlayerBottomMargin = 40.0f;
  inline constexpr double RockSpawnSeconds = 0.6;
  inline constexpr float RockMinHalfSize = 10.0f;
  inline constexpr float RockMaxHalfSize = 28.0f;
  inline constexpr float RockMinFallSpeed = 140.0f;
  inline constexpr float RockMaxFallSpeed = 320.0f;

  struct Run {
    double startTime = 0.0;
    double survivedSeconds = 0.0;
    std::minstd_rand random;
  };

  void installGameplay(oly::app::Engine& engine, Run& run);
}
