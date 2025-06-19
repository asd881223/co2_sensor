#include "spidrv.h"
#include "sl_spidrv_instances.h"
#include "sl_assert.h"


#include "sl_spidrv_mikroe_config.h"
SPIDRV_HandleData_t sl_spidrv_mikroe_handle_data;
SPIDRV_Handle_t sl_spidrv_mikroe_handle = &sl_spidrv_mikroe_handle_data;
SPIDRV_Init_t sl_spidrv_init_mikroe = {
  .port = SL_SPIDRV_MIKROE_PERIPHERAL,
#if defined(_USART_ROUTELOC0_MASK)
  .portLocationTx = SL_SPIDRV_MIKROE_TX_LOC,
  .portLocationRx = SL_SPIDRV_MIKROE_RX_LOC,
  .portLocationClk = SL_SPIDRV_MIKROE_CLK_LOC,
#if defined(SL_SPIDRV_MIKROE_CS_LOC)
  .portLocationCs = SL_SPIDRV_MIKROE_CS_LOC,
#endif
#elif defined(_GPIO_USART_ROUTEEN_MASK)
  .portTx = SL_SPIDRV_MIKROE_TX_PORT,
  .portRx = SL_SPIDRV_MIKROE_RX_PORT,
  .portClk = SL_SPIDRV_MIKROE_CLK_PORT,
#if defined(SL_SPIDRV_MIKROE_CS_PORT)
  .portCs = SL_SPIDRV_MIKROE_CS_PORT,
#endif
  .pinTx = SL_SPIDRV_MIKROE_TX_PIN,
  .pinRx = SL_SPIDRV_MIKROE_RX_PIN,
  .pinClk = SL_SPIDRV_MIKROE_CLK_PIN,
#if defined(SL_SPIDRV_MIKROE_CS_PIN)
  .pinCs = SL_SPIDRV_MIKROE_CS_PIN,
#endif
#else
  .portLocation = SL_SPIDRV_MIKROE_ROUTE_LOC,
#endif
  .bitRate = SL_SPIDRV_MIKROE_BITRATE,
  .frameLength = SL_SPIDRV_MIKROE_FRAME_LENGTH,
  .dummyTxValue = 0,
  .type = SL_SPIDRV_MIKROE_TYPE,
  .bitOrder = SL_SPIDRV_MIKROE_BIT_ORDER,
  .clockMode = SL_SPIDRV_MIKROE_CLOCK_MODE,
  .csControl = SL_SPIDRV_MIKROE_CS_CONTROL,
  .slaveStartMode = SL_SPIDRV_MIKROE_SLAVE_START_MODE,
};

void sl_spidrv_init_instances(void) {
  SPIDRV_Init(sl_spidrv_mikroe_handle, &sl_spidrv_init_mikroe);
}
