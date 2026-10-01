#include "wifi.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "wifi";

static const int WIFI_MAX_RETRIES = 10;

static const EventBits_t WIFI_CONNECTED_BIT = BIT0;
static const EventBits_t WIFI_FAIL_BIT = BIT1;

static esp_netif_t *netif = NULL;
static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_handler;
static EventGroupHandle_t s_wifi_event_group = NULL;

static int wifi_retry_count = 0;

static void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
    {
        ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IPv4: " IPSTR, IP2STR(&event_ip->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    }
    case IP_EVENT_GOT_IP6:
    {
        ip_event_got_ip6_t *event_ip6 = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(event_ip6->ip6_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    }
    case IP_EVENT_STA_LOST_IP:
        ESP_LOGI(TAG, "Lost IP");
        break;
    default:
        break;
    }
}

static void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_WIFI_READY:
        ESP_LOGI(TAG, "WiFi ready");
        break;
    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "WiFi scan finished");
        break;
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WiFi connected");
        break;
    case WIFI_EVENT_STA_STOP:
        ESP_LOGI(TAG, "WiFi stopped");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WiFi disconnected");
        if (wifi_retry_count < WIFI_MAX_RETRIES)
        {
            ESP_LOGI(TAG, "Attempting to re-connect WiFi (%i/%i)...", ++wifi_retry_count, WIFI_MAX_RETRIES);
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect to WiFi after %i attempts", wifi_retry_count);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        break;
    case WIFI_EVENT_STA_AUTHMODE_CHANGE:
        ESP_LOGI(TAG, "WiFi auth mode changed");
        break;
    default:
        break;
    }
}

esp_err_t sta_wifi_init()
{
    // Init non-volatile storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_RETURN_ON_ERROR(
            nvs_flash_erase(),
            TAG,
            "Failed to erase nvs flash");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(
        err,
        TAG,
        "Failed to init NVS flash");

    s_wifi_event_group = xEventGroupCreate();

    // Init TCP/IP network stack
    ESP_RETURN_ON_ERROR(
        esp_netif_init(),
        TAG,
        "Failed to initialize TCP/IP network stack");

    // Create default event loop
    ESP_RETURN_ON_ERROR(
        esp_event_loop_create_default(),
        TAG,
        "Failed to create default event loop");

    // Set default WiFi STA handlers
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_default_wifi_sta_handlers(),
        TAG,
        "Failed to set default handlers");

    // Create default WiFi STA interface
    netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default WiFi sta interface");
        return ESP_FAIL;
    }

    // Configure wifi stack
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_wifi_init(&cfg),
        TAG,
        "Failed to init wifi with default config");

    // Register event handlers
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_cb,
            NULL,
            &wifi_event_handler),
        TAG,
        "Failed to register wifi event handler");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            ESP_EVENT_ANY_ID,
            &ip_event_cb,
            NULL,
            &ip_event_handler),
        TAG,
        "Failed to register ip event handler");

    return ESP_OK;
}

esp_err_t sta_wifi_connect(const char *ssid, const char *password)
{
    wifi_config_t cfg = {0};
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    strlcpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_ps(WIFI_PS_NONE),
        TAG,
        "Failed to set wifi power save mode");

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_storage(WIFI_STORAGE_RAM),
        TAG,
        "Failed to set wifi storage mode");

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode(WIFI_MODE_STA),
        TAG,
        "Failed to set wifi to STA mode");

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_STA, &cfg),
        TAG,
        "Failed to set wifi config");

    ESP_LOGI(TAG, "Connecting to WiFi network %s", (char *)cfg.sta.ssid);

    ESP_RETURN_ON_ERROR(
        esp_wifi_start(),
        TAG,
        "Failed to start wifi");

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_max_tx_power(8.5 * 4),
        TAG,
        "Failed to lower max wifi tx power to 8.5 dBm");

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "WiFi connected successfully");
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "Unexpected WiFi error");
    return ESP_FAIL;
}

esp_err_t sta_wifi_disconnect()
{
    xEventGroupClearBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    return esp_wifi_disconnect();
}

esp_err_t sta_wifi_deinit()
{
    ESP_RETURN_ON_ERROR(
        esp_wifi_stop(),
        TAG,
        "Failed to stop wifi");
    ESP_RETURN_ON_ERROR(
        esp_wifi_deinit(),
        TAG,
        "Failed to de-init wifi");
    ESP_RETURN_ON_ERROR(
        esp_wifi_clear_default_wifi_driver_and_handlers(netif),
        TAG,
        "Failed to clear IF driver and handlers");
    esp_netif_destroy(netif);
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler),
        TAG,
        "Failed to unregister ip event handler");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler),
        TAG,
        "Failed to unregister wifi event handler");
    return ESP_OK;
}
