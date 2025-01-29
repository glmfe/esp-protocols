/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ESP libwebsockets server example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// TODO: add debug info. used hen connected and trade data.

#include <libwebsockets.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h" // TODO: Remove?
#include "freertos/semphr.h" // TODO: Remove?
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"

#define RING_DEPTH 4096
#define LWS_MAX_PAYLOAD 1024  // TODO: Remove? 

static int callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                        void *user, void *in, size_t len);
/* one of these created for each message */

static TaskHandle_t wifi_wdt_task_handle = NULL;

struct msg {
    void *payload; /* is malloc'd */
    size_t len;
};

/* one of these is created for each client connecting to us */

struct per_session_data__minimal {
    struct per_session_data__minimal *pss_list;
    struct lws *wsi;
    int last; /* the last message number we sent */
    unsigned char buffer[RING_DEPTH]; // TODO: Remove?
    size_t buffer_len; // TODO: Remove?
    int is_receiving_fragments; // TODO: Remove?
    int is_ready_to_send; // TODO: Remove?
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__minimal {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__minimal *pss_list; /* linked-list of live pss*/

    struct msg amsg; /* the one pending message... */
    int current; /* the current message number we are caching */
};

static struct lws_protocols protocols[] = {
    {
        .name = "lws-minimal-server-echo",
        .callback = callback_minimal_server_echo,
        .per_session_data_size = sizeof(struct per_session_data__minimal),
        .rx_buffer_size = RING_DEPTH, // TODO: Was 128
        .id = 0,
        .user = NULL,
        .tx_packet_size = 8192 // TODO: Was 0
    },
    LWS_PROTOCOL_LIST_TERM
};

static int options;
static const char *TAG = "lws-server-echo", *iface = "";

/* pass pointers to shared vars to the protocol */
static const struct lws_protocol_vhost_options pvo_options = {
    NULL,
    NULL,
    "options",      /* pvo name */
    (void *) &options   /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
    &pvo_options,
    NULL,
    "interrupted",      /* pvo name */
    NULL    /* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
    NULL,               /* "next" pvo linked-list */
    &pvo_interrupted,       /* "child" pvo linked-list */
    "lws-minimal-server-echo",  /* protocol name we belong to on this vhost */
    ""              /* ignored */
};

// Task WDT Wifi
void wifi_wdt_task(void *pvParameters)
{
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    esp_task_wdt_add(self); // Add Task

    while (1)
    {
        esp_task_wdt_reset(); // TODO: Check later.
        vTaskDelay(pdMS_TO_TICKS(1000)); // TODO: Check later.
    }
}

void print_esp32_ip()
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "ESP32_IP: %s", ip4addr_ntoa((const ip4_addr_t *)&ip_info.ip.addr));
    } else {
        ESP_LOGE(TAG, "Erro ao obter o endereço IP do ESP32");
    }
}

