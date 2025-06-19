// ===== sht30_sensor.cpp =====
#include "sht30_sensor.h"
#include "em_device.h"
#include "em_chip.h"
#include "sl_system_init.h"
#include "sl_i2cspm.h"
#include "em_gpio.h"
#include <stdio.h>

#define SHT30_ADDR           0x44  // 7-bit
#define SHT30_CMD_MSB        0x2C
#define SHT30_CMD_LSB        0x06

// 一次性寫入命令並讀取 raw data
static bool sensor_transfer(uint8_t *cmd, uint8_t cmd_len, uint8_t *buf, uint16_t len) {
    I2C_TransferSeq_TypeDef seq = { 0 };
    seq.addr  = (SHT30_ADDR << 1);
    seq.flags = I2C_FLAG_WRITE_READ;
    seq.buf[0].data = cmd;
    seq.buf[0].len  = cmd_len;
    seq.buf[1].data = buf;
    seq.buf[1].len  = len;
    return (I2CSPM_Transfer(I2C0, &seq) == i2cTransferDone);
}

void sht30_init(void) {
    // SHT30 無專屬 enable pin，僅確保 I2C 已 init
}

bool sht30_read(float *cTemp, float *humidity) {
    uint8_t cmd[2] = { SHT30_CMD_MSB, SHT30_CMD_LSB };
    uint8_t raw[6];

    // 發送測量命令並讀取 6 bytes
    if (!sensor_transfer(cmd, 2, raw, 6)) {
        return false;
    }
    // 等待 20 ms
    delay_ms(20);

    uint16_t rawTemp = (raw[0] << 8) | raw[1];
    uint16_t rawHum  = (raw[3] << 8) | raw[4];

    // 計算攝氏與濕度
    *cTemp   = ((175.0f * rawTemp) / 65535.0f) - 45.0f;
    *humidity = (100.0f * rawHum) / 65535.0f;
    return true;
}
