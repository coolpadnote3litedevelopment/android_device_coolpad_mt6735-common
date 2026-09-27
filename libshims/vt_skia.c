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

/* Skia APIs removed in Oreo that libvtmal uses for video call snapshots. */

#include <stdbool.h>
#include <stddef.h>

/* SkImageDecoder::DecodeFile(const char*, SkBitmap*, SkColorType, Mode, Format*) */
bool _ZN14SkImageDecoder10DecodeFileEPKcP8SkBitmap11SkColorTypeNS_4ModeEPNS_6FormatE(
        const char *file, void *bitmap, int colorType, int mode, void *format)
{
    (void)file; (void)bitmap; (void)colorType; (void)mode; (void)format;
    return false;
}

/* SkImageEncoder::EncodeFile(const char*, const SkBitmap&, Type, int) */
bool _ZN14SkImageEncoder10EncodeFileEPKcRK8SkBitmapNS_4TypeEi(
        const char *file, const void *bitmap, int type, int quality)
{
    (void)file; (void)bitmap; (void)type; (void)quality;
    return false;
}

/* SkImageEncoder::EncodeStream(SkWStream*, const SkBitmap&, Type, int) */
bool _ZN14SkImageEncoder12EncodeStreamEP9SkWStreamRK8SkBitmapNS_4TypeEi(
        void *stream, const void *bitmap, int type, int quality)
{
    (void)stream; (void)bitmap; (void)type; (void)quality;
    return false;
}

/* SkMemoryWStream::SkMemoryWStream(void*, size_t); only handed to EncodeStream */
void *_ZN15SkMemoryWStreamC1EPvj(void *thiz, void *buffer, size_t size)
{
    (void)buffer; (void)size;
    return thiz;
}

/* SkBitmap::tryAllocPixels(Allocator*, SkColorTable*) */
bool _ZN8SkBitmap14tryAllocPixelsEPNS_9AllocatorEP12SkColorTable(
        void *thiz, void *allocator, void *ctable)
{
    (void)thiz; (void)allocator; (void)ctable;
    return false;
}
