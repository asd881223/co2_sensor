#include "delay_util.h"

static volatile bool delay_complete = false;
static sl_sleeptimer_timer_handle_t delay_timer;

// 回呼函式：非阻塞延遲計時結束時呼叫
static void timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
    (void)handle;
    (void)data;
    delay_complete = true;
}

// 阻塞式延遲實作
void delay_ms_blocking(uint32_t ms) {
    sl_sleeptimer_delay_millisecond(ms);
}

// 非阻塞延遲啟動
void delay_ms_non_blocking_start(uint32_t ms) {
    delay_complete = false;
    sl_sleeptimer_start_timer_ms(&delay_timer, ms, timer_callback, NULL, 0, 0);
}

// 非阻塞延遲是否完成
bool delay_ms_non_blocking_done(void) {
    return delay_complete;
}
