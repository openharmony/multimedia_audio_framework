/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef AUDIO_UTILS_C_H
#define AUDIO_UTILS_C_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif
#define SPRINTF_STRING_LEN 256
#define AUTO_CLEANUP(func) __attribute__((cleanup(func)))
#define AUTO_CLEAR AUTO_CLEANUP(CallEndAndClear)

typedef struct CTrace CTrace;

// must use string length less than 256
#define AUTO_CTRACE(fmt, args...)                                           \
    char str[SPRINTF_STRING_LEN] = {0};                                     \
    int ret = sprintf_s(str, SPRINTF_STRING_LEN, fmt, ##args);              \
    AUTO_CLEAR CTrace *tmpCtrace = (ret >= 0 ? GetAndStart(str) : NULL);    \
    (void)tmpCtrace

// must call with AUTO_CLEAR
CTrace *GetAndStart(const char *traceName);

void EndCTrace(CTrace *cTrace);

void CTraceCount(const char *traceName, int64_t count);

void CallEndAndClear(CTrace **cTrace);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_UTILS_C_H