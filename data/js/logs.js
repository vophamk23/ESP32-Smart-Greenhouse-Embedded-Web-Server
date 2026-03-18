/* ============================================================
   logs.js — Greenhouse IoT
   Ghi log các lần bơm nước.
   Được trigger tự động từ websocket.js khi pump state thay đổi.
============================================================ */
"use strict";

/* ── Called by websocket.js when pump turns ON ── */
function startPumpLog(d) {
  const id = State._logIdCounter++;
  State._activeLogId = id;
  State.logs.unshift({
    id,
    start: Date.now(),
    end: null,
    duration: null,
    reason: d.autoMode ? "AUTO" : "MANUAL",
    status: "running",
    soilPct: d.soilMoisture != null ? d.soilMoisture.toFixed(0) : "--",
  });
  renderLogs();
}

/* ── Called by websocket.js when pump turns OFF ── */
function endPumpLog() {
  const log = State.logs.find((l) => l.id === State._activeLogId);
  if (log) {
    log.end = Date.now();
    log.duration = Math.round((log.end - log.start) / 1000);
    log.status = "ok";
  }
  State._activeLogId = null;
  renderLogs();
  updateLogStats();
}

/* ── Filter ── */
function filterLogs(val) {
  State.logsFilter = val.toLowerCase();
  State.logsPage = 1;
  renderLogs();
}

/* ── Render table ── */
function renderLogs() {
  const filtered = State.logs.filter((l) => {
    if (!State.logsFilter) return true;
    return (
      (l.reason || "").toLowerCase().includes(State.logsFilter) ||
      fmtDatetime(l.start).toLowerCase().includes(State.logsFilter)
    );
  });

  const total = filtered.length;
  const pages = Math.max(1, Math.ceil(total / State.logsPerPage));
  if (State.logsPage > pages) State.logsPage = pages;
  const start = (State.logsPage - 1) * State.logsPerPage;
  const slice = filtered.slice(start, start + State.logsPerPage);

  const tbody = document.getElementById("logTbody");
  if (!tbody) return;

  if (slice.length === 0) {
    tbody.innerHTML = `<tr><td colspan="6" class="px-6 py-10 text-center text-on-surface-variant/50 italic">Chưa có nhật ký nào.</td></tr>`;
  } else {
    tbody.innerHTML = slice.map((l) => logRow(l)).join("");
  }

  const countLbl = document.getElementById("log-count-label");
  if (countLbl) countLbl.textContent = total + " bản ghi";

  renderPagination(pages);
  updateLogStats();
}

