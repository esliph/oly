#pragma once

#include <cstdint>

namespace oly::app {
  class Engine;

  using PluginKey = uint32_t;

  // A unit of engine functionality that installs itself into an Engine: install registers its systems, handlers,
  // hooks and observers, and keeps its own state, which reaches its systems through userData. The Engine owns the
  // instance and destroys it in reverse install order, after discarding the pending tasks, so a task's discard may still
  // reach the plugin through its payload. Systems, handlers and hooks die with the Engine, but observers
  // belong to the World, which outlives the Engine: the destructor removes the observers install registered
  // (World::removeObserver), or the World would call into a destroyed plugin.
  class Plugin {
   public:
    virtual ~Plugin() = default;

    virtual void install(Engine& engine) = 0;
  };

  namespace detail {
    PluginKey nextPluginKey() noexcept;
  }

  // Dense per process, like eventKey.
  template <class P>
  PluginKey pluginKey() noexcept {
    static const PluginKey key = detail::nextPluginKey();
    return key;
  }
}
