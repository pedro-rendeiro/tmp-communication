#include <Arduino.h>

#include "config.h"
#include "sd_gimpsi.h"
#include "SdFat.h"
#include "sdios.h"

// Format SD config
#define SPI_FORMAT_CLOCK SD_SCK_MHZ(4)
#define SD_CONFIG SdSpiConfig(CS_PIN, DEDICATED_SPI, SPI_FORMAT_CLOCK)
#define TAG "SD"

void ListDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  ESP_LOGD(TAG, "Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    ESP_LOGE(TAG, "Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    ESP_LOGE(TAG, "Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      ESP_LOGI(TAG, "  DIR : ");
      ESP_LOGI(TAG, "%s", file.name());
      if (levels) {
        ListDir(fs, file.path(), levels - 1);
      }
    } else {
      ESP_LOGI(TAG, "FILE: %s", file.name());
      ESP_LOGI(TAG, "  SIZE: %lu", file.size());
    }
    file = root.openNextFile();
  }
}

void SD_GimpsiSetup(uint8_t* card_access_failure_counter) {
  ESP_LOGD(TAG, "Entering SD SETUP");
  if (!SD.begin(CS_PIN)) {
    ESP_LOGE(TAG, "Card Mount Failed");
    CardAccessFailureCounter(card_access_failure_counter);
    delay(500);
    // ESP.restart();
  }else{
    (*card_access_failure_counter) = 0; //reset flag
  }
  
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    ESP_LOGE(TAG, "No SD card attached");
    delay(500);
    // ESP.restart();
  }

  if (!SD.exists("/samples")) {
    ESP_LOGI(TAG, "Creating '/sample' dir");
    SD.mkdir("/samples");
  }
  if (!SD.exists("/firmware")) {
    ESP_LOGI(TAG, "Creating '/firmware' dir");
    SD.mkdir("/firmware");
  }
}

// Pointer to generic SD card.
SdCard *m_card = nullptr;

// Format the SD card to FAT32
void SD_GimpsiFormat() {
  SD.end();
  uint32_t cardSectorCount = 0;
  uint8_t sectorBuffer[512];
  // SdCardFactory constructs and initializes the appropriate card.
  SdCardFactory cardFactory;

  ESP_LOGI(TAG, "Try to format SD card to FAT32");
  m_card = cardFactory.newCard(SD_CONFIG);
  if (!m_card || m_card->errorCode()) {
    ESP_LOGE(TAG, "Card init failed.");
    return;
  }
  cardSectorCount = m_card->sectorCount();
  if (!cardSectorCount) {
    ESP_LOGE(TAG, "Get sector count failed.");
    return;
  }

  EraseCard(cardSectorCount, sectorBuffer);
  FormatCard(cardSectorCount, sectorBuffer);

  m_card->end();
  SPI.end();
  return;
}

void EraseCard(uint32_t cardSectorCount, uint8_t sectorBuffer[]) {
  uint32_t const ERASE_SIZE = 262144L;
  uint32_t firstBlock = 0;
  uint32_t lastBlock;
  uint16_t n = 0;

  do {
    lastBlock = firstBlock + ERASE_SIZE - 1;
    if (lastBlock >= cardSectorCount) {
      lastBlock = cardSectorCount - 1;
    }
    if (!m_card->erase(firstBlock, lastBlock)) {
      ESP_LOGE(TAG, "Erase failed");
      return;
    }
    Serial.printf(".");
    if ((n++) % 64 == 63) {
      Serial.printf("\n");
    }
    firstBlock += ERASE_SIZE;
  } while (firstBlock < cardSectorCount);
  Serial.printf("\n");

  if (!m_card->readSector(0, sectorBuffer)) {
    ESP_LOGD(TAG, "readBlock");
  }
  ESP_LOGI(TAG, "Erase done\n");
  return;
}

void FormatCard(uint32_t cardSectorCount, uint8_t sectorBuffer[]) {
  ExFatFormatter exFatFormatter;
  FatFormatter fatFormatter;

  // Format exFAT if larger than 32GB.
  bool rtn = cardSectorCount > 67108864 ? exFatFormatter.format(m_card, sectorBuffer, &Serial) : fatFormatter.format(m_card, sectorBuffer, &Serial);

  if (!rtn) {
    ESP_LOGE(TAG, "Format card failed");
  }
  return;
}

// perform the actual update from a given stream
bool performUpdate(Stream &updateSource, size_t update_size) {
  if (Update.begin(update_size)) {
    size_t written = Update.writeStream(updateSource);
    if (written == update_size) {
      ESP_LOGI(TAG, "Written : %s successfully", String(written));
    } else {
      ESP_LOGE(TAG, "Written only : %s / %s", String(written), String(update_size));
      return false;
    }
    if (Update.end()) {
      ESP_LOGE(TAG, "OTA done!");
      if (Update.isFinished()) {
        ESP_LOGE(TAG, "Update successfully completed. Rebooting.");
        return true;
      } else {
        ESP_LOGE(TAG, "Update not finished? Something went wrong!");
        return false;
      }
    } else {
      ESP_LOGE(TAG, "Error Occurred. Error #: %s", String(Update.getError()));
      return false;
    }

  } else {
    ESP_LOGE(TAG, "Not enough space to begin OTA");
    return false;
  }
}

// check given FS for valid update.bin and perform update if available
bool updateFromFS(fs::FS &fs, String file_name) {
  File update_bin = fs.open(file_name);
  if (update_bin) {
    if (update_bin.isDirectory()) {
      ESP_LOGE(TAG, "Error, %s is not a file\n", file_name.c_str());
      update_bin.close();
      return false;
    }

    size_t update_size = update_bin.size();

    if (update_size > 0) {
      ESP_LOGD(TAG, "Try to start update");
      bool OTA_result = performUpdate(update_bin, update_size);
      update_bin.close();
      return OTA_result;
    } else {
      ESP_LOGE(TAG, "Error, file is empty");
      return false;
    }

    update_bin.close();

  } else {
    ESP_LOGE(TAG, "Could not load %s from sd root", file_name.c_str());
    return false;
  }
}

void DisableSPI(void) {
  SD.end();
  SPI.end();
  pinMode(MISO, INPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  pinMode(CS_PIN, INPUT);
}

#ifdef FINAL
void SPI_SelectESP(void) {
  digitalWrite(SPI_SEL, LOW);
}

void SPI_SelectSTM(void) {
  SD.end();
  SPI.end();
  digitalWrite(SPI_SEL, HIGH);
  digitalWrite(MOSI, HIGH);
  digitalWrite(SCK, HIGH);
  digitalWrite(CS_PIN, HIGH);
}

void CardAccessFailureCounter(uint8_t* card_access_failure_counter){
  (*card_access_failure_counter)++;
  ESP_LOGE(TAG, "Card access failure counter: %d", (*card_access_failure_counter));
}
#endif
