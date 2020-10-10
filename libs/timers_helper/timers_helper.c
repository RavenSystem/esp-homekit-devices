/*
 * ESP Timers Helper
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdio.h>

#include "timers_helper.h"

#define XTIMER_BLOCK_TIME               (0)
#define XTIMER_PERIOD_BLOCK_TIME        (0)

void esp_timer_start(TimerHandle_t xTimer) {
    if (xTimer) {
        if (!xTimerStart(xTimer, XTIMER_BLOCK_TIME)) {
            printf("! Timer Start Failed\n ");
        }
    }
}

void esp_timer_start_from_ISR(TimerHandle_t xTimer) {
    if (xTimer) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (!xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken)) {
            printf("! Timer Start from ISR Failed\n ");
        }
    }
}

void esp_timer_stop(TimerHandle_t xTimer) {
    if (xTimer) {
        if (!xTimerStop(xTimer, XTIMER_BLOCK_TIME)) {
            printf("! Timer Stop Failed\n ");
        }
    }
}

void esp_timer_stop_from_ISR(TimerHandle_t xTimer) {
    if (xTimer) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (!xTimerStopFromISR(xTimer, &xHigherPriorityTaskWoken)) {
            printf("! Timer Stop from ISR Failed\n ");
        }
    }
}

void esp_timer_change_period(TimerHandle_t xTimer, const uint32_t new_period_ms) {
    if (xTimer) {
        if (!xTimerChangePeriod(xTimer, pdMS_TO_TICKS(new_period_ms), XTIMER_PERIOD_BLOCK_TIME)) {
            printf("! Timer Change Period Failed\n ");
        }
    }
}

TimerHandle_t esp_timer_create(const uint32_t period_ms, const bool auto_reload, void* pvTimerID, TimerCallbackFunction_t pxCallbackFunction) {
    UBaseType_t uxAutoReload = pdFALSE;
    if (auto_reload) {
        uxAutoReload = pdTRUE;
    }
    return xTimerCreate(0, pdMS_TO_TICKS(period_ms), uxAutoReload, pvTimerID, pxCallbackFunction);
}

void esp_timer_delete(TimerHandle_t xTimer) {
    if (xTimer) {
        if (!xTimerDelete(xTimer, XTIMER_BLOCK_TIME)) {
            printf("! Timer Delete Failed\n ");
        }
    }
}
