#include <esp_vfs.h>
#include <driver/i2s.h>
#include <errno.h>

#define I2S_BCLK_PIN 26
#define I2S_LRC_PIN 25
#define I2S_DOUT_PIN 33

#ifndef I2S_BCLK_PIN
#define I2S_BCLK_PIN 8
#endif

#ifndef I2S_LRC_PIN
#define I2S_LRC_PIN 9
#endif

#ifndef I2S_DOUT_PIN
#define I2S_DOUT_PIN 10
#endif

#ifndef I2S_OUTPUT_NUM
#define I2S_OUTPUT_NUM I2S_NUM_0
#endif

#define AUDIO_I2S_BUFF_SIZE 2048

struct WavHeader
    {
        //   RIFF Section    
        char RIFFSectionID[4];      // Letters "RIFF"
        uint32_t Size;              // Size of entire file less 8
        char RiffFormat[4];         // Letters "WAVE"
        
        //   Format Section    
        char FormatSectionID[4];    // letters "fmt"
        uint32_t FormatSize;        // Size of format section less 8
        uint16_t FormatID;          // 1=uncompressed PCM
        uint16_t NumChannels;       // 1=mono,2=stereo
        uint32_t SampleRate;        // 44100, 16000, 8000 etc.
        uint32_t ByteRate;          // =SampleRate * Channels * (BitsPerSample/8)
        uint16_t BlockAlign;        // =Channels * (BitsPerSample/8)
        uint16_t BitsPerSample;     // 8,16,24 or 32
    
        // Data Section
        char DataSectionID[4];      // The letters "data"
        uint32_t DataSize;          // Size of the data that follows
};

struct AudioState {
    FILE* fd;
    uint8_t* byteBuffer;
    uint bufferOffset;
    WavHeader header;
};

void audio_i2s_init();
AudioState* audio_i2s_load(char* fileName);
void audio_i2s_free(AudioState* audiostate);
void audio_i2s_play(AudioState* audiostate);