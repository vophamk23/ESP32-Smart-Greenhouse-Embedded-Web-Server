/* ============================================================
   notify.js — Greenhouse IoT
   Cấu hình thông báo: Telegram Bot + Email SMTP
   Lưu config cục bộ, test kết nối qua fetch
============================================================ */
"use strict";

function switchNotifyTab(tab, btn) {
  // Update tab buttons styling
  document.querySelectorAll(".ntab-btn").forEach((b) => {
    b.className =
      "ntab-btn px-6 py-2 rounded-full text-on-surface-variant hover:text-primary transition-all font-bold";
  });
  if (btn) {
    btn.className =
      "ntab-btn px-6 py-2 rounded-full bg-white text-primary font-bold shadow-sm transition-all";
  }

  // Show/hide content
  document.getElementById("ntab-tg").style.display =
    tab === "tg" ? "block" : "none";
  document.getElementById("ntab-email").style.display =
    tab === "email" ? "block" : "none";
}

function saveNotify() {
  State.notify.tgToken = document.getElementById("tgToken")?.value.trim() || "";
  State.notify.tgChatId =
    document.getElementById("tgChatId")?.value.trim() || "";
  State.notify.smtpHost =
    document.getElementById("smtpHost")?.value.trim() || "";
  State.notify.emailTo = document.getElementById("emailTo")?.value.trim() || "";
  State.notify.triggers = {
    dry: document.getElementById("ne-dry")?.checked ?? true,
    pumpLong: document.getElementById("ne-pump-long")?.checked ?? true,
    dht: document.getElementById("ne-dht")?.checked ?? true,
    temp: document.getElementById("ne-temp")?.checked ?? false,
  };
  // Send config to ESP32
  sendWS({ cmd: "setNotifyConfig", value: State.notify });
  showToast("✓ Đã lưu cấu hình thông báo.");
}

function testTelegram() {
  const token = document.getElementById("tgToken")?.value.trim();
  const chatId = document.getElementById("tgChatId")?.value.trim();
  if (!token || !chatId) {
    showToast("Nhập Bot Token và Chat ID.", true);
    return;
  }
  showToast("Đang kiểm tra kết nối Telegram...");
  const msg =
    "🌿 Greenhouse IoT — Kiểm tra kết nối thành công!\n⏰ " +
    new Date().toLocaleString("vi-VN");
  const url = `https://api.telegram.org/bot${token}/sendMessage`;
  fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ chat_id: chatId, text: msg }),
  })
    .then((r) => r.json())
    .then((d) => {
      if (d.ok) showToast("✓ Telegram kết nối thành công!");
      else showToast("Lỗi Telegram: " + (d.description || "unknown"), true);
    })
    .catch((e) => showToast("Lỗi mạng: " + e.message, true));
}

function testEmail() {
  showToast("Email test: chức năng cần SMTP server trên ESP32.", true);
}

/* ── Called from websocket.js when alerts fire ── */
function triggerNotification(type, message) {
  const triggers = State.notify.triggers;
  const shouldNotify =
    (type === "dry" && triggers.dry) ||
    (type === "pumpLong" && triggers.pumpLong) ||
    (type === "dht" && triggers.dht) ||
    (type === "temp" && triggers.temp);
  if (!shouldNotify) return;
  // Delegate to ESP32 which handles actual sending
  sendWS({ cmd: "notify", alertType: type, message });
}
