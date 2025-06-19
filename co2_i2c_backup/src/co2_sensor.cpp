// ===== co2_sensor.cpp =====
#include "co2_sensor.h"
#include "em_device.h"
#include "em_chip.h"
#include "sl_system_init.h"
#include "sl_i2cspm.h"
#include "em_gpio.h"
#include <stdio.h>

#define CO2_I2C_ADDR                 0x68
#define CO2_EN_PORT                  gpioPortA
#define CO2_EN_PIN                   0
#define WARMUP_DELAY_MS              10000
#define STABILIZE_DELAY_MS           35
#define EEPROM_UPDATE_DELAY_MS       25
#define CONTINUOUS_MODE_REG          0x95
#define CONTINUOUS_MODE_VALUE        0x00

#define ERROR_STATUS_LSB             0x01
#define CAL_STATUS_REG               0x81
#define CAL_MSB_REG                  0x82
#define CAL_PREFIX                   0x7C
#define CAL_BACK_CMD                 0x06
#define CAL_DONE_FLAG                0x20

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

void co2_init(void) {
    // 上電 & 暖機
    GPIO_PinModeSet(CO2_EN_PORT, CO2_EN_PIN, gpioModePushPull, 1);
    delay_ms(WARMUP_DELAY_MS);
    // 喚醒
    sensor_wakeup();
    printf("Wake up complete...\r\n");
    delay_ms(20);
    // 設定連續模式
    uint8_t modeBuf[2] = { CONTINUOUS_MODE_REG, CONTINUOUS_MODE_VALUE };
    sensor_write(modeBuf, 2);
    delay_ms(EEPROM_UPDATE_DELAY_MS);
}

void co2_calibrate(uint16_t period_s) {
    // 清除舊校正
    uint8_t clrBuf[2] = { CAL_STATUS_REG, 0x00 };
    sensor_write(clrBuf, 2);
    delay_ms(EEPROM_UPDATE_DELAY_MS);
    // 發起背景校正命令: MSB + PREFIX + CMD
    uint8_t cmdBuf[3] = { CAL_MSB_REG, CAL_PREFIX, CAL_BACK_CMD };
    sensor_write(cmdBuf, 3);
    // 等待完成
    for (int i = 0; i < 3; i++) {
        delay_ms(period_s * 1000);
        uint8_t status;
        sensor_read(CAL_STATUS_REG, &status, 1);
        if (status & CAL_DONE_FLAG) {
            break;
        }
    }
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
