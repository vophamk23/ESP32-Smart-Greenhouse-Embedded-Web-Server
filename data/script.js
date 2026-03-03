/* ═══════════════════════════════════════════════
   GREENHOUSE IoT — script.js
   14 Use Cases: Monitoring, Control, Management, System
═══════════════════════════════════════════════ */

"use strict";

// ══════════════════════════════════════════════
// STATE
// ══════════════════════════════════════════════
const State = {
  user: null,
  data: {},
  ws: null,
  autoMode: true,
  schedules: [],
  devices: [],
  pumpLogs: [],
  schedActive: false,
  smartActive: false,
  histData: { day: null, week: null, month: null },
  charts: {},
  wsRetry: 0,
  logPage: 1,
  logFilter: "",
  thresholds: { dry: 30, wet: 70, pumpMax: 60, pumpCool: 120, tempHigh: 35 },
  notifySettings: {},
  smartSettings: { apiKey: "", city: "Ho Chi Minh City", rainThresh: 60 },
};

const USERS = { admin: "admin123", greenhouse: "iot2024" };

// ══════════════════════════════════════════════
// UC9: AUTH
// ══════════════════════════════════════════════
function doLogin() {
  const u = document.getElementById("loginUser").value.trim();
  const p = document.getElementById("loginPass").value;
  if (USERS[u] && USERS[u] === p) {
    State.user = u;
    document.getElementById("loginOverlay").style.display = "none";
    document.getElementById("app").style.display = "flex";
    document.getElementById("avatarName").textContent = u[0].toUpperCase();
    document.getElementById("accountName").textContent =
      u === "admin" ? "Greenhouse Admin" : u;
    document.getElementById("accountAvatar").textContent = u[0].toUpperCase();
    initApp();
  } else {
    document.getElementById("loginError").textContent =
      "Sai tài khoản hoặc mật khẩu";
    document.getElementById("loginPass").value = "";
  }
}

document.getElementById("loginPass").addEventListener("keydown", (e) => {
  if (e.key === "Enter") doLogin();
});

function doLogout() {
  showConfirm("Đăng xuất", "Bạn có chắc muốn đăng xuất?", () => {
    if (State.ws) State.ws.close();
    document.getElementById("app").style.display = "none";
    document.getElementById("loginOverlay").style.display = "flex";
    document.getElementById("loginUser").value = "";
    document.getElementById("loginPass").value = "";
    State.user = null;
  });
}

function saveAccount() {
  const oldPass = document.getElementById("accPassOld").value;
  const newPass = document.getElementById("accPassNew").value;
  const cfmPass = document.getElementById("accPassCfm").value;
  if (oldPass || newPass || cfmPass) {
    if (!USERS[State.user] || USERS[State.user] !== oldPass) {
      showToast("Mật khẩu hiện tại không đúng", true);
      return;
    }
    if (newPass !== cfmPass) {
      showToast("Mật khẩu mới không khớp", true);
      return;
    }
    if (newPass.length < 6) {
      showToast("Mật khẩu tối thiểu 6 ký tự", true);
      return;
    }
    USERS[State.user] = newPass;
  }
  showToast("✓ Đã lưu thông tin tài khoản");
}

// ══════════════════════════════════════════════
// INIT
// ══════════════════════════════════════════════
function initApp() {
  startClock();
  loadFromStorage();
  initDevices();
  renderLogs();
  generateHistoryData();
  updateThresholdDisplay();
  renderScheduleList();
  renderThresholdPreview();
  renderDeviceGrid();
  connectWS();
  navigate("dashboard");
}

// ══════════════════════════════════════════════
// WEBSOCKET (UC13: Nhận dữ liệu real-time)
// ══════════════════════════════════════════════
// function connectWS() {
//   const dot = document.getElementById("wsDot");
//   const label = document.getElementById("wsLabel");

//   dot.className = "ws-dot";
//   label.textContent = "Đang kết nối...";

//   // In production: ws = new WebSocket('ws://' + location.host + '/ws');
//   // For demo: simulate with mock data
//   simulateMockData();
//   dot.className = "ws-dot connected";
//   label.textContent = "Đã kết nối (Demo)";
// }

function connectWS() {
  const dot = document.getElementById("wsDot");
  const label = document.getElementById("wsLabel");

  dot.className = "ws-dot";
  label.textContent = "Đang kết nối...";

  const wsUrl = "ws://" + location.host + "/ws";
  State.ws = new WebSocket(wsUrl);

  State.ws.onopen = () => {
    dot.className = "ws-dot connected";
    label.textContent = "Đã kết nối"; // bỏ chữ Demo
    State.wsRetry = 0;
    console.log("[WS] Connected:", wsUrl);

    fetch("/api/status")
      .then((r) => r.json())
      .then((d) => {
        updateUI(d);
        State.data = { ...State.data, ...d };
      })
      .catch((e) => console.warn("[API]", e));
  };

  State.ws.onmessage = (event) => {
    try {
      const d = JSON.parse(event.data);
      State.autoMode = d.autoMode;
      updateUI(d);
      State.data = { ...State.data, ...d };

      if (d.pump && !State.data._pumpWasOn) {
        addPumpLog("AUTO — Đất " + (d.soilMoisture || 0).toFixed(0) + "%");
        State.data._pumpWasOn = true;
      }
      if (!d.pump && State.data._pumpWasOn) {
        closePumpLog("OK");
        State.data._pumpWasOn = false;
      }
    } catch (e) {
      console.warn("[WS] Parse error:", e);
    }
  };

  State.ws.onclose = () => {
    dot.className = "ws-dot error";
    label.textContent = "Mất kết nối - thử lại...";
    setTimeout(connectWS, 3000);
  };

  State.ws.onerror = () => {
    dot.className = "ws-dot error";
    label.textContent = "Lỗi kết nối";
  };
}

