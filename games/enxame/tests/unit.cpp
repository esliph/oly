#include <cstdint>
#include <cstdio>
#include <vector>

#include "flow_field.hpp"
#include "hashing.hpp"
#include "random.hpp"
#include "spatial_hash.hpp"
#include "statistics.hpp"
#include "tile_map.hpp"

namespace {
  int failures = 0;

  void check(bool condition, const char* description) {
    if (!condition) {
      std::fprintf(stderr, "FAIL %s\n", description);
      ++failures;
    }
  }

  uint32_t countWithin(const enxame::SpatialHash& hash, float x, float y, float radius, std::vector<uint32_t>* visited = nullptr) {
    uint32_t count = 0;

    hash.forEachWithin(x, y, radius, [&](oly::ecs::Entity entity, float, float) {
      if (visited != nullptr) {
        visited->push_back(entity.index);
      }

      ++count;
    });

    return count;
  }

  void randomFollowsSplitMix64() {
    enxame::Random random(0);

    const uint64_t first = random.nextU64();
    const uint64_t second = random.nextU64();

    check(first == 0xe220a8397b1dcdafull, "Random(0): first value is SplitMix64's");
    check(second == 0x6e789e6aa1b965f4ull, "Random(0): second value is SplitMix64's");
  }

  void blockRefusesOutsideBaseAndBlockedTiles() {
    enxame::TileMap map(5, 5, 8.0f, {2, 2});

    const bool first = map.block({0, 0});
    const bool again = map.block({0, 0});
    const bool base = map.block({2, 2});
    const bool outside = map.block({5, 0});

    check(first && map.blocked({0, 0}), "block: a free tile becomes blocked");
    check(!again && !base && !outside, "block: blocked, base and outside tiles are refused");
    check(map.version() == 1, "block: only the accepted call bumps the version");
  }

  void flowFieldOnAnOpenMapGoesDiagonally() {
    enxame::TileMap map(5, 5, 8.0f, {2, 2});
    enxame::FlowField field;

    field.build(map);

    check(field.distance({0, 0}) == 2, "open map: corner is 2 steps from the base");
    check(field.direction({0, 0}).x == 1 && field.direction({0, 0}).y == 1, "open map: corner points diagonally");
    check(field.direction({2, 2}).x == 0 && field.direction({2, 2}).y == 0, "open map: base has no direction");
  }

  void flowFieldDoesNotCutCorners() {
    enxame::TileMap map(5, 5, 8.0f, {2, 2});
    map.block({1, 2});
    map.block({2, 1});
    enxame::FlowField field;

    field.build(map);

    check(field.distance({1, 1}) == 6, "walls: (1, 1) goes around, 6 steps");
    check(field.direction({1, 1}).x == 0 && field.direction({1, 1}).y == -1, "walls: (1, 1) points north first");
    check(field.distance({2, 1}) == enxame::FlowField::Unreachable, "walls: a blocked tile is unreachable");
  }

  void flowFieldMarksEnclosedTilesUnreachable() {
    enxame::TileMap map(5, 5, 8.0f, {2, 2});
    map.block({0, 1});
    map.block({1, 0});
    map.block({1, 1});
    enxame::FlowField field;

    field.build(map);

    check(field.distance({0, 0}) == enxame::FlowField::Unreachable, "enclosed: (0, 0) is unreachable");
    check(field.direction({0, 0}).x == 0 && field.direction({0, 0}).y == 0, "enclosed: (0, 0) has no direction");
    check(field.builtVersion() == map.version(), "enclosed: the field records the map version");
  }

  void spatialHashFindsPointsWithinRadius() {
    enxame::SpatialHash hash(0.0f, 0.0f, 32.0f, 32.0f, 8.0f);
    const std::vector<oly::ecs::Entity> entities{{1, 0}, {2, 0}, {3, 0}};
    const std::vector<float> xs{1.0f, 5.0f, 20.0f};
    const std::vector<float> ys{1.0f, 5.0f, 20.0f};

    hash.rebuild(entities, xs, ys);
    std::vector<uint32_t> visited;
    const uint32_t count = countWithin(hash, 0.0f, 0.0f, 8.0f, &visited);

    check(hash.size() == 3, "hash: size counts the rebuilt entries");
    check(count == 2, "hash: a query visits the two points within the radius");
    check(visited == std::vector<uint32_t>({1, 2}), "hash: the visited points are the first two");
  }

