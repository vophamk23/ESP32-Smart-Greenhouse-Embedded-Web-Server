/* ============================================================
   helpers.js — Greenhouse IoT
   Utility functions: toast, modal, time, format
============================================================ */
"use strict";

/* ── Toast ── */
function showToast(msg, isError = false) {
  const t = document.getElementById("toast");
  t.textContent = msg;
  t.className = "toast show" + (isError ? " error" : "");
  clearTimeout(t._timer);
  t._timer = setTimeout(() => {
    t.className = "toast";
  }, 3000);
}

/* ── Confirm Modal ── */
function showConfirm(title, msg, onOk) {
  document.getElementById("confirmTitle").textContent = title;
  document.getElementById("confirmMsg").textContent = msg;
  const modal = document.getElementById("confirmModal");
  modal.style.display = "flex";
  const okBtn = document.getElementById("confirmOk");
  okBtn.onclick = () => {
    closeConfirm();
    onOk();
  };
}
function closeConfirm() {
  document.getElementById("confirmModal").style.display = "none";
}

/* ── Navigation ── */
function navigate(page) {
  State.currentPage = page;

  // Hide all pages
  document
    .querySelectorAll(".page")
    .forEach((p) => p.classList.remove("active"));
  const el = document.getElementById("page-" + page);
  if (el) el.classList.add("active");

  // Update sidebar active state
  document.querySelectorAll(".nav-item").forEach((a) => {
    a.classList.toggle("active", a.dataset.page === page);
  });

  // Update page title
  const titles = {
    dashboard: "Dashboard",
    charts: "Phân tích cảm biến",
    logs: "Nhật ký hoạt động",
    control: "Điều khiển thiết bị",
    threshold: "Ngưỡng tưới & Logic",
    schedule: "Lịch tưới",
    smartlogic: "Smart Logic",
    devices: "Quản lý thiết bị",
    notify: "Cấu hình thông báo",
    account: "Tài khoản",
  };
  const t = document.getElementById("pageTitle");
  if (t) t.textContent = titles[page] || page;

  // On navigate to charts, init chart if needed
  if (page === "charts") initCharts();
  if (page === "devices") renderDeviceGrid();
  if (page === "logs") renderLogs();
  if (page === "schedule") renderScheduleList();
}

/* ── Topbar clock ── */
function startClock() {
  function tick() {
    const now = new Date();
    const s = now.toLocaleTimeString("vi-VN", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
    const el = document.getElementById("topbarTime");
    if (el) el.textContent = s;
  }
  tick();
  setInterval(tick, 1000);
}

/* ── Format seconds ── */
function fmtSec(s) {
  s = Math.round(s);
  if (s < 60) return s + "s";
  if (s < 3600) return Math.floor(s / 60) + "m" + (s % 60) + "s";
  return Math.floor(s / 3600) + "h" + Math.floor((s % 3600) / 60) + "m";
}

/* ── Format bytes ── */
function fmtBytes(b) {
  if (b >= 1024 * 1024) return (b / (1024 * 1024)).toFixed(1) + "MB";
  if (b >= 1024) return (b / 1024).toFixed(0) + "KB";
  return b + "B";
}

/* ── Format datetime ── */
function fmtDatetime(ts) {
  if (!ts) return "--";
  const d = new Date(ts);
  return (
    d.toLocaleDateString("vi-VN") +
    " " +
    d.toLocaleTimeString("vi-VN", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    })
  );
}

/* ── LED indicator helper ── */
function setLedIndicator(elId, on) {
  const el = document.getElementById(elId);
  if (!el) return;
  el.style.background = on ? "#10b981" : "#cbd5e1";
  if (on) el.style.boxShadow = "0 0 6px #10b981";
  else el.style.boxShadow = "none";
}

/* ── Alert chip helper ── */
function setAlertChip(elId, warn, label) {
  const el = document.getElementById(elId);
  if (!el) return;
  if (warn) {
    el.className =
      "flex items-center gap-2 px-3 py-2 rounded-full text-xs font-bold bg-error/10 text-error border border-error/30";
  } else {
    el.className =
      "flex items-center gap-2 px-3 py-2 rounded-full text-xs font-bold bg-primary/5 text-primary border border-primary/20";
  }
  if (label) el.textContent = label;
}
