/* ============================================================
   auth.js — Greenhouse IoT
   Client-side auth (demo). Credentials stored in State.accounts.
============================================================ */
"use strict";

function doLogin() {
  const user = document.getElementById("loginUser").value.trim();
  const pass = document.getElementById("loginPass").value;
  const errEl = document.getElementById("loginError");

  const match = State.accounts.find(
    (a) => a.username === user && a.password === pass,
  );
  if (!match) {
    errEl.classList.remove("hidden");
    return;
  }
  errEl.classList.add("hidden");
  State.isLoggedIn = true;
  State.username = user;

  // Update avatar
  const initials = user.substring(0, 2).toUpperCase();
  const el = document.getElementById("avatarName");
  const av = document.getElementById("accountAvatar");
  if (el) el.textContent = initials;
  if (av) av.textContent = initials;
  const nameEl = document.getElementById("accountName");
  if (nameEl) nameEl.textContent = user;

  // Show app
  document.getElementById("loginOverlay").style.display = "none";
  const app = document.getElementById("app");
  app.style.display = "flex";

  // Boot
  startClock();
  navigate("dashboard");
  connectWS();
}

function doLogout() {
  State.isLoggedIn = false;
  if (State.ws) {
    State.ws.close();
    State.ws = null;
  }
  document.getElementById("app").style.display = "none";
  document.getElementById("loginOverlay").style.display = "flex";
  document.getElementById("loginUser").value = "";
  document.getElementById("loginPass").value = "";
}

function saveAccount() {
  const oldPass = document.getElementById("accPassOld").value;
  const newPass = document.getElementById("accPassNew").value;
  const cfm = document.getElementById("accPassCfm").value;
  if (!oldPass && !newPass) {
    showToast("Đã lưu thông tin.");
    return;
  }
  const acc = State.accounts.find((a) => a.username === State.username);
  if (!acc || acc.password !== oldPass) {
    showToast("Mật khẩu hiện tại không đúng.", true);
    return;
  }
  if (newPass !== cfm) {
    showToast("Mật khẩu xác nhận không khớp.", true);
    return;
  }
  if (newPass.length < 6) {
    showToast("Mật khẩu cần ít nhất 6 ký tự.", true);
    return;
  }
  acc.password = newPass;
  document.getElementById("accPassOld").value = "";
  document.getElementById("accPassNew").value = "";
  document.getElementById("accPassCfm").value = "";
  showToast("✓ Đã cập nhật mật khẩu.");
}
