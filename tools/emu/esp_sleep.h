// Stub del emulador: la suspension en deep sleep del port 1.75C no existe en el PC.
#pragma once
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_EXT0 = 2 } esp_sleep_wakeup_cause_t;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
inline int esp_sleep_enable_ext0_wakeup(int, int) { return 0; }
inline void esp_deep_sleep_start() {}
