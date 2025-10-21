#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "api.h"  // MLDSA44 API header (from PQClean or your PQC lib)

#define TAG "CRYPTO_APP"

// ========================== CONFIGURATION ==========================
#define MAX_PACKET_SIZE 10000
#define LISTEN_PORT 8000
#define WIFI_SSID      "DIVYPC5070"      // <-- Replace
#define WIFI_PASSWORD  "3921XX2z"  // <-- Replace
#define RUN_WIFI_SERVER 0               // 1: Wi-Fi signing server, 0: benchmark
// ===================================================================

// ========== Wi-Fi Event Handling ==========
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi init done, waiting for connection...");

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    if ((bits & WIFI_CONNECTED_BIT) == 0)
        ESP_LOGW(TAG, "Wi-Fi connection timeout!");
    else
        ESP_LOGI(TAG, "Wi-Fi connected successfully!");
}

// ========== Helpers ==========

static int send_all(int sock, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        int r = send(sock, buf + sent, len - sent, 0);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += r;
    }
    return sent;
}

static ssize_t recv_once(int sock, uint8_t *buf, size_t maxlen)
{
    ssize_t r;
    do
    {
        r = recv(sock, buf, maxlen, 0);
    } while (r < 0 && errno == EINTR);
    return r;
}

// ========== ML-DSA Performance Benchmark ==========
static void measure_task(void *param)
{
    printf("\n--- ML-DSA Performance Measurement ---\n");

    uint8_t *pk = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES);
    uint8_t *sk = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES);
    uint8_t *sig = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES);

    if (!pk || !sk || !sig)
    {
        printf("Memory allocation failed!\n");
        goto cleanup;
    }

    // Keygen
    int64_t t0 = esp_timer_get_time();
    PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair(pk, sk);
    int64_t t1 = esp_timer_get_time();
    printf("KeyGen Time:  %lld us\n", (long long)(t1 - t0));

    // Sign
    const uint8_t msg[] = "Hello from ESP32!";
    size_t siglen;
    t0 = esp_timer_get_time();
    PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature(sig, &siglen, msg, sizeof(msg) - 1, sk);
    t1 = esp_timer_get_time();
    printf("Sign Time:    %lld us\n", (long long)(t1 - t0));

    // Verify
    t0 = esp_timer_get_time();
    int ok = PQCLEAN_MLDSA44_CLEAN_crypto_sign_verify(sig, siglen, msg, sizeof(msg) - 1, pk);
    t1 = esp_timer_get_time();
    printf("Verify Time:  %lld us\n", (long long)(t1 - t0));
    printf(ok == 0 ? "Verification OK\n" : "Verification FAILED\n");

    printf("Heap free: %ld bytes\n", esp_get_free_heap_size());

cleanup:
    if (sk) memset(sk, 0, PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES);
    free(pk);
    free(sk);
    free(sig);
    vTaskDelete(NULL);
}

// ========== Wi-Fi Signing Server ==========
static void listen_and_sign_task(void *param)
{
    // Generate ML-DSA keypair once
    uint8_t *pk = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_PUBLICKEYBYTES);
    uint8_t *sk = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES);

    if (!pk || !sk || PQCLEAN_MLDSA44_CLEAN_crypto_sign_keypair(pk, sk))
    {
        ESP_LOGE(TAG, "Keypair generation failed");
        free(pk);
        free(sk);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "ML-DSA keypair generated");

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Socket creation failed: %d", errno);
        goto cleanup_keys;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "Bind failed: %d", errno);
        goto cleanup_sock;
    }

    if (listen(sock, 1) < 0)
    {
        ESP_LOGE(TAG, "Listen failed: %d", errno);
        goto cleanup_sock;
    }

    ESP_LOGI(TAG, "Listening on port %d", LISTEN_PORT);

    while (1)
    {
        int csock = accept(sock, NULL, NULL);
        if (csock < 0)
        {
            ESP_LOGE(TAG, "Accept failed: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ESP_LOGI(TAG, "Client connected");

        uint8_t *incoming = malloc(MAX_PACKET_SIZE);
        if (!incoming)
        {
            ESP_LOGE(TAG, "malloc failed for incoming data");
            close(csock);
            continue;
        }

        ssize_t r = recv_once(csock, incoming, MAX_PACKET_SIZE);
        if (r <= 0)
        {
            ESP_LOGE(TAG, "recv failed or closed (%d)", (int)r);
            free(incoming);
            close(csock);
            continue;
        }

        ESP_LOGI(TAG, "Received %d bytes", (int)r);

        uint8_t *sig = malloc(PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES);
        size_t siglen = 0;

        if (!sig)
        {
            ESP_LOGE(TAG, "malloc failed for signature");
            free(incoming);
            close(csock);
            continue;
        }

        if (PQCLEAN_MLDSA44_CLEAN_crypto_sign_signature(sig, &siglen, incoming, (size_t)r, sk) == 0)
        {
            send_all(csock, sig, siglen);
            ESP_LOGI(TAG, "Sent signature (%zu bytes)", siglen);
        }
        else
        {
            ESP_LOGE(TAG, "Signing failed");
        }

        memset(sig, 0, PQCLEAN_MLDSA44_CLEAN_CRYPTO_BYTES);
        free(sig);
        free(incoming);
        shutdown(csock, SHUT_RDWR);
        close(csock);
        ESP_LOGI(TAG, "Client disconnected");
    }

cleanup_sock:
    close(sock);
cleanup_keys:
    memset(sk, 0, PQCLEAN_MLDSA44_CLEAN_CRYPTO_SECRETKEYBYTES);
    free(sk);
    free(pk);
    vTaskDelete(NULL);
}

// ========== Main Entry ==========
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if RUN_WIFI_SERVER
    ESP_LOGI(TAG, "Starting Wi-Fi Signing Server...");
    wifi_init_sta();
    xTaskCreate(listen_and_sign_task, "SignServer", 8192, NULL, 5, NULL);
#else
    ESP_LOGI(TAG, "Starting ML-DSA Benchmark...");
    xTaskCreate(measure_task, "MeasureTask", 8192, NULL, 5, NULL);
#endif
}
