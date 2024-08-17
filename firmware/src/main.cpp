#include <Arduino.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>
#include <cJSON.h>

//#include <SPI.h>
//#define FASTLED_ALL_PINS_HARDWARE_SPI
//#define FASTLED_ESP32_SPI_BUS VSPI
//#include <FastLED.h>

/// P9813

#include "statecontrol.hpp"
#include "filesystems.hpp"
#include "idf_audio.hpp"

// FASTLED STUFF
#define NUM_LEDS SEGMENTS
//CRGB leds[NUM_LEDS];
#define DATA_PIN 19
#define CLOCK_PIN 18

#include <NeoPixelBus.h>
#define USE_DEFAULT_SPI_PORT 1

NeoPixelBus<P9813BgrFeature, P9813SpiMethod> leds(NUM_LEDS);
// /FASTLED STUFF

#define TAG_MAIN "mandala"

RunState runstate = {
  .patternName = {},
  .nextPatternIndex = 0,
  .config = NULL,
  .lastCommandIndex = 0,
  .currentTime = 0,
  .runMode = RunMode::NoSDInternal
};

AudioState* audiostate;

static cJSON_Hooks hooks = {
  .malloc_fn = malloc,
  .free_fn = free
};

void setup() {
  ESP_EARLY_LOGI(TAG_MAIN, "RUNSTATE TIME %u", runstate.currentTime);
  ESP_EARLY_LOGI(TAG_MAIN, "Initialising SPIFS");
  initialiseSPIFS();
  pinMode(SD_DET_PIN, INPUT);
  if (isSDDetected()) {
    ESP_EARLY_LOGI(TAG_MAIN, "Initialising SDMMC");
    initialiseSDCard();
  } else {
    ESP_EARLY_LOGI(TAG_MAIN, "No SD card inserted");
  }
  // ESP_EARLY_LOGI(TAG_MAIN, "Initialising FASTLED");
  //FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
  ESP_EARLY_LOGI(TAG_MAIN, "Initialising FASTLED");
  leds.Begin(CLOCK_PIN, 0, DATA_PIN, 0);
  ESP_EARLY_LOGI(TAG_MAIN, "Initialising cJSON");
  cJSON_InitHooks(&hooks);
  //ESP_EARLY_LOGI(TAG_MAIN, "Initialising I2S");
  //audio_i2s_init();
  ESP_EARLY_LOGI(TAG_MAIN, "Initialising SegmentGroups");
  initSegmentGroups();
  ESP_EARLY_LOGI(TAG_MAIN, "Initialising complete");
  //ESP_EARLY_LOGI(TAG_MAIN, "RUNSTATE TIME %u", runstate.currentTime);
  //printf("%s", runstate.config->string);
}
  /*
  if (file == NULL) {
    char* filePath = "";
    strcat(filePath, spiff_config.base_path);
    strcat(filePath, "/patterns.txt");
    struct stat* backupPatterns = {};
    int err = stat(filePath, backupPatterns);
    if (err == 0) {
      FILE* fd = fopen(filePath, "r");
      char* line = NULL;
      size_t size = 0;

      while ((err = __getline(&line, &size, fd)) != -1) {

      }
      filePath = "";
      strcat(filePath, spiff_config.base_path);
      strcat(filePath, line);
      strcat(filePath, ".json");
      FILE* ffd = fopen(filePath, "r");
      rewind(ffd);
      fseek(ffd, 0L, SEEK_END);
      long sz = ftell(ffd);
      file = cJSON_ParseWithLength("", 0);
      // Load file
      free(line);

    }
  }
  */

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;;

void outputFastLed(RunState* runstate) {
  uint8_t s0c[3] = {};
    for (int i = 0; i < SEGMENTS; i++) {
      runstate->segmentControllers[i].getCurrentColor(s0c);
      //ESP_LOGI(TAG_MAIN, "Setting led %u to color (&%u, %u, %u)", i, s0c[0], s0c[1], s0c[2]);
      //leds[i].setRGB(s0c[0], s0c[1], s0c[2]);
      leds.SetPixelColor(i, RgbColor(s0c[0], s0c[1], s0c[2]));
    }
    taskENTER_CRITICAL(&mux);
    // FastLED.show();
    leds.Show();
    taskEXIT_CRITICAL(&mux);
}

void loop() {
  /*
  ESP_LOGV(TAG_MAIN, "RUNSTATE TIME NON EARLY %u", runstate.currentTime);
  ESP_LOGD(TAG_MAIN, "Updating controllers");
  updateControlersFromJson(&runstate);
  ESP_LOGD(TAG_MAIN, "Ticking controllers");
  tickControllers(&runstate);
  uint8_t s0c[3] = {};
  for (int i = 0; i < SEGMENTS; i++) {
    runstate.segmentControllers[i].getCurrentColor(s0c);
    leds[i].setRGB(s0c[0], s0c[1], s0c[2]);
  }
  FastLED.show();
  //ESP_LOGI(TAG_MAIN, "Seg 0 Color is (%u, %u, %u) at time %llu", s0c[0], s0c[1], s0c[2], runstate.currentTime);
  runstate.currentTime += 1;
  ESP_LOGD(TAG_MAIN, "RUNSTATE TIME POST ADD %u", runstate.currentTime);
  delayMicroseconds(getTickTime(&runstate));
  if (isEndOfPattern(&runstate)) {
    runstate.lastCommandIndex = 0;
    runstate.currentTime = 0;
  }
  */
 if (isEndOfPattern(&runstate)) {
    runstate.lastCommandIndex = 0;
    runstate.currentTime = 0;
    //if (audiostate) {
      //audio_i2s_free(audiostate);
      //audiostate = NULL;
    //}
    while (!doLoad(&runstate)) {
      ESP_LOGI(TAG_MAIN, "Changing pattern source...");
      if (runstate.runMode != RunMode::MainSD && isSDDetected()) {
        ESP_LOGI(TAG_MAIN, "Changing to SD...");
        if (!sdIsMounted) {
          initialiseSDCard();
        }
        runstate.runMode = RunMode::MainSD;

      } else {
        if (!isSDDetected()) {
          ESP_LOGI(TAG_MAIN, "Changing to Internal...");
          runstate.runMode = RunMode::NoSDInternal;
          if (sdIsMounted) {
            freeSDCard();
          }
        }
      }
    }
    char audioName[255] = "";
    getAudioName(&runstate, audioName);
    if (strcmp(audioName, "") != 0) {
      ESP_LOGW(TAG_MAIN, "Pattern %s has audio", runstate.patternName);
      char audioPath[255] = "";
      if (runstate.runMode == RunMode::MainSD && isSDDetected()) {
        strcat(audioPath, SD_MOUNT);
      } else {
        strcat(audioPath, SPIFFS_MOUNT);
      }
      strcat(audioPath, "/");
      strcat(audioPath, audioName);
      strcat(audioPath, ".wav");
      //audiostate = audio_i2s_load(audioPath);
    } else {
      ESP_LOGW(TAG_MAIN, "Pattern %s lacks audio", runstate.patternName);
      //audiostate = NULL;
    }
    ESP_LOGD(TAG_MAIN, "Loaded pattern %s", runstate.patternName);
  }
  //if (audiostate != NULL) {
    //audio_i2s_play(audiostate);
  //}
  doTick(&runstate, outputFastLed);
}