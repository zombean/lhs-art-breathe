#include <Arduino.h>
#include "filesystems.hpp"

#define TAG_FS "mandala-fs"

bool sdIsMounted = false;

void initialiseSDCard() {
  sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 4,
  };
  sdmmc_card_t* card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT, &host_config, &slot_config, &mount_config, &card);
  if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_FS, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_FS, "Failed to find SD partition");
        } else {
            ESP_LOGE(TAG_FS, "Failed to initialize SD (%s)", esp_err_to_name(ret));
        }
    } else {
      sdIsMounted = true;
    }
}

void freeSDCard() {
  sdIsMounted = false;
  esp_vfs_fat_sdmmc_unmount();
}

bool isSDDetected() {
  //return digitalRead(SD_DET_PIN);
  return false;
}

void initialiseSPIFS() {
  esp_err_t ret = esp_vfs_spiffs_register(&spiff_config);
  if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_FS, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_FS, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG_FS, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
}

bool isFile(const char* path) {
  struct stat stat_results = {};
  int result = stat(path, &stat_results);
  if (result == 0) {
    return ((stat_results.st_mode & S_IFMT) == S_IFREG);
  } 
  return false;
}