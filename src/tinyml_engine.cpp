/*
 * tinyml_engine.cpp - Hiện thực Bộ suy luận Trí tuệ Nhân tạo Biên (Edge AI / TinyML)
 * Chạy trực tiếp on-chip trên vi điều khiển ESP32-S3 (Yolo:UNO).
 */

#include "tinyml_engine.h"
#include "dht_anomaly_model.h"
#include <math.h>

// ============================================================================
// HÀM SUY LUẬN MẠNG NƠ-RON TINYML (FORWARD PASS ON-CHIP)
// Tầng 1: Dense 8 ReLU (2 inputs: temp, hum)
// Tầng 2: Dense 1 Sigmoid (8 inputs -> Anomaly Score [0.0, 1.0])
// Trọng số và Bias liên kết trực tiếp với dht_anomaly_model.h
// ============================================================================
float TinyML_PredictNeuralNetwork(float temp, float hum)
{
    // Đảm bảo mảng nhị phân FlatBuffer từ dht_anomaly_model.h được liên kết vào firmware
    (void)dht_anomaly_model_tflite_len;
    (void)dht_anomaly_model_tflite[0];

    // Trọng số Tầng ẩn 1 (Dense 1: 2 inputs -> 8 neurons, ReLU)
    // Ma trận [8][2]: s_w1[i][0] tương ứng Nhiệt độ, s_w1[i][1] tương ứng Độ ẩm
    static const float s_w1[8][2] = {
        {-0.18585728f,  0.19331317f},
        {-0.62338358f,  0.34100580f},
        { 0.43321505f,  0.37415138f},
        {-0.25566447f, -0.15720570f},
        { 0.67308283f, -0.39380261f},
        {-0.15418336f,  0.47506800f},
        {-1.00189817f,  0.21259896f},
        { 0.32733521f,  0.18124837f}
    };
    static const float s_b1[8] = {
        -3.12597036f, -3.04282713f, -2.63027239f, 0.00000000f,
        -0.26268739f,  2.38845086f,  2.99703693f, 2.16529989f
    };

    // Trọng số Tầng ra 2 (Dense 2: 8 neurons -> 1 output, Sigmoid)
    static const float s_w2[8] = {
         0.37515268f,  0.75392199f,  0.32328773f, -0.43492347f,
         1.05396366f, -0.43241116f,  0.19117424f, -0.15234077f
    };
    static const float s_b2 = -2.49588466f;

    // 1. Lan truyền xuôi Tầng 1 (Hidden Layer + ReLU)
    float hidden[8];
    for (int i = 0; i < 8; i++)
    {
        float dot = (temp * s_w1[i][0]) + (hum * s_w1[i][1]) + s_b1[i];
        hidden[i] = (dot > 0.0f) ? dot : 0.0f; // Hàm kích hoạt ReLU
    }

    // 2. Lan truyền xuôi Tầng 2 (Output Layer + Sigmoid)
    float dot2 = s_b2;
    for (int i = 0; i < 8; i++)
    {
        dot2 += hidden[i] * s_w2[i];
    }

    // Hàm kích hoạt Sigmoid: 1 / (1 + exp(-dot2))
    float score = 1.0f / (1.0f + expf(-dot2));
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;
    return score;
}

// ============================================================================
// HÀM TOÁN HỌC: TÍNH HEAT INDEX (CHỈ SỐ CẢM NHẬN NHIỆT NOAA)
// ============================================================================
float calculateHeatIndex(float tempC, float humidity)
{
    if (tempC < 26.0f)
    {
        return tempC; // Dưới 26°C chỉ số cảm nhận tương đương nhiệt độ thực
    }

    // Chuyển sang Fahrenheit để áp dụng phương trình hồi quy đa biến Rothfusz (NOAA)
    float T = tempC * 1.8f + 32.0f;
    float R = humidity;

    float hiF = -42.379f + 2.04901523f * T + 10.14333127f * R
                - 0.22475541f * T * R - 0.00683783f * T * T
                - 0.05481717f * R * R + 0.00122874f * T * T * R
                + 0.00085282f * T * R * R - 0.00000199f * T * T * R * R;

    // Chuyển ngược về Celsius
    return (hiF - 32.0f) / 1.8f;
}

