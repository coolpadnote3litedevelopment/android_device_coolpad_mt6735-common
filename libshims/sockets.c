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

#include <stdlib.h>
#include <string.h>

#define PREFIX "ANDROID_SOCKET_"

extern char **environ;

__attribute__((constructor))
static void publish_legacy_socket_names(void)
{
    size_t n = 0, i;
    char **copy;

    while (environ[n])
        n++;

    copy = calloc(n, sizeof(*copy));
    if (!copy)
        return;

    for (i = 0; i < n; i++)
        copy[i] = strncmp(environ[i], PREFIX, strlen(PREFIX)) ? NULL : strdup(environ[i]);

    for (i = 0; i < n; i++) {
        char *name, *val, *c;

        if (!copy[i])
            continue;
        name = copy[i];
        val = strchr(name, '=');
        if (val) {
            *val++ = '\0';
            for (c = name + strlen(PREFIX); *c; c++)
                if (*c == '_')
                    *c = '-';
            setenv(name, val, 0);
        }
        free(copy[i]);
    }
    free(copy);
}
