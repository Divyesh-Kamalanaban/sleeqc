#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <sstream>
#include <iomanip>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_mac.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

// PQC includes
extern "C" {
#include "mldsa44/api.h"
#include "mldsa87/api.h"
}

// TinyML model runner
#include "tflite_runner.h"

#define PORT 8080
#define TAG "PQC_SERVER"
#define RECV_BUFFER_SIZE 1024

// --- Dilithium2 (ML-DSA-44) ---
#define D2_KEYPAIR  PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair
#define D2_SIGN     PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature
#define D2_PUBBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES
#define D2_SECBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES
#define D2_SIGBYTES PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES

// --- Dilithium5 (ML-DSA-87) ---
#define D5_KEYPAIR  PQCLEAN_MLDSA87_CLEAN_crypto_sign_keypair
#define D5_SIGN     PQCLEAN_MLDSA87_CLEAN_crypto_sign_signature
#define D5_PUBBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_PUBLICKEYBYTES
#define D5_SECBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_SECRETKEYBYTES
#define D5_SIGBYTES PQCLEAN_MLDSA87_CLEAN_CRYPTO_BYTES

// --- Global TinyML Runner ---
TFLiteRunner ml_runner;

// --- Global PQC keys ---
static uint8_t pk2[D2_PUBBYTES], sk2[D2_SECBYTES];
static uint8_t pk5[D5_PUBBYTES], sk5[D5_SECBYTES];

// --- Wi-Fi Event Group ---
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

/**
 * @brief Converts bytes to hex string.
 */
std::string bytes_to_hex_cpp(const uint8_t *bytes, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

/**
 * @brief Wi-Fi event handler.
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize Wi-Fi (Station mode, DHCP)
 */
void wifi_init_dhcp(void) {
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "divyeshhotspot",
            .password = "divyesh123",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization complete.");
}

/**
 * @brief TCP server for PQC signing.
 */
void tcp_server_task(void *pvParams) {
    char rx_buffer[RECV_BUFFER_SIZE];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    float duration_ms = 0.0f;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created, binding...");
    bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    listen(listen_sock, 1);

    while (1) {
        ESP_LOGI(TAG, "Listening for client...");
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Accept failed");
            break;
        }

        inet_ntoa_r(source_addr.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Client connected: %s", addr_str);

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len <= 0) {
            ESP_LOGW(TAG, "No data received or connection closed");
            close(sock);
            continue;
        }

        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received %d bytes", len);

        // --- ML-Adaptive PQC Logic ---
        uint8_t sig[D5_SIGBYTES];
        size_t siglen = 0;
        uint64_t start_time, end_time;
        const char* algo_used = nullptr;

        size_t free_heap = esp_get_free_heap_size() / 1024;
        int use_d5 = ml_runner.predict((float)free_heap, duration_ms, 0.0f);

        start_time = esp_timer_get_time();

        if (use_d5) {
            D5_SIGN(sig, &siglen, (uint8_t*)rx_buffer, len, sk5);
            algo_used = "ML-DSA-87 (D5)";
            ESP_LOGI(TAG, "🔒 Used Dilithium5 for signing");
        } else {
            D2_SIGN(sig, &siglen, (uint8_t*)rx_buffer, len, sk2);
            algo_used = "ML-DSA-44 (D2)";
            ESP_LOGI(TAG, "🔒 Used Dilithium2 for signing");
        }

        end_time = esp_timer_get_time();
        duration_ms = (end_time - start_time) / 1000.0f;

        ESP_LOGI(TAG, "SignTime: %.2f ms | FreeHeap: %.1f KB", duration_ms, (float)free_heap);

        // --- Send Response ---
        std::stringstream ss_header;
        ss_header << "ALG:" << algo_used
                  << "|TIME_MS:" << std::fixed << std::setprecision(2) << duration_ms
                  << "|SIG:";
        std::string header_str = ss_header.str();
        send(sock, header_str.c_str(), header_str.length(), 0);

        std::string hex_sig = bytes_to_hex_cpp(sig, siglen);
        send(sock, hex_sig.c_str(), hex_sig.length(), 0);

        const char* pk_header = "|PK:";
        send(sock, pk_header, strlen(pk_header), 0);

        std::string hex_pk;
        if (use_d5) {
            hex_pk = bytes_to_hex_cpp(pk5, D5_PUBBYTES);
            ESP_LOGI(TAG, "Sent public key (D5) len=%d bytes", D5_PUBBYTES);
        } else {
            hex_pk = bytes_to_hex_cpp(pk2, D2_PUBBYTES);
            ESP_LOGI(TAG, "Sent public key (D2) len=%d bytes", D2_PUBBYTES);
        }
        send(sock, hex_pk.c_str(), hex_pk.length(), 0);

        ESP_LOGI(TAG, "✅ Response sent successfully (%s)", algo_used);

        shutdown(sock, 0);
        close(sock);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

/**
 * @brief App entry point.
 */
extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_dhcp();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected.");
        ml_runner.init();

        ESP_LOGI(TAG, "Generating keypairs...");
        D2_KEYPAIR(pk2, sk2);
        D5_KEYPAIR(pk5, sk5);
        ESP_LOGI(TAG, "Keypairs ready.");

        xTaskCreate(tcp_server_task, "tcp_server", 36864, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed.");
    }
}