// // ─── Mock data simulation ───
// function simulateMockData() {
//   function gen() {
//     const d = {
//       temperature: 22 + Math.random() * 16,
//       humidity: 40 + Math.random() * 45,
//       soilMoisture: 15 + Math.random() * 70,
//       lightLux: Math.random() > 0.4 ? 1200 : 80,
//       pump: State.data.pump || false,
//       fan: State.data.fan || false,
//       autoMode: State.autoMode,
//       alertTemp: false,
//       alertHumidity: false,
//       alertSoil: false,
//       alertLight: false,
//       dhtError: false,
//       pumpCount: State.pumpLogs.length,
//       totalPumpTimeSec: State.pumpLogs.reduce(
//         (a, l) => a + (l.duration || 0),
//         0,
//       ),
//       ledColor: "#00FF00",
//       wifiRssi: -40 + Math.floor(Math.random() * -30),
//       freeHeap: 220000 + Math.floor(Math.random() * 50000),
//       lcdPage: Math.floor(Math.random() * 4),
//     };
//     // Compute alerts
//     d.alertTemp = d.temperature > State.thresholds.tempHigh;
//     d.alertHumidity = d.humidity < 30 || d.humidity > 90;
//     d.alertSoil = d.soilMoisture < State.thresholds.dry;
//     d.alertLight = d.lightLux < 200;
//     // LED color logic
//     if (d.temperature > 45) d.ledColor = "#FF0000";
//     else if (d.pump) d.ledColor = "#0064FF";
//     else if (d.alertTemp || d.alertSoil) d.ledColor = "#FFC800";
//     else if (d.lightLux < 200) d.ledColor = "#FFFFFF";
//     else d.ledColor = "#00FF00";

//     // UC12: Auto pump logic
//     if (State.autoMode && !d.pump && d.soilMoisture < State.thresholds.dry) {
//       d.pump = true;
//       State.data.pump = true;
//       addPumpLog("AUTO — Đất khô (" + d.soilMoisture.toFixed(0) + "%)");
//     }
//     if (State.autoMode && d.pump && d.soilMoisture >= State.thresholds.wet) {
//       d.pump = false;
//       State.data.pump = false;
//       closePumpLog("OK");
//     }

//     updateUI(d);
//     State.data = { ...State.data, ...d };
//   }
//   gen();
//   setInterval(gen, 3000);
// }

// ══════════════════════════════════════════════
// UC1 + UC2: UPDATE UI
// ══════════════════════════════════════════════
function updateUI(d) {
  // UC1: Sensors
  const tempOk = !d.dhtError;
  setText("val-temp", tempOk ? d.temperature.toFixed(1) : "ERR");
  setText("val-humi", tempOk ? d.humidity.toFixed(0) : "--");
  setText("val-soil", d.soilMoisture.toFixed(0));

  const bright = d.lightLux > 500;
  setText("val-light", bright ? "SÁNG" : "TỐI");
  setText("unit-light", bright ? "☀️" : "🌙");

  setStyle(
    "bar-temp",
    "width",
    Math.min((d.temperature / 50) * 100, 100) + "%",
  );
  setStyle("bar-humi", "width", d.humidity + "%");
  setStyle("bar-soil", "width", d.soilMoisture + "%");

  toggleClass("sc-temp", "alert", d.alertTemp);
  toggleClass("sc-humi", "alert", d.alertHumidity);
  toggleClass("sc-soil", "alert", d.alertSoil);
  toggleClass("sc-light", "alert", d.alertLight);

  // UC2: Device status
  const pumpOn = d.pump,
    fanOn = d.fan;
  setText("dstate-pump", pumpOn ? "● RUNNING" : "○ STANDBY");
  setText("dstate-fan", fanOn ? "● RUNNING" : "○ STANDBY");
  setLedState("dled-pump", pumpOn);
  setLedState("dled-fan", fanOn);

  toggleClass("dstat-pump", "active", pumpOn);
  toggleClass("dstat-fan", "active", fanOn);

  const rssi = d.wifiRssi || -60;
  const wifiStr =
    rssi > -50
      ? "Xuất sắc"
      : rssi > -65
        ? "Tốt"
        : rssi > -75
          ? "Yếu"
          : "Rất yếu";
  setText("dstate-wifi", wifiStr + " (" + rssi + "dBm)");
  setLedState("dled-wifi", rssi > -75);

  setText("dstate-lcd", "Trang " + ((d.lcdPage || 0) + 1) + "/4");
  setText("dstate-mode", d.autoMode ? "AUTO" : "MANUAL");
  setText("dstate-led", d.ledColor === "#000000" ? "Tắt" : "Đang sáng");

  // LED preview
  const ledEl = document.getElementById("dled-color");
  if (ledEl) {
    ledEl.style.background = d.ledColor || "#000";
    ledEl.style.boxShadow =
      d.ledColor && d.ledColor !== "#000000"
        ? "0 0 12px " + d.ledColor + "80"
        : "none";
  }

  // UC14: Alerts
  setAlert("al-temp", d.alertTemp, "🌡 Nhiệt độ CAO!", "🌡 Nhiệt độ OK");
  setAlert("al-humi", d.alertHumidity, "💧 Độ ẩm bất thường", "💧 Độ ẩm OK");
  setAlert("al-soil", d.alertSoil, "🪴 Đất KHÔ!", "🪴 Đất OK");
  setAlert("al-light", d.alertLight, "☀️ Thiếu sáng!", "☀️ Ánh sáng OK");
  setAlert("al-dht", d.dhtError, "📡 Lỗi DHT!", "📡 DHT OK");
  setAlert("al-pump-wd", false, "⏱ Watchdog!", "⏱ Bơm an toàn");

  // Stats
  setText("stat-count", d.pumpCount || 0);
  const sec = d.totalPumpTimeSec || 0;
  setText(
    "stat-time",
    sec >= 60 ? Math.floor(sec / 60) + "m" + (sec % 60) + "s" : sec + "s",
  );
  setText("stat-heap", d.freeHeap ? d.freeHeap.toLocaleString() : "--");

  const now = new Date();
  setText(
    "stat-uptime",
    now.getHours().toString().padStart(2, "0") +
      ":" +
      now.getMinutes().toString().padStart(2, "0") +
      ":" +
      now.getSeconds().toString().padStart(2, "0"),
  );

  // Control page badges
  updateControlPage(d);
}

