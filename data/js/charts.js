/* ============================================================
   charts.js — Greenhouse IoT
   Chart.js: sensor history + pump activity bar chart
============================================================ */
"use strict";

let _histChart = null;
let _pumpChart = null;
let _chartsReady = false;

function initCharts() {
  if (_chartsReady) return;
  _chartsReady = true;

  const hCtx = document.getElementById("historyChart");
  const pCtx = document.getElementById("pumpChart");
  if (!hCtx || !pCtx) return;

  _histChart = new Chart(hCtx, {
    type: "line",
    data: {
      labels: State.history.labels,
      datasets: [
        {
          label: "Nhiệt độ (°C)",
          data: State.history.temp,
          borderColor: "#006c49",
          backgroundColor: "rgba(0,108,73,0.08)",
          borderWidth: 2,
          tension: 0.4,
          fill: true,
          pointRadius: 0,
        },
        {
          label: "Độ ẩm KK (%)",
          data: State.history.humi,
          borderColor: "#00668a",
          backgroundColor: "rgba(0,102,138,0.06)",
          borderWidth: 2,
          tension: 0.4,
          fill: true,
          pointRadius: 0,
          borderDash: [5, 3],
        },
        {
          label: "Độ ẩm đất (%)",
          data: State.history.soil,
          borderColor: "#674bb5",
          backgroundColor: "rgba(103,75,181,0.06)",
          borderWidth: 2,
          tension: 0.4,
          fill: true,
          pointRadius: 0,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      plugins: {
        legend: {
          position: "top",
          labels: {
            font: { family: "Plus Jakarta Sans", size: 11 },
            boxWidth: 12,
          },
        },
      },
      scales: {
        x: {
          grid: { color: "rgba(0,0,0,0.04)" },
          ticks: { maxTicksLimit: 10, font: { size: 10 } },
        },
        y: {
          grid: { color: "rgba(0,0,0,0.04)" },
          ticks: { font: { size: 10 } },
          beginAtZero: false,
        },
      },
    },
  });

  _pumpChart = new Chart(pCtx, {
    type: "bar",
    data: {
      labels: State.history.labels,
      datasets: [
        {
          label: "Bơm",
          data: State.history.pump,
          backgroundColor: "rgba(255,255,255,0.5)",
          borderColor: "rgba(255,255,255,0.8)",
          borderWidth: 1,
          borderRadius: 3,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { display: false },
        y: { display: false, min: 0, max: 1 },
      },
    },
  });
}

/* ── Push new data point from WebSocket ── */
function pushChartPoint(d) {
  const h = State.history;
  const now = new Date().toLocaleTimeString("vi-VN", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });

  h.labels.push(now);
  h.temp.push(d.temperature != null ? d.temperature : null);
  h.humi.push(d.humidity != null ? d.humidity : null);
  h.soil.push(d.soilMoisture != null ? d.soilMoisture : null);
  h.light.push(d.lightLux != null ? d.lightLux : null);
  h.pump.push(d.pump ? 1 : 0);

  // Keep buffer limited
  if (h.labels.length > State.historyMax) {
    h.labels.shift();
    h.temp.shift();
    h.humi.shift();
    h.soil.shift();
    h.light.shift();
    h.pump.shift();
  }

  if (_histChart) _histChart.update("none");
  if (_pumpChart) _pumpChart.update("none");

  // Cập nhật tổng thời gian bơm
  const volEl = document.getElementById("pump-total-vol");
  if (volEl && State.data.totalPumpTimeSec != null) {
    volEl.textContent = fmtSec(State.data.totalPumpTimeSec);
  }

  // Cập nhật bảng nhật ký sự kiện mỗi 2 phút
  if (!window._lastLogTime || Date.now() - window._lastLogTime >= 120000) {
    window._lastLogTime = Date.now();
    addChartLogRow(d);
  }

  // Cập nhật tổng thời gian bơm cho charts page
  const el = document.getElementById("pump-total-vol");
  if (el && State.data.totalPumpTimeSec != null) {
    el.textContent = fmtSec(State.data.totalPumpTimeSec);
  }
}

/* ── Range filter button ── */
function setChartRange(range, btn) {
  document.querySelectorAll(".chart-tab-btn").forEach((b) => {
    b.className =
      "chart-tab-btn px-5 py-2 text-sm font-bold rounded-full text-on-surface-variant hover:text-primary";
  });
  btn.className =
    "chart-tab-btn px-5 py-2 text-sm font-bold rounded-full bg-primary text-white";

  // Filter visible data by range
  const limits = { "30m": 180, day: 1440, week: 10080, month: 43200 };
  const limit = limits[range] || 180;
  if (!_histChart) return;

  // Slice from end
  const slice = (arr) => arr.slice(-Math.min(arr.length, limit));
  _histChart.data.labels = slice(State.history.labels);
  _histChart.data.datasets[0].data = slice(State.history.temp);
  _histChart.data.datasets[1].data = slice(State.history.humi);
  _histChart.data.datasets[2].data = slice(State.history.soil);
  _histChart.update();
  if (_pumpChart) {
    _pumpChart.data.labels = slice(State.history.labels);
    _pumpChart.data.datasets[0].data = slice(State.history.pump);
    _pumpChart.update();
  }
}

/* ── Event log store + pagination ── */
const _chartLog = []; // tất cả events
let _chartLogPage = 1;
const _chartLogPerPage = 10;

function addChartLogRow(d) {
  const now = new Date().toLocaleTimeString("vi-VN", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });

  const khuVuc = {
    "TEMP-001": "Nhà kính A-1",
    "HUM-002": "Nhà kính A-1",
    "SOIL-003": "Khu vực trồng",
    "LIGHT-004": "Mái che",
  };

  const newRows = [];
  if (d.temperature != null) {
    const alert = d.alertTemp || d.dhtError;
    newRows.push({
      time: now,
      sensor: "TEMP-001",
      chi: "Nhiệt độ",
      val: d.temperature.toFixed(1) + "°C",
      color: alert ? "text-error" : "text-primary",
      badge: alert ? "bg-error/10 text-error" : "bg-primary/10 text-primary",
      status: d.dhtError ? "LỖI CẢM BIẾN" : alert ? "CẢNH BÁO" : "BÌNH THƯỜNG",
    });
  }
  if (d.humidity != null) {
    const alert = d.alertHumidity;
    newRows.push({
      time: now,
      sensor: "HUM-002",
      chi: "Độ ẩm KK",
      val: d.humidity.toFixed(0) + "%",
      color: alert ? "text-tertiary" : "text-secondary",
      badge: alert
        ? "bg-tertiary/10 text-tertiary"
        : "bg-secondary/10 text-secondary",
      status: alert ? "BẤT THƯỜNG" : "ỔN ĐỊNH",
    });
  }
  if (d.soilMoisture != null) {
    const alert = d.alertSoil || d.soilMoisture < 30;
    newRows.push({
      time: now,
      sensor: "SOIL-003",
      chi: "Độ ẩm đất",
      val: d.soilMoisture.toFixed(0) + "%",
      color: alert ? "text-error" : "text-tertiary",
      badge: alert ? "bg-error/10 text-error" : "bg-primary/10 text-primary",
      status: alert ? "CRITICAL" : "MỤC TIÊU",
    });
  }
  if (d.lightLux != null) {
    const alert = d.alertLight || d.lightLux < 500;
    newRows.push({
      time: now,
      sensor: "LIGHT-004",
      chi: "Ánh sáng",
      val:
        d.lightLux >= 1000
          ? (d.lightLux / 1000).toFixed(1) + "k lux"
          : d.lightLux.toFixed(0) + " lux",
      color: alert ? "text-amber-600" : "text-primary",
      badge: alert
        ? "bg-amber-50 text-amber-600"
        : "bg-primary/10 text-primary",
      status: alert ? "THIẾU SÁNG" : "TỐI ƯU",
    });
  }

  if (newRows.length === 0) return;

  // Prepend vào store, giữ tối đa 200
  newRows.forEach((r) => {
    r.khuVuc = khuVuc[r.sensor] || "--";
    _chartLog.unshift(r);
  });
  if (_chartLog.length > 200) _chartLog.splice(200);

  // Luôn về trang 1 khi có data mới
  _chartLogPage = 1;
  renderChartLogTable();
}

function renderChartLogTable() {
  const tbody = document.getElementById("chart-log-tbody");
  const countEl = document.getElementById("chart-log-count");
  const prevBtn = document.getElementById("chart-log-prev");
  const nextBtn = document.getElementById("chart-log-next");
  const pageEl = document.getElementById("chart-log-page");
  if (!tbody) return;

  const total = _chartLog.length;
  const totalPages = Math.max(1, Math.ceil(total / _chartLogPerPage));
  if (_chartLogPage > totalPages) _chartLogPage = totalPages;

  const start = (_chartLogPage - 1) * _chartLogPerPage;
  const slice = _chartLog.slice(start, start + _chartLogPerPage);

  if (slice.length === 0) {
    tbody.innerHTML = `<tr><td colspan="6" class="px-8 py-8 text-center text-on-surface-variant/50 italic text-sm">Chờ dữ liệu WebSocket...</td></tr>`;
  } else {
    tbody.innerHTML = slice
      .map(
        (r) => `
      <tr class="hover:bg-primary/5 transition-colors group">
        <td class="px-8 py-8 text-sm font-medium text-on-surface">Hôm nay, ${r.time}</td>
        <td class="px-6 py-8 font-mono text-xs text-on-surface-variant">${r.sensor}</td>
        <td class="px-6 py-8 text-sm text-on-surface-variant">${r.khuVuc}</td>
        <td class="px-6 py-8 text-sm font-semibold">${r.chi}</td>
        <td class="px-6 py-8 text-sm font-bold ${r.color}">${r.val}</td>
        <td class="px-8 py-5"><span class="px-3 py-1 ${r.badge} text-[10px] font-black uppercase rounded-full">${r.status}</span></td>
      </tr>`,
      )
      .join("");
  }

  if (countEl)
    countEl.textContent = `Hiển thị ${start + 1}–${Math.min(start + _chartLogPerPage, total)} trong số ${total} sự kiện`;
  if (pageEl) pageEl.textContent = `${_chartLogPage} / ${totalPages}`;
  if (prevBtn) prevBtn.disabled = _chartLogPage <= 1;
  if (nextBtn) nextBtn.disabled = _chartLogPage >= totalPages;
}

function chartLogPrev() {
  if (_chartLogPage > 1) {
    _chartLogPage--;
    renderChartLogTable();
  }
}
function chartLogNext() {
  const totalPages = Math.ceil(_chartLog.length / _chartLogPerPage);
  if (_chartLogPage < totalPages) {
    _chartLogPage++;
    renderChartLogTable();
  }
}