int app_main(int argc, const char **argv)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Show IP */
    print_esp32_ip();

    /* Configure WDT. */
    esp_task_wdt_config_t esp_task_wdt_config = {
        .idle_core_mask = 0,
        .timeout_ms = 5000, // TODO: was: portMAX_DELAY
        .trigger_panic = false
    };
    esp_task_wdt_reconfigure(&esp_task_wdt_config); // WO this line, wdt will be activate with 300ms

    // TaskHandle_t handle = xTaskGetCurrentTaskHandle();  // Check it, create by Gui
    // esp_task_wdt_add(handle);
    esp_task_wdt_add(NULL);

    // TODO: Remove?
    // Dedicated task to reset the WDT on the same core as Wi-Fi... Check impact later.
    if (xTaskCreatePinnedToCore(wifi_wdt_task, "wifi_wdt_task", 2048, NULL, 5, &wifi_wdt_task_handle, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Error creating wifi_wdt_task! Retrying...");
        vTaskDelay(pdMS_TO_TICKS(100));
        if (xTaskCreatePinnedToCore(wifi_wdt_task, "wifi_wdt_task", 2048, NULL, 5, &wifi_wdt_task_handle, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Critical error: Failed to create wifi_wdt_task. The Watchdog may fail.");
        }
    }

    /* Create LWS Context - Server. */
    struct lws_context_creation_info info;
    struct lws_context *context;
    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    lws_set_log_level(logs, NULL);
    ESP_LOGI(TAG, "LWS minimal ws server echo\n");

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = CONFIG_WEBSOCKET_PORT;
    info.iface = iface;
    info.protocols = protocols;
    info.pvo = &pvo;
    info.pt_serv_buf_size = 64 * 1024;

#ifdef CONFIG_WS_OVER_TLS
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    /* Configuring server certificates for mutual authentification */
    extern const char cert_start[] asm("_binary_server_cert_pem_start");    // Server certificate
    extern const char cert_end[]   asm("_binary_server_cert_pem_end");
    extern const char key_start[] asm("_binary_server_key_pem_start");      // Server private key
    extern const char key_end[]   asm("_binary_server_key_pem_end");
    extern const char cacert_start[] asm("_binary_ca_cert_pem_start");      // CA certificate
    extern const char cacert_end[] asm("_binary_ca_cert_pem_end");

    info.server_ssl_cert_mem            = cert_start;
    info.server_ssl_cert_mem_len        = cert_end - cert_start - 1;
    info.server_ssl_private_key_mem     = key_start;
    info.server_ssl_private_key_mem_len = key_end - key_start - 1;
    info.server_ssl_ca_mem              = cacert_start;
    info.server_ssl_ca_mem_len          = cacert_end - cacert_start;
#endif

    context = lws_create_context(&info);
    if (!context) {
        ESP_LOGE(TAG, "lws init failed\n");
        return 1;
    }

    while (n >= 0)
    {
        int64_t start_time = esp_timer_get_time();
        n = lws_service(context, 100); //TODO: Check

        int64_t elapsed_time = esp_timer_get_time() - start_time;
        if (elapsed_time > 200000)
        {
            ESP_LOGW(TAG, "lws_service took too long: %" PRId64 " us", elapsed_time);
        }

        esp_task_wdt_reset(); // TODO: Remove later?
    }

    lws_context_destroy(context);

    return 0;
}

static void __minimal_destroy_message(void *_msg)
{
    struct msg *msg = _msg;

    free(msg->payload);
    msg->payload = NULL;
    msg->len = 0;
}

static int callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                        void *user, void *in, size_t len)
{
    struct per_session_data__minimal *pss = (struct per_session_data__minimal *)user;
    struct per_vhost_data__minimal *vhd = (struct per_vhost_data__minimal *)
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));
    char client_address[128];

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                                          lws_get_protocol(wsi),
                                          sizeof(struct per_vhost_data__minimal));
        if (!vhd) {
            ESP_LOGE("LWS_SERVER", "Failed to allocate vhost data.");
            return -1;
        }
        vhd->context = lws_get_context(wsi);
        vhd->protocol = lws_get_protocol(wsi);
        vhd->vhost = lws_get_vhost(wsi);
        vhd->current = 0;
        vhd->amsg.payload = NULL;
        vhd->amsg.len = 0;
        break;

    case LWS_CALLBACK_ESTABLISHED:
        lws_get_peer_simple(wsi, client_address, sizeof(client_address));
        ESP_LOGI("LWS_SERVER", "New client connected: %s", client_address);
        lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
        pss->wsi = wsi;
        pss->last = vhd->current;
        pss->buffer_len = 0;
        pss->is_receiving_fragments = 0;
        pss->is_ready_to_send = 0;
        memset(pss->buffer, 0, RING_DEPTH);
        break;

    case LWS_CALLBACK_CLOSED:
        lws_get_peer_simple(wsi, client_address, sizeof(client_address));
        ESP_LOGI("LWS_SERVER", "Client disconnected: %s", client_address);
        lws_ll_fwd_remove(struct per_session_data__minimal, pss_list, pss, vhd->pss_list);
        break;

    case LWS_CALLBACK_RECEIVE:
        lws_get_peer_simple(wsi, client_address, sizeof(client_address));

        // Identificar se é binário ou texto
        bool is_binary = lws_frame_is_binary(wsi);

        ESP_LOGI("LWS_SERVER", "%s fragment received from %s (%d bytes)",
                is_binary ? "Binary" : "Text", client_address, (int)len);

        // Primeiro fragmento: reinicia o buffer
        if (lws_is_first_fragment(wsi)) {
            pss->buffer_len = 0;
        }

        // Acumular fragmentos no buffer
        if (pss->buffer_len + len < RING_DEPTH) {
            memcpy(pss->buffer + pss->buffer_len, in, len);
            pss->buffer_len += len;
        } else {
            ESP_LOGE("LWS_SERVER", "Fragmented message exceeded buffer limit.");
            return -1;
        }

        // Se for a última parte do fragmento, processar a mensagem completa
        if (lws_is_final_fragment(wsi)) {
            ESP_LOGI("LWS_SERVER", "Complete %s message received from %s (%d bytes)",
                    is_binary ? "binary" : "text", client_address, (int)pss->buffer_len);

            if (!is_binary) {
                // Exibir mensagem completa se for texto
                ESP_LOGI("LWS_SERVER", "Full text message: %.*s", (int)pss->buffer_len, (char *)pss->buffer);
            } else {
                // Exibir mensagem binária como hexadecimal
                char hex_output[pss->buffer_len * 2 + 1];
                for (int i = 0; i < pss->buffer_len; i++) {
                    snprintf(&hex_output[i * 2], 3, "%02X", pss->buffer[i]);
                }
                ESP_LOGI("LWS_SERVER", "Full binary message (hex): %s", hex_output);
            }

            // Responder ao cliente
            int write_type = is_binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT;
            int m = lws_write(wsi, (unsigned char *)pss->buffer, pss->buffer_len, write_type);
            pss->buffer_len = 0;

            if (m < (int)pss->buffer_len) {
                ESP_LOGE("LWS_SERVER", "Failed to send %s message.", is_binary ? "binary" : "text");
                return -1;
            }
            break;
        }

        // **Caso a mensagem não seja fragmentada, processar JSON, echo e outras mensagens normais**
        
        // JSON
        if (strstr((char *)in, "{") && strstr((char *)in, "}")) {
            ESP_LOGI("LWS_SERVER", "JSON message received from %s: %.*s", client_address, (int)len, (char *)in);
            int m = lws_write(wsi, (unsigned char *)in, len, LWS_WRITE_TEXT);
            if (m < (int)len) {
                ESP_LOGE("LWS_SERVER", "Failed to send JSON message.");
                return -1;
            }
            break;
        }

        // Texto simples (Echo)
        if (!is_binary) {
            ESP_LOGI("LWS_SERVER", "Text message received from %s (%d bytes): %.*s",
                    client_address, (int)len, (int)len, (char *)in);
            int m = lws_write(wsi, (unsigned char *)in, len, LWS_WRITE_TEXT);
            if (m < (int)len) {
                ESP_LOGE("LWS_SERVER", "Failed to send text message.");
                return -1;
            }
            break;
        }

        // Binário único
        ESP_LOGI("LWS_SERVER", "Binary message received from %s (%d bytes)", client_address, (int)len);
        int m = lws_write(wsi, (unsigned char *)in, len, LWS_WRITE_BINARY);
        if (m < (int)len) {
            ESP_LOGE("LWS_SERVER", "Failed to send binary message.");
            return -1;
        }
        break;

    default:
        break;
    }

    return 0;
}