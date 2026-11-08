"""
# [T08] IOT GATEWAY HIEN THUC TREN PC (HOAC RASPBERRY PI)
# Yeu to cong nghe T08: Tong hop du lieu, gui len CoreIOT, nhan lenh dieu khien tu CoreIOT
# va phan phoi cho cac thiet bi trong mang local
#
gateway_core.py - Core IoT Gateway Cục bộ trên PC / Raspberry Pi
Đồ án Môn học: Internet of Things (CO3061 / CO3062) - ĐH Bách Khoa TP.HCM (HCMUT)
Mã yếu tố công nghệ: T08 (1.0 Điểm)
Chức năng: Tổng hợp dữ liệu từ ESP32, gửi lên CoreIOT Cloud, nhận lệnh điều khiển RPC từ CoreIOT và phân phối cho thiết bị trong mạng local.
Sinh viên thực hiện: Phạm Công Võ (MSSV: 2313946)
"""

import os
import json
import time
import threading
import paho.mqtt.client as mqtt

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
    import minimalmodbus
    MINIMALMODBUS_AVAILABLE = True
except ImportError:
    MINIMALMODBUS_AVAILABLE = False

try:
    from gateway_ai import GatewayAIEngine
except ImportError:
    from gateway.gateway_ai import GatewayAIEngine


