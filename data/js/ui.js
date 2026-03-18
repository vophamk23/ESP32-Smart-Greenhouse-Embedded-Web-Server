/* ============================================================
   ui.js — Greenhouse IoT
   updateUI(d) — called every WebSocket message.
   Maps ESP32 JSON fields → DOM elements.
============================================================ */
"use strict";

function updateUI(d) {
  /* ── Sensors ── */
  if (d.temperature != null) {
    setText("val-temp", d.temperature.toFixed(1));
    setBar("bar-temp", d.temperature, 0, 50);
  }
  if (d.humidity != null) {
    setText("val-humi", d.humidity.toFixed(0));
    setBar("bar-humi", d.humidity, 0, 100);
  }
  if (d.soilMoisture != null) {
    setText("val-soil", d.soilMoisture.toFixed(0));
    setBar("bar-soil", d.soilMoisture, 0, 100);
  }
  if (d.lightLux != null) {
    const lux = d.lightLux;
    setText(
      "val-light",
      lux >= 1000 ? (lux / 1000).toFixed(1) + "k" : lux.toFixed(0) + " lx",
    );
  }

  /* ── Pump ── */
  if (d.pump != null) {
    setLedIndicator("dled-pump", d.pump);
    setText("dstate-pump", d.pump ? "ĐANG CHẠY" : "STANDBY");
    updateBadge("badge-pump", d.pump);
    updateBadge("pump-badge-dash", d.pump);
    updatePumpStatusText(d.pump);
    // Toggle nút BẬT/TẮT pump
    const pOn = document.getElementById("pump-btn-on");
    const pOff = document.getElementById("pump-btn-off");
    if (pOn) {
      pOn.className =
        "flex-1 text-white font-bold py-4 rounded-xl transition-all active:scale-95";
      pOn.style.background = d.pump
        ? "#4d9e7a"
        : "linear-gradient(to right,#006c49,#10b981)";
      pOn.style.boxShadow = d.pump
        ? "0 8px 24px rgba(0,108,73,0.4)"
        : "0 4px 16px rgba(0,108,73,0.2)";
    }
    if (pOff)
      pOff.className = d.pump
        ? "flex-1 bg-surface-container-low text-on-surface/50 font-bold py-4 rounded-xl transition-all active:scale-95"
        : "flex-1 bg-surface-container-low text-on-surface font-bold py-4 rounded-xl hover:bg-surface-variant transition-all active:scale-95";
  }

  /* ── Fan ── */
  if (d.fan != null) {
    setLedIndicator("dled-fan", d.fan);
    setText("dstate-fan", d.fan ? "ĐANG CHẠY" : "STANDBY");
    updateBadge("badge-fan", d.fan);
    updateBadge("fan-badge-dash", d.fan);
    // Toggle nút BẬT/TẮT fan
    const fOn = document.getElementById("fan-btn-on");
    const fOff = document.getElementById("fan-btn-off");
    if (fOn) {
      fOn.className =
        "flex-1 text-white font-bold py-4 rounded-xl transition-all active:scale-95";
      fOn.style.background = d.fan ? "#4d90a8" : "#00668a";
      fOn.style.boxShadow = d.fan
        ? "0 8px 24px rgba(0,102,138,0.4)"
        : "0 4px 16px rgba(0,102,138,0.2)";
    }
    if (fOff)
      fOff.className = d.fan
        ? "flex-1 bg-surface-container-low text-on-surface/50 font-bold py-4 rounded-xl transition-all active:scale-95"
        : "flex-1 bg-surface-container-low text-on-surface font-bold py-4 rounded-xl hover:bg-surface-variant transition-all active:scale-95";
    // Fan speed bar: 100% khi bật, 0% khi tắt
    const fanBar = document.getElementById("dled-fan-bar");
    if (fanBar) fanBar.style.width = d.fan ? "60%" : "0%";
  }

  /* ── Mode ── */
  if (d.autoMode != null) {
    State.data.autoMode = d.autoMode;
    const modeEl = document.getElementById("dstate-mode");
    if (modeEl) {
      modeEl.textContent = d.autoMode ? "AUTO" : "MANUAL";
      modeEl.className = d.autoMode
        ? "text-tertiary font-bold"
        : "text-[#7c3aed] font-bold";
    }
    updateModeButtons(d.autoMode);
  }

  /* ── LED color ── */
  if (d.ledColor) {
    // Update tất cả elements có id dled-color và dstate-led (dashboard + control page)
    document.querySelectorAll("#dled-color").forEach((el) => {
      el.style.background = d.ledColor;
      el.title = d.ledColor;
    });
    document.querySelectorAll("#dstate-led").forEach((el) => {
      el.textContent = d.ledColor;
    });
  }

  /* ── WiFi ── */
  if (d.wifiRssi != null) {
    const connected = d.wifiRssi < 0;
    setLedIndicator("dled-wifi", connected);
    const rssiText = connected
      ? d.wifiRssi + " dBm • Secured"
      : "Không kết nối";
    // Update tất cả dstate-wifi (dashboard + control)
    document.querySelectorAll("#dstate-wifi").forEach((el) => {
      el.textContent = rssiText;
    });
    // WiFi signal bars — cả 2 trang
    updateWifiBars(d.wifiRssi);
  }

  /* ── LCD ── */
  if (d.lcdPage != null) {
    const pages = [
      "Nhiệt độ & Độ ẩm",
      "Đất & Ánh sáng",
      "Thiết bị",
      "Thống kê",
    ];
    const lcdText = "Trang " + (d.lcdPage + 1) + "/" + pages.length;
    document.querySelectorAll("#dstate-lcd").forEach((el) => {
      el.textContent = lcdText;
    });
  }

  /* ── Light ring SVG ── */
  if (d.lightLux != null) {
    const ring = document.getElementById("light-ring");
    if (ring) {
      const pct = Math.min(1, d.lightLux / 1500);
      const offset = 339 * (1 - pct);
      ring.style.strokeDashoffset = offset.toFixed(1);
      ring.style.stroke = d.lightLux < 500 ? "#f59e0b" : "#10b981";
    }
    // Light badges
    const isLow = d.lightLux < 500;
    const specEl = document.getElementById("light-badge-spectrum");
    const statEl = document.getElementById("light-badge-status");
    if (specEl) specEl.textContent = isLow ? "Grow Light ON" : "Phổ tự nhiên";
    if (statEl) {
      statEl.textContent = isLow ? "CẦN ÁNH SÁNG" : "Tối ưu";
      statEl.className = isLow
        ? "px-4 py-1.5 rounded-full bg-error/10 text-error text-[10px] font-black uppercase"
        : "px-4 py-1.5 rounded-full bg-surface-container text-on-surface-variant text-[10px] font-black uppercase";
    }
  }

  /* ── Sensor status labels ── */
  if (d.temperature != null) {
    const ok = !d.alertTemp && !d.dhtError;
    setText(
      "stat-temp-status",
      d.dhtError ? "SENSOR LỖI" : d.alertTemp ? "CẢNH BÁO" : "BÌNH THƯỜNG",
    );
    const el = document.getElementById("stat-temp-status");
    if (el)
      el.className =
        d.dhtError || d.alertTemp ? "text-error font-black" : "text-primary";
  }
  if (d.humidity != null) {
    setText("stat-humi-status", d.alertHumidity ? "BẤT THƯỜNG" : "ỔN ĐỊNH");
    const el = document.getElementById("stat-humi-status");
    if (el)
      el.className = d.alertHumidity
        ? "text-error font-black"
        : "text-secondary";
  }
  if (d.soilMoisture != null) {
    const crit = d.soilMoisture < 30;
    setText(
      "stat-soil-status",
      crit ? "CRITICAL" : d.soilMoisture > 70 ? "QUÁ ẨM" : "MỤC TIÊU",
    );
    const el = document.getElementById("stat-soil-status");
    if (el) el.className = crit ? "text-error font-black" : "text-tertiary";
  }

  /* ── Stats ── */
  if (d.pumpCount != null) setText("stat-count", d.pumpCount);
  if (d.totalPumpTimeSec != null) {
    setText("stat-time", fmtSec(d.totalPumpTimeSec));
    setText("pump-count-ctrl", d.pumpCount || 0);
  }
  if (d.freeHeap != null) setText("stat-heap", fmtBytes(d.freeHeap));

  /* ── Uptime from freeHeap proxy (not in WS) ── */
  // Show millis/1000 if available; otherwise skip

  /* ── Alert chips ── */
  if (d.alertTemp != null)
    setAlertChip(
      "al-temp",
      d.alertTemp || d.dhtError,
      d.dhtError
        ? "🌡 DHT Lỗi!"
        : d.alertTemp
          ? "🌡 NHIỆT ĐỘ CAO!"
          : "🌡 Nhiệt độ OK",
    );
  if (d.alertHumidity != null)
    setAlertChip(
      "al-humi",
      d.alertHumidity,
      d.alertHumidity ? "💧 ĐỘ ẨM BẤT THƯỜNG!" : "💧 Độ ẩm OK",
    );
  if (d.alertSoil != null)
    setAlertChip(
      "al-soil",
      d.alertSoil,
      d.alertSoil ? "🪴 ĐẤT KHÔ!" : "🪴 Đất OK",
    );
  if (d.alertLight != null)
    setAlertChip(
      "al-light",
      d.alertLight,
      d.alertLight ? "☀️ THIẾU SÁNG!" : "☀️ Ánh sáng OK",
    );
  if (d.dhtError != null)
    setAlertChip(
      "al-dht",
      d.dhtError,
      d.dhtError ? "📡 DHT LỖI!" : "📡 DHT OK",
    );

  /* ── Fan temp display in control page ── */
  if (d.temperature != null)
    setText("fan-temp-display", d.temperature.toFixed(1) + "°C");

  /* ── Sensor card warning state ── */
  highlightCard("sc-temp", d.alertTemp);
  highlightCard("sc-humi", d.alertHumidity);
  highlightCard("sc-soil", d.alertSoil);
  highlightCard("sc-light", d.alertLight);
}

