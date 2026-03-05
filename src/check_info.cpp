#include "check_info.h"

// ============================================================================
// LOAD
// ============================================================================
void Load_info_File()
{
    File file = LittleFS.open("/info.dat", "r");
    if (!file)
    {
        Serial.println("[CFG] /info.dat not found");
        return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err)
    {
        Serial.printf("[CFG] JSON parse error: %s\n", err.c_str());
        return;
    }

    if (doc["WIFI_SSID"].is<const char*>())      WIFI_SSID       = doc["WIFI_SSID"].as<String>();
    if (doc["WIFI_PASS"].is<const char*>())      WIFI_PASS       = doc["WIFI_PASS"].as<String>();
    if (doc["CORE_IOT_TOKEN"].is<const char*>()) CORE_IOT_TOKEN  = doc["CORE_IOT_TOKEN"].as<String>();
    if (doc["CORE_IOT_SERVER"].is<const char*>()) CORE_IOT_SERVER = doc["CORE_IOT_SERVER"].as<String>();
    if (doc["CORE_IOT_PORT"].is<const char*>())  CORE_IOT_PORT   = doc["CORE_IOT_PORT"].as<String>();

    Serial.printf("[CFG] Loaded: SSID=%s, Server=%s:%s\n",
                  WIFI_SSID.c_str(),
                  CORE_IOT_SERVER.c_str(),
                  CORE_IOT_PORT.c_str());
}

// ============================================================================
// SAVE
// ============================================================================
void Save_info_File(String wifi_ssid,
                    String wifi_pass,
                    String core_token,
                    String core_server,
                    String core_port)
{
    WIFI_SSID       = wifi_ssid;
    WIFI_PASS       = wifi_pass;
    CORE_IOT_TOKEN  = core_token;
    CORE_IOT_SERVER = core_server;
    CORE_IOT_PORT   = core_port;

    DynamicJsonDocument doc(1024);
    doc["WIFI_SSID"]       = wifi_ssid;
    doc["WIFI_PASS"]       = wifi_pass;
    doc["CORE_IOT_TOKEN"]  = core_token;
    doc["CORE_IOT_SERVER"] = core_server;
    doc["CORE_IOT_PORT"]   = core_port;

    File file = LittleFS.open("/info.dat", "w");
    if (!file)
    {
        Serial.println("[CFG] Cannot open /info.dat for write!");
        return;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("[CFG] Saved! Restarting...");
    delay(200);
    ESP.restart();
}

// ============================================================================
// DELETE (factory reset)
// ============================================================================
void Delete_info_File()
{
    if (LittleFS.exists("/info.dat"))
    {
        LittleFS.remove("/info.dat");
        Serial.println("[CFG] /info.dat deleted");
    }
    delay(200);
    ESP.restart();
}

// ============================================================================
// CHECK
// ============================================================================
bool check_info_File(bool check)
{
    if (!check)
    {
        if (!LittleFS.begin(true))
        {
            Serial.println("[CFG] LittleFS mount failed!");
            return false;
        }
        Load_info_File();
    }

    if (WIFI_SSID.isEmpty())
    {
        // Không có config → initWiFi() trong main.cpp sẽ tự bật AP
        Serial.println("[CFG] No WiFi config found - AP mode will start");
        return false;
    }

    return true;
}