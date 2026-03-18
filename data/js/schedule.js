/* ============================================================
   schedule.js — Greenhouse IoT
   Quản lý lịch tưới — CRUD + send to ESP32
============================================================ */
"use strict";

let _schedIdCounter = 1;
const DAYS_LABEL = ["CN", "T2", "T3", "T4", "T5", "T6", "T7"];

function toggleSchedule() {
  const enabled = document.getElementById("schedEnable")?.checked;
  State.scheduleEnabled = !!enabled;
  sendWS({ cmd: "setScheduleEnabled", value: State.scheduleEnabled });
  showToast("Lịch tưới: " + (State.scheduleEnabled ? "BẬT" : "TẮT"));
}

function addSchedule() {
  const id = _schedIdCounter++;
  State.schedules.push({
    id,
    time: "06:00",
    duration: 15,
    days: [1, 3, 5],
    enabled: true,
  });
  renderScheduleList();
}

function removeSchedule(id) {
  State.schedules = State.schedules.filter((s) => s.id !== id);
  renderScheduleList();
}

function renderScheduleList() {
  const el = document.getElementById("scheduleList");
  if (!el) return;
  if (State.schedules.length === 0) {
    el.innerHTML = `<div class="text-center py-8 text-on-surface-variant/50 italic text-sm border-2 border-dashed border-outline-variant/30 rounded-2xl">Chưa có lịch tưới nào. Nhấn "Thêm lịch mới".</div>`;
    return;
  }
  el.innerHTML = State.schedules.map((s) => schedItem(s)).join("");
}

function schedItem(s) {
  const dayBtns = DAYS_LABEL.map((d, i) => {
    const active = s.days.includes(i);
    return `<button onclick="toggleSchedDay(${s.id},${i})" class="w-8 h-8 rounded-lg text-xs font-bold transition-all ${active ? "bg-primary text-white" : "bg-surface-container-low text-on-surface-variant hover:bg-primary/10"}">${d}</button>`;
  }).join("");

  return `
  <div class="group bg-white p-5 rounded-2xl border border-slate-100 shadow-sm flex flex-col sm:flex-row items-start sm:items-center gap-4 hover:shadow-md transition-all relative overflow-hidden">
    <div class="absolute top-0 left-0 w-1 h-full bg-primary opacity-0 group-hover:opacity-100 transition-opacity rounded-l-2xl"></div>
    <div class="w-12 h-12 rounded-xl bg-primary/5 flex items-center justify-center text-primary flex-shrink-0">
      <span class="material-symbols-outlined text-2xl">water_drop</span>
    </div>
    <div class="flex-1 space-y-2">
      <div class="flex flex-wrap gap-3 items-center">
        <label class="text-xs font-bold text-on-surface-variant uppercase">Giờ</label>
        <input type="time" value="${s.time}" onchange="updateSchedTime(${s.id},this.value)"
          class="bg-surface-container-low border-none rounded-xl py-1.5 px-3 text-sm font-bold outline-none focus:ring-2 focus:ring-primary/20"/>
        <label class="text-xs font-bold text-on-surface-variant uppercase">Thời lượng</label>
        <div class="flex items-center gap-1">
          <input type="number" min="1" max="120" value="${s.duration}" onchange="updateSchedDur(${s.id},this.value)"
            class="w-16 bg-surface-container-low border-none rounded-xl py-1.5 px-3 text-sm font-bold outline-none focus:ring-2 focus:ring-primary/20"/>
          <span class="text-xs text-on-surface-variant">phút</span>
        </div>
      </div>
      <div class="flex gap-1 flex-wrap">${dayBtns}</div>
    </div>
    <div class="flex items-center gap-2">
      <label class="toggle-switch">
        <input type="checkbox" ${s.enabled ? "checked" : ""}  onchange="toggleSchedEnabled(${s.id},this.checked)"/>
        <div class="toggle-track"></div><div class="toggle-thumb"></div>
      </label>
      <button onclick="removeSchedule(${s.id})" class="w-9 h-9 rounded-full flex items-center justify-center text-on-surface-variant hover:text-error hover:bg-error/5 transition-all">
        <span class="material-symbols-outlined text-lg">delete</span>
      </button>
    </div>
  </div>`;
}

function toggleSchedDay(id, day) {
  const s = State.schedules.find((x) => x.id === id);
  if (!s) return;
  const i = s.days.indexOf(day);
  if (i >= 0) s.days.splice(i, 1);
  else s.days.push(day);
  renderScheduleList();
}

function updateSchedTime(id, val) {
  const s = State.schedules.find((x) => x.id === id);
  if (s) s.time = val;
}

function updateSchedDur(id, val) {
  const s = State.schedules.find((x) => x.id === id);
  if (s) s.duration = parseInt(val) || 15;
}

function toggleSchedEnabled(id, val) {
  const s = State.schedules.find((x) => x.id === id);
  if (s) s.enabled = val;
}

function saveSchedule() {
  sendWS({ cmd: "setSchedules", value: State.schedules });
  showToast("✓ Đã lưu " + State.schedules.length + " lịch tưới.");
}
