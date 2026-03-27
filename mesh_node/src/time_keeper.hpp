#pragma once

#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

//time tracker
extern unsigned long baseEpoch;
extern unsigned long lastMillis;
extern int lastSyncDay;

void TimeInit();
unsigned long GetEpochTime();
bool isDST(int year, int month, int day, int hour, int wday);
uint16_t GetTimeStamp();
void TimeSyncDaily();
