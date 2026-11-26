#include "oly/app/detail/schedule_graph.hpp"

#include <algorithm>
#include <deque>
#include <set>
#include <unordered_map>

namespace oly::app {
  namespace {
    // Function with a local static: avoids initialization order problems. std::deque keeps the names in place.
    std::deque<std::string_view>& scheduleNames() {
      static std::deque<std::string_view> names;
      return names;
    }
  }

  ScheduleKey detail::registerSchedule(std::string_view name) {
    scheduleNames().push_back(name);
    return static_cast<ScheduleKey>(scheduleNames().size() - 1);
  }

  std::string_view detail::scheduleName(ScheduleKey key) {
    return scheduleNames()[key];
  }
}

namespace oly::app::detail {
  namespace {
    using Successors = std::vector<std::set<uint32_t>>;

    struct Sorted {
      std::vector<uint32_t> order;
      std::vector<uint32_t> cyclic;
      std::set<std::pair<uint32_t, uint32_t>> ambiguous;
    };

    // Kahn's algorithm: each time, the ready node that `before` ranks first goes next, so the order depends on the ranking and
    // the edges only. Nodes that never become ready are on, or behind, a cycle.
    template <class Before>
    Sorted sortGraph(const Successors& successors, Before before) {
      std::vector<uint32_t> indegree(successors.size(), 0);

      for (const std::set<uint32_t>& targets : successors) {
        for (const uint32_t target : targets) {
          ++indegree[target];
        }
      }

      std::set<uint32_t, Before> ready(before);

      for (uint32_t node = 0; node < successors.size(); ++node) {
        if (indegree[node] == 0) {
          ready.insert(node);
        }
      }

      Sorted sorted;

      while (!ready.empty()) {
        for (auto first = ready.begin(); first != ready.end(); ++first) {
          for (auto second = std::next(first); second != ready.end(); ++second) {
            sorted.ambiguous.emplace(std::min(*first, *second), std::max(*first, *second));
          }
        }

        const uint32_t node = *ready.begin();
        ready.erase(ready.begin());
        sorted.order.push_back(node);

        for (const uint32_t target : successors[node]) {
          if (--indegree[target] == 0) {
            ready.insert(target);
          }
        }
      }

      std::vector<bool> placed(successors.size(), false);

      for (const uint32_t node : sorted.order) {
        placed[node] = true;
      }

      for (uint32_t node = 0; node < successors.size(); ++node) {
        if (!placed[node]) {
          sorted.cyclic.push_back(node);
        }
      }

      std::sort(sorted.cyclic.begin(), sorted.cyclic.end(), before);

      return sorted;
    }

    // `cyclic` also holds the nodes behind a cycle, and such a node may have no successor left. Every node that was never placed has a
    // predecessor that was never placed either (its indegree did not reach zero), so the walk goes backwards from the first node that
    // `before` ranks, following the first such predecessor until a node repeats; the repeat closes the loop, which is then reversed.
    template <class Before>
    std::vector<uint32_t> findCycle(const Successors& successors, const std::vector<uint32_t>& cyclic, Before before) {
      const std::set<uint32_t> remaining(cyclic.begin(), cyclic.end());
      std::vector<uint32_t> path{cyclic.front()};

      while (true) {
        std::vector<uint32_t> candidates;

        for (const uint32_t node : cyclic) {
          if (successors[node].contains(path.back())) {
            candidates.push_back(node);
          }
        }

        const uint32_t previous = *std::min_element(candidates.begin(), candidates.end(), before);
        const auto repeated = std::find(path.begin(), path.end(), previous);

        if (repeated != path.end()) {
          std::vector<uint32_t> cycle(repeated, path.end());
          cycle.push_back(previous);
          std::reverse(cycle.begin(), cycle.end());

          return cycle;
        }

        path.push_back(previous);
      }
    }

    template <class Name>
    std::string joinCycle(const std::vector<uint32_t>& cycle, Name nameOf) {
      std::string text;

      for (const uint32_t node : cycle) {
        text += text.empty() ? "" : " -> ";
        text += nameOf(node);
      }

      return text;
    }

    // Keeps the first report of each message, in the order they were found.
    class Errors {
     private:
      std::vector<std::string>& list_;
      std::set<std::string> seen_;

     public:
      explicit Errors(std::vector<std::string>& list)
        : list_(list) {}

      void add(std::string message) {
        if (seen_.insert(message).second) {
          list_.push_back(std::move(message));
        }
      }
    };

