#include "app.h"
#include "openthread-system.h"
#include "reset_util.h"
#include "gatt_db.h"
#include "sl_component_catalog.h"
#include "sl_memory_manager.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_assert.h"
#include "sl_ot_rtos_adaptation.h"

#include "src/co2_sensor.h"
#include "src/co2_calibration.h"
#include "src/delay_util.h"

#include <openthread/platform/logging.h>
#include <stdarg.h>
#include <stdio.h>

// ========= Platform log redirect =========
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    (void)aLogRegion;

    const char *level_str[] = { "NONE", "CRIT", "WARN", "NOTE", "INFO", "DEBG" };
    printf("[OT][%s] ", level_str[aLogLevel]);

    va_list args;
    va_start(args, aFormat);
    vprintf(aFormat, args);
    va_end(args);

    printf("\r\n");
}

// ========= Global flags / state =========
static sl_sleeptimer_timer_handle_t loop_timer;
static bool co2_timer_started = false;

bool co2_notify_flag = false;
bool notify_enabled = false;
static bool co2_calibration_done = false;
static bool calibration_retry_once = false;
static bool sensor_initialized = false;
sl_sleeptimer_timer_handle_t co2_notify_timer;
extern uint8_t gatt_connection;

static otInstance *sInstance = NULL;

otInstance *otGetInstance(void)
{
    return sInstance;
}

// ========= Periodic Event Timer (Every 1s to dispatch app task) =========
static void app_loop_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;
//    app_log_info("[I] timer loop\r\n");
    app_log_info("[I] [STATUS] sensor_initialized: %d, co2_calibration_done: %d\r\n",
                  sensor_initialized, co2_calibration_done);
    sl_ot_rtos_set_pending_event(SL_OT_RTOS_EVENT_APP);
}

// ========= CO₂ Notify Timer (Every 5s) =========
static void co2_notify_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)handle;
    (void)data;
    co2_notify_flag = true;
    sl_ot_rtos_set_pending_event(SL_OT_RTOS_EVENT_APP);
}

void start_co2_notify_timer(void)
{
    if (!co2_timer_started) {
        sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
            &co2_notify_timer,
            5000,
            co2_notify_timer_callback,
            NULL,
            0,
            false);
        if (sc == SL_STATUS_OK) {
            co2_timer_started = true;
        } else {
            app_log_error("start_co2_notify_timer: Status: sc = 0x%04X\r\n", sc);
        }
    }
}

void stop_co2_notify_timer(void)
{
    if (co2_timer_started) {
        sl_sleeptimer_stop_timer(&co2_notify_timer);
        co2_timer_started = false;
    }
}

// ========= OT Instance + CLI =========
void sl_ot_create_instance(void)
{
    app_log_info("==> [sl_ot_create_instance] Creating OpenThread instance\r\n");
    sInstance = otInstanceInitSingle();
    assert(sInstance);
}

void sl_ot_cli_init(void)
{
    app_log_info("==> [sl_ot_cli_init] Initializing CLI\r\n");
    otAppCliInit(sInstance);
}

// ========= Application Init =========
void app_init(void)
{
    app_log_info("start openthread init\r\n");
    OT_SETUP_RESET_JUMP(argv);

    // loop timer = 每 1 秒喚醒 sl_ot_rtos_event_handler
    sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
        &loop_timer,
        5000,
        app_loop_timer_callback,
        NULL,
        0,
        false);
    app_assert_status(sc);
}

// ========= 主業務處理函數（會被 RTOS 呼叫） =========
void sl_ot_rtos_application_tick(void)
{
    sl_ot_rtos_event_handler();
}

void sl_ot_rtos_event_handler(void)
{
    otTaskletsProcess(sInstance);
    otSysProcessDrivers(sInstance);

    if (!sensor_initialized) {
        app_log_info("[INIT] start co2 init\r\n");
        if (!co2_init()) {
            app_log_info("[INIT] warming up...\r\n");
            return;
        }
        sensor_initialized = true;
        app_log_info("[INIT] sensor initialized!\r\n");

        // 🟢 啟動第一次校正（最重要）
        co2_calibration_start();
    }

    if (!co2_calibration_done) {
        app_log_info("[CALIB] process...\r\n");
        co2_calibration_process();

        if (co2_calibration_is_done()) {
            co2_calibration_done = true;

            if (co2_calibration_success()) {
                app_log_info("🎯 Calibration completed, enabling CO2 notify\n");
                start_co2_notify_timer();
            } else if (!calibration_retry_once) {
                calibration_retry_once = true;
                app_log_warning("⚠️ Calibration failed, retrying once...\n");
                co2_calibration_start();
                co2_calibration_done = false;
            } else {
                app_log_warning("⚠️ Calibration failed. Notify may be inaccurate.\n");
                start_co2_notify_timer();
            }
        }
        return;
    }
    if (co2_notify_flag && notify_enabled && gatt_connection != 0xFF) {
      co2_notify_flag = false;
      app_log_info("⏰ Notify triggered\n");

      uint16_t ppm = 0xFFFF;
      if (co2_read_ppm(&ppm)) {
        char ascii_buf[8];
        snprintf(ascii_buf, sizeof(ascii_buf), "%u", ppm);

        sl_status_t sc = sl_bt_gatt_server_notify_all(
          gattdb_co2_ppm,
          strlen(ascii_buf),
          (uint8_t*)ascii_buf);

        app_log_info("Notify sent. CO2 = %u ppm (status=0x%lX)\n", ppm, (unsigned long)sc);
      } else {
        app_log_warning("CO2 read failed for notify.\n");
      }
    }


//    if (co2_notify_flag && notify_enabled && gatt_connection != 0xFF) {
//        co2_notify_flag = false;
//        uint16_t ppm = 0xFFFF;
//        if (co2_read_ppm(&ppm)) {
//            app_log_info("Notify CO2 = %u ppm\n", ppm);
//
//            uint8_t buffer[2] = { ppm >> 8, ppm & 0xFF };
//            sl_status_t sc = sl_bt_gatt_server_send_notification(
//                gatt_connection,
//                gattdb_co2_ppm,
//                sizeof(buffer),
//                buffer);
//            app_log_info("Notify status: 0x%04X\n", sc);
//        } else {
//            app_log_warning("CO2 read failed.\n");
//        }
//    }
}

// ========= Clean Exit =========
void app_exit(void)
{
    otInstanceFinalize(sInstance);
}
