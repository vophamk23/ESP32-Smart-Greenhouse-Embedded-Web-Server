/* ============================================================
   threshold.js — Greenhouse IoT
   Ngưỡng tưới — UI đồng bộ với State.threshold
   Khi Save → gửi qua WebSocket để ESP32 cập nhật
============================================================ */
"use strict";

function updateThresholdDisplay() {
  const dry = parseInt(document.getElementById("threshDry")?.value || 30);
  const wet = parseInt(document.getElementById("threshWet")?.value || 70);
  const pMax = parseInt(document.getElementById("pumpMax")?.value || 60);
  const pCool = parseInt(document.getElementById("pumpCool")?.value || 120);
  const tHigh = parseInt(document.getElementById("tempHigh")?.value || 32);

  setText("threshDryVal", dry);
  setText("threshWetVal", wet);
  setText("pumpMaxVal", pMax);
  setText("pumpCoolVal", pCool);
  setText("tempHighVal", tHigh);

  // Visualizer preview labels
  setText("prev-dry", dry);
  setText("prev-wet", wet);

  // Preview text
  const prev = document.getElementById("threshPreview");
  if (prev) {
    prev.innerHTML = `
      <span class="text-primary font-bold">Khô &lt; ${dry}%</span> → Bật bơm &nbsp;|&nbsp;
      <span class="text-primary font-bold">Ướt &gt; ${wet}%</span> → Tắt bơm &nbsp;|&nbsp;
      Max bơm: <strong>${pMax}s</strong> &nbsp;|&nbsp;
      Nghỉ: <strong>${pCool}s</strong> &nbsp;|&nbsp;
      Quạt bật khi T ≥ <strong>${tHigh}°C</strong>
    `;
  }
}

// ============================================================
// Sync slider từ cfg_* do ESP32 gửi về qua WebSocket
// Gọi từ ws.onmessage handler khi nhận data
// ============================================================
function syncThresholdFromESP32(data) {
  let changed = false;

  if (data.cfg_soilDry !== undefined) {
    document.getElementById("threshDry").value = data.cfg_soilDry;
    changed = true;
  }
  if (data.cfg_soilWet !== undefined) {
    document.getElementById("threshWet").value = data.cfg_soilWet;
    changed = true;
  }
  if (data.cfg_tempHigh !== undefined) {
    document.getElementById("tempHigh").value = data.cfg_tempHigh;
    changed = true;
  }
  if (data.cfg_pumpMax !== undefined) {
    document.getElementById("pumpMax").value = data.cfg_pumpMax;
    changed = true;
  }
  if (data.cfg_pumpCool !== undefined) {
    document.getElementById("pumpCool").value = data.cfg_pumpCool;
    changed = true;
  }

  if (changed) updateThresholdDisplay();
}

function saveThreshold() {
  const dry = parseInt(document.getElementById("threshDry")?.value);
  const wet = parseInt(document.getElementById("threshWet")?.value);
  const pMax = parseInt(document.getElementById("pumpMax")?.value);
  const pCool = parseInt(document.getElementById("pumpCool")?.value);
  const tHigh = parseInt(document.getElementById("tempHigh")?.value);

  if (dry >= wet) {
    showToast("Ngưỡng khô phải nhỏ hơn ngưỡng ướt!", true);
    return;
  }

  State.threshold = {
    soilDry: dry,
    soilWet: wet,
    pumpMax: pMax,
    pumpCool: pCool,
    tempHigh: tHigh,
  };

  // ✅ Gửi flat object — ESP32 đọc trực tiếp các field, không wrap trong "value"
  sendWS({
    cmd: "setThreshold",
    soilDry: dry,
    soilWet: wet,
    tempHigh: tHigh,
    pumpMax: pMax,
    pumpCool: pCool,
  });

  showToast("✓ Đã lưu ngưỡng tưới.");
}

function resetThreshold() {
  const defaults = {
    soilDry: 30,
    soilWet: 70,
    pumpMax: 60,
    pumpCool: 120,
    tempHigh: 32,
  };

  document.getElementById("threshDry").value = defaults.soilDry;
  document.getElementById("threshWet").value = defaults.soilWet;
  document.getElementById("pumpMax").value = defaults.pumpMax;
  document.getElementById("pumpCool").value = defaults.pumpCool;
  document.getElementById("tempHigh").value = defaults.tempHigh;

  State.threshold = { ...defaults };
  updateThresholdDisplay();
  showToast("Đã đặt lại mặc định.");
}

// Init display sau khi partials đã được inject vào DOM
document.addEventListener("partials-loaded", updateThresholdDisplay);
