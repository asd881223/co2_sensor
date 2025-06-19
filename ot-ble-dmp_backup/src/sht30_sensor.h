// ===== sht30_sensor.h =====
#ifndef SHT30_SENSOR_H
#define SHT30_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "co2_sensor.h"   // for delay_ms

// 初始化 SHT30: I2C 已由 sl_system_init() 初始化
void sht30_init(void);

// 讀取一次溫度 (°C) 與相對濕度 (%RH)
bool sht30_read(float *cTemp, float *humidity);

#endif // SHT30_SENSOR_H
