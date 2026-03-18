/* ============================================================
   state.js — Greenhouse IoT
   Single source of truth. Khớp 100% với webserver_greenhouse.cpp
   WS JSON fields: temperature, humidity, soilMoisture, lightLux,
   pump, fan, autoMode, alertTemp, alertHumidity, alertSoil,
   alertLight, pumpCount, totalPumpTimeSec, dhtError,
   wifiRssi, freeHeap, lcdPage, ledColor
============================================================ */
"use strict";

window.State = {
  isLoggedIn: false,
  username: "",

  ws: null,
  wsRetry: 0,
  wsConnected: false,

  currentPage: "dashboard",

  data: {
    temperature: null,
    humidity: null,
    soilMoisture: null,
    lightLux: null,
    pump: false,
    fan: false,
    autoMode: true,
    alertTemp: false,
    alertHumidity: false,
    alertSoil: false,
    alertLight: false,
    dhtError: false,
    pumpCount: 0,
    totalPumpTimeSec: 0,
    wifiRssi: 0,
    freeHeap: 0,
    lcdPage: 0,
    ledColor: "#000000",
    _pumpWasOn: false,
  },

  threshold: {
    soilDry: 30,
    soilWet: 70,
    pumpMax: 60,
    pumpCool: 120,
    tempHigh: 35,
  },

  scheduleEnabled: false,
  schedules: [],

  smartEnabled: false,
  smartConfig: { apiKey: "", city: "Ho Chi Minh City", rainThresh: 60 },

  logs: [],
  logsPage: 1,
  logsPerPage: 10,
  logsFilter: "",
  _activeLogId: null,
  _logIdCounter: 1,

  history: { labels: [], temp: [], humi: [], soil: [], light: [], pump: [] },
  historyMax: 120,

  devices: [
    {
      id: "TH-001-A",
      type: "sensor_temp",
      name: "Cảm biến Nhiệt độ",
      online: true,
    },
    {
      id: "HU-042-B",
      type: "sensor_humi",
      name: "Cảm biến Độ ẩm",
      online: true,
    },
    {
      id: "SL-101-C",
      type: "sensor_light",
      name: "Cảm biến Ánh sáng",
      online: true,
    },
    { id: "SM-200-D", type: "sensor_soil", name: "Cảm biến Đất", online: true },
    {
      id: "WP-992-X",
      type: "actuator_pump",
      name: "Máy bơm nước",
      online: true,
    },
    {
      id: "FN-812-C",
      type: "actuator_fan",
      name: "Hệ thống Quạt",
      online: true,
    },
    { id: "DS-011-L", type: "display_lcd", name: "Màn hình LCD", online: true },
  ],

  notify: {
    tgToken: "",
    tgChatId: "",
    smtpHost: "",
    emailTo: "",
    triggers: { dry: true, pumpLong: true, dht: true, temp: false },
  },

  accounts: [{ username: "vopham", password: "vopham123" }],
};
