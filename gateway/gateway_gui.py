"""
# Cleanroom Bio-Chamber Edge-AI Gateway — SCADA Mission Control (Dark Theme)
#
# gateway_gui.py - Trạm Giám sát & Điều khiển Phòng sạch Nuôi cấy Vi tảo / Nấm Công nghiệp
"""

import sys
import os
import time
from collections import deque
import tkinter as tk
from tkinter import ttk

try:
    from gateway_core import LocalIoTGatewayCore
except ImportError:
    from gateway.gateway_core import LocalIoTGatewayCore


class SparklineCanvas(tk.Canvas):
    """
    Component đồ họa vẽ biểu đồ đường xu hướng thời gian thực (Vector Sparkline)
    với đường cong làm mịn (smooth curve), vùng sáng chuyển sắc (Area Glow),
    lưới tham chiếu kỹ thuật và điểm xung nhịp LED phát quang.
    """

    def __init__(self, parent, width=220, height=48, line_color="#06B6D4",
                 fill_color="#0C2434", bg_color="#111827", initial_val=27.0, **kwargs):
        super().__init__(parent, width=width, height=height, bg=bg_color,
                         highlightthickness=0, bd=0, **kwargs)
        self.w = width
        self.h = height
        self.line_color = line_color
        self.fill_color = fill_color
        self.bg_color = bg_color
        # Khởi tạo 6 điểm đầu tiên bằng giá trị ban đầu để biểu đồ hiển thị mượt ngay khi bật
        self.data = deque([float(initial_val)] * 6, maxlen=25)

    def add_point(self, val: float):
        self.data.append(float(val))
        self.redraw()

    def redraw(self):
        self.delete("all")
        pts = list(self.data)
        if len(pts) < 2:
            return

        min_v = min(pts)
        max_v = max(pts)
        span = max_v - min_v if (max_v - min_v) > 0.001 else 1.0

        y_top = 8
        y_bottom = self.h - 8
        y_mid = (y_top + y_bottom) / 2.0

        # 1. Vẽ các đường lưới tham chiếu mờ (Subtle Gridlines)
        self.create_line(6, y_top, self.w - 6, y_top, fill="#1A2538", dash=(2, 4), width=1)
        self.create_line(6, y_mid, self.w - 6, y_mid, fill="#1A2538", dash=(2, 4), width=1)
        self.create_line(6, y_bottom, self.w - 6, y_bottom, fill="#1A2538", dash=(2, 4), width=1)

        # 2. Tính tọa độ các điểm
        x_start = 8
        x_end = self.w - 8
        x_step = (x_end - x_start) / max(1, len(pts) - 1)

        coords = []
        for i, val in enumerate(pts):
            x = x_start + i * x_step
            normalized = (val - min_v) / span
            y = y_bottom - normalized * (y_bottom - y_top)
            coords.extend([x, y])

        # 3. Vẽ vùng phủ mờ dưới đường xu hướng (Area Glow Polygon)
        poly_coords = [coords[0], y_bottom] + coords + [coords[-2], y_bottom]
        self.create_polygon(poly_coords, fill=self.fill_color, outline="")

        # 4. Vẽ đường xu hướng làm mịn (Vector Trend Curve)
        self.create_line(coords, fill=self.line_color, width=2, smooth=True)

        # 5. Điểm LED phát quang tại mẫu dữ liệu mới nhất (Pulsing Marker)
        last_x, last_y = coords[-2], coords[-1]
        self.create_oval(last_x - 4.5, last_y - 4.5, last_x + 4.5, last_y + 4.5,
                         fill="", outline=self.line_color, width=1)
        self.create_oval(last_x - 2.5, last_y - 2.5, last_x + 2.5, last_y + 2.5,
                         fill="#FFFFFF", outline=self.line_color, width=1)


class ThreatGaugeCanvas(tk.Canvas):
    """
    Thước đo chỉ số nguy cơ / dị thường AI đa phân đoạn (10-Segment LED Threat Bar).
    Chuyển màu mượt mà từ Xanh (An toàn) -> Vàng (Cảnh báo) -> Đỏ (Nguy hiểm).
    """

    def __init__(self, parent, width=230, height=18, bg_color="#0D1322", **kwargs):
        super().__init__(parent, width=width, height=height, bg=bg_color,
                         highlightthickness=0, bd=0, **kwargs)
        self.w = width
        self.h = height
        self.set_score(0.05)

    def set_score(self, score: float):
        self.delete("all")
        score = max(0.0, min(1.0, float(score)))
        num_segments = 10
        gap = 3
        pad_x = 4
        pad_y = 3
        seg_w = (self.w - 2 * pad_x - (num_segments - 1) * gap) / num_segments
        seg_h = self.h - 2 * pad_y

        active_count = int(round(score * num_segments))
        if score > 0.02 and active_count == 0:
            active_count = 1

        for i in range(num_segments):
            x0 = pad_x + i * (seg_w + gap)
            x1 = x0 + seg_w
            y0 = pad_y
            y1 = y0 + seg_h

            if i < 4:
                glow_color = "#10B981"  # Xanh an toàn
                dim_color = "#064E3B"
            elif i < 7:
                glow_color = "#F59E0B"  # Vàng cảnh giác
                dim_color = "#78350F"
            else:
                glow_color = "#EF4444"  # Đỏ nguy cấp
                dim_color = "#7F1D1D"

            fill_c = glow_color if (i < active_count) else dim_color
            border_c = glow_color if (i < active_count) else "#1E293B"

            self.create_rectangle(x0, y0, x1, y1, fill=fill_c, outline=border_c, width=1)


