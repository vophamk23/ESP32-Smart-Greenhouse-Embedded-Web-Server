# BIO-SCADA Fog Gateway & Predictive Analytics

Thư mục này chứa dịch vụ **Fog Gateway** đa luồng chạy trên máy trạm PC / Raspberry Pi, đóng vai trò cầu nối song công giữa tầng Biên (**ESP32-S3 Cleanroom Bio-Chamber**) và tầng Đám mây (**CoreIOT / ThingsBoard**), đồng thời vận hành **AI Engine** và **Desktop SCADA GUI**.

## Kiến trúc & Chức năng chính

1. **Cầu nối Đa giao thức (`gateway_core.py`)**:
   - Đọc dữ liệu cảm biến trực tiếp từ phần cứng qua **Modbus RS485** (`minimalmodbus`), **USB-CDC Serial** (`115200 baud`), hoặc **Wi-Fi REST API** (`GET /api/status`), tự động chuyển sang chế độ giả lập telemetry khi chưa cắm phần cứng.
   - Đóng gói và đẩy chuỗi dữ liệu vi khí hậu + chỉ số phân tích AI lên CoreIOT MQTT (`v1/devices/me/telemetry`) theo chu kỳ 2 giây.
   - Lắng nghe lệnh RPC hai chiều từ Cloud (`setPump`, `setFan`, `setMode`) để điều phối xuống bộ chấp hành thực tế (Bơm phun sương tạo ẩm, Quạt đối lưu phòng sạch).

2. **Bộ suy luận AI trên Gateway (`gateway_ai.py`)**:
   - **Phát hiện dị thường đa biến (Z-Score + Derivative Drift):** Phát hiện tăng/giảm nhiệt đột ngột, sốc nhiệt hoặc nguy cơ khô giá thể (`DROUGHT_STRESS` / suy giảm độ ẩm phôi).
   - **Dự báo xu hướng nhiệt độ 30 phút (OLS Linear Regression):** Tính tốc độ biến thiên nhiệt (°C/phút) trên cửa sổ trượt `window_size=20` mẫu và độ tin cậy $R^2$.
   - **Tư vấn phun sương thông minh:** Tính toán thời lượng kích hoạt bơm phun sương tạo ẩm (15–120 giây) dựa trên mức hụt ẩm giá thể và nhiệt độ buồng nuôi cấy.

3. **Giao diện Giám sát Công nghiệp BIO-SCADA (`gateway_gui.py`)**:
   - Giao diện Tkinter Dark Theme tích hợp biểu đồ vector Sparklines, thước đo LED Anomaly Threat Gauge, khối điều khiển Actuator Matrix và Terminal lọc log gói tin MQTT / Serial / AI theo thời gian thực.

## Hướng dẫn khởi chạy

Cài đặt thư viện phụ thuộc (Python 3.10+):
```bash
pip install pyserial paho-mqtt requests numpy minimalmodbus
```

Chạy chế độ giao diện Desktop SCADA (mặc định):
```bash
python gateway_app.py
```

Chạy chế độ Headless Daemon (không GUI, phù hợp cho Raspberry Pi / Server):
```bash
python gateway_app.py --headless
```

Kiểm tra cấu hình kết nối tại [config.json](./config.json).
