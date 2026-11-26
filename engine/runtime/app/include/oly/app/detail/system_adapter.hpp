#pragma once

#include <cstddef>
#include <utility>

#include "oly/app/access.hpp"
#include "oly/app/system_parameters.hpp"

namespace oly::app::detail {
  template <class Function>
  struct SystemSignature;
  template <class... Parameters>
  struct SystemSignature<void (*)(Parameters...)> {
    using ParameterList = TypeList<Parameters...>;
  };
  template <class... Parameters>
  struct SystemSignature<void (*)(Parameters...) noexcept> {
    using ParameterList = TypeList<Parameters...>;
  };

  template <auto Function, class ParameterList>
  struct SystemAdapter;

  // Turns a function pointer into the three fixed-signature entry points of a SystemDescriptor.
  template <auto Function, class... Parameters>
  struct SystemAdapter<Function, TypeList<Parameters...>> {
    static_assert(sizeof...(Parameters) <= MaxSystemParameters, "addSystem: a system takes at most 16 parameters");

    static void declare(Access& access) {
      (SystemParameter<Parameters>::declare(access), ...);
    }

    // The fold evaluates left to right, so the errors come in the order of the parameters.
    static void resolve(ResolveContext& context, SystemSlots& slots) {
      size_t slot = 0;
      ((slots[slot++] = SystemParameter<Parameters>::resolve(context)), ...);
    }

    static void run(const SystemSlots& slots, RunContext& context) {
      call(slots, context, std::index_sequence_for<Parameters...>{});
    }

    template <size_t... Slots>
    static void call(const SystemSlots& slots, RunContext& context, std::index_sequence<Slots...>) {
      Function(SystemParameter<Parameters>::fetch(slots[Slots], context)...);
    }
  };
}
