#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STM_PRO_MODE_H
#define _STM_PRO_MODE_H

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_system.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

#include "nvs_flash.h"

//Macro for error checking
#define IS_ESP_OK(x) \
  if ((x) != ESP_OK) break;

#define TXD_PIN (GPIO_NUM_17)  // GPIO_NUM_17
#define RXD_PIN (GPIO_NUM_16)  // GPIO_NUM_16
#define UART_BAUD_RATE 115200
#define UART_BUF_SIZE 1024
#define UART_CONTROLLER UART_NUM_1

#define HIGH 1
#define LOW 0

#define ACK 0x79
#define SERIAL_TIMEOUT 5000

#define FILE_PATH_MAX 128
#define BASE_PATH "/"

//Initialize UART functionalities
void InitFlashUART(void);

//Reset the STM32Fxx
void ResetSTM(void);

//Increment the memory address for the next write operation
void IncrementLoadAddress(char *loadAddr);

//Start the connection with STM32Fxx
void StartConn(void);

//End the connection with STM32Fxx
void EndConn(void);

//Get in sync with STM32Fxx
int CmdSync(void);

//Get the version and the allowed commands supported by the current version of the bootloader
int CmdGet(void);

//Get the bootloader version and the Read Protection status of the Flash memory
int CmdVersion(void);

//Get the chip ID
int CmdId(void);

//Erase from one to all the Flash memory pages
int CmdErase(void);

//Erases from one to all the Flash memory pages using 2-byte addressing mode
int CmdExtErase(void);

//Setup STM32Fxx for the 'flashing' process
void SetupFlashSTM(void);

//Write data to flash memory address
int CmdWrite(void);

//Read data from flash memory address
int CmdRead(void);

//UART send data to STM32Fxx & wait for response
int SendBytes(const char *bytes, int count, int resp);

//UART send data byte-by-byte to STM32Fxx
int SendData(const char *logName, const char *datas, const int count);

//Wait for response from STM32Fxx
int WaitForSerialData(int dataCount, int timeout);

//Send the STM32Fxx the memory address, to be written
int LoadAddress(const char adrMS, const char adrMI, const char adrLI, const char adrLS);

//UART write the flash memory address of the STM32Fxx with blocks of data
esp_err_t FlashPage(const char *address, const char *datas);

//UART read the flash memory address of the STM32Fxx and verify with the given block of data
esp_err_t ReadPage(const char *address, const char *datas);

#endif

#ifdef __cplusplus
}  // extern "C"
#endif
