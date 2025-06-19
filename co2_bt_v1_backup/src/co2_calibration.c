#include "src/co2_calibration.h"
#include "src/co2_sensor.h"
#include "sl_sleeptimer.h"
#include <stdio.h>

// 定義寄存器與旗標
#define ERROR_STATUS_LSB         0x01
#define CAL_STATUS_REG           0x81
#define CAL_MSB_REG              0x82
#define CAL_PREFIX               0x7C
#define CAL_BACK_CMD             0x06
#define CAL_DONE_FLAG            0x20
#define ERROR_CALIBRATION_FLAG   0x80
#define EEPROM_UPDATE_DELAY_MS   25

// 狀態與時間控制變數
static co2_calibration_state_t state = CAL_IDLE;
static uint32_t last_tick = 0;
static uint32_t wait_period_ms = 4000;
static uint8_t retry_count = 0;

static uint32_t ms_ticks(void) {
    uint64_t tick = sl_sleeptimer_get_tick_count64();
    uint32_t freq = sl_sleeptimer_get_timer_frequency();
    return (uint32_t)((tick * 1000) / freq);
}

void co2_calibration_start(uint16_t meas_period_s) {
    state = CAL_RESETTING;
    retry_count = 0;
    wait_period_ms = meas_period_s * 1000;
    last_tick = ms_ticks();
    printf("Calibration: start\r\n");
}

bool co2_calibration_is_done(void) {
    return state == CAL_DONE;
}

bool co2_calibration_is_error(void) {
    return state == CAL_ERROR;
}

co2_calibration_state_t co2_calibration_get_state(void) {
    return state;
}

void co2_calibration_process(void) {
    uint8_t buf[3], status;

    switch (state) {
        case CAL_IDLE:
        case CAL_DONE:
        case CAL_ERROR:
            return;

        case CAL_RESETTING:
            buf[0] = CAL_STATUS_REG;
            buf[1] = 0x00;
            if (sensor_write(buf, 2)) {
                delay_ms(EEPROM_UPDATE_DELAY_MS);
                state = CAL_SENDING_CMD;
                printf("Calibration: cleared old status\r\n");
            } else {
                state = CAL_ERROR;
                printf("Calibration: failed to clear status\r\n");
            }
            break;

        case CAL_SENDING_CMD:
            buf[0] = CAL_MSB_REG;
            buf[1] = CAL_PREFIX;
            buf[2] = CAL_BACK_CMD;
            if (sensor_write(buf, 3)) {
                last_tick = ms_ticks();
                state = CAL_WAITING;
                printf("Calibration: background command sent\r\n");
            } else {
                state = CAL_ERROR;
                printf("Calibration: failed to send command\r\n");
            }
            break;

        case CAL_WAITING:
            if ((ms_ticks() - last_tick) >= wait_period_ms) {
                state = CAL_CHECKING;
            }
            break;

        case CAL_CHECKING:
            if (sensor_read(ERROR_STATUS_LSB, &status, 1)) {
                if (status & ERROR_CALIBRATION_FLAG) {
                    state = CAL_ERROR;
                    printf("Calibration: error detected (unstable environment)\r\n");
                    break;
                }
            } else {
                state = CAL_ERROR;
                printf("Calibration: failed to read error status\r\n");
                break;
            }

            if (sensor_read(CAL_STATUS_REG, &status, 1)) {
                if (status & CAL_DONE_FLAG) {
                    state = CAL_DONE;
                    printf("Calibration: completed successfully\r\n");
                } else {
                    retry_count++;
                    if (retry_count >= 3) {
                        state = CAL_ERROR;
                        printf("Calibration: timeout after 3 retries\r\n");
                    } else {
                        last_tick = ms_ticks();
                        state = CAL_WAITING;
                        printf("Calibration: not done yet, retrying (%u)\r\n", retry_count);
                    }
                }
            } else {
                state = CAL_ERROR;
                printf("Calibration: failed to read calibration status\r\n");
            }
            break;
    }
}
