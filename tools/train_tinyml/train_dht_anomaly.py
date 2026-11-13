"""
# Cleanroom Bio-Chamber Edge-AI — TinyML Model Training & C Header Export Pipeline
#
# train_dht_anomaly.py - Huấn luyện mô hình MLP Phát hiện Dị thường Vi khí hậu (DHT20/RS485)
# và xuất trọng số nhúng thành C++ header (dht_anomaly_model.h) cho ESP32-S3.
"""

import numpy as np
import tensorflow as tf
import os
from sklearn.model_selection import train_test_split
from sklearn.metrics import confusion_matrix, classification_report

print(f"TensorFlow Version: {tf.__version__}")
print(f"NumPy Version: {np.__version__}")

# ============================================================================
# 1. TẠO TẬP DỮ LIỆU HUẤN LUYỆN (DATASET GENERATION)
# ============================================================================
np.random.seed(42)
N_SAMPLES = 10000

# Dữ liệu Bình thường (NORMAL: 55%) - Vi khí hậu phòng sạch Bio-Chamber lý tưởng
n_norm = int(N_SAMPLES * 0.55)
temp_norm = np.random.uniform(20.0, 34.0, n_norm) + np.random.normal(0, 0.4, n_norm)
hum_norm = np.random.uniform(45.0, 80.0, n_norm) + np.random.normal(0, 0.8, n_norm)
labels_norm = np.zeros(n_norm)

# Dữ liệu Bất thường (ANOMALY: 45%)
n_anom = N_SAMPLES - n_norm
n_part = n_anom // 4

# Kịch bản A: Hơ lửa phá hoại / Nắng nóng cực đoan
temp_hot = np.random.uniform(36.0, 55.0, n_part)
hum_hot = np.random.uniform(10.0, 65.0, n_part)

# Kịch bản B: Lạnh buốt / Sương giá
temp_cold = np.random.uniform(0.0, 15.5, n_part)
hum_cold = np.random.uniform(40.0, 95.0, n_part)

# Kịch bản C: Mất ẩm buồng sạch / Khô giá thể nghiêm trọng
temp_dry = np.random.uniform(22.0, 36.0, n_part)
hum_dry = np.random.uniform(5.0, 34.0, n_part)

# Kịch bản D: Ẩm ướt cực đoan / Đọng sương
temp_wet = np.random.uniform(24.0, 35.0, n_anom - 3 * n_part)
hum_wet = np.random.uniform(88.0, 99.0, n_anom - 3 * n_part)

temp_anom = np.concatenate([temp_hot, temp_cold, temp_dry, temp_wet])
hum_anom = np.concatenate([hum_hot, hum_cold, hum_dry, hum_wet])
labels_anom = np.ones(n_anom)

# Gộp và xáo trộn
X = np.column_stack([
    np.concatenate([temp_norm, temp_anom]),
    np.concatenate([hum_norm, hum_anom])
])
y = np.concatenate([labels_norm, labels_anom])

shuffle_idx = np.random.permutation(len(y))
X, y = X[shuffle_idx], y[shuffle_idx]

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.20, random_state=42)
print(f"Tổng mẫu: {len(X)} | Train: {len(X_train)} | Test: {len(X_test)}")

# ============================================================================
# 2. XÂY DỰNG MẠNG NƠ-RON (KERAS MLP MODEL)
# ============================================================================
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(2,), name='input_1'),
    tf.keras.layers.Dense(8, activation='relu', name='dense'),
    tf.keras.layers.Dense(1, activation='sigmoid', name='dense_1')
])

model.compile(
    optimizer=tf.keras.optimizers.Adam(learning_rate=0.005),
    loss='binary_crossentropy',
    metrics=['accuracy']
)

model.summary()

# ============================================================================
# 3. HUẤN LUYỆN MÔ HÌNH (TRAINING)
# ============================================================================
history = model.fit(
    X_train, y_train,
    epochs=40,
    batch_size=32,
    validation_split=0.2,
    verbose=1
)

# ============================================================================
# 4. ĐÁNH GIÁ ĐỘ CHÍNH XÁC (EVALUATION)
# ============================================================================
test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
print(f"\nĐộ chính xác trên tập Test: {test_acc * 100:.2f}%")

y_probs = model.predict(X_test)
y_pred = (y_probs >= 0.60).astype(int).flatten()

cm = confusion_matrix(y_test, y_pred)
print("\n--- CONFUSION MATRIX ---")
print(cm)
print("\n--- CLASSIFICATION REPORT ---")
print(classification_report(y_test, y_pred, target_names=['NORMAL', 'ANOMALY']))

# ============================================================================
# 5. CHUYỂN ĐỔI SANG TENSORFLOW LITE (.TFLITE)
# ============================================================================
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()

tflite_filename = "dht_anomaly_model.tflite"
with open(tflite_filename, "wb") as f:
    f.write(tflite_model)
print(f"\nKích thước file TFLite: {len(tflite_model)} bytes (~{len(tflite_model)/1024:.2f} KB)")

# ============================================================================
# 6. XUẤT RA FILE HEADER C++ (dht_anomaly_model.h) CHO ESP32-S3
# ============================================================================
header_filename = "dht_anomaly_model.h"
c_str = f"/*\n * {header_filename} - TinyML DHT20 Anomaly Detection Model\n"
c_str += f" * Tự động sinh từ pipeline TensorFlow/Keras (Edge AI Bio-Chamber)\n"
c_str += f" * Kích thước: {len(tflite_model)} bytes\n */\n\n"
c_str += "#ifndef DHT_ANOMALY_MODEL_H\n#define DHT_ANOMALY_MODEL_H\n\n"
c_str += "#include <Arduino.h>\n\n"
c_str += "alignas(16) const unsigned char dht_anomaly_model_tflite[] = {\n  "

for i, b in enumerate(tflite_model):
    c_str += f"0x{b:02x}, "
    if (i + 1) % 12 == 0:
        c_str += "\n  "

c_str = c_str.rstrip(", \n") + "\n};\n\n"
c_str += f"const unsigned int dht_anomaly_model_tflite_len = {len(tflite_model)};\n\n"
c_str += "#endif // DHT_ANOMALY_MODEL_H\n"

with open(header_filename, "w", encoding="utf-8") as f:
    f.write(c_str)

print(f"Đã xuất thành công file: {header_filename} ({len(tflite_model)} bytes)!")

