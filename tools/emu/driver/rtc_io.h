// Stub del emulador: pull-up RTC de la tecla PWR durante la suspension (port 1.75C).
#pragma once
#include "driver/gpio.h"
inline int rtc_gpio_pullup_en(gpio_num_t) { return 0; }
inline int rtc_gpio_pulldown_dis(gpio_num_t) { return 0; }
inline int rtc_gpio_deinit(gpio_num_t) { return 0; }