/* ── Helpers ── */
function setText(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function setBar(id, val, min, max) {
  const el = document.getElementById(id);
  if (!el) return;
  const pct = Math.min(100, Math.max(0, ((val - min) / (max - min)) * 100));
  el.style.width = pct + "%";
}

function updateBadge(id, on) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = on ? "ON" : "OFF";
  if (on) {
    el.className =
      "bg-primary/20 text-primary px-3 py-1 rounded-full text-xs font-bold tracking-wider";
  } else {
    el.className =
      "bg-surface-container-high text-on-surface-variant px-3 py-1 rounded-full text-xs font-bold tracking-wider";
  }
}

function updatePumpStatusText(on) {
  const el = document.getElementById("pump-status-text");
  if (!el) return;
  el.innerHTML = on
    ? '<div class="w-2.5 h-2.5 rounded-full bg-primary animate-pulse"></div><span class="text-lg font-bold text-primary">Đang chạy</span>'
    : '<div class="w-2.5 h-2.5 rounded-full bg-slate-300"></div><span class="text-lg font-bold">Tắt</span>';
}

function highlightCard(id, warn) {
  const el = document.getElementById(id);
  if (!el) return;
  if (warn) el.classList.add("ring-2", "ring-error/40");
  else el.classList.remove("ring-2", "ring-error/40");
}

