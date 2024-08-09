#include <Arduino.h>
#include "statecontrol.hpp"
//#include <jsonlite.h>

#define TAG_STATE "mandala-state"

SegmentGroup* segmentGroups = initSegmentGroups();

uint center_len = 50;
uint center[] = {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73};
uint inner_len = 4;
uint inner[] = {3, 9, 14, 20};
uint mid1_len = 8;
uint mid1[] = {0, 2, 6, 8, 15, 17, 19, 23};
uint mid2_len = 8;
uint mid2[] = {1, 5, 7, 11, 12, 16, 18, 22};
uint outer_len = 4;
uint outer[] = {4, 10, 13, 21}; //, , 15

SegmentGroup* initSegmentGroups() {
  // -1 = ALL
  SegmentGroup* segmentGroups = new SegmentGroup();
  segmentGroups->length = SEGMENTS;
  segmentGroups->segments = (uint*) malloc(SEGMENTS*sizeof(uint));
  for (int i = 0; i < SEGMENTS; i++) {
    segmentGroups->segments[i] = i;
  }
  segmentGroups->next = NULL;
  // -2 = CENTER
  SegmentGroup* neg2 = new SegmentGroup();
  neg2->length = center_len;
  neg2->segments = center;
  neg2->next = NULL;
  segmentGroups->next = neg2;
  // -3 = INNER
  SegmentGroup* neg3 = new SegmentGroup();
  neg3->length = inner_len;
  neg3->segments = inner;
  neg3->next = NULL;
  neg2->next = neg3;
  // -4 = MID1
  SegmentGroup* neg4 = new SegmentGroup();
  neg4->length = mid1_len;
  neg4->segments = mid1;
  neg4->next = NULL;
  neg3->next = neg4;
  // -5 = MID2
  SegmentGroup* neg5 = new SegmentGroup();
  neg5->length = mid2_len;
  neg5->segments = mid2;
  neg5->next = NULL;
  neg4->next = neg5;
  // -6 = OUTER
  SegmentGroup* neg6 = new SegmentGroup();
  neg6->length = outer_len;
  neg6->segments = outer;
  neg6->next = NULL;
  neg5->next = neg6;
  return segmentGroups;
};

void getNextPattern(const char* path, RunState* runstate) {
  char* linev = (char*)malloc(1);
  size_t lengthv = 1;
  char** line = &linev;
  size_t* length = &lengthv;
  uint i = 0;
  // TODO: validate path, line, length?
  // Initially open file and read 
  ESP_EARLY_LOGD(TAG_STATE, "Opening Patterns File");
  FILE* fd = fopen(path, "r");
  if (fd == NULL) {
    ESP_EARLY_LOGE(TAG_STATE, "FAILED TO OPEN Patterns File");
    return;
  }
  // TODO: add file opening error handling
  ESP_EARLY_LOGD(TAG_STATE, "Grabbing File Line");
  int result = __getline(line, length, fd);
  
  // If no lines can be loaded we need to error
  if (result == -1) {
    ESP_EARLY_LOGW(TAG_STATE, "File error");
    goto cleanupAndReturn;
  }
  // If no pattern is given we just return the first pattern
  if (runstate->nextPatternIndex == 0) {
    ESP_EARLY_LOGI(TAG_STATE, "Returning first pattern");
    runstate->nextPatternIndex++;
    goto closeAndReturn;
  } else {
    //TODO FIXME
    while (i != runstate->nextPatternIndex) {
      i++;
      if ((result = __getline(line, length, fd)) == -1) {
        // End of file while searching for next pattern
        // So get first pattern and return
        result = fseek(fd, 0, SEEK_SET); 
        if (result == -1) {
          ESP_EARLY_LOGW(TAG_STATE, "No next pattern");
          goto cleanupAndReturn;
        }
        // cannot fail to seek to start of file AFIK
        result = __getline(line, length, fd);
        if (result == -1) {
          goto cleanupAndReturn;
        }
        runstate->nextPatternIndex = 0;
        goto closeAndReturn;
      }
    }
    runstate->nextPatternIndex = i+1; 
    goto closeAndReturn;
  }
  cleanupAndReturn: 
  // Null it out as freeing null is valid
  // Where double free isn't
  runstate->nextPatternIndex = 0;
  free(*line);
  *line = NULL;
  *length = 0;
  closeAndReturn: 
  //TODO FIX ME AND ADD FREE ON SUCCESS
  if (*line != NULL) {
    strcpy(runstate->patternName, *line);
    if ((runstate->patternName[strlen(runstate->patternName)-2] == '\n') || (runstate->patternName[strlen(runstate->patternName)-2] == '\r' )) {
      runstate->patternName[strlen(runstate->patternName)-2] = 0;
    }
  }

  fclose(fd);
  return;
}

