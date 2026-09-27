/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <private/android_filesystem_config.h>

#define NO_ANDROID_FILESYSTEM_CONFIG_DEVICE_DIRS

/* The MTK graphics blobs need shims, and file capabilities make the
 * linker ignore LD_PRELOAD and LD_SHIM_LIBS. */
static const struct fs_path_config android_device_files[] = {
    { 00755, AID_SYSTEM, AID_GRAPHICS, 0, "system/bin/surfaceflinger" },
#ifdef NO_ANDROID_FILESYSTEM_CONFIG_DEVICE_DIRS
    { 00000, AID_ROOT, AID_ROOT, 0, "system/etc/fs_config_dirs" },
#endif
};
