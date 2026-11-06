"""
gateway_app.py - Điểm khởi động Master IoT Gateway (Chạy đồng thời T08, T09, T10)
Đồ án Môn học: Internet of Things (CO3061 / CO3062) - ĐH Bách Khoa TP.HCM (HCMUT)
Phân rã kiến trúc mô-đun chuẩn hóa:
  - T08: gateway_core.py  (IoT Gateway Core Daemon, kết nối ESP32 & CoreIOT Cloud 2 chiều)
  - T09: gateway_ai.py    (Chức năng Trí tuệ Nhân tạo phân tích dữ liệu trên Gateway)
  - T10: gateway_gui.py   (Giao diện Desktop GUI giám sát & điều khiển cục bộ trên PC)
Sinh viên thực hiện: Phạm Công Võ (MSSV: 2313946)
"""

import sys
import time
import tkinter as tk

try:
    from gateway_core import LocalIoTGatewayCore
    from gateway_gui import GatewayGUI
except ImportError:
    from gateway.gateway_core import LocalIoTGatewayCore
    from gateway.gateway_gui import GatewayGUI


def main():
    if "--headless" in sys.argv:
        print("=================================================================")
        print("  🌿 HCMUT LOCAL IOT GATEWAY — HEADLESS MODE (MÃ T08 & T09)      ")
        print("  Đồng bộ dữ liệu ESP32 ↔ CoreIOT Cloud và chạy AI Gateway       ")
        print("=================================================================")
        gateway_core = LocalIoTGatewayCore()
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            gateway_core.running = False
            print("\nĐã dừng Gateway Core an toàn.")
    else:
        root = tk.Tk()
        # Khởi chạy Gateway Core (T08) lồng trong Giao diện Desktop GUI (T10)
        app = GatewayGUI(root)
        root.mainloop()


if __name__ == "__main__":
    main()
