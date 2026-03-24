#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include "time_keeper.hpp"

//The NTP client
static WiFiUDP* udp = nullptr;
static NTPClient* timeClient = nullptr;

//time tracker
unsigned long baseEpoch = 0;
unsigned long lastMillis = 0;
int lastSyncDay = -1;

void TimeInit(){

    if (!udp){
        udp = new WiFiUDP();
        timeClient = new NTPClient(*udp, "pool.ntp.org", 0, 60000);
    }


    udp->begin(2390);
    timeClient->begin();


    for (int i = 0; i < 5; i++){
        timeClient->forceUpdate();
        if (timeClient->isTimeSet()){
            baseEpoch = timeClient->getEpochTime();
            lastMillis = millis();

            time_t raw = baseEpoch;
            struct tm *t = gmtime(&raw);
            lastSyncDay = t->tm_mday;

            if(baseEpoch != 0){
                Serial.println("NTP SUCCESS");
                return;
            }
        }
        Serial.println("NTP retry...");
        delay(500);
    }
    Serial.println("NTP FAIL");
}

unsigned long GetEpochTime(){
    return baseEpoch + (millis() - lastMillis) / 1000;
}

bool isDST(int year, int month, int day, int hour, int wday){
    
    if (month < 3 || month > 10) return false;
    if (month < 3 && month > 10) return true;

    int lastSunday = day - wday;

    if (month == 3){
        return lastSunday >= 25 && (wday != 0 || hour >= 1);
    }

    if (month == 10){
        return lastSunday >= 25 && (wday != 0 || hour >= 1);
    } 
    return false;

}

uint16_t GetTimeStamp(){
    unsigned long ts = GetEpochTime();

    time_t raw = ts;
    struct tm *t = gmtime(&raw);

    int year = t->tm_year + 1900;
    int month = t->tm_mon +1;
    int day = t->tm_mday;
    int hour = t->tm_hour;
    int minute = t->tm_min;
    int wday = t->tm_wday;

    hour += 1;

    if (isDST(year, month, day, hour, wday)){
        hour += 1;
    }

    if (hour >= 24){
        hour -= 1;
    }

    return (hour * 100) + minute;
}

void TimeSyncDaily(){
    unsigned long now = GetTimeStamp();

    time_t raw = now;
    struct tm *t = gmtime(&raw);

    int day = t->tm_mday;
    int hour = t->tm_hour;
    int minute = t->tm_min;

    if (day != lastSyncDay && hour == 0 && minute <= 1){
        if (timeClient->update()){
            baseEpoch = timeClient->getEpochTime();
            lastMillis = millis();
            lastSyncDay = day;
        }
    }
}
