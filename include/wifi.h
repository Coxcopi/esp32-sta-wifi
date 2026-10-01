#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

esp_err_t sta_wifi_init(void);
esp_err_t sta_wifi_connect(const char *ssid, const char *password);
esp_err_t sta_wifi_disconnect(void);
esp_err_t sta_wifi_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
