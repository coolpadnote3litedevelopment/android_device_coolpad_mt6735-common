#include <dlfcn.h>
#include <stddef.h>
#include <string.h>

#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>

namespace android {

class Sensor;
class SensorEventQueue : public RefBase {};

// The camera blobs allocate a Marshmallow-sized SensorManager and construct
// it in place; Oreo's is larger, so only a pointer to the real one lives there.
class SensorManager {
public:
    SensorManager(const String16& opPackageName);
    Sensor const* getDefaultSensor(int type);
    sp<SensorEventQueue> createEventQueue(String8 packageName, int mode);
    static SensorManager& getInstanceForPackage(const String16& packageName);

private:
    SensorManager* mReal;
};

namespace {

template <typename P>
P realMethod(const char* name) {
    static void* lib = dlopen("libsensor.so", RTLD_NOW);
    struct { void* ptr; ptrdiff_t adj; } raw = { lib ? dlsym(lib, name) : nullptr, 0 };
    P pmf;
    memcpy(&pmf, &raw, sizeof(pmf));
    return pmf;
}

}

SensorManager::SensorManager(const String16& opPackageName)
    : mReal(&getInstanceForPackage(opPackageName)) {
}

Sensor const* SensorManager::getDefaultSensor(int type) {
    using Fn = Sensor const* (SensorManager::*)(int);
    static Fn fn = realMethod<Fn>("_ZN7android13SensorManager16getDefaultSensorEi");
    return (mReal->*fn)(type);
}

sp<SensorEventQueue> SensorManager::createEventQueue(String8 packageName, int mode) {
    using Fn = sp<SensorEventQueue> (SensorManager::*)(String8, int);
    static Fn fn = realMethod<Fn>("_ZN7android13SensorManager16createEventQueueENS_7String8Ei");
    return (mReal->*fn)(packageName, mode);
}

}
