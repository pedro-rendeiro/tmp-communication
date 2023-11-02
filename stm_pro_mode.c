#include "esp_log.h"
#include "esp32-hal-gpio.h"

#include "config.h"
#include "stm_pro_mode.h"

static const char *TAG_STM_PRO = "stm_pro_mode";

//Functions for custom adjustments
void InitFlashUART(void) {
  const uart_config_t uart_config = {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  uart_driver_install(UART_CONTROLLER, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

  uart_param_config(UART_CONTROLLER, &uart_config);
  uart_set_pin(UART_CONTROLLER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  ESP_LOGI(TAG_STM_PRO, "%s", "Initialized Flash UART");
}

void ResetSTM(void) {
  pinMode(RESET_PIN, OUTPUT);
  gpio_set_level(RESET_PIN, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(RESET_PIN, HIGH);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  pinMode(RESET_PIN, INPUT);
}

void StartConn(void) {
  gpio_set_level(BOOT0_PIN, HIGH);
  ESP_LOGI(TAG_STM_PRO, "%s", "Starting Connection");
}

void EndConn(void) {
  gpio_set_level(RESET_PIN, LOW);
  gpio_set_level(BOOT0_PIN, LOW);

  ResetSTM();

  ESP_LOGI(TAG_STM_PRO, "%s", "Ending Connection");
}

void SetupFlashSTM(void) {
  ResetSTM();
  CmdSync();
  CmdGet();
  CmdVersion();
  CmdId();
  CmdErase();
  CmdExtErase();
}

int CmdSync(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "SYNC");

  char bytes[] = { 0x7F };
  int resp = 1;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int CmdGet(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "GET");

  char bytes[] = { 0x00, 0xFF };
  int resp = 15;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int CmdVersion(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "GET VERSION & READ PROTECTION STATUS");

  char bytes[] = { 0x01, 0xFE };
  int resp = 5;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int CmdId(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "CHECK ID");
  char bytes[] = { 0x02, 0xFD };
  int resp = 5;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int CmdErase(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "ERASE MEMORY");
  char bytes[] = { 0x43, 0xBC };
  int resp = 1;
  int a = SendBytes(bytes, sizeof(bytes), resp);

  if (a == 1) {
    char params[] = { 0xFF, 0x00 };
    resp = 1;

    return SendBytes(params, sizeof(params), resp);
  }
  return 0;
}

int CmdExtErase(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "EXTENDED ERASE MEMORY");
  char bytes[] = { 0x44, 0xBB };
  int resp = 1;
  int a = SendBytes(bytes, sizeof(bytes), resp);

  if (a == 1) {
    char params[] = { 0xFF, 0xFF, 0x00 };
    resp = 1;

    return SendBytes(params, sizeof(params), resp);
  }
  return 0;
}

int CmdWrite(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "WRITE MEMORY");
  char bytes[2] = { 0x31, 0xCE };
  int resp = 1;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int CmdRead(void) {
  ESP_LOGI(TAG_STM_PRO, "%s", "READ MEMORY");
  char bytes[2] = { 0x11, 0xEE };
  int resp = 1;
  return SendBytes(bytes, sizeof(bytes), resp);
}

int LoadAddress(const char adrMS, const char adrMI, const char adrLI, const char adrLS) {
  char xor01 = adrMS ^ adrMI ^ adrLI ^ adrLS;
  char params[] = { adrMS, adrMI, adrLI, adrLS, xor01 };
  int resp = 1;

  return SendBytes(params, sizeof(params), resp);
}

int SendBytes(const char *bytes, int count, int resp) {
  SendData(TAG_STM_PRO, bytes, count);
  int len = WaitForSerialData(resp, SERIAL_TIMEOUT);

  if (len > 0) {
    uint8_t datas[len];
    const int rxBytes = uart_read_bytes(UART_CONTROLLER, datas, len, 1000 / portTICK_RATE_MS);

    if (rxBytes > 0 && datas[0] == ACK) {
      ESP_LOGI(TAG_STM_PRO, "%s", "Sync Success");
      return 1;
    } else {
      ESP_LOGE(TAG_STM_PRO, "%s", "Sync Failure");
      return 0;
    }
  } else {
    ESP_LOGE(TAG_STM_PRO, "%s", "Serial Timeout");
    return 0;
  }

  return 0;
}

int SendData(const char *logName, const char *datas, const int count) {
  const int txBytes = uart_write_bytes(UART_CONTROLLER, datas, count);
  ESP_LOGV(logName, "Wrote %d bytes", count);
  return txBytes;
}

int WaitForSerialData(int dataCount, int timeout) {
  int timer = 0;
  int len = 0;
  while (timer < timeout) {
    uart_get_buffered_data_len(UART_CONTROLLER, (size_t *)&len);
    if (len >= dataCount) {
      return len;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
    timer++;
  }
  return 0;
}

void IncrementLoadAddress(char *loadAddr) {
  loadAddr[2] += 0x1;

  if (loadAddr[2] == 0) {
    loadAddr[1] += 0x1;

    if (loadAddr[1] == 0) {
      loadAddr[0] += 0x1;
    }
  }
}

esp_err_t FlashPage(const char *address, const char *datas) {
  ESP_LOGI(TAG_STM_PRO, "%s", "Flashing Page");

  CmdWrite();

  LoadAddress(address[0], address[1], address[2], address[3]);

  char xor01 = 0xFF;
  char sz = 0xFF;

  SendData(TAG_STM_PRO, &sz, 1);

  for (int i = 0; i < 256; i++) {
    SendData(TAG_STM_PRO, &datas[i], 1);
    xor01 ^= datas[i];
  }

  SendData(TAG_STM_PRO, &xor01, 1);

  int len = WaitForSerialData(1, SERIAL_TIMEOUT);
  if (len > 0) {
    uint8_t datas[len];
    const int rxBytes = uart_read_bytes(UART_CONTROLLER, datas, len, 1000 / portTICK_RATE_MS);
    if (rxBytes > 0 && datas[0] == ACK) {
      ESP_LOGI(TAG_STM_PRO, "%s", "Flash Success");
      return ESP_OK;
    } else {
      ESP_LOGE(TAG_STM_PRO, "%s", "Flash Failure");
      return ESP_FAIL;
    }
  } else {
    ESP_LOGE(TAG_STM_PRO, "%s", "Serial Timeout");
  }
  return ESP_FAIL;
}

esp_err_t ReadPage(const char *address, const char *datas) {
  ESP_LOGI(TAG_STM_PRO, "%s", "Reading page");
  char param[] = { 0xFF, 0x00 };

  CmdRead();

  LoadAddress(address[0], address[1], address[2], address[3]);

  SendData(TAG_STM_PRO, param, sizeof(param));
  int len = WaitForSerialData(257, SERIAL_TIMEOUT);
  if (len > 0) {
    uint8_t uart_data[len];
    const int rxBytes = uart_read_bytes(UART_NUM_1, uart_data, len, 1000 / portTICK_RATE_MS);

    if (rxBytes > 0 && uart_data[0] == 0x79) {
      ESP_LOGI(TAG_STM_PRO, "%s", "Success");
      if (!memcpy((void *)datas, uart_data, 257)) {
        return ESP_FAIL;
      }

    } else {
      ESP_LOGE(TAG_STM_PRO, "%s", "Failure");
      return ESP_FAIL;
    }
  } else {
    ESP_LOGE(TAG_STM_PRO, "%s", "Serial Timeout");
    return ESP_FAIL;
  }

  return ESP_OK;
}
