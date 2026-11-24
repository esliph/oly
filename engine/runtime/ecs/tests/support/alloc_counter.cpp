#include "alloc_counter.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {
  std::atomic<size_t> g_count{0};

  void* allocate(size_t size) {
    g_count.fetch_add(1, std::memory_order_relaxed);

    if (void* pointer = std::malloc(size ? size : 1)) {
      return pointer;
    }
    throw std::bad_alloc();
  }

  // msvcrt has no aligned_alloc, and memory from _aligned_malloc must go back through _aligned_free.
#if defined(_WIN32)
  void* alignedMalloc(size_t alignmentBytes, size_t size) {
    return _aligned_malloc(size, alignmentBytes);
  }

  void alignedFree(void* pointer) noexcept {
    _aligned_free(pointer);
  }
#else
  void* alignedMalloc(size_t alignmentBytes, size_t size) {
    return std::aligned_alloc(alignmentBytes, (size + alignmentBytes - 1) / alignmentBytes * alignmentBytes); // aligned_alloc requires a multiple of the alignment
  }

  void alignedFree(void* pointer) noexcept {
    std::free(pointer);
  }
#endif

  void* allocateAligned(size_t size, std::align_val_t alignment) {
    g_count.fetch_add(1, std::memory_order_relaxed);

    const size_t alignmentBytes = static_cast<size_t>(alignment);

    if (void* pointer = alignedMalloc(alignmentBytes, size ? size : alignmentBytes)) {
      return pointer;
    }
    throw std::bad_alloc();
  }
}

size_t allocationCount() noexcept {
  return g_count.load(std::memory_order_relaxed);
}

void* operator new(size_t size) {
  return allocate(size);
}
void* operator new[](size_t size) {
  return allocate(size);
}
void* operator new(size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new(size_t size, std::align_val_t alignment) {
  return allocateAligned(size, alignment);
}
void* operator new[](size_t size, std::align_val_t alignment) {
  return allocateAligned(size, alignment);
}
void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return allocateAligned(size, alignment);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return allocateAligned(size, alignment);
  } catch (...) {
    return nullptr;
  }
}

void operator delete(void* pointer) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, size_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, size_t) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
  std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
  alignedFree(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
  alignedFree(pointer);
}
void operator delete(void* pointer, size_t, std::align_val_t) noexcept {
  alignedFree(pointer);
}
void operator delete[](void* pointer, size_t, std::align_val_t) noexcept {
  alignedFree(pointer);
}
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
  alignedFree(pointer);
}
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept {
  alignedFree(pointer);
}