function updateControlPage(d) {
  // Mode
  State.autoMode = d.autoMode;
  const banner = document.getElementById("modeBanner");
  const bannerText = document.getElementById("modeBannerText");
  if (banner) {
    banner.className = "mode-banner" + (d.autoMode ? "" : " manual");
    bannerText.textContent = d.autoMode
      ? "⚙️ Chế độ: AUTO — Chuyển sang MANUAL để điều khiển thủ công"
      : "✋ Chế độ: MANUAL — Bạn đang điều khiển trực tiếp";
  }
  const mAutoBtn = document.getElementById("modeAutoBtn");
  const mManBtn = document.getElementById("modeManBtn");
  if (mAutoBtn)
    mAutoBtn.className = "mode-btn" + (d.autoMode ? " active-auto" : "");
  if (mManBtn)
    mManBtn.className = "mode-btn" + (!d.autoMode ? " active-manual" : "");

  // Pump badge
  const bPump = document.getElementById("badge-pump");
  if (bPump) {
    bPump.textContent = d.pump ? "ON" : "OFF";
    bPump.className = "ctrl-badge" + (d.pump ? " on" : "");
  }
  const bFan = document.getElementById("badge-fan");
  if (bFan) {
    bFan.textContent = d.fan ? "ON" : "OFF";
    bFan.className = "ctrl-badge" + (d.fan ? " on" : "");
  }

  const cPump = document.getElementById("ctrl-pump");
  const cFan = document.getElementById("ctrl-fan");
  if (cPump) cPump.className = "ctrl-card" + (d.pump ? " active-pump" : "");
  if (cFan) cFan.className = "ctrl-card" + (d.fan ? " active-fan" : "");
}

// ══════════════════════════════════════════════
// UC5: MANUAL CONTROL
// ══════════════════════════════════════════════
function ctrlDevice(device, action) {
  if (State.autoMode) {
    showToast("⚠ Chuyển sang MANUAL trước", true);
    return;
  }
  const on = action === "on";
  State.data[device] = on;

  if (device === "pump") {
    if (on) {
      addPumpLog("MANUAL — Người dùng bật");
      showToast("✓ Bật máy bơm");
    } else {
      closePumpLog("OK");
      showToast("✓ Tắt máy bơm");
    }
  } else {
    showToast("✓ " + (on ? "Bật" : "Tắt") + " quạt");
  }

  // Send to ESP32 via WS
  sendWS({
    cmd: "set" + device.charAt(0).toUpperCase() + device.slice(1),
    value: on,
  });
  updateControlPage(State.data);
  renderLogs();
}

function setMode(mode) {
  State.autoMode = mode === "AUTO";
  State.data.autoMode = State.autoMode;
  sendWS({ cmd: "setMode", value: mode });
  updateControlPage(State.data);
  showToast("✓ Chế độ: " + mode);
}

// ══════════════════════════════════════════════
// PUMP LOG HELPERS (UC4)
// ══════════════════════════════════════════════
let activePumpLog = null;

function addPumpLog(reason) {
  activePumpLog = {
    id: State.pumpLogs.length + 1,
    start: new Date(),
    end: null,
    duration: null,
    reason,
    status: "running",
  };
  State.pumpLogs.unshift(activePumpLog);
}

function closePumpLog(status) {
  if (!activePumpLog) return;
  activePumpLog.end = new Date();
  activePumpLog.duration = Math.round(
    (activePumpLog.end - activePumpLog.start) / 1000,
  );
  activePumpLog.status = status;
  activePumpLog = null;
  renderLogs();
  saveToStorage();
}

// ══════════════════════════════════════════════
// UC4: LOGS
// ══════════════════════════════════════════════
function renderLogs() {
  const tbody = document.getElementById("logTbody");
  if (!tbody) return;

  const filtered = State.pumpLogs.filter(
    (l) =>
      !State.logFilter ||
      l.reason.toLowerCase().includes(State.logFilter.toLowerCase()) ||
      fmtDate(l.start).includes(State.logFilter),
  );

  const perPage = 10;
  const total = Math.ceil(filtered.length / perPage) || 1;
  if (State.logPage > total) State.logPage = total;
  const slice = filtered.slice(
    (State.logPage - 1) * perPage,
    State.logPage * perPage,
  );

  if (slice.length === 0) {
    tbody.innerHTML =
      '<tr><td colspan="6" style="text-align:center;color:var(--muted);padding:32px">Chưa có nhật ký bơm</td></tr>';
  } else {
    tbody.innerHTML = slice
      .map((l) => {
        const reasonClass = l.reason.startsWith("AUTO")
          ? "log-reason-auto"
          : l.reason.startsWith("MANUAL")
            ? "log-reason-manual"
            : l.reason.startsWith("WD")
              ? "log-reason-wd"
              : l.reason.startsWith("SCHED")
                ? "log-reason-sched"
                : "log-reason-smart";
        const statusHtml =
          l.status === "running"
            ? '<span class="log-status-ok">Đang chạy</span>'
            : l.status === "OK"
              ? '<span class="log-status-ok">Hoàn thành</span>'
              : '<span class="log-status-err">Lỗi/Watchdog</span>';
        return `<tr>
        <td>${l.id}</td>
        <td>${fmtDate(l.start)}</td>
        <td>${l.end ? fmtDate(l.end) : "—"}</td>
        <td>${l.duration != null ? l.duration + "s" : "..."}</td>
        <td class="${reasonClass}">${l.reason}</td>
        <td>${statusHtml}</td>
      </tr>`;
      })
      .join("");
  }

  // Pagination
  const pgEl = document.getElementById("logPagination");
  if (pgEl) {
    let html = "";
    for (let i = 1; i <= total; i++) {
      html += `<button class="pg-btn${i === State.logPage ? " active" : ""}" onclick="goLogPage(${i})">${i}</button>`;
    }
    pgEl.innerHTML = html;
  }
}

