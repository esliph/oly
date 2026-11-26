#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "oly/app/access.hpp"
#include "oly/app/detail/event_queues.hpp"
#include "oly/app/detail/state_scoped.hpp"
#include "oly/app/event.hpp"
#include "oly/app/phase.hpp"
#include "oly/app/scheduler.hpp"
#include "oly/app/state.hpp"
#include "oly/app/system.hpp"
#include "oly/core/type_name.hpp"
#include "oly/ecs/world.hpp"

namespace oly::app {
  // Constant for a whole phase run: `paused` is the pause request read when the phase starts.
  struct StepInfo {
    double time;
    float fixedDeltaTime;
    float interpolation;
    StateId state;
    bool paused;
  };

  struct SystemInfo {
    SystemId id;
    std::string_view name;
    PhaseId phase;
    ScheduleKind schedule;
  };

  namespace detail {
    template <class...>
    struct TypeList {};

    template <class T>
    struct IsFilter : std::false_type {};
    template <class... Us>
    struct IsFilter<ecs::With<Us...>> : std::true_type {};
    template <class... Us>
    struct IsFilter<ecs::Without<Us...>> : std::true_type {};

    // Splits the parameters of Query<Ts...> into the components it iterates and the filters it only matches.
    template <class Components, class Filters, class... Rest>
    struct SplitQuery;
    template <class... Components, class... Filters>
    struct SplitQuery<TypeList<Components...>, TypeList<Filters...>> {
      using ComponentList = TypeList<Components...>;
      using FilterList = TypeList<Filters...>;
      using EcsQuery = ecs::Query<Components...>;
    };
    template <class... Components, class... Filters, class T, class... Rest>
    struct SplitQuery<TypeList<Components...>, TypeList<Filters...>, T, Rest...>
      : std::conditional_t<IsFilter<T>::value, SplitQuery<TypeList<Components...>, TypeList<Filters..., T>, Rest...>, SplitQuery<TypeList<Components..., T>, TypeList<Filters...>, Rest...>> {};

    template <class T>
    inline constexpr bool IsTag = std::is_empty_v<std::remove_cv_t<T>>;
  }

  // Ts: T or const T, plus ecs::With<...> and ecs::Without<...> in any position.
  template <class... Ts>
  class Query {
   private:
    using Split = detail::SplitQuery<detail::TypeList<>, detail::TypeList<>, Ts...>;

    typename Split::EcsQuery* query_;

   public:
    explicit Query(typename Split::EcsQuery& query) noexcept
      : query_(&query) {}

    size_t matchedCount() const noexcept {
      return query_->matchedCount();
    }
    template <class F>
    void each(F&& callback) {
      query_->each(std::forward<F>(callback));
    }
    template <class F>
    void eachBlock(F&& callback) {
      query_->eachBlock(std::forward<F>(callback));
    }
    auto single() {
      return query_->single();
    }
    auto trySingle() {
      return query_->trySingle();
    }
  };

  // Lookup<const Tag> gives has only; a tag cannot be written.
  template <class T>
  class Lookup {
    static_assert(!detail::IsTag<T> || std::is_const_v<T>, "Lookup<Tag> is not allowed; use Lookup<const Tag>, which gives has only");

   private:
    ecs::World* world_;

   public:
    explicit Lookup(ecs::World& world) noexcept
      : world_(&world) {}

    T& get(ecs::Entity entity) const
      requires(!detail::IsTag<T>)
    {
      return world_->template get<T>(entity);
    }
    T* tryGet(ecs::Entity entity) const noexcept
      requires(!detail::IsTag<T>)
    {
      return world_->template tryGet<T>(entity);
    }
    bool has(ecs::Entity entity) const noexcept {
      return world_->template has<T>(entity);
    }
  };

  template <class T>
  class Res {
   private:
    T* object_;

   public:
    explicit Res(T* object) noexcept
      : object_(object) {}

    T& get() const noexcept {
      return *object_;
    }
    T& operator*() const noexcept {
      return *object_;
    }
    T* operator->() const noexcept {
      return object_;
    }
  };

  // Structural changes deferred to the phase's flush. Entities born while a state is active are scoped until they receive persist.
  class Commands {
   private:
    ecs::CommandBuffer* commands_;
    const ecs::World* world_;
    bool spawnsScoped_;

   public:
    Commands(ecs::CommandBuffer& commands, const ecs::World& world, bool spawnsScoped) noexcept
      : commands_(&commands),
        world_(&world),
        spawnsScoped_(spawnsScoped) {}

