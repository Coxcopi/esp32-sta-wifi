#pragma once

#include "esp_event.h"
#include "esp_err.h"

esp_err_t wifi_init();
esp_err_t wifi_connect(const char* ssid, const char* password);
esp_err_t wifi_disconnect();
esp_err_t wifi_deinit();

void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
