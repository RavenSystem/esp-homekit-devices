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
#define XTIMER_MAX_TRIES                (5)

void esp_timer_start(TimerHandle_t xTimer) {
    if (xTimer) {
        uint8_t tries = 0;
        while (!xTimerStart(xTimer, XTIMER_BLOCK_TIME) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Start Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        }
    }
}

void esp_timer_start_from_ISR(TimerHandle_t xTimer) {
    if (xTimer) {
        uint8_t tries = 0;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        while (!xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Start from ISR Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        }
    }
}

void esp_timer_stop(TimerHandle_t xTimer) {
    if (xTimer) {
        uint8_t tries = 0;
        while (!xTimerStop(xTimer, XTIMER_BLOCK_TIME) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Stop Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        }
    }
}

void esp_timer_stop_from_ISR(TimerHandle_t xTimer) {
    if (xTimer) {
        uint8_t tries = 0;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        while (!xTimerStopFromISR(xTimer, &xHigherPriorityTaskWoken) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Stop from ISR Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        }
    }
}

void esp_timer_change_period(TimerHandle_t xTimer, const uint32_t new_period_ms) {
    if (xTimer) {
        uint8_t tries = 0;
        while (!xTimerChangePeriod(xTimer, pdMS_TO_TICKS(new_period_ms), XTIMER_PERIOD_BLOCK_TIME) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Change Period Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
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
        uint8_t tries = 0;
        while (!xTimerDelete(xTimer, XTIMER_BLOCK_TIME) && tries < XTIMER_MAX_TRIES) {
            tries++;
            printf("! Timer Delete Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        }
    }
}