function filterLogs(v) {
  State.logFilter = v;
  State.logPage = 1;
  renderLogs();
}
function goLogPage(n) {
  State.logPage = n;
  renderLogs();
}

function clearLogs() {
  showConfirm("Xóa nhật ký", "Xóa toàn bộ nhật ký hoạt động?", () => {
    State.pumpLogs = [];
    activePumpLog = null;
    renderLogs();
    showToast("✓ Đã xóa nhật ký");
    saveToStorage();
  });
}

function exportLogs() {
  const csv = ["#,Bắt đầu,Kết thúc,Thời lượng,Lý do,Trạng thái"]
    .concat(
      State.pumpLogs.map(
        (l) =>
          `${l.id},${fmtDate(l.start)},${l.end ? fmtDate(l.end) : ""},${l.duration || ""}s,${l.reason},${l.status}`,
      ),
    )
    .join("\n");
  const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "pump_logs_" + new Date().toISOString().slice(0, 10) + ".csv";
  a.click();
  showToast("✓ Xuất CSV thành công");
}

// ══════════════════════════════════════════════
// UC3: HISTORY CHARTS
// ══════════════════════════════════════════════
function generateHistoryData() {
  function genSeries(count, base, range) {
    return Array.from(
      { length: count },
      () => +(base + (Math.random() - 0.5) * range).toFixed(1),
    );
  }
  function genLabels(count, unit) {
    const labels = [];
    const now = new Date();
    for (let i = count - 1; i >= 0; i--) {
      const d = new Date(now - i * unit);
      labels.push(
        unit < 86400000
          ? d.getHours() + ":00"
          : d.getDate() + "/" + (d.getMonth() + 1),
      );
    }
    return labels;
  }
  State.histData.day = {
    labels: genLabels(24, 3600000),
    temp: genSeries(24, 28, 8),
    humi: genSeries(24, 65, 20),
    soil: genSeries(24, 50, 30),
    pump: Array.from({ length: 24 }, () =>
      Math.random() > 0.8 ? Math.floor(Math.random() * 3) : 0,
    ),
  };
  State.histData.week = {
    labels: genLabels(7, 86400000),
    temp: genSeries(7, 28, 8),
    humi: genSeries(7, 65, 20),
    soil: genSeries(7, 50, 30),
    pump: Array.from({ length: 7 }, () => Math.floor(Math.random() * 5)),
  };
  State.histData.month = {
    labels: genLabels(30, 86400000),
    temp: genSeries(30, 28, 8),
    humi: genSeries(30, 65, 20),
    soil: genSeries(30, 50, 30),
    pump: Array.from({ length: 30 }, () => Math.floor(Math.random() * 4)),
  };
}

function initCharts() {
  if (State.charts.history) return;

  const histCtx = document.getElementById("historyChart");
  const pumpCtx = document.getElementById("pumpChart");
  if (!histCtx || !pumpCtx) return;

  const d = State.histData.day;
  const chartDefaults = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { display: false },
      tooltip: {
        backgroundColor: "#131b13",
        titleColor: "#5eead4",
        bodyColor: "#94a394",
        borderColor: "#1e2d1e",
        borderWidth: 1,
      },
    },
    scales: {
      x: {
        grid: { color: "rgba(94,234,212,.05)" },
        ticks: {
          color: "#4a5e4a",
          font: { family: "JetBrains Mono", size: 11 },
        },
      },
      y: {
        grid: { color: "rgba(94,234,212,.05)" },
        ticks: {
          color: "#4a5e4a",
          font: { family: "JetBrains Mono", size: 11 },
        },
      },
    },
  };

  State.charts.history = new Chart(histCtx, {
    type: "line",
    data: {
      labels: d.labels,
      datasets: [
        {
          label: "Nhiệt độ",
          data: d.temp,
          borderColor: "#4ade80",
          backgroundColor: "rgba(74,222,128,.08)",
          tension: 0.4,
          pointRadius: 2,
        },
        {
          label: "Độ ẩm KK",
          data: d.humi,
          borderColor: "#60a5fa",
          backgroundColor: "rgba(96,165,250,.08)",
          tension: 0.4,
          pointRadius: 2,
        },
        {
          label: "Đất",
          data: d.soil,
          borderColor: "#a78bfa",
          backgroundColor: "rgba(167,139,250,.08)",
          tension: 0.4,
          pointRadius: 2,
        },
      ],
    },
    options: {
      ...chartDefaults,
      plugins: {
        ...chartDefaults.plugins,
        title: {
          display: true,
          text: "Cảm biến theo thời gian",
          color: "#e8f5e8",
          font: { family: "Syne", size: 13, weight: 700 },
        },
      },
    },
  });

  State.charts.pump = new Chart(pumpCtx, {
    type: "bar",
    data: {
      labels: d.labels,
      datasets: [
        {
          label: "Lần bơm",
          data: d.pump,
          backgroundColor: "rgba(45,212,191,.3)",
          borderColor: "#2dd4bf",
          borderWidth: 1,
          borderRadius: 4,
        },
      ],
    },
    options: {
      ...chartDefaults,
      plugins: {
        ...chartDefaults.plugins,
        title: {
          display: true,
          text: "Lần bơm theo giờ",
          color: "#e8f5e8",
          font: { family: "Syne", size: 13, weight: 700 },
        },
      },
    },
  });
}