  void spatialHashClampsOutsidePoints() {
    enxame::SpatialHash hash(0.0f, 0.0f, 32.0f, 32.0f, 8.0f);
    const std::vector<oly::ecs::Entity> entities{{1, 0}};
    const std::vector<float> xs{-10.0f};
    const std::vector<float> ys{40.0f};

    hash.rebuild(entities, xs, ys);
    const uint32_t count = countWithin(hash, -10.0f, 40.0f, 1.0f);

    check(count == 1, "hash: a point outside the area is found at its real position");
  }

  void spatialHashEmptyRebuild() {
    enxame::SpatialHash hash(0.0f, 0.0f, 32.0f, 32.0f, 8.0f);

    hash.rebuild({}, {}, {});
    const uint32_t count = countWithin(hash, 0.0f, 0.0f, 100.0f);

    check(hash.size() == 0, "hash: an empty rebuild has size 0");
    check(count == 0, "hash: a query over an empty hash visits nothing");
  }

  void percentileIsNearestRank() {
    std::vector<uint64_t> unsorted{5, 1, 3, 2, 4};
    std::vector<uint64_t> ten{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    const uint64_t median = enxame::percentile(unsorted, 50.0);
    const uint64_t high = enxame::percentile(unsorted, 99.0);
    const uint64_t maximum = enxame::percentile(unsorted, 100.0);
    const uint64_t tenMedian = enxame::percentile(ten, 50.0);
    const uint64_t tenHigh = enxame::percentile(ten, 99.0);

    check(median == 3 && high == 5 && maximum == 5, "percentile: {5,1,3,2,4} gives p50 3, p99 5, p100 5");
    check(tenMedian == 5 && tenHigh == 10, "percentile: 1..10 gives p50 5 and p99 10");
  }

  void fnv1aMatchesTheReference() {
    const uint64_t hash = enxame::fnv1a(enxame::FnvOffsetBasis, "a", 1);

    check(hash == 0xaf63dc4c8601ec8cull, "fnv1a: \"a\" matches the reference value");
  }

  void generateKeepsTheBaseFree() {
    enxame::Random full(7);
    enxame::Random none(7);

    const enxame::TileMap blocked = enxame::TileMap::generate(7, 7, 8.0f, 1.0f, full);
    const enxame::TileMap open = enxame::TileMap::generate(7, 7, 8.0f, 0.0f, none);

    bool baseAndNeighboursFree = true;
    bool othersBlocked = true;
    bool openFree = true;

    for (int32_t y = 0; y < 7; ++y) {
      for (int32_t x = 0; x < 7; ++x) {
        const bool nearBase = x >= 2 && x <= 4 && y >= 2 && y <= 4;

        baseAndNeighboursFree = baseAndNeighboursFree && (!nearBase || !blocked.blocked({x, y}));
        othersBlocked = othersBlocked && (nearBase || blocked.blocked({x, y}));
        openFree = openFree && !open.blocked({x, y});
      }
    }

    check(baseAndNeighboursFree, "generate: the base and its 8 neighbours stay free at rate 1");
    check(othersBlocked, "generate: every other tile is blocked at rate 1");
    check(openFree, "generate: no tile is blocked at rate 0");
  }
}

int main() {
  randomFollowsSplitMix64();
  blockRefusesOutsideBaseAndBlockedTiles();
  flowFieldOnAnOpenMapGoesDiagonally();
  flowFieldDoesNotCutCorners();
  flowFieldMarksEnclosedTilesUnreachable();
  spatialHashFindsPointsWithinRadius();
  spatialHashClampsOutsidePoints();
  spatialHashEmptyRebuild();
  percentileIsNearestRank();
  fnv1aMatchesTheReference();
  generateKeepsTheBaseFree();

  return failures == 0 ? 0 : 1;
}
