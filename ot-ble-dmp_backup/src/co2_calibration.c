#include "co2_sensor.h"
#include "delay_util.h"
#include <stdio.h>
#include "app_log.h"

#define EEPROM_UPDATE_DELAY_MS 25
#define CAL_STATUS_REG         0x81
#define CAL_MSB_REG            0x82
#define CAL_PREFIX             0x7C
#define CAL_BACK_CMD           0x06
#define CAL_DONE_FLAG          0x20

// Calibration state machine
typedef enum {
    CO2_CAL_IDLE,
    CO2_CAL_CLEAR,
    CO2_CAL_CMD_SEND,
    CO2_CAL_WAIT,
    CO2_CAL_CHECK,
    CO2_CAL_DONE,
    CO2_CAL_FAIL
} co2_cal_state_t;

static co2_cal_state_t cal_state = CO2_CAL_IDLE;
static uint8_t cal_try = 0;

void co2_calibration_start(void) {
    // Clear previous calibration flag
    uint8_t clrBuf[2] = { CAL_STATUS_REG, 0x00 };
    sensor_write(clrBuf, 2);
    delay_ms_blocking(EEPROM_UPDATE_DELAY_MS);

    // Send background calibration command
    uint8_t cmdBuf[3] = { CAL_MSB_REG, CAL_PREFIX, CAL_BACK_CMD };
    sensor_write(cmdBuf, 3);

    cal_try = 0;
    cal_state = CO2_CAL_WAIT;
    delay_ms_non_blocking_start(3000);  // wait 3 seconds
}

void co2_calibration_process(void) {
    switch (cal_state) {
        case CO2_CAL_WAIT:
            if (delay_ms_non_blocking_done()) {
                cal_state = CO2_CAL_CHECK;
            }
            break;

        case CO2_CAL_CHECK: {
            uint8_t status = 0;
            if (sensor_read(CAL_STATUS_REG, &status, 1)) {
                app_log_info("[CAL] Try %d, status = 0x%02X\r\n", cal_try, status);
                if (status & CAL_DONE_FLAG) {
                    cal_state = CO2_CAL_DONE;
                    app_log_info("[CO2] Calibration complete.\r\n");
                    break;
                }
            } else {
                app_log_info("[CAL] sensor_read() failed!\r\n");
            }

            cal_try++;
            if (cal_try >= 3) {
                cal_state = CO2_CAL_FAIL;
                printf("[CO2] Calibration failed.\r\n");
            } else {
                delay_ms_non_blocking_start(3000);
                cal_state = CO2_CAL_WAIT;
            }
            break;
        }

        case CO2_CAL_DONE:
        case CO2_CAL_FAIL:
        case CO2_CAL_IDLE:
        default:
            // No action
            break;
    }
}

bool co2_calibration_is_done(void) {
    return (cal_state == CO2_CAL_DONE || cal_state == CO2_CAL_FAIL);
}

bool co2_calibration_success(void) {
    return (cal_state == CO2_CAL_DONE);
}
