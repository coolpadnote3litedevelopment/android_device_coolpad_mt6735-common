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

/* MediaTek libnetutils additions used by mtk-ril, backed by the ccmni driver ioctls. */

#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/sockios.h>

#define SIOCSTXQSTATE   (SIOCDEVPRIVATE + 0)
#define SIOCCCMNICFG    (SIOCDEVPRIVATE + 1)

static int ccmni_ioctl(const char *ifname, unsigned long cmd, int value)
{
    struct ifreq ifr;
    int sock, ret;

    sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
        return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_ifru.ifru_ivalue = value;

    ret = ioctl(sock, cmd, &ifr);
    close(sock);

    return ret;
}

int ifc_set_txq_state(const char *ifname, int state)
{
    return ccmni_ioctl(ifname, SIOCSTXQSTATE, state);
}

int ifc_ccmni_md_cfg(const char *ifname, int md_id)
{
    return ccmni_ioctl(ifname, SIOCCCMNICFG, md_id);
}
