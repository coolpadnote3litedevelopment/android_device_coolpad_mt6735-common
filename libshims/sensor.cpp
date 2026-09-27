#include <dlfcn.h>
#include <pthread.h>
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

private:
    SensorManager* mReal;
};

namespace {

void* libsensor() {
    static void* lib = dlopen("libsensor.so", RTLD_NOW);
    return lib;
}

template <typename P>
P realMethod(const char* name) {
    void* lib = libsensor();
    struct { void* ptr; ptrdiff_t adj; } raw = { lib ? dlsym(lib, name) : nullptr, 0 };
    P pmf;
    memcpy(&pmf, &raw, sizeof(pmf));
    return pmf;
}

}

/*
 * The blob inlines M's getInstanceForPackage() and calls this constructor with
 * its own copy of sLock held, which libsensor also binds to, so going through
 * libsensor's getInstanceForPackage() here deadlocks. Build one real instance
 * with the Oreo constructor instead.
 */
static SensorManager* realInstance(const String16& opPackageName) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static SensorManager* real;

    pthread_mutex_lock(&lock);
    if (!real) {
        using Ctor = void (*)(void*, const String16&);
        Ctor ctor = reinterpret_cast<Ctor>(
                dlsym(libsensor(), "_ZN7android13SensorManagerC2ERKNS_8String16E"));
        if (ctor) {
            /* larger than the Oreo SensorManager, whose layout isn't visible here */
            void* mem = ::operator new(1024);
            memset(mem, 0, 1024);
            ctor(mem, opPackageName);
            real = static_cast<SensorManager*>(mem);
        }
    }
    pthread_mutex_unlock(&lock);
    return real;
}

SensorManager::SensorManager(const String16& opPackageName)
    : mReal(realInstance(opPackageName)) {
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
