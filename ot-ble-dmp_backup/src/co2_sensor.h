// ===== co2_sensor.h =====
#ifndef CO2_SENSOR_H
#define CO2_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_sleeptimer.h"

// 初始化感測器（上電、暖機、喚醒、設定連續模式）
bool co2_init(void);

// 背景校正，傳入量測週期（秒）
void co2_calibrate(uint16_t period_s);

// 讀取 24-byte 初始狀態，回傳 abc_hours, meas_period, num_samples
bool co2_read_initial_state(uint16_t *abc_hours,
                            uint16_t *meas_period,
                            uint16_t *num_samples);
bool sensor_read(uint8_t reg, uint8_t *buf, uint16_t len);
bool sensor_write(uint8_t *buf, uint16_t len);
// 讀取一次 CO₂ 原始量測值（ppm）
bool co2_read_ppm(uint16_t *co2_ppm);

#endif // CO2_SENSOR_H