class GatewayGUI:
    """
    Giao diện Desktop GUI Giám sát & Điều khiển Phòng sạch Bio-Chamber (SCADA Dark Theme).
    Thiết kế theo chuẩn SCADA Mission Control tông màu tối Obsidian / Slate:
      - Header điều hành hệ thống: Đồng hồ thời gian thực, Link Badges, Heartbeat.
      - 4 Thẻ vi khí hậu buồng nuôi cấy tích hợp Sparklines đa mẫu động.
      - Trạm suy luận Edge/Fog AI & Hồi quy ngoại suy nhiệt độ 30 phút.
      - Bảng công tắc chấp hành cảm ứng lực kèm đèn báo trạng thái LED.
      - Terminal dòng lệnh chuyên dụng kiểm tra gói tin Telemetry / MQTT.
    """

    def __init__(self, root, gateway_core=None):
        self.root = root
        self.start_time = time.time()
        self.rx_count = 0
        self.tx_count = 0

        # Lịch sử đo lường cho thống kê Min/Max/Avg
        self.stats = {
            "temp": {"min": 999.0, "max": -999.0, "sum": 0.0, "count": 0},
            "hum": {"min": 999.0, "max": -999.0, "sum": 0.0, "count": 0},
            "soil": {"min": 999.0, "max": -999.0, "sum": 0.0, "count": 0},
            "light": {"min": 99999.0, "max": -99999.0, "sum": 0.0, "count": 0},
        }

        # Bộ đệm nhật ký có lọc
        self.all_logs = []
        self.current_filter = "ALL"
        self.auto_scroll = True

        # Kết nối Fog Gateway Core
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
        self.start_clock_timer()

        # Đóng ứng dụng an toàn khi tắt cửa sổ
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def on_close(self):
        try:
            self.gateway.running = False
        except Exception:
            pass
        self.root.destroy()

    def setup_ui(self):
        self.root.title("BIO-SCADA Cleanroom Bio-Chamber Edge Gateway")
        self.root.geometry("1060x780")
        self.root.minsize(1000, 700)
        self.root.configure(bg="#0A0E17")

        # Cấu hình thanh cuộn và style ttk
        style = ttk.Style()
        style.theme_use("clam")
        style.configure(".", background="#0A0E17", foreground="#F8FAFC")
        style.configure("TFrame", background="#0A0E17")
        style.configure("Vertical.TScrollbar", background="#1E293B", troughcolor="#0A0E17",
                        bordercolor="#0A0E17", arrowcolor="#94A3B8")

        # =====================================================================
        # 1. TOP MISSION CONTROL HEADER
        # =====================================================================
        top_bar = tk.Frame(self.root, bg="#0D1322", height=58, bd=0, relief=tk.FLAT)
        top_bar.pack(fill=tk.X, side=tk.TOP)

        accent_line = tk.Frame(self.root, bg="#1E293B", height=1)
        accent_line.pack(fill=tk.X, side=tk.TOP)

        # Khối thương hiệu bên trái (đã tinh giản tiêu đề chuẩn SCADA công nghiệp)
        brand_box = tk.Frame(top_bar, bg="#0D1322")
        brand_box.pack(side=tk.LEFT, padx=16, pady=10)

        logo_row = tk.Frame(brand_box, bg="#0D1322")
        logo_row.pack(anchor=tk.W)

        scada_badge = tk.Label(logo_row, text="● SCADA-EDGE", font=("Consolas", 8, "bold"),
                               bg="#064E3B", fg="#34D399", padx=6, pady=2, bd=0)
        scada_badge.pack(side=tk.LEFT, padx=(0, 8))

        title_lbl = tk.Label(logo_row, text="BIO-SCADA", font=("Segoe UI", 13, "bold"),
                             bg="#0D1322", fg="#FFFFFF")
        title_lbl.pack(side=tk.LEFT)

        sub_title_lbl = tk.Label(logo_row, text=" | CLEANROOM BIO-CHAMBER GATEWAY",
                                 font=("Segoe UI", 11, "bold"), bg="#0D1322", fg="#38BDF8")
        sub_title_lbl.pack(side=tk.LEFT)

        # Khối trạng thái & đồng hồ bên phải
        right_status_box = tk.Frame(top_bar, bg="#0D1322")
        right_status_box.pack(side=tk.RIGHT, padx=16, pady=7)

        self.lbl_clock = tk.Label(right_status_box, text="--:--:-- UTC+7",
                                  font=("Consolas", 10, "bold"), bg="#111827", fg="#38BDF8",
                                  padx=10, pady=4, relief=tk.FLAT, bd=0)
        self.lbl_clock.pack(side=tk.RIGHT, padx=(8, 0))

        self.lbl_badge_mqtt = tk.Label(right_status_box, text="☁ COREIOT: MQTT ONLINE",
                                       font=("Segoe UI", 8, "bold"), bg="#064E3B", fg="#34D399",
                                       padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_mqtt.pack(side=tk.RIGHT, padx=4)

        self.lbl_badge_esp = tk.Label(right_status_box, text="⚡ TELEMETRY: SIMULATED",
                                      font=("Segoe UI", 8, "bold"), bg="#78350F", fg="#FDE68A",
                                      padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_esp.pack(side=tk.RIGHT, padx=4)

        # =====================================================================
        # 2. BỐ CỤC CHÍNH (MAIN BODY)
        # =====================================================================
        main_container = tk.Frame(self.root, bg="#0A0E17")
        main_container.pack(fill=tk.BOTH, expand=True, padx=12, pady=8)

        # =====================================================================
        # 2.1 ROW 1: 4 THẺ VI KHÍ HẬU KỸ THUẬT SỐ KÈM SPARKLINE VECTOR
        # =====================================================================
        cards_row = tk.Frame(main_container, bg="#0A0E17")
        cards_row.pack(fill=tk.X, pady=(0, 8))
        for col in range(4):
            cards_row.columnconfigure(col, weight=1)

        self.card_temp = self.create_advanced_metric_card(
            cards_row, col=0,
            title="AIR TEMPERATURE", tag="DHT20 / RS485",
            init_val="27.5", unit="°C",
            accent_color="#F43F5E", fill_color="#28111A", normal_range="20.0 - 32.0 °C"
        )

        self.card_hum = self.create_advanced_metric_card(
            cards_row, col=1,
            title="RELATIVE HUMIDITY", tag="CAPACITIVE",
            init_val="65.0", unit="%",
            accent_color="#06B6D4", fill_color="#0C2434", normal_range="50.0 - 75.0 %"
        )

        self.card_soil = self.create_advanced_metric_card(
            cards_row, col=2,
            title="SUBSTRATE MOISTURE", tag="ANALOG ADC1",
            init_val="50.0", unit="%",
            accent_color="#10B981", fill_color="#0A251E", normal_range="40.0 - 70.0 %"
        )

        self.card_light = self.create_advanced_metric_card(
            cards_row, col=3,
            title="PHOTOPERIOD LIGHT", tag="LUX / LDR",
            init_val="720", unit="Lux",
            accent_color="#F59E0B", fill_color="#261C0E", normal_range="300 - 900 Lux"
        )

        # =====================================================================
        # 2.2 ROW 2: KHU VỰC KÉP (AI TELEMETRY CONSOLE + ACTUATOR SWITCHBOARD)
        # =====================================================================
        mid_row = tk.Frame(main_container, bg="#0A0E17")
        mid_row.pack(fill=tk.X, pady=(0, 8))
        mid_row.columnconfigure(0, weight=6, uniform="mid_cols")
        mid_row.columnconfigure(1, weight=4, uniform="mid_cols")

        # ---------------------------------------------------------------------
        # CỘT TRÁI: EMBEDDED EDGE AI & PREDICTIVE TELEMETRY ENGINE
        # ---------------------------------------------------------------------
        ai_panel = self.create_section_panel(mid_row, "🧠 EMBEDDED EDGE AI & PREDICTIVE TELEMETRY ENGINE",
                                             row=0, col=0, sticky="nsew", padx=(0, 5))

        ai_grid = tk.Frame(ai_panel, bg="#111827")
        ai_grid.pack(fill=tk.BOTH, expand=True, padx=10, pady=8)
        ai_grid.columnconfigure(0, weight=1)
        ai_grid.columnconfigure(1, weight=1)

        # Ô 1: THREAT / ANOMALY DETECTOR
        cell1 = tk.Frame(ai_grid, bg="#0D1322", padx=10, pady=8, relief=tk.FLAT)
        cell1.grid(row=0, column=0, sticky="nsew", padx=3, pady=3)

        tk.Label(cell1, text="ANOMALY THREAT LEVEL", font=("Segoe UI", 8, "bold"),
                 bg="#0D1322", fg="#94A3B8").pack(anchor=tk.W)

        score_row = tk.Frame(cell1, bg="#0D1322")
        score_row.pack(anchor=tk.W, pady=(2, 2))
        self.lbl_ai_score = tk.Label(score_row, text="0.05", font=("Segoe UI", 17, "bold"),
                                     bg="#0D1322", fg="#10B981")
        self.lbl_ai_score.pack(side=tk.LEFT)
        tk.Label(score_row, text=" / 1.00 THREAT INDEX", font=("Segoe UI", 8),
                 bg="#0D1322", fg="#64748B").pack(side=tk.LEFT, pady=(4, 0), padx=(4, 0))

        # Thước đo LED Threat Gauge
        self.threat_gauge = ThreatGaugeCanvas(cell1, width=205, height=14, bg_color="#0D1322")
        self.threat_gauge.pack(anchor=tk.W, pady=(2, 4))

        self.lbl_ai_status_badge = tk.Label(cell1, text="● NOMINAL - ZERO DRIFT",
                                            font=("Consolas", 8, "bold"), bg="#064E3B", fg="#34D399",
                                            padx=6, pady=2)
        self.lbl_ai_status_badge.pack(anchor=tk.W)

        # Ô 2: 30-MINUTE DYNAMIC FORECAST
        cell2 = tk.Frame(ai_grid, bg="#0D1322", padx=10, pady=8, relief=tk.FLAT)
        cell2.grid(row=0, column=1, sticky="nsew", padx=3, pady=3)

        tk.Label(cell2, text="30-MIN CLIMATE FORECAST", font=("Segoe UI", 8, "bold"),
                 bg="#0D1322", fg="#94A3B8").pack(anchor=tk.W)

        fc_row = tk.Frame(cell2, bg="#0D1322")
        fc_row.pack(anchor=tk.W, pady=(2, 2))
        self.lbl_forecast_val = tk.Label(fc_row, text="28.0", font=("Segoe UI", 17, "bold"),
                                         bg="#0D1322", fg="#38BDF8")
        self.lbl_forecast_val.pack(side=tk.LEFT)
        tk.Label(fc_row, text=" °C", font=("Segoe UI", 11, "bold"), bg="#0D1322", fg="#38BDF8").pack(side=tk.LEFT, pady=(3, 0))

        self.lbl_forecast_trend = tk.Label(cell2, text="↗ STEADY TREND (+0.02 °C/min)",
                                           font=("Segoe UI", 8), bg="#0D1322", fg="#94A3B8")
        self.lbl_forecast_trend.pack(anchor=tk.W, pady=(1, 1))

        self.lbl_forecast_confidence = tk.Label(cell2, text="Model Confidence: 98.2% (OLS Linear Fit)",
                                                font=("Segoe UI", 7, "italic"), bg="#0D1322", fg="#64748B")
        self.lbl_forecast_confidence.pack(anchor=tk.W)

        # Ô 3: BIO-CHAMBER ADVISOR & MIST HUMIDIFICATION AI
        cell3 = tk.Frame(ai_grid, bg="#0D1322", padx=10, pady=6, relief=tk.FLAT)
        cell3.grid(row=1, column=0, columnspan=2, sticky="nsew", padx=3, pady=(4, 0))

        tk.Label(cell3, text="BIO-CHAMBER ADVISOR & MIST HUMIDIFICATION AI",
                 font=("Segoe UI", 8, "bold"), bg="#0D1322", fg="#94A3B8").pack(anchor=tk.W)

        self.lbl_irrigation_advice = tk.Label(
            cell3, text="✓ ĐỘ ẨM GIÁ THỂ TỐI ƯU (50%): Điều kiện nuôi cấy ổn định, không cần phun sương.",
            font=("Segoe UI", 9, "bold"), bg="#0D1322", fg="#F8FAFC", anchor=tk.W, justify=tk.LEFT
        )
        self.lbl_irrigation_advice.pack(anchor=tk.W, pady=(2, 2))

        # ---------------------------------------------------------------------
        # CỘT PHẢI: HARDWARE ACTUATOR CONTROL MATRIX (LOCKED GEOMETRY)
        # ---------------------------------------------------------------------
        ctrl_panel = self.create_section_panel(mid_row, "⚡ HARDWARE ACTUATOR MATRIX",
                                               row=0, col=1, sticky="nsew", padx=(5, 0))

        ctrl_inner = tk.Frame(ctrl_panel, bg="#111827", padx=8, pady=8)
        ctrl_inner.pack(fill=tk.BOTH, expand=True)

        tiles_frame = tk.Frame(ctrl_inner, bg="#111827")
        tiles_frame.pack(fill=tk.X)
        for i in range(3):
            tiles_frame.columnconfigure(i, weight=1, uniform="act_tile")

        # Tile 1: Mist Humidifier Pump
        self.btn_pump_tile, self.lbl_pump_led = self.create_tactile_actuator_tile(
            tiles_frame, col=0, icon="💧", name="MIST PUMP",
            state_text="STANDBY", active_color="#10B981", cmd_action=self.toggle_pump
        )

        # Tile 2: Cleanroom Airflow Fan
        self.btn_fan_tile, self.lbl_fan_led = self.create_tactile_actuator_tile(
            tiles_frame, col=1, icon="🌀", name="AIRFLOW FAN",
            state_text="STANDBY", active_color="#0EA5E9", cmd_action=self.toggle_fan
        )

        # Tile 3: Control Mode
        self.btn_mode_tile, self.lbl_mode_led = self.create_tactile_actuator_tile(
            tiles_frame, col=2, icon="⚙️", name="CTRL MODE",
            state_text="AUTO", active_color="#0284C7", cmd_action=self.toggle_mode
        )

        # Hàng công cụ khẩn cấp & kiểm thử (cố định tỷ lệ khung viền chống nhíp)
        action_row = tk.Frame(ctrl_inner, bg="#111827")
        action_row.pack(fill=tk.X, pady=(8, 0))
        action_row.columnconfigure(0, weight=7, uniform="act_row")
        action_row.columnconfigure(1, weight=3, uniform="act_row")

        self.btn_flame_test = tk.Button(
            action_row, text="🔥 TEST ĐỘT BIẾN NHIỆT AI (FLAME / HEAT TEST)", font=("Segoe UI", 8, "bold"),
            bg="#991B1B", fg="#FFFFFF", activebackground="#DC2626", activeforeground="#FFFFFF",
            relief=tk.FLAT, bd=0, padx=6, pady=6, cursor="hand2", command=self.trigger_flame_test
        )
        self.btn_flame_test.grid(row=0, column=0, sticky="nsew", padx=(0, 2))
        self.add_button_hover(self.btn_flame_test, "#991B1B", "#DC2626")

        self.btn_sync_now = tk.Button(
            action_row, text="🔄 FORCE SYNC", font=("Segoe UI", 8, "bold"),
            bg="#1E293B", fg="#94A3B8", activebackground="#334155", activeforeground="#FFFFFF",
            relief=tk.FLAT, bd=0, padx=6, pady=6, cursor="hand2", command=self.trigger_sync_now
        )
        self.btn_sync_now.grid(row=0, column=1, sticky="nsew", padx=(2, 0))
        self.add_button_hover(self.btn_sync_now, "#1E293B", "#334155")

        # =====================================================================
        # 2.3 ROW 3: CONSOLE NHẬT KÝ SỰ KIỆN ĐỒNG BỘ TELEMETRY STREAM
        # =====================================================================
        term_panel = self.create_section_panel(main_container,
                                              "📋 REAL-TIME TELEMETRY STREAM & MQTT PACKET INSPECTOR",
                                              row=None, col=None)

        term_head = tk.Frame(term_panel, bg="#111827", padx=8, pady=3)
        term_head.pack(fill=tk.X)

        filter_box = tk.Frame(term_head, bg="#111827")
        filter_box.pack(side=tk.LEFT)
        tk.Label(filter_box, text="FILTER:", font=("Consolas", 8, "bold"), bg="#111827", fg="#64748B").pack(side=tk.LEFT, padx=(0, 5))

        self.filter_buttons = {}
        filters = ["ALL", "MQTT", "AI", "HARDWARE", "WARNINGS"]
        for f in filters:
            btn = tk.Button(filter_box, text=f, font=("Consolas", 8),
                            bg="#1E293B" if f == "ALL" else "#0D1322",
                            fg="#38BDF8" if f == "ALL" else "#64748B",
                            activebackground="#334155", activeforeground="#FFFFFF",
                            relief=tk.FLAT, bd=0, padx=7, pady=2, cursor="hand2",
                            command=lambda flt=f: self.set_log_filter(flt))
            btn.pack(side=tk.LEFT, padx=2)
            self.filter_buttons[f] = btn

        term_ctrls = tk.Frame(term_head, bg="#111827")
        term_ctrls.pack(side=tk.RIGHT)

        self.btn_scroll = tk.Button(term_ctrls, text="AUTO-SCROLL: ON", font=("Consolas", 8, "bold"),
                                    bg="#064E3B", fg="#34D399", relief=tk.FLAT, bd=0, padx=6, pady=2,
                                    cursor="hand2", command=self.toggle_autoscroll)
        self.btn_scroll.pack(side=tk.LEFT, padx=3)

        btn_clear = tk.Button(term_ctrls, text="CLEAR", font=("Consolas", 8),
                              bg="#1E293B", fg="#94A3B8", relief=tk.FLAT, bd=0, padx=6, pady=2,
                              cursor="hand2", command=self.clear_logs)
        btn_clear.pack(side=tk.LEFT, padx=3)

        term_body = tk.Frame(term_panel, bg="#070B14")
        term_body.pack(fill=tk.BOTH, expand=True, padx=6, pady=(0, 6))

        self.txt_term = tk.Text(term_body, bg="#070B14", fg="#E2E8F0", insertbackground="#38BDF8",
                                font=("Consolas", 9), height=6, wrap=tk.NONE, bd=0, padx=8, pady=5)
        scrollbar_y = ttk.Scrollbar(term_body, orient="vertical", command=self.txt_term.yview)
        self.txt_term.configure(yscrollcommand=scrollbar_y.set)

        scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)
        self.txt_term.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Cú pháp màu (Syntax Highlighting Tags)
        self.txt_term.tag_configure("time", foreground="#475569")
        self.txt_term.tag_configure("mqtt", foreground="#38BDF8", font=("Consolas", 9, "bold"))
        self.txt_term.tag_configure("ai", foreground="#C084FC", font=("Consolas", 9, "bold"))
        self.txt_term.tag_configure("act", foreground="#34D399", font=("Consolas", 9, "bold"))
        self.txt_term.tag_configure("warn", foreground="#FBBF24", font=("Consolas", 9, "bold"))
        self.txt_term.tag_configure("err", foreground="#F87171", font=("Consolas", 9, "bold"))
        self.txt_term.tag_configure("normal", foreground="#CBD5E1")

        # =====================================================================
        # 3. BOTTOM INDUSTRIAL STATUS FOOTER BAR
        # =====================================================================
        footer_bar = tk.Frame(self.root, bg="#0D1322", height=24)
        footer_bar.pack(fill=tk.X, side=tk.BOTTOM)

        self.lbl_footer_stats = tk.Label(
            footer_bar,
            text="LINK: ACTIVE | RATE: 0.5 Hz (1 pkt/2s) | PROTOCOL: MQTT v3.1.1 QoS 1 | BROKER: app.coreiot.io | UPTIME: 00:00:00",
            font=("Consolas", 8), bg="#0D1322", fg="#64748B", padx=12, pady=3
        )
        self.lbl_footer_stats.pack(side=tk.LEFT)

        self.lbl_footer_packets = tk.Label(
            footer_bar, text="RX: 0 pkts | TX: 0 pkts | DROP: 0.0%",
            font=("Consolas", 8, "bold"), bg="#0D1322", fg="#38BDF8", padx=12, pady=3
        )
        self.lbl_footer_packets.pack(side=tk.RIGHT)

    # =========================================================================
    # CÁC HÀM XÂY DỰNG GIAO DIỆN (UI BUILDERS)
    # =========================================================================
    def create_section_panel(self, parent, title, row=None, col=None, sticky=None, padx=0, pady=0):
        panel = tk.Frame(parent, bg="#111827", bd=1, relief=tk.SOLID)
        panel.configure(highlightbackground="#1E293B", highlightcolor="#1E293B", highlightthickness=1)

        if row is not None and col is not None:
            panel.grid(row=row, column=col, sticky=sticky, padx=padx, pady=pady)
        else:
            panel.pack(fill=tk.BOTH, expand=True, pady=(0, 6))

        phead = tk.Frame(panel, bg="#0D1322", height=26)
        phead.pack(fill=tk.X, side=tk.TOP)
        tk.Label(phead, text=title, font=("Segoe UI", 8, "bold"),
                 bg="#0D1322", fg="#94A3B8", padx=10, pady=4).pack(anchor=tk.W)

        return panel

    def create_advanced_metric_card(self, parent, col, title, tag, init_val, unit, accent_color, fill_color, normal_range):
        card = tk.Frame(parent, bg="#111827", bd=1, relief=tk.SOLID)
        card.configure(highlightbackground="#1E293B", highlightcolor="#1E293B", highlightthickness=1)
        card.grid(row=0, column=col, sticky="nsew", padx=3)

        # Header card
        head_box = tk.Frame(card, bg="#111827", padx=8, pady=5)
        head_box.pack(fill=tk.X)

        lbl_title = tk.Label(head_box, text=title, font=("Segoe UI", 8, "bold"), bg="#111827", fg="#94A3B8")
        lbl_title.pack(side=tk.LEFT)

        tag_chip = tk.Label(head_box, text=tag, font=("Consolas", 7), bg="#1E293B", fg="#64748B", padx=4, pady=1)
        tag_chip.pack(side=tk.RIGHT)

        # Value display
        val_box = tk.Frame(card, bg="#111827", padx=8)
        val_box.pack(anchor=tk.W)

        val_lbl = tk.Label(val_box, text=init_val, font=("Segoe UI", 20, "bold"), bg="#111827", fg=accent_color)
        val_lbl.pack(side=tk.LEFT)

        unit_lbl = tk.Label(val_box, text=f" {unit}", font=("Segoe UI", 10, "bold"), bg="#111827", fg="#94A3B8")
        unit_lbl.pack(side=tk.LEFT, pady=(6, 0))

        # Status & Range
        status_lbl = tk.Label(card, text=f"● NOMINAL ({normal_range})",
                              font=("Segoe UI", 7), bg="#111827", fg="#10B981", padx=8)
        status_lbl.pack(anchor=tk.W)

        # Vector Sparkline Canvas
        sparkline = SparklineCanvas(card, width=220, height=44, line_color=accent_color,
                                    fill_color=fill_color, bg_color="#111827", initial_val=float(init_val))
        sparkline.pack(fill=tk.X, padx=6, pady=(3, 2))

        # Footer statistics
        footer_box = tk.Frame(card, bg="#0D1322", padx=6, pady=3)
        footer_box.pack(fill=tk.X, side=tk.BOTTOM)

        stat_lbl = tk.Label(footer_box, text="MIN -- | MAX -- | AVG --",
                            font=("Consolas", 7), bg="#0D1322", fg="#64748B")
        stat_lbl.pack(anchor=tk.W)

        return {
            "val_lbl": val_lbl,
            "unit": unit,
            "status_lbl": status_lbl,
            "sparkline": sparkline,
            "stat_lbl": stat_lbl,
            "normal_range": normal_range
        }

    def create_tactile_actuator_tile(self, parent, col, icon, name, state_text, active_color, cmd_action):
        tile = tk.Frame(parent, bg="#0D1322", padx=6, pady=8, bd=1, relief=tk.SOLID)
        tile.configure(highlightbackground="#1E293B", highlightcolor="#1E293B", highlightthickness=1)
        tile.grid(row=0, column=col, sticky="nsew", padx=2)

        hdr = tk.Frame(tile, bg="#0D1322")
        hdr.pack(fill=tk.X)
        tk.Label(hdr, text=f"{icon} {name}", font=("Segoe UI", 8, "bold"), bg="#0D1322", fg="#94A3B8").pack(side=tk.LEFT)

        # Cố định width=9 và căn giữa để nhãn LED không bao giờ co giãn/nhíp khung khi đổi trạng thái
        lbl_led = tk.Label(hdr, text=f"● {state_text}", font=("Consolas", 7, "bold"),
                           bg="#1E293B", fg="#64748B", width=9, anchor=tk.CENTER, padx=2, pady=1)
        lbl_led.pack(side=tk.RIGHT)

        btn_action = tk.Button(
            tile, text="TOGGLE POWER", font=("Segoe UI", 8, "bold"),
            bg="#1E293B", fg="#F8FAFC", activebackground="#334155", activeforeground="#FFFFFF",
            relief=tk.FLAT, bd=0, pady=5, cursor="hand2", command=cmd_action
        )
        btn_action.pack(fill=tk.X, pady=(6, 0))
        self.add_button_hover(btn_action, "#1E293B", "#334155")

        return btn_action, lbl_led

    def add_button_hover(self, btn, default_bg, hover_bg):
        btn.custom_bg = default_bg
        btn.hover_bg = hover_bg

        def on_enter(e):
            if btn["state"] != tk.DISABLED:
                btn.config(bg=getattr(btn, "hover_bg", hover_bg))

        def on_leave(e):
            if btn["state"] != tk.DISABLED:
                btn.config(bg=getattr(btn, "custom_bg", default_bg))

        btn.bind("<Enter>", on_enter)
        btn.bind("<Leave>", on_leave)

    # =========================================================================
    # VÒNG LẶP ĐỒNG BỘ THỜI GIAN & GIAO DIỆN
    # =========================================================================
    def start_clock_timer(self):
        self.update_clock()

    def update_clock(self):
        now_str = time.strftime("%H:%M:%S") + " UTC+7"
        self.lbl_clock.config(text=f"🕒 {now_str}")

        elapsed = int(time.time() - self.start_time)
        hrs = elapsed // 3600
        mins = (elapsed % 3600) // 60
        secs = elapsed % 60
        uptime_str = f"{hrs:02d}:{mins:02d}:{secs:02d}"

        self.lbl_footer_stats.config(
            text=f"LINK: ACTIVE | RATE: 0.5 Hz | PROTOCOL: MQTT v3.1.1 (QoS 1) | BROKER: app.coreiot.io | UPTIME: {uptime_str}"
        )

        self.root.after(1000, self.update_clock)

    def trigger_gui_update(self):
        try:
            self.root.after(0, self.update_display)
        except Exception:
            pass

    def update_display(self):
        t = self.gateway.telemetry
        self.rx_count += 1
        self.tx_count += 1

        # Cập nhật số liệu & Sparkline
        temp = t.get("temperature", 27.5)
        hum = t.get("humidity", 65.0)
        soil = t.get("soilMoisture", 50.0)
        light = t.get("lightLux", 700.0)

        self.card_temp["val_lbl"].config(text=f"{temp:.1f}")
        self.card_temp["sparkline"].add_point(temp)
        self.update_stats("temp", temp, self.card_temp["stat_lbl"], "°C")

        self.card_hum["val_lbl"].config(text=f"{hum:.1f}")
        self.card_hum["sparkline"].add_point(hum)
        self.update_stats("hum", hum, self.card_hum["stat_lbl"], "%")

        self.card_soil["val_lbl"].config(text=f"{soil:.0f}")
        self.card_soil["sparkline"].add_point(soil)
        self.update_stats("soil", soil, self.card_soil["stat_lbl"], "%")

        self.card_light["val_lbl"].config(text=f"{light:.0f}")
        self.card_light["sparkline"].add_point(light)
        self.update_stats("light", light, self.card_light["stat_lbl"], "Lux")

        # Cập nhật trạng thái vi khí hậu
        if temp > 35.0:
            self.card_temp["status_lbl"].config(text="▲ WARNING: HIGH TEMP HAZARD", fg="#EF4444")
        elif temp < 18.0:
            self.card_temp["status_lbl"].config(text="▼ WARNING: LOW CHILL TEMP", fg="#38BDF8")
        else:
            self.card_temp["status_lbl"].config(text=f"● NOMINAL ({self.card_temp['normal_range']})", fg="#10B981")

        if soil < 30.0:
            self.card_soil["status_lbl"].config(text="▼ ALERT: CRITICAL DRY SUBSTRATE", fg="#EF4444")
        else:
            self.card_soil["status_lbl"].config(text=f"● OPTIMAL MOISTURE ({self.card_soil['normal_range']})", fg="#10B981")

        # ---------------------------------------------------------------------
        # 2. CẬP NHẬT KẾT QUẢ AI & HỒI QUY
        # ---------------------------------------------------------------------
        ai = self.gateway.ai_results
        score = ai.get("anomaly_score", 0.05)
        self.lbl_ai_score.config(text=f"{score:.2f}")
        self.threat_gauge.set_score(score)

        if score >= 0.60:
            self.lbl_ai_score.config(fg="#EF4444")
            self.lbl_ai_status_badge.config(
                text=f"▲ CRITICAL: {ai.get('anomaly_type', 'HAZARD')}",
                bg="#7F1D1D", fg="#FCA5A5"
            )
        elif score >= 0.35:
            self.lbl_ai_score.config(fg="#F59E0B")
            self.lbl_ai_status_badge.config(
                text=f"● ELEVATED: {ai.get('anomaly_type', 'CAUTION')}",
                bg="#78350F", fg="#FDE68A"
            )
        else:
            self.lbl_ai_score.config(fg="#10B981")
            self.lbl_ai_status_badge.config(
                text="● NOMINAL - ZERO DRIFT",
                bg="#064E3B", fg="#34D399"
            )

        proj_temp = ai.get("projected_temp_30m", temp)
        self.lbl_forecast_val.config(text=f"{proj_temp:.1f}")

        delta_t = proj_temp - temp
        if delta_t > 0.3:
            trend_str = f"↗ WARMING TREND (+{delta_t:.1f} °C)"
            trend_fg = "#F59E0B"
        elif delta_t < -0.3:
            trend_str = f"↘ COOLING TREND ({delta_t:.1f} °C)"
            trend_fg = "#06B6D4"
        else:
            trend_str = f"→ STEADY THERMAL INERTIA (±0.2 °C)"
            trend_fg = "#94A3B8"
        self.lbl_forecast_trend.config(text=trend_str, fg=trend_fg)

        conf = ai.get("confidence", 98.0)
        self.lbl_forecast_confidence.config(text=f"Model Confidence: {conf:.1f}% (OLS Linear Fit)")

        # Khuyến nghị phun sương buồng nuôi cấy
        advice_txt = ai.get("irrigation_recommendation", "Đang cập nhật chỉ số...")
        if "CẦN PHUN SƯƠNG" in advice_txt or "CẢNH BÁO" in advice_txt:
            self.lbl_irrigation_advice.config(text=f"⚠️ {advice_txt}", fg="#FDE68A")
        else:
            self.lbl_irrigation_advice.config(text=f"✓ {advice_txt}", fg="#34D399")

        # ---------------------------------------------------------------------
        # 3. CẬP NHẬT CƠ CẤU CHẤP HÀNH
        # ---------------------------------------------------------------------
        p_on = t.get("pumpState", False)
        self.lbl_pump_led.config(
            text="● ACTIVE" if p_on else "○ STANDBY",
            bg="#064E3B" if p_on else "#1E293B",
            fg="#34D399" if p_on else "#64748B"
        )
        p_bg = "#059669" if p_on else "#1E293B"
        p_hover = "#10B981" if p_on else "#334155"
        self.btn_pump_tile.custom_bg = p_bg
        self.btn_pump_tile.hover_bg = p_hover
        self.btn_pump_tile.config(
            text="TẮT PHUN SƯƠNG" if p_on else "BẬT PHUN SƯƠNG",
            bg=p_bg
        )

        f_on = t.get("fanState", False)
        self.lbl_fan_led.config(
            text="● ACTIVE" if f_on else "○ STANDBY",
            bg="#0C4A6E" if f_on else "#1E293B",
            fg="#38BDF8" if f_on else "#64748B"
        )
        f_bg = "#0284C7" if f_on else "#1E293B"
        f_hover = "#38BDF8" if f_on else "#334155"
        self.btn_fan_tile.custom_bg = f_bg
        self.btn_fan_tile.hover_bg = f_hover
        self.btn_fan_tile.config(
            text="TẮT QUẠT" if f_on else "BẬT QUẠT",
            bg=f_bg
        )

        is_auto = t.get("autoMode", True)
        self.lbl_mode_led.config(
            text="● AUTO" if is_auto else "● MANUAL",
            bg="#0C4A6E" if is_auto else "#78350F",
            fg="#38BDF8" if is_auto else "#FDE68A"
        )
        m_bg = "#0369A1" if is_auto else "#D97706"
        m_hover = "#0284C7" if is_auto else "#F59E0B"
        self.btn_mode_tile.custom_bg = m_bg
        self.btn_mode_tile.hover_bg = m_hover
        self.btn_mode_tile.config(
            text="CHUYỂN THỦ CÔNG" if is_auto else "CHUYỂN AUTO",
            bg=m_bg
        )

        # ---------------------------------------------------------------------
        # 4. CẬP NHẬT BADGES & THỐNG KÊ GÓI TIN
        # ---------------------------------------------------------------------
        if self.gateway.esp32_connected:
            self.lbl_badge_esp.config(text="● ESP32-S3: USB-CDC 115200", bg="#064E3B", fg="#34D399")
        else:
            self.lbl_badge_esp.config(text="⚡ TELEMETRY: SIMULATED", bg="#78350F", fg="#FDE68A")

        if self.gateway.coreiot_connected:
            self.lbl_badge_mqtt.config(text="☁ COREIOT: MQTT ONLINE", bg="#064E3B", fg="#34D399")
        else:
            self.lbl_badge_mqtt.config(text="☁ COREIOT: DISCONNECTED", bg="#7F1D1D", fg="#FCA5A5")

        self.lbl_footer_packets.config(
            text=f"RX: {self.rx_count:,} pkts | TX: {self.tx_count:,} pkts | DROP: 0.0%"
        )

    def update_stats(self, key: str, val: float, lbl: tk.Label, unit: str):
        st = self.stats[key]
        st["min"] = min(st["min"], val)
        st["max"] = max(st["max"], val)
        st["sum"] += val
        st["count"] += 1
        avg = st["sum"] / st["count"]
        lbl.config(text=f"MIN {st['min']:.1f} | MAX {st['max']:.1f} | AVG {avg:.1f} {unit}")

    # =========================================================================
    # HỆ THỐNG TERMINAL NHẬT KÝ & BỘ LỌC CÚ PHÁP
    # =========================================================================
    def append_log(self, text: str):
        try:
            self.root.after(0, self._process_log_entry, text)
        except Exception:
            pass

    def _process_log_entry(self, text: str):
        self.all_logs.append(text)
        if len(self.all_logs) > 500:
            self.all_logs.pop(0)

        if self.should_display_log(text, self.current_filter):
            self.insert_styled_log(text)

    def should_display_log(self, text: str, flt: str) -> bool:
        if flt == "ALL":
            return True
        if flt == "MQTT" and ("COREIOT" in text or "MQTT" in text):
            return True
        if flt == "AI" and ("AI" in text or "ANOMALY" in text or "Score" in text):
            return True
        if flt == "HARDWARE" and ("ACTUATOR" in text or "CONTROL" in text or "Bơm" in text or "Quạt" in text or "Mode" in text or "SERIAL" in text):
            return True
        if flt == "WARNINGS" and ("ERROR" in text or "CẢNH BÁO" in text or "LỖI" in text or "HAZARD" in text or "FLAME" in text):
            return True
        return False

    def insert_styled_log(self, text: str):
        if not hasattr(self, "txt_term") or not self.txt_term:
            return

        timestamp = ""
        body = text
        if text.startswith("[") and "]" in text[:12]:
            idx = text.find("]")
            timestamp = text[:idx + 1] + " "
            body = text[idx + 1:].strip()

        tag = "normal"
        if "MQTT" in text or "COREIOT" in text:
            tag = "mqtt"
        elif "AI" in text or "ANOMALY" in text or "Score" in text:
            tag = "ai"
        elif "Bơm" in text or "Quạt" in text or "Mode" in text or "LOCAL GUI" in text:
            tag = "act"
        elif "ERROR" in text or "✗" in text or "FLAME" in text or "TEST" in text:
            tag = "err"
        elif "Warning" in text or "Cảnh báo" in text:
            tag = "warn"

        self.txt_term.insert(tk.END, timestamp, "time")
        self.txt_term.insert(tk.END, body + "\n", tag)

        if self.auto_scroll:
            self.txt_term.see(tk.END)

    def set_log_filter(self, flt: str):
        self.current_filter = flt
        for name, btn in self.filter_buttons.items():
            if name == flt:
                btn.config(bg="#1E293B", fg="#38BDF8")
            else:
                btn.config(bg="#0D1322", fg="#64748B")

        self.txt_term.delete("1.0", tk.END)
        for log_line in self.all_logs:
            if self.should_display_log(log_line, flt):
                self.insert_styled_log(log_line)

    def toggle_autoscroll(self):
        self.auto_scroll = not self.auto_scroll
        if self.auto_scroll:
            self.btn_scroll.config(text="AUTO-SCROLL: ON", bg="#064E3B", fg="#34D399")
            self.txt_term.see(tk.END)
        else:
            self.btn_scroll.config(text="AUTO-SCROLL: OFF", bg="#1E293B", fg="#94A3B8")

    def clear_logs(self):
        self.all_logs.clear()
        self.txt_term.delete("1.0", tk.END)

    # =========================================================================
    # CÁC THAO TÁC ĐIỀU KHIỂN THIẾT BỊ (ACTUATOR COMMANDS)
    # =========================================================================
    def toggle_pump(self):
        new_state = not self.gateway.telemetry.get("pumpState", False)
        self.gateway.telemetry["pumpState"] = new_state
        self.gateway.log(f"[ACTUATOR] Lệnh đóng/ngắt Bơm phun sương -> {'BẬT (ON)' if new_state else 'TẮT (OFF)'}")
        self.gateway.send_command_to_esp32("p")
        self.update_display()

    def toggle_fan(self):
        new_state = not self.gateway.telemetry.get("fanState", False)
        self.gateway.telemetry["fanState"] = new_state
        self.gateway.log(f"[ACTUATOR] Lệnh đóng/ngắt Quạt đối lưu phòng sạch -> {'BẬT (ON)' if new_state else 'TẮT (OFF)'}")
        self.gateway.send_command_to_esp32("f")
        self.update_display()

    def toggle_mode(self):
        new_mode = not self.gateway.telemetry.get("autoMode", True)
        self.gateway.telemetry["autoMode"] = new_mode
        self.gateway.log(f"[CONTROL] Chuyển đổi chế độ hệ thống -> {'AUTO' if new_mode else 'MANUAL OVERRIDE'}")
        self.gateway.send_command_to_esp32("a")
        self.update_display()

    def trigger_flame_test(self):
        self.gateway.log("[TEST] !!! KÍCH HOẠT KỊCH BẢN ĐỘT BIẾN NHIỆT THỬ NGHIỆM (FLAME / HEAT TEST) TRÊN GATEWAY GUI !!!")
        self.gateway.telemetry["temperature"] = 46.5
        self.gateway.telemetry["humidity"] = 18.0
        self.gateway.telemetry["fanState"] = True
        self.update_display()

    def trigger_sync_now(self):
        self.gateway.log("[COMMAND] Yêu cầu cưỡng bức đồng bộ Telemetry lên Cloud CoreIOT...")
        self.gateway.publish_to_coreiot()


def main():
    root = tk.Tk()
    app = GatewayGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
