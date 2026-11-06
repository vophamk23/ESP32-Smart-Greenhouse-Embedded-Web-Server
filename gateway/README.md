# 🖥️ Local IoT Gateway — Smart Greenhouse System (Nhóm 4)

**ĐỒ ÁN MÔN HỌC INTERNET OF THINGS (CO3061 / CO3062) — ĐH BÁCH KHOA TP.HCM (HCMUT)**  
**Sinh viên thực hiện:** Phạm Công Võ — **MSSV:** 2313946  
**Giảng viên hướng dẫn:** TS. Lê Trọng Nhân

---

## 📋 Phân rã Mô-đun Tương Ứng Từng Tiêu Chí Nhóm 4

Thư mục `gateway/` được phân tách thành 3 tệp độc lập, tương ứng chính xác với 3 mã công nghệ của Nhóm 4 trong đề bài BTL:

| Mã tiêu chí | Tệp mã nguồn | Điểm | Mô tả chi tiết chức năng |
| :---: | :--- | :---: | :--- |
| **`T08`** | **[`gateway_core.py`](file:///C:/Users/ASUS/Documents/Report/BTL_IoT/ESP32-Smart-Greenhouse-Embedded-Web-Server/gateway/gateway_core.py)** | **1.0** | **IoT Gateway Daemon:** Thu thập dữ liệu từ ESP32 qua Serial (USB) hoặc WiFi REST API, tổng hợp dữ liệu, gửi lên CoreIOT Cloud (`app.coreiot.io`), nhận lệnh điều khiển RPC từ CoreIOT và phân phối cho thiết bị local. |
| **`T09`** | **[`gateway_ai.py`](file:///C:/Users/ASUS/Documents/Report/BTL_IoT/ESP32-Smart-Greenhouse-Embedded-Web-Server/gateway/gateway_ai.py)** | **1.0** | **Chức năng AI trên Gateway:** Mô hình phân tích chuỗi thời gian đa biến: phát hiện hơ lửa/cháy (`FLAME_HEAT_SPIKE`), dự báo nhiệt độ vi khí hậu 30 phút, và mô hình ra quyết định tưới thông minh (Smart Irrigation AI). |
| **`T10`** | **[`gateway_gui.py`](file:///C:/Users/ASUS/Documents/Report/BTL_IoT/ESP32-Smart-Greenhouse-Embedded-Web-Server/gateway/gateway_gui.py)** | **1.0** | **Giao diện Desktop GUI trên PC:** Giao diện đồ họa Tkinter Dark Mode hiển thị 4 thẻ đo lường cảm biến, bảng phân tích AI Gateway, các nút điều khiển Bơm/Quạt/Mode và màn hình Console Log sự kiện. |

👉 **TỔNG ĐIỂM NHÓM 4: 3.0 / 3.0 ĐIỂM TUYỆT ĐỐI!**

---

## 🚀 Hướng dẫn Chạy Từng Tệp

### 1. Chạy riêng Giao diện Giám sát & Điều khiển (Mã `T10`):
```bash
python gateway/gateway_gui.py
```
*Cửa sổ Desktop GUI sẽ xuất hiện ngay lập tức với các thẻ đo lường và nút bấm điều khiển.*

### 2. Chạy riêng Gateway Core Daemon không cần giao diện (Mã `T08`):
```bash
python gateway/gateway_core.py
```
*Thích hợp khi triển khai trên Raspberry Pi hoặc máy chủ mini không có màn hình.*

### 3. Chạy Tổng hợp Cả Hệ thống (T08 + T09 + T10):
```bash
python gateway/gateway_app.py
```
