***

```markdown
# 🔐 ML-DSA44 on ESP32
### Post-Quantum Signature Benchmark & Wi-Fi Signing Server

> **Lightweight ML-DSA44 (Dilithium2-class) digital signature implementation** for ESP32, built with FreeRTOS and ESP-IDF.  
> Offers both **performance benchmarking** and a **real-time Wi-Fi signing server** for post-quantum cryptographic operations.

---

## 🚀 Overview

This project showcases how **post-quantum cryptography (PQC)** — specifically **ML-DSA44**, a lattice-based digital signature algorithm — can be efficiently deployed on embedded systems like the **ESP32**.

It supports two primary operation modes:

1. 🧮 **Benchmark Mode** — Measures key generation, signing, and verification speeds.  
2. 🌐 **Wi-Fi Signing Server Mode** — Runs a TCP server that receives messages and returns PQ signatures in real time.

---

## ⚙️ Features

- 🧩 Complete **ML-DSA44** implementation (based on PQClean)
- ⏱️ High-resolution timing using `esp_timer`
- 🔄 **FreeRTOS task isolation** for benchmarking and server operation
- 📶 Integrated **Wi-Fi STA mode** for networked signing
- 🧠 **Memory-efficient** (under 400 KB RAM usage)
- 🧰 Fully compatible with **ESP-IDF v5+**

---

## 🧱 Project Structure

```
/main
├── main.c                # Entry point
├── api.h                 # ML-DSA44 PQC API header
└── CMakeLists.txt
/docs
└── ML-DSA44_Expert_Documentation.md  # Full technical deep dive
```

---

## 🧠 Modes of Operation

| Mode | Define | Description |
|------|---------|-------------|
| **Benchmark Mode** | `#define RUN_WIFI_SERVER 0` | Runs key generation, signing, and verification benchmarks. |
| **Wi-Fi Signing Server** | `#define RUN_WIFI_SERVER 1` | Starts a TCP server on port 8000, listens for messages, and returns digital signatures. |

---

## 🧩 Function Overview

| Function | Description |
|-----------|-------------|
| `measure_task()` | Executes performance benchmarking for keygen, sign, and verify. |
| `listen_and_sign_task()` | Waits for TCP clients, signs incoming data, and returns the generated signature. |
| `wifi_init_sta()` | Configures and connects the ESP32 in Wi-Fi Station (STA) mode. |
| `app_main()` | Initializes NVS, sets up Wi-Fi, and selects the FreeRTOS task based on mode. |

---

## 📊 Sample Benchmark Output

```
--- ML-DSA Performance Measurement ---
KeyGen Time:  22251 us
Sign Time:    35991 us
Verify Time:  23629 us
Verification OK
Heap free: 370460 bytes
```

| Operation | Avg Time (ms) | Description |
|------------|---------------|-------------|
| KeyGen | 22.3 ms | Generates PQ keypair |
| Sign | 36.0 ms | Creates a digital signature |
| Verify | 23.6 ms | Verifies the authenticity of the signature |

---

## ⚡ Getting Started

### 🧰 Requirements

- ESP-IDF **v5.0+**
- Integrated **PQClean** or **ML-DSA** library (`api.h`)
- Compatible hardware: **ESP32 DevKitC / ESP32-WROOM / ESP32-S3**

### 🔧 Build & Flash

```
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

---

### 🌐 Running the Wi-Fi Signing Server

Edit `main.c`:

```
#define RUN_WIFI_SERVER 1
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"
```

Then flash and monitor:

```
idf.py build flash monitor
```

Once running, connect a TCP client to:

```
<ESP32_IP>:8000
```

Send any message to receive its **ML-DSA44 post-quantum signature**.

---

## 🧩 Future Improvements

- [ ] Add **ML-KEM (Kyber)** key exchange support  
- [ ] Implement **Python message verification client**  
- [ ] Support **MQTT / HTTPS** for large-scale IoT secure messaging  
- [ ] Optimize **heap memory** and parallel **task scheduling**  
- [ ] Integrate with **ESP Secure Boot** for firmware attestation  

---

## 🔬 Technical Documentation

A full technical deep dive — covering architecture, function details, and performance analysis — is available here:

👉 **[ML-DSA44 Expert Documentation →](../../wiki/ML-DSA44-Expert-Documentation)**

---

## 🧠 Acknowledgements

- [PQClean](https://github.com/pqclean/pqclean) — Clean implementations of PQC algorithms  
- **Espressif ESP-IDF Framework**  
- **NIST PQC Standardization** (FIPS 204: ML‑DSA)

---

## 📄 License

MIT License — see [LICENSE](LICENSE)

---

⭐ **Author:** Divyesh K.  
💡 *If this project helped you, consider starring the repository!*

***