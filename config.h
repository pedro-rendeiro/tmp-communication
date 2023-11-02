#ifndef _CONFIG_H
#define _CONFIG_H


// Digital analog converter pin
#define PIN_ADC PA1 

// USART
#define STM_UART_RX PA10 
#define STM_UART_TX PA9 

/* SPI - SD card - Pins: */
#define SPI_MOSI PA7
#define SPI_MISO PA6
#define SPI_SCLK PA5
#define SPI_CS PA4
/* SPI2 - POWER - Pins: */
#define STM_PB15_MOSI2 PB15
#define STM_PB13_SCLOK2 PB13
#define STM_PB12_NSS2 PB12
#define STM_PB1 PB1
#define STM_PB0 PB0

// pin LED 
#define LED_2 PC13

// Macro to switch between MVP (undefined) and final (defined) version of the boards
// Comment the following line if you wish to flash the code on the MVP board
#define FINAL

#ifdef FINAL 
  //GIMPSI:
  #define LED_1 PB5 // pin LED
  #define ESP_STM_CMD1 PA11 // Pin that receives the start sampling command
  #define STM_ESP_CMD1 PA8 // Pin that sends the command that indicates the end of the sampling
#else 
  // MVP:
  #define LED_1 PA3 // pin LED 
  #define ESP_STM_CMD1 PB4 // Pin that receives the start sampling command
  #define STM_ESP_CMD1 PB3 // Pin that sends the command that indicates the end of the sampling
#endif

#define ENABLEPRINTS 0
#if ENABLEPRINTS
  #define MYPRINT Serial.printf
#else
  #define MYPRINT //
#endif

struct SampleConfig{
	int sample_size;    //#samples
  uint16_t sample_period;  //in us
};

#define SAMPLE_DIRECTORY "/samples"
#define COMPILE_DATE_DIRECTORY "/config"
#define SEPARATOR '-'

#define TIMEOUT_MS 30000 // 30000ms=30s 

// Sampling return messages
#define SAMPLE_OK        1
#define SAMPLE_FAIL      0


// Functions
bool initSPI();
void disableSPI();
void configPinCMD();
void configPinAdjustFreqResp();
void resetSamplingFlags();

// Functions that use Strings
bool hasNewParameters_str(String received_str, String current_parameters);
String parameters_str(int sample_size, int sample_period);
bool updateParameters_str(String received_parameters);

String parseTimestamp_str(String received_str);
int storeSamples_str(String dir, volatile uint16_t* w,  int NsamplesToStore);

int storeParameters_str(String dir, SampleConfig *parameters);

// Functions that use char array
// bool hasNewParameters(char* received_str, char* current_parameters);
// void parametersChar(int sample_size, int sample_period, char* parametersStr);
// bool updateParameters(char* receivedParameters);
// void parseTimestamp(char* received_str, char* timestamp);
// int storeSamples(char* dir, volatile uint16_t* w, int NsamplesToStore);
#endif