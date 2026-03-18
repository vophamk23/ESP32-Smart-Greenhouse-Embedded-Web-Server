/* ============================================================
   control.js — Greenhouse IoT
   Điều khiển thiết bị qua WebSocket → ESP32 RPC queue
   Commands: setPump, setFan, setMode
============================================================ */
"use strict";

function ctrlDevice(device, action) {
  if (State.data.autoMode) {
    showToast("⚠ Chuyển sang chế độ THỦ CÔNG trước.", true);
    return;
  }
  const on = action === "on";
  if (device === "pump") sendWS({ cmd: "setPump", value: on });
  if (device === "fan") sendWS({ cmd: "setFan", value: on });
  showToast(
    (device === "pump" ? "Máy bơm" : "Quạt") + " → " + (on ? "BẬT" : "TẮT"),
  );
}

function setMode(mode) {
  const isAuto = mode === "AUTO";
  sendWS({ cmd: "setMode", value: mode });
  State.data.autoMode = isAuto;
  updateModeButtons(isAuto);
  showToast("Chế độ: " + (isAuto ? "TỰ ĐỘNG" : "THỦ CÔNG"));
}

function setFanSpeed(speed) {
  if (State.data.autoMode) {
    showToast("⚠ Chuyển sang chế độ THỦ CÔNG trước.", true);
    return;
  }
  // Bật quạt trước nếu chưa bật
  if (!State.data.fan) sendWS({ cmd: "setFan", value: true });
  // Gửi tốc độ
  sendWS({ cmd: "setFanSpeed", value: speed });

  // Cập nhật UI bar + nút active
  const bar = document.getElementById("dled-fan-bar");
  const midBtn = document.getElementById("fan-speed-mid");
  const highBtn = document.getElementById("fan-speed-high");
  const activeClass =
    "py-2 rounded-xl text-xs font-bold bg-secondary text-white transition-all active:scale-95";
  const inactiveClass =
    "py-2 rounded-xl text-xs font-bold border border-secondary/30 text-secondary hover:bg-secondary/10 transition-all active:scale-95";

  if (speed === "MID") {
    if (bar) bar.style.width = "50%";
    if (midBtn) midBtn.className = activeClass;
    if (highBtn) highBtn.className = inactiveClass;
    showToast("Quạt → Tốc độ VỪA");
  } else {
    if (bar) bar.style.width = "100%";
    if (midBtn) midBtn.className = inactiveClass;
    if (highBtn) highBtn.className = activeClass;
    showToast("Quạt → Tốc độ CAO");
  }
}
