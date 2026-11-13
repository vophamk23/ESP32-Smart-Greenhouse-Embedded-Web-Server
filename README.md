# Cleanroom Bio-Chamber Edge-AI & Industrial IoT System (Edge–Fog–Cloud)

![Board](https://img.shields.io/badge/MCU-Yolo%3AUNO%20(ESP32--S3)-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![TinyML](https://img.shields.io/badge/Edge%20AI-C%2B%2B%20TinyML%20%2B%20Fog%20AI-FF6F00?style=for-the-badge&logo=tensorflow&logoColor=white)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20Dual--Core%20AMP-00979D?style=for-the-badge&logo=cplusplus&logoColor=white)
![Modbus](https://img.shields.io/badge/Protocol-Modbus%20RS485%20%7C%20MQTT-FFD700?style=for-the-badge)
![Web](https://img.shields.io/badge/Web%20SPA-ESPAsyncWebServer%20%2B%20LittleFS-4A154B?style=for-the-badge)
![Gateway](https://img.shields.io/badge/Fog%20SCADA-Python%20Tkinter%20%2B%20OLS%20AI-3776AB?style=for-the-badge&logo=python&logoColor=white)
![Cloud](https://img.shields.io/badge/Cloud-CoreIOT%20(ThingsBoard)-0052CC?style=for-the-badge)

Hệ thống IoT công nghiệp 3 tầng (**Edge – Fog – Cloud**) tích hợp **Trí tuệ nhân tạo tại biên (TinyML C++ On-Chip)** và **trạm Fog Gateway đa luồng (BIO-SCADA)**, thiết kế cho bài toán **giám sát và điều khiển buồng sạch nuôi cấy vi tảo / nấm công nghiệp (*Cleanroom Bio-Chamber*)** trên bo mạch **Yolo:UNO (`ESP32-S3`)**.



## Mục lục
- [1. Ngăn xếp công nghệ (Technology Stack)](#1-ngăn-xếp-công-nghệ-technology-stack)
- [2. Kiến trúc hệ thống 3 tầng](#2-kiến-trúc-hệ-thống-3-tầng)
- [3. Trí tuệ nhân tạo hai tầng (Dual-Tier Edge & Fog AI)](#3-trí-tuệ-nhân-tạo-hai-tầng-dual-tier-edge--fog-ai)
- [4. Quy hoạch phần cứng, Flash & Logic bảo vệ](#4-quy-hoạch-phần-cứng-flash--logic-bảo-vệ)
- [5. Lập lịch đa nhiệm Dual-Core AMP (`Core 0` & `Core 1`)](#5-lập-lịch-đa-nhiệm-dual-core-amp-core-0--core-1)
- [6. Giao tiếp API, WebSocket & CoreIOT MQTT](#6-giao-tiếp-api-websocket--coreiot-mqtt)
- [7. Cấu trúc mã nguồn](#7-cấu-trúc-mã-nguồn)
- [8. Hướng dẫn cài đặt & vận hành](#8-hướng-dẫn-cài-đặt--vận-hành)


## 1. Ngăn xếp công nghệ (Technology Stack)

| Phân hệ | Công nghệ / Thư viện | Vai trò & Điểm nhấn kỹ thuật |
| :--- | :--- | :--- |
| **Vi điều khiển & Toolchain** | **Yolo:UNO (`ESP32-S3`)** Dual-Core Xtensa LX7 @ `240 MHz`, `320 KB` SRAM, `8 MB` Flash; **PlatformIO** (`espressif32`, Arduino Framework) | Nút biên xử lý song song 2 nhân, USB-CDC (`115200 bps`), hỗ trợ build dự phòng cho `esp32dev`. |
| **Hệ điều hành & Lập lịch** | **FreeRTOS** (Preemptive) + **Cooperative Scheduler** (`10 ms` Hardware Timer ISR) | Cô lập hoàn toàn tác vụ mạng (`Core 0`) khỏi vòng lặp cảm biến/điều khiển tất định (`Core 1`). |
| **Đồng bộ liên nhân (IPC)** | `xQueue` (`xTelemetryQueue`, `xRpcCommandQueue`), `SemaphoreHandle_t` (`g_sensorMutex`), `EventGroup` (`xWiFiEventGroup`) | Trao đổi snapshot dữ liệu & hàng đợi lệnh RPC không chặn, an toàn đa luồng giữa 2 nhân. |
| **Cảm biến & Bus công nghiệp** | **Modbus RTU RS485** (`ModbusMaster@^2.0.1` trên UART1 `9600 bps`), **I2C Bus** (`DHT20`, `RTClib DS3231`, `LCD 1602`), **ADC1 12-bit + Lọc EMA** ($\alpha = 0{,}2$) | Ưu tiên đầu dò RS485 công nghiệp, tự động **Fail-Safe sang `DHT20`** khi đứt cáp/timeout; chọn **ADC1** để không xung đột Wi-Fi; đồng bộ giờ **NTP $\leftrightarrow$ DS3231 RTC**. |
| **Edge AI / TinyML** | **TensorFlow / Keras** $\rightarrow$ **TFLite FlatBuffer** (`dht_anomaly_model.h`, `1.836 bytes`) + **C++32-bit Static Forward Pass** + **NOAA Heat Index** + **Decision Forest (4 cây)** | Suy luận MLP `2 → 8 (ReLU) → 1 (Sigmoid)` cấp phát tĩnh (`malloc`-free) ngay trên ESP32-S3 với độ trễ **`~50 µs`**, kết hợp đạo hàm vật lý ($\Delta T/\Delta t, \Delta RH/\Delta t$). |
| **Embedded Web Server & OTA** | **ESPAsyncWebServer**, **AsyncTCP**, **LittleFS** (`~3,94 MB`), **ArduinoJson 6**, **ElegantOTA** (`3.1.7`) | Giao diện **BioChamber Orbit SPA** (`10` trang HTML, `13` module JS ES6, **Chart.js 4.4**, **OpenWeather API**, xuất `biochamber_logs.csv`), WebSocket `1 Hz` và nạp Dual-OTA (`app0`/`app1`) tại `/update`. |
| **Tầng Fog Gateway & AI** | **Python 3** đa luồng (`threading`), `paho-mqtt`, `pyserial`, `requests`, `NumPy`, `minimalmodbus` (tùy chọn), **Tkinter Dark Mode (`BIO-SCADA`)** | Thu thập kép **RS485 / USB-CDC Serial / Wi-Fi HTTP REST / Simulation**, chạy **Z-Score + Inverse Coupling Anomaly**, hồi quy **OLS (`np.polyfit`)** dự báo `+30 phút`, khuyến nghị **Smart Mist AI**. |
| **Tầng Cloud** | **CoreIOT (ThingsBoard Platform)** qua **MQTT v3.1.1** (`PubSubClient` / `paho-mqtt`, TCP `1883`) | Xác thực Access Token, lưu Telemetry dài hạn, **RuleChain** phát Alarm tự động và **Two-Way RPC** điều khiển từ xa. |


## 2. Kiến trúc hệ thống 3 tầng

```text
+-----------------------------------------------------------------------------------+
| TẦNG CLOUD — CoreIOT ThingsBoard Platform (app.coreiot.io:1883)                   |
|  • Telemetry & Attributes: Lưu chuỗi thời gian vi khí hậu + chỉ số AI 2 tầng      |
|  • RuleChain & Alarm: Cảnh báo quá nhiệt/dị thường/khô giá thể & RPC tự động      |
+-----------------------------------------+-----------------------------------------+
                                          ^  MQTT v3.1.1 (Telemetry, Attr, RPC)
                                          |
+-----------------------------------------+-----------------------------------------+
| TẦNG FOG GATEWAY — Python Multithreaded BIO-SCADA (gateway/)                      |
|  • Multi-Protocol Polling: Modbus RS485 Direct | USB-CDC Serial ('j\n') | HTTP    |
|  • GatewayAIEngine: Sliding Window (20 mẫu) Z-Score + OLS Forecast + Smart Mist AI|
|  • BIO-SCADA GUI (Tkinter Dark Theme): Vector Sparklines, LED Threat Bar, RPC     |
+-----------------------------------------+-----------------------------------------+
                                          ^  USB-CDC (115200 bps) / HTTP REST / RS485
                                          |
+-----------------------------------------+-----------------------------------------+
| TẦNG EDGE NODE — Yolo:UNO ESP32-S3 (FreeRTOS Dual-Core AMP)                       |
|  • Core 0 (Network — FreeRTOS Preemptive):                                        |
|      wifi_task (AP+STA + NTP) | coreiot_task (MQTT) | webserver_task (WS+OTA)     |
|  • Core 1 (Real-Time — Cooperative Scheduler 10 ms tick, 14 tác vụ):              |
|      Modbus/DHT20 + EMA → TinyML On-Chip (~50 µs) → Hysteresis + Watchdog 120s    |
+-----------------------------------------------------------------------------------+
```

> Xem trực quan kiến trúc chân, luồng tín hiệu và scheduler tại sơ đồ tương tác: [`biochamber_block_diagram.html`](biochamber_block_diagram.html).



## 3. Trí tuệ nhân tạo hai tầng (Dual-Tier Edge & Fog AI)

### 3.1. Tầng Edge AI — C++ TinyML On-Chip (`src/tinyml_engine.cpp`)
1. **Pipeline huấn luyện (`tools/train_tinyml/train_dht_anomaly.py`):**
   - Sinh **10.000 mẫu** nhiệt độ – độ ẩm ($55\%$ bình thường trong vùng vận hành buồng sạch, $45\%$ bất thường chia đều 4 kịch bản: quá nhiệt/hơ lửa, rét lạnh, khô buồng sạch, ngưng tụ ẩm cực đoan; chia `80% train / 20% test`).
   - Huấn luyện mạng **Keras Sequential MLP (`2 → 8 [ReLU] → 1 [Sigmoid]`)** bằng `Adam (lr=0.005)`, loss `binary_crossentropy`, `40 epochs`, `batch_size=32`.
   - Lượng tử & đóng gói qua `TFLiteConverter` thành `dht_anomaly_model.tflite` (**`1.836 bytes`**) và xuất sang header C++ [`include/dht_anomaly_model.h`](include/dht_anomaly_model.h) cùng bộ trọng số tĩnh `s_w1[8][2]`, `s_b1[8]`, `s_w2[8]`, `s_b2`.
2. **Bộ suy luận hợp nhất thời gian thực (`TinyML_Predict()`, chu kỳ `3 s`):**
   - **MLP Forward Pass:** Tính điểm xác suất bất thường $\in [0{,}0;\; 1{,}0]$ (ngưỡng kích hoạt `>= 0.60`), độ trễ đo qua `micros()` đạt **`~50 µs`**, không cấp phát động `malloc`/`new`.
   - **Phân tích đạo hàm vật lý & 8 mã sự cố:** Theo dõi tốc độ biến thiên $\Delta T/\Delta t$, $\Delta RH/\Delta t$ để bắt kịp thời `SENSOR_FAULT`, `FLAME_TAMPER` ($\Delta T/\Delta t > 0{,}25^\circ\text{C/s}$ và $\Delta RH/\Delta t < -0{,}30\%/\text{s}$), `RAPID_DRIFT`, `HIGH_HEAT_ALERT`, `FREEZING_ALERT`, `EXTREME_DRY_ALERT`, `EXTREME_HUMID_ALERT`, `NORMAL`, đồng thời tự tính **Online Accuracy** đối chứng vật lý.
   - **Decision Forest (4 cây có trọng số):** Kết hợp nhiệt độ thực, **NOAA Rothfusz Heat Index** (`calculateHeatIndex`), giao thoa ẩm–nhiệt, xu hướng $\Delta T/\Delta t$ và cờ dị thường để phân 3 trạng thái (`COMFORTABLE`, `HEAT_WARNING`, `EXTREME_ALERT`) và phát tín hiệu can thiệp sớm (`shouldTriggerFan`, `shouldTriggerPump`) khi bật **AI Auto Control**.

### 3.2. Tầng Fog AI — Predictive Time-Series Engine (`gateway/gateway_ai.py`)
Chạy trong lớp `GatewayAIEngine(window_size=20)` trên trạm Gateway:
- **Multi-Variate Anomaly Score:** Tính Z-Score chuẩn hóa quanh baseline buồng sạch ($\mu_T = 27{,}5^\circ\text{C}, \sigma_T = 3{,}5^\circ\text{C};\; \mu_{RH} = 65{,}0\%, \sigma_{RH} = 12{,}0\%$) cộng hệ số ghép nghịch biến nhiệt–ẩm (`coupling_factor = 0.70` khi $dT/dt > 0{,}20$ và $dRH/dt < -0{,}25$); cảnh báo khi `anomaly_score >= 0.60` với 6 nhãn (`FLAME_HEAT_SPIKE`, `EXTREME_HEATWAVE`, `FROST_COLD_HAZARD`, `DROUGHT_STRESS`, `SENSOR_RAPID_DRIFT`, `NORMAL`).
- **Hồi quy ngoại suy nhiệt độ 30 phút tới:** Khớp đường xu hướng OLS (`np.polyfit`) trên cửa sổ trượt (`>= 5` mẫu), có giới hạn độ dốc chống nhiễu tức thời và ràng buộc biên vật lý buồng sạch.
- **Smart Mist AI Advisor:** Tính thời lượng phun sương khuyến nghị (`15–120 s`) dựa trên mức thiếu hụt độ ẩm giá thể (mục tiêu `60%`) nhân hệ số bù bay hơi theo nhiệt độ buồng nuôi.



## 4. Quy hoạch phần cứng, Flash & Logic bảo vệ

### 4.1. Sơ đồ chân GPIO (`Yolo:UNO ESP32-S3` — [`include/config.h`](include/config.h))

| Thành phần phần cứng | Ngữ nghĩa Bio-Chamber | Chân / Địa chỉ | Giao tiếp | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- |
| **Cảm biến RS485 (`MAX485`)** | Nhiệt độ & Độ ẩm không khí buồng sạch | `GPIO44` (RX1), `GPIO43` (TX1) | UART1 / Modbus RTU | Slave ID `4`, `9600 bps` 8N1, Reg `0` (`RH`) & `1` (`T`) |
| **Cảm biến `DHT20`** | Cảm biến vi khí hậu dự phòng Fail-Safe | `0x38` (`SDA=GPIO11`, `SCL=GPIO12`) | I2C Bus | Tự động chuyển sang DHT20 khi RS485 Timeout |
| **Cảm biến ẩm điện dung** | Độ ẩm phôi / giá thể (JSON: `soilMoisture`) | `GPIO1` (`A0`) | ADC1 12-bit | Trung bình `5` mẫu (`3900` khô – `2200` ướt) + EMA ($\alpha = 0{,}2$) |
| **Quang trở `LDR`** | Cường độ sáng chu kỳ sinh học (`Photoperiod`) | `GPIO2` (`A1`) | ADC1 12-bit | Trung bình `5` mẫu; ngưỡng sáng thấp `< 500 lux` |
| **`DS3231 RTC` & `LCD 1602`** | Đồng hồ thời gian thực & hiển thị tại chỗ | `0x68` (RTC), `0x21`/`0x27` (LCD) | I2C Bus | `4` trang LCD (`Vi khí hậu`, `Giá thể/Sáng`, `Trạng thái`, `Edge AI`), xoay vòng `5 s` |
| **Relay Bơm phun sương** | Bơm phun sương tạo ẩm buồng sạch | `GPIO7` (`D4`) | GPIO Digital (**Active-LOW**) | Watchdog tối đa `120 s`, khóa nghỉ `30 s` |
| **Quạt đối lưu buồng sạch** | Quạt đối lưu / giải nhiệt tuần hoàn | `GPIO8` (`D5`) | GPIO Digital | Hysteresis: Bật `>= 32°C`, Tắt `< 31°C` (+ AI trigger) |
| **NeoPixel WS2812B & LED** | Đèn chỉ thị trạng thái & Bio-LED | `GPIO45` (RGB), `GPIO48` (`D13`) | Single-Wire / GPIO | Chỉ thị nhiệt cực đoan `>= 36°C`, bơm chạy, lỗi cảm biến |
| **Cụm 3 nút nhấn vật lý** | `Mode` / `Mist Pump` / `Airflow Fan` | `GPIO17` (`D8`), `GPIO18` (`D9`), `GPIO21` (`D10`) | `INPUT_PULLUP` | Quét mỗi `50 ms`, chống dội phím |

### 4.2. Phân vùng Flash `8 MB` (`partitions.csv`) & Logic vận hành an toàn
- **Bảng phân vùng Dual-OTA [`partitions.csv`](partitions.csv) (`yolo_uno`, Flash `8 MB`):** `nvs` (`20 KB`), `otadata` (`8 KB`), **`app0` (`ota_0`, `2,0 MB`) + `app1` (`ota_1`, `2,0 MB`)** hỗ trợ cập nhật không dây cả Firmware lẫn hệ thống tập tin qua **ElegantOTA** (`/update`), và **`spiffs`** (mount **LittleFS**, `~3,94 MB` tại `0x410000`) cho Web SPA + `/info.dat` + `/threshold.dat` + `/schedules.dat`.
- **Bảng phân vùng dự phòng 4 MB [`partitions_4mb.csv`](partitions_4mb.csv) (`esp32dev`):** `app0` / `app1` (`~1,56 MB` / `0x190000` mỗi slot) và `spiffs` (`832 KB` / `0xD0000`, chứa đủ gói Web SPA).
- **Luật điều khiển & Fail-Safe (`src/actuator_handler.cpp`):**
  - **Bơm phun sương (Hysteresis):** Bật khi `soilMoisture < 30%`, ngắt khi `soilMoisture >= 65%` (mặc định). Tự động kích hoạt **Smart Mist Skip** hoãn phun khi độ ẩm buồng `RH > 85%` để tránh ngưng tụ nước/nhiễm tạp.
  - **Pump Watchdog & Cooldown:** Cưỡng bức ngắt nếu chạy liên tục vượt `120 s` (phòng tuột cảm biến/cạn nước) và ép nghỉ giải nhiệt `30 s` giữa các chu kỳ.
  - **Lịch phun tuần theo DS3231 RTC:** Hỗ trợ tối đa `8` khung giờ (`MAX_SCHEDULES = 8`) lưu bền vững trên LittleFS `/schedules.dat`, thời lượng mỗi phiên `1..120 s`.
  - **Điều khiển khẩn cấp từ AI:** Ngắt an toàn khi phát hiện `FLAME_TAMPER`; hỗ trợ lệnh nối tiếp gỡ lỗi qua Serial `115200 bps`: `'a'` (AUTO/MANUAL), `'p'` (bơm), `'f'` (quạt), `'n'` (trang LCD), `'s'` (in thông số), `'j'` (xuất 1 dòng JSON telemetry).



## 5. Lập lịch đa nhiệm Dual-Core AMP (`Core 0` & `Core 1`)

### Core 0 — FreeRTOS Preemptive Tasks
- **`wifi_task`** (`4 KB` stack, ưu tiên `3`): Duy trì Wi-Fi `AP+STA` (phát `BioChamber-AP` tại `192.168.4.1`), tự kết nối lại khi mất STA và đồng bộ giờ NTP.
- **`coreiot_task`** (`8 KB` stack, ưu tiên `2`): Kết nối MQTT tới `app.coreiot.io:1883`, gửi Attributes & Telemetry từ `xTelemetryQueue`, nhận lệnh RPC 2 chiều qua `xRpcCommandQueue`.
- **`webserver_task`** (`8 KB` stack, ưu tiên `1`): Chạy `ESPAsyncWebServer` (Port `80`), phục vụ file tĩnh từ LittleFS, đẩy WebSocket `/ws` mỗi `1 s` và xử lý vòng lặp `ElegantOTA`.

### Core 1 — Cooperative Scheduler (`10 ms` Timer Tick — 14 tác vụ)
| Tác vụ | Chu kỳ | Chức năng |
| :--- | :---: | :--- |
| `Task_ScanButtons` | `50 ms` | Quét cụm 3 nút nhấn `Mode` / `Mist Pump` / `Airflow Fan` và chống dội phím |
| `Task_ProcessRpc` | `200 ms` | Tiêu thụ hàng đợi lệnh `xRpcCommandQueue` từ Cloud / Web / Gateway / Serial |
| `Task_ReadAirSensors` | `500 ms` | Đọc RS485 Modbus RTU (Fail-safe sang DHT20 I2C) và cảm biến quang trở LDR |
| `Task_UpdateLEDStatus` | `500 ms` | Cập nhật màu NeoPixel theo cảnh báo nhiệt/ẩm, trạng thái bơm và AI |
| `Task_UpdateLCD` | `500 ms` | Cập nhật nội dung hiển thị hiện hành trên màn hình LCD `16x2` |
| `Task_ReadSoilSensor` | `1.000 ms` | Đọc độ ẩm giá thể ADC1 (`5` mẫu trung bình) kết hợp bộ lọc EMA |
| `Task_CheckAlerts` | `1.000 ms` | Kiểm tra ngưỡng cảnh báo nhiệt độ, ẩm độ buồng, giá thể và ánh sáng |
| `Task_AutoControl` | `1.000 ms` | Thực thi vòng lặp Hysteresis bơm/quạt, Smart Mist Skip và quyết định AI |
| `Task_PumpWatchdog` | `1.000 ms` | Giám sát thời gian chạy tối đa `120 s` và đếm lùi thời gian nghỉ `30 s` |
| `Task_TinyML_Inference` | `3.000 ms` | Chạy `TinyML_Predict()` (MLP + Heat Index + Decision Forest + $\Delta T/\Delta t$) |
| `Task_SendTelemetry` | `3.000 ms` | Đẩy gói `TelemetryPacket_t` qua `xTelemetryQueue` sang Core 0 |
| `Task_AutoSwitchLCDPage` | `5.000 ms` | Tự động xoay vòng 4 trang thông tin trên LCD `16x2` |
| `Task_CheckSchedule` | `5.000 ms` | Đối chiếu giờ thực tế từ DS3231 RTC để kích hoạt lịch phun sương tuần |
| `Task_Heartbeat` | `15.000 ms` | In nhật ký tình trạng Free Heap, mạng và AI lên Serial Monitor |



## 6. Giao tiếp API, WebSocket & CoreIOT MQTT

### 6.1. Embedded Web Server SPA (`src/web_server.cpp`)
| Giao thức / Method | Endpoint | Chức năng |
| :--- | :--- | :--- |
| **WebSocket** | `WS /ws` | Broadcast JSON `1 Hz` toàn bộ cảm biến, Relay, ngưỡng, cấu hình LED hex, kết quả AI, `wifiRssi`, `freeHeap`; nhận lệnh tức thời (`setPump`, `setFan`, `setMode`, `setAiMode`, `setScheduleEnabled`, `setSchedules`, `setThreshold`) |
| **REST** | `GET /api/status` | Trả về snapshot JSON hiện tại kèm kết quả TinyML (hỗ trợ HTTP polling cho Gateway) |
| **REST** | `POST /api/control` | Nhận lệnh JSON điều khiển từ Gateway/Browser đẩy vào `xRpcCommandQueue` |
| **REST** | `GET /api/threshold` | Đọc cấu hình `soilDry`, `soilWet`, `tempHigh`, `tempLow`, `pumpMax`, `pumpCool` từ `/threshold.dat` |
| **REST** | `GET /api/schedules` | Đọc trạng thái và danh sách tối đa `8` lịch phun sương từ `/schedules.dat` |
| **OTA** | `GET \| POST /update` | Giao diện **ElegantOTA** nạp không dây `firmware.bin` và `littlefs.bin` qua trình duyệt |

> *Ghi chú thiết kế:* Nhật ký vận hành chu kỳ phun sương (`AUTO`, `MANUAL`, `SCHED`, `WD`) và tính năng xuất tệp **`biochamber_logs.csv`** được xử lý phía trình duyệt trên `data/js/logs.js` + `localStorage` nhằm tiết kiệm RAM vi điều khiển. Trang `smartlogic.html` hỗ trợ kiểm tra điều kiện ẩm/mưa ngoại vi qua **OpenWeather API**.

### 6.2. Giao thức CoreIOT MQTT (`app.coreiot.io:1883`)
- **Edge Node (`src/coreiot_client.cpp`):**
  - `v1/devices/me/telemetry`: `temperature`, `humidity`, `soilMoisture`, `lightLux`, `pump`, `fan`, `autoMode`, `dhtError`, `pumpCount`, `totalPumpSec`, `ai_state`, `ai_confidence`, `ai_latency_us`, `anomaly_score`, `anomaly_pred`, `anomaly_type`, `online_acc`.
  - `v1/devices/me/attributes`: `firmware`, `board`, `ip`, `mac`, `ai_engine`.
  - `v1/devices/me/rpc/request/+`: Hỗ trợ `setPump`, `setFan`, `setMode`, `setAiMode`, `getStatus`.
- **Fog Gateway (`gateway/gateway_core.py`):**
  - Gửi bổ sung lên `v1/devices/me/telemetry`: `soil`, `light`, `mode`, `gateway_ai_anomaly_score`, `gateway_ai_is_anomaly`, `gateway_ai_type`, `gateway_projected_temp_30m`, `gateway_irrigation_advice`, `gateway_ai_confidence`; chuyển tiếp RPC `setPump`, `setFan`, `setMode` xuống ESP32 qua Serial hoặc `POST /api/control`.



## 7. Cấu trúc mã nguồn

```text
cleanroom-biochamber-edge-ai/
├── platformio.ini                 # Cấu hình PlatformIO (yolo_uno & esp32dev, ElegantOTA, LittleFS)
├── partitions.csv                 # Bảng phân vùng Flash 8MB Dual-OTA (nvs, otadata, app0, app1, spiffs)
├── partitions_4mb.csv             # Bảng phân vùng dự phòng Flash 4MB Dual-OTA cho env esp32dev
├── biochamber_block_diagram.html  # Sơ đồ khối phần cứng & kiến trúc tương tác
├── include/
│   ├── config.h                   # Chân GPIO, thông số Modbus RS485, ADC, ngưỡng & Watchdog
│   ├── dht_anomaly_model.h        # Mảng TFLite FlatBuffer C++ (1.836 bytes)
│   ├── tinyml_engine.h            # Header suy luận MLP, Decision Forest & phát hiện dị thường
│   └── ...                        # Header cho sensors, actuators, iot_bridge, scheduler, web, mqtt
├── src/
│   ├── main.cpp                   # Setup ngoại vi, LittleFS, IPC, FreeRTOS Core 0 & Scheduler Core 1
│   ├── iot_bridge.cpp             # Khởi tạo g_sensorMutex, xTelemetryQueue, xRpcCommandQueue, EventGroup
│   ├── sensor_handler.cpp         # Modbus RS485 Master + Fallback DHT20 + ADC1 EMA + DS3231 RTC
│   ├── tinyml_engine.cpp          # Suy luận C++ MLP + Heat Index + Decision Forest + dT/dt (~50 µs)
│   ├── actuator_handler.cpp       # Hysteresis, Smart Mist Skip, Watchdog 120s & Cooldown 30s
│   ├── coreiot_client.cpp         # MQTT Telemetry/Attributes & RPC Handler trên Core 0
│   ├── web_server.cpp             # ESPAsyncWebServer, WebSocket /ws, REST API, LittleFS & ElegantOTA
│   ├── scheduler.cpp / _tasks.cpp # Bộ lập lịch hợp tác 14 tác vụ trên Core 1
│   ├── lcd_display.cpp            # Giao diện LCD 1602 (4 trang buồng sạch + AI)
│   ├── button_handler.cpp         # Xử lý cụm 3 nút bấm vật lý
│   └── wifi_manager.cpp / ...     # Quản lý BioChamber-AP/STA, NTP Sync & lưu cấu hình /info.dat
├── data/                          # Web SPA nhúng trên LittleFS (BioChamber Orbit)
│   ├── index.html & css/          # Shell chính + bộ CSS offline-first (4 tệp)
│   ├── image/                     # Hình ảnh Cleanroom Bio-Chamber tối ưu dung lượng cho Flash
│   ├── partials/ & pages/         # 4 partials + 10 trang (Dashboard, Control, Charts, SmartLogic, Logs...)
│   └── js/                        # 13 mô-đun JS (WebSocket, Chart.js, xuất CSV, cấu hình...)
├── gateway/                       # Tầng Fog Gateway Python & BIO-SCADA GUI
│   ├── gateway_app.py             # Launcher chính (GUI mặc định hoặc --headless cho server/RPi)
│   ├── gateway_core.py            # Đa luồng thu thập Modbus/Serial/HTTP <-> MQTT CoreIOT & RPC
│   ├── gateway_ai.py              # Z-Score Anomaly, OLS Forecast 30p & Smart Mist AI
│   ├── gateway_gui.py             # Giao diện Desktop BIO-SCADA Dark Mode (Tkinter + Sparkline)
│   ├── config.json                # Cấu hình cổng COM, Wi-Fi IP, Modbus & CoreIOT Access Token
│   └── requirements.txt           # paho-mqtt, pyserial, requests, numpy
└── tools/train_tinyml/            # Pipeline huấn luyện AI
    ├── train_dht_anomaly.py       # Sinh 10.000 mẫu, train Keras MLP -> xuất .tflite & .h
    └── train_dht_anomaly.ipynb    # Notebook phân tích & trực quan hóa mô hình
```



## 8. Hướng dẫn cài đặt & vận hành

### 8.1. Biên dịch & Nạp Firmware + Web SPA (`ESP32-S3`)
Yêu cầu cài đặt **PlatformIO CLI** hoặc VS Code + PlatformIO IDE:
```bash
git clone https://github.com/vophamk23/cleanroom-biochamber-edge-ai.git
cd cleanroom-biochamber-edge-ai  # (hoặc cd ESP32-Smart-Greenhouse-Embedded-Web-Server trong workspace cục bộ)

# 1. Biên dịch Firmware (mặc định env yolo_uno)
pio run -e yolo_uno

# 2. Nạp Firmware qua cổng USB-CDC
pio run -t upload -e yolo_uno

# 3. Cập nhật thông tin Wi-Fi & CoreIOT Token trong data/info.dat (thay YOUR_*) rồi nạp LittleFS
pio run -t buildfs -e yolo_uno
pio run -t uploadfs -e yolo_uno
```
- Truy cập giao diện cục bộ qua IP của ESP32 trong mạng LAN, hoặc kết nối Wi-Fi **`BioChamber-AP`** (mật khẩu mặc định `12345678`) rồi mở `http://192.168.4.1` (mặc định nếu chưa điền `data/info.dat` thiết bị tự giữ chế độ AP).
- Nạp cập nhật phần mềm/giao diện không dây qua trình duyệt tại `http://<ESP32_IP>/update` (**ElegantOTA**).

### 8.2. Chạy trạm Fog Gateway (`BIO-SCADA GUI` hoặc `Headless`)
Trước khi chạy, cập nhật `coreiot.token`, `esp32_connection.wifi_ip` / `serial_port` trong [`gateway/config.json`](gateway/config.json):
```bash
cd gateway
pip install -r requirements.txt

# Chạy kèm giao diện Desktop BIO-SCADA Dark Mode
python gateway_app.py
# (Hoặc chạy trực tiếp: python gateway_gui.py)

# Chạy chế độ Headless trên Server / Docker / Raspberry Pi
python gateway_app.py --headless
# (Hoặc chạy daemon core: python gateway_core.py)
```

### 8.3. Huấn luyện lại mô hình TinyML (`C++ Header`)
```bash
cd tools/train_tinyml
pip install tensorflow numpy scikit-learn
python train_dht_anomaly.py
# Sao chép dht_anomaly_model.h sinh ra vào thư mục include/ trước khi biên dịch lại firmware
```

---

## 9. Giấy phép

Mã nguồn được phát hành theo giấy phép **MIT License**.