    constexpr uint32_t Missing = 0xFFFFFFFFu;

    struct Index {
      std::unordered_map<ScheduleKey, uint32_t> phaseByKey;
      std::unordered_map<ScheduleKey, std::vector<SystemId>> membersOfSet;
      std::unordered_map<SystemFunctionKey, std::vector<SystemId>> registrationsOfFunction;
      std::unordered_map<std::string, SystemId> systemByName;
    };

    Index indexOf(const ScheduleInput& input) {
      Index index;

      for (uint32_t phase = 0; phase < input.phases.size(); ++phase) {
        index.phaseByKey.emplace(input.phases[phase].key, phase);
      }

      for (SystemId system = 0; system < input.systems.size(); ++system) {
        const GraphSystem& graphSystem = input.systems[system];

        for (const ScheduleKey set : graphSystem.sets) {
          index.membersOfSet[set].push_back(system);
        }

        index.registrationsOfFunction[graphSystem.function].push_back(system);
        index.systemByName.emplace(graphSystem.name, system);
      }

      return index;
    }

    std::vector<SystemId> expand(const OrderTarget& target, const Index& index, size_t systemCount) {
      switch (target.kind) {
        case TargetKind::Set: {
          const auto found = index.membersOfSet.find(target.key);
          return found == index.membersOfSet.end() ? std::vector<SystemId>{} : found->second;
        }
        case TargetKind::Function: {
          const auto found = index.registrationsOfFunction.find(target.key);
          return found == index.registrationsOfFunction.end() ? std::vector<SystemId>{} : found->second;
        }
        case TargetKind::Name: {
          const auto found = index.systemByName.find(target.name);
          return found == index.systemByName.end() ? std::vector<SystemId>{} : std::vector<SystemId>{found->second};
        }
        case TargetKind::System:
          return target.key < systemCount ? std::vector<SystemId>{target.key} : std::vector<SystemId>{};
      }

      return {};
    }

    // What a constraint that names no system says about its target.
    std::string describe(const OrderTarget& target) {
      switch (target.kind) {
        case TargetKind::Set:
          return "set \"" + target.name + "\", which has no member";
        case TargetKind::Function:
          return "a function that is never registered";
        case TargetKind::Name:
        case TargetKind::System:
          return "system \"" + target.name + "\", which is not registered";
      }

      return {};
    }

    std::string scheduleLabel(ScheduleKind schedule) {
      return schedule == ScheduleKind::FixedStep ? "the fixed step" : "the frame";
    }

    // Orders the phases of one schedule; returns their indices into input.phases in run order.
    std::vector<uint32_t> orderPhases(const ScheduleInput& input, ScheduleKind schedule, const std::vector<std::pair<uint32_t, uint32_t>>& edges, Errors& errors) {
      std::vector<uint32_t> nodes;
      std::unordered_map<uint32_t, uint32_t> localOf;

      for (uint32_t phase = 0; phase < input.phases.size(); ++phase) {
        if (input.phases[phase].declared && input.phases[phase].schedule == schedule) {
          localOf.emplace(phase, static_cast<uint32_t>(nodes.size()));
          nodes.push_back(phase);
        }
      }

      Successors successors(nodes.size());

      for (const auto& [first, second] : edges) {
        const auto firstLocal = localOf.find(first);
        const auto secondLocal = localOf.find(second);

        if (firstLocal != localOf.end() && secondLocal != localOf.end()) {
          successors[firstLocal->second].insert(secondLocal->second);
        }
      }

      const auto byDeclaration = [](uint32_t left, uint32_t right) { return left < right; };
      Sorted sorted = sortGraph(successors, byDeclaration);

      for (const auto& [first, second] : sorted.ambiguous) {
        errors.add("phases \"" + input.phases[nodes[first]].name + "\" and \"" + input.phases[nodes[second]].name + "\" of " + scheduleLabel(schedule) + " have no order between them");
      }

      if (!sorted.cyclic.empty()) {
        const std::vector<uint32_t> cycle = findCycle(successors, sorted.cyclic, byDeclaration);
        errors.add("cycle of phase constraints: " + joinCycle(cycle, [&](uint32_t node) { return input.phases[nodes[node]].name; }));
        sorted.order.insert(sorted.order.end(), sorted.cyclic.begin(), sorted.cyclic.end());
      }

      std::vector<uint32_t> order;

      for (const uint32_t node : sorted.order) {
        order.push_back(nodes[node]);
      }

      return order;
    }

