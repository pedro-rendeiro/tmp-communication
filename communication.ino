#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include <ESPmDNS.h>

#include "config.h"            // General
#include "esp_to_stm.h"        // Interface-STM
#include "http_gimpsi.h"       // HTTP
#include "sd_gimpsi.h"         // Micro SD card
#include "stm_flash_sd.h"      // Updated-STM
#include "timestamp_gimpsi.h"  // RTC

#define TAG "STATES MACHINE"

//********************************************
// GLOBAL VARIABLES AND FUNCTION PROTOTYPES
//********************************************
// Macro that saves the moment when the code was compiled
const char compile_date[] = "Compile date: " __DATE__ " " __TIME__;
char compile_esp[sizeof(compile_date)];

// NTP and HTTP related
// WiFiUDP ntpUDP;
// NTPClient* time_client;
#ifdef USE_HOSTNAME
IPAddress ntp_ip;
IPAddress http_ip;
bool hostname_ok = false;
#endif

bool ntp_ok = false;

uint8_t gCard_access_failure_counter = 0;

// Controls the states machine (switch case)
enum State state = REST;

// Functions prototypes
static void EnablePWR_STM(void);
static void DisablePWR_STM(void);
static void SetupLEDs(void);
static void SetupPins(void);
static bool SetupWiFi(void);
static String FormatCompileDates(String rawString);
static void UpdateParameters(int sampleSize, int samplePeriod);
static int CheckForUpdates(WiFiClient *client, SampleConfig *parameters);
static void SampleBegin(int sample_size, int sample_period, NTPClient *time_client_addr);
static void SetCompileDate(void);
static void ValidateApp(bool wifi_ok, bool dns_ok);
static void GetParams(SampleConfig *parameters);
static fs::File SendSamplesFile(fs::File root, fs::File file, WiFiClient *client, fs::FS &fs);
static bool FirmwareDownload(String file_name);
static void UpdateSTM(String file_name);
static void UpdateESP(String file_name);
static void DisableSTM(void);
static void EnableSTM(void);
static void GetID(char id[]);
static bool ResolveDNS(void);

//************************
// MAIN CODE
//************************
void setup() {
  Serial.begin(115200);

#ifndef FINAL
  DisableSPI();
#endif

  SetupPins();
  ResetSTM();

  // Get the partition where the current app is running and displays it
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  ESP_LOGI(TAG, "Running partition: %s", running_partition->label);

  strcpy(compile_esp, compile_date);

  nvs_flash_init();
  SetCompileDate();
  bool wifi_ok = SetupWiFi();
#ifdef USE_HOSTNAME
  bool dns_ok = ResolveDNS();
#endif

  // TODO: should we also check NTP and HTTP connection inside ValidateApp?
  ValidateApp(wifi_ok, dns_ok);
}

