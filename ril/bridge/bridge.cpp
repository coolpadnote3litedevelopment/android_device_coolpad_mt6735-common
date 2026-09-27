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

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>

#include <cutils/sockets.h>
#include <log/log.h>

#include "bridge.h"

namespace bridge {
namespace {

enum {
    RESPONSE_SOLICITED = 0,
    RESPONSE_UNSOLICITED = 1,
    RESPONSE_SOLICITED_ACK_EXP = 3,
    RESPONSE_UNSOLICITED_ACK_EXP = 4,
};

const size_t kMaxMessage = 8 * 1024;
const char *const kSockets[] = {"rild", "rild2"};

struct Entry {
    int request;
    In in;
    Out out;
};

const Entry kRequests[] = {
    {RIL_REQUEST_GET_SIM_STATUS, In::VOID, Out::SIM_STATUS},
    {RIL_REQUEST_ENTER_SIM_PIN, In::STRINGS, Out::INTS},
    {RIL_REQUEST_ENTER_SIM_PUK, In::STRINGS, Out::INTS},
    {RIL_REQUEST_ENTER_SIM_PIN2, In::STRINGS, Out::INTS},
    {RIL_REQUEST_ENTER_SIM_PUK2, In::STRINGS, Out::INTS},
    {RIL_REQUEST_CHANGE_SIM_PIN, In::STRINGS, Out::INTS},
    {RIL_REQUEST_CHANGE_SIM_PIN2, In::STRINGS, Out::INTS},
    {RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION, In::STRINGS, Out::INTS},
    {RIL_REQUEST_GET_CURRENT_CALLS, In::VOID, Out::CALL_LIST},
    {RIL_REQUEST_DIAL, In::DIAL, Out::VOID},
    {RIL_REQUEST_GET_IMSI, In::STRINGS, Out::STRING},
    {RIL_REQUEST_HANGUP, In::INTS, Out::VOID},
    {RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND, In::VOID, Out::VOID},
    {RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND, In::VOID, Out::VOID},
    {RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE, In::VOID, Out::VOID},
    {RIL_REQUEST_CONFERENCE, In::VOID, Out::VOID},
    {RIL_REQUEST_UDUB, In::VOID, Out::VOID},
    {RIL_REQUEST_LAST_CALL_FAIL_CAUSE, In::VOID, Out::FAIL_CAUSE},
    {RIL_REQUEST_SIGNAL_STRENGTH, In::VOID, Out::SIGNAL},
    {RIL_REQUEST_VOICE_REGISTRATION_STATE, In::VOID, Out::VOICE_REG},
    {RIL_REQUEST_DATA_REGISTRATION_STATE, In::VOID, Out::DATA_REG},
    {RIL_REQUEST_OPERATOR, In::VOID, Out::OPERATOR},
    {RIL_REQUEST_RADIO_POWER, In::INTS, Out::VOID},
    {RIL_REQUEST_DTMF, In::STRING, Out::VOID},
    {RIL_REQUEST_SEND_SMS, In::STRINGS, Out::SMS},
    {RIL_REQUEST_SEND_SMS_EXPECT_MORE, In::STRINGS, Out::SMS},
    {RIL_REQUEST_SETUP_DATA_CALL, In::SETUP_DATA, Out::SETUP_DATA},
    {RIL_REQUEST_SIM_IO, In::SIM_IO, Out::SIM_IO},
    {RIL_REQUEST_SEND_USSD, In::STRING, Out::VOID},
    {RIL_REQUEST_CANCEL_USSD, In::VOID, Out::VOID},
    {RIL_REQUEST_GET_CLIR, In::VOID, Out::INTS},
    {RIL_REQUEST_SET_CLIR, In::INTS, Out::VOID},
    {RIL_REQUEST_QUERY_CALL_FORWARD_STATUS, In::CALL_FORWARD, Out::CALL_FORWARD},
    {RIL_REQUEST_SET_CALL_FORWARD, In::CALL_FORWARD, Out::VOID},
    {RIL_REQUEST_QUERY_CALL_WAITING, In::INTS, Out::INTS},
    {RIL_REQUEST_SET_CALL_WAITING, In::INTS, Out::VOID},
    {RIL_REQUEST_SMS_ACKNOWLEDGE, In::INTS, Out::VOID},
    {RIL_REQUEST_GET_IMEI, In::VOID, Out::STRING},
    {RIL_REQUEST_GET_IMEISV, In::VOID, Out::STRING},
    {RIL_REQUEST_ANSWER, In::VOID, Out::VOID},
    {RIL_REQUEST_DEACTIVATE_DATA_CALL, In::STRINGS, Out::VOID},
    {RIL_REQUEST_QUERY_FACILITY_LOCK, In::STRINGS, Out::INTS},
    {RIL_REQUEST_SET_FACILITY_LOCK, In::STRINGS, Out::INTS},
    {RIL_REQUEST_CHANGE_BARRING_PASSWORD, In::STRINGS, Out::VOID},
    {RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE, In::VOID, Out::INTS},
    {RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC, In::VOID, Out::VOID},
    {RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL, In::STRING, Out::VOID},
    {RIL_REQUEST_QUERY_AVAILABLE_NETWORKS, In::VOID, Out::NETWORKS},
    {RIL_REQUEST_DTMF_START, In::STRING, Out::VOID},
    {RIL_REQUEST_DTMF_STOP, In::VOID, Out::VOID},
    {RIL_REQUEST_BASEBAND_VERSION, In::VOID, Out::STRING},
    {RIL_REQUEST_SEPARATE_CONNECTION, In::INTS, Out::VOID},
    {RIL_REQUEST_SET_MUTE, In::INTS, Out::VOID},
    {RIL_REQUEST_GET_MUTE, In::VOID, Out::INTS},
    {RIL_REQUEST_QUERY_CLIP, In::VOID, Out::INTS},
    {RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE, In::VOID, Out::INTS},
    {RIL_REQUEST_DATA_CALL_LIST, In::VOID, Out::DATA_LIST},
    {RIL_REQUEST_RESET_RADIO, In::VOID, Out::VOID},
    {RIL_REQUEST_OEM_HOOK_RAW, In::RAW, Out::RAW},
    {RIL_REQUEST_OEM_HOOK_STRINGS, In::STRINGS, Out::STRINGS},
    {RIL_REQUEST_SCREEN_STATE, In::INTS, Out::VOID},
    {RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION, In::INTS, Out::VOID},
    {RIL_REQUEST_WRITE_SMS_TO_SIM, In::SMS_WRITE, Out::INTS},
    {RIL_REQUEST_DELETE_SMS_ON_SIM, In::INTS, Out::VOID},
    {RIL_REQUEST_SET_BAND_MODE, In::INTS, Out::VOID},
    {RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE, In::VOID, Out::INTS},
    {RIL_REQUEST_STK_GET_PROFILE, In::VOID, Out::STRING},
    {RIL_REQUEST_STK_SET_PROFILE, In::STRING, Out::VOID},
    {RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND, In::STRING, Out::STRING},
    {RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE, In::STRING, Out::VOID},
    {RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM, In::INTS, Out::INTS},
    {RIL_REQUEST_EXPLICIT_CALL_TRANSFER, In::VOID, Out::VOID},
    {RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE, In::INTS, Out::VOID},
    {RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE, In::VOID, Out::INTS},
    {RIL_REQUEST_SET_LOCATION_UPDATES, In::INTS, Out::VOID},
    {RIL_REQUEST_SET_TTY_MODE, In::INTS, Out::VOID},
    {RIL_REQUEST_QUERY_TTY_MODE, In::VOID, Out::INTS},
    {RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG, In::VOID, Out::GSM_BR_CONFIG},
    {RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG, In::GSM_BR_CONFIG, Out::VOID},
    {RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION, In::INTS, Out::VOID},
    {RIL_REQUEST_DEVICE_IDENTITY, In::VOID, Out::DEVICE_ID},
    {RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE, In::VOID, Out::VOID},
    {RIL_REQUEST_GET_SMSC_ADDRESS, In::VOID, Out::STRING},
    {RIL_REQUEST_SET_SMSC_ADDRESS, In::STRING, Out::VOID},
    {RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, In::INTS, Out::VOID},
    {RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING, In::VOID, Out::VOID},
    {RIL_REQUEST_ISIM_AUTHENTICATION, In::STRING, Out::STRING},
    {RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU, In::STRINGS, Out::VOID},
    {RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS, In::STRING, Out::SIM_IO},
    {RIL_REQUEST_VOICE_RADIO_TECH, In::VOID, Out::INTS},
    {RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE, In::INTS, Out::VOID},
    {RIL_REQUEST_SET_INITIAL_ATTACH_APN, In::IA_APN, Out::VOID},
    {RIL_REQUEST_IMS_REGISTRATION_STATE, In::VOID, Out::INTS},
    {RIL_REQUEST_IMS_SEND_SMS, In::IMS_SMS, Out::SMS},
    {RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC, In::SIM_APDU, Out::SIM_IO},
    {RIL_REQUEST_SIM_OPEN_CHANNEL, In::STRING, Out::INTS},
    {RIL_REQUEST_SIM_CLOSE_CHANNEL, In::INTS, Out::VOID},
    {RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL, In::SIM_APDU, Out::SIM_IO},
    {RIL_REQUEST_ALLOW_DATA, In::INTS, Out::VOID},
    {RIL_REQUEST_SIM_AUTHENTICATION, In::SIM_AUTH, Out::SIM_IO},
    {RIL_REQUEST_SHUTDOWN, In::VOID, Out::VOID},
};

const RIL_Env *sEnv;
Slot sSlots[BRIDGE_SIM_COUNT];

const Entry *lookup(int request) {
    for (const Entry &e : kRequests) {
        if (e.request == request) return &e;
    }
    return nullptr;
}

bool writeAll(int fd, const void *data, size_t len) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    while (len > 0) {
        ssize_t n = TEMP_FAILURE_RETRY(write(fd, p, len));
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

bool readAll(int fd, void *data, size_t len) {
    uint8_t *p = static_cast<uint8_t *>(data);
    while (len > 0) {
        ssize_t n = TEMP_FAILURE_RETRY(read(fd, p, len));
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

void failPending(Slot &slot) {
    std::map<int32_t, Pending> pending;
    std::vector<Pending> deferred;
    {
        std::lock_guard<std::mutex> lock(slot.pendingLock);
        pending.swap(slot.pending);
        deferred.swap(slot.deferred);
        for (int &cid : slot.dataCids) cid = -1;
    }
    for (auto &it : pending) {
        if (it.second.token) complete(it.second.token, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
    }
    for (const Pending &req : deferred) complete(req.token, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
}

/*
 * The framework asks for the IMEI once, while it still has both radios off, and mtkrild
 * powers the modem down entirely when that happens.
 */
bool isIdentity(int request) {
    return request == RIL_REQUEST_DEVICE_IDENTITY || request == RIL_REQUEST_GET_IMEI ||
           request == RIL_REQUEST_GET_IMEISV;
}

void resendDeferred(Slot &slot) {
    std::vector<Pending> deferred;
    {
        std::lock_guard<std::mutex> lock(slot.pendingLock);
        deferred.swap(slot.deferred);
    }
    for (const Pending &req : deferred) {
        if (!send(slot, req.request, req.out, req.token, -1, Parcel())) {
            complete(req.token, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
        }
    }
}

void setState(Slot &slot, RIL_RadioState state) {
    slot.state = state;
    unsolicited(slot, RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, nullptr, 0);
    if (state == RADIO_STATE_ON) resendDeferred(slot);
}

void handleSolicited(Slot &slot, Parcel &p) {
    int32_t serial = p.readInt32();
    int32_t err = p.readInt32();
    Pending req;
    {
        std::lock_guard<std::mutex> lock(slot.pendingLock);
        auto it = slot.pending.find(serial);
        if (it == slot.pending.end()) {
            ALOGW("slot %d: response for unknown serial %d", slot.id, serial);
            return;
        }
        req = it->second;
        slot.pending.erase(it);
        if (err == RIL_E_RADIO_NOT_AVAILABLE && req.token && isIdentity(req.request)) {
            slot.deferred.push_back(req);
            return;
        }
    }
    readResponse(slot, req, static_cast<RIL_Errno>(err), p);
}

void handleUnsolicited(Slot &slot, Parcel &p) {
    int32_t unsol = p.readInt32();
    switch (unsol) {
        case RIL_UNSOL_RIL_CONNECTED:
            if (p.readInt32() == 1) slot.rilVersion = p.readInt32();
            ALOGI("slot %d: connected to RIL version %d", slot.id, slot.rilVersion);
            return;
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED:
            setState(slot, static_cast<RIL_RadioState>(p.readInt32()));
            return;
    }
    if (!mtkUnsolicited(slot, unsol, p)) readUnsolicited(slot, unsol, p);
}

void readerLoop(Slot *slot) {
    std::vector<uint8_t> buf(kMaxMessage);
    bool logged = false;

    for (;;) {
        int fd = socket_local_client(slot->socket, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
        if (fd < 0) {
            if (!logged) ALOGW("slot %d: waiting for %s: %s", slot->id, slot->socket, strerror(errno));
            logged = true;
            sleep(1);
            continue;
        }
        logged = false;
        ALOGI("slot %d: connected to %s", slot->id, slot->socket);
        {
            std::lock_guard<std::mutex> lock(slot->writeLock);
            slot->fd = fd;
        }

        for (;;) {
            uint32_t header;
            if (!readAll(fd, &header, sizeof(header))) break;
            size_t len = ntohl(header);
            if (len > buf.size()) buf.resize(len);
            if (!readAll(fd, buf.data(), len)) break;

            Parcel p;
            p.setData(buf.data(), len);
            int32_t type = p.readInt32();
            if (type == RESPONSE_SOLICITED || type == RESPONSE_SOLICITED_ACK_EXP) {
                handleSolicited(*slot, p);
            } else if (type == RESPONSE_UNSOLICITED || type == RESPONSE_UNSOLICITED_ACK_EXP) {
                handleUnsolicited(*slot, p);
            }
        }

        ALOGE("slot %d: lost %s", slot->id, slot->socket);
        {
            std::lock_guard<std::mutex> lock(slot->writeLock);
            slot->fd = -1;
        }
        close(fd);
        failPending(*slot);
        setState(*slot, RADIO_STATE_UNAVAILABLE);
        sleep(1);
    }
}

void onRequest(int request, void *data, size_t len, RIL_Token t, RIL_SOCKET_ID id) {
    Slot &slot = sSlots[id < BRIDGE_SIM_COUNT ? id : 0];

    if (mtkRequest(slot, request, data, len, t)) return;

    const Entry *e = lookup(request);
    if (e == nullptr) {
        complete(t, RIL_E_REQUEST_NOT_SUPPORTED, nullptr, 0);
        return;
    }
    Parcel payload;
    if (!writeRequest(e->in, payload, data, len)) {
        complete(t, RIL_E_INVALID_ARGUMENTS, nullptr, 0);
        return;
    }
    if (!send(slot, request, e->out, t, -1, payload)) {
        complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
    }
}

RIL_RadioState onStateRequest(RIL_SOCKET_ID id) {
    return sSlots[id < BRIDGE_SIM_COUNT ? id : 0].state;
}

int onSupports(int request) {
    return lookup(request) != nullptr;
}

void onCancel(RIL_Token) {}

const char *getVersion() {
    return "mtk-ril-bridge";
}

const RIL_RadioFunctions sFunctions = {
    RIL_VERSION, onRequest, onStateRequest, onSupports, onCancel, getVersion,
};

}  // namespace

bool send(Slot &slot, int request, Out out, RIL_Token t, int dataIndex, const Parcel &payload) {
    int32_t serial;
    {
        std::lock_guard<std::mutex> lock(slot.pendingLock);
        serial = ++slot.serial;
        slot.pending[serial] = {t, request, out, dataIndex};
    }

    Parcel p;
    p.writeInt32(request);
    p.writeInt32(serial);
    p.appendFrom(&payload, 0, payload.dataSize());
    uint32_t header = htonl(p.dataSize());

    bool ok;
    {
        std::lock_guard<std::mutex> lock(slot.writeLock);
        ok = slot.fd >= 0 && writeAll(slot.fd, &header, sizeof(header)) &&
             writeAll(slot.fd, p.data(), p.dataSize());
    }
    if (!ok) {
        std::lock_guard<std::mutex> lock(slot.pendingLock);
        slot.pending.erase(serial);
    }
    return ok;
}

void complete(RIL_Token t, RIL_Errno err, void *data, size_t len) {
    if (t) sEnv->OnRequestComplete(t, err, data, len);
}

void unsolicited(Slot &slot, int unsol, const void *data, size_t len) {
    sEnv->OnUnsolicitedResponse(unsol, data, len, static_cast<RIL_SOCKET_ID>(slot.id));
}

}  // namespace bridge

extern "C" const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int, char **) {
    using namespace bridge;

    sEnv = env;
    for (int i = 0; i < BRIDGE_SIM_COUNT; i++) {
        sSlots[i].id = i;
        sSlots[i].socket = kSockets[i];
        std::thread(readerLoop, &sSlots[i]).detach();
    }
    return &sFunctions;
}
