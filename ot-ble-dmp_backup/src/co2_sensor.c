// ===== co2_sensor.c =====
#include "co2_sensor.h"
#include "delay_util.h"
#include "em_device.h"
#include "em_chip.h"
#include "sl_system_init.h"
#include "sl_i2cspm.h"
#include "em_gpio.h"
#include <stdio.h>
#include "stdint.h"
#include "sl_sleeptimer.h"

#define CO2_I2C_ADDR                 0x68
#define CO2_EN_PORT                  gpioPortA
#define CO2_EN_PIN                   0
#define WARMUP_DELAY_MS              10000
#define STABILIZE_DELAY_MS           35
#define EEPROM_UPDATE_DELAY_MS       25
#define CONTINUOUS_MODE_REG          0x95
#define CONTINUOUS_MODE_VALUE        0x00

#define ERROR_STATUS_LSB             0x01

static bool sensor_wakeup(void) {
    I2C_TransferSeq_TypeDef seq = {0};
    seq.addr  = CO2_I2C_ADDR<<1;
    seq.flags = I2C_FLAG_WRITE;
    return (I2CSPM_Transfer(I2C0, &seq) == i2cTransferDone);
}

bool sensor_read(uint8_t reg, uint8_t *buf, uint16_t len) {
    I2C_TransferSeq_TypeDef seq = {0};
    seq.addr        = CO2_I2C_ADDR<<1;
    seq.flags       = I2C_FLAG_WRITE_READ;
    seq.buf[0].data = &reg;
    seq.buf[0].len  = 1;
    seq.buf[1].data = buf;
    seq.buf[1].len  = len;
    return (I2CSPM_Transfer(I2C0, &seq) == i2cTransferDone);
}

bool sensor_write(uint8_t *buf, uint16_t len) {
    I2C_TransferSeq_TypeDef seq = {0};
    seq.addr        = CO2_I2C_ADDR<<1;
    seq.flags       = I2C_FLAG_WRITE;
    seq.buf[0].data = buf;
    seq.buf[0].len  = len;
    return (I2CSPM_Transfer(I2C0, &seq) == i2cTransferDone);
}

static bool warmup_started = false;
static bool warmup_complete = false;
static uint64_t warmup_start_time = 0;

bool co2_init(void) {
    if (!warmup_started) {
        GPIO_PinModeSet(CO2_EN_PORT, CO2_EN_PIN, gpioModePushPull, 1);
        warmup_start_time = sl_sleeptimer_get_tick_count64();
        warmup_started = true;
        return false; // Not yet ready
    }

    uint64_t elapsed_ticks = sl_sleeptimer_get_tick_count64() - warmup_start_time;
    uint64_t elapsed_ms = (elapsed_ticks * 1000) / sl_sleeptimer_get_timer_frequency();

    if (!warmup_complete && elapsed_ms >= WARMUP_DELAY_MS) {
        warmup_complete = true;

        if (!sensor_wakeup()) {
            printf("Sensor wakeup failed!\r\n");
            return false;
        }
        printf("Wake up complete...\r\n");
        delay_ms_blocking(20);

        uint8_t modeBuf[2] = { CONTINUOUS_MODE_REG, CONTINUOUS_MODE_VALUE };
        sensor_write(modeBuf, 2);
        delay_ms_blocking(EEPROM_UPDATE_DELAY_MS);

        return true; // Init done
    }

    return warmup_complete; // Return true only if ready
}

bool co2_read_initial_state(uint16_t *abc_hours,
                            uint16_t *meas_period,
                            uint16_t *num_samples) {
    uint8_t buf[24];
    if (!sensor_read(0xC4, buf, 24)) return false;
    *abc_hours   = (buf[0]<<8) | buf[1];
    *meas_period = (buf[2]<<8) | buf[3];
    *num_samples = (buf[4]<<8) | buf[5];
    return true;
}

bool co2_read_ppm(uint16_t *co2_ppm) {
    uint8_t buf[7];
    if (!sensor_read(0x01, buf, 7)) return false;
    *co2_ppm = (buf[5]<<8) | buf[6];
    return true;
}