// Infinite loop
void loop() {
  static SampleConfig factory_config = { SAMPLE_SIZE, SAMPLE_PERIOD };
  static WiFiClient client;
  static uint32_t rest_time;
  static int updates = 0;
  static WiFiUDP ntpUDP;
  // static ClassName *pInstance  = new ClassName();
  static NTPClient *time_client = new NTPClient(ntpUDP, ntp_ip, gmt_offset);
  // static NTPClient time_client(ntpUDP, ntp_ip, gmt_offset);
  
  // Finite-state machine (FSM)
  switch (state) {
    case SAMPLE:
      {
        ESP_LOGI(TAG, "\nEntering SAMPLE state");
        digitalWrite(LEDR, LOW);
        digitalWrite(LEDY, HIGH);
        digitalWrite(LEDG, LOW);

      GetParams(&factory_config);
      SampleBegin(factory_config.sample_size, factory_config.sample_period, time_client);

        double timeout = (DELAY_FACTOR * factory_config.sample_size * factory_config.sample_period);
        int sample_status = SampleStatus(timeout);

        state = CONNECT;
        break;
      }

    case CONNECT:
      {
        ESP_LOGI(TAG, "\nEntering CONNECT state");
        digitalWrite(LEDR, LOW);
        digitalWrite(LEDY, HIGH);
        digitalWrite(LEDG, HIGH);

        if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
        double t0 = millis();
        while (WiFi.status() != WL_CONNECTED) {
          if (millis() - t0 > 5000) {
            ESP_LOGE(TAG, "WiFi connection failed");
            break;
          }
        }

      #ifdef USE_HOSTNAME
      if (!hostname_ok) {
        if (ResolveDNS()) {
          ESP_LOGI(TAG, "Updating time_client IPAddress");
          time_client = new NTPClient(ntpUDP, ntp_ip, gmt_offset);
        }
      }
      #endif
      
      
      // Check if you have lost access to the NTP server
      ntp_ok = Reconnect_ServerNTP(time_client, &ntp_ok);           

        state = ((WiFi.status() == WL_CONNECTED) ? TRANSMIT : REST);
        break;
      }

    case TRANSMIT:
      {
        ESP_LOGI(TAG, "\nEntering TRANSMIT state");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LEDY, LOW);
        digitalWrite(LEDG, LOW);

        static uint32_t start_new = 0, start_old = 0;

#ifdef FINAL
        SPI_SelectESP();
#endif
        SD_GimpsiSetup(&gCard_access_failure_counter);

        File root = SD.open(SAMPLE_DIR);
        File file = root.openNextFile();
        if (!file) {
          ESP_LOGE(TAG, "There is no sample file available to transmit");
        }

        // TODO: Limit total time?
        while (file) {
          file = SendSamplesFile(root, file, &client, SD);
        }

      start_new = millis();
      if (CheckRTC_Update(start_new, start_old)) {
        start_old = start_new;
        ntp_ok = RTC_Update(time_client);
      }

        updates = CheckForUpdates(&client, &factory_config);
        switch (updates) {
          case NOTHING:
            {
              WiFi.disconnect();
              state = REST;
              break;
            }
          case NEW_STM:
          case NEW_STM_ESP:
            {
              state = UPDATE_FIRMWARE_STM;
              break;
            }
          case NEW_ESP:
            {
              state = UPDATE_FIRMWARE_ESP;
              break;
            }
          case RESET:
            {
              ESP_LOGI(TAG, "Restarting ESP32");
              ESP.restart();
              break;
            }
          case FORMAT:
            {
              SD_GimpsiFormat();
              ESP_LOGI(TAG, "Restarting ESP32");
              ESP.restart();
              break;
            }
          default:
            {
              ESP_LOGW(TAG, "'updates' code is unknown");
              WiFi.disconnect();
              state = REST;
            }
        }

        break;
      }

    case REST:
      {
        ESP_LOGI(TAG, "\nEntering REST state");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LEDY, HIGH);
        digitalWrite(LEDG, HIGH);

      #ifdef FINAL
        SPI_SelectSTM();
      #else
        DisableSPI();
      #endif
      
      if (!time_client->isTimeSet()) {
        // Starts NTP client
        ESP_LOGI(TAG, "Running BeginRTC()");
        ntp_ok =  BeginRTC(time_client);
      }

      // ESP_LOGI(TAG, "Card access failure counter = %d", gCard_access_failure_counter);
      // verificar quantas falhas ocorreram ao tentar acessar a memoria externa
      if (gCard_access_failure_counter == MAX_CARD_ACCESS_FAILURE) {
        #ifdef FINAL
          SPI_SelectESP();
        #endif
        SD_GimpsiFormat();
        ESP_LOGI(TAG, "Restarting ESP32");
        ESP.restart();
      }

      
      // TODO: use TIME_BETWEEN_SAMPLING_WINDOWS macro in final version
      int current_seconds = time_client->getSeconds();
      int time_to_rest = (current_seconds >= 30 ? (60 - current_seconds) : (30 - current_seconds));
      delay(time_to_rest*1000);

      state = SAMPLE;
      break;
    }

    case UPDATE_FIRMWARE_STM:
      {
        ESP_LOGI(TAG, "\nEntering UPDATE_FIRMWARE_STM state ");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LEDY, LOW);
        digitalWrite(LEDG, HIGH);

        String file_name = "/firmware/stm.bin";

        if (FirmwareDownload(file_name)) {
          UpdateSTM(file_name);
        }

        state = ((updates == NEW_STM_ESP) ? UPDATE_FIRMWARE_ESP : REST);
        break;
      }

    case UPDATE_FIRMWARE_ESP:
      {
        ESP_LOGI(TAG, "\nEntering UPDATE_FIRMWARE_ESP state ");
        digitalWrite(LEDR, HIGH);
        digitalWrite(LEDY, HIGH);
        digitalWrite(LEDG, LOW);

        String file_name = "/firmware/esp.bin";
        if (updates == NEW_STM_ESP) {
          delay(1000);
          DisableSTM();
#ifdef FINAL
          SPI_SelectESP();
#endif
          SD_GimpsiSetup(&gCard_access_failure_counter);
        }

        if (FirmwareDownload(file_name)) {
          UpdateESP(file_name);
        } else {
          ESP_LOGE(TAG, "Firmware download failed");
        }

        ESP_LOGE(TAG, "ESP update failed");
        EnableSTM();

        state = REST;
        break;
      }
  }
}

