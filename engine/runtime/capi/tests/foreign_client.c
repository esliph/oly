#include <stdint.h>
#include <stdio.h>

#include "oly/capi/c_api.h"
#include "oly/capi/engine_api.h"

struct ClientEvent {
  uint32_t value;
  float amount;
};

struct ClientComponent {
  uint64_t value;
};

struct Record {
  uint32_t componentId;
  uint32_t eventSum;
  uint32_t handlerCalls;
  uint32_t taskRuns;
  uint32_t taskId;
  uint32_t taskEventCount;
  uint32_t enteredFrom;
  uint32_t enteredCalls;
};

static int failed = 0;

static void check(int condition, int line) {
  if (!condition) {
    printf("foreign_client.c:%d\n", line);
    failed = 1;
  }
}

#define CHECK(condition) check((condition) != 0, __LINE__)

_Static_assert(sizeof(OlyEntity) == 8, "OlyEntity is two uint32_t");
_Static_assert(sizeof(OlyTaskHandle) == 8, "OlyTaskHandle is two uint32_t");
_Static_assert(sizeof(OlyActionState) == 12, "OlyActionState is two floats and a uint32_t");

static void addOneToEveryValue(struct Record* record, OlyEngine* engine) {
  OlyWorld* world = olyEngineWorld(engine);
  const uint32_t componentIds[] = {record->componentId};
  OlyQuery* query = olyQueryCreate(world, componentIds, 0, 1, 0, 0);
  uint32_t blockCount = 0;
  const uintptr_t* descriptors = NULL;
  OlyIteration* iteration = olyQueryBegin(world, query, &blockCount, &descriptors);

  for (uint32_t block = 0; block < blockCount; ++block) {
    const uintptr_t* descriptor = descriptors + block * 3;
    const uintptr_t count = descriptor[0];
    struct ClientComponent* values = (struct ClientComponent*)descriptor[2];

    for (uintptr_t row = 0; row < count; ++row) {
      values[row].value += 1;
    }
  }

  olyQueryEnd(world, iteration);
}

static void invoke(void* userData, OlyEngine* engine, uint32_t kind, uint32_t id, const void* events, uint32_t eventCount) {
  struct Record* record = (struct Record*)userData;

  if (kind == OLY_SCRIPT_SYSTEM) {
    addOneToEveryValue(record, engine);
  } else if (kind == OLY_SCRIPT_HANDLER) {
    const struct ClientEvent* batch = (const struct ClientEvent*)events;

    for (uint32_t index = 0; index < eventCount; ++index) {
      record->eventSum += batch[index].value;
    }

    record->handlerCalls += 1;
  } else if (kind == OLY_SCRIPT_TASK) {
    record->taskRuns += 1;
    record->taskId = id;
    record->taskEventCount = eventCount;
  } else if (kind == OLY_SCRIPT_STATE_HOOK) {
    record->enteredFrom = eventCount;
    record->enteredCalls += 1;
  }
}

int main(void) {
  struct Record record = {0};
  OlyTaskHandle handle = {0, 0};
  OlyEntity entity = {0, 0};
  const struct ClientEvent event = {7, 1.5f};
  struct ClientComponent initial = {41};

  CHECK(olySchemaComponentCount() == 3);
  CHECK(olyComponentSize(olySchemaComponentId(0)) == 8);
  CHECK(olySchemaEventCount() == 1);
  CHECK(olySchemaEventSize(0) == 8);
  CHECK(olySchemaActionCount() == 3);
  CHECK(olySchemaActionKind(1) == OLY_ACTION_AXIS2D);
  CHECK(olySchemaStateCount() == 2);

  record.componentId = olySchemaComponentId(0);
  OlyEngine* engine = olyEngineCreate(60.0, 5, 16);
  olyEngineSetScriptInvoker(engine, invoke, &record);
  olyEntitySpawn(olyEngineWorld(engine), &entity);
  olyComponentAdd(olyEngineWorld(engine), entity.index, entity.generation, record.componentId, &initial);
  olyEngineAddScriptSystem(engine, 1, "addOne");
  olyEngineAddScriptHandler(engine, 0, "sum");
  olyEngineScheduleScriptTask(engine, 0.5, 0.0, 9, &handle);
  olyEngineAddScriptStateHook(engine, OLY_STATE_ENTER, 1, "entered");
  olyEngineRequestState(engine, 1);
  olyEngineEmit(engine, 0, &event);

  olyEngineFrame(engine, 0.0);

  for (int step = 1; step <= 60; ++step) {
    olyEngineFrame(engine, step / 60.0);
  }

  const struct ClientComponent* value = (const struct ClientComponent*)olyComponentGet(olyEngineWorld(engine), entity.index, entity.generation, record.componentId);
  CHECK(value != NULL);
  CHECK(value != NULL && value->value == 101);
  CHECK(record.eventSum == 7);
  CHECK(record.handlerCalls == 1);
  CHECK(record.taskRuns == 1);
  CHECK(record.taskEventCount == 1);
  CHECK(record.taskId == 9);
  CHECK(record.enteredCalls == 1);
  CHECK(record.enteredFrom == OLY_NO_STATE);
  CHECK(olyEngineState(engine) == 1);
  CHECK(olyEngineIsTaskPending(engine, handle.slot, handle.generation) == 0);

  olyEngineDestroy(engine);

  return failed;
}
