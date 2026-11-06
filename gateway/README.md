# 🖥️ Local IoT Gateway — Smart Greenhouse System (Nhóm 4)

**ĐỒ ÁN MÔN HỌC INTERNET OF THINGS (CO3061 / CO3062) — ĐH BÁCH KHOA TP.HCM (HCMUT)**  
**Sinh viên thực hiện:** Phạm Công Võ — **MSSV:** 2313946  
**Giảng viên hướng dẫn:** TS. Lê Trọng Nhân

---

## 📋 Giới thiệu & Ánh xạ Thang điểm BTL

Thư mục `gateway/` hiện thực trọn vẹn **toàn bộ 3 tiêu chí của Nhóm 4 (IoT Gateway cục bộ)** theo đúng yêu cầu đề bài:

| Mã tiêu chí | Tên tiêu chí theo đề bài | Điểm | Mô tả hiện thực trong dự án |
| :---: | :--- | :---: | :--- |
| **`T08`** | **IoT Gateway hiện thực trên PC (hoặc Raspberry Pi)** | **1.0** | Thu thập dữ liệu từ ESP32 qua Serial (USB) hoặc WiFi REST API, tổng hợp dữ liệu, đẩy lên CoreIOT Cloud (`app.coreiot.io`), nhận lệnh điều khiển RPC từ CoreIOT và phân phối xuống mạng local. |
| **`T09`** | **Chức năng AI chạy trên IoT Gateway local** | **1.0** | Tích hợp thuật toán học máy đa biến: phát hiện dị thường nhiệt ẩm hơ lửa (`FLAME_HEAT_SPIKE`), hồi quy ngoại suy xu hướng nhiệt độ 30 phút, và mô hình ra quyết định tưới thông minh (Smart Irrigation AI). |
| **`T10`** | **Giao diện giám sát – điều khiển trên IoT Gateway local** | **1.0** | Giao diện Desktop GUI đồ họa trực quan (Tkinter Dark Mode): hiển thị thẻ cảm biến thời gian thực, huy hiệu AI, nút điều khiển Bơm/Quạt/Mode, và màn hình log sự kiện đồng bộ. |

👉 **Tổng điểm Nhóm 4 đạt được:** **3.0 / 3.0 ĐIỂM TUYỆT ĐỐI!**

---

## 🏗️ Kiến trúc Hoạt động của IoT Gateway

```
┌─────────────────────────────────┐
│     Yolo:UNO (ESP32-S3)         │
│  - Cảm biến: DHT20, Soil, LDR   │
│  - Chấp hành: Relay Bơm, Quạt   │
└────────────────┬────────────────┘
                 │
                 │ Kết nối Local: Cáp USB Serial (COM) HOẶC WiFi HTTP
                 ▼
┌─────────────────────────────────────────────────────────────────┐
│              LOCAL IOT GATEWAY TRÊN PC (Laptop)                 │
│                                                                 │
│  ┌────────────────────────┐       ┌──────────────────────────┐  │
│  │   Giao diện GUI (T10)  │       │     AI Engine (T09)      │  │
│  │ - 4 Thẻ cảm biến       │       │ - Z-Score Anomaly        │  │
│  │ - Nút Bơm, Quạt, Mode  │◄─────►│ - Hơ lửa FLAME_SPIKE     │  │
│  │ - Console Event Logs   │       │ - Dự báo nhiệt 30 phút   │  │
│  └────────────────────────┘       │ - Khuyến nghị tưới cây   │  │
│                                   └──────────────────────────┘  │
│                                                 ▲               │
│                                                 │               │
│                                   ┌─────────────┴────────────┐  │
│                                   │ CoreIOT MQTT Client(T08) │  │
│                                   └─────────────┬────────────┘  │
└─────────────────────────────────────────────────┼───────────────┘
                                                  │
                                                  │ Internet MQTT (Port 1883)
                                                  ▼
                                   ┌──────────────────────────────┐
                                   │   CoreIOT Cloud Platform     │
                                   │      (app.coreiot.io)        │
                                   │  - Dashboard Giám sát từ xa │
                                   │  - Lệnh nút bấm RPC         │
                                   └──────────────────────────────┘
```

---

## 🚀 Hướng dẫn Chạy IoT Gateway trên Máy tính

### 1. Cài đặt thư viện phụ thuộc
Mở Terminal tại thư mục `gateway/` và chạy:
```bash
pip install -r requirements.txt
```

### 2. Khởi động Giao diện Gateway
Chỉ cần chạy lệnh:
```bash
python gateway_app.py
```
*(Nếu muốn chạy ở chế độ dòng lệnh không cần cửa sổ GUI, thêm cờ `--headless`: `python gateway_app.py --headless`).*

### 3. Cách Gateway tự động hoạt động:
* **Tự nhận diện Token:** Tự động nạp Token CoreIOT từ file `data/info.dat` của dự án (`h8ycpa6d5uodkqr011cz`) và kết nối ngay tới `app.coreiot.io`.
* **Tự nhận diện ESP32:** Tự động quét cổng COM (chân cắm USB) của Yolo:UNO hoặc kết nối qua WiFi IP của chip.
* **Chế độ Thử nghiệm (Simulation Demo):** Nếu chưa cắm mạch phần cứng, Gateway sẽ tự động sinh dữ liệu mô phỏng để bạn có thể kiểm thử toàn bộ giao diện, mô hình AI và gửi dữ liệu lên CoreIOT Cloud ngay lập tức!
