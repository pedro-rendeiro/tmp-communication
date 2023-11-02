#include <Arduino.h>
#include <IPAddress.h>

#include "config.h"
#include "http_gimpsi.h"

#define TAG "http_gimpsi"

//***********************
// Global constants
//***********************
String BOUNDARY = "GIMPSI";

// ENDPOINTS
String upload_endpoint = "/upload";
String download_endpoint = "/download";
String verify_endpoint = "/verify";

//***********************
// Generic functions
//***********************
void HTTP_GimpsiGetParams(WiFiClient* client, int* response_code, SampleConfig* parameters) {
  String response = client->readStringUntil('\r');
  ESP_LOGI(TAG, "HTTP response: %s", response.c_str());
  int space_index_one = response.indexOf(" ");
  int space_index_two = response.indexOf(" ", space_index_one + 1);
  int space_index_three = response.indexOf(" ", space_index_two + 1);
  if (space_index_one != -1) {
    (*response_code) = response.substring(space_index_one + 1, space_index_two).toInt();
    (*parameters).sample_size = response.substring(space_index_two + 1, space_index_three).toInt();
    (*parameters).sample_period = response.substring(space_index_three + 1).toInt();
  } else ESP_LOGE(TAG, "Server did not respond");
  return;
}

int HTTP_GimpsiGetResponse(WiFiClient* client) {
  String response = client->readStringUntil('\r');
  ESP_LOGI(TAG, "HTTP response: %s", response.c_str());
  int space_index_one = response.indexOf(" ");
  int space_index_two = response.indexOf(" ", space_index_one + 1);
  return response.substring(space_index_one + 1, space_index_two).toInt();
}

bool HTTP_GimpsiConnect(WiFiClient* client, const IPAddress server) {
  if (client->connected()) {
    return true;
  }

  if (client->connect(server, server_port, HTTP_SERVER_TIMEOUT)) {
    ESP_LOGI(TAG, "Connecting to server: %s", server.toString().c_str());
    return true;
  }

  else {
    ESP_LOGE(TAG, "Connection to %s failed.", server.toString().c_str());
    return false;
  }
}


bool HTTP_GimpsiBegin(HTTPClient* http, String path, const IPAddress server) {
  ESP_LOGD(TAG, "[HTTP] begin...\n");

  // configure server and url
  http->begin(server.toString(), server_port, path);

  ESP_LOGD(TAG, "[HTTP] GET...\n");
  // start connection and send HTTP header
  int http_code = http->GET();
  if (http_code > 0) {
    // HTTP header has been send and Server response header has been handled
    ESP_LOGI(TAG, "[HTTP] GET... code: %d\n", http_code);
    // file found at server
    if (http_code == HTTP_CODE_OK) {
      return true;
    }
  }
  ESP_LOGE(TAG, "[HTTP] GET... failed, error: %s\n", http->errorToString(http_code).c_str());
  return false;
}


//***********************
// GET-related functions
//***********************
String HTTP_GimpsiBuildGET_Request(String param1, String param2, String id, const IPAddress server) {
  String get_request = "GET " + verify_endpoint + "?CompileSTM=" + param1 + "&CompileESP=" + param2 + "&ID=" + id + "&App=" + "Main%20App" + " HTTP/1.1\r\n" + "Host: " + server.toString().c_str() + ":" + String(server_port) + "\r\n" + "Connection: close\r\n\r\n";
  return get_request;
}


bool HTTP_GimpsiGET_AndWriteToSD(File* file, HTTPClient* http) {
  uint8_t http_buff[256] = { 0 };
  int len = http->getSize();
  WiFiClient* stream = http->getStreamPtr();
  double t0 = millis();
  
  while ( len > 0 ) {
    size_t size = stream->available();

    if (size) {
      if (!http->connected()){
        ESP_LOGE(TAG, "Error: server conncection lost");
        return false;
      }
      int c = stream->readBytes(http_buff, ((size > sizeof(http_buff)) ? sizeof(http_buff) : size));
      file->write(http_buff, c);
      len -= c;
    }

    if (millis() - t0 > 15000){
      ESP_LOGE(TAG, "Error: timeout reached");
      return false;
    }
    
  }

  ESP_LOGD(TAG, "Download was sucessfull");
  return true;
}

//***********************
// POST-related functions
//***********************
String HTTP_GimpsiBuildPOST_Request(uint32_t totalLen, const IPAddress server) {
  String post_request = ("POST " + upload_endpoint + " HTTP/1.1\r\nHost: " + server.toString().c_str() + ":" + String(server_port) + "\r\nContent-Length: " + String(totalLen) + "\r\nContent-Type: multipart/form-data; boundary=" + BOUNDARY + "\r\n");
  return post_request;
}

String HTTP_GimpsiBuildPOST_Head(String file_name) {
  String head = ("--" + BOUNDARY + "\r\nContent-Disposition: form-data; name=\"files\"; filename=\"" + file_name + "\"\r\nContent-Type: Multipart/form-data\r\n\r\n");
  return head;
}

String HTTP_GimpsiBuildPOST_Tail(void) {
  String tail = ("\r\n--" + BOUNDARY + "--\r\n");
  return tail;
}

bool HTTP_GimpsiReadFromSD_AndPOST(File* file, WiFiClient* client) {
  #define POST_BUFFER_SIZE 1024
  uint8_t fbBuf[POST_BUFFER_SIZE];

  size_t fbLen = file->size();
  ESP_LOGD(TAG, "fbLen = %lu", fbLen);
  ESP_LOGD(TAG, "fbLen mod %d = %d", POST_BUFFER_SIZE, (fbLen % POST_BUFFER_SIZE));
  ESP_LOGD(TAG, "Entering the for loop:");
  for (size_t n = 0; n < fbLen; n = n + POST_BUFFER_SIZE) {
    ESP_LOGD(TAG, "n = %lu", n);
    if (n + POST_BUFFER_SIZE <= fbLen) {
      file->read(fbBuf, POST_BUFFER_SIZE);
      if(client->write(fbBuf, POST_BUFFER_SIZE) < POST_BUFFER_SIZE) {
        return false;
      }
    } else if (fbLen % POST_BUFFER_SIZE > 0) {
      size_t remainder = fbLen % POST_BUFFER_SIZE;
      file->read(fbBuf, remainder);
      if(client->write(fbBuf, remainder) < remainder) {
        return false;
      }
    }
  }
  ESP_LOGD(TAG, "Left the for loop");
  return true;
}