function setChartRange(range, btn) {
  document
    .querySelectorAll(".ctab")
    .forEach((b) => b.classList.remove("active"));
  btn.classList.add("active");

  const d = State.histData[range];
  if (!State.charts.history || !d) return;

  const ch = State.charts.history;
  ch.data.labels = d.labels;
  ch.data.datasets[0].data = d.temp;
  ch.data.datasets[1].data = d.humi;
  ch.data.datasets[2].data = d.soil;
  ch.update();

  const cp = State.charts.pump;
  cp.data.labels = d.labels;
  cp.data.datasets[0].data = d.pump;
  cp.update();
}

// ══════════════════════════════════════════════
// UC6: THRESHOLDS
// ══════════════════════════════════════════════
function updateThresholdDisplay() {
  const ids = ["threshDry", "threshWet", "pumpMax", "pumpCool", "tempHigh"];
  const units = ["%", "%", "s", "s", "°C"];
  ids.forEach((id, i) => {
    const el = document.getElementById(id);
    const valEl = document.getElementById(id + "Val");
    if (el && valEl) valEl.textContent = el.value + units[i];
  });
  renderThresholdPreview();
}

function renderThresholdPreview() {
  const el = document.getElementById("threshPreview");
  if (!el) return;
  const dry =
    document.getElementById("threshDry")?.value || State.thresholds.dry;
  const wet =
    document.getElementById("threshWet")?.value || State.thresholds.wet;
  const pmax =
    document.getElementById("pumpMax")?.value || State.thresholds.pumpMax;
  const cool =
    document.getElementById("pumpCool")?.value || State.thresholds.pumpCool;
  const th =
    document.getElementById("tempHigh")?.value || State.thresholds.tempHigh;
  el.innerHTML = `Đất &lt; <strong style="color:var(--red)">${dry}%</strong> → BẬT bơm &nbsp;|&nbsp; Đất ≥ <strong style="color:var(--green2)">${wet}%</strong> → TẮT bơm &nbsp;|&nbsp; Bơm max <strong style="color:var(--yellow)">${pmax}s</strong> &nbsp;|&nbsp; Chờ <strong style="color:var(--teal)">${cool}s</strong> &nbsp;|&nbsp; Quạt bật khi T ≥ <strong style="color:var(--orange)">${th}°C</strong>`;
}

function saveThreshold() {
  State.thresholds = {
    dry: +document.getElementById("threshDry").value,
    wet: +document.getElementById("threshWet").value,
    pumpMax: +document.getElementById("pumpMax").value,
    pumpCool: +document.getElementById("pumpCool").value,
    tempHigh: +document.getElementById("tempHigh").value,
  };
  if (State.thresholds.dry >= State.thresholds.wet) {
    showToast("⚠ Ngưỡng khô phải nhỏ hơn ngưỡng ướt", true);
    return;
  }
  sendWS({ cmd: "setThresholds", value: State.thresholds });
  saveToStorage();
  showToast("✓ Đã lưu ngưỡng tưới");
}

function resetThreshold() {
  document.getElementById("threshDry").value = 30;
  document.getElementById("threshWet").value = 70;
  document.getElementById("pumpMax").value = 60;
  document.getElementById("pumpCool").value = 120;
  document.getElementById("tempHigh").value = 35;
  updateThresholdDisplay();
  showToast("✓ Đặt lại mặc định");
}

// ══════════════════════════════════════════════
// UC7: SCHEDULE
// ══════════════════════════════════════════════
function toggleSchedule() {
  State.schedActive = document.getElementById("schedEnable").checked;
  sendWS({ cmd: "setScheduleEnabled", value: State.schedActive });
  showToast(State.schedActive ? "✓ Bật lịch tưới" : "✓ Tắt lịch tưới");
}

function renderScheduleList() {
  if (State.schedules.length === 0) {
    State.schedules = [
      { id: 1, time: "05:00", duration: 30, days: "daily", enabled: true },
      { id: 2, time: "17:30", duration: 20, days: "daily", enabled: true },
    ];
  }
  const el = document.getElementById("scheduleList");
  if (!el) return;
  el.innerHTML = State.schedules
    .map(
      (s) => `
    <div class="schedule-item">
      <span class="sched-label">🕐 Giờ tưới</span>
      <input type="time" value="${s.time}" onchange="updateSched(${s.id},'time',this.value)" />
      <span class="sched-label">⏱ Thời lượng (giây)</span>
      <input type="number" min="5" max="300" value="${s.duration}" style="width:80px" onchange="updateSched(${s.id},'duration',this.value)" />
      <select onchange="updateSched(${s.id},'days',this.value)">
        <option value="daily"   ${s.days === "daily" ? "selected" : ""}>Hằng ngày</option>
        <option value="weekday" ${s.days === "weekday" ? "selected" : ""}>Ngày thường</option>
        <option value="weekend" ${s.days === "weekend" ? "selected" : ""}>Cuối tuần</option>
      </select>
      <label class="toggle" style="margin-left:auto">
        <input type="checkbox" ${s.enabled ? "checked" : ""} onchange="updateSched(${s.id},'enabled',this.checked)" />
        <div class="toggle-track"></div>
        <div class="toggle-thumb"></div>
      </label>
      <button class="btn-del-sched" onclick="deleteSched(${s.id})">✕</button>
    </div>
  `,
    )
    .join("");
}

