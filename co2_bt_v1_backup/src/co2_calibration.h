#ifndef CO2_CALIBRATION_H
#define CO2_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

// 狀態機列舉
typedef enum {
    CAL_IDLE = 0,
    CAL_RESETTING,
    CAL_SENDING_CMD,
    CAL_WAITING,
    CAL_CHECKING,
    CAL_DONE,
    CAL_ERROR
} co2_calibration_state_t;

// 啟動校正流程
void co2_calibration_start(uint16_t meas_period_s);

// 執行校正步驟（需每次主迴圈呼叫）
void co2_calibration_process(void);

// 查詢狀態
bool co2_calibration_is_done(void);
bool co2_calibration_is_error(void);

// 取得當前狀態（選用）
co2_calibration_state_t co2_calibration_get_state(void);

#endif
