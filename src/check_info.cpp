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

    if (doc["WIFI_SSID"].is<const char *>())
        snprintf(WIFI_SSID, sizeof(WIFI_SSID), "%s", (const char *)(doc["WIFI_SSID"] | ""));
    if (doc["WIFI_PASS"].is<const char *>())
        snprintf(WIFI_PASS, sizeof(WIFI_PASS), "%s", (const char *)(doc["WIFI_PASS"] | ""));
    if (doc["CORE_IOT_TOKEN"].is<const char *>())
        snprintf(CORE_IOT_TOKEN, sizeof(CORE_IOT_TOKEN), "%s", (const char *)(doc["CORE_IOT_TOKEN"] | ""));
    if (doc["CORE_IOT_SERVER"].is<const char *>())
        snprintf(CORE_IOT_SERVER, sizeof(CORE_IOT_SERVER), "%s", (const char *)(doc["CORE_IOT_SERVER"] | ""));
    if (doc["CORE_IOT_PORT"].is<const char *>())
        snprintf(CORE_IOT_PORT, sizeof(CORE_IOT_PORT), "%s", (const char *)(doc["CORE_IOT_PORT"] | "1883"));
    if (doc["LOCAL_MQTT_HOST"].is<const char *>())
        snprintf(LOCAL_MQTT_HOST, sizeof(LOCAL_MQTT_HOST), "%s", (const char *)(doc["LOCAL_MQTT_HOST"] | ""));
    if (doc["LOCAL_MQTT_PORT"].is<const char *>())
        snprintf(LOCAL_MQTT_PORT, sizeof(LOCAL_MQTT_PORT), "%s", (const char *)(doc["LOCAL_MQTT_PORT"] | "1883"));

    Serial.printf("[CFG] Loaded: SSID=%s, Server=%s:%s, LocalMQTT=%s:%s\n",
                  WIFI_SSID, CORE_IOT_SERVER, CORE_IOT_PORT,
                  LOCAL_MQTT_HOST, LOCAL_MQTT_PORT);
}

// ============================================================================
// SAVE
// ============================================================================
void Save_info_File(const char *wifi_ssid,
                    const char *wifi_pass,
                    const char *core_token,
                    const char *core_server,
                    const char *core_port,
                    const char *local_host,
                    const char *local_port)
{
    snprintf(WIFI_SSID, sizeof(WIFI_SSID), "%s", wifi_ssid);
    snprintf(WIFI_PASS, sizeof(WIFI_PASS), "%s", wifi_pass);
    snprintf(CORE_IOT_TOKEN, sizeof(CORE_IOT_TOKEN), "%s", core_token);
    snprintf(CORE_IOT_SERVER, sizeof(CORE_IOT_SERVER), "%s", core_server);
    snprintf(CORE_IOT_PORT, sizeof(CORE_IOT_PORT), "%s", core_port);
    snprintf(LOCAL_MQTT_HOST, sizeof(LOCAL_MQTT_HOST), "%s", local_host);
    snprintf(LOCAL_MQTT_PORT, sizeof(LOCAL_MQTT_PORT), "%s", local_port);

    DynamicJsonDocument doc(1024);
    doc["WIFI_SSID"] = wifi_ssid;
    doc["WIFI_PASS"] = wifi_pass;
    doc["CORE_IOT_TOKEN"] = core_token;
    doc["CORE_IOT_SERVER"] = core_server;
    doc["CORE_IOT_PORT"] = core_port;
    doc["LOCAL_MQTT_HOST"] = local_host;
    doc["LOCAL_MQTT_PORT"] = local_port;

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

    if (WIFI_SSID[0] == '\0')
    {
        Serial.println("[CFG] No WiFi config found - AP mode will start");
        return false;
    }

    return true;
}