    ecs::Entity spawn() {
      const ecs::Entity entity = commands_->spawn();

      if (spawnsScoped_) {
        commands_->add(entity, detail::StateScoped{});
      }

      return entity;
    }
    template <class... Ts>
    ecs::Entity spawn(Ts&&... components) {
      const ecs::Entity entity = spawn();
      (commands_->add(entity, std::forward<Ts>(components)), ...);

      return entity;
    }
    // Deferred like remove: no effect if the entity does not have the tag.
    void persist(ecs::Entity entity) {
      commands_->remove<detail::StateScoped>(entity);
    }
    void destroy(ecs::Entity entity) {
      commands_->destroy(entity);
    }
    template <class T>
    void add(ecs::Entity entity, T&& value) {
      commands_->add(entity, std::forward<T>(value));
    }
    template <class T>
    void remove(ecs::Entity entity) {
      commands_->template remove<T>(entity);
    }
    void addById(ecs::Entity entity, ecs::ComponentId componentId, void* value) {
      commands_->addById(entity, componentId, value);
    }
    void removeById(ecs::Entity entity, ecs::ComponentId componentId) {
      commands_->removeById(entity, componentId);
    }
    // The entity store changes only at the phase's flush, so reading liveness declares nothing.
    bool isAlive(ecs::Entity entity) const noexcept {
      return world_->isAlive(entity);
    }
  };

  // The event is dispatched in the next round of the current step.
  template <class E>
  class Events {
    static_assert(std::is_trivially_copyable_v<E>, "Events<E>: the event must be trivially copyable");

   private:
    detail::EventQueues* queues_;
    EventTypeId type_;

   public:
    Events(detail::EventQueues& queues, EventTypeId type) noexcept
      : queues_(&queues),
        type_(type) {}

    void emit(const E& event) {
      queues_->push(type_, &event);
    }
  };

  // Appends only: cancel, isPending and persist are not here, so the parameter declares nothing.
  class Tasks {
   private:
    Scheduler* scheduler_;

   public:
    explicit Tasks(Scheduler& scheduler) noexcept
      : scheduler_(&scheduler) {}

    template <class... Arguments>
    TaskHandle schedule(Arguments&&... arguments) {
      return scheduler_->schedule(std::forward<Arguments>(arguments)...);
    }
    template <class... Arguments>
    TaskHandle after(Arguments&&... arguments) {
      return scheduler_->after(std::forward<Arguments>(arguments)...);
    }
    template <class... Arguments>
    TaskHandle every(Arguments&&... arguments) {
      return scheduler_->every(std::forward<Arguments>(arguments)...);
    }
  };

  namespace detail {
    inline constexpr uint32_t MaxSystemParameters = 16;
    using SystemSlots = std::array<void*, MaxSystemParameters>;

    // The Engine's own resources (StateRequest, PauseRequest, ActionMap, Scheduler), looked up before the World's. An entry
    // with a null pointer is empty.
    struct EngineControls {
      std::array<std::pair<ecs::ResourceId, void*>, 4> entries;

      void* find(ecs::ResourceId id) const noexcept {
        for (const auto& [entryId, object] : entries) {
          if (object != nullptr && entryId == id) {
            return object;
          }
        }

        return nullptr;
      }
    };

    struct ResolveContext {
      ecs::World& world;
      const EngineControls& controls;
      EventQueues& queues;
      Scheduler& scheduler;
      std::string_view systemName;
      std::vector<std::string>& errors;
    };

    struct RunContext {
      ecs::World& world;
      ecs::CommandBuffer& commands;
      EventQueues& queues;
      const StepInfo& step;
      const SystemInfo& system;
      bool spawnsScoped;
    };

    template <class>
    inline constexpr bool AlwaysFalse = false;

    // One specialization per parameter kind: declare(Access&), resolve(ResolveContext&) -> void*, fetch(void* slot, RunContext&) -> P.
    template <class P>
    struct SystemParameter {
      static_assert(AlwaysFalse<P>, "addSystem: this parameter type is not a system parameter (Query, Lookup, Res, Commands, Events, Tasks, const StepInfo&, const SystemInfo&, ecs::World&)");
    };

    template <class... Ts>
    struct SystemParameter<Query<Ts...>> {
      using Split = SplitQuery<TypeList<>, TypeList<>, Ts...>;

      template <class... Components>
      static void declareComponents(Access& access, TypeList<Components...>) {
        (declareComponent<Components>(access), ...);
      }

