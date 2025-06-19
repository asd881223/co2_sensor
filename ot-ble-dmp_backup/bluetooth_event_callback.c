/*******************************************************************************
 * @file
 * @brief Bluetooth event callback support
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#if SL_OPENTHREAD_BLE_CLI_ENABLE

#include "app.h"
#include "sl_bluetooth.h"
#include "sl_bt_api.h"
#include <openthread/cli.h>
#include "gatt_db.h"
#include "src/co2_sensor.h"
#pragma once
#include "sl_sleeptimer.h"
extern void printBleAddress(bd_addr address);

extern bool notify_enabled;
extern sl_sleeptimer_timer_handle_t co2_notify_timer;

// 宣告在檔案上方
static uint8_t adv_handle = 0xFF;
uint8_t gatt_connection = 0xFF;


void sl_bt_on_event(sl_bt_msg_t *evt)
{
    sl_status_t sc;

    switch (SL_BT_MSG_ID(evt->header))
    {
      case sl_bt_evt_system_boot_id:
          {
            // 印出 boot 狀態
            otCliOutputFormat("[I] start openthread init\n");

            // 1. 建立 advertising handle（⚠️ 必須）
            sc = sl_bt_advertiser_create_set(&adv_handle);
            otCliOutputFormat("[I] create advertiser handle: %s\n", (sc == SL_STATUS_OK) ? "OK" : "FAIL");

            // 2. 設定廣播間隔（160 = 100ms）
            sc = sl_bt_advertiser_set_timing(
                adv_handle,
                160,  // min
                160,  // max
                0,    // duration
                0     // max events
            );
            otCliOutputFormat("[I] set advertising timing: %s\n", (sc == SL_STATUS_OK) ? "OK" : "FAIL");

            // 3. 產生廣播封包內容
            sc = sl_bt_legacy_advertiser_generate_data(
                adv_handle,
                sl_bt_advertiser_general_discoverable
            );
            otCliOutputFormat("[I] generate advertising data: %s\n", (sc == SL_STATUS_OK) ? "OK" : "FAIL");

            // 4. 啟動 legacy 廣播（最相容手機）
            sc = sl_bt_legacy_advertiser_start(
                adv_handle,
                sl_bt_legacy_advertiser_connectable
            );
            otCliOutputFormat("[I] start advertising: %s\n", (sc == SL_STATUS_OK) ? "OK" : "FAIL");

            break;
          }
      case sl_bt_evt_connection_opened_id:
      {
          sl_bt_evt_connection_opened_t *conn_evt = (sl_bt_evt_connection_opened_t *)&(evt->data);
          otCliOutputFormat("BLE connection opened handle=%d address=", conn_evt->connection);
          printBleAddress(conn_evt->address);
          otCliOutputFormat(" address_type=%d master=%d advertising_set=%d\r\n",
                            conn_evt->connection,
                            conn_evt->role,
                            conn_evt->advertiser);
          gatt_connection = conn_evt->connection;

      }
      break;
      case sl_bt_evt_connection_closed_id:
      {
          sl_bt_evt_connection_closed_t *conn_evt = &evt->data.evt_connection_closed;
          otCliOutputFormat("BLE connection closed handle=%d reason=0x%2x\r\n", conn_evt->connection, conn_evt->reason);

          // ✅ 重新啟動 advertising
          sl_status_t sc;
          sc = sl_bt_legacy_advertiser_start(
              adv_handle,
              sl_bt_legacy_advertiser_connectable
          );
          otCliOutputFormat("[I] Restart advertising: %s\n", (sc == SL_STATUS_OK) ? "OK" : "FAIL");
      }
      break;
      case sl_bt_evt_connection_parameters_id:
      {
          sl_bt_evt_connection_parameters_t *params_evt = (sl_bt_evt_connection_parameters_t *)&(evt->data);
          otCliOutputFormat("BLE connection parameters handle=%d interval=%d latency=%d timeout=%d security_mode=%d\r\n",
                            params_evt->connection,
                            params_evt->interval,
                            params_evt->latency,
                            params_evt->timeout,
                            params_evt->security_mode);
      }
      break;
      case sl_bt_evt_scanner_legacy_advertisement_report_id:
      {
          sl_bt_evt_scanner_legacy_advertisement_report_t *rsp_evt =
              (sl_bt_evt_scanner_legacy_advertisement_report_t *)&(evt->data);
          otCliOutputFormat("BLE scan response address=");
          printBleAddress(rsp_evt->address);
          otCliOutputFormat(" address_type=%d\r\n", rsp_evt->address_type);
      }
      break;
      case sl_bt_evt_scanner_extended_advertisement_report_id:
      {
          sl_bt_evt_scanner_extended_advertisement_report_t *rsp_evt =
              (sl_bt_evt_scanner_extended_advertisement_report_t *)&(evt->data);
          otCliOutputFormat("BLE scan response address=");
          printBleAddress(rsp_evt->address);
          otCliOutputFormat(" address_type=%d\r\n", rsp_evt->address_type);
      }
      break;
      case sl_bt_evt_gatt_server_characteristic_status_id:
        if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_co2_ppm) {
            uint16_t flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
            if (flags & sl_bt_gatt_notification) {
                notify_enabled = true;
                start_co2_notify_timer();  // 啟動timer
                otCliOutputFormat("🟢 Notify enabled by client\n");
            } else {
                notify_enabled = false;
                stop_co2_notify_timer();   // 停止timer
                otCliOutputFormat("🔴 Notify disabled by client\n");
            }
        }
        break;
      case sl_bt_evt_gatt_server_user_read_request_id:
        if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_co2_ppm) {
          uint16_t ppm = 0xFFFF;
          char buffer[6];
          if (co2_read_ppm(&ppm)) {
            snprintf(buffer, sizeof(buffer), "%u", ppm);
          } else {
            snprintf(buffer, sizeof(buffer), "ERR");
          }

          sl_bt_gatt_server_send_user_read_response(
            evt->data.evt_gatt_server_user_read_request.connection,
            gattdb_co2_ppm,
            SL_STATUS_OK,
            strlen(buffer),
            (uint8_t*)buffer,
            NULL);

          otCliOutputFormat("CO2 read request → %s ppm\n", buffer);
        }
        break;

      case sl_bt_evt_gatt_procedure_completed_id:
      {
          sl_bt_evt_gatt_procedure_completed_t *proc_comp_evt = (sl_bt_evt_gatt_procedure_completed_t *)&(evt->data);
          otCliOutputFormat("BLE procedure completed handle=%d result=0x%2x\r\n",
                            proc_comp_evt->connection,
                            proc_comp_evt->result);
      }
      break;
      default:
          otCliOutputFormat("BLE event: 0x%04x\r\n", SL_BT_MSG_ID(evt->header));
      }
}

#endif // SL_OPENTHREAD_BLE_CLI_ENABLE
