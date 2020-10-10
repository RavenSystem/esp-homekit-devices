/*
 * ESP Timers Helper
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __TIMERS_HELPER_H__
#define __TIMERS_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <FreeRTOS.h>
#include <timers.h>

void esp_timer_start(TimerHandle_t xTimer);
void esp_timer_start_from_ISR(TimerHandle_t xTimer);
void esp_timer_stop(TimerHandle_t xTimer);
void esp_timer_stop_from_ISR(TimerHandle_t xTimer);
void esp_timer_change_period(TimerHandle_t xTimer, const uint32_t new_period_ms);
TimerHandle_t esp_timer_create(const uint32_t period_ms, const bool auto_reload, void* pvTimerID, TimerCallbackFunction_t pxCallbackFunction);
void esp_timer_delete(TimerHandle_t xTimer);

#ifdef __cplusplus
}
#endif

#endif  // __TIMERS_HELPER_H__
