/*
 * check_info.h
 * Chứa hàm kiểm tra và quản lý file config trên LittleFS
 */

#ifndef CHECK_INFO_H
#define CHECK_INFO_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Biến config toàn cục (định nghĩa trong global_vars.cpp)
extern char WIFI_SSID[33];
extern char WIFI_PASS[65];
extern char CORE_IOT_TOKEN[64];
extern char CORE_IOT_SERVER[128];
extern char CORE_IOT_PORT[6];
extern char LOCAL_MQTT_HOST[64]; // IP máy chạy NestJS
extern char LOCAL_MQTT_PORT[6];  // Port Mosquitto (mặc định 1883)

void Load_info_File();
void Save_info_File(const char *wifi_ssid,
                    const char *wifi_pass,
                    const char *core_token,
                    const char *core_server,
                    const char *core_port,
                    const char *local_host,
                    const char *local_port);
void Delete_info_File();

bool check_info_File(bool check);

#endif // CHECK_INFO_H