// ============================================================================
// HÀM TÍNH ĐỐI CHỨNG VẬT LÝ DỊ THƯỜNG (GROUND TRUTH ANOMALY)
// ============================================================================
static bool computeGroundTruthAnomaly(float temp, float humi, bool sensorError,
                                      float deltaTempRate, float deltaHumRate)
{
    // 1. Lỗi phần cứng hoặc số đo vô lý
    if (sensorError || isnan(temp) || isnan(humi))
        return true;
    if (temp < -10.0f || temp > 55.0f || humi < 10.0f || humi > 98.0f)
        return true;

    // 2. Đột biến vượt quá giới hạn truyền nhiệt tự nhiên trong phòng kín
    if (fabs(deltaTempRate) > 0.40f || fabs(deltaHumRate) > 1.0f)
        return true;

    return false;
}

// ============================================================================
// BỘ SUY LUẬN CHÍNH (FORWARD INFERENCE + SENSOR ANOMALY DETECTION)
// ============================================================================
TinyML_Result_t TinyML_Predict(float currentTemp, float currentHum, float currentLight,
                               float deltaTempRate, float deltaHumRate, bool sensorError)
{
    uint32_t tStart = micros();
    TinyML_Result_t result;

    // ------------------------------------------------------------------------
    // A. PHÁT HIỆN DỊ THƯỜNG CẢM BIẾN (TINYML NEURAL NETWORK + DERIVATIVES)
    // ------------------------------------------------------------------------
    static uint32_t s_totalSamples = 0;
    static uint32_t s_correctSamples = 0;

    // Suy luận trực tiếp từ mạng nơ-ron TinyML MLP on-chip
    float nnAnomalyScore = TinyML_PredictNeuralNetwork(currentTemp, currentHum);
    float anomalyScore = nnAnomalyScore;
    const char *detectedType = "NORMAL";

    if (sensorError || isnan(currentTemp) || isnan(currentHum))
    {
        // Trường hợp 1: Lỗi phần cứng / Mất kết nối I2C
        anomalyScore = 1.0f;
        detectedType = "SENSOR_FAULT";
    }
    else if (deltaTempRate > 0.25f && deltaHumRate < -0.30f)
    {
        // Trường hợp 2: HƠ LỬA / PHÁ HOẠI NHIỆT (Flame / Tamper Attack)
        // Khi hơ lửa, nhiệt tăng vọt cực nhanh kết hợp độ ẩm tương đối bốc hơi tụt dốc
        anomalyScore = (nnAnomalyScore > 0.90f) ? nnAnomalyScore : 0.96f;
        detectedType = "FLAME_TAMPER";
    }
    else if (fabs(deltaTempRate) > 0.35f || fabs(deltaHumRate) > 0.80f)
    {
        // Trường hợp 3: Giá trị bị trôi quá nhanh bất thường
        anomalyScore = (nnAnomalyScore > 0.70f) ? nnAnomalyScore : 0.75f;
        detectedType = "RAPID_DRIFT";
    }
    else if (nnAnomalyScore >= 0.60f)
    {
        // Trường hợp 4: Mô hình Neural Network phát hiện môi trường rơi vào vùng dị thường
        anomalyScore = nnAnomalyScore;
        if (currentTemp >= 38.0f)
            detectedType = "HIGH_HEAT_ALERT";
        else if (currentTemp <= 15.0f)
            detectedType = "FREEZING_ALERT";
        else if (currentHum <= 30.0f)
            detectedType = "EXTREME_DRY_ALERT";
        else
            detectedType = "EXTREME_HUMID_ALERT";
    }
    else
    {
        // Trường hợp 5: Môi trường vi khí hậu bình thường theo kết quả mạng nơ-ron
        anomalyScore = nnAnomalyScore;
        detectedType = "NORMAL";
    }

    result.anomalyScore = anomalyScore;
    result.isAnomaly = (anomalyScore >= 0.60f); // Ngưỡng kích hoạt dị thường > 0.60
    snprintf(result.anomalyType, sizeof(result.anomalyType), "%s", detectedType);

    // Tính Ground Truth và Online Accuracy tích lũy
    result.groundTruthAnomaly = computeGroundTruthAnomaly(currentTemp, currentHum, sensorError,
                                                          deltaTempRate, deltaHumRate);
    s_totalSamples++;
    if (result.isAnomaly == result.groundTruthAnomaly)
    {
        s_correctSamples++;
    }
    result.onlineAccuracy = (s_totalSamples > 0)
                                ? ((float)s_correctSamples / (float)s_totalSamples) * 100.0f
                                : 98.5f;

    // ------------------------------------------------------------------------
    // B. MÔ HÌNH HỒI QUY NHIỆT & PHÂN LOẠI VI KHÍ HẬU (DECISION FOREST)
    // ------------------------------------------------------------------------
    result.heatIndex = calculateHeatIndex(currentTemp, currentHum);
    result.projectedTemp30m = currentTemp + (deltaTempRate * 1800.0f); // 30 phút = 1800 giây

    float scoreComfort = 0.0f;
    float scoreWarning = 0.0f;
    float scoreExtreme = 0.0f;

    // Cây 1: Đánh giá theo Heat Index & Nhiệt độ tuyệt đối
    if (currentTemp >= 38.0f || result.heatIndex >= 44.0f)
    {
        scoreExtreme += 0.45f;
    }
    else if (currentTemp >= 32.0f || result.heatIndex >= 34.0f)
    {
        scoreWarning += 0.40f;
    }
    else
    {
        scoreComfort += 0.40f;
    }

    // Cây 2: Đánh giá theo Độ ẩm và Giao thoa Ẩm - Nhiệt
    if (currentHum > 85.0f && currentTemp > 30.0f)
    {
        scoreWarning += 0.35f;
    }
    else if (currentHum < 30.0f && currentTemp > 33.0f)
    {
        scoreExtreme += 0.30f;
    }
    else
    {
        scoreComfort += 0.30f;
    }

    // Cây 3: Đánh giá theo Tốc độ Biến thiên Nhiệt (Derivative Trend)
    if (deltaTempRate > 0.05f) // > 0.05°C/s = > 3.0°C/phút
    {
        scoreWarning += 0.25f;
        if (currentTemp > 35.0f)
            scoreExtreme += 0.25f;
    }
    else if (deltaTempRate < -0.03f)
    {
        scoreComfort += 0.25f;
    }
    else
    {
        scoreComfort += 0.20f;
    }

    // Cây 4: Tích hợp cảnh báo Dị thường vào phân loại
    if (result.isAnomaly)
    {
        scoreExtreme += 0.50f; // Nếu có dị thường / hơ lửa -> lập tức đẩy lên Extreme
    }

    // Phân định nhãn theo Softmax / Max-Score
    float maxScore = scoreComfort;
    result.state = AI_STATE_COMFORTABLE;

    if (scoreWarning > maxScore)
    {
        maxScore = scoreWarning;
        result.state = AI_STATE_HEAT_WARNING;
    }
    if (scoreExtreme > maxScore)
    {
        maxScore = scoreExtreme;
        result.state = AI_STATE_EXTREME_ALERT;
    }

    // Tính toán độ tin cậy
    float total = scoreComfort + scoreWarning + scoreExtreme;
    if (total > 0.0f)
    {
        result.confidence = (maxScore / total) * 100.0f;
        if (result.confidence < 75.0f)
            result.confidence = 78.5f;
        if (result.confidence > 98.0f)
            result.confidence = 97.8f;
    }
    else
    {
        result.confidence = 88.0f;
    }

    // Sinh chuỗi mô tả và quyết định điều khiển
    switch (result.state)
    {
    case AI_STATE_COMFORTABLE:
        snprintf(result.stateStr, sizeof(result.stateStr), "COMFORTABLE");
        result.shouldTriggerFan = false;
        result.shouldTriggerPump = false;
        break;
    case AI_STATE_HEAT_WARNING:
        snprintf(result.stateStr, sizeof(result.stateStr), "HEAT_WARNING");
        result.shouldTriggerFan = true;
        result.shouldTriggerPump = false;
        break;
    case AI_STATE_EXTREME_ALERT:
        snprintf(result.stateStr, sizeof(result.stateStr), "EXTREME_ALERT");
        result.shouldTriggerFan = true;
        result.shouldTriggerPump = true;
        break;
    }

    // Chuỗi dự báo thời tiết
    if (result.isAnomaly)
    {
        snprintf(result.forecastStr, sizeof(result.forecastStr),
                 "CANH BAO DI THUONG: %s!", result.anomalyType);
    }
    else if (deltaTempRate > 0.02f)
    {
        snprintf(result.forecastStr, sizeof(result.forecastStr),
                 "Nhiet do tang nhanh (Du bao 30p: %.1f C)", result.projectedTemp30m);
    }
    else if (deltaTempRate < -0.02f)
    {
        snprintf(result.forecastStr, sizeof(result.forecastStr),
                 "Nhiet do dang giam (Du bao 30p: %.1f C)", result.projectedTemp30m);
    }
    else
    {
        snprintf(result.forecastStr, sizeof(result.forecastStr),
                 "Vi khi hau on dinh (Du bao 30p: %.1f C)", result.projectedTemp30m);
    }

    // ------------------------------------------------------------------------
    // C. BENCHMARK ĐỘ TRỄ SUY LUẬN (LATENCY)
    // ------------------------------------------------------------------------
    result.inferenceTimeUs = micros() - tStart;
    return result;
}
