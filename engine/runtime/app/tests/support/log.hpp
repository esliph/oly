#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "oly/app/system_parameters.hpp"

struct Position {
  float x = 0, y = 0;
};
struct Velocity {
  float x = 0, y = 0;
};

struct Log {
  std::vector<std::string> calls;
  std::vector<float> interpolations;
  std::vector<double> times;

  int count(const std::string& name) const {
    return static_cast<int>(std::count(calls.begin(), calls.end(), name));
  }
};

inline void recordName(oly::app::Res<Log> log, const oly::app::SystemInfo& self, const oly::app::StepInfo& step) {
  log->calls.push_back(std::string(self.name));

  if (self.schedule == oly::app::ScheduleKind::Frame) {
    log->interpolations.push_back(step.interpolation);
  } else {
    log->times.push_back(step.time);
  }
}