bool isEndOfCommands(RunState* runstate) {
  ESP_EARLY_LOGV(TAG_STATE, "Grabbing Commands");
  if (!cJSON_HasObjectItem(runstate->config, "commands")) {
    ESP_EARLY_LOGW(TAG_STATE, "Commands does not exist");
  }
  cJSON* commands = cJSON_GetObjectItem(runstate->config, "commands");
  return runstate->lastCommandIndex >= cJSON_GetArraySize(commands);
}

bool isEndOfPattern(RunState* runstate) {
    if (isEndOfCommands(runstate)) {
        for (auto sc : runstate->segmentControllers) {
            if (sc.getEndTime() >= runstate->currentTime) {
                return false;
            }
        }
        return true;
    }
    return false;
}

int getTickTime(RunState* runstate) {
  ESP_EARLY_LOGV(TAG_STATE, "Grabbing TickTime");
  if (!cJSON_HasObjectItem(runstate->config, "ticktime")) {
    ESP_EARLY_LOGW(TAG_STATE, "TickTime does not exist");
    return 1000;
  }
  cJSON* time = cJSON_GetObjectItem(runstate->config, "ticktime");
  return time->valueint;
}

void updateControlersFromJson(RunState* runstate) {
  ESP_EARLY_LOGV(TAG_STATE, "Grabbing Commands");
  cJSON* commands = cJSON_GetObjectItem(runstate->config, "commands");
  if (runstate->lastCommandIndex >= cJSON_GetArraySize(commands)) {
    ESP_EARLY_LOGV(TAG_STATE, "End of commands; quick clear");
    return;
  }
  //ESP_EARLY_LOGV(TAG_STATE, "Grabbing Commands A");
  commands = cJSON_GetArrayItem(commands, runstate->lastCommandIndex);
  ESP_EARLY_LOGV(TAG_STATE, "Grabbing Start Time");
  cJSON* commandStartTime;
  commandStartTime = cJSON_GetObjectItem(commands, "times");
  commandStartTime = cJSON_GetObjectItem(commandStartTime, "start");
  while (((uint64_t)commandStartTime->valuedouble) <= runstate->currentTime) {
    ESP_EARLY_LOGV(TAG_STATE, "Applying command %u", runstate->lastCommandIndex);
    // Apply command
    uint64_t startTime = ((uint64_t)commandStartTime->valuedouble);
    // VARIABLE ABUSE :3
    commandStartTime = cJSON_GetObjectItem(commands, "times");
    commandStartTime = cJSON_GetObjectItem(commandStartTime, "end");
    uint64_t endTime = ((uint64_t)commandStartTime->valuedouble);
    uint8_t color[3];
    commandStartTime = cJSON_GetObjectItem(commands, "color");
    commandStartTime = commandStartTime->child;
    color[0] = (uint8_t)commandStartTime->valuedouble;
    commandStartTime = commandStartTime->next;
    color[1] = (uint8_t)commandStartTime->valuedouble;
    commandStartTime = commandStartTime->next;
    color[2] = (uint8_t)commandStartTime->valuedouble;
    commandStartTime = cJSON_GetObjectItem(commands, "mode");
    ESP_LOGD(TAG_STATE, "Mode requested is %s", commandStartTime->valuestring);
    ControlMode cm = ControlMode::Set;
    if (!strcmp(commandStartTime->valuestring, "LINEAR")) {
        ESP_LOGD(TAG_STATE, "Mode requested %s is apparently \"LINEAR\"", commandStartTime->valuestring);
        cm = ControlMode::Linear;
    }
    ESP_LOGD(TAG_STATE, "Applying state (%llu, %llu, (%u, %u, %u), %u)", startTime, endTime, color[0], color[1], color[2], cm);
    cJSON* segment = cJSON_GetObjectItem(commands, "segments");
    cJSON_ArrayForEach(segment, segment) {
      int idx = segment->valueint;
      if (idx >= 0) {
        ESP_LOGV(TAG_STATE, "Applying state to %u", idx);
        runstate->segmentControllers[idx].loadConfig(startTime, endTime, color, cm);
      } else {
        SegmentGroup* segmentGroup = segmentGroups;
        while (idx < -1) {
          idx++;
          if (segmentGroup) {
           segmentGroup = segmentGroup->next;
          } 
        }
        if (!segmentGroup) {
          ESP_LOGE(TAG_STATE, "Invalid group %u", segment->valueint);
          continue;
        } else {
          ESP_LOGV(TAG_STATE, "Applying state to group %u", -segment->valueint);
          for (int i = 0; i < segmentGroup->length; i++) {
            runstate->segmentControllers[segmentGroup->segments[i]].loadConfig(startTime, endTime, color, cm);
          }
        }
      }
    }
    // Cycle to next config
    commands = commands->next;
    runstate->lastCommandIndex++;
    if (commands == NULL) {
      // End of commands so IDK how we signal 
      //runstate->lastCommandIndex = 0;
      break;
    }
    commandStartTime = cJSON_GetObjectItem(commands, "times");
    commandStartTime = cJSON_GetObjectItem(commandStartTime, "start");
  }
}

