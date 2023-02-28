/*
 * Home Accessory Architect
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_IRRF_CODE_H__
#define __HAA_IRRF_CODE_H__

//#define IRRF_CODE_DIGITS        (2)
#define IRRF_CODE_LEN           (83)
#define IRRF_CODE_LEN_2         (IRRF_CODE_LEN * IRRF_CODE_LEN)
#define IRRF_CODE_SCALE         (5)

const char baseRaw_dic[] = "0ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789+/!@#$%&()=?*,.;:-_<>";
const char baseUC_dic[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char baseLC_dic[] = "abcdefghijklmnopqrstuvwxyz";

#endif  // __HAA_IRRF_CODE_H__
