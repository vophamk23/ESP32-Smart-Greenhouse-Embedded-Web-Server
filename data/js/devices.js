/* ============================================================
   devices.js — Greenhouse IoT
   Quản lý thiết bị — Factory Method Pattern
   Hiển thị device grid, thêm/xóa thiết bị
============================================================ */
"use strict";

/* ── Device type config (Factory Method) ── */
const DEVICE_FACTORY = {
  sensor_temp: {
    icon: "device_thermostat",
    label: "Cảm biến Nhiệt độ",
    color: "text-secondary",
    bg: "bg-secondary-container/20",
  },
  sensor_humi: {
    icon: "humidity_mid",
    label: "Cảm biến Độ ẩm",
    color: "text-secondary",
    bg: "bg-secondary-container/20",
  },
  sensor_light: {
    icon: "light_mode",
    label: "Cảm biến Ánh sáng",
    color: "text-amber-500",
    bg: "bg-amber-50",
  },
  sensor_soil: {
    icon: "grass",
    label: "Cảm biến Đất",
    color: "text-tertiary",
    bg: "bg-tertiary/10",
  },
  actuator_pump: {
    icon: "water_drop",
    label: "Máy bơm nước",
    color: "text-primary",
    bg: "bg-primary/10",
  },
  actuator_fan: {
    icon: "air",
    label: "Quạt thông gió",
    color: "text-tertiary",
    bg: "bg-tertiary/10",
  },
  actuator_led: {
    icon: "light_mode",
    label: "LED NeoPixel",
    color: "text-amber-500",
    bg: "bg-amber-50",
  },
  display_lcd: {
    icon: "screenshot_monitor",
    label: "Màn hình LCD",
    color: "text-secondary",
    bg: "bg-secondary-container/20",
  },
};

function renderDeviceGrid() {
  const grid = document.getElementById("deviceGrid");
  const label = document.getElementById("device-count-label");
  if (!grid) return;

  const online = State.devices.filter((d) => d.online).length;
  if (label)
    label.textContent =
      State.devices.length + " thiết bị — " + online + " đang hoạt động";

  if (State.devices.length === 0) {
    grid.innerHTML = `<div class="col-span-full border-2 border-dashed border-outline-variant/30 rounded-3xl p-12 flex flex-col items-center gap-4 text-center hover:border-primary/40 cursor-pointer transition-all" onclick="openAddDevice()">
      <div class="w-16 h-16 rounded-full bg-surface-container-low flex items-center justify-center text-primary">
        <span class="material-symbols-outlined text-4xl">add_circle</span>
      </div>
      <p class="font-serif text-xl text-on-surface">Đăng ký thiết bị đầu tiên</p>
      <p class="text-on-surface-variant text-sm">Nhấn để thêm cảm biến hoặc thiết bị điều khiển</p>
    </div>`;
    return;
  }

  grid.innerHTML = State.devices.map((d) => deviceCard(d)).join("") + addCard();
}

function deviceCard(d) {
  const cfg = DEVICE_FACTORY[d.type] || {
    icon: "device_unknown",
    label: d.type,
    color: "text-on-surface-variant",
    bg: "bg-surface-container-low",
  };
  const onlineBadge = d.online
    ? `<div class="flex items-center gap-1.5 px-3 py-1 bg-primary/10 text-primary rounded-full text-[10px] font-bold uppercase"><div class="w-1.5 h-1.5 bg-primary rounded-full animate-pulse"></div>Online</div>`
    : `<div class="flex items-center gap-1.5 px-3 py-1 bg-surface-container-highest text-on-surface-variant/50 rounded-full text-[10px] font-bold uppercase"><div class="w-1.5 h-1.5 bg-on-surface-variant/30 rounded-full"></div>Offline</div>`;

  return `
  <div class="bg-white p-6 rounded-3xl shadow-sm border border-slate-100 group hover:shadow-[0_8px_32px_rgba(0,108,73,0.08)] transition-all duration-300">
    <div class="flex justify-between items-start mb-5">
      <div class="p-3 ${cfg.bg} ${cfg.color} rounded-2xl">
        <span class="material-symbols-outlined text-3xl">${cfg.icon}</span>
      </div>
      ${onlineBadge}
    </div>
    <h4 class="font-serif text-xl mb-1 group-hover:text-primary transition-colors">${d.name}</h4>
    <p class="text-sm text-on-surface-variant/70 mb-4">Loại: ${d.type}</p>
    <div class="bg-surface-container-low rounded-xl p-3 flex justify-between items-center">
      <div>
        <p class="text-[10px] uppercase tracking-widest text-on-surface-variant font-bold">Device ID</p>
        <p class="font-mono text-sm font-bold">${d.id}</p>
      </div>
      <button onclick="removeDevice('${d.id}')" class="text-on-surface-variant hover:text-error hover:bg-error/5 p-2 rounded-lg transition-all">
        <span class="material-symbols-outlined text-sm">delete</span>
      </button>
    </div>
  </div>`;
}

function addCard() {
  return `
  <div class="border-2 border-dashed border-outline-variant/30 rounded-3xl p-6 flex flex-col items-center justify-center text-center gap-3 hover:bg-primary/5 hover:border-primary/40 cursor-pointer transition-all" onclick="openAddDevice()">
    <div class="w-12 h-12 rounded-full bg-surface-container-low flex items-center justify-center text-primary">
      <span class="material-symbols-outlined text-3xl">add_circle</span>
    </div>
    <p class="text-on-surface-variant font-medium text-sm">Thêm thiết bị khác</p>
  </div>`;
}

/* ── Add Device Modal ── */
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
  const cfg = DEVICE_FACTORY[type];
  const el = document.getElementById("deviceFormFields");
  if (!el) return;
  if (!type) {
    el.innerHTML = "";
    return;
  }

  el.innerHTML = `
    <div class="space-y-4">
      <div>
        <label class="text-sm font-bold text-on-surface-variant block mb-2">Tên định danh</label>
        <input id="newDeviceName" type="text" placeholder="${cfg ? cfg.label : ""}" value="${cfg ? cfg.label : ""}"
          class="w-full bg-surface-container-low border-none rounded-xl py-3 px-4 focus:ring-2 focus:ring-primary/20 outline-none"/>
      </div>
      <div>
        <label class="text-sm font-bold text-on-surface-variant block mb-2">Device ID</label>
        <input id="newDeviceId" type="text" placeholder="VD: TH-001-A"
          class="w-full bg-surface-container-low border-none rounded-xl py-3 px-4 focus:ring-2 focus:ring-primary/20 outline-none font-mono"/>
      </div>
    </div>`;
}

function registerDevice() {
  const type = document.getElementById("deviceType").value;
  const name = document.getElementById("newDeviceName")?.value.trim();
  const id = document.getElementById("newDeviceId")?.value.trim();

  if (!type) {
    showToast("Chọn loại thiết bị.", true);
    return;
  }
  if (!id) {
    showToast("Nhập Device ID.", true);
    return;
  }
  if (State.devices.find((d) => d.id === id)) {
    showToast("Device ID đã tồn tại.", true);
    return;
  }

  State.devices.push({ id, type, name: name || id, online: false });
  closeAddDevice();
  renderDeviceGrid();
  showToast("✓ Đã đăng ký: " + (name || id));
}

function removeDevice(id) {
  showConfirm("Xóa thiết bị", "Xóa thiết bị " + id + "?", () => {
    State.devices = State.devices.filter((d) => d.id !== id);
    renderDeviceGrid();
    showToast("Đã xóa thiết bị " + id + ".");
  });
}