function addSchedule() {
  const id = Date.now();
  State.schedules.push({
    id,
    time: "08:00",
    duration: 30,
    days: "daily",
    enabled: true,
  });
  renderScheduleList();
}

function updateSched(id, key, val) {
  const s = State.schedules.find((s) => s.id === id);
  if (s)
    s[key] = key === "duration" ? +val : key === "enabled" ? Boolean(val) : val;
}

function deleteSched(id) {
  State.schedules = State.schedules.filter((s) => s.id !== id);
  renderScheduleList();
}

function saveSchedule() {
  sendWS({ cmd: "setSchedules", value: State.schedules });
  saveToStorage();
  showToast("✓ Đã lưu lịch tưới");
}

// UC12: Check schedules
setInterval(() => {
  if (!State.schedActive) return;
  const now = new Date();
  const hm =
    now.getHours().toString().padStart(2, "0") +
    ":" +
    now.getMinutes().toString().padStart(2, "0");
  State.schedules.forEach((s) => {
    if (s.enabled && s.time === hm && !State.data.pump) {
      State.data.pump = true;
      addPumpLog("SCHED — Lịch " + s.time);
      setTimeout(() => {
        State.data.pump = false;
        closePumpLog("OK");
      }, s.duration * 1000);
    }
  });
}, 60000);

// ══════════════════════════════════════════════
// UC8: SMART LOGIC
// ══════════════════════════════════════════════
function toggleSmart() {
  State.smartActive = document.getElementById("smartEnable").checked;
  const form = document.getElementById("smartForm");
  if (form)
    form.className = "smart-form" + (State.smartActive ? " enabled" : "");
  showToast(State.smartActive ? "✓ Smart Logic bật" : "✓ Smart Logic tắt");
}

function saveSmartLogic() {
  State.smartSettings = {
    apiKey: document.getElementById("weatherApiKey")?.value || "",
    city: document.getElementById("weatherCity")?.value || "",
    rainThresh: +document.getElementById("rainThresh")?.value || 60,
  };
  saveToStorage();
  showToast("✓ Đã lưu Smart Logic");
}

function fetchWeather() {
  const container = document.getElementById("weatherPreview");
  if (!container) return;
  container.innerHTML =
    '<div style="color:var(--teal);font-family:JetBrains Mono,monospace;font-size:13px">Đang lấy dữ liệu thời tiết...</div>';
  setTimeout(() => {
    const mock = {
      city: "Ho Chi Minh City",
      temp: 31,
      humi: 78,
      rain: 15,
      desc: "Nắng nhẹ",
      icon: "🌤",
    };
    container.innerHTML = `
      <div style="font-size:13px;color:var(--text2);margin-bottom:10px">📍 ${mock.city}</div>
      <div class="weather-info">
        <div class="w-item"><div class="w-val">${mock.temp}°C</div><div class="w-desc">Nhiệt độ</div></div>
        <div class="w-item"><div class="w-val">${mock.humi}%</div><div class="w-desc">Độ ẩm KK</div></div>
        <div class="w-item"><div class="w-val">${mock.rain}%</div><div class="w-desc">Xác suất mưa</div></div>
      </div>
      <div style="margin-top:12px;font-size:13px">
        ${mock.icon} ${mock.desc} &nbsp;→&nbsp;
        <span style="color:${mock.rain < 60 ? "var(--green2)" : "var(--yellow)"}">
          ${mock.rain < 60 ? "✓ Nên tưới hôm nay" : "⚠ Có thể mưa, không cần tưới"}
        </span>
      </div>`;
    showToast("✓ Đã lấy dữ liệu thời tiết");
  }, 1200);
}

// ══════════════════════════════════════════════
// UC10: NOTIFICATIONS
// ══════════════════════════════════════════════
function switchNotifyTab(tab, btn) {
  document
    .querySelectorAll(".ntab")
    .forEach((b) => b.classList.remove("active"));
  document
    .querySelectorAll(".ntab-content")
    .forEach((c) => c.classList.remove("active"));
  btn.classList.add("active");
  document.getElementById("ntab-" + tab).classList.add("active");
}

function testTelegram() {
  const token = document.getElementById("tgToken")?.value;
  if (!token) {
    showToast("⚠ Nhập Bot Token trước", true);
    return;
  }
  showToast("✓ Gửi tin nhắn test thành công (Demo)");
}

function testEmail() {
  const email = document.getElementById("emailTo")?.value;
  if (!email) {
    showToast("⚠ Nhập email trước", true);
    return;
  }
  showToast("✓ Gửi email test thành công (Demo)");
}

function saveNotify() {
  State.notifySettings = {
    tgToken: document.getElementById("tgToken")?.value,
    tgChatId: document.getElementById("tgChatId")?.value,
    smtpHost: document.getElementById("smtpHost")?.value,
    emailTo: document.getElementById("emailTo")?.value,
    events: {
      dry: document.getElementById("ne-dry")?.checked,
      pumpLong: document.getElementById("ne-pump-long")?.checked,
      dht: document.getElementById("ne-dht")?.checked,
      temp: document.getElementById("ne-temp")?.checked,
      offline: document.getElementById("ne-offline")?.checked,
    },
  };
  saveToStorage();
  showToast("✓ Đã lưu cài đặt thông báo");
}

