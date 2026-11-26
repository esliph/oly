#pragma once

#include <cstdint>

#include "oly/app/access.hpp"
#include "oly/app/detail/system_adapter.hpp"
#include "oly/app/system_parameters.hpp"

namespace oly::app {
  using SystemFunctionKey = uint32_t;

  namespace detail {
    SystemFunctionKey nextSystemFunctionKey() noexcept;
  }

  // Dense per process and separate from SystemId: one key per function, whatever the names it is registered under.
  template <auto Function>
  SystemFunctionKey systemFunctionKey() {
    static const SystemFunctionKey key = detail::nextSystemFunctionKey();
    return key;
  }

  struct SystemDescriptor {
    void (*declare)(Access& access);
    void (*resolve)(detail::ResolveContext& context, detail::SystemSlots& slots);
    void (*run)(const detail::SystemSlots& slots, detail::RunContext& context);
    SystemFunctionKey key;
  };

  // Function is a function pointer; a captureless lambda passes as +[](...) {}. One instance per Function.
  template <auto Function>
  const SystemDescriptor& systemDescriptor() {
    using Adapter = detail::SystemAdapter<Function, typename detail::SystemSignature<decltype(Function)>::ParameterList>;

    static const SystemDescriptor descriptor{&Adapter::declare, &Adapter::resolve, &Adapter::run, systemFunctionKey<Function>()};
    return descriptor;
  }
}
