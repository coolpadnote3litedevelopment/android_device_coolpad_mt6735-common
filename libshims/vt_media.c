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

#include <log/log.h>

#define NAME_NOT_FOUND (-2)
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

/* android::DataSource::RegisterDefaultSniffers(); Oreo's MediaExtractor registers them itself */
void _ZN7android10DataSource23RegisterDefaultSniffersEv(void)
{
}

/*
 * android::MediaCodec::CreateByType(const sp<ALooper>&, const AString&, bool, status_t*, pid_t)
 * Oreo hands out MediaCodecBuffer instead of ABuffer, so no codec is created for these libraries.
 */
void _ZN7android10MediaCodec12CreateByTypeERKNS_2spINS_7ALooperEEERKNS_7AStringEbPii(
        void **codec, const void *looper, const void *mime, int encoder, int *err, int pid)
{
    (void)looper; (void)mime; (void)encoder; (void)pid;
    *codec = NULL;
    if (err)
        *err = NAME_NOT_FOUND;
}

/* android::MediaCodec::getInputBuffers(Vector<sp<ABuffer>>*) const */
int _ZNK7android10MediaCodec15getInputBuffersEPNS_6VectorINS_2spINS_7ABufferEEEEE(
        const void *thiz, void *buffers)
{
    (void)thiz; (void)buffers;
    return INVALID_OPERATION;
}

/* android::MediaCodec::getOutputBuffers(Vector<sp<ABuffer>>*) const */
int _ZNK7android10MediaCodec16getOutputBuffersEPNS_6VectorINS_2spINS_7ABufferEEEEE(
        const void *thiz, void *buffers)
{
    (void)thiz; (void)buffers;
    return INVALID_OPERATION;
}

/*
 * The Nougat android::AudioTrack constructor. libsink allocates 0x298 bytes for it,
 * the Oreo AudioTrack is larger, so it cannot be built in place.
 */
void *_ZN7android10AudioTrackC1E19audio_stream_type_tj14audio_format_tjj20audio_output_flags_tPFviPvS4_ES4_i15audio_session_tNS0_13transfer_typeEPK20audio_offload_info_tiiPK18audio_attributes_tbf(
        void *thiz)
{
    LOG_ALWAYS_FATAL("video call audio is not supported");
    return thiz;
}
