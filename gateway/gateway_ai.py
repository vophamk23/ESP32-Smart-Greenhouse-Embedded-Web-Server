"""
# Cleanroom Bio-Chamber Edge-AI — Fog Predictive Analytics Engine
#
# gateway_ai.py - Bộ phân tích AI đa biến & dự báo vi khí hậu trên Fog IoT Gateway
"""

import numpy as np
import time
from collections import deque


class GatewayAIEngine:
    """
    Bộ phân tích dữ liệu vi khí hậu phòng sạch và phát hiện dị thường chạy trên Fog IoT Gateway.
    Tích hợp 3 mô hình học máy & suy luận thống kê:
      1. Multi-variate Anomaly Detection (Z-Score + Tốc độ biến thiên đạo hàm).
      2. Mô hình Hồi quy Ngoại suy Xu hướng Nhiệt độ 30 phút tới (OLS Linear Regression).
      3. Thuật toán Ra quyết định Phun sương & Bù ẩm Giá thể Phòng sạch (Smart Mist AI).
    """

    def __init__(self, window_size: int = 20):
        self.window_size = window_size
        self.history_temp = deque(maxlen=window_size)
        self.history_hum = deque(maxlen=window_size)
        self.history_soil = deque(maxlen=window_size)
        self.history_timestamps = deque(maxlen=window_size)

        # Baseline tham chiếu vi khí hậu phòng sạch nuôi cấy vi tảo / nấm công nghiệp (Bio-Chamber)
        self.baseline_temp_mean = 27.5
        self.baseline_temp_std = 3.5
        self.baseline_hum_mean = 65.0
        self.baseline_hum_std = 12.0

    def add_sample(self, temp: float, hum: float, soil: float) -> dict:
        """
        Nạp một mẫu dữ liệu mới từ ESP32 và chạy toàn bộ quy trình suy luận AI.
        """
        now = time.time()
        self.history_temp.append(temp)
        self.history_hum.append(hum)
        self.history_soil.append(soil)
        self.history_timestamps.append(now)

        return self.analyze()

    def analyze(self) -> dict:
        """
        Chạy phân tích đa biến trên chuỗi thời gian (Time-series Analysis).
        """
        if len(self.history_temp) < 2:
            return {
                "anomaly_score": 0.05,
                "is_anomaly": False,
                "anomaly_type": "INITIALIZING",
                "projected_temp_30m": self.history_temp[-1] if self.history_temp else 28.0,
                "irrigation_recommendation": "Đang thu thập dữ liệu...",
                "irrigation_duration_sec": 0,
                "confidence": 75.0
            }

        temps = np.array(self.history_temp)
        hums = np.array(self.history_hum)
        soils = np.array(self.history_soil)
        times = np.array(self.history_timestamps)

        cur_temp = temps[-1]
        cur_hum = hums[-1]
        cur_soil = soils[-1]

        dt = times[-1] - times[-2] if len(times) >= 2 and (times[-1] - times[-2]) > 0 else 1.0
        d_temp_rate = (cur_temp - temps[-2]) / dt
        d_hum_rate = (cur_hum - hums[-2]) / dt

        # --------------------------------------------------------------------
        # 1. TÍNH ĐIỂM DỊ THƯỜNG ĐA BIẾN (MULTI-VARIATE ANOMALY SCORE)
        # --------------------------------------------------------------------
        z_temp = abs(cur_temp - self.baseline_temp_mean) / self.baseline_temp_std
        z_hum = abs(cur_hum - self.baseline_hum_mean) / self.baseline_hum_std

        # Tương quan nghịch giữa nhiệt tăng vọt và ẩm tụt dốc (Dấu hiệu hơ lửa / cháy)
        coupling_factor = 0.0
        if d_temp_rate > 0.20 and d_hum_rate < -0.25:
            coupling_factor = 0.70

        # Tổng hợp điểm dị thường chuẩn hóa về đoạn [0.0, 1.0]
        base_score = min(1.0, (z_temp * 0.25 + z_hum * 0.15 + coupling_factor))
        anomaly_score = float(np.clip(base_score, 0.02, 1.0))

        # Phân loại kịch bản bất thường
        is_anomaly = anomaly_score >= 0.60
        if coupling_factor >= 0.50 or (d_temp_rate > 0.30 and d_hum_rate < -0.30):
            anomaly_type = "FLAME_HEAT_SPIKE"
            anomaly_score = max(anomaly_score, 0.98)
            is_anomaly = True
        elif cur_temp >= 40.0:
            anomaly_type = "EXTREME_HEATWAVE"
            is_anomaly = True
        elif cur_temp <= 14.0:
            anomaly_type = "FROST_COLD_HAZARD"
            is_anomaly = True
        elif cur_soil <= 25.0 and cur_temp >= 33.0:
            anomaly_type = "DROUGHT_STRESS"
            is_anomaly = True
        elif abs(d_temp_rate) > 0.40 or abs(d_hum_rate) > 0.90:
            anomaly_type = "SENSOR_RAPID_DRIFT"
            is_anomaly = True
        else:
            anomaly_type = "NORMAL"

        # --------------------------------------------------------------------
        # 2. HỒI QUY NGOẠI SUY XU HƯỚNG NHIỆT ĐỘ 30 PHÚT TỚI (30-MIN FORECAST)
        # --------------------------------------------------------------------
        if len(temps) >= 5:
            # Hồi quy tuyến tính trên cửa sổ trượt gần nhất
            x_indices = np.arange(len(temps))
            slope, intercept = np.polyfit(x_indices, temps, 1)
            # Giới hạn độ dốc trên mỗi chu kỳ để tránh ngoại suy phi lý do nhiễu tức thời
            clamped_slope = float(np.clip(slope, -0.15, 0.25))
            delta_30m = clamped_slope * min(15.0, len(temps)) * 0.4
            projected_temp = float(cur_temp + delta_30m)
        else:
            # Dự báo khởi động: quán tính nhiệt nhẹ
            smoothed_rate = float(np.clip(d_temp_rate, -0.05, 0.05))
            projected_temp = float(cur_temp + (smoothed_rate * 25.0))

        # Ràng buộc vật lý vi khí hậu thực tế trong khoảng [cur_temp - 3.0, cur_temp + 5.0]
        if anomaly_type == "FLAME_HEAT_SPIKE":
            projected_temp = min(58.0, cur_temp + 8.5)
        else:
            projected_temp = float(np.clip(projected_temp, max(15.0, cur_temp - 3.0), min(45.0, cur_temp + 4.5)))

        projected_temp = round(projected_temp, 1)

        # --------------------------------------------------------------------
        # 3. MÔ HÌNH TỐI ƯU HÓA PHUN SƯƠNG TẠO ẨM BUỒNG SẠCH (SMART MIST AI)
        # --------------------------------------------------------------------
        rec_duration = 0
        if cur_soil < 30.0:
            # Thiếu ẩm giá thể: Tính thời gian phun sương bù theo độ khô phôi và nhiệt độ buồng sạch
            deficit = 60.0 - cur_soil  # Mục tiêu đưa độ ẩm giá thể về 60%
            heat_multiplier = 1.0 + max(0.0, (cur_temp - 30.0) * 0.05)
            rec_duration = int(np.clip(deficit * 1.5 * heat_multiplier, 15, 120))
            recommendation = f"CẦN PHUN SƯƠNG: Bật bơm {rec_duration} giây (Độ ẩm giá thể thấp {cur_soil:.0f}%)"
        elif cur_soil < 45.0 and cur_temp > 34.0:
            rec_duration = 30
            recommendation = f"CẢNH BÁO SỐC NHIỆT BUỒNG SẠCH: Phun sương làm mát nhẹ {rec_duration} giây"
        elif cur_soil > 75.0:
            rec_duration = 0
            recommendation = f"GIÁ THỂ BÃO HÒA ẨM ({cur_soil:.0f}%): Ngưng phun sương hoàn toàn"
        else:
            rec_duration = 0
            recommendation = "ĐỘ ẨM GIÁ THỂ TỐI ƯU: Không cần phun sương tạo ẩm"

        return {
            "anomaly_score": round(anomaly_score, 2),
            "is_anomaly": is_anomaly,
            "anomaly_type": anomaly_type,
            "projected_temp_30m": projected_temp,
            "irrigation_recommendation": recommendation,
            "irrigation_duration_sec": rec_duration,
            "confidence": round(float(np.clip(98.5 - (anomaly_score * 8.0), 80.0, 99.2)), 1)
        }
