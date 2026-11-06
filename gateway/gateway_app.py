"""
gateway_app.py - IoT Gateway Cục bộ trên PC (Local IoT Gateway)
Đồ án Môn học: Internet of Things (CO3061 / CO3062) - ĐH Bách Khoa TP.HCM (HCMUT)
Hiện thực trọn bộ Nhóm 4:
  - T08: IoT Gateway PC tổng hợp dữ liệu, gửi lên CoreIOT, nhận lệnh điều khiển RPC (1.0đ)
  - T09: Chức năng AI chạy trên IoT Gateway local phân tích dữ liệu, phát hiện dị thường (1.0đ)
  - T10: Giao diện Desktop GUI giám sát - điều khiển trên IoT Gateway local (1.0đ)
Sinh viên thực hiện: Phạm Công Võ (MSSV: 2313946)
"""

import sys
import os
import json
import time
import threading
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import paho.mqtt.client as mqtt

# Hỗ trợ cả Serial và HTTP REST
try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

try:
    from gateway_ai import GatewayAIEngine
except ImportError:
    from gateway.gateway_ai import GatewayAIEngine


class LocalIoTGateway:
    def __init__(self, root=None, headless=False):
        self.root = root
        self.headless = headless
        self.running = True

        # Khởi tạo mô hình AI trên Gateway (T09)
        self.ai_engine = GatewayAIEngine(window_size=20)

        # Trạng thái thiết bị & cảm biến
        self.telemetry = {
            "temperature": 27.5,
            "humidity": 65.0,
            "soilMoisture": 50.0,
            "lightLux": 650.0,
            "pumpState": False,
            "fanState": False,
            "autoMode": True,
            "anomalyScore": 0.05,
            "isAnomaly": False,
            "anomalyType": "NORMAL",
            "onlineAccuracy": 98.5
        }
        self.ai_results = {
            "anomaly_score": 0.05,
            "is_anomaly": False,
            "anomaly_type": "NORMAL",
            "projected_temp_30m": 28.0,
            "irrigation_recommendation": "Đang phân tích vi khí hậu...",
            "irrigation_duration_sec": 0,
            "confidence": 98.0
        }

        # Trạng thái kết nối
        self.esp32_connected = False
        self.coreiot_connected = False
        self.simulation_mode = False

        # Cấu hình nạp từ info.dat hoặc config.json
        self.config = self.load_config()

        # Serial & MQTT clients
        self.ser = None
        self.mqtt_client = None

        # Setup GUI nếu không chạy headless (T10)
        if not self.headless and self.root:
            self.setup_gui()

        # Khởi động luồng nền kết nối CoreIOT và ESP32 (T08)
        self.worker_thread = threading.Thread(target=self.gateway_loop, daemon=True)
        self.worker_thread.start()

    def load_config(self):
        cfg = {
            "coreiot_server": "app.coreiot.io",
            "coreiot_port": 1883,
            "coreiot_token": "h8ycpa6d5uodkqr011cz",
            "esp32_ip": "192.168.1.100",
            "esp32_port": 80,
            "serial_port": "AUTO",
            "serial_baud": 115200,
            "poll_interval": 2.0
        }
        # Thử đọc từ data/info.dat nếu có
        info_dat_path = os.path.join(os.path.dirname(__file__), "..", "data", "info.dat")
        if os.path.exists(info_dat_path):
            try:
                with open(info_dat_path, "r", encoding="utf-8") as f:
                    dat = json.load(f)
                    cfg["coreiot_token"] = dat.get("CORE_IOT_TOKEN", cfg["coreiot_token"])
                    cfg["coreiot_server"] = dat.get("CORE_IOT_SERVER", cfg["coreiot_server"])
                    cfg["coreiot_port"] = int(dat.get("CORE_IOT_PORT", cfg["coreiot_port"]))
            except Exception as e:
                print(f"[CONFIG] Warning loading info.dat: {e}")

        # Thử đọc từ config.json nội bộ gateway
        cfg_json_path = os.path.join(os.path.dirname(__file__), "config.json")
        if os.path.exists(cfg_json_path):
            try:
                with open(cfg_json_path, "r", encoding="utf-8") as f:
                    c = json.load(f)
                    ci = c.get("coreiot", {})
                    cfg["coreiot_server"] = ci.get("server", cfg["coreiot_server"])
                    cfg["coreiot_port"] = ci.get("port", cfg["coreiot_port"])
                    if ci.get("token") and "YOUR_" not in ci["token"]:
                        cfg["coreiot_token"] = ci["token"]
                    esp = c.get("esp32_connection", {})
                    cfg["esp32_ip"] = esp.get("wifi_ip", cfg["esp32_ip"])
                    cfg["serial_port"] = esp.get("serial_port", cfg["serial_port"])
                    cfg["serial_baud"] = esp.get("serial_baudrate", cfg["serial_baud"])
            except Exception as e:
                print(f"[CONFIG] Warning loading config.json: {e}")

        return cfg

    # =========================================================================
    # COREIOT MQTT GIAO TIẾP 2 CHIỀU (T08)
    # =========================================================================
    def init_coreiot_mqtt(self):
        try:
            self.mqtt_client = mqtt.Client(client_id=f"PC_Gateway_{int(time.time())}")
            self.mqtt_client.username_pw_set(self.config["coreiot_token"])

            def on_connect(client, userdata, flags, rc):
                if rc == 0:
                    self.coreiot_connected = True
                    self.log("[MQTT] ✓ Kết nối thành công CoreIOT Cloud (app.coreiot.io:1883)")
                    # Lắng nghe lệnh RPC từ Cloud CoreIOT (T08)
                    client.subscribe("v1/devices/me/rpc/request/+")
                    self.log("[MQTT] ✓ Đã đăng ký lắng nghe lệnh RPC từ Cloud: v1/devices/me/rpc/request/+")
                else:
                    self.coreiot_connected = False
                    self.log(f"[MQTT] ✗ Lỗi kết nối CoreIOT (Mã lỗi: {rc})")

            def on_disconnect(client, userdata, rc):
                self.coreiot_connected = False
                self.log("[MQTT] Cảnh báo: Mất kết nối tới CoreIOT Cloud")

            def on_message(client, userdata, msg):
                self.handle_coreiot_rpc(msg.topic, msg.payload.decode("utf-8", errors="ignore"))

            self.mqtt_client.on_connect = on_connect
            self.mqtt_client.on_disconnect = on_disconnect
            self.mqtt_client.on_message = on_message

            self.log(f"[MQTT] Đang kết nối CoreIOT {self.config['coreiot_server']}:{self.config['coreiot_port']}...")
            self.mqtt_client.connect(self.config["coreiot_server"], self.config["coreiot_port"], 60)
            self.mqtt_client.loop_start()
        except Exception as e:
            self.coreiot_connected = False
            self.log(f"[MQTT] Exception kết nối CoreIOT: {e}")

    def handle_coreiot_rpc(self, topic: str, payload_str: str):
        """
        Nhận lệnh RPC từ CoreIOT và phân phối xuống mạng local ESP32 (T08).
        """
        self.log(f"[RPC NHẬN TỪ CLOUD] Topic: {topic} | Data: {payload_str}")
        try:
            req_id = topic.split("/")[-1]
            data = json.loads(payload_str)
            method = data.get("method")
            params = data.get("params")

            cmd_sent = None
            if method == "setPump":
                val = bool(params)
                self.telemetry["pumpState"] = val
                cmd_sent = {"cmd": "setPump", "value": val}
                self.send_command_to_esp32("p" if val != self.telemetry["pumpState"] else "")
            elif method == "setFan":
                val = bool(params)
                self.telemetry["fanState"] = val
                cmd_sent = {"cmd": "setFan", "value": val}
                self.send_command_to_esp32("f")
            elif method == "setMode":
                is_auto = (params is True or str(params).upper() == "AUTO")
                self.telemetry["autoMode"] = is_auto
                cmd_sent = {"cmd": "setMode", "value": "AUTO" if is_auto else "MANUAL"}
                self.send_command_to_esp32("a")

            if cmd_sent and REQUESTS_AVAILABLE and not self.simulation_mode:
                try:
                    url = f"http://{self.config['esp32_ip']}/api/control"
                    requests.post(url, json=cmd_sent, timeout=1.5)
                except Exception:
                    pass

            # Phản hồi xác nhận hoàn tất RPC về CoreIOT
            resp_topic = f"v1/devices/me/rpc/response/{req_id}"
            resp_payload = json.dumps({"status": "SUCCESS", "gateway": "HCMUT-Gateway", "method": method})
            if self.mqtt_client and self.coreiot_connected:
                self.mqtt_client.publish(resp_topic, resp_payload)
                self.log(f"[RPC PHẢN HỒI] {resp_topic} -> SUCCESS")

        except Exception as e:
            self.log(f"[RPC LỖI] {e}")

    # =========================================================================
    # GIAO TIẾP VỚI ESP32 (SERIAL HOẶC WIFI REST API)
    # =========================================================================
    def connect_serial(self):
        if not SERIAL_AVAILABLE:
            return False
        port = self.config["serial_port"]
        if port == "AUTO":
            ports = list(serial.tools.list_ports.comports())
            for p in ports:
                # Ưu tiên các cổng USB Serial của ESP32/CH340/CP2102
                if any(x in p.description.upper() for x in ["CH340", "CP210", "USB", "ESP32", "SERIAL"]):
                    port = p.device
                    break
            if port == "AUTO" and ports:
                port = ports[0].device

        if port != "AUTO":
            try:
                self.ser = serial.Serial(port, self.config["serial_baud"], timeout=1.0)
                self.esp32_connected = True
                self.log(f"[SERIAL] ✓ Đã mở cổng {port} (Baudrate: {self.config['serial_baud']})")
                return True
            except Exception as e:
                self.log(f"[SERIAL] Không thể mở cổng {port}: {e}")
        return False

    def poll_esp32(self):
        """
        Đọc dữ liệu vi khí hậu mới nhất từ thiết bị ESP32 (Local Node).
        """
        # 1. Thử qua Serial (nếu đã kết nối)
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(b"j\n")
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if line.startswith("{") and line.endswith("}"):
                    d = json.loads(line)
                    self.update_telemetry(d)
                    self.esp32_connected = True
                    return True
            except Exception as e:
                self.log(f"[SERIAL POLL LỖI] {e}")
                self.esp32_connected = False

        # 2. Thử qua WiFi REST API: http://<esp32_ip>/api/status
        if REQUESTS_AVAILABLE and not self.ser:
            try:
                url = f"http://{self.config['esp32_ip']}/api/status"
                res = requests.get(url, timeout=1.8)
                if res.status_code == 200:
                    d = res.json()
                    self.update_telemetry(d)
                    self.esp32_connected = True
                    return True
            except Exception:
                self.esp32_connected = False

        # 3. Chế độ Mô phỏng (Simulation Mode) nếu chưa cắm phần cứng
        if self.simulation_mode or not self.esp32_connected:
            self.generate_simulated_sample()
            return True

        return False

    def update_telemetry(self, d: dict):
        self.telemetry["temperature"] = float(d.get("temperature", d.get("temp", self.telemetry["temperature"])))
        self.telemetry["humidity"] = float(d.get("humidity", d.get("hum", self.telemetry["humidity"])))
        self.telemetry["soilMoisture"] = float(d.get("soilMoisture", d.get("soil", self.telemetry["soilMoisture"])))
        self.telemetry["lightLux"] = float(d.get("lightLux", d.get("light", self.telemetry["lightLux"])))
        self.telemetry["pumpState"] = bool(d.get("pumpState", d.get("pump", self.telemetry["pumpState"])))
        self.telemetry["fanState"] = bool(d.get("fanState", d.get("fan", self.telemetry["fanState"])))
        self.telemetry["autoMode"] = bool(d.get("autoMode", True))

    def generate_simulated_sample(self):
        """Sinh dữ liệu giả lập vi khí hậu khi kiểm thử trên PC không có mạch."""
        import random
        # Dao động nhiệt ẩm tự nhiên
        self.telemetry["temperature"] = round(28.0 + random.uniform(-0.5, 0.5), 1)
        self.telemetry["humidity"] = round(65.0 + random.uniform(-1.0, 1.0), 1)
        self.telemetry["soilMoisture"] = round(max(20.0, min(80.0, self.telemetry["soilMoisture"] - 0.05)), 1)
        self.telemetry["lightLux"] = round(720.0 + random.uniform(-15, 15), 0)

    def send_command_to_esp32(self, cmd_char: str):
        if self.ser and self.ser.is_open and cmd_char:
            try:
                self.ser.write(cmd_char.encode("utf-8"))
                self.log(f"[SERIAL SEND] Gửi lệnh '{cmd_char}' tới ESP32")
            except Exception as e:
                self.log(f"[SERIAL ERROR] {e}")

    # =========================================================================
    # VÒNG LẶP CHÍNH CỦA GATEWAY (T08, T09)
    # =========================================================================
    def gateway_loop(self):
        self.connect_serial()
        self.init_coreiot_mqtt()

        while self.running:
            # 1. Thu thập dữ liệu từ ESP32
            self.poll_esp32()

            # 2. Chạy suy luận AI trên Gateway (T09)
            self.ai_results = self.ai_engine.add_sample(
                temp=self.telemetry["temperature"],
                hum=self.telemetry["humidity"],
                soil=self.telemetry["soilMoisture"]
            )

            # 3. Đóng gói & Gửi dữ liệu tổng hợp lên CoreIOT Cloud (T08)
            self.publish_to_coreiot()

            # 4. Cập nhật giao diện Desktop GUI (T10)
            if not self.headless and self.root:
                try:
                    self.root.after(0, self.update_gui)
                except Exception:
                    pass

            time.sleep(self.config["poll_interval"])

    def publish_to_coreiot(self):
        """Đẩy dữ liệu cảm biến + Kết quả phân tích AI Gateway lên CoreIOT (T08, T09)."""
        if not self.mqtt_client or not self.coreiot_connected:
            return

        payload = {
            # Dữ liệu cảm biến thu thập từ ESP32
            "temperature": self.telemetry["temperature"],
            "humidity": self.telemetry["humidity"],
            "soil": self.telemetry["soilMoisture"],
            "light": self.telemetry["lightLux"],
            "pump": self.telemetry["pumpState"],
            "fan": self.telemetry["fanState"],
            "mode": "AUTO" if self.telemetry["autoMode"] else "MANUAL",
            # Dữ liệu nội suy từ AI Gateway (T09)
            "gateway_ai_anomaly_score": self.ai_results["anomaly_score"],
            "gateway_ai_is_anomaly": self.ai_results["is_anomaly"],
            "gateway_ai_type": self.ai_results["anomaly_type"],
            "gateway_projected_temp_30m": self.ai_results["projected_temp_30m"],
            "gateway_irrigation_advice": self.ai_results["irrigation_recommendation"],
            "gateway_ai_confidence": self.ai_results["confidence"]
        }

        try:
            self.mqtt_client.publish("v1/devices/me/telemetry", json.dumps(payload))
            self.log(f"[GATEWAY → COREIOT] T={payload['temperature']}°C | H={payload['humidity']}% | "
                     f"Soil={payload['soil']}% | AI_Score={payload['gateway_ai_anomaly_score']} ({payload['gateway_ai_type']})")
        except Exception as e:
            self.log(f"[MQTT PUBLISH LỖI] {e}")

    # =========================================================================
    # GIAO DIỆN GIÁM SÁT & ĐIỀU KHIỂN CỤC BỘ TRÊN PC (T10)
    # =========================================================================
    def setup_gui(self):
        self.root.title("HCMUT IoT Gateway — Smart Greenhouse & Edge AI (T08 / T09 / T10)")
        self.root.geometry("980x720")
        self.root.minsize(900, 650)
        self.root.configure(bg="#0f172a")

        # Style chung
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#0f172a")
        style.configure("TLabel", background="#0f172a", foreground="#f8fafc")
        style.configure("Header.TLabel", font=("Segoe UI", 16, "bold"), foreground="#38bdf8")
        style.configure("SubHeader.TLabel", font=("Segoe UI", 9), foreground="#94a3b8")

        # 1. TOP BAR: Tiêu đề & Thông tin tác giả
        top_frame = ttk.Frame(self.root, padding="15 12 15 10")
        top_frame.pack(fill=tk.X)

        title_box = ttk.Frame(top_frame)
        title_box.pack(side=tk.LEFT)
        ttk.Label(title_box, text="🌿 HCMUT LOCAL IOT GATEWAY", style="Header.TLabel").pack(anchor=tk.W)
        ttk.Label(title_box, text="CO3061 IoT | GVHD: TS. Lê Trọng Nhân | SV: Phạm Công Võ (MSSV: 2313946)",
                  style="SubHeader.TLabel").pack(anchor=tk.W)

        # Badges trạng thái
        badge_box = ttk.Frame(top_frame)
        badge_box.pack(side=tk.RIGHT)
        self.lbl_badge_esp = tk.Label(badge_box, text="ESP32: SCANNING", font=("Segoe UI", 9, "bold"),
                                      bg="#334155", fg="#f8fafc", padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_esp.pack(side=tk.LEFT, padx=5)

        self.lbl_badge_mqtt = tk.Label(badge_box, text="COREIOT: CONNECTING", font=("Segoe UI", 9, "bold"),
                                       bg="#334155", fg="#f8fafc", padx=10, pady=4, relief=tk.FLAT)
        self.lbl_badge_mqtt.pack(side=tk.LEFT, padx=5)

        # 2. KHỐI THẺ ĐO LƯỜNG THỜI GIAN THỰC (4 CARDS)
        cards_frame = ttk.Frame(self.root, padding="15 5 15 10")
        cards_frame.pack(fill=tk.X)
        for i in range(4):
            cards_frame.columnconfigure(i, weight=1)

        self.card_temp = self.create_metric_card(cards_frame, 0, "🌡️ NHIỆT ĐỘ KHÍ", "27.5 °C", "#ef4444")
        self.card_hum = self.create_metric_card(cards_frame, 1, "💧 ĐỘ ẨM KHÔNG KHÍ", "65.0 %", "#06b6d4")
        self.card_soil = self.create_metric_card(cards_frame, 2, "🌱 ĐỘ ẨM ĐẤT", "50.0 %", "#10b981")
        self.card_light = self.create_metric_card(cards_frame, 3, "☀️ CƯỜNG ĐỘ SÁNG", "720 Lux", "#f59e0b")

        # 3. KHỐI AI GATEWAY INSIGHTS (T09)
        ai_frame = tk.LabelFrame(self.root, text=" 🧠 CHỨC NĂNG TRÍ TUỆ NHÂN TẠO TRÊN GATEWAY (MÃ T09) ",
                                 font=("Segoe UI", 10, "bold"), bg="#1e293b", fg="#38bdf8", padx=15, pady=10)
        ai_frame.pack(fill=tk.X, padx=15, pady=8)

        ai_cols = ttk.Frame(ai_frame)
        ai_cols.pack(fill=tk.X)
        ai_cols.columnconfigure(0, weight=1)
        ai_cols.columnconfigure(1, weight=1)
        ai_cols.columnconfigure(2, weight=1)

        # Cột 1: Anomaly Score
        c1 = tk.Frame(ai_cols, bg="#1e293b")
        c1.grid(row=0, column=0, sticky="nsew", padx=5)
        tk.Label(c1, text="ĐIỂM DỊ THƯỜNG AI (ANOMALY SCORE):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_score = tk.Label(c1, text="0.05", font=("Segoe UI", 20, "bold"), bg="#1e293b", fg="#22c55e")
        self.lbl_ai_score.pack(anchor=tk.W)
        self.lbl_ai_badge = tk.Label(c1, text="STATUS: NORMAL", font=("Segoe UI", 10, "bold"), bg="#14532d", fg="#86efac", padx=8, pady=2)
        self.lbl_ai_badge.pack(anchor=tk.W, pady=3)

        # Cột 2: Dự báo nhiệt độ 30 phút
        c2 = tk.Frame(ai_cols, bg="#1e293b")
        c2.grid(row=0, column=1, sticky="nsew", padx=5)
        tk.Label(c2, text="DỰ BÁO NHIỆT ĐỘ (30 PHÚT TỚI):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_forecast = tk.Label(c2, text="28.2 °C", font=("Segoe UI", 20, "bold"), bg="#1e293b", fg="#38bdf8")
        self.lbl_ai_forecast.pack(anchor=tk.W)
        tk.Label(c2, text="Mô hình Hồi quy Ngoại suy Chuỗi thời gian", font=("Segoe UI", 9, "italic"), bg="#1e293b", fg="#64748b").pack(anchor=tk.W)

        # Cột 3: Khuyến nghị tưới thông minh
        c3 = tk.Frame(ai_cols, bg="#1e293b")
        c3.grid(row=0, column=2, sticky="nsew", padx=5)
        tk.Label(c3, text="KHUYẾN NGHỊ TƯỚI THÔNG MINH (SMART AI):", font=("Segoe UI", 9), bg="#1e293b", fg="#94a3b8").pack(anchor=tk.W)
        self.lbl_ai_irrigation = tk.Label(c3, text="Độ ẩm đất tối ưu, không cần tưới nước", font=("Segoe UI", 10, "bold"),
                                          bg="#1e293b", fg="#f8fafc", wraplength=260, justify=tk.LEFT)
        self.lbl_ai_irrigation.pack(anchor=tk.W, pady=4)

        # 4. KHỐI ĐIỀU KHIỂN CỤC BỘ TRÊN GATEWAY (T10)
        ctrl_frame = tk.LabelFrame(self.root, text=" ⚡ BẢNG ĐIỀU KHIỂN CỤC BỘ TRÊN GATEWAY (MÃ T10) ",
                                   font=("Segoe UI", 10, "bold"), bg="#1e293b", fg="#10b981", padx=15, pady=10)
        ctrl_frame.pack(fill=tk.X, padx=15, pady=6)

        btn_box = ttk.Frame(ctrl_frame)
        btn_box.pack(fill=tk.X)

        self.btn_pump = tk.Button(btn_box, text="BƠM NƯỚC: OFF", font=("Segoe UI", 10, "bold"),
                                  bg="#334155", fg="#f8fafc", activebackground="#475569", width=18, pady=6,
                                  command=self.toggle_pump_local)
        self.btn_pump.pack(side=tk.LEFT, padx=6)

        self.btn_fan = tk.Button(btn_box, text="QUẠT LÀM MÁT: OFF", font=("Segoe UI", 10, "bold"),
                                 bg="#334155", fg="#f8fafc", activebackground="#475569", width=18, pady=6,
                                 command=self.toggle_fan_local)
        self.btn_fan.pack(side=tk.LEFT, padx=6)

        self.btn_mode = tk.Button(btn_box, text="CHẾ ĐỘ: AUTO", font=("Segoe UI", 10, "bold"),
                                  bg="#0284c7", fg="#ffffff", activebackground="#0369a1", width=18, pady=6,
                                  command=self.toggle_mode_local)
        self.btn_mode.pack(side=tk.LEFT, padx=6)

        self.btn_tamper_test = tk.Button(btn_box, text="🔥 TEST HƠ LỬA AI", font=("Segoe UI", 10, "bold"),
                                         bg="#b91c1c", fg="#ffffff", activebackground="#991b1b", width=18, pady=6,
                                         command=self.trigger_flame_test)
        self.btn_tamper_test.pack(side=tk.RIGHT, padx=6)

        # 5. CONSOLE NHẬT KÝ SỰ KIỆN THỜI GIAN THỰC (EVENT LOGS)
        log_frame = tk.LabelFrame(self.root, text=" 📋 NHẬT KÝ ĐỒNG BỘ GATEWAY ↔ COREIOT (T08 LOGS) ",
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

    def update_gui(self):
        # 1. Cập nhật thẻ đo lường
        self.card_temp.config(text=f"{self.telemetry['temperature']:.1f} °C")
        self.card_hum.config(text=f"{self.telemetry['humidity']:.1f} %")
        self.card_soil.config(text=f"{self.telemetry['soilMoisture']:.0f} %")
        self.card_light.config(text=f"{self.telemetry['lightLux']:.0f} Lux")

        # 2. Cập nhật AI Insights (T09)
        score = self.ai_results["anomaly_score"]
        self.lbl_ai_score.config(text=f"{score:.2f}")
        if score >= 0.60:
            self.lbl_ai_score.config(fg="#ef4444")
            self.lbl_ai_badge.config(text=f"CẢNH BÁO: {self.ai_results['anomaly_type']}", bg="#7f1d1d", fg="#fca5a5")
        else:
            self.lbl_ai_score.config(fg="#22c55e")
            self.lbl_ai_badge.config(text="TRẠNG THÁI: BÌNH THƯỜNG", bg="#14532d", fg="#86efac")

        self.lbl_ai_forecast.config(text=f"{self.ai_results['projected_temp_30m']:.1f} °C")
        self.lbl_ai_irrigation.config(text=self.ai_results["irrigation_recommendation"])

        # 3. Cập nhật nút điều khiển (T10)
        p_on = self.telemetry["pumpState"]
        self.btn_pump.config(
            text=f"BƠM NƯỚC: {'ON' if p_on else 'OFF'}",
            bg="#16a34a" if p_on else "#334155"
        )
        f_on = self.telemetry["fanState"]
        self.btn_fan.config(
            text=f"QUẠT LÀM MÁT: {'ON' if f_on else 'OFF'}",
            bg="#0284c7" if f_on else "#334155"
        )
        is_auto = self.telemetry["autoMode"]
        self.btn_mode.config(
            text=f"CHẾ ĐỘ: {'AUTO' if is_auto else 'MANUAL'}",
            bg="#0284c7" if is_auto else "#f59e0b"
        )

        # 4. Badges
        if self.esp32_connected:
            self.lbl_badge_esp.config(text="ESP32: CONNECTED (OK)", bg="#16a34a")
        else:
            self.lbl_badge_esp.config(text="ESP32: SIMULATED / OFFLINE", bg="#d97706")

        if self.coreiot_connected:
            self.lbl_badge_mqtt.config(text="COREIOT: ONLINE (MQTT)", bg="#16a34a")
        else:
            self.lbl_badge_mqtt.config(text="COREIOT: DISCONNECTED", bg="#dc2626")

    def log(self, text: str):
        t_str = time.strftime("[%H:%M:%S] ")
        full_msg = t_str + text
        print(full_msg)
        if not self.headless and self.root:
            try:
                self.root.after(0, self._append_log, full_msg)
            except Exception:
                pass

    def _append_log(self, text):
        if hasattr(self, "txt_log") and self.txt_log:
            self.txt_log.insert(tk.END, text + "\n")
            self.txt_log.see(tk.END)

    # =========================================================================
    # HÀNH ĐỘNG ĐIỀU KHIỂN TỪ NÚT BẤM GUI (T10)
    # =========================================================================
    def toggle_pump_local(self):
        new_state = not self.telemetry["pumpState"]
        self.telemetry["pumpState"] = new_state
        self.log(f"[LOCAL GUI] Đổi trạng thái Bơm -> {'BẬT' if new_state else 'TẮT'}")
        self.send_command_to_esp32("p")

    def toggle_fan_local(self):
        new_state = not self.telemetry["fanState"]
        self.telemetry["fanState"] = new_state
        self.log(f"[LOCAL GUI] Đổi trạng thái Quạt -> {'BẬT' if new_state else 'TẮT'}")
        self.send_command_to_esp32("f")

    def toggle_mode_local(self):
        new_mode = not self.telemetry["autoMode"]
        self.telemetry["autoMode"] = new_mode
        self.log(f"[LOCAL GUI] Đổi chế độ -> {'AUTO' if new_mode else 'MANUAL'}")
        self.send_command_to_esp32("a")

    def trigger_flame_test(self):
        self.log("[TEST] !!! KÍCH HOẠT KỊCH BẢN HƠ LỬA THỬ NGHIỆM TRÊN GATEWAY !!!")
        self.telemetry["temperature"] = 46.5
        self.telemetry["humidity"] = 18.0
        self.telemetry["fanState"] = True


def main():
    headless = "--headless" in sys.argv
    if headless:
        print("=== CHẠY IOT GATEWAY Ở CHẾ ĐỘ HEADLESS (NO GUI) ===")
        gw = LocalIoTGateway(headless=True)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("Dừng Gateway.")
    else:
        root = tk.Tk()
        app = LocalIoTGateway(root=root, headless=False)
        root.mainloop()


if __name__ == "__main__":
    main()