class LocalIoTGatewayCore:
    """
    Hiện thực mã T08:
      - Quản lý kênh kết nối Local với ESP32 (qua cáp USB Serial hoặc WiFi HTTP).
      - Quản lý kết nối MQTT hai chiều với Server CoreIOT Cloud (app.coreiot.io).
      - Đóng gói Telemetry đẩy lên Cloud.
      - Nhận lệnh RPC từ CoreIOT và chuyển tiếp (phân phối) xuống thiết bị local.
    """

    def __init__(self, log_callback=None, update_callback=None):
        self.log_callback = log_callback
        self.update_callback = update_callback
        self.running = True

        # Mô hình AI trên Gateway (Mã T09)
        self.ai_engine = GatewayAIEngine(window_size=20)

        # Dữ liệu vi khí hậu hiện tại
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

        # Kết quả AI Gateway phân tích (T09)
        self.ai_results = {
            "anomaly_score": 0.05,
            "is_anomaly": False,
            "anomaly_type": "NORMAL",
            "projected_temp_30m": 28.0,
            "irrigation_recommendation": "Đang khởi tạo phân tích...",
            "irrigation_duration_sec": 0,
            "confidence": 98.0
        }

        # Trạng thái kết nối
        self.esp32_connected = False
        self.coreiot_connected = False
        self.simulation_mode = False

        # Cấu hình nạp tự động
        self.config = self.load_config()

        # Clients
        self.ser = None
        self.mqtt_client = None

        # Khởi động thread chạy Gateway nền (T08)
        self.worker_thread = threading.Thread(target=self.gateway_loop, daemon=True)
        self.worker_thread.start()

    def log(self, msg: str):
        t_str = time.strftime("[%H:%M:%S] ")
        line = t_str + msg
        print(line)
        if self.log_callback:
            try:
                self.log_callback(line)
            except Exception:
                pass

    def load_config(self):
        cfg = {
            "coreiot_server": "app.coreiot.io",
            "coreiot_port": 1883,
            "coreiot_token": "h8ycpa6d5uodkqr011cz",
            "esp32_ip": "192.168.1.100",
            "serial_port": "AUTO",
            "serial_baud": 115200,
            "modbus_enabled": False,
            "modbus_port": "COM8",
            "modbus_slave_id": 4,
            "modbus_baud": 9600,
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
                self.log(f"[CONFIG] Warning loading info.dat: {e}")

        # Thử đọc config.json
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
                self.log(f"[CONFIG] Warning loading config.json: {e}")

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
                    self.log("[COREIOT MQTT] ✓ Kết nối thành công Cloud CoreIOT (app.coreiot.io:1883)")
                    # Lắng nghe lệnh điều khiển RPC từ xa (T08)
                    client.subscribe("v1/devices/me/rpc/request/+")
                    self.log("[COREIOT MQTT] ✓ Đã đăng ký nhận lệnh điều khiển RPC từ Cloud")
                else:
                    self.coreiot_connected = False
                    self.log(f"[COREIOT MQTT] ✗ Lỗi kết nối CoreIOT (Mã lỗi: {rc})")

            def on_disconnect(client, userdata, rc):
                self.coreiot_connected = False
                self.log("[COREIOT MQTT] Cảnh báo: Mất kết nối tới CoreIOT Cloud")

            def on_message(client, userdata, msg):
                self.handle_coreiot_rpc(msg.topic, msg.payload.decode("utf-8", errors="ignore"))

            self.mqtt_client.on_connect = on_connect
            self.mqtt_client.on_disconnect = on_disconnect
            self.mqtt_client.on_message = on_message

            self.log(f"[COREIOT MQTT] Đang kết nối tới {self.config['coreiot_server']}:{self.config['coreiot_port']}...")
            self.mqtt_client.connect(self.config["coreiot_server"], self.config["coreiot_port"], 60)
            self.mqtt_client.loop_start()
        except Exception as e:
            self.coreiot_connected = False
            self.log(f"[COREIOT MQTT] Lỗi khởi tạo: {e}")

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
                self.send_command_to_esp32("p")
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

            # Phản hồi hoàn tất RPC về CoreIOT Cloud
            resp_topic = f"v1/devices/me/rpc/response/{req_id}"
            resp_payload = json.dumps({"status": "SUCCESS", "gateway": "HCMUT-Gateway", "method": method})
            if self.mqtt_client and self.coreiot_connected:
                self.mqtt_client.publish(resp_topic, resp_payload)
                self.log(f"[RPC PHẢN HỒI] {resp_topic} -> SUCCESS")

        except Exception as e:
            self.log(f"[RPC LỖI] {e}")

    # =========================================================================
    # GIAO TIẾP VỚI ESP32 TRONG MẠNG LOCAL (T08)
    # =========================================================================
    def connect_serial(self):
        if not SERIAL_AVAILABLE:
            return False
        port = self.config["serial_port"]
        if port == "AUTO":
            ports = list(serial.tools.list_ports.comports())
            for p in ports:
                if any(x in p.description.upper() for x in ["CH340", "CP210", "USB", "ESP32", "SERIAL"]):
                    port = p.device
                    break
            if port == "AUTO" and ports:
                port = ports[0].device

        if port != "AUTO":
            try:
                self.ser = serial.Serial(port, self.config["serial_baud"], timeout=1.0)
                self.esp32_connected = True
                self.log(f"[SERIAL LOCAL] ✓ Đã mở kết nối cổng {port} (Baud: {self.config['serial_baud']})")
                return True
            except Exception as e:
                self.log(f"[SERIAL LOCAL] Không thể mở cổng {port}: {e}")
        return False

    def poll_modbus_rs485(self):
        """Đọc trực tiếp từ Cảm biến Nhiệt ẩm Modbus RS485 (Mã T02)."""
        if not MINIMALMODBUS_AVAILABLE:
            return False
        try:
            if not hasattr(self, "modbus_instrument") or self.modbus_instrument is None:
                port = self.config.get("modbus_port", "COM8")
                slave_id = int(self.config.get("modbus_slave_id", 4))
                baud = int(self.config.get("modbus_baud", 9600))
                self.modbus_instrument = minimalmodbus.Instrument(port, slave_id)
                self.modbus_instrument.serial.baudrate = baud
                self.modbus_instrument.serial.bytesize = 8
                self.modbus_instrument.serial.parity = serial.PARITY_NONE
                self.modbus_instrument.serial.stopbits = 1
                self.modbus_instrument.serial.timeout = 1.0
                self.modbus_instrument.mode = minimalmodbus.MODE_RTU
                self.log(f"[MODBUS RS485] ✓ Đã mở kết nối cổng {port} (Slave ID: {slave_id}, Baud: {baud})")

            # Đọc thanh ghi 0 (Độ ẩm) và thanh ghi 1 (Nhiệt độ) - Function Code 03
            humidity_raw = self.modbus_instrument.read_register(0, number_of_decimals=0, functioncode=3)
            temperature_raw = self.modbus_instrument.read_register(1, number_of_decimals=0, functioncode=3)

            self.telemetry["humidity"] = round(humidity_raw / 10.0, 1)
            self.telemetry["temperature"] = round(temperature_raw / 10.0, 1)
            self.esp32_connected = True
            return True
        except Exception as e:
            self.log(f"[MODBUS RS485 ERROR] {e}")
            self.modbus_instrument = None
            return False

    def poll_esp32(self):
        # 0. Thử đọc trực tiếp qua Modbus RS485 nếu bật
        if MINIMALMODBUS_AVAILABLE and self.config.get("modbus_enabled", False):
            if self.poll_modbus_rs485():
                return True

        # 1. Thử qua Serial
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
                self.log(f"[SERIAL POLL ERROR] {e}")
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

        # 3. Chế độ Mô phỏng (Simulation) nếu chưa cắm phần cứng
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
        import random
        self.telemetry["temperature"] = round(28.0 + random.uniform(-0.5, 0.5), 1)
        self.telemetry["humidity"] = round(65.0 + random.uniform(-1.0, 1.0), 1)
        self.telemetry["soilMoisture"] = round(max(20.0, min(80.0, self.telemetry["soilMoisture"] - 0.05)), 1)
        self.telemetry["lightLux"] = round(720.0 + random.uniform(-15, 15), 0)

    def send_command_to_esp32(self, cmd_char: str):
        if self.ser and self.ser.is_open and cmd_char:
            try:
                self.ser.write(cmd_char.encode("utf-8"))
                self.log(f"[LỆNH ĐIỀU KHIỂN LOCAL] Gửi '{cmd_char}' tới ESP32")
            except Exception as e:
                self.log(f"[SERIAL SEND ERROR] {e}")

    # =========================================================================
    # VÒNG LẶP CHÍNH CỦA GATEWAY (T08, T09)
    # =========================================================================
    def gateway_loop(self):
        self.connect_serial()
        self.init_coreiot_mqtt()

        while self.running:
            # 1. Thu thập dữ liệu từ ESP32
            self.poll_esp32()

            # 2. Phân tích AI trên Gateway (T09)
            self.ai_results = self.ai_engine.add_sample(
                temp=self.telemetry["temperature"],
                hum=self.telemetry["humidity"],
                soil=self.telemetry["soilMoisture"]
            )

            # 3. Đẩy dữ liệu tổng hợp lên CoreIOT Cloud (T08)
            self.publish_to_coreiot()

            # 4. Báo cho giao diện GUI cập nhật nếu có callback (T10)
            if self.update_callback:
                try:
                    self.update_callback()
                except Exception:
                    pass

            time.sleep(self.config["poll_interval"])

    def publish_to_coreiot(self):
        if not self.mqtt_client or not self.coreiot_connected:
            return

        payload = {
            "temperature": self.telemetry["temperature"],
            "humidity": self.telemetry["humidity"],
            "soil": self.telemetry["soilMoisture"],
            "light": self.telemetry["lightLux"],
            "pump": self.telemetry["pumpState"],
            "fan": self.telemetry["fanState"],
            "mode": "AUTO" if self.telemetry["autoMode"] else "MANUAL",
            # Dữ liệu phân tích từ AI Gateway (T09)
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
            self.log(f"[MQTT PUBLISH ERROR] {e}")


if __name__ == "__main__":
    print("=== CHẠY IOT GATEWAY CORE (T08) Ở CHẾ ĐỘ TERMINAL DAEMON ===")
    gw = LocalIoTGatewayCore()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        gw.running = False
        print("Dừng Gateway Core.")
