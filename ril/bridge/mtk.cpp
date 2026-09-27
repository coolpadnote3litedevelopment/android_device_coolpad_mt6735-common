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

#define LOG_TAG "RilBridge"

#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>

#include <cutils/properties.h>
#include <log/log.h>

#include "bridge.h"

namespace bridge {
namespace {

enum {
    RIL_REQUEST_RESUME_REGISTRATION = 2065,
    RIL_REQUEST_SET_CALL_INDICATION = 2086,
    RIL_REQUEST_EMERGENCY_DIAL = 2087,
    RIL_REQUEST_SET_ECC_SERVICE_CATEGORY = 2088,

    RIL_UNSOL_MTK_BASE = 3000,
    RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED = 3015,
    RIL_UNSOL_RESPONSE_REGISTRATION_SUSPENDED = 3024,
    RIL_UNSOL_INCOMING_CALL_INDICATION = 3042,
    RIL_UNSOL_CALL_INFO_INDICATION = 3049,
    RIL_UNSOL_SET_ATTACH_APN = 3073,
};

/* ccmni0-3 carry regular PDNs, the IMS stack expects its PDN on ccmni4 */
const int kImsInterface = 4;
const int kCidReserved = -2;

const int kRefreshSessionReset = 6;
const char kEccListPath[] = "/system/etc/ecc_list.xml";

void sendInts(Slot &slot, int request, std::initializer_list<int> values) {
    Parcel p;
    p.writeInt32(values.size());
    for (int v : values) p.writeInt32(v);
    send(slot, request, Out::VOID, nullptr, -1, p);
}

std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string property(const char *name) {
    char value[PROPERTY_VALUE_MAX];
    property_get(name, value, "");
    return value;
}

std::string simOperator(const Slot &slot) {
    std::vector<std::string> v = split(property("gsm.sim.operator.numeric"), ',');
    return static_cast<size_t>(slot.id) < v.size() ? v[slot.id] : "";
}

/* libril hands empty strings over as NULL, which mtkrild's attach APN code dereferences */
const char *orEmpty(const char *s) {
    return s ? s : "";
}

void sendAttachApn(Slot &slot, Out out, RIL_Token t, const RIL_InitialAttachApn *iaa) {
    std::string op = simOperator(slot);
    Parcel p;
    writeString(p, iaa ? orEmpty(iaa->apn) : "");
    writeString(p, iaa ? orEmpty(iaa->protocol) : "");
    p.writeInt32(iaa ? iaa->authtype : 0);
    writeString(p, iaa ? orEmpty(iaa->username) : "");
    writeString(p, iaa ? orEmpty(iaa->password) : "");
    writeString(p, op.c_str());
    p.writeInt32(1);
    p.writeInt32(-1);
    if (!send(slot, RIL_REQUEST_SET_INITIAL_ATTACH_APN, out, t, -1, p)) {
        complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
    }
}

bool matches(const std::string &ecc, const std::string &number) {
    return number == ecc || number == ecc + "+";
}

std::string attr(const std::string &tag, const char *name) {
    std::string key = std::string(name) + "=\"";
    size_t b = tag.find(key);
    if (b == std::string::npos) return "";
    b += key.size();
    size_t e = tag.find('"', b);
    return e == std::string::npos ? "" : tag.substr(b, e - b);
}

/* Emergency numbers the way MTK's telephony resolved them: network list, SIM lists,
 * then the customised list (entries marked for no-SIM only are skipped with a SIM) */
bool emergencyCategory(const std::string &number, int *category) {
    for (const std::string &entry : split(property("ril.ecc.service.category.list"), ';')) {
        std::vector<std::string> kv = split(entry, ',');
        if (kv.size() == 2 && matches(kv[0], number)) {
            *category = atoi(kv[1].c_str());
            return true;
        }
    }

    bool simInserted = false;
    bool found = false;
    for (const char *prop : {"ril.ecclist", "ril.ecclist1"}) {
        std::vector<std::string> list = split(property(prop), ',');
        if (!list.empty()) simInserted = true;
        for (const std::string &ecc : list) {
            if (matches(ecc, number)) found = true;
        }
    }

    std::ifstream in(kEccListPath);
    std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (size_t b = xml.find("<EccEntry"); b != std::string::npos; b = xml.find("<EccEntry", b + 1)) {
        std::string tag = xml.substr(b, xml.find('>', b) - b);
        if (!matches(attr(tag, "Ecc"), number)) continue;
        if (simInserted && attr(tag, "Condition") == "0") continue;
        *category = atoi(attr(tag, "Category").c_str());
        return true;
    }

    *category = 0;
    return found;
}

std::string networkPortion(const char *address) {
    std::string out;
    for (const char *c = address; c && *c && *c != ',' && *c != ';'; c++) {
        if ((*c >= '0' && *c <= '9') || *c == '*' || *c == '#' || *c == '+') out += *c;
    }
    return out;
}

bool emergencyDial(Slot &slot, const RIL_Dial *d, RIL_Token t) {
    int category;
    if (!emergencyCategory(networkPortion(d->address), &category)) return false;

    sendInts(slot, RIL_REQUEST_SET_ECC_SERVICE_CATEGORY, {category});

    Parcel p;
    writeString(p, d->address);
    p.writeInt32(d->clir);
    p.writeInt32(0);
    writeUus(p, d->uusInfo);
    if (!send(slot, RIL_REQUEST_EMERGENCY_DIAL, Out::VOID, t, -1, p)) {
        complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
    }
    return true;
}

}  // namespace

int reserveInterface(Slot &slot, int profile) {
    std::lock_guard<std::mutex> lock(slot.pendingLock);
    if (profile == RIL_DATA_PROFILE_IMS) {
        if (slot.dataCids[kImsInterface] != -1) return -1;
        slot.dataCids[kImsInterface] = kCidReserved;
        return kImsInterface;
    }
    for (int i = 0; i < kImsInterface; i++) {
        if (slot.dataCids[i] == -1) {
            slot.dataCids[i] = kCidReserved;
            return i;
        }
    }
    return -1;
}

void releaseInterface(Slot &slot, int index, int cid) {
    if (index < 0) return;
    std::lock_guard<std::mutex> lock(slot.pendingLock);
    slot.dataCids[index] = cid >= 0 ? cid : -1;
}

bool mtkRequest(Slot &slot, int request, const void *data, size_t len, RIL_Token t) {
    switch (request) {
        case RIL_REQUEST_SETUP_DATA_CALL: {
            char *const *s = static_cast<char *const *>(data);
            int n = len / sizeof(char *);
            if (n < 7) {
                complete(t, RIL_E_INVALID_ARGUMENTS, nullptr, 0);
                return true;
            }
            int index = reserveInterface(slot, s[1] ? atoi(s[1]) : 0);
            std::string interfaceId = std::to_string(index + 1);
            Parcel p;
            p.writeInt32(8);
            /* mtkrild compares these against its attach APN cache without null checks */
            for (int i = 0; i < 7; i++) writeString(p, orEmpty(s[i]));
            writeString(p, index >= 0 ? interfaceId.c_str() : "0");
            if (!send(slot, request, Out::SETUP_DATA, t, index, p)) {
                releaseInterface(slot, index, -1);
                complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
            }
            return true;
        }
        case RIL_REQUEST_DEACTIVATE_DATA_CALL: {
            char *const *s = static_cast<char *const *>(data);
            if (len >= sizeof(char *) && s[0]) {
                int cid = atoi(s[0]);
                std::lock_guard<std::mutex> lock(slot.pendingLock);
                for (int &c : slot.dataCids) {
                    if (c == cid) c = -1;
                }
            }
            return false;
        }
        case RIL_REQUEST_DEVICE_IDENTITY:
            /* mtkrild only answers this on C2K builds; M asked for the IMEI directly */
            if (!send(slot, RIL_REQUEST_GET_IMEI, Out::IDENTITY_IMEI, t, -1, Parcel())) {
                complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
            }
            return true;
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN:
            sendAttachApn(slot, Out::VOID, t, static_cast<const RIL_InitialAttachApn *>(data));
            return true;
        case RIL_REQUEST_IMS_SEND_SMS: {
            /* the modem routes SMS over IMS on its own and has no IMS_SEND_SMS */
            const RIL_IMS_SMS_Message *m = static_cast<const RIL_IMS_SMS_Message *>(data);
            if (m->tech != RADIO_TECH_3GPP || !m->message.gsmMessage) {
                complete(t, RIL_E_REQUEST_NOT_SUPPORTED, nullptr, 0);
                return true;
            }
            Parcel p;
            p.writeInt32(2);
            writeString(p, m->message.gsmMessage[0]);
            writeString(p, m->message.gsmMessage[1]);
            if (!send(slot, RIL_REQUEST_SEND_SMS, Out::SMS, t, -1, p)) {
                complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
            }
            return true;
        }
        case RIL_REQUEST_DIAL:
            return emergencyDial(slot, static_cast<const RIL_Dial *>(data), t);
    }
    return false;
}

void mtkResponse(Slot &slot, const Pending &req, RIL_Errno err, void *data) {
    switch (req.request) {
        case RIL_REQUEST_SETUP_DATA_CALL: {
            const RIL_Data_Call_Response_v11 *dc = static_cast<const RIL_Data_Call_Response_v11 *>(data);
            releaseInterface(slot, req.dataIndex, dc && dc->status == 0 ? dc->cid : -1);
            break;
        }
        case RIL_REQUEST_ALLOW_DATA:
            if (err == RIL_E_SUCCESS) {
                unsolicited(slot, RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, nullptr, 0);
            }
            break;
    }
}

bool mtkUnsolicited(Slot &slot, int unsol, Parcel &p) {
    Arena a;

    switch (unsol) {
        case RIL_UNSOL_RESPONSE_PS_NETWORK_STATE_CHANGED:
            if (p.readInt32() > 0 && p.readInt32() != 4) {
                unsolicited(slot, RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, nullptr, 0);
            }
            return true;
        case RIL_UNSOL_RESPONSE_REGISTRATION_SUSPENDED:
            if (p.readInt32() > 0) sendInts(slot, RIL_REQUEST_RESUME_REGISTRATION, {p.readInt32()});
            return true;
        case RIL_UNSOL_INCOMING_CALL_INDICATION: {
            std::vector<char *> v = readStrings(p, a);
            if (v.size() >= 5 && v[0] && v[3] && v[4]) {
                sendInts(slot, RIL_REQUEST_SET_CALL_INDICATION, {atoi(v[3]), atoi(v[0]), atoi(v[4])});
            }
            unsolicited(slot, RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, nullptr, 0);
            return true;
        }
        case RIL_UNSOL_CALL_INFO_INDICATION: {
            std::vector<char *> v = readStrings(p, a);
            /* message 129 is the call being released */
            if (v.size() >= 2 && v[1] && !strcmp(v[1], "129")) {
                unsolicited(slot, RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, nullptr, 0);
            }
            return true;
        }
        case RIL_UNSOL_SET_ATTACH_APN:
            /* the stack keeps asking until an initial attach APN is set, even an empty one */
            sendAttachApn(slot, Out::VOID, nullptr, nullptr);
            return true;
        case RIL_UNSOL_ON_USSD: {
            std::vector<char *> v = readStrings(p, a);
            v.resize(2, nullptr);
            /* session end and handled-by-STK are normal completions */
            if (v[0] && atoi(v[0]) >= 2 && atoi(v[0]) <= 5) v[0] = a.str("0");
            unsolicited(slot, unsol, v.data(), v.size() * sizeof(char *));
            return true;
        }
        case RIL_UNSOL_SIM_REFRESH: {
            RIL_SimRefreshResponse_v7 *r = a.make<RIL_SimRefreshResponse_v7>();
            int result = p.readInt32();
            char *efId = readString(p, a);
            r->aid = readString(p, a);
            r->ef_id = efId ? atoi(efId) : 0;
            if (result > SIM_RESET) result = result == kRefreshSessionReset ? SIM_RESET : SIM_INIT;
            r->result = static_cast<RIL_SimRefreshResult>(result);
            unsolicited(slot, unsol, r, sizeof(*r));
            return true;
        }
    }
    return unsol >= RIL_UNSOL_MTK_BASE;
}

}  // namespace bridge