/* ── WiFi signal bars ── */
function updateWifiBars(rssi) {
  const bars =
    rssi === 0 ? 0 : rssi > -60 ? 4 : rssi > -70 ? 3 : rssi > -80 ? 2 : 1;
  document.querySelectorAll("#dled-wifi-bars").forEach((el) => {
    el.querySelectorAll("div").forEach((d, i) => {
      d.style.background = i < bars ? "var(--c-primary)" : "#e0e3e5";
    });
  });
}

function updateModeButtons(isAuto) {
  const autoBtn = document.getElementById("modeAutoBtn");
  const manBtn = document.getElementById("modeManBtn");
  if (!autoBtn || !manBtn) return;
  if (isAuto) {
    autoBtn.className =
      "px-8 py-2.5 rounded-full text-sm font-bold bg-primary text-white shadow-lg shadow-primary/20";
    manBtn.className =
      "px-8 py-2.5 rounded-full text-sm font-bold text-on-surface-variant hover:text-primary transition-all";
  } else {
    autoBtn.className =
      "px-8 py-2.5 rounded-full text-sm font-bold text-on-surface-variant hover:text-primary transition-all";
    manBtn.className =
      "px-8 py-2.5 rounded-full text-sm font-bold bg-primary text-white shadow-lg shadow-primary/20";
  }
  const banner = document.getElementById("modeBanner");
  const bannerText = document.getElementById("modeBannerText");
  if (banner && bannerText) {
    if (isAuto) {
      banner.className =
        "mb-6 px-4 py-3 bg-primary/5 border border-primary/20 rounded-2xl";
      bannerText.textContent =
        "Chế độ TỰ ĐỘNG: Hệ thống tự điều khiển dựa trên cảm biến.";
    } else {
      banner.className =
        "mb-6 px-4 py-3 bg-[#7c3aed]/5 border border-[#7c3aed]/20 rounded-2xl";
      bannerText.textContent =
        "⚠ Chế độ THỦ CÔNG: Bạn có toàn quyền kiểm soát. Hệ thống tự động tạm dừng.";
    }
  }
}
