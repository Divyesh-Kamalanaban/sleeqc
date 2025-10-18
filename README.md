
```markdown
# 🔐 ML-DSA44 on ESP32  
### Post-Quantum Signature Benchmark & Wi-Fi Signing Server

> **Lightweight ML-DSA44 (Dilithium2-class) digital signature implementation** for ESP32, built on FreeRTOS and ESP-IDF.  
> Enables both **performance benchmarking** and a **real-time signing server** over Wi-Fi.

---

## 🚀 Overview
This project demonstrates how **post-quantum cryptography (PQC)** — specifically ML-DSA44, a lattice-based signature algorithm — can be deployed efficiently on **embedded systems** like the ESP32.

It includes two primary operation modes:
1. 🧮 **Benchmark Mode** — measures key generation, signing, and verification speed.  
2. 🌐 **Wi-Fi Signing Server Mode** — listens for TCP messages and returns PQ signatures on demand.

---

## ⚙️ Features
- 🧩 **Fully functional ML-DSA44 implementation** (based on PQClean)
- ⏱️ Microsecond-level performance timing via `esp_timer`
- 🔄 **FreeRTOS task isolation** for benchmark and server
- 📶 **Wi-Fi STA mode integration** for real-time signing
- 🧠 **Memory-efficient** (<400 KB heap used)
- 🧰 Built on ESP-IDF (v5+)

---

## 🧱 Project Structure

```

/main
├── main.c              # Entry point (this file)
├── api.h               # ML-DSA44 PQC API header
└── CMakeLists.txt
/docs
└── ML-DSA44_Expert_Documentation.md  # Full technical deep dive

```

---

## 🧠 Modes of Operation

| Mode | Define | Description |
|------|---------|-------------|
| **Benchmark Mode** | `#define RUN_WIFI_SERVER 0` | Runs ML-DSA44 keygen, sign, verify, and measures time. |
| **Wi-Fi Signing Server** | `#define RUN_WIFI_SERVER 1` | Starts TCP server on port 8000; signs incoming messages. |

---

## 🧩 Function Overview

| Function | Description |
|-----------|--------------|
| `measure_task()` | Runs performance benchmarks for keygen, sign, and verify. |
| `listen_and_sign_task()` | Waits for TCP clients, signs received data, and returns the signature. |
| `wifi_init_sta()` | Configures and connects ESP32 to Wi-Fi (STA mode). |
| `app_main()` | Initializes NVS, sets up Wi-Fi, and launches the appropriate FreeRTOS task. |

---

## 📊 Sample Benchmark Output

```

--- ML-DSA Performance Measurement ---
KeyGen Time:  22251 us
Sign Time:    35991 us
Verify Time:  23629 us
Verification OK
Heap free: 370460 bytes

````

| Operation | Avg Time (ms) | Description |
|------------|---------------|--------------|
| KeyGen | 22.3 ms | Generates PQ keypair |
| Sign | 36.0 ms | Creates digital signature |
| Verify | 23.6 ms | Confirms signature authenticity |

---

## ⚡ Getting Started

### 🧰 Requirements
- ESP-IDF v5.0+
- PQClean or ML-DSA library integrated (header: `api.h`)
- ESP32 DevKitC / ESP32-WROOM / ESP32-S3

### 🔧 Build & Flash
```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
````

### 🌐 Optional: Run Wi-Fi Signing Server

In `main.c`:

```c
#define RUN_WIFI_SERVER 1
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

Then:

```bash
idf.py build flash monitor
```

A Python client can then connect to:

```
<ESP32_IP>:8000
```

and send any message to get its ML-DSA signature.

---

## 🧩 Future Improvements

* [ ] Add ML-KEM (Kyber) key exchange support
* [ ] Implement message verification client in Python
* [ ] Add MQTT / HTTPS layer for IoT-scale secure messaging
* [ ] Optimize heap usage and parallel task scheduling
* [ ] Integrate with ESP-Secure-Boot for firmware attestation

---

## 🔬 Technical Documentation

A full deep-dive explanation of all functions, architecture, and performance analysis is available here:
👉 [**ML-DSA44 Expert Documentation →**](../../wiki/ML-DSA44-Expert-Documentation)

---

## 🧠 Acknowledgements

* [PQClean](https://github.com/pqclean/pqclean) — clean implementations of PQC algorithms
* Espressif ESP-IDF Framework
* NIST PQC Standardization (FIPS 204: ML-DSA)

---

## 📄 License

MIT License — see [LICENSE](LICENSE)

---

⭐ **Author:** Divyesh K.
💡 *If this helped you, consider starring the repo!*

