/*******************************************************************************
 * @file
 * @brief main() function.
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

#include "app.h"
#include "sl_component_catalog.h"
#include "sl_system_init.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif // SL_CATALOG_POWER_MANAGER_PRESENT
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else // !SL_CATALOG_KERNEL_PRESENT
#include "sl_system_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT
#include "app_log.h"

int main(void)
{
    sl_system_init();
    app_init();

    app_log_info(">>> ENTER main loop\n");  // ✅ 加這行

#if defined(SL_CATALOG_KERNEL_PRESENT)
    app_log_info("Enter Kernel start!\r\n");
    sl_system_kernel_start();
#else
    while (1)
    {
        app_log_info(">>> inside while loop\n");  // ✅ 加這行

        sl_system_process_action();
        app_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
        sl_power_manager_sleep();
#endif
    }
    app_exit();
#endif
}

