# 🌐 Hệ Thống IoT Đa Tầng Giám Sát Vi Khí Hậu & Điều Khiển Tự Động Tích Hợp TinyML

### Multi-Tier IoT Platform: Edge TinyML • Industrial Modbus RS485 • Embedded Web SPA • Fog Gateway AI • CoreIOT Cloud OTA

<div align="center">

![ESP32-S3](<https://img.shields.io/badge/Board-Yolo%3AUNO%20(ESP32--S3)-E7352C?style=for-the-badge&logo=espressif&logoColor=white>)
![TinyML](https://img.shields.io/badge/AI-TinyML%20On--Chip-FF6F00?style=for-the-badge&logo=tensorflow&logoColor=white)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20Dual--Core%20AMP-00979D?style=for-the-badge&logo=cplusplus&logoColor=white)
![Modbus RS485](https://img.shields.io/badge/Protocol-Modbus%20RS485-FFD700?style=for-the-badge)
![CoreIOT](https://img.shields.io/badge/Cloud-CoreIOT%20MQTT-0052CC?style=for-the-badge)
![LittleFS](https://img.shields.io/badge/Web%20Server-LittleFS%20Async%20SPA-4A154B?style=for-the-badge)
![Gateway](https://img.shields.io/badge/Gateway-Python%20Tkinter%20GUI-3776AB?style=for-the-badge&logo=python&logoColor=white)

</div>

Dự án nghiên cứu, thiết kế và hiện thực một **Hệ Thống IoT Đa Tầng (Multi-Tier IoT System)** toàn diện phục vụ giám sát vi khí hậu và điều khiển tự động theo **Kiến trúc phân tầng lai 3 tầng (Three-Tier Hybrid Edge–Fog–Cloud Architecture)**. Hệ thống tích hợp các công nghệ IoT tiên tiến:

- **Tầng Biên (Edge Computing):** Mạch vi điều khiển nhân kép **Yolo:UNO (ESP32-S3)** chạy bộ điều phối hợp tác thời gian thực kết hợp mạng nơ-ron **TinyML On-Chip** suy luận siêu tốc độ trễ micro-giây ($t_{\text{infer}} < 50\,\mu\text{s}$) phân loại 6 trạng thái vi khí hậu và phát hiện dị thường nhiệt ẩm (`FLAME_TAMPER`), đọc cảm biến nhiệt ẩm công nghiệp qua giao thức **Modbus RS485** (Holding Registers 0 & 1), máy chủ web nhúng **LittleFS Single Page Application** phục vụ giám sát và điều khiển local không cần Internet.
- **Tầng Trạm (Fog Gateway):** Trạm **Local IoT Gateway** đa luồng bằng Python trên PC hỗ trợ thu thập dữ liệu kép (Serial/HTTP/RS485), phân tích AI chuỗi thời gian Z-Score, dự báo nhiệt độ 30 phút bằng hồi quy OLS và giao diện Desktop GUI Tkinter Dark Mode.
- **Tầng Mây (Cloud Computing):** Nền tảng **CoreIOT Cloud (ThingsBoard)** quản lý thiết bị tập trung, giám sát Telemetry trực quan, chuỗi luật RuleChain thông minh, cảnh báo Notification tức thời và quản trị cập nhật Firmware OTA không dây.


## 📋 Mục lục

1. [Bảng Đánh Giá Yếu Tố Công Nghệ (Thang điểm BTL)](#1-bảng-đánh-giá-yếu-tố-công-nghệ-thang-điểm-btl)
2. [Kiến trúc Hệ thống Tổng thể](#2-kiến-trúc-hệ-thống-tổng-thể)
3. [Bộ Máy Trí Tuệ Nhân Tạo Phân Tán 2 Tầng (Dual-Tier AI)](#3-bộ-máy-trí-tuệ-nhân-tạo-phân-tán-2-tầng-dual-tier-ai)
4. [Sơ đồ Ghép nối Phần cứng (Pinout Yolo:UNO)](#4-sơ-đồ-ghép-nối-phần-cứng-pinout-yolouno)
5. [Lớp Cảm Biến & Thuật toán Lọc số / Đồng bộ Thời gian](#5-lớp-cảm-biến--thuật-toán-lọc-số--đồng-bộ-thời-gian)
6. [Hệ Điều Phối Đa Nhiệm Hai Nhân (Dual-Core AMP FreeRTOS)](#6-hệ-điều-phối-đa-nhiệm-hai-nhân-dual-core-amp-freertos)
7. [Máy Chủ Web Nhúng SPA & Hệ Thống REST API / WebSocket](#7-máy-chủ-web-nhúng-spa--hệ-thống-rest-api--websocket)
8. [Trạm Local IoT Gateway Đa Luồng trên PC (Python)](#8-trạm-local-iot-gateway-đa-luồng-trên-pc-python)
9. [Cấu Trúc Thư Mục Mã Nguồn](#9-cấu-trúc-thư-mục-mã-nguồn)
10. [Hướng Dẫn Cài Đặt & Nạp Chương Trình](#10-hướng-dẫn-cài-đặt--nạp-chương-trình)
11. [Giao Diện LCD 1602 & Bảng Lệnh Console Serial](#11-giao-diện-lcd-1602--bảng-lệnh-console-serial)
12. [Giấy Phép & Bản Quyền](#12-giấy-phép--bản-quyền)

---

## 1. Bảng Đánh Giá Yếu Tố Công Nghệ (Thang điểm BTL)

Dự án hiện thực đầy đủ **11 yếu tố công nghệ chuẩn (T)** và **1 tính năng nâng cao (N)** theo đúng đề cương môn học của TS. Lê Trọng Nhân, tổng điểm tự đánh giá đạt **12.5 điểm** (vượt trần tối đa $8.0$ điểm công nghệ + $1.0$ điểm nâng cao):

|    Mã    | Tên yếu tố công nghệ                                                      | Điểm chuẩn | Minh chứng hiện thực trong dự án                                                                                                                                                           |  Trạng thái  |
| :------: | :------------------------------------------------------------------------ | :--------: | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :----------: |
| **T02**  | Cảm biến sử dụng giao tiếp **Modbus RS485**                               |  **1.0**   | Tích hợp thư viện `ModbusMaster` trên ESP32 (UART1: GPIO44/43) đọc Holding Registers 0 & 1 và thư viện `minimalmodbus` trên Gateway PC. Hỗ trợ Fail-Safe fallback sang DHT20.              |   **ĐẠT**    |
| **T04**  | Thông tin nội suy từ AI (**TinyML** chạy trên thiết bị)                   |  **1.0**   | Mạng MLP $[5 \to 8 \to 3]$ phân loại vi khí hậu on-chip & Autoencoder $[3 \to 2 \to 3]$ phát hiện dị thường tức thời ($t_{\text{infer}} < 50\,\mu\text{s}$) trong `src/tinyml_engine.cpp`. |   **ĐẠT**    |
| **T05**  | Công cụ thu thập dữ liệu, huấn luyện & xuất model TinyML                  |  **1.0**   | Bộ công cụ Python `tools/train_tinyml/train_dht_anomaly.py` sinh 3.000 mẫu tổng hợp, huấn luyện TensorFlow/Keras và xuất mảng C++ Header `dht_anomaly_model.h`.                            |   **ĐẠT**    |
| **T07**  | **Web Server** trên thiết bị (tự hiện thực — hiển thị & điều khiển local) |  **1.0**   | `ESPAsyncWebServer` phục vụ Single Page Application từ phân vùng Flash LittleFS 8MB, giao tiếp hai chiều WebSocket `/ws` và hệ thống REST API không cần Internet.                          |   **ĐẠT**    |
| **T08**  | **IoT Gateway** trên PC tổng hợp dữ liệu, gửi lên CoreIOT, nhận lệnh RPC  |  **1.0**   | Phần mềm `gateway/gateway_core.py` đa luồng hướng đối tượng, thu thập kép USB CDC Serial ($115200\,\text{bps}$) / HTTP LAN và cầu nối RPC hai chiều.                                       |   **ĐẠT**    |
| **T09**  | Chức năng **AI chạy trên IoT Gateway** để phân tích dữ liệu & phát hiện   |  **1.0**   | Mô-đun `gateway_ai.py` phân tích cửa sổ trượt 30 mẫu: phát hiện đột biến Z-Score, dự báo nhiệt độ 30 phút bằng Hồi quy OLS và tính chỉ số Drought Stress Index.                            |   **ĐẠT**    |
| **T10**  | **Giao diện giám sát — điều khiển** trên IoT Gateway local                |  **1.0**   | Ứng dụng Desktop GUI Tkinter Dark Mode `gateway_gui.py`: đồng hồ đo vi khí hậu, biểu đồ xu hướng AI, nhật ký luồng dữ liệu và điều khiển thiết bị tại chỗ.                                 |   **ĐẠT**    |
| **T11**  | Sử dụng template phát triển ứng dụng, thiết kế **Dashboard CoreIOT**      |  **1.0**   | Cấu hình Device Profile `smart-greenhouse-node`, thiết kế Dashboard trực quan trên `app.coreiot.io`: thẻ hiển thị gauges, công tắc RPC, biểu đồ telemetry.                                 |   **ĐẠT**    |
| **T12**  | Sử dụng **RuleChain** phát triển tính năng thông minh                     |  **1.0**   | Thiết lập chuỗi luật xử lý sự kiện ThingsBoard RuleChain: lọc dữ liệu bất thường, kích hoạt logic liên hoàn đóng/cắt rơ-le và gửi lệnh RPC.                                                |   **ĐẠT**    |
| **T13**  | Gửi cảnh báo bằng **Notification** khi dữ liệu vượt ngưỡng                |  **0.5**   | Tạo Alarm Rules trên CoreIOT: phát cảnh báo khẩn cấp khi nhiệt độ $> 38^\circ\text{C}$ hoặc có cờ cháy `FLAME_TAMPER`, gửi thông báo về Notification Center.                               |   **ĐẠT**    |
| **T14**  | Cập nhật firmware **OTA không dây** cho thiết bị                          |  **2.0**   | Tích hợp thư viện `ElegantOTA` trên nền AsyncWebServer, cho phép tải firmware `.bin` hoặc filesystem `.bin` qua trình duyệt tại endpoint `/update`.                                        |   **ĐẠT**    |
| **N06**  | **Giao diện Dashboard thiết kế riêng**, phát hiện & ghi nhận sự kiện      |  **1.0**   | Giao diện Emerald Orbit SPA trên LittleFS tích hợp thẻ **Anomaly Badge** thời gian thực, nhật ký sự kiện tưới tiêu và xuất báo cáo tập tin `.CSV`.                                         |   **ĐẠT**    |
| **TỔNG** | **Tổng điểm tự đánh giá các yếu tố công nghệ**                            |  **12.5**  | **Đạt mức trần tối đa 8.0/8.0 điểm công nghệ + 1.0/1.0 điểm nâng cao**                                                                                                                     | **XUẤT SẮC** |

---

## 2. Kiến trúc Hệ thống Tổng thể

Hệ thống hoạt động theo mô hình 3 tầng phân cấp:

```
┌───────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                           TẦNG 1: NÚT ĐIỆN TOÁN BIÊN (YOLO:UNO ESP32-S3)                              │
│                                                                                                       │
│   ┌───────────────────────────────────────────────┐   ┌───────────────────────────────────────────┐   │
│   │           NHÂN THỜI GIAN THỰC (CORE 1)        │   │           NHÂN TRUYỀN THÔNG (CORE 0)      │   │
│   │                                               │   │                                           │   │
│   │  • Đọc cảm biến: Modbus RS485 (T02)           │   │  • Task WiFi: Tự động khôi phục mạng      │   │
│   │    + DHT20 I2C + ADC Soil EMA + LDR           │   │  • Task MQTT: CoreIOT Cloud Client (T11)  │   │
│   │  • TinyML Edge AI: MLP & Autoencoder (T04)    │   │  • Task Web: AsyncWebServer SPA (T07)     │   │
│   │  • Điều khiển tự động: Hysteresis, Lịch RTC   │   │  • WebSocket Broadcast /ws                │   │
│   │  • Hiển thị: LCD 1602 (4 trang) + NeoPixel    │   │  • Cập nhật OTA không dây /update (T14)   │   │
│   └───────────────────────┬───────────────────────┘   └─────────────────────▲─────────────────────┘   │
│                           │                                                 │                         │
│                           └───────────► FreeRTOS SemaphoreMutex & xQueue ───┘                         │
│                                         (Thread-Safe Inter-Core IPC)                                  │
└──────────────────────────────────────────────────────────────────────────────────────────────────────┘
                                    │                                         │
                   Kênh USB CDC Serial / HTTP REST               Kênh MQTT v3.1.1 Dự phòng song song
                                    │                                         │
                                    ▼                                         │
┌────────────────────────────────────────────────────────────────────────┐    │
│                 TẦNG 2: LOCAL IOT GATEWAY (PC PYTHON)                  │    │
│                                                                        │    │
│  • Gateway Core (T08): Dual-Mode Poller, đệm đồng bộ & cầu nối RPC     │    │
│  • Gateway AI Engine (T09): Z-Score Anomaly, Dự báo OLS 30 phút        │    │
│  • Desktop GUI Tkinter (T10): Bảng điều khiển Dark Mode & Nhật ký      │    │
└───────────────────────────────────┬────────────────────────────────────┘    │
                                    │                                         │
                     Giao thức MQTT v3.1.1 (QoS 1)                            │
                                    │                                         │
                                    ▼                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                TẦNG 3: NỀN TẢNG ĐÁM MÂY COREIOT CLOUD                               │
│                                                                                                     │
│  • Quản lý & Hiển thị: Device Profile, RuleChain thông minh (T12), Dashboard giám sát từ xa (T11)   │
│  • Cảnh báo & Tự động hóa: Notification Center (T13), Quản trị gói Firmware OTA tập trung (T14)     │
└─────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Bộ Máy Trí Tuệ Nhân Tạo Phân Tán 2 Tầng (Dual-Tier AI)

Hệ thống kết hợp sức mạnh tính toán của vi điều khiển tại biên và máy tính trạm cục bộ:

```
                                  ┌───► [Nhánh 1: TinyML On-Chip (ESP32-S3)] ───► Nhãn Vi khí hậu Tức thời
                                  │                                               MLP [5 -> 8 -> 3] + Autoencoder [3 -> 2 -> 3]
                                                                                   & Cảnh báo Cháy (tinfer < 50 us)
[Dữ liệu Cảm biến Đa biến] ───────┤
T, RH, SM, Elux, Heat Index (HI)  │
                                  └───► [Nhánh 2: Gateway AI (Python PC)] ──────► Dự báo Nhiệt độ OLS 30 phút
                                                                                  Cửa sổ trượt 30 mẫu, Z-Score & OLS
                                                                                  & Chỉ số Khô hạn (Drought Index)
```

### Tầng 1: TinyML On-Chip trên ESP32-S3 (Mã T04, T05)

1. **Mạng MLP phân loại vi khí hậu ($5 \to 8 \to 3$):**
   - **Đầu vào (Vector $\bm{x} \in \mathbb{R}^5$):** Nhiệt độ ($T$), Độ ẩm không khí ($RH$), Độ ẩm đất ($SM$), Cường độ sáng ($E_{\text{lux}}$) và Chỉ số cảm nhận nhiệt ($HI$ - tính theo công thức NOAA Rothfusz).
   - **Đầu ra:** Phân loại 3 trạng thái môi trường: `COMFORTABLE`, `HEAT_WARNING`, `DRY_SOIL`.
   - **Hành vi tự động:** Khi phát hiện `HEAT_WARNING` $\to$ AI tự động kích hoạt quạt làm mát; khi môi trường trở về bình thường $\to$ tự động ngắt quạt.

2. **Mạng Autoencoder ($3 \to 2 \to 3$) kết hợp Đạo hàm vật lý:**
   - Nén vector $[\tilde{T}, \tilde{RH}, \tilde{HI}]^T$ xuống không gian tiềm ẩn 2 chiều rồi tái tạo để tính sai số MSE.
   - Kết hợp tốc độ biến thiên đạo hàm thời gian $\Delta T/\Delta t$ và $\Delta RH/\Delta t$ để nhận diện:
     - `NORMAL`: Trạng thái vi khí hậu an toàn.
     - `FLAME_TAMPER`: **Cảnh báo lửa/cháy** khi $\Delta T/\Delta t > 0{,}25^\circ\text{C/s}$ và $\Delta RH/\Delta t < -0{,}30\%/\text{s}$ (ví dụ có người hơ bật lửa gần cảm biến).
     - `RAPID_DRIFT`: Trôi số liệu cảm biến hoặc gió lùa bất thường.
     - `SENSOR_FAULT`: Lỗi phần cứng, đứt cáp hoặc mất nguồn cảm biến.

### Tầng 2: AI trên trạm Local Gateway PC (Mã T09)

- **Phát hiện đột biến thống kê Z-Score:** Tính kỳ vọng $\mu$ và độ lệch chuẩn $\sigma$ theo cửa sổ trượt $N = 30$ mẫu. Cảnh báo đột biến khi $|Z| = \frac{|x_t - \mu|}{\sigma} > 2{,}2$.
- **Dự báo nhiệt độ 30 phút tới (OLS Trend Forecasting):** Khớp đường xu hướng tuyến tính theo phương pháp bình phương bé nhất (_Ordinary Least Squares_):
  $$\beta_1 = \frac{\sum (t_i - \bar{t})(T_i - \bar{T})}{\sum (t_i - \bar{t})^2}, \qquad \hat{T}_{t+1800\text{s}} = T_t + 1800 \cdot \beta_1$$
- **Chỉ số căng thẳng khô hạn (Drought Stress Index):** Tổng hợp trọng số $55\%$ từ độ thiếu hụt ẩm đất + $30\%$ từ nhiệt độ cao + $15\%$ từ không khí khô để đưa ra khuyến nghị thời gian tưới chính xác theo số giây.

---

## 4. Sơ đồ Ghép nối Phần cứng (Pinout Yolo:UNO)

Bo mạch sử dụng chuẩn chân cắm **Yolo:UNO (ESP32-S3)**:

| Thiết bị / Ngoại vi     |           Chân GPIO            |        Chuẩn giao tiếp         |            Mức logic            | Vai trò & Cấu hình phần cứng                                                             |
| :---------------------- | :----------------------------: | :----------------------------: | :-----------------------------: | :--------------------------------------------------------------------------------------- |
| **Modbus RS485 (T02)**  |  **RX: GPIO 44, TX: GPIO 43**  |        UART1 Modbus RTU        |      $3{,}3\,\text{V}$ TTL      | Nối Module MAX485 đọc Cảm biến nhiệt ẩm công nghiệp (Baud: 9600, Slave ID: 4)            |
| **Bus I2C dùng chung**  | **SDA: GPIO 11, SCL: GPIO 12** | I2C Master ($100\,\text{kHz}$) |    $3{,}3\,\text{V}$ Pull-up    | Cổng Đỏ Grove: DHT20 (`0x38`), RTC DS3231 (`0x68`), LCD 1602 (`0x21/0x27`)               |
| **Cảm biến độ ẩm đất**  |        **GPIO 1 (A0)**         |       ADC1_CH0 (12-bit)        | $0 \div 3{,}3\,\text{V}$ Analog | Đo điện áp biến thiên theo độ ẩm đất; lấy trung bình 5 mẫu + bộ lọc EMA ($\alpha=0{,}2$) |
| **Quang trở LDR**       |        **GPIO 2 (A1)**         |       ADC1_CH1 (12-bit)        | $0 \div 3{,}3\,\text{V}$ Analog | Đo cường độ ánh sáng, ánh xạ tuyến tính $0 \div 1000\,\text{lux}$                        |
| **Relay Kênh 1 (Bơm)**  |        **GPIO 7 (D4)**         |          GPIO Output           |           Active LOW            | Cổng Trắng D4: Đóng/cắt mạch cách ly quang điều khiển máy bơm $5\,\text{V}$              |
| **Relay Kênh 2 (Quạt)** |        **GPIO 8 (D5)**         |          GPIO Output           |           Active LOW            | Cổng Trắng D5: Đóng/cắt mạch cách ly quang điều khiển quạt thông gió $5\,\text{V}$       |
| **LED RGB NeoPixel**    |          **GPIO 45**           |      WS2812B Single-Wire       |        $800\,\text{kHz}$        | Đèn LED on-board hiển thị màu theo trạng thái vi khí hậu và cảnh báo AI                  |
| **LED đơn on-board**    |       **GPIO 48 (D13)**        |          GPIO Output           |           Active HIGH           | Nhấp nháy chỉ thị nhịp tim hệ thống (_Heartbeat_)                                        |
| **3 Nút nhấn tại chỗ**  |      **GPIO 17, 18, 21**       |           GPIO Input           |           Active LOW            | Nút bấm vật lý cấu hình `INPUT_PULLUP`: Đổi Mode, Bơm tay, Quạt tay                      |

---

## 5. Lớp Cảm Biến & Thuật toán Lọc số / Đồng bộ Thời gian

1. **Bộ lọc số EMA khử nhiễu ẩm đất:**
   Áp dụng bộ lọc thông thấp trung bình trượt hàm mũ (_Exponential Moving Average_) với hệ số làm mịn $\alpha = 0{,}2$ để loại bỏ triệt để xung nhiễu điện từ (_Back-EMF_) khi đóng/ngắt rơ-le:
   $$S_t = 0{,}2 \cdot Y_t + 0{,}8 \cdot S_{t-1}$$

2. **Cơ chế Fallback cảm biến Fail-Safe:**
   - Hệ thống luôn ưu tiên đọc cảm biến công nghiệp **Modbus RS485** qua UART1.
   - Nếu cảm biến RS485 gặp sự cố (mất nguồn, đứt cáp), hệ thống tự động kích hoạt cơ chế phòng vệ chuyển sang đọc cảm biến số **DHT20 qua I2C**.
   - Nếu bus I2C bị nghẽn, hàm phục hồi `Wire.begin()` tự động tái thiết lập kết nối mà không làm treo hệ thống.

3. **Kiến trúc đồng bộ thời gian thực 3 cấp (RTC Sync):**
   - **Cấp 1 (Phần cứng):** Đọc trực tiếp từ RTC DS3231 có bù nhiệt TCXO (sai số $< 2\,\text{ppm}$).
   - **Cấp 2 (Mạng NTP):** Khi mất nguồn pin RTC, tự động đồng bộ giờ `UTC+7` qua 3 máy chủ NTP (`pool.ntp.org`, `time.google.com`, `time.cloudflare.com`).
   - **Cấp 3 (Nội suy Offline):** Nội suy mốc thời gian biên dịch (`__DATE__`, `__TIME__`) kết hợp hàm đếm `millis()` khi khởi động cô lập.

---

## 6. Hệ Điều Phối Đa Nhiệm Hai Nhân (Dual-Core AMP FreeRTOS)

Hệ thống phân tách kiến trúc bất đối xứng (_Asymmetric Multiprocessing_) trên chip ESP32-S3:

### Core 0: Các tác vụ Mạng & Đám mây bất đồng bộ (FreeRTOS Tasks)

- `greenhouse_wifi_task` (Priority 3): Duy trì kết nối WiFi STA, tự động quét và kết nối lại khi mất sóng.
- `greenhouse_coreiot_task` (Priority 2): Kết nối MQTT broker `app.coreiot.io`, xuất bản Telemetry và phân phối lệnh RPC.
- `greenhouse_webserver_task` (Priority 1): Phục vụ Web SPA từ LittleFS, xử lý REST API và WebSocket broadcast.

### Core 1: Bộ lập lịch hợp tác thời gian thực (Cooperative Scheduler - Tick 10ms)

|  STT   | Tên tác vụ               |   Chu kỳ    | Chức năng chi tiết                                                           |
| :----: | :----------------------- | :---------: | :--------------------------------------------------------------------------- |
| **1**  | `Task_ReadAirSensors`    | **500 ms**  | Đọc nhiệt ẩm (ưu tiên Modbus RS485 $\to$ fallback DHT20) và đo quang trở LDR |
| **2**  | `Task_ReadSoilSensor`    | **1 giây**  | Đọc ADC độ ẩm đất 12-bit kết hợp bộ lọc số EMA ($\alpha=0{,}2$)              |
| **3**  | `Task_TinyML_Inference`  | **3 giây**  | Chạy mạng nơ-ron MLP & Autoencoder on-chip, phát hiện dị thường tức thời     |
| **4**  | `Task_AutoControl`       | **1 giây**  | Thực thi luật điều khiển tự động có vùng trễ (Hysteresis) cho quạt và bơm    |
| **5**  | `Task_PumpWatchdog`      | **1 giây**  | Giám sát an toàn: ngắt bơm cưỡng bức sau 120s, cưỡng chế cooldown 30s        |
| **6**  | `Task_UpdateLEDStatus`   | **500 ms**  | Cập nhật màu đèn NeoPixel (Xanh lá: Good, Cam: Cảnh báo, Đỏ: Cháy/Dị thường) |
| **7**  | `Task_UpdateLCD`         | **500 ms**  | Cập nhật dữ liệu lên trang màn hình LCD 1602 hiện tại                        |
| **8**  | `Task_AutoSwitchLCDPage` | **5 giây**  | Tự động lật lần lượt giữa 4 trang thông tin LCD                              |
| **9**  | `Task_Heartbeat`         | **15 giây** | In snapshot toàn diện trạng thái hệ thống ra Serial Monitor                  |
| **10** | `Task_ScanButtons`       |  **50 ms**  | Quét trạng thái 3 nút bấm vật lý kèm thuật toán chống rung phím (Debounce)   |
| **11** | `Task_SendTelemetry`     | **3 giây**  | Chụp dữ liệu an toàn luồng và đẩy vào hàng đợi `xTelemetryQueue` sang Core 0 |
| **12** | `Task_ProcessRpc`        | **200 ms**  | Lấy lệnh RPC từ `xRpcCommandQueue` để điều khiển chấp hành ngay lập tức      |
| **13** | `Task_CheckSchedule`     | **5 giây**  | Đối chiếu thời gian thực DS3231 với lịch hẹn tưới nước (Smart Weather Skip)  |

---

## 7. Máy Chủ Web Nhúng SPA & Hệ Thống REST API / WebSocket

Giao diện **Emerald Orbit** được lưu trữ trực tiếp trên phân vùng bộ nhớ Flash **LittleFS (8MB)** của ESP32-S3, hỗ trợ cập nhật dữ liệu hai chiều tức thời qua **WebSocket** (`ws://<ESP32_IP>/ws`) mà không cần tải lại trang.

### Danh mục REST API Endpoint:

| Phương thức | Endpoint               | Chức năng                                                   | Định dạng dữ liệu |
| :---------: | :--------------------- | :---------------------------------------------------------- | :---------------: |
|    `GET`    | `/api/status`          | Lấy toàn bộ trạng thái vi khí hậu, thiết bị, AI và RAM      |       JSON        |
|    `GET`    | `/api/sensors`         | Lấy thông số cảm biến thô (nhiệt, ẩm, đất, quang thông)     |       JSON        |
|   `POST`    | `/api/control`         | Điều khiển bật/tắt bơm, quạt hoặc chuyển chế độ Auto/Manual |       JSON        |
|    `GET`    | `/api/config`          | Đọc cấu hình ngưỡng điều khiển và lịch hẹn tưới             |       JSON        |
|   `POST`    | `/api/config`          | Lưu cấu hình ngưỡng mới vào Flash LittleFS (`info.dat`)     |       JSON        |
|    `GET`    | `/api/logs?format=csv` | Xuất và tải về nhật ký sự kiện đóng/ngắt thiết bị           |    File `.CSV`    |
|    `GET`    | `/update`              | Giao diện nạp Firmware hoặc Filesystem OTA qua trình duyệt  |    HTML / Web     |

---

## 8. Trạm Local IoT Gateway Đa Luồng trên PC (Python)

Thư mục `gateway/` chứa phần mềm Gateway cục bộ viết bằng Python đa luồng:

```
gateway/
├── gateway_app.py        # Điểm khởi động ứng dụng trạm Gateway
├── gateway_core.py       # Core IoT Gateway đa luồng (T08): Poller Serial/HTTP/RS485, đệm đồng bộ
├── gateway_gui.py        # Giao diện Desktop GUI Tkinter Dark Mode (T10)
├── gateway_ai.py         # Bộ máy AI Gateway (T09): Z-Score, OLS 30 phút, Drought Stress
├── config.json           # Cấu hình cổng COM, Token CoreIOT Cloud, IP mạng LAN
└── requirements.txt      # Thư viện phụ thuộc (paho-mqtt, pyserial, minimalmodbus, requests)
```

### Các bước khởi chạy Gateway trên PC:

1. Cài đặt các thư viện phụ thuộc:
   ```bash
   cd gateway
   pip install -r requirements.txt
   ```
2. Khởi chạy giao diện Desktop Gateway:
   ```bash
   python gateway_app.py
   ```
3. Giao diện Dark Mode cho phép chọn kênh kết nối: **USB CDC Serial**, **WiFi LAN REST**, hoặc **Modbus RS485 trực tiếp** (qua cổng COM USB-RS485).

---

## 9. Cấu Trúc Thư Mục Mã Nguồn

```
ESP32-Smart-Greenhouse-Embedded-Web-Server/
├── include/                                # Tệp tiêu đề C++
│   ├── config.h                            # Cấu hình chân phần cứng, Modbus RS485, ngưỡng mặc định
│   ├── sensor_handler.h                    # Khai báo hàm đọc cảm biến (Modbus RS485, DHT20, ADC)
│   ├── actuator_handler.h                  # Khai báo điều khiển Relay Bơm, Quạt, LED NeoPixel
│   ├── tinyml_engine.h                     # Khai báo mô hình TinyML On-Chip & Heat Index
│   ├── dht_anomaly_model.h                 # Mảng trọng số mạng nơ-ron C++ FlatBuffer
│   ├── scheduler.h                         # Bộ lập lịch hợp tác Cooperative Scheduler
│   ├── scheduler_tasks.h                   # Định nghĩa chu kỳ 13 tác vụ định thời Core 1
│   ├── global_vars.h                       # Khai báo biến toàn cục & cờ trạng thái an toàn
│   ├── iot_bridge.h                        # Đồng bộ liên nhân (FreeRTOS Mutex & xQueue)
│   ├── coreiot_client.h                    # Client MQTT kết nối đám mây CoreIOT
│   ├── web_server.h                        # Máy chủ Web nhúng AsyncWebServer & WebSocket
│   ├── lcd_display.h                       # Driver hiển thị LCD 1602 I2C 4 trang
│   ├── config_manager.h                    # Đọc/ghi tập tin cấu hình LittleFS
│   ├── ntp_sync.h                          # Đồng bộ thời gian 3 máy chủ NTP
│   └── wifi_manager.h                      # Tác vụ giám sát kết nối WiFi FreeRTOS
│
├── src/                                    # Mã nguồn hiện thực C++
│   ├── main.cpp                            # Khởi tạo phần cứng 2 nhân & vòng lặp loop()
│   ├── sensor_handler.cpp                  # Đọc Modbus RS485 (T02), DHT20 I2C, ADC Soil EMA, LDR
│   ├── actuator_handler.cpp                # Logic điều khiển chấp hành rơ-le, quạt và NeoPixel
│   ├── tinyml_engine.cpp                   # Hiện thực Forward Pass MLP & Autoencoder (T04)
│   ├── scheduler.cpp                       # Bộ điều phối Cooperative Scheduler Core 1
│   ├── scheduler_tasks.cpp                 # Hiện thực 13 tác vụ định kỳ thời gian thực
│   ├── global_vars.cpp                     # Cấp phát bộ nhớ biến toàn cục
│   ├── iot_bridge.cpp                      # Hiện thực SemaphoreMutex và hàng đợi IPC
│   ├── coreiot_client.cpp                  # Đóng gói JSON Telemetry & thực thi RPC
│   ├── web_server.cpp                      # Xử lý HTTP REST API & WebSocket broadcast
│   ├── lcd_display.cpp                     # Render 4 trang thông tin LCD 1602
│   ├── config_manager.cpp                  # Quản lý đọc/ghi JSON trên LittleFS
│   ├── ntp_sync.cpp                        # Xử lý đồng bộ giờ mạng NTP
│   ├── wifi_manager.cpp                    # Tác vụ WiFi Auto-Reconnect FreeRTOS
│   └── button_handler.cpp                  # Xử lý khử rung nút bấm vật lý
│
├── data/                                   # Phân vùng bộ nhớ Flash LittleFS (Web SPA)
│   ├── index.html                          # Giao diện Single Page Application chính
│   ├── info.dat                            # Tập tin lưu trữ cấu hình mạng & Token
│   ├── css/                                # Định kiểu giao diện CSS
│   └── js/                                 # Kịch bản JavaScript, biểu đồ & WebSocket
│
├── gateway/                                # Ứng dụng Local IoT Gateway PC (Python GUI)
├── tools/train_tinyml/                     # Bộ công cụ sinh dữ liệu & huấn luyện TinyML (T05)
└── platformio.ini                          # Cấu hình PlatformIO môi trường bo mạch yolo_uno
```

---

## 10. Hướng Dẫn Cài Đặt & Nạp Chương Trình

### Yêu cầu môi trường

- **Phần mềm:** Visual Studio Code cài đặt tiện ích mở rộng **PlatformIO IDE**.
- **Phần cứng:** Bo mạch phát triển **Yolo:UNO (ESP32-S3)** kết nối qua cáp USB Type-C.

### Các bước thực hiện:

1. **Clone kho mã nguồn:**

   ```bash
   git clone https://github.com/vophamk23/edge-fog-cloud-iot-system.git
   cd edge-fog-cloud-iot-system
   ```

2. **Cấu hình thông tin WiFi & Token CoreIOT:**
   Chỉnh sửa tệp `data/info.dat`:

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
   pio run -t uploadfs -e yolo_uno
   ```

4. **Biên dịch và nạp Firmware lên ESP32-S3:**

   ```powershell
   pio run -t upload -e yolo_uno
   ```

5. **Mở Serial Monitor kiểm tra:**
   ```powershell
   pio device monitor --baud 115200
   ```

---

## 11. Giao Diện LCD 1602 & Bảng Lệnh Console Serial

### 4 Trang thông tin hiển thị trên màn hình LCD 1602:

```
[Trang 0: Vi khí hậu]        [Trang 1: Đất & Sáng]        [Trang 2: Thiết bị]          [Trang 3: TinyML AI]
┌────────────────┐          ┌────────────────┐          ┌────────────────┐          ┌────────────────┐
│Temp: 28.5 C    │          │Soil: 55.0 %    │          │PUMP:OFF FAN:OFF│          │AI: COMFORTABLE │
│Humi: 65.2 %    │          │Light: 720 Lux  │          │Mode: AUTO      │          │Conf:98% Lat:42u│
└────────────────┘          └────────────────┘          └────────────────┘          └────────────────┘
```

### Bảng phím tắt điều khiển qua Serial Terminal (115200 baud):

- **`A` / `a`**: Chuyển đổi qua lại giữa chế độ **AUTO** và **MANUAL**.
- **`P` / `p`**: Bật / Tắt máy bơm nước thủ công (khi ở chế độ MANUAL).
- **`F` / `f`**: Bật / Tắt quạt làm mát thủ công (khi ở chế độ MANUAL).
- **`N` / `n`**: Chuyển ngay sang trang tiếp theo trên màn hình LCD 1602.
- **`S` / `s`**: In ngay lập tức bảng tóm tắt cảm biến & TinyML ra màn hình.
- **`J` / `j`**: Xuất khung dữ liệu **JSON Telemetry** phục vụ trạm Gateway PC thu thập.

---

## 12. Giấy Phép & Bản Quyền

Dự án được phát triển và công bố theo giấy phép mã nguồn mở **MIT License**. Toàn bộ mã nguồn và tài liệu kỹ thuật được lưu trữ phục vụ mục đích học tập và nghiên cứu tại:

**Khoa Khoa học và Kỹ thuật Máy tính — Trường Đại học Bách Khoa, ĐHQG-HCM**  
_Môn học: Internet of Things (CO3061 / CO3062) — Giảng viên hướng dẫn: TS. Lê Trọng Nhân_
