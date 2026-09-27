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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <log/log.h>
#include <utils/String16.h>
#include <utils/String8.h>

#include "bridge.h"

namespace bridge {

using android::String16;
using android::String8;

char *Arena::str(const char *s, size_t len) {
    mStrings.emplace_back(s, len);
    return &mStrings.back()[0];
}

void *Arena::bytes(const void *data, size_t len) {
    mBlobs.emplace_back(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + len);
    return mBlobs.back().data();
}

void writeString(Parcel &p, const char *s) {
    if (s == nullptr) {
        p.writeInt32(-1);
    } else {
        p.writeString16(String16(s));
    }
}

char *readString(Parcel &p, Arena &a) {
    size_t len;
    const char16_t *s = p.readString16Inplace(&len);
    if (s == nullptr) return nullptr;
    String8 s8(s, len);
    return a.str(s8.string(), s8.length());
}

std::vector<char *> readStrings(Parcel &p, Arena &a) {
    std::vector<char *> v;
    int32_t n = p.readInt32();
    for (int32_t i = 0; i < n && p.dataAvail() > 0; i++) v.push_back(readString(p, a));
    return v;
}

namespace {

int readIntOr(Parcel &p, int fallback) {
    return p.dataAvail() >= sizeof(int32_t) ? p.readInt32() : fallback;
}

void writeBytes(Parcel &p, const void *data, int len) {
    p.writeInt32(len);
    if (len > 0) p.write(data, len);
}

void deliverStrings(RIL_Token t, RIL_Errno err, std::vector<char *> &v) {
    complete(t, err, v.empty() ? nullptr : v.data(), v.size() * sizeof(char *));
}

/* libril checks the exact number of strings for these */
void resizeStrings(std::vector<char *> &v, size_t n) {
    v.resize(n, nullptr);
}

void readCardStatus(Parcel &p, Arena &a, RIL_CardStatus_v6 &cs) {
    cs.card_state = static_cast<RIL_CardState>(p.readInt32());
    cs.universal_pin_state = static_cast<RIL_PinState>(p.readInt32());
    cs.gsm_umts_subscription_app_index = p.readInt32();
    cs.cdma_subscription_app_index = p.readInt32();
    cs.ims_subscription_app_index = p.readInt32();
    int n = p.readInt32();
    if (n > RIL_CARD_MAX_APPS) n = RIL_CARD_MAX_APPS;
    cs.num_applications = n < 0 ? 0 : n;
    for (int i = 0; i < cs.num_applications; i++) {
        RIL_AppStatus &app = cs.applications[i];
        app.app_type = static_cast<RIL_AppType>(p.readInt32());
        app.app_state = static_cast<RIL_AppState>(p.readInt32());
        app.perso_substate = static_cast<RIL_PersoSubstate>(p.readInt32());
        app.aid_ptr = readString(p, a);
        app.app_label_ptr = readString(p, a);
        app.pin1_replaced = p.readInt32();
        app.pin1 = static_cast<RIL_PinState>(p.readInt32());
        app.pin2 = static_cast<RIL_PinState>(p.readInt32());
    }
    /* libril drops the whole status if any index points past the apps */
    int *index[] = {&cs.gsm_umts_subscription_app_index, &cs.cdma_subscription_app_index,
                    &cs.ims_subscription_app_index};
    for (int *i : index) {
        if (*i >= cs.num_applications) *i = -1;
    }
}

void readSignal(Parcel &p, RIL_SignalStrength_v10 &ss) {
    ss.GW_SignalStrength.signalStrength = readIntOr(p, 99);
    ss.GW_SignalStrength.bitErrorRate = readIntOr(p, -1);
    ss.CDMA_SignalStrength.dbm = readIntOr(p, -1);
    ss.CDMA_SignalStrength.ecio = readIntOr(p, -1);
    ss.EVDO_SignalStrength.dbm = readIntOr(p, -1);
    ss.EVDO_SignalStrength.ecio = readIntOr(p, -1);
    ss.EVDO_SignalStrength.signalNoiseRatio = readIntOr(p, -1);
    ss.LTE_SignalStrength.signalStrength = readIntOr(p, 99);
    ss.LTE_SignalStrength.rsrp = readIntOr(p, INT_MAX);
    ss.LTE_SignalStrength.rsrq = readIntOr(p, INT_MAX);
    ss.LTE_SignalStrength.rssnr = readIntOr(p, INT_MAX);
    ss.LTE_SignalStrength.cqi = readIntOr(p, INT_MAX);
    ss.LTE_SignalStrength.timingAdvance = INT_MAX;
    ss.TD_SCDMA_SignalStrength.rscp = readIntOr(p, INT_MAX);
}

void readDataCall(Parcel &p, Arena &a, int version, RIL_Data_Call_Response_v11 &dc) {
    if (version < 5) {
        dc.cid = p.readInt32();
        dc.active = p.readInt32();
        dc.type = readString(p, a);
        if (version < 4) readString(p, a);
        dc.addresses = readString(p, a);
        return;
    }
    dc.status = p.readInt32();
    dc.suggestedRetryTime = p.readInt32();
    dc.cid = p.readInt32();
    dc.active = p.readInt32();
    dc.type = readString(p, a);
    dc.ifname = readString(p, a);
    dc.addresses = readString(p, a);
    dc.dnses = readString(p, a);
    dc.gateways = readString(p, a);
    if (version >= 10) dc.pcscf = readString(p, a);
    dc.mtu = version >= 11 ? p.readInt32() : 0;
}

void readSetupDataCall(Parcel &p, Arena &a, RIL_Data_Call_Response_v11 &dc) {
    int version = p.readInt32();
    int n = p.readInt32();
    if (version >= 5) {
        readDataCall(p, a, version, dc);
        return;
    }
    char *s[6] = {};
    for (int i = 0; i < n && i < 6; i++) s[i] = readString(p, a);
    dc.status = 0;
    dc.active = 2;
    dc.cid = s[0] ? atoi(s[0]) : -1;
    dc.ifname = s[1];
    dc.addresses = s[2];
    dc.dnses = s[3];
    dc.gateways = s[4];
    dc.pcscf = s[5];
}

RIL_Data_Call_Response_v11 *readDataCallList(Parcel &p, Arena &a, int *count) {
    int version = p.readInt32();
    int n = p.readInt32();
    if (n < 0) n = 0;
    RIL_Data_Call_Response_v11 *list = a.array<RIL_Data_Call_Response_v11>(n);
    for (int i = 0; i < n; i++) readDataCall(p, a, version, list[i]);
    *count = n;
    return list;
}

void readCallForward(Parcel &p, Arena &a, RIL_CallForwardInfo &cf) {
    cf.status = p.readInt32();
    cf.reason = p.readInt32();
    cf.serviceClass = p.readInt32();
    cf.toa = p.readInt32();
    cf.number = readString(p, a);
    cf.timeSeconds = p.readInt32();
}

void readCallList(Parcel &p, Arena &a, std::vector<RIL_Call *> &out) {
    int n = p.readInt32();
    for (int i = 0; i < n; i++) {
        RIL_Call *c = a.make<RIL_Call>();
        c->state = static_cast<RIL_CallState>(p.readInt32());
        c->index = p.readInt32();
        c->toa = p.readInt32();
        c->isMpty = p.readInt32();
        c->isMT = p.readInt32();
        c->als = p.readInt32();
        c->isVoice = p.readInt32();
        c->isVoicePrivacy = p.readInt32();
        c->number = readString(p, a);
        c->numberPresentation = p.readInt32();
        c->name = readString(p, a);
        c->namePresentation = p.readInt32();
        if (p.readInt32() == 1) {
            RIL_UUS_Info *uus = a.make<RIL_UUS_Info>();
            uus->uusType = static_cast<RIL_UUS_Type>(p.readInt32());
            uus->uusDcs = static_cast<RIL_UUS_DCS>(p.readInt32());
            int len = p.readInt32();
            if (len > 0) {
                uus->uusLength = len;
                uus->uusData = static_cast<char *>(a.bytes(p.readInplace(len), len));
            }
            c->uusInfo = uus;
        }
        out.push_back(c);
    }
}

void readSsData(Parcel &p, Arena &a, RIL_StkCcUnsolSsResponse &ss) {
    ss.serviceType = static_cast<RIL_SsServiceType>(p.readInt32());
    ss.requestType = static_cast<RIL_SsRequestType>(p.readInt32());
    ss.teleserviceType = static_cast<RIL_SsTeleserviceType>(p.readInt32());
    ss.serviceClass = p.readInt32();
    ss.result = static_cast<RIL_Errno>(p.readInt32());
    int n = p.readInt32();
    if (ss.serviceType >= SS_CFU && ss.serviceType <= SS_CF_ALL_CONDITIONAL &&
        ss.requestType == SS_INTERROGATION) {
        if (n > NUM_SERVICE_CLASSES) n = NUM_SERVICE_CLASSES;
        ss.cfData.numValidIndexes = n < 0 ? 0 : n;
        for (int i = 0; i < ss.cfData.numValidIndexes; i++) readCallForward(p, a, ss.cfData.cfInfo[i]);
        return;
    }
    for (int i = 0; i < n; i++) {
        int v = p.readInt32();
        if (i < SS_INFO_MAX) ss.ssInfo[i] = v;
    }
}

}  // namespace

void writeUus(Parcel &p, const RIL_UUS_Info *uus) {
    if (uus == nullptr) {
        p.writeInt32(0);
        return;
    }
    p.writeInt32(1);
    p.writeInt32(uus->uusType);
    p.writeInt32(uus->uusDcs);
    writeBytes(p, uus->uusData, uus->uusLength);
}

bool writeRequest(In in, Parcel &p, const void *data, size_t len) {
    switch (in) {
        case In::VOID:
            return true;
        case In::INTS: {
            const int *v = static_cast<const int *>(data);
            int n = len / sizeof(int);
            p.writeInt32(n);
            for (int i = 0; i < n; i++) p.writeInt32(v[i]);
            return true;
        }
        case In::STRINGS: {
            char *const *v = static_cast<char *const *>(data);
            int n = len / sizeof(char *);
            p.writeInt32(n);
            for (int i = 0; i < n; i++) writeString(p, v[i]);
            return true;
        }
        case In::STRING:
            writeString(p, static_cast<const char *>(data));
            return true;
        case In::RAW:
            writeBytes(p, data, len);
            return true;
        case In::DIAL: {
            const RIL_Dial *d = static_cast<const RIL_Dial *>(data);
            writeString(p, d->address);
            p.writeInt32(d->clir);
            writeUus(p, d->uusInfo);
            return true;
        }
        case In::SIM_IO: {
            const RIL_SIM_IO_v6 *io = static_cast<const RIL_SIM_IO_v6 *>(data);
            p.writeInt32(io->command);
            p.writeInt32(io->fileid);
            writeString(p, io->path);
            p.writeInt32(io->p1);
            p.writeInt32(io->p2);
            p.writeInt32(io->p3);
            writeString(p, io->data);
            writeString(p, io->pin2);
            writeString(p, io->aidPtr);
            return true;
        }
        case In::SMS_WRITE: {
            const RIL_SMS_WriteArgs *w = static_cast<const RIL_SMS_WriteArgs *>(data);
            p.writeInt32(w->status);
            writeString(p, w->pdu);
            writeString(p, w->smsc);
            return true;
        }
        case In::CALL_FORWARD: {
            const RIL_CallForwardInfo *cf = static_cast<const RIL_CallForwardInfo *>(data);
            p.writeInt32(cf->status);
            p.writeInt32(cf->reason);
            p.writeInt32(cf->serviceClass);
            p.writeInt32(cf->toa);
            writeString(p, cf->number);
            p.writeInt32(cf->timeSeconds);
            return true;
        }
        case In::GSM_BR_CONFIG: {
            RIL_GSM_BroadcastSmsConfigInfo *const *v =
                    static_cast<RIL_GSM_BroadcastSmsConfigInfo *const *>(data);
            int n = len / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);
            p.writeInt32(n);
            for (int i = 0; i < n; i++) {
                p.writeInt32(v[i]->fromServiceId);
                p.writeInt32(v[i]->toServiceId);
                p.writeInt32(v[i]->fromCodeScheme);
                p.writeInt32(v[i]->toCodeScheme);
                p.writeInt32(v[i]->selected ? 1 : 0);
            }
            return true;
        }
        case In::SIM_APDU: {
            const RIL_SIM_APDU *apdu = static_cast<const RIL_SIM_APDU *>(data);
            p.writeInt32(apdu->sessionid);
            p.writeInt32(apdu->cla);
            p.writeInt32(apdu->instruction);
            p.writeInt32(apdu->p1);
            p.writeInt32(apdu->p2);
            p.writeInt32(apdu->p3);
            writeString(p, apdu->data);
            return true;
        }
        case In::SIM_AUTH: {
            const RIL_SimAuthentication *auth = static_cast<const RIL_SimAuthentication *>(data);
            p.writeInt32(auth->authContext);
            writeString(p, auth->authData);
            writeString(p, auth->aid);
            return true;
        }
        default:
            return false;
    }
}

