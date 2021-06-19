/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

//*****************************************************************************
//
// Include OS functionality.
//
//*****************************************************************************

/* ------------------------ System architecture includes ----------------------------- */
#include "arch/sys_arch.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/opt.h"

#include "lwip/err.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "lwip/tcpip.h"

#if configUSE_16_BIT_TICKS == 1
#error This port requires 32 bit ticks or timer overflow will fail
#endif

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates and returns a new semaphore. The "ucCount" argument specifies
 *      the initial state of the semaphore.
 *      NOTE: Currently this routine only creates counts of 1 or 0
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      u8_t ucCount            -- Initial ucCount of semaphore (1 or 0)
 * Outputs:
 *      sys_sem_t               -- Created semaphore or 0 if could not create.
 *---------------------------------------------------------------------------*/
err_t sys_sem_new(sys_sem_t *pxSemaphore, u8_t initial_count)
{
    LWIP_ASSERT("initial_count invalid (not 0 or 1)",
                (initial_count == 0) || (initial_count == 1));

    *pxSemaphore = xSemaphoreCreateBinary();
    if (*pxSemaphore == NULL) {
        SYS_STATS_INC(sem.err);
        return ERR_MEM;
    }
    SYS_STATS_INC_USED(sem);

    if (initial_count == 1) {
        BaseType_t ret = xSemaphoreGive(*pxSemaphore);
        LWIP_ASSERT("sys_sem_new: initial give failed", ret == pdTRUE);
    }
    return ERR_OK;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to free
 *---------------------------------------------------------------------------*/
void sys_sem_free(sys_sem_t *pxSemaphore)
{
    SYS_STATS_DEC(sem.used);
    vSemaphoreDelete(*pxSemaphore);
    *pxSemaphore = NULL;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_sem_signal
 *---------------------------------------------------------------------------*
 * Description:
 *      Signals (releases) a semaphore
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to signal
 *---------------------------------------------------------------------------*/
void sys_sem_signal(sys_sem_t *pxSemaphore)
{
    xSemaphoreGive(*pxSemaphore);
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_sem_wait
 *---------------------------------------------------------------------------*
 * Description:

 *      Blocks the thread while waiting for the semaphore to be signaled. If the
 *      "timeout" argument is non-zero, the thread should only be blocked for
 *      the specified time (measured in milliseconds). If the "timeout" argument
 *      is zero, the thread should be blocked until the semaphore is signalled.
 *
 *      The return value is SYS_ARCH_TIMEOUT if the semaphore wasn't signaled
 *      within the specified time or any other value if it was signaled (with or
 *      without waiting).
 *
 *      Notice that lwIP implements a function with a similar name,
 *      sys_sem_wait(), that uses the sys_arch_sem_wait() function.
 * Inputs:
 *      sys_sem_t sem           -- Semaphore to wait on
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- SYS_ARCH_TIMEOUT on timeout, any other value on success
 *---------------------------------------------------------------------------*/
u32_t sys_arch_sem_wait(sys_sem_t *pxSemaphore, u32_t timeout_ms)
{
    if (timeout_ms == 0) {
        /* Wait infinite */
        while (xSemaphoreTake(*pxSemaphore, portMAX_DELAY) != pdTRUE);
        return 0;
    }

    if (xSemaphoreTake(*pxSemaphore, timeout_ms / portTICK_PERIOD_MS) == pdTRUE) {
        return 0;
    } else {
        return SYS_ARCH_TIMEOUT;
    }
}

/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex */
err_t sys_mutex_new(sys_mutex_t *pxMutex)
{
    *pxMutex = xSemaphoreCreateRecursiveMutex();

    if (*pxMutex == NULL) {
        SYS_STATS_INC(mutex.err);
        return ERR_MEM;
    }

    SYS_STATS_INC_USED(mutex);
    return ERR_OK;
}

/** Lock a mutex
 * @param mutex the mutex to lock */
void sys_mutex_lock(sys_mutex_t *pxMutex)
{
    while (xSemaphoreTakeRecursive(*pxMutex, portMAX_DELAY) != pdTRUE);
}

/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *pxMutex)
{
    BaseType_t ret = xSemaphoreGiveRecursive(*pxMutex);
    LWIP_ASSERT("failed to give the mutex", ret == pdTRUE);
}


/** Delete a semaphore
 * @param mutex the mutex to delete */
void sys_mutex_free(sys_mutex_t *pxMutex)
{
    SYS_STATS_DEC(mutex.used);
    vSemaphoreDelete(*pxMutex);
    *pxMutex = NULL;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Creates a new mailbox
 * Inputs:
 *      int size                -- Size of elements in the mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    LWIP_ASSERT("size > 0", size > 0);

    *mbox = xQueueCreate(size, sizeof(void *));

    if (*mbox == NULL) {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }

    SYS_STATS_INC_USED(mbox);
    return ERR_OK;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_free
 *---------------------------------------------------------------------------*
 * Description:
 *      Deallocates a mailbox. If there are messages still present in the
 *      mailbox when the mailbox is deallocated, it is an indication of a
 *      programming error in lwIP and the developer should be notified.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 * Outputs:
 *      sys_mbox_t              -- Handle to new mailbox
 *---------------------------------------------------------------------------*/
void sys_mbox_free(sys_mbox_t *mbox)
{
    UBaseType_t msgs_waiting;

    msgs_waiting = uxQueueMessagesWaiting(*mbox);
    configASSERT(msgs_waiting == 0);

#if SYS_STATS
    if (msgs_waiting != 0) {
        SYS_STATS_INC(mbox.err);
    }
    SYS_STATS_DEC(mbox.used);
#endif /* SYS_STATS */

    vQueueDelete(*mbox);
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_post
 *---------------------------------------------------------------------------*
 * Description:
 *      Post the "msg" to the mailbox.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *data              -- Pointer to data to post
 *---------------------------------------------------------------------------*/
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE);
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_mbox_trypost
 *---------------------------------------------------------------------------*
 * Description:
 *      Try to post the "msg" to the mailbox.  Returns immediately with
 *      error if cannot.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void *msg               -- Pointer to data to post
 * Outputs:
 *      err_t                   -- ERR_OK if message posted, else ERR_MEM
 *                                  if not.
 *---------------------------------------------------------------------------*/
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE) {
        return ERR_OK;
    }

    /* The queue was already full. */
    SYS_STATS_INC(mbox.err);
    return ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg)
{
    /* Quietly fail - not implemented. */
    return ERR_MEM;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_fetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Blocks the thread until a message arrives in the mailbox, but does
 *      not block the thread longer than "timeout" milliseconds (similar to
 *      the sys_arch_sem_wait() function). The "msg" argument is a result
 *      parameter that is set by the function (i.e., by doing "*msg =
 *      ptr"). The "msg" parameter maybe NULL to indicate that the message
 *      should be dropped.
 *
 *      The return values are the same as for the sys_arch_sem_wait() function:
 *      SYS_ARCH_TIMEOUT if there was a timeout, any other value if a messages
 *      is received.
 *
 *      Note that a function with a similar name, sys_mbox_fetch(), is
 *      implemented by lwIP.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 *      u32_t timeout           -- Number of milliseconds until timeout
 * Outputs:
 *      u32_t                   -- SYS_ARCH_TIMEOUT on timeout, any other value if a message has been received
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    void *msg_dummy;

    if (msg == NULL) {
        msg = &msg_dummy;
    }

    if (timeout == 0) {
        while (xQueueReceive(*mbox, &(*msg), portMAX_DELAY) != pdTRUE);
        return 0;
    }

    if (xQueueReceive(*mbox, &(*msg), timeout / portTICK_PERIOD_MS) == pdTRUE) {
        return 0;
    }

    /* Timed out. */
    *msg = NULL;
    return SYS_ARCH_TIMEOUT;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_mbox_tryfetch
 *---------------------------------------------------------------------------*
 * Description:
 *      Similar to sys_arch_mbox_fetch, but if message is not ready
 *      immediately, we'll return with SYS_MBOX_EMPTY.  On success, 0 is
 *      returned.
 * Inputs:
 *      sys_mbox_t mbox         -- Handle of mailbox
 *      void **msg              -- Pointer to pointer to msg received
 * Outputs:
 *      u32_t                   -- SYS_MBOX_EMPTY if no messages.  Otherwise,
 *                                  return ERR_OK.
 *---------------------------------------------------------------------------*/
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *msg_dummy;

    if (msg == NULL) {
        msg = &msg_dummy;
    }

    if (xQueueReceive(*mbox, &(*msg), 0) == pdTRUE) {
        return ERR_OK;
    }

    *msg = NULL;
    return SYS_MBOX_EMPTY;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_init
 *---------------------------------------------------------------------------*
 * Description:
 *      Initialize sys arch
 *---------------------------------------------------------------------------*/
void sys_init(void)
{
}

u32_t sys_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void sys_arch_msleep(u32_t delay_ms)
{
    TickType_t delay_ticks = delay_ms / portTICK_PERIOD_MS;
    vTaskDelay(delay_ticks);
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_thread_new
 *---------------------------------------------------------------------------*
 * Description:
 *      Starts a new thread with priority "prio" that will begin its
 *      execution in the function "thread()". The "arg" argument will be
 *      passed as an argument to the thread() function. The id of the new
 *      thread is returned. Both the id and the priority are system
 *      dependent.
 * Inputs:
 *      char *name              -- Name of thread
 *      void (* thread)(void *arg) -- Pointer to function to run.
 *      void *arg               -- Argument passed into function
 *      int stacksize           -- Required stack amount in bytes
 *      int prio                -- Thread priority
 * Outputs:
 *      sys_thread_t            -- Pointer to per-thread timeouts.
 *---------------------------------------------------------------------------*/
sys_thread_t sys_thread_new(const char *pcName, void(*pxThread)(void *pvParameters), void *pvArg, int stacksize, int iPriority)
{
    TaskHandle_t xCreatedTask;
    BaseType_t xResult;

    xResult = xTaskCreate(pxThread, pcName, stacksize, pvArg, iPriority, &xCreatedTask);
    LWIP_ASSERT("task creation failed", xResult == pdPASS);

    if (xResult == pdPASS) {
        return xCreatedTask;
    }

    return NULL;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_protect
 *---------------------------------------------------------------------------*
 * Description:
 *      This optional function does a "fast" critical region protection and
 *      returns the previous protection level. This function is only called
 *      during very short critical regions. An embedded system which supports
 *      ISR-based drivers might want to implement this function by disabling
 *      interrupts. Task-based systems might want to implement this by using
 *      a mutex or disabling tasking. This function should support recursive
 *      calls from the same task or interrupt. In other words,
 *      sys_arch_protect() could be called while already protected. In
 *      that case the return value indicates that it is already protected.
 *
 *      sys_arch_protect() is only required if your port is supporting an
 *      operating system.
 * Outputs:
 *      sys_prot_t              -- Previous protection level (not used here)
 *---------------------------------------------------------------------------*/
static sys_prot_t critical_nesting;
sys_prot_t sys_arch_protect(void)
{
    sys_prot_t prev;
    taskENTER_CRITICAL();
    prev = critical_nesting;
    critical_nesting++;
    return prev;
    //return (sys_prot_t)1;
}

/*---------------------------------------------------------------------------*
 * Routine:  sys_arch_unprotect
 *---------------------------------------------------------------------------*
 * Description:
 *      This optional function does a "fast" set of critical region
 *      protection to the value specified by pval. See the documentation for
 *      sys_arch_protect() for more information. This function is only
 *      required if your port is supporting an operating system.
 * Inputs:
 *      sys_prot_t              -- Previous protection level (not used here)
 *---------------------------------------------------------------------------*/
void sys_arch_unprotect(sys_prot_t pval)
{
    //(void) xValue;
    critical_nesting--;
    //LWIP_ASSERT("unexpected critical_nestion", pval == critical_nesting);
    if (pval != critical_nesting) {
        printf("lwip nesting %d\n", critical_nesting);
    }
    taskEXIT_CRITICAL();
}

#if LWIP_TCPIP_CORE_LOCKING

/** Flag the core lock held. A counter for recusive locks. */
u8_t lwip_core_lock_count;
TaskHandle_t lwip_core_lock_holder_thread;
void sys_lock_tcpip_core(void)
{
    sys_mutex_lock(&lock_tcpip_core);
    if (lwip_core_lock_count == 0) {
        lwip_core_lock_holder_thread = xTaskGetCurrentTaskHandle();
    }
    lwip_core_lock_count++;
}

void sys_unlock_tcpip_core(void)
{
    lwip_core_lock_count--;
    if (lwip_core_lock_count == 0) {
        lwip_core_lock_holder_thread = 0;
    }
    sys_mutex_unlock(&lock_tcpip_core);
}

#endif /* LWIP_TCPIP_CORE_LOCKING */

TaskHandle_t lwip_tcpip_thread;
void sys_mark_tcpip_thread(void)
{
    lwip_tcpip_thread = xTaskGetCurrentTaskHandle();
}

void sys_check_core_locking(void)
{
    /* Embedded systems should check we are NOT in an interrupt context here */

    if (lwip_tcpip_thread != 0) {
        TaskHandle_t current_thread = xTaskGetCurrentTaskHandle();

#if LWIP_TCPIP_CORE_LOCKING
        if (current_thread != lwip_core_lock_holder_thread ||
            lwip_core_lock_count == 0) {
            printf("Function called without core lock\n");
        }
        //LWIP_ASSERT("Function called without core lock", current_thread == lwip_core_lock_holder_thread && lwip_core_lock_count > 0);
#else
        if (current_thread != lwip_tcpip_thread) {
            printf("Function called from wrong thread\n");
        }
        //LWIP_ASSERT("Function called from wrong thread", current_thread == lwip_tcpip_thread);
#endif /* LWIP_TCPIP_CORE_LOCKING */
    }
}

/*-------------------------------------------------------------------------*
 * End of File:  sys_arch.c
 *-------------------------------------------------------------------------*/