// ══════════════════════════════════════════════
// UC11: DEVICES (Factory Method)
// ══════════════════════════════════════════════
function initDevices() {
  if (State.devices.length > 0) return;
  State.devices = [
    {
      id: 1,
      type: "sensor_temp",
      name: "DHT22 #1",
      gpio: 4,
      status: "online",
      desc: "Cảm biến nhiệt độ/độ ẩm",
      factory: "TempSensorFactory",
    },
    {
      id: 2,
      type: "sensor_soil",
      name: "Capacitive Soil",
      gpio: 35,
      status: "online",
      desc: "Cảm biến độ ẩm đất",
      factory: "SoilSensorFactory",
    },
    {
      id: 3,
      type: "sensor_light",
      name: "LDR Module",
      gpio: 34,
      status: "online",
      desc: "Cảm biến ánh sáng DO",
      factory: "LightSensorFactory",
    },
    {
      id: 4,
      type: "actuator_pump",
      name: "Relay Pump",
      gpio: 26,
      status: "online",
      desc: "Máy bơm 12V DC",
      factory: "PumpActuatorFactory",
    },
    {
      id: 5,
      type: "actuator_fan",
      name: "Relay Fan",
      gpio: 27,
      status: "online",
      desc: "Quạt làm mát",
      factory: "FanActuatorFactory",
    },
    {
      id: 6,
      type: "actuator_led",
      name: "NeoPixel WS2812",
      gpio: 25,
      status: "online",
      desc: "LED RGB x4",
      factory: "LEDActuatorFactory",
    },
    {
      id: 7,
      type: "display_lcd",
      name: "LCD 16x2 I2C",
      gpio: 21,
      status: "online",
      desc: "Màn hình LCD, addr 0x27",
      factory: "LCDDisplayFactory",
    },
  ];
}

const DEVICE_TYPE_MAP = {
  sensor_temp: { icon: "🌡", label: "Cảm biến nhiệt độ" },
  sensor_humi: { icon: "💧", label: "Cảm biến độ ẩm" },
  sensor_soil: { icon: "🪴", label: "Cảm biến đất" },
  sensor_light: { icon: "☀️", label: "Cảm biến ánh sáng" },
  actuator_pump: { icon: "💦", label: "Máy bơm" },
  actuator_fan: { icon: "🌀", label: "Quạt" },
  actuator_led: { icon: "🔆", label: "LED" },
  display_lcd: { icon: "🖥", label: "Màn hình LCD" },
};

function renderDeviceGrid() {
  const el = document.getElementById("deviceGrid");
  if (!el) return;
  el.innerHTML = State.devices
    .map((d) => {
      const t = DEVICE_TYPE_MAP[d.type] || { icon: "⚙️", label: d.type };
      return `
      <div class="device-card ${d.status}">
        <div class="dc-header">
          <span class="dc-type-ico">${t.icon}</span>
          <div><div class="dc-name">${d.name}</div><div class="dc-type">${t.label}</div></div>
        </div>
        <div class="dc-status">
          <div class="dc-dot ${d.status}"></div>
          <span>${d.status === "online" ? "Online" : "Offline"}</span>
        </div>
        <div class="dc-meta">GPIO: ${d.gpio} &nbsp;|&nbsp; Factory: ${d.factory}</div>
        <div class="dc-meta" style="margin-top:4px">${d.desc}</div>
        <div class="dc-actions">
          <button class="btn-dc-del" onclick="deleteDevice(${d.id})">🗑 Xóa</button>
        </div>
      </div>`;
    })
    .join("");
}

function openAddDevice() {
  document.getElementById("addDeviceModal").style.display = "flex";
  document.getElementById("deviceType").value = "";
  document.getElementById("deviceFormFields").innerHTML = "";
}

function closeAddDevice() {
  document.getElementById("addDeviceModal").style.display = "none";
}

function onDeviceTypeChange() {
  const type = document.getElementById("deviceType").value;
  const container = document.getElementById("deviceFormFields");
  if (!type) {
    container.innerHTML = "";
    return;
  }
  const t = DEVICE_TYPE_MAP[type];
  container.innerHTML = `
    <div class="form-group"><label>Tên thiết bị</label><input type="text" id="newDevName" placeholder="${t.label} #${State.devices.length + 1}" /></div>
    <div class="form-group"><label>GPIO Pin</label><input type="number" id="newDevGpio" placeholder="4" min="0" max="39" /></div>
    <div class="form-group"><label>Mô tả</label><input type="text" id="newDevDesc" placeholder="Mô tả thiết bị..." /></div>
  `;
}

function registerDevice() {
  const type = document.getElementById("deviceType").value;
  if (!type) {
    showToast("⚠ Chọn loại thiết bị", true);
    return;
  }
  const name =
    document.getElementById("newDevName")?.value ||
    DEVICE_TYPE_MAP[type]?.label;
  const gpio = +document.getElementById("newDevGpio")?.value || 0;
  const desc = document.getElementById("newDevDesc")?.value || "";
  const factory =
    type
      .split("_")
      .map((w) => w[0].toUpperCase() + w.slice(1))
      .join("") + "Factory";
  State.devices.push({
    id: Date.now(),
    type,
    name,
    gpio,
    desc,
    status: "online",
    factory,
  });
  renderDeviceGrid();
  closeAddDevice();
  saveToStorage();
  showToast("✓ Đã đăng ký thiết bị: " + name);
}

function deleteDevice(id) {
  showConfirm("Xóa thiết bị", "Bạn có chắc muốn xóa thiết bị này?", () => {
    State.devices = State.devices.filter((d) => d.id !== id);
    renderDeviceGrid();
    saveToStorage();
    showToast("✓ Đã xóa thiết bị");
  });
}

// ══════════════════════════════════════════════
// NAVIGATION
// ══════════════════════════════════════════════
const PAGE_TITLES = {
  dashboard: "Dashboard",
  charts: "Biểu đồ lịch sử",
  logs: "Nhật ký hoạt động",
  control: "Điều khiển thiết bị",
  threshold: "Ngưỡng tưới",
  schedule: "Lịch tưới",
  smartlogic: "Smart Logic",
  devices: "Quản lý thiết bị",
  notify: "Cài đặt thông báo",
  account: "Tài khoản",
};

