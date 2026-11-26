#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

#include "oly/core/type_name.hpp"

namespace oly::ecs {
  using ResourceId = uint32_t;

  // The name stays valid until the end of the process. `id` must be less than registeredResourceCount() (not checked).
  std::string_view resourceName(ResourceId id);
  uint32_t registeredResourceCount() noexcept;

  namespace detail {
    ResourceId registerResource(std::string_view name);

    // The static lives in U (already without cvref) so that resourceId<const A&> and resourceId<A> share the ID.
    template <class U>
    ResourceId resourceIdOf() {
      static const ResourceId id = registerResource(core::typeName<U>());
      return id;
    }

    struct ResourceSlot {
      void* object = nullptr;
      void (*destroy)(void* object) noexcept = nullptr;
    };

    // Owns one object per ResourceId. The objects never move; the destructor destroys them in reverse insertion order.
    class ResourceStore {
     private:
      std::vector<ResourceSlot> slots_;
      std::vector<ResourceId> insertionOrder_;

     public:
      ResourceStore() = default;
      ResourceStore(const ResourceStore&) = delete;
      ResourceStore& operator=(const ResourceStore&) = delete;
      ~ResourceStore();

      void* find(ResourceId id) const noexcept {
        return id < slots_.size() ? slots_[id].object : nullptr;
      }

      // Takes ownership only when it returns: if it throws, the caller still owns `object`.
      void adopt(ResourceId id, void* object, void (*destroy)(void* object) noexcept);

      const std::vector<ResourceSlot>& slots() const noexcept {
        return slots_;
      }

      const std::vector<ResourceId>& insertionOrder() const noexcept {
        return insertionOrder_;
      }
    };
  }

  // Dense ID, assigned on the first call for each type: it depends on the order of those calls,
  // so it is not suitable for persisting. Ignores const, volatile and reference.
  template <class T>
  ResourceId resourceId() {
    return detail::resourceIdOf<std::remove_cvref_t<T>>();
  }
}
