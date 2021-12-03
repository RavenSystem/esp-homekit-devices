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

#define TIMER_MANAGER_START                                     (0)
#define TIMER_MANAGER_STOP                                      (1)
#define TIMER_MANAGER_DELETE                                    (2)

#define esp_timer_start(xTimer)                                 esp_timer_manager(TIMER_MANAGER_START, xTimer, 0)
#define esp_timer_stop(xTimer)                                  esp_timer_manager(TIMER_MANAGER_STOP, xTimer, 0)
#define esp_timer_delete(xTimer)                                esp_timer_manager(TIMER_MANAGER_DELETE, xTimer, 0)
#define esp_timer_change_period(xTimer, new_period_ms)          esp_timer_change_period_manager(xTimer, new_period_ms, 0)

#define esp_timer_start_forced(xTimer)                          esp_timer_manager(TIMER_MANAGER_START, xTimer, portMAX_DELAY)
#define esp_timer_stop_forced(xTimer)                           esp_timer_manager(TIMER_MANAGER_STOP, xTimer, portMAX_DELAY)
#define esp_timer_delete_forced(xTimer)                         esp_timer_manager(TIMER_MANAGER_DELETE, xTimer, portMAX_DELAY)
#define esp_timer_change_period_forced(xTimer, new_period_ms)   esp_timer_change_period_manager(xTimer, new_period_ms, portMAX_DELAY)

#define esp_timer_start_from_ISR(xTimer)                        esp_timer_manager_from_ISR(TIMER_MANAGER_START, xTimer)
#define esp_timer_stop_from_ISR(xTimer)                         esp_timer_manager_from_ISR(TIMER_MANAGER_STOP, xTimer)

BaseType_t esp_timer_manager(const uint8_t option, TimerHandle_t xTimer, TickType_t xBlockTime);
BaseType_t esp_timer_change_period_manager(TimerHandle_t xTimer, const uint32_t new_period_ms, TickType_t xBlockTime);
BaseType_t IRAM esp_timer_manager_from_ISR(const uint8_t option, TimerHandle_t xTimer);

TimerHandle_t esp_timer_create(const uint32_t period_ms, const bool auto_reload, void* pvTimerID, TimerCallbackFunction_t pxCallbackFunction);

#ifdef __cplusplus
}
#endif

#endif  // __TIMERS_HELPER_H__
