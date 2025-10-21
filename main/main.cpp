#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_mac.h" // For newer ESP-IDF versions

// Include both PQC implementations
// Include both PQC implementations
extern "C" {
#include "mldsa44/api.h"
#include "mldsa87/api.h"
}
// TinyML Runner
#include "tflite_runner.h" 

#define PORT 8080
#define TAG "PQC_RUNTIME"

// Aliases for Dilithium2
#define D2_KEYPAIR  PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair
#define D2_SIGN     PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature
#define D2_PUBBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES
#define D2_SECBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES
#define D2_SIGBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES

// Aliases for Dilithium5
#define D5_KEYPAIR  PQCLEAN_MLDSA87_CLEAN_crypto_sign_keypair
#define D5_SIGN     PQCLEAN_MLDSA87_CLEAN_crypto_sign_signature
#define D5_PUBBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_PUBLICKEYBYTES
#define D5_SECBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_SECRETKEYBYTES
#define D5_SIGBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_BYTES

// Static IP configuration
#define STATIC_IP_ADDR  "192.168.1.xx"
#define GATEWAY_ADDR    "192.168.x.x"
#define NETMASK_ADDR    "255.255.255.x"

// --- Global instance of the TFLite Runner ---
TFLiteRunner ml_runner;

// PQC keys
static uint8_t pk2[D2_PUBBYTES], sk2[D2_SECBYTES];
static uint8_t pk5[D5_PUBBYTES], sk5[D5_SECBYTES];


// ⚙️ Initialize Wi-Fi in Station mode with static IP
void wifi_init_static_ip(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();

    esp_netif_ip_info_t ip_info;
    inet_pton(AF_INET, STATIC_IP_ADDR, &ip_info.ip);
    inet_pton(AF_INET, GATEWAY_ADDR, &ip_info.gw);
    inet_pton(AF_INET, NETMASK_ADDR, &ip_info.netmask);
    esp_netif_dhcpc_stop(sta);
    esp_netif_set_ip_info(sta, &ip_info);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Your SSID",
            .password = "Your Password",
        },
    };
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi initialized with static IP %s", STATIC_IP_ADDR);
}

// --- Redundant ml_model_init() and ml_predict() functions removed ---


// 🔐 Main PQC worker
void pqc_worker_task(void *pvParams) {
    uint8_t message[] = "Resource adaptive PQC signing test";
    uint8_t sig5[D5_SIGBYTES], sig2[D2_SIGBYTES];
    size_t siglen;
    uint64_t start_time, end_time;
    float duration_ms = 0.0f; // Store duration for next loop's prediction

    while (1) {
        size_t free_heap = esp_get_free_heap_size() / 1024;

        // Predict using ONLY heap and the *previous* loop's sign time.
        // Stack HWM is not a useful *input* here.
        int use_d5 = ml_runner.predict((float)free_heap, duration_ms, 0.0f);
        
        start_time = esp_timer_get_time();
        if (use_d5) {
            D5_SIGN(sig5, &siglen, message, sizeof(message), sk5);
            ESP_LOGI(TAG, "🔒 Used Dilithium5 for signing");
        } else {
            D2_SIGN(sig2, &siglen, message, sizeof(message), sk2);
            ESP_LOGI(TAG, "🔒 Used DilithNium2 for signing");
        }
        end_time = esp_timer_get_time();

        // --- THIS IS THE PROPER WAY TO MEASURE ---
        duration_ms = (end_time - start_time) / 1000.0f;
        UBaseType_t stack_hwm_after = uxTaskGetStackHighWaterMark(NULL); // Get HWM *after* the work

        ESP_LOGI(TAG, "FreeHeap: %.1f KB | SignTime: %.2f ms | StackHWM: %lu",
                 (float)free_heap, duration_ms, stack_hwm_after);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// 🧩 Main Entry Point
extern "C" void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_static_ip();
    
    // Initialize the ML runner
    ml_runner.init();

    // Generate both keypairs
    D2_KEYPAIR(pk2, sk2);
    D5_KEYPAIR(pk5, sk5);

    ESP_LOGI(TAG, "Generated Dilithium2 and Dilithium5 keypairs");

    // Start PQC worker
    xTaskCreate(pqc_worker_task, "pqc_worker", 16384, NULL, 5, NULL);
}