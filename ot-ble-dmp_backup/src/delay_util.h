#ifndef DELAY_UTIL_H
#define DELAY_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_sleeptimer.h"

// 阻塞式延遲
void delay_ms_blocking(uint32_t ms);

// 非阻塞式延遲：啟動與查詢是否完成
void delay_ms_non_blocking_start(uint32_t ms);
bool delay_ms_non_blocking_done(void);

#endif // DELAY_UTIL_H
