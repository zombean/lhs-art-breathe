#include "idf_audio.hpp"
#include "filesystems.hpp"

#define TAG_AUDIO "mandala-audio"

static const i2s_config_t i2s_config = 
      {
          .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
          .sample_rate = 44100,                                 // Note, this will be changed later
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
          .intr_alloc_flags = ESP_INTR_FLAG_INTRDISABLED,             // high interrupt priority
          .dma_buf_count = 4,                                   // 8 buffers
          .dma_buf_len = 1024,                                    // 64 bytes per buffer, so 8K of buffer space
          .use_apll = 0,
          .tx_desc_auto_clear= true, 
          .fixed_mclk = -1    
      };

static const i2s_pin_config_t pin_config = 
      {
          .bck_io_num = I2S_BCLK_PIN,                           // The bit clock connectiom, goes to pin 27 of ESP32
          .ws_io_num = I2S_LRC_PIN,                             // Word select, also known as word select or left right clock
          .data_out_num = I2S_DOUT_PIN,                         // Data out from the ESP32, connect to DIN on 38357A
          .data_in_num = I2S_PIN_NO_CHANGE                      // we are not interested in I2S data into the ESP32
      };

void audio_i2s_init() {
    i2s_driver_install(I2S_OUTPUT_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_OUTPUT_NUM, &pin_config);
}

bool validateHeader(WavHeader* header);

AudioState* audio_i2s_load(char* fileName) {
  if (!isFile(fileName)) {
    ESP_EARLY_LOGI(TAG_AUDIO, "File not found??? %s...", fileName);
    return NULL;
  }
  ESP_EARLY_LOGI(TAG_AUDIO, "Opening File %s", fileName);
  FILE* fd = fopen(fileName, "r");
  if ( errno != 0 ) 
    {
        ESP_EARLY_LOGW(TAG_AUDIO, "Error occurred while opening file: %s", strerror(errno));
        return NULL;
    }
  uint8_t* buff = (uint8_t*) malloc(AUDIO_I2S_BUFF_SIZE);
  AudioState* audiostate = new AudioState();
  audiostate->fd = fd;
  audiostate->byteBuffer = buff;
  audiostate->bufferOffset = 0;
  fread((void*)(&audiostate->header), 44, 1, fd);
  if (validateHeader(&audiostate->header)) {
    i2s_set_sample_rates(I2S_OUTPUT_NUM, audiostate->header.SampleRate);
    return audiostate;
  }
  fseek(fd, 44, SEEK_SET);
  audio_i2s_free(audiostate);
  return NULL;
}

bool validateHeader(WavHeader* header)
{
  
  if(memcmp(header->RIFFSectionID,"RIFF",4)!=0) 
  {    
    ESP_LOGE(TAG_AUDIO, "Invalid data - Not RIFF format");
    return false;        
  }
  if(memcmp(header->RiffFormat,"WAVE",4)!=0)
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - Not Wave file");
    return false;           
  }
  if(memcmp(header->FormatSectionID,"fmt",3)!=0) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - No format section found");
    return false;       
  }
  if(memcmp(header->DataSectionID,"data",4)!=0) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - data section not found");
    return false;      
  }
  if(header->FormatID!=1) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - format Id must be 1");
    return false;                          
  }
  if(header->FormatSize!=16) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - format section size must be 16.");
    return false;                          
  }
  if((header->NumChannels!=1)&(header->NumChannels!=2))
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - only mono or stereo permitted.");
    return false;   
  }
  if(header->SampleRate>48000) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - Sample rate cannot be greater than 48000");
    return false;                       
  }
  if((header->BitsPerSample!=8)& (header->BitsPerSample!=16)) 
  {
    ESP_LOGE(TAG_AUDIO, "Invalid data - Only 8 or 16 bits per sample permitted.");
    return false;                        
  }
  return true;
}

void audio_i2s_free(AudioState* audiostate) {
    free(audiostate->byteBuffer);
    fclose(audiostate->fd);
    free(audiostate);
}

void audio_i2s_play(AudioState* audiostate) {
    bool writing = true;
    while (writing) {
        if (audiostate->bufferOffset >= AUDIO_I2S_BUFF_SIZE) {
            size_t nread = fread(audiostate->byteBuffer, AUDIO_I2S_BUFF_SIZE, 1, audiostate->fd);
            if (nread < AUDIO_I2S_BUFF_SIZE) {
                fseek(audiostate->fd, 44, SEEK_SET);
                fread(audiostate->byteBuffer, AUDIO_I2S_BUFF_SIZE, 1, audiostate->fd);
            }
        }
        uint bytesWritten = 0;
        i2s_write(I2S_OUTPUT_NUM, audiostate->byteBuffer+audiostate->bufferOffset, AUDIO_I2S_BUFF_SIZE-audiostate->bufferOffset, &bytesWritten, 1);
        audiostate->bufferOffset += bytesWritten;
        if (bytesWritten < AUDIO_I2S_BUFF_SIZE) {
            writing = false;
        }
    }
}