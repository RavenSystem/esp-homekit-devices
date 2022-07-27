/*
 * ESP Timers Helper
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __TIMERS_HELPER_H__
#define __TIMERS_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <FreeRTOS.h>
#include <timers.h>

#define TIMER_MANAGER_START                                     (0)
#define TIMER_MANAGER_STOP                                      (1)
#define TIMER_MANAGER_DELETE                                    (2)

BaseType_t esp_timer_start(TimerHandle_t xTimer);
BaseType_t esp_timer_stop(TimerHandle_t xTimer);
BaseType_t esp_timer_delete(TimerHandle_t xTimer);
BaseType_t esp_timer_change_period(TimerHandle_t xTimer, const uint32_t new_period_ms);

BaseType_t esp_timer_start_forced(TimerHandle_t xTimer);
BaseType_t esp_timer_stop_forced(TimerHandle_t xTimer);
BaseType_t esp_timer_delete_forced(TimerHandle_t xTimer);
BaseType_t esp_timer_change_period_forced(TimerHandle_t xTimer, const uint32_t new_period_ms);

BaseType_t IRAM esp_timer_start_from_ISR(TimerHandle_t xTimer);
BaseType_t IRAM esp_timer_stop_from_ISR(TimerHandle_t xTimer);

BaseType_t esp_timer_manager(const uint8_t option, TimerHandle_t xTimer, TickType_t xBlockTime);
BaseType_t esp_timer_change_period_manager(TimerHandle_t xTimer, const uint32_t new_period_ms, TickType_t xBlockTime);
BaseType_t IRAM esp_timer_manager_from_ISR(const uint8_t option, TimerHandle_t xTimer);

TimerHandle_t esp_timer_create(const uint32_t period_ms, const bool auto_reload, void* pvTimerID, TimerCallbackFunction_t pxCallbackFunction);

#ifdef __cplusplus
}
#endif

#endif  // __TIMERS_HELPER_H__
