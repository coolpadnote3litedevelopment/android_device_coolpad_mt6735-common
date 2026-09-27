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

#define LOG_TAG "AudioWrapper"

#include <errno.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>

struct wrapped_stream_in {
    struct audio_stream_in stream;
    struct audio_stream_in *vendor;
};

static struct audio_module *vendor_module;
static int (*vendor_open_input_stream)(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices,
        struct audio_config *config, struct audio_stream_in **stream_in,
        audio_input_flags_t flags, const char *address,
        audio_source_t source);
static void (*vendor_close_input_stream)(struct audio_hw_device *dev,
        struct audio_stream_in *stream_in);

#define VENDOR_IN(s) (((struct wrapped_stream_in *)(s))->vendor)

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_sample_rate(&in->common);
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.set_sample_rate(&in->common, rate);
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_buffer_size(&in->common);
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_channels(&in->common);
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_format(&in->common);
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.set_format(&in->common, format);
}

static int in_standby(struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.standby(&in->common);
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.dump(&in->common, fd);
}

static audio_devices_t in_get_device(const struct audio_stream *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_device(&in->common);
}

static int in_set_device(struct audio_stream *stream, audio_devices_t device)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.set_device(&in->common, device);
}

static int in_set_parameters(struct audio_stream *stream, const char *kv_pairs)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.set_parameters(&in->common, kv_pairs);
}

static char *in_get_parameters(const struct audio_stream *stream,
        const char *keys)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.get_parameters(&in->common, keys);
}

static int in_add_audio_effect(const struct audio_stream *stream,
        effect_handle_t effect)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.add_audio_effect(&in->common, effect);
}

static int in_remove_audio_effect(const struct audio_stream *stream,
        effect_handle_t effect)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->common.remove_audio_effect(&in->common, effect);
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->set_gain(in, gain);
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
        size_t bytes)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->read(in, buffer, bytes);
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    struct audio_stream_in *in = VENDOR_IN(stream);
    return in->get_input_frames_lost(in);
}

static int wrapper_open_input_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle, audio_devices_t devices,
        struct audio_config *config, struct audio_stream_in **stream_in,
        audio_input_flags_t flags, const char *address,
        audio_source_t source)
{
    struct wrapped_stream_in *w;
    struct audio_stream_in *in = NULL;
    int ret;

    w = calloc(1, sizeof(*w));
    if (!w)
        return -ENOMEM;

    ret = vendor_open_input_stream(dev, handle, devices, config, &in, flags,
            address, source);
    if (ret || !in) {
        free(w);
        *stream_in = NULL;
        return ret ? ret : -EINVAL;
    }

    w->vendor = in;
    w->stream.common.get_sample_rate = in_get_sample_rate;
    w->stream.common.set_sample_rate = in_set_sample_rate;
    w->stream.common.get_buffer_size = in_get_buffer_size;
    w->stream.common.get_channels = in_get_channels;
    w->stream.common.get_format = in_get_format;
    w->stream.common.set_format = in_set_format;
    w->stream.common.standby = in_standby;
    w->stream.common.dump = in_dump;
    w->stream.common.get_device = in_get_device;
    w->stream.common.set_device = in_set_device;
    w->stream.common.set_parameters = in_set_parameters;
    w->stream.common.get_parameters = in_get_parameters;
    w->stream.common.add_audio_effect = in_add_audio_effect;
    w->stream.common.remove_audio_effect = in_remove_audio_effect;
    w->stream.set_gain = in_set_gain;
    w->stream.read = in_read;
    w->stream.get_input_frames_lost = in_get_input_frames_lost;

    *stream_in = &w->stream;
    return 0;
}

static void wrapper_close_input_stream(struct audio_hw_device *dev,
        struct audio_stream_in *stream_in)
{
    struct wrapped_stream_in *w = (struct wrapped_stream_in *)stream_in;

    vendor_close_input_stream(dev, w->vendor);
    free(w);
}

static int wrapper_open(const hw_module_t *module __unused, const char *name,
        hw_device_t **device)
{
    struct audio_hw_device *dev;
    int ret;

    if (!vendor_module)
        return -ENODEV;

    ret = vendor_module->common.methods->open(&vendor_module->common, name,
            device);
    if (ret)
        return ret;

    dev = (struct audio_hw_device *)*device;
    vendor_open_input_stream = dev->open_input_stream;
    vendor_close_input_stream = dev->close_input_stream;
    dev->open_input_stream = wrapper_open_input_stream;
    dev->close_input_stream = wrapper_close_input_stream;

    return 0;
}

static struct hw_module_methods_t wrapper_module_methods = {
    .open = wrapper_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "MT6735 Audio Wrapper",
        .author = "The LineageOS Project",
        .methods = &wrapper_module_methods,
    },
};

__attribute__((constructor))
static void wrapper_load_vendor_module(void)
{
    if (hw_get_module_by_class(AUDIO_HARDWARE_MODULE_ID, "vendor",
            (const hw_module_t **)&vendor_module)) {
        ALOGE("failed to load vendor audio module");
        vendor_module = NULL;
    }
}
