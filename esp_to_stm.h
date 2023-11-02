#ifndef _ESP_TO_STM_H
#define _ESP_TO_STM_H

#include <Arduino.h>
#include "config.h"

// USART
#define RX2 16
#define TX2 17

// #define SAMPLE_ONGOING   0
#define SAMPLE_OK   1
#define SAMPLE_FAIL 2

#define SAMPLE_SIZE   256
#define SAMPLE_PERIOD 100
#define DELAY_FACTOR   2

#define BAUD_to_STM 9600

#ifdef FINAL
  #define ESP_STM_CMD GPIO_NUM_26
  #define STM_ESP_CMD GPIO_NUM_13
#else
  #define ESP_STM_CMD GPIO_NUM_15
  #define STM_ESP_CMD GPIO_NUM_13
#endif


#define SEPARATOR "-"


struct SampleConfig {
  int sample_size;    //#samples
  int sample_period;  //sec/sample
};                    // name of the new struct type of data


void SetupSTM(void);
String ParametersStr(int sample_size, int sample_period);
String TimestampPath(String baseDir, String timestamp, String id);
String CreateMessage(int sample_size, int sample_period, String timestamp, String baseDir, String id);
void SendSampleSignal(String message);
int SampleStatus(double timeout);


#endif