    std::string conflictMessage(const GraphPhase& phase, const GraphSystem& first, const GraphSystem& second, const Conflict& conflict) {
      const std::string pair = "systems \"" + first.name + "\" and \"" + second.name + "\" of phase \"" + phase.name + "\" have no order and ";

      if (conflict.kind == ConflictKind::Exclusive) {
        return pair + "\"" + (conflict.firstWrites ? first.name : second.name) + "\" is exclusive";
      }

      const std::string name = conflict.kind == ConflictKind::Component ? std::string(ecs::typeInfo(conflict.id).name) : std::string(ecs::resourceName(conflict.id));
      const char* kind = conflict.kind == ConflictKind::Component ? "component" : "resource";

      return pair + "both access " + kind + " \"" + name + "\" (" + first.name + (conflict.firstWrites ? " writes, " : " reads, ") + second.name + (conflict.secondWrites ? " writes)" : " reads)");
    }

    // Two systems of a phase that no path of constraints orders, and whose access conflicts, are refused. The pairs are
    // visited in dispatch order of the first system, then of the second.
    void reportConflicts(const ScheduleInput& input, const GraphPhase& phase, const std::vector<SystemId>& dispatch, const Successors& graph, const std::unordered_map<SystemId, uint32_t>& localOf, Errors& errors) {
      std::vector<std::vector<bool>> reaches(graph.size(), std::vector<bool>(graph.size(), false));

      for (uint32_t start = 0; start < graph.size(); ++start) {
        std::vector<uint32_t> pending(graph[start].begin(), graph[start].end());

        while (!pending.empty()) {
          const uint32_t node = pending.back();
          pending.pop_back();

          if (!reaches[start][node]) {
            reaches[start][node] = true;
            pending.insert(pending.end(), graph[node].begin(), graph[node].end());
          }
        }
      }

      for (size_t firstSlot = 0; firstSlot < dispatch.size(); ++firstSlot) {
        for (size_t secondSlot = firstSlot + 1; secondSlot < dispatch.size(); ++secondSlot) {
          const uint32_t firstLocal = localOf.at(dispatch[firstSlot]);
          const uint32_t secondLocal = localOf.at(dispatch[secondSlot]);

          if (reaches[firstLocal][secondLocal] || reaches[secondLocal][firstLocal]) {
            continue;
          }

          const GraphSystem& first = input.systems[dispatch[firstSlot]];
          const GraphSystem& second = input.systems[dispatch[secondSlot]];

          for (const Conflict& conflict : conflictsBetween(first.access, second.access)) {
            errors.add(conflictMessage(phase, first, second, conflict));
          }
        }
      }
    }
  }

