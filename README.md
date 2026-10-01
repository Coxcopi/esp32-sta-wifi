**Installation**
Add this to idf_components.yml
```yaml
dependencies:
  wifi:
    git: https://github.com/Coxcopi/esp32-sta-wifi.git
    version: main # or a specific version tag like 1.0.0
```
and run `idf.py reconfigure`.


**Example usage**
```C
// main.c

#include "freertos/FreeRTOS.h"
#include "esp_check.h"
#include "wifi.h"

constexpr const WIFI_SSID = "MyWiFiNetwork";
constexpr const WIFI_PSW = "supersecretpassword";

void app_main(void)
{
  ESP_ERROR_CHECK(wifi_init());
  ESP_ERROR_CHECK(wifi_connect(WIFI_SSID, WIFI_PSW));
}
```
