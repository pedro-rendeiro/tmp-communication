#ifndef _TIMERSTAMP_FUNCTIONS_H
#define _TIMERSTAMP_FUNCTIONS_H

#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "NTPClient.h"

#define LENGTH_DATE 8
#define LENGTH_TIME 6
#define LENGTH_TIMESTMP (LENGTH_DATE + LENGTH_TIME + 7)

const long gmt_offset = -10800;     // Adjust UTC offset to GMT -3


/**********************************************************************************
 * Def.: Format the date as "AAAAMMDD"
 * parameters: static struct tm*
 * return: String 
**********************************************************************************/
String FormattedDate(struct tm* ptm);

/**********************************************************************************
 * Def.: Format the time as "hhmmss".
 * parameters: static struct tm*
 * return: String 
**********************************************************************************/
String FormattedTime(NTPClient* ntp);

/**********************************************************************************
 * Def.: Format the timestamp as "AAAAMMDD_hhmmss".
 * parameters: NTPClient*
 * return: String 
**********************************************************************************/
String CreateTimestamp(NTPClient* ntp);

/**********************************************************************************
 * Def.: initializes and gives first RTC update through NTP server
 * parameters: NTPClient*
 * return: bool that indicates if there was an update
**********************************************************************************/
bool BeginRTC(NTPClient* ntp);

/**********************************************************************************
 * Def.: checks if it's time to update the RTC, and executes it if necessary
**********************************************************************************/
bool CheckRTC_Update(uint32_t start_new, uint32_t start_old);
bool RTC_Update(NTPClient* ntp);
#endif

bool Reconnect_ServerNTP(NTPClient* ntp, bool* server_ntp_ok);