      template <class Component>
      static void declareComponent(Access& access) {
        if constexpr (std::is_const_v<Component>) {
          access.readComponent(ecs::componentId<Component>());
        } else {
          access.writeComponent(ecs::componentId<Component>());
        }
      }

      template <class... Components, class... Filters>
      static void* queryOf(ecs::World& world, TypeList<Components...>, TypeList<Filters...>) {
        return &world.template query<Components...>(Filters{}...);
      }

      static void declare(Access& access) {
        declareComponents(access, typename Split::ComponentList{});
      }
      static void* resolve(ResolveContext& context) {
        return queryOf(context.world, typename Split::ComponentList{}, typename Split::FilterList{});
      }
      static Query<Ts...> fetch(void* slot, RunContext&) {
        return Query<Ts...>(*static_cast<typename Split::EcsQuery*>(slot));
      }
    };

    template <class T>
    struct SystemParameter<Lookup<T>> {
      static void declare(Access& access) {
        if constexpr (std::is_const_v<T>) {
          access.readComponent(ecs::componentId<T>());
        } else {
          access.writeComponent(ecs::componentId<T>());
        }
      }
      static void* resolve(ResolveContext& context) {
        return &context.world;
      }
      static Lookup<T> fetch(void* slot, RunContext&) {
        return Lookup<T>(*static_cast<ecs::World*>(slot));
      }
    };

    template <class T>
    struct SystemParameter<Res<T>> {
      static void declare(Access& access) {
        if constexpr (std::is_const_v<T>) {
          access.readResource(ecs::resourceId<T>());
        } else {
          access.writeResource(ecs::resourceId<T>());
        }
      }
      static void* resolve(ResolveContext& context) {
        const ecs::ResourceId id = ecs::resourceId<T>();
        void* object = context.controls.find(id);

        if (object == nullptr) {
          object = context.world.tryResourceById(id);
        }

        if (object == nullptr) {
          context.errors.push_back("system \"" + std::string(context.systemName) + "\" needs resource \"" + std::string(ecs::resourceName(id)) + "\", which is not inserted");
        }

        return object;
      }
      static Res<T> fetch(void* slot, RunContext&) {
        return Res<T>(static_cast<T*>(slot));
      }
    };

    template <>
    struct SystemParameter<Commands> {
      static void declare(Access&) {}
      static void* resolve(ResolveContext&) {
        return nullptr;
      }
      static Commands fetch(void*, RunContext& context) {
        return Commands(context.commands, context.world, context.spawnsScoped);
      }
    };

    template <class E>
    struct SystemParameter<Events<E>> {
      static void declare(Access&) {}
      static void* resolve(ResolveContext& context) {
        const EventTypeId type = context.queues.typeOf(eventKey<E>());

        if (type == InvalidEventType) {
          context.errors.push_back("system \"" + std::string(context.systemName) + "\" emits event \"" + std::string(core::typeName<E>()) + "\", which is not declared");
        }

        return reinterpret_cast<void*>(static_cast<uintptr_t>(type));
      }
      static Events<E> fetch(void* slot, RunContext& context) {
        return Events<E>(context.queues, static_cast<EventTypeId>(reinterpret_cast<uintptr_t>(slot)));
      }
    };

    template <>
    struct SystemParameter<Tasks> {
      static void declare(Access&) {}
      static void* resolve(ResolveContext& context) {
        return &context.scheduler;
      }
      static Tasks fetch(void* slot, RunContext&) {
        return Tasks(*static_cast<Scheduler*>(slot));
      }
    };

    template <>
    struct SystemParameter<const StepInfo&> {
      static void declare(Access&) {}
      static void* resolve(ResolveContext&) {
        return nullptr;
      }
      static const StepInfo& fetch(void*, RunContext& context) {
        return context.step;
      }
    };

    template <>
    struct SystemParameter<const SystemInfo&> {
      static void declare(Access&) {}
      static void* resolve(ResolveContext&) {
        return nullptr;
      }
      static const SystemInfo& fetch(void*, RunContext& context) {
        return context.system;
      }
    };

    template <>
    struct SystemParameter<ecs::World&> {
      static void declare(Access& access) {
        access.exclusive = true;
      }
      static void* resolve(ResolveContext& context) {
        return &context.world;
      }
      static ecs::World& fetch(void* slot, RunContext&) {
        return *static_cast<ecs::World*>(slot);
      }
    };
  }
}
