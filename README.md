

```
███████╗ ██████╗ ██████╗  ██████╗ ██████╗ ███████╗███████╗███╗   ██╗
██╔════╝██╔════╝██╔═══██╗██╔════╝ ██╔══██╗██╔════╝██╔════╝████╗  ██║
█████╗  ██║     ██║   ██║██║  ███╗██████╔╝█████╗  █████╗  ██╔██╗ ██║
██╔══╝  ██║     ██║   ██║██║   ██║██╔══██╗██╔══╝  ██╔══╝  ██║╚██╗██║
███████╗╚██████╗╚██████╔╝╚██████╔╝██║  ██║███████╗███████╗██║ ╚████║
╚══════╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═══╝
```

### 🌿 Smart Greenhouse IoT System

**Hệ thống nhà kính thông minh — Giám sát thời gian thực, điều khiển tự động, thông báo đa kênh**

<br/>

[![ESP32](https://img.shields.io/badge/ESP32-FreeRTOS-blue?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![NestJS](https://img.shields.io/badge/NestJS-Backend-E0234E?style=for-the-badge&logo=nestjs&logoColor=white)](https://nestjs.com/)
[![Next.js](https://img.shields.io/badge/Next.js-14-black?style=for-the-badge&logo=nextdotjs&logoColor=white)](https://nextjs.org/)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-Database-336791?style=for-the-badge&logo=postgresql&logoColor=white)](https://www.postgresql.org/)
[![TypeScript](https://img.shields.io/badge/TypeScript-Language-3178C6?style=for-the-badge&logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![MQTT](https://img.shields.io/badge/MQTT-Protocol-660066?style=for-the-badge&logo=mqtt&logoColor=white)](https://mqtt.org/)

</div>

---

## 📖 Mục lục

- [🌐 Tổng quan hệ thống](#-tổng-quan-hệ-thống)
- [⚙️ Tech Stack](#️-tech-stack)
- [📁 Cấu trúc dự án](#-cấu-trúc-dự-án)
- [🧩 Design Patterns](#-design-patterns)
- [✅ Functional Requirements](#-functional-requirements)
- [🔧 Cài đặt & Chạy](#-cài-đặt--chạy)
- [🌍 Biến môi trường](#-biến-môi-trường)
- [📡 API Endpoints](#-api-endpoints)
- [⚡ Kiến trúc Dual-Core ESP32](#-kiến-trúc-dual-core-esp32)

---

## 🌐 Tổng quan hệ thống

EcoGreen là hệ thống IoT nhà kính 3 tầng hoạt động độc lập và phối hợp nhịp nhàng:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        EcoGreen Architecture                        │
├──────────────────┬──────────────────────┬───────────────────────────┤
│   🔌 FIRMWARE    │     ☁️  BACKEND     │      🖥️  FRONTEND         │
│   ESP32 C++      │   NestJS + PostgreSQL│    Next.js + Tailwind     │
│                  │                      │                           │
│  • Đọc sensor    │  • Xử lý dữ liệu     │  • Dashboard real-time    │
│  • Điều khiển    │  • Auth & JWT        │  • Biểu đồ lịch sử        │
│    actuator      │  • MQTT broker       │  • Điều khiển từ xa       │
│  • Dual-core     │  • Thông báo         │  • Cấu hình lịch tưới     │
│    FreeRTOS      │  • REST + WebSocket  │  • Quản lý thiết bị       │
└──────────────────┴──────────────────────┴───────────────────────────┘
```

### Hai chế độ truy cập

| Chế độ | URL | Tính năng |
|:---:|:---|:---|
| 🏠 **Local (Offline)** | `http://192.168.4.1` | Xem sensor, điều khiển cơ bản — trực tiếp qua ESP32 |
| 🌍 **Full (Online)** | Domain/IP server | Auth, lịch sử, lịch tưới, thông báo, smart logic |

---

## ⚙️ Tech Stack

<details>
<summary><strong>🔌 Firmware (ESP32)</strong></summary>

| Thư viện | Mục đích |
|:---|:---|
| **PlatformIO** | Build system |
| **Arduino Framework** | ESP32 core |
| **FreeRTOS** | Dual-core task management |
| **ESPAsyncWebServer** | Async HTTP + WebSocket server |
| **PubSubClient** | MQTT client (CoreIoT / ThingsBoard) |
| **ArduinoJson** | JSON serialization / deserialization |
| **LittleFS** | Lưu cấu hình WiFi, MQTT trên flash |
| **DHT** | Cảm biến nhiệt độ & độ ẩm không khí |
| **Adafruit NeoPixel** | Điều khiển LED RGB |
| **RTClib (DS3231)** | Đồng hồ thời gian thực |

</details>

<details>
<summary><strong>☁️ Backend (NestJS)</strong></summary>

| Thư viện | Mục đích |
|:---|:---|
| **NestJS** | Framework Node.js chính |
| **TypeScript** | Static typing |
| **PostgreSQL** | Cơ sở dữ liệu quan hệ |
| **Socket.io** | Real-time WebSocket |
| **JWT + Bcrypt** | Xác thực & bảo mật |
| **MQTT** | Bridge kết nối ESP32 |
| **Nodemailer** | Gửi email SMTP |
| **node-cron** | Lập lịch tưới tự động |

</details>

<details>
<summary><strong>🖥️ Frontend (Next.js)</strong></summary>

| Thư viện | Mục đích |
|:---|:---|
| **Next.js 14** | App Router framework |
| **TypeScript** | Static typing |
| **Tailwind CSS** | Utility-first styling |
| **Socket.io Client** | Nhận data real-time |
| **Axios** | HTTP client |

</details>

---

## 📁 Cấu trúc dự án

```
Project-Greenhouse-IOT/
│
├── 📂 src/                         ← ESP32 firmware (C++)
│   ├── main.cpp                    ← Entry point, khởi tạo tasks
│   ├── sensor_handler.cpp          ← Đọc DHT, soil, LDR
│   ├── actuator_handler.cpp        ← Điều khiển relay, pump, fan
│   ├── App_Tasks.cpp               ← Định nghĩa FreeRTOS tasks
│   ├── scheduler.cpp               ← Cooperative scheduler Core 1
│   ├── iot_bridge.cpp              ← MQTT telemetry & RPC handler
│   ├── webserver_greenhouse.cpp    ← Async WebSocket server
│   ├── wifi_greenhouse.cpp         ← WiFi STA + AP mode
│   ├── lcd_display.cpp             ← Hiển thị LCD I2C
│   └── ...
│
├── 📂 include/                     ← Header files (.h)
├── 📂 lib/                         ← Thư viện bên thứ ba
├── 📂 data/                        ← Web UI nhúng trong ESP32
│   ├── pages/                      ← HTML pages (dashboard, control...)
│   ├── js/                         ← JavaScript modules
│   └── css/                        ← Stylesheets
│
├── 📂 backend/                     ← NestJS Backend
│   └── src/
│       ├── main.ts
│       ├── app.module.ts
│       ├── 📂 config/              ← Database, MQTT config
│       ├── 📂 database/migrations/ ← SQL init scripts
│       ├── 📂 common/              ← Guards, decorators
│       ├── 📂 models/devices/      ← Factory Method products
│       ├── 📂 observers/           ← Observer pattern (notifiers)
│       ├── 📂 strategies/          ← Strategy pattern (irrigation)
│       ├── 📂 services/            ← AlertMonitor, DeviceFactory...
│       └── 📂 modules/             ← auth, sensors, devices, schedules...
│
└── 📂 frontend/                    ← Next.js Frontend
    └── src/
        ├── middleware.ts           ← Auth guard
        ├── 📂 app/(app)/           ← Protected pages
        │   ├── dashboard/
        │   ├── charts/
        │   ├── control/
        │   ├── schedule/
        │   ├── threshold/
        │   ├── smartlogic/
        │   ├── devices/
        │   ├── logs/
        │   ├── notify/
        │   └── account/
        ├── 📂 hooks/               ← useAuth, useSensors, useWebSocket
        ├── 📂 lib/                 ← API client, auth helpers
        └── 📂 types/               ← TypeScript interfaces
```

---

## 🧩 Design Patterns

### 🏭 Factory Method — FR11 (Quản lý thiết bị)

> Cho phép đăng ký thiết bị IoT mới mà **không cần sửa mã nguồn lõi**.

```
device.base.ts               ←  Abstract Product
├── sensor.device.ts         ←  ConcreteProduct: cảm biến (DHT, soil, LDR)
├── actuator.device.ts       ←  ConcreteProduct: bơm, quạt, đèn relay
└── display.device.ts        ←  ConcreteProduct: LCD I2C, NeoPixel

device-factory.service.ts    ←  Creator (Factory Method)
```

---

### 👁️ Observer Pattern — FR10 (Thông báo tự động)

> Phân tách hoàn toàn logic **phát hiện sự cố** và **gửi thông báo**.

```
alert-monitor.service.ts              ←  Subject (setInterval 30s)
  │
  ├── telegram.notifier.ts            ←  Observer: retry 3 lần, cooldown 10'
  ├── email.notifier.ts               ←  Observer: SMTP
  ├── websocket.notifier.ts           ←  Observer: push real-time
  └── activity.logger.ts             ←  Observer: ghi log vào DB

notification-observer.interface.ts   ←  Observer interface
```

---

### 🔀 Strategy Pattern — FR06/07/08 (Logic tưới thông minh)

> Cho phép **chuyển đổi linh hoạt** giữa các chiến lược tưới mà không thay đổi logic gốc.

```
irrigation-context.service.ts             ←  Context

irrigation-strategy.interface.ts          ←  Strategy interface
├── threshold-irrigation.strategy.ts      ←  Tưới theo ngưỡng độ ẩm đất
├── schedule-irrigation.strategy.ts       ←  Tưới theo lịch cố định
└── weather-irrigation.strategy.ts        ←  Tưới theo dự báo thời tiết
```

---

## ✅ Functional Requirements

| FR | Tính năng | Trạng thái | Ghi chú |
|:---:|:---|:---:|:---|
| FR01 | Hiển thị sensor real-time | ✅ | ESP32 WebSocket + Backend Socket.io |
| FR02 | Giám sát sức khỏe thiết bị | ✅ | WiFi RSSI, sensor status |
| FR03 | Lịch sử & xuất dữ liệu CSV | ✅ | PostgreSQL time-series |
| FR04 | Nhật ký hoạt động | ✅ | ActivityLogger Observer |
| FR05 | Điều khiển thủ công | ✅ | WebSocket RPC → ESP32 |
| FR06 | Tưới theo ngưỡng độ ẩm | ✅ | ThresholdIrrigationStrategy |
| FR07 | Lập lịch tưới tự động | ✅ | ScheduleIrrigationStrategy + node-cron |
| FR08 | Smart Logic thời tiết | ✅ | WeatherIrrigationStrategy + OpenWeather API |
| FR09 | Xác thực & phiên làm việc | ✅ | JWT + Bcrypt |
| FR10 | Thông báo Telegram / Email | ✅ | Observer Pattern |
| FR11 | Quản lý thiết bị IoT | ✅ | Factory Method Pattern |
| FR12 | Auto mode điều khiển | ✅ | autoControlPump/Fan/LED trong ESP32 |
| FR13 | Gửi telemetry định kỳ | ✅ | Task_SendTelemetry mỗi 5s |
| FR14 | Watchdog máy bơm | ✅ | Task_PumpWatchdog — force OFF khi timeout |

---

## 🔧 Cài đặt & Chạy

### Yêu cầu hệ thống

| Thành phần | Phiên bản tối thiểu |
|:---|:---|
| Node.js | >= 18.x |
| PostgreSQL | >= 14 |
| PlatformIO | Mới nhất |
| MQTT Broker | Mosquitto hoặc CoreIoT |

**Phần cứng ESP32:** DHT11/22 · LDR · Soil Moisture Sensor · Relay Module · NeoPixel · LCD I2C · RTC DS3231

---

### 1️⃣ ESP32 Firmware

```bash
# Clone repository
git clone https://github.com/your-repo/Project-Greenhouse-IOT.git
cd Project-Greenhouse-IOT

# Build và flash firmware lên ESP32
pio run --target upload

# Upload web UI lên LittleFS
pio run --target uploadfs

# Mở Serial Monitor để debug
pio device monitor --baud 115200
```

> 💡 Sau khi flash, kết nối WiFi **`Greenhouse-AP`** (password: `12345678`), vào `http://192.168.4.1` để cấu hình WiFi & MQTT.

---

### 2️⃣ Backend (NestJS)

```bash
cd backend

# Cài đặt dependencies
npm install

# Tạo file cấu hình môi trường
cp .env.example .env
# ✏️  Chỉnh sửa .env theo hướng dẫn bên dưới

# Khởi tạo cơ sở dữ liệu
psql -U postgres -d greenhouse -f src/database/migrations/001_init.sql

# Chạy môi trường development
npm run start:dev

# Build & chạy production
npm run build && npm run start:prod
```

---

### 3️⃣ Frontend (Next.js)

```bash
cd frontend

# Cài đặt dependencies
npm install

# Tạo file cấu hình môi trường
cp .env.example .env.local
# ✏️  Điền NEXT_PUBLIC_API_URL và NEXT_PUBLIC_WS_URL

# Chạy môi trường development
npm run dev

# Build & chạy production
npm run build && npm run start
```

---

## 🌍 Biến môi trường

### `backend/.env`

```env
# ── Database ──────────────────────────────────────────
DATABASE_URL=postgresql://postgres:password@localhost:5432/greenhouse

# ── Authentication ────────────────────────────────────
JWT_SECRET=your-super-secret-jwt-key-min-32-chars
JWT_EXPIRES_IN=24h

# ── MQTT Broker ───────────────────────────────────────
MQTT_BROKER_URL=mqtt://localhost:1883
MQTT_USERNAME=
MQTT_PASSWORD=

# ── Telegram ──────────────────────────────────────────
TELEGRAM_ENCRYPT_KEY=your-32-char-encryption-key

# ── Email SMTP ────────────────────────────────────────
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USER=your@gmail.com
SMTP_PASS=your-app-password

# ── App ───────────────────────────────────────────────
PORT=3001
CORS_ORIGIN=http://localhost:3000
```

### `frontend/.env.local`

```env
NEXT_PUBLIC_API_URL=http://localhost:3001
NEXT_PUBLIC_WS_URL=http://localhost:3001
```

---

## 📡 API Endpoints

### 🔐 Auth — `/api/auth`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `POST` | `/login` | Đăng nhập → trả về JWT token |
| `GET` | `/check-session` | Kiểm tra token còn hợp lệ |
| `POST` | `/logout` | Đăng xuất |

### 👤 Users — `/api/user`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `GET` | `/profile` | Lấy thông tin người dùng |
| `PUT` | `/profile` | Cập nhật profile và mật khẩu |

### 📊 Sensors — `/api/sensors`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `GET` | `/history?range=24h` | Lịch sử sensor (`24h` / `7d` / `30d`) |
| `GET` | `/export?format=csv` | Xuất dữ liệu CSV / Excel |

### 🔌 Devices — `/api/devices`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `GET` | `/` | Danh sách thiết bị |
| `POST` | `/` | Đăng ký thiết bị mới |
| `DELETE` | `/:id` | Xóa thiết bị |

### 🕐 Schedules — `/api/schedules`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `GET` | `/` | Danh sách lịch tưới |
| `POST` | `/` | Tạo lịch mới |
| `PUT` | `/:id` | Cập nhật lịch |
| `DELETE` | `/:id` | Xóa lịch |

### 🔔 Notifications — `/api/notifications`

| Method | Endpoint | Mô tả |
|:---:|:---|:---|
| `POST` | `/telegram/test` | Gửi tin nhắn test Telegram |
| `POST` | `/telegram/save` | Lưu cấu hình Telegram |
| `POST` | `/email/test` | Gửi email test |
| `POST` | `/email/save` | Lưu cấu hình Email |

---

## ⚡ Kiến trúc Dual-Core ESP32

```
┌──────────────────────────────────┬───────────────────────────────────┐
│         CORE 0 — FreeRTOS        │        CORE 1 — Scheduler         │
│                                  │                                   │
│  greenhouse_wifi_task            │  Task_ReadAirSensors   (2s)       │
│  greenhouse_coreiot_task  ◄──┐   │  Task_ReadSoilSensor   (5s)       │
│  greenhouse_webserver_task ─►│   │  Task_SendTelemetry    (5s)       │
│                              │   │  Task_AutoControl      (1s)       │
│  ┌──────────────────────┐   │    │  Task_ProcessRpc       (event)    │
│  │  xTelemetryQueue     │◄──┘    │  Task_PumpWatchdog     (1s)       │
│  │  xRpcCommandQueue    │──────► │  Task_UpdateLCD        (1s)       │
│  └──────────────────────┘        │  Task_ScanButtons      (50ms)     │
│                                  │                                   │
└──────────────────────────────────┴───────────────────────────────────┘
           ▲  MQTT / WebSocket                    ▼  GPIO / I2C / SPI
           │                                      │
    ☁️  Backend Server                  🔌 Sensors & Actuators


**Được xây dựng với ❤️ bởi nhóm EcoGreen**

[![MIT License](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

</div>
