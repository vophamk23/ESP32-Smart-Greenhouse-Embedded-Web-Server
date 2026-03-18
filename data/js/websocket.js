/* ============================================================
   websocket.js — Greenhouse IoT
   WebSocket ↔ ESP32 webserver_greenhouse.cpp
   Receive: JSON with all sensor + actuator state
   Send:    { cmd: "setPump"|"setFan"|"setMode", value: ... }
============================================================ */
"use strict";

function connectWS() {
  const dot = document.getElementById("wsDot");
  const label = document.getElementById("wsLabel");
  if (dot) dot.className = "ws-dot";
  if (label) label.textContent = "Đang kết nối...";

  const wsUrl = "ws://" + location.host + "/ws";
  State.ws = new WebSocket(wsUrl);

  State.ws.onopen = () => {
    State.wsConnected = true;
    State.wsRetry = 0;
    if (dot) dot.className = "ws-dot connected";
    if (label) label.textContent = "Đã kết nối";
    console.log("[WS] Connected:", wsUrl);

    // Initial snapshot via REST
    fetch("/api/status")
      .then((r) => r.json())
      .then((d) => {
        updateUI(d);
        Object.assign(State.data, d);
      })
      .catch((e) => console.warn("[API]", e));
  };

  State.ws.onmessage = (event) => {
    try {
      const d = JSON.parse(event.data);
      Object.assign(State.data, d);
      updateUI(d);
      pushChartPoint(d);

      // Auto-log pump activity
      if (d.pump && !State.data._pumpWasOn) {
        State.data._pumpWasOn = true;
        State.data._pumpSessionStart = Date.now();
        startPumpLog(d);
      }
      if (!d.pump && State.data._pumpWasOn) {
        State.data._pumpWasOn = false;
        State.data._pumpSessionStart = null;
        endPumpLog();
      }
      // Cập nhật thời gian bơm phiên hiện tại
      const sesEl = document.getElementById("pump-session-time");
      if (sesEl) {
        if (d.pump && State.data._pumpSessionStart) {
          const sec = Math.floor(
            (Date.now() - State.data._pumpSessionStart) / 1000,
          );
          sesEl.textContent = fmtSec(sec);
        } else {
          sesEl.textContent = "--";
        }
      }
    } catch (e) {
      console.warn("[WS] Parse error:", e);
    }
  };

  State.ws.onclose = () => {
    State.wsConnected = false;
    if (dot) dot.className = "ws-dot error";
    if (label) label.textContent = "Mất kết nối — thử lại...";
    console.warn("[WS] Closed, retry in 3s");
    setTimeout(connectWS, 3000);
  };

  State.ws.onerror = () => {
    if (dot) dot.className = "ws-dot error";
    if (label) label.textContent = "Lỗi kết nối";
  };
}

function sendWS(msg) {
  if (State.ws && State.ws.readyState === WebSocket.OPEN) {
    State.ws.send(JSON.stringify(msg));
    console.log("[WS→ESP32]", msg);
  } else {
    showToast("⚠ Chưa kết nối ESP32", true);
  }
}
