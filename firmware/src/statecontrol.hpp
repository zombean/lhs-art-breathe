#ifndef JMT_STATECONTROL
#define JMT_STATECONTROL

#include <stdint.h>
#include <string.h>
#include <esp_vfs.h>
#include <errno.h>
#include <cJSON.h>

#include "filesystems.hpp"

struct SegmentGroup {
  SegmentGroup* next;
  uint length;
  uint* segments;
};

extern SegmentGroup* segmentGroups;

enum ControlMode {
    Set = 0,
    Linear = 1,
};

class SegmentController {
  public:
    SegmentController() {};
    void loadConfig(uint64_t startTime, uint64_t endTime, uint8_t targetColor[3], ControlMode cm);
    ControlMode getControlMode() { return this->cm; };
    void getCurrentColor(uint8_t color[3]);
    uint64_t getEndTime() { return endTime; };
    void tick(uint64_t newTime);
    void reset();
    //unsigned int stepDelta(unsigned int step);
  private:
    ControlMode cm = Set;
    uint64_t startTime = 0, endTime = 0, currentTime = 0;
    uint8_t initialColor[3] = {0,0,0}, targetColor[3] = {0,0,0}, currentColor[3] = {0,0,0};
};

#define SECTORS 4
#define SEG_PER_QUAD 6
#define CENTER 50
#define SEGMENTS ((SECTORS*SEG_PER_QUAD)+CENTER)

enum RunMode {
  InitialInternal = 0,
  NoSDInternal,
  MainSD,
};

struct RunState {
  char patternName[128];
  uint nextPatternIndex;
  cJSON* config;
  uint lastCommandIndex;
  uint64_t currentTime;
  RunMode runMode;
  
  SegmentController segmentControllers[SEGMENTS];
};

SegmentGroup* initSegmentGroups();
void getNextPattern(const char* path, const char* pattern, char** line, size_t* length);
void updateControlersFromJson(RunState* runstate);
void tickControllers(RunState* runstate);
void loadPattern(RunState* runstate);
bool doLoad(RunState* runstate);
int getTickTime(RunState* runstate);
bool isEndOfPattern(RunState* runstate);
bool isEndOfCommands(RunState* runstate);
void doTick(RunState* runstate, void(*outTick)(RunState*));
void getAudioName(RunState* runstate, char name[255]);

#endif // JMT_FILESYSTEMS