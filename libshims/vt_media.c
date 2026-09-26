/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* MediaTek media additions used by the video call libraries. Video call audio is not supported. */

#include <stddef.h>
#include <stdint.h>

#define INVALID_OPERATION (-38)

/* android::AudioPCMxWay::AudioPCMxWay(int, void (*)(int, void*, void*), void*) */
void _ZN7android12AudioPCMxWayC1EiPFviPvS1_ES1_(void *thiz, int type, void *cb, void *user)
{
    (void)thiz; (void)type; (void)cb; (void)user;
}

/* android::AudioPCMxWay::~AudioPCMxWay() */
void _ZN7android12AudioPCMxWayD1Ev(void *thiz)
{
    (void)thiz;
}

int _ZN7android12AudioPCMxWay5startEv(void *thiz)
{
    (void)thiz;
    return INVALID_OPERATION;
}

int _ZN7android12AudioPCMxWay4stopEv(void *thiz)
{
    (void)thiz;
    return INVALID_OPERATION;
}

int _ZN7android12AudioPCMxWay4readEPvj(void *thiz, void *buf, uint32_t size)
{
    (void)thiz; (void)buf; (void)size;
    return INVALID_OPERATION;
}

/* android::FindAVCSPSInfo(uint8_t*, size_t, android::SPSInfo*) */
int _ZN7android14FindAVCSPSInfoEPhjPNS_7SPSInfoE(uint8_t *data, size_t size, void *info)
{
    (void)data; (void)size; (void)info;
    return INVALID_OPERATION;
}
