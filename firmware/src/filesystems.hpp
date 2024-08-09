#ifndef JMT_FILESYSTEMS
#define JMT_FILESYSTEMS

#include <esp_vfs_fat.h>
#include <esp_spiffs.h>
#include <esp_vfs.h>

#define SD_DET_PIN 13

#define SD_MOUNT "/sd"
#define SPIFFS_MOUNT "/spiff"
static const esp_vfs_spiffs_conf_t spiff_config = {
    .base_path = SPIFFS_MOUNT,
    .partition_label = NULL,
    .max_files = 4,
    .format_if_mount_failed = false
  };

extern bool sdIsMounted;

bool isFile(const char* path);
void initialiseSDCard();
void freeSDCard();
void initialiseSPIFS();
bool isSDDetected();

#endif // JMT_FILESYSTEMS