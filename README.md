# 🌿 ESP32-S3 Smart Greenhouse & TinyML Edge AI System

<div align="center">

![ESP32-S3](https://img.shields.io/badge/Board-Yolo%3AUNO%20(ESP32--S3)-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![TinyML](https://img.shields.io/badge/AI-TinyML%20On--Device-FF6F00?style=for-the-badge&logo=tensorflow&logoColor=white)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20Dual--Core-00979D?style=for-the-badge&logo=cplusplus&logoColor=white)
![CoreIOT](https://img.shields.io/badge/Cloud-CoreIOT%20MQTT-0052CC?style=for-the-badge)
![LittleFS](https://img.shields.io/badge/Web%20Server-LittleFS%20Async-4A154B?style=for-the-badge)

**ĐỒ ÁN MÔN HỌC INTERNET OF THINGS (CO3061 / CO3062)**  
**TRƯỜNG ĐẠI HỌC BÁCH KHOA - ĐHQG TP.HCM (HCMUT)**  
**KHOA KHOA HỌC VÀ KỸ THUẬT MÁY TÍNH**

**Giảng viên hướng dẫn:** TS. Lê Trọng Nhân  
**Sinh viên thực hiện:** Phạm Công Võ — **MSSV:** 2313946  
**GitHub ID:** `vophamk23`

---

</div>

Hệ thống **Nhà kính thông minh (Smart Greenhouse)** hoàn chỉnh chạy trên phần cứng **Yolo:UNO (ESP32-S3)**, kết hợp **Trí tuệ nhân tạo biên (Edge AI / TinyML)** suy luận on-chip phát hiện dị thường cảm biến với độ trễ micro-giây, máy chủ Web Server nhúng thời gian thực (WebSocket + LittleFS), và đồng bộ hai chiều lên nền tảng đám mây **CoreIOT Cloud**.

---

## 📋 Mục lục

1. [Tính năng nổi bật](#1-tính-năng-nổi-bật)
2. [Kiến trúc hệ thống (Dual-Core & TinyML Pipeline)](#2-kiến-trúc-hệ-thống)
3. [Mô hình Trí tuệ Nhân tạo Biên (TinyML Edge AI)](#3-mô-hình-trí-tuệ-nhân-tạo-biên-tinyml-edge-ai)
4. [Sơ đồ kết nối phần cứng (Pinout Yolo:UNO)](#4-sơ-đồ-kết-nối-phần-cứng-pinout-yolouno)
5. [Cấu trúc mã nguồn (Clean Architecture)](#5-cấu-trúc-mã-nguồn)
6. [Hệ điều phối Cooperative Scheduler (Core 1)](#6-hệ-điều-phối-cooperative-scheduler)
7. [Giao tiếp Đám mây CoreIOT & Web Server Nhúng](#7-giao-tiếp-đám-mây-coreiot--web-server-nhúng)
8. [Hướng dẫn cài đặt & Nạp chương trình](#8-hướng-dẫn-cài-đặt--nạp-chương-trình)
9. [Lệnh điều khiển Serial & Giao diện LCD 16x2](#9-lệnh-điều-khiển-serial--giao-diện-lcd-16x2)
10. [Bản quyền & Giấy phép](#10-bản-quyền--giấy-phép)

---

## 1. Tính năng nổi bật

* **TinyML On-Device Neural Network:** Mạng nơ-ron nhân tạo MLP 2 tầng (Dense 8 ReLU $\to$ Dense 1 Sigmoid) được huấn luyện từ 10.000 mẫu vi khí hậu trên Google Colab, biên dịch thành mã suy luận Forward Pass chạy trực tiếp on-chip với độ trễ cực thấp **$< 2\ \mu\text{s}$**, tốn **0 KB Tensor Arena RAM**.
* **Phát hiện dị thường & Tấn công nhiệt (Sensor Anomaly Detection):** Nhận diện tức thời các kịch bản hơ lửa phá hoại cảm biến (`FLAME_TAMPER`), trôi số liệu (`RAPID_DRIFT`), quá nhiệt cực đoan (`HIGH_HEAT`), rét hại (`FREEZING`), khô hạn (`EXTREME_DRY`), ngập úng (`EXTREME_HUMID`).
* **Kiến trúc 2 nhân (Dual-Core Separation):** Phân tách độc lập tuyệt đối giữa tác vụ mạng/cloud/web (Core 0 - FreeRTOS Tasks) và tác vụ điều khiển vi khí hậu thời gian thực (Core 1 - Deterministic Cooperative Scheduler).
* **Đồng bộ Đám mây CoreIOT (app.coreiot.io):** Giao tiếp hai chiều qua giao thức MQTT: đẩy chuỗi Telemetry vi khí hậu & số liệu AI; nhận lệnh điều khiển RPC bật/tắt bơm, quạt, đổi chế độ Auto/Manual từ Dashboard Cloud.
* **Web Server nhúng hoàn chỉnh (LittleFS):** Phục vụ giao diện Web trực quan (Dashboard, Điều khiển thiết bị, Biểu đồ lịch sử, Lập lịch tưới nước, Cài đặt ngưỡng cảnh báo, Xem nhật ký Logs) lưu trữ trực tiếp trên bộ nhớ Flash 8MB của ESP32-S3, cập nhật Real-time qua WebSocket không cần reload trang.
* **An toàn & Bền bỉ:** Giám sát máy bơm thông minh (Pump Watchdog 3 lớp chống cháy bơm), vùng trễ nhiệt ẩm (Hysteresis), bảo vệ truy cập cảm biến bằng FreeRTOS Mutex (`SENSOR_LOCK`).
* **Hỗ trợ cập nhật OTA không dây (ElegantOTA):** Nâng cấp firmware hoặc giao diện Web qua trình duyệt tại endpoint `/update`.

---

## 2. Kiến trúc hệ thống

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 (Yolo:UNO - 240MHz)                             │
│                                                                                  │
│   ┌─────────────────────────────────────┐  ┌─────────────────────────────────┐   │
│   │           CORE 0 (FreeRTOS)         │  │    CORE 1 (Cooperative Sched)   │   │
│   │       I/O Mạng, Web & MQTT          │  │       Cảm biến & TinyML AI      │   │
│   │                                     │  │                                 │   │
│   │  ┌───────────────────────────────┐  │  │  ┌───────────────────────────┐  │   │
│   │  │ greenhouse_wifi_task (Prio 3) │  │  │  │ Task_ReadAirSensors (2s)  │  │   │
│   │  │  - Giữ kết nối WiFi & Auto-rec│  │  │  │ Task_ReadSoilSensor (5s) │  │   │
│   │  └───────────────────────────────┘  │  │  │ Task_TinyML_Inference (3s)│  │   │
│   │  ┌───────────────────────────────┐  │  │  │ Task_CheckAlerts (1s)     │  │   │
│   │  │ greenhouse_coreiot (Prio 2)   │  │  │  │ Task_AutoControl (1s)     │  │   │
│   │  │  - CoreIOT MQTT Telemetry/RPC │  │  │  │ Task_PumpWatchdog (1s)    │  │   │
│   │  └───────────────────────────────┘  │  │  │ Task_UpdateLEDStatus (0.5s│  │   │
│   │  ┌───────────────────────────────┐  │  │  │ Task_UpdateLCD (0.5s)     │  │   │
│   │  │ greenhouse_webserver (Prio 1) │  │  │  │ Task_AutoSwitchLCD (5s)   │  │   │
│   │  │  - ESPAsyncWebServer          │  │  │  │ Task_SendTelemetry (3s)   │  │   │
│   │  │  - WebSocket /ws Broadcast    │  │  │  │ Task_ProcessRpc (200ms)   │  │   │
│   │  │  - LittleFS Static UI Files   │  │  │  │ Task_CheckSchedule (10s)  │  │   │
│   │  └───────────────────────────────┘  │  │  └───────────────────────────┘  │   │
│   │                                     │  │                 ▲               │   │
│   └──────────────────┬──────────────────┘  └─────────────────┼───────────────┘   │
│                      │                                       │                   │
│                      └───► FreeRTOS Queue & Mutex ───────────┘                   │
│                            (xTelemetryQueue / xRpcQueue)                         │
└──────────────────────────────────────┬───────────────────────────────────────────┘
                                       │
            ┌──────────────────────────┴──────────────────────────┐
            ▼                                                     ▼
   [ Trình duyệt / Mobile ]                             [ CoreIOT Cloud Platform ]
   http://192.168.x.x (LittleFS)                        mqtt://app.coreiot.io:1883
```

---

## 3. Mô hình Trí tuệ Nhân tạo Biên (TinyML Edge AI)

### Quy trình huấn luyện & Triển khai (MLOps on Embedded)

```
[1. Tạo Dataset 10.000 mẫu] ──► [2. Keras MLP 2 tầng] ──► [3. TensorFlow Lite] ──► [4. Header C++ FlatBuffer]
   tools/train_tinyml/             Dense 8 ReLU              Quantized FlatBuffer     include/dht_anomaly_model.h
   train_dht_anomaly.ipynb         Dense 1 Sigmoid           (1.836 bytes)            Trọng số W1, b1, W2, b2
                                                                                             │
                                                                                             ▼
                                                                             [5. Forward Pass Trên ESP32-S3]
                                                                                tinyml_engine.cpp (< 2 us)
```

### Kiến trúc mạng Nơ-ron (Forward Pass Equation)
* **Tầng 1 (Hidden Layer - 8 nơ-ron):**
  $$z_i = \text{ReLU}\left(w_{1,i}^{temp} \cdot T + w_{1,i}^{hum} \cdot H + b_{1,i}\right) \quad (i = 0 \dots 7)$$
* **Tầng 2 (Output Layer - 1 nơ-ron):**
  $$\text{Anomaly Score} = \sigma\left(\sum_{i=0}^7 w_{2,i} \cdot z_i + b_2\right) = \frac{1}{1 + e^{-\left(\sum w_{2,i} z_i + b_2\right)}}$$

### Bảng đối chiếu nhận diện dị thường

| Kịch bản thực tế | Nhiệt độ $T$ | Độ ẩm $RH$ | $\Delta T/\Delta t$ | $\Delta RH/\Delta t$ | AI Score | Nhãn trạng thái | Hành vi hệ thống |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **Bình thường (Lý tưởng)** | 25°C | 60% | $0.00^\circ\text{C}/\text{s}$ | $0.00\%/\text{s}$ | `0.0091` | `NORMAL` | LED Xanh lá, quạt/bơm chạy tự động theo ẩm đất. |
| **Hơ lửa phá hoại cảm biến** | 45°C | 20% | $> +0.25^\circ\text{C}/\text{s}$ | $< -0.30\%/\text{s}$ | `1.0000` | `FLAME_TAMPER` | LED Đỏ chớp khẩn cấp, bật quạt làm mát tức thời. |
| **Sương giá / Lạnh đột ngột** | 5°C | 50% | Bình thường | Bình thường | `0.9194` | `FREEZING_ALERT` | LED Cam, kích hoạt cảnh báo nhà kính. |
| **Trôi cảm biến / Kẹt giá trị** | Bất thường | Bất thường | $> 0.35^\circ\text{C}/\text{s}$ | $> 0.80\%/\text{s}$ | `0.7500` | `RAPID_DRIFT` | LED Vàng, gắn nhãn nghi ngờ số liệu trôi nhanh. |
| **Mất kết nối cảm biến I2C** | NaN | NaN | — | — | `1.0000` | `SENSOR_FAULT` | Báo lỗi I2C trên LCD và Web Dashboard. |

---

## 4. Sơ đồ kết nối phần cứng (Pinout Yolo:UNO)

Kit sử dụng chuẩn chân cắm **Yolo:UNO (ESP32-S3)** được giảng dạy tại ĐH Bách Khoa TP.HCM:

| Thiết bị / Module ngoại vi | Chân Yolo:UNO (ESP32-S3) | Chuẩn giao tiếp | Ghi chú vận hành |
| :--- | :---: | :---: | :--- |
| **Cảm biến DHT20** | **SDA: GPIO 11, SCL: GPIO 12** | I2C (0x38) | Cắm Cổng Đỏ Grove I2C, đo nhiệt độ & độ ẩm |
| **Màn hình LCD 1602** | **SDA: GPIO 11, SCL: GPIO 12** | I2C (0x21 / 0x27) | Hiển thị 4 trang thông tin vi khí hậu & Edge AI |
| **Đồng hồ RTC DS3231** | **SDA: GPIO 11, SCL: GPIO 12** | I2C (0x68) | Đồng bộ giờ thực tế cho lịch hẹn tưới nước |
| **Cảm biến độ ẩm đất** | **GPIO 1** | Analog (ADC1 kênh 0) | Đo điện áp biến thiên theo độ ẩm đất (0 - 100%) |
| **Cảm biến ánh sáng LDR** | **GPIO 2** | Analog (ADC1 kênh 1) | Đo cường độ quang thông (0 - 1000 Lux) |
| **Rơ-le Máy bơm (Pump)** | **GPIO 4 (Chân D4)** | Digital Output | Kích mức THẤP (Active-LOW), bảo vệ quá tải |
| **Quạt làm mát (Cooling Fan)** | **GPIO 5 (Chân D5)** | Digital Output | Điều khiển qua transistor/MOSFET tản nhiệt |
| **LED RGB NeoPixel WS2812B** | **GPIO 45** | Single-Wire Digital | 1 hạt LED on-board đổi màu báo trạng thái AI |
| **LED đơn on-board** | **GPIO 48** | Digital Output | Đèn nháy trạng thái hệ thống |
| **Nút nhấn Mode (Auto/Manual)**| **GPIO 8 (Chân D8)** | Input Pullup | Nhấn để chuyển đổi chế độ vận hành |
| **Nút nhấn Bơm thủ công** | **GPIO 9 (Chân D9)** | Input Pullup | Bật/tắt bơm khi ở chế độ Manual |
| **Nút nhấn Quạt thủ công** | **GPIO 10 (Chân D10)** | Input Pullup | Bật/tắt quạt khi ở chế độ Manual |

---

## 5. Cấu trúc mã nguồn

```
ESP32-Smart-Greenhouse-Embedded-Web-Server/
├── include/                           # Header files định nghĩa giao diện modul
│   ├── config.h                       # Cấu hình pinout, ngưỡng mặc định, I2C address
│   ├── dht_anomaly_model.h            # Mảng FlatBuffer C++ mô hình TinyML (1.836 bytes)
│   ├── tinyml_engine.h                # Khai báo bộ suy luận AI on-chip & Heat Index
│   ├── global_vars.h                  # Cấu trúc dữ liệu toàn cục, LED color, sensor data
│   ├── iot_bridge.h                   # Cầu nối thread-safe giữa Core 0 và Core 1
│   ├── scheduler.h                    # Cooperative Scheduler Timer ISR & Task Queue
│   ├── scheduler_tasks.h              # Định nghĩa 14 tác vụ định kỳ trên Core 1
│   ├── sensor_handler.h               # Driver đọc I2C DHT20, ADC Soil, ADC Light
│   ├── actuator_handler.h             # Driver điều khiển Relay, Quạt, NeoPixel
│   ├── lcd_display.h                  # Driver điều khiển LCD 16x2 với 4 trang hiển thị
│   ├── coreiot_client.h               # Client MQTT kết nối đám mây CoreIOT Cloud
│   ├── web_server.h                   # ESPAsyncWebServer, REST API, WebSocket handler
│   ├── wifi_manager.h                 # Giám sát kết nối WiFi
│   └── config_manager.h               # Đọc/ghi cấu hình ngưỡng và lịch trên LittleFS
│
├── src/                               # Hiện thực mã nguồn C++
│   ├── main.cpp                       # Điểm khởi động setup() & loop() 2 nhân
│   ├── tinyml_engine.cpp              # Hiện thực Forward Pass MLP & Anomaly Detection
│   ├── global_vars.cpp                # Cấp phát biến toàn cục
│   ├── iot_bridge.cpp                 # Quản lý FreeRTOS Queue & SENSOR_LOCK Mutex
│   ├── scheduler.cpp                  # Bộ điều phối Cooperative Scheduler Core 1
│   ├── scheduler_tasks.cpp            # Logic 14 tác vụ định thời thời gian thực
│   ├── sensor_handler.cpp             # Lấy mẫu cảm biến và làm mịn số liệu
│   ├── actuator_handler.cpp           # Điều khiển rơ-le, quạt và phối màu NeoPixel
│   ├── lcd_display.cpp                # Render 4 trang thông tin vi khí hậu & Edge AI
│   ├── coreiot_client.cpp             # Đóng gói JSON Telemetry & giải mã lệnh RPC
│   ├── web_server.cpp                 # Xử lý HTTP requests, WebSocket client broadcast
│   ├── wifi_manager.cpp               # FreeRTOS WiFi keep-alive task
│   ├── button_handler.cpp             # Quét nút bấm khử rung (Debounce)
│   ├── config_manager.cpp             # Quản lý file JSON trên LittleFS
│   └── ntp_sync.cpp                   # Đồng bộ giờ mạng NTP
│
├── tools/train_tinyml/                # Pipeline đào tạo Trí tuệ Nhân tạo
│   ├── train_dht_anomaly.ipynb        # Jupyter Notebook huấn luyện trên Google Colab
│   ├── train_dht_anomaly.py           # Script Python sinh dữ liệu & xuất file C++
│   └── dht_anomaly_model.h            # Bản sao lưu file trọng số mô hình
│
├── data/                              # Bộ nhớ flash LittleFS (Giao diện Web)
│   ├── index.html                     # Giao diện đơn trang Dashboard chính
│   ├── info.dat                       # Lưu trữ thông tin kết nối WiFi & CoreIOT Token
│   ├── pages/                         # Các trang: Dashboard, Charts, Control, Schedule...
│   ├── js/                            # Module JavaScript: websocket.js, ui.js, charts.js...
│   └── css/                           # Giao diện responsive tùy biến hiện đại
│
└── platformio.ini                     # Cấu hình môi trường PlatformIO (board: yolo_uno)
```

---

## 6. Hệ điều phối Cooperative Scheduler

Trên **Core 1**, toàn bộ các tác vụ định thời được điều phối tuần tự và xác định qua bộ lập lịch **Cooperative Scheduler** (chu kỳ ngắt timer 10ms):

| STT | Tác vụ (Task Name) | Chu kỳ | Chức năng chi tiết |
| :---: | :--- | :---: | :--- |
| **1** | `Task_ReadAirSensors` | **2 giây** | Đọc nhiệt độ, độ ẩm qua I2C từ DHT20 và cường độ sáng từ LDR |
| **2** | `Task_ReadSoilSensor` | **5 giây** | Đọc độ ẩm đất từ ADC qua trung bình mẫu (Averaging filter) |
| **3** | `Task_TinyML_Inference` | **3 giây** | **Chạy suy luận Forward Pass AI**, tính Anomaly Score và phân loại trạng thái |
| **4** | `Task_CheckAlerts` | **1 giây** | So sánh thông số với các ngưỡng báo động an toàn |
| **5** | `Task_AutoControl` | **1 giây** | Tự động kích hoạt bơm khi đất khô và bật quạt khi quá nhiệt |
| **6** | `Task_PumpWatchdog` | **1 giây** | Giám sát an toàn: ngắt cưỡng bức nếu bơm quá 2 phút, đảm bảo thời gian nghỉ cooldown |
| **7** | `Task_UpdateLEDStatus` | **500 ms** | Đổi màu LED NeoPixel WS2812 theo trạng thái: Xanh lá, Cam, Đỏ, Vàng |
| **8** | `Task_UpdateLCD` | **500 ms** | Cập nhật dữ liệu lên trang LCD hiện tại |
| **9** | `Task_AutoSwitchLCDPage` | **5 giây** | Tự động lật lần lượt giữa 4 trang LCD |
| **10** | `Task_Heartbeat` | **5 giây** | In snapshot trạng thái toàn diện ra Serial Monitor |
| **11** | `Task_ScanButtons` | **50 ms** | Quét trạng thái các nút bấm vật lý với thuật toán chống dội phím |
| **12** | `Task_SendTelemetry` | **3 giây** | Chụp dữ liệu (Snapshot) an toàn và đẩy vào Queue sang Core 0 |
| **13** | `Task_ProcessRpc` | **200 ms** | Quét và thực thi ngay lập tức các lệnh điều khiển nhận từ Cloud |
| **14** | `Task_CheckSchedule` | **10 giây** | So khớp thời gian thực RTC DS3231 với lịch tưới hẹn trước |

---

## 7. Giao tiếp Đám mây CoreIOT & Web Server Nhúng

### 1. Đồng bộ Đám mây CoreIOT Platform (app.coreiot.io)
Dữ liệu Telemetry được đóng gói JSON chuẩn và đẩy định kỳ lên topic MQTT:
```json
{
  "temperature": 28.5,
  "humidity": 65.2,
  "soil": 55.0,
  "light": 720.0,
  "pump": false,
  "fan": false,
  "mode": "AUTO",
  "ai_state": "COMFORTABLE",
  "ai_confidence": 98.5,
  "ai_latency_us": 2,
  "anomaly_score": 0.01,
  "anomaly_pred": false,
  "anomaly_type": "NORMAL",
  "online_accuracy": 99.1
}
```

### 2. Giao diện Web nội bộ qua WebSocket (`ws://<IP>/ws`)
* Không cần kết nối Internet, chỉ cần kết nối cùng mạng WiFi với ESP32-S3.
* Cập nhật tức thời thông số cảm biến, tình trạng thiết bị và **Thẻ Cảnh Báo Dị Thường AI (Anomaly Alert Badge)** theo thời gian thực.

---

## 8. Hướng dẫn cài đặt & Nạp chương trình

### Yêu cầu môi trường
* Cài đặt **Visual Studio Code** với tiện ích mở rộng **PlatformIO IDE**.
* Cáp USB Type-C cắm vào cổng **UART** hoặc **USB** của kit Yolo:UNO.

### Các bước thực hiện:

1. **Clone mã nguồn dự án về máy:**
   ```bash
   git clone https://github.com/vophamk23/ESP32-Smart-Greenhouse-Embedded-Web-Server.git
   cd ESP32-Smart-Greenhouse-Embedded-Web-Server
   ```

2. **Cấu hình WiFi & Cloud trong file `data/info.dat`:**
   ```json
   {
     "WIFI_SSID": "Ten_WiFi_Cua_Ban",
     "WIFI_PASS": "Mat_Khau_WiFi",
     "CORE_IOT_TOKEN": "Token_Thiet_Bi_CoreIOT",
     "CORE_IOT_SERVER": "app.coreiot.io",
     "CORE_IOT_PORT": "1883"
   }
   ```

3. **Nạp giao diện Web lên phân vùng Flash LittleFS:**
   ```powershell
   pio run -t uploadfs
   ```

4. **Biên dịch và nạp Firmware:**
   ```powershell
   pio run -t upload
   ```

5. **Mở Serial Monitor kiểm tra kết nối:**
   ```powershell
   pio device monitor --baud 115200
   ```

---

## 9. Lệnh điều khiển Serial & Giao diện LCD 16x2

### Danh sách phím bấm điều khiển qua Serial (115200 baud)

| Phím bấm | Hành động tương ứng |
| :---: | :--- |
| `A` / `a` | Chuyển đổi qua lại giữa chế độ **AUTO** và **MANUAL** |
| `P` / `p` | Bật / Tắt máy bơm nước thủ công (khi ở chế độ MANUAL) |
| `F` / `f` | Bật / Tắt quạt làm mát thủ công (khi ở chế độ MANUAL) |
| `N` / `n` | Chuyển ngay sang trang tiếp theo trên màn hình LCD |
| `S` / `s` | In tức thì ảnh chụp toàn bộ dữ liệu cảm biến & AI ra Serial |
| `I` / `i` | Hiển thị địa chỉ IP, trạng thái WiFi và kết nối CoreIOT Cloud |
| `?` | Hiển thị menu trợ giúp các lệnh điều khiển |

### 4 Trang thông tin hiển thị trên màn hình LCD 16x2
* **Trang 0 (Nhiệt độ & Độ ẩm không khí):**
  ```
  Temp: 28.5 C
  Humi: 65.2 %
  ```
* **Trang 1 (Độ ẩm đất & Cường độ ánh sáng):**
  ```
  Soil: 55.0 %
  Light: 720 Lux
  ```
* **Trang 2 (Trạng thái thiết bị ngoại vi):**
  ```
  PUMP:OFF FAN:OFF
  Mode:AUTO
  ```
* **Trang 3 (Trí tuệ Nhân tạo Biên TinyML & Latency):**
  ```
  AI:COMFORTABLE  
  C:98% Lat:  2u
  ```

---

## 10. Bản quyền & Giấy phép

Dự án được xây dựng và phát hành theo giấy phép mã nguồn mở **MIT License**. Mọi quyền được bảo lưu cho mục đích học tập và nghiên cứu tại **Trường Đại học Bách Khoa - ĐHQG TP.HCM (HCMUT)**.

<div align="center">
  <b>Tác giả: Phạm Công Võ (MSSV: 2313946) — Khoa KH&KT Máy tính, HCMUT</b><br/>
  <i>Đề tài Đồ án môn học Internet of Things (CO3061 / CO3062) — GVHD: TS. Lê Trọng Nhân</i>
</div>
