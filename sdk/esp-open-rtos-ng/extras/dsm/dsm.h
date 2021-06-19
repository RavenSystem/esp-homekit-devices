/* Implementation of Delta-Sigma modulator support.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2018 ourairquality (https://github.com/ourairquality)
 * Copyright (C) 2018 Zaltora (https://github.com/Zaltora)
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_DSM_H_
#define EXTRAS_DSM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MAX_DSM_PINS    (8)
#define DSM_DEBUG       (0)

/*
 * Freq = (80,000,000/prescale) * (target / 256) HZ           (0   < target < 128)
 * Freq = (80,000,000/prescale) * ((256 - target) / 256)  HZ  (128 < target < 256)
 */

void dsm_init(uint8_t npins, const uint8_t* pins);
void dsm_set_prescale(uint8_t prescale);
void dsm_set_target(uint8_t target);

void dsm_start();
void dsm_stop();

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_DSM_H_ */
