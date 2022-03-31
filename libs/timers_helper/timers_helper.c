/*
 * ESP Timers Helper
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdio.h>

#include "timers_helper.h"

#define XTIMER_MAX_TRIES                (5)

BaseType_t esp_timer_manager(const uint8_t option, TimerHandle_t xTimer, TickType_t xBlockTime) {
    if (xTimer) {
        switch (option) {
            case TIMER_MANAGER_STOP:
                return xTimerStop(xTimer, xBlockTime);
                
            case TIMER_MANAGER_DELETE:
                return xTimerDelete(xTimer, xBlockTime);
                
            default:    // TIMER_MANAGER_START:
                return xTimerStart(xTimer, xBlockTime);
        }
    }
    
    return pdFALSE;
}

BaseType_t esp_timer_change_period_manager(TimerHandle_t xTimer, const uint32_t new_period_ms, TickType_t xBlockTime) {
    if (xTimer) {
        return xTimerChangePeriod(xTimer, pdMS_TO_TICKS(new_period_ms), xBlockTime);
    }
    
    return pdFALSE;
}

BaseType_t IRAM esp_timer_manager_from_ISR(const uint8_t option, TimerHandle_t xTimer) {
    if (xTimer) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        switch (option) {
            case TIMER_MANAGER_STOP:
                return xTimerStopFromISR(xTimer, &xHigherPriorityTaskWoken);
                
            default:    // TIMER_MANAGER_START:
                return xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken);
        }
    }
    
    return pdFALSE;
}

TimerHandle_t esp_timer_create(const uint32_t period_ms, const bool auto_reload, void* pvTimerID, TimerCallbackFunction_t pxCallbackFunction) {
    UBaseType_t uxAutoReload = pdFALSE;
    if (auto_reload) {
        uxAutoReload = pdTRUE;
    }
    
    uint8_t tries = 0;
    
    TimerHandle_t result = xTimerCreate(0, pdMS_TO_TICKS(period_ms), uxAutoReload, pvTimerID, pxCallbackFunction);
    while (!result) {
        tries++;
        printf("! Timer Create Failed (%i/%i)\n", tries, XTIMER_MAX_TRIES);
        if (tries == XTIMER_MAX_TRIES) {
            break;
        }
        vTaskDelay(tries);
        result = xTimerCreate(0, pdMS_TO_TICKS(period_ms), uxAutoReload, pvTimerID, pxCallbackFunction);
    }
    
    return result;
}