function navigate(page) {
  document
    .querySelectorAll(".page")
    .forEach((p) => p.classList.remove("active"));
  document
    .querySelectorAll(".nav-item")
    .forEach((n) => n.classList.remove("active"));

  const pageEl = document.getElementById("page-" + page);
  if (pageEl) pageEl.classList.add("active");

  const navEl = document.querySelector('[data-page="' + page + '"]');
  if (navEl) navEl.classList.add("active");

  document.getElementById("pageTitle").textContent = PAGE_TITLES[page] || page;

  // Close sidebar on mobile
  if (window.innerWidth <= 700) {
    document.getElementById("sidebar").classList.remove("open");
  }

  // Init charts on first visit
  if (page === "charts") setTimeout(initCharts, 100);

  // Refresh logs
  if (page === "logs") renderLogs();
}

function toggleSidebar() {
  document.getElementById("sidebar").classList.toggle("open");
}

// ══════════════════════════════════════════════
// HELPERS
// ══════════════════════════════════════════════
function setText(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function setStyle(id, prop, val) {
  const el = document.getElementById(id);
  if (el) el.style[prop] = val;
}

function toggleClass(id, cls, on) {
  const el = document.getElementById(id);
  if (el) el.classList.toggle(cls, on);
}

function setLedState(id, on) {
  const el = document.getElementById(id);
  if (el) {
    el.className = "dstat-led " + (on ? "on" : "off");
  }
}

function setAlert(id, isAlert, warn, ok) {
  const el = document.getElementById(id);
  if (!el) return;
  el.className = "alert-chip " + (isAlert ? "warn" : "ok");
  el.textContent = isAlert ? warn : ok;
}

function fmtDate(d) {
  if (!d) return "";
  const dt = d instanceof Date ? d : new Date(d);
  return (
    dt.toLocaleDateString("vi-VN") +
    " " +
    dt.getHours().toString().padStart(2, "0") +
    ":" +
    dt.getMinutes().toString().padStart(2, "0") +
    ":" +
    dt.getSeconds().toString().padStart(2, "0")
  );
}

// function sendWS(msg) {
//   if (State.ws && State.ws.readyState === WebSocket.OPEN) {
//     State.ws.send(JSON.stringify(msg));
//   }
//   // In demo mode: just log
//   console.log("[WS Send]", msg);
// }

function sendWS(msg) {
  if (State.ws && State.ws.readyState === WebSocket.OPEN) {
    State.ws.send(JSON.stringify(msg));
    console.log("[WS Send]", msg);
  } else {
    console.warn("[WS] Not connected - cannot send:", msg);
    showToast("⚠ Chưa kết nối ESP32", true);
  }
}
// Toast
let toastTimer = null;
function showToast(msg, isError = false) {
  const el = document.getElementById("toast");
  el.textContent = msg;
  el.className = "toast show" + (isError ? " error" : "");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => (el.className = "toast"), 2800);
}

// Confirm Modal
let confirmCallback = null;
function showConfirm(title, msg, cb) {
  document.getElementById("confirmTitle").textContent = title;
  document.getElementById("confirmMsg").textContent = msg;
  document.getElementById("confirmModal").style.display = "flex";
  confirmCallback = cb;
}
function closeConfirm() {
  document.getElementById("confirmModal").style.display = "none";
  confirmCallback = null;
}
document.getElementById("confirmOk").onclick = () => {
  closeConfirm();
  if (confirmCallback) confirmCallback();
};

// Clock
function startClock() {
  function tick() {
    const now = new Date();
    const el = document.getElementById("topbarTime");
    if (el)
      el.textContent =
        now.getHours().toString().padStart(2, "0") +
        ":" +
        now.getMinutes().toString().padStart(2, "0") +
        ":" +
        now.getSeconds().toString().padStart(2, "0") +
        "  " +
        now.toLocaleDateString("vi-VN");
  }
  tick();
  setInterval(tick, 1000);
}

// LocalStorage
function saveToStorage() {
  try {
    localStorage.setItem("gh_schedules", JSON.stringify(State.schedules));
    localStorage.setItem("gh_devices", JSON.stringify(State.devices));
    localStorage.setItem("gh_thresholds", JSON.stringify(State.thresholds));
    localStorage.setItem(
      "gh_logs",
      JSON.stringify(State.pumpLogs.slice(0, 200)),
    );
    localStorage.setItem("gh_notify", JSON.stringify(State.notifySettings));
    localStorage.setItem("gh_smart", JSON.stringify(State.smartSettings));
  } catch (e) {
    console.warn("Storage error", e);
  }
}

function loadFromStorage() {
  try {
    const s = localStorage.getItem("gh_schedules");
    if (s) State.schedules = JSON.parse(s);
    const d = localStorage.getItem("gh_devices");
    if (d) State.devices = JSON.parse(d);
    const t = localStorage.getItem("gh_thresholds");
    if (t) State.thresholds = { ...State.thresholds, ...JSON.parse(t) };
    const l = localStorage.getItem("gh_logs");
    if (l)
      State.pumpLogs = JSON.parse(l).map((x) => ({
        ...x,
        start: new Date(x.start),
        end: x.end ? new Date(x.end) : null,
      }));
    const n = localStorage.getItem("gh_notify");
    if (n) State.notifySettings = JSON.parse(n);
    const sm = localStorage.getItem("gh_smart");
    if (sm) State.smartSettings = JSON.parse(sm);
  } catch (e) {
    console.warn("Load storage error", e);
  }
}

// Auto-save every 30s
setInterval(saveToStorage, 30000);

// Close sidebar on outside click (mobile)
document.addEventListener("click", (e) => {
  const sb = document.getElementById("sidebar");
  const btn = document.querySelector(".menu-btn");
  if (sb && !sb.contains(e.target) && btn && !btn.contains(e.target)) {
    sb.classList.remove("open");
  }
});
