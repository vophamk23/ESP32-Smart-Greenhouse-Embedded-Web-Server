# 🌿 EcoGreen — Greenhouse IoT System

Hệ thống nhà kính thông minh tích hợp ESP32, Backend NestJS và Frontend Next.js. Hỗ trợ giám sát cảm biến thời gian thực, điều khiển thiết bị tự động và gửi thông báo đa kênh.

---

## 📋 Mục lục

- [Tổng quan hệ thống](#tổng-quan-hệ-thống)
- [Tech Stack](#tech-stack)
- [Cấu trúc dự án](#cấu-trúc-dự-án)
- [Design Patterns](#design-patterns)
- [Yêu cầu hệ thống](#yêu-cầu-hệ-thống)
- [Cài đặt và chạy](#cài-đặt-và-chạy)
- [Biến môi trường](#biến-môi-trường)
- [API Endpoints](#api-endpoints)
- [Functional Requirements](#functional-requirements)
- [Kiến trúc dual-core ESP32](#kiến-trúc-dual-core-esp32)

---

## Tổng quan hệ thống

EcoGreen là hệ thống IoT nhà kính gồm 3 tầng hoạt động độc lập:

| Tầng | Công nghệ | Vai trò |
|---|---|---|
| Firmware | ESP32 + PlatformIO | Thu thập sensor, điều khiển actuator |
| Backend | NestJS + PostgreSQL | Xử lý dữ liệu, auth, thông báo, lưu trữ |
| Frontend | Next.js + Tailwind CSS | Dashboard giám sát và điều khiển |

**2 cách truy cập:**
- **Local (offline):** `http://192.168.4.1` — truy cập trực tiếp ESP32, xem sensor và điều khiển cơ bản
- **Full (online):** Domain/IP server — đầy đủ tính năng auth, lịch sử, lịch tưới, thông báo

---

## Tech Stack

### Firmware (ESP32)
- **PlatformIO** — Build system
- **Arduino Framework** — ESP32 core
- **FreeRTOS** — Dual-core task management
- **ESPAsyncWebServer** — Async HTTP + WebSocket
- **PubSubClient** — MQTT client (CoreIoT/ThingsBoard)
- **ArduinoJson** — JSON serialization
- **LittleFS** — File system lưu config
- **DHT** — Cảm biến nhiệt độ/độ ẩm
- **Adafruit NeoPixel** — LED RGB
- **RTClib** — RTC DS3231

### Backend
- **NestJS** (Node.js) — Framework chính
- **TypeScript** — Ngôn ngữ lập trình
- **PostgreSQL** — Cơ sở dữ liệu quan hệ
- **Socket.io** — Real-time communication
- **JWT + Bcrypt** — Xác thực và bảo mật
- **MQTT** — Kết nối với ESP32
- **Nodemailer** — Gửi email SMTP
- **node-cron** — Lập lịch tưới tự động

### Frontend
- **Next.js 14** (App Router) — Framework React
- **TypeScript** — Ngôn ngữ lập trình
- **Tailwind CSS** — Styling utility-first
- **Socket.io Client** — Nhận data real-time
- **Axios** — HTTP client

---

## Cấu trúc dự án

```
Project-Greenhouse-IOT/
├── src/                    ← ESP32 firmware (C++)
├── include/                ← Header files
├── lib/                    ← Libraries
├── data/                   ← Frontend cũ (serve trực tiếp từ ESP32)
├── platformio.ini
│
├── backend/                ← NestJS Backend
│   ├── .env
│   ├── package.json
│   ├── nest-cli.json
│   └── src/
│       ├── main.ts
│       ├── app.module.ts
│       ├── config/         ← Database, MQTT config
│       ├── database/       ← SQL migrations
│       ├── common/         ← Guards, decorators
│       ├── models/devices/ ← Factory Method products
│       ├── observers/      ← Observer pattern
│       ├── strategies/     ← Strategy pattern
│       ├── modules/        ← Feature modules (auth, sensors, devices...)
│       └── services/       ← AlertMonitor, DeviceFactory, IrrigationContext
│
└── frontend/               ← Next.js Frontend
    ├── .env.local
    ├── package.json
    └── src/
        ├── middleware.ts   ← Auth guard
        ├── app/            ← Next.js App Router pages
        ├── components/     ← UI components
        ├── hooks/          ← Custom React hooks
        ├── lib/            ← API client, auth helpers
        └── types/          ← TypeScript interfaces
```

---

## Design Patterns

### Factory Method — FR11 (Quản lý thiết bị)

```
device.base.ts          ← Abstract Product
sensor.device.ts        ← ConcreteProduct: cảm biến
actuator.device.ts      ← ConcreteProduct: bơm, quạt, đèn
display.device.ts       ← ConcreteProduct: LCD, NeoPixel
device-factory.service.ts ← Creator (Factory Method)
```

Cho phép đăng ký thiết bị IoT mới mà không cần sửa mã nguồn lõi.

### Observer Pattern — FR10 (Thông báo tự động)

```
notification-observer.interface.ts  ← Observer interface
telegram.notifier.ts    ← ConcreteObserver: retry 3 lần, cooldown 10 phút
email.notifier.ts       ← ConcreteObserver: SMTP
websocket.notifier.ts   ← ConcreteObserver: push real-time
activity.logger.ts      ← ConcreteObserver: ghi DB
alert-monitor.service.ts ← Subject: setInterval 30s
```

Phân tách hoàn toàn logic phát hiện sự cố và logic gửi thông báo.

### Strategy Pattern — FR06, FR07, FR08 (Logic tưới)

```
irrigation-strategy.interface.ts        ← Strategy interface
threshold-irrigation.strategy.ts        ← Tưới theo ngưỡng độ ẩm
schedule-irrigation.strategy.ts         ← Tưới theo lịch cố định
weather-irrigation.strategy.ts          ← Tưới theo dự báo thời tiết
irrigation-context.service.ts           ← Context
```

Cho phép chuyển đổi linh hoạt giữa các chiến lược tưới mà không thay đổi logic gốc.

---

## Yêu cầu hệ thống

### ESP32
- PlatformIO IDE hoặc VS Code + PlatformIO extension
- ESP32 DevKit v1 hoặc tương đương
- Phần cứng: DHT11/22, LDR, Soil moisture sensor, Relay, NeoPixel, LCD I2C, RTC DS3231

### Backend
- Node.js >= 18.x
- PostgreSQL >= 14
- MQTT Broker (Mosquitto hoặc CoreIoT)

### Frontend
- Node.js >= 18.x

---

## Cài đặt và chạy

### 1. ESP32 Firmware

```bash
# Clone repository
git clone https://github.com/your-repo/Project-Greenhouse-IOT.git
cd Project-Greenhouse-IOT

# Build và flash firmware
pio run --target upload

# Upload file hệ thống lên LittleFS
pio run --target uploadfs

# Mở Serial Monitor
pio device monitor --baud 115200
```

Sau khi flash, kết nối WiFi `Greenhouse-AP` (password: `12345678`), truy cập `http://192.168.4.1` để cấu hình WiFi và MQTT.

### 2. Backend

```bash
cd backend

# Cài dependencies
npm install

# Tạo file .env (xem phần Biến môi trường)
cp .env.example .env

# Chạy migration tạo bảng DB
psql -U postgres -d greenhouse -f src/database/migrations/001_init.sql

# Chạy development
npm run start:dev

# Build production
npm run build
npm run start:prod
```

### 3. Frontend

```bash
cd frontend

# Cài dependencies
npm install

# Tạo file .env.local
cp .env.example .env.local

# Chạy development
npm run dev

# Build production
npm run build
npm run start
```

---

## Biến môi trường

### Backend — `backend/.env`

```env
# Database
DATABASE_URL=postgresql://postgres:password@localhost:5432/greenhouse

# JWT
JWT_SECRET=your-super-secret-jwt-key-min-32-chars
JWT_EXPIRES_IN=24h

# MQTT
MQTT_BROKER_URL=mqtt://localhost:1883
MQTT_USERNAME=
MQTT_PASSWORD=

# Telegram encryption
TELEGRAM_ENCRYPT_KEY=your-32-char-encryption-key

# Email SMTP
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USER=your@gmail.com
SMTP_PASS=your-app-password

# App
PORT=3001
CORS_ORIGIN=http://localhost:3000
```

### Frontend — `frontend/.env.local`

```env
NEXT_PUBLIC_API_URL=http://localhost:3001
NEXT_PUBLIC_WS_URL=http://localhost:3001
```

---

## API Endpoints

### Auth — `/api/auth`

| Method | Endpoint | Mô tả |
|---|---|---|
| POST | `/login` | Đăng nhập, trả về JWT token |
| GET | `/check-session` | Kiểm tra token còn hợp lệ |
| POST | `/logout` | Đăng xuất, xóa session |

### Users — `/api/user`

| Method | Endpoint | Mô tả |
|---|---|---|
| GET | `/profile` | Lấy thông tin người dùng |
| PUT | `/profile` | Cập nhật profile và mật khẩu |

### Sensors — `/api/sensors`

| Method | Endpoint | Mô tả |
|---|---|---|
| GET | `/history?range=24h` | Lịch sử sensor (24h/7d/30d) |
| GET | `/export?format=csv` | Xuất dữ liệu CSV/Excel |

### Devices — `/api/devices`

| Method | Endpoint | Mô tả |
|---|---|---|
| GET | `/` | Danh sách thiết bị |
| POST | `/` | Đăng ký thiết bị mới |
| DELETE | `/:id` | Xóa thiết bị |

### Schedules — `/api/schedules`

| Method | Endpoint | Mô tả |
|---|---|---|
| GET | `/` | Danh sách lịch tưới |
| POST | `/` | Tạo lịch mới |
| PUT | `/:id` | Cập nhật lịch |
| DELETE | `/:id` | Xóa lịch |

### Notifications — `/api/notifications`

| Method | Endpoint | Mô tả |
|---|---|---|
| POST | `/telegram/test` | Gửi tin nhắn test Telegram |
| POST | `/telegram/save` | Lưu cấu hình Telegram |
| POST | `/email/test` | Gửi email test |
| POST | `/email/save` | Lưu cấu hình Email |

---

## Functional Requirements

| FR | Tính năng | Trạng thái |
|---|---|---|
| FR01 | Hiển thị sensor real-time | ✅ ESP32 WebSocket + Backend Socket.io |
| FR02 | Giám sát sức khỏe thiết bị | ✅ WiFi RSSI, sensor status |
| FR03 | Lịch sử & xuất dữ liệu CSV | ✅ PostgreSQL time-series |
| FR04 | Nhật ký hoạt động | ✅ ActivityLogger Observer |
| FR05 | Điều khiển thủ công | ✅ WebSocket RPC → ESP32 |
| FR06 | Ngưỡng độ ẩm tưới | ✅ ThresholdIrrigationStrategy |
| FR07 | Lập lịch tưới tự động | ✅ ScheduleIrrigationStrategy + node-cron |
| FR08 | Smart Logic thời tiết | ✅ WeatherIrrigationStrategy + OpenWeather API |
| FR09 | Xác thực & phiên làm việc | ✅ JWT + Bcrypt |
| FR10 | Thông báo Telegram/Email | ✅ Observer Pattern |
| FR11 | Quản lý thiết bị | ✅ Factory Method Pattern |
| FR12 | Auto mode điều khiển | ✅ autoControlPump/Fan/LED trong ESP32 |
| FR13 | Gửi telemetry định kỳ | ✅ Task_SendTelemetry mỗi 5s |
| FR14 | Watchdog máy bơm | ✅ Task_PumpWatchdog, force OFF |

---

## Kiến trúc dual-core ESP32

```
Core 0 (FreeRTOS)              Core 1 (Arduino loop)
─────────────────              ──────────────────────
greenhouse_wifi_task           Cooperative Scheduler
greenhouse_coreiot_task   ←──  Task_SendTelemetry
greenhouse_webserver_task ──►  Task_ProcessRpc
                               Task_ReadAirSensors
xTelemetryQueue (C1→C0)        Task_ReadSoilSensor
xRpcCommandQueue (C0→C1)       Task_AutoControl
                               Task_PumpWatchdog
                               Task_UpdateLCD
                               Task_ScanButtons
```

---

## Thành viên nhóm

| Tên | MSSV | Vai trò |
|---|---|---|
| ... | ... | ... |

---

## License

MIT License — xem file [LICENSE](LICENSE) để biết thêm chi tiết.