void tickControllers(RunState* runstate) {
  for (int i = 0; i<SEGMENTS; i++) {
    runstate->segmentControllers[i].tick(runstate->currentTime);
  }
}

/**
 * Assumes runstate->config is freed and nulled pre-emptively
 */
void loadPattern(RunState* runstate) {
  char fileName[255] = "";
  if (runstate->runMode == RunMode::MainSD && isSDDetected()) {
    strcat(fileName, SD_MOUNT);
  } else {
    strcat(fileName, SPIFFS_MOUNT);
  }
  strcat(fileName, "/");
  strcat(fileName, runstate->patternName);
  strcat(fileName, ".json");
  ESP_EARLY_LOGI(TAG_STATE, "Checking File Presence %s...", fileName);
  if (!isFile(fileName)) {
    ESP_EARLY_LOGI(TAG_STATE, "File not found??? %s...", fileName);
    //TODO ERROR
    return;
  }
  ESP_EARLY_LOGI(TAG_STATE, "Opening File %s", fileName);
  FILE* fd = fopen(fileName, "r");
  if ( errno != 0 ) 
   {
     ESP_EARLY_LOGW(TAG_STATE, "Error occurred while opening file: %s", strerror(errno));
     exit(1);
   }
  fseek(fd, 0, SEEK_END);
  size_t fsize = ftell(fd);
  rewind(fd);
  ESP_EARLY_LOGI(TAG_STATE, "File is %u bytes", fsize);
  if (fsize > 16384) {
    ESP_LOGE(TAG_STATE, "FILE TOO BIG");
    fclose(fd);
    return;
  }
  char* fileContent = (char*)malloc(fsize+1);
  if(!fileContent) {
    ESP_LOGE(TAG_STATE, "OUT OF MEMORY");
    fclose(fd);
    return;
  }
  fread(fileContent, 1, fsize, fd);
  fclose(fd);
  fileContent[fsize] = 0;
  runstate->config = cJSON_Parse(fileContent);
  if (runstate->config == 0) {
    ESP_LOGE(TAG_STATE, "Error parsing: %s", cJSON_GetErrorPtr());
  }
  /*
  char* parsed = cJSON_Print(runstate->config);
  if (parsed != NULL) {
    ESP_LOGI(TAG_STATE, "Parsed pointer %p\n%s", runstate->config, parsed);
  } else {
    ESP_LOGI(TAG_STATE, "Parsed pointer %p; COULD NOT PRINT", runstate->config);
  }
  cJSON_free(parsed);
  */
  free(fileContent);
}

bool doLoad(RunState* runstate) {
  // Load new pattern name
  char patternsPath[255] = "";
  if (runstate->runMode == RunMode::MainSD && isSDDetected()) {
    strcat(patternsPath, SD_MOUNT);
  } else {
    strcat(patternsPath, SPIFFS_MOUNT);
  }
  strcat(patternsPath, "/patterns.txt");
  if (!isFile(patternsPath)) {
    return false;
  }
  getNextPattern(patternsPath, runstate);
  if (runstate->nextPatternIndex == 0) {
    ESP_EARLY_LOGI(TAG_STATE, "End of patterns - Looping");
    return false;
    //getNextPattern(patternsPath, runstate);
  }
  if (runstate->config != NULL) {
    cJSON_Delete(runstate->config);
    runstate->config = NULL;
  }
  loadPattern(runstate);
  return true;
}

