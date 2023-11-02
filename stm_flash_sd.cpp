#include "stm_flash_sd.h"

static const char *TAG_STM_FLASH = "stm_flash";

esp_err_t WriteTaskSD(fs::File sd_file) {
  ESP_LOGI(TAG_STM_FLASH, "%s", "Write Task");

  char LoadAddress[4] = { 0x08, 0x00, 0x00, 0x00 };
  uint8_t block[256] = { 0 };
  int curr_block = 0, bytes_read = 0;

  sd_file.seek(0, SeekSet);
  SetupFlashSTM();

  while ((bytes_read = sd_file.read(block, 256)) > 0)
  {
    curr_block++;

    esp_err_t ret = FlashPage(LoadAddress, (char *)block);
    if (ret == ESP_FAIL) {
      return ESP_FAIL;
    }

    IncrementLoadAddress(LoadAddress);
    memset(block, 0xff, 256);
  }

  return ESP_OK;
}

esp_err_t ReadTaskSD(fs::File sd_file)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "Read & Verification Task");
  char readAddress[4] = { 0x08, 0x00, 0x00, 0x00 };

  uint8_t block[257] = { 0 };
  int curr_block = 0, bytes_read = 0;

  sd_file.seek(sd_file, SeekSet);

  while ((bytes_read = sd_file.read(block, 256)) > 0)
  {
    curr_block++;

    esp_err_t ret = ReadPage(readAddress, (char *)block);
    if (ret == ESP_FAIL) {
      return ESP_FAIL;
    }

    IncrementLoadAddress(readAddress);
    memset(block, 0xff, 256);
  }

  return ESP_OK;
}

esp_err_t SD_STM(fs::FS &fs, const char *file_path)
{

  StartConn();

  esp_err_t err = ESP_FAIL;

  File sd_file = fs.open(file_path, "rb");



  if (sd_file != NULL)
  {
    do {
      ESP_LOGI(TAG_STM_FLASH, "%s", "Writing STM32 Memory");
      IS_ESP_OK(WriteTaskSD(sd_file));

      ESP_LOGI(TAG_STM_FLASH, "%s", "Reading STM32 Memory");
      IS_ESP_OK(ReadTaskSD(sd_file));

      err = ESP_OK;
      ESP_LOGI(TAG_STM_FLASH, "%s", "STM32 Flashed Successfully!!!");
    } while (0);
  }

  ESP_LOGI(TAG_STM_FLASH, "%s", "Ending Connection");
  EndConn();

  ESP_LOGI(TAG_STM_FLASH, "%s", "Closing file");
  sd_file.close();
  return err;
}
