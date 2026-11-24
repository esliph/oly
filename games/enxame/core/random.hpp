#pragma once

#include <cstdint>

namespace enxame {
  // SplitMix64: the same sequence on every platform and standard library, which std::*_distribution does not give.
  class Random {
   private:
    uint64_t state_;

   public:
    explicit Random(uint64_t seed) noexcept
      : state_(seed) {}

    uint64_t nextU64() noexcept {
      state_ += 0x9e3779b97f4a7c15ull;
      uint64_t mixed = state_;
      mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
      mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;

      return mixed ^ (mixed >> 31);
    }

    // [0, 1), from the top 24 bits: a float holds exactly 24 bits of mantissa.
    float nextFloat() noexcept {
      return static_cast<float>(nextU64() >> 40) * (1.0f / 16777216.0f);
    }

    // [0, bound), bound > 0. A multiply and a shift instead of a modulo: no division, and no bias that matters here.
    uint32_t nextBelow(uint32_t bound) noexcept {
      return static_cast<uint32_t>(((nextU64() >> 32) * bound) >> 32);
    }

    bool chance(float probability) noexcept {
      return nextFloat() < probability;
    }
  };
}
