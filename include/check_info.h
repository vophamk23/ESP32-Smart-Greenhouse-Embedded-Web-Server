
/*
 * check_info.h
 * Chứa hàm kiểm tra và quản lý file config trên LittleFS
 * - Load thông tin từ file config vào biến toàn cục
 * - Lưu thông tin từ biến toàn cục vào file config
 * - Xóa file config khi cần thiết
 * - Hàm check để xác định xem có cần mount LittleFS + load config hay không
 *
 * Cách sử dụng:
 * 1. Gọi check_info_File(false) trong setup() để kiểm tra và load config nếu cần.
 *    Nếu file config không tồn tại, sẽ tự động tạo AP để cấu hình WiFi.
 * 2. Gọi check_info_File(true) trong các task khác để chỉ kiểm tra biến toàn cục mà không đụng đến LittleFS.
 *
 * Lưu ý:
 * - Biến toàn cục (WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT) được định nghĩa ở main.cpp và extern ở đây.
 * - Các hàm Load_info_File(), Save_info_File(), Delete_info_File() sẽ thao tác trực tiếp với LittleFS để quản lý file config.
 */
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>


// Biến config toàn cục (định nghĩa trong main.cpp)
extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

void Load_info_File();
void Save_info_File(String wifi_ssid, String wifi_pass,
                    String core_token, String core_server, String core_port);
void Delete_info_File();

// check=false : mount LittleFS + load + startAP() nếu chưa có config
// check=true  : chỉ kiểm tra biến, không đụng LittleFS
bool check_info_File(bool check);

