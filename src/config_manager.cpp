#include "config_manager.h"

// ============================================================================
// LOAD
// ============================================================================
void Load_info_File()
{
    File file = LittleFS.open("/info.dat", "r");
    if (!file)
    {
        Serial.println("[CFG] /info.dat not found, using default configuration");
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

    if (doc["WIFI_SSID"].is<const char *>())
        snprintf(WIFI_SSID, sizeof(WIFI_SSID), "%s", (const char *)(doc["WIFI_SSID"] | ""));
    if (doc["WIFI_PASS"].is<const char *>())
        snprintf(WIFI_PASS, sizeof(WIFI_PASS), "%s", (const char *)(doc["WIFI_PASS"] | ""));
    if (doc["CORE_IOT_TOKEN"].is<const char *>())
        snprintf(CORE_IOT_TOKEN, sizeof(CORE_IOT_TOKEN), "%s", (const char *)(doc["CORE_IOT_TOKEN"] | ""));
    if (doc["CORE_IOT_SERVER"].is<const char *>())
        snprintf(CORE_IOT_SERVER, sizeof(CORE_IOT_SERVER), "%s", (const char *)(doc["CORE_IOT_SERVER"] | "app.coreiot.io"));
    if (doc["CORE_IOT_PORT"].is<const char *>())
        snprintf(CORE_IOT_PORT, sizeof(CORE_IOT_PORT), "%s", (const char *)(doc["CORE_IOT_PORT"] | "1883"));

    Serial.printf("[CFG] Loaded: SSID=%s, CoreIOT Server=%s:%s, Token=%s\n",
                  WIFI_SSID, CORE_IOT_SERVER, CORE_IOT_PORT, CORE_IOT_TOKEN);
}

// ============================================================================
// SAVE
// ============================================================================
void Save_info_File(const char *wifi_ssid,
                    const char *wifi_pass,
                    const char *core_token,
                    const char *core_server,
                    const char *core_port)
{
    snprintf(WIFI_SSID, sizeof(WIFI_SSID), "%s", wifi_ssid);
    snprintf(WIFI_PASS, sizeof(WIFI_PASS), "%s", wifi_pass);
    snprintf(CORE_IOT_TOKEN, sizeof(CORE_IOT_TOKEN), "%s", core_token);
    snprintf(CORE_IOT_SERVER, sizeof(CORE_IOT_SERVER), "%s", core_server);
    snprintf(CORE_IOT_PORT, sizeof(CORE_IOT_PORT), "%s", core_port);

    DynamicJsonDocument doc(1024);
    doc["WIFI_SSID"] = wifi_ssid;
    doc["WIFI_PASS"] = wifi_pass;
    doc["CORE_IOT_TOKEN"] = core_token;
    doc["CORE_IOT_SERVER"] = core_server;
    doc["CORE_IOT_PORT"] = core_port;

    File file = LittleFS.open("/info.dat", "w");
    if (!file)
    {
        Serial.println("[CFG] Cannot open /info.dat for write!");
        return;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("[CFG] Config saved to LittleFS! Restarting...");
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
// CHECK & MOUNT LITTLEFS
// ============================================================================
bool check_info_File(bool check)
{
    if (!LittleFS.begin(true))
    {
        Serial.println("[CFG] LittleFS Mount Failed!");
        return false;
    }
    Serial.println("[CFG] LittleFS Mounted Successfully");

    if (check)
    {
        return LittleFS.exists("/info.dat");
    }
    else
    {
        Load_info_File();
        return true;
    }
}
