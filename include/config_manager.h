/*
 * check_info.h - Quản lý file cấu hình WiFi và CoreIOT trên LittleFS
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Biến config toàn cục (định nghĩa trong global_vars.cpp)
extern char WIFI_SSID[33];
extern char WIFI_PASS[65];
extern char CORE_IOT_TOKEN[64];
extern char CORE_IOT_SERVER[128];
extern char CORE_IOT_PORT[6];

void Load_info_File();
void Save_info_File(const char *wifi_ssid,
                    const char *wifi_pass,
                    const char *core_token,
                    const char *core_server = "app.coreiot.io",
                    const char *core_port = "1883");
void Delete_info_File();

bool check_info_File(bool check);

#endif // CONFIG_MANAGER_H
