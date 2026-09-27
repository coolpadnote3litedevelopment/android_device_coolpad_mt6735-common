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

#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <binder/Parcel.h>
#include <telephony/ril.h>

#if !defined(ANDROID_MULTI_SIM) || !defined(ANDROID_SIM_COUNT_2)
#error "the bridge serves both MTK SIM sockets and needs SIM_COUNT := 2"
#endif
#define BRIDGE_SIM_COUNT 2

namespace bridge {

using android::Parcel;

/* Owns the strings and buffers behind a decoded response until it is delivered */
class Arena {
  public:
    char *str(const char *s, size_t len);
    char *str(const std::string &s) { return str(s.data(), s.size()); }
    void *bytes(const void *data, size_t len);
    template <typename T> T *make() {
        mBlobs.emplace_back(sizeof(T), 0);
        return reinterpret_cast<T *>(mBlobs.back().data());
    }
    template <typename T> T *array(size_t n) {
        mBlobs.emplace_back(sizeof(T) * (n ? n : 1), 0);
        return reinterpret_cast<T *>(mBlobs.back().data());
    }

  private:
    std::deque<std::string> mStrings;
    std::deque<std::vector<uint8_t>> mBlobs;
};

enum class In {
    NONE,
    VOID,
    INTS,
    STRINGS,
    STRING,
    RAW,
    DIAL,
    SIM_IO,
    SMS_WRITE,
    CALL_FORWARD,
    GSM_BR_CONFIG,
    SIM_APDU,
    SIM_AUTH,
    IA_APN,
    SETUP_DATA,
    IMS_SMS,
};

enum class Out {
    VOID,
    INTS,
    STRINGS,
    STRING,
    RAW,
    SIM_STATUS,
    CALL_LIST,
    SIGNAL,
    SIM_IO,
    SMS,
    SETUP_DATA,
    DATA_LIST,
    CALL_FORWARD,
    FAIL_CAUSE,
    GSM_BR_CONFIG,
    VOICE_REG,
    DATA_REG,
    OPERATOR,
    NETWORKS,
    DEVICE_ID,
    /* DEVICE_IDENTITY built from GET_IMEI and GET_IMEISV */
    IDENTITY_IMEI,
    IDENTITY_IMEISV,
};

struct Pending {
    RIL_Token token;
    int request;
    Out out;
    int dataIndex;
};

struct Slot {
    int id;
    const char *socket;
    int fd = -1;
    int rilVersion = 0;
    RIL_RadioState state = RADIO_STATE_UNAVAILABLE;
    int32_t serial = 0;
    std::mutex writeLock;
    std::mutex pendingLock;
    std::map<int32_t, Pending> pending;
    /* identity requests that hit a powered-down modem, resent once the radio is on */
    std::vector<Pending> deferred;
    std::string imei;
    /* cid per ccmni interface, -1 when free, CID_RESERVED while a setup is in flight */
    int dataCids[5] = {-1, -1, -1, -1, -1};
};

/* codec.cpp: request payloads from the Oreo RIL structs to the M wire format */
bool writeRequest(In in, Parcel &p, const void *data, size_t len);
/* codec.cpp: M wire payloads back to the structs libril expects */
void readResponse(Slot &slot, const Pending &req, RIL_Errno err, Parcel &p);
void readUnsolicited(Slot &slot, int unsol, Parcel &p);

void writeString(Parcel &p, const char *s);
void writeUus(Parcel &p, const RIL_UUS_Info *uus);
char *readString(Parcel &p, Arena &a);
std::vector<char *> readStrings(Parcel &p, Arena &a);

/* bridge.cpp */
void complete(RIL_Token t, RIL_Errno err, void *data, size_t len);
void unsolicited(Slot &slot, int unsol, const void *data, size_t len);
bool send(Slot &slot, int request, Out out, RIL_Token t, int dataIndex, const Parcel &payload);

/* mtk.cpp */
bool mtkRequest(Slot &slot, int request, const void *data, size_t len, RIL_Token t);
void mtkResponse(Slot &slot, const Pending &req, RIL_Errno err, void *data);
bool mtkUnsolicited(Slot &slot, int unsol, Parcel &p);
int reserveInterface(Slot &slot, int profile);
void releaseInterface(Slot &slot, int index, int cid);

}  // namespace bridge