function logRow(l) {
  // Badge lý do — Stitch style: pill nhạt với dot màu
  const reasonCfg = {
    AUTO: {
      bg: "bg-secondary/10 text-secondary",
      dot: "bg-secondary",
      label: "AUTO",
    },
    MANUAL: {
      bg: "bg-surface-variant text-on-surface-variant",
      dot: "bg-on-surface-variant/40",
      label: "THỦ CÔNG",
    },
    SCHED: {
      bg: "bg-tertiary/10 text-tertiary",
      dot: "bg-tertiary",
      label: "LỊCH TRÌNH",
    },
    WD: { bg: "bg-error/10 text-error", dot: "bg-error", label: "WATCHDOG" },
  }[l.reason] || {
    bg: "bg-surface-variant text-on-surface-variant",
    dot: "bg-outline",
    label: l.reason,
  };

  // Status — dot tròn + text, không icon
  const statusHtml =
    l.status === "running"
      ? `<span class="inline-flex items-center gap-2 px-3 py-1.5 rounded-full bg-primary/10 text-primary text-[11px] font-bold uppercase tracking-wide"><span class="w-2 h-2 rounded-full bg-primary animate-pulse"></span>Đang chạy</span>`
      : l.status === "ok"
        ? `<span class="inline-flex items-center gap-2 px-3 py-1.5 rounded-full bg-primary/10 text-primary text-[11px] font-bold uppercase tracking-wide"><span class="w-2 h-2 rounded-full bg-primary flex-shrink-0"></span>Thành công</span>`
        : `<span class="inline-flex items-center gap-2 px-3 py-1.5 rounded-full bg-error/10 text-error text-[11px] font-bold uppercase tracking-wide"><span class="w-2 h-2 rounded-full bg-error flex-shrink-0"></span>Bị gián đoạn</span>`;

  // Format ngày 2 dòng — bold date, nhạt time
  function fmtCell(ts) {
    if (!ts)
      return '<span class="text-on-surface-variant/40 italic text-sm">Đang chạy...</span>';
    const d = new Date(ts);
    const date = d.toLocaleDateString("vi-VN");
    const time = d.toLocaleTimeString("vi-VN", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
    return `<div class="flex flex-col gap-0.5"><span class="text-sm font-bold text-on-surface">${date}</span><span class="text-[11px] text-on-surface-variant/50">${time}</span></div>`;
  }

  return `<tr class="hover:bg-surface-container/20 transition-colors group">
    <td class="px-6 py-6 text-sm text-on-surface-variant font-medium">${String(l.id).padStart(5, "0")}</td>
    <td class="px-6 py-6">${fmtCell(l.start)}</td>
    <td class="px-6 py-6">${fmtCell(l.end)}</td>
    <td class="px-6 py-5 text-sm font-semibold text-on-surface">${l.duration != null ? fmtSec(l.duration) : "--"}</td>
    <td class="px-6 py-5">
      <span class="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-full ${reasonCfg.bg} text-[11px] font-bold uppercase tracking-wide">
        <span class="w-1.5 h-1.5 rounded-full ${reasonCfg.dot} flex-shrink-0"></span>${reasonCfg.label}
      </span>
    </td>
    <td class="px-6 py-5">${statusHtml}</td>
    <td class="px-6 py-6 text-right">
      <button class="w-8 h-8 rounded-full border border-outline-variant/30 flex items-center justify-center text-on-surface-variant hover:text-primary hover:border-primary/40 transition-all ml-auto">
        <span class="material-symbols-outlined text-base">chevron_right</span>
      </button>
    </td>
  </tr>`;
}

function renderPagination(pages) {
  const el = document.getElementById("logPagination");
  const countEl = document.getElementById("log-count-label");
  if (!el) return;

  // Cập nhật text "Đang hiển thị X đến Y trong số Z bản ghi"
  const filtered = State.logs.filter((l) => {
    if (!State.logsFilter) return true;
    return (
      (l.reason || "").toLowerCase().includes(State.logsFilter) ||
      fmtDatetime(l.start).toLowerCase().includes(State.logsFilter)
    );
  });
  const total = filtered.length;
  const start = (State.logsPage - 1) * State.logsPerPage + 1;
  const end = Math.min(State.logsPage * State.logsPerPage, total);
  if (countEl)
    countEl.textContent =
      total > 0
        ? `Đang hiển thị ${start} đến ${end} trong số ${total.toLocaleString("vi-VN")} bản ghi`
        : "0 bản ghi";

  if (pages <= 1) {
    el.innerHTML = "";
    return;
  }

  const dis = (cond) =>
    cond ? "opacity-30 cursor-default pointer-events-none" : "";
  const btn = (icon, page, disabled) =>
    `<button onclick="logsGoPage(${page})" class="w-8 h-8 flex items-center justify-center rounded-lg hover:bg-surface-container transition-colors ${dis(disabled)}">
      <span class="material-symbols-outlined text-lg">${icon}</span>
    </button>`;
  const numBtn = (i) =>
    `<button onclick="logsGoPage(${i})" class="w-8 h-8 flex items-center justify-center rounded-lg ${i === State.logsPage ? "bg-primary text-white shadow-sm" : "hover:bg-surface-container"} text-xs font-bold transition-colors">${i}</button>`;

  let html = btn("first_page", 1, State.logsPage === 1);
  html += btn("chevron_left", State.logsPage - 1, State.logsPage === 1);

  // Page numbers: 1, 2, 3, ..., last
  const p = State.logsPage;
  if (pages <= 5) {
    for (let i = 1; i <= pages; i++) html += numBtn(i);
  } else {
    html += numBtn(1);
    if (p > 3)
      html += `<span class="mx-1 text-on-surface-variant/40 self-center">...</span>`;
    for (let i = Math.max(2, p - 1); i <= Math.min(pages - 1, p + 1); i++)
      html += numBtn(i);
    if (p < pages - 2)
      html += `<span class="mx-1 text-on-surface-variant/40 self-center">...</span>`;
    html += numBtn(pages);
  }

  html += btn("chevron_right", State.logsPage + 1, State.logsPage === pages);
  html += btn("last_page", pages, State.logsPage === pages);

  el.innerHTML = html;
}

function logsGoPage(p) {
  const pages = Math.ceil(State.logs.length / State.logsPerPage);
  State.logsPage = Math.max(1, Math.min(pages, p));
  renderLogs();
}

function updateLogStats() {
  const done = State.logs.filter((l) => l.status !== "running");
  setText("log-total-count", State.logs.length);
  if (done.length > 0) {
    const ok = done.filter((l) => l.status === "ok").length;
    const rate = Math.round((ok / done.length) * 100);
    setText("log-success-rate", rate + "%");
    const totalSec = done.reduce((s, l) => s + (l.duration || 0), 0);
    setText("log-total-time", fmtSec(totalSec));
  }
}

function clearLogs() {
  showConfirm("Xóa nhật ký", "Xóa toàn bộ nhật ký bơm nước?", () => {
    State.logs = [];
    State.logsPage = 1;
    renderLogs();
    showToast("Đã xóa nhật ký.");
  });
}

function exportLogs() {
  if (State.logs.length === 0) {
    showToast("Không có dữ liệu.", true);
    return;
  }
  const rows = ["ID,Bắt đầu,Kết thúc,Thời lượng(s),Lý do,Trạng thái"];
  State.logs.forEach((l) => {
    rows.push(
      [
        l.id,
        fmtDatetime(l.start),
        l.end ? fmtDatetime(l.end) : "",
        l.duration || "",
        l.reason,
        l.status,
      ].join(","),
    );
  });
  const blob = new Blob([rows.join("\n")], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "greenhouse_logs.csv";
  a.click();
  URL.revokeObjectURL(url);
  showToast("Đã xuất CSV.");
}
