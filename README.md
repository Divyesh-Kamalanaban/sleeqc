

# SleeQC: Resource-Adaptive PQC on ESP32

**SleeQC** is a proof-of-concept project demonstrating a **resource-adaptive cryptography system** on an ESP32-S3. It uses a TinyML model to dynamically select the optimal Post-Quantum Cryptography (PQC) algorithm, balancing security with real-time resource availability.

This system intelligently decides whether to use the faster, less-intensive **ML-DSA-44 (Dilithium2)** or the more secure, resource-heavy **ML-DSA-87 (Dilithium5)** based on the device's current state.

-----

## 1\. Core Concept

**The Problem:** High-security Post-Quantum Cryptography algorithms like Dilithium5 are computationally expensive. On a resource-constrained IoT device like an ESP32, running these heavy algorithms can consume excessive CPU time and stack memory, potentially starving other critical tasks (like network handling or sensor reading).

**The Solution:** Instead of a fixed security level, this project implements a "controller" using a TinyML model. This model acts as a high-speed decision engine:

  * It monitors real-time system metrics (Free Heap, previous task duration).
  * It predicts whether the system is "idle" or "under load."
  * Based on this prediction, it dynamically selects the appropriate PQC algorithm for the next signing operation.

This creates an intelligent balance:

  * **System Idle?** Use the maximum security (Dilithium5).
  * **System Busy?** Use the "good enough" security (Dilithium2) to ensure stability and responsiveness.

-----

## 2\. Features

  * **Dynamic PQC Switching:** Automatically switches between `mldsa44` (Dilithium2) and `mldsa87` (Dilithium5).
  * **TinyML Decision Engine:** Uses a lightweight TensorFlow Lite model to make real-time predictions.
  * **Real-time Resource Monitoring:** Feeds free heap and task execution time into the model.
  * **Parallel PQC Implementation:** Includes complete, working implementations of two Dilithium levels as ESP-IDF components.
  * **Connectivity:** Initializes Wi-Fi in STA mode with a static IP for potential future IoT communication.
  * **RTOS-Based:** Built on FreeRTOS for task management.

-----

## 3\. How It Works: The Logic Flow

1.  **Boot & Connect:** The ESP32-S3 boots, initializes NVS, and connects to the hardcoded Wi-Fi network with a static IP.
2.  **Initialize ML:** The `TFLiteRunner` class is instantiated. It loads the `model_data.h` and prepares the TensorFlow Lite interpreter, registering all necessary ops (including `Logistic`).
3.  **Generate Keys:** `app_main` generates keypairs for *both* Dilithium2 and Dilithium5, storing them in static memory.
4.  **Start Worker Task:** `app_main` creates the `pqc_worker_task` with a **16KB stack** (essential to prevent overflows).
5.  **Adaptive Loop:** The `pqc_worker_task` enters an infinite loop:
    1.  **Sense:** It measures `free_heap` and fetches the `duration_ms` from the *previous* loop's signing operation.
    2.  **Predict:** It calls `ml_runner.predict()`, feeding in these metrics.
    3.  **Decide:** The model outputs a value between 0.0 and 1.0. A threshold (`> 0.5f`) converts this to a binary decision: `0` (Use Dilithium2) or `1` (Use Dilithium5).
    4.  **Act:** The chosen function (`D2_SIGN` or `D5_SIGN`) is called to sign a static message.
    5.  **Report:** The loop logs the results (Heap, new SignTime, and Stack HWM) to the serial monitor.
    6.  **Delay:** The task sleeps for 3 seconds before repeating.

-----

## 4\. Hardware & Software

### Hardware

  * **ESP32-S3** (Sufficient PSRAM and performance for PQC + ML).
  * Standard USB cable for flashing and monitoring.

### Software & Dependencies

  * **ESP-IDF v5.5.1**
  * **Custom Components:**
      * `mldsa44`: PQClean implementation of ML-DSA-44 (Dilithium2).
      * `mldsa87`: PQClean implementation of ML-DSA-87 (Dilithium5).
  * **Managed Components** (auto-downloaded via `idf.py build`):
      * `espressif/esp-tflite-micro`
      * `espressif/esp-nn`

-----

## 5\. Project Structure

This project uses a clean, component-based structure.

