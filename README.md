<div align="center">

# ESP32-S3 Low Power Water Meter + On‑Device Vision (Custom YOLO11 trained model) 💧📷 

Smart water meter reader that snaps low‑res frames, runs a tiny YOLO11 model locally with ESP-DL to detect/segment dial digits, and publishes raw frames + detection metadata over MQTT — all configurable via a captive Wi‑Fi AP + minimal web UI served from LittleFS.

<sub>Hardware target: ESP32‑S3 (PSRAM) + camera module (OV2640/GC2145 variants).</sub>

</div>

---

## ✨ Features

- 🧠 On‑device inference: Custom YOLO11 (quantized/optimized `.espdl` model) executed via ESP-DL C++ wrapper (`dl_wrapper.cpp`).
- 📸 Periodic camera capture (default QQVGA for speed / RAM) with adjustable interval (NVS‑stored).
- 📡 Dual Wi‑Fi: SoftAP for local config (`WaterMeterEsp32` / `WaterMeterPassword`) + STA auto‑connect to stored network.
- 🌐 Embedded HTTP server (port 80) serving `index.html` from LittleFS and `/config` POST endpoint for provisioning.
- 💾 LittleFS partition for static UI + future logs or cached model updates.
- 🔐 NVS‑backed persistent config: Wi‑Fi SSID/password, MQTT broker URI, capture interval.
- 📬 MQTT publishing: Unique per‑device topic (MAC derived) for JPEG frame + JSON detection payloads.
- 🧱 Modular components: `wifi/`, `mqtt/`, `config/`, `route/`, `dl/`.