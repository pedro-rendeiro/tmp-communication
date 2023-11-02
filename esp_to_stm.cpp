#include "config.h"
#include "esp32-hal-gpio.h"
#include "esp_to_stm.h"

void SetupSTM(void) {
  //config RESET and BOOT0 pins
  pinMode(RESET_PIN, INPUT);
  pinMode(BOOT0_PIN, OUTPUT);
  digitalWrite(BOOT0_PIN, LOW);

  //config sample pin
  pinMode(ESP_STM_CMD, OUTPUT);
  digitalWrite(ESP_STM_CMD, LOW);

  //config confirmation pin
  pinMode(STM_ESP_CMD, INPUT);

  Serial2.begin(BAUD_to_STM, SERIAL_8N1, RX2, TX2);
  return;
}

// Generates the default string format of the parameters
String ParametersStr(int sample_size, int sample_period) {
  String parameters_str;

  char s[10];
  char p[10];
  char tmpstr[sizeof(s) + sizeof(p) + 10];
  sprintf(tmpstr, "s%dp%d", sample_size, sample_period);
  return tmpstr;
}

String TimestampPath(String baseDir, String timestamp, String id) {
  return (baseDir + "/" + timestamp + "-" + id + ".bin");
}

String CreateMessage(int sample_size, int sample_period, String timestamp, String baseDir, String id) {
  String parameters = ParametersStr(sample_size, sample_period);
  String path = TimestampPath(baseDir, timestamp, id);
  return (parameters + SEPARATOR + path);
}

void SendSampleSignal(String message) {
  // timestamp generator
  Serial2.print(message);
  Serial2.print('#');
  //command to begin sampling
  digitalWrite(ESP_STM_CMD, HIGH);
  delay(100);
  digitalWrite(ESP_STM_CMD, LOW);
  return;
}

volatile bool interrupt_triggered = false;

void InterruptHandler() {
  interrupt_triggered = true;
}

int SampleStatus(double timeout) {
  attachInterrupt((STM_ESP_CMD), InterruptHandler, FALLING);
  // Waiting for confirmation signal
  uint32_t t0 = micros();
  while (!interrupt_triggered) {
    if ((micros() - t0 > timeout)) {
      detachInterrupt((STM_ESP_CMD));
      return SAMPLE_FAIL;
    }
  }  // Wait puts STM_ESP_CMD in high level
  interrupt_triggered = false;
  detachInterrupt((STM_ESP_CMD));
  return SAMPLE_OK;
}