void doTick(RunState* runstate, void(*outTick)(RunState*)) {
    //ESP_LOGD(TAG_MAIN, "Updating controllers");
    updateControlersFromJson(runstate);
    //ESP_LOGD(TAG_MAIN, "Ticking controllers %u", runstate->currentTime);
    tickControllers(runstate);
    outTick(runstate);
    runstate->currentTime += 1;
    delayMicroseconds(getTickTime(runstate));
}

void getAudioName(RunState* runstate, char name[255]) {
  ESP_EARLY_LOGV(TAG_STATE, "Grabbing AudioName");
  if (!cJSON_HasObjectItem(runstate->config, "audio")) {
    ESP_EARLY_LOGW(TAG_STATE, "audioname does not exist");
    return;
  }
  cJSON* audio = cJSON_GetObjectItem(runstate->config, "audio");
  strcpy(name, audio->valuestring);
  ESP_LOGI(TAG_STATE, "Found audio file %s", name);
}

void SegmentController::loadConfig(uint64_t startTime, uint64_t endTime, uint8_t targetColor[3], ControlMode cm) {
    memcpy(initialColor, currentColor, 3);
    memcpy(this->targetColor, targetColor, 3);
    //this->startTime = startTime;
    // Ignore provided start time to avoid misinterpolation
    this->startTime = this->currentTime;
    this->endTime = endTime;
    this->cm = cm;
    // needed?
    this->tick(this->currentTime);
}

void SegmentController::tick(uint64_t newTime) {
    //ESP_LOGV(TAG_STATE, "PreUpdate state (%llu, %llu, (%u, %u, %u), %u)", 
    //    this->startTime, this->endTime, this->currentColor[0], 
    //    this->currentColor[1], this->currentColor[2], this->cm);
    this->currentTime = newTime;
    if (newTime <= this->startTime) {
        if (this->cm == ControlMode::Linear && memcmp(this->currentColor, this->initialColor, 3) != 0) ESP_LOGD(TAG_STATE, "Pre-start set %u, %u, %u", this->initialColor[0], this->initialColor[1], this->initialColor[2]);
        memcpy(this->currentColor, this->initialColor, 3);
        return;
    }
    if (newTime >= this->endTime) {
        if (this->cm == ControlMode::Linear && memcmp(this->currentColor, this->targetColor, 3) != 0) ESP_LOGD(TAG_STATE, "Post-end set %u, %u, %u", this->targetColor[0], this->targetColor[1], this->targetColor[2]);
        memcpy(this->currentColor, this->targetColor, 3);
        return;
    }
    switch (this->cm)
    {
    case Set:
        memcpy(this->currentColor, this->targetColor, 3);
        break;
    case Linear:
        for (uint8_t i = 0; i<3; i++) {
            if (this->endTime-this->startTime == 0) {
                ESP_LOGE(TAG_STATE, "TIMING OUT OF BOUNDS %llu - %llu = %llu", this->endTime, this->startTime, this->endTime-this->startTime);
                this->currentColor[i] = this->targetColor[i];
            } else {
                this->currentColor[i] = this->initialColor[i] + (int)((float)((((this->targetColor[i]-this->initialColor[i]))*((float)(newTime-this->startTime)))/((float)(this->endTime-this->startTime))));
            }
        }
        break;
    default:
        //?????
        break;
    }
    //ESP_LOGV(TAG_STATE, "PostUpdate state (%llu, %llu, (%u, %u, %u), %u)", this->startTime, this->endTime, this->currentColor[0], this->currentColor[1], this->currentColor[2], cm);
    //ESP_LOGV(TAG_STATE, "PostUpdate info %llu", this->currentColor);
}

void SegmentController::getCurrentColor(uint8_t color[3]) { 
    ESP_LOGV(TAG_STATE, "PostUpdate info %llu", this->currentColor);
    ESP_LOGV(TAG_STATE, "Exporting color (%u, %u, %u)", this->currentColor[0], this->currentColor[1], this->currentColor[2]); 
    memcpy(color, currentColor, 3); 
};

void SegmentController::reset() {
    cm = Set;
    startTime = 0;
    endTime = 0;
    currentTime = 0;
    uint8_t off[] = {0,0,0};
    memcpy(this->initialColor, off, 3);
    memcpy(this->targetColor, off, 3);
    memcpy(this->currentColor, off, 3);
}