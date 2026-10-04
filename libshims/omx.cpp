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

#include <new>
#include <stdlib.h>

/*
 * MtkOmxVdec and MtkOmxVenc never set nSize in their port definitions and
 * hand them back from GetParameter as is, so the components need zeroed
 * memory to pass the IOMX size check.
 */

void *operator new(size_t size) {
    void *p = calloc(1, size ? size : 1);
    if (!p) abort();
    return p;
}

void *operator new(size_t size, const std::nothrow_t &) noexcept {
    return calloc(1, size ? size : 1);
}
