/* ============================================================
   smartlogic.js — Greenhouse IoT
   Smart Logic: tích hợp OpenWeather API
   Nếu xác suất mưa > ngưỡng → bỏ qua chu kỳ tưới
============================================================ */
"use strict";

function toggleSmart() {
  const enabled = document.getElementById("smartEnable")?.checked;
  State.smartEnabled = !!enabled;
  const form = document.getElementById("smartForm");
  if (form) {
    if (State.smartEnabled) form.classList.remove("smart-form-disabled");
    else form.classList.add("smart-form-disabled");
  }
  showToast("Smart Logic: " + (State.smartEnabled ? "BẬT" : "TẮT"));
}

function saveSmartLogic() {
  const key = document.getElementById("weatherApiKey")?.value.trim();
  const city = document.getElementById("weatherCity")?.value.trim();
  const rain = parseInt(document.getElementById("rainThresh")?.value || 60);
  if (State.smartEnabled && !key) {
    showToast("Vui lòng nhập API key OpenWeather.", true);
    return;
  }
  State.smartConfig = {
    apiKey: key,
    city: city || "Ho Chi Minh City",
    rainThresh: rain,
  };
  sendWS({
    cmd: "setSmartLogic",
    value: { enabled: State.smartEnabled, ...State.smartConfig },
  });
  addSmartLog("✓ Đã lưu cấu hình Smart Logic.");
  showToast("✓ Smart Logic đã áp dụng.");
}

function fetchWeather() {
  const key = document.getElementById("weatherApiKey")?.value.trim();
  const city =
    document.getElementById("weatherCity")?.value.trim() || "Ho Chi Minh City";
  if (!key) {
    showToast("Nhập API key OpenWeather trước.", true);
    return;
  }
  addSmartLog("🔍 Đang tải dữ liệu thời tiết cho " + city + "...");

  const url = `https://api.openweathermap.org/data/2.5/forecast?q=${encodeURIComponent(city)}&appid=${key}&units=metric&lang=vi`;
  fetch(url)
    .then((r) => {
      if (!r.ok) throw new Error("HTTP " + r.status);
      return r.json();
    })
    .then((data) => {
      renderWeatherPreview(data);
      addSmartLog("✓ Đã tải dữ liệu thời tiết thành công.");
    })
    .catch((e) => {
      addSmartLog("✗ Lỗi: " + e.message);
      showToast("Lỗi tải thời tiết: " + e.message, true);
    });
}

function renderWeatherPreview(data) {
  const el = document.getElementById("weatherPreview");
  if (!el || !data.list) return;

  const next24 = data.list.slice(0, 8); // 8 × 3h = 24h
  const maxRain = Math.max(...next24.map((f) => (f.pop || 0) * 100));
  const avgTemp = next24.reduce((s, f) => s + f.main.temp, 0) / next24.length;

  const rainThresh = parseInt(
    document.getElementById("rainThresh")?.value || 60,
  );
  const willSkip = maxRain >= rainThresh;

  el.innerHTML = `
    <div class="grid grid-cols-2 gap-4 mb-4">
      <div class="bg-white rounded-2xl p-4 shadow-sm">
        <p class="text-xs font-bold text-on-surface-variant uppercase mb-1">Xác suất mưa (24h)</p>
        <p class="font-serif text-2xl ${maxRain >= rainThresh ? "text-error" : "text-primary"}">${maxRain.toFixed(0)}%</p>
      </div>
      <div class="bg-white rounded-2xl p-4 shadow-sm">
        <p class="text-xs font-bold text-on-surface-variant uppercase mb-1">Nhiệt độ TB</p>
        <p class="font-serif text-2xl text-on-surface">${avgTemp.toFixed(1)}°C</p>
      </div>
    </div>
    <div class="p-3 rounded-xl ${willSkip ? "bg-error/10 text-error border border-error/20" : "bg-primary/5 text-primary border border-primary/20"} text-sm font-bold">
      ${willSkip ? "⚠ Dự báo mưa vượt ngưỡng → Bỏ qua chu kỳ tưới tiếp theo." : "✓ Không có mưa → Thực hiện tưới theo lịch."}
    </div>
    <p class="text-xs text-on-surface-variant/50 mt-2">📍 ${data.city?.name || "?"} — Cập nhật: ${new Date().toLocaleTimeString("vi-VN")}</p>
  `;

  if (willSkip)
    addSmartLog(
      "⚠ Mưa " +
        maxRain.toFixed(0) +
        "% ≥ ngưỡng " +
        rainThresh +
        "% → Chu kỳ tưới sẽ bị bỏ qua.",
    );
  else
    addSmartLog(
      "✓ Mưa " +
        maxRain.toFixed(0) +
        "% < ngưỡng " +
        rainThresh +
        "% → Tưới bình thường.",
    );
}

function addSmartLog(msg) {
  const el = document.getElementById("smartLogList");
  if (!el) return;
  const time = new Date().toLocaleTimeString("vi-VN", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
  const div = document.createElement("div");
  div.className =
    "flex items-center gap-3 py-2 border-b border-outline-variant/10";
  div.innerHTML = `<span class="text-[10px] font-mono text-on-surface-variant/40 flex-shrink-0">${time}</span><span class="w-2 h-2 rounded-full bg-primary flex-shrink-0"></span><p class="text-sm">${msg}</p>`;
  el.prepend(div);
  // Keep only last 10 entries
  while (el.children.length > 10) el.removeChild(el.lastChild);
}
