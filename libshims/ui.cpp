#include <new>

#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>

extern "C" {
    android::status_t _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handlejRKNS_4RectEP13android_ycbcr(void *thiz, buffer_handle_t, uint32_t, const android::Rect&, android_ycbcr*);

    android::status_t _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handleiRKNS_4RectEP13android_ycbcr(void *thiz, buffer_handle_t handle, int usage, const android::Rect& bounds, android_ycbcr *ycbcr) {
        return _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handlejRKNS_4RectEP13android_ycbcr(thiz, handle, static_cast<uint32_t>(usage), bounds, ycbcr);
    }

    android::status_t _ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPv(void *thiz, buffer_handle_t, uint32_t, const android::Rect&, void**);

    android::status_t _ZN7android19GraphicBufferMapper4lockEPK13native_handleiRKNS_4RectEPPv(void *thiz, buffer_handle_t handle, int usage, const android::Rect& bounds, void** vaddr) {
        return _ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPv(thiz, handle, static_cast<uint32_t>(usage), bounds, vaddr);
    }

    void *_ZN7android13GraphicBufferC1Ejjij(void *thiz, uint32_t inWidth, uint32_t inHeight, int32_t inFormat, uint32_t inUsage) {
        return new (thiz) android::GraphicBuffer(inWidth, inHeight, inFormat, inUsage);
    }

    android::status_t _ZN7android5Fence4waitEi(void *thiz, int);

    android::status_t _ZN7android5Fence4waitEj(void *thiz, unsigned int timeout) {
        return _ZN7android5Fence4waitEi(thiz, static_cast<int>(timeout));
    }
}
