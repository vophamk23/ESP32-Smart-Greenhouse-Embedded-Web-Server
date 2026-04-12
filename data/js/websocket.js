/* ============================================================
   websocket.js — Greenhouse IoT
   Kết nối WebSocket 2 chiều với ESP32 (webserver_greenhouse.cpp)

   NHẬN:  JSON chứa toàn bộ trạng thái cảm biến + thiết bị (mỗi 1s)
   GỬI:   { cmd: "setPump"|"setFan"|"setMode", value: ... }
============================================================ */
/* ────────────────────────────────────────────────────────────
   connectWS()
   Tạo kết nối WebSocket tới ESP32 và đăng ký toàn bộ event handler.
   Được gọi 1 lần khi trang load, và tự gọi lại sau 3s nếu mất kết nối.
──────────────────────────────────────────────────────────── */
function connectWS() {
  // ── Lấy các element hiển thị trạng thái kết nối trên UI ──
  const dot = document.getElementById("wsDot"); // Chấm tròn màu (xanh/đỏ)
  const label = document.getElementById("wsLabel"); // Text trạng thái

  // Báo UI đang kết nối (chấm xám + text)
  if (dot) dot.className = "ws-dot";
  if (label) label.textContent = "Đang kết nối...";

  // ── Tạo WebSocket trỏ về ESP32 (cùng host, endpoint /ws) ──
  // Ví dụ: ws://192.168.1.100/ws
  const wsUrl = "ws://" + location.host + "/ws";
  State.ws = new WebSocket(wsUrl); // Lưu vào State để sendWS() dùng lại

  /* ──────────────────────────────────────────────────────────
     onopen — Kết nối WebSocket thành công
     Chạy 1 lần ngay khi handshake hoàn tất.
  ────────────────────────────────────────────────────────── */
  State.ws.onopen = () => {
    State.wsConnected = true; // Đánh dấu đã kết nối
    State.wsRetry = 0; // Reset bộ đếm retry

    // Cập nhật UI: chấm xanh
    if (dot) dot.className = "ws-dot connected";
    if (label) label.textContent = "Đã kết nối";
    console.log("[WS] Connected:", wsUrl);

    // ── Lấy snapshot ngay qua REST thay vì chờ WebSocket message đầu tiên ──
    // Lý do: WebSocket chỉ push sau 1 giây, REST trả về ngay lập tức
    // → UI không bị trống khi vừa mở trang
    fetch("/api/status")
      .then((r) => r.json())
      .then((d) => {
        updateUI(d); // Vẽ UI với dữ liệu hiện tại
        Object.assign(State.data, d); // Merge vào State để các module khác dùng
      })
      .catch((e) => console.warn("[API]", e));
  };

  /* 
     onmessage — Nhận JSON từ ESP32 (mỗi ~1 giây)
     d là object chứa: temperature, humidity, soilMoisture,
     lightLux, pump, fan, autoMode, alertTemp, alertSoil, ...
     cfg_soilDry, cfg_soilWet, cfg_tempHigh, cfg_pumpMax, cfg_pumpCool
 */
  State.ws.onmessage = (event) => {
    try {
      const d = JSON.parse(event.data); // Parse JSON nhận được

      // ── Cập nhật State và UI ──
      Object.assign(State.data, d); // Merge dữ liệu mới vào State toàn cục
      updateUI(d); // Cập nhật tất cả các DOM element
      pushChartPoint(d); // Thêm điểm dữ liệu mới vào biểu đồ (charts.js)

      // ── Sync slider ngưỡng tưới từ cfg_* do ESP32 gửi về ──
      // Chỉ chạy 1 lần hiệu quả khi có thay đổi (hàm tự kiểm tra)
      syncThresholdFromESP32(d);

      // ── Theo dõi trạng thái bơm để tự ghi nhật ký hoạt động ──

      // Trường hợp: bơm vừa BẬT (trước đó đang tắt)
      if (d.pump && !State.data._pumpWasOn) {
        State.data._pumpWasOn = true; // Đánh dấu bơm đang chạy
        State.data._pumpSessionStart = Date.now(); // Ghi thời điểm bắt đầu
        startPumpLog(d); // Tạo dòng nhật ký mới
      }

      // Trường hợp: bơm vừa TẮT (trước đó đang bật)
      if (!d.pump && State.data._pumpWasOn) {
        State.data._pumpWasOn = false; // Đánh dấu bơm đã dừng
        State.data._pumpSessionStart = null; // Xóa thời điểm bắt đầu
        endPumpLog(); // Ghi thời lượng vào nhật ký
      }

      // ── Cập nhật bộ đếm thời gian phiên bơm hiện tại trên UI ──
      const sesEl = document.getElementById("pump-session-time");
      if (sesEl) {
        if (d.pump && State.data._pumpSessionStart) {
          // Bơm đang chạy → tính số giây đã chạy từ lúc bắt đầu
          const sec = Math.floor(
            (Date.now() - State.data._pumpSessionStart) / 1000,
          );
          sesEl.textContent = fmtSec(sec); // Hiển thị dạng "2p 34s"
        } else {
          // Bơm đang tắt → hiển thị dấu gạch
          sesEl.textContent = "--";
        }
      }
    } catch (e) {
      // JSON bị lỗi (hiếm gặp) → log ra console, không crash app
      console.warn("[WS] Parse error:", e);
    }
  };

  /*
     onclose — Mất kết nối WebSocket
     Xảy ra khi: ESP32 reset, mất điện, mất WiFi, hoặc timeout.
     Tự động thử kết nối lại sau 3 giây.
   */
  State.ws.onclose = () => {
    State.wsConnected = false;

    // Cập nhật UI: chấm đỏ + thông báo
    if (dot) dot.className = "ws-dot error";
    if (label) label.textContent = "Mất kết nối — thử lại...";

    console.warn("[WS] Closed, retry in 3s");
    setTimeout(connectWS, 3000); // Gọi lại connectWS() sau 3 giây
  };

  /* 
     onerror — Lỗi kết nối WebSocket (network error, refused...)
     Chỉ cập nhật UI, không cần retry vì onclose sẽ tự gọi sau đó.
  */
  State.ws.onerror = () => {
    if (dot) dot.className = "ws-dot error";
    if (label) label.textContent = "Lỗi kết nối";
    // Không cần setTimeout ở đây — onerror luôn đi kèm onclose
  };
}

/* 
   sendWS(msg)
   Gửi lệnh điều khiển từ browser đến ESP32 qua WebSocket.

   Ví dụ sử dụng:
     sendWS({ cmd: "setPump", value: true  })  → bật bơm
     sendWS({ cmd: "setFan",  value: false })  → tắt quạt
     sendWS({ cmd: "setMode", value: "AUTO" }) → chuyển chế độ tự động

   ESP32 nhận → handleBrowserMessage() → xRpcCommandQueue → thực thi
*/
function sendWS(msg) {
  if (State.ws && State.ws.readyState === WebSocket.OPEN) {
    // Kết nối đang mở → gửi bình thường
    State.ws.send(JSON.stringify(msg));
    console.log("[WS→ESP32]", msg);
  } else {
    // Chưa kết nối → thông báo lỗi cho người dùng
    showToast("⚠ Chưa kết nối ESP32", true);
  }
}
