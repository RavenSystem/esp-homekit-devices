/*
 * RavenSystem NTP
 *
 * Copyright 2021-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __RAVEN_NTP_H__
#define __RAVEN_NTP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

int raven_ntp_update(char* ntp_server);
time_t raven_ntp_get_time_t();
void raven_ntp_get_log_time(char* buffer, const size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif  // __RAVEN_NTP_H__
