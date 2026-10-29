/*
 * tinyml_engine.h - Giao diện Bộ suy luận Trí tuệ Nhân tạo Biên (Edge AI / TinyML)
 * Chạy trực tiếp on-chip trên vi điều khiển ESP32-S3 (Yolo:UNO).
 * 
 * Tích hợp:
 *   1. Đánh giá chỉ số tiện nghi sinh học / sốc nhiệt (Heat Stress Index NOAA).
 *   2. Phân loại vi khí hậu (Multi-Stage Decision Forest).
 *   3. Dự báo xu hướng nhiệt độ 30 phút tới.
 *   4. PHÁT HIỆN DỊ THƯỜNG CẢM BIẾN (Sensor Anomaly Detection):
 *      - Phát hiện hơ lửa / phá hoại nhiệt (Flame / Thermal Tamper).
 *      - Phát hiện trôi cảm biến / kẹt số liệu (Sensor Drift / Stuck).
 *      - Tính Anomaly Score (0.00 - 1.00), Ground Truth, Online Accuracy (%).
 *   5. Benchmark thời gian suy luận (Latency) micro-giây.
 */

#ifndef TINYML_ENGINE_H
#define TINYML_ENGINE_H

#include <Arduino.h>

// Các nhãn phân loại trạng thái vi khí hậu
enum AI_Environment_State {
    AI_STATE_COMFORTABLE = 0,   // Môi trường lý tưởng, an toàn (Xanh lá)
    AI_STATE_HEAT_WARNING = 1,  // Cảnh báo sốc nhiệt / Cần làm mát (Vàng)
    AI_STATE_EXTREME_ALERT = 2  // Cực đoan / Nguy hiểm quá nhiệt hoặc cháy (Đỏ)
};

// Cấu trúc kết quả suy luận AI TinyML toàn diện
struct TinyML_Result_t {
    // 1. Phân loại vi khí hậu & Chỉ số nhiệt
    AI_Environment_State state; // Nhãn trạng thái (0, 1, 2)
    float confidence;           // Độ tin cậy của mô hình (0.0 - 100.0%)
    float heatIndex;            // Chỉ số cảm nhận nhiệt tính toán (°C)
    float projectedTemp30m;     // Nhiệt độ dự báo sau 30 phút (°C)
    uint32_t inferenceTimeUs;   // Thời gian suy luận (micro-giây)
    char stateStr[32];          // Chuỗi trạng thái hiển thị
    char forecastStr[64];       // Chuỗi dự báo xu hướng vi khí hậu
    bool shouldTriggerFan;      // Đề xuất AI kích hoạt quạt
    bool shouldTriggerPump;     // Đề xuất AI kích hoạt bơm tưới

    // 2. Phát hiện Dị thường Cảm biến (Sensor Anomaly Detection)
    float anomalyScore;         // Điểm dị thường (0.00 - 1.00, > 0.60 là bất thường)
    bool isAnomaly;             // Cờ báo dị thường (predictedAnomaly)
    bool groundTruthAnomaly;    // Cờ đối chứng vật lý (groundTruthAnomaly)
    float onlineAccuracy;       // Tỷ lệ dự đoán đúng tích lũy (%)
    char anomalyType[32];       // "NORMAL", "FLAME_TAMPER", "SENSOR_FAULT", "RAPID_DRIFT"
};

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

/**
 * @brief Tính toán chỉ số cảm nhận nhiệt Heat Index (NOAA Rothfusz Regression)
 */
float calculateHeatIndex(float tempC, float humidity);

/**
 * @brief Bộ suy luận AI TinyML on-device phân loại vi khí hậu và phát hiện dị thường
 * @param currentTemp Nhiệt độ hiện tại (°C)
 * @param currentHum Độ ẩm hiện tại (%)
 * @param currentLight Cường độ ánh sáng (Lux)
 * @param deltaTempRate Tốc độ biến thiên nhiệt (°C/giây)
 * @param deltaHumRate Tốc độ biến thiên độ ẩm (%/giây)
 * @param sensorError Cờ báo lỗi phần cứng từ driver cảm biến
 * @return TinyML_Result_t Kết quả suy luận và phân tích dị thường đầy đủ
 */
TinyML_Result_t TinyML_Predict(float currentTemp, float currentHum, float currentLight,
                               float deltaTempRate, float deltaHumRate, bool sensorError = false);

#endif // TINYML_ENGINE_H
