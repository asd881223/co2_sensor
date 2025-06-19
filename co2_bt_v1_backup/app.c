#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app.h"
#include "src/co2_sensor.h"
#include "src/co2_calibration.h"
#include "gatt_db.h"
#include "app_log.h"
#include "sl_sleeptimer.h"
#include "stdio.h"

static uint8_t advertising_set_handle = 0xff;
static sl_sleeptimer_timer_handle_t co2_notify_timer;
static bool co2_notify_flag = false;
static bool notify_enabled = false;
static bool co2_calibration_done = false;

// 定時器 callback：5 秒觸發一次
static void co2_notify_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  co2_notify_flag = true;
}

// 初始化
SL_WEAK void app_init(void)
{
  app_log_info("start co2 init\r\n");
  co2_init();

  // 等待 sensor 初始化完成
  delay_ms(5000);  // 或讀 period 時用 retry 探測也可以

  app_log_info("start new co2 calibration\r\n");

  uint16_t abc_hours, period_sec, num_samples;
  if (co2_read_initial_state(&abc_hours, &period_sec, &num_samples)) {
    app_log_info("Sensor period: %us, samples: %u, ABC: %u\r\n",
                 period_sec, num_samples, abc_hours);

    // 加一個 sanity check：如果 period 是 0，就 fallback
    if (period_sec == 0) {
      period_sec = 5;
      app_log_warning("Period=0 detected, fallback to 5s\n");
    }

    co2_calibration_start(period_sec);
  } else {
    app_log_warning("Failed to read sensor state. Use default 5s period.\r\n");
    co2_calibration_start(5);
  }

  co2_calibration_done = false;
}


// 主迴圈處理
SL_WEAK void app_process_action(void)
{
  // 處理非阻塞式校正流程
  if (!co2_calibration_done && !co2_calibration_is_error()) {
    co2_calibration_process();

    if (co2_calibration_is_done()) {
      co2_calibration_done = true;
      app_log_info("🎯 Calibration completed, enabling CO2 notify\n");

      sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
        &co2_notify_timer,
        5000,
        co2_notify_timer_callback,
        NULL,
        0,
        false);
      app_assert_status(sc);
    }

    if (co2_calibration_is_error()) {
      co2_calibration_done = true;
      app_log_warning("⚠️ Calibration failed. Notify may be inaccurate.\n");

      sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
        &co2_notify_timer,
        5000,
        co2_notify_timer_callback,
        NULL,
        0,
        false);
      app_assert_status(sc);
    }

    return; // 尚未完成校正時，不進行 notify
  }

  // 推播 CO2 值
  if (co2_notify_flag && notify_enabled) {
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
}

// BLE 事件處理器
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      app_log("BLE system booted\n");

      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_generate_data(
        advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_connection_opened_id:
      app_log("BLE connection opened\n");
      break;

    case sl_bt_evt_connection_closed_id:
      app_log("BLE disconnected, restarting advertising...\n");

      sc = sl_bt_legacy_advertiser_generate_data(
        advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      notify_enabled = false;
      break;

    case sl_bt_evt_gatt_server_user_read_request_id:
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_co2_ppm) {
        app_log(">> Entered CO2 read handler (ASCII)\n");

        uint16_t co2_ppm = 0xFFFF;
        char buffer[6];

        if (co2_read_ppm(&co2_ppm)) {
          snprintf(buffer, sizeof(buffer), "%u", co2_ppm);
        } else {
          snprintf(buffer, sizeof(buffer), "ERR");
        }

        sl_status_t sc = sl_bt_gatt_server_send_user_read_response(
          evt->data.evt_gatt_server_user_read_request.connection,
          gattdb_co2_ppm,
          SL_STATUS_OK,
          strlen(buffer),
          (uint8_t*)buffer,
          NULL);

        app_log("   Sent ASCII CO2 = %s (status=0x%lX)\n", buffer, (unsigned long)sc);
      }
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_co2_ppm) {
        uint16_t flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;

        if (flags & sl_bt_gatt_notification) {
          notify_enabled = true;
          app_log("Notify enabled by client\n");
        } else {
          notify_enabled = false;
          app_log("Notify disabled by client\n");
        }
      }
      break;

    default:
      break;
  }
}