//*****************************
// AUXILIARY FUNCTIONS
//*****************************

#ifdef FINAL
static void EnablePWR_STM(void) {
  digitalWrite(nEN_PWR_STM, LOW);
}

static void DisablePWR_STM(void) {
  digitalWrite(nEN_PWR_STM, HIGH);
}
#endif

static void SetupSPI(void) {
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(MOSI, HIGH);
  digitalWrite(SCK, HIGH);
  digitalWrite(CS_PIN, HIGH);
}

static void SetupLEDs(void) {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDY, OUTPUT);
  pinMode(LEDG, OUTPUT);
  digitalWrite(LEDR, LOW);
  digitalWrite(LEDY, LOW);
  digitalWrite(LEDG, HIGH);
}

static void SetupPins(void) {
#ifdef FINAL
  pinMode(SPI_SEL, OUTPUT);
  digitalWrite(SPI_SEL, HIGH);
  pinMode(nEN_PWR_STM, OUTPUT);
  digitalWrite(nEN_PWR_STM, LOW);
#endif
  SetupSPI();
  SetupLEDs();
  SetupSTM();
}

static bool SetupWiFi(void) {
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ESP_LOGD(TAG, "Connecting to WiFi...");
  double t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 10000) {
      ESP_LOGE(TAG, "WiFi connection failed");
      return false;
    }
  }
  ESP_LOGI(TAG, "IP address: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// Formats compile dates to be sent as HTTP parameters
static String FormatCompileDates(String rawString) {
  String new_string = "";
  // Replace all occurrences of " " with "%20"
  for (int i = 14; i < rawString.length(); i++) {
    if (rawString.charAt(i) == ' ') {
      new_string += "%20";
    } else {
      new_string += rawString.charAt(i);
    }
  }
  return new_string;
}

static void UpdateParameters(int sampleSize, int samplePeriod) {
  nvs_handle nvs_handler;
  nvs_open("UMSCF", NVS_READWRITE, &nvs_handler);
  nvs_set_i32(nvs_handler, "sample_size", sampleSize);
  nvs_set_i32(nvs_handler, "sample_period", samplePeriod);
  nvs_commit(nvs_handler);
  nvs_close(nvs_handler);
  return;
}

static int CheckForUpdates(WiFiClient *client, SampleConfig *parameters) {
  int response_code = NOTHING;

  String compile_stm_str;
  File file = SD.open("/CompileSTM.txt", FILE_READ);
  if (file) {
    while (file.available()) {
      char c = file.read();
      if (c != '\n') {
        compile_stm_str += c;
      }
    }
  }
  file.close();
  ESP_LOGI(TAG, "Compile STM: %s", compile_stm_str.c_str());
  ESP_LOGI(TAG, "Compile ESP: %s", compile_esp);

  String formated_compile_stm_str = FormatCompileDates(compile_stm_str);
  String formated_compile_esp_str = FormatCompileDates(String(compile_esp));

  char id[3];
  GetID(id);

  String get_request = HTTP_GimpsiBuildGET_Request(formated_compile_stm_str, formated_compile_esp_str, String(id), http_ip);

  if (HTTP_GimpsiConnect(client, http_ip)) {
    client->print(get_request);
    long timeout_start = millis();
    while (millis() - timeout_start < 3000) {
      if (client->available()) {
        HTTP_GimpsiGetParams(client, &response_code, parameters);
        UpdateParameters((*parameters).sample_size, (*parameters).sample_period);
        break;
      }
    }
    client->stop();
  } else ESP_LOGE(TAG, "Can't connect to the server");

  ESP_LOGD(TAG, "response_code: %d", response_code);
  ESP_LOGD(TAG, "newSampleSize: %d", (*parameters).sample_size);
  ESP_LOGD(TAG, "newSamplePeriod: %d", (*parameters).sample_period);

  return response_code;
}

// Creates and send sample message from ESP to STM
static void SampleBegin(int sample_size, int sample_period, NTPClient *time_client_addr) {
  String timestamp = CreateTimestamp(time_client_addr);
  char id[3];
  GetID(id);
  String message = CreateMessage(sample_size, sample_period, timestamp, SAMPLE_DIR, String(id));
  ESP_LOGI(TAG, "\nSendingMessage...");
  ESP_LOGI(TAG, "\n%s", message.c_str());
  SendSampleSignal(message);
  ESP_LOGI(TAG, "\nsendSample Done");
  return;
}

/*
 * Choose between real or forced compile date
 * based on the validity of the previous running app.
 * If it has been marked as invalid,
 * the current app will force the compile date of the previous app.
 * This way, the system will not try to update back to an invalid version.
 */
static void SetCompileDate(void) {
  nvs_handle nvs_handler;
  nvs_open("UMSCF", NVS_READWRITE, &nvs_handler);
  uint8_t use_real_compile_date = 1;
  esp_err_t result = nvs_get_u8(nvs_handler, "use-real-date", &use_real_compile_date);
  ESP_LOGD(TAG, "result value: %d", result);
  ESP_LOGD(TAG, "use_real_compile_date value: %d", use_real_compile_date);
  if (!result && !use_real_compile_date) {
    size_t compileSize = sizeof(compile_esp);
    ESP_LOGI(TAG, "Real compile date: %s", compile_esp);
    nvs_get_str(nvs_handler, "compile-date", compile_esp, &compileSize);
    ESP_LOGI(TAG, "Forced compile date: %s", compile_esp);
  }
  nvs_close(nvs_handler);
  return;
}

/*
 * On the first boot, checks if there is a critic error on this version.
 * In case there is, mark the app as INVALID and rolls back to the previous version on the next boot.
 * In case everything is okay, mark the app as VALID.
 */
static void ValidateApp(bool wifi_ok, bool dns_ok) {
  esp_err_t rollback = ESP_OK;
  uint8_t app_is_valid = 0;
  nvs_handle nvs_handler;
  nvs_open("UMSCF", NVS_READWRITE, &nvs_handler);
  nvs_get_u8(nvs_handler, "app-is-valid", &app_is_valid);
  ESP_LOGI(TAG, "app_is_valid value: %d", app_is_valid);

  if (!app_is_valid) {
    if (!wifi_ok || !dns_ok) {
      // Store compile date
      nvs_set_str(nvs_handler, "compile-date", compile_esp);

      // Resets use-real-date
      nvs_set_u8(nvs_handler, "use-real-date", 0);

      // Resets app-is-valid
      nvs_set_u8(nvs_handler, "app-is-valid", 0);

      // Save changes and close "UMSCF"
      nvs_commit(nvs_handler);
      nvs_close(nvs_handler);

      // Execute rollback
      rollback = esp_ota_mark_app_invalid_rollback_and_reboot();
    } else {
      // Sets app-is-valid
      nvs_set_u8(nvs_handler, "app-is-valid", 1);
      esp_ota_mark_app_valid_cancel_rollback();
    }
  }
  nvs_commit(nvs_handler);
  nvs_close(nvs_handler);

  if (!rollback) ESP_LOGI(TAG, "App is valid.");
  else ESP_LOGE(TAG, "App in invalid. Rollback failed.");
  return;
}

static void GetParams(SampleConfig *parameters) {
  nvs_handle nvs_handler;
  nvs_open("UMSCF", NVS_READONLY, &nvs_handler);
  int size, period;
  esp_err_t result_size = nvs_get_i32(nvs_handler, "sample_size", &size);
  esp_err_t result_period = nvs_get_i32(nvs_handler, "sample_period", &period);
  nvs_commit(nvs_handler);
  nvs_close(nvs_handler);

  ESP_LOGD(TAG, "result_size: %d\tresult_period: %d", result_size, result_period);
  ESP_LOGD(TAG, "S_old = %d\tP_old = %d", parameters->sample_size, parameters->sample_period);
  parameters->sample_size = (!result_size ? size : parameters->sample_size);
  parameters->sample_period = (!result_period ? period : parameters->sample_period);
  ESP_LOGD(TAG, "S_new = %d\tP_new = %d\n", parameters->sample_size, parameters->sample_period);
}

static fs::File SendSamplesFile(fs::File root, fs::File file, WiFiClient *client, fs::FS &fs) {
  if (!file.isDirectory()) {
    if (HTTP_GimpsiConnect(client, http_ip)) {
      ESP_LOGD(TAG, "Before building full_path");
      String full_path = "/";
      String full_path_sd_card = SAMPLE_DIR "/";
      full_path.concat(file.name());
      full_path_sd_card.concat(file.name());
      ESP_LOGD(TAG, "*** Filename: %s ***", full_path.c_str());
      ESP_LOGD(TAG, "Before buiding HTTP head and tail");
      String head = HTTP_GimpsiBuildPOST_Head(full_path);
      String tail = HTTP_GimpsiBuildPOST_Tail();
      ESP_LOGD(TAG, "Before sendinig POST request");
      client->println(HTTP_GimpsiBuildPOST_Request(file.size() + head.length() + tail.length(), http_ip));
      client->println();
      ESP_LOGD(TAG, "Before sending POST head");
      client->print(head);
      ESP_LOGD(TAG, "Before sending POST data");
      if (!HTTP_GimpsiReadFromSD_AndPOST(&file, client)) {
        ESP_LOGE(TAG, "Failed during the POST of %s", full_path.c_str());
        return FileImplPtr();  // If connection fails once, stops trying to send more samples
      }
      ESP_LOGD(TAG, "Before sending POST tail");
      client->print(tail);
      int http_code = 0;
      long timeout_start = millis();
      ESP_LOGD(TAG, "Before entering while loop");
      while (millis() - timeout_start < 3000) {
        if (client->available()) {
          ESP_LOGD(TAG, "Before waiting for HTTP response");
          http_code = HTTP_GimpsiGetResponse(client);
          break;
        }
      }

      ESP_LOGD(TAG, "Before testing HTTP response code");
      if (http_code == HTTP_CODE_NO_CONTENT) {
        ESP_LOGI(TAG, "Good POST -> HTTP Code: %d", http_code);
        ESP_LOGI(TAG, "Removing %s from the SD card", full_path_sd_card.c_str());
        SD.remove(full_path_sd_card);
      } else {
        ESP_LOGE(TAG, "Bad POST -> HTTP Code: %d", http_code);
      }
      client->stop();
    } else {
      ESP_LOGE(TAG, "Can't connect to gimpsiserver");
      hostname_ok = false;
      return FileImplPtr();  // If connection fails once, stops trying to send more samples
    }
  }

  return root.openNextFile();
}

static bool FirmwareDownload(String file_name) {
  HTTPClient http;
  File file = SD.open(file_name, FILE_WRITE);

  if (!file) {
    return false;
  }

  if (HTTP_GimpsiBegin(&http, file_name, http_ip)) {
    if (!HTTP_GimpsiGET_AndWriteToSD(&file, &http)) {
      http.end();
      ESP_LOGE(TAG, "HTTP_GimpsiGET_AndWriteToSD failed");
      return false;
    }
    ESP_LOGD(TAG, "HTTP_GimpsiGET_AndWriteToSD ok :)");
    file.close();
    ESP_LOGD(TAG, "[HTTP] connection closed or file end.");
    ESP_LOGD(TAG, "Reading file from SD card:");
    http.end();

    return true;
  }

  return false;
}

static void UpdateSTM(String file_name) {
  InitFlashUART();
  SD_STM(SD, file_name.c_str());
  uart_driver_delete(UART_CONTROLLER);
#ifdef FINAL
  SPI_SelectSTM();
#else
  DisableSPI();
#endif
  Serial2.begin(BAUD_to_STM, SERIAL_8N1, RX2, TX2);
  return;
}

static void UpdateESP(String file_name) {
  if (updateFromFS(SD, file_name)) {
    nvs_handle nvs_handler;
    nvs_open("UMSCF", NVS_READWRITE, &nvs_handler);
    nvs_set_u8(nvs_handler, "app-is-valid", 0);   // Reset app-is-valid
    nvs_set_u8(nvs_handler, "use-real-date", 1);  // Sets use-real-date
    nvs_commit(nvs_handler);
    nvs_close(nvs_handler);
    ESP.restart();
  }
  ESP_LOGE(TAG, "ESP32 firmware update failed... Going to REST state");
  return;
}

static void DisableSTM(void) {
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  return;
}

static void EnableSTM(void) {
  digitalWrite(RESET_PIN, HIGH);
  pinMode(RESET_PIN, INPUT);
  return;
}

static void GetID(char id[]) {
  size_t id_size = sizeof(id);
  nvs_handle nvs_handler;
  nvs_open("UMSCF", NVS_READONLY, &nvs_handler);
  nvs_get_str(nvs_handler, "id", id, &id_size);
  nvs_commit(nvs_handler);
  nvs_close(nvs_handler);
  return;
}

bool ResolveDNS() {
  char id[3];
  GetID(id);
  char client_name[10] = "UMSCF_";
  strcat(client_name, id);

  if (!MDNS.begin(client_name)) {
    ESP_LOGE(TAG, "Error setting up MDNS responder!");
  }

  ESP_LOGI(TAG, "mDNS responder started");
  ntp_ip = MDNS.queryHost(ntp_hostname);
  http_ip = MDNS.queryHost(http_hostname);

  if (ntp_ip == IPAddress() || http_ip == IPAddress()) {
    ESP_LOGE(TAG, "Host resolution failed");
    hostname_ok = false;
    return false;  // DNS resolution failed
  }
  ESP_LOGI(TAG, "Host resolution was successful [IP: %s]", ntp_ip.toString().c_str());
  hostname_ok = true;
  return true;
}
