/* 99_iokit_hid_plugin — IOKit CFPlugIn (COM) interfaces from i386 code, on the
 * exact path Halo's gamepad support takes:
 *   IOMasterPort -> IONotificationPortCreate -> IOServiceAddMatchingNotification
 *   (IOHIDDevice) -> walk the iterator -> IOCreatePlugInInterfaceForService
 *   (HID user client) -> (*plugin)->QueryInterface(kIOHIDDeviceInterfaceID)
 *   -> AddRef/Release, allocQueue -> (*queue)->create/dispose.
 * The interface pointers are `Vtbl **` and every call goes through a function
 * pointer read out of the table. Natively both levels are 64-bit and every
 * entry is x86_64 code, so without the bridge the out-param truncates and the
 * first (*iface)->... call faults; the matching notification also used to hand
 * back an empty iterator on purpose (devices == 0).
 * No device is opened (a keyboard would need Input Monitoring). Every Mac has
 * at least one HID device (keyboard / trackpad). macOS itself refuses a queue
 * on some devices (allocQueue == NULL natively too), so the queue check is that
 * every queue that WAS allocated creates and disposes cleanly.
 */
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <stdio.h>
#include <stdlib.h>

static void matched(void *refcon, io_iterator_t it) { (void)refcon; (void)it; }

int main(void) {
    mach_port_t master = MACH_PORT_NULL;
    IOMasterPort(MACH_PORT_NULL, &master);
    IONotificationPortRef port = IONotificationPortCreate(master);
    io_iterator_t it = 0;
    kern_return_t kr = IOServiceAddMatchingNotification(
        port, kIOFirstMatchNotification, IOServiceMatching(kIOHIDDeviceKey),
        matched, NULL, &it);
    int devices = 0, plugins = 0, hids = 0, allocs = 0, queues = 0;
    io_service_t svc;
    while (kr == KERN_SUCCESS && (svc = IOIteratorNext(it))) {
        devices++;
        IOCFPlugInInterface **plugin = NULL;
        SInt32 score = 0;
        if (IOCreatePlugInInterfaceForService(svc, kIOHIDDeviceUserClientTypeID,
                kIOCFPlugInInterfaceID, &plugin, &score) == KERN_SUCCESS && plugin) {
            plugins++;
            IOHIDDeviceInterface **dev = NULL;
            HRESULT hr = (*plugin)->QueryInterface(plugin,
                CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (LPVOID *)&dev);
            if (hr == S_OK && dev) {
                ULONG n = (*dev)->AddRef(dev);
                if (n >= 2 && (*dev)->Release(dev) == n - 1) hids++;
                IOHIDQueueInterface **q = (*dev)->allocQueue(dev);
                if (q) {
                    allocs++;
                    if ((*q)->create(q, 0, 8) == kIOReturnSuccess &&
                        (*q)->dispose(q) == kIOReturnSuccess) queues++;
                    (*q)->Release(q);
                }
                (*dev)->Release(dev);
            }
            IODestroyPlugInInterface(plugin);
        }
        IOObjectRelease(svc);
        if (devices >= 4) break;
    }
    printf("devices: %s\n", devices > 0 ? "found" : "none");
    printf("plugin: %s\n", plugins == devices ? "ok" : "FAIL");
    printf("hid interface: %s\n", hids == plugins ? "ok" : "FAIL");
    printf("queue: %s\n", queues == allocs ? "ok" : "FAIL");
    exit(0);
}
