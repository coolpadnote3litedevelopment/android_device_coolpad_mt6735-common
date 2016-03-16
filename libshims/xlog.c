/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#include <stdarg.h>
#include <stdlib.h>
#include <sys/system_properties.h>
#include <android/log.h>

struct xlog_record {
    const char *tag_str;
    const char *fmt_str;
    int prio;
};

void __attribute__((weak)) __xlog_buf_printf(__unused int bufid, const struct xlog_record *xlog_record, ...) {
    va_list args;
    va_start(args, xlog_record);
    int len = 0;
    int do_xlog = 0;
    char results[PROP_VALUE_MAX];

    // MobileLog
    len = __system_property_get ("debug.MB.running", results);
    if (len && atoi(results))
        do_xlog = 1;

    // ModemLog
    len = __system_property_get ("debug.mdlogger.Running", results);
    if (len && atoi(results))
        do_xlog = 1;

    // Manual
    len = __system_property_get ("persist.debug.xlog.enable", results);
    if (len && atoi(results))
        do_xlog = 1;

    if (do_xlog > 0)
        __android_log_vprint(xlog_record->prio, xlog_record->tag_str, xlog_record->fmt_str, args);

    va_end(args);
    return;
}
