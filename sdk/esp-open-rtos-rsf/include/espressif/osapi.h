#ifndef _OSAPI_H_
#define _OSAPI_H_

void sdk_os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void sdk_os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag);
void sdk_os_timer_disarm(ETSTimer *ptimer);

#endif
