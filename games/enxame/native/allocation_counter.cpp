#include "allocation_counter.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
  std::atomic<uint64_t> allocations{0};

  void* allocate(std::size_t size) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    void* memory = std::malloc(size == 0 ? 1 : size);

    if (memory == nullptr) {
      throw std::bad_alloc();
    }

    return memory;
  }

  void* allocateAligned(std::size_t size, std::align_val_t alignment) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    const std::size_t boundary = static_cast<std::size_t>(alignment);
    const std::size_t rounded = ((size == 0 ? 1 : size) + boundary - 1) / boundary * boundary;
#ifdef _WIN32
    void* memory = _aligned_malloc(rounded, boundary);
#else
    void* memory = std::aligned_alloc(boundary, rounded);
#endif

    if (memory == nullptr) {
      throw std::bad_alloc();
    }

    return memory;
  }

  void release(void* memory) noexcept {
    std::free(memory);
  }

  void releaseAligned(void* memory) noexcept {
#ifdef _WIN32
    _aligned_free(memory);
#else
    std::free(memory);
#endif
  }
}

namespace enxame {
  uint64_t allocationCount() noexcept {
    return allocations.load(std::memory_order_relaxed);
  }
}

void* operator new(std::size_t size) {
  return allocate(size);
}

void* operator new[](std::size_t size) {
  return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
  return allocateAligned(size, alignment);
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return allocateAligned(size, alignment);
}

void operator delete(void* memory) noexcept {
  release(memory);
}

void operator delete[](void* memory) noexcept {
  release(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
  release(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
  release(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept {
  releaseAligned(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept {
  releaseAligned(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
  releaseAligned(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
  releaseAligned(memory);
}