```
SleeQC/
├── CMakeLists.txt          <-- Root project file. *Must* set EXTRA_COMPONENT_DIRS.
├── components/
│   ├── mldsa44/
│   │   ├── include/        <-- Header files (.h)
│   │   │   └── mldsa44/
│   │   │       └── api.h
│   │   ├── src/            <-- Source files (.c)
│   │   └── CMakeLists.txt  <-- Registers component, sets SRCS_DIRS and INCLUDE_DIRS.
│   └── mldsa87/
│       ├── include/
│       │   └── mldsa87/
│       │       └── api.h
│       ├── src/
│       └── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt      <-- Main component. *Must* have PRIV_REQUIRES.
│   ├── main.cpp            <-- Main application logic (app_main, pqc_worker_task).
│   ├── tflite_runner.h     <-- C++ class to manage the TFLite interpreter.
│   └── model_data.h        <-- TFLite model converted to a C array.
└── sdkconfig               <-- Project configuration.
```

-----

## 6\. How to Build & Flash

1.  **Clone Repository:**

    ```bash
    git clone [\[https://github.com/Divyesh-Kamalanaban/sleeqc\]](https://github.com/Divyesh-Kamalanaban/sleeqc.git)
    cd SleeQC
    ```

2.  **Set up ESP-IDF:**
    Ensure you have the ESP-IDF v5.5.1 environment activated.

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3.  **Clean & Build:**
    It's critical to start with a clean configuration to avoid caching issues.

    ```bash
    idf.py fullclean
    idf.py build
    ```

    *(Alternatively, manually delete the `build` directory and then run `idf.py build`)*

4.  **Flash & Monitor:**
    Connect your ESP32-S3 and run:

    ```bash
    idf.py flash monitor
    ```

-----

## 7\. Example Output

A successful run will show the system booting, connecting to Wi-Fi, and then starting the adaptive loop. Notice how it correctly chooses Dilithium2 on the first run (when `duration_ms` is 0.0) and then switches to the more secure Dilithium5 once it confirms the system is stable.

```
... (boot sequence) ...
I (546) PQC_RUNTIME: WiFi initialized with static IP 192.168.1.50
I (546) TFLITE: TFLite model loaded. Input dims: 2, Output dims: 2
I (636) PQC_RUNTIME: Generated Dilithium2 and Dilithium5 keypairs

I (636) TFLITE: ML predicted: 0.05 → Using Dilithium2
I (716) PQC_RUNTIME: 🔒 Used DilithNium2 for signing
I (716) PQC_RUNTIME: FreeHeap: 222.0 KB | SignTime: 79.15 ms | StackHWM: 7248

I (3716) TFLITE: ML predicted: 1.00 → Using Dilithium5
I (3876) PQC_RUNTIME: 🔒 Used Dilithium5 for signing
I (3876) PQC_RUNTIME: FreeHeap: 226.0 KB | SignTime: 166.41 ms | StackHWM: 7248

I (6876) TFLITE: ML predicted: 1.00 → Using Dilithium5
I (6986) PQC_RUNTIME: 🔒 Used Dilithium5 for signing
I (6986) PQC_RUNTIME: FreeHeap: 226.0 KB | SignTime: 118.04 ms | StackHWM: 7248
...
```

The **`StackHWM: 7248`** log shows that out of the 16384 bytes allocated, 7248 bytes were free at peak, meaning the task used **9136 bytes** of stack. This confirms the 8KB stack was insufficient.

-----

## 8\. Key Troubleshooting & Fixes

This project required solving several complex integration problems:

1.  **C/C++ Linker Errors (`undefined reference to ...`):**

      * **Problem:** The C++ `main.cpp` file could not link against the compiled C libraries from the PQC components.
      * **Fix:** Wrapped the PQC header includes in `main.cpp` with `extern "C" { ... }` to prevent C++ name mangling.

2.  **Component Linker Errors (Build Failure):**

      * **Problem:** The `main` component was not linked with the `mldsa44` and `mldsa87` components, even though the headers were found.
      * **Fix:** Added `PRIV_REQUIRES mldsa44 mldsa87` to the `main/CMakeLists.txt` file.

3.  **Compiler Errors (`mldsa44/api.h: No such file`):**

      * **Problem:** The build system didn't know where to find the custom components or their headers.
      * **Fix:**
        1.  Added `set(EXTRA_COMPONENT_DIRS components)` to the **root** `CMakeLists.txt`.
        2.  Added `INCLUDE_DIRS "include"` to each component's `CMakeLists.txt`.

4.  **TFLite Crash (`LoadProhibited`):**

      * **Problem:** The TFLite model used the `LOGISTIC` (Sigmoid) operation, but the TFLite interpreter didn't have this op loaded.
      * **Fix:** Added `resolver.AddLogistic();` to the `TFLiteRunner::init()` function in `tflite_runner.h`.

5.  **Stack Overflow (`***ERROR*** A stack overflow...`):**

      * **Problem:** The PQC signing functions are extremely stack-intensive and were overflowing the default 8KB task stack.
      * **Fix:** Increased the stack size for `pqc_worker_task` to `16384` bytes in the `xTaskCreate` call.

-----

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.