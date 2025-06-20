#include "sl_event_handler.h"

#include "em_chip.h"
#include "sl_interrupt_manager.h"
#include "sl_board_init.h"
#include "sl_clock_manager_init.h"
#include "sl_device_init_dcdc.h"
#include "sl_clock_manager.h"
#include "sl_device_init_emu.h"
#include "sl_board_control.h"
#include "sl_sleeptimer.h"
#include "sl_debug_swo.h"
#include "sl_gpio.h"
#include "gpiointerrupt.h"
#include "sl_i2cspm_instances.h"
#include "sl_iostream_init_eusart_instances.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "sl_spidrv_instances.h"
#include "sl_uartdrv_instances.h"
#include "sl_iostream_init_instances.h"
#include "sl_cos.h"
#include "sl_iostream_handles.h"

void sl_platform_init(void)
{
  CHIP_Init();
  sl_interrupt_manager_init();
  sl_board_preinit();
  sl_clock_manager_init();
  sl_device_init_dcdc();
  sl_clock_manager_runtime_init();
  sl_device_init_emu();
  sl_board_init();
}

void sl_driver_init(void)
{
  sl_debug_swo_init();
  sl_gpio_init();
  GPIOINT_Init();
  sl_i2cspm_init_instances();
  sl_simple_button_init_instances();
  sl_simple_led_init_instances();
  sl_spidrv_init_instances();
  sl_uartdrv_init_instances();
  sl_cos_send_config();
}

void sl_service_init(void)
{
  sl_board_configure_vcom();
  sl_sleeptimer_init();
  sl_iostream_init_instances();
}

void sl_stack_init(void)
{
}

void sl_internal_app_init(void)
{
}

void sl_platform_process_action(void)
{
}

void sl_service_process_action(void)
{
}

void sl_stack_process_action(void)
{
}

void sl_internal_app_process_action(void)
{
}

void sl_iostream_init_instances(void)
{
  sl_iostream_eusart_init_instances();
  sl_iostream_set_console_instance();
}

