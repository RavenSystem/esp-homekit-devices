/*
 * Home Accessory Architect
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HAA_IR_CODE_H__
#define __HAA_IR_CODE_H__

//#define IR_CODE_DIGITS          2
#define IR_CODE_LEN             83
#define IR_CODE_LEN_2           (IR_CODE_LEN * IR_CODE_LEN)
#define IR_CODE_SCALE           5

const char baseRaw_dic[] = "0ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789+/!@#$%&()=?*,.;:-_<>";
const char baseUC_dic[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char baseLC_dic[] = "abcdefghijklmnopqrstuvwxyz";

#endif  // __HAA_IR_CODE_H__
