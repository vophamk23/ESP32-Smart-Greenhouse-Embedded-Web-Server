"""
# Cleanroom Bio-Chamber Edge-AI — Local Fog IoT Gateway Launcher
#
# gateway_app.py - Điểm khởi động dịch vụ Fog Gateway hợp nhất:
#   - gateway_core.py : Fog Gateway Core Daemon (Modbus/Serial/HTTP <-> CoreIOT MQTT)
#   - gateway_ai.py   : Predictive Analytics (Z-Score Anomaly, 30-min OLS Forecast, Smart Mist AI)
#   - gateway_gui.py  : Desktop BIO-SCADA Mission Control (Tkinter Dark Theme)
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
        print("  BIOCHAMBER LOCAL IOT GATEWAY - HEADLESS MODE                   ")
        print("  Dong bo du lieu ESP32-S3 <-> CoreIOT Cloud & Fog AI Engine    ")
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
        # Khởi chạy Gateway Core lồng trong Giao diện Desktop BIO-SCADA GUI
        app = GatewayGUI(root)
        root.mainloop()


if __name__ == "__main__":
    main()
