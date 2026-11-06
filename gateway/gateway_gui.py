"""
gateway_gui.py - Giao diện Giám sát & Điều khiển Cục bộ trên IoT Gateway PC
Đồ án Môn học: Internet of Things (CO3061 / CO3062) - ĐH Bách Khoa TP.HCM (HCMUT)
Mã yếu tố công nghệ: T10 (1.0 Điểm)
Chức năng: Giao diện Desktop GUI đồ họa trực quan hiển thị dữ liệu cảm biến, phân tích AI và điều khiển thiết bị local.
Sinh viên thực hiện: Phạm Công Võ (MSSV: 2313946)
"""

import sys
import os
import time
import tkinter as tk
from tkinter import ttk, scrolledtext

try:
    from gateway_core import LocalIoTGatewayCore
except ImportError:
    from gateway.gateway_core import LocalIoTGatewayCore


class GatewayGUI:
    """
    Hiện thực mã T10:
      - Thiết kế giao diện Desktop GUI hoàn chỉnh bằng Tkinter (theme Dark Mode công nghệ).
      - Hiển thị 4 thông số vi khí hậu thời gian thực từ mạng local.
      - Hiển thị bảng phân tích Trí tuệ Nhân tạo từ Gateway AI (T09).
      - Bảng nút bấm điều khiển thiết bị cục bộ: Bơm, Quạt, Chế độ Auto/Manual.
      - Màn hình Event Logs ghi nhận toàn bộ lưu lượng đồng bộ với CoreIOT Cloud.
    """

    def __init__(self, root, gateway_core=None):
        self.root = root

        # Khởi tạo Gateway Core (T08) nếu chưa có
        if gateway_core:
            self.gateway = gateway_core
            self.gateway.log_callback = self.append_log
            self.gateway.update_callback = self.trigger_gui_update
        else:
            self.gateway = LocalIoTGatewayCore(
                log_callback=self.append_log,
                update_callback=self.trigger_gui_update
            )

        self.setup_ui()

    def setup_ui(self):
        self.root.title("HCMUT IoT Gateway GUI — Smart Greenhouse (Mã T10)")
        self.root.geometry("980x720")
        self.root.minsize(900, 650)
        self.root.configure(bg="#0f172a")

        # Style cấu hình
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#0f172a")
        style.configure("TLabel", background="#0f172a", foreground="#f8fafc")
        style.configure("Header.TLabel", font=("Segoe UI", 16, "bold"), foreground="#38bdf8")
        style.configure("SubHeader.TLabel", font=("Segoe UI", 9), foreground="#94a3b8")

        # =====================================================================
        # 1. TOP HEADER: THÔNG TIN DỰ ÁN & STATUS BADGES
        # =====================================================================
        top_frame = ttk.Frame(self.root, padding="15 12 15 10")
        top_frame.pack(fill=tk.X)

        title_box = ttk.Frame(top_frame)
        title_box.pack(side=tk.LEFT)
        ttk.Label(title_box, text="🌿 HCMUT LOCAL IOT GATEWAY GUI (T10)", style="Header.TLabel").pack(anchor=tk.W)
        ttk.Label(title_box, text="Môn học: Internet of Things (CO3061) | GVHD: TS. Lê Trọng Nhân | SV: Phạm Công Võ (MSSV: 2313946)",
                  style="SubHeader.TLabel").pack(anchor=tk.W)

        badge_box = ttk.Frame(top_frame)
        badge_box.pack(side=tk.RIGHT)
        self.lbl_badge_esp = tk.Label(badge_box, text="ESP32: SCANNING", font=("Segoe UI", 9, "bold"),
                                      bg="#334155", fg="#f8fafc", padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_esp.pack(side=tk.LEFT, padx=5)

        self.lbl_badge_mqtt = tk.Label(badge_box, text="COREIOT: CONNECTING", font=("Segoe UI", 9, "bold"),
                                       bg="#334155", fg="#f8fafc", padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_mqtt.pack(side=tk.LEFT, padx=5)

        # =====================================================================
        # 2. BẢNG THẺ ĐO LƯỜNG CẢM BIẾN THỜI GIAN THỰC (4 CARDS)
        # =====================================================================
        cards_frame = ttk.Frame(self.root, padding="15 5 15 10")
        cards_frame.pack(fill=tk.X)
        for i in range(4):
            cards_frame.columnconfigure(i, weight=1)

        self.card_temp = self.create_metric_card(cards_frame, 0, "🌡️ NHIỆT ĐỘ KHÍ", "27.5 °C", "#ef4444")
        self.card_hum = self.create_metric_card(cards_frame, 1, "💧 ĐỘ ẨM KHÔNG KHÍ", "65.0 %", "#06b6d4")
        self.card_soil = self.create_metric_card(cards_frame, 2, "🌱 ĐỘ ẨM ĐẤT", "50.0 %", "#10b981")
        self.card_light = self.create_metric_card(cards_frame, 3, "☀️ CƯỜNG ĐỘ SÁNG", "720 Lux", "#f59e0b")

        # =====================================================================
        # 3. BẢNG HIỂN THỊ PHÂN TÍCH AI TRÊN GATEWAY (MÃ T09)
        # =====================================================================
        ai_frame = tk.LabelFrame(self.root, text=" 🧠 KẾT QUẢ PHÂN TÍCH TRÍ TUỆ NHÂN TẠO TRÊN GATEWAY (MÃ T09) ",
                                 font=("Segoe UI", 10, "bold"), bg="#1e293b", fg="#38bdf8", padx=15, pady=10)
        ai_frame.pack(fill=tk.X, padx=15, pady=6)

        ai_cols = ttk.Frame(ai_frame)
        ai_cols.pack(fill=tk.X)
        ai_cols.columnconfigure(0, weight=1)
        ai_cols.columnconfigure(1, weight=1)
        ai_cols.columnconfigure(2, weight=1)

        # Cột 1: Điểm Dị thường
        c1 = tk.Frame(ai_cols, bg="#1e293b")
        c1.grid(row=0, column=0, sticky="nsew", padx=5)
        tk.Label(c1, text="ĐIỂM DỊ THƯỜNG AI (ANOMALY SCORE):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_score = tk.Label(c1, text="0.05", font=("Segoe UI", 20, "bold"), bg="#1e293b", fg="#22c55e")
        self.lbl_ai_score.pack(anchor=tk.W)
        self.lbl_ai_badge = tk.Label(c1, text="STATUS: NORMAL", font=("Segoe UI", 10, "bold"), bg="#14532d", fg="#86efac", padx=8, pady=2)
        self.lbl_ai_badge.pack(anchor=tk.W, pady=2)

        # Cột 2: Dự báo nhiệt độ 30 phút
        c2 = tk.Frame(ai_cols, bg="#1e293b")
        c2.grid(row=0, column=1, sticky="nsew", padx=5)
        tk.Label(c2, text="DỰ BÁO NHIỆT ĐỘ (30 PHÚT TỚI):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_forecast = tk.Label(c2, text="28.0 °C", font=("Segoe UI", 20, "bold"), bg="#1e293b", fg="#38bdf8")
        self.lbl_ai_forecast.pack(anchor=tk.W)
        tk.Label(c2, text="Mô hình Hồi quy Ngoại suy Chuỗi thời gian", font=("Segoe UI", 9, "italic"), bg="#1e293b", fg="#64748b").pack(anchor=tk.W)

        # Cột 3: Khuyến nghị tưới thông minh
        c3 = tk.Frame(ai_cols, bg="#1e293b")
        c3.grid(row=0, column=2, sticky="nsew", padx=5)
        tk.Label(c3, text="KHUYẾN NGHỊ TƯỚI THÔNG MINH (SMART AI):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_irrigation = tk.Label(c3, text="Đang phân tích độ ẩm đất...", font=("Segoe UI", 10, "bold"),
                                          bg="#1e293b", fg="#f8fafc", wraplength=260, justify=tk.LEFT)
        self.lbl_ai_irrigation.pack(anchor=tk.W, pady=4)

        # =====================================================================
        # 4. BẢNG ĐIỀU KHIỂN THIẾT BỊ CỤC BỘ TRÊN GATEWAY (MÃ T10)
        # =====================================================================
        ctrl_frame = tk.LabelFrame(self.root, text=" ⚡ BẢNG ĐIỀU KHIỂN THIẾT BỊ CỤC BỘ (MÃ T10 CONTROLS) ",
                                   font=("Segoe UI", 10, "bold"), bg="#1e293b", fg="#10b981", padx=15, pady=10)
        ctrl_frame.pack(fill=tk.X, padx=15, pady=6)

        btn_box = ttk.Frame(ctrl_frame)
        btn_box.pack(fill=tk.X)

        self.btn_pump = tk.Button(btn_box, text="BƠM NƯỚC: OFF", font=("Segoe UI", 10, "bold"),
                                  bg="#334155", fg="#f8fafc", activebackground="#475569", width=18, pady=6,
                                  command=self.toggle_pump)
        self.btn_pump.pack(side=tk.LEFT, padx=6)

        self.btn_fan = tk.Button(btn_box, text="QUẠT LÀM MÁT: OFF", font=("Segoe UI", 10, "bold"),
                                 bg="#334155", fg="#f8fafc", activebackground="#475569", width=18, pady=6,
                                 command=self.toggle_fan)
        self.btn_fan.pack(side=tk.LEFT, padx=6)

        self.btn_mode = tk.Button(btn_box, text="CHẾ ĐỘ: AUTO", font=("Segoe UI", 10, "bold"),
                                  bg="#0284c7", fg="#ffffff", activebackground="#0369a1", width=18, pady=6,
                                  command=self.toggle_mode)
        self.btn_mode.pack(side=tk.LEFT, padx=6)

        self.btn_tamper_test = tk.Button(btn_box, text="🔥 TEST HƠ LỬA AI", font=("Segoe UI", 10, "bold"),
                                         bg="#b91c1c", fg="#ffffff", activebackground="#991b1b", width=18, pady=6,
                                         command=self.trigger_flame_test)
        self.btn_tamper_test.pack(side=tk.RIGHT, padx=6)

        # =====================================================================
        # 5. CONSOLE NHẬT KÝ SỰ KIỆN ĐỒNG BỘ THỜI GIAN THỰC
        # =====================================================================
        log_frame = tk.LabelFrame(self.root, text=" 📋 NHẬT KÝ LƯU LƯỢNG ĐỒNG BỘ GATEWAY ↔ COREIOT CLOUD ",
                                  font=("Segoe UI", 9, "bold"), bg="#0f172a", fg="#94a3b8", padx=10, pady=6)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=8)

        self.txt_log = scrolledtext.ScrolledText(log_frame, bg="#020617", fg="#38bdf8", insertbackground="#ffffff",
                                                font=("Consolas", 9), height=7)
        self.txt_log.pack(fill=tk.BOTH, expand=True)

    def create_metric_card(self, parent, col, title, initial_val, accent_color):
        card = tk.Frame(parent, bg="#1e293b", padx=12, pady=10, relief=tk.FLAT)
        card.grid(row=0, column=col, sticky="nsew", padx=5)

        tk.Label(card, text=title, font=("Segoe UI", 9, "bold"), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        val_lbl = tk.Label(card, text=initial_val, font=("Segoe UI", 20, "bold"), bg="#1e293b", fg=accent_color)
        val_lbl.pack(anchor=tk.W, pady=2)
        return val_lbl

    def trigger_gui_update(self):
        try:
            self.root.after(0, self.update_display)
        except Exception:
            pass

    def update_display(self):
        # 1. Đo lường
        t = self.gateway.telemetry
        self.card_temp.config(text=f"{t['temperature']:.1f} °C")
        self.card_hum.config(text=f"{t['humidity']:.1f} %")
        self.card_soil.config(text=f"{t['soilMoisture']:.0f} %")
        self.card_light.config(text=f"{t['lightLux']:.0f} Lux")

        # 2. AI Insights (T09)
        ai = self.gateway.ai_results
        score = ai["anomaly_score"]
        self.lbl_ai_score.config(text=f"{score:.2f}")
        if score >= 0.60:
            self.lbl_ai_score.config(fg="#ef4444")
            self.lbl_ai_badge.config(text=f"CẢNH BÁO: {ai['anomaly_type']}", bg="#7f1d1d", fg="#fca5a5")
        else:
            self.lbl_ai_score.config(fg="#22c55e")
            self.lbl_ai_badge.config(text="TRẠNG THÁI: BÌNH THƯỜNG", bg="#14532d", fg="#86efac")

        self.lbl_ai_forecast.config(text=f"{ai['projected_temp_30m']:.1f} °C")
        self.lbl_ai_irrigation.config(text=ai["irrigation_recommendation"])

        # 3. Nút điều khiển
        p_on = t["pumpState"]
        self.btn_pump.config(
            text=f"BƠM NƯỚC: {'ON' if p_on else 'OFF'}",
            bg="#16a34a" if p_on else "#334155"
        )
        f_on = t["fanState"]
        self.btn_fan.config(
            text=f"QUẠT LÀM MÁT: {'ON' if f_on else 'OFF'}",
            bg="#0284c7" if f_on else "#334155"
        )
        is_auto = t["autoMode"]
        self.btn_mode.config(
            text=f"CHẾ ĐỘ: {'AUTO' if is_auto else 'MANUAL'}",
            bg="#0284c7" if is_auto else "#f59e0b"
        )

        # 4. Badges
        if self.gateway.esp32_connected:
            self.lbl_badge_esp.config(text="ESP32: CONNECTED (OK)", bg="#16a34a")
        else:
            self.lbl_badge_esp.config(text="ESP32: SIMULATED / OFFLINE", bg="#d97706")

        if self.gateway.coreiot_connected:
            self.lbl_badge_mqtt.config(text="COREIOT: ONLINE (MQTT)", bg="#16a34a")
        else:
            self.lbl_badge_mqtt.config(text="COREIOT: DISCONNECTED", bg="#dc2626")

    def append_log(self, text: str):
        try:
            self.root.after(0, self._insert_log, text)
        except Exception:
            pass

    def _insert_log(self, text):
        if hasattr(self, "txt_log") and self.txt_log:
            self.txt_log.insert(tk.END, text + "\n")
            self.txt_log.see(tk.END)

    # Thao tác điều khiển
    def toggle_pump(self):
        new_state = not self.gateway.telemetry["pumpState"]
        self.gateway.telemetry["pumpState"] = new_state
        self.gateway.log(f"[LOCAL GUI T10] Đổi Bơm -> {'BẬT' if new_state else 'TẮT'}")
        self.gateway.send_command_to_esp32("p")

    def toggle_fan(self):
        new_state = not self.gateway.telemetry["fanState"]
        self.gateway.telemetry["fanState"] = new_state
        self.gateway.log(f"[LOCAL GUI T10] Đổi Quạt -> {'BẬT' if new_state else 'TẮT'}")
        self.gateway.send_command_to_esp32("f")

    def toggle_mode(self):
        new_mode = not self.gateway.telemetry["autoMode"]
        self.gateway.telemetry["autoMode"] = new_mode
        self.gateway.log(f"[LOCAL GUI T10] Đổi Mode -> {'AUTO' if new_mode else 'MANUAL'}")
        self.gateway.send_command_to_esp32("a")

    def trigger_flame_test(self):
        self.gateway.log("[TEST] !!! KÍCH HOẠT KỊCH BẢN HƠ LỬA THỬ NGHIỆM TRÊN GATEWAY GUI !!!")
        self.gateway.telemetry["temperature"] = 46.5
        self.gateway.telemetry["humidity"] = 18.0
        self.gateway.telemetry["fanState"] = True


def main():
    root = tk.Tk()
    app = GatewayGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
