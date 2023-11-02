#ifndef _SD_FUNCTIONS_H
#define _SD_FUNCTIONS_H

#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Update.h"

#define CONFIG_DIR   "/config"
#define SAMPLE_DIR   "/samples"
#define FIRMWARE_DIR "/firmware"

#define MAX_CARD_ACCESS_FAILURE   3

// SPI bus
#define MOSI  23
#define MISO  19
#define SCK   18
#define CS_PIN 5


void ListDir(fs::FS &fs, const char *dirname, uint8_t levels);
void SD_GimpsiSetup(uint8_t* card_access_failure_counter);
void SD_GimpsiFormat();
void CardAccessFailureCounter(uint8_t* card_access_failure_counter);

bool performUpdate(Stream &updateSource, size_t updateSize);
bool updateFromFS(fs::FS &fs, String file_name);

void DisableSPI(void);

void SPI_SelectESP(void);
void SPI_SelectSTM(void);

void EraseCard(uint32_t cardSectorCount, uint8_t  sectorBuffer[]);
void FormatCard(uint32_t cardSectorCount, uint8_t  sectorBuffer[]);

#endif
