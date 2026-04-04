```
███████╗ ██████╗ ██████╗  ██████╗ ██████╗ ███████╗███████╗███╗   ██╗
██╔════╝██╔════╝██╔═══██╗██╔════╝ ██╔══██╗██╔════╝██╔════╝████╗  ██║
█████╗  ██║     ██║   ██║██║  ███╗██████╔╝█████╗  █████╗  ██╔██╗ ██║
██╔══╝  ██║     ██║   ██║██║   ██║██╔══██╗██╔══╝  ██╔══╝  ██║╚██╗██║
███████╗╚██████╗╚██████╔╝╚██████╔╝██║  ██║███████╗███████╗██║ ╚████║
╚══════╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═══╝
```

# 🌿 ESP32 Smart Greenhouse — Embedded Web Server

**Hệ thống nhà kính thông minh chạy trên **ESP32**, tích hợp Web Server nhúng với giao diện quản lý đầy đủ qua trình duyệt: giám sát cảm biến, điều khiển thiết bị, lập lịch tự động, cảnh báo thông minh và cập nhật firmware OTA.**
<div align="center">
<br/>

[![ESP32](https://img.shields.io/badge/ESP32-FreeRTOS-blue?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![TypeScript](https://img.shields.io/badge/TypeScript-Language-3178C6?style=for-the-badge&logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![MQTT](https://img.shields.io/badge/MQTT-Protocol-660066?style=for-the-badge&logo=mqtt&logoColor=white)](https://mqtt.org/)

</div>



## 📋 Mục lục

- [Tính năng](#-tính-năng)
- [Kiến trúc hệ thống](#-kiến-trúc-hệ-thống)
- [Dual-Core & Cooperative Scheduler](#-dual-core--cooperative-scheduler)
- [Cấu trúc thư mục](#-cấu-trúc-thư-mục)
- [Phần cứng yêu cầu](#-phần-cứng-yêu-cầu)
- [Thư viện sử dụng](#-thư-viện-sử-dụng)
- [Cài đặt & Cấu hình](#-cài-đặt--cấu-hình)
- [Giao diện Web](#-giao-diện-web)
- [Giao tiếp thời gian thực](#-giao-tiếp-thời-gian-thực)
- [Lập lịch & Smart Logic](#-lập-lịch--smart-logic)
- [OTA Update](#-ota-update)
- [Đóng góp](#-đóng-góp)

## ✨ Tính năng

| Tính năng                   | Mô tả                                                      |
| --------------------------- | ---------------------------------------------------------- |
| 📊 **Dashboard**            | Hiển thị trạng thái tổng quan hệ thống theo thời gian thực |
| 🌡️ **Giám sát cảm biến**    | Đọc nhiệt độ, độ ẩm (DHT), đồng bộ thời gian thực (RTC)    |
| 🔌 **Điều khiển thiết bị**  | Bật/tắt actuator thủ công hoặc tự động qua giao diện web   |
| 🗓️ **Lập lịch (Scheduler)** | Đặt lịch tưới nước, chiếu sáng theo giờ/ngày               |
| 🧠 **Smart Logic**          | Tự động điều chỉnh thiết bị dựa trên ngưỡng cảm biến       |
| 🔔 **Thông báo**            | Cảnh báo khi thông số vượt ngưỡng cài đặt                  |
| 📈 **Biểu đồ**              | Hiển thị lịch sử dữ liệu cảm biến dưới dạng đồ thị         |
| 📋 **Nhật ký (Logs)**       | Ghi lại lịch sử hoạt động của hệ thống                     |
| 🖥️ **LCD Display**          | Hiển thị thông tin trực tiếp trên màn hình LCD I2C         |
| 🔄 **OTA Update**           | Cập nhật firmware không dây qua ElegantOTA                 |
| 📡 **MQTT / IoT Bridge**    | Kết nối với broker MQTT để tích hợp IoT platform           |
| 👤 **Quản lý tài khoản**    | Xác thực người dùng, bảo vệ giao diện web                  |



## 🏗️ Kiến trúc hệ thống

Hệ thống khai thác kiến trúc **dual-core** của ESP32 — phân tách rõ ràng giữa tác vụ mạng/cloud (Core 0) và tác vụ điều khiển thực tế (Core 1).

```
┌─────────────────────────────────────────────────────────────────┐
│                            ESP32                                │
│                                                                 │
│  ┌───────────────────────────┐  ┌───────────────────────────┐   │
│  │         CORE 0            │  │         CORE 1            │   │
│  │   (FreeRTOS Tasks)        │  │  (Cooperative Scheduler)  │   │
│  │                           │  │      Arduino loop()       │   │
│  │  ┌─────────────────────┐  │  │                           │   │
│  │  │ greenhouse_wifi_task│  │  │  Task_ReadAirSensors      │   │
│  │  │   priority 3        │  │  │  Task_ReadSoilSensor      │   │
│  │  └─────────────────────┘  │  │  Task_AutoControl         │   │
│  │  ┌─────────────────────┐  │  │  Task_PumpWatchdog        │   │
│  │  │greenhouse_coreiot   │  │  │  Task_CheckSchedule       │   │
│  │  │  _task  priority 2  │  │  │  Task_CheckAlerts         │   │
│  │  └─────────────────────┘  │  │  Task_UpdateLEDStatus     │   │
│  │  ┌─────────────────────┐  │  │  Task_UpdateLCD           │   │
│  │  │greenhouse_webserver │  │  │  Task_AutoSwitchLCDPage   │   │
│  │  │  _task  priority 1  │  │  │  Task_SendTelemetry       │   │
│  │  └─────────────────────┘  │  │  Task_ProcessRpc          │   │
│  │                           │  │  Task_ScanButtons         │   │
│  │  ESPAsyncWebServer        │  │  Task_Heartbeat           │   │
│  │  WebSocket endpoint       │  │                           │   │
│  │  REST API + LittleFS      │  │  sensor_handler           │   │
│  └───────────────────────────┘  │  actuator_handler         │   │
│                                 │  lcd_display              │   │
│  ┌──────────┐  ┌─────────────┐  │  scheduler / smartlogic   │   │
│  │ DHT22    │  │ Relay/Fan   │  └───────────────────────────┘   │
│  │ RTC DS   │  │ NeoPixel    │                                  │
│  │ Soil/Lux │  │ LCD I2C     │                                  │
│  │ Button   │  │ Pump        │                                  │
│  └──────────┘  └─────────────┘                                  │
└─────────────────────────────────────────────────────────────────┘
          │                              │
          ▼                              ▼
   [Browser / Mobile]           [CoreIoT / MQTT Broker]
   WebSocket + REST API         Telemetry + RPC Commands
```



## ⚡ Dual-Core & Cooperative Scheduler

### Phân chia Core

| Core       | Loại        | Task                                                 | Priority | Stack  |
| ---------- | ----------- | ---------------------------------------------------- | -------- | ------ |
| **Core 0** | FreeRTOS    | `greenhouse_wifi_task` — giữ kết nối WiFi, reconnect | 3        | 4096   |
| **Core 0** | FreeRTOS    | `greenhouse_coreiot_task` — giao tiếp CoreIoT/MQTT   | 2        | 8192   |
| **Core 0** | FreeRTOS    | `greenhouse_webserver_task` — phục vụ HTTP/WebSocket | 1        | 8192   |
| **Core 1** | Cooperative | Tất cả task điều khiển & cảm biến (xem bảng dưới)    | —        | loop() |

> Core 0 xử lý toàn bộ I/O mạng không blocking. Core 1 chạy vòng lặp `SCH_Dispatch_Tasks()` và các lệnh Serial debug.

### Cooperative Scheduler — Core 1

| #   | Task                     | Chu kỳ   | Chức năng                                                                |
| --- | ------------------------ | -------- | ------------------------------------------------------------------------ |
| 1   | `Task_ReadAirSensors`    | 2 giây   | Đọc DHT22 (nhiệt độ, độ ẩm) + cảm biến ánh sáng                          |
| 2   | `Task_ReadSoilSensor`    | 5 giây   | Đọc độ ẩm đất (thay đổi chậm)                                            |
| 3   | `Task_CheckAlerts`       | 1 giây   | Kiểm tra ngưỡng cảnh báo, cập nhật flag                                  |
| 4   | `Task_AutoControl`       | 1 giây   | Tự động điều khiển bơm & quạt theo ngưỡng                                |
| 5   | `Task_PumpWatchdog`      | 1 giây   | Bảo vệ bơm: force OFF nếu chạy quá `PUMP_MAX_ON_TIME_MS`, xử lý cooldown |
| 6   | `Task_UpdateLEDStatus`   | 500 ms   | Cập nhật NeoPixel theo trạng thái hệ thống                               |
| 7   | `Task_UpdateLCD`         | 500 ms   | Vẽ lại nội dung trang LCD hiện tại                                       |
| 8   | `Task_AutoSwitchLCDPage` | 5 giây   | Tự động chuyển trang LCD                                                 |
| 9   | `Task_Heartbeat`         | 5 giây   | In snapshot trạng thái hệ thống ra Serial                                |
| 10  | `Task_ScanButtons`       | debounce | Quét nút bấm vật lý                                                      |
| 11  | `Task_SendTelemetry`     | 5 giây   | Đóng gói & đẩy dữ liệu cảm biến lên CoreIoT                              |
| 12  | `Task_ProcessRpc`        | 200 ms   | Poll & thực thi lệnh RPC từ cloud                                        |
| 13  | `Task_CheckSchedule`     | 10 giây  | So khớp giờ RTC với lịch tưới, kích bơm nếu đúng giờ                     |

### Pump Watchdog

Watchdog bơm (`Task_PumpWatchdog`) có 3 lớp bảo vệ theo thứ tự ưu tiên:

```
1. Lịch tưới còn thời gian  →  tắt bơm khi hết duration
2. Không có lịch, chạy quá MAX_ON_TIME  →  force OFF
3. Bơm đã tắt  →  đếm cooldown, mở lại khi hết PUMP_COOLDOWN_MS
```

### Serial Debug Commands (Core 1)

| Phím | Hành động                      |
| ---- | ------------------------------ |
| `A`  | Toggle AUTO / MANUAL mode      |
| `P`  | Toggle bơm (chỉ trong MANUAL)  |
| `F`  | Toggle quạt (chỉ trong MANUAL) |
| `N`  | Chuyển trang LCD               |
| `S`  | In dữ liệu cảm biến            |
| `I`  | In thông tin WiFi & IoT        |
| `?`  | Hiển thị danh sách lệnh        |



## 📁 Cấu trúc thư mục

```
ESP32-Smart-Greenhouse-Embedded-Web-Server/
├── src/                          # Source code C++ (firmware)
│   ├── main.cpp                  # Entry point, khởi tạo hệ thống
│   ├── app_tasks.cpp             # Quản lý các FreeRTOS tasks
│   ├── sensor_handler.cpp        # Đọc & xử lý dữ liệu cảm biến
│   ├── actuator_handler.cpp      # Điều khiển relay, thiết bị đầu ra
│   ├── scheduler.cpp             # Lập lịch tự động
│   ├── smartlogic.cpp            # Logic tự động theo ngưỡng
│   ├── webserver_greenhouse.cpp  # Khởi tạo & xử lý Web Server
│   ├── wifi_greenhouse.cpp       # Kết nối WiFi
│   ├── lcd_display.cpp           # Hiển thị LCD I2C
│   ├── ntp_sync.cpp              # Đồng bộ thời gian NTP
│   ├── iot_bridge.cpp            # Kết nối MQTT/IoT platform
│   ├── button_handler.cpp        # Xử lý nút bấm vật lý
│   ├── check_info.cpp            # Kiểm tra thông tin hệ thống
│   ├── coreiot_greenhouse.cpp    # Tích hợp CoreIoT platform
│   └── global_vars.cpp           # Biến toàn cục
│
├── include/                      # Header files
│   ├── config.h                  # Cấu hình chính (WiFi, MQTT, Pins...)
│   ├── global_vars.h
│   └── ...
│
├── data/                         # SPIFFS filesystem (Web UI)
│   ├── index.html                # Trang chủ / redirect
│   ├── pages/                    # Các trang giao diện
│   │   ├── dashboard.html
│   │   ├── control.html
│   │   ├── charts.html
│   │   ├── logs.html
│   │   ├── schedule.html
│   │   ├── smartlogic.html
│   │   ├── threshold.html
│   │   ├── notify.html
│   │   ├── devices.html
│   │   └── account.html
│   ├── partials/                 # Thành phần dùng chung
│   │   ├── sidebar.html
│   │   ├── topbar.html
│   │   ├── login.html
│   │   └── modals.html
│   ├── js/                       # JavaScript modules
│   │   ├── websocket.js          # Kết nối WebSocket
│   │   ├── charts.js             # Vẽ biểu đồ
│   │   ├── control.js            # Điều khiển thiết bị
│   │   ├── schedule.js           # Quản lý lịch
│   │   ├── smartlogic.js         # Cài đặt smart logic
│   │   ├── threshold.js          # Cài đặt ngưỡng
│   │   ├── notify.js             # Thông báo
│   │   ├── logs.js               # Xem nhật ký
│   │   ├── devices.js            # Quản lý thiết bị
│   │   ├── auth.js               # Xác thực
│   │   ├── state.js              # Quản lý trạng thái
│   │   ├── ui.js                 # Cập nhật giao diện
│   │   └── helpers.js            # Hàm tiện ích
│   └── css/                      # Stylesheet
│       ├── styles.css
│       ├── base.css
│       ├── components.css
│       └── utilities.css
│
├── lib/                          # Thư viện cục bộ
│   ├── DHT/                      # Cảm biến DHT11/DHT22
│   ├── LCD/                      # LiquidCrystal I2C
│   ├── RTClib/                   # Đồng hồ thời gian thực
│   ├── ArduinoJson/              # Xử lý JSON
│   ├── AsyncTCP/                 # TCP bất đồng bộ cho ESP32
│   ├── ESPAsyncWebServer/        # Web server bất đồng bộ
│   ├── PubSubClient/             # MQTT client
│   ├── ElegantOTA-master/        # OTA update qua web
│   ├── Adafruit_NeoPixel/        # LED RGB WS2812
│   └── Adafruit_Sensor/          # Cảm biến Adafruit
│
├── platformio.ini                # Cấu hình PlatformIO
├── partitions.csv                # Phân vùng flash tùy chỉnh
└── greenhouse_block_diagram.html # Sơ đồ khối hệ thống
```



## 🔧 Phần cứng yêu cầu

| Linh kiện           | Mô tả                                       |
| ------------------- | ------------------------------------------- |
| **ESP32 DevKit**    | Vi điều khiển chính (esp32dev)              |
| **DHT22 / DHT11**   | Cảm biến nhiệt độ & độ ẩm                   |
| **DS1307 / DS3231** | Module đồng hồ thời gian thực (RTC)         |
| **LCD 16x2 I2C**    | Hiển thị thông tin trực tiếp                |
| **Relay Module**    | Điều khiển thiết bị công suất (bơm, đèn...) |
| **WS2812 NeoPixel** | LED RGB (tuỳ chọn, hiển thị trạng thái)     |
| **Button**          | Nút bấm vật lý reset / thao tác nhanh       |

---

## 📦 Thư viện sử dụng

| Thư viện            | Mục đích                                 |
| ------------------- | ---------------------------------------- |
| `ESPAsyncWebServer` | Web server bất đồng bộ, không blocking   |
| `AsyncTCP`          | TCP bất đồng bộ cho ESP32                |
| `ArduinoJson`       | Parse/serialize JSON cho API             |
| `DHT`               | Đọc cảm biến DHT11/DHT22                 |
| `RTClib`            | Giao tiếp module RTC (DS1307, DS3231...) |
| `LiquidCrystal_I2C` | Điều khiển LCD qua I2C                   |
| `PubSubClient`      | MQTT client                              |
| `ElegantOTA`        | Cập nhật firmware qua Web OTA            |
| `Adafruit_NeoPixel` | Điều khiển LED WS2812                    |
| `Adafruit_Sensor`   | Abstraction layer cảm biến Adafruit      |



## ⚙️ Cài đặt & Cấu hình

### Yêu cầu

- VSCode + [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
- Cáp USB Micro-B
- Driver CH340 hoặc CP2102 (tuỳ board)

### Clone repository

```bash
git clone https://github.com/vophamk23/ESP32-Smart-Greenhouse-Embedded-Web-Server.git
cd ESP32-Smart-Greenhouse-Embedded-Web-Server
```

### Cấu hình `data/info.dat` (LittleFS credentials)

File `data/info.dat` chứa thông tin kết nối WiFi và MQTT, được firmware đọc từ LittleFS khi khởi động. Tạo file với nội dung sau:

```json
{
  "WIFI_SSID":       "tên wifi",
  "WIFI_PASS":       "mật khẩu wifi",
  "CORE_IOT_TOKEN":  "token core iot",
  "CORE_IOT_SERVER": "app.coreiot.io",
  "CORE_IOT_PORT":   "1883",
  "LOCAL_MQTT_HOST": "broker.emqx.io",
  "LOCAL_MQTT_PORT": "1883"
}
```

> ⚠️ **Lưu ý bảo mật:** Không commit file `info.dat` chứa thông tin thật lên Git công khai. Thêm vào `.gitignore` nếu cần.



### Cấu hình GPIO trong `include/config.h`

```cpp
// GPIO pins — xem sơ đồ khối để biết cách đấu nối
#define DHT_PIN           27
#define LIGHT_SENSOR_PIN  34
#define SOIL_MOISTURE_PIN 33
#define RELAY_PUMP_PIN    17
#define FAN_S1_PIN        25
#define NEO_PIN           26
#define BTN_MODE_PIN      19
#define BTN_PUMP_PIN      18
#define BTN_FAN_PIN        5
```


### Các lệnh build (PlatformIO CLI)

```bash
cd ecogreen-iot

# Xoá toàn bộ flash (reset hoàn toàn)
pio run -t erase

# Xoá build cache
pio run -t clean

# Build firmware
pio run

# Upload firmware lên ESP32
pio run -t upload

# Upload LittleFS (web UI + info.dat trong thư mục data/)
pio run -t uploadfs

# Mở Serial Monitor (115200 baud)
pio device monitor --baud 115200
```

Hoặc dùng VSCode PlatformIO toolbar: **Build → Upload → Upload Filesystem → Monitor**


### Kiểm tra ESP32 hoạt động

Mở Serial Monitor, phải thấy output tương tự:

```
==============================
  Greenhouse IoT + CoreIoT
==============================
[WiFi] Connected! IP: 192.168.x.x
[MQTT] Connecting to CoreIoT... OK
[MQTT-LOCAL] Connecting to Mosquitto... OK
[TELE] Queued: T=28.5 H=72 Soil=45
========== GREENHOUSE STATUS ==========
  Time: 04/04/2026 08:30:00
  WiFi: OK  IP: 192.168.x.x
  Temp: 28.5°C  Humi: 72%
  Soil: 45%  Light: 1000lux (BRIGHT)
  PUMP: OFF  FAN: OFF  LED_GROW: OFF
  Mode: AUTO  PumpManual: N  Cooldown: N
=======================================
```

### Truy cập giao diện Web

Mở Serial Monitor để lấy địa chỉ IP, sau đó truy cập:

```
http://<IP: 192.168.x.x>
```

## 🌐 Giao diện Web

Giao diện được lưu trên **LittleFS** của ESP32 và phục vụ bởi ESPAsyncWebServer.

| Trang       | Đường dẫn                | Mô tả                         |
| ----------- | ------------------------ | ----------------------------- |
| Dashboard   | `/pages/dashboard.html`  | Tổng quan hệ thống            |
| Control     | `/pages/control.html`    | Điều khiển thiết bị thủ công  |
| Charts      | `/pages/charts.html`     | Biểu đồ lịch sử cảm biến      |
| Logs        | `/pages/logs.html`       | Nhật ký hoạt động             |
| Schedule    | `/pages/schedule.html`   | Lập lịch tự động              |
| Smart Logic | `/pages/smartlogic.html` | Quy tắc điều khiển thông minh |
| Threshold   | `/pages/threshold.html`  | Cài đặt ngưỡng cảnh báo       |
| Notify      | `/pages/notify.html`     | Quản lý thông báo             |
| Devices     | `/pages/devices.html`    | Danh sách thiết bị            |
| Account     | `/pages/account.html`    | Tài khoản & bảo mật           |



## 📡 Giao tiếp thời gian thực

Hệ thống sử dụng **WebSocket** để đẩy dữ liệu cảm biến và trạng thái thiết bị lên trình duyệt theo thời gian thực mà không cần reload trang.

```
ESP32  ──WebSocket──▶  Browser
       ◀──Command───   Browser
```

- File phía client: `data/js/websocket.js`
- Endpoint: `ws://<ESP32_IP>/ws`



## 🧠 Lập lịch & Smart Logic

### Scheduler

Cho phép đặt lịch bật/tắt thiết bị theo khung giờ cụ thể trong ngày (sử dụng RTC để đảm bảo chính xác ngay cả khi mất điện).

### Smart Logic (Threshold-based Automation)

Tự động điều khiển thiết bị khi giá trị cảm biến vượt ngưỡng đã cài đặt:

```
Nhiệt độ > 35°C  →  Bật quạt làm mát
Độ ẩm đất < 40%  →  Bật bơm tưới
Độ ẩm không khí > 80%  →  Bật thông gió
```



## 🔄 OTA Update

Hệ thống hỗ trợ cập nhật firmware không cần cắm cáp qua **ElegantOTA**:

1. Truy cập `http://<ESP32_IP>/update`
2. Chọn file `.bin` firmware mới
3. Upload và ESP32 tự động restart


## 📊 Sơ đồ khối

Xem sơ đồ khối hệ thống chi tiết tại file:

```
greenhouse_block_diagram.html
```

Mở bằng trình duyệt để xem kiến trúc kết nối phần cứng.



## 📄 Giấy phép

Dự án được phân phối theo giấy phép **MIT**. Xem file `LICENSE` để biết thêm chi tiết.

---

<div align="center">
  Made with ❤️ for Smart Agriculture
</div>
