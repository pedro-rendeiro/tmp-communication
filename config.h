#ifndef _CONFIG_H
#define _CONFIG_H

// Network (enter your credentials and server IP)
// TODO: store credentials in NVS and encrypt it
#define USE_HOSTNAME  // Comment this line to connect to HTTP and NTP servers via hardcoded IP addresses
#define WIFI_SSID "Intelbras_GImpSI"
#define WIFI_PASS "lassesemfio"

static const int server_port = 8081;

#ifdef USE_HOSTNAME
static const char* ntp_hostname = "gimpsirasp";
static const char* http_hostname = "gimpsirasp";
#else  // Replace by the correct IP addresses
IPAddress ntp_ip(200,239,93,149);
IPAddress http_ip(200,239,93,149);
#endif

// Macro to switch between MVP (undefined) and final (defined) version of the boards
// Comment the following line if you wish to flash the code on the MVP board
#define FINAL

// Finite States Machine
enum State {
  SAMPLE,               // Sends the "SampleBegin" message to the signals conditioning module
  CONNECT,              // Tries to connect to the local server
  TRANSMIT,             // Reads data from the micro SD card and sends it to the local server
  REST,                 // Waits for the beggining of the next sampling window
  UPDATE_FIRMWARE_ESP,  // Remotely updates ESP's firmware
  UPDATE_FIRMWARE_STM,  // Remotely updates STM's firmware
};

// GPIOs
#ifdef FINAL
  #define LEDR 27
  #define LEDY 12
  #define LEDG  2
  #define RESET_PIN (GPIO_NUM_14)
  #define BOOT0_PIN (GPIO_NUM_4)
  #define SPI_SEL 25
  #define nEN_PWR_STM 15 
  #define AC_ON 21
#else
  #define LEDR 27
  #define LEDY 14
  #define LEDG 12
  #define RESET_PIN (GPIO_NUM_2)
  #define BOOT0_PIN (GPIO_NUM_33)
#endif


// Firmware update codes
#define NOTHING     230
#define NEW_STM     231
#define NEW_ESP     232
#define NEW_STM_ESP 233

// Other types of codes
#define RESET       240
#define FORMAT      241

// Delays
#define TIME_BETWEEN_SAMPLING_WINDOWS      30000  // 30000 ms = 30s
#define UPDATE_TIME_FROM_NTP_SERVER_MS    120000  // 60000 ms = 1min; 120000ms = 5min; 18000000 ms = 5h; 43200000 ms = 12h

#endif
