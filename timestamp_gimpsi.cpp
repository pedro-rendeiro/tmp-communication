#include "config.h"
#include "timestamp_gimpsi.h"

#define TAG "Timestamp Generator"

String FormattedDate(struct tm* ptm) {
  String f_date;  // AAAAMMDD

  // Year
  std::string year_str = std::to_string(ptm->tm_year + 1900);
  f_date.concat(year_str.c_str());
  // Month
  if ((ptm->tm_mon + 1) < 10) {
    f_date.concat('0');
  }
  std::string month_str = std::to_string(ptm->tm_mon + 1);
  f_date.concat(month_str.c_str());
  // Day
  if ((ptm->tm_mday) < 10) {
    f_date.concat('0');
  }
  std::string day_str = std::to_string(ptm->tm_mday);
  f_date.concat(day_str.c_str());

  ESP_LOGI(TAG, "Date generated: %s", f_date.c_str());
  return f_date;
}

String FormattedTime(NTPClient* ntp) {
  String f_time;  // hhmmss
  // Hour
  if ((ntp->getHours()) < 10) {
    f_time.concat('0');
  }
  std::string hour_str = std::to_string(ntp->getHours());
  f_time.concat(hour_str.c_str());
  // Minute
  if ((ntp->getMinutes()) < 10) {
    f_time.concat('0');
  }
  std::string min_str = std::to_string(ntp->getMinutes());
  f_time.concat(min_str.c_str());
  // Second
  if ((ntp->getSeconds()) < 10) {
    f_time.concat('0');
  }
  std::string sec_str = std::to_string(ntp->getSeconds());
  f_time.concat(sec_str.c_str());

  ESP_LOGI(TAG, "Time generated: %s", f_time.c_str());
  return f_time;
}

String CreateTimestamp(NTPClient* ntp) {
  String timestamp_filename;
  time_t epochTime = ntp->getEpochTime();
  struct tm* ptm = gmtime((time_t*)&epochTime);
  // Date
  timestamp_filename.concat(FormattedDate(ptm));
  timestamp_filename.concat("_");
  // TIME
  timestamp_filename.concat(FormattedTime(ntp));
  ESP_LOGI(TAG, "Timestamp generated: %s", timestamp_filename.c_str());
  return timestamp_filename;
}

bool CheckRTC_Update(uint32_t start_new, uint32_t start_old) {
  bool t_update = false;
  if ((start_new - start_old) >= UPDATE_TIME_FROM_NTP_SERVER_MS) t_update = true;
  return t_update;
}

bool RTC_Update(NTPClient* ntp) {
  bool update_completed = false;
  if (ntp->forceUpdate()) {
    ESP_LOGI(TAG, "Time updated from NTP");
    update_completed = true;
  } else {
    ESP_LOGE(TAG, "unable to update from server. Using Local RTC to update time.");
  }
  return update_completed;
}

bool BeginRTC(NTPClient* ntp) {
  bool beginRTC_completed = false;  
  ntp->begin();
  for (int i=0; i<10; i++) {
    if (ntp->forceUpdate()) {
      ESP_LOGI(TAG, "NTP server time retrieved successfully");
      beginRTC_completed = true;
      return beginRTC_completed;
    }
  }
  ESP_LOGE(TAG, "Failed to obtain time from NTP server");
  return beginRTC_completed;
  // ESP.restart();
}

bool Reconnect_ServerNTP(NTPClient* ntp, bool* server_ntp_ok){
  ESP_LOGI(TAG, "NTP status : %s", (*server_ntp_ok) ? "ON" : "OFF"); 
  bool reconnect = false;  
  if (!(*server_ntp_ok)){
    ESP_LOGI(TAG, "Reconnecting ...");     
    ntp->begin();
    if (!ntp->update()){
      ESP_LOGE(TAG, "Failed to reconnect with NTP server");
      return reconnect;   
    }
    reconnect = true;
  }        
  return reconnect;
}
