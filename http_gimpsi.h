#ifndef HTTP_GIMPSI_H
#define HTTP_GIMPSI_H

#include "esp_to_stm.h"
#include "FS.h"
#include "HTTPClient.h"
#include "WiFiClient.h"

#define HTTP_SERVER_TIMEOUT 3000

//Generic functions
int HTTP_GimpsiGetResponse(WiFiClient* client);
void HTTP_GimpsiGetParams(WiFiClient* client, int* response_code, SampleConfig* parameters);
bool HTTP_GimpsiConnect(WiFiClient* client, const IPAddress server);
bool HTTP_GimpsiBegin(HTTPClient* http, String path, const IPAddress server);

//GET-related functions
String HTTP_GimpsiBuildGET_Request(String param1, String param2, String id, const IPAddress server);
bool HTTP_GimpsiGET_AndWriteToSD(File* file, HTTPClient* http);

//POST-related functions
String HTTP_GimpsiBuildPOST_Request(uint32_t totalLen, const IPAddress server);
String HTTP_GimpsiBuildPOST_Head(String file_name);
String HTTP_GimpsiBuildPOST_Tail(void);
bool HTTP_GimpsiReadFromSD_AndPOST(File* file, WiFiClient* client);

#endif
