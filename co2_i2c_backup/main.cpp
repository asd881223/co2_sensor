// ===== main.cpp =====
#include "src/co2_sensor.h"
#include "em_device.h"
#include "em_chip.h"
#include "sl_system_init.h"
#include <stdio.h>
#include "src/co2_calibration.h"

int main(void) {
    CHIP_Init();
    sl_system_init();
    printf("Start init\r\n");

    co2_init();

    printf("Start calibration\r\n");
    co2_calibration_start(4); // 你也可以讀取 co2_read_initial_state() 裡的 period

    while (!co2_calibration_is_done() && !co2_calibration_is_error()) {
        co2_calibration_process();  // 每次主迴圈呼叫
    }

    if (co2_calibration_is_done()) {
        printf("Calibration successful\r\n");
    } else {
        printf("Calibration failed\r\n");
    }

    printf("Start Measurement\r\n");
    while (1) {
        uint16_t ppm;
        if (co2_read_ppm(&ppm)) {
            printf("CO2: %u ppm\r\n", ppm);
        }
        delay_ms(2000);
    }

    return 0;
}