void readResponse(Slot &slot, const Pending &req, RIL_Errno err, Parcel &p) {
    Arena a;
    RIL_Token t = req.token;

    if (req.out == Out::IDENTITY_IMEI) {
        char *imei = err == RIL_E_SUCCESS ? readString(p, a) : nullptr;
        if (!imei) {
            complete(t, err == RIL_E_SUCCESS ? RIL_E_GENERIC_FAILURE : err, nullptr, 0);
            return;
        }
        slot.imei = imei;
        if (!send(slot, RIL_REQUEST_GET_IMEISV, Out::IDENTITY_IMEISV, t, -1, Parcel())) {
            complete(t, RIL_E_RADIO_NOT_AVAILABLE, nullptr, 0);
        }
        return;
    }
    if (req.out == Out::IDENTITY_IMEISV) {
        char *sv = err == RIL_E_SUCCESS ? readString(p, a) : nullptr;
        std::vector<char *> v = {a.str(slot.imei), sv ? sv : a.str(""), a.str(""), a.str("")};
        complete(t, RIL_E_SUCCESS, v.data(), v.size() * sizeof(char *));
        return;
    }

    if (p.dataAvail() == 0 && req.out != Out::SETUP_DATA) {
        mtkResponse(slot, req, err, nullptr);
        complete(t, err, nullptr, 0);
        return;
    }

    switch (req.out) {
        case Out::VOID:
            mtkResponse(slot, req, err, nullptr);
            complete(t, err, nullptr, 0);
            break;
        case Out::INTS: {
            int n = p.readInt32();
            std::vector<int> v;
            for (int i = 0; i < n && p.dataAvail() > 0; i++) v.push_back(p.readInt32());
            complete(t, err, v.empty() ? nullptr : v.data(), v.size() * sizeof(int));
            break;
        }
        case Out::STRINGS: {
            std::vector<char *> v = readStrings(p, a);
            deliverStrings(t, err, v);
            break;
        }
        case Out::STRING: {
            char *s = readString(p, a);
            complete(t, err, s, s ? strlen(s) + 1 : 0);
            break;
        }
        case Out::RAW: {
            int n = p.readInt32();
            const void *d = n > 0 ? p.readInplace(n) : nullptr;
            complete(t, err, const_cast<void *>(d), d ? n : 0);
            break;
        }
        case Out::SIM_STATUS: {
            RIL_CardStatus_v6 *cs = a.make<RIL_CardStatus_v6>();
            readCardStatus(p, a, *cs);
            complete(t, err, cs, sizeof(*cs));
            break;
        }
        case Out::CALL_LIST: {
            std::vector<RIL_Call *> calls;
            readCallList(p, a, calls);
            complete(t, err, calls.empty() ? nullptr : calls.data(), calls.size() * sizeof(RIL_Call *));
            break;
        }
        case Out::SIGNAL: {
            RIL_SignalStrength_v10 *ss = a.make<RIL_SignalStrength_v10>();
            readSignal(p, *ss);
            complete(t, err, ss, sizeof(*ss));
            break;
        }
        case Out::SIM_IO: {
            RIL_SIM_IO_Response *io = a.make<RIL_SIM_IO_Response>();
            io->sw1 = p.readInt32();
            io->sw2 = p.readInt32();
            io->simResponse = readString(p, a);
            complete(t, err, io, sizeof(*io));
            break;
        }
        case Out::SMS: {
            RIL_SMS_Response *sms = a.make<RIL_SMS_Response>();
            sms->messageRef = p.readInt32();
            sms->ackPDU = readString(p, a);
            sms->errorCode = readIntOr(p, -1);
            complete(t, err, sms, sizeof(*sms));
            break;
        }
        case Out::SETUP_DATA: {
            RIL_Data_Call_Response_v11 *dc = nullptr;
            if (p.dataAvail() > 0) {
                dc = a.make<RIL_Data_Call_Response_v11>();
                readSetupDataCall(p, a, *dc);
            }
            mtkResponse(slot, req, err, dc);
            complete(t, err, dc, dc ? sizeof(*dc) : 0);
            break;
        }
        case Out::DATA_LIST: {
            int n;
            RIL_Data_Call_Response_v11 *list = readDataCallList(p, a, &n);
            complete(t, err, n ? list : nullptr, n * sizeof(*list));
            break;
        }
        case Out::CALL_FORWARD: {
            int n = p.readInt32();
            std::vector<RIL_CallForwardInfo *> v;
            for (int i = 0; i < n; i++) {
                RIL_CallForwardInfo *cf = a.make<RIL_CallForwardInfo>();
                readCallForward(p, a, *cf);
                v.push_back(cf);
            }
            complete(t, err, v.empty() ? nullptr : v.data(), v.size() * sizeof(RIL_CallForwardInfo *));
            break;
        }
        case Out::FAIL_CAUSE: {
            RIL_LastCallFailCauseInfo *fc = a.make<RIL_LastCallFailCauseInfo>();
            fc->cause_code = static_cast<RIL_LastCallFailCause>(p.readInt32());
            fc->vendor_cause = p.dataAvail() > 0 ? readString(p, a) : nullptr;
            complete(t, err, fc, sizeof(*fc));
            break;
        }
        case Out::GSM_BR_CONFIG: {
            int n = p.readInt32();
            std::vector<RIL_GSM_BroadcastSmsConfigInfo *> v;
            for (int i = 0; i < n; i++) {
                RIL_GSM_BroadcastSmsConfigInfo *c = a.make<RIL_GSM_BroadcastSmsConfigInfo>();
                c->fromServiceId = p.readInt32();
                c->toServiceId = p.readInt32();
                c->fromCodeScheme = p.readInt32();
                c->toCodeScheme = p.readInt32();
                c->selected = p.readInt32() != 0;
                v.push_back(c);
            }
            complete(t, err, v.empty() ? nullptr : v.data(),
                     v.size() * sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
            break;
        }
        case Out::VOICE_REG: {
            std::vector<char *> v = readStrings(p, a);
            resizeStrings(v, 15);
            deliverStrings(t, err, v);
            break;
        }
        case Out::DATA_REG: {
            std::vector<char *> v = readStrings(p, a);
            /* MTK reports its own radio technologies from 128 up; they are HSPA+ variants */
            if (v.size() > 3 && v[3] && atoi(v[3]) >= 128) v[3] = a.str("15");
            resizeStrings(v, v.size() > 6 ? 11 : 6);
            deliverStrings(t, err, v);
            break;
        }
        case Out::OPERATOR: {
            std::vector<char *> v = readStrings(p, a);
            resizeStrings(v, 3);
            deliverStrings(t, err, v);
            break;
        }
        case Out::NETWORKS: {
            std::vector<char *> v = readStrings(p, a);
            if (v.size() % 4 != 0 && v.size() % 5 == 0) {
                std::vector<char *> four;
                for (size_t i = 0; i < v.size(); i++) {
                    if (i % 5 < 4) four.push_back(v[i]);
                }
                v.swap(four);
            }
            v.resize(v.size() - v.size() % 4);
            deliverStrings(t, err, v);
            break;
        }
        case Out::DEVICE_ID: {
            std::vector<char *> v = readStrings(p, a);
            resizeStrings(v, 4);
            deliverStrings(t, err, v);
            break;
        }
        case Out::IDENTITY_IMEI:
        case Out::IDENTITY_IMEISV:
            /* handled before the switch */
            break;
    }
}

void readUnsolicited(Slot &slot, int unsol, Parcel &p) {
    Arena a;

    switch (unsol) {
        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED:
        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED:
        case RIL_UNSOL_STK_SESSION_END:
        case RIL_UNSOL_SIM_SMS_STORAGE_FULL:
        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE:
        case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE:
        case RIL_UNSOL_RESEND_INCALL_MUTE:
        case RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED:
        case RIL_UNSOL_CALL_RING:
            unsolicited(slot, unsol, nullptr, 0);
            break;
        case RIL_UNSOL_RESPONSE_NEW_SMS:
        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT: {
            /* libril decodes the hex PDU using the length it is given */
            char *pdu = readString(p, a);
            if (pdu) unsolicited(slot, unsol, pdu, strlen(pdu));
            break;
        }
        case RIL_UNSOL_NITZ_TIME_RECEIVED:
        case RIL_UNSOL_STK_PROACTIVE_COMMAND:
        case RIL_UNSOL_STK_EVENT_NOTIFY:
        case RIL_UNSOL_STK_CC_ALPHA_NOTIFY: {
            char *s = readString(p, a);
            if (s) unsolicited(slot, unsol, s, strlen(s) + 1);
            break;
        }
        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM:
        case RIL_UNSOL_RESTRICTED_STATE_CHANGED:
        case RIL_UNSOL_RINGBACK_TONE:
        case RIL_UNSOL_VOICE_RADIO_TECH_CHANGED:
        case RIL_UNSOL_SRVCC_STATE_NOTIFY:
        case RIL_UNSOL_STK_CALL_SETUP: {
            if (p.readInt32() < 1) break;
            int v = p.readInt32();
            unsolicited(slot, unsol, &v, sizeof(v));
            break;
        }
        case RIL_UNSOL_SIGNAL_STRENGTH: {
            RIL_SignalStrength_v10 *ss = a.make<RIL_SignalStrength_v10>();
            readSignal(p, *ss);
            unsolicited(slot, unsol, ss, sizeof(*ss));
            break;
        }
        case RIL_UNSOL_DATA_CALL_LIST_CHANGED: {
            int n;
            RIL_Data_Call_Response_v11 *list = readDataCallList(p, a, &n);
            unsolicited(slot, unsol, n ? list : nullptr, n * sizeof(*list));
            break;
        }
        case RIL_UNSOL_SUPP_SVC_NOTIFICATION: {
            RIL_SuppSvcNotification *n = a.make<RIL_SuppSvcNotification>();
            n->notificationType = p.readInt32();
            n->code = p.readInt32();
            n->index = p.readInt32();
            n->type = p.readInt32();
            n->number = readString(p, a);
            unsolicited(slot, unsol, n, sizeof(*n));
            break;
        }
        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS:
        case RIL_UNSOL_OEM_HOOK_RAW: {
            int n = p.readInt32();
            if (n <= 0) break;
            unsolicited(slot, unsol, p.readInplace(n), n);
            break;
        }
        case RIL_UNSOL_ON_SS: {
            RIL_StkCcUnsolSsResponse *ss = a.make<RIL_StkCcUnsolSsResponse>();
            readSsData(p, a, *ss);
            unsolicited(slot, unsol, ss, sizeof(*ss));
            break;
        }
        default:
            ALOGV("slot %d: dropping unsolicited %d", slot.id, unsol);
            break;
    }
}

}  // namespace bridge
