#pragma once

#include <cstddef>
#include <cstdint>

namespace enxame {
  inline constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;

  // 64-bit FNV-1a: folds bytes into a running hash, so a world hash is the same on every platform.
  inline uint64_t fnv1a(uint64_t hash, const void* bytes, size_t size) noexcept {
    const unsigned char* data = static_cast<const unsigned char*>(bytes);

    for (size_t index = 0; index < size; ++index) {
      hash ^= data[index];
      hash *= 1099511628211ull;
    }

    return hash;
  }
}