  SchedulePlan buildSchedule(const ScheduleInput& input) {
    SchedulePlan plan;
    plan.systemsByPhase.resize(input.phases.size());
    Errors errors(plan.errors);
    const Index index = indexOf(input);

    std::set<ScheduleKey> setKeys;

    for (const GraphSystem& system : input.systems) {
      setKeys.insert(system.sets.begin(), system.sets.end());
    }

    for (const OrderConstraint& constraint : input.constraints) {
      for (const OrderTarget* target : {&constraint.first, &constraint.second}) {
        if (target->kind == TargetKind::Set) {
          setKeys.insert(target->key);
        }
      }
    }

    for (const ScheduleKey key : setKeys) {
      if (index.phaseByKey.count(key) != 0) {
        errors.add("\"" + std::string(scheduleName(key)) + "\" is both a phase and a set");
      }
    }

    std::vector<std::pair<uint32_t, uint32_t>> phaseEdges;

    for (const auto& [firstKey, secondKey] : input.phaseConstraints) {
      const auto first = index.phaseByKey.find(firstKey);
      const auto second = index.phaseByKey.find(secondKey);
      bool valid = true;

      for (const auto& [key, found] : {std::pair{firstKey, first}, std::pair{secondKey, second}}) {
        if (found == index.phaseByKey.end() || !input.phases[found->second].declared) {
          const std::string name = found == index.phaseByKey.end() ? std::string(scheduleName(key)) : input.phases[found->second].name;
          errors.add("phase constraint names \"" + name + "\", which is not declared");
          valid = false;
        }
      }

      if (!valid) {
        continue;
      }

      if (input.phases[first->second].schedule != input.phases[second->second].schedule) {
        errors.add("phases \"" + input.phases[first->second].name + "\" and \"" + input.phases[second->second].name + "\" are in different schedules");
        continue;
      }

      phaseEdges.emplace_back(first->second, second->second);
    }

    plan.fixedStepPhases = orderPhases(input, ScheduleKind::FixedStep, phaseEdges, errors);
    plan.framePhases = orderPhases(input, ScheduleKind::Frame, phaseEdges, errors);

    std::vector<uint32_t> position(input.phases.size(), Missing);
    uint32_t next = 0;

    for (const auto* phases : {&plan.fixedStepPhases, &plan.framePhases}) {
      for (const uint32_t phase : *phases) {
        position[phase] = next++;
      }
    }

    std::vector<uint32_t> phaseOfSystem(input.systems.size(), Missing);
    std::vector<std::vector<SystemId>> members(input.phases.size());

    for (SystemId system = 0; system < input.systems.size(); ++system) {
      const auto found = index.phaseByKey.find(input.systems[system].phase);

      if (found == index.phaseByKey.end() || !input.phases[found->second].declared) {
        const std::string name = found == index.phaseByKey.end() ? std::string(scheduleName(input.systems[system].phase)) : input.phases[found->second].name;
        errors.add("system \"" + input.systems[system].name + "\" is in phase \"" + name + "\", which is not declared");
        continue;
      }

      phaseOfSystem[system] = found->second;
      members[found->second].push_back(system);
    }

    std::vector<Successors> graphs(input.phases.size());
    std::vector<std::unordered_map<SystemId, uint32_t>> localOf(input.phases.size());

    for (uint32_t phase = 0; phase < input.phases.size(); ++phase) {
      graphs[phase].resize(members[phase].size());

      for (uint32_t local = 0; local < members[phase].size(); ++local) {
        localOf[phase].emplace(members[phase][local], local);
      }
    }

    for (const OrderConstraint& constraint : input.constraints) {
      const std::vector<SystemId> firsts = expand(constraint.first, index, input.systems.size());
      const std::vector<SystemId> seconds = expand(constraint.second, index, input.systems.size());

      for (const auto& [target, expanded] : {std::pair{&constraint.first, &firsts}, std::pair{&constraint.second, &seconds}}) {
        if (expanded->empty()) {
          errors.add("constraint names " + describe(*target));
        }
      }

      for (const SystemId first : firsts) {
        for (const SystemId second : seconds) {
          const uint32_t firstPhase = phaseOfSystem[first];
          const uint32_t secondPhase = phaseOfSystem[second];

          if (firstPhase == Missing || secondPhase == Missing) {
            continue;
          }

          if (firstPhase == secondPhase) {
            graphs[firstPhase][localOf[firstPhase][first]].insert(localOf[firstPhase][second]);
          } else if (position[firstPhase] > position[secondPhase]) {
            errors.add("system \"" + input.systems[first].name + "\" (phase \"" + input.phases[firstPhase].name + "\") must run before \"" + input.systems[second].name + "\" (phase \"" + input.phases[secondPhase].name + "\"), but phase \"" + input.phases[secondPhase].name + "\" runs before \"" + input.phases[firstPhase].name + "\"");
          }
        }
      }
    }

    for (uint32_t phase = 0; phase < input.phases.size(); ++phase) {
      const std::vector<SystemId>& phaseMembers = members[phase];
      const auto byName = [&](uint32_t left, uint32_t right) {
        const std::string& leftName = input.systems[phaseMembers[left]].name;
        const std::string& rightName = input.systems[phaseMembers[right]].name;

        return leftName != rightName ? leftName < rightName : left < right;
      };
      Sorted sorted = sortGraph(graphs[phase], byName);

      if (!sorted.cyclic.empty()) {
        const std::vector<uint32_t> cycle = findCycle(graphs[phase], sorted.cyclic, byName);
        errors.add("cycle of system constraints in phase \"" + input.phases[phase].name + "\": " + joinCycle(cycle, [&](uint32_t node) { return input.systems[phaseMembers[node]].name; }));
        sorted.order.insert(sorted.order.end(), sorted.cyclic.begin(), sorted.cyclic.end());
      }

      for (const uint32_t node : sorted.order) {
        plan.systemsByPhase[phase].push_back(phaseMembers[node]);
      }

      reportConflicts(input, input.phases[phase], plan.systemsByPhase[phase], graphs[phase], localOf[phase], errors);
    }

    return plan;
  